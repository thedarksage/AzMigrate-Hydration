#include <ace/Init_ACE.h>

#include <boost/algorithm/string/replace.hpp>

#include <curl/curl.h>

#include "logger.h"
#include "inmageex.h"
#include "HandlerCurl.h"
#include "irprofiling.h"
#include "inmsafecapis.h"
#include "RcmConfigurator.h"
#include "dpRcmProgramargs.h"
#include "dataprotectionRcm.h"
#include "resyncStateManager.h"
#include "portablehelpersmajor.h"
#include "dataprotectionexception.h"
#include "terminateonheapcorruption.h"
#include "globalHealthCollator.h"
#include "resthelper/HttpUtil.h"

#ifdef SV_UNIX
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "utilfdsafeclose.h"
#endif

#include "cdputil.h"
#include "LogCutter.h"
#include "common/Trace.h" 


#define INVALID_ARG_ERROR -1
#define INVALID_CONFIG_PATH_ERROR -2

using namespace Logger;

// Note:dataprotecyionSyncRcm never initializes TheConfigurator. This is added only for legacy code compilation purpose.
Configurator* TheConfigurator = NULL; // singleton

extern Log gLog;
const int WaitTimeInSeconds = 60;


static void LogCallback(unsigned int logLevel, const char *msg)
{
    DebugPrintf((SV_LOG_LEVEL)logLevel, msg);
}

int main(int argc, char* argv[])
{
    TerminateOnHeapCorruption();
    init_inm_safe_c_apis();

    ACE::init();

    SV_ULONGLONG retval = 0;
    boost::shared_ptr<Logger::LogCutter> dpRcmSyncLogCutter;

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
    const char DEV_NULL[] = "/dev/null";
    int fdin, fdout, fderr;

    fdin = fdout = fderr = -1;
    fdin = open(DEV_NULL, O_RDONLY);
    fdout = open(DEV_NULL, O_WRONLY);
    fderr = open(DEV_NULL, O_WRONLY);

    boost::shared_ptr<void> fdGuardIn(static_cast<void*>(0), boost::bind(fdSafeClose, fdin));
    boost::shared_ptr<void> fdGuardOut(static_cast<void*>(0), boost::bind(fdSafeClose, fdout));
    boost::shared_ptr<void> fdGuardErr(static_cast<void*>(0), boost::bind(fdSafeClose, fderr));
    set_resource_limits();

    prctl(PR_SET_PDEATHSIG, SIGTERM);
#endif

    // Initialize CURL
    tal::GlobalInitialize();
    boost::shared_ptr<void> guardTal(static_cast<void*>(0), boost::bind(tal::GlobalShutdown));

    MaskRequiredSignals();

    try
    {
        // Disable log level and set daemon mode first as arg parsing and rcmConfigurator init on target happen before
        // logger initialization.
        // So if daemon mode of logger is not set then default o/p to stdError and stdOutPut get enabled and remains enabled.
        // This can lead deadlock if multiple threads try to log same time.
        // We ave seen such case (BUG 7039164) where deadlock scenario hit because of common fd of stdError when 4 SyncApply
        // threads on target failed in AzureApply and try to log error message same time.
        SetLogLevel(SV_LOG_DISABLE);
        SetLogRemoteLogLevel(SV_LOG_DISABLE);
        SetDaemonMode();

        // Parse arguments
        DpRcmProgramArgs dpProgramArgs(argc, argv);

        // On MT prime first initialize RcmConfigurator in FILE_CONFIGURATOR_MODE_CS_PRIME_APPLAINCE_TO_AZURE mode.
        // This is to ensure using local config params for example log level here.
        // Note: any bug in RcmConfigurator::Initialize will not be captured as logger is not initialized yet.
        if (!dpProgramArgs.IsSourceSide())
        {
            RcmClientLib::RcmConfigurator::Initialize(FILE_CONFIGURATOR_MODE_CS_PRIME_APPLAINCE_TO_AZURE, dpProgramArgs.GetTargetReplicationSettingsFileName());
            
            // On appliance first start rcm configurator thread so that config params get initialized for LocalConfigurator to use
            RcmClientLib::RcmConfigurator::getInstance()->Start(RcmClientLib::SETTINGS_SOURCE_LOCAL_CACHE);
        }

        LocalConfigurator localConfigurator;

        //on source only gobal health collator should be implemented
        if (dpProgramArgs.IsSourceSide())
        {
            DataProtectionGlobalHealthCollator(dpProgramArgs.GetDiskId(), localConfigurator.getHealthCollatorDirPath(),
                localConfigurator.getHostId());
        }

        //
        // set the curl_verbose option based on settings in 
        // drscout.conf
        //
        tal::set_curl_verbose(localConfigurator.get_curl_verbose());

        Curl_setsendwindowsize(localConfigurator.getTcpSendWindowSize());
        Curl_setrecvwindowsize(localConfigurator.getTcpRecvWindowSize());

        // Initialize logger and log cutter
        SetLogLevel(localConfigurator.getLogLevel());

        boost::filesystem::path logPathName(localConfigurator.getLogPathname());
        logPathName /= dpProgramArgs.GetLogDir();
        SVMakeSureDirectoryPathExists(logPathName.string().c_str());

        logPathName /= "dataprotection.log";

        // The log cutter must complete before the CloseDebug() is invoked to close the log files.
        dpRcmSyncLogCutter = boost::make_shared<Logger::LogCutter>(Logger::LogCutter(gLog));
        const int maxCompletedLogFiles = 3;
        const boost::chrono::seconds cutInterval(300); // 5 mins.
        std::set<std::string> setCompLogFilesPaths;
        std::string completedLogPath(logPathName.string());
        boost::ireplace_first(completedLogPath, Resync_Folder, localConfigurator.getEvtCollForwIRCompletedFilesMoveDir());
        setCompLogFilesPaths.insert(completedLogPath);
        dpRcmSyncLogCutter->Initialize(logPathName.string(), maxCompletedLogFiles, cutInterval, setCompLogFilesPaths);

        DebugPrintf(SV_LOG_DEBUG, "%s: The log file name is %s\n", FUNCTION_NAME, logPathName.string().c_str());

        if (localConfigurator.getIsAzureStackHubVm())
        {
            // Auzre Stack blob storage supports a lower version of API.
            // https://learn.microsoft.com/en-us/azure-stack/user/azure-stack-acs-differences?view=azs-2206
            AzureStorageRest::HttpRestUtil::Set_X_MS_Version(
                    AzureStorageRest::HttpRestUtil::AZURE_STACK_X_MS_VERSION);
        }

        Trace::Init("",
            (LogLevel)localConfigurator.getLogLevel(),
            boost::bind(&LogCallback, _1, _2));

        BOOST_ASSERT(dpRcmSyncLogCutter);
        dpRcmSyncLogCutter->StartCutting();

        // Initialize quit event
        if (!CDPUtil::InitQuitEvent(false))
        {
            throw INMAGE_EX("Failed to initialize quit event.");
        }

        // Start rcm configurator thread
        RcmClientLib::RcmConfigurator::getInstance()->Start(RcmClientLib::SETTINGS_SOURCE_LOCAL_CACHE);

        // Create instance of ResyncRcmSettings
        if (!ResyncRcmSettings::CreateInstance(dpProgramArgs))
        {
            throw INMAGE_EX("Failed to create ResyncRcmSettings instance.");
        }

        // Create resync state manager
        const boost::shared_ptr<ResyncStateManager> resyncStateManager(
            ResyncStateManager::GetResyncStateManager(dpProgramArgs));

        if (resyncStateManager->IsInlineRestartResyncMarked())
        {
            do
            {
                // Retry clenup untill succeed or quit requested.
                if (resyncStateManager->CleanupRemoteCache())
                {
                    break;
                }
                DebugPrintf(SV_LOG_ERROR, "%s: Failed to cleanup remote cache - retrying in %d sec.", FUNCTION_NAME, WaitTimeInSeconds);
            } while (!CDPUtil::QuitRequested(WaitTimeInSeconds));
        }
        else if (resyncStateManager->CanContinueResync())
        {
            resyncStateManager->DumpCurrentResyncState();

            DataProtectionRcm dataProtectionRcm(dpProgramArgs, resyncStateManager);

            dataProtectionRcm.Protect();
        }
    }
    catch (const DataProtectionInvalidArgsException& dpe)
    {
        retval = INVALID_ARG_ERROR;
        return retval;
    }
    catch (const DataProtectionInvalidConfigPathException& dpe)
    {
        retval = INVALID_CONFIG_PATH_ERROR;
        return retval;
    }
    catch (DataProtectionException& dpe)
    {
        DebugPrintf(dpe.LogLevel(), "%s, Dataprotection got an exception: %s\n", FUNCTION_NAME, dpe.what());
        IRSummaryPostException jirs;
        jirs.UpdateIRProfilingSummaryOnException(DpRcmProgramArgs::GetArgsStr(argc, argv));
    }
    catch (const std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: DataProtectionSyncRcm got an exception: %s\n", FUNCTION_NAME, e.what());
        IRSummaryPostException jirs;
        jirs.UpdateIRProfilingSummaryOnException(DpRcmProgramArgs::GetArgsStr(argc, argv));
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s, DataProtectionSyncRcm got an unknown exception\n", FUNCTION_NAME);
        IRSummaryPostException jirs;
        jirs.UpdateIRProfilingSummaryOnException(DpRcmProgramArgs::GetArgsStr(argc, argv));
    }

    try
    {
        ProfilerFactory::exitProfiler();

        RcmClientLib::RcmConfigurator::getInstance()->Stop();
    }
    catch (const std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: DataProtectionSyncRcm got an exception in cleanup path: %s\n", FUNCTION_NAME, e.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s, DataProtectionSyncRcm got an unknown exception in cleanup path.\n", FUNCTION_NAME);
    }

    if (dpRcmSyncLogCutter)
    {
        // Only "dataprotection_completed_*.log" files are being processed to upload to mds.
        // Complete current log before exit for IR source/MT.
        dpRcmSyncLogCutter->StopCutting();
    }

    CloseDebug();

    return retval;
}

