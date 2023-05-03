#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

#include <exception>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <algorithm>
#include <ace/OS_NS_errno.h>
#include <ace/os_include/os_limits.h>
#include "configurator2.h"
#include "logger.h"
#include "pair_iterator.hpp"
#include "portable.h"		// todo: remove dependency on common
#include "inmageproduct.h"
#include "host.h"
#include "operatingsystem.h"
#include "volume.h"
#include "compatibility.h"
#include "device.h"
#include "devicestream.h"
#include "svdparse.h"
#include "VsnapUser.h"
#include "cdputil.h"
#include "../tal/HandlerCurl.h"
#include "inmsafecapis.h"

#ifdef SV_WINDOWS
#include <winioctl.h>
#include "cdpsnapshotrequest.h"
#include "virtualvolume.h"
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "executecommand.h"
#include "utilfdsafeclose.h"
#include "unixvirtualvolume.h"	
#include "persistentdevicenames.h"
#endif 

#include "devicefilter.h"
#include "volumeinfocollector.h"
#include "branding_parameters.h"
#include "vxservice.h"
#include "isXen.h"
#include "XenServerDiscovery.h"
#include <ace/File_Lock.h>

#include "retentioninformation.h"
#include "configwrapper.h"
#include "cdplock.h"
#include "portablehelpersmajor.h"
#include "iscsiinitiatorregistration.h"

#include "serializationtype.h"
#include "serializeinitialsettings.h"
#include "stopvolumegroups.h"
#include "datacacher.h"
#include "inmalertdefs.h"
#include "inmuuid.h"
#include "setpermissions.h"
#include "securityutils.h"
#include "terminateonheapcorruption.h"
#include "azurevmrecoverymanager.h"

#include "cdpvolume.h"
#include "common/Trace.h"
#include "RcmClient.h"
#include "LogCutter.h"

#include "Monitoring.h"
#include "RcmClientErrorCodes.h"
#include "VacpConfDefs.h"
#include "RcmConfigurator.h"

#include "devicestream.h"
#include "filterdrvifmajor.h"
#include "devicefilter.h"

#include "CertHelpers.h"

using namespace std;
using namespace Logger;

extern Log gLog;

int const SETTINGS_POLLING_INTERVAL = 5;

bool quit_signalled = false;

FilterDriverNotifier* VxService::m_sPFilterDriverNotifier = NULL;

static const std::string InMageEvtCollForwName = "InMageEvtCollForw";
static const int InMageEvtCollForwId = -4;

static const std::string InMageVacpName = "InMageAgentVacp";
static const int InMageVacpId = -5;

static const std::string InMageSentinelName = "InMageAgentS2";
static const int InMageSentinelId = 0;

static const std::string InMageDataprotectionName = "InMageAgentDataprotection";
static const int InMageDataprotectionId = -8;

static const long int AGENTSTIME_TOQUIT_INSECS = 5;

#ifdef SV_WINDOWS
boost::once_flag g_manualBitlockerStateUpdateOnce = BOOST_ONCE_INIT;
#endif // SV_WINDOWS


#ifndef SV_WINDOWS
#include <boost/thread.hpp>
extern boost::mutex g_MutexForkProcess;
#endif

/*
 * a call back function used to log messages by other libraries that do not use
 * DebugPrintf(...) logging
 */
static void LogCallback(unsigned int logLevel, const char *msg)
{
    DebugPrintf((SV_LOG_LEVEL)logLevel, "%s", msg);
}

/*
* FUNCTION NAME :  SetupLocalLogging
*
* DESCRIPTION : get the local logging settings from configurator 
*               and enable local logging
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, false otherwise
*
*/
static bool SetupLocalLogging(boost::shared_ptr<Logger::LogCutter> logCutter, std::string &errMessage)
{
    bool rv = false;

    try 
    {
        LocalConfigurator localconfigurator;

        SetDaemonMode();

        boost::filesystem::path logPath(localconfigurator.getLogPathname());
        logPath /= "svagents.log";

        // TODO-SanKumar-1612: Move this to configuration.
        const int maxCompletedLogFiles = 3;
        const boost::chrono::seconds cutInterval(300); // 5 mins.

        logCutter->Initialize(logPath, maxCompletedLogFiles, cutInterval);
        SetLogLevel(localconfigurator.getLogLevel());
        SetLogHttpIpAddress( localconfigurator.getHttp().ipAddress );
        SetLogHttpPort( localconfigurator.getHttp().port );
        SetLogHostId( localconfigurator.getHostId().c_str() );
        SetLogRemoteLogLevel(SV_LOG_DISABLE);
        SetSerializeType( localconfigurator.getSerializerType() ) ;
        SetLogHttpsOption(localconfigurator.IsHttps());

        Trace::Init("",
            (LogLevel)localconfigurator.getLogLevel(),
            boost::bind(&LogCallback, _1, _2));

        rv = true;

    }
    catch (ContextualException &ce)
    {
        errMessage = ce.what();
        rv = false;
    }
    catch(...) 
    {
        errMessage = std::string("Local Logging initialization failed.");
        rv = false;
    }

    return rv;
}

int
ACE_TMAIN (int argc, ACE_TCHAR* argv[])
{	

    //
    // PR # 6107
    // Issue: svagents stopped local logging after logging
    // first few messages.
    // The last message that was logged was
    // "UnixService::run_startup_code"
    // The issue got in when we moved the local logging initialization code 
    // much ahead in  the main before the first debugprintf gets called (see PR#5519).
    // on unix, svagents calls ACE::daemonize in run_startup_code 
    // to runs service as deamon. ACE::ACE::daemonize was invoked with option
    // to close all handles. As a result even local log file handle was closed.
    // This was not a issue earlier because local logging was being initialized 
    // after it was setup as deamon. But it had other problems, that debugprintfs
    // were being called even without opening the local log.
    // This issue has been fixed as follows:
    //  1. First close all handles in main
    //  2. Setup local logging in main before first debugprintf gets invoked
    //  3. Start as deamon but do not close handles
    //

    TerminateOnHeapCorruption();
   
    // step 1 for PR #6107 : close down the I/O handles
    
    //calling init funtion of inmsafec library
    init_inm_safe_c_apis();
#ifdef SV_UNIX
    for (int i = ACE::max_handles () - 1; i >= 0; i--)
        ACE_OS::close (i);

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
#endif

    MaskRequiredSignals();

    //
    // Initialize logging options as soon as the service starts
    //
    // PR# 5519 svagent crash
    // changes made as part of code review:
    // make sure we initialize local logging before calling debugprintf
    // else we end up using an uninitialized handle
    // For example, VxService constructor uses DebugPrintf
    //

    int ret = 0;

    {
        // The log cutter must complete before the CloseDebug() is invoked to close the log files.
        boost::shared_ptr<Logger::LogCutter> logCutter(new Logger::LogCutter(gLog));

        std::string startupError;
        if (!SetupLocalLogging(logCutter, startupError))
        {
            LOG2SYSLOG((LM_ERROR, startupError.c_str()));
            return -1;
        }

        if (IsProcessRunning(ACE_TEXT_ALWAYS_CHAR(ACE::basename(argv[0], ACE_DIRECTORY_SEPARATOR_CHAR))))
        {
            const int RETRIES = 10;
            int iTries = 0;
            for (; iTries < RETRIES; iTries++)
            {
                if (!IsProcessRunning(ACE_TEXT_ALWAYS_CHAR(ACE::basename(argv[0], ACE_DIRECTORY_SEPARATOR_CHAR))))
                    break;
                else
                    ACE_OS::sleep(1);
            }
            if (RETRIES == iTries)
            {
                //       DebugPrintf(SV_LOG_ERROR,"svagent already running.Exiting...\n");
                printf("svagent already running.Exiting...\n");
                //   	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
                return 0;
            }
        }

        boost::shared_ptr<void> guardRcm(static_cast<void*>(0), boost::bind(RcmConfigurator::Uninitialize));
        RcmConfigurator::Initialize();

        try {
            VxService srv;

            if (srv.parse_args(argc, argv) != 0)
            {
                std::cout << "svagents not running as a service. Exiting." << std::endl;
                ACE_OS::exit(-1);
            }
            srv.name();
            srv.m_LogCutter = logCutter;

            try {
                if (srv.m_optStandalone)
                {
                    // Start actual work
                    ret = VxService::StartWork(&srv);
                }
                else if ((ret = srv.run_startup_code()) == -1)
                {
                    std::cout << "svagents not running as a service. Exiting." << std::endl;
                    //
                    // PR# 5519 svagent crash
                    // added below comment as part of code review:
                    //  run_startup_code had already setup up service start handler
                    //  if it returns -1, it means we failed
                    //  in such scenario, we should just exit and not call
                    //  start handler explicitly. 
                    // so it would seem pointless to call StartWork below. 
                    ACE_OS::exit(-1);
                }
            }
            catch (std::exception& e) {
                ret = -1;
                DebugPrintf(SV_LOG_FATAL, "Exception thrown: %s\n", e.what());
            }
            catch (const char* e) {
                ret = -1;
                DebugPrintf(SV_LOG_FATAL, "Exception thrown: %s\n", e);

            }
            catch (...) {
                ret = -1;
                DebugPrintf(SV_LOG_FATAL, "Exception thrown\n");
            }
        }
        catch (...) {
            DebugPrintf(SV_LOG_FATAL, "Exception occurred during startup. Failed to start service. Exiting...\n");
            return ret;
        }


        LOG_EVENT((LM_DEBUG, "Service stopped"));

        DebugPrintf(">>>>>>> EXIT svagents <<<<<<<\n");

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    }
    CloseDebug();

    return(ret);
}

VxService::VxService() 
    : m_bInitialized(false),
    m_quit(0),
    m_bUninstall(false),
    m_bEnableVolumeMonitor(false),
    m_RegisterHostThread(0), 
    m_ProcessMgr(0),
    m_IsFilterDriverNotified(false),
    m_bHostMonitorStarted(false),
    m_MonitorHostThread(0),
    m_RcmWorkerThread(0),
    m_ConsistencyThread(0),
    m_bIsProtectionDisabled(false),
    m_bRestartOnDisableReplication(false),
    m_bVacpStopRequested(false),
    m_optStandalone(0)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    
    try {
        // Although this is not true, it is performed to delay the beginning of EvtCollForw, as it's
        // a low priority worker process, which shouldn't impact other important processes at startup,
        // when there would already be high utilization.
        m_lastEvtCollForwForkAttemptTime = boost::chrono::steady_clock::now();

        
    }
    catch( std::string const& e ) {
        DebugPrintf( SV_LOG_FATAL,"VxService: exception %s\n", e.c_str() );
    }
    catch( char const* e ) {
        DebugPrintf( SV_LOG_FATAL,"VxService: exception %s\n", e );
    }
    catch( exception const& e ) {
        DebugPrintf( SV_LOG_FATAL,"VxService: exception %s\n", e.what() );
    }
    catch(...) {
        DebugPrintf( SV_LOG_FATAL,"VxService: exception...\n" );
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

}

void VxService::CleanUp()
{

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try {
        StopMonitors();

#ifdef SV_WINDOWS
        ((CService*)SERVICE::instance())->getBitlockerMonitorSignal().disconnect(this);
#endif // SV_WINDOWS

        RcmConfigurator::getInstance()->GetProtectionDisabledSignal().disconnect(this);

        RcmConfigurator::getInstance()->Stop();

        if (NULL != m_RegisterHostThread)
        {
          m_RegisterHostThread->Stop();
          delete m_RegisterHostThread;
          m_RegisterHostThread = NULL;
        }

        if (NULL != m_RcmWorkerThread)
        {
            m_RcmWorkerThread->GetPrepareSwitchApplianceSignal().disconnect(this);
            m_RcmWorkerThread->GetSwitchApplianceSignal().disconnect(this);
            m_RcmWorkerThread->Stop();
            delete m_RcmWorkerThread;
            m_RcmWorkerThread = NULL;
        }

        if (NULL != m_ConsistencyThread)
        {
            m_ConsistencyThread->Stop();
            delete m_ConsistencyThread;
            m_ConsistencyThread = NULL;
        }

        StopAgents();

        m_AgentProcesses.clear();
        m_bInitialized = false;

        if (m_ProcessMgr)
        {
            m_ProcessMgr->close();
            delete m_ProcessMgr;
            m_ProcessMgr = NULL;
        }

#ifdef SV_WINDOWS
        if (m_failoverClusterConsistencyUtil)
        {
            delete m_failoverClusterConsistencyUtil;
            m_failoverClusterConsistencyUtil = NULL;
        }
#endif
        SERVICE::instance()->closeJobObjectHandle();

        if (m_bIsProtectionDisabled)
        {
            HostRecoveryManager::ResetReplicationState();
        }

        if (m_bRestartOnDisableReplication ||
            (RcmConfigurator::getInstance()->getSwitchApplianceState() ==
            SwitchAppliance::SWITCH_APPLIANCE_SUCCEEDED))
        {
            int expected = 1;
            if (!m_quit.compare_exchange_strong(expected, 0))
            {
                // if m_quit > 1 then a service quit is requested
                // if m_quit == 0 then service is stopping for internal reasons
                DebugPrintf(SV_LOG_ALWAYS, "%s: quit is expected. m_quit = %d\n", FUNCTION_NAME, expected);
            }
        }
    }
    catch(...) {
        DebugPrintf( SV_LOG_ERROR,"VxService: exception during cleanup...\n" );
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

VxService::~VxService()
{
    //   DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    LocalConfigurator localConfigurator;

    //CleanUp();
    //Bug #8065
    CDPUtil::UnInitQuitEvent();
    this->sig_handler_.remove_handler(SIGTERM);
    this->sig_handler_.remove_handler(SIGUSR1);

    if(localConfigurator.IsFilterDriverAvailable())
    {
        StopFiltering();
        if ( NULL != m_sPFilterDriverNotifier )
        {
            delete m_sPFilterDriverNotifier;
            m_sPFilterDriverNotifier = NULL;
        }
    }

    //
    // PR# 5519 svagent crash
    // changes made as part of code review:
    // VxService destructor gets called after closing
    // the local log handle. Calling debugprint now
    // would result in trying to write to closed stream
    // commented out the below call to Debugprintf
    // DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

}

//
// Parses program aguments to check whether run in Standalone mode.
//
int VxService::parse_args(int argc, ACE_TCHAR* argv[])
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    int iRet = 0;
    for (int index = 1; index < argc; index++)
    {
        if ((ACE_OS::strcasecmp(ACE_TEXT("/Standalone"), argv[index]) == 0) ||
            (ACE_OS::strcasecmp(ACE_TEXT("-Standalone"), argv[index]) == 0))
        {
            m_optStandalone = 1;
        }

        else
            iRet = (-1);
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return iRet;
}

int VxService::run_startup_code()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    try {
        DebugPrintf(SV_LOG_DEBUG, "run_startup_code 1\n");
        SERVICE::instance()->RegisterStartCallBackHandler(VxService::StartWork, this);
        SERVICE::instance()->RegisterStopCallBackHandler(VxService::StopWork, this);
        //      SERVICE::instance()->RegisterServiceEventHandler(VxService::ServiceEventHandler, this);
        DebugPrintf(SV_LOG_DEBUG, "run_startup_code 3\n");
    }
    catch (std::string const& e) {
        DebugPrintf(SV_LOG_ERROR, "run_startup_code: exception %s\n", e.c_str());
    }
    catch (char const* e) {
        DebugPrintf(SV_LOG_ERROR, "run_startup_code: exception %s\n", e);
    }
    catch (exception const& e) {
        DebugPrintf(SV_LOG_ERROR, "run_startup_code: exception %s\n", e.what());
    }
    catch (...) {
        DebugPrintf(SV_LOG_ERROR, "run_startup_code: exception...\n");
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return (SERVICE::instance()->run_startup_code());
}


void VxService::name()
{
    SERVICE::instance ()->name ( SERVICE_NAME, SERVICE_TITLE, SERVICE_DESC);
}

int VxService::ServiceEventHandler(signed long param,void *data)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try {    
        switch(param)
        {
        case MSG_DEVICE_EVENT:
            break;
        default:
            DebugPrintf( SV_LOG_ERROR,"ServiceEventHandler: Unknown event notification...\n" );
            break;

        }
    }
    catch(...) {
        DebugPrintf( SV_LOG_ERROR,"ServiceEventHandler: exception...\n" );
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return 0;
}

bool VxService::IsConfiguredBiosIdMatchingSystemBiosId(LocalConfigurator& lc, bool update)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool bRet = false;

    boost::system::error_code ec;
    std::string rcmSettingsPath = lc.getRcmSettingsPath();
    if (!boost::filesystem::exists(rcmSettingsPath, ec))
    {
        std::stringstream errMsg;
        errMsg << "RCM client settings file "
            << rcmSettingsPath
            << " does not exist. Error=" << ec.message().c_str() << '.';
        throw INMAGE_EX(errMsg.str().c_str());
    }

    std::string systemUUID = GetSystemUUID();

    RcmClientSettings rcmClientSettings(rcmSettingsPath);
    bRet = boost::iequals(rcmClientSettings.m_BiosId, systemUUID);
    if (update && !bRet)
    {
        rcmClientSettings.m_BiosId = systemUUID;
        SVSTATUS status = SVS_FALSE;
        if (SVS_OK == (status = rcmClientSettings.PersistRcmClientSettings()))
        {
            status = RcmConfigurator::getInstance()->RefreshRcmClientSettings();
        }

        bRet = (status == SVS_OK);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bRet;
}

bool VxService::UpdateConfiguredBiosId(LocalConfigurator& lc)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool bUpdateConfiguredBiosId = true;
    bool bRet = IsConfiguredBiosIdMatchingSystemBiosId(lc, bUpdateConfiguredBiosId);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bRet;
}

#ifdef SV_WINDOWS

std::string GetRecoveryCmdLogFile()
{
    LocalConfigurator lConfig;
    std::stringstream outfile;
    outfile << lConfig.getInstallPath()
        << "\\Failover\\Data\\CmdOut_" << ACE_OS::getpid()
        << "_" << GenerateUuid() << ".log";

    return outfile.str();
}

void VxService::RunRecoveryCommand()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    // Get the recovery command
    std::string strRecvryCmd = GetRecoveryScriptCmd();
    BOOST_ASSERT(!strRecvryCmd.empty());
    DebugPrintf(SV_LOG_DEBUG, "Recovery command: %s\n", strRecvryCmd.c_str());

    ACE_HANDLE hLogFile = ACE_INVALID_HANDLE;
    std::string logFile = GetRecoveryCmdLogFile();
    if (!logFile.empty())
    {
        DebugPrintf(SV_LOG_DEBUG, "Recovery command LogFile: %s\n", logFile.c_str());
        hLogFile = ACE_OS::open(logFile.c_str(), O_CREAT | O_TRUNC | O_WRONLY);
    }

    // Run the command
    ACE_Process_Options recoveryCmdOptions;
    recoveryCmdOptions.command_line("%s", strRecvryCmd.c_str());
    recoveryCmdOptions.set_handles(ACE_INVALID_HANDLE, hLogFile, hLogFile);
    if (recoveryCmdOptions.startup_info())
        recoveryCmdOptions.startup_info()->wShowWindow = SW_HIDE;

    ACE_Process recoveryProc;
    pid_t recoveryCmdPid = recoveryProc.spawn(recoveryCmdOptions);
    if (-1 == recoveryCmdPid)
        throw std::string("Could not start the recovery command. ") + strRecvryCmd;

    do
    {
        DebugPrintf(SV_LOG_DEBUG, "Process is still running.\n");

        if (m_quit)
        {
            DebugPrintf(SV_LOG_WARNING, "Terminating the process.\n");

            recoveryProc.terminate();
            recoveryProc.close_dup_handles();
            ACE_OS::close(hLogFile); hLogFile = ACE_INVALID_HANDLE;
            // Not caring about return codes as nothing to do at this stage.

            throw std::string("Service stop has initiated");
        }

        ACE_OS::sleep(2); // Sleep for 2 sec and check the process running status.

    } while (recoveryProc.running() == 1);

    //Close the handles
    recoveryProc.close_dup_handles();
    ACE_OS::close(hLogFile); hLogFile = ACE_INVALID_HANDLE;

    // Verify command exit code
    switch (recoveryProc.exit_code())
    {
        // 0 & 1 are know error codes from EsxUtil. Any other error code indicates unexpected failure.
    case 0:
    case 1:
        DebugPrintf(SV_LOG_DEBUG, "Command existed with exit code: %d\n", recoveryProc.exit_code());
        break;

    default:
        std::stringstream errStream;
        errStream << "Command " << strRecvryCmd << " execution failed. Exit code: "
            << recoveryProc.exit_code();

        throw errStream.str();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

#else // Non-Windows
void VxService::RunRecoveryCommand()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    //To do

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}
#endif


SVSTATUS VxService::CompleteRecoveryStepsForAzureToAzure(LocalConfigurator &lc)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    if (AzureVmRecoveryManager::IsRecoveryRequired())
    {
        DebugPrintf(SV_LOG_ALWAYS, "%s : Running recovery on recovered host.\n", FUNCTION_NAME);

        if (!DeleteProtectedDeviceDetails())
        {
            throw INMAGE_EX("Failed to clear protected device details file.");
        }

        AzureVmRecoveryManager::ResetReplicationState();

        // For A2A there are no recovery steps required

        DebugPrintf(SV_LOG_ALWAYS, "%s : Pausing the service until stopped on recovered host.\n", FUNCTION_NAME);

        while (!m_quit)
            ACE_OS::sleep(5);
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS VxService::CompleteRecoveryStepsForHybrid(LocalConfigurator &lc, bool& bClone)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    bool bFailoverVmDetectionIdUpdated = false;

    try
    {
        bool bHydrationWorkflow = false;

        if (!HostRecoveryManager::IsRecoveryInProgressEx(bHydrationWorkflow, bClone, CDPUtil::QuitRequested))
        {
            DebugPrintf(SV_LOG_INFO, "No recovery is in progress.\n");

            //
            // Reset the VM info, such as Hypervisor, System-UUID, if there is any
            // different in persisted and/or current system info.
            //
            HostRecoveryManager::ResetVMInfo();
        }
        else
        {
            DebugPrintf(SV_LOG_INFO, "Recovery is in progress.\n");

            if (!bClone)
            {
                DebugPrintf(SV_LOG_INFO, "Updating FailoverVmDetectionId.\n");

                lc.setFailoverVmDetectionId(lc.getHostId());
                if (!boost::iequals(lc.getFailoverVmDetectionId(), lc.getHostId()))
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: failed to update FailoverVmDetectionId. %s does not match hostId %s\n",
                        FUNCTION_NAME,
                        lc.getFailoverVmDetectionId().c_str(),
                        lc.getHostId().c_str());

                    return SVE_FAIL;
                }

                bFailoverVmDetectionIdUpdated = true;
                DebugPrintf(SV_LOG_ALWAYS, "Updated the FailoverVmDetectionId %s with HostId %s.\n",
                    lc.getFailoverVmDetectionId().c_str(),
                    lc.getHostId().c_str());

                //set the source control plane type for AVS scenario
                std::string failoverTargetType = lc.getFailoverTargetType();
                if (!failoverTargetType.empty())
                {
                    if (boost::iequals(failoverTargetType, RcmClientLib::ONPREM_TO_AZUREAVS_FAILOVER_TARGET_TYPE))
                    {
                        lc.setSourceControlPlane(RcmClientLib::RCM_AZURE_CONTROL_PLANE);
                    }
                    else if (boost::iequals(failoverTargetType, RcmClientLib::AZUREAVS_TO_ONPREM_FAILOVER_TARGET_TYPE))
                    {
                        lc.setSourceControlPlane(RcmClientLib::RCM_ONPREM_CONTROL_PLANE);
                    }
                    DebugPrintf(SV_LOG_ALWAYS, "Updated the Failover VM Source Control plane type with %s.\n",
                        lc.getSourceControlPlane().c_str());
                }
                else
                {
                    if (IsAzureStackVirtualMachine())
                    {
                        lc.setSourceControlPlane(RcmClientLib::RCM_AZURE_CONTROL_PLANE);
                        DebugPrintf(SV_LOG_ALWAYS, "Updated the Failover VM Source Control plane type with %s.\n",
                            lc.getSourceControlPlane().c_str());
                    }
                }
            }

            if (bHydrationWorkflow)
            {
                DebugPrintf(SV_LOG_INFO,
                    "This is Hydration workflow. Completing the recovery.\n");

#ifdef SV_WINDOWS
                bool isRebootRequired = false;
                HostRecoveryManager::RunDiskRecoveryWF(isRebootRequired);
#endif


                // Reset the VMInfo on recovery command completion.
                HostRecoveryManager::ResetVMInfo();
                RunRecoveryCommand();

#ifdef SV_WINDOWS
                if (isRebootRequired) {
                    DebugPrintf(SV_LOG_ALWAYS, "%s: Azure Site Recovery: Shutting down system to recover dynamic disks\n", FUNCTION_NAME);

                    if (!RebootMachine()) {
                        system("shutdown /r /t 0 /c \"Azure Site Recovery: Fixing Failover VM\"");
                    }
                }
#endif
            }
            else
            {
                if (bClone) {
                    DebugPrintf(SV_LOG_INFO,
                        "This is Clone Workflow. Completing the recovery.\n");
                }
                else {
                    DebugPrintf(SV_LOG_INFO,
                        "This is No-Hydration workflow. Completing the recovery.\n");
                }

                HostRecoveryManager::CompleteRecovery(bClone);
            }

            // Verify the recovery completion.
            if (!HostRecoveryManager::IsRecoveryInProgressEx(bHydrationWorkflow, bClone, CDPUtil::QuitRequested))
            {
                DebugPrintf(SV_LOG_ALWAYS, "%s: Recovery completed.\n", FUNCTION_NAME);
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "%s: Recovery might not be successful.\n", FUNCTION_NAME);
                status = SVS_FALSE;
            }
        }
    }
    catch (const HostRecoveryException& exp)
    {
        DebugPrintf(SV_LOG_ERROR, "Exception completing recovery. %s\n", exp.what());
        status = SVS_FALSE;
    }
    catch (const std::exception& exp)
    {
        DebugPrintf(SV_LOG_ERROR, "Exception completing recovery. %s\n", exp.what());
        status = SVS_FALSE;
    }
    catch (const std::string& expMsg)
    {
        DebugPrintf(SV_LOG_ERROR, "Exception completing recovery. %s\n", expMsg.c_str());
        status = SVS_FALSE;
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "Exception completing recovery.\n");
        status = SVS_FALSE;
    }

    // in case of recovery failure and HostId is changed, reset HostId to value before the recovery steps.
    // next time service comes up after recovery issues fixed, the FailoverVmDetectionId will be set
    // correctly and HostId gets updated.
    if ((status != SVS_OK) &&
        (bFailoverVmDetectionIdUpdated) &&
        (!boost::iequals(lc.getFailoverVmDetectionId(), lc.getHostId())))
    {
        try {
            lc.setHostId(lc.getFailoverVmDetectionId());
        }
        catch (const std::exception& exp)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Exception resetting the HostId to %s: %s. \
Manually update HostId in the drscout.conf before restarting the service.\n", lc.getHostId().c_str(), exp.what());
            status = SVS_FALSE;
        }
        catch (...)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Exception resetting the HostId to %s. \
Manually update HostId in the drscout.conf before restarting the service.\n", lc.getHostId().c_str());
            status = SVS_FALSE;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS VxService::CompleteRecoveryStepsCommon(LocalConfigurator &lc)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    bool bClone = false;
    status = CompleteRecoveryStepsForHybrid(lc, bClone);
    if (SVS_OK != status)
    {
        return status;
    }

    // do not use RcmConfigurator instance before this point to avoid
    // missing the changes in config file in the RcmConfigurator.
    // Fix RcmConfigurator/FileConfigurator to refresh in-memory config settings
    // on config file modified time though it is not an issue for now
    if (!IsConfiguredBiosIdMatchingSystemBiosId(lc))
    {
        RcmConfigurator::getInstance()->PersistDisableProtectionState();

        if (!RcmConfigurator::getInstance()->IsProtectionDisabled())
        {
            DebugPrintf(SV_LOG_ERROR, "%s: failed to persist protection disabled state. hostId %s\n",
                FUNCTION_NAME,
                lc.getHostId().c_str());
            return SVE_FAIL;
        }

        if (!UpdateConfiguredBiosId(lc))
        {
            DebugPrintf(SV_LOG_ERROR, "%s: failed to update configured biod id.\n", FUNCTION_NAME);
            return SVE_FAIL;
        }

        if (bClone)
        {
            if (RcmConfigurator::getInstance()->IsOnPremToAzureReplication())
            {
                boost::system::error_code ec;
                std::string persistedConfigFilePath = RcmConfigurator::getInstance()->GetSourceAgentConfigPath();
                if (boost::filesystem::exists(persistedConfigFilePath, ec))
                {
                    securitylib::CertHelpers::BackupFile(persistedConfigFilePath);
                    if (!boost::filesystem::remove(persistedConfigFilePath, ec))
                    {
                        DebugPrintf(SV_LOG_ERROR, "%s: failed to remove source config file %s with error %d: %s\n",
                            FUNCTION_NAME,
                            persistedConfigFilePath.c_str(),
                            ec.value(),
                            ec.message().c_str());
                        return SVE_FAIL;
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_ALWAYS, "%s: Detected cloned VM, removed source config file %s\n", FUNCTION_NAME, persistedConfigFilePath.c_str());
                    }
                }
            }
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}
//
// Throws HostRecoveryException on failure to determine if a recovery is required
// or recovery fails.
//
SVSTATUS VxService::CompleteRecoverySteps()
{
    LocalConfigurator lc;
    if (lc.IsAzureToAzureReplication())
    {
        return CompleteRecoveryStepsForAzureToAzure(lc);
    }
    else
    {
        return 	CompleteRecoveryStepsCommon(lc);
    }
}

// complete agent registration
SVSTATUS VxService::CompleteAgentRegistration()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    SVSTATUS ret = SVE_FAIL;
    RcmConfigurator::EnableVerboseLogging();
    RcmConfiguratorPtr rcmConfiguratorPtr = RcmConfigurator::getInstance();
    RcmClientPtr rcmClientPtr = rcmConfiguratorPtr->GetRcmClient();

    do {
        if (rcmConfiguratorPtr->IsOnPremToAzureReplication())
        {
            ret = rcmConfiguratorPtr->VerifyClientAuth();
            if (SVS_OK == ret)
            {
                DebugPrintf(SV_LOG_ALWAYS, "%s: Verify client auth succeeded with persisted source config.\n", FUNCTION_NAME);
            }
            else
            {
                if (SVE_FILE_NOT_FOUND == ret)
                {
                    const unsigned VERIFY_RETRY_INTERVAL_SECONDS = 120;

                    DebugPrintf(SV_LOG_ERROR, "%s: Verify client auth failed as there is no persisted source config. Waiting for %d sec before retry.\n",
                        FUNCTION_NAME,
                        VERIFY_RETRY_INTERVAL_SECONDS);
                    CDPUtil::QuitRequested(VERIFY_RETRY_INTERVAL_SECONDS);
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: Verify client auth failed with persisted source config. status = %d\n", FUNCTION_NAME, ret);
                }
                break;
            }
        }

        ret = rcmConfiguratorPtr->RegisterMachine(CDPUtil::QuitRequested);
        if (SVS_OK != ret)
        {
            if (rcmConfiguratorPtr->IsRegistrationNotSupported()) {
                DebugPrintf(SV_LOG_ERROR, "%s: RegisterMachine not supported for hostId %s failing with error %d.\n", FUNCTION_NAME, rcmConfiguratorPtr->getHostId().c_str(), rcmClientPtr->GetErrorCode());
                break;
            }
            if (rcmConfiguratorPtr->IsRegistrationNotAllowed()) {
                DebugPrintf(SV_LOG_ERROR, "%s: RegisterMachine not allowed for hostId %s failing with error %d.\n", FUNCTION_NAME, rcmConfiguratorPtr->getHostId().c_str(), rcmClientPtr->GetErrorCode());
                break;
            }
            if (rcmConfiguratorPtr->IsRegistrationFailedDueToMachinedoesNotExistError()) {
                DebugPrintf(SV_LOG_ERROR, "%s: RegisterMachine failed due to machine does not exist for hostId %s failing with error %d.\n", FUNCTION_NAME, rcmConfiguratorPtr->getHostId().c_str(), rcmClientPtr->GetErrorCode());
                break;
            }

            DebugPrintf(SV_LOG_ERROR, "%s: Failed to register machine with RCM with error %d for host-id %s.\n", FUNCTION_NAME, rcmClientPtr->GetErrorCode(), rcmConfiguratorPtr->getHostId().c_str());
        }
        else
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s: RegisterMachine succeeded for host-id %s.\n", FUNCTION_NAME, rcmConfiguratorPtr->getHostId().c_str());
            ret = rcmClientPtr->RegisterSourceAgent();
            if (SVS_OK != ret)
            {
                DebugPrintf(SV_LOG_ERROR, "%s: Failed to register source agent with RCM with error %d for host-id %s.\n", FUNCTION_NAME, rcmClientPtr->GetErrorCode(), rcmConfiguratorPtr->getHostId().c_str());
            }
            else
            {
                DebugPrintf(SV_LOG_ALWAYS, "%s: RegisterSourceAgent succeeded for host-id %s.\n", FUNCTION_NAME, rcmConfiguratorPtr->getHostId().c_str());
                break;
            }
        }
    } while (!CDPUtil::QuitRequested(300));

    RcmConfigurator::DisableVerboseLogging();

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return ret;
}

int VxService::StartWork(void *data)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    const int WAIT_SECS_FOR_QUIT = 5;
    const int REGISTER_RETRY_INTERVAL_SECONDS = 120;

#ifdef SV_UNIX
    // ACE::demonize() sets the umask to 0, so set umask here instead of main()
    umask(S_IWOTH);
#endif

    VxService *srv = NULL;
    try {
        LOG_EVENT((LM_DEBUG, "Service started"));

        srv = (VxService *)data;

        if (srv == NULL)
            return 0;

        //
        // Register signal handlers
        //
        srv->sig_handler_.register_handler(SIGTERM, srv);
        srv->sig_handler_.register_handler(SIGUSR1, srv);

        BOOST_ASSERT(srv->m_LogCutter);
        if (srv->m_LogCutter)
            srv->m_LogCutter->StartCutting();

        LocalConfigurator lc;
        int status = SV_SUCCESS;

        if (!lc.isMobilityAgent())
        {
            DebugPrintf(SV_LOG_ERROR, "%s: quitting as not starting as mobility agent.\n" , FUNCTION_NAME);
            return 0;
        }

        bool bCompleteRecoveryStepsFinished = false;
        while( !srv->m_quit)
        {
            try {

                bool bQuitEventInitialized = false;

                while (!srv->m_quit && !bQuitEventInitialized)
                {
                    {
                        ACE_Guard<ACE_Mutex> quitSignalGuard(srv->m_quitEventLock);

                        if (!srv->m_quit)
                        {
                            CDPUtil::UnInitQuitEvent();

                            bool callerCli = false;
                            bQuitEventInitialized = CDPUtil::InitQuitEvent(callerCli);
                        }
                    }

                    if (bQuitEventInitialized)
                        break;

                    DebugPrintf(SV_LOG_ERROR, "%s : Unable to Initialize QuitEvent\n", FUNCTION_NAME);
                    ACE_OS::sleep(WAIT_SECS_FOR_QUIT);
                }

                // Complete the recovery if recovery is in-progress.
                //
                if (!bCompleteRecoveryStepsFinished)
                {
                    if (srv->CompleteRecoverySteps() != SVS_OK)
                    {
                        DebugPrintf(SV_LOG_ERROR, "Recovery was not successful. " 
                                                   "Verify the recovery command errors, "
                                                   "correct the errors and start the service again.\n");
                        if (!srv->m_quit) // If already quit signaled then no need to call StopWork again
                            StopWork(data);

                        break;
                    }

                    bCompleteRecoveryStepsFinished = true;
                }

                if (RcmConfigurator::getInstance()->IsAzureToAzureReplication())
                {
                    if (RcmConfigurator::getInstance()->IsProtectionDisabled())
                    {
                        DebugPrintf(SV_LOG_ERROR, "%s : Protection is disabled by RCM. Pausing the service until stopped.\n", FUNCTION_NAME);

                        while (!srv->m_quit)
                            ACE_OS::sleep(WAIT_SECS_FOR_QUIT);

                        continue;
                    }
                }
                else if (!srv->m_quit)
                {
                    //
                    // if the cloud pairing is not complete, do the agent registration until it succeeds.
                    //

                    if (RcmConfigurator::getInstance()->IsOnPremToAzureReplication())
                    {
                        SVSTATUS status = RcmConfigurator::getInstance()->ALRRecoverySteps();
                        if (status != SVS_OK)
                        {
                            if (status == SVE_ABORT)
                            {
                                DebugPrintf(SV_LOG_ERROR,
                                    "Manual intervention required. ALR Recovery Steps failed.\n");
                                while (!srv->m_quit)
                                    ACE_OS::sleep(WAIT_SECS_FOR_QUIT);
                            }
                            else
                            {
                                ACE_OS::sleep(Migration::SleepTime);
                                continue;
                            }
                        }
                        if (!RcmConfigurator::getInstance()->IsCloudPairingComplete())
                        {
                            if (SVS_OK == srv->CompleteAgentRegistration())
                            {
                                RcmConfigurator::getInstance()->setCloudPairingStatus("complete");
                            }
                            else
                            {
                                continue;
                            }
                        }
                    }

                    if (RcmConfigurator::getInstance()->IsProtectionDisabled())
                    {
                        std::string disabledHostId = RcmConfigurator::getInstance()->GetProtectionDisabledHostId();
                        std::string currentHostId = RcmConfigurator::getInstance()->getHostId();

                        if (disabledHostId.empty())
                        {
                            DebugPrintf(SV_LOG_ERROR, "%s : Protection is disabled but inconsisten agent state. Manual intervention required. Pausing the service until stopped.\n", FUNCTION_NAME);

                            while (!srv->m_quit)
                                ACE_OS::sleep(WAIT_SECS_FOR_QUIT);

                            continue;
                        }

                        if (boost::iequals(disabledHostId, currentHostId))
                        {
                            std::string HostId = GetUuid();

                            DebugPrintf(SV_LOG_ALWAYS,
                                "Protection is disabled for hostid %s, resetting hostid to %s\n",
                                currentHostId.c_str(),
                                HostId.c_str());

                            RcmConfigurator::getInstance()->setHostId(HostId);
                        }

                        if (SVS_OK == srv->CompleteAgentRegistration())
                        {
                            RcmConfigurator::getInstance()->ClearDisableProtectionState();
                            srv->m_bRestartOnDisableReplication = false;
                            srv->m_bIsProtectionDisabled = false;
                        }
                        else
                        {
                            std::string errMsg;
                            if (RcmConfigurator::getInstance()->IsRegistrationNotSupported())
                                errMsg = "Protection not supported by RCM.Pausing the service until stopped.\n";
                            else if (RcmConfigurator::getInstance()->IsRegistrationNotAllowed()) 
                                errMsg = "Protection not allowed by RCM. Pausing the service until stopped.\n";
                            else if (RcmConfigurator::getInstance()->IsRegistrationFailedDueToMachinedoesNotExistError())
                                errMsg = "Protection not allowed by RCM becasue of machine does not exist error. Pausing the service until stopped.\n";

                            if (!errMsg.empty()) {
                                DebugPrintf(SV_LOG_ERROR, "%s : %s", FUNCTION_NAME, errMsg.c_str());
                                while (!srv->m_quit)
                                    ACE_OS::sleep(WAIT_SECS_FOR_QUIT);
                            }
                            else {
                                CDPUtil::QuitRequested(REGISTER_RETRY_INTERVAL_SECONDS);
                                continue;
                            }
                        }
                    }

                    if (RcmConfigurator::getInstance()->IsOnPremToAzureReplication())
                    {
                        const unsigned VERIFY_RETRY_INTERVAL_SECONDS = 120;

                        SwitchAppliance::State switchApplianceState =
                            RcmConfigurator::getInstance()->getSwitchApplianceState();
                        if (switchApplianceState < SwitchAppliance::VERIFY_CLIENT_SUCCEEDED)
                        {
                            SVSTATUS status = RcmConfigurator::getInstance()->VerifyClientAuth();
                            if (status == SVS_OK)
                            {
                                RcmConfigurator::getInstance()->setSwitchApplianceState(
                                    SwitchAppliance::NONE);
                            }
                            else if (switchApplianceState != SwitchAppliance::NONE)
                            {
                                RcmConfigurator::getInstance()->setSwitchApplianceState(
                                    SwitchAppliance::INCONSISTENT);
                            }
                            else
                            {                                

                                DebugPrintf(SV_LOG_ERROR, "%s: Verify client auth failed with error %d. Waiting for %d sec before retry.\n",
                                    FUNCTION_NAME,
                                    status,
                                    VERIFY_RETRY_INTERVAL_SECONDS);
                                CDPUtil::QuitRequested(VERIFY_RETRY_INTERVAL_SECONDS);
                                continue;
                            }
                        }

                        switchApplianceState =
                            RcmConfigurator::getInstance()->getSwitchApplianceState();
                        if(switchApplianceState == SwitchAppliance::INCONSISTENT)
                        {
                            DebugPrintf(SV_LOG_ERROR,
                                "Manual intervention required. Inconsistent state of switch appliance\n");
                            while (!srv->m_quit)
                                ACE_OS::sleep(WAIT_SECS_FOR_QUIT);
                        }
                        else if(switchApplianceState >= SwitchAppliance::VERIFY_CLIENT_SUCCEEDED)
                        {
                            srv->HandleSwitchApplianceState();
                            CDPUtil::QuitRequested(VERIFY_RETRY_INTERVAL_SECONDS);
                            continue;
                        }
                    }
                }

                if (SV_SUCCESS == srv->init())
                {
                    //creating jobobject for all the child process of svagents,
                    // so that when svagents crash all the child processes will be terminated
                    if (SERVICE::instance()->InitializeJobObject() != true)
                    {
                        DebugPrintf(SV_LOG_ERROR, "%s: InitializeJobObject is failed \n", FUNCTION_NAME);
                    }

                    int status = SV_SUCCESS;
                    
                    if (!srv->m_quit)
                    {
#ifdef SV_WINDOWS
                        // Trigger one manual invocation, since the callback from the monitor
                        // will only come in, if there are any changes to the bitlocker state
                        // of any volume. Ensure to place this before any start filtering would
                        // be issued to avoid resync.
                        boost::call_once(g_manualBitlockerStateUpdateOnce,
                            boost::bind(&VxService::ProcessBitLockerEvent, srv, nullptr));
#endif // SV_WINDOWS
                        // start the configurator thread
                        RcmConfigurator::getInstance()->Start(RcmClientLib::SETTINGS_SOURCE_LOCAL_CACHE_IF_RCM_NOT_AVAILABLE);

                        srv->StartRegistrationTask();

                        srv->StartRcmWorker();

                        while (!srv->m_quit
                            && RcmConfigurator::getInstance()->IsOnPremToAzureReplication()
                            && (RcmConfigurator::getInstance()->getMigrationState() == Migration::MIGRATION_SWITCHED_TO_RCM_PENDING))
                        {
                            DebugPrintf(SV_LOG_ALWAYS, "%s:  migration State= %d \n", FUNCTION_NAME, 
                                RcmConfigurator::getInstance()->getMigrationState());
                            CDPUtil::QuitRequested(Migration::RCMSettingPollSleepTime);
                        }

                        if (!srv->m_quit 
                            && RcmConfigurator::getInstance()->IsOnPremToAzureReplication()
                            && (RcmConfigurator::getInstance()->getMigrationState() == Migration::MIGRATION_SWITCHED_TO_RCM_PENDING
                                || RcmConfigurator::getInstance()->getMigrationState() == Migration::MIGRATION_ROLLBACK_PENDING))
                        {
                            DebugPrintf(SV_LOG_ERROR, "%s marking Migration to RCM rollback pending. current state=%d\n",
                                FUNCTION_NAME, RcmConfigurator::getInstance()->getMigrationState());
                            srv->PerformRollbackIfMigrationInProgress();
                            status = SVE_FAIL;
                            srv->CleanUp();
                            return 0;
                        }

                        srv->StartMonitors();
                        srv->StartConsistenyThread();

                        if (!srv->PerformSourceTasks(
                                        srv->m_FilterDrvIf.GetDriverName(VolumeSummary::DISK),
                                        lc))
                            status = SV_FAILURE;
                    }

                    if (status == SV_SUCCESS)
                    {
                        if (!srv->m_quit)
                        {
                            srv->ProcessMessageLoop();
                        }
                    }

                    srv->CleanUp();
                }
                else
                {
                    if (!srv->m_quit)
                        ACE_OS::sleep(30);
                }
            }
            catch( std::string const& e ) {
                DebugPrintf( SV_LOG_ERROR,"%s: exception %s\n", FUNCTION_NAME, e.c_str() );
                ACE_OS::sleep(60);
                srv->CleanUp();
            }
            catch( char const* e ) {
                DebugPrintf( SV_LOG_ERROR,"%s: exception %s\n", FUNCTION_NAME, e );
                ACE_OS::sleep(60);
                srv->CleanUp();
            }
            catch( exception const& e ) {
                DebugPrintf( SV_LOG_ERROR,"%s: exception %s\n", FUNCTION_NAME, e.what() );
                ACE_OS::sleep(60);
                srv->CleanUp();
            }
            catch(...) {
                DebugPrintf( SV_LOG_ERROR,"%s: exception...\n", FUNCTION_NAME);
                ACE_OS::sleep(60);
                srv->CleanUp();
            }
        }
    }
    catch (const HostRecoveryException& exp)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s: FAILED: Recovery exception while starting service. Exception Message: %s\n",
            FUNCTION_NAME,
            exp.what());
    }
    catch(...)
    {
        DebugPrintf(SV_LOG_ERROR,"%s FAILED: An error occurred while starting service.\n", FUNCTION_NAME);
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return 0;
}

SwitchAppliance::State VxService::HandleSwitchApplianceState()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    RcmConfiguratorPtr rcmConfiguratorPtr = RcmConfigurator::getInstance();
    RcmClientPtr rcmClientPtr = rcmConfiguratorPtr->GetRcmClient();
    SwitchAppliance::State switchApplianceState = rcmConfiguratorPtr->getSwitchApplianceState();
    static boost::chrono::system_clock::time_point prepSASucceedStart;

    DebugPrintf(SV_LOG_DEBUG, "Current Switch Appliance state : %d\n", switchApplianceState);

    switch(switchApplianceState) {

    case SwitchAppliance::PREPARE_SWITCH_APPLIANCE_START:
        StopAgents();
        DebugPrintf(SV_LOG_ALWAYS, "Changing Switch Appliance state to : %d\n",
            SwitchAppliance::PREPARE_SWITCH_APPLIANCE_SUCCEEDED);
        rcmConfiguratorPtr->setSwitchApplianceState(
            SwitchAppliance::PREPARE_SWITCH_APPLIANCE_SUCCEEDED);
        prepSASucceedStart = boost::chrono::system_clock::now();
    case SwitchAppliance::PREPARE_SWITCH_APPLIANCE_SUCCEEDED:
    {
        boost::chrono::system_clock::time_point prepSASucceedCurr =
            boost::chrono::system_clock::now();
        if (prepSASucceedCurr > prepSASucceedStart +
            boost::chrono::minutes(SwitchAppliance::JobWaitTimeOut))
        {
            DebugPrintf(SV_LOG_ERROR,
                "%s : Prepare switch appliance succeeded "
                "but no switch appliance job received for last %d mins.\n",
                FUNCTION_NAME, SwitchAppliance::JobWaitTimeOut);
            DebugPrintf(SV_LOG_ALWAYS, "Changing Switch Appliance state to : %d\n",
                SwitchAppliance::NONE);
            rcmConfiguratorPtr->setSwitchApplianceState(SwitchAppliance::NONE);
        }
    }
        break;
    case SwitchAppliance::SWITCH_APPLIANCE_START:
        m_quit++;
        CDPUtil::SignalQuit();
        rcmConfiguratorPtr->ClearCachedReplicationSettings();
        // switch appliance will take care of marking the state of SwitchAppliance
        rcmConfiguratorPtr->SwitchAppliance(m_switchApplianceSettings);
        break;

    case SwitchAppliance::VERIFY_CLIENT_FAILED:
        status = rcmConfiguratorPtr->VerifyClientAuth();
        if (status == SVS_OK)
        {
            DebugPrintf(SV_LOG_ALWAYS, "Changing Switch Appliance state to : %d\n",
                SwitchAppliance::NONE);
            rcmConfiguratorPtr->setSwitchApplianceState(SwitchAppliance::NONE);
        }
        else
        {
            DebugPrintf(SV_LOG_ALWAYS, "Changing Switch Appliance state to : %d\n",
                SwitchAppliance::INCONSISTENT);
            rcmConfiguratorPtr->setSwitchApplianceState(SwitchAppliance::INCONSISTENT);
        }
        break;

    case SwitchAppliance::VERIFY_CLIENT_SUCCEEDED:
    case SwitchAppliance::REGISTER_SOURCE_AGENT_FAILED:
        status = rcmClientPtr->RegisterSourceAgent();
        if (status == SVS_OK)
        {
            DebugPrintf(SV_LOG_ALWAYS, "Changing Switch Appliance state to : %d\n",
                SwitchAppliance::REGISTER_SOURCE_AGENT_SUCCEEDED);
            rcmConfiguratorPtr->setSwitchApplianceState(
                SwitchAppliance::REGISTER_SOURCE_AGENT_SUCCEEDED);
        }
        else
            break;

    case SwitchAppliance::REGISTER_SOURCE_AGENT_SUCCEEDED:
    case SwitchAppliance::GET_NON_CACHED_SETTINGS_FAILED:
        status = rcmConfiguratorPtr->GetNonCachedSettings();

        if (status == SVS_OK)
        {
            DebugPrintf(SV_LOG_ALWAYS, "Changing Switch Appliance state to : %d\n",
                SwitchAppliance::SWITCH_APPLIANCE_SUCCEEDED);
            rcmConfiguratorPtr->setSwitchApplianceState(
                SwitchAppliance::SWITCH_APPLIANCE_SUCCEEDED);
        }
        else
            break;
    }
    switchApplianceState = rcmConfiguratorPtr->getSwitchApplianceState();
    DebugPrintf(SV_LOG_DEBUG, "Final Switch Appliance state : %d\n", switchApplianceState);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return switchApplianceState;
}

void VxService::ProcessMessageLoop()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    LocalConfigurator lc;

    while( !this->m_quit)
    {
        try 
        {
            if ( !isInitialized())
                this->init();

            if ( isInitialized() )
            {
                SwitchAppliance::State switchApplianceState = HandleSwitchApplianceState();
                if (switchApplianceState == SwitchAppliance::INCONSISTENT)
                    ACE_OS::sleep(SwitchAppliance::WaitTime);
                else if (switchApplianceState >= SwitchAppliance::PREPARE_SWITCH_APPLIANCE_START &&
                    switchApplianceState <= SwitchAppliance::GET_NON_CACHED_SETTINGS_FAILED)
                {
                    ACE_OS::sleep(SwitchAppliance::WaitTime);
                    continue;
                }

                if (!m_bIsProtectionDisabled)
                {
                    //Start sentinel only if filter driver was notified
                    if (m_IsFilterDriverNotified && lc.getSentinalStartStatus())
                    {
                        StartDataProtection(lc);

                        StartSentinel(lc);

                        StartVacp(lc);
                    }
                }
                else
                {
                    StopReplicationOnProtectionDisabled();
                    return;
                }

                StartEvtCollForw(lc);
            }

            DebugPrintf(SV_LOG_DEBUG,"Waiting for agent(s) \n" );

            ACE_Time_Value timeout;
            timeout.set(SETTINGS_POLLING_INTERVAL,0);

            if( WAIT_FOR_STOP_AGENTS_FOR_OFFLINE_RESOURCE == WaitForMultipleAgents(timeout) ) {
                StopAgents();
                //WaitForMultipleAgents(ACE_Time_Value::max_time);
                ((CService*)SERVICE::instance())->ResetStopAgentsForOfflineResource();
            } 

            if( 0 == m_AgentProcesses.size() ) 
            {
                for( int i = 0; i < SETTINGS_POLLING_INTERVAL && !m_quit; ++i ) 
                {
                    ACE_OS::sleep( 1 );
                }
            }
        }
        catch( std::string const& e ) 
        {
            DebugPrintf( SV_LOG_ERROR,"%s:  caught exception: %s\n", FUNCTION_NAME, e.c_str() );
            throw e;
        }   
        catch( char const* e ) 
        {
            DebugPrintf( SV_LOG_ERROR,"%s: caught exception: %s\n", FUNCTION_NAME, e );
            throw e;
        }
        catch(...) 
        {
            DebugPrintf( SV_LOG_ERROR,"%s: caught exception:...\n", FUNCTION_NAME);
            throw;
        }
    }  
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

}

int VxService::WaitForMultipleAgents(const ACE_Time_Value &timeout)
{
    ACE_Time_Value minWait;

    int count;

    // NOTE: this is broken up into 1 second intervals because
    // if a cluster volume goes offline we need to respond very quickly 
    // to limit our impact on a physical disk resource going offline
    // however if the timeout is max_wait, then don't break it up
    // do the max wait as that will wait for all agents to exit, which 
    // is what needs to be done for StopAgentsForOfflineResource for anyway
    // (i.e. when returning from a max wait all agents will be stopped so no 
    // need to stop them)
    if (ACE_Time_Value::max_time == timeout) {       
        count = 1;
        minWait = timeout;
    } else {
        // for now we round msec up to 1 second
        count = static_cast<int>(timeout.sec()) + (timeout.msec() > 0 ? 1 : 0);    
        minWait.set(1,0);        
    }

    for (int i = 0; i < count; ++i) {
        if (-1 == m_ProcessMgr->wait(minWait)) {
            DebugPrintf(SV_LOG_WARNING,"WARNING: Wait for multiple agents failed. @LINE %d in FILE: %s\n",LINE_NO,FUNCTION_NAME);
            return WAIT_FOR_ERROR;
        }

        if (minWait != ACE_Time_Value::max_time && ((CService*)SERVICE::instance())->StopAgentsForOfflineResource()) {
            return WAIT_FOR_STOP_AGENTS_FOR_OFFLINE_RESOURCE;
        }
    }    

    return WAIT_FOR_SUCCESS;
}

int VxService::StopWork(void *data)
{
    //
    // Note: this method called from a signal context; don't use glibc functions and debug printf
    //
    VxService *vs = (VxService *)data;

    if (vs == NULL)
        return 0;

    ACE_Guard<ACE_Mutex> quitSignalGuard(vs->m_quitEventLock);

    vs->m_quit++;
    quit_signalled = true;
    CDPUtil::SignalQuit();
    return 0;
}

int VxService::handle_exit (ACE_Process *proc)
{
    // Do not call LOG_EVENT in this function. 
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    pid_t pid = proc->getpid();
    AgentProcess::Ptr agentInfo;
    AgentProcesses_t::const_iterator iter = this->m_AgentProcesses.begin();
    AgentProcesses_t::const_iterator endIter = this->m_AgentProcesses.end();

    for( /* empty */ ; (iter != endIter); iter++ ) 
    {
        if( (*iter)->ProcessCreated() && pid == (*iter)->ProcessHandle())
        {
            if(m_quit)
            {
                (*iter)->markDead();
                DebugPrintf(SV_LOG_ALWAYS, "process with pid %d terminated. marking the Agent Process as dead.\n", pid); // TODO: make SV_LOG_DEBUG;
                m_AgentProcesses.erase(*iter);
                break;
            }
            else
            {
                SV_LOG_LEVEL logLevel = SV_LOG_ALWAYS;
                if (proc->exit_code() != 0)
                {
                    logLevel = SV_LOG_ERROR;
                }
                DebugPrintf(logLevel, "process %s with pid %d exited with exit code %d.\n", (*iter)->DeviceName().c_str(), pid, proc->exit_code());
                m_AgentProcesses.erase(*iter);
                break;
            }
        }        
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return 0;
}

bool VxService::IsUninstallSignalled() const
{
    return m_bUninstall;
}

int VxService::handle_signal(int signum, siginfo_t *si, ucontext_t * uc) 
{
    /* Do not call LOG_EVENT or any glibc functions or debug printf;
    * We are in a signal context */
    int iRet = -1;

    if ( (SIGTERM == signum) || ( SIGUSR1 == signum))
    {
        if ( SIGUSR1 == signum )
        {
            this->m_bUninstall = true;
        }
        this->StopWork(this);
        iRet = 0;
    }

    return iRet;
}

void VxService::StopAgents()
{   
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    ACE_Time_Value time_value;
    time_value.set(AGENTSTIME_TOQUIT_INSECS, 0);
    try 
    {
        while (!m_AgentProcesses.empty())
        { 
            AgentProcesses_t::const_iterator iter = m_AgentProcesses.begin();
            for( /* empty */ ; iter != m_AgentProcesses.end(); iter++ ) 
            {        
                if((*iter)->IsAlive())
                {
                    int iRet = (*iter)->PostQuitMessage();
                    if (-1 == iRet)
                    {
                        DebugPrintf(SV_LOG_DEBUG,"PostQuitMessage Failed...\n");
                    }
                }
            }
            DebugPrintf(SV_LOG_DEBUG,"Waiting for agents to exit\n");

            DebugPrintf(SV_LOG_DEBUG,"Number of Agent Processes = %d. Waiting till all of them exit.\n",m_AgentProcesses.size());

            if(WAIT_FOR_ERROR == WaitForMultipleAgents(time_value))
            {
                DebugPrintf(SV_LOG_ERROR, "svagents was trying to wait for all the remaining processes. But, the wait failed. Terminating the processes by issueing the the term signal.\n");
                for(iter = m_AgentProcesses.begin(); iter != m_AgentProcesses.end(); iter++)
                {
                    if((*iter)->IsAlive())
                    {
                        DebugPrintf(SV_LOG_ERROR, "The process with PID: %d is not responding to the stop. Terminating it.\n", (*iter)->ProcessHandle());
                        if(0 != m_ProcessMgr->terminate((*iter)->ProcessHandle()))
                        {
                            DebugPrintf(SV_LOG_ERROR, "Agent with PID: %d cannot be terminated.\n", (*iter)->ProcessHandle());
                        }
                    }
                }
                break;
            }
        }
    }
    catch( std::string const& e ) {
        DebugPrintf( SV_LOG_ERROR,"VxService::StopAgents caught exception: %s\n", e.c_str() );
    }   
    catch( char const* e ) {
        DebugPrintf( SV_LOG_ERROR,"VxService::StopAgents caught exception: %s\n", e );
    }
    catch( exception const& e ) {
        DebugPrintf( SV_LOG_ERROR,"VxService::StopAgents caught exception: %s\n", e.what() );
    }
    catch(...) {
        DebugPrintf( SV_LOG_ERROR,"VxService::StopAgents caught exception:...\n" );        
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

}

bool StartProcess(ACE_Process_Manager *pm, AgentProcess::Ptr agentInfoPtr, const char * exePath, const char * args)
{
#ifndef SV_WINDOWS
  boost::mutex::scoped_lock guard(g_MutexForkProcess);
#endif
  return (agentInfoPtr->Start(pm, exePath, args));
}


///
/// Starts the consistency process.
/// At most one consistency process running at any time.
///
void VxService::StartVacp(const LocalConfigurator &lc)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    
    using namespace boost::chrono;

    ACE_Guard<ACE_Mutex> SentinelGuard(m_vacpLock);

    try {

        AgentProcess::Ptr agentInfoPtr(new AgentProcess(InMageVacpName, InMageVacpId));

        AgentProcesses_t::iterator findIter = m_AgentProcesses.find(agentInfoPtr);
        const bool vacpRunning = (findIter != m_AgentProcesses.end());

        if (RcmConfigurator::getInstance()->IsProtectionDisabled() || m_bVacpStopRequested)
        {
            if (vacpRunning)
            {
                DebugPrintf(SV_LOG_ALWAYS,
                    "%s: stopping vacp as %s.\n",
                    FUNCTION_NAME,
                    m_bVacpStopRequested? "stop is requested" : "protection is disabled");
                StopAgentsHavingAttributes(InMageVacpName, std::string(), InMageVacpId, lc.getVacpExitWaitTime());
            }

            return;
        }

        if (!IsSentinelRunning())
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s: not starting vacp as s2 is not running.\n", FUNCTION_NAME);
            return;
        }

        std::string vacpArgs, masterNodeFingerPrint;

        // fetch the cached settings
        AgentSettings agentSettings;
        SVSTATUS ret = RcmConfigurator::getInstance()->GetAgentSettings(agentSettings);
        if (ret != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Failed to fetch cached consistency settings.\n", FUNCTION_NAME);
            return;
        }

        uint64_t crashConsistencyInterval = agentSettings.m_consistencySettings.CrashConsistencyInterval;
        crashConsistencyInterval /= 10 * 1000 * 1000;

        uint64_t appConsistencyInterval = agentSettings.m_consistencySettings.AppConsistencyInterval;
        appConsistencyInterval /= 10 * 1000 * 1000;

        // if both app and crash consistency intervals are 0, do not start VACP
        if (!crashConsistencyInterval && !appConsistencyInterval)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s : Consistency intervals are 0.\n", FUNCTION_NAME);
            if (vacpRunning)
            {
                DebugPrintf(SV_LOG_ALWAYS, "%s: stopping VACP as consistency intervals are 0.\n", FUNCTION_NAME);
                StopAgentsHavingAttributes(InMageVacpName, std::string(), InMageVacpId, lc.getVacpExitWaitTime());
            }
            return;
        }

        bool isSharedDiskCluster = false;
#ifdef SV_WINDOWS
        isSharedDiskCluster = (agentSettings.m_properties.find(FailvoerClusterMemberType::IsClusterMember) != agentSettings.m_properties.end());
#endif

        // Verify restart required on settings change
        std::string serializedSettings = JSON::producer<ConsistencySettings>::convert(agentSettings.m_consistencySettings);
        std::string checksum = securitylib::genSha256Mac(serializedSettings.c_str(), serializedSettings.length());

        // BUG 6582380 - svagents killed by SIGBUS
        // there is a race condition in svagents calling VACP to quit and VACP also exiting at the same time.
        // the ACE_Events are memory mapped files on Linux and svagents maps the event in memory to send quit signal.
        // but if VACP has exited while the event still mapped in memory of svagents, in a very narrow time window,
        // a SIGBUS is signalled when the mapped memory is not accessible as event is deleted.
        // adding additional wait time before stopping VACP from service.  If VACP has not quit by then, service will stop it
        // with a message 'max run time elapsed' and you should check why that is the case
        const seconds maxRunTime = seconds(lc.getVacpParallelMaxRunTime()) + seconds(lc.getVacpExitWaitTime());
        const bool restartRequiredForSettingsChange = !boost::iequals(checksum, m_prevConsSetChecksum);
        const bool restartRequiredAsMaxRunTimeElapsed = (steady_clock::now() > m_lastVacpStartTime + maxRunTime);

        // if vacp is already running and restart is not required, nothing to do
        if (vacpRunning && !(restartRequiredForSettingsChange || restartRequiredAsMaxRunTimeElapsed))
            return;

        if (vacpRunning) // restart already running vacp by stopping it
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s: stopping vacp on %s \n", FUNCTION_NAME,
                restartRequiredForSettingsChange ? "consistency settings change" : "max run time elapsed");
            StopAgentsHavingAttributes(InMageVacpName, std::string(), InMageVacpId, lc.getVacpExitWaitTime());
        }
        else
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s: vacp not running\n", FUNCTION_NAME);
        }

        // Construct arguments
        vacpArgs = " -systemlevel -cc -parallel ";

#ifndef _WIN32
        if (boost::iequals(RcmConfigurator::getInstance()->getCSType(), CSTYPE_CSPRIME))
        {
            vacpArgs += "-prescript ";
            vacpArgs += lc.getInstallPath();
            vacpArgs += ACE_DIRECTORY_SEPARATOR_CHAR;
            vacpArgs += "scripts";
            vacpArgs += ACE_DIRECTORY_SEPARATOR_CHAR;
            vacpArgs += "vCon";
            vacpArgs += ACE_DIRECTORY_SEPARATOR_CHAR;
            vacpArgs += "AzureDiscovery.sh ";
        }
#endif
        std::string coordinatorMachine;
        std::vector<MultiVmConsistencyParticipatingNode> participatingNodes;

        do
        {
#ifdef SV_WINDOWS
            if (isSharedDiskCluster)
            {
                SVSTATUS status = m_failoverClusterConsistencyUtil->GetVacpMultiVMStartUpParameters(coordinatorMachine,
                    participatingNodes, agentSettings);
                if (status != SVS_OK)
                {
                    std::string errMsg = m_failoverClusterConsistencyUtil->GetErrorMessage();
                    DebugPrintf(SV_LOG_ERROR, "%s: vacp startup parameter fetch failed for failover cluster with errMsg=%s, skipping vacp restart. \n",
                        FUNCTION_NAME, errMsg.c_str());
                    return;
                }
                break;
            }
#endif
            coordinatorMachine = agentSettings.m_consistencySettings.CoordinatorMachine;
            participatingNodes = agentSettings.m_consistencySettings.MultiVmConsistencyParticipatingNodes;

        } while (false);

        if (participatingNodes.size() > 1)
        {                

            vacpArgs += " -distributed -mn ";
            vacpArgs += coordinatorMachine;
            vacpArgs += " -cn \"";

            std::vector<MultiVmConsistencyParticipatingNode>::const_iterator itr =
                participatingNodes.begin();
            std::vector<MultiVmConsistencyParticipatingNode>::const_iterator itrEnd =
                participatingNodes.end();

            std::string coordNodes;
            for (/*empty*/; itr != itrEnd; itr++)
            {
                if (boost::iequals(itr->Name, coordinatorMachine))
                {
                    masterNodeFingerPrint = itr->CertificateThumbprint;
                }
                else
                {
                    if (!coordNodes.empty())
                    {
                        coordNodes += ",";
                    }

                    coordNodes += itr->Name;
                }
            }
            coordNodes += "\"";
            vacpArgs += coordNodes;
        }

        if (isSharedDiskCluster)
        {
            vacpArgs += " -cluster ";
        }

        // update vacp.conf
        std::string confDir;
        lc.getConfigDirname(confDir);

        boost::filesystem::path vacpConfPath(confDir);
        vacpConfPath /= "vacp.conf";

        std::string lockFile(vacpConfPath.string());
        lockFile += ".lck";
        if (!boost::filesystem::exists(lockFile)) {
            std::ofstream tmpLockFile(lockFile.c_str());
        }
        boost::interprocess::file_lock fileLock(lockFile.c_str());
        boost::interprocess::scoped_lock<boost::interprocess::file_lock> fileLockGuard(fileLock);

        std::ofstream ofstr(vacpConfPath.string().c_str(), std::ofstream::trunc);
        std::stringstream ss;
        ss << VACP_CONF_PARAM_CRASH_CONSISTENCY_INTERVAL << "=" << crashConsistencyInterval << std::endl;
        ss << VACP_CONF_PARAM_APP_CONSISTENCY_INTERVAL << "=" << appConsistencyInterval << std::endl;
            
        if (!masterNodeFingerPrint.empty())
            ss << VACP_CONF_PARAM_MASTER_NODE_FINGERPRINT << "=" << masterNodeFingerPrint << std::endl;

        ss << VACP_CONF_PARAM_RCM_SETTINGS_PATH << "=" << lc.getRcmSettingsPath() << std::endl;
        ss << VACP_CONF_PARAM_PROXY_SETTINGS_PATH << "=" << lc.getProxySettingsPath() << std::endl;
        ss << VACP_CONF_PARAM_EXIT_TIME << "=" << lc.getVacpParallelMaxRunTime() << std::endl;

        ss << VACP_CONF_PARAM_DRAIN_BARRIER_TIMEOUT << "=" << lc.getVacpDrainBarrierTimeout() << std::endl;
        ss << VACP_CONF_PARAM_TAG_COMMIT_MAX_TIMEOUT << "=" << lc.getVacpTagCommitMaxTimeOut() << std::endl;
        long appConsistencyServerResymeAckWaitTime = lc.getVacpDrainBarrierTimeout() + VACP_SERVER_RESUMEACK_TIMEDELTA;
        ss << VACP_CONF_PARAM_APPCONSISTENCY_SERVER_RESUMEACK_WAITTIME << "=" << appConsistencyServerResymeAckWaitTime << std::endl;

        std::string    excludedVolumes = lc.getApplicationConsistentExcludedVolumes();
        if (!excludedVolumes.empty()) {
            ss << VACP_CONF_EXCLUDED_VOLUMES << "=" << excludedVolumes << std::endl;
        }

        if (isSharedDiskCluster)
        {
            ss << VACP_CLUSTER_HOSTID_NODE_MAPPING << "=";
            std::string hostId;
            std::vector<std::string> hostnameSplits;
            std::string hostname;
            std::vector<MultiVmConsistencyParticipatingNode>::const_iterator multiVmNodesItr =
                participatingNodes.begin();

            for (/**/;
                multiVmNodesItr != participatingNodes.end();
                multiVmNodesItr++)
            {
                hostId = (*multiVmNodesItr).Id;
                hostname = (*multiVmNodesItr).Name;
                if (!hostId.empty()
                    && !hostname.empty())
                {
                    boost::split(hostnameSplits, hostname, boost::is_any_of("."));
                    if (hostnameSplits.size() > 0)
                    {
                        ss << hostnameSplits[0] << ":" << hostId << ";";
                    }
                }
            }
            ss << std::endl;
        }

        DebugPrintf(SV_LOG_ALWAYS, "%s: Adding consistency policy input to %s: %s \n", FUNCTION_NAME, vacpConfPath.string().c_str(), ss.str().c_str());
        ofstr << ss.str().c_str();
        ofstr.close();

        const std::string vacpCmd = (boost::filesystem::path(lc.getInstallPath()) / INM_VACP_SUBPATH).string();

        DebugPrintf(SV_LOG_ALWAYS, "%s: forking vacp %s %s\n", FUNCTION_NAME, vacpCmd.c_str(), vacpArgs.c_str());

        if (!StartProcess(m_ProcessMgr, agentInfoPtr, vacpCmd.c_str(), vacpArgs.c_str()))
        {
            DebugPrintf(SV_LOG_ERROR, "FAILED %s: vacp did not start. It will be retried.\n", FUNCTION_NAME);
        }
        else {
            DebugPrintf(SV_LOG_ALWAYS, "%s: vacp successfully started, processid=%ld\n", FUNCTION_NAME, agentInfoPtr->ProcessHandle());
            m_AgentProcesses.insert(agentInfoPtr);

            // save the settings checksum only on success
            m_prevConsSetChecksum = checksum;
            m_lastVacpStartTime = steady_clock::now();
        }
    }
    catch (std::string const& e) {
        DebugPrintf(SV_LOG_ERROR, "%s: failed with exception %s\n", FUNCTION_NAME, e.c_str());
    }
    catch (char const* e) {
        DebugPrintf(SV_LOG_ERROR, "%s: failed with exception %s\n", FUNCTION_NAME, e);
    }
    catch (exception const& e) {
        DebugPrintf(SV_LOG_ERROR, "%s: failed with exception %s\n", FUNCTION_NAME, e.what());
    }
    catch (...) {
        DebugPrintf(SV_LOG_ERROR, "%s: failed with unknown exception...\n", FUNCTION_NAME);
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

}

///
/// Starts the sentinel process, cleaning up any old sentinel handles if necessary.
/// At most one sentinel process running at any time.
///
void VxService::StartSentinel(LocalConfigurator &lc)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    ACE_Guard<ACE_Mutex> SentinelGuard(m_sentinelLock);

    try {         
        std::stringstream args;

        args << " svagents";

        AgentProcess::Ptr agentInfoPtr(new AgentProcess(InMageSentinelName, InMageSentinelId)); // InMageSentinel is id for the sentinel 

        AgentProcesses_t::iterator findIter = m_AgentProcesses.find(agentInfoPtr);

        if (findIter == m_AgentProcesses.end()) 
        {
            if (!StartProcess(m_ProcessMgr, agentInfoPtr, lc.getDiffSourceExePathname().c_str(), args.str().c_str())) {
                DebugPrintf(SV_LOG_ERROR, "%s : sentinel did not start\n", FUNCTION_NAME);
            }
            else {
                DebugPrintf(SV_LOG_ALWAYS, "%s: sentinel (s2) successfully started, processid=%ld\n", FUNCTION_NAME, agentInfoPtr->ProcessHandle());

                m_AgentProcesses.insert(agentInfoPtr);
            }
        }
        else
        {
            DebugPrintf( SV_LOG_INFO, "%s: sentinel is running with pid %d.\n", FUNCTION_NAME,(*findIter)->ProcessHandle());
        }
    }
    catch( std::string const& e ) {
        DebugPrintf( SV_LOG_ERROR,"%s: exception %s\n", FUNCTION_NAME, e.c_str() );
    }
    catch( char const* e ) {
        DebugPrintf( SV_LOG_ERROR,"%s: exception %s\n", FUNCTION_NAME, e );
    }
    catch( exception const& e ) {
        DebugPrintf( SV_LOG_ERROR,"%s: exception %s\n", FUNCTION_NAME, e.what() );
    }
    catch(...) {
        DebugPrintf( SV_LOG_ERROR,"%s: exception...\n", FUNCTION_NAME);
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

}


///
/// Starts the evtcollforw process, cleaning up any old evtcollforw handles if necessary.
/// At most one evtcollforw process running at any time.
///
void VxService::StartEvtCollForw(LocalConfigurator &lc)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    try {
        using namespace boost::chrono;
        steady_clock::time_point currTime = steady_clock::now();
        steady_clock::duration retryInterval = seconds(lc.getEvtCollForwProcSpawnInterval());
        if (currTime < m_lastEvtCollForwForkAttemptTime + retryInterval)
        {
            DebugPrintf(SV_LOG_INFO,
                "EvtCollForw was started (or at least attempted) within the last retry interval : %lld.\n",
                m_lastEvtCollForwForkAttemptTime.time_since_epoch().count());
            goto Cleanup;
        }
        m_lastEvtCollForwForkAttemptTime = currTime;

        // log unknown env only once
        static bool checkedEnvironment = false;

        std::stringstream args;

        if (lc.isMobilityAgent())
        {
            args << ' ' << EvtCollForw_CmdOpt_Environment << ' ';

            if (RcmConfigurator::getInstance()->IsAzureToAzureReplication())
            {
                args << EvtCollForw_Env_A2ASource;
            }
            else if (RcmConfigurator::getInstance()->IsOnPremToAzureReplication())
            {
                args << EvtCollForw_Env_V2ARcmSource;
            }
            else if (RcmConfigurator::getInstance()->IsAzureToOnPremReplication())
            {
                    args << EvtCollForw_Env_V2ARcmSourceOnAzure;
            }
            else
            {
                if (!checkedEnvironment)
                {
                    DebugPrintf(SV_LOG_ERROR,
                        "%s: Not starting in unknow environment.\n",
                        FUNCTION_NAME);

                    checkedEnvironment = true;
                }
            }
        }

        AgentProcess::Ptr agentInfoPtr(new AgentProcess(InMageEvtCollForwName, InMageEvtCollForwId));

        AgentProcesses_t::iterator findIter = m_AgentProcesses.find(agentInfoPtr);

        if (findIter == m_AgentProcesses.end())
        {
            const char* EvtCollForwSubPath =
#ifdef SV_WINDOWS
                "evtcollforw.exe";
#else
                "bin/evtcollforw";
#endif

            // TODO-SanKumar-1704: Add this value to the configuration.
            const std::string evtCollForwExeName =
                (boost::filesystem::path(lc.getInstallPath()) / EvtCollForwSubPath).string();

            if (!StartProcess(m_ProcessMgr, agentInfoPtr, evtCollForwExeName.c_str(), args.str().c_str())) {
                DebugPrintf(SV_LOG_ERROR, "FAILED StartEvtCollForw: evtcollforw did not start.\n");
            }
            else {
                m_AgentProcesses.insert(agentInfoPtr);
            }
        }
        else
        {
            DebugPrintf(SV_LOG_INFO,
                "StartEvtCollForw: evtcollforw is running with pid %d.\n", (*findIter)->ProcessHandle());
        }
    }
    catch (std::string const& e) {
        DebugPrintf(SV_LOG_ERROR, "StartEvtCollForw: exception %s\n", e.c_str());
    }
    catch (char const* e) {
        DebugPrintf(SV_LOG_ERROR, "StartEvtCollForw: exception %s\n", e);
    }
    catch (exception const& e) {
        DebugPrintf(SV_LOG_ERROR, "StartEvtCollForw: exception %s\n", e.what());
    }
    catch (...) {
        DebugPrintf(SV_LOG_ERROR, "StartEvtCollForw: exception...\n");
    }

Cleanup:
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

}


// starts data protection of IR or resync
void VxService::StartDataProtection(LocalConfigurator &lc)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try 
    {
        boost::system::error_code ec;
        AgentSettings settings;

        SVSTATUS ret = RcmConfigurator::getInstance()->GetAgentSettings(settings);
        if (ret != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Failed to fetch cached replication settings: %d\n", FUNCTION_NAME, ret);
            return;
        }

        ACE_Guard<ACE_Mutex> resyncBatchGuard(m_ResyncBatchLock);

        // Iterate through sync settings and prepare set of disk ID.
        // Also prepare map of resync priority DiskIds.
        std::set<std::string> setSyncDiskIds;
        
        typedef std::multimap<RcmClientLib::ResyncProgressType::ResyncTypePriority, std::string> MapPrioritySyncSettings;
        MapPrioritySyncSettings mapPrioritySyncSettings;

        const boost::posix_time::time_duration currentTime = boost::posix_time::second_clock::local_time().time_of_day();
        const bool bCanScheduleNow = ((currentTime.hours() < settings.m_autoResyncTimeWindow.StartHour) ||
            ((currentTime.hours() == settings.m_autoResyncTimeWindow.StartHour) && (currentTime.minutes() < settings.m_autoResyncTimeWindow.StartMinute))) ||
            ((currentTime.hours() > settings.m_autoResyncTimeWindow.EndHour) ||
            ((currentTime.hours() == settings.m_autoResyncTimeWindow.EndHour) && (currentTime.minutes() > settings.m_autoResyncTimeWindow.EndMinute))) ? false : true;
        
        std::vector<SyncSettings>::const_iterator syncSettingsItr = settings.m_syncSettings.begin();
        for (/*empty*/; syncSettingsItr != settings.m_syncSettings.end(); syncSettingsItr++)
        {
            setSyncDiskIds.insert(syncSettingsItr->DiskId);

            RcmClientLib::ResyncProgressType::ResyncTypePriority resyncTypePriority;
            if(boost::iequals(syncSettingsItr->ResyncProgressType, RcmClientLib::ResyncProgressType::ManualResync))
            {
                resyncTypePriority = RcmClientLib::ResyncProgressType::PriorityManualResyncType;
            }
            else if (boost::iequals(syncSettingsItr->ResyncProgressType, RcmClientLib::ResyncProgressType::InitialReplication))
            {
                resyncTypePriority = RcmClientLib::ResyncProgressType::PriorityInitialReplicationType;
            }
            else if (boost::iequals(syncSettingsItr->ResyncProgressType, RcmClientLib::ResyncProgressType::AutoResync))
            {
                resyncTypePriority = RcmClientLib::ResyncProgressType::PriorityAutoResyncType;

                if (!bCanScheduleNow)
                {
                    // If schedule time for auto resync is in future then skip adding this DiskId.
                    // This way resync batch will have only DiskId's which are immdediately scheduled.
                    continue;
                }
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "%s: Invalid ResyncProgressType %s for DiskId %s\n",
                    FUNCTION_NAME, syncSettingsItr->ResyncProgressType.c_str(), syncSettingsItr->DiskId.c_str());
            }

            mapPrioritySyncSettings.insert(
                std::pair<RcmClientLib::ResyncProgressType::ResyncTypePriority, std::string>(resyncTypePriority, syncSettingsItr->DiskId));
        }

        // Check contents of resync batching are valid in setSyncDiskIds
        std::map<std::string, int>::iterator resyncBatchItr = m_ResyncBatch.InprogressResyncDiskIDsMap.begin();
        while(resyncBatchItr != m_ResyncBatch.InprogressResyncDiskIDsMap.end())
        {
            // Also remove auto resync type DiskIds in resync batch which has future time schedule to accommodate oher priority DiskIds.
            const bool bRemoveDiskId = ((RcmClientLib::ResyncProgressType::ResyncTypePriority)resyncBatchItr->second ==
                RcmClientLib::ResyncProgressType::PriorityAutoResyncType) ? !bCanScheduleNow : false;

            if (bRemoveDiskId || setSyncDiskIds.find(resyncBatchItr->first) == setSyncDiskIds.end())
            {
                // DiskID not present in latest settings, remove this DiskID in resync batch.
                // Before removing the DIskID keep note of next DiskID as resyncBatchItr will become nvalid after removal of DiskID.
                std::string nextDiskId;
                if (++resyncBatchItr != m_ResyncBatch.InprogressResyncDiskIDsMap.end())
                {
                    nextDiskId = resyncBatchItr->first;
                }

                --resyncBatchItr;

                std::stringstream ss;
                if (bRemoveDiskId)
                {
                    ss << "Removing DiskId " << resyncBatchItr->first
                        << " with priority " << ResyncProgressType::ResyncTypes[resyncBatchItr->second]
                        << " from resync batch";
                }
                else
                {
                    ss << "DiskId " << resyncBatchItr->first << " not present in latest syncSettings, removing from resync batch";
                }
                DebugPrintf(SV_LOG_WARNING, "%s: %s.\n", FUNCTION_NAME, ss.str().c_str());

                m_ResyncBatch.InprogressResyncDiskIDsMap.erase(resyncBatchItr);
                
                resyncBatchItr = m_ResyncBatch.InprogressResyncDiskIDsMap.find(nextDiskId);
            }
            else
            {
                resyncBatchItr++;
            }
        }

        // Update resync batching for empty slot if any
        static const int MaxResyncBatchSize = lc.getMaxResyncBatchSize();
        MapPrioritySyncSettings::const_iterator pitr = mapPrioritySyncSettings.begin();
        while((m_ResyncBatch.InprogressResyncDiskIDsMap.size() < MaxResyncBatchSize) &&
            (pitr != mapPrioritySyncSettings.end()))
        {
            m_ResyncBatch.InprogressResyncDiskIDsMap[pitr->second] = (int)pitr->first;
            pitr++;
        }

        // Spawn DataprotectionSyncRcm instance for resync if not already running for DiskIds in resync batch.
        resyncBatchItr = m_ResyncBatch.InprogressResyncDiskIDsMap.begin();
        for (/*empty*/; resyncBatchItr != m_ResyncBatch.InprogressResyncDiskIDsMap.end(); resyncBatchItr++)
        {
            BOOST_ASSERT(resyncBatchItr->first.empty());

            const bool bScheduleNow = ((RcmClientLib::ResyncProgressType::ResyncTypePriority)resyncBatchItr->second ==
                RcmClientLib::ResyncProgressType::PriorityAutoResyncType) ? bCanScheduleNow : true;

            if (!bScheduleNow)
            {
                DebugPrintf(SV_LOG_INFO, "%s: Not starting resync for DiskId %s as schedule is not reached.\n",
                    FUNCTION_NAME, resyncBatchItr->first.c_str());
                continue;
            }

            std::string dpProcessName = InMageDataprotectionName + resyncBatchItr->first;
            AgentProcess::Ptr agentInfoPtr(new AgentProcess(dpProcessName, InMageDataprotectionId));
            AgentProcesses_t::iterator findIter = m_AgentProcesses.find(agentInfoPtr);

            if (findIter == m_AgentProcesses.end())
            {
                std::stringstream args;
                args << Agent_Mode_Source << ' ' << resyncBatchItr->first;

                if (!StartProcess(m_ProcessMgr, agentInfoPtr, lc.getDataProtectionExePathname().c_str(), args.str().c_str()))
                {
                    DebugPrintf(SV_LOG_ERROR, "%s : DataprotectionSyncRcm for %s did not start\n",
                        FUNCTION_NAME, resyncBatchItr->first.c_str());
                }
                else
                {
                    m_AgentProcesses.insert(agentInfoPtr);
                }
            }
            else
            {
                DebugPrintf(SV_LOG_INFO, "%s: DataprotectionSyncRcm for DiskId %s is running with pid %d.\n",
                    FUNCTION_NAME, resyncBatchItr->first.c_str(), (*findIter)->ProcessHandle());
            }
        }

        // Persist resync batch
        const std::string strNewResyncBatch = JSON::producer<ResyncBatch>::convert(m_ResyncBatch);
        if (0 != strNewResyncBatch.compare(m_strResyncBatch))
        {
            static const std::string resyncBatchCachePath = RcmConfigurator::getInstance()->getResyncBatchCachePath();
            std::ofstream ofstr(resyncBatchCachePath.c_str(), std::ofstream::trunc);
            ofstr << strNewResyncBatch;
            ofstr.close();
            m_strResyncBatch = strNewResyncBatch;
        }
    }
    catch(const std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with exception %s.\n", FUNCTION_NAME, e.what());
    }
    catch(...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with an unknown exception.\n", FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

void VxService::ProcessDeviceEvent(void* param)
{            
    DebugPrintf( SV_LOG_DEBUG,"ENTERED: ProcessDeviceEvent\n" );
    if(!m_quit)
    {
        try 
        {
#ifdef SV_WINDOWS  
            std::string sVolumes = static_cast<WinService*>(SERVICE::instance())->GetVolumesOnlineAsString();
            if (m_sVolumesOnline != sVolumes) 
            {
                m_sVolumesOnline = sVolumes;
#endif
                if (SVS_OK == RcmConfigurator::getInstance()->GetRcmClient()->RegisterMachine(0, true))
                    m_RegisterHostThread->SetLastRegisterHostTime();
                else
                    DebugPrintf(SV_LOG_ERROR, "ProcessDeviceEvent: RegisterMachine failed.\n");
#ifdef SV_WINDOWS  
            }
#endif
        }
        catch( std::string const& e ) {
            DebugPrintf( SV_LOG_ERROR,"ProcessDeviceEvent: exception %s\n", e.c_str() );
        }
        catch( char const* e ) {
            DebugPrintf( SV_LOG_ERROR,"ProcessDeviceEvent: exception %s\n", e );
        }
        catch( exception const& e ) {
            DebugPrintf( SV_LOG_ERROR,"ProcessDeviceEvent: exception %s\n", e.what() );
        }
        catch(...) {
            DebugPrintf( SV_LOG_ERROR,"ProcessDeviceEvent: exception...\n" );
        }
    }
    DebugPrintf( SV_LOG_DEBUG,"EXITED: ProcessDeviceEvent\n" );

}

#ifdef SV_WINDOWS
void VxService::ProcessBitLockerEvent(void *param)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED: ProcessBitLockerEvent\n");
    if (!m_quit)
    {
        try
        {
            boost::shared_ptr<DeviceStream> devStream;
            devStream.reset(new DeviceStream(m_FilterDrvIf.GetDriverName(VolumeSummary::DISK)));

            if (SV_SUCCESS != devStream->Open(
                DeviceStream::Mode_Open | DeviceStream::Mode_Asynchronous |
                DeviceStream::Mode_RW | DeviceStream::Mode_ShareRW))
            {
                DebugPrintf(SV_LOG_ERROR,
                    "ProcessBitLockerEvent: Failed to open driver in async mode. Error: %d.\n",
                    devStream->GetErrorCode());

                return;
            }

            DebugPrintf(SV_LOG_DEBUG, "ProcessBitLockerEvent: Opened driver in async mode.\n");

            VolumeSummaries_t volumeSummaries;
            VolumeDynamicInfos_t volumeDynamicInfos;
            VolumeInfoCollector volumeCollector;

            volumeCollector.GetVolumeInfos(volumeSummaries, volumeDynamicInfos, true);

            // TODO-SanKumar-1806: Currently the bitmap files are assumed
            // to be always present on the system drive. Rather query the
            // driver / driver's registry key to decisively conclude, which
            // volume contains the bitmap file.

            // NOTE-SanKumar-1805: Another assumption here is that all
            // the bitmap files would be present under the same folder.
            // Although the driver has the provision to place various
            // bitmap files in different locations, its currently only
            // used in testing.

            bool systemVolumeFound = false;
            bool bAreBitmapFilesEncryptedOnDisk = false;

            for (VolumeSummary &currVolSum : volumeSummaries)
            {
                if (!currVolSum.isSystemVolume)
                    continue;

                BOOST_ASSERT(!systemVolumeFound);
                systemVolumeFound = true;
                // TODO-SanKumar-1806: Should we break after the first hit?

                auto itr = currVolSum.attributes.find(NsVolumeAttributes::BITLOCKER_CONV_STATUS);

                // NOTE-SanKumar-1805: If the volume is locked, the conversion
                // status won't be present but the system volume can't be locked.
                // Another case where the conversion status won't be present is
                // when the system drive isn't yet prepared for bitlocker encryption.
                // So, we can safely assume that if the attribute is not present,
                // the system volume is not encrypted.

                if (itr != currVolSum.attributes.end() && !itr->second.empty())
                {
                    UINT32 convStatus = std::stoul(itr->second);

                    BOOST_ASSERT(convStatus < VolumeSummary::BitlockerConversionStatus::CONVERSION_COUNT);

                    // NOTE-SanKumar-1805: In all other cases of conversion apart from "fully
                    // decrypted", the volume is at least partially (or) at best fully encrypted.
                    // Too many complicated indirect calculations are necessary to determine if
                    // the bitmap files are encrypted (or) not.
                    // So, we take the simplistic approach that even if there's a little bit of
                    // encrypted data on the volume, we consider our bitmap files are encrypted.
                    bAreBitmapFilesEncryptedOnDisk =
                        (convStatus != VolumeSummary::BitlockerConversionStatus::CONVERSION_FULLY_DECRYPTED);
                }

                // TODO-SanKumar-1806: Currently the VolInfoCollector has a best effort collection
                // mechanism, i.e. if there are any error in collecting the data, it's not popped
                // up. So, we might end up with conversion status attribute / the entire system
                // volume info missing. Unfortunately, this collides with few cases, where
                // we will not see the conversion status attribute for fully decrypted system
                // volume, if it's not yet prepared for encryption. So, we might end up sending
                // false positives saying the files are not encrypted, in cases where we face
                // errors in the WMI data collection.
                // We could either optimize this into a direct query, where the WMI call
                // GetConversionStatus() could be invoked only on the system drive (or) the
                // volume monitor could keep track of the changes and could be queried by us
                // in this callback [1st is preferred].
            }

            if (m_FilterDrvIf.NotifySystemState(devStream.get(), bAreBitmapFilesEncryptedOnDisk))
            {
                DebugPrintf(SV_LOG_DEBUG,
                    "ProcessBitLockerEvent: Notify system state to driver succeeded. AreBitmapFilesEncryptedOnDisk: %d.\n",
                    bAreBitmapFilesEncryptedOnDisk);
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR,
                    "ProcessBitLockerEvent: Notify system state to driver failed. AreBitmapFilesEncryptedOnDisk: %d.\n",
                    bAreBitmapFilesEncryptedOnDisk);
            }
        }
        catch (std::string const& e) {
            DebugPrintf(SV_LOG_ERROR, "ProcessBitLockerEvent: exception %s\n", e.c_str());
        }
        catch (char const* e) {
            DebugPrintf(SV_LOG_ERROR, "ProcessBitLockerEvent: exception %s\n", e);
        }
        catch (exception const& e) {
            DebugPrintf(SV_LOG_ERROR, "ProcessBitLockerEvent: exception %s\n", e.what());
        }
        catch (...) {
            DebugPrintf(SV_LOG_ERROR, "ProcessBitLockerEvent: exception...\n");
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED: ProcessBitLockerEvent\n");
}
#endif // SV_WINDOWS

bool VxService::updateResourceID()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool updated = true;

    if (RcmConfigurator::getInstance()->getResourceId().empty()) {
        try {
            RcmConfigurator::getInstance()->setResourceId(GetUuid());
        } catch (ContextualException &e) {
            updated = false;
            DebugPrintf(SV_LOG_ERROR, "Failed to generate/update resource id with exception: %s\n", e.what());
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return updated;
}

bool VxService::updateSourceGroupID()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool updated = true;
    if (RcmConfigurator::getInstance()->getSourceGroupId().empty()) {
        try {
            RcmConfigurator::getInstance()->setSourceGroupId(GetUuid());
        }
        catch (ContextualException &e) {
            updated = false;
            DebugPrintf(SV_LOG_ERROR, "Failed to generate/update source group id with exception: %s\n", e.what());
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return updated;
}

bool VxService::doPreConfiguratorTasks()
{
    return updateResourceID() && updateSourceGroupID();
}

void VxService::ReadPersistedResyncBatching()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    boost::system::error_code ec;
    try
    {
        // Read persisted resync batching if exist. Ignore if cached resync batching file does not exists or read failed.
        const std::string resyncBatchCachePath(RcmConfigurator::getInstance()->getResyncBatchCachePath());

        if (boost::filesystem::exists(resyncBatchCachePath, ec))
        {
            std::ifstream hResyncBatchCachePath(resyncBatchCachePath.c_str());
            if (hResyncBatchCachePath.good())
            {
                m_strResyncBatch = std::string((std::istreambuf_iterator<char>(hResyncBatchCachePath)), (std::istreambuf_iterator<char>()));
                m_ResyncBatch = JSON::consumer<ResyncBatch>::convert(m_strResyncBatch);
            }
            else
            {
                DebugPrintf(SV_LOG_WARNING, "%s: Failed to open cached resync batch file %s for reading.\n",
                    FUNCTION_NAME, resyncBatchCachePath.c_str());
            }
            hResyncBatchCachePath.close();
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: Cached resync batch file does not exist.\n", FUNCTION_NAME);
        }
    }
    catch (const std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with exception %s.\n", FUNCTION_NAME, e.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with an unknown exception.\n", FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void VxService::GetResyncBatch(ResyncBatch &resyncBatch)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    ACE_Guard<ACE_Mutex> resyncBatchGuard(m_ResyncBatchLock);
    resyncBatch = m_ResyncBatch;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

int VxService::init()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    int ret = SV_FAILURE;

    try 
    {
        LocalConfigurator localConfigurator;

        if (doPreConfiguratorTasks() == false)
        {
            /* to avoid spinning on cpu
             * as this will be retried */
            CDPUtil::QuitRequested(2);
            return SV_FAILURE ;
        }

        DebugPrintf( SV_LOG_DEBUG,"VxService started\n" );        

        m_ProcessMgr = new ACE_Process_Manager();
        m_ProcessMgr->open();
        //
        // Register ACE_Event_Handler for exit notification
        //
        m_ProcessMgr->register_handler(this);

        /* Create RegisterHostThread */
        m_RegisterHostThread = new RegisterHostThread(
          localConfigurator.getRegisterHostInterval(),
          SETTINGS_POLLING_INTERVAL);

        HandlerMap_t handlers;
        handlers.insert(std::make_pair(RcmWorkerThreadHandlerNames::SENTINELSTOP, boost::bind(&VxService::StopSentinel, this)));
        handlers.insert(std::make_pair(RcmWorkerThreadHandlerNames::VACPSTOP, boost::bind(&VxService::StopVacp, this)));
        handlers.insert(std::make_pair(RcmWorkerThreadHandlerNames::VACPRESTART, boost::bind(&VxService::RestartVacp, this)));

        m_RcmWorkerThread = new RcmWorkerThread(handlers);

        m_ConsistencyThread = new ConsistencyThreadRCM();

        m_MonitorHostThread = new MonitorHostThread();

#ifdef SV_WINDOWS
        ((CService*)SERVICE::instance())->getBitlockerMonitorSignal().connect(this, &VxService::ProcessBitLockerEvent);
        m_failoverClusterConsistencyUtil = new ClusterConsistencyUtil();
#endif // SV_WINDOWS

        RcmConfigurator::getInstance()->GetProtectionDisabledSignal().connect(this, &VxService::ProtectionDisabledCallback);

        m_RcmWorkerThread->GetPrepareSwitchApplianceSignal().connect(this, &VxService::PrepareSwitchApplianceCallback);
        m_RcmWorkerThread->GetSwitchApplianceSignal().connect(this, &VxService::SwitchApplianceCallback);

        m_MonitorHostThread->GetLogUploadAccessFailSignal().connect(m_RcmWorkerThread, &RcmWorkerThread::UpdateLogUploadAccessFail);

        ReadPersistedResyncBatching();

        m_MonitorHostThread->GetResyncBatchSignal().connect(this, &VxService::GetResyncBatch);

        m_bInitialized = true;
        ret = SV_SUCCESS;
    }
    catch( std::string const& e ) 
    {
        DebugPrintf( SV_LOG_ERROR,"init: exception %s\n", e.c_str() );
        throw e;
    }
    catch( char const* e )
    {
        DebugPrintf( SV_LOG_ERROR,"init: exception %s\n", e );
        throw e;
    }
    catch( exception const& e )
    {
        DebugPrintf( SV_LOG_ERROR,"init: exception %s\n", e.what() );
        throw e;
    }
    catch(...) 
    {
        DebugPrintf( SV_LOG_ERROR,"init: exception...\n" );
        throw;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return ret;
}

const bool VxService::isInitialized() const
{
    return m_bInitialized;
}


void VxService::StopFiltering()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    ACE_Guard<ACE_Mutex> PropertyChangeGuard(m_PropertyChangeLock);

    if( IsUninstallSignalled() )
    {
        AzureVmRecoveryManager::ResetReplicationState();
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

}

void VxService::StartRegistrationTask()
{
    DebugPrintf(SV_LOG_ALWAYS, "ENTERED %s\n", FUNCTION_NAME);

    /* Invalidate the volumes cache which can 
     * be used by children; This is needed
     * where s2, dp should not issue
     * filetering, mirror on wrong 
     * devices when names change across reboots. */
    const unsigned CLEAR_RETRY_INTERVAL_SECONDS = 300;
    do {
        DataCacher dataCacher;
        
        if (dataCacher.Init()) {
            DebugPrintf(SV_LOG_DEBUG, "Initialized data cacher to clean volumeinfocollector cache.\n");
            if (dataCacher.ClearVolumesCache()) {
                DebugPrintf(SV_LOG_ALWAYS, "%s: Cleaned volumeinfocollector cache.\n", FUNCTION_NAME);
                break;
            } else
                DebugPrintf(SV_LOG_ERROR, "Vx service failed to clean volumeinfocollector cache with error %s.\n", dataCacher.GetErrMsg().c_str());
        } else
            DebugPrintf(SV_LOG_ERROR, "Vx service failed to initialize data cacher to clean volumeinfocollector cache.");

        DebugPrintf(SV_LOG_ERROR, "%s: Retrying after %u minutes...\n", FUNCTION_NAME, CLEAR_RETRY_INTERVAL_SECONDS/60);
    } while (!CDPUtil::QuitRequested(CLEAR_RETRY_INTERVAL_SECONDS));

    //Start registration task
    m_RegisterHostThread->Start();

    //Get the cache
    const int WAIT_SECS_FOR_CACHE = 10;
    do
    {
        std::string err;
        if (DataCacher::UpdateVolumesCache(m_VolumesCache, err, SV_LOG_DEBUG))
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s: volume cache updated.\n", FUNCTION_NAME);
            break;
        }
    } while (!CDPUtil::QuitRequested(WAIT_SECS_FOR_CACHE));

    DebugPrintf(SV_LOG_ALWAYS, "EXITED %s\n", FUNCTION_NAME);

    return;
}

void VxService::StartRcmWorker()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    if (m_RcmWorkerThread)
    {
        m_RcmWorkerThread->Start();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void VxService::StartConsistenyThread()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (m_ConsistencyThread)
    {
        if (m_MonitorHostThread) {
            m_ConsistencyThread->SetTagStatusCallBack(boost::bind(&MonitorHostThread::SetLastTagState, m_MonitorHostThread, _1, _2,_3));
        }

        m_ConsistencyThread->Start();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void VxService::StartMonitors()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    if (m_MonitorHostThread)
    {
        m_MonitorHostThread->Start();
    }

#ifdef SV_WINDOWS
    SERVICE::instance()->startVolumeMonitor();
#endif // SV_WINDOWS

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

void VxService::StopMonitors()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

#ifdef SV_WINDOWS
    SERVICE::instance()->stopVolumeMonitor();
#endif // SV_WINDOWS

    if (m_MonitorHostThread)
    {
        m_MonitorHostThread->Stop();
        delete m_MonitorHostThread;
        m_MonitorHostThread = NULL;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

#ifdef SV_WINDOWS
//If you change InMageAgent, please do the necessary modifications in winservice.h as well
ACE_NT_SERVICE_DEFINE (InMageAgent, WinService, SERVICE_NAME)
#endif /* SV_WINDOWS */


void VxService::StopAgentsHavingAttributes(const std::string &DeviceName,
    const std::string &EndpointDeviceName,
    const int &group,
    const uint32_t &waitTimeBeforeTerminate)
{
    std::stringstream ss;
    ss << "DeviceName " << DeviceName << ", EndpointDeviceName " << EndpointDeviceName << ", group " << group;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s with %s\n", FUNCTION_NAME, ss.str().c_str());

    ACE_Time_Value time_value;
    time_value.set(AGENTSTIME_TOQUIT_INSECS, 0);

    steady_clock::time_point endtimeToWaitForQuit = steady_clock::now();
    if (waitTimeBeforeTerminate)
        endtimeToWaitForQuit += seconds(waitTimeBeforeTerminate);
    else
        endtimeToWaitForQuit = steady_clock::time_point::max();

    do {
        AgentProcessesIterVec_t agentprocesses;
        GetAgentsHavingAttributes(DeviceName, EndpointDeviceName, group, agentprocesses);
        if (0 == agentprocesses.size()) {
            DebugPrintf(SV_LOG_DEBUG, "Did not find agent process(es) with above attributes.\n");
            break;
        }
        
        DebugPrintf(SV_LOG_DEBUG, "Agent process(es) exists for above attributes. Issuing quit message.\n");
        
        for (AgentProcessesIterVec_t::iterator vit = agentprocesses.begin(); vit != agentprocesses.end(); vit++) {
            AgentProcessesIter_t it = *vit;
            const boost::shared_ptr<AgentProcess> &ap = *it;
        
            if (steady_clock::now() > endtimeToWaitForQuit) {
                ap->Stop(m_ProcessMgr);
            }
            else if (!PostQuitMessageToAgent(ap)) {
                DebugPrintf(SV_LOG_ERROR, "Posting quit message failed on trying to stop agent(s) having attributes %s. It will be retried.\n", ss.str().c_str());
            }
        }
        
        DebugPrintf(SV_LOG_DEBUG, "Quit message has been posted for agent(s) having above attributes. Waiting for these to exit...\n");
        
        if (WAIT_FOR_ERROR == WaitForMultipleAgents(time_value))
            DebugPrintf(SV_LOG_ERROR, "Wait failed on trying to stop agent(s) having attributes %s. It will be retried.\n", ss.str().c_str());

    } while (true);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


bool VxService::PostQuitMessageToAgent(const boost::shared_ptr<AgentProcess> &ap)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool posted = true;

    std::stringstream sspid;
    sspid << ap->ProcessHandle();
    DebugPrintf(SV_LOG_DEBUG, "Issuing quit message to agent process having pid %s\n", sspid.str().c_str());
    if (ap->IsAlive()) {
        if (-1 != ap->PostQuitMessage())
            DebugPrintf(SV_LOG_DEBUG, "Issued quit message to above agent.\n");
        else {
            DebugPrintf(SV_LOG_ERROR, "For agent with pid %s, posting quit message failed.\n", sspid.str().c_str());
            posted = false;
        }
    } else
        DebugPrintf(SV_LOG_DEBUG, "Above agent is not alive. Not issuing quit message.\n", sspid.str().c_str());

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return posted;
}


void VxService::GetAgentsHavingAttributes(const std::string &DeviceName, const std::string &EndpointDeviceName, 
                                          const int &group, AgentProcessesIterVec_t &agentprocesses)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool isdevicenamevalid = !DeviceName.empty();
    bool isendpointvalid = !EndpointDeviceName.empty();
    bool isgroupvalid = (~0 != group);

    if (isdevicenamevalid && isendpointvalid && isgroupvalid) {
        DebugPrintf(SV_LOG_DEBUG, "Searching agent process having devicename %s, endpoint %s and group %d.\n", 
                                  DeviceName.c_str(), EndpointDeviceName.c_str(), group);
        AgentProcess::Ptr agentInfoPtr(new AgentProcess(DeviceName, EndpointDeviceName, group));
        AgentProcesses_t::iterator findIter = m_AgentProcesses.find(agentInfoPtr);
        if (findIter != m_AgentProcesses.end()) 
            agentprocesses.push_back(findIter);
    } else if (isdevicenamevalid && (!isendpointvalid) && isgroupvalid) {
        DebugPrintf(SV_LOG_DEBUG, "Searching agent process having devicename %s and group %d.\n", 
                                  DeviceName.c_str(), group);
        AgentProcess::Ptr agentInfoPtr(new AgentProcess(DeviceName, group));
        AgentProcesses_t::iterator findIter = m_AgentProcesses.find(agentInfoPtr);
        if (findIter != m_AgentProcesses.end()) 
            agentprocesses.push_back(findIter);
    } else if (isdevicenamevalid && (!isendpointvalid) && (!isgroupvalid)) {
        DebugPrintf(SV_LOG_DEBUG, "Searching agent processes having devicename %s.\n", DeviceName.c_str());
        AgentProcesses_t::iterator begin = m_AgentProcesses.begin();
        AgentProcesses_t::iterator end = m_AgentProcesses.end();
        DoesAgentProcessHaveDeviceName d(DeviceName);
        do {
            AgentProcesses_t::iterator it = std::find_if(begin, end, d);
            if (end != it) {
                agentprocesses.push_back(it);
                it++;
            }
            begin = it;
        } while (begin != end);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

void VxService::StopVacp()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    ACE_Guard<ACE_Mutex> SentinelGuard(m_vacpLock);
    m_bVacpStopRequested = true;
    StopAgentsHavingAttributes(InMageVacpName, std::string(), InMageVacpId);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void VxService::RestartVacp()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    ACE_Guard<ACE_Mutex> SentinelGuard(m_vacpLock);
    m_bVacpStopRequested = false;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void VxService::StopSentinel()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    ACE_Guard<ACE_Mutex> SentinelGuard(m_sentinelLock);
    StopAgentsHavingAttributes(InMageSentinelName, std::string(), InMageSentinelId);
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

bool VxService::IsSentinelRunning()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    ACE_Guard<ACE_Mutex> SentinelGuard(m_sentinelLock);
    bool sentinelRunning = false;
    try {
        AgentProcess::Ptr agentInfoPtr(new AgentProcess(InMageSentinelName, InMageSentinelId));

        AgentProcesses_t::iterator findIter = m_AgentProcesses.find(agentInfoPtr);
        sentinelRunning = (findIter != m_AgentProcesses.end());
    }
    catch (const std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: failed with error %s\n", e.what());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return sentinelRunning;
}

void VxService::StopAgentsRunningOnSource(const std::string &devicename)
{	
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    /* TODO: Better if we be first setting quit on s2, dps on source and 
     * then wait ? */
    StopSentinel();
    StopAgentsHavingAttributes(devicename, std::string(), ~0);
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


bool VxService::PerformSourceTasks(const std::string &filterDriverName, LocalConfigurator &lc)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool isperformed = false;

    if (!PersistPlatformTypeForDriver())
        return isperformed;

    if (m_IsFilterDriverNotified) {
        //Already notification was issued.
        isperformed = true;
    }
    else if (lc.IsFilterDriverAvailable()) {
        // Notification was not issued. Hence issue it.
        isperformed = NotifyFilterDriver(filterDriverName);
#ifdef SV_WINDOWS
        if (isperformed) {
            ConfigureDataPagedPool(lc);
        }
#endif
    }

#ifdef SV_UNIX
    if (isperformed)
    {
        if (SVS_OK != VerifyProtectedDevicesOnStartup())
        {
            isperformed = false;
        }
    }
#endif

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return isperformed;
}

#ifdef SV_WINDOWS
/*
* FUNCTION NAME : VxService::ConfigureDataPagedPool
*
* DESCRIPTION
*       Configures driver with data paged pool in MB.
*       It does following,
*           1) Figure RAM Size
*           2) Get min and max DPP Usage Values from config.
*           3) Get Percentage of RAM to be used by driver.
*           4) Get Data Paged Pool Aligment,
*
*       It calcualtes Data Paged Pool usage as follows,
*       1) Calculates current DPP size by multiplying DPP usage percentage
*           with RAM size.
*       2) If current DPP usage is greater than Max DPP Usage,
*           reset current DPP usage to max DPP USage.
*       3) Align DPP usage to alignment boundary.
*       4) If calculated DPP usage is less than minimum DPP usage, reset DPP
*               usage to minimum DPP usage.
*       Post calculation it calls FilterDriverNotifier to configure filter
*           driver with calculated Data Paged Pool.
*
* return value : None.
*
*/
bool VxService::ConfigureDataPagedPool(const LocalConfigurator& lc)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool bStatus = false;

    SV_ULONG ulDppSizeInPercentage = lc.getDriverDppRamUsageInPercent();
    SV_ULONG ulMaxDppSizeInMB = lc.getDriverMaxDppUsageInMB();
    SV_ULONG ulMinDppSizeInMB = lc.getDriverMinDppUsageInMB();
    SV_ULONG ulDppAlignmentInMB = lc.getDriverDppAlignmentInMB();

    DebugPrintf(SV_LOG_DEBUG, "ulDppSizeInPercentage = %lu ulMaxDppSizeInMB = %lu ulMinDppSizeInMB = %lu ulDppAlignmentInMB = %lu\n",
        ulDppSizeInPercentage,
        ulMaxDppSizeInMB,
        ulMinDppSizeInMB,
        ulDppAlignmentInMB);

    MEMORYSTATUSEX  memoryStatusEx = { 0 };
    memoryStatusEx.dwLength = sizeof(memoryStatusEx);

    if (!GlobalMemoryStatusEx(&memoryStatusEx)){
        DebugPrintf(SV_LOG_ERROR, "Failed to get Ram Size Error = %d\n", GetLastError());
        return false;
    }


    SV_ULONGLONG    ullTotalPhysicalRamInMB = memoryStatusEx.ullTotalPhys / (1024 * 1024);
    DebugPrintf(SV_LOG_DEBUG, "Total Physical RAM = %lu MB\n", ullTotalPhysicalRamInMB);

    SV_ULONG    ulDppSizeInMB = (ullTotalPhysicalRamInMB * ulDppSizeInPercentage) / 100;

    if (ulDppSizeInMB > ulMaxDppSizeInMB) {
        ulDppSizeInMB = ulMaxDppSizeInMB;
    }

    // Align Before configuring driver
    ulDppSizeInMB = ((ulDppSizeInMB / ulDppAlignmentInMB) * ulDppAlignmentInMB);

    if (ulDppSizeInMB < ulMinDppSizeInMB) {
        ulDppSizeInMB = ulMinDppSizeInMB;
    }

    try{
        DebugPrintf(SV_LOG_INFO, "Configuring Data Paged Pool size = %lu MB RamSize = %llu MB\n", ulDppSizeInMB, ullTotalPhysicalRamInMB);
        m_sPFilterDriverNotifier->ConfigureDataPagedPool(ulDppSizeInMB);
    }
    catch (FilterDriverNotifier::Exception ex) {
        bStatus = false;
        DebugPrintf(SV_LOG_ERROR, "Failed to configure driver err=%s.\n", ex.what());
    }
    catch (std::exception& ex){
        bStatus = false;
        DebugPrintf(SV_LOG_ERROR, "Failed to configure driver err=%s.\n", ex.what());
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bStatus;
}
#endif

bool VxService::NotifyFilterDriver(const std::string &filterDriverName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    //Allocate the notifier object if needed
    if (!m_sPFilterDriverNotifier) {
        try {
            m_sPFilterDriverNotifier = new FilterDriverNotifier(filterDriverName);
            DebugPrintf(SV_LOG_DEBUG, "Created filter driver notifier.\n");
        }
        catch (std::bad_alloc &e) {
            DebugPrintf(SV_LOG_ERROR, "FAILED: Memory allocation for FilterDriverNotifier object failed with error %s.@LINE %d in FILE %s\n", e.what(), LINE_NO, FILE_NAME);
            return false;
        }
    }

    unsigned nfailures = 0;
    LocalConfigurator localconfigurator;
    const unsigned FAILURE_NO_FOR_ALERT = 15;
    //Notify with retry
    while (!this->m_quit) {
        try {
            //Notify
            m_sPFilterDriverNotifier->Notify(FilterDriverNotifier::E_SERVICE_SHUTDOWN_NOTIFICATION);
            //Success
            DebugPrintf(SV_LOG_DEBUG, "Filter driver notified.\n");
            m_IsFilterDriverNotified = true;
            break;
        }
        catch (FilterDriverNotifier::Exception &e) {
            nfailures++;
            nfailures %= FAILURE_NO_FOR_ALERT;
            if (1 == nfailures)
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to issue process shutdown notify with error: %s\n", e.what());
            }

#ifdef SV_UNIX
            UpdateCleanShutdownFile();
#endif
            CDPUtil::QuitRequested(60);

            StartEvtCollForw(localconfigurator);
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return m_IsFilterDriverNotified;
}

#ifdef SV_UNIX
bool VxService::UpdateCleanShutdownFile()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    static bool bRetVal = false;

    if (bRetVal)
        return bRetVal;

    std::string file = DEFAULT_CLEAN_SHUTDOWN_STATUS_FILE;
    DebugPrintf(SV_LOG_DEBUG, "Default clean shutdown status file=%s\n", file.c_str());

    boost::filesystem::path shutdown_file(file);
    if (!boost::filesystem::exists(shutdown_file))
    {
        DebugPrintf(SV_LOG_ERROR, "%s ::  %s file is not found\n", FUNCTION_NAME, file.c_str());
    }

    std::ofstream outFile(file.c_str(), std::ios::trunc);

    if (!outFile.is_open())
    {
        DebugPrintf(SV_LOG_ERROR, "%s :: Failed to open file : %s\n", FUNCTION_NAME, file.c_str());
        return bRetVal;
    }

    outFile << STATUS_SHUTDOWN_UNCLEAN;

    if (!outFile.good())
    {
        DebugPrintf(SV_LOG_ERROR, "%s :: Write [%d] failed for file : %s\n", FUNCTION_NAME, STATUS_SHUTDOWN_UNCLEAN, file.c_str());
        return bRetVal;
    }
    outFile.flush();
    outFile.close();

    bRetVal = true;
    DebugPrintf(SV_LOG_ALWAYS, "%s : Successfully updated CleanShutdown file %s.\n", FUNCTION_NAME, file.c_str());

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bRetVal;
}
#endif

bool VxService::PerformRollbackIfMigrationInProgress()
{
    DebugPrintf(SV_LOG_ALWAYS, "ENTERED %s\n", FUNCTION_NAME);
    bool isMigrationInProgress = false;
    ACE_Guard<ACE_Mutex> SentinelGuard(m_migrationStateLock);

    if (RcmConfigurator::getInstance()->IsOnPremToAzureReplication()
        && (RcmConfigurator::getInstance()->getMigrationState() == Migration::MIGRATION_SWITCHED_TO_RCM
            || RcmConfigurator::getInstance()->getMigrationState() == Migration::MIGRATION_SWITCHED_TO_RCM_PENDING
            || RcmConfigurator::getInstance()->getMigrationState() == Migration::MIGRATION_ROLLBACK_PENDING))
    {
        isMigrationInProgress = true;
        RcmConfigurator::getInstance()->setMigrationState(Migration::MIGRATION_ROLLBACK_PENDING);
        m_bRestartOnDisableReplication = false;
        m_bIsProtectionDisabled = false;
        DebugPrintf(SV_LOG_ALWAYS, "%s migration set to rollback pending, quitting svagentRCM.\n", FUNCTION_NAME);
        m_quit++;
        CDPUtil::SignalQuit();
    }

    DebugPrintf(SV_LOG_ALWAYS, "EXITED %s\n", FUNCTION_NAME);
    return isMigrationInProgress;
}

void VxService::ProtectionDisabledCallback()
{
    DebugPrintf(SV_LOG_ALWAYS, "ENTERED %s\n", FUNCTION_NAME);     

    LocalConfigurator lc;
    do
    {
        if (PerformRollbackIfMigrationInProgress())
        {
            break;
        }

        if (!RcmConfigurator::getInstance()->IsAzureToAzureReplication())
        {
            m_bRestartOnDisableReplication = true;
        }

        m_bIsProtectionDisabled = true;

    } while (false);

    DebugPrintf(SV_LOG_ALWAYS, "EXITED %s\n", FUNCTION_NAME);
    return;
}

void VxService::PrepareSwitchApplianceCallback()
{
    DebugPrintf(SV_LOG_ALWAYS, "ENTERED %s\n", FUNCTION_NAME);

    RcmConfigurator::getInstance()->setSwitchApplianceState(
        SwitchAppliance::PREPARE_SWITCH_APPLIANCE_START);

    DebugPrintf(SV_LOG_ALWAYS, "EXITED %s\n", FUNCTION_NAME);
}

void VxService::SwitchApplianceCallback(RcmProxyTransportSetting &rcmProxySettings)
{
    DebugPrintf(SV_LOG_ALWAYS, "ENTERED %s\n", FUNCTION_NAME);

    m_switchApplianceSettings = rcmProxySettings;
    RcmConfigurator::getInstance()->setSwitchApplianceState(
        SwitchAppliance::SWITCH_APPLIANCE_START);

    DebugPrintf(SV_LOG_ALWAYS, "EXITED %s\n", FUNCTION_NAME);
}

void VxService::StopReplicationOnProtectionDisabled()
{
    DebugPrintf(SV_LOG_ALWAYS, "ENTERED %s\n", FUNCTION_NAME);

    try {

        m_quit++;
        CDPUtil::SignalQuit();

        RcmConfigurator::getInstance()->PersistDisableProtectionState();

    }
    catch (const std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: exception %s\n", FUNCTION_NAME, e.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: unknown exception.\n", FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_ALWAYS, "EXITED %s\n", FUNCTION_NAME);
    return;
}

#ifdef SV_UNIX
SVSTATUS VxService::VerifyProtectedDevicesOnStartup()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    AgentSettings settings;
    status = RcmConfigurator::getInstance()->GetAgentSettings(settings);
    if (status != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Failed to fetch cached replication settings: %d\n", FUNCTION_NAME, status);
        return status;
    }

    std::vector<std::string>::const_iterator dit = settings.m_protectedDiskIds.begin();
    for (/*empty*/; dit != settings.m_protectedDiskIds.end(); dit++)
    {
        VolumeReporter::VolumeReport_t vr;
        SVSTATUS retval = GetVolumeReportForPersistentDeviceName(*dit, m_VolumesCache.m_Vs, vr);

        // if a disk is removed, just let the service initialize and replicate rest of the disks
        if ((retval != SVS_OK) || (!vr.m_bIsReported))
        {
            DebugPrintf(SV_LOG_ERROR, "%s: for disk-id %s volume report doesn't exist. status %d\n", FUNCTION_NAME, dit->c_str(), retval);
            continue;
        }

        std::string driverFilteredName;
        bool bSuccess = GetDriverFilteredDeviceForPersistentDevice(*dit, driverFilteredName);

        if (driverFilteredName.empty())
        {
            DebugPrintf(SV_LOG_ERROR, "%s: for disk-id %s GetDriverFilteredDeviceForPersistentDevice() returned empty name. status %d\n", FUNCTION_NAME, dit->c_str(), bSuccess);
            continue;
        }

        if ((bSuccess) &&
            !boost::iequals(driverFilteredName, vr.m_DeviceName))
        {

            DebugPrintf(SV_LOG_ERROR,
                "%s: restacking disk-id %s as device name %s and driver device name %s doesn't match.\n",
                FUNCTION_NAME,
                dit->c_str(),
                vr.m_DeviceName.c_str(),
                driverFilteredName.c_str());

            try {

                // overwrite vr.m_DeviceName with what the driver stacked
                vr.m_DeviceName = driverFilteredName;

                // use the driver's device name so that start filtering succeeds
                DpDriverComm ddc(driverFilteredName, VolumeSummary::DISK, *dit);

                // issue a start filtering so that the device will be stacked
                // once the device is stacked, a stop will clear the bitmap
                ddc.StartFiltering(vr, &m_VolumesCache.m_Vs);
                ddc.StopFiltering();
            }
            catch (const DataProtectionException &e)
            {
                DebugPrintf(SV_LOG_ERROR,
                    "%s: failed to stop filtering. Error: %s \n",
                    FUNCTION_NAME,
                    e.what());
                status = SVE_FAIL;
                break;
            }
            catch (const std::exception &e)
            {
                DebugPrintf(SV_LOG_ERROR,
                    "%s: failed to stop filtering. Error: %s \n",
                    FUNCTION_NAME,
                    e.what());
                status = SVE_FAIL;
                break;
            }
            catch (...)
            {
                DebugPrintf(SV_LOG_ERROR, "%s: caught unknown exception.\n", FUNCTION_NAME);
                status = SVE_FAIL;
                break;
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return status;
}

bool VxService::GetDriverFilteredDeviceForPersistentDevice(const std::string &persistentName, std::string& deviceName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (!IsDriverPnameCheckSupported())
    {
        DebugPrintf(SV_LOG_INFO, "%s: driver doesn't support persistent names.\n", FUNCTION_NAME);
        return false;
    }

    std::string cmdOutput;
    std::string drvCmd = RcmConfigurator::getInstance()->getInstallPath();
    drvCmd += ACE_DIRECTORY_SEPARATOR_CHAR;
    drvCmd += "bin";
    drvCmd += ACE_DIRECTORY_SEPARATOR_CHAR;
    drvCmd += "inm_dmit --op=map_name --pname=";

    std::string pname = persistentName;
    boost::replace_all(pname, "/", "_");
    drvCmd += pname;

    std::stringstream results;

    if (!executePipe(drvCmd, results))
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s: command %s failed.\n output %s\n",
            FUNCTION_NAME,
            drvCmd.c_str(),
            results.str().c_str());

        return false;
    }

    results >> cmdOutput;
    deviceName = cmdOutput;
    boost::trim(deviceName);

    DebugPrintf(SV_LOG_ALWAYS, "%s: persistentName %s, deviceName %s, output %s\n", FUNCTION_NAME, persistentName.c_str(), deviceName.c_str(), cmdOutput.c_str());

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return true;
}

bool VxService::IsDriverPnameCheckSupported()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    static bool bCheckSupported = false;
    static bool bHaveDriverVersion = false;
    static DRIVER_VERSION stCurrentDrvVersion;

    // version of the driver that supports the check
    const DRIVER_VERSION stSupportedDrvVersion = { 2, 3, 3, 15, 9, 12, 0, 0 };

    if (bHaveDriverVersion)
        return bCheckSupported;

    FilterDrvIfMajor drvif;
    std::string driverName = drvif.GetDriverName(VolumeSummary::DISK);
    DeviceStream::Ptr m_pDriverStream;

    if (!m_pDriverStream) 
    {
        try {
            m_pDriverStream.reset(new DeviceStream(Device(driverName)));
            DebugPrintf(SV_LOG_DEBUG, "Created device stream.\n");
        }
        catch (std::bad_alloc &e) {
            std::string errMsg("Memory allocation failed for creating driver stream object with exception: ");
            errMsg += e.what();
            DebugPrintf(SV_LOG_ERROR, "%s : error %s.\n", FUNCTION_NAME, errMsg.c_str());
            return bCheckSupported;
        }
    }

    //Open device stream in asynchrnous mode if needed
    if (m_pDriverStream && !m_pDriverStream->IsOpen()) 
    {
        if (SV_SUCCESS == m_pDriverStream->Open(DeviceStream::Mode_Open | DeviceStream::Mode_Asynchronous | DeviceStream::Mode_RW | DeviceStream::Mode_ShareRW))
        {
            DebugPrintf(SV_LOG_DEBUG, "Opened driver %s in async mode.\n", driverName.c_str());
        }
        else
        {
            std::string errMsg("Failed to open driver");
            DebugPrintf(SV_LOG_ERROR, "%s : error %s %d.\n",
                FUNCTION_NAME, errMsg.c_str(), m_pDriverStream->GetErrorCode());
            return bCheckSupported;
        }
    }

    if (!drvif.GetDriverVersion(m_pDriverStream.get(), stCurrentDrvVersion))
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s : Skipping pname check as filter driver version ioctl failed.\n",
            FUNCTION_NAME);

        return bCheckSupported;
    }

    bHaveDriverVersion = true;

    if ((stCurrentDrvVersion.ulPrMajorVersion < stSupportedDrvVersion.ulPrMajorVersion) ||
        ((stCurrentDrvVersion.ulPrMajorVersion == stSupportedDrvVersion.ulPrMajorVersion) &&
         (stCurrentDrvVersion.ulPrMinorVersion < stSupportedDrvVersion.ulPrMinorVersion)))
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s : Skipping pname check for filter driver version %u.%u\n",
            FUNCTION_NAME, 
            stCurrentDrvVersion.ulPrMajorVersion,
            stCurrentDrvVersion.ulPrMinorVersion);

        return bCheckSupported;
    }

    bCheckSupported = true;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return bCheckSupported;
}

#endif
