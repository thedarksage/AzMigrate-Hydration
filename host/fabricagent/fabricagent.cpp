#include <exception>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <ace/OS_NS_errno.h>
#include <ace/os_include/os_limits.h>

#include "ace/OS_NS_unistd.h"
#include "ace/OS_main.h"
#include "ace/Service_Config.h"
#include "ace/Thread_Manager.h"
#include "ace/Process_Manager.h"
#include "ace/Get_Opt.h"

#include "configurator2.h"
#include "logger.h"
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include "fabricagent.h"
#include "compatibility.h"
#include "portable.h" 
#include "configurecxagent.h"
#include "devicehelper.h"
#include "executecommand.h"

#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <stdint.h> 
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "devicefilter.h"
#include "inmsafecapis.h"

int const SETTINGS_POLLING_INTERVAL = 15;
time_t const GET_FABRIC_SETTINGS_INTERVAL_IN_SECONDS = 15 * 2;
time_t const REPORT_INITIATOR_INTERVAL_IN_SECONDS = 60 * 15;



ACE_Time_Value LastFabricSettingsTime_g;
ACE_Time_Value LastReportInitiatorsTime_g;
// TODO:
// needed until cx has a way to request hosts to re-register
// for now call RegisterHost once every 15 minutes. 
time_t const REGISTER_HOST_INTERVAL_IN_SECONDS = 60 * 3; 

ACE_Time_Value LastRegisterHostTime_g;

bool quit_signalled = false;

bool fabricagent::init()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool ret = false;
    try {
        DebugPrintf(SV_LOG_DEBUG,"Instantiating fabric agent configurator.\n");

        if(m_bRemoteCxReachable)
        {
            DebugPrintf(SV_LOG_WARNING, "%s :: RemoteCx is reachable. re instantiating the configurator.\n", FUNCTION_NAME);
            delete m_configurator;
            m_configurator = NULL;
            m_bRemoteCxReachable = false; //Reset the flag.
        }
        //Initializing the configurator thread.
        if(!InitializeConfigurator((Configurator**)&m_configurator, USE_CACHE_SETTINGS_IF_CX_NOT_AVAILABLE, PHPSerialize))
        {
            return false;
        }

        if ( NULL != m_configurator )
        {
            ret = true;
            DebugPrintf(SV_LOG_DEBUG, "Starting fabricagent configurator thread\n");
            m_configurator->Start();
            m_bInitialized = true;
        }
        DebugPrintf( SV_LOG_DEBUG,"fabricagent started\n" );

    }
    catch ( ContextualException& ce )
    {
        DebugPrintf(SV_LOG_ERROR, "\n%s encountered exception %s\n", ce.what());
        throw ce;
    }
    catch( std::string const& e ) {
        DebugPrintf( SV_LOG_ERROR,"init: exception %s\n", e.c_str() );
        throw e;
    }
    catch( char const* e ) {
        DebugPrintf( SV_LOG_ERROR,"init: exception %s\n", e );
        throw e;
    }
    catch( exception const& e ) {
        DebugPrintf( SV_LOG_ERROR,"init: exception %s\n", e.what() );
        throw e;
    }
    catch(...) {
        DebugPrintf( SV_LOG_ERROR,"init: exception...\n" );
        throw;
    }


    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return ret;
}

const bool fabricagent::isInitialized() const
{
    return m_bInitialized;
}

int
ACE_TMAIN (int argc, ACE_TCHAR* argv[])
{

    //calling init funtion of inmsafec library
	init_inm_safe_c_apis();

	int ret = 0;
    MaskRequiredSignals();
    try {
        //
        // Initialize logging options as soon as the service starts
        //

        LocalConfigurator localConfigurator;
        std::string logPathname = localConfigurator.getLogPathname();
        SetDaemonMode();
        logPathname += "fabricagent.log";
        SetLogFileName( logPathname.c_str() );
        SetLogLevel( localConfigurator.getLogLevel() );
        SetLogHttpIpAddress( localConfigurator.getHttp().ipAddress );
        SetLogHttpPort( localConfigurator.getHttp().port );
        SetLogHostId( localConfigurator.getHostId().c_str() );
        SetLogRemoteLogLevel( localConfigurator.getRemoteLogLevel() );
		SetLogHttpsOption(localConfigurator.IsHttps());

        if ((argc == 2) && (strcmp("uninstall", argv[1]) == 0))
        {
            //cout << "hello world\n";
            fabricagent srv;       
            srv.CleanupATLuns();
            srv.CleanupScst();
            return 0;
        }

        if (IsProcessRunning(ACE::basename (argv[0], ACE_DIRECTORY_SEPARATOR_CHAR)))
        {
            const int RETRIES = 10;
            int iTries = 0;
            for(; iTries < RETRIES;iTries++)
            {
                if ( !IsProcessRunning(ACE::basename (argv[0], ACE_DIRECTORY_SEPARATOR_CHAR)))
                    break;
                else
                    ACE_OS::sleep( 1 );
            }
            if ( RETRIES == iTries )
            {
                printf("fabricagent already running.Exiting...\n");
                return 0;
            }
        }

        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

        fabricagent srv;       

        if (srv.parse_args(argc, argv) != 0)
        {
            srv.display_usage();
            ACE_OS::exit(-1);
        }
        std::string agentMode = localConfigurator.getAgentMode();
        if ("host" == agentMode)
            ACE_OS::exit(-1);

        srv.name (ACE_TEXT (SERVICE_NAME), ACE_TEXT (SERVICE_TITLE), ACE_TEXT(SERVICE_DESC));
        DebugPrintf( SV_LOG_DEBUG,"main(): starting as %s\n", SERVICE_NAME );

        if (srv.opt_install)
            return (srv.install_service());
        if (srv.opt_start)
            return (srv.start_service());


        if (srv.opt_kill)
            return (srv.stop_service());
        if (srv.opt_remove)
            return (srv.remove_service());

        try {
            if ((ret = srv.run_startup_code()) == -1)
            {
                // Start actual work
                DebugPrintf( SV_LOG_DEBUG,"main(): run_startup_code return -1\n" );

                ret = fabricagent::StartWork(&srv);
            }
        }
        catch( std::exception& e ) {
            ret = -1;
            DebugPrintf( SV_LOG_FATAL, "Exception thrown: %s\n", e.what() );
        }
        catch( const char* e ) {
            ret = -1;
            DebugPrintf( SV_LOG_FATAL, "Exception thrown: %s\n", e );

        }
        catch(...) {
            ret = -1;
            DebugPrintf( SV_LOG_FATAL, "Exception thrown\n" );
        }
    }
    catch(...) {
        DebugPrintf( SV_LOG_FATAL, "Exception occurred during startup. Failed to start fabricagent service. Exiting...\n" );
        return ret;
    }      
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    CloseDebug();
    return(ret);
}

fabricagent::fabricagent()
    : m_bInitialized(false),
    quit(0),
    m_configurator(NULL),
    service_started(false),
    m_bRemoteCxReachable(false)
{
    // Commented for the timebeing for testing other things..
    //VacpStart();
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

}

int fabricagent:: RegisterHost()
{
    return 0;
}

void fabricagent::CleanUp()
{

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try {
        if ( NULL != m_configurator)
        {
            //destructor will stop the configurator thread.
            delete m_configurator;
        }
        m_configurator = NULL;
        m_bInitialized = false;

    }
    catch(...) {
        DebugPrintf( SV_LOG_ERROR,"fabricagent: exception during cleanup...\n" );
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED fabricagent %s\n",FUNCTION_NAME);
}

fabricagent::~fabricagent()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    CleanUp();

    this->sig_handler_.remove_handler(SIGTERM);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

}

void fabricagent::display_usage(char *exename)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    const char *progname = ((exename == NULL) ? "<this_exe_name>" : exename);
    DebugPrintf(SV_LOG_INFO, "Usage: %s -i -r -s -k\n"
        "  -i: Install this program as NT-service/Unix-Daemon process\n"
        "  -r: Remove this program from the Service\n"
        "  -s: Start the service/daemon\n"
        "  -k: Kill the service/daemon\n", progname ); 

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

int fabricagent::parse_args(int argc, ACE_TCHAR* argv[])
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    int iRet = 0;
    for (int index = 1; index < argc; index++)
    {  
        if ((ACE_OS::strcasecmp(ACE_TEXT("/RegServer"), argv[index]) == 0) ||
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

        else if ((ACE_OS::strcasecmp(ACE_TEXT("/UnregServer"), argv[index]) == 0) ||
            (ACE_OS::strcasecmp(ACE_TEXT("-UnregServer"), argv[index]) == 0) ||
            (ACE_OS::strcasecmp(ACE_TEXT("/Remove"), argv[index]) == 0) ||
            (ACE_OS::strcasecmp(ACE_TEXT("-Remove"), argv[index]) == 0) ||
            (ACE_OS::strcasecmp(ACE_TEXT("/R"), argv[index]) == 0) ||
            (ACE_OS::strcasecmp(ACE_TEXT("-R"), argv[index]) == 0) )
        {
            opt_remove = 1;
            iRet = 0;
        }

        else if ((ACE_OS::strcasecmp(ACE_TEXT("/Start"), argv[index]) == 0) ||
            (ACE_OS::strcasecmp(ACE_TEXT("-Start"), argv[index]) == 0) ||
            (ACE_OS::strcasecmp(ACE_TEXT("/S"), argv[index]) == 0) ||
            (ACE_OS::strcasecmp(ACE_TEXT("-S"), argv[index]) == 0) )
        {
            opt_start = 1;
            iRet = 0;
        }

        else if ((ACE_OS::strcasecmp(ACE_TEXT("/Kill"), argv[index]) == 0) ||
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

int fabricagent::install_service()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    int ret = -1;

    do
    {
        if (isInstalled() == true)
        {
            ret = 0;
            break;
        }

        ret = start_daemon(); 

    } while (false);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return ret;
}

int fabricagent::remove_service()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    int ret = -1;

    do
    {
        if (isInstalled() == false)
        {
            ret = 0;
            break;
        }

        ret = CService::remove_service();  

    } while (false);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return ret;
}


int fabricagent::start_daemon()
{
    ACE::daemonize( ACE_TEXT ("/"), 0 /* don't close handles */ );
    service_started = true;
    return 0;
}

int fabricagent::start_service()
{
    return start_daemon();
}

int fabricagent::run_startup()
{
    int status = -1;

    DebugPrintf( "fabricagent::run_startup\n" );

    (void)start_daemon();
    DebugPrintf( "fabricagent::run_startup\n" );
    //No startup code to run, return always -1.
    if (startfn != NULL)
    {
        status = (*startfn)(startfn_data);
    }
    DebugPrintf( "fabricagent::run_startup\n" );
    return status;
}

bool fabricagent::isInstalled()
{
    return service_started;
}

int fabricagent::RegisterStartCallBackHandler(int (*fn)(void *), void *fn_data)
{
    startfn = fn;
    startfn_data = fn_data;
    return 0;
}

int fabricagent::RegisterStopCallBackHandler(int (*fn)(void *), void *fn_data)
{
    stopfn = fn;
    stopfn_data = fn_data;
    return 0;
}


int fabricagent::run_startup_code()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    try {
        RegisterStartCallBackHandler(fabricagent::StartWork, this);
        RegisterStopCallBackHandler(fabricagent::StopWork, this);
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

    return (run_startup());
}


int fabricagent::ServiceEventHandler(signed long param,void *data)
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

int fabricagent::StartWork(void *data)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    fabricagent *srv = NULL;
    try {

        srv = (fabricagent *)data;

        if (srv == NULL)
            return 0;
        //Register signal handler for SIGTERM 
        srv->sig_handler_.register_handler(SIGTERM, srv);   

    }

    catch(...)
    {
        DebugPrintf(SV_LOG_ERROR,"FAILED: An error occurred while starting service.\n");
    }


    while( !srv->quit)
    {
        try 
        {
            if ( true == srv->init() )
            {
                if( ! srv->IsAtLunPresent("DUMMY_LUN_ZERO") )
                {
                    DebugPrintf(SV_LOG_DEBUG, "DUMMY LUN ZERO lun doesn't exists, Creating...\n");

                    srv->DeleteBitMapFilesOnReboot();

                    if (!srv->ProcessAtOperationsOnReboot())
                    {
                        //On successful only create dummy lun0, continue on failures.
                        DebugPrintf(SV_LOG_DEBUG, "Got Error while in ProcessAtOperationsOnReboot\n");
                        ACE_OS::sleep(SETTINGS_POLLING_INTERVAL);
                        continue;
                    }

                    if (!srv->CheckScstGroup(DEFAULT_KEYWORD))
                    {
                        // if the Default group is not available, there is an issue with scst, don't proceed
                        DebugPrintf(SV_LOG_ERROR, "Failed!!! Default group doesn't exist.\n");
                        ACE_OS::sleep(SETTINGS_POLLING_INTERVAL);
                        continue;

                    }

                    if (0 != srv->CreateATLun(DUMMY_LUN_NAME, 512, 512, 0))
                    {
                        DebugPrintf(SV_LOG_ERROR, "Failed to create DUMMY LUN ZERO lun\n");
                        ACE_OS::sleep(SETTINGS_POLLING_INTERVAL);
                        continue;
                    }

                    if (!srv->AddLunToScstGroup(DUMMY_LUN_NAME, 0, DEFAULT_KEYWORD))
                    {
                        // if dummy lun is not added to the Default group, don't proceed
                        DebugPrintf(SV_LOG_ERROR, "Failed to add DUMMY LUN ZERO lun to Default group\n");
                        ACE_OS::sleep(SETTINGS_POLLING_INTERVAL);
                        continue;
                    }
                    DebugPrintf(SV_LOG_DEBUG, "DUMMY LUN ZERO lun created successfully\n");
                }
                else
                {

                    if (!srv->CheckScstGroup(DEFAULT_KEYWORD))
                    {
                        // if the Default group is not available, there is an issue with scst, don't proceed
                        DebugPrintf(SV_LOG_ERROR, "Failed!!! Default group doesn't exist.\n");
                        ACE_OS::sleep(SETTINGS_POLLING_INTERVAL);
                        continue;
                    }

                    unsigned long long oldLunNo=0; 
                    if(!srv->CheckLunInScstGroup(DUMMY_LUN_NAME, 0, DEFAULT_KEYWORD, oldLunNo))
                    {
                        DebugPrintf(SV_LOG_ERROR, "Failed to find DUMMY LUN ZERO lun in Default group\n");

                        if (!srv->AddLunToScstGroup(DUMMY_LUN_NAME, 0, DEFAULT_KEYWORD))
                        {
                            DebugPrintf(SV_LOG_ERROR, "Failed to add DUMMY LUN ZERO lun to Default group\n");
                            ACE_OS::sleep(SETTINGS_POLLING_INTERVAL);
                            continue;
                        }
                    }
                    else
                    {
                        if (oldLunNo != 0)
                        {
                            if (!srv->RemoveLunFromScstGroup(DUMMY_LUN_NAME, oldLunNo, DEFAULT_KEYWORD))
                            {
                                DebugPrintf(SV_LOG_ERROR, "Failed to remove old lun\n");
                                ACE_OS::sleep(SETTINGS_POLLING_INTERVAL);
                                continue;

                            }

                            if (!srv->AddLunToScstGroup(DUMMY_LUN_NAME, 0, DEFAULT_KEYWORD))
                            {
                                DebugPrintf(SV_LOG_ERROR, "Failed to add DUMMY LUN ZERO lun to Default group\n");
                                ACE_OS::sleep(SETTINGS_POLLING_INTERVAL);
                                continue;
                            }
                        }
                    }

                    DebugPrintf(SV_LOG_DEBUG, "DUMMY LUN ZERO lun already exists\n");
                }

                LastRegisterHostTime_g = ACE_OS::gettimeofday();

                // To deal with the new port configurations on reboot of InMage Appliance

                srv->ProcessMessageLoop();
            }
            else
            {
                DebugPrintf( SV_LOG_ERROR,"StartWork: Init returned false.\n") ;
                ACE_OS::sleep(60);
            }
        }
        catch( std::string const& e ) {
            DebugPrintf( SV_LOG_ERROR,"StartWork: exception %s\n", e.c_str() );
            ACE_OS::sleep(5);
            srv->CleanUp();
        }
        catch( char const* e ) {
            DebugPrintf( SV_LOG_ERROR,"StartWork: exception %s\n", e );
            ACE_OS::sleep(5);
            srv->CleanUp();
        }
        catch( exception const& e ) {
            DebugPrintf( SV_LOG_ERROR,"StartWork: exception %s\n", e.what() );
            ACE_OS::sleep(5);
            srv->CleanUp();
        }
        catch(...) {
            DebugPrintf( SV_LOG_ERROR,"StartWork: exception...\n" );
            ACE_OS::sleep(5);
            srv->CleanUp();
        }
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return 0;
}

int fabricagent::StopWork(void *data)
{

    fabricagent *vs = (fabricagent *)data;

    if (vs == NULL)
        return 0;

    //__asm int 3;
    vs->quit = 1;
    quit_signalled = true;

    return 0;
}

void fabricagent::DeleteBitMapFilesOnReboot()
{

    string cmd = string("/bin/rm -rf /root/*.VolumeLog /etc/vxagent/involflt/inmage0000*/VolumeFilteringDisabled");
    cmd += " 2> /dev/null"; 
    std::stringstream results;
    if (!executePipe(cmd, results))
    {
        DebugPrintf(SV_LOG_ERROR,"fabricagent::DeleteBitMapFilesOnReboot executePipe failed, cmd: %s\n", cmd.c_str());
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG,"fabricagent::DeleteBitMapFilesOnReboot executePipe success, cmd: %s\n", cmd.c_str());
    }
}

void fabricagent::InvokeAtOperations(list<ATLunOperations> & atLunOpList)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG,"ATLUN Setup/Remove Requests Info:\n");
    int increment = 1;
    for (list<ATLunOperations>::iterator linfoiter = atLunOpList.begin(); 
        linfoiter!=atLunOpList.end(); linfoiter++, increment++)
    {
        unsigned long long lunno = linfoiter->applianceTargetLunInfo.front().lunno;
        DebugPrintf(SV_LOG_DEBUG, "LUN Setup/Remove Request No: %d\n", increment);
        DebugPrintf(SV_LOG_DEBUG, "LUN No: %ld\n", lunno);
        /* ATLunOperations.size is total number of blocks */
        DebugPrintf(SV_LOG_DEBUG, "LUN No. of Blocks: %ld\n", linfoiter->size);
        DebugPrintf(SV_LOG_DEBUG, "LUN Block Size: %d\n", linfoiter->blockSize);
        DebugPrintf(SV_LOG_DEBUG, "Appliance LUN ID: %s\n", linfoiter->applianceLunId.c_str());

        AcgInfo acgInfo;
        acgInfo.lunno = lunno;
        acgInfo.groupName = AT_GROUP_NAME;
        acgInfo.sanInitiator = VI_NODE_WWN;
        AcgInfoList acgList;
        acgList.push_back(acgInfo);

        if (linfoiter->state == CREATE_AT_LUN_PENDING)
        {
            /* startingPhysicalOffset is always zero for fabric */
            if (0 == ExportATLun(linfoiter->applianceLunId, linfoiter->size, 0,
                linfoiter->blockSize, acgList, DISABLE_APPLIANCE_PASSTHROUGH, NULL))
            {
                DebugPrintf(SV_LOG_DEBUG, "AT LUN Setup for LUN No: %ld, SUCCESS\n", lunno);
                linfoiter->resultState = CREATE_AT_LUN_DONE;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "AT LUN Setup for LUN No: %ld, FAILED\n", lunno);
                linfoiter->resultState = FA_ERROR_STATE(CREATE_AT_LUN_FAILURE);
            }
        }
        else if (linfoiter->state == DELETE_AT_LUN_PENDING )
        {
            DebugPrintf(SV_LOG_DEBUG, "AT LUN remove for request with LUN No: %ld\n", lunno);
            if (0 == RemoveATLun (linfoiter->applianceLunId, linfoiter->size,
                linfoiter->blockSize, acgList))
            {
                DebugPrintf(SV_LOG_DEBUG, "AT LUN remove for LUN No: %ld, SUCCESS\n", lunno);
                linfoiter->resultState = DELETE_AT_LUN_DONE;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "AT LUN remove for LUN No: %ld, FAILED\n", lunno);
                linfoiter->resultState = FA_ERROR_STATE(DELETE_AT_LUN_FAILURE);
            }
        }
        else if ( FAILOVER_DRIVER_DATA_CLEANUP_PENDING == linfoiter->state )
        {
            DebugPrintf(SV_LOG_DEBUG, "AT LUN request for driver data cleanup(clear diffs): %s\n", linfoiter->applianceLunId.c_str());
            if( ! IsAtLunPresent(linfoiter->applianceLunId) )
            {
                DebugPrintf(SV_LOG_ERROR, "AT LUN : %s, Doesn't exist, cannot perform Cleardiffs\n", linfoiter->applianceLunId.c_str());
                linfoiter->resultState = FAILOVER_DRIVER_DATA_CLEANUP_DONE;
            }
            else
            {
                if (ClearDiffsOnATLun(linfoiter->applianceLunId))
                {
                    DebugPrintf(SV_LOG_DEBUG, "fabricagent::InvokeAtOperations clear diffs on AT Lun[%s] SUCCESS\n",
                        linfoiter->applianceLunId.c_str());
                    linfoiter->resultState = FAILOVER_DRIVER_DATA_CLEANUP_DONE;
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR,"Failed!!!, fabricagent::InvokeAtOperations clear diffs on AT Lun[%s]\n",
                        linfoiter->applianceLunId.c_str());
                    linfoiter->resultState = FA_ERROR_STATE(FAILOVER_DRIVER_DATA_CLEANUP_FAILURE);
                }
            }
        }

        while ( true)
        {
            try 
            {
                m_configurator->getVxAgent().updateApplianceLunState(*linfoiter);
                break;
            }
            catch (std::exception const & ie) {
                DebugPrintf(SV_LOG_ERROR, "fabricagent::InvokeAtOperations %s", ie.what());
            }
            catch( std::string const& e ) {
                DebugPrintf( SV_LOG_ERROR,"fabricagent::InvokeAtOperations caught exception: %s\n", e.c_str() );
            }
            catch( char const* e ) {
                DebugPrintf( SV_LOG_ERROR,"fabricagent::InvokeAtOperations caught exception: %s\n", e );
            }
            catch(...) 
            {
                DebugPrintf( SV_LOG_ERROR, "fabricagent::InvokeAtOperations caught exception while updateApplianceLunState\n" );        
            }
            ACE_OS::sleep(5);
        }

    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

void fabricagent::InvokeDeviceLunOperations(list<DeviceLunOperations> & deviceLunOpList, std::list<AccessCtrlGroupInfo> & groupInfoList)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG,"Device LUN Setup/Remove Requests Info:\n");

    int increment = 1;
    for (list<DeviceLunOperations>::iterator linfoiter = deviceLunOpList.begin(); 
        linfoiter!=deviceLunOpList.end(); linfoiter++, increment++)
    {
        unsigned long long lunno = linfoiter->deviceLunInfo.front().lunno;
        string deviceID = linfoiter->deviceLunInfo.front().deviceID;


        if (linfoiter->state == CREATE_DEVICE_LUN_PENDING)
        {
            DebugPrintf(SV_LOG_DEBUG, "Device LUN Setup Request No: %d\n", increment);
            DebugPrintf(SV_LOG_DEBUG, "Device Name: %s\n", linfoiter->deviceName.c_str());
            DebugPrintf(SV_LOG_DEBUG, "Device LUN No: %ld\n", lunno);
            DebugPrintf(SV_LOG_DEBUG, "Device LUN No. of Blocks: %ld\n", linfoiter->size);
            DebugPrintf(SV_LOG_DEBUG, "Device LUN ID: %s\n", deviceID.c_str());

            int resultState = FA_ERROR_STATE(DEVICE_LUN_INVALID_STATE);
            DebugPrintf(SV_LOG_DEBUG, "Device LUN setup for request with LUN No: %ld\n", lunno);
            if (0 == ExportDeviceLun (linfoiter->deviceName, lunno, linfoiter->size, deviceID, resultState, linfoiter->flag))
            {
                DebugPrintf(SV_LOG_DEBUG, "Device LUN Setup for LUN No: %ld, SUCCESS\n", lunno);
                linfoiter->resultState = CREATE_DEVICE_LUN_DONE;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "Device LUN Setup for LUN No: %ld, FAILED\n", lunno);
                linfoiter->resultState = resultState;
            }
            while (true)
            {
                try 
                {
                    if (m_configurator->getVxAgent().updateDeviceLunState(*linfoiter))
                    {
                        DebugPrintf(SV_LOG_DEBUG, "Update Device LUN state to CX: SUCCESS\n");
                        break;
                    }
                    else
                        DebugPrintf(SV_LOG_ERROR, "Update Device LUN state to CX: FAILED\n");
                }
                catch (...)
                {
                    DebugPrintf(SV_LOG_ERROR, "FAILED!!! Exception while update Device LUN state to CX\n");
                }
                ACE_OS::sleep(5);
            }
        }
    }

        increment = 1;
        // ADD LUNs to each initiator group
        string groupName;
        for (list<AccessCtrlGroupInfo>::iterator ginfoiter = groupInfoList.begin(); 
            ginfoiter!= groupInfoList.end(); ginfoiter++, increment++)
        {

            if ((ginfoiter->type == VSNAP_LUN_TYPE) && (ginfoiter->operation == ADD_GROUP))
            {
                ginfoiter->state = CREATE_ACG_DONE;
                DebugPrintf(SV_LOG_DEBUG, "LUN group add Request No: %d\n", increment);
                DebugPrintf(SV_LOG_DEBUG, "LUN No: %ld\n", ginfoiter->LunNo);
                DebugPrintf(SV_LOG_DEBUG, "Appliance LUN ID: %s\n", ginfoiter->applianceLundId.c_str());
                for (std::list<std::string>::iterator itr = ginfoiter->initiatorList.begin();
                    itr != ginfoiter->initiatorList.end(); itr++)
                {
                    //groupName = string(GROUP_NAME_PREFIX) + *itr;
                    groupName = *itr;
                    if (!CheckScstGroup(groupName))
                    {
                        if (!CreateScstGroup(groupName))
                        {
                            ginfoiter->state = FA_ERROR_STATE(CREATE_ACG_FAILUE);
                        }
                    }

                    if (ginfoiter->state !=  FA_ERROR_STATE(CREATE_ACG_FAILUE))
                    {
                        unsigned long long oldLunNo=0; 
                        if (!CheckLunInScstGroup(DUMMY_LUN_NAME, 0, groupName, oldLunNo))
                        {
                            if (!AddLunToScstGroup(DUMMY_LUN_NAME, 0, groupName))
                            {
                                ginfoiter->state = FA_ERROR_STATE(CREATE_ACG_FAILUE);
                            }
                        }
                    }

                    if (ginfoiter->state !=  FA_ERROR_STATE(CREATE_ACG_FAILUE))
                    {
                        if (!CheckWwnInScstGroup(groupName, groupName))
                        {
                            if (!AddWwnToScstGroup(groupName, groupName))
                            {
                                DebugPrintf(SV_LOG_ERROR, "Failed to add wwn[%s] to group[%s]\n", groupName.c_str(), groupName.c_str());
                                ginfoiter->state = FA_ERROR_STATE(CREATE_ACG_FAILUE);
                            }
                        }
                    }

                    if (ginfoiter->state !=  FA_ERROR_STATE(CREATE_ACG_FAILUE))
                    {
                        if (!AddLunToScstGroup(ginfoiter->applianceLundId, ginfoiter->LunNo, groupName))
                            ginfoiter->state = FA_ERROR_STATE(CREATE_ACG_FAILUE);
                    }
                }
                try {
                    if (m_configurator->getVxAgent().updateGroupInfoListState(*ginfoiter))

                        DebugPrintf(SV_LOG_DEBUG, "Update Group Info List state to CX: SUCCESS\n");
                    else
                        DebugPrintf(SV_LOG_ERROR, "Update Group Info List state to CX: FAILED\n");
                }
                catch (...)
                {
                    DebugPrintf(SV_LOG_ERROR, "FAILED!!! Exception while update Group Info List state to CX\n");
                }
            }
        }

        increment = 1;
        // DELETE LUNs from each initiator group
        for (list<AccessCtrlGroupInfo>::iterator ginfoiter = groupInfoList.begin(); 
            ginfoiter!= groupInfoList.end(); ginfoiter++, increment++)
        {

            if ((ginfoiter->type == VSNAP_LUN_TYPE) && (ginfoiter->operation == REMOVE_GROUP ))
            {
                ginfoiter->state = DELETE_ACG_DONE;
                DebugPrintf(SV_LOG_DEBUG, "LUN group remove Request No: %d\n", increment);
                DebugPrintf(SV_LOG_DEBUG, "LUN No: %ld\n", ginfoiter->LunNo);
                DebugPrintf(SV_LOG_DEBUG, "Appliance LUN ID: %s\n", ginfoiter->applianceLundId.c_str());
                for (std::list<std::string>::iterator itr = ginfoiter->initiatorList.begin();
                    itr != ginfoiter->initiatorList.end(); itr++)
                {
                    //groupName = string(GROUP_NAME_PREFIX) + *itr;
                    groupName = *itr;

                    if (CheckScstGroup(groupName))
                    {
                        if (!RemoveLunFromScstGroup(ginfoiter->applianceLundId, ginfoiter->LunNo, groupName))
                            ginfoiter->state = FA_ERROR_STATE(DELETE_ACG_FAILURE);
                    }
                }
                try {
                    if (m_configurator->getVxAgent().updateGroupInfoListState(*ginfoiter))
                        DebugPrintf(SV_LOG_DEBUG, "Update Group Info List state to CX: SUCCESS\n");
                    else
                        DebugPrintf(SV_LOG_ERROR, "Update Group Info List state to CX: FAILED\n");
                }
                catch (...)
                {
                    DebugPrintf(SV_LOG_ERROR, "FAILED!!! Exception while update Group Info List state to CX\n");
                }
            }
        }

        increment = 1;
        for (list<DeviceLunOperations>::iterator linfoiter = deviceLunOpList.begin(); 
            linfoiter!=deviceLunOpList.end(); linfoiter++, increment++)
        {
            unsigned long long lunno = linfoiter->deviceLunInfo.front().lunno;
            string deviceID = linfoiter->deviceLunInfo.front().deviceID;

            if (linfoiter->state == DELETE_DEVICE_LUN_PENDING )
            {
                DebugPrintf(SV_LOG_DEBUG, "Device LUN Remove Request No: %d\n", increment);
                DebugPrintf(SV_LOG_DEBUG, "Device Name: %s\n", linfoiter->deviceName.c_str());
                DebugPrintf(SV_LOG_DEBUG, "Device LUN No: %ld\n", lunno);
                DebugPrintf(SV_LOG_DEBUG, "Device LUN No. of Blocks: %ld\n", linfoiter->size);
                DebugPrintf(SV_LOG_DEBUG, "Device LUN ID: %s\n", deviceID.c_str());

            int resultState = FA_ERROR_STATE(DEVICE_LUN_INVALID_STATE);
            DebugPrintf(SV_LOG_DEBUG, "Device LUN remove for request with LUN No: %ld\n", lunno);
            if (0 == RemoveDeviceLun (linfoiter->deviceName, lunno, linfoiter->size, deviceID, resultState))
            {
                DebugPrintf(SV_LOG_DEBUG, "Device LUN remove for LUN No: %ld, SUCCESS\n", lunno);
                linfoiter->resultState = DELETE_DEVICE_LUN_DONE;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "Device LUN remove for LUN No: %ld, FAILED\n", lunno);
                linfoiter->resultState = resultState;
            }
            try {
                if (m_configurator->getVxAgent().updateDeviceLunState(*linfoiter))
                    DebugPrintf(SV_LOG_DEBUG, "Update Device LUN state to CX: SUCCESS\n");
                else
                    DebugPrintf(SV_LOG_ERROR, "Update Device LUN state to CX: FAILED\n");
            }
            catch (...)
            {
                DebugPrintf(SV_LOG_ERROR, "FAILED!!! Exception while update Device LUN state to CX\n");
            }
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

// Target Mode
void fabricagent::PrintTargetModeOperations(list<TargetModeOperation> & tmLunOpList)
{
    list<TargetModeOperation>::iterator it = tmLunOpList.begin();
    int i = 0;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    for (; it != tmLunOpList.end(); it++)
    {
        DebugPrintf(SV_LOG_DEBUG,"Target Mode Operation List: %d\n", i+1);
        DebugPrintf(SV_LOG_DEBUG,"Target Mode Operation: %d\n", it->tmOperation);
        int j = 0;
        for (list<MemberInfo>::iterator linfoiter = it->tmInfo.begin(); 
            linfoiter!=it->tmInfo.end(); linfoiter++)
        {
            DebugPrintf(SV_LOG_DEBUG,"Member: %d:\n", j);
            DebugPrintf(SV_LOG_DEBUG,"Target Mode Operation is for PWWN: %s, NWWN: %s\n", 
                linfoiter->pwwn.c_str(), linfoiter->nwwn.c_str());
            j++;
        }
        i++;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

void fabricagent::InvokeTargetModeOperations(list<TargetModeOperation> & tmLunOpList)
{
    list<TargetModeOperation>::iterator it = tmLunOpList.begin();
    int i = 0;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    for (; it != tmLunOpList.end(); it++)
    {
        DebugPrintf(SV_LOG_DEBUG,"Target Mode Operation List: %d\n", i+1);
        DebugPrintf(SV_LOG_DEBUG,"Target Mode Operation: %d\n", it->tmOperation);

        int j = 0;
        for (list<MemberInfo>::iterator linfoiter = it->tmInfo.begin(); 
            linfoiter!=it->tmInfo.end(); linfoiter++)
        {
            DebugPrintf(SV_LOG_DEBUG,"Member: %d:\n", j);

            if (it->tmOperation == TM_ENABLE)
            {
                DebugPrintf(SV_LOG_DEBUG,"Enabling Target Mode for PWWN: %s, NWWN: %s\n", 
                    linfoiter->pwwn.c_str(), linfoiter->nwwn.c_str());

                /* delete the scsi paths of luns that are mapped for the AT port */
                DeviceHelper dh;
                std::vector<std::string> hostIdVec;
                bool hostIdFound;
                std::string pwwn = dh.getColonWWN(linfoiter->pwwn);
                if(dh.getHostId(pwwn, "", hostIdVec, hostIdFound))
                {
                    if(!hostIdFound)
                    {
                        DebugPrintf(SV_LOG_ERROR, "Host is not found for AT port %s.\n",pwwn.c_str());
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_DEBUG, "Removing all scsi devices mapped to AT port %s.\n",pwwn.c_str());
                        for(int index=0; index<hostIdVec.size(); ++index)
                        {
                            dh.removeAllDevice(hostIdVec[index],"","");
                        }
                    }
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "Error while getting Host for AT port %s.\n",linfoiter->pwwn.c_str());
                }

                if (0 == EnableTargetMode (linfoiter->pwwn, linfoiter->nwwn))
                {
                    DebugPrintf(SV_LOG_DEBUG,"Result: Enabling Target Mode for PWWN: %s, NWWN: %s SUCCES\n", 
                        linfoiter->pwwn.c_str(), linfoiter->nwwn.c_str());

                    linfoiter->tmStatus = TM_OP_SUCCESS;
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "Result: Enabling Target Mode for PWWN: %s, NWWN: %s FAILED\n", 
                        linfoiter->pwwn.c_str(), linfoiter->nwwn.c_str());
                    linfoiter->tmStatus = TM_OP_FAILURE;
                }
            }
            else if (it->tmOperation == TM_DISABLE)
            {
                DebugPrintf(SV_LOG_DEBUG, "Disabling Target Mode for PWWN: %s, NWWN: %s\n", 
                    linfoiter->pwwn.c_str(), linfoiter->nwwn.c_str());

                if (0 == DisableTargetMode (linfoiter->pwwn, linfoiter->nwwn))
                {
                    DebugPrintf(SV_LOG_DEBUG, "Result: Disabling Target Mode for PWWN: %s, NWWN: %s SUCCES\n", 
                        linfoiter->pwwn.c_str(), linfoiter->nwwn.c_str());
                    linfoiter->tmStatus = TM_OP_SUCCESS;
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "Result: Disabling Target Mode for PWWN: %s, NWWN: %s FAILED\n", 
                        linfoiter->pwwn.c_str(), linfoiter->nwwn.c_str());
                    linfoiter->tmStatus = TM_OP_FAILURE;
                }
            }
            j++;
        }
        i++;
    }	
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}
// -- Target Mode end
 
void fabricagent::InvokePrismAtOperations(list<PrismATLunOperations> & atLunOpList)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG,"Prism ATLUN Setup/Remove Requests Info:\n");
    int increment = 1;
    for (list<PrismATLunOperations>::iterator linfoiter = atLunOpList.begin(); 
        linfoiter!=atLunOpList.end(); linfoiter++, increment++)
    {
        DebugPrintf(SV_LOG_DEBUG, "LUN Setup/Remove Request No: %d\n", increment);
        DebugPrintf(SV_LOG_DEBUG, "Appliance LUN ID: %s\n", linfoiter->applianceLunId.c_str());
        /* PrismATLunOperations.size is total number of blocks */
        DebugPrintf(SV_LOG_DEBUG, "LUN Num of Blocks: %ld\n", linfoiter->size);
        DebugPrintf(SV_LOG_DEBUG, "startingPhysicalOffset : %ld\n", linfoiter->startingPhysicalOffset);
        DebugPrintf(SV_LOG_DEBUG, "LUN Block Size: %d\n", linfoiter->blockSize);

        if (linfoiter->state == CREATE_AT_LUN_PENDING)
        {
            DebugPrintf(SV_LOG_DEBUG, "Prism AT LUN Create request\n");

            if (0 == ExportATLun (linfoiter->applianceLunId, linfoiter->size, linfoiter->startingPhysicalOffset,
                        linfoiter->blockSize, linfoiter->acgList, 
                        linfoiter->enableAppliancePassthrough, linfoiter->physicalLunPath))
            {
                DebugPrintf(SV_LOG_DEBUG, "Prism AT LUN Setup SUCCESS\n");
                linfoiter->resultState = CREATE_AT_LUN_DONE;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "Prism AT LUN Setup FAILED\n");
                linfoiter->resultState = FA_ERROR_STATE(CREATE_AT_LUN_FAILURE);
            }

        }
        else if (linfoiter->state == DELETE_AT_LUN_PENDING )
        {

            DebugPrintf(SV_LOG_DEBUG, "Prism AT LUN Delete request\n");
            if (0 == RemoveATLun (linfoiter->applianceLunId, linfoiter->size,
                linfoiter->blockSize, linfoiter->acgList))
            {
                DebugPrintf(SV_LOG_DEBUG, "Prism AT LUN remove SUCCESS\n");
                linfoiter->resultState = DELETE_AT_LUN_DONE;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "Prism AT LUN remove FAILED\n");
                linfoiter->resultState = FA_ERROR_STATE(DELETE_AT_LUN_FAILURE);
            }
        } 
        else if ( FAILOVER_DRIVER_DATA_CLEANUP_PENDING == linfoiter->state )
        {
            DebugPrintf(SV_LOG_DEBUG, "AT LUN request for driver data cleanup(clear diffs): %s\n", linfoiter->applianceLunId.c_str());
            if( ! IsAtLunPresent(linfoiter->applianceLunId) )
            {
                DebugPrintf(SV_LOG_ERROR, "AT LUN : %s, Doesn't exist, cannot perform Cleardiffs\n", linfoiter->applianceLunId.c_str());
                linfoiter->resultState = FAILOVER_DRIVER_DATA_CLEANUP_DONE;
            }
            else
            {
                if (ClearDiffsOnATLun(linfoiter->applianceLunId))
                {
                    DebugPrintf(SV_LOG_DEBUG, "fabricagent::InvokeAtOperations clear diffs on AT Lun[%s] SUCCESS\n",
                        linfoiter->applianceLunId.c_str());
                    linfoiter->resultState = FAILOVER_DRIVER_DATA_CLEANUP_DONE;
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR,"Failed!!!, fabricagent::InvokeAtOperations clear diffs on AT Lun[%s]\n",
                        linfoiter->applianceLunId.c_str());
                    linfoiter->resultState = FA_ERROR_STATE(FAILOVER_DRIVER_DATA_CLEANUP_FAILURE);
                }
            }
        }

        while ( true)
        {
            try 
            {
                if(m_configurator->getVxAgent().updatePrismApplianceLunState(*linfoiter))
                {
                    DebugPrintf( SV_LOG_INFO, "fabricagent::InvokePrismAtOperations updatePrismApplianceLunState returned success.\n" );        
                    break;
                }
                DebugPrintf( SV_LOG_ERROR, "fabricagent::InvokePrismAtOperations returned error while updatePrismApplianceLunState\n" );        
            }
            catch(...) 
            {
                DebugPrintf( SV_LOG_ERROR, "fabricagent::InvokePrismAtOperations caught exception while updatePrismApplianceLunState\n" );        
            }
            ACE_OS::sleep(5);
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


void fabricagent::ProcessAtOperations()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    FabricAgentInitialSettings fabricInitialSettings;
    try {
        fabricInitialSettings = m_configurator->getVxAgent().getFabricServiceSetting();	
    }
    catch (std::exception const & ie) {
        DebugPrintf(SV_LOG_ERROR, "fabricagent::ProcessAtOperations getFabricSettings %s\n", ie.what());             
    }
    catch( std::string const& e ) {
        DebugPrintf( SV_LOG_ERROR,"fabricagent::ProcessAtOperations getFabricSettings caught exception: %s\n", e.c_str() );
        //throw e;
    }   
    catch( char const* e ) {
        DebugPrintf( SV_LOG_ERROR,"fabricagent::ProcessAtOperations getFabricSettings caught exception: %s\n", e );
        //throw e;
    }
    catch(...) {
        DebugPrintf( SV_LOG_ERROR,"fabricagent::ProcessAtOperations getFabricSettings caught exception:...\n" );        
        //throw;
    }

    try {
        InvokeAtOperations(fabricInitialSettings.atLunOperationList);
    }
    catch(...) {
        DebugPrintf( SV_LOG_ERROR,"fabricagent::ProcessAtOperations caught InvokeAtOperations\n" );        
        //throw;
    }
    try {
        DeviceHelper resyncDeviceManager;
        resyncDeviceManager.doDeviceOperation(fabricInitialSettings.discoverBindingDeviceSettingsList);
    }
    catch(...) {
        DebugPrintf( SV_LOG_ERROR,"fabricagent::ProcessAtOperations caught doDeviceOperation\n" );        
        //throw;
    }

    try {
        PrintTargetModeOperations(fabricInitialSettings.atModeConfiguration);
        InvokeTargetModeOperations(fabricInitialSettings.atModeConfiguration);
    }
    catch(...) {
        DebugPrintf( SV_LOG_ERROR,"fabricagent::ProcessAtOperations caught InvokeTargetModeOperations\n" );        
        //throw;
    }
    try {
        m_configurator->getVxAgent().updateTargetModeStatus(fabricInitialSettings.atModeConfiguration);	
    }
    catch(...) {
        DebugPrintf( SV_LOG_ERROR,"fabricagent::ProcessAtOperations caught updateTargetModeStatus\n" );        
        //throw;
    }
    try {
        InvokeDeviceLunOperations(fabricInitialSettings.deviceLunOperationList, fabricInitialSettings.groupInfoList);
    }
    catch (std::exception const & ie) {
        DebugPrintf(SV_LOG_ERROR, "fabricagent::ProcessAtOperations %s\n", ie.what());             
    }
    catch( std::string const& e ) {
        DebugPrintf( SV_LOG_ERROR,"fabricagent::ProcessAtOperations caught exception: %s\n", e.c_str() );
        //throw e;
    }   
    catch( char const* e ) {
        DebugPrintf( SV_LOG_ERROR,"fabricagent::ProcessAtOperations caught exception: %s\n", e );
        //throw e;
    }
    catch(...) {
        DebugPrintf( SV_LOG_ERROR,"fabricagent::ProcessAtOperations caught exception:...\n" );        
        //throw;
    }
 
    PrismAgentInitialSettings prismInitialSettings;

    try {

        prismInitialSettings = m_configurator->getVxAgent().getPrismServiceSetting();
        DeviceHelper resyncDeviceManager;
        resyncDeviceManager.doDeviceOperation(prismInitialSettings.discoverBindingDeviceSettingsList);
    }
    catch (std::exception const & ie) {
        DebugPrintf(SV_LOG_ERROR, "fabricagent::ProcessAtOperations %s\n", ie.what());             
    }
    catch( std::string const& e ) {
        DebugPrintf( SV_LOG_ERROR,"fabricagent::ProcessAtOperations caught exception: %s\n", e.c_str() );
        //throw e;
    }   
    catch( char const* e ) {
        DebugPrintf( SV_LOG_ERROR,"fabricagent::ProcessAtOperations caught exception.: %s\n", e );
        //throw e;
    }
    catch(...) {
        DebugPrintf( SV_LOG_ERROR,"fabricagent::ProcessAtOperations caught exception:...\n" );        
        //throw;
    }

    try {
        InvokePrismAtOperations(prismInitialSettings.prismAtLunOperationList);
    }
    catch(...) {
        DebugPrintf(SV_LOG_ERROR, "fabricagent::ProcessAtOperations caught InvokePrismAtOperations\n");
    }

#if 0
    try {
        CleanUpEmptyScstGroups();
    }
    catch(...) {
        DebugPrintf(SV_LOG_ERROR, "fabricagent::ProcessAtOperations caught CleanUpEmptyScstGroups\n");
    }
#endif

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

}

bool fabricagent::ProcessAtOperationsOnReboot()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool bflag = false;
    try {

        FabricAgentInitialSettings fabricInitialSettings = m_configurator->getVxAgent().getFabricServiceSettingOnReboot();	
        PrintTargetModeOperations(fabricInitialSettings.atModeConfiguration);
        InvokeTargetModeOperations(fabricInitialSettings.atModeConfiguration);
        m_configurator->getVxAgent().updateTargetModeStatusOnReboot(fabricInitialSettings.atModeConfiguration);
        bflag = true;
    }
    catch (std::exception const & ie) 
    {
        DebugPrintf(SV_LOG_ERROR, "fabricagent::ProcessAtOperationsOnReboot %s", ie.what());             
    }
    catch( std::string const& e ) 
    {
        DebugPrintf( SV_LOG_ERROR,"fabricagent::ProcessAtOperationsOnReboot caught exception: %s\n", e.c_str() );
    }   
    catch( char const* e ) 
    {
        DebugPrintf( SV_LOG_ERROR,"fabricagent::ProcessAtOperationsOnReboot caught exception: %s\n", e );
    }
    catch(...) 
    {
        DebugPrintf( SV_LOG_ERROR,"fabricagent::ProcessAtOperationsOnReboot caught exception:...\n" );        
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return bflag;
}


void fabricagent::onFailoverCxSignal(ReachableCx r)
{
    m_bRemoteCxReachable = true;
}

///
/// Pump messages until service decides to stop. Restarts any agent process that has exited.
/// Before restarting any process, fetches the most recent settings from the CX server.
///

void fabricagent::ProcessMessageLoop()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    while(!this->quit && !m_bRemoteCxReachable)
    {
        try 
        {
            if ( !isInitialized())
                this->init();

            DebugPrintf( SV_LOG_DEBUG,"Polling for CX settings change\n" );

            if ( isInitialized() )
            {
                ProcessAtOperationsIfRequired();
                ReportInitiatorsIfRequired();
            }
            ACE_OS::sleep(SETTINGS_POLLING_INTERVAL);
        }
        catch( std::string const& e ) {
            DebugPrintf( SV_LOG_ERROR,"fabricagent::ProcessMessageLoop caught exception: %s\n", e.c_str() );
            throw e;
        }   
        catch( char const* e ) {
            DebugPrintf( SV_LOG_ERROR,"fabricagent::ProcessMessageLoop caught exception: %s\n", e );
            throw e;
        }
        catch(...) {
            DebugPrintf( SV_LOG_ERROR,"fabricagent::ProcessMessageLoop caught exception:...\n" );        
            throw;
        }
    }  
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

}


int fabricagent::handle_signal(int signum, siginfo_t *si, ucontext_t * uc) 
{
    // Do not call LOG_EVENT in this function. 
    int iRet = -1;

    if (signum == SIGTERM)
    {
        this->StopWork(this);
        iRet = 0;
    }

    return iRet;
}


// TODO:
// this is needed in case a db clean up is done on the cx (which wipes out all volume information).
// once we have a way for the cx to request hosts to re-register this will no longer be needed
// NOTE: volume monitor is used to requests RegisterHost when there are changes to any volume
void fabricagent::RegisterHostIfNeeded()
{
    DebugPrintf( SV_LOG_DEBUG,"ENTERED: RegisterHostIfNeeded\n" );

    ACE_Time_Value currentTime = ACE_OS::gettimeofday();
    if( (currentTime.sec() - LastRegisterHostTime_g.sec()) > REGISTER_HOST_INTERVAL_IN_SECONDS )
    {
        DebugPrintf( SV_LOG_DEBUG,"RegisterHost in RegisterHostIfNeeded\n" );
        RegisterHost();
        LastRegisterHostTime_g = currentTime;
    }
    DebugPrintf( SV_LOG_DEBUG,"EXITED: RegisterHostIfNeeded\n" );
}

void fabricagent::ProcessAtOperationsIfRequired()
{
    DebugPrintf( SV_LOG_DEBUG,"ENTERED: ProcessAtOperationsIfRequired\n" );

    ACE_Time_Value currentTime = ACE_OS::gettimeofday();
    if( (currentTime.sec() - LastFabricSettingsTime_g.sec()) > GET_FABRIC_SETTINGS_INTERVAL_IN_SECONDS )
    {
        DebugPrintf( SV_LOG_DEBUG,"Calling ProcessAtOperations\n" );
        ProcessAtOperations();
        LastFabricSettingsTime_g = currentTime;
    }
    DebugPrintf( SV_LOG_DEBUG,"EXITED: ProcessAtOperationsIfRequired\n" );
}

void fabricagent::ReportInitiatorsIfRequired()
{
    DebugPrintf( SV_LOG_DEBUG,"ENTERED: ReportInitiatorsIfRequired\n" );

    ACE_Time_Value currentTime = ACE_OS::gettimeofday();
    if( (currentTime.sec() - LastReportInitiatorsTime_g.sec()) > REPORT_INITIATOR_INTERVAL_IN_SECONDS )
    {
        DebugPrintf( SV_LOG_DEBUG,"Calling DeviceHelper::reportInitiators\n" );
        DeviceHelper deviceHelper;
        std::vector<SanInitiatorSummary> sanInitiatorList;
        if(deviceHelper.reportInitiators(sanInitiatorList))
        {
            try
            {
                m_configurator->getVxAgent().SanRegisterInitiators(sanInitiatorList);
            }
            catch (ContextualException &ce)
            {
                DebugPrintf(SV_LOG_ERROR, "%s :: %s\n", FUNCTION_NAME, ce.what());
            }
            catch (std::exception &e)
            {
                DebugPrintf(SV_LOG_ERROR, "%s :: %s\n", FUNCTION_NAME, e.what());
            }
            catch (...)
            {
                DebugPrintf(SV_LOG_ERROR, "%s :: unknown exception.\n", FUNCTION_NAME);
            }
        }
        LastReportInitiatorsTime_g = currentTime;
    }
    DebugPrintf( SV_LOG_DEBUG,"EXITED: ReportInitiatorsIfRequired\n" );
}


int fabricagent::FindHBA(string portName, string nodeName, string & hostName)
{

    DIR             *dip;
    struct dirent   *dit;

    int             i = 0;
    /* check to see if user entered a directory name */

    /* DIR *opendir(const char *name);
    *
    * Open a directory stream to argv[1] and make sure
    * it's a readable and valid (directory) */
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    if ((dip = opendir("/sys/class/fc_host")) == NULL)
    {
        perror("opendir /sys/class/fc_host error");
        DebugPrintf(SV_LOG_ERROR, "opendir /sys/class/fc_host FAILED\n");
        return 0;
    }

    DebugPrintf(SV_LOG_DEBUG, "opendir /sys/class/fc_host SUCCESS\n");

    /*  struct dirent *readdir(DIR *dir);
    *
    * Read in the files from argv[1] and print */
    bool gotHBA = false;
    while ((dit = readdir(dip)) != NULL)
    {
        //i++;
        //("\n%s", dit->d_name);
        if (strncmp(dit->d_name, "host", 4) == 0)
        {
            string portNameFile = "/sys/class/fc_host/" + string(dit->d_name) + "/port_name";
            string nodeNameFile = "/sys/class/fc_host/" + string(dit->d_name) + "/node_name";
            string currentPortName, currentNodeName;
            ifstream portNameFileStream(portNameFile.c_str(), ifstream::in);
            ifstream nodeNameFileStream(nodeNameFile.c_str(), ifstream::in);
            if (portNameFileStream.good())
                portNameFileStream >> currentPortName;
            if (nodeNameFileStream.good())
                nodeNameFileStream >> currentNodeName;
            portNameFileStream.close();
            nodeNameFileStream.close();

            unsigned long long currentPortWWN,hexPortWWN;
            currentPortWWN = strtoll(currentPortName.c_str(),NULL,16);
            hexPortWWN = strtoll(portName.c_str(),NULL,16);
            if (currentPortWWN == hexPortWWN )
            {
                hostName = dit->d_name;
                gotHBA = true;
                break;
            }

        }
    }
    DebugPrintf(SV_LOG_DEBUG, "readdir(): found total of %i files\n", i);

    if (closedir(dip) == -1)
    {
        DebugPrintf(SV_LOG_ERROR, "closedir FAILED\n");
        return 0;
    }
    else
        DebugPrintf(SV_LOG_INFO, "closedir SUCCESS\n");

    if (gotHBA)
    {
        DebugPrintf(SV_LOG_DEBUG, "Given port name: %s, node name: %s, corresponding host name is: %s\n",
            portName.c_str(), nodeName.c_str(), hostName.c_str());

    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "Given port name: %s, node name: %s, No host name found\n",
            portName.c_str(), nodeName.c_str());

    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", FUNCTION_NAME);

    return 1;
}

void fabricagent::issueLip(string hbaName)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    string issueLipStr = "/sys/class/fc_host/" + hbaName + "/issue_lip";
    ofstream issueLipStream (issueLipStr.c_str(), ifstream::out);

    if (issueLipStream.good())
    {
        DebugPrintf(SV_LOG_DEBUG,"Opening %s\n", issueLipStr.c_str());
        DebugPrintf(SV_LOG_DEBUG,"Appending: 1\n");
        issueLipStream << "1";
        if (issueLipStream.good())
            DebugPrintf(SV_LOG_DEBUG,"Append operation: SUCCESS\n");
        else
            DebugPrintf(SV_LOG_ERROR,"Append operation: FAILED with errno = %d\n", errno);

        if (issueLipStream.is_open()) 
            issueLipStream.close();
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", FUNCTION_NAME);
    return;
}

int fabricagent::EnableTargetMode(string portName, string nodeName)
{
    string hbaName;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "portName = %s, nodeName = %s\n", portName.c_str(),  nodeName.c_str());
    if ( 1 == FindHBA(portName, nodeName, hbaName))
    {
        DebugPrintf(SV_LOG_DEBUG, "Got hbaName = %s\n", hbaName.c_str());
        string enableTargetStr = "/sys/class/scsi_host/" + hbaName + "/target_mode_enabled";
        ofstream enableTargetStream (enableTargetStr.c_str(), ifstream::out);
        if (enableTargetStream.good())
        {
            DebugPrintf(SV_LOG_DEBUG,"Opening %s\n", enableTargetStr.c_str());
            DebugPrintf(SV_LOG_DEBUG,"Appending: 1\n");
            enableTargetStream << "1";
            if (enableTargetStream.good())
            {
                DebugPrintf(SV_LOG_DEBUG,"Append operation: SUCCESS\n");
                ACE_OS::sleep(30);
                issueLip(hbaName);
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR,"Append operation: FAILED with errno = %d\n", errno);
                if (enableTargetStream.is_open()) enableTargetStream.close();
                return 1;	
            }
            enableTargetStream.close();
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG,"Opening file: %s FAILED\n", enableTargetStr.c_str());
            return 1;
        }
        DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", FUNCTION_NAME);
        return 0;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", FUNCTION_NAME);
    return 1;
}

int fabricagent::DisableTargetMode(string portName, string nodeName)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "portName = %s, nodeName = %s\n", portName.c_str(),  nodeName.c_str());
    string hbaName;
    if ( 1 == FindHBA(portName, nodeName, hbaName))
    {
        DebugPrintf(SV_LOG_DEBUG, "Got hbaName = %s\n", hbaName.c_str());
        string enableTargetStr = "/sys/class/scsi_host/" + hbaName + "/target_mode_enabled";
        ofstream enableTargetStream (enableTargetStr.c_str(), ifstream::out);
        if (enableTargetStream.good())
        {
            DebugPrintf(SV_LOG_DEBUG,"Opening %s\n", enableTargetStr.c_str());
            DebugPrintf(SV_LOG_DEBUG,"Appending: 0\n");
            enableTargetStream << "0";
            if (enableTargetStream.good())
            {
                DebugPrintf(SV_LOG_DEBUG,"Append operation: SUCCESS\n");
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR,"Append operation: FAILED with errno = %d\n", errno);
                if (enableTargetStream.is_open()) enableTargetStream.close();
                return 1;	
            }
            enableTargetStream.close();
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR,"Opening file: %s FAILED\n", enableTargetStr.c_str());
            return 1;
        }
        DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", FUNCTION_NAME);
        return 0;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", FUNCTION_NAME);
    return 1;
}

bool fabricagent::checkBlockSizeMatch(string LunName, int blockSize, int& oldBlockSize)
{
    string volBsize;
    if (GetATLunAttr(LunName, "VolumeBsize", volBsize))
    {
        istringstream strVolBsize(volBsize);
        strVolBsize >> oldBlockSize;

        if (oldBlockSize == blockSize)
            return true;
        else
            return false;
    }
    else
        return false;
}

bool fabricagent::checkLunNBlksMatch(string LunName, unsigned long long nBlks, unsigned long long &oldNBlks)
{
    string volNblks;
    if (GetATLunAttr(LunName, "VolumeNblks", volNblks))
    {
        istringstream strVolBsize(volNblks);
        strVolBsize >> oldNBlks;

        if (nBlks == oldNBlks)
            return true;
        else
            return false;
    }
    else
        return false;
}

bool fabricagent::CheckATLun(string atLunName, unsigned long long lunSize, int blockSize,
                    unsigned long long& oldLunSize, int& oldBlockSize)
{
    if( ! IsAtLunPresent(atLunName) )
    {
        return false;
    }

    if (checkBlockSizeMatch(atLunName, blockSize, oldBlockSize))
    {
        unsigned long long oldNBlks;
        checkLunNBlksMatch(atLunName, (blockSize == 0) ? 0 : (lunSize / blockSize), oldNBlks);
        oldLunSize = oldNBlks * blockSize;
    }

    return true;
}

int fabricagent::CreateATLun(string atLunName, unsigned long long lunSize, 
                    int blockSize, unsigned long long startingPhysicalOffset)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "atLunName = %s, lunSize = %ld, blockSize = %d\n", 
        atLunName.c_str(), lunSize, blockSize);

    int rc = 0;

    int fd = open(INMAGE_FILTER_DEVICE_NAME, O_RDWR, 0);

    if (fd < 0)
    {
        DebugPrintf(SV_LOG_ERROR, "FAILED!!!, Could not open /dev/involflt\n");
        DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", FUNCTION_NAME);
        return 1;
    }

    LUN_CREATE_INPUT lun_data;
    memset(&lun_data, 0, sizeof(lun_data));
    inm_strncpy_s(lun_data.uuid,ARRAYSIZE(lun_data.uuid), atLunName.c_str(), GUID_SIZE_IN_CHARS);

    if (lunSize % blockSize)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed!!! lunSize[%d] not multiple of blockSize[%d]\n", lunSize, blockSize);
        return 1;
    }

    /* lunSize in number of blocks */
    lun_data.lunSize = (blockSize == 0) ? 0 : (lunSize / blockSize);
    lun_data.lunStartOff = startingPhysicalOffset;
    lun_data.blockSize = blockSize;
    lun_data.lunType = FILTER_DEV_FABRIC_LUN;

    if ( (rc = ioctl(fd, IOCTL_INMAGE_AT_LUN_CREATE, &lun_data)) < 0 )
    {
        /* driver returns EEXIST */
        /* if stale driver entry with different parameters present */
        /* errno is a per process variable when making this multithreaded, appropriate care should be taken */
        /* for further details "man ioctl" */
        if (EEXIST == errno)
        {
            DebugPrintf(SV_LOG_ERROR, "Lun: %s creation failed, ioctl returned EEXIST(%d), with errno = %d\n",
                atLunName.c_str(), rc, errno);
            close(fd);
            DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", FUNCTION_NAME);
            return 1;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Lun: %s creation failed %d, with errno = %d\n",
                atLunName.c_str(), rc, errno);
            close(fd);
            DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", FUNCTION_NAME);
            return 1;
        }
    }

    close(fd);
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", FUNCTION_NAME);
    return 0;
}

int fabricagent::ExportATLun (string atLunName, unsigned long long lunSize, 
                    unsigned long long startingPhysicalOffset,
                    int blockSize, AcgInfoList acgList, int enablePassthrough,
                    string PTPath)
{
    unsigned long long oldLunSize=lunSize;
    int oldBlockSize=blockSize;

    if (!CheckATLun(atLunName, lunSize, blockSize, oldLunSize, oldBlockSize))
    {
        if (0 != CreateATLun(atLunName, lunSize, blockSize, startingPhysicalOffset))
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to create lun[%s]\n", atLunName.c_str());
            return 1;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "LUN : %s already exists.\n", atLunName.c_str());
    }

    if ((oldBlockSize != blockSize) || (oldLunSize != lunSize))
    {
        DebugPrintf(SV_LOG_DEBUG, "LUN %s size mismatch. Recreating after deleting existing LUN.\n", atLunName.c_str());

        // if an existing entry is found with different size, clean up 
        RemoveLunFromAllScstGroups(atLunName);
        DeleteATLun(atLunName);

        if (0 != CreateATLun(atLunName, lunSize, blockSize, startingPhysicalOffset))
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to create lun[%s]\n", atLunName.c_str());
            return 1;
        }
    }

    if (enablePassthrough == ENABLE_APPLIANCE_PASSTHROUGH)
    {
        if (0 == AddPTPath(atLunName, PTPath))
        {
            DebugPrintf(SV_LOG_DEBUG, "AT Passthrough SUCCESS\n");
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "AT Passthrough FAILED\n");
            return 1;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "LUN Export for LUN : acgList size %d\n", acgList.size());

    for (AcgInfoListIter acginfoiter = acgList.begin();
                acginfoiter!= acgList.end(); acginfoiter++)
    {
        if (0 == ExportScstLun(atLunName, acginfoiter->lunno, acginfoiter->groupName, acginfoiter->sanInitiator))
        {
            DebugPrintf(SV_LOG_DEBUG, "LUN Export for LUN : %s, Group %s, Num %d, Initiator %s SUCCESS\n", atLunName.c_str(), acginfoiter->groupName.c_str(), acginfoiter->lunno, acginfoiter->sanInitiator.c_str());
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "LUN Export for LUN : %s, Group %s, Num %d, Initiator %s FAILED\n", atLunName.c_str(), acginfoiter->groupName.c_str(), acginfoiter->lunno, acginfoiter->sanInitiator.c_str());
            return 1;
        }
    }

    return 0;
}

int fabricagent::ExportScstLun (string atLunName, unsigned long long atLunNo, string groupName, string wwn)
{

    if (!CheckScstGroup(groupName))
    {
        if (!CreateScstGroup(groupName))
        {
            return 1;
        }

        if (!AddLunToScstGroup(DUMMY_LUN_NAME, 0, groupName))
        {
            // if dummy lun is not added to the group, don't proceed
            DebugPrintf(SV_LOG_ERROR, "Failed to add DUMMY LUN ZERO lun to group %s\n", groupName.c_str());
            return 1;
        }
    }

    unsigned long long oldLunNo=atLunNo;
    if (!CheckLunInScstGroup(atLunName, atLunNo, groupName, oldLunNo))
    {
        if (!AddLunToScstGroup(atLunName, atLunNo, groupName))
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to create lun[%s]\n", atLunName.c_str());
            return 1;
        }
    }

    if (oldLunNo != atLunNo)
    {
        RemoveLunFromAllScstGroups(atLunName);
        DeleteATLun(atLunName);

        if (!AddLunToScstGroup(atLunName, atLunNo, groupName))
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to create lun[%s]\n", atLunName.c_str());
            return 1;
        }
    }

    if (!CheckWwnInScstGroup(groupName, wwn))
    {
        if (!AddWwnToScstGroup(groupName, wwn))
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to add wwn[%s] to group[%s]\n", wwn.c_str(), groupName.c_str());
            return 1;
        }
    }

    return 0;
}

bool fabricagent::DeleteATLun(string atLunName)
{
    // issue ioctl remove (atLunName)
    int fd = 0, rc = 0;
    LUN_DELETE_INPUT lun_delete_data;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    fd = open(INMAGE_FILTER_DEVICE_NAME, O_RDWR, 0);
    if (fd < 0)
    {
        DebugPrintf(SV_LOG_DEBUG,"Could not open /dev/involflt :\n");
        DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", FUNCTION_NAME);
        return false;
    }
    memset(&lun_delete_data, 0, sizeof(lun_delete_data));
    inm_strncpy_s(lun_delete_data.uuid,ARRAYSIZE(lun_delete_data.uuid), atLunName.c_str(), GUID_SIZE_IN_CHARS);
    lun_delete_data.lunType = FILTER_DEV_FABRIC_LUN;
    if ( (rc = ioctl(fd, IOCTL_INMAGE_AT_LUN_DELETE, &lun_delete_data)) < 0 )
    {
        DebugPrintf(SV_LOG_ERROR, "Lun %s delete Failed with return value = %d, with errno = %d\n",
            atLunName.c_str(), rc, errno);
        close(fd);
        DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", FUNCTION_NAME);
        return false;
    }
    close(fd);
    return true;
}

int fabricagent::RemoveATLun (string atLunName, unsigned long long lunSize,
                              int blockSize, AcgInfoList acgList)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "atLunName = %s, lunSize = %ld, blockSize = %d\n",
        atLunName.c_str(), lunSize, blockSize);

    for (AcgInfoListIter acginfoiter = acgList.begin();
                acginfoiter!= acgList.end(); acginfoiter++)
    {
        string groupName = acginfoiter->groupName;
        unsigned long long atLunNo = acginfoiter->lunno;
        string wwn = acginfoiter->sanInitiator;
        
        DebugPrintf(SV_LOG_DEBUG, "groupName %s, lunno = %ld, wwn = %s\n",
            groupName.c_str(), atLunNo, wwn.c_str());

        if (0== UnexportScstLun (atLunName, atLunNo, groupName, wwn))
        {
            DebugPrintf(SV_LOG_DEBUG, "fabricagent::RemoveATLun for LUN : %s, Group %s, Num %d, Initiator %s SUCCESS\n", atLunName.c_str(), groupName.c_str(), atLunNo, wwn.c_str());
            
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "fabricagent::RemoveATLun for LUN : %s, Group %s, Num %d, Initiator %s FAILED\n", atLunName.c_str(), groupName.c_str(), atLunNo, wwn.c_str());

        }
    }

    unsigned long long oldLunSize=lunSize;
    int oldBlockSize=blockSize;
    if (!CheckATLun(atLunName, lunSize, blockSize, oldLunSize, oldBlockSize))
    {
        RemoveLunFromAllScstGroups(atLunName);
    }

    if (IsAtLunPresent(atLunName) && !DeleteATLun(atLunName))
    {
        DebugPrintf(SV_LOG_ERROR, "fabricagent::RemoveATLun failed to delete ATLun[%s]\n", atLunName.c_str());
        return 1;
    }

    return 0;
}

int fabricagent::UnexportScstLun (string atLunName, unsigned long long atLunNo, string groupName, string wwn)
{
    if (!CheckScstGroup(groupName))
    {
        DebugPrintf(SV_LOG_DEBUG,"fabricagent::UnexportScstLun group[%s] not present.\n", groupName.c_str());
        return 1;
    }

    unsigned long long oldLunNo=atLunNo;
    if (!CheckLunInScstGroup(atLunName, atLunNo, groupName, oldLunNo))
    {
        DebugPrintf(SV_LOG_DEBUG,"fabricagent::UnexportScstLun lun[%s] not present in group %s.\n", atLunName.c_str(), groupName.c_str());
        return 1;
    }

    if (!RemoveLunFromScstGroup(atLunName, oldLunNo, groupName))
    {
        DebugPrintf(SV_LOG_ERROR, "fabricagent::UnexportScstLun failed to remove lun %d from group %s\n", oldLunNo,  groupName.c_str());
        return 1;
    }

    // the wwn need not be removed from the group...

    return 0;
}

int fabricagent::ExportDeviceLun (string deviceLunName, unsigned long long deviceLunNo, unsigned long long lunSize, string deviceID,
                                     int & resultState, bool flag)
{
    // issue ioctl add (atLunName, lunSize)
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG,
        "ExportDeviceLun: deviceLunName = %s,  deviceLunNo = %d, lunSize = %ld, deviceID = %s, flag = %d\n",
        deviceLunName.c_str(), deviceLunNo, lunSize, deviceID.c_str(), flag);

    string devicesFile = SCSI_TGT_VDISK_PATH;
    ofstream filestreamVdisk(devicesFile.c_str(), ofstream::out);
    if (filestreamVdisk.good())
    {
        DebugPrintf(SV_LOG_DEBUG,"Opening %s\n", devicesFile.c_str());
        std::string ioMode = flag ? "BLOCKIO" : " "; //1 for rw, and 0 for ro
        DebugPrintf(SV_LOG_DEBUG,"Appending: open %s %s %s\n", deviceLunName.c_str(), deviceID.c_str(), ioMode.c_str());
        filestreamVdisk << "open " << deviceLunName << " " << deviceID << " " << ioMode << endl;  
        if (filestreamVdisk.good())
        {
            DebugPrintf(SV_LOG_DEBUG,"Append operation: SUCCESS\n");
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR,"Append operation: FAILED with errno = %d\n", errno);
            if (filestreamVdisk.is_open()) filestreamVdisk.close();
            resultState = FA_ERROR_STATE(CREATE_DEVICE_LUN_ID_FAILURE);
            return 1;	
        }
        filestreamVdisk.close();
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR,"Opening %s Failed\n", devicesFile.c_str());
        resultState = FA_ERROR_STATE(CREATE_DEVICE_LUN_ID_FAILURE);
        return 1;  
    }


    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", FUNCTION_NAME);
    resultState = CREATE_DEVICE_LUN_DONE;
    return 0;

}

int fabricagent::RemoveDeviceLun (string deviceLunName, unsigned long long deviceLunNo, unsigned long long lunSize, string deviceID,
                                  int & resultState)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG,
        "RemoveDeviceLun: deviceLunName = %s,  deviceLunNo = %d, lunSize = %ld, deviceID = %s\n",
        deviceLunName.c_str(), deviceLunNo, lunSize, deviceID.c_str());


    std::string devicesFile = SCSI_TGT_VDISK_PATH;
    ofstream filestreamVdisk(devicesFile.c_str(), ofstream::out);
    if (filestreamVdisk.good())
    {
        DebugPrintf(SV_LOG_DEBUG,"Opening %s\n", devicesFile.c_str());
        DebugPrintf(SV_LOG_DEBUG,"Appending: close %s\n", deviceLunName.c_str());
        filestreamVdisk << "close " << deviceLunName << endl;  
        if (filestreamVdisk.good())
        {
            DebugPrintf(SV_LOG_DEBUG,"Append operation: SUCCESS\n");
        }
        else
        {
            if(errno != EINVAL)   //consider invalid as success.
            {
                DebugPrintf(SV_LOG_ERROR,"Append operation: FAILED with errno = %d\n", errno);
                if (filestreamVdisk.is_open()) filestreamVdisk.close();
                //return 1;	
                resultState = FA_ERROR_STATE(DELETE_DEVICE_LUN_ID_FAILURE);
                //goto cleanup;	
                return 1;
            }
        } 
        filestreamVdisk.close();
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR,"Opening %s Failed\n", devicesFile.c_str());
        //return 1;  
        resultState = FA_ERROR_STATE(DELETE_DEVICE_LUN_ID_FAILURE);
        //goto cleanup;
        return 1;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", FUNCTION_NAME);
    resultState = DELETE_DEVICE_LUN_DONE;
    return 0;
}

bool fabricagent::Append(string fileName, string command)
{
    ofstream fileStream(fileName.c_str(), ofstream::out);
    if (fileStream.good())
    {
        DebugPrintf(SV_LOG_DEBUG, "Opening file: %s\n", fileName.c_str());
        DebugPrintf(SV_LOG_DEBUG, "Appending string: %s\n", command.c_str());
        fileStream << command << endl;
        if (fileStream.good())
        {
            DebugPrintf(SV_LOG_DEBUG, "fabricagent::Append operation: SUCCESS\n");
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "fabricagent::Append operation: FAILED with errno = %d\n", errno);
            if (fileStream.is_open()) 
                fileStream.close();
            return false;
        }
        fileStream.close();
        return true;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR,"fabricagent::Append Opening %s Failed\n", fileName.c_str());
        return false;
    }
}

bool fabricagent::CheckScstGroup(string groupName)
{
    struct stat statbuf;
    string groupPath = string(SCSI_TGT_GROUPS_PATH) + string(PATH_SEPERATOR_STR) +
        groupName;

    DebugPrintf(SV_LOG_DEBUG,"fabricagent::CheckScstGroup group[%s] path: %s\n", groupName.c_str(), groupPath.c_str()); 

    if( -1 ==  stat(groupPath.c_str(), &statbuf))
    {
        DebugPrintf(SV_LOG_DEBUG,"fabricagent::CheckScstGroup group[%s] doesn't exist.\n", groupName.c_str()); 
        return false;
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG,"fabricagent::CheckScstGroup group[%s] present already\n", groupName.c_str()); 
        return true;
    }
}

bool fabricagent::CreateScstGroup(string groupName)
{
    string scstPath = string(SCSI_TGT_PATH) + string(PATH_SEPERATOR_STR) + 
        string(SCSI_TGT_KEYWORD);
    string addGrpCommand = string(ADD_GROUP_KEYWORD) + string(" ") + groupName;
    if (Append(scstPath, addGrpCommand))
    {
        DebugPrintf(SV_LOG_DEBUG,"Add Group: %s SUCCESS\n", groupName.c_str());
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR,"Failed!!!, Add Group: %s\n", groupName.c_str());
        return false;
    }

    return true;
}

void fabricagent::DeleteScstGroup(string groupName)
{
    string scsiTgtPath = string("/proc/scsi_tgt/groups/") + groupName + string("/names");
    string command = "clear";

    if (Append(scsiTgtPath, command))
    {
        DebugPrintf(SV_LOG_DEBUG, "clear names on group[%s] SUCCESS\n",
            scsiTgtPath.c_str());
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Failed!!!, clear names on group[%s]\n",
            scsiTgtPath.c_str());
    }
    scsiTgtPath = "/proc/scsi_tgt/scsi_tgt";
    command = string("del_group ") + groupName;

    if (Append(scsiTgtPath, command))
    {
        DebugPrintf(SV_LOG_DEBUG, "DeleteScstGroup del_group on group[%s] SUCCESS\n",
            groupName.c_str());
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Failed!!!, DeleteScstGroup del_group on group[%s]\n",
            groupName.c_str());
    }
}

bool fabricagent::CheckWwnInScstGroup(string groupName, string wwn)
{
    string namesFile = string(SCSI_TGT_GROUPS_PATH) +
        string(PATH_SEPERATOR_STR) + 
        groupName + string(PATH_SEPERATOR_STR) +
        string(NAMES_KEYWORD);

    ifstream results(namesFile.c_str(), ifstream::in);
    string field1;

    do {
        field1.clear();
        results >> field1;
        if (field1 == "")
            break;
        if (field1 == wwn)
        {
            return true;
        }
    } while (!results.eof() && results.good());

    results.close();
    return false;
}


bool fabricagent::AddWwnToScstGroup(string groupName, string wwn)
{
    string scsiGrpNamesPath = string(SCSI_TGT_GROUPS_PATH) +
        string(PATH_SEPERATOR_STR) + 
        groupName + string(PATH_SEPERATOR_STR) +
        string(NAMES_KEYWORD);

    string scsiGrpNamesAdd = string(ADD_KEYWORD) + string(" ") + wwn;

    if (Append(scsiGrpNamesPath, scsiGrpNamesAdd))
    {
        DebugPrintf(SV_LOG_DEBUG,"Add Name[%s] to group[%s]: SUCCESS\n",
            wwn.c_str(), groupName.c_str());
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Failed!!!, Add Name[%s] to group[%s]\n",
            wwn.c_str(), groupName.c_str());
        return false;
    }

    string scsiTgtCtrlPath = string(SCSI_TGT_CTRL_PATH);

    if (wwn.find("iqn") != string::npos)
    {
        scsiTgtCtrlPath=string(ISCSI_TGT_CTRL_PATH);
    }

    ofstream fileStream(scsiTgtCtrlPath.c_str(), ofstream::out);
    if (fileStream.good())
    {
	    string scsiTgtCtrlAdd = string(UNREGISTER_KEYWORD) + string(" ") + wwn;

	    if (Append(scsiTgtCtrlPath, scsiTgtCtrlAdd))
	    {
		DebugPrintf(SV_LOG_DEBUG,"Add Name[%s] to file[%s]: SUCCESS\n",
		    wwn.c_str(), scsiTgtCtrlPath.c_str());
	    }
	    else
	    {
		if (wwn.find("iqn") != string::npos)
		{
		       return true;// Return true if it is iscsi export and session is not formed with backend.
		}
		DebugPrintf(SV_LOG_ERROR, "Failed!!!, Add Name[%s] to file[%s]\n",
		    wwn.c_str(), scsiTgtCtrlPath.c_str());
		RemoveWwnFromScstGroup(groupName, wwn);
		return false;
	    }
    }
    return true;
}

bool fabricagent::RemoveWwnFromScstGroup(string groupName, string wwn)
{
    DebugPrintf(SV_LOG_DEBUG,"RemoveWwnFromScstGroup: Removing name[%s] from group[%s]\n",
        wwn.c_str(), groupName.c_str());

    string scsiGrpNamesPath = string(SCSI_TGT_GROUPS_PATH) +
        string(PATH_SEPERATOR_STR) + 
        groupName + string(PATH_SEPERATOR_STR) +
        string(NAMES_KEYWORD);

    string scsiGrpNamesDel = string(DEL_KEYWORD) + string(" ") + wwn;

    if (Append(scsiGrpNamesPath, scsiGrpNamesDel))
    {
        DebugPrintf(SV_LOG_DEBUG,"Del Name[%s] to group[%s]: SUCCESS\n",
            wwn.c_str(), groupName.c_str());
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Failed!!!, Del Name[%s] to group[%s]\n",
            wwn.c_str(), groupName.c_str());
        return false;
    }

    return true;
}

bool fabricagent::CheckLunInScstGroup(std::string lunName, unsigned long long lunNo, string groupName, unsigned long long & oldLunNo)
{
    char line[512];
    string devNameFile = string("/proc/scsi_tgt/groups/") + groupName + string("/devices");

    ifstream results(devNameFile.c_str(), ifstream::in);
    string field1;
    int field2;

    if (!results.eof() && results.good())
        results.getline(line, sizeof(line));

    do {
        field1.clear();
        results >> field1 >> field2;
        if (field1 == "")
            break;
        // if (field1 == DUMMY_LUN_NAME)
          //  continue;
        if (lunName == field1)
        {
            oldLunNo = field2;
            return true;
        }
    } while (!results.eof() && results.good());

    results.close();
    return false;
}

bool fabricagent::AddLunToScstGroup(std::string lunName, unsigned long long lunNo, string groupName)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "LunName = %s, LunNo = %d, groupName = %s\n", 
        lunName.c_str(), lunNo, groupName.c_str());

    string devicesFile = string(SCSI_TGT_GROUPS_PATH) + string(PATH_SEPERATOR_STR) +
        groupName + string(PATH_SEPERATOR_STR) + 
        string(DEVICES_KEYWORD);
    string addDeviceCommand = string(ADD_KEYWORD) + string(" ") + lunName + string(" ") +
        boost::lexical_cast<std::string>(lunNo);
    if (Append(devicesFile, addDeviceCommand))
    {
        DebugPrintf(SV_LOG_INFO,"fabricagent::AddLunToScstGroup add device[%s] on group[%s] SUCCESS\n",
            lunName.c_str(), groupName.c_str());
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR,"Failed!!!, fabricagent::AddLunToScstGroup add device[%s] on group[%s]\n",
            lunName.c_str(), groupName.c_str());
        return false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", FUNCTION_NAME);
    return true;
}

bool fabricagent::RemoveLunFromScstGroup(std::string lunName, unsigned long long LunNo, std::string groupName)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "lunName = %s, LunNo = %d, groupName = %s\n",
        lunName.c_str(), LunNo, groupName.c_str());

    string devicesFile = string(SCSI_TGT_GROUPS_PATH) + string(PATH_SEPERATOR_STR) +
        groupName + string(PATH_SEPERATOR_STR) + 
        string(DEVICES_KEYWORD);

    string delDeviceCommand = string(DEL_KEYWORD) + string(" ") + lunName + string(" ") +
        boost::lexical_cast<std::string>(LunNo);

    ofstream fileStream(devicesFile.c_str(), ofstream::out);
    if (fileStream.good())
    {
        DebugPrintf(SV_LOG_DEBUG, "Opening file: %s\n", devicesFile.c_str());
        DebugPrintf(SV_LOG_DEBUG, "Appending string: %s\n", delDeviceCommand.c_str());
        fileStream << delDeviceCommand << endl;
        if (fileStream.good())
        {
            DebugPrintf(SV_LOG_DEBUG, "fabricagent::RemoveLunFromScstGroup append operation: SUCCESS\n");
        }
        else
        {
            if (errno != EINVAL)
            {
                DebugPrintf(SV_LOG_ERROR,"Failed!!!, fabricagent::RemoveLunFromScstGroup del device[%s] on group[%s]\n",
                    lunName.c_str(), groupName.c_str());
                if (fileStream.is_open())
                    fileStream.close();
                return false;
            }
        }
        DebugPrintf(SV_LOG_INFO,"fabricagent::RemoveLunFromScstGroup del device[%s] on group[%s] SUCCESS\n",
            lunName.c_str(), groupName.c_str());
        fileStream.close();
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", FUNCTION_NAME);
    return true;
}

bool fabricagent::RemoveLunFromAllScstGroups(string atLunName)
{
    bool status = true;
    char line[512];
    struct dirent *pDirent;
    DebugPrintf(SV_LOG_DEBUG, "Entering : %s\n", FUNCTION_NAME);
    do 
    {
        string dirName = "/proc/scsi_tgt/groups";
        DIR *dir = opendir(dirName.c_str());
        if (NULL == dir)
        {
            DebugPrintf(SV_LOG_ERROR, "%s /proc/scsi_tgt/groups does not exist.\n", FUNCTION_NAME);
            status = false;
            break;
        }
        while ((pDirent=readdir(dir)) != NULL)
        {
            string entName = string(pDirent->d_name);
            if((entName == ".") || (entName == ".."))
                continue;

            string devNameFile = string("/proc/scsi_tgt/groups/") + entName + string("/devices");

            ifstream results(devNameFile.c_str(), ifstream::in);
            string field1;
            int field2;

            if (!results.eof() && results.good())
                results.getline(line, sizeof(line));

            do {
                field1.clear();
                results >> field1 >> field2;
                if (field1 == "")
                    break;
                if (field1 == DUMMY_LUN_NAME)
                    continue;
                if (atLunName == field1)
                {
                    if (!RemoveLunFromScstGroup(field1, field2, entName))
                        status=false;
                }
            } while (!results.eof() && results.good());
        }

        if (closedir(dir) == -1)
        {
            DebugPrintf(SV_LOG_ERROR, "closedir FAILED\n");
        }
        else
            DebugPrintf(SV_LOG_INFO, "closedir SUCCESS\n");
    } while (false);
    DebugPrintf(SV_LOG_DEBUG, "Exiting : %s\n", FUNCTION_NAME);
    return status;
}

void fabricagent::CleanUpEmptyScstGroups()
{
    struct stat statbuf;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    if( ! IsAtLunPresent("DUMMY_LUN_ZERO") )
    {
        // Do nothing in case of non-fabric
    }
    else
    {
        // perform cleanup
        bool isGroupEmpty = true;
        char line[512];
        struct dirent *pDirent;
        do
        {
            string dirName = "/proc/scsi_tgt/groups";
            DIR *dir = opendir(dirName.c_str());
            if (NULL == dir)
            {
                DebugPrintf(SV_LOG_ERROR, "%s : /proc/scsi_tgt/groups is not present. \n", FUNCTION_NAME);
                break;
            }
            while ((pDirent=readdir(dir)) != NULL)
            {
                string entName = string(pDirent->d_name);
                if((entName == ".") || (entName == "..") || (entName == "Default"))
                    continue;

                string devNameFile = string("/proc/scsi_tgt/groups/") + entName + string("/devices");

                ifstream results(devNameFile.c_str(), ifstream::in);
                string field1;
                int field2;

                if (!results.eof() && results.good())
                    results.getline(line, sizeof(line));

                isGroupEmpty = true;
                do {
                    field1.clear();
                    results >> field1 >> field2;
                    if (field1 == "")
                        break;
                    if (field1 == DUMMY_LUN_NAME)
                        continue;
                    isGroupEmpty = false;
                } while (!results.eof() && results.good());

                results.close();

                if (isGroupEmpty)
                {
                    RemoveLunFromScstGroup(DUMMY_LUN_NAME, 0, entName);
                    DeleteScstGroup(entName);
                }
            }

            if (closedir(dir) == -1)
            {
                DebugPrintf(SV_LOG_ERROR, "closedir FAILED\n");
            }
            else
                DebugPrintf(SV_LOG_INFO, "closedir SUCCESS\n");
        } while(false);

    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", FUNCTION_NAME);
}

void fabricagent::CleanupScst()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    if( ! IsAtLunPresent("DUMMY_LUN_ZERO") )
    {
        // perform cleanup
        char line[512];
        struct dirent *pDirent;
        do
        {
            string dirName = "/proc/scsi_tgt/groups";
            DIR *dir = opendir(dirName.c_str());
            if (NULL == dir)
            {
                DebugPrintf(SV_LOG_ERROR, "%s : /proc/scsi_tgt/groups is not present.\n", FUNCTION_NAME);
                break;
            }
            while ((pDirent=readdir(dir)) != NULL)
            {
                string entName = string(pDirent->d_name);
                if((entName == ".") || (entName == "..")) 
                    continue;

                if(entName == "Default")
                {
                    string scsiTgtPath = string("/proc/scsi_tgt/groups/") + entName + string("/names");
                    string command = "clear";

                    if (Append(scsiTgtPath, command))
                    {
                        DebugPrintf(SV_LOG_DEBUG, "clear names on group[%s] SUCCESS\n",
                                scsiTgtPath.c_str());
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_ERROR, "clear names on group[%s] FAILED\n",
                                scsiTgtPath.c_str());
                    }

                    scsiTgtPath = string("/proc/scsi_tgt/groups/") + entName + string("/devices");

                    if (Append(scsiTgtPath, command))
                    {
                        DebugPrintf(SV_LOG_DEBUG, "clear devices on group[%s] SUCCESS\n",
                                scsiTgtPath.c_str());
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_ERROR, "clear devices on group[%s] FAILED\n",
                                scsiTgtPath.c_str());
                    }

                    continue;
                }

                DebugPrintf(SV_LOG_DEBUG, "Deleting SCST group %s\n", entName.c_str());

                ACE_OS::sleep(3);
                DeleteScstGroup(entName.c_str());
            }

            if (closedir(dir) == -1)
            {
                DebugPrintf(SV_LOG_ERROR, "closedir FAILED\n");
            }
            else
                DebugPrintf(SV_LOG_INFO, "closedir SUCCESS\n");
        } while (false);

    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", FUNCTION_NAME);
}

void fabricagent::CleanupATLuns()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    if( ! IsAtLunPresent("DUMMY_LUN_ZERO") )
    {
        // Do nothing in case of non-fabric
    }
    else
    {
        LocalConfigurator localConfigurator;
        std::string agentInstallPath = localConfigurator.getInstallPath();
        std::string cmd;
        cmd += agentInstallPath;
        cmd += "bin/inm_dmit --get_protected_volume_list";
        std::stringstream results;

        DebugPrintf(SV_LOG_DEBUG,"fabricagent::CleanupATLuns cmd = %s \n", cmd.c_str());

        if (!executePipe(cmd, results)) {
            DebugPrintf(SV_LOG_ERROR,"fabricagent::CleanupATLuns result = %s \n", results.str().c_str());
            return;
        }

        DebugPrintf(SV_LOG_DEBUG,"fabricagent::CleanupATLuns result = %s \n", results.str().c_str());

        if(results.good())
        {
            std::string atLunName;
            while(std::getline(results, atLunName))
            {
                if (!DeleteATLun(atLunName))
                {
                    DebugPrintf(SV_LOG_ERROR, "failed to delete ATLun[%s]\n", atLunName.c_str());
                }
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", FUNCTION_NAME);
}

int fabricagent::AddPTPath(string atLunName, string PTPath)
{
    if (PTPath.length() == 0)
    {
        DebugPrintf(SV_LOG_DEBUG,"AddPTPath: no PT Path added for LUN %s\n", atLunName.c_str());
        return 0;
    }

    if (SetATLunAttr(atLunName, "VolumePTPath", PTPath))
    {
        DebugPrintf(SV_LOG_DEBUG,"AddPTPath: %s  %s SUCCESS\n", atLunName.c_str(), PTPath.c_str());
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR,"AddPTPath  %s  %s FAILED\n", atLunName.c_str(), PTPath.c_str());
        return 1;
    }

    return 0;
}

bool fabricagent::IsAtLunPresent(const std::string atLunName)
{
    DebugPrintf(SV_LOG_DEBUG,"fabricagent::IsAtLunPresent Entered\n");
    LocalConfigurator localConfigurator;
    std::string agentInstallPath = localConfigurator.getInstallPath();
    std::string cmd;
    cmd += agentInstallPath;
    cmd += "bin/inm_dmit --get_protected_volume_list";
    std::stringstream results;

    DebugPrintf(SV_LOG_DEBUG,"fabricagent::IsAtLunPresent cmd = %s \n", cmd.c_str());

    if (!executePipe(cmd, results)) {
        DebugPrintf(SV_LOG_ERROR,"fabricagent::IsAtLunPresent result = %s \n", results.str().c_str());
        return false;
    }

    DebugPrintf(SV_LOG_DEBUG,"fabricagent::IsAtLunPresent result = %s \n", results.str().c_str());
    if(results.good())
    {
        std::string volumeName;
        while(std::getline(results,volumeName))
        {
            if ( volumeName.compare(atLunName) == 0)
            {
                return true;
            }
        }
    }
    return false;
}

bool fabricagent::ClearDiffsOnATLun(const std::string atLunName)
{
    DebugPrintf(SV_LOG_DEBUG,"fabricagent::ClearDiffsOnATLun Entered\n");
    LocalConfigurator localConfigurator;
    std::string agentInstallPath = localConfigurator.getInstallPath();
    std::string cmd;
    cmd += agentInstallPath;
    cmd += "bin/inm_dmit --op=clear_diffs " + atLunName;
    std::stringstream results;

    DebugPrintf(SV_LOG_DEBUG,"fabricagent::ClearDiffsOnATLun cmd = %s \n", cmd.c_str());

    if (!executePipe(cmd, results)) {
        DebugPrintf(SV_LOG_ERROR,"fabricagent::ClearDiffsOnATLun result = %s \n", results.str().c_str());
        return false;
    }

    DebugPrintf(SV_LOG_DEBUG,"fabricagent::ClearDiffsOnATLun result = %s \n", results.str().c_str());

    if(results.good())
    {
        return true;
    }
    return false;
}

bool fabricagent::GetATLunAttr(const std::string atLunName, const std::string attrName, std::string & attrValue)
{
    DebugPrintf(SV_LOG_DEBUG,"fabricagent::GetATLunAttr Entered\n");
    LocalConfigurator localConfigurator;
    std::string agentInstallPath = localConfigurator.getInstallPath();
    std::string cmd;
    cmd += agentInstallPath;
    cmd += "bin/inm_dmit --get_attr volume " + atLunName + " " + attrName;
    std::stringstream results;

    DebugPrintf(SV_LOG_DEBUG,"fabricagent::GetATLunAttr cmd = %s \n", cmd.c_str());

    if (!executePipe(cmd, results)) {
        DebugPrintf(SV_LOG_ERROR,"fabricagent::GetATLunAttr result = %s \n", results.str().c_str());
        return false;
    }

    DebugPrintf(SV_LOG_DEBUG,"fabricagent::GetATLunAttr result = %s \n", results.str().c_str());

    if(results.good())
    {
        results >> attrValue;
        return true;
    }
    return false;
}

bool fabricagent::SetATLunAttr(const std::string atLunName, const std::string attrName, std::string attrValue)
{
    DebugPrintf(SV_LOG_DEBUG,"fabricagent::SetATLunAttr Entered\n");
    LocalConfigurator localConfigurator;
    std::string agentInstallPath = localConfigurator.getInstallPath();
    std::string cmd;
    cmd += agentInstallPath;
    cmd += "bin/inm_dmit --set_attr volume " + atLunName + " " + attrName + " " + attrValue;
    std::stringstream results;

    DebugPrintf(SV_LOG_DEBUG,"fabricagent::GetATLunAttr cmd = %s \n", cmd.c_str());

    if (!executePipe(cmd, results)) {
        DebugPrintf(SV_LOG_ERROR,"fabricagent::GetATLunAttr result = %s \n", results.str().c_str());
        return false;
    }

    DebugPrintf(SV_LOG_DEBUG,"fabricagent::GetATLunAttr result = %s \n", results.str().c_str());

    if(results.good())
    {
        return true;
    }
    return false;
}
