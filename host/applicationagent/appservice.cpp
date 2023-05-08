#include "appglobals.h"

#include <exception>
#include <sstream>
#include <fstream>
#include <cstdio>
#ifdef SV_WINDOWS
#include <windows.h>
#include <winioctl.h>
#include <atlbase.h>
#include <psapi.h>
#endif
#include <ace/Init_ACE.h>
#include "ace/ACE.h"
#include "ace/Log_Msg.h"
#include "ace/OS_main.h"
#include "config/appconfigurator.h"
#include "config/appconfchangemediator.h"
#include "configwrapper.h"
#include "appservice.h"
#include "appcommand.h"
#include "logger.h"
#include <ace/OS_NS_errno.h>
#include "../tal/HandlerCurl.h"
#include <ace/os_include/os_limits.h>
#include "cdputil.h"
#include "config/appconfchangemediator.h"
#include "app/appfactory.h"
#include "appexception.h"
#include "serializationtype.h"
#include <boost/lexical_cast.hpp>
#include "util/common/util.h"
#include "configurator2.h"
#include "inmsafecapis.h"
#include "terminateonheapcorruption.h"
#include "hostrecoverymanager.h"

#include <boost/filesystem.hpp>
#include "LogCutter.h"

#ifdef SV_WINDOWS
extern bool RebootMachine();
#endif

using namespace std;
using namespace Logger;

Configurator* TheConfigurator = 0; // singleton

const int SV_FAILURE            = -9999;
const int SV_SUCCESS            = 20000;
int const SETTINGS_POLLING_INTERVAL = 15;

ACE_Time_Value LastRegisterHostTime_g;
bool quit_signalled = false;

extern Log gLog;

static bool SetupLocalLogging(
    boost::shared_ptr<Logger::LogCutter> logCutter,
    std::string &errMessage)
{
    bool rv = false;

    try 
    {
        AppLocalConfigurator localconfigurator ;

        tal::set_curl_verbose(localconfigurator.get_curl_verbose());

        SetDaemonMode();
        boost::filesystem::path logPath(localconfigurator.getLogPathname()); 
        logPath /= "appservice.log";

        const int maxCompletedLogFiles = localconfigurator.getLogMaxCompletedFiles();
        const boost::chrono::seconds cutInterval(localconfigurator.getLogCutInterval());
        const uint32_t logFileSize = localconfigurator.getLogMaxFileSize();

        logCutter->Initialize(logPath, maxCompletedLogFiles, cutInterval);
        
        SetLogLevel(localconfigurator.getLogLevel());
        SetLogInitSize(logFileSize);
        
        SetLogHostId( localconfigurator.getHostId().c_str() );
        SetLogRemoteLogLevel( SV_LOG_DISABLE );
        SetSerializeType( localconfigurator.getSerializerType() ) ;
        SetLogHttpsOption(localconfigurator.IsHttps());
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


#ifdef SV_WINDOWS
//If you change InMageAgent, please do the necessary modifications in winservice.h as well
ACE_NT_SERVICE_DEFINE (InMageAgent, WinService, SERVICE_NAME)

std::string GetRecoveryCmdLogFile()
{
    LocalConfigurator lConfig;
    std::stringstream outfile;
    outfile << lConfig.getInstallPath()
        << "\\Failover\\Data\\CmdOut_" << ACE_OS::getpid()
        << "_" << GenerateUuid() << ".log";

    return outfile.str();
}

#endif /* SV_WINDOWS */

int
ACE_TMAIN (int argc, ACE_TCHAR* argv[])
{
    TerminateOnHeapCorruption();
    init_inm_safe_c_apis();
    ACE::init();

#ifdef SV_UNIX
    for (int i = ACE::max_handles () - 1; i >= 0; i--)
        ACE_OS::close (i);
#endif


    AppService srv;
    MaskRequiredSignals();

    int ret = 0;
    {
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
                if (!(IsProcessRunning(ACE_TEXT_ALWAYS_CHAR(ACE::basename(argv[0], ACE_DIRECTORY_SEPARATOR_CHAR)))))
                    break;
                else
                    ACE_OS::sleep(1);
            }
            if (RETRIES == iTries)
            {
                cout << "appservice already running.Exiting...\n";
                DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
                return 0;
            }
        }
        try
        {
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

            try
            {
                srv.m_LogCutters.push_back(logCutter);

                if ((ret = srv.run_startup_code()) == -1)
                {
                    ret = AppService::StartWork(&srv);
                }
            }
            catch (std::exception& e)
            {
                ret = -1;
                DebugPrintf(SV_LOG_FATAL, "Exception thrown: %s\n", e.what());
            }
        }
        catch (...)
        {
            DebugPrintf(SV_LOG_FATAL, "Exception occurred during startup. Failed to start service. Exiting...\n");
            return ret;
        }


        LOG_EVENT((LM_DEBUG, "Service stopped"));

        DebugPrintf(">>>>>>> EXIT appservice <<<<<<<\n");

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    }
    CloseDebug();
    return(ret);

}
AppService::AppService() : m_bInitialized(false),opt_install (0), opt_remove (0), opt_start (0), 
opt_kill (0), quit(0), m_lastUpdateTime(0),m_bUninstall(false)
{
}

AppService::~AppService()
{
    this->sig_handler_.remove_handler(SIGTERM);
    this->sig_handler_.remove_handler(SIGUSR1);
}

int AppService::parse_args(int argc, ACE_TCHAR* argv[])
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

        else
            iRet = (-1);
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return iRet;
}

void AppService::display_usage(char *exename)
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


int AppService::install_service()
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
        LOG_EVENT((LM_ERROR, "Service: %s could not be installed",SERVICE_NAME));
#ifdef SV_WINDOWS
        ::MessageBox(NULL, "Service could not be installed", ACE_TEXT_ALWAYS_CHAR(SERVICE_NAME), MB_OK);
#endif /* SV_WINDOWS */
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return ret;
}

int AppService::remove_service()
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
int AppService::start_service()
{
    return (SERVICE::instance ()->start_service());  
}

int AppService::stop_service()
{
    return (SERVICE::instance ()->stop_service());  
}

void AppService::name(const ACE_TCHAR *name, const ACE_TCHAR *long_name, const ACE_TCHAR *desc)
{
    SERVICE::instance ()->name (SERVICE_NAME, SERVICE_TITLE, SERVICE_DESC);
}
int AppService::run_startup_code()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try {
        DebugPrintf( SV_LOG_DEBUG, "run_startup_code 1\n" );
        SERVICE::instance()->RegisterStartCallBackHandler(AppService::StartWork, this);
        SERVICE::instance()->RegisterStopCallBackHandler(AppService::StopWork, this);        

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
    return (SERVICE::instance ()->run_startup_code());	
}



int AppService::StartWork(void *data)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
  
    AppService *srv = NULL;
    try {
        LOG_EVENT((LM_DEBUG, "Service started"));

        srv = (AppService *)data;

        if (srv == NULL)
            return 0;
        //
        // Register signal handlers
        //
        srv->sig_handler_.register_handler(SIGTERM, srv);   
        srv->sig_handler_.register_handler(SIGUSR1, srv);   

        AppLocalConfigurator localConfigurator;
        if (localConfigurator.isMobilityAgent() &&
            (localConfigurator.IsAzureToAzureReplication() ||
            (boost::iequals(localConfigurator.getCSType(), CSTYPE_CSPRIME) &&
            localConfigurator.getMigrationState() != Migration::MIGRATION_ROLLBACK_PENDING)))
        {
            DebugPrintf(SV_LOG_ERROR, "%s : Control plane is RCM. Pausing the service until stopped.\n", FUNCTION_NAME);
            bool doExit = true;
            while (!srv->quit)
            {
                ACE_OS::sleep(5);

                //Check if there is a pending migration rollback for CSPrime
                if (boost::iequals(localConfigurator.getCSType(), CSTYPE_CSPRIME))
                {
                    LocalConfigurator lc;
                    if (lc.getMigrationState() == Migration::MIGRATION_ROLLBACK_PENDING)
                    {
                        doExit = false;
                        break;
                    }
                }
            }
            if (doExit)
            {
                DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
                return 0;
            }
        }

        //
        // Complete the recovery if recovery is in-progress.
        //
        if (srv->CompleteRecovery() != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR, "Recovery was not successful. \
                                       Verify the recovery command errors, \
                                       correct the errors and start the service again.\n");
            if (!srv->quit) // If already quit signaled then no need to call StopWork again
                StopWork(data);
        }

        //
        // If its a test failover VM then exit the service.
        //
        if (!srv->quit && IsItTestFailoverVM())
        {
            DebugPrintf(SV_LOG_INFO, "This is a Test Failover Machine. Service will be stoped.\n");
            StopWork(data);
        }

        std::list < boost::shared_ptr < Logger::LogCutter > > ::iterator it = srv->m_LogCutters.begin();
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

                if ( SV_SUCCESS == srv->init() )
                {
                    DebugPrintf( SV_LOG_INFO,"Infinite Message Loop...\n" );	
                    srv->ApplicationMessageLoop();
                    srv->CleanUp();
                }
            }
            catch(...) {
                DebugPrintf( SV_LOG_ERROR,"StartWork: exception...\n" );
                ACE_OS::sleep(5);
                srv->CleanUp();
            }
        }
    }
    catch(...)
    {
        DebugPrintf(SV_LOG_ERROR,"FAILED: An error occurred while starting service.\n");
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return 0;
}

void AppService::ApplicationMessageLoop()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    int runRetryCount = 0;
    while( !this->quit)
    {
        DebugPrintf( SV_LOG_INFO,"Application Message Loop...\n" );
#ifdef SV_WINDOWS
        m_controllerPtr->resetUserHandle();
#endif
        startAppConfigurator();
        startScheduler();
        startDiscoveryController();
        if( m_controllerPtr->run() == SVS_FALSE)
        {
            if(runRetryCount < 5)
            {
                runRetryCount++;
                DebugPrintf(SV_LOG_ERROR,"FAILED: An error occured while starting threads. Retrying to start the threads...\n");
                ACE_OS::sleep(5);
            }
            else
            {
                DebugPrintf( SV_LOG_ERROR,"FAILED: An error occured while starting threads.\n");
                StopWork(this);
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


int AppService::init()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    int ret = SV_FAILURE;

    try 
    {
        getSupportingApplicationNames("APP", m_selectedApplicationNameSet) ;
        initController();
        initAppConfigurator();
        initConfigurator();
        initScheduler();
        initDiscoveryController();
        m_bInitialized = true;
        ret = SV_SUCCESS;
    }
    catch( std::string const& e ) 
    {
        DebugPrintf( SV_LOG_ERROR,"init: exception %s\n", e.c_str() );
        throw e;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return ret;
}

const bool AppService::isInitialized() const
{
    return m_bInitialized;
}

void AppService::CleanUp()
{

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try {
        //Clean all resoruces acquired by application here.	
        AppConfigChangeMediatorPtr configChangeMediatorPtr = AppConfigChangeMediator::GetInstance();
        configChangeMediatorPtr->getApplicationSettingsSignal().disconnect(this) ;
        getApplicationSchedSettingsSignal().disconnect(this);
        m_ThreadManager.wait() ;
        m_bInitialized = false;
    }
    catch(...) {
        DebugPrintf( SV_LOG_ERROR,"AppService: exception during cleanup...\n" );
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}
int AppService::StopWork(void *data)
{

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME ) ;
    AppService *as = (AppService *)data;

    if (as == NULL)
        return 0;

    as->quit = 1;
    quit_signalled = true;
    if (as->isInitialized())
    {
        try
        {
            AppScheduler::getInstance()->stop();
        }
        catch (std::string const& e) {
            DebugPrintf(SV_LOG_ERROR, "AppScheduler: stop exception %s\n", e.c_str());
        }
        catch (char const* e) {
            DebugPrintf(SV_LOG_ERROR, "AppScheduler: stop exception %s\n", e);
        }
        catch (...) {
            DebugPrintf(SV_LOG_ERROR, "AppScheduler: stop exception...\n");
        }

        try
        {
            Controller::getInstance()->Stop();
        }
        catch (std::string const& e) {
            DebugPrintf(SV_LOG_ERROR, "Controller: stop exception %s\n", e.c_str());
        }
        catch (char const* e) {
            DebugPrintf(SV_LOG_ERROR, "Controller: stop exception %s\n", e);
        }
        catch (...) {
            DebugPrintf(SV_LOG_ERROR, "Controller: stop exception...\n");
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "AppService is not initialized yet.\n");
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME ) ;
    return 0;
}
int AppService::handle_signal(int signum, siginfo_t *si, ucontext_t * uc) 
{
    // Do not call LOG_EVENT or any glibc functions; we are in a signal context
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME ) ;
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

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME ) ;
    return iRet;
}
int AppService::handle_exit (ACE_Process *proc)
{ 
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    pid_t pid = proc->getpid();
    
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return 0;
}

bool AppService::IsUninstallSignalled() const
{
    return m_bUninstall;
}

bool AppService::initConfigurator()
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);

    bool rv = false;
    do
    {
        try
        {
            if (!InitializeConfigurator(&TheConfigurator, USE_ONLY_CACHE_SETTINGS, PHPSerialize))
            {
                rv = false;
                break;
            }
            rv = true;
        }
        catch (ContextualException& ce)
        {
            rv = false;
            //cout << " encountered exception. " << ce.what() << endl;
            DebugPrintf(SV_LOG_ERROR, "\n encountered exception. %s \n", ce.what());
        }
        catch (exception const& e)
        {
            rv = false;
            //cout << " encountered exception. " << e.what() << endl;
            DebugPrintf(SV_LOG_ERROR, "\n encountered exception. %s \n", e.what());
        }
        catch (...)
        {
            rv = false;
            //cout << FUNCTION_NAME << " encountered unknown exception." << endl;
            //cout << "encountered unknown exception." << endl;
            DebugPrintf(SV_LOG_ERROR, "\n encountered unknown exception. \n");
        }

    } while (0);

    if (!rv)
    {
        // cout << "[WARNING]: CX server is unreachable. \nPlease verify if CX server is running..." << endl;
        DebugPrintf(SV_LOG_INFO, "[WARNING]: Configurator initialization failed. Verify if Vx agent service is running...\n");
    }

    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);

    return rv;
}

SVERROR AppService::initAppConfigurator()
{   
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SVERROR bRet = SVS_FALSE ;
    AppLocalConfigurator configurator ;
    m_configuratorPtr = AppAgentConfigurator::CreateAppAgentConfigurator(PHPSerialize) ;
    if( m_configuratorPtr.get() == NULL )
    {        
        DebugPrintf(SV_LOG_ERROR,"Failed to get app configurator\n");
    }
    else
    {
        AppConfigChangeMediatorPtr configChangeMediatorPtr = AppConfigChangeMediator::CreateInstance(thrMgr()) ;
        configChangeMediatorPtr->getApplicationSettingsSignal().connect(this, &AppService::ApplicationSettingsCallback) ;
        m_configuratorPtr->init() ;
        bRet = SVS_OK;
    }				    
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return bRet;
}    

SVERROR AppService::initScheduler()
{ 
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SVERROR bRet = SVS_FALSE ;
    AppLocalConfigurator configurator ;
    m_schedulerPtr = AppScheduler::createInstance(this->thrMgr(), configurator.getAppSchedulerCachePath()) ;
    if( m_schedulerPtr.get() == NULL )
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to get Discovery Controller\n");
    }
    else
    {        
        if( m_configuratorPtr.get() != NULL )
        {
            std::map<SV_ULONG, TaskInformation> tasksFromConfigCache ;
            m_configuratorPtr->getTasksFromConfiguratorCache( tasksFromConfigCache ) ;
            m_schedulerPtr->init() ;
            SV_UINT defaultDiscoveryInterval = configurator.getApplicationDiscoveryInterval() ;
            TaskInformation task(defaultDiscoveryInterval) ;
            tasksFromConfigCache.insert(std::make_pair( 0, task)) ;
            m_schedulerPtr->loadTasks( tasksFromConfigCache ) ;
            TaskExecution::getInstance()->getTaskTriggerSignal().connect(Controller::getInstance().get(), &Controller::PostMsg) ;
            m_applicationSchedSettingsSignal.connect(AppScheduler::getInstance().get(), &AppScheduler::ApplicationScheduleCallback) ;
            bRet = SVS_OK;
        }
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return bRet;
}   

SVERROR AppService::initController()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SVERROR bRet = SVS_FALSE;
    m_controllerPtr = Controller::createInstance(thrMgr());
    if(m_controllerPtr.get() != NULL)
    {
        bRet = m_controllerPtr->init(m_selectedApplicationNameSet,false);
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return bRet;
}
SVERROR AppService::startAppConfigurator()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SVERROR bRet = SVS_OK ;
    ConfigChangeMediatorPtr configChangeMediatorPtr = AppConfigChangeMediator::GetInstance() ;
    m_configuratorPtr->start(configChangeMediatorPtr) ;
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return bRet;
}				    

SVERROR AppService::startScheduler()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SVERROR bRet = SVS_OK ;
    m_schedulerPtr->start() ;
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return bRet;
}    

ACE_Thread_Manager* AppService::thrMgr()
{
    return &m_ThreadManager ;
}    

SVERROR AppService::initDiscoveryController()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SVERROR bRet = SVS_OK ;
    if(!m_selectedApplicationNameSet.empty())
    {
        if(DiscoveryController::createInstance())
        {
            m_discoveryControllerPtr = DiscoveryController::getInstance() ;
            if(m_discoveryControllerPtr.get() != NULL)
            {
                m_discoveryControllerPtr->init(m_selectedApplicationNameSet) ;
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG, "m_discoveryControllerPtr is NULL\n");
            }
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "DiscoveryController is not created. ");
        }
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "m_selectedApplicationNameSet is empty. so not starting the DiscoveryController thread.\n");
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return bRet;
}

SVERROR AppService::startDiscoveryController()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    SVERROR bRet = SVS_OK ;
    if(m_discoveryControllerPtr.get() != NULL)
    {
        m_discoveryControllerPtr->start() ;
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "Not starting the Discovery controler as m_selectedApplicationNameSet is empty\n");
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return bRet;
}

void AppService::ApplicationSettingsCallback(ApplicationSettings settings)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::map<SV_ULONG, TaskInformation> tasksMap ;
    std::list<ApplicationPolicy> policies = settings.m_policies ;
    std::list<ApplicationPolicy>::const_iterator policyIter = policies.begin() ;
    for (/*empty*/; policyIter != policies.end(); policyIter++)
    {
        bool disabled = true ;
        SV_ULONG configId ;
        const std::map<std::string, std::string>& policyProps = policyIter->m_policyProperties ;
        try
        {
            configId = boost::lexical_cast<SV_ULONG>(policyProps.find("PolicyId")->second ) ;
        }
        catch(boost::bad_lexical_cast& blc)
        {
            DebugPrintf(SV_LOG_WARNING, "Bad lexical cast exception while finding policy Id from map : %s\n", blc.what());
            policyIter++;
        }
        SV_ULONG frequency = 0 ;
        SV_ULONG excludeFrom = 0 ;
        SV_ULONG excludeTo = 0 ;
        SV_ULONG firstTriggerTimeUL = 0;
        int scheduleType = 0 ;
        try
        {
            if(policyProps.find("ScheduleInterval") != policyProps.end() )
            {
                frequency = boost::lexical_cast<SV_ULONG>(policyProps.find("ScheduleInterval")->second) ;
            }
            if(policyProps.find("StartTime") != policyProps.end())
            {
                firstTriggerTimeUL = boost::lexical_cast<SV_ULONG>(policyProps.find("StartTime")->second) ;
            }
            if(policyProps.find("ExcludeFrom") != policyProps.end() )
            {
                excludeFrom = boost::lexical_cast<SV_ULONG>(policyProps.find("ExcludeFrom")->second) ;
            }
            if(policyProps.find("ExcludeTo") != policyProps.end() )
            {
                excludeTo = boost::lexical_cast<SV_ULONG>(policyProps.find("ExcludeTo")->second) ;
            }
            if( policyProps.find("ScheduleType") != policyProps.end() )
            {
                scheduleType = boost::lexical_cast<int>(policyProps.find("ScheduleType")->second ) ;
                if( scheduleType == 0 )
                {
                    disabled = true ;
                }   
                else
                {
                    disabled = false ;
                }
            }
        }
        catch(boost::bad_lexical_cast & blc)
        {
            std::stringstream sserr;
            sserr << "Bad lexical cast exception : " << blc.what();
            DebugPrintf(SV_LOG_ERROR, "%s: %s.\n", FUNCTION_NAME, sserr.str().c_str());
            throw INMAGE_EX(sserr.str().c_str());
        }
        TaskInformation task(frequency) ;
        task.m_StartTime = firstTriggerTimeUL;
        task.m_firstTriggerTime = firstTriggerTimeUL;
        task. m_ScheduleExcludeFromTime = excludeFrom;
        task.m_ScheduleExcludeToTime = excludeTo ;
        task.m_NextExecutionTime = firstTriggerTimeUL;
        if( disabled )
        {
            task.m_bIsActive = false;
            if( policyProps.find("CommandUpdateSent") != policyProps.end() )
            {
                if(policyProps.find("CommandUpdateSent")->second.compare("1") == 0)
                {
                    DebugPrintf(SV_LOG_DEBUG,"CommandUpdate for the policy %ld already sent,skipping policy\n",configId);
                    continue;
                }
            }
        }
        // Disable scheduling policy for single vm consistency and Multi vm consistency new flow
        // Disable only for ApplicationType CLOUD and PolicyType Consistency
        std::map<std::string, std::string>::const_iterator citrApplicationType = policyProps.find("ApplicationType");
        std::map<std::string, std::string>::const_iterator citrPolicyType = policyProps.find("PolicyType");
        if ((citrApplicationType != policyProps.end()) &&
            (citrPolicyType != policyProps.end()) &&
            (0 == citrApplicationType->second.compare("CLOUD")) &&
            (0 == citrPolicyType->second.compare("Consistency")))
        {
            std::map<std::string, std::string>::const_iterator citrMvCPScheduleInterval =
                policyProps.find("MultiVMConsistencyPolicyScheduleInterval");
            if ((citrMvCPScheduleInterval != policyProps.end()) // Multi vm consistency new flow
                || (0 != frequency)) // Single vm consistency case
            {
                if (citrMvCPScheduleInterval != policyProps.end())
                {
                    DebugPrintf(SV_LOG_DEBUG, "%s: Disabling scheduling for policy %lu - multi vm new flow\n", FUNCTION_NAME, configId);
                }
                else if (0 != frequency)
                {
                    DebugPrintf(SV_LOG_DEBUG, "%s: Disabling scheduling for policy %lu - single vm\n", FUNCTION_NAME, configId);
                }
                task.m_bIsActive = false;
                task.m_bRemoveFromExisting = true; // Upgrade case remove from existing task map of schedular
            }
        }
        tasksMap.insert(std::make_pair(configId, task)) ;

    } // for loop

    m_applicationSchedSettingsSignal(tasksMap) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}    


sigslot::signal1<std::map<SV_ULONG, TaskInformation> >& AppService::getApplicationSchedSettingsSignal()
{
    return m_applicationSchedSettingsSignal ;
}

SVERROR AppService::CompleteRecovery()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVERROR bRet = SVS_OK;

    try
    {
        bool bHydrationWorkflow = false;
        bool bClone = false;

        if (!HostRecoveryManager::IsRecoveryInProgress(bHydrationWorkflow, bClone, std::bind1st(std::mem_fun(&AppService::QuitRequested), this)))
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
                    DebugPrintf(SV_LOG_ALWAYS, "Azure Site Recovery: Shutting down system to recover dynamic disks\n");

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
            if (!HostRecoveryManager::IsRecoveryInProgress(std::bind1st(std::mem_fun(&AppService::QuitRequested), this)))
            {
                DebugPrintf(SV_LOG_DEBUG, "Recovery completed.\n");
            }
            else
            {
                DebugPrintf(SV_LOG_WARNING, "Recovery might not be successful.\n");
                bRet = SVS_FALSE;
            }
        }
    }
    catch (const HostRecoveryException& exp)
    {
        DebugPrintf(SV_LOG_ERROR, "Exception completing recovery. %s\n", exp.what());
        bRet = SVS_FALSE;
    }
    catch (const std::exception& exp)
    {
        DebugPrintf(SV_LOG_ERROR, "Exception completing recovery. %s\n", exp.what());
        bRet = SVS_FALSE;
    }
    catch (const std::string& expMsg)
    {
        DebugPrintf(SV_LOG_ERROR, "Exception completing recovery. %s\n", expMsg.c_str());
        bRet = SVS_FALSE;
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "Exception completing recovery.\n");
        bRet = SVS_FALSE;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bRet;
}

bool AppService::QuitRequested(int waitTimeSeconds)
{
    if(quit)
        return true;

    if(waitTimeSeconds)
    {
        ACE_OS::sleep(waitTimeSeconds);
    }

    return quit;
}

#ifdef SV_WINDOWS
void AppService::RunRecoveryCommand()
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
    recoveryCmdOptions.command_line("%s",strRecvryCmd.c_str());
    recoveryCmdOptions.set_handles(ACE_INVALID_HANDLE, hLogFile, hLogFile);
    if (recoveryCmdOptions.startup_info())
        recoveryCmdOptions.startup_info()->wShowWindow = SW_HIDE;

    ACE_Process recoveryProc;
    pid_t recoveryCmdPid = recoveryProc.spawn(recoveryCmdOptions);
    if (-1 == recoveryCmdPid)
        throw std::string("Could not start the recovery command.");

    do 
    {
        DebugPrintf(SV_LOG_DEBUG, "Process is still running.\n");

        if (quit)
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
        errStream << "Command execution failed. Exit code: "
            << recoveryProc.exit_code();

        throw errStream.str();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

#else // Non-Windows
void AppService::RunRecoveryCommand()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    //To do

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}
#endif
