#include <ace/Init_ACE.h>

#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>
#include <sstream>

#include "cdputil.h"
#include "localconfigurator.h"
#include "logger.h"
#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include "terminateonheapcorruption.h"

#include "EvtCollForwCommon.h"
#include "LoggerReader.h"
#include "RunnerFactories.h"
#include "pssettingsconfigurator.h"

#ifdef SV_UNIX
#include <sys/prctl.h>
#include "utilfdsafeclose.h"
#endif // SV_UNIX

using namespace EvtCollForw;

boost::shared_ptr<LocalConfigurator> lc;
bool gbPrivateEndpointEnabled = false;

#ifdef SV_WINDOWS

static HRESULT InitializeCom()
{
    HRESULT hr = S_OK;

    do
    {
        hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);

        if (FAILED(hr))
        {
            DebugPrintf("FAILED: CoInitializeEx() failed. hr = 0x%08x.\n", hr);
            break;
        }

        hr = CoInitializeSecurity(NULL,
            -1,
            NULL,
            NULL,
            RPC_C_AUTHN_LEVEL_CONNECT,
            RPC_C_IMP_LEVEL_IMPERSONATE,
            NULL,
            EOAC_NONE,
            NULL);


        if (hr == RPC_E_TOO_LATE)
        {
            hr = S_OK;
        }

        if (hr != S_OK)
        {
            DebugPrintf("FAILED: CoInitializeSecurity() failed. hr = 0x%08x.\n", hr);
            CoUninitialize();
            break;
        }
    } while (false);

    return hr;
}

static void UninitializeCom()
{
    CoUninitialize();
}

#endif // SV_WINDOWS


static bool ParseArguments(int argc, char* argv[], CmdLineSettings &parsedSettings)
{
    int currArg = 1;

    while (currArg < argc)
    {
        if (boost::iequals(EvtCollForw_CmdOpt_Environment, argv[currArg]))
        {
            if (++currArg >= argc)
            {
                DebugPrintf(EvCF_LOG_ERROR, "Invalid cmd line argument usage. Value not found for : %s.\n", argv[currArg - 1]);
                return false;
            }

            parsedSettings.Environment = argv[currArg];
        }
        else
        {
            DebugPrintf(EvCF_LOG_ERROR, "Invalid cmd line argument found : %s.\n", argv[currArg]);
            return false;
        }

        ++currArg;
    }

    BOOST_ASSERT(currArg == argc);
    DebugPrintf(EvCF_LOG_INFO, "Successfuly parsed the cmd line arguments.\n");
    return true;
}

static boost::shared_ptr<IRunnerFactory> GetRunnerFactory(CmdLineSettings &settings)
{
    if (boost::iequals(settings.Environment, EvtCollForw_Env_V2ASource))
    {
        return boost::make_shared<V2ASourceRunnerFactory>(settings);
    }
    else if (boost::iequals(settings.Environment, EvtCollForw_Env_V2ARcmSource))
    {
        return boost::make_shared<V2ARcmSourceRunnerFactory>(settings);
    }
    else if (boost::iequals(settings.Environment, EvtCollForw_Env_V2APS))
    {
        return boost::make_shared<V2AProcessServerRunnerFactory>(settings);
    }
    else if (boost::iequals(settings.Environment, EvtCollForw_Env_V2ARcmPS))
    {
        return boost::make_shared<V2ARcmProcessServerRunnerFactory>(settings);
    }
    else if (boost::iequals(settings.Environment, EvtCollForw_Env_A2ASource))
    {
        return boost::make_shared<A2ASourceRunnerFactory>(settings);
    }
    else if (boost::iequals(settings.Environment, EvtCollForw_Env_V2ARcmSourceOnAzure))
    {
        return boost::make_shared<V2ARcmSourceOnAzureRunnerFactory>(settings);
    }
    else
    {
        const char *errMsg = "EvtCollForw has been initiated in an unknown environment";
        DebugPrintf(EvCF_LOG_ERROR, "%s - %s : %s.\n", FUNCTION_NAME, errMsg, settings.Environment.c_str());
        throw INMAGE_EX(errMsg);
    }
}

void InitializeEnvironment(const CmdLineSettings& settings, std::vector < boost::shared_ptr < void > > &uninitializers)
{
    // We want to achieve the operation similarity of constructors and destructors, i.e. in a stack
    // like manner perform uninits. A destruction of vector takes place as a FIFO destructing from
    // begin to end. We can't use std:stack for this, as the destruction order would depend on the
    // underlying storage used by it (vector/deque/etc.). So, we are performing a push front for the
    // vector via insert.
    BOOST_ASSERT(uninitializers.empty());
    uninitializers.clear();

    // In RCM mode always initialize RcmConfigurator before creating LocalConfigurator instance.
    if (boost::iequals(settings.Environment, EvtCollForw_Env_V2ARcmSource) ||
        boost::iequals(settings.Environment, EvtCollForw_Env_V2ARcmSourceOnAzure) ||
        boost::iequals(settings.Environment, EvtCollForw_Env_V2ARcmPS))
    {
        if (boost::iequals(settings.Environment, EvtCollForw_Env_V2ARcmPS))
        {
            Utils::InitRcmConfigurator();
            Utils::InitPSSettingConfigurator();
            Utils::InitGlobalParams();
            uninitializers.insert(uninitializers.begin(), boost::shared_ptr<void>(
                static_cast<void*>(0), boost::bind(Utils::CleanupPSSettingConfigurator)));
        }
        // Start rcm configurator thread. No need to handle settings change call back.
        // This way use latest in-mem settings always.
        Utils::StartRcmConfigurator();
        uninitializers.insert(uninitializers.begin(), boost::shared_ptr<void>(
            static_cast<void*>(0), boost::bind(Utils::StopRcmConfigurator)));

        gbPrivateEndpointEnabled = Utils::IsPrivateEndpointEnabled(settings.Environment);
    }
    // Initialize global local configurator
    lc = boost::make_shared<LocalConfigurator>();

    // Initialize Quit Event
    {
        if (!CDPUtil::InitQuitEvent(false, false))
            throw "Failed to initialize quit event";

        uninitializers.insert(uninitializers.begin(), boost::shared_ptr<void>(
            static_cast<void*>(0), boost::bind(CDPUtil::UnInitQuitEvent)));
    }

    // Initialize logger
    {
        boost::filesystem::path logPath;
        if (boost::iequals(settings.Environment, EvtCollForw_Env_V2ARcmPS))
        {
            // define and use function to get install path of evtcollforw
            // Read "Install Location" for EvtCollForw.exe directly in registry
            logPath = lc->getPSInstallPathOnCsPrimeApplianceToAzure();
            logPath /= PSComponentPaths::HOME;
            logPath /= PSComponentPaths::SVSYSTEMS;
            logPath /= PSComponentPaths::VAR;
        }
        else
        {
            logPath = lc->getLogPathname();
        }
        logPath /= "evtcollforw.log";

        SetLogFileName(logPath.string().c_str());
        uninitializers.insert(uninitializers.begin(),
            boost::shared_ptr<void>(static_cast<void*>(0), boost::bind(CloseDebug)));

        SetLogLevel(lc->getLogLevel());
        SetLogHostId(lc->getHostId().c_str());

        // Never log the traces of this binary to CS.
        SetLogRemoteLogLevel(SV_LOG_DISABLE);
        //SetLogHttpIpAddress(lc->getHttp().ipAddress);
        //SetLogHttpPort(lc->getHttp().port);
        //SetLogHttpsOption(lc->IsHttps());
        //SetSerializeType(lc->getSerializerType());
        SetDaemonMode();
    }

#ifdef SV_WINDOWS
    // Initialize COM
    {
        HRESULT hr = InitializeCom();
        if (S_OK != hr)
        {
            const char *errMsg = "Failed to initialize COM";
            DebugPrintf(EvCF_LOG_ERROR, "%s - %s. hr = 0x%08x.\n", FUNCTION_NAME, errMsg, hr);
            throw INMAGE_EX(errMsg);
        }

        uninitializers.insert(uninitializers.begin(),
            boost::shared_ptr<void>(static_cast<void*>(0), boost::bind(UninitializeCom)));
    }

    {
        EventLogToMdsLogConverter::InitializeGdprFilteringInfo();

        // TODO-SanKumar-1805:  Find the explanation at the definition of this method.
        // InitializeEventLogPlaceholderInsertionValues();
    }
#endif

    // TODO-SanKumar-1702: There are two such if-else for environment name based switching going to
    // the same class in here and GetRunnerFactory. Should we abstract all the related switching into
    // one, since if a third handling is to be added, one more similar if-else needs to be added.

    // Environment-specific initializations
    if (boost::iequals(settings.Environment, EvtCollForw_Env_A2ASource))
    {
        A2ASourceRunnerFactory::InitializeEnvironment(settings, uninitializers);
    }
    else if (boost::iequals(settings.Environment, EvtCollForw_Env_V2ASource))
    {
        V2ASourceRunnerFactory::InitializeEnvironment(settings, uninitializers);
    }
    else if (boost::iequals(settings.Environment, EvtCollForw_Env_V2ARcmSource))
    {
        V2ARcmSourceRunnerFactory::InitializeEnvironment(settings, uninitializers);
    }
    else if (boost::iequals(settings.Environment, EvtCollForw_Env_V2ARcmSourceOnAzure))
    {
        V2ARcmSourceOnAzureRunnerFactory::InitializeEnvironment(settings, uninitializers);
    }
    else if (boost::iequals(settings.Environment, EvtCollForw_Env_V2APS))
    {
        V2AProcessServerRunnerFactory::InitializeEnvironment(settings, uninitializers);
    }
    else if (boost::iequals(settings.Environment, EvtCollForw_Env_V2ARcmPS))
    {
        V2ARcmProcessServerRunnerFactory::InitializeEnvironment(settings, uninitializers);
    }
    else
    {
        const char *errMsg = "Unknown environment during initialization";
        DebugPrintf(EvCF_LOG_ERROR, "%s - %s : %s.\n", FUNCTION_NAME, errMsg, settings.Environment.c_str());
        throw INMAGE_EX(errMsg);
    }
}

int main(int argc, char* argv[])
{
    const char DEV_NULL[] = "/dev/null";

    ErrorCode returnCode = EvCF_Success;
    std::vector < boost::shared_ptr < void > > globalUninitializers;
    boost::shared_ptr<void> fdGuardIn, fdGuardOut, fdGuardErr;

    ACE::init();
    TerminateOnHeapCorruption();
    CmdLineSettings cmdLineSettings;
    if (!ParseArguments(argc, argv, cmdLineSettings))
    {
        returnCode = EvCF_InvalidCmdLineArg;
        goto Cleanup;
    }

    if (IsProcessRunning(basename_r(argv[0])))
    {
        DebugPrintf(EvCF_LOG_ERROR, "%s is already running. Exiting...\n", argv[0]);
        returnCode = EvCF_DuplicateProcess;
        goto Cleanup;
    }

    CDPUtil::disable_signal(ACE_SIGRTMIN, ACE_SIGRTMAX);

#ifdef SV_WINDOWS
    // SetErrorMode controls  whether the process will handle specified 
    // type of errors or whether the system will handle them
    // by creating a dialog box and notifying the user.
    // In our case, we want all errors to be sent to the program.
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
#endif

#ifdef SV_UNIX
    /* predefining file descriptors 0, 1 and 2
    * so that children never accidentally write
    * anything to valid file descriptors of
    * service like driver or svagents.log etc,. */
    int fdin, fdout, fderr;

    fdin = fdout = fderr = -1;
    fdin = open(DEV_NULL, O_RDONLY);
    fdout = open(DEV_NULL, O_WRONLY);
    fderr = open(DEV_NULL, O_WRONLY);

    fdGuardIn.reset(static_cast<void*>(0), boost::bind(fdSafeClose, fdin));
    fdGuardOut.reset(static_cast<void*>(0), boost::bind(fdSafeClose, fdout));
    fdGuardErr.reset(static_cast<void*>(0), boost::bind(fdSafeClose, fderr));

    set_resource_limits();
    prctl(PR_SET_PDEATHSIG, SIGTERM);
#endif

    MaskRequiredSignals();

    try {
        InitializeEnvironment(cmdLineSettings, globalUninitializers);
    }
    GENERIC_CATCH_LOG_ERRCODE("Initializing EvtCollForw", returnCode);

    if (EvCF_Success != returnCode)
        goto Cleanup;

    try
    {
        BOOST_ASSERT(lc);
        const int pollIntervalInSeconds = lc->getEvtCollForwPollIntervalInSecs();
        const int penaltyTimeInSeconds = lc->getEvtCollForwPenaltyTimeInSecs();
        const int maxStrikes = lc->getEvtCollForwMaxStrikes();
        int currStrikes = 0;

        for (;;)
        {
            try
            {
                boost::shared_ptr<IRunnerFactory> factory;
                boost::shared_ptr<IRunner> runner;

                // TODO-SanKumar-1702: Get the factory only once in the process lifetime!
                factory = GetRunnerFactory(cmdLineSettings);

                if (!CDPUtil::Quit())
                {
                    BOOST_ASSERT(factory != NULL);
                    runner = factory->GetRunner(CDPUtil::Quit);
                }

                if (!CDPUtil::Quit())
                {
                    BOOST_ASSERT(runner != NULL);
                    runner->Run(CDPUtil::Quit);
                }
            }
            CATCH_LOG_AND_RETHROW_ON_NO_MEMORY() // Force quit, on any unforeseen leak scenarios
            GENERIC_CATCH_LOG_ACTION("Executing EvtCollForw runners", currStrikes++);

            if (currStrikes < maxStrikes)
            {
                if (CDPUtil::QuitRequested(pollIntervalInSeconds))
                {
                    returnCode = EvCF_CancellationRequested;
                    break;
                }
            }
            else
            {
                // When the indefinite operation fails for more than the limit, wait out for the penalty
                // time, which is usually long and then quit. Thus, on too many errors, the process would
                // be restarted cleanly after a break.
                DebugPrintf(EvCF_LOG_ERROR, "EvtCollForw has crossed the max strikes limit : %d (%d).\n", currStrikes, maxStrikes);
                if (CDPUtil::QuitRequested(penaltyTimeInSeconds))
                    returnCode = EvCF_CancellationRequested;
                else
                    returnCode = EvCF_Penalized;

                break;
            }
        }

        DebugPrintf(EvCF_LOG_INFO, "evtcollforw completed running in the environment : %s. Error code : %d.\n",
            cmdLineSettings.Environment.c_str(), returnCode);
    }
    GENERIC_CATCH_LOG_ERRCODE("EvtCollForw main logic execution", returnCode);

    if (EvCF_Success != returnCode)
        goto Cleanup;

Cleanup:
    return returnCode;
}
