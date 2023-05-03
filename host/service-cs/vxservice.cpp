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
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include "inmsafecapis.h"

#ifdef SV_WINDOWS
#include <winioctl.h>
#include "cdpsnapshotrequest.h"
#include "virtualvolume.h"
#include <Windows.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
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
#include "terminateonheapcorruption.h"
#include "hostrecoverymanager.h"

#include "cdpvolume.h"
#include "LogCutter.h"

#include "securityutils.h"
#include "ConsistencySettings.h"
#include "VacpConfDefs.h"
#include "csgetfingerprint.h"

#define DP_NW_ERROR_EXIT_CODE                           62720
#define SV_ERROR_INIT_TELEMETRY_LOG_FAILED              -5

#ifdef SV_WINDOWS
#define DRIVER_SUPPORTS_EXTENDED_VOLUME_STATS           1
#elif SV_UNIX
#define DRIVER_SUPPORTS_EXTENDED_VOLUME_STATS           0x8
#endif

using namespace std;
using namespace Logger;

#define SafeCloseAceHandle(handle, filepath)    if (handle != ACE_INVALID_HANDLE) \
                                                { \
                                                    if (ACE_OS::close(handle) == -1) \
                                                    { \
                                                        DebugPrintf(SV_LOG_ERROR, \
                                                            "%s failed to close handle %#p for %s. Error: %d.\n", \
                                                            FUNCTION_NAME, handle, filepath, ACE_OS::last_error()); \
                                                    } \
                                                    else \
                                                    { \
                                                        handle = ACE_INVALID_HANDLE; \
                                                    } \
                                                }

extern Log gLog;
Log gSrcTelemetryLog;

Configurator* TheConfigurator = 0; // singleton

int const SETTINGS_POLLING_INTERVAL = 5;

bool quit_signalled = false;

bool VxService::m_VsnapCheckRemount=true;
bool VxService::m_VirVolumeCheckRemount=true;
FilterDriverNotifier* VxService::m_sPFilterDriverNotifier = NULL;

static const std::string InMageCacheMgrName = "InMageCacheMgr";
static const int InMageCacheMgrId = -2;

static const std::string InMageCdpMgrName = "InMageCdpMgr";
static const int InMageCdpMgrId = -3;

static const std::string InMageEvtCollForwName = "InMageEvtCollForw";
static const int InMageEvtCollForwId = -4;

static const std::string InMageVacplName = "InMageAgentVacp";
static const int InMageVacpId = -5;

static const std::string CsJobProcessorName = "CsJobProcessor";
static const int CsJobProcessorNameId = -6;

static const std::string InMageSentinelName = "InMageAgentS2";
static const int InMageSentinelId = 0;

static const std::string InMageDataprotectionlName = "InMageAgentDataprotection";

static const long int AGENTSTIME_TOQUIT_INSECS = 5;

#ifndef SV_WINDOWS
#include <boost/thread.hpp>
extern boost::mutex g_MutexForkProcess;
#endif

extern void GetNicInfos(NicInfos_t &nicinfos);

//Configurator* TheConfigurator = NULL;

/** Commenting since already available in common
static bool IsColon( std::string::value_type ch )
{
return ':' == ch;
}
*/

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
static bool SetupLocalLogging(
    boost::shared_ptr<Logger::LogCutter> logCutter,
    std::string &errMessage)
{
    bool rv = false;

    try 
    {
        LocalConfigurator localconfigurator;

        //
        // PR # 6337
        // set the curl_verbose option based on settings in 
        // drscout.conf
        //
        tal::set_curl_verbose(localconfigurator.get_curl_verbose());

        SetDaemonMode();
        boost::filesystem::path logPath(localconfigurator.getLogPathname());
        logPath /= "svagents.log";

        const int maxCompletedLogFiles = localconfigurator.getLogMaxCompletedFiles();
        const boost::chrono::seconds cutInterval(localconfigurator.getLogCutInterval());
        const uint32_t  logFileSize = localconfigurator.getLogMaxFileSize();

        logCutter->Initialize(logPath, maxCompletedLogFiles, cutInterval);

        SetLogLevel( localconfigurator.getLogLevel() );
        SetLogInitSize(logFileSize);

        SetLogHostId( localconfigurator.getHostId().c_str() );
        SetLogRemoteLogLevel(SV_LOG_DISABLE);
        SetSerializeType( localconfigurator.getSerializerType() ) ;
        SetLogHttpsOption(localconfigurator.IsHttps());

        DebugPrintf(SV_LOG_DEBUG,
            "%s svagents max files %d cut interval %d max file size %d\n",
            FUNCTION_NAME,
            maxCompletedLogFiles,
            cutInterval.count(),
            logFileSize);

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

    tal::GlobalInitialize();
    boost::shared_ptr<void> guardTal( static_cast<void*>(0), boost::bind(tal::GlobalShutdown) );

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
        boost::shared_ptr<Logger::LogCutter> logCutter(new Logger::LogCutter(gLog));

        std::string startupError;
        if (!SetupLocalLogging(logCutter, startupError))
        {
            LOG2SYSLOG((LM_ERROR, startupError.c_str()));
            return -1;
        }

        boost::shared_ptr<Logger::LogCutter> telemetryLogCutter(new Logger::LogCutter(gSrcTelemetryLog));;

        LocalConfigurator localconfigurator;
        const int maxCompletedLogFiles = localconfigurator.getLogMaxCompletedFiles();
        const boost::chrono::seconds cutInterval(localconfigurator.getLogCutInterval());
        const uint32_t  logFileSize = localconfigurator.getLogMaxFileSizeForTelemetry();

        boost::filesystem::path tagTelemeteryLogPath(localconfigurator.getLogPathname());
        tagTelemeteryLogPath /= "srcTelemetry.log";
        gSrcTelemetryLog.m_logFileName = tagTelemeteryLogPath.string();
        gSrcTelemetryLog.SetLogFileSize(logFileSize);
        telemetryLogCutter->Initialize(tagTelemeteryLogPath, maxCompletedLogFiles, cutInterval);

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

        try {

            VxService srv;

            if (srv.parse_args(argc, argv) != 0)
            {
                srv.display_usage();
                ACE_OS::exit(-1);
            }

            srv.name(SERVICE_NAME, SERVICE_TITLE, SERVICE_DESC);

            if (srv.opt_install)
                return (srv.install_service());
            if (srv.opt_start)
                return (srv.start_service());

            if (srv.opt_kill)
                return (srv.stop_service());
            if (srv.opt_remove)
                return (srv.remove_service());

            try {

                srv.m_LogCutters.push_back(logCutter);
                srv.m_LogCutters.push_back(telemetryLogCutter);

                if (srv.opt_standalone)
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
    gSrcTelemetryLog.CloseLogFile();
    CloseDebug();
    return(ret);
}

VxService::VxService() : m_bInitialized(false),opt_install (0), opt_remove (0), opt_start (0), 
opt_kill (0), quit(0), opt_standalone(0), m_lastUpdateTime(0), m_configurator(NULL),m_bUninstall(false),
m_switchCx(false), m_bTSChecksEnabled(false), m_bEnableVolumeMonitor(true), m_RegisterHostThread(0), 
m_ProcessMgr(0), m_IsFilterDriverNotified(false), m_bHostMonitorStarted(false), m_MonitorHostThread(0),
#ifdef SV_WINDOWS
    m_ConsistencyThread(0), m_MarsHealthMonitorThread(0)
#else
    m_ConsistencyThread(0)
#endif
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try {
        // Although this is not true, it is performed to delay the beginning of EvtCollForw, as it's
        // a low priority worker process, which shouldn't impact other important processes at startup,
        // when there would already be high utilization.
        m_lastEvtCollForwForkAttemptTime = boost::chrono::steady_clock::now();

        m_lastCsJobProcessorForkAttemptTime = boost::chrono::steady_clock::now();
        //Bug# 8065
        bool callerCli = false;
        if(!CDPUtil::InitQuitEvent(callerCli))
        {
            DebugPrintf( SV_LOG_ERROR, "VxService: Unable to Initialize QuitEvent\n");
        }
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
        StopMarsHealthMonitor();
#endif

        if (m_bEnableVolumeMonitor)
        {
            ((CService*)SERVICE::instance())->getVolumeMonitorSignal().disconnect(this);
        }

        if (NULL != m_ConsistencyThread)
        {
            m_ConsistencyThread->Stop();
            delete m_ConsistencyThread;
            m_ConsistencyThread = NULL;
        }

        if (NULL != m_RegisterHostThread)
        {
          m_RegisterHostThread->Stop();
          if ( NULL != m_configurator)
          {
            m_configurator->getRegisterImmediatelySignal().disconnect(m_RegisterHostThread);
          }
          delete m_RegisterHostThread;
          m_RegisterHostThread = NULL;
        }

        if ( NULL != m_configurator)
        {
            m_configurator->getInitialSettingsSignal().disconnect(this);
            m_configurator->getConfigurationChangeSignal().disconnect(this);
            m_configurator->getFailoverCxSignal().disconnect(this);
            m_configurator->getCsAddressForAzureComponentsChangeSignal().disconnect(this);
            m_configurator->Stop();
            delete m_configurator;
            m_configurator = NULL; //avoiding double deletes.
        }

        StopAgents();

        m_AgentProcesses.clear();
        m_configurator = NULL;
        m_bInitialized = false;

        if (m_ProcessMgr)
        {
            m_ProcessMgr->close();
            delete m_ProcessMgr;
            m_ProcessMgr = NULL;
        }

        SERVICE::instance()->closeJobObjectHandle();
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
    //DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

}

void VxService::display_usage(char *exename)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    const char *progname = ((exename == NULL) ? "<this_exe_name>" : exename);
    printf("Usage: %s -i -r -s -k\n"
        "  -i: Install this program as NT-service/Unix-Daemon process\n"
        "  -r: Remove this program from the Service\n"
        "  -s: Start the service/daemon\n"
        "  -k: Kill the service/daemon\n", progname ); 

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

}

int VxService::parse_args(int argc, ACE_TCHAR* argv[])
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    int iRet = 0;
    for (int index = 1; index < argc; index++)
    {	  
        if (	(ACE_OS::strcasecmp(ACE_TEXT("/RegServer"), argv[index]) == 0) ||
            (ACE_OS::strcasecmp(ACE_TEXT("-RegServer"), argv[index]) == 0) ||
            (ACE_OS::strcasecmp(ACE_TEXT("/Service"), argv[index]) == 0) ||
            (ACE_OS::strcasecmp(ACE_TEXT("-Service"), argv[index]) == 0) ||
            (ACE_OS::strcasecmp(ACE_TEXT("/Install"), argv[index]) == 0) ||
            (ACE_OS::strcasecmp(ACE_TEXT("-Install"), argv[index]) == 0) ||
            (ACE_OS::strcasecmp(ACE_TEXT("/I"), argv[index]) == 0) ||			
            (ACE_OS::strcasecmp(ACE_TEXT("-I"), argv[index]) == 0) )
        {
            opt_install = 1;
            iRet = 0;
        }

        else if (	(ACE_OS::strcasecmp(ACE_TEXT("/UnregServer"), argv[index]) == 0) ||
            (ACE_OS::strcasecmp(ACE_TEXT("-UnregServer"), argv[index]) == 0) ||
            (ACE_OS::strcasecmp(ACE_TEXT("/Remove"), argv[index]) == 0) ||
            (ACE_OS::strcasecmp(ACE_TEXT("-Remove"), argv[index]) == 0) ||
            (ACE_OS::strcasecmp(ACE_TEXT("/R"), argv[index]) == 0) ||
            (ACE_OS::strcasecmp(ACE_TEXT("-R"), argv[index]) == 0) )
        {
            opt_remove = 1;
            iRet = 0;
        }

        else if (	(ACE_OS::strcasecmp(ACE_TEXT("/Start"), argv[index]) == 0) ||
            (ACE_OS::strcasecmp(ACE_TEXT("-Start"), argv[index]) == 0) ||
            (ACE_OS::strcasecmp(ACE_TEXT("/S"), argv[index]) == 0) ||
            (ACE_OS::strcasecmp(ACE_TEXT("-S"), argv[index]) == 0) )
        {
            opt_start = 1;
            iRet = 0;
        }

        else if (	(ACE_OS::strcasecmp(ACE_TEXT("/Kill"), argv[index]) == 0) ||
            (ACE_OS::strcasecmp(ACE_TEXT("-Kill"), argv[index]) == 0) ||
            (ACE_OS::strcasecmp(ACE_TEXT("/Stop"), argv[index]) == 0) ||
            (ACE_OS::strcasecmp(ACE_TEXT("-Stop"), argv[index]) == 0) ||
            (ACE_OS::strcasecmp(ACE_TEXT("/K"), argv[index]) == 0) ||			
            (ACE_OS::strcasecmp(ACE_TEXT("-K"), argv[index]) == 0) )
        {
            opt_kill = 1;
            iRet = 0;
        }

        else if ((ACE_OS::strcasecmp(ACE_TEXT("/Standalone"), argv[index]) == 0) ||
            (ACE_OS::strcasecmp(ACE_TEXT("-Standalone"), argv[index]) == 0))
        {
            opt_standalone = 1;
            iRet = 0;
        }

        else
            iRet = (-1);
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return iRet;
}

int VxService::install_service()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    int ret = -1;

    do
    {
        if (SERVICE::instance ()->isInstalled() == true)
        {
            ret = 0;
            break;
        }

        ret = SERVICE::instance ()->install_service();  

    } while (false);

    if (ret != 0)
    {
        LOG_EVENT((LM_ERROR, "Service: %s could not be installed", SERVICE_NAME));
#ifdef SV_WINDOWS
        ::MessageBox(NULL, "Service could not be installed", ACE_TEXT_ALWAYS_CHAR(SERVICE_NAME), MB_OK);
#endif /* SV_WINDOWS */
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return ret;
}

int VxService::remove_service()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    int ret = -1;

    do
    {
        if (SERVICE::instance ()->isInstalled() == false)
        {
            ret = 0;
            break;
        }

        ret = SERVICE::instance ()->remove_service();  

    } while (false);

    if (ret != 0)
    {
        LOG_EVENT((LM_ERROR, "Service: %s could not be deleted", SERVICE_NAME));
#ifdef SV_WINDOWS
        MessageBox(NULL, "Service could not be deleted", ACE_TEXT_ALWAYS_CHAR(SERVICE_NAME), MB_OK);
#endif /* SV_WINDOWS */
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return ret;
}

int VxService::start_service()
{
    return (SERVICE::instance ()->start_service());  
}

int VxService::stop_service()
{
    return (SERVICE::instance ()->stop_service());  
}

int VxService::run_startup_code()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try {
        DebugPrintf( SV_LOG_DEBUG, "run_startup_code 1\n" );
        SERVICE::instance()->RegisterStartCallBackHandler(VxService::StartWork, this);
        SERVICE::instance()->RegisterStopCallBackHandler(VxService::StopWork, this);
        //	    SERVICE::instance()->RegisterServiceEventHandler(VxService::ServiceEventHandler, this);
        DebugPrintf( SV_LOG_DEBUG,"run_startup_code 3\n" );
    }
    catch( std::string const& e ) {
        DebugPrintf( SV_LOG_ERROR,"run_startup_code: exception %s\n", e.c_str() );
    }
    catch( char const* e ) {
        DebugPrintf( SV_LOG_ERROR,"run_startup_code: exception %s\n", e );
    }
    catch( exception const& e ) {
        DebugPrintf( SV_LOG_ERROR,"run_startup_code: exception %s\n", e.what() );
    }
    catch(...) {
        DebugPrintf( SV_LOG_ERROR,"run_startup_code: exception...\n" );
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return (SERVICE::instance ()->run_startup_code());	
}

void VxService::name(const ACE_TCHAR *name, const ACE_TCHAR *long_name, const ACE_TCHAR *desc)
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



int VxService::StartWork(void *data)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

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

        //
        // If its a test failover VM then exit the service.
        //
        if (!srv->quit && IsItTestFailoverVM())
        {
            DebugPrintf(SV_LOG_INFO, "This is a Test Failover Machine. Service will be stoped\n");
            StopWork(data);
        }

        // 
        // Check for recovrey progress.
        //
        bool shouldSetLogSettingsAgain = false;
        while (!srv->quit && HostRecoveryManager::IsRecoveryInProgress(CDPUtil::QuitRequested))
        {
            shouldSetLogSettingsAgain = true;
            DebugPrintf(SV_LOG_DEBUG,"Recovery is in progress. Wait for 2sec and check recovery status again.\n");
            //
            // Wait for 2sec and check recovery progress again.
            //
            ACE_OS::sleep(2);
        }

        std::list < boost::shared_ptr < Logger::LogCutter > >::iterator it = srv->m_LogCutters.begin();
        while (it != srv->m_LogCutters.end())
        {
            BOOST_ASSERT(*it);

            if (*it)
                (*it)->StartCutting();

            it++;
        }

        while( !srv->quit)
        {
            try {
                //
                // if switch to remote cx was requested, do the switch
                //
                if(srv ->m_switchCx)
                {
                    srv -> switchcx();
                }

                if ( SV_SUCCESS == srv->init() )
                {
                    //creating jobobject for all the child process of svagents,so that when svagents crash all the child processes will be terminated
                    if (SERVICE::instance()->InitializeJobObject() != true)
                    {
                        DebugPrintf(SV_LOG_ERROR, "StartWork: InitializeJobObject is failed \n");
                    }

                    srv->StartMonitors();

                    LocalConfigurator lc;
                    {
                        if (lc.isMobilityAgent())
                        {
                            // moving the driver notify check to begining as otherwise the reason to mark
                            // a resync when driver is not loaded will be lost because of other conditions
                            // like volume caches not available, some error in discovering the disk, ip, hypervisor etc.
                            // though the driver name should be based on the actual device type in settings
                            // for both v2a and a2a only disk based filter driver is used hence using DISK
                            srv->PerformSourceTasks(
                                srv->m_FilterDrvIf.GetDriverName(VolumeSummary::DISK),
                                lc);
                        }
                    }

#ifdef SV_WINDOWS
                    if (lc.isMasterTarget())
                    {
                        std::string mtSupportedDataPlanes = lc.getMTSupportedDataPlanes();
                        std::size_t foundAzureDataPlane = mtSupportedDataPlanes.find("AZURE_DATA_PLANE");
                        if (foundAzureDataPlane != string::npos)
                        {
                            srv->StartMarsHealthMonitor();
                        }
                    }
#endif

                    srv->StartRegistrationTask();

                    //Removed by Ranjan bug#10404
                    //srv -> RegisterXenInfo();
                    
                    srv->StartConsistenyThread();

                    srv->ProcessMessageLoop();

                    srv->CleanUp();
                }
            }
            catch( std::string const& e ) {
                DebugPrintf(SV_LOG_ERROR, "%s: caught an exception %s\n", FUNCTION_NAME, e.c_str());
                ACE_OS::sleep(5); //Putting this sleep because if the cx ip is valid but there is no cx installed. this infinite loop will result in high cpu usage.
                srv->CleanUp();
            }
            catch( char const* e ) {
                DebugPrintf(SV_LOG_ERROR, "%s: caught an exception %s\n", FUNCTION_NAME, e);
                ACE_OS::sleep(5);
                srv->CleanUp();
            }
            catch( exception const& e ) {
                DebugPrintf(SV_LOG_ERROR, "%s: caught an exception %s\n", FUNCTION_NAME, e.what());
                ACE_OS::sleep(5);
                srv->CleanUp();
            }
            catch(...) {
                DebugPrintf(SV_LOG_ERROR, "%s: caught an unknown exception.\n", FUNCTION_NAME);
                ACE_OS::sleep(5);
                srv->CleanUp();
            }
        }
    }
    catch (const HostRecoveryException& exp)
    {
        DebugPrintf(SV_LOG_ERROR, 
            "FAILED: Recovery exception while starting service. Exception Message: %s\n",
            exp.what());
    }
    catch(...)
    {
        DebugPrintf(SV_LOG_ERROR,"FAILED: An error occurred while starting service.\n");
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return 0;
}

///
/// Pump messages until service decides to stop. Restarts any agent process that has exited.
/// Before restarting any process, fetches the most recent settings from the CX server.
///
void VxService::ProcessMessageLoop()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {
        if(m_VirVolumeCheckRemount)
        {
            VirVolume object;
            string volume="";
            object.VirVolumeCheckForRemount(volume,false);
            m_VirVolumeCheckRemount = false;	// Done only once for the first time.
        }

        if( m_VsnapCheckRemount && IsVsnapDriverAvailable() )
        {
            // VsnapLocalPersistence=1(by default) which enables CX independent persitence
            // of vsnaps. This can be turned off by setting to 0. In this case
            // CX is used to get the list of completed vsnaps. CLI vsnaps will be lost



#ifdef SV_WINDOWS
            WinVsnapMgr mgr;
#else
            UnixVsnapMgr mgr;
#endif
            VsnapMgr* Vsnap = &mgr;

            LocalConfigurator localConfigurator;
            if(localConfigurator.isVsnapLocalPersistenceEnabled())
            {
                Vsnap->RemountVsnapsFromPersistentStore();
            }
            else
            {
                Vsnap->RemountVsnapsFromCX();
            }
            m_VsnapCheckRemount = false;	// Done only once for the first time.
        }
    } 
    catch(...) 
    {
        DebugPrintf(SV_LOG_ERROR,
            "FAILED: Unable to remount virtual snapshot volumes. @LINE %d in FILE %s\n",LINE_NO,FILE_NAME);
    }

    LocalConfigurator lc;

    while( !this->quit && !m_switchCx )
    {
        try 
        {
            if ( !isInitialized())
                this->init();

            DebugPrintf( SV_LOG_DEBUG,"Polling for CX settings change\n" );

            if ( isInitialized() )
            {
                StopFiltering();

                if (lc.isMasterTarget())
                {
                    if (lc.getCacheManagerStartStatus())
                    {
                        StartCacheManger(lc);
                    }
                    if (lc.getCDPManagerStartStatus())
                    {
                        StartCdpManger(lc);
                    }
                }

                StartVolumeGroups(lc);

                StartPrismAgents(lc);

                //Start sentinel only if filter driver was notified
                if (m_IsFilterDriverNotified && lc.getSentinalStartStatus())
                {
                    StartSentinel();

                    StartVacp(lc);
                }

                LogSourceReplicationStatus();

                // Specific for V2A. Shouldn't be run for V2V.
                StartEvtCollForw(lc);

                StartCsJobProcessor(lc);
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
                for( int i = 0; i < SETTINGS_POLLING_INTERVAL && !quit; ++i ) 
                {
                    ACE_OS::sleep( 1 );
                }
            }
        }
        catch( std::string const& e ) 
        {
            DebugPrintf(SV_LOG_ERROR, "%s: caught an exception: %s\n", FUNCTION_NAME, e.c_str());
            throw e;
        }   
        catch( char const* e ) 
        {
            DebugPrintf(SV_LOG_ERROR, "%s: caught an exception: %s\n", FUNCTION_NAME, e);
            throw e;
        }
        catch (std::exception const &e)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: caught an exception: %s\n", FUNCTION_NAME, e.what());
            throw e;
        }
        catch(...) 
        {
            DebugPrintf(SV_LOG_ERROR, "%s: caught an unknown exception.\n", FUNCTION_NAME);
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

    vs->quit = 1;
    quit_signalled = true;
    CDPUtil::SignalQuit();
    return 0;
}

int VxService::handle_exit (ACE_Process *proc)
{
    // Do not call LOG_EVENT in this function. 
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    pid_t pid = proc->getpid();
    ACE_exitcode agentExitCode = proc->exit_code();
    AgentProcess::Ptr agentInfo;
    AgentProcesses_t::const_iterator iter = this->m_AgentProcesses.begin();
    AgentProcesses_t::const_iterator endIter = this->m_AgentProcesses.end();

    for( /* empty */ ; (iter != endIter); iter++ ) 
    {
        if( (*iter)->ProcessCreated() && pid == (*iter)->ProcessHandle())
        {
            if(quit)
            {
                (*iter)->markDead();
                DebugPrintf(SV_LOG_DEBUG, "process %s with pid %d terminated. marking the Agent Process as dead.\n",(*iter)->ProcessName().c_str(), pid);
                m_AgentProcesses.erase(*iter);
                break;
            }
            else
            {
#ifdef SV_UNIX
                if (agentExitCode == DP_NW_ERROR_EXIT_CODE)
#else 
                if (agentExitCode == TRANSPORT_ERROR)
#endif
                {
                    ACE_Guard<ACE_Mutex> lastPSLogTimeGuard(m_lastPSTransErrLogTimeLock);
                    std::map<std::string, ACE_Time_Value>::iterator agentsMapItr = m_agentsExitStatusMap.find((*iter)->DeviceName());
                    if (agentsMapItr == m_agentsExitStatusMap.end())
                    {
                        m_agentsExitStatusMap[(*iter)->DeviceName()] = ACE_OS::gettimeofday();
                        agentsMapItr = m_agentsExitStatusMap.find((*iter)->DeviceName());
                    }
                    ACE_Time_Value currentTime = ACE_OS::gettimeofday();
                    if ((currentTime.sec() - agentsMapItr->second.sec()) > m_configurator->getVxAgent().getTransportErrorLogInterval())
                    {
                        m_agentsExitStatusMap.erase(agentsMapItr);
                    }
                }
                else
                {
                    SV_LOG_LEVEL logLevel = SV_LOG_ALWAYS;
                    if (agentExitCode != 0)
                    {
                        logLevel = SV_LOG_ERROR;
                    }
                    DebugPrintf(logLevel, "process %s with pid %d exited with exit code %d.\n", (*iter)->ProcessName().c_str(), pid, agentExitCode);
                }
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

// removes one or more instances of option in input
void ProcessVacpOption(std::string& processedOptions, const std::string& option)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool processed = false;

    do {
        size_t optionPos = processedOptions.find(option);
        if (optionPos != string::npos)
        {
            size_t nextOptPos = processedOptions.find("-", optionPos + 1);
            processedOptions.erase(optionPos, nextOptPos == string::npos ? string::npos : nextOptPos - optionPos);
            continue;
        }
        processed = true;
    } while (!processed);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

// removes the deprecated options from vacp options
std::string ProcessVacpOptions(const std::string& cmdOptions)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    std::string processedOptions(cmdOptions);
    ProcessVacpOption(processedOptions, "-prescript");
    ProcessVacpOption(processedOptions, "-postscript");

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return processedOptions;
}

///
/// Starts the consistency process.
/// At most one consistency process running at any time.
///
void VxService::StartVacp(const LocalConfigurator &lc)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    using namespace boost::chrono;
    boost::system::error_code ec;
    static bool bLastCheckedIRMode = false;
    static bool bforceCacheLoad = true;
    const boost::filesystem::path csCachePath(lc.getConsistencySettingsCachePath());
    LocalConfigurator lConf;
    
    try {

        AgentProcess::Ptr agentInfoPtr(new AgentProcess(InMageVacplName, InMageVacpId, InMageVacplName));

        AgentProcesses_t::iterator findIter = m_AgentProcesses.find(agentInfoPtr);
        const bool bVacpRunning = (findIter != m_AgentProcesses.end());

        bool bIsIRMode = false;
        if (!CanStartVacp(bIsIRMode))
        {
            if (bVacpRunning)
            {
                DebugPrintf(SV_LOG_ALWAYS, "%s: stopping vacp as protection is disabled.\n", FUNCTION_NAME);
                StopAgentsHavingAttributes(InMageVacplName, std::string(), InMageVacpId, lc.getVacpExitWaitTime());
            }
            if (lConf.getVacpState() == VacpConf::VACP_STOP_REQUESTED)
            {
                lConf.setVacpState(VacpConf::VACP_STOPPED);
            }

            return;
        }

        if (!boost::filesystem::exists(csCachePath, ec))
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: file %s does not exist. Error: %d\n", FUNCTION_NAME, csCachePath.string().c_str(), ec.value());
            return;
        }

        static std::time_t tcsCacheLastWriteTime = boost::filesystem::last_write_time(csCachePath, ec);

        // fetch the cached settings
        CS::ConsistencySettings cs;

        // Verify restart required on settings change
        const double settingsChangeTimeDiffSeconds = difftime(boost::filesystem::last_write_time(csCachePath, ec), tcsCacheLastWriteTime);

        // BUG 6582380 - svagents killed by SIGBUS
        // there is a race condition in svagents calling VACP to quit and VACP also exiting at the same time.
        // the ACE_Events are memory mapped files on Linux and svagents maps the event in memory to send quit signal.
        // but if VACP has exited while the event still mapped in memory of svagents, in a very narrow time window,
        // a SIGBUS is signalled when the mapped memory is not accessible as event is deleted.
        // adding additional wait time before stopping VACP from service.  If VACP has not quit by then, service will stop it
        // with a message 'max run time elapsed' and you should check why that is the case
        const seconds maxRunTime = seconds(lc.getVacpParallelMaxRunTime()) + seconds(lc.getVacpExitWaitTime());
        const bool restartRequiredAsMaxRunTimeElapsed = (steady_clock::now() > m_lastVacpStartTime + maxRunTime);

        const bool brestartRequired = !bVacpRunning
            || settingsChangeTimeDiffSeconds
            || lConf.getVacpState() == VacpConf::VACP_START_REQUESTED
            || lConf.getVacpState() == VacpConf::VACP_STOP_REQUESTED
            || restartRequiredAsMaxRunTimeElapsed
            || (bLastCheckedIRMode != bIsIRMode);
        
        bLastCheckedIRMode = bIsIRMode;

        // Check update required for vacp.conf
        std::string confDir;
        lc.getConfigDirname(confDir);
        boost::filesystem::path vacpConfPath(confDir);
        vacpConfPath /= VACP_CONF_FILE_NAME;
        if (!boost::filesystem::exists(vacpConfPath, ec))
        {
            bforceCacheLoad = true;
        }

        if (brestartRequired || bforceCacheLoad)
        {
            try
            {
                std::string csCacheLockFile(csCachePath.string());
                csCacheLockFile += ".lck";
                if (!boost::filesystem::exists(csCacheLockFile, ec))
                {
                    std::ofstream tmpLockFile(csCacheLockFile.c_str());
                    tmpLockFile.close();
                }
                boost::interprocess::file_lock fileLock(csCacheLockFile.c_str());
                boost::interprocess::scoped_lock<boost::interprocess::file_lock> fileLockGuard(fileLock);
                std::ifstream hcsInCacheFile(csCachePath.string().c_str());
                if (hcsInCacheFile.good())
                {
                    std::string strcsInCacheFile((std::istreambuf_iterator<char>(hcsInCacheFile)), (std::istreambuf_iterator<char>()));
                    hcsInCacheFile.close();
                    cs = JSON::consumer<CS::ConsistencySettings>::convert(strcsInCacheFile);
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: Failed to open file %s for reading.\n", FUNCTION_NAME, csCachePath.string().c_str());
                    return;
                }
            }
            catch (const std::exception &e)
            {
                DebugPrintf(SV_LOG_ERROR, "%s: Failed to read ConsistencySettings in %s. Exception: %s\n", FUNCTION_NAME, csCachePath.string().c_str(), e.what());
                return;
            }
            catch (...)
            {
                DebugPrintf(SV_LOG_ERROR, "%s: Failed to read ConsistencySettings in %s with unknown exception.\n", FUNCTION_NAME, csCachePath.string().c_str());
                return;
            }

            // if both app and crash consistency intervals are 0, do not start VACP
            // Migration state also need VACP to be stopped
            bool vacpStopRequired = (lConf.getVacpState() == VacpConf::VACP_STOP_REQUESTED) ||
                (lConf.getVacpState() == VacpConf::VACP_STOPPED);
            if ((!cs.m_CrashConsistencyInterval && !cs.m_AppConsistencyInterval) || vacpStopRequired)
            {
                // Application agent may have cleaned up old instance of consistencysettings.json as part of switch to old flow.
                // Stop vacp and return.
                if (vacpStopRequired)
                {
                    DebugPrintf(SV_LOG_DEBUG, "%s : VACP current state : %d.\n", FUNCTION_NAME,
                        lConf.getVacpState());
                }
                else
                {
                    DebugPrintf(SV_LOG_DEBUG, "%s : Consistency intervals are 0.\n", FUNCTION_NAME);
                }

                if (bVacpRunning)
                {
                    if (vacpStopRequired)
                    {
                        DebugPrintf(SV_LOG_ALWAYS, "%s: stopping VACP as current state is : %d.\n",
                            FUNCTION_NAME, lConf.getVacpState());
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_ALWAYS,
                            "%s: stopping VACP as consistency intervals are 0.\n", FUNCTION_NAME);
                    }
                    StopAgentsHavingAttributes(InMageVacplName, std::string(),
                        InMageVacpId, lc.getVacpExitWaitTime());
                }
                if (lConf.getVacpState() == VacpConf::VACP_STOP_REQUESTED)
                {
                    lConf.setVacpState(VacpConf::VACP_STOPPED);
                }
                return;
            }

            if (brestartRequired)
            {
                tcsCacheLastWriteTime = boost::filesystem::last_write_time(csCachePath, ec);
            }

            std::string vacpConfLockFile(vacpConfPath.string());
            vacpConfLockFile += ".lck";
            if (!boost::filesystem::exists(vacpConfLockFile, ec))
            {
                std::ofstream tmpLockFile(vacpConfLockFile.c_str());
            }
            boost::interprocess::file_lock vacpConfFileLock(vacpConfLockFile.c_str());
            boost::interprocess::scoped_lock<boost::interprocess::file_lock> fileLockGuard(vacpConfFileLock);

            std::ofstream ofstr(vacpConfPath.string().c_str(), std::ofstream::trunc);
            std::stringstream ss;
            ss << VACP_CONF_PARAM_CRASH_CONSISTENCY_INTERVAL << "=" << cs.m_CrashConsistencyInterval << std::endl;
            ss << VACP_CONF_PARAM_APP_CONSISTENCY_INTERVAL << "=" << cs.m_AppConsistencyInterval << std::endl;
            ss << VACP_CONF_PARAM_EXIT_TIME << "=" << lc.getVacpParallelMaxRunTime() << std::endl;

            ss << VACP_CONF_PARAM_DRAIN_BARRIER_TIMEOUT << "=" << lc.getVacpDrainBarrierTimeout() << std::endl;
            ss << VACP_CONF_PARAM_TAG_COMMIT_MAX_TIMEOUT << "=" << lc.getVacpTagCommitMaxTimeOut() << std::endl;
            long appConsistencyServerResymeAckWaitTime = lc.getVacpDrainBarrierTimeout() + VACP_SERVER_RESUMEACK_TIMEDELTA;
            ss << VACP_CONF_PARAM_APPCONSISTENCY_SERVER_RESUMEACK_WAITTIME << "=" << appConsistencyServerResymeAckWaitTime << std::endl;

            std::string    excludedVolumes = lc.getApplicationConsistentExcludedVolumes();
            if (!excludedVolumes.empty()) {
                ss << VACP_CONF_EXCLUDED_VOLUMES << "=" << excludedVolumes << std::endl;
            }

            DebugPrintf(SV_LOG_ALWAYS, "%s: Adding consistency policy input to %s: %s \n", FUNCTION_NAME, vacpConfPath.string().c_str(), ss.str().c_str());
            ofstr << ss.str().c_str();
            ofstr.close();
        }

        bforceCacheLoad = false;

        if (brestartRequired)
        {
            // Construct arguments
            std::string vacpArgs = " -systemlevel -parallel -vtoa ";
            if (bLastCheckedIRMode)
            {
                vacpArgs += " -irmode ";
            }

            // starting 9.47 release, CS is not sending the prescript to run
            // remove -prescript or -postscript options if added
            vacpArgs += " ";
            vacpArgs += ProcessVacpOptions(cs.m_CmdOptions); // crash tag has "-cc" as extra param and rest options are common for app and crash

#ifdef SV_UNIX
            // add prescript for Linux
            vacpArgs += " -prescript ";
            vacpArgs += lc.getInstallPath();
            vacpArgs += ACE_DIRECTORY_SEPARATOR_CHAR;
            vacpArgs += "scripts";
            vacpArgs += ACE_DIRECTORY_SEPARATOR_CHAR;
            vacpArgs += "vCon";
            vacpArgs += ACE_DIRECTORY_SEPARATOR_CHAR;
            vacpArgs += "AzureDiscovery.sh ";
#endif

            if (bVacpRunning)
            {
                DebugPrintf(SV_LOG_ALWAYS, "%s: stopping vacp on %s \n", FUNCTION_NAME,
                    !restartRequiredAsMaxRunTimeElapsed ? "consistency settings change" : "max run time elapsed");
                StopAgentsHavingAttributes(InMageVacplName, std::string(), InMageVacpId, lc.getVacpExitWaitTime());
            }

            const std::string vacpCmd = (boost::filesystem::path(lc.getInstallPath()) / INM_VACP_SUBPATH).string();

            DebugPrintf(SV_LOG_ALWAYS, "%s: forking vacp %s %s\n", FUNCTION_NAME, vacpCmd.c_str(), vacpArgs.c_str());
            if (!StartProcess(m_ProcessMgr, agentInfoPtr, vacpCmd.c_str(), vacpArgs.c_str()))
            {
                DebugPrintf(SV_LOG_ERROR, "FAILED %s: vacp did not start. It will be retried.\n", FUNCTION_NAME);
            }
            else
            {
                DebugPrintf(SV_LOG_ALWAYS, "%s: vacp successfully started, processid=%ld\n", FUNCTION_NAME, agentInfoPtr->ProcessHandle());
                m_AgentProcesses.insert(agentInfoPtr);

                m_lastVacpStartTime = boost::chrono::steady_clock::now();
                if (lConf.getVacpState() == VacpConf::VACP_START_REQUESTED)
                {
                    lConf.setVacpState(VacpConf::VACP_STARTED);
                }
            }
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
void VxService::StartSentinel()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try {         
        std::stringstream args;

        args << " svagents";

        AgentProcess::Ptr agentInfoPtr(new AgentProcess(InMageSentinelName, InMageSentinelId, InMageSentinelName)); // InMageSentinel is id for the sentinel 

        AgentProcesses_t::iterator findIter = m_AgentProcesses.find(agentInfoPtr);

        if (findIter == m_AgentProcesses.end()) 
        {
            if (!StartProcess(m_ProcessMgr, agentInfoPtr, m_configurator->getVxAgent().getDiffSourceExePathname().c_str(), args.str().c_str())) {
                DebugPrintf( SV_LOG_ERROR, "FAILED StartSentinel: sentinel did not start\n");
            } else {    
                m_AgentProcesses.insert(agentInfoPtr);
            }
        }
        else
        {
            DebugPrintf( SV_LOG_INFO, "StartSentinel: sentinel is running with pid %d.\n",(*findIter)->ProcessHandle());
        }
    }
    catch( std::string const& e ) {
        DebugPrintf( SV_LOG_ERROR,"StartSentinel: exception %s\n", e.c_str() );
    }
    catch( char const* e ) {
        DebugPrintf( SV_LOG_ERROR,"StartSentinel: exception %s\n", e );
    }
    catch( exception const& e ) {
        DebugPrintf( SV_LOG_ERROR,"StartSentinel: exception %s\n", e.what() );
    }
    catch(...) {
        DebugPrintf( SV_LOG_ERROR,"StartSentinel: exception...\n" );
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

        // TODO-SanKumar-1704: This serves as the validator for the config value set by the installer.
        // Ensure from Kusto that this error log has never been found in V2A and A2A admin log tables
        // and then remove this. Then, instead of hard coding the A2A/V2A source cmd line arg, the
        // arg value must be decided using this configuration value at run time.
        static bool checkedVmPlatform = false;
        if (!checkedVmPlatform)
        {
            std::string vmPlatform = lc.getVmPlatform();
            const char ExpectedVmPlatform[] = "VmWare";
            if (vmPlatform.compare(ExpectedVmPlatform))
            {
                DebugPrintf(SV_LOG_ERROR,
                    "Unexpected Vm Platform : %s from configuration. Expected : %s\n.", vmPlatform.c_str(), ExpectedVmPlatform);
            }

            checkedVmPlatform = true;
        }

        std::stringstream args;

        if (lc.isMobilityAgent() || lc.isMasterTarget())
        {
            args << ' ' << EvtCollForw_CmdOpt_Environment << ' ';
            if (lc.isMobilityAgent())
                args << EvtCollForw_Env_V2ASource;
            else
                args << EvtCollForw_Env_V2APS;
        }

        AgentProcess::Ptr agentInfoPtr(new AgentProcess(InMageEvtCollForwName, InMageEvtCollForwId, InMageEvtCollForwName));

        AgentProcesses_t::iterator findIter = m_AgentProcesses.find(agentInfoPtr);

        if (findIter == m_AgentProcesses.end())
        {
            const char* EvtCollForwSubPath =
#ifdef SV_WINDOWS
                "evtcollforw.exe";
#else
                "bin/evtcollforw";
#endif

            // TODO-SanKumar-1702: Add this value to the configuration.
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

///
/// Starts the csjobprocessor process, cleaning up any old handles if necessary.
/// At most one csjobprocessor process running at any time.
/// For now, the csjobprocessor needs to run only on windows master target.
///
void VxService::StartCsJobProcessor(LocalConfigurator &lc)
{
#ifdef SV_WINDOWS

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    try {
        using namespace boost::chrono;
        steady_clock::time_point currTime = steady_clock::now();
        steady_clock::duration retryInterval = seconds(lc.getCsJobProcessorProcSpawnInterval());
        if (currTime < m_lastCsJobProcessorForkAttemptTime + retryInterval)
        {
            DebugPrintf(
                SV_LOG_INFO,
                "CsJobProcessor was started (or at least attempted) within the last retry interval : %lld.\n",
                m_lastCsJobProcessorForkAttemptTime.time_since_epoch().count());
            goto Cleanup;
        }
        m_lastCsJobProcessorForkAttemptTime = currTime;


        if (!lc.isMasterTarget())
        {
            DebugPrintf(SV_LOG_INFO, "Not starting CsJobProcessor as this is not master target.\n");
            goto Cleanup;
        }

        if (!m_configurator->getInitialSettings().AnyJobsAvailableForProcessing())
        {
            DebugPrintf(SV_LOG_INFO,"No Jobs to process.\n");
            goto Cleanup;
        }

        AgentProcess::Ptr agentInfoPtr(new AgentProcess(CsJobProcessorName, CsJobProcessorNameId, CsJobProcessorName));

        AgentProcesses_t::iterator findIter = m_AgentProcesses.find(agentInfoPtr);

        if (findIter == m_AgentProcesses.end())
        {
            const char* CsJobProcessorSubPath = "cdpcli.exe";

            const std::string CsJobProcessorExeName =
                (boost::filesystem::path(lc.getInstallPath()) / CsJobProcessorSubPath).string();

            std::stringstream args;
            args << " --processcsjobs --loglevel=7";

            if (!StartProcess(m_ProcessMgr, agentInfoPtr, CsJobProcessorExeName.c_str(), args.str().c_str())) {
                DebugPrintf(
                    SV_LOG_ERROR, 
                    "FAILED StartCsJobProcessor: %s %s did not start.\n",
                    CsJobProcessorExeName.c_str(),
                    args.str().c_str());
            }
            else {
                m_AgentProcesses.insert(agentInfoPtr);
            }
        }
        else
        {
            DebugPrintf(
                SV_LOG_INFO,
                "StartCsJobProcessor: CsJobProcessor is running with pid %d.\n",
                (*findIter)->ProcessHandle());
        }
    }
    catch (std::string const& e) {
        DebugPrintf(SV_LOG_ERROR, "StartCsJobProcessor: exception %s\n", e.c_str());
    }
    catch (char const* e) {
        DebugPrintf(SV_LOG_ERROR, "StartCsJobProcessor: exception %s\n", e);
    }
    catch (exception const& e) {
        DebugPrintf(SV_LOG_ERROR, "StartCsJobProcessor: exception %s\n", e.what());
    }
    catch (...) {
        DebugPrintf(SV_LOG_ERROR, "StartCsJobProcessor: exception...\n");
    }

Cleanup:
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

#endif
}

///
/// Starts the cachemanager process, cleaning up any old handles if necessary.
/// At most one cachemgr process running at any time.
///
void VxService::StartCacheManger(LocalConfigurator &localConfigurator)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try 
    {         
        std::stringstream args;

        AgentProcess::Ptr agentInfoPtr(new AgentProcess(InMageCacheMgrName, InMageCacheMgrId, InMageCacheMgrName));
        AgentProcesses_t::iterator findIter = m_AgentProcesses.find(agentInfoPtr);

        if (findIter == m_AgentProcesses.end()) 
        {
            if (!StartProcess(m_ProcessMgr, agentInfoPtr, localConfigurator.getCacheMgrExePathname().c_str(), 
                args.str().c_str())) 
            {
                DebugPrintf( SV_LOG_ERROR, "%s encountered error (%d)\n",
                    FUNCTION_NAME,  ACE_OS::last_error());

            } else 
            {    
                m_AgentProcesses.insert(agentInfoPtr);
            }
        }
        else
        {
            DebugPrintf( SV_LOG_INFO, "%s: cachemgr is running with pid %d.\n",
                FUNCTION_NAME, (*findIter)->ProcessHandle());
        }
    }

    catch( std::string const& e ) 
    {
        DebugPrintf( SV_LOG_ERROR,"%s encountered exception %s\n", FUNCTION_NAME, e.c_str() );
    }
    catch( char const* e ) 
    {
        DebugPrintf( SV_LOG_ERROR,"%s encountered exception %s\n",FUNCTION_NAME, e );
    }
    catch( exception const& e ) 
    {
        DebugPrintf( SV_LOG_ERROR,"%s encountered exception %s\n",FUNCTION_NAME, e.what() );
    }
    catch(...) 
    {
        DebugPrintf( SV_LOG_ERROR,"%s encountered unknown exception.\n" );
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

///
/// Starts the cdpmanager process, cleaning up any old handles if necessary.
/// At most one cachemgr process running at any time.
///
void VxService::StartCdpManger(LocalConfigurator &localConfigurator)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try 
    {         
        std::stringstream args;

        AgentProcess::Ptr agentInfoPtr(new AgentProcess(InMageCdpMgrName, InMageCdpMgrId, InMageCdpMgrName));
        AgentProcesses_t::iterator findIter = m_AgentProcesses.find(agentInfoPtr);

        if (findIter == m_AgentProcesses.end()) 
        {
            if (!StartProcess(m_ProcessMgr, agentInfoPtr, localConfigurator.getCdpMgrExePathname().c_str(), 
                args.str().c_str())) 
            {
                DebugPrintf( SV_LOG_ERROR, "%s encountered error (%d)\n",
                    FUNCTION_NAME,  ACE_OS::last_error());

            } else 
            {    
                m_AgentProcesses.insert(agentInfoPtr);
            }
        }
        else
        {
            DebugPrintf( SV_LOG_INFO, "%s: cdpmgr is running with pid %d.\n",
                FUNCTION_NAME, (*findIter)->ProcessHandle());
        }
    }

    catch( std::string const& e ) 
    {
        DebugPrintf( SV_LOG_ERROR,"%s encountered exception %s\n", FUNCTION_NAME, e.c_str() );
    }
    catch( char const* e ) 
    {
        DebugPrintf( SV_LOG_ERROR,"%s encountered exception %s\n",FUNCTION_NAME, e );
    }
    catch( exception const& e ) 
    {
        DebugPrintf( SV_LOG_ERROR,"%s encountered exception %s\n",FUNCTION_NAME, e.what() );
    }
    catch(...) 
    {
        DebugPrintf( SV_LOG_ERROR,"%s encountered unknown exception.\n" );
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


// --------------------------------------------------------------------------
// Checks protection state
// @IsIRMode: set flag IsIRMode if any of protected disk is in IR/resync mode
// Returns true if protection is enabled otherwise false
// --------------------------------------------------------------------------
bool VxService::CanStartVacp(bool &IsIRMode) const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    bool bretval = true;

    HOST_VOLUME_GROUP_SETTINGS hostVolumeGroupSettings = m_configurator->getHostVolumeGroupSettings();

    HOST_VOLUME_GROUP_SETTINGS::volumeGroups_constiterator citrVolGroups = hostVolumeGroupSettings.volumeGroups.begin();

    HOST_VOLUME_GROUP_SETTINGS::volumeGroups_constiterator citrVolGroupsEnd = hostVolumeGroupSettings.volumeGroups.end();

    if (citrVolGroups == citrVolGroupsEnd)
    {
        bretval = false;
    }

    for (/*empty*/; citrVolGroups != citrVolGroupsEnd; citrVolGroups++)
    {
        VOLUME_GROUP_SETTINGS::volumes_constiterator citrVolumes = citrVolGroups->volumes.begin();
        if (citrVolumes != citrVolGroups->volumes.end())
        {
            if (VOLUME_SETTINGS::PAIR_PROGRESS != citrVolumes->second.pairState)
            {
                bretval = false;
                break;
            }
            else if ((VOLUME_SETTINGS::SYNC_DIFF != citrVolumes->second.syncMode) ||
                (VOLUME_SETTINGS::SYNC_QUASIDIFF != citrVolumes->second.syncMode))
            {
                if (VOLUME_SETTINGS::SYNC_FAST_TBC == citrVolumes->second.syncMode)
                {
                    IsIRMode = true;
                }
            }
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bretval;
}


// --------------------------------------------------------------------------
// starts target volume groups 
// NOTE: currently only a diff sync will actually start a true volume group
// fast or offload sync and snapshot only will still invoke 1 process
// per target as they always have
// --------------------------------------------------------------------------
void VxService::StartVolumeGroups(LocalConfigurator &localConfigurator)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try 
    {
        AgentProcess::Ptr agentInfoPtr;

        AgentProcesses_t::iterator findIter;  
        

        vector<std::string> diffTargetVolumes; // track all the volumes that have diff sync set
        vector<std::string> syncTargetVolumes; // track all the volumes that have fast or offload sync

        static bool isxen = isXen();
        bool addVolumeGroup  = false;

        HOST_VOLUME_GROUP_SETTINGS hostVolumeGroupSettings = m_configurator->getHostVolumeGroupSettings();

        VOLUME_GROUP_SETTINGS::volumes_iterator volumeIter;
        VOLUME_GROUP_SETTINGS::volumes_iterator volumeEnd;

        HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator volumeGroupIter = 
            hostVolumeGroupSettings.volumeGroups.begin();

        HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator volumeGroupEnd = 
            hostVolumeGroupSettings.volumeGroups.end();

        bool iscachedirchecked = false;
        for (/* empty */; volumeGroupIter != volumeGroupEnd; ++volumeGroupIter) 
        {
            addVolumeGroup = false;     
            volumeIter = (*volumeGroupIter).volumes.begin();
            volumeEnd = (*volumeGroupIter).volumes.end();

            if(quit)
            {
                DebugPrintf(SV_LOG_DEBUG, "Received quit request ....\n");
                break;
            }

            for (/* empty*/; volumeIter != volumeEnd; ++volumeIter) 
            {
                /* Bug : 8017 (move retention) start
                1. if the state is in pause pending state, stop dp and send response to cx
                2. if the state in pause state, then do maintenance action if any action requested
                */
                std::string deviceName = (*volumeIter).second.deviceName;
                VOLUME_SETTINGS::PAIR_STATE state = (*volumeIter).second.pairState;
                std::string maintenance_action = (*volumeIter).second.maintenance_action;
                /**
                * 1 TO N sync: Get the target end point
                * Need not go through the end point list since 
                * only one end point will be there now
                */
                std::string endpointDeviceName;
                VOLUME_SETTINGS::endpoints_iterator endPointsIter = 
                    volumeIter->second.endpoints.begin();
                endpointDeviceName = endPointsIter->deviceName;

                if (SOURCE == (*volumeGroupIter).direction) {
                    //Notify filter driver and start sentinel
                    if (!PerformSourceTasks(m_FilterDrvIf.GetDriverName((*volumeIter).second.devicetype), localConfigurator)) {
                        /*
                         * Do not process current settings in case of failure. This is needed not to fork dp.
                         * Otherwise dp will issue start filtering, rsn etc,. without shutdown notify.
                         */
                        continue;
                    }
                }

                /**
                * 1 TO N sync: TODO:
                * ===========
                * Changes:
                * =======
                * 1. dataprotection is creating a lock by name of local deviceName
                *    But for more than one dataprotection to run, the lock has
                *    to contain target name. Hence changed that only for source
                * 2. TODO: dataprotection should delete the resync logs, when transition
                *          to step 2 happens successfully. Remove that code from service.
                *          --> Need to discuss about the dataprotection diff sync log (Who will delete?)
                *          --> Finding out the resync logs, and to find dataprotection running, 
                *              added syncmode and the direction and the endpointDeviceName.
                *          Need to understand pause and pair deletion cases 
                * 
                */
                if(VOLUME_SETTINGS::PAUSE_PENDING == state)
                {
                    PerformPendingPauseAction(localConfigurator,
                        deviceName,
                        endpointDeviceName,
                        (*volumeGroupIter).direction,
                        (*volumeGroupIter).id,
                        maintenance_action,
                        volumeIter->second.syncMode,
                        volumeIter->second.GetResyncJobId());
                }
                else
                {
                    unsigned long long nRemovedElems = (unsigned long long)m_pausePendingAckMap.erase(deviceName);

                    if (nRemovedElems > 0)
                    {
                        DebugPrintf(SV_LOG_ALWAYS,
                            "Removed %llu key = %s from pause pending ack map, since its current state has moved to %d.\n",
                            nRemovedElems, deviceName.c_str(), state);
                    }
                }

                //code for bug 8017 ends

                //code for bug 8719
                //when pair is on delete pending deleting the dataprotection log
                /**
                * 1 to N sync: Will DELETE_PENDING come to source? looks like yes
                *              but only action is to delete the logs? What about the
                *              other actions that target does? States have to be 
                *              changed for source too if source receives DELETE_PENDING
                */
                if(VOLUME_SETTINGS::DELETE_PENDING == (*volumeIter).second.pairState)
                {
                    bool retvalue = true;
                    if(!IsDeviceInUse(
                        (*volumeIter).second.deviceName,
                        endpointDeviceName,
                        (*volumeGroupIter).direction,
                        (*volumeGroupIter).id,
                        (*volumeIter).second.syncMode  
                        ))
                    {
                        std::string logfileName;
                        retvalue = getDataprotectionResyncLog(logfileName, (*volumeIter).second.deviceName,
                            endpointDeviceName, (*volumeGroupIter).direction,
                            (*volumeGroupIter).id,
                            (*volumeIter).second.syncMode);
                        if(retvalue)
                        {
                            /* do not delete dp logs */
                            /* deleteDataprotectionLog(logfileName); */
                        }
                        if(TARGET == (*volumeGroupIter).direction)
                        {
                            logfileName.clear();
                            retvalue = getDataprotectionDiffLog(logfileName,(*volumeGroupIter).id);
                            if(retvalue)
                            {
                                /* deleteDataprotectionLog(logfileName); */
                            }
                        }
                    }
                }
                //deleting the resync log when the pair moves to resync-II
                else if(VOLUME_SETTINGS::SYNC_QUASIDIFF == (*volumeIter).second.syncMode)
                {
                    if(!IsDeviceInUse((*volumeIter).second.deviceName, endpointDeviceName, (*volumeGroupIter).direction, 
                        (*volumeGroupIter).id, (*volumeIter).second.syncMode))
                    {
                        std::string logfileName;
                        if(getDataprotectionResyncLog(logfileName,(*volumeIter).second.deviceName, 
                            endpointDeviceName, (*volumeGroupIter).direction,
                            (*volumeGroupIter).id,
                            (*volumeIter).second.syncMode))
                        {
                            /* deleteDataprotectionLog(logfileName); */
                        }
                    }
                }

                // start source resyncs
                // for direct sync, resync is started only for the target direction
                if( (SOURCE == (*volumeGroupIter).direction)
                    && (VOLUME_SETTINGS::SYNC_DIFF != (*volumeIter).second.syncMode)
                    && (VOLUME_SETTINGS::SYNC_QUASIDIFF != (*volumeIter).second.syncMode) 
                    && (VOLUME_SETTINGS::SYNC_DIRECT != (*volumeIter).second.syncMode)
                    )
                {
                    /**
                    * 1 TO N sync: Used one more constructor that initializes the end point
                    */
                    if(!localConfigurator.IsFilterDriverAvailable())
                    {
                        DebugPrintf( SV_LOG_DEBUG, "Dataprotection for %s is not invoked as filter driver is not available\n", (*volumeIter).second.deviceName.c_str());
                        continue;
                    }
                    if (VOLUME_SETTINGS::SYNC_OFFLOAD == (*volumeIter).second.syncMode)
                    {
                        agentInfoPtr.reset(new AgentProcess((*volumeIter).second.deviceName, (int)false, InMageDataprotectionlName));
                    }
                    else
                    {
                        /**
                        * 1 TO N Sync: Added usage of volume group ID, since in case of two
                        *              pairs having same target name ef: 
                        *              fast sync pair , E -> F (tgt 1) and direct or fastsync E -> F (tgt 2)
                        *              Then on source, more than one dataprotection will not be able to run
                        *              since lock is E_F and log is also E/F. Hence added volume group ID in
                        *              between E and F 
                        */
                        agentInfoPtr.reset(new AgentProcess((*volumeIter).second.deviceName, endpointDeviceName, (*volumeGroupIter).id, InMageDataprotectionlName));
                    }

                    findIter = m_AgentProcesses.find(agentInfoPtr);
                    if( m_AgentProcesses.end() == findIter )
                    {
                        // windows cluster, check for volume availability
                        if( SERVICE::instance() ->isClusterVolume((*volumeIter).second.deviceName) )
                        {
                            if ( !SERVICE::instance()->VolumeAvailableForNode((*volumeIter).second.deviceName) ) 
                            {
                                // don't start any resync jobs for this volume
                                DebugPrintf(SV_LOG_DEBUG, "volume %s is not available\n",
                                    (*volumeIter).second.deviceName.c_str());
                                continue;
                            }
                        }

                        // linux cluster, check for volume availability/ownership
                        if(isxen)
                        {
                            if ( !SERVICE::instance()->VolumeAvailableForNode((*volumeIter).second.deviceName) ) 
                            {
                                // don't start any resync jobs for this volume
                                DebugPrintf(SV_LOG_DEBUG, "volume %s is not available\n",
                                    (*volumeIter).second.deviceName.c_str());


                                // for linux cluster volumes, enable filtering for offline source volumes
                                // when the volume becomes available, filtering should immediately start
                                // to avoid loosing any changes
                                // on windows, filtering is on by default by for all cluster volumes.
                                // this is a no-op for windows.
                                SERVICE::instance()-> 
                                    EnableResumeFilteringForOfflineVolume((*volumeIter).second.deviceName);

                                continue;
                            }


                            bool owner = false;
                            int exitcode = 0;
                            if(0 == (exitcode = is_xsowner((*volumeIter).second.deviceName, owner)))
                            {
                                if(!owner)
                                {
                                    DebugPrintf(SV_LOG_DEBUG, 
                                        "%s is not owned by this system. Resync will not be done from here.\n",
                                        (*volumeIter).second.deviceName.c_str());
                                    continue;
                                }                  
                            } else
                            {
                                DebugPrintf(SV_LOG_ERROR, 
                                    "%s encountered error (%d) in routine is_xsowner."
                                    "Resync job will not be invoked for %s.\n",     
                                    FUNCTION_NAME, exitcode, (*volumeIter).second.deviceName.c_str());
                                continue;
                            } 
                        }

                        /**
                        * 1 TO N sync: TODO: Need to handle progress states
                        */
                        if( VOLUME_SETTINGS::PAIR_PROGRESS != (*volumeIter).second.pairState )
                        {
                            DebugPrintf(SV_LOG_DEBUG, "device %s state (%d) is not in-progress.\n",
                                (*volumeIter).second.deviceName.c_str(), (*volumeIter).second.pairState);
                            continue;
                        }

                        if(IsResyncThrottled( (*volumeIter).second.deviceName, endpointDeviceName, (*volumeGroupIter).id))
                        {
                            DebugPrintf(SV_LOG_DEBUG, "device %s is resync throttled.\n",
                                (*volumeIter).second.deviceName.c_str());
                            continue;
                        }

                        /* No resize workflow for the prism, pillar as of now */
                        if (!(*volumeIter).second.isFabricVolume()) 
                        {
                            std::string reasonifcant;
                            if (!CanStartFastsyncSourceAgent(volumeIter->second, *volumeGroupIter, reasonifcant))
                            {
                                DebugPrintf(SV_LOG_ERROR, "Cannot start fastsync source agent for volume %s. Reason is: %s. It will be retried.\n", 
                                    (*volumeIter).second.deviceName.c_str(), reasonifcant.c_str());
                                continue;
                            }
                        }

                        /**
                        * 1 TO N sync: Changed this call to call with the target device name
                        */
                        StartFastOffloadSyncAgent(agentInfoPtr, (*volumeIter).second.syncMode, 
                            (*volumeGroupIter).direction);
                    }
                }

                if (TARGET == (*volumeGroupIter).direction)
                {
                    if (!iscachedirchecked)
                        iscachedirchecked = CheckCacheDir();
                    std::string deviceName = (*volumeIter).second.deviceName;
                    std::vector<std::string>::iterator iter = find(m_cleanupVolumes.begin(),m_cleanupVolumes.end(),deviceName.c_str());
                    if( VOLUME_SETTINGS::DELETE_PENDING == (*volumeIter).second.pairState )
                    {
                        // check if the deletion action is already done by checking the map
                        // if not done, call cleanupaction routine
                        string cleanup_action = (*volumeIter).second.cleanup_action;
                        if(iter == m_cleanupVolumes.end())
                        {
                            if(CleanupAction(deviceName, endpointDeviceName, (*volumeGroupIter).direction, 
                                (*volumeGroupIter).id, cleanup_action, 
                                (*volumeIter).second.syncMode))
                            {
                                DebugPrintf(SV_LOG_DEBUG, "Adding one entry %s to the m_cleanupVolumes map.\n",deviceName.c_str());
                                m_cleanupVolumes.push_back(deviceName);
                            }
                        }
                        else
                        {
                            DebugPrintf(SV_LOG_DEBUG,"Found device %s in the m_cleanupVolumes map.\n",deviceName.c_str());
                        }

                    } else 
                    {
                        // remove the device from the map
                        if(iter != m_cleanupVolumes.end())
                            m_cleanupVolumes.erase(iter);
                    }
                }
                // start target resyncs
                if( (TARGET == (*volumeGroupIter).direction)
                    && (VOLUME_SETTINGS::SYNC_DIFF != (*volumeIter).second.syncMode)
                    && (VOLUME_SETTINGS::SYNC_QUASIDIFF != (*volumeIter).second.syncMode) 
                    ) 
                {
                    if((!localConfigurator.IsFilterDriverAvailable()) && (VOLUME_SETTINGS::SYNC_DIRECT == (*volumeIter).second.syncMode))
                    {
                        DebugPrintf( SV_LOG_DEBUG, "Dataprotection (direct sync) for %s is not invoked as filter driver is not available\n", (*volumeIter).second.deviceName.c_str());
                        continue;
                    }
                    if( VOLUME_SETTINGS::RESTART_RESYNC_CLEANUP == (*volumeIter).second.pairState )
                    {
                        DebugPrintf(SV_LOG_DEBUG, "device %s is in RESTART_RESYNC_CLEANUP state.\n",
                            (*volumeIter).second.deviceName.c_str());
                        continue;
                    }

                    if( VOLUME_SETTINGS::PAIR_PROGRESS != (*volumeIter).second.pairState )
                    {
                        DebugPrintf(SV_LOG_DEBUG, "device %s state (%d) is not in-progress.\n",
                            (*volumeIter).second.deviceName.c_str(), (*volumeIter).second.pairState);
                        continue;
                    }

                    if(!OkToProcessDiffs((*volumeIter).second))
                    {
                        DebugPrintf(SV_LOG_DEBUG, "device %s is not avaiable for processing differentials.\n",
                            (*volumeIter).second.deviceName.c_str());
                        continue;
                    }

                    if(IsTargetThrottled((*volumeIter).second.deviceName))
                    {
                        DebugPrintf(SV_LOG_DEBUG, "device %s is target throttled.\n",
                            (*volumeIter).second.deviceName.c_str());
                        continue;
                    }
                    
                    if (VOLUME_SETTINGS::SYNC_DIRECT == (*volumeIter).second.syncMode) 
                    {
                        /* No resize workflow for prism, pillar for now */
                        VOLUME_SETTINGS::endpoints_iterator eit = (*volumeIter).second.endpoints.begin();
                        if (!eit->isFabricVolume()) 
                        {
                            std::string reasonifcant;
                            if (!CanStartDirectsyncAgent(volumeIter->second, *volumeGroupIter, reasonifcant))
                            {
                                DebugPrintf(SV_LOG_DEBUG, "Cannot start direct sync agent for volume %s. Reason is: %s. It will be retried.\n", 
                                    (*volumeIter).second.deviceName.c_str(), reasonifcant.c_str());
                                continue;
                            }
                        }

                        DebugPrintf(SV_LOG_DEBUG, "For local direct sync, source = %s, target = %s, vgid = %d\n",
                                                 endpointDeviceName.c_str(), (*volumeIter).second.deviceName.c_str(), (*volumeGroupIter).id);
                        agentInfoPtr.reset(new AgentProcess(endpointDeviceName, (*volumeIter).second.deviceName, (*volumeGroupIter).id, InMageDataprotectionlName));
                    }
                    else
                        agentInfoPtr.reset(new AgentProcess((*volumeIter).second.deviceName, (int)false, InMageDataprotectionlName));

                    findIter = m_AgentProcesses.find(agentInfoPtr);
                    if ( m_AgentProcesses.end() == findIter )
                    {
                        StartFastOffloadSyncAgent(agentInfoPtr, (*volumeIter).second.syncMode, 
                            (*volumeGroupIter).direction);
                    }
                }

                // start target diffsync
                if  ( (TARGET == (*volumeGroupIter).direction) 
                    && ((VOLUME_SETTINGS::SYNC_DIFF == (*volumeIter).second.syncMode) ||
                    (VOLUME_SETTINGS::SYNC_QUASIDIFF == (*volumeIter).second.syncMode)) && 
                    (*volumeIter).second.transportProtocol != TRANSPORT_PROTOOL_MEMORY )
                {
                    //if( VOLUME_SETTINGS::PAIR_PROGRESS == (*volumeIter).second.pairState )
                    //{
                    addVolumeGroup = true;
                    diffTargetVolumes.push_back(std::string((*volumeIter).second.deviceName));
                    //}
                }                                
            }

            if (addVolumeGroup)
            {
                // check if volume group on list, if not add it            
                agentInfoPtr.reset(new AgentProcess("InMageVolumeGroup", (*volumeGroupIter).id, InMageDataprotectionlName));
                findIter = m_AgentProcesses.find(agentInfoPtr);
                if (m_AgentProcesses.end() == findIter) 
                {
                    // hide/unhide volumes in the group before launching the process
                    vector<std::string> visibleDrives;
                    bool atleastOneHiddenDrive = false;
                    ProcessVolumeGroupsPendingVolumeVisibililities(volumeGroupIter,
                        visibleDrives, atleastOneHiddenDrive);

                    // PR #2273 
                    // launch agent if there is atleast one hidden drive to
                    // process differentials
                    if(atleastOneHiddenDrive)
                    {
                        StartDiffSyncTargetAgent(agentInfoPtr, visibleDrives);
                    }
                }
            }
        }
        std::vector<std::string>::iterator it = m_cleanupVolumes.begin();
        while(it != m_cleanupVolumes.end())
        {
            string devicename = (*it);
            if(IsRemoveCleanupDevice(devicename))
            {
                DebugPrintf(SV_LOG_DEBUG, 
                    "Erasing one entry %s from m_cleanupVolumes as it is not found in volume groups.\n",
                    devicename.c_str());
                m_cleanupVolumes.erase(it);
                it = m_cleanupVolumes.begin();
            }
            else
            {
                ++it;
            }
        }
    }
    catch( std::string const& e )
    {
        DebugPrintf( SV_LOG_ERROR,"StartVolumeGroups: exception %s\n", e.c_str() );
    }
    catch( char const* e ) 
    {
        DebugPrintf( SV_LOG_ERROR,"StartVolumeGroups: exception %s\n", e );
    }
    catch( exception const& e ) 
    {
        DebugPrintf( SV_LOG_ERROR,"StartVolumeGroups: exception %s\n", e.what() );
    }
    catch(...)
    {
        DebugPrintf( SV_LOG_ERROR,"StartVolumeGroups: exception...\n" );
    }

    m_FilterDrvBitmapDeletedVolumes.clear();
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

void VxService::StartPrismAgents(LocalConfigurator &lc)
{
    //For prism case, always register with involflt
    if (m_configurator->getPrismSettings().size() > 1)
        PerformSourceTasks(INMAGE_FILTER_DEVICE_NAME, lc);
}

bool VxService::CanStartDirectsyncAgent(VOLUME_SETTINGS &vs, VOLUME_GROUP_SETTINGS &vgs, std::string &reasonifcant)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    std::string targetdevicename = vs.deviceName;
    SV_ULONGLONG targetrawsize = vs.GetRawSize();

    std::string sourcedevicename = vs.GetEndpointDeviceName();
    SV_ULONGLONG sourcecapacity = vs.sourceCapacity;
    SV_ULONGLONG sourcerawsize = vs.GetEndpointRawSize();
    int sourcedevicetype = vs.GetEndpointDeviceType();

    VOLUME_SETTINGS::PROTECTION_DIRECTION pd = vs.GetProtectionDirection();

    bool canstart = CanStartSyncAgentOnSource(sourcedevicename, sourcecapacity, sourcerawsize, sourcedevicetype, targetdevicename, targetrawsize, pd, reasonifcant);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return canstart;
}

bool VxService::CanStartFastsyncSourceAgent(VOLUME_SETTINGS &vs, VOLUME_GROUP_SETTINGS &vgs, std::string &reasonifcant)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    std::string sourcedevicename = vs.deviceName;

#ifdef SV_UNIX
    sourcedevicename = GetDiskNameForPersistentDeviceName(vs.deviceName, m_VolumesCache.m_Vs);
#endif

    SV_ULONGLONG sourcecapacity = vs.sourceCapacity;
    SV_ULONGLONG sourcerawsize = vs.GetRawSize();
    int sourcedevicetype = vs.devicetype;

    std::string targetdevicename = vs.GetEndpointDeviceName();
    SV_ULONGLONG targetrawsize = vs.GetEndpointRawSize();

    VOLUME_SETTINGS::PROTECTION_DIRECTION pd = vs.GetProtectionDirection();

    bool canstart = CanStartSyncAgentOnSource(sourcedevicename, sourcecapacity, sourcerawsize, sourcedevicetype, targetdevicename, targetrawsize, pd, reasonifcant);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return canstart;
}

bool VxService::CanStartSyncAgentOnSource(const std::string &sourcedevicename, 
                                          const SV_ULONGLONG &sourcecapacity, 
                                          const SV_ULONGLONG &sourcerawsize, 
                                          const int &sourcedevicetype,
                                          const std::string &targetdevicename, 
                                          const SV_ULONGLONG &targetrawsize, 
                                          const VOLUME_SETTINGS::PROTECTION_DIRECTION &pd, 
                                          std::string &reasonifcant
                                          )
{
    std::stringstream ss;
    ss << "sourcedevicename = " << sourcedevicename
        << ", sourcecapacity = " << sourcecapacity
        << ", sourcerawsize = " << sourcerawsize
        << ", sourcedevicetype = " << sourcedevicetype
        << ", targetdevicename = " << targetdevicename
        << ", targetrawsize = " << targetrawsize
        << ", protection direction = " << StrProtectionDirection[pd];

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with %s\n", FUNCTION_NAME, ss.str().c_str());

    SV_ULONGLONG currentsourcecapacity = GetCurrentSourceCapacity(sourcedevicename, sourcedevicetype);
    DebugPrintf(SV_LOG_DEBUG, "current source capacity found is " ULLSPEC ".\n", currentsourcecapacity);

    bool canstart = m_Vopr.IsSourceDeviceCapacityValidInSettings(sourcedevicename, currentsourcecapacity, sourcecapacity, pd, reasonifcant) &&
                    IsTargetDeviceSizeAppropriate(sourcedevicename, sourcecapacity, targetdevicename, targetrawsize, reasonifcant) &&
                    DoPreResyncFilterDriverTasks(sourcedevicename, sourcecapacity, sourcerawsize, sourcedevicetype, reasonifcant);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with can start as %s\n",FUNCTION_NAME, STRBOOL(canstart));
    return canstart;
}


SV_ULONGLONG VxService::GetCurrentSourceCapacity(const std::string &sourcedevicename, const int &sourcedevicetype)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s with %s\n", FUNCTION_NAME, sourcedevicename.c_str());
    bool iscacheavailable = false;
    
    SV_ULONGLONG size = 0;
    std::string err;
    if (DataCacher::UpdateVolumesCache(m_VolumesCache, err))
    {
        cdp_volume_t::CDP_VOLUME_TYPE vtype = (VolumeSummary::DISK == sourcedevicetype) ? cdp_volume_t::CDP_DISK_TYPE : cdp_volume_t::CDP_REAL_VOLUME_TYPE;
        cdp_volume_t v(sourcedevicename, false, vtype, &m_VolumesCache.m_Vs);
        BasicIo::BioOpenMode mode;
        mode = BasicIo::BioReadExisting | BasicIo::BioShareAll | BasicIo::BioBinary;
        if (SV_SUCCESS == v.Open(mode))
            size = v.GetSize();
        else
        {
#ifdef SV_WINDOWS
            if (ERROR_FILE_NOT_FOUND == v.LastError())
#else
            if (ENOENT == v.LastError())
#endif 
            {
                DiskNotFoundErrRateControlMap::iterator it = m_diskNotFoundErrRateControlMap.find(sourcedevicename);
                if (it == m_diskNotFoundErrRateControlMap.end())
                {
                    m_diskNotFoundErrRateControlMap[sourcedevicename] = DiskNotFoundErrRateControl();
                }
                m_diskNotFoundErrRateControlMap[sourcedevicename].m_diskNotFoundCnt++;
                ACE_Time_Value currentTime = ACE_OS::gettimeofday();
                int diskNotFoundErrorLogInterval = m_configurator->getVxAgent().getDiskNotFoundErrorLogInterval();
                if ((currentTime.sec() - m_diskNotFoundErrRateControlMap[sourcedevicename].m_lastDiskNotFoundErrLogTime) > diskNotFoundErrorLogInterval)
                {
                    DebugPrintf(SV_LOG_ERROR, "svagents failed to open source device name %s when starting sync with error %lu. Failures since %dsec=%d.\n", sourcedevicename.c_str(), v.LastError(),
                        diskNotFoundErrorLogInterval, m_diskNotFoundErrRateControlMap[sourcedevicename].m_diskNotFoundCnt);
                    m_diskNotFoundErrRateControlMap[sourcedevicename].m_lastDiskNotFoundErrLogTime = ACE_OS::gettimeofday().sec();
                    m_diskNotFoundErrRateControlMap[sourcedevicename].m_diskNotFoundCnt = 0;
                }
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "svagents failed to open source device name %s when starting sync with error %lu.\n", sourcedevicename.c_str(), v.LastError());
            }
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s with %s\n", FUNCTION_NAME, sourcedevicename.c_str());
    return size;
}


bool VxService::IsTargetDeviceSizeAppropriate(const std::string &sourcedevicename, const SV_ULONGLONG &sourcecapacity, 
                                              const std::string &targetdevicename, const SV_ULONGLONG &targetrawsize,
                                              std::string &reasonifnot)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    /* TODO: review this check */
    bool isappropriate = (targetrawsize >= sourcecapacity);
    if (isappropriate) 
        DebugPrintf(SV_LOG_DEBUG, "Target device size is appropriate.\n");
    else {
        VolumeResizeAlert a(sourcedevicename, targetdevicename, targetrawsize, sourcecapacity);
        reasonifnot = a.GetMessage();
        SendAlertToCx(SV_ALERT_ERROR, SV_ALERT_MODULE_RESYNC, a);
        CDPUtil::QuitRequested(60);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return isappropriate;
}

bool VxService::DoPreResyncFilterDriverTasks(const std::string &sourcedevicename, const SV_ULONGLONG &sourcecapacity, 
                                             const SV_ULONGLONG &sourcerawsize, const int &sourcedevicetype, std::string &reasonifcant)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    
    bool done = true;

    if (m_FilterDrvBitmapDeletedVolumes.end() == m_FilterDrvBitmapDeletedVolumes.find(sourcedevicename)) {
        SV_ULONGLONG sizefrominvolflt;
        std::stringstream ss;
        bool shouldStopFiltering = false;

        done = m_FilterDrvIf.GetVolumeTrackingSize(sourcedevicename, sourcedevicetype, m_sPFilterDriverNotifier->GetDeviceStream(), sizefrominvolflt, reasonifcant);

        if (done) {
            DebugPrintf(SV_LOG_DEBUG, "For volume %s, volume size from filter driver is " ULLSPEC "\n", sourcedevicename.c_str(), sizefrominvolflt);
            if (0 == sizefrominvolflt) {
                DebugPrintf(SV_LOG_DEBUG, "Volume %s is not currently being tracked by filter driver. " 
                                          "Hence nothing to do.\n", sourcedevicename.c_str());
            } else if (m_FilterDrvIf.DoesVolumeTrackingSizeMatch(sourcedevicetype, sizefrominvolflt, sourcecapacity, sourcerawsize)) {
                DebugPrintf(SV_LOG_DEBUG, "Filter driver has correct size for volume %s. Hence nothing to do.\n", sourcedevicename.c_str());
            } else {
                ss << "For source volume " << sourcedevicename
                   << ", capacity in settings is " << sourcecapacity
                   << ", rawsize in settings is " << sourcerawsize
                   << ", tracking size at filter driver " << sizefrominvolflt;

                shouldStopFiltering = true;
            }
        }

#ifdef SV_WINDOWS
        // Only if 
        // * V2A
        // * Windows
        // * No error in the above tracking size check
        // * Not already adjudged to stop filtering above

        if (done && !shouldStopFiltering) {
            ULONG flags;

            // On best effort basis. Ignore on failure (expected to fail on upgrade - to be exact,
            // >= 1804 agent with < 1804 driver.
            if (m_FilterDrvIf.GetVolumeFlags(sourcedevicename, sourcedevicetype, m_sPFilterDriverNotifier->GetDeviceStream(), flags)) {
                if (0 != (flags & VSF_RECREATE_BITMAP_FILE)) {
                    ss << "Filter driver indicates that the bitmap file should be"
                        << " recreated for volume: " << sourcedevicename;

                    shouldStopFiltering = true;
                }
            }
        }
#endif // SV_WINDOWS

        if (shouldStopFiltering) {
            DebugPrintf(SV_LOG_DEBUG, "%s. Hence need to delete volume tracking bitmap from filter driver.\n", ss.str().c_str());
            done = DeleteFilterDriverVolumeTrackingSize(sourcedevicename, sourcedevicetype, reasonifcant);
            if (done) {
                m_FilterDrvBitmapDeletedVolumes.insert(sourcedevicename);
                ss << ". Hence deleted volume tracking bitmap from filter driver";
                RecordFilterDrvVolumeBitmapDelMsg(ss.str());
            }
        }
    } else {
        std::stringstream ss;
        ss << "For source volume " << sourcedevicename
           << ", capacity in settings is " << sourcecapacity
           << ", rawsize in settings is " << sourcerawsize
           << ", had filter driver bitmap deleted already during processing of first 1toN settings. Hence doing nothing now";
        RecordFilterDrvVolumeBitmapDelMsg(ss.str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return done;
}

// TODO-SanKumar-1806: Rename this method to StopFilteringAndDeleteBitmap()
bool VxService::DeleteFilterDriverVolumeTrackingSize(const std::string &devicename, const int &devicetype, std::string &reasonifcant)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    StopAgentsRunningOnSource(devicename);
    DebugPrintf(SV_LOG_DEBUG, "Stopped all source agents running on device %s\n", devicename.c_str());
    bool deleted = m_FilterDrvIf.DeleteVolumeTrackingSize(devicename, devicetype, m_sPFilterDriverNotifier->GetDeviceStream(), reasonifcant);
    if (deleted)
        DebugPrintf(SV_LOG_DEBUG, "Successfully deleted volume tracking size from filter driver for device %s\n", devicename.c_str());

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return deleted;
}

void VxService::StartFastOffloadSyncAgent(AgentProcess::Ptr& agentInfo, VOLUME_SETTINGS::SYNC_MODE syncMode, CDP_DIRECTION direction)
{  
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try
    {           	
        std::stringstream args;

        if (syncMode == VOLUME_SETTINGS::SYNC_DIRECT)
        {
            /**
            * 1 TO N sync: endpointDeviceName for direct sync is source
            * For direct sync, agentInfo->m_endpointDeviceId has to be empty
            * otherwise functor LessAgentProcess will not work
            */
            args<<" directsync "
                << '\"' << agentInfo ->DeviceName() << '\"'
                << " " << '\"' << agentInfo ->EndpointDeviceName() << '\"';
        }
        else
        {
            args << (TARGET == direction ? " target " : " source ")
                << (syncMode == VOLUME_SETTINGS::SYNC_FAST || syncMode == VOLUME_SETTINGS::SYNC_FAST_TBC ? "fast " : "offload ")
                << '\"' << agentInfo->DeviceName() << '\"' ; 
            if ((SOURCE == direction) && (VOLUME_SETTINGS::SYNC_OFFLOAD != syncMode))
            {
                args << " " << '\"' << agentInfo->EndpointDeviceName() << '\"';
                args << " " << '\"' << agentInfo ->GroupId() << '\"';
            }

            std::map<std::string, ACE_Time_Value>::iterator agentsMapItr = m_agentsExitStatusMap.find(agentInfo->DeviceName());
            if (agentsMapItr == m_agentsExitStatusMap.end())
            {
                args << " " << '\"' << AGENT_FRESH_START << '\"';
            }
            else
            {
                args << " " << '\"' << AGENT_RESTART << '\"';
            }
        }

        std::stringstream msg;
        msg<< "Starting dataprotection "
            << args.str()<<"\n";
        DebugPrintf(SV_LOG_INFO, "%s",msg.str().c_str());

        if (!StartProcess(m_ProcessMgr, agentInfo, m_configurator->getVxAgent().getDataProtectionExePathname().c_str(), 
            args.str().c_str())) 
        {
            DebugPrintf(SV_LOG_ERROR, 
                "FAILED VxService::StartFastSyncTargetAgent: couldn't start %s\n", args.str().c_str());
        }
        else
        {
            m_AgentProcesses.insert(agentInfo);
            DebugPrintf(SV_LOG_DEBUG, "The Sync Target started with PID: %d\n", 
                agentInfo->ProcessHandle());
        }
    }
    catch( std::string const& e ) {
        DebugPrintf( SV_LOG_ERROR,"StartFastOffloadSyncAgent: exception %s\n", e.c_str() );
    }
    catch( char const* e ) {
        DebugPrintf( SV_LOG_ERROR,"StartFastOffloadSyncAgent: exception %s\n", e );
    }
    catch( exception const& e ) {
        DebugPrintf( SV_LOG_ERROR,"StartFastOffloadSyncAgent: exception %s\n", e.what() );
    }
    catch(...) {
        DebugPrintf( SV_LOG_ERROR,"StartFastOffloadSyncAgent: exception...\n" );
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

}

void VxService::StartDiffSyncTargetAgent(AgentProcess::Ptr& agentInfo, vector<std::string> visibleVolumes)
{    
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try 
    {
        std::stringstream args;

        args << " target diff" << " " 
            << m_configurator->getVxAgent().getDiffTargetSourceDirectoryPrefix() 
            << '/' 
            << m_configurator->getVxAgent().getHostId() 
            << " \"" 
            << m_configurator->getVxAgent().getDiffTargetCacheDirectoryPrefix() 
            << ACE_DIRECTORY_SEPARATOR_STR_A 
            << m_configurator->getVxAgent().getHostId() 
            << '"' 
            << ' ' << agentInfo->GroupId() 
            << ' ' << m_configurator->getVxAgent().getDiffTargetFilenamePrefix()
            << ' ' << RUN_OUTPOST_SNAPSHOT_AGENT
            << " \"";

        // Append list of visible volumes to args
        // Visible volumes are delimited by semi-colon ';'
        vector<std::string>::iterator volIter;
        for(volIter = visibleVolumes.begin(); volIter != visibleVolumes.end(); volIter++)
        {
            if (volIter != visibleVolumes.begin())
            {             
                args << ';' << (*volIter);
            }
            else
            {
                args << (*volIter);
            }
        }

        args << '"';

        if (!StartProcess(m_ProcessMgr, agentInfo, m_configurator->getVxAgent().getDataProtectionExePathname().c_str(), args.str().c_str())) 
        {
            DebugPrintf(SV_LOG_ERROR, "FAILED VxService::StartVolumeGroupTargetAgent: couldn't start %s\n", args.str().c_str());
        }
        else
        {
            m_AgentProcesses.insert(agentInfo);			
            DebugPrintf(SV_LOG_DEBUG, "The DiffSyncTarget Agent started with PID: %d\n", agentInfo->ProcessHandle());
        }
    }
    catch( std::string const& e ) {
        DebugPrintf( SV_LOG_ERROR,"StartDiffSyncTargetAgent: exception %s\n", e.c_str() );
    }
    catch( char const* e ) {
        DebugPrintf( SV_LOG_ERROR,"StartDiffSyncTargetAgent: exception %s\n", e );
    }
    catch( exception const& e ) {
        DebugPrintf( SV_LOG_ERROR,"StartDiffSyncTargetAgent: exception %s\n", e.what() );
    }
    catch(...) {
        DebugPrintf( SV_LOG_ERROR,"StartDiffSyncTargetAgent: exception...\n" );
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

}

void VxService::StartSnapshotAgent(AgentProcess::Ptr& agentInfo) 
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try 
    {
        std::string drive = agentInfo->Id();

        std::stringstream args;
        args << " target diff" << " " 
            << m_configurator->getVxAgent().getDiffTargetSourceDirectoryPrefix() 
            << '/' << m_configurator->getVxAgent().getHostId() << '/' 
            << GetSourceDirectoryNameOnCX(drive) << " \"" 
            << m_configurator->getVxAgent().getDiffTargetCacheDirectoryPrefix() 
            << ACE_DIRECTORY_SEPARATOR_STR_A 
            << m_configurator->getVxAgent().getHostId() << '"' 
            << ' ' <<  drive                                                                   
            << ' ' << GetCxIpAddress()
            << ' ' << m_configurator->getVxTransport().getHttp().port
            << ' ' << m_configurator->getVxAgent().getHostId() 
            << ' ' << m_configurator->getVxAgent().getDiffTargetFilenamePrefix()
            << ' ' << RUN_ONLY_SNAPSHOT_AGENT;                             

        if (!StartProcess(m_ProcessMgr, agentInfo, m_configurator->getVxAgent().getDataProtectionExePathname().c_str(), args.str().c_str())) 
        {
            DebugPrintf(SV_LOG_ERROR, "FAILED CServiceModule::StartSnapshotAgentProcess: couldn't start %s\n", args.str().c_str());
        }
        else
        {
            m_AgentProcesses.insert(agentInfo);
            DebugPrintf(SV_LOG_DEBUG, "The Snapshot Agent started with PID: %d\n", agentInfo->ProcessHandle());
        }
    }
    catch( std::string const& e ) {
        DebugPrintf( SV_LOG_ERROR,"StartSnapshotAgent: exception %s\n", e.c_str() );
    }
    catch( char const* e ) {
        DebugPrintf( SV_LOG_ERROR,"StartSnapshotAgent: exception %s\n", e );
    }
    catch( exception const& e ) {
        DebugPrintf( SV_LOG_ERROR,"StartSnapshotAgent: exception %s\n", e.what() );
    }
    catch(...) {
        DebugPrintf( SV_LOG_ERROR,"StartSnapshotAgent: exception...\n" );
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

void VxService::ProcessVolumeGroupsPendingVolumeVisibililities(HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator const & volumeGroupIter, std::vector<std::string> &visibleDrives, bool &atleastOneHiddenDrive)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    atleastOneHiddenDrive = false;

    try {  
        VOLUME_GROUP_SETTINGS::volumes_iterator volumeIter((*volumeGroupIter).volumes.begin());
        VOLUME_GROUP_SETTINGS::volumes_iterator volumeEnd((*volumeGroupIter).volumes.end());

        for (/*empty*/; volumeIter != volumeEnd; ++volumeIter) 
        {
            //SRM: DataProtection should run for both flush pending and resume pending states. 
            //Visibilty operations(hide, unhide) are disabled during flush and hold states. CX should not allow any visibility 
            //operations when pair is in any flush and hold states. Hide and Unhide operations are disabled through cdpcli also.
            //Flush and hold operations are also not allowed when the target is visible(ro,rw).
            if (!OkToProcessDiffs((*volumeIter).second)) 
            {             
                visibleDrives.push_back(std::string((*volumeIter).second.deviceName));        
            } 
            else if( VOLUME_SETTINGS::PAIR_PROGRESS == (*volumeIter).second.pairState ||
                VOLUME_SETTINGS::FLUSH_AND_HOLD_WRITES_PENDING == (*volumeIter).second.pairState ||
                VOLUME_SETTINGS::FLUSH_AND_HOLD_RESUME_PENDING == (*volumeIter).second.pairState) 

            {
                atleastOneHiddenDrive = true;
            }
        } 
    }
    catch( std::string const& e ) {
        DebugPrintf( SV_LOG_ERROR,"ProcessVolumeGroupsPendingVolumeVisibililities: exception %s\n", e.c_str() );
    }
    catch( char const* e ) {
        DebugPrintf(SV_LOG_ERROR, "ProcessVolumeGroupsPendingVolumeVisibililities: exception %s\n", e );
    }
    catch( exception const& e ) {
        DebugPrintf( SV_LOG_ERROR,"ProcessVolumeGroupsPendingVolumeVisibililities: exception %s\n", e.what() );
    }
    catch(...) {
        DebugPrintf( SV_LOG_ERROR,"ProcessVolumeGroupsPendingVolumeVisibililities: exception...\n" );
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

}

/*
* FUNCTION NAME :  checkConfigurator
*
* DESCRIPTION : To catch the exception while communicating to CX 
*				for getting the volume attribute, Volume Mount Point, Source file system
*
* INPUT PARAMETERS : volume
*
* OUTPUT PARAMETERS : RequestedState, VolumeState, sMountPoint, sFileSystem
*
* NOTES :	Previously this changes are in function OkToProcessDiff without try and catch block, 
*			Now this part moved here as a new function with try and catch block
* 
* return value : true on success, false otherwise
*
*/

bool VxService::CheckConfigurator(int &RequestedState,	std::string &sMountPoint, std::string &sFileSystem,const std::string &Volume)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    try{
        RequestedState = m_configurator->getVolumeAttribute(Volume);
    }
    catch( ContextualException &exception )
    {
        DebugPrintf( SV_LOG_ERROR,"FAILED: CheckConfigurator::Get Volume Attribute call to CX failed  for the volume %s with exception %s\n",Volume.c_str(),exception.what());
        return false;
    }
    catch (...) 
    {
        DebugPrintf(SV_LOG_ERROR,"FAILED: CheckConfigurator::Get Volume Attribute call to CX failed  for the volume %s with unknown Exception\n", Volume.c_str());
        return false;
    }

    try{
        sMountPoint = m_configurator->getVxAgent().getVolumeMountPoint(Volume);
    }
    catch( ContextualException &exception )
    {
        DebugPrintf( SV_LOG_ERROR,"FAILED: CheckConfigurator::Get Volume Mount Point call to CX failed for the volume %s with exception %s\n",Volume.c_str(),exception.what());
        return false;
    }
    catch (...) 
    {
        DebugPrintf(SV_LOG_ERROR,"FAILED: CheckConfigurator::Get Volume Mount Point call to CX failed for the volume %s with unknown exception\n", Volume.c_str());
        return false;
    }

    try{
        sFileSystem = m_configurator->getVxAgent().getSourceFileSystem(Volume);
    }
    catch( ContextualException &exception )
    {
        DebugPrintf( SV_LOG_ERROR,"FAILED: CheckConfigurator::Get Source File System call to CX failed for the volume %s with exception %s\n",Volume.c_str(),exception.what());
        return false;
    }
    catch (...) 
    {
        DebugPrintf(SV_LOG_ERROR,"FAILED: CheckConfigurator::Get Source File System call to CX failed for the volume %s with unknown exception \n", Volume.c_str());
        return false;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return true;
}

//Removing the replication pair in CX for the pending rollback
//Input : destination volume and pending rollback file
// Output: return true if suceeded otherwise false
//functionality: Delete the replication from CX if the replication pair is not deleted

//Algorithm:
//1. read the content;  
//2. check whether stop replication request send to cx from the content of the file
//3. if not send then sent the stop replication request to cx and update the file content

bool VxService::ProcessRollback(const std::string& Volume, const std::string& RollbackActionsFilePath)
{
    std::string cxFormattedVolumeName = "";
    cxFormattedVolumeName = Volume;
    FormatVolumeNameForCxReporting(cxFormattedVolumeName);
    try
    {
        string cleanupaction = "cleanupaction=;";
        // PR#10815: Long Path support
        ifstream infile(getLongPathName(RollbackActionsFilePath.c_str()).c_str());
        infile >> cleanupaction;
        infile.close();
        if(cleanupaction == "sent_stop_replication_to_cx")
        {
            DebugPrintf(SV_LOG_DEBUG,"The rollback information is already sent to cx for the volume %s\n",
                Volume.c_str());
            return false;
        }
        m_configurator->getVxAgent().cdpStopReplication(cxFormattedVolumeName,cleanupaction);
        // PR#10815: Long Path support
        ofstream outfile(getLongPathName(RollbackActionsFilePath.c_str()).c_str(),std::ios::trunc);
        outfile << "sent_stop_replication_to_cx";
        outfile.close();
        DebugPrintf(SV_LOG_INFO,"Replication pair for %s is removed due to a pending rollback action\n",cxFormattedVolumeName.c_str());
    }
    catch (ContextualException& e)
    {
        DebugPrintf(SV_LOG_ERROR, "Replication pair deletion request for %s did not succeed.Failed with the exception:%s\n", cxFormattedVolumeName.c_str(), e.what());
        return false;
    }
    catch (exception& e)
    {
        DebugPrintf(SV_LOG_ERROR,
            "Replication pair deletion request for %s did not succeed.Failed with the exception:%s\n",cxFormattedVolumeName.c_str(), e.what());
        return false;
    }
    catch(...)
    {
        DebugPrintf(SV_LOG_ERROR,"Replication pair deletion request for %s did not succeed.\n",cxFormattedVolumeName.c_str());
        return false;
    }
    return true;
}


///
/// Checks if the volume has to be hidden or unhidden from the agent params
/// and makes the volume hidden or unhidden and returns true if it is ok
/// to process diffs (volume is hidden) otherwise returns false (volume
/// is either visible read-only or visible read-write)
/// 
/// Hide operation is not performed in svagents.
/// It has been moved in dataprotection as part of bug 5117 fix
///BUGBUG: need to check for read-only/read-write visible drives.
///
/// Added second parameter to find if CX is trying to resync the target volume(for resolving the bug#5276)
bool VxService::OkToProcessDiffs( const VOLUME_SETTINGS & volsetting)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    std::string Volume = volsetting.deviceName;
    VOLUME_SETTINGS::SYNC_MODE syncMode = volsetting.syncMode;
    bool bProcessDiffs = false;
    int RequestedState = volsetting.visibility;
    std::string sMountPoint = volsetting.mountPoint;
    std::string sFileSystem = "";

    try {
        /*if(!CheckConfigurator(RequestedState,sMountPoint,sFileSystem,Volume))
        return bProcessDiffs;*/

        VOLUME_SETTINGS::TARGET_DATA_PLANE targetDataPlane = volsetting.getTargetDataPlane();

        if (volsetting.isUnSupportedDataPlane()) {
            DebugPrintf(SV_LOG_ERROR, "Received invalid target data plane for replication; replication for target device %s cannot proceed.\n", Volume.c_str());
            return false;
        }

        if (volsetting.isAzureDataPlane()) {
            return true;
        }


        std::list<VOLUME_SETTINGS>::const_iterator endIter = volsetting.endpoints.begin();
        if ( endIter != volsetting.endpoints.end() )
        {
            sFileSystem = endIter->fstype;
        }



        /*** added the following code to resovle bug# 5276 ***
        *   this code checks for any pending unhide actions(stored as dummy files in the "pendingactions" directory)
        *   performed on the target volumes, and updates the CX with the current volumeState when it comes up
        ***/


        string UnhideROActionsFilePath = CDPUtil::getPendingActionsFilePath(Volume, ".unhideRO");
        string UnhideRWActionsFilePath = CDPUtil::getPendingActionsFilePath(Volume, ".unhideRW");
        string RollbackActionsFilePath = CDPUtil::getPendingActionsFilePath(Volume, ".rollback");

        DebugPrintf(SV_LOG_DEBUG, "rollback file is %s\n", RollbackActionsFilePath.c_str());

        ACE_stat statbuf = {0};
        bool unhideRW = false;
        bool unhideRO = false;
        bool rollback = false;
        //check if unhideRO file exists
        // PR#10815: Long Path support
        if ((0 == sv_stat(getLongPathName(UnhideROActionsFilePath.c_str()).c_str(), &statbuf)) && ( (statbuf.st_mode & S_IFREG) == S_IFREG ) )
        {
            if(	(syncMode != VOLUME_SETTINGS::SYNC_DIFF) && (syncMode != VOLUME_SETTINGS::SYNC_QUASIDIFF) )
            {// if CX tries to resynch the target volume and unhideRO file exist delete it
                // PR#10815: Long Path support
                ACE_OS::unlink(getLongPathName(UnhideROActionsFilePath.c_str()).c_str());
            }
            else
            {
                unhideRO = true;
            }
        }
        //check if unhideRW file exists
        // PR#10815: Long Path support
        if ((0 == sv_stat(getLongPathName(UnhideRWActionsFilePath.c_str()).c_str(), &statbuf)) && ( (statbuf.st_mode & S_IFREG) == S_IFREG ) )
        {
            if(	(syncMode != VOLUME_SETTINGS::SYNC_DIFF) && (syncMode != VOLUME_SETTINGS::SYNC_QUASIDIFF) )
            {// if CX tries to resynch the target volume and unhideRW file exist delete it
                // PR#10815: Long Path support
                ACE_OS::unlink(getLongPathName(UnhideRWActionsFilePath.c_str()).c_str());
            }
            else
            {
                unhideRW = true;
            }
        }
        //added this conditiopn to take care for pending rollback for the bug 6723
        // PR#10815: Long Path support
        if ((0 == sv_stat(getLongPathName(RollbackActionsFilePath.c_str()).c_str(), &statbuf)) && ( (statbuf.st_mode & S_IFREG) == S_IFREG ) )
        {
            if(unhideRO)
            {
                // PR#10815: Long Path support
                ACE_OS::unlink(getLongPathName(UnhideROActionsFilePath.c_str()).c_str());
            }
            if(unhideRW)
            {
                // PR#10815: Long Path support
                ACE_OS::unlink(getLongPathName(UnhideRWActionsFilePath.c_str()).c_str());
            }

            //PR# 24805 
            // If the pair is in resync
            // and this is newly setup pair
            // deleting the rollback file to pogress the replication 
            //and to launch dataprotection
            if( ((volsetting.syncMode == VOLUME_SETTINGS::SYNC_OFFLOAD) || 
                (volsetting.syncMode == VOLUME_SETTINGS::SYNC_FAST)||
                (volsetting.syncMode == VOLUME_SETTINGS::SYNC_FAST_TBC) ||
                (volsetting.syncMode == VOLUME_SETTINGS::SYNC_DIRECT)) 
                && (volsetting.resyncCounter == 0 ) )
            {
                DebugPrintf(SV_LOG_ERROR, 
                    "Note: this is not an error. deleting pending action file left from previous session for volume %s and continuing with resync\n",
                    Volume.c_str());

                if((ACE_OS::unlink(getLongPathName(RollbackActionsFilePath.c_str()).c_str()) < 0)
                    && (ACE_OS::last_error() != ENOENT))
                {
                    DebugPrintf(SV_LOG_ERROR,
                        "\n%s encountered error (%d) while trying unlink operation for file %s.\n",
                        FUNCTION_NAME,ACE_OS::last_error(), RollbackActionsFilePath.c_str());
                }
            } else
            {
                rollback = ProcessRollback(Volume,RollbackActionsFilePath);
            }

            bProcessDiffs = false;
            return bProcessDiffs;
        }else
        {
            DebugPrintf(SV_LOG_DEBUG, "rollback file %s not found\n", RollbackActionsFilePath.c_str());
        }

        // for disk, we are not currently allowing unhide_ro, unhide_rw operations
        // so we can safely assume that it is ok to process diffs
        VolumeSummary::Devicetype devType = (VolumeSummary::Devicetype)volsetting.GetDeviceType();
        if (cdp_volume_t::GetCdpVolumeType(devType) == cdp_volume_t::CDP_DISK_TYPE)
            return true;

        VOLUME_STATE VolumeState = GetVolumeState(Volume.c_str());

        //added the following code for the bug:7703
        //checks if the volume is a raw volume and the volume has to be unhide 
        //( request getting from cx)  
        //then we will return false for not to process diff
        //as the raw volume can not be unhide.
        //also unmounting the vsnaps for the target volume
        std::string sparsefile;
        bool isnewvirtualvolume = false;
        if((sFileSystem.empty()) || 
            (!IsVolpackDriverAvailable() && 
            (IsVolPackDevice(Volume.c_str(),sparsefile,isnewvirtualvolume))))
        {
            if(RequestedState != VOLUME_HIDDEN)
            {
                std::list<std::string>::iterator iter = find(m_unhiddenRawVolumes.begin(),m_unhiddenRawVolumes.end(),Volume.c_str());
                if(iter == m_unhiddenRawVolumes.end())
                {
                    VsnapMgr *Vsnap;
#ifdef SV_WINDOWS
                    WinVsnapMgr obj;
                    Vsnap=&obj;
#else
                    UnixVsnapMgr obj;
                    Vsnap=&obj;
#endif
                    DebugPrintf(SV_LOG_DEBUG,
                        "%s is raw volume. ignoring unhide request, unmounting vsnaps associated with the volume\n",
                        Volume.c_str());
                    Vsnap->UnMountVsnapVolumesForTargetVolume(std::string(Volume.c_str()));
                    m_unhiddenRawVolumes.push_back(Volume);
                }
                return bProcessDiffs;
            }
            else
            {
                std::list<std::string>::iterator iter = find(m_unhiddenRawVolumes.begin(),m_unhiddenRawVolumes.end(),Volume.c_str());
                if(iter != m_unhiddenRawVolumes.end())
                {
                    m_unhiddenRawVolumes.erase(iter);
                }
            }
        }

        if (RequestedState == VOLUME_HIDDEN)
        {
            if( unhideRW || unhideRO)
            {//if there are pending files
                if( VolumeState == VOLUME_HIDDEN )
                {//when the target volume is already hidden
                    if(unhideRO)
                    {
                        // PR#10815: Long Path support
                        ACE_OS::unlink(getLongPathName(UnhideROActionsFilePath.c_str()).c_str());
                    }
                    if(unhideRW)
                    {
                        try
                        {
                            bool status = false;
                            const std::string &resyncReasonCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncReason::DeviceUnlockedInReadWriteMode;
                            std::stringstream msg;
                            msg << "Target volume: " << Volume << " is unlocked in readwrite mode."
                                << " Resync is required for this volume."
                                << " Marking resync with resyncReasonCode=" << resyncReasonCode;
                            DebugPrintf(SV_LOG_ERROR, "%s: %s", FUNCTION_NAME, msg.str().c_str());
                            ResyncReasonStamp resyncReasonStamp;
                            STAMP_RESYNC_REASON(resyncReasonStamp, resyncReasonCode);

                            if(!setTargetResyncRequired(*m_configurator, Volume, status, msg.str(), resyncReasonStamp) || !status)
                            {
                                DebugPrintf(SV_LOG_ERROR,"Failed to set ResyncRequired for volume %s.\n", Volume.c_str());
                            }
                            // PR#10815: Long Path support
                            ACE_OS::unlink(getLongPathName(UnhideRWActionsFilePath.c_str()).c_str());
                        }
                        catch (ContextualException& e)
                        {
                            DebugPrintf(SV_LOG_ERROR, "Setting ResyncRequired on %s failed with the exception:%s\n", Volume.c_str(), e.what());
                        } 
                        catch(...)
                        {
                            DebugPrintf(SV_LOG_ERROR, "Setting ResyncRequired on %s failed.\n", Volume.c_str());
                        }
                    }
                }
                else
                {// if the target volume is left unhidden send update to CX
                    try
                    {
                        DebugPrintf(SV_LOG_INFO, "Sending UPDATE_NOTIFY to Cx about the pending unhide action on %s Volume\n", Volume.c_str());
                        bool status = false;
                        bool rv = updateVolumeAttribute((*m_configurator), UPDATE_NOTIFY, Volume, VolumeState,"", status);
                        if(!rv)
                        {
                            DebugPrintf(SV_LOG_ERROR, "UPDATE_NOTIFY about the pending unhide action on %s did not succeed. \n",
                                Volume.c_str());
                        }
                        if(!status)
                        {
                            DebugPrintf(SV_LOG_ERROR,"Failed to update CX with current volume state for Volume %s.\n", Volume.c_str());
                        }
                        if(unhideRW)
                        {
                            const std::string &resyncReasonCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncReason::DeviceUnlockedInReadWriteMode;
                            std::stringstream msg;
                            msg << "Target volume: " << Volume << " is unlocked in readwrite mode."
                                << " Resync is required for this volume."
                                << " Marking resync with resyncReasonCode=" << resyncReasonCode;
                            DebugPrintf(SV_LOG_ERROR, "%s: %s", FUNCTION_NAME, msg.str().c_str());
                            ResyncReasonStamp resyncReasonStamp;
                            STAMP_RESYNC_REASON(resyncReasonStamp, resyncReasonCode);

                            rv = setTargetResyncRequired((*m_configurator), Volume, status, msg.str(), resyncReasonStamp);
                            if(!rv)
                            {
                                DebugPrintf(SV_LOG_ERROR, "Failed to setResyncRequired for %s. \n",
                                    Volume.c_str());
                            }
                            if(!status)
                            {
                                DebugPrintf(SV_LOG_ERROR,"FAILED: OkToProcessDiffs::Failed to setResyncRequired for volume %s.\n", Volume.c_str());
                            }
                        }
                    }
                    catch (ContextualException& e)
                    {
                        DebugPrintf(SV_LOG_ERROR, "UPDATE_NOTIFY about the pending unhide action on %s did not succeed. Failed with the exception:%s\n",
                            Volume.c_str(), e.what());
                    } 
                    catch(...)
                    {
                        DebugPrintf(SV_LOG_ERROR,"UPDATE_NOTIFY about the pending unhide action on %s did not succeed.\n", Volume.c_str());
                    }
                }
            }
            else
            {// if there are no pending files(i.e no pending actions on the target volume)
                bProcessDiffs =true;
                return  bProcessDiffs;
            }
        }
        if( (RequestedState == VOLUME_VISIBLE_RO) || (RequestedState == VOLUME_VISIBLE_RW) )
        {// if CX is updated with the current volume state 
            if(unhideRO)
            {
                // PR#10815: Long Path support
                ACE_OS::unlink(getLongPathName(UnhideROActionsFilePath.c_str()).c_str());
            }
            if(unhideRW)
            {
                bool rv = false;
                bool status = false;

                const std::string &resyncReasonCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncReason::DeviceUnlockedInReadWriteMode;
                std::stringstream msg;
                msg << "Target volume: " << Volume << " is unlocked in readwrite mode."
                    << " Resync is required for this volume."
                    << " Marking resync with resyncReasonCode=" << resyncReasonCode;
                DebugPrintf(SV_LOG_ERROR, "%s: %s", FUNCTION_NAME, msg.str().c_str());
                ResyncReasonStamp resyncReasonStamp;
                STAMP_RESYNC_REASON(resyncReasonStamp, resyncReasonCode);

                rv = setTargetResyncRequired((*m_configurator), Volume, status, msg.str(), resyncReasonStamp);

                if(rv && status)
                {
                    // PR#10815: Long Path support
                    ACE_OS::unlink(getLongPathName(UnhideRWActionsFilePath.c_str()).c_str());
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR,"FAILED: OkToProcessDiffs::Failed to setResyncRequired for volume %s.\n", Volume.c_str());
                }
            }
        }
        /***** code changes to resovle bug# 5276 ends here *****/

        if(RequestedState == VOLUME_DO_NOTHING  )
            return false;

        if ( VolumeState != RequestedState )
        {

            bool bValidDir = true;
#ifdef SV_WINDOWS
            sMountPoint = Volume;
#else
            if (sMountPoint.empty()) 
            {
                DebugPrintf(SV_LOG_ERROR, 
                    "Empty Mount Point recieved from cx for %s\n", Volume.c_str());
                bValidDir= false;
            }
            else
            {
                if(SVMakeSureDirectoryPathExists(sMountPoint.c_str()).failed())
                {
                    DebugPrintf(SV_LOG_ERROR,
                        "Failed to create mount point directory %s for volume %s.\n", 
                        sMountPoint.c_str(), Volume.c_str());
                    bValidDir = false;
                }
            }
#endif
            if ( bValidDir )
            {

                if (VOLUME_VISIBLE_RO == RequestedState)
                {

                    bProcessDiffs =false;
                    if ( UnhideDrive_RO(Volume.c_str(),sMountPoint.c_str(),sFileSystem.c_str()).succeeded() )
                    {
                        DebugPrintf(SV_LOG_DEBUG,"SUCCESS: OkToProcessDiffs::Volume %s made visible in Read-Only mode.\n",
                            Volume.c_str());
                        try{
                            bool status = false;
                            bool rv = updateVolumeAttribute((*m_configurator), UPDATE_NOTIFY, Volume, VOLUME_VISIBLE_RO,"", status);
                            if (!rv)
                            {
                                DebugPrintf( SV_LOG_ERROR,"FAILED: OkToProcessDiffs::Update Volume Attribute function call to CX failed for the volume %s\n",Volume.c_str());
                                return bProcessDiffs;
                            }
                            if(!status)
                            {
                                DebugPrintf(SV_LOG_ERROR,"FAILED: OkToProcessDiffs::Volume unhide readonly operation succeeded for %s but failed to update CX with volume status.\n", Volume.c_str());
                            }
                        }
                        catch( ContextualException &exception )
                        {
                            DebugPrintf( SV_LOG_ERROR,"FAILED: OkToProcessDiffs::Update Volume Attribute function call to CX failed for the volume %s with exception %s\n",Volume.c_str(),exception.what());
                            return bProcessDiffs;
                        }
                        catch (...) 
                        {
                            DebugPrintf(SV_LOG_ERROR,"FAILED: OkToProcessDiffs::Update Volume Attribute function call to CX failed for the volume %s with unknown exception\n", Volume.c_str());
                            return bProcessDiffs;
                        }
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_ERROR,"FAILED: OkToProcessDiffs::Volume %s could not be made visible in Read-Only mode.\n",Volume.c_str());
                        //if (!m_configurator->getVxAgent().updateVolumeAttribute(UPDATE_NOTIFY,Volume,VolumeState) )
                        //DebugPrintf(SV_LOG_ERROR,"FAILED: OkToProcessDiffs::Failed to unhide volume %s in Read-Only mode and failed to update CX with volume status.\n", Volume.c_str());
                    }                            

                }

                else if (VOLUME_VISIBLE_RW == RequestedState)
                {
                    //Part Of 3301 
                    VsnapMgr *Vsnap;
#ifdef SV_WINDOWS
                    WinVsnapMgr obj;
                    Vsnap=&obj;
#else	
                    UnixVsnapMgr obj;
                    Vsnap=&obj;
#endif						

                    bProcessDiffs =false;
                    if ( UnhideDrive_RW(Volume.c_str(),sMountPoint.c_str()/* TODO: MPOINT */,sFileSystem.c_str()).succeeded() )
                    {

                        Vsnap->UnMountVsnapVolumesForTargetVolume(std::string(Volume.c_str()));
                        DebugPrintf(SV_LOG_DEBUG,"SUCCESS: OkToProcessDiffs::Volume %s made visible in Read-Write mode.\n",Volume.c_str());
                        try{
                            bool status = false;
                            bool rv = updateVolumeAttribute((*m_configurator), UPDATE_NOTIFY,Volume,VOLUME_VISIBLE_RW,"",status);
                            if (!rv)
                            {
                                DebugPrintf( SV_LOG_ERROR,"FAILED: OkToProcessDiffs::updateVolumeAttribute function call to CX failed for the voleme %s\n",Volume.c_str());
                                return bProcessDiffs;

                            }
                            if(!status)
                            {
                                DebugPrintf(SV_LOG_WARNING,"FAILED: OkToProcessDiffs::Volume unhide readwrite operation succeeded for %s but failed to update CX with volume status.\n", Volume.c_str());
                            }
                        }
                        catch( ContextualException &exception )
                        {
                            DebugPrintf( SV_LOG_ERROR,"FAILED: OkToProcessDiffs::updateVolumeAttribute function call to CX failed for the voleme %s with exception %s\n",Volume.c_str(),exception.what());
                            return bProcessDiffs;
                        }
                        catch (...) 
                        {
                            DebugPrintf(SV_LOG_ERROR,"FAILED: OkToProcessDiffs::updateVolumeAttribute function call to CX failed for the voleme %s with unknown exception\n", Volume.c_str());
                            return bProcessDiffs;
                        }
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_ERROR,"FAILED: OkToProcessDiffs::Volume %s could not be made visible in Read-Write mode.\n",Volume.c_str());
                        //if (!m_configurator->getVxAgent().updateVolumeAttribute(UPDATE_NOTIFY,Volume,VolumeState) )
                        //  DebugPrintf(SV_LOG_ERROR,"FAILED: OkToProcessDiffs::Failed to unhide volume %s in Read-Write mode and failed to update CX with volume status.\n", Volume.c_str());
                    }                            

                }

                else // Requested State is invalid
                {
                    DebugPrintf(SV_LOG_ERROR,"FAILED: OkToProcessDiffs::Specified state (%d) is invalid for volume %s\n", RequestedState, Volume.c_str());
                }
            }
            else // bValidDir is false
            {
                DebugPrintf(SV_LOG_ERROR,"Invalid mountpoint specified %s for %s.\n",sMountPoint.c_str(), Volume.c_str());
                try{
                    bool status = false;
                    bool rv = updateVolumeAttribute((*m_configurator), UPDATE_NOTIFY,Volume,VolumeState,"", status);
                    if (!rv)
                    {
                        DebugPrintf( SV_LOG_ERROR,"FAILED: OkToProcessDiffs::updateVolumeAttribute function call to CX failed for the volume %s\n",Volume.c_str());
                        return bProcessDiffs;
                    }
                    if(!status)
                    {
                        DebugPrintf(SV_LOG_ERROR,"FAILED: OkToProcessDiffs::Invalid mountpoint specified %s for %s. failed to update CX with volume status.\n", sMountPoint.c_str(), Volume.c_str());
                    }
                }
                catch( ContextualException &exception )
                {
                    DebugPrintf( SV_LOG_ERROR,"FAILED: OkToProcessDiffs::updateVolumeAttribute function call to CX failed for the volume %s with exception %s\n",Volume.c_str(),exception.what());
                    return bProcessDiffs;
                }
                catch (...) 
                {
                    DebugPrintf(SV_LOG_ERROR,"FAILED: OkToProcessDiffs::updateVolumeAttribute function call to CX failed for the volume %s with unknown exception\n", Volume.c_str());
                    return bProcessDiffs;
                }
            }
        }  // End of if(VolumeState != RequestedState)
        else if ( ( VOLUME_VISIBLE_RO == VolumeState ) || ( VOLUME_VISIBLE_RW == VolumeState ) )
        {
            bProcessDiffs = false;
        }
        else if ( VOLUME_HIDDEN == VolumeState )
        {
            bProcessDiffs = true;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR,"FAILED: OkToProcessDiffs::Unknown volume state for %s.\n", Volume.c_str());
        }
    }
    catch (...) 
    {
        DebugPrintf(SV_LOG_ERROR,"FAILED: OkToProcessDiffs::Unable to acquire volume state for %s\n", Volume.c_str());
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return bProcessDiffs;
}

bool VxService::IsTargetThrottled(std::string const& deviceName )
{ 
    return m_configurator->getVxAgent().shouldThrottleTarget( deviceName );
}

bool VxService::IsSourceThrottled(std::string const& deviceName )
{ 
    return m_configurator->getVxAgent().shouldThrottleSource(deviceName );
}

bool VxService::IsResyncThrottled( std::string const& deviceName , const std::string &endpointdeviceName, 
                                  const int &grpid)
{ 
    return m_configurator->getVxAgent().shouldThrottleResync(deviceName, endpointdeviceName, grpid);
}

void VxService::ProcessDeviceEvent(void* param)
{            
    DebugPrintf( SV_LOG_DEBUG,"ENTERED: ProcessDeviceEvent\n" );
    if(!quit)
    {
        try 
        {
#ifdef SV_WINDOWS  
            std::string sVolumes = static_cast<WinService*>(SERVICE::instance())->GetVolumesOnlineAsString();
            if (m_sVolumesOnline != sVolumes) 
            {
                m_sVolumesOnline = sVolumes;
#endif
                RegisterHost();
                m_RegisterHostThread->SetLastRegisterHostTime();
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

bool VxService::updateResourceID(LocalConfigurator &lc)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool updated = true;

    if (lc.getResourceId().empty()) {
        try {
            lc.setResourceId(GetUuid());
        } catch (ContextualException &e) {
            updated = false;
            DebugPrintf(SV_LOG_ERROR, "Failed to generate/update resource id with exception: %s\n", e.what());
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return updated;
}

bool VxService::updateSourceGroupID(LocalConfigurator &lc)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool updated = true;
    if (lc.getSourceGroupId().empty()) {
        try {
            lc.setSourceGroupId(GetUuid());
        }
        catch (ContextualException &e) {
            updated = false;
            DebugPrintf(SV_LOG_ERROR, "Failed to generate/update source group id with exception: %s\n", e.what());
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return updated;
}
bool VxService::updateUpgradeInformation(LocalConfigurator &lc)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool bUpdatedCX = lc.getUpdatedUpgradeToCX() ;
    if( bUpdatedCX == false ) 
    {
        std::stringstream agentVersion ;
        agentVersion << CurrentCompatibilityNum() ;
        std::string hostId = lc.getHostId() ;
        HTTP_CONNECTION_SETTINGS s = lc.getHttp() ;
        std::string upgradePHP = lc.getUpgradePHPPath() ;
        std::string postBuffer ;
        postBuffer = "hostid=" + hostId ;
        postBuffer += "&compatibility=" + agentVersion.str() ;

        SVERROR result = PostToSVServer( GetCxIpAddress().c_str(), 
            s.port, 
            upgradePHP.c_str(), 
            (char*)postBuffer.c_str(), 
            postBuffer.length(),lc.IsHttps() ) ;
        if( result.succeeded() )
        {
            lc.setUpdatedUpgradeToCX( true ) ;
            bUpdatedCX = lc.getUpdatedUpgradeToCX() ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR,"posting agent version upgrade info to config server failed. %s\n", postBuffer.c_str());
        }
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return bUpdatedCX ;
}

bool VxService::doPreConfiguratorTasks(void)
{
    LocalConfigurator lc;
    bool bupdateUpgradeInformation = false;
    bupdateUpgradeInformation = updateUpgradeInformation(lc);
    if (!bupdateUpgradeInformation)
    {
        /*This will prevent DOS ing the CS!**/
        ACE_OS::sleep(lc.getUpdateUpgradeWaitTimeSecs());
    }
    return bupdateUpgradeInformation && updateResourceID(lc) && updateSourceGroupID(lc);
}

int VxService::init()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    int ret = SV_FAILURE;

    try 
    {
        if( doPreConfiguratorTasks()  == false )
        {
            /* to avoid spinning on cpu
             * as this will be retried */
            CDPUtil::QuitRequested(2);
            return SV_FAILURE ;
        }

        if(!InitializeConfigurator())
        {
            return SV_FAILURE;
        }

        DebugPrintf( SV_LOG_DEBUG,"VxService started\n" );
        try
        {
            Configurator* lConfigurator ;
            if(::InitializeConfigurator(&lConfigurator, USE_ONLY_CACHE_SETTINGS, PHPSerialize))
            {
                InitialSettings settings = lConfigurator->getInitialSettings() ;
                m_volGroups = settings.hostVolumeSettings ;
            }
        }
        catch(...)
        {
            DebugPrintf(SV_LOG_DEBUG, "The cached settings are empty..\n") ;
        }

        m_ProcessMgr = new ACE_Process_Manager();
        m_ProcessMgr->open();
        //
        // Register ACE_Event_Handler for exit notification
        //
        m_ProcessMgr->register_handler(this);

        m_bEnableVolumeMonitor = m_configurator->getVxAgent().getEnableVolumeMonitor();
        DebugPrintf(SV_LOG_DEBUG, "Volume monitor enabled = %s\n", m_bEnableVolumeMonitor?"true":"false");

        if (m_bEnableVolumeMonitor)
        {
            ((CService*)SERVICE::instance())->getVolumeMonitorSignal().connect(this,&VxService::ProcessDeviceEvent);
        }

        updateConfigurationChange(m_configurator->getInitialSettings());
        ConfigChangeCallback(m_configurator->getInitialSettings());
        m_configurator->getInitialSettingsSignal().connect( this, &VxService::ConfigChangeCallback );
        m_configurator->getConfigurationChangeSignal().connect( this, &VxService::updateConfigurationChange);
        m_configurator->getFailoverCxSignal().connect( this, &VxService::onFailoverCxSignal );
        m_configurator->getCsAddressForAzureComponentsChangeSignal().connect(this, &VxService::CsAddressForAzureComponentsChangeCallback);

        /* Create RegisterHostThread */
        m_RegisterHostThread = new RegisterHostThread(
          m_configurator->getVxAgent().getRegisterHostInterval(),
          SETTINGS_POLLING_INTERVAL);
        m_configurator->getRegisterImmediatelySignal().connect(
          m_RegisterHostThread, &RegisterHostThread::SetRegisterImmediatelyCallback);

        LocalConfigurator lc;
        m_isAgentRoleSource = (0 == lc.getAgentRole().compare(MOBILITY_AGENT_ROLE));

        if (m_isAgentRoleSource)
        {
            m_MonitorHostThread = new MonitorHostThread(m_configurator);
        }

#ifdef SV_WINDOWS
        if (lc.isMasterTarget())
        {
            std::string mtSupportedDataPlanes = lc.getMTSupportedDataPlanes();
            std::size_t foundAzureDataPlane = mtSupportedDataPlanes.find("AZURE_DATA_PLANE");
            if (foundAzureDataPlane != string::npos)
            {
                m_MarsHealthMonitorThread = new MarsHealthMonitorThread(m_configurator);
            }
        }
#endif

        m_ConsistencyThread = new ConsistencyThreadCS(m_configurator);

        /* Create the configurator thread */
        m_configurator->Start();
        m_bTSChecksEnabled = m_configurator->getVxAgent().getTSCheck() ;

        m_SrcTelemeteryPollInterval = lc.getSrcTelemetryPollInterval();
        m_SrcTelemeteryStartDelay = lc.getSrcTelemetryStartDelay();

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

void VxService::prepareStoppedVolumesList(InitialSettings initialSettings)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    try 
    {
        bool bprintnewsettings = true;
        HOST_VOLUME_GROUP_SETTINGS  l_volGroups ;
        l_volGroups = initialSettings.hostVolumeSettings ;
        HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator oldVolGrpIter = m_volGroups.volumeGroups.begin();

        while( oldVolGrpIter != m_volGroups.volumeGroups.end() )
        {
            if ( oldVolGrpIter->direction == SOURCE )
            {
                VOLUME_GROUP_SETTINGS::volumes_iterator oldVolIter = oldVolGrpIter->volumes.begin() ;
                while( oldVolIter != oldVolGrpIter->volumes.end() )
                {
                    DebugPrintf(SV_LOG_DEBUG, "The volume %s is protected as source in cache settings\n", oldVolIter->first.c_str());
                    bool bProtectedVolume = false ;
                    if( !IsUninstallSignalled() )
                    {
                        HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator updVolGrpIter = l_volGroups.volumeGroups.begin();
                        while( updVolGrpIter != l_volGroups.volumeGroups.end() )
                        {
                            if ( updVolGrpIter->direction == SOURCE )
                            {
                                if (bprintnewsettings)
                                {
                                    for (VOLUME_GROUP_SETTINGS::volumes_iterator newVolIter = updVolGrpIter->volumes.begin(); 
                                        newVolIter != updVolGrpIter->volumes.end(); ++newVolIter)
                                    {
                                        DebugPrintf(SV_LOG_DEBUG, "The volume %s is protected as source in new settings\n", newVolIter->first.c_str());
                                    }
                                }
                                VOLUME_GROUP_SETTINGS::volumes_iterator updVolIter = updVolGrpIter->volumes.begin() ;
                                VOLUME_GROUP_SETTINGS::volumes_iterator volumeIter;
                                volumeIter = updVolGrpIter->volumes.find(oldVolIter->first) ;
                                if( volumeIter != updVolGrpIter->volumes.end() )
                                {
                                    DebugPrintf(SV_LOG_DEBUG, "Volume %s is still protected as source in new and cached settings\n", oldVolIter->first.c_str());
                                    bProtectedVolume = true ;

                                    uint32_t reason = 0;

                                    if (oldVolIter->second.throttleSettings.IsResyncThrottled()
                                        != volumeIter->second.throttleSettings.IsResyncThrottled())
                                    {
                                        reason |= volumeIter->second.throttleSettings.IsResyncThrottled() ?
                                            LOG_ON_RESYNC_THROTTLE_START :
                                            LOG_ON_RESYNC_THROTTLE_END;
                                    }

                                    if (oldVolIter->second.throttleSettings.IsSourceThrottled()
                                        != volumeIter->second.throttleSettings.IsSourceThrottled())
                                    {
                                        reason |= volumeIter->second.throttleSettings.IsSourceThrottled() ?
                                            LOG_ON_DIFF_THROTTLE_START :
                                            LOG_ON_DIFF_THROTTLE_END;
                                    }

                                    if ((volumeIter->second.pairState == VOLUME_SETTINGS::PAUSE) &&
                                        (volumeIter->second.pairState != oldVolIter->second.pairState))
                                    {
                                        reason |= LOG_ON_PAUSE_START;
                                    }

                                    if ((oldVolIter->second.pairState == VOLUME_SETTINGS::PAUSE) &&
                                        (volumeIter->second.pairState != oldVolIter->second.pairState))
                                    {
                                        reason |= LOG_ON_PAUSE_END;
                                    }

                                    if ((volumeIter->second.syncMode == VOLUME_SETTINGS::SYNC_FAST_TBC) &&
                                        (volumeIter->second.syncMode != oldVolIter->second.syncMode))
                                    {
                                        reason |= LOG_ON_RESYNC_START;
                                    }

                                    if ((volumeIter->second.syncMode == VOLUME_SETTINGS::SYNC_QUASIDIFF) &&
                                        (volumeIter->second.syncMode != oldVolIter->second.syncMode))
                                    {
                                        reason |= LOG_ON_RESYNC_END;
                                    }

                                    if ((volumeIter->second.resyncRequiredFlag != 0) &&
                                        (volumeIter->second.resyncRequiredFlag != oldVolIter->second.resyncRequiredFlag))
                                    {
                                        reason |= LOG_ON_RESYNC_SET;
                                    }

                                    if (reason != 0)
                                        LogSourceReplicationStatus(*updVolGrpIter, reason);
                                }
                            }
                            updVolGrpIter++ ;
                        }
                        bprintnewsettings = false;
                    }
                    if( bProtectedVolume == false )
                    {
                        std::string stopFilterVolName ;
                        if( oldVolIter->second.sanVolumeInfo.virtualName.compare("") != 0 )
                        {
                            stopFilterVolName = oldVolIter->second.sanVolumeInfo.virtualName ;
                        }
                        else
                        {
                            stopFilterVolName = oldVolIter->first ;
                        }
                        DebugPrintf(SV_LOG_DEBUG, "Volume %s is added to stop filtering list, since not present in new settings (or) agent uninstall\n", 
                            stopFilterVolName.c_str());
                        FilterDrvVol_t v = m_FilterDrvIf.GetFormattedDeviceNameForDriverInput(stopFilterVolName, oldVolIter->second.devicetype);
                        LogSourceReplicationStatus(*oldVolGrpIter, LOG_ON_STOP_FLT);

                        m_stoppedVolumes.insert(std::string(v.begin(), v.end()));
                    }
                    oldVolIter ++ ;
                }
            }
            oldVolGrpIter ++ ;
        }

        HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator updVolGrpIter = l_volGroups.volumeGroups.begin();
        while (updVolGrpIter != l_volGroups.volumeGroups.end())
        {
            if (updVolGrpIter->direction == SOURCE)
            {
                VOLUME_GROUP_SETTINGS::volumes_iterator updVolIter = updVolGrpIter->volumes.begin();
                while (updVolIter != updVolGrpIter->volumes.end())
                {
                    VOLUME_GROUP_SETTINGS::volumes_iterator volumeIter;
                    bool bNewProtectedVolume = true;

                    oldVolGrpIter = m_volGroups.volumeGroups.begin();
                    while (oldVolGrpIter != m_volGroups.volumeGroups.end())
                    {
                        if (oldVolGrpIter->direction == SOURCE)
                        {
                            volumeIter = oldVolGrpIter->volumes.find(updVolIter->first);
                            if (volumeIter != oldVolGrpIter->volumes.end())
                            {
                                bNewProtectedVolume = false;
                            }
                        }
                        oldVolGrpIter++;
                    }

                    if (bNewProtectedVolume)
                    {
                        DebugPrintf(SV_LOG_DEBUG, "Volume %s is newly protected as source in new settings\n", updVolIter->first.c_str());
                        LogSourceReplicationStatus(*updVolGrpIter, LOG_ON_START_FLT);
                    }

                    updVolIter++;
                }
            }
            updVolGrpIter++;
        }

        m_volGroups = l_volGroups ;
    }
    catch(...) {
        DebugPrintf(SV_LOG_ERROR,"FAILED: Unable to add to list of protected volumes. @LINE %d in FILE %s\n",LINE_NO,FILE_NAME);
    }    
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
// TODO: Modification and testing pending

/*
* FUNCTION NAME :  VxService::ConfigChangeCallback
*
* DESCRIPTION : callback on change to initial settings.
*               1. stop filtering for replication pairs which are removed
*               2. update the protected volume list
*
* INPUT PARAMETERS : InitialSettings
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : none
*
*/

void VxService::ConfigChangeCallback(InitialSettings initialSettings)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    ACE_Guard<ACE_Mutex> PropertyChangeGuard(m_PropertyChangeLock);
    prepareStoppedVolumesList(initialSettings) ;
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

}

/*
* FUNCTION NAME :  VxService::CsAddressForAzureComponentsChangeCallback
*
* DESCRIPTION : callback on change of CS address for Azure components.
*               persist the new CS address to drscout.conf
*               create a copy of the CS fingerprint with the new CS addresss
*
* INPUT PARAMETERS : 
*               LocalConfigurator & - reference to local configurator which needs to be updated.
*               std::string - new CS address
*
* OUTPUT PARAMETERS : none
*
* NOTES :
*
*
* return value : none
*
*/

void VxService::CsAddressForAzureComponentsChangeCallback(LocalConfigurator &localconfigurator, std::string currentCsAddrForAzureComponents)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    static bool s_bUpdated = false;

    std::string prevCsAddrForAzureComponents = localconfigurator.getCsAddressForAzureComponents();
    if (s_bUpdated &&
        (currentCsAddrForAzureComponents == prevCsAddrForAzureComponents))
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: CS address for Azure Components not updated as previous update was successful.\n", FUNCTION_NAME);
        return;
    }

    try {
        s_bUpdated = false;
        std::string reply, csip, csport;
        csip = GetCxIpAddress();

        if (boost::iequals(csip, currentCsAddrForAzureComponents))
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: CS address for Azure Components not updated as it is same as on-prem CS address.\n", FUNCTION_NAME);
            return;
        }

        csport = boost::lexical_cast<std::string>(localconfigurator.getHttp().port);
        if (!securitylib::copyCsFingerprintForAzureComponents(reply, csip, currentCsAddrForAzureComponents, csport))
        {
            DebugPrintf(SV_LOG_ERROR,
                "Setting the CS address for Azure components failed as %s.\n",
                reply.c_str());
        }
        else
        {
            DebugPrintf(SV_LOG_ALWAYS,
                "Setting the CS address for Azure components as %s.\n",
                currentCsAddrForAzureComponents.c_str());
            localconfigurator.setCsAddressForAzureComponents(currentCsAddrForAzureComponents);
            s_bUpdated = true;
        }
    }
    catch (const std::exception& e) {
        DebugPrintf(SV_LOG_ERROR, "Failed to update CS address for Azure components with exception %s\n", e.what());
    }
    catch (...) {
        DebugPrintf(SV_LOG_ERROR, "Failed to update CS address for Azure components\n");
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}
/*
* FUNCTION NAME :  VxService::ConfigChangeCallback
*
* DESCRIPTION : callback on change to initial settings.
*               1. persist the initial settings to local store
*               
* INPUT PARAMETERS : InitialSettings
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : none
*
*/

void VxService::updateConfigurationChange(InitialSettings initialSettings)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    ACE_Guard<ACE_Mutex> PropertyChangeGuard(m_PropertyChangeLock);
    try {

        SerializeConfigSettings(initialSettings);
    } 
    catch(...) {
        DebugPrintf(SV_LOG_ERROR,"FAILED: Unable to update config.dat file. @LINE %d in FILE %s\n",LINE_NO,FILE_NAME);
    }    
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

/*
* FUNCTION NAME :  VxService::SerializeConfigSettings
*
* DESCRIPTION : persist the initial settings to local store
*
*
* INPUT PARAMETERS : InitialSettings
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : none
*
*/
void VxService::SerializeConfigSettings(const InitialSettings & initialSettings)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    ACE_HANDLE hOut = ACE_INVALID_HANDLE;
    std::string temp_cachepath;

    try
    {
        std::string cachePath;

        LocalConfigurator::getConfigCachePathname(cachePath);

        std::string lockPath = cachePath + ".lck";
        temp_cachepath = cachePath + "_tmp";

        // PR#10815: Long Path support
        ACE_File_Lock lock(getLongPathName(lockPath.c_str()).c_str(), O_CREAT | O_RDWR, SV_LOCK_PERMISSIONS,0);
        lock.acquire();

        hOut = ACE_OS::open(temp_cachepath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, ACE_DEFAULT_OPEN_PERMS);
        if (hOut == ACE_INVALID_HANDLE) {
            int err = ACE_OS::last_error();
            std::ostringstream msg;
            msg << "Failed to create file " << temp_cachepath << " with error no: " << err << std::endl;
            throw msg.str();
        }

        std::stringstream ss;
        ss << CxArgObj<InitialSettings>(initialSettings);
        const size_t slen = ss.str().length();
        ssize_t bytesWritten = 0;
        do
        {
            ssize_t tmp = ACE_OS::write(hOut, ss.str().c_str() + bytesWritten, slen - bytesWritten);
            if (tmp == -1)
            {
                int err = ACE_OS::last_error();
                std::stringstream msg;
                msg << "Unable to cache the replication settings to: " << temp_cachepath << ", write failed with error no: " << err << std::endl;
                throw msg.str();
            }
            bytesWritten += tmp;
        } while (bytesWritten != slen);

        SafeCloseAceHandle(hOut, temp_cachepath.c_str());

        if(ACE_OS::rename(temp_cachepath.c_str(), cachePath.c_str()) < 0)
        {
            int err = ACE_OS::last_error();
            std::stringstream msg;
            msg << "Unable to cache the replication settings due to rename fails for the file "
                << temp_cachepath.c_str() << " to " << cachePath.c_str()
                << ". error no: " << err << std::endl;
            ACE_OS::unlink(temp_cachepath.c_str());
            throw msg.str();
        }
    }
    catch (std::string const& e)
    {
        DebugPrintf(SV_LOG_ERROR, "SerializeConfigSettings failed. Exception: %s\n", e.c_str());
        SafeCloseAceHandle(hOut, temp_cachepath.c_str());
    }
    catch(...)
    {
        DebugPrintf(SV_LOG_ERROR,"FAILED: SerializeConfigSettings: Unable to cache the replication settings. @LINE %d in FILE %s\n",LINE_NO,FILE_NAME);
        SafeCloseAceHandle(hOut, temp_cachepath.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

void VxService::StopFiltering()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    ACE_Guard<ACE_Mutex> PropertyChangeGuard(m_PropertyChangeLock);

    if( IsUninstallSignalled() )
    {
        Configurator* lConfigurator ;
        InitialSettings settings ;
        try
        {
            if(::InitializeConfigurator(&lConfigurator, USE_ONLY_CACHE_SETTINGS, PHPSerialize))
            {
                settings = lConfigurator->getInitialSettings() ;
                m_volGroups = settings.hostVolumeSettings ;
            }
        }
        catch(...)
        {
            DebugPrintf(SV_LOG_DEBUG, "The cached settings are empty..\n") ;
        }
        DebugPrintf(SV_LOG_DEBUG, "Uninstall Signalled.. Issuing stop filtering on all protected volumes\n") ;
        prepareStoppedVolumesList(settings) ;
    }
    if(!m_stoppedVolumes.empty() && m_IsFilterDriverNotified)
    {
        /*
        * issue stop to s2 only when ts checks are enabled
        */
        if (m_bTSChecksEnabled)
            StopSentinel();
        PauseTrackingStatus_t statusInfo;
        PauseTrackingUpdateInfo update;
        std::set<std::string>::iterator stoppedVolumeBegIter = m_stoppedVolumes.begin();
        std::set<std::string>::iterator stoppedVolumeEndIter = m_stoppedVolumes.end();
        while(stoppedVolumeBegIter != stoppedVolumeEndIter)
        {
            statusInfo.insert(std::make_pair(*stoppedVolumeBegIter, update));
            stoppedVolumeBegIter++;
        }
        stopFiltering(m_sPFilterDriverNotifier->GetDeviceStream(), statusInfo);
        m_stoppedVolumes.erase(m_stoppedVolumes.begin(),m_stoppedVolumes.end());
        m_stoppedVolumes.clear();
    }	        
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

}

void VxService::StartRegistrationTask()
{
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
                DebugPrintf(SV_LOG_DEBUG, "Cleaned volumeinfocollector cache.\n");
                break;
            } else
                DebugPrintf(SV_LOG_ERROR, "Vx service failed to clean volumeinfocollector cache with error %s.\n", dataCacher.GetErrMsg().c_str());
        } else
            DebugPrintf(SV_LOG_ERROR, "Vx service failed to initialize data cacher to clean volumeinfocollector cache.");

        DebugPrintf(SV_LOG_ERROR, " Retrying after %u minutes...\n", CLEAR_RETRY_INTERVAL_SECONDS/60);
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
            break;
        }
    } while (!CDPUtil::QuitRequested(WAIT_SECS_FOR_CACHE));
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

    SERVICE::instance()->startClusterMonitor();

    if (m_bEnableVolumeMonitor)
    {
        SERVICE::instance()->startVolumeMonitor();
    }

    if (m_MonitorHostThread)
    {
        m_MonitorHostThread->Start();
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

void VxService::StopMonitors()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    if (m_bEnableVolumeMonitor)
    {
        SERVICE::instance()->stopVolumeMonitor();
    }
    SERVICE::instance()->stopClusterMonitor();

    if (m_MonitorHostThread)
    {
        m_MonitorHostThread->Stop();
        delete m_MonitorHostThread;
        m_MonitorHostThread = NULL;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

#ifdef SV_WINDOWS
void VxService::StartMarsHealthMonitor()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    assert(NULL != m_MarsHealthMonitorThread);
    m_MarsHealthMonitorThread->Start();

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void VxService::StopMarsHealthMonitor()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (m_MarsHealthMonitorThread)
    {
        m_MarsHealthMonitorThread->Stop();
        delete m_MarsHealthMonitorThread;
        m_MarsHealthMonitorThread = NULL;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

//If you change InMageAgent, please do the necessary modifications in winservice.h as well
ACE_NT_SERVICE_DEFINE (InMageAgent, WinService, SERVICE_NAME)
#endif /* SV_WINDOWS */

void VxService::onSetReadOnly( std::string const& deviceName ) {
    DebugPrintf( SV_LOG_DEBUG,"Device %s set to read only\n", deviceName.c_str() );
}

void VxService::onSetReadWrite( std::string const& deviceName ) {
    DebugPrintf( SV_LOG_DEBUG,"Device %s set to read only\n", deviceName.c_str() );
}

void VxService::onSetVisible( std::string const& deviceName ) {
    DebugPrintf( SV_LOG_DEBUG,"Device %s set to visible\n", deviceName.c_str() );
}

void VxService::onSetInvisible( std::string const& deviceName ) {
    DebugPrintf( SV_LOG_DEBUG,"Device %s set to invisible\n", deviceName.c_str() );
}

void VxService::onThrottle( ConfigureVxAgent::ThrottledAgent a, std::string const& deviceName, bool throttle ){
    DebugPrintf( SV_LOG_DEBUG,"setting throttle on %s to %s\n", deviceName.c_str(), throttle ? "true" : "false" );
}

void VxService::onStartSnapshot( int id, std::string const& sourceDeviceName, std::string const& targetDeviceName, std::string const& preScript, std::string const& postScript ) {
    DebugPrintf( SV_LOG_DEBUG,"starting snapshot %d (%s -> %s)\n", id, sourceDeviceName.c_str(), targetDeviceName.c_str() );
}

void VxService::onStopSnapshot( int id ) {
    DebugPrintf( SV_LOG_DEBUG,"stopping snapshot %d\n", id );
}


std::string VxService::GetSourceDirectoryNameOnCX(std::string drive)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    std::string dirname;

#ifdef SV_WINDOWS
    //Get drive letter (i.e E for E:\)
    if(IsDrive(drive))
    {
        dirname = drive[0];
    }

    if(IsMountPoint(drive))
    {
        dirname = drive;
    }
#else
    //Get basename of drive name (i.e sda1 for /dev/sda1)
    dirname = basename_r(drive.c_str(), ACE_DIRECTORY_SEPARATOR_CHAR_A);
#endif

    return dirname;
}

/*
* FUNCTION NAME :  VxService::InitializeConfigurator
*
* DESCRIPTION : 
*  
*    try instantiate configurator
*    on success return true
*    on failure 
*           idle for few (60) secs
*          return false
*    
*               
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
bool VxService::InitializeConfigurator()
{
    bool rv = false;

    DebugPrintf(SV_LOG_INFO, "Instantiating Configurator...\n");

    do
    {
        LocalConfigurator localConfigurator;
        SetLogLevel( localConfigurator.getLogLevel() );
        SetSerializeType(localConfigurator.getSerializerType());

        m_configurator = NULL;
        if(!::InitializeConfigurator(&m_configurator, USE_CACHE_SETTINGS_IF_CX_NOT_AVAILABLE, PHPSerialize))
        {
            rv = false;
            break;
        }

        rv = true;

    } while(0);


    if(!rv)
    {
        // TODO: we should use some kind of wait on event for 60 secs
        //       instead of sleep
        DebugPrintf(SV_LOG_INFO, "will retry configurator instantiation again after 60 secs.\n");
        ACE_OS::sleep(60);
    }

    return rv;
}

/*
* FUNCTION NAME :  VxService::switchcx
*
* DESCRIPTION : Switch to the Cx that is reachable. This could be primary Cx or one of remote Cxs
*  If it is the cx that we're using return.
*  else,
*  create a configurator instance using reachable cx ip and port
*  send retention refresh information to the cx
*  update drscout.conf with new cx ip and port
* 
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on successful switch, false on failure.
*
*/
bool VxService::switchcx()
{
    bool rv = false;

    LocalConfigurator localConfigurator;

    //nothing to do here if the cx is same as the earlier one
    if(m_reachablecx.ip == GetCxIpAddress() && m_reachablecx.port == localConfigurator.getHttp().port)
    {
        m_switchCx = false;
        return true;
    }

    std::string logPathname = localConfigurator.getLogPathname();
    logPathname += ACE_DIRECTORY_SEPARATOR_CHAR_A;
    logPathname += Vxlogs_Folder;
    logPathname += ACE_DIRECTORY_SEPARATOR_CHAR_A;
    SVMakeSureDirectoryPathExists(logPathname.c_str());
    logPathname += "cxswitch.log";
    std::ofstream out(logPathname.c_str(), std::ios::app);
    if (!out) {
        DebugPrintf(SV_LOG_ERROR, "Cannot create file %s for local logging.\n", logPathname.c_str());
    }
    out.seekp(0, std::ios::end);

    DebugPrintf(SV_LOG_INFO, "Switching to CX ip:%s, port:%d ...\n", m_reachablecx.ip.c_str(), m_reachablecx.port);
    out << CDPUtil::getCurrentTimeAsString() << "INFO: Switching to CX ip:"
        << m_reachablecx.ip << ", port:" << m_reachablecx.port << " ...\n";


    try 
    {
        if(!::InitializeConfigurator(&m_configurator, m_reachablecx.ip,m_reachablecx.port, localConfigurator.getHostId(), PHPSerialize))
        {
            DebugPrintf(SV_LOG_ERROR, 
                "Configurator instantiation failed cx ip:%s port:%d \n",
                m_reachablecx.ip.c_str(), m_reachablecx.port);
            out << CDPUtil::getCurrentTimeAsString() << "ERROR: Configurator instantiation failed cx ip:"
                << m_reachablecx.ip << ", port:" << m_reachablecx.port << ".\n";
            rv = false;
        } 
        else
        {
            rv = true;
        }
    } 
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "Configurator instantiation failed cx ip:%s port:%d exception:%s.\n",
            m_reachablecx.ip.c_str(), m_reachablecx.port, ce.what());
        out << CDPUtil::getCurrentTimeAsString() << "ERROR: Configurator instantiation failed cx ip:"
            << m_reachablecx.ip << ", port:" << m_reachablecx.port << " exception:" << ce.what() << ".\n";
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "Configurator instantiation failed cx ip:%s port:%d exception:unknown.\n",
            m_reachablecx.ip.c_str(), m_reachablecx.port);
        out << CDPUtil::getCurrentTimeAsString() << "ERROR: Configurator instantiation failed cx ip:"
            << m_reachablecx.ip << ", port:" << m_reachablecx.port << " exception:unknown.\n";
    }

    if(rv)
    {
        if(SendRetentionRefresh())
        {
            HTTP_CONNECTION_SETTINGS http;
            inm_strncpy_s(http.ipAddress,ARRAYSIZE(http.ipAddress), m_reachablecx.ip.c_str(), sizeof(http.ipAddress));
            http.port = m_reachablecx.port;
            localConfigurator.setHttp(http);
            DebugPrintf(SV_LOG_INFO, "Agent succesfully switched to new cx ip:%s port:%d.\n", m_reachablecx.ip.c_str(), m_reachablecx.port);
            out << CDPUtil::getCurrentTimeAsString() << "INFO: Agent successfully switched to new cx ip:"
                << m_reachablecx.ip << ", port:" << m_reachablecx.port << ".\n";
        } else
        {
            DebugPrintf(SV_LOG_ERROR, 
                "Sending Retention update failed cx ip:%s port:%d.\n",m_reachablecx.ip.c_str(), m_reachablecx.port);
            out << CDPUtil::getCurrentTimeAsString() << "ERROR: Sending Retention update failed cx ip:"
                << m_reachablecx.ip << ", port:" << m_reachablecx.port << ".\n";
            rv = false;
        }

        m_configurator->Stop();
        delete m_configurator;
        m_configurator = NULL; //avoiding double deletes.
    }

    out.close();
    m_switchCx = false;
    return rv;
}

/*
* FUNCTION NAME :  VxService::FetchRemoteCxInfo
*
* DESCRIPTION : 
*   read the local persistent store and get the
*   remote cx ip, port and failover timeout
*               
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : failover timeout
*
* NOTES :
* 
*
* return value : true on success, false otherwise
*
*/
bool VxService::FetchRemoteCxInfo(SV_ULONG & timeout)
{
    bool rv = false;
    DebugPrintf(SV_LOG_INFO, "Fetching Remote CX ip and port information ...\n");

    do
    {

        try 
        {
            bool useCachedSettings = true;
            if(!GetConfigurator(&m_configurator))
            {
                DebugPrintf(SV_LOG_DEBUG, "Unable to obtain configurator %s %d\n", FUNCTION_NAME, LINE_NO);
                rv = false;
                break;
            }
            else
            {
                if(m_configurator ->getViableCxs(m_viablecxs))
                {
                    timeout = m_configurator ->getFailoverTimeout();
                    rv = true;            
                }
                else
                {
                    rv = false;
                    break;
                }
                m_configurator->Stop();
                delete m_configurator;
                m_configurator = NULL; //avoiding double deletes.
            }
        } 
        catch ( ... )
        {
            // 
            // we can get an exception if the local peristent store is not yer created or is
            // from previous version. do not log an error in this case.
            //
            rv = false;
        }
    }while(0);

    if(rv)
    {
        DebugPrintf(SV_LOG_INFO, "Fetched Remote Cx Information successfully.\n");
    } else
    {
        DebugPrintf(SV_LOG_INFO, "Remote cx information is not available ...\n");
    }        

    return rv;
}


/*
* FUNCTION NAME :  VxService::SendRetentionRefresh
*
* DESCRIPTION : 
*   get the host retention window from remote cx, and update
*   it with the latest events and ranges from the agent side
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
* return value : true on success, false otherwise
*
*/
bool VxService::SendRetentionRefresh()
{
    bool rv=true;

    DebugPrintf(SV_LOG_DEBUG, "Sending RetentionInfo to CX.\n");
    do
    {

        if(m_configurator == NULL)
        {
            rv = false;
            break;
        }

        HostRetentionWindow hostRetentionWindow;
        try
        {
            DebugPrintf(SV_LOG_DEBUG, "Getting host retention windows from CX.\n");
            hostRetentionWindow = m_configurator->getVxAgent().getHostRetentionWindow();
        }
        catch( ContextualException& ce )
        {
            DebugPrintf(SV_LOG_ERROR, 
                "Exception caught: %s, Failed to get host retention windows from CX.\n",ce.what());
            rv = false;
            break;
        }
        catch(...)
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to get host retention windows from CX.\n");
            rv = false;
            break;
        }

        HostRetentionInformation hostRetInfo;
        HostRetentionWindow::const_iterator hostRetWindowIter = hostRetentionWindow.begin();
        for(;hostRetWindowIter != hostRetentionWindow.end(); hostRetWindowIter++)
        {
            std::string dbName;
            RetentionInformation retInfo;

            dbName = m_configurator->getCdpDbName(hostRetWindowIter->first);
            if(dbName=="")
            {
                DebugPrintf(SV_LOG_ERROR, 
                    "Unable to get dbname for volume: %s, continuing with next volume\n",
                    hostRetWindowIter->first.c_str());
                continue;
            }


            if(!CDPUtil::getRetentionInfo(dbName,
                hostRetWindowIter->second.m_endTimeinNSecs, 
                retInfo, true))
            {
                DebugPrintf(SV_LOG_ERROR, 
                    "Unable to get retention information for volume: %s, continuing with next volume\n",
                    hostRetWindowIter->first.c_str());
                continue;
            }

            hostRetInfo.insert(make_pair(hostRetWindowIter->first,retInfo));
        }

        try
        {
            if(!m_configurator->getVxAgent().updateRetentionInfo(hostRetInfo))
            {
                std::stringstream l_stdfatal;
                l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                    << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                    << "Error: updateRetentionInfo returned failed status\n";

                DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
                rv = false;
                break;
            }
        }
        catch( ContextualException& ce )
        {
            DebugPrintf(SV_LOG_ERROR, 
                "Exception caught: %s, Failed to update host retention information to CX.\n",ce.what());
            rv = false;
            break;
        }
        catch(...)
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to update host retention information to CX.\n");
            rv = false;
            break;
        }

    } while(false);

    return rv;
}

/*
* FUNCTION NAME :  VxService::onFailoverCxSignal
*
* DESCRIPTION : 
*   This signal is emitted by configurator
*   if the current cx is not available for specified 
*   timeout period.
*               
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
void VxService::onFailoverCxSignal(ReachableCx reachablecx)
{
    m_reachablecx = reachablecx;
    m_switchCx = true;
}


// FUNCTION NAME :  VxService::CleanupAction
//
// DESCRIPTION: For all the target volumes with  DELETE_PENDING(replication stopped)  
//				clean the pending directory, cleans the ckdir files for that volume, unlock the target volume
//
// INPUT PARAMETERS : Initial Settings
//
// OUTPUT PARAMETERS :  None
//
// NOTES : 
//
// return value : false if dataprotection is running or fails to send to cx otherwise true
//

/**
* 1 TO N sync: 
* ===========
* Changes:
* =======
* 1. For PerformPendingPauseAction, IsDeviceInUse
*    and CleanupAction functions, added endpointDeviceName and
*    the replication direction to know whether dp is running
*/ 


bool VxService::CleanupAction(const string & deviceName, 
                              const std::string &endpointDeviceName, 
                              const CDP_DIRECTION &direction,
                              const int &grpid,
                              const string & cleanup_action,
                              const VOLUME_SETTINGS::SYNC_MODE &syncmode)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, 
        "Cleanup action needs to be taken as per the request %s for the target volume %s\n",
        cleanup_action.c_str(),deviceName.c_str());
    bool rv = true;
    std::map<string,string> cleanupActionMap;
    string error;
    stringstream cleanupString;
    string str;
    //Parsing the inputs comming from CX
    CleanupActionMap(cleanupActionMap,cleanup_action);
    //checking whether dataprotection is running on the volume or not

    if(IsDeviceInUse(deviceName, endpointDeviceName, direction, grpid, syncmode))
    {
        DebugPrintf(SV_LOG_WARNING, 
            "Exclusive lock denied for %s. Dataprotection is running on that volume\n", deviceName.c_str());
        return false;
    }

    //Delete all files related to the pair in pendingaction directory
    //SVS_OK : the files are deleted, SVS_FALSE: the volume is dismounted, SVE_FAIL: not able to delete the files
    SVERROR ret_val = CDPUtil::CleanPendingActionFiles(deviceName,error);
    if(ret_val.failed())
    {
        rv = false;
    } else if(ret_val == SVS_FALSE)
    {
        DebugPrintf(SV_LOG_DEBUG, "The device %s is dismounted from the system, so cleanup of pending action files are not executed."
            "This will be executed once the device is mounted back.\n",deviceName.c_str());
        rv = true;
    }
    else
    {
        rv = true;
    }

    if(error.empty())
        error ="0";

    VOLUME_SETTINGS::PAIR_STATE state = rv ? VOLUME_SETTINGS::PENDING_FILES_CLEANUP_COMPLETE : VOLUME_SETTINGS::PENDING_FILES_CLEANUP_FAILED;
    cleanupString << "pending_action_folder_cleanup=yes;"
        << "pending_action_folder_cleanup_status="
        << state <<";"
        << "pending_action_folder_cleanup_message="
        << error <<";";

    std::map<string,string>::iterator iter = cleanupActionMap.find("unlock_volume");
    if(iter != cleanupActionMap.end())
    {
        string mountpoint="";
        string fsType="";
        if(ACE_OS::strcasecmp(iter->second.c_str(),"YES") == 0)
        {
            cleanupString << "unlock_volume=yes;";
            std::map<string,string>::iterator it= cleanupActionMap.find("unlock_vol_mount_pt");
            if(it != cleanupActionMap.end())
            {
                cleanupString << "unlock_vol_mount_pt="
                    << it->second <<";";
                if(strcmp(it->second.c_str(),"0") != 0)
                    mountpoint = it->second;
            }
            it=cleanupActionMap.find("unlock_vol_fs");
            if(it != cleanupActionMap.end())
            {
                cleanupString << "unlock_vol_fs="
                    << it->second <<";";
                if(strcmp(it->second.c_str(),"0") != 0)
                    fsType = it->second;
            }
            rv = doUnlockVol(deviceName,mountpoint,fsType,error);
            if(error.empty())
                error ="0";
            state = rv ? VOLUME_SETTINGS::UNLOCK_COMPLETE : VOLUME_SETTINGS::UNLOCK_FAILED;
            cleanupString << "unlock_vol_status="
                << state <<";"
                << "unlock_vol_message="
                << error <<";";

        }
        else
        {
            cleanupString << "unlock_volume=no;"
                << "unlock_vol_mount_pt=0;"
                << "unlock_vol_fs=0;"
                << "unlock_vol_status=0;"
                << "unlock_vol_message=0;";
        }
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, 
            "Cleanup action for unlock_volume is not found in the cleanup_action request for the target volume %s .\n",
            deviceName.c_str());
    }
    str = cleanupString.str();
    DebugPrintf(SV_LOG_DEBUG, 
        "Sending Cleanup action status string: %s to CX for the target volume %s\n",
        str.c_str(),deviceName.c_str());

    //cleaning up cksums files for the volume
    CDPUtil::CleanTargetCksumsFiles(deviceName);

    //sending cleanup status to CX
    if(!updateCleanUpActionStatus((*m_configurator),deviceName,str))
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to update Cx with the clean up status for %s.", deviceName.c_str());
        return false;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return true;
}

// FUNCTION NAME :  VxService::IsRemoveCleanupDevice
//
// DESCRIPTION: Find the target device is in the volume group or not  
//
// INPUT PARAMETERS : Device name
//
// OUTPUT PARAMETERS :  None
//
// NOTES : 
//
// return value : false if found otherwise true
//

bool VxService::IsRemoveCleanupDevice(const string & deviceName)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;
    HOST_VOLUME_GROUP_SETTINGS hostVolumeGroupSettings = m_configurator->getHostVolumeGroupSettings();
    VOLUME_GROUP_SETTINGS::volumes_iterator volumeIter;
    VOLUME_GROUP_SETTINGS::volumes_iterator volumeEnd;
    HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator volumeGroupIter = hostVolumeGroupSettings.volumeGroups.begin();
    HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator volumeGroupEnd =  hostVolumeGroupSettings.volumeGroups.end();
    for (/* empty */; volumeGroupIter != volumeGroupEnd; ++volumeGroupIter) 
    {
        volumeEnd = (*volumeGroupIter).volumes.end();
        if(TARGET == (*volumeGroupIter).direction)
        {
            volumeIter = (*volumeGroupIter).find_by_name(deviceName);
            if(volumeIter != volumeEnd)
            {
                rv = false;
                break;
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

/*
* FUNCTION NAME :  CleanupActionMap
*
* DESCRIPTION : parse the cleanup string and stored in a map during the state DELETE_PENDING
*
* INPUT PARAMETERS : string
*
* OUTPUT PARAMETERS : map
*
* NOTES :
* 
*
* return value : none
*
*/
void VxService::CleanupActionMap(std::map<std::string,std::string> & cleanupMap,const std::string & replication_clean)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n",FUNCTION_NAME);
    svector_t svpair = CDPUtil::split(replication_clean,";");
    for(size_t i = 0; i < svpair.size(); ++i){
        svector_t currarg = CDPUtil::split(svpair[i],"=");
        if (currarg.size() == 2)
        {
            CDPUtil::trim(currarg[1]);
            CDPUtil::trim(currarg[0]);
            cleanupMap[currarg[0]]=currarg[1];
        }
        else if(currarg.size() == 1)
        {
            CDPUtil::trim(currarg[0]);
            cleanupMap[currarg[0]]="0";
        }
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}



// FUNCTION NAME :  VxService::doUnlockVol
//
// DESCRIPTION: Unlock the target volume if the user give permission
//
// INPUT PARAMETERS : String target,mountpoint, fstype
//
// OUTPUT PARAMETERS :  string (Error Message)
//
// NOTES : 
//
// return value : true if successfully unhide, otherwise false
//
bool VxService::doUnlockVol(const string & DeviceName, const string & mnt, const string & fsType, string & errorMessage)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n",FUNCTION_NAME);
    VsnapMgr *Vsnap;
    bool rv = true;
    string mountpoint;
    std::string sparsefile;
    bool isnewvirtualvolume = false;
    if(!IsVolpackDriverAvailable() && (IsVolPackDevice(DeviceName.c_str(),sparsefile,isnewvirtualvolume)))
    {
        DebugPrintf(SV_LOG_DEBUG,"There is no virtual volume driver present.So skiping unhide drive for the virtual volume %s\n",
            DeviceName.c_str());
        return true;
    }
    if(mnt.empty())
    {
#ifdef SV_WINDOWS
        mountpoint = DeviceName;
#endif
    }
    else
    {
        mountpoint = mnt;
    }
#ifdef SV_WINDOWS
    DebugPrintf(SV_LOG_DEBUG,"Mount Point %s recieved from cx for %s\n", mountpoint.c_str(), DeviceName.c_str());
    WinVsnapMgr obj;
    Vsnap=&obj;
#else
    UnixVsnapMgr obj;
    Vsnap=&obj;
    if (mountpoint.empty()) 
    {
        DebugPrintf(SV_LOG_ERROR,"Empty Mount Point recieved from cx for %s\n", DeviceName.c_str());
        errorMessage += ", Mount point is empty, so failed to unhide ";
        errorMessage += DeviceName;
        rv = false;
    }
    else
    {
        if(SVMakeSureDirectoryPathExists(mountpoint.c_str()).failed())
        {
            DebugPrintf(SV_LOG_ERROR,
                "Failed to create mount point directory %s for volume %s.\n", 
                mountpoint.c_str(), DeviceName.c_str());
            errorMessage += ", Failed to create mount point directory ";
            errorMessage +=	mountpoint;
            errorMessage +=	", so failed to unhide ";
            errorMessage += DeviceName;
            rv = false;
        }
    }
#endif
    if(rv)
    {
        string fstype;
        if(fsType.empty())
            fstype = "auto";
        else
            fstype=fsType;

        SVERROR sve = SVS_OK;
        sve = UnhideDrive_RW(DeviceName.c_str(),mountpoint.c_str(),fstype.c_str());
        if ( sve.succeeded() )
        {
            Vsnap->UnMountVsnapVolumesForTargetVolume(std::string(DeviceName.c_str()));
            DebugPrintf(SV_LOG_INFO, "%s is now accessible in read-write mode by all applications\n\n",	DeviceName.c_str());
            rv = true;
        }
        else if ( sve == SVS_FALSE )
        {
            Vsnap->UnMountVsnapVolumesForTargetVolume(std::string(DeviceName.c_str()));
            DebugPrintf(SV_LOG_ERROR,"%s was already unhidden. It is now read-write accessible by all applications\n\n",DeviceName.c_str());
            rv = true;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to unhide %s \n", DeviceName.c_str());
            errorMessage += ", Failed to unhide ";
            errorMessage += DeviceName;
            rv = false;
        }
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

// FUNCTION NAME :  VxService::IsDeviceInUse
//
// DESCRIPTION: Determines whether the dataprotection is running for the target volume 
//
// INPUT PARAMETERS : String target volume
//
// OUTPUT PARAMETERS : 
//
// NOTES : 
//
// return value : true if the device is used by dataprotection/cdpcli, otherwise false
//
/**
* 1 TO N sync: 
* ===========
* Changes:
* =======
* 1. For PerformPendingPauseAction, IsDeviceInUse
*    and CleanupAction functions, added endpointDeviceName and
*    the replication direction to know whether dp is running
* 2. Algorithm:
*    a. if source is direction and this is not direct sync, 
*       then then lock name is "source_target" else lock
*       name is "target".
*    b. This may fail if the sync mode is step 2 or diffsync
*       and we are trying to see if resync dataprotection is 
*       running or not in case of direct sync. since sync mode
*       will be SYNC_QUASIDIFF or SYNC_DIFF and we just try to 
*       acquire lock on target only. But when pair moves to
*       SYNC_QUASIDIFF or SYNC_DIFF, resync dataprotection is 
*    c. TODO: What if source is in diff sync since dp will not
*             run at all 
*        
*/ 
bool VxService::IsDeviceInUse(const std::string & deviceName,
                              const std::string & endpointDeviceName, 
                              const CDP_DIRECTION &direction,
                              const int &grpid,
                              const VOLUME_SETTINGS::SYNC_MODE &syncmode)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool isDevInUse = false;

    AgentProcess::Ptr agentInfoPtr;
    //check for target resync-I (fast/offload) or source offload sync
    if ((VOLUME_SETTINGS::SYNC_OFFLOAD == syncmode) || ( (TARGET == direction)
        && (VOLUME_SETTINGS::SYNC_DIFF != syncmode)
        && (VOLUME_SETTINGS::SYNC_QUASIDIFF != syncmode) 
        ))
    {
        if (VOLUME_SETTINGS::SYNC_DIRECT == syncmode)
            agentInfoPtr.reset(new AgentProcess(endpointDeviceName, deviceName, grpid, InMageDataprotectionlName));
        else
            agentInfoPtr.reset(new AgentProcess(deviceName, (int)false, InMageDataprotectionlName));
    }
    //check for source fast sync
    else if(direction == SOURCE)
    {
        /**
        * 1 TO N Sync: Added usage of volume group ID, since in case of two
        *              pairs having same target name ef: 
        *              fast sync pair , E -> F (tgt 1) and direct or fastsync E -> F (tgt 2)
        *              Then on source, more than one dataprotection will not be able to run
        *              since lock is E_F and log is also E/F. Hence added volume group ID in
        *              between E and F 
        */
        agentInfoPtr.reset(new AgentProcess(deviceName, endpointDeviceName, grpid, InMageDataprotectionlName));
    }
    //target resync-II or diff sync
    else
    {
        agentInfoPtr.reset(new AgentProcess("InMageVolumeGroup", grpid, InMageDataprotectionlName));
    }

    if(m_AgentProcesses.find(agentInfoPtr) != m_AgentProcesses.end())
    {
        isDevInUse = true;
    }
    if( !isDevInUse && (direction == TARGET) )
    {
        std::string lockname = deviceName;
        DebugPrintf(SV_LOG_DEBUG, "Trying to acquire lockname = %s\n", lockname.c_str());
        CDPLock cdplock(lockname);
        if (!cdplock.try_acquire())
        {
            DebugPrintf(SV_LOG_WARNING, 
                "Exclusive lock denied for %s. Dataprotection/cdpcli is using the volume.",
                deviceName.c_str());
            isDevInUse = true;
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXIT %s\n",FUNCTION_NAME);
    return isDevInUse;
}

// FUNCTION NAME :  VxService::PerformPendingPauseAction
//
// DESCRIPTION: stop the dataprotection and send response to cx for pause pending state 
//
// INPUT PARAMETERS : String volume name, int hosttype, String maintenance_action
//
// OUTPUT PARAMETERS : None
//
// NOTES : 
//
// return value : None
//

/**
* 1 TO N sync: 
* ===========
* Changes:
* =======
* 1. For PerformPendingPauseAction, IsDeviceInUse
*    and CleanupAction functions, added endpointDeviceName and
*    the replication direction to know whether dp is running
*/ 

void VxService::PerformPendingPauseAction(const LocalConfigurator &lc,
                                          const std::string &deviceName, 
                                          const std::string &endpointDeviceName, 
                                          const CDP_DIRECTION &direction,
                                          const int &grpid,
                                          const std::string &maintenance_action, 
                                          const VOLUME_SETTINGS::SYNC_MODE &syncmode,
                                          const std::string &prevResyncJobId)
{
    using namespace boost::chrono;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    int hosttype =(int)((SOURCE == direction)?HOSTSOURCE:HOSTTARGET);
    std::string msg = maintenance_action;
    std::map<std::string, steady_clock::time_point>::iterator it = m_pausePendingAckMap.find(deviceName);

    DebugPrintf(SV_LOG_ALWAYS,
        "deviceName = %s, endpointDeviceName = %s,"
        "direction = %d, maintenance_action = %s,"
        "syncmode = %d, prevResyncJobId = %s\n", 
        deviceName.c_str(), endpointDeviceName.c_str(),
        direction, maintenance_action.c_str(), syncmode, prevResyncJobId.c_str());

    /**
    * 1 TO N sync: If dp is not running, it just replaces, 
    *              pause_components_status=requested to complete
    *              why was it added for source and what are its
    *              impact on s2?
    */
    if((it == m_pausePendingAckMap.end()) ||
       (steady_clock::now() >= it->second + boost::chrono::seconds(lc.getPausePendingAckRepeatIntervalInSecs())))
    {
        if(!IsDeviceInUse(deviceName, endpointDeviceName, direction, grpid, syncmode))
        {
            DebugPrintf(SV_LOG_DEBUG, "dataprotection is not running on deviceName = %s\n", deviceName.c_str());
            //send response if success push into m_pausePendingAckMap
            size_t sz = msg.find("pause_components_status=requested");
            if(sz != std::string::npos)
            {
                msg.replace(sz,33,"pause_components_status=completed");
                DebugPrintf(SV_LOG_ALWAYS,
                    "Sending pause pending response %s to CX for the volume %s (prevResyncJobId %s)\n",
                    msg.c_str(), deviceName.c_str(), prevResyncJobId.c_str());

                if(!NotifyMaintenanceActionStatus((*m_configurator),deviceName,hosttype,msg))
                {
                    DebugPrintf(SV_LOG_ERROR, 
                        "Failed to update Cx with the pending maintenance status for %s (prevResyncJobId %s)\n", 
                        deviceName.c_str(), prevResyncJobId.c_str());
                }
                else
                {
                    m_pausePendingAckMap[deviceName] = steady_clock::now();

                    DebugPrintf(SV_LOG_ALWAYS,
                        "Successfully acknowledged pause status for %s (prevResyncJobId = %s) to Cx at %lld ticks.\n",
                        deviceName.c_str(), prevResyncJobId.c_str(), m_pausePendingAckMap[deviceName].time_since_epoch().count()/100);
                }
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ALWAYS,
                "Not acknowledging pause status for %s (prevResyncJobId = %s) to Cx, since DP is running for the device.\n",
                deviceName.c_str(), prevResyncJobId.c_str());
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ALWAYS,
            "Not acknowledging pause status for %s (prevResyncJobId = %s) to Cx, since last ack'd timepoint is %llu ticks, while current time is %llu ticks.\n",
            deviceName.c_str(), prevResyncJobId.c_str(),
            it->second.time_since_epoch().count()/100,
            steady_clock::now().time_since_epoch().count()/100);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

// FUNCTION NAME :  VxService::getDataprotectionDiffLog
//
// DESCRIPTION: get the Dataprotection log for the volume group 
//
// INPUT PARAMETERS : int volume group id
//
// OUTPUT PARAMETERS : string Logfile Name
//
// NOTES : 
//
// return value : true if successfull, otherwise false
//
bool VxService::getDataprotectionDiffLog(std::string & logFile, int id)
{
    bool rv = true;
    try
    {
        LocalConfigurator localConfigurator;
        logFile = localConfigurator.getLogPathname();
        std::string logDir = Vxlogs_Folder;
        logDir += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
        logDir += Diffsync_Folder;
        logDir += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
        logDir += boost::lexical_cast<std::string>(id);
        logDir += ACE_DIRECTORY_SEPARATOR_CHAR_A;
        logFile += logDir;
        logFile += "dataprotection.log";
    }
    catch(...)
    {
        DebugPrintf(SV_LOG_DEBUG,
            "Not able to get the diff log file for the volume group id %d.\n",
            id);
        rv = false;
    }
    return rv;
}
// FUNCTION NAME :  VxService::getDataprotectionResyncLog
//
// DESCRIPTION: get the Dataprotection log for the device 
//
// INPUT PARAMETERS : String volume name
//
// OUTPUT PARAMETERS : string logfile name
//
// NOTES : 
//
// return value : true if successfull, otherwise false
//
bool VxService::getDataprotectionResyncLog(std::string & resynclogFile, const std::string & deviceName,
                                           const std::string &endpointDeviceName, const CDP_DIRECTION &direction,
                                           const int &grpid,
                                           const VOLUME_SETTINGS::SYNC_MODE &syncmode)
{
    DebugPrintf(SV_LOG_DEBUG, "Entered %s with "
        "deviceName = %s, endpointDeviceName = %s,"
        "direction = %d, syncmode = %d\n", FUNCTION_NAME,
        deviceName.c_str(), endpointDeviceName.c_str(),
        direction, syncmode);
    //TODO: Need to remove all hard coded constants
    bool rv = false;
    try
    {
        LocalConfigurator localConfigurator;
        std::string logFile = localConfigurator.getLogPathname();
        std::string logDir = Vxlogs_Folder;
        logDir += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
        logDir += Resync_Folder;
        logDir += ACE_DIRECTORY_SEPARATOR_CHAR_A ;

        logDir += GetVolumeDirName(deviceName);
        logDir += ACE_DIRECTORY_SEPARATOR_CHAR_A;

        if (SOURCE == direction)
        {
            std::string srclogDir = logDir;
            std::string srclogFile = logFile;
            std::stringstream strgrpid;
            strgrpid << grpid;
            srclogDir += strgrpid.str();
            srclogDir += ACE_DIRECTORY_SEPARATOR_CHAR_A;
            srclogDir += GetVolumeDirName(endpointDeviceName);
            srclogDir += ACE_DIRECTORY_SEPARATOR_CHAR_A;
            srclogFile += srclogDir;
            srclogFile += "dataprotection.log";

            if (File::IsExisting(srclogFile))
            {
                resynclogFile = srclogFile;
                rv = true;
            } 
            else
            {
                srclogDir = logDir;
                srclogFile = logFile;
                srclogDir += GetVolumeDirName(endpointDeviceName);
                srclogDir += ACE_DIRECTORY_SEPARATOR_CHAR_A;
                srclogFile += srclogDir;
                srclogFile += "dataprotection.log";
                if (File::IsExisting(srclogFile))
                {
                    resynclogFile = srclogFile;
                    rv = true;
                }
            }
        }

        if (!rv)
        {
            logFile += logDir;
            logFile += "dataprotection.log";

            if (File::IsExisting(logFile))
            {
                resynclogFile = logFile;
                rv = true;
            }
        }

        if (rv)
        {
            DebugPrintf(SV_LOG_DEBUG, "The resynclogFile formed is %s\n", resynclogFile.c_str());
        }
    }
    catch(...)
    {
        DebugPrintf(SV_LOG_DEBUG,
            "Not able to get the resync log file for the device %s.\n",
            deviceName.c_str());
        rv = false;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITING %s with rv = %s\n", FUNCTION_NAME, rv?"true":"false");
    return rv;
}
// FUNCTION NAME :  VxService::deleteDataprotectionLog
//
// DESCRIPTION: delete the given log file 
//
// INPUT PARAMETERS : String log file
//
// OUTPUT PARAMETERS : None
//
// NOTES : 
//
// return value : true on success, otherwise false 
//
bool VxService::deleteDataprotectionLog(const std::string & filename)
{
    bool rv = true;
    do
    {
        ACE_stat statbuf = {0};
        // PR#10815: Long Path support
        if (0 != sv_stat(getLongPathName(filename.c_str()).c_str(), &statbuf))
        {
            rv = false;
            break;
        }

        if ( (statbuf.st_mode & S_IFREG)!= S_IFREG )
        {
            rv = false;
            break;
        }
    } while (FALSE);
    if(rv)
    {
        /* kept just to test instrument this code
        std::string cpy = filename;
        cpy += ".copy";
        rename(filename.c_str(), cpy.c_str());
        */

        // PR#10815: Long Path support 
        if(ACE_OS::unlink(getLongPathName(filename.c_str()).c_str()) < 0)
        {
            DebugPrintf(SV_LOG_DEBUG,
                "The dataprotection log file (%s) cannot be deleted.\n",
                filename.c_str());
        }
    }
    return rv;
}


void VxService::StopAgentsHavingAttributes(const std::string &DeviceName, const std::string &EndpointDeviceName, const int &group, const uint32_t &waitTimeBeforeTerminate)
{
    std::stringstream ss;
    ss << "DeviceName " << DeviceName << ", EndpointDeviceName " << EndpointDeviceName << ", group " << group;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s with %s\n", FUNCTION_NAME, ss.str().c_str());

    ACE_Time_Value time_value;
    time_value.set(AGENTSTIME_TOQUIT_INSECS, 0);

    boost::chrono::steady_clock::time_point endtimeToWaitForQuit = boost::chrono::steady_clock::now();
    if (waitTimeBeforeTerminate)
        endtimeToWaitForQuit += boost::chrono::seconds(waitTimeBeforeTerminate);
    else
        endtimeToWaitForQuit = boost::chrono::steady_clock::time_point::max();

    do {
        AgentProcessesIterVec_t agentprocesses;
        GetAgentsHavingAttributes(DeviceName, EndpointDeviceName, group, agentprocesses);
        if (0 == agentprocesses.size()) {
            DebugPrintf(SV_LOG_DEBUG, "Did not find agent process(es) with above attributes.\n");
            break;
        }
        DebugPrintf(SV_LOG_DEBUG, "Agent process(es) exist for above attributes. Issuing quit message.\n");
        for (AgentProcessesIterVec_t::iterator vit = agentprocesses.begin(); vit != agentprocesses.end(); vit++) {
            AgentProcessesIter_t it = *vit;
            const boost::shared_ptr<AgentProcess> &ap = *it;
            if (boost::chrono::steady_clock::now() > endtimeToWaitForQuit) {
                ap->Stop(m_ProcessMgr);
            }
            else if(!PostQuitMessageToAgent(ap))
                DebugPrintf(SV_LOG_ERROR, "Posting quit message failed on trying to stop agent(s) having attributes %s. It will be retried.\n", ss.str().c_str());
        }
        DebugPrintf(SV_LOG_DEBUG, "Quit message has been posted for agent(s) having above attributes. Waiting for these to exit...\n");
        if(WAIT_FOR_ERROR == WaitForMultipleAgents(time_value))
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
        AgentProcess::Ptr agentInfoPtr(new AgentProcess(DeviceName, EndpointDeviceName, group, InMageDataprotectionlName));
        AgentProcesses_t::iterator findIter = m_AgentProcesses.find(agentInfoPtr);
        if (findIter != m_AgentProcesses.end()) 
            agentprocesses.push_back(findIter);
    } else if (isdevicenamevalid && (!isendpointvalid) && isgroupvalid) {
        DebugPrintf(SV_LOG_DEBUG, "Searching agent process having devicename %s and group %d.\n", 
                                  DeviceName.c_str(), group);
        AgentProcess::Ptr agentInfoPtr(new AgentProcess(DeviceName, group, InMageDataprotectionlName));
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


void VxService::StopSentinel()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    StopAgentsHavingAttributes(InMageSentinelName, std::string(), InMageSentinelId);
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
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


void VxService::StopCacheManager()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    StopAgentsHavingAttributes(InMageCacheMgrName, std::string(), InMageCacheMgrId);
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


void VxService::StopCdpManager()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    StopAgentsHavingAttributes(InMageCdpMgrName, std::string(), InMageCdpMgrId);
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


bool VxService::CheckCacheDir(void)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;

    std::string dir = m_configurator->getVxAgent().getCacheDirectory();
    try {
        securitylib::setPermissions(dir, securitylib::SET_PERMISSIONS_NAME_IS_DIR);
    } catch(std::exception& e) {
        DebugPrintf(SV_LOG_ERROR, "Failed to access cache dir %s with error %s\n", dir.c_str(), e.what());
        CacheDirBadAlert a(dir);
        rv = SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_GENERAL, a);
        CDPUtil::QuitRequested(120);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
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
        if (isperformed){
            ConfigureDataPagedPool(lc);
        }
#endif
    }
    else {
        //log error because local configurator says no driver, but ther are source side settings.
        isperformed = false;
        DebugPrintf(SV_LOG_ERROR, "Filter driver is not available as per agent local configurator, but there are source devices to drain.\n");
    }

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


    SV_ULONGLONG    ullTotalPhysicalRamInMB = memoryStatusEx.ullTotalPhys/(1024*1024);
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
    const unsigned FAILURE_NO_FOR_ALERT = 15;
    //Notify with retry
    while (!this->quit) {
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
                //Failure to notify. Retry along with logging of alert
                DebugPrintf(SV_LOG_ERROR, "Failed to issue process shutdown notify with error: %s\n", e.what());
            }

#ifdef SV_UNIX
            UpdateCleanShutdownFile();
#endif
            CDPUtil::QuitRequested(60);
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

void VxService::LogSourceReplicationStatus()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (!m_isAgentRoleSource)
        return;

    static bool initialiUpdateComplete = false;
    static uint64_t lastLogTime = GetTimeInSecSinceEpoch1970();
    
    uint32_t waitInterval = initialiUpdateComplete ? m_SrcTelemeteryPollInterval : m_SrcTelemeteryStartDelay;
    uint64_t currentTime = GetTimeInSecSinceEpoch1970();
    if ((currentTime - lastLogTime) < waitInterval)
        return;

    ACE_Guard<ACE_Mutex> PropertyChangeGuard(m_PropertyChangeLock);

    HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator oldVolGrpIter = m_volGroups.volumeGroups.begin();

    while (oldVolGrpIter != m_volGroups.volumeGroups.end())
    {
        if (oldVolGrpIter->direction == SOURCE)
        {
            LogSourceReplicationStatus(*oldVolGrpIter, LOG_PROTECTED);
        }
        oldVolGrpIter++;
    }

    initialiUpdateComplete = true;
    lastLogTime = currentTime;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

void VxService::LogSourceReplicationStatus(const VOLUME_GROUP_SETTINGS &vgs, uint32_t reason)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (!m_isAgentRoleSource)
        return;

    ReplicationStatus repStatus;

    ADD_INT_TO_MAP(repStatus, LOGREASON, reason);
    ADD_INT_TO_MAP(repStatus, MESSAGETYPE, SOURCE_TABLE_AGENT_LOG);

    try {

        FillReplicationStatusFromSettings(repStatus, vgs);

        FillReplicationStatusFromDriverStats(repStatus, vgs);

        FillReplicationStatusWithSystemInfo(repStatus);

        std::string strRepStatus = JSON::producer<ReplicationStatus>::convert(repStatus);
        gSrcTelemetryLog.Printf(strRepStatus);
    }
    catch (std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s : failed to serialize replication status. %s\n", FUNCTION_NAME, e.what());
    }
    
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

void VxService::FillReplicationStatusFromDriverStats(ReplicationStatus &repStatus, const VOLUME_GROUP_SETTINGS &vgs)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (m_sPFilterDriverNotifier == NULL)
    {
        DebugPrintf(SV_LOG_DEBUG, "%s : Filter driver is not initialized.\n", FUNCTION_NAME);
        ADD_INT_TO_MAP(repStatus, MESSAGETYPE, SOURCE_TABLE_DRIVER_NOT_LOADED);
        ADD_INT_TO_MAP(repStatus, DRVLOADED, DISK_DRIVER_NOT_LOADED);

        return;
    }

    DRIVER_VERSION stDrvVersion;
    if (m_FilterDrvIf.GetDriverVersion(m_sPFilterDriverNotifier->GetDeviceStream(), stDrvVersion))
    {

        std::stringstream ssDrvVersion;
        ssDrvVersion << stDrvVersion.ulDrMajorVersion << "." << stDrvVersion.ulDrMinorVersion << ".";
        ssDrvVersion << stDrvVersion.ulDrMinorVersion2 << "." << stDrvVersion.ulDrMinorVersion3;

        std::stringstream ssDrvProdVersion;
        ssDrvProdVersion << stDrvVersion.ulPrMajorVersion << "." << stDrvVersion.ulPrMinorVersion << ".";
        ssDrvProdVersion << stDrvVersion.ulPrMinorVersion2 << "." << stDrvVersion.ulPrBuildNumber;

        ADD_STRING_TO_MAP(repStatus, DRVVER, ssDrvVersion.str());
        ADD_STRING_TO_MAP(repStatus, DRVPRODVER, ssDrvProdVersion.str());
        ADD_INT_TO_MAP(repStatus, DRVLOADED, DISK_DRIVER_LOADED);

        if (DRIVER_SUPPORTS_EXTENDED_VOLUME_STATS !=
                (stDrvVersion.ulDrMinorVersion3 & DRIVER_SUPPORTS_EXTENDED_VOLUME_STATS))
        {
            DebugPrintf(SV_LOG_DEBUG,
                "%s : Filter driver version %s doesn't support extended stats.\n",
                FUNCTION_NAME,
                ssDrvVersion.str().c_str());

            ADD_INT_TO_MAP(repStatus, MESSAGETYPE, SOURCE_TABLE_DRIVER_STATS_NOT_SUPPORTED);

            return;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s : Filter driver version ioctl failed.\n",
            FUNCTION_NAME);

        ADD_INT_TO_MAP(repStatus, MESSAGETYPE, SOURCE_TABLE_DRIVER_VERSION_FAILED);
        return;
    }

    VOLUME_GROUP_SETTINGS::volumes_constiterator volIter = vgs.volumes.begin();
    size_t statSize = 0;

#ifdef SV_WINDOWS
    statSize = sizeof(VOLUME_STATS_DATA) + sizeof(VOLUME_STATS);
#else
    statSize = sizeof(TELEMETRY_VOL_STATS);
#endif

    VOLUME_STATS_DATA* drvStats = 0;
    drvStats = (VOLUME_STATS_DATA *) new (std::nothrow) char[statSize];

    if (NULL == drvStats)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s: failed to allocate memory for driver stats. size %d",
            FUNCTION_NAME,
            statSize);
        return;
    }

    memset((void *)drvStats, 0, statSize);

    if (m_FilterDrvIf.GetDriverStats(volIter->first,
        volIter->second.devicetype,
        m_sPFilterDriverNotifier->GetDeviceStream(),
        drvStats,
        statSize))
    {
        uint64_t nonPgMemLimit = drvStats->ulNonPagedMemoryLimitInMB;
        nonPgMemLimit *= 1024 * 1024;
        ADD_INT_TO_MAP(repStatus, NONPAGEDPOOLLIMIT, nonPgMemLimit);

        ADD_INT_TO_MAP(repStatus, LOCKDATABLKCNT, drvStats->LockedDataBlockCounter);


        ADD_INT_TO_MAP(repStatus, NUMDISKPROTBYDRV, drvStats->ulNumProtectedDisk);
        ADD_INT_TO_MAP(repStatus, TOTALNUMDISKBYDRV, drvStats->ulTotalVolumes);

        uint64_t drvFlags = 0;
        // 63-40 - 24 bits - reserved
        // 39-38 - 02 bits - drvStats->eDiskFilterMode
        // 37-36 - 02 bits - drvStats->eServiceState
        // 35-34 - 02 bits - drvStats->LastShutdownMarker
        // 33-32 - 02 bits - drvStats->PersistentRegistryCreated
        // 31-00 - 32 bits - drvStats->ulDriverFlags

        uint64_t flags = 0;
        flags = drvStats->eDiskFilterMode;
        drvFlags |= (flags << DISK_FILTER_MODE_SHIFT);

        flags = drvStats->eServiceState;
        drvFlags |= (flags << SERVICE_STATE_SHIFT);

        flags = drvStats->LastShutdownMarker;
        drvFlags |= (flags << LAST_SHUTDOWN_MARKER_SHIFT);

        flags = drvStats->PersistentRegistryCreated;
        drvFlags |= (flags << PERSISTENT_REG_CREATED_SHIFT);

        flags = drvStats->ulDriverFlags;
        drvFlags |= (flags << DRIVER_FLAGS_SHIFT);

        ADD_INT_TO_MAP(repStatus, DRVFLAGS, drvFlags);

        ADD_INT_TO_MAP(repStatus, SYSBOOTCNT, drvStats->ulCommonBootCounter);
        ADD_INT_TO_MAP(repStatus, DATAPOOLSIZESET, drvStats->ullDataPoolSizeAllocated);
        ADD_INT_TO_MAP(repStatus, PERSTIMESTAMPONLASTBOOT, drvStats->ullPersistedTimeStampAfterBoot);
        ADD_INT_TO_MAP(repStatus, PERSTIMESEQONLASTBOOT, drvStats->ullPersistedSequenceNumberAfterBoot);

        if (drvStats->ulVolumesReturned != 0)
        {
#ifdef SV_WINDOWS
            PVOLUME_STATS   volumeStats = (PVOLUME_STATS)((PUCHAR)drvStats + sizeof(VOLUME_STATS_DATA));
#else
            VOLUME_STATS_V2 *volumeStats = (VOLUME_STATS_V2 *)&(((TELEMETRY_VOL_STATS *)drvStats)->vol_stats);
#endif
            ADD_INT_TO_MAP(repStatus, DATAPOOLSIZEMAX, volumeStats->ullDataPoolSize);
            ADD_INT_TO_MAP(repStatus, DRVLOADTIME, volumeStats->liDriverLoadTime.QuadPart);

            ADD_INT_TO_MAP(repStatus, STOPALL, volumeStats->liStopFilteringAllTimeStamp.QuadPart);

            ADD_INT_TO_MAP(repStatus, TIMEBEFOREJUMP, volumeStats->llTimeJumpDetectedTS);
            ADD_INT_TO_MAP(repStatus, TIMEJUMPED, volumeStats->llTimeJumpedTS);

#ifdef SV_WINDOWS
            uint32_t fltState =
                (volumeStats->ulVolumeFlags & VSF_FILTERING_STOPPED) ? DISK_FILTERING_STOP : DISK_FILTERING_START;
            ADD_INT_TO_MAP(repStatus, FLTSTATE, fltState);
#endif

            ADD_INT_TO_MAP(repStatus, DEVICEDRVSIZE, volumeStats->ulVolumeSize.QuadPart);
            ADD_INT_TO_MAP(repStatus, DEVCONTEXTTIME, volumeStats->liVolumeContextCreationTS.QuadPart);
            ADD_INT_TO_MAP(repStatus, STARTFILTKERN, volumeStats->liStartFilteringTimeStamp.QuadPart);
            ADD_INT_TO_MAP(repStatus, STARTFILTUSER, volumeStats->liStartFilteringTimeStampByUser.QuadPart);

            ADD_INT_TO_MAP(repStatus, STOPFILTKERN, volumeStats->liStopFilteringTimeStamp.QuadPart);
            ADD_INT_TO_MAP(repStatus, STOPFILTUSER, volumeStats->liStopFilteringTimestampByUser.QuadPart);

            ADD_INT_TO_MAP(repStatus, CLEARDIFF, volumeStats->liClearDiffsTimeStamp.QuadPart);
            ADD_INT_TO_MAP(repStatus, CHURN, volumeStats->ullTotalTrackedBytes);

            ADD_INT_TO_MAP(repStatus, LASTS2START, volumeStats->liLastS2StartTime.QuadPart);
            ADD_INT_TO_MAP(repStatus, LASTS2STOP, volumeStats->liLastS2StopTime.QuadPart);

            ADD_INT_TO_MAP(repStatus, LASTSVCSTART, volumeStats->liLastAgentStartTime.QuadPart);
            ADD_INT_TO_MAP(repStatus, LASTSVCSTOP, volumeStats->liLastAgentStopTime.QuadPart);

            ADD_INT_TO_MAP(repStatus, LASTCOMMITDB, volumeStats->liCommitDBTimeStamp.QuadPart);
            ADD_INT_TO_MAP(repStatus, LASTDRAINDB, volumeStats->liGetDBTimeStamp.QuadPart);

            ADD_INT_TO_MAP(repStatus, LASTTAGREQTIME, volumeStats->liLastTagReq.QuadPart);
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR,
                "%s: driver stats not returned for disk %s\n.",
                FUNCTION_NAME,
                volIter->first.c_str());

            ADD_INT_TO_MAP(repStatus, MESSAGETYPE, SOURCE_TABLE_AGENT_DISK_REMOVED);
        }
    }

    if (NULL != drvStats)
        delete drvStats;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

void VxService::FillReplicationStatusFromSettings(ReplicationStatus &repStatus, const VOLUME_GROUP_SETTINGS &vgs)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    VOLUME_GROUP_SETTINGS::volumes_constiterator volIter = vgs.volumes.begin();
    
    ADD_STRING_TO_MAP(repStatus, PROTDISKID, volIter->first);
    ADD_INT_TO_MAP(repStatus, RAWSIZEINPOLICY, volIter->second.GetRawSize());

    ADD_STRING_TO_MAP(repStatus, PSIPADDR, vgs.transportSettings.ipAddress);
    ADD_STRING_TO_MAP(repStatus, PSPORT, vgs.transportSettings.sslPort);
    ADD_STRING_TO_MAP(repStatus, CSIPADDR, std::string(m_configurator->getHttpSettings().ipAddress));

    ADD_INT_TO_MAP(repStatus, RESYNCSETINPOLICY, volIter->second.resyncRequiredFlag);
    uint64_t srcResyncStarttime = volIter->second.srcResyncStarttime;
    uint64_t srcResyncEndtime = volIter->second.srcResyncEndtime;
    uint64_t resyncSetTime = volIter->second.resyncRequiredTimestamp;

#ifdef SV_UNIX
    uint64_t diff = GetSecsBetweenEpoch1970AndEpoch1601();
    // convert to 100 nano sec
    diff *= 10 * 1000 * 1000;
    srcResyncStarttime += (0 == srcResyncStarttime) ? 0 : diff;
    srcResyncEndtime += (0 == srcResyncEndtime) ? 0 : diff;
    resyncSetTime += (0 == resyncSetTime) ? 0 : diff;
#endif

    ADD_INT_TO_MAP(repStatus, RESYNCSTART, srcResyncStarttime);
    ADD_INT_TO_MAP(repStatus, RESYNCEND, srcResyncEndtime);
    ADD_INT_TO_MAP(repStatus, RESYNCTIMESTAMP, resyncSetTime);
    ADD_INT_TO_MAP(repStatus, RESYNCCAUSE, volIter->second.resyncRequiredCause);
    ADD_INT_TO_MAP(repStatus, RESYNCCOUNTER, volIter->second.resyncCounter);

    uint64_t pairState = 0;
    if ((volIter->second.syncMode == VOLUME_SETTINGS::SYNC_DIFF) ||
        (volIter->second.syncMode == VOLUME_SETTINGS::SYNC_QUASIDIFF))
        pairState = PAIR_STATE_DIFF;
    else if (volIter->second.syncMode == VOLUME_SETTINGS::SYNC_FAST_TBC)
        pairState = PAIR_STATE_RESYNC;

    if (volIter->second.pairState == VOLUME_SETTINGS::PAUSE)
        pairState |= PAIR_STATE_PAUSED;

    if (volIter->second.throttleSettings.IsSourceThrottled())
        pairState |= PAIR_STATE_DIFF_THROTTLE;

    if (volIter->second.throttleSettings.IsResyncThrottled())
        pairState |= PAIR_STATE_SYNC_THROTTLE;

    ADD_INT_TO_MAP(repStatus, PAIRSTATE, pairState);
    ADD_INT_TO_MAP(repStatus, NUMOFDISKPROT, m_volGroups.volumeGroups.size());

    ADD_INT_TO_MAP(repStatus, DISKAVB, DISK_NOT_AVAILABLE);

    uint32_t numDiskDiscovered = 0;

    std::string dataCacherError;
    if (!DataCacher::UpdateVolumesCache(m_VolumesCache, dataCacherError))
    {
        DebugPrintf(SV_LOG_ERROR, "%s %s\n", FUNCTION_NAME, dataCacherError.c_str());
        ADD_STRING_TO_MAP(repStatus, MESSAGE, dataCacherError);
        ADD_INT_TO_MAP(repStatus, MESSAGETYPE, SOURCE_TABLE_AGENT_DISK_CACHE_NOT_FOUND);
        return;
    }

    ConstVolumeSummariesIter diter = m_VolumesCache.m_Vs.begin();
    for ( /* empty */; diter != m_VolumesCache.m_Vs.end(); diter++)
    {
        const VolumeSummary &dvseach = *diter;
        if (VolumeSummary::DISK == dvseach.type)
        {
            numDiskDiscovered++;

            if (dvseach.name == volIter->first)
            {
                ADD_INT_TO_MAP(repStatus, RAWSIZEAGENT, dvseach.rawcapacity);
                ADD_INT_TO_MAP(repStatus, DISKAVB, DISK_AVAILABLE);
                
                ConstAttributesIter_t iter = dvseach.attributes.find(NsVolumeAttributes::DEVICE_NAME);
                if (iter != dvseach.attributes.end())
                    ADD_STRING_TO_MAP(repStatus, DISKNAME, iter->second);
            }
        }
    }

    ADD_INT_TO_MAP(repStatus, NUMDISKBYAGENT, numDiskDiscovered);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

void VxService::FillReplicationStatusWithSystemInfo(ReplicationStatus &repStatus)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    // hostname
    std::string hostName;
    if (m_configurator->getVxAgent().getUseConfiguredHostname()) {
        hostName = m_configurator->getVxAgent().getConfiguredHostname();
    }
    else {
        hostName = Host::GetInstance().GetHostName();
    }

    // ip address
    std::string ipAddress;
    if (m_configurator->getVxAgent().getUseConfiguredIpAddress()) {
        ipAddress = m_configurator->getVxAgent().getConfiguredIpAddress();
    }
    else {
        ipAddress = Host::GetInstance().GetIPAddress();
    }

    ADD_STRING_TO_MAP(repStatus, HOSTNAME, hostName);
    ADD_STRING_TO_MAP(repStatus, HOSTIPADDR, ipAddress);

    std::string macAddrs;
    NicInfos_t nicinfos;
    GetNicInfos(nicinfos);
    ConstNicInfosIter_t nicIter = nicinfos.begin();
    while (nicIter != nicinfos.end())
    {
        macAddrs += nicIter->first;
        boost::trim(macAddrs);
        macAddrs += ",";
        nicIter++;
    }

    ADD_STRING_TO_MAP(repStatus, MACADDR, macAddrs);

    const Object & osinfo = OperatingSystem::GetInstance().GetOsInfo();
    ConstAttributesIter_t iter = osinfo.m_Attributes.find(NSOsInfo::CAPTION);
    if (iter != osinfo.m_Attributes.end())
        ADD_STRING_TO_MAP(repStatus, SRCOSVER, iter->second);

    ADD_INT_TO_MAP(repStatus, SYSTIME, GetTimeInSecSinceEpoch1970());
    ADD_INT_TO_MAP(repStatus, RAMSIZE, Host::GetInstance().GetAvailableMemory());

    unsigned long long freeMemory = 0;
    if (Host::GetInstance().GetFreeMemory(freeMemory))
        ADD_INT_TO_MAP(repStatus, AVBPHYMEM, freeMemory);

    unsigned long long sysUpTime = 0;
    if (Host::GetInstance().GetSystemUptimeInSec(sysUpTime))
    {
        ADD_INT_TO_MAP(repStatus, SYSUPTIME, sysUpTime);
    
        uint64_t sysBootTime = GetTimeInSecSinceEpoch1970();
        sysBootTime -= sysUpTime;
        ADD_INT_TO_MAP(repStatus, SYSBOOTTIME, sysBootTime);
    }

    ADD_STRING_TO_MAP(repStatus, SRCAGENTVER, InmageProduct::GetInstance().GetProductVersion());

    std::string strKernelVersion;
    GetKernelVersion(strKernelVersion);
    ADD_STRING_TO_MAP(repStatus, KERNELVERSION, strKernelVersion);

    ADD_STRING_TO_MAP(repStatus, AGENTRESOURCEID, m_configurator->getVxAgent().getResourceId());
    ADD_STRING_TO_MAP(repStatus, AGENTSOURCEGROUPID, m_configurator->getVxAgent().getSourceGroupId());

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}
