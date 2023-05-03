#pragma warning (disable :4786)

#include "CXConnection.h"
#include "IRepositoryManager.h"
#include "FileRepManager.h"
#include "SnapshotManager.h"
#include "ExecutionController.h"
#include "ERP.h"
#include <time.h>
#include "queue.h"
#include "ExecutionEngine.h"
#include "Log.h"
#include <list>
#include "svconfig.h"
#include "util.h"
#include <ace/Thread_Mutex.h>
#include <openssl/crypto.h>
#include <ace/OS_NS_Thread.h>
#include <curl/curl.h>
#include <ace/Thread.h>
#include "inmsafecapis.h"
#include "terminateonheapcorruption.h"


#ifdef WIN32
#include <tchar.h>
#else
#include <signal.h>
#endif

#ifdef WIN32
#define AMETHYST_CONF "../etc/amethyst.conf"
#else
#define AMETHYST_CONF "/home/svsystems/etc/amethyst.conf"
#endif

int gMaxCount = 0;
static ACE_Thread_Mutex *lock_cs;

Connection* createConnection()
{
	return new CXConnectionEx;
}

#include <string.h>
#include <stdio.h>

void TestEventGen();
void TestRemoval();

#ifdef WIN32
unsigned long WINAPI TestConnection(void* context);
#endif

void TestConnectionThreads();
void TestConnectionEx();
void TestIdGenerator();

void testDaily();
void testWeekly();
void testRepositoryManager();
string getNewsnapshotId(Connection *con);

void startScheduler();
void stopScheduler();

bool stopService();
bool deleteService();

bool bRunning = true;

inline bool NoQuit() {
	return bRunning;
}

SCH_CONF SCH_CONF::m_conf;

bool SCH_CONF::Initialize()
{
	static bool bShoudlInitialize = true;
	static bool bSuccess = false;
	static Mutex inm_mtx;

	inm_mtx.lock();

	if(bShoudlInitialize) {
		//Restrict Initialization to only once.
		bShoudlInitialize = false;

		

		//Initialize cx-api-url
		m_conf.m_cx_api_url = "http://127.0.0.1:80/ScoutAPI/CXAPI.php"; //Set default url
		SVConfig *config = NULL;
		config = SVConfig::GetInstance();
		if(config && config->Parse(AMETHYST_CONF).succeeded())
		{	
			std::string tmpStr;
			if(config->GetValue("CS_SECURED_COMMUNICATION", tmpStr).succeeded() && tmpStr.compare("1") == 0)
				m_conf.m_bUseHttps = true;
			
			if(config->GetValue("MAX_PROC_COUNT", tmpStr).succeeded())
			{
				gMaxCount = (tmpStr.size() > 0) ? atoi(tmpStr.c_str()) : 100;
			}
			else
			{
				gMaxCount = 100;
			}

			if(config->GetValue("HOST_GUID", tmpStr).succeeded())
			{
				m_conf.m_cx_host_id = tmpStr;
				Logger::Log(0,1," HOST_ID = %s",m_conf.m_cx_host_id.c_str());
			}

			std::string strUlr;

			if(m_conf.m_bUseHttps)
				strUlr = "https://";
			else
				strUlr = "http://";
			
			if(config->GetValue("CS_IP", tmpStr).failed())
				return bSuccess;
			else
				strUlr += tmpStr;
			if(config->GetValue("CS_PORT", tmpStr).failed())
				return bSuccess;
			else
				strUlr += ":" + tmpStr;

			strUlr += "/ScoutAPI/CXAPI.php";
			m_conf.m_cx_api_url = strUlr;

			bSuccess = true;
		}
		Logger::Log(0,1,"API URL [%s]",m_conf.m_cx_api_url.c_str());
	}

	inm_mtx.unlock();

	return bSuccess;
}

bool CheckConnectionHealth()
{
	bool bHealthy = false;

	Logger::Log("main.cpp",__LINE__,0,0,"Checking CS Reachability");

    CXConnectionEx con;

	do {
		DBResultSet lInmrset = con.execQuery("select 1 from hosts limit 1");
		if(!lInmrset.isEmpty() || con.getLastError() == CX_E_SUCCESS) {
			Logger::Log("main.cpp",__LINE__,0,0,"CS is reachable");
			bHealthy = true;
			break;
		}
		sch::Sleep(1000);
	}while(NoQuit());

	return bHealthy;
}

#ifdef WIN32
//----------------------------------Windows Service section-----------------------------//
SERVICE_STATUS m_ServiceStatus;
SERVICE_STATUS_HANDLE m_ServiceStatusHandle;
void WINAPI ServiceMain(DWORD argc, LPTSTR *argv);
void WINAPI ServiceCtrlHandler(DWORD Opcode);
#define SCHEDULER_SERVICE_NAME "INMAGE-AppScheduler"
#define SCHEDULER_SERVICE_DISPLAY_NAME "INMAGE-AppScheduler"
#define SCHEDULER_SERVICE_DESCRIPTION "Application Scheduler"

void WINAPI ServiceMain(DWORD argc, LPTSTR *argv)
{
    m_ServiceStatus.dwServiceType        = SERVICE_WIN32; 
    m_ServiceStatus.dwCurrentState       = SERVICE_START_PENDING; 
    m_ServiceStatus.dwControlsAccepted   = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN; 
    m_ServiceStatus.dwWin32ExitCode      = 0;
    m_ServiceStatus.dwServiceSpecificExitCode = 0;
    m_ServiceStatus.dwCheckPoint         = 0; 
    m_ServiceStatus.dwWaitHint           = 0; 
 
    m_ServiceStatusHandle = RegisterServiceCtrlHandler(SCHEDULER_SERVICE_NAME,ServiceCtrlHandler);
    if (m_ServiceStatusHandle == (SERVICE_STATUS_HANDLE)0) 
    {
        return; 
    }     

    m_ServiceStatus.dwCurrentState       = SERVICE_RUNNING; 
    m_ServiceStatus.dwCheckPoint         = 0; 
    m_ServiceStatus.dwWaitHint           = 0;  

    if (!SetServiceStatus (m_ServiceStatusHandle, &m_ServiceStatus)) 
    { 

    }

	/* startScheduler() is the entry point for scheduling functionality. This routine starts the other threads such as 
	       1. ExecutionController: Polls for the new jobs in certain intervals.
		   3. EventGenerator     : Schedules the jobs.
	   After starting these threads it goes to ExecutionEngine::run() method to initiate the scheduled jobs.
	   
	   Returning from this method indicates the service stop.
	*/

	startScheduler();

	/*
	   Exiting the scheduler.
	*/

	m_ServiceStatus.dwWin32ExitCode = 0; 
    m_ServiceStatus.dwCurrentState  = SERVICE_STOPPED; 
    m_ServiceStatus.dwCheckPoint    = 0; 
    m_ServiceStatus.dwWaitHint      = 0; 
    SetServiceStatus (m_ServiceStatusHandle,&m_ServiceStatus);

	exit(0);

    return; 
}

void WINAPI ServiceCtrlHandler(DWORD Opcode)
{
    switch(Opcode) 
    { 
        case SERVICE_CONTROL_PAUSE: 
            m_ServiceStatus.dwCurrentState = SERVICE_PAUSED; 
            break; 
 
        case SERVICE_CONTROL_CONTINUE: 
            m_ServiceStatus.dwCurrentState = SERVICE_RUNNING; 
            break; 
 
        case SERVICE_CONTROL_SHUTDOWN:
        case SERVICE_CONTROL_STOP: 
			m_ServiceStatus.dwWin32ExitCode = 0; 
            m_ServiceStatus.dwCurrentState  = SERVICE_STOP_PENDING; 
            m_ServiceStatus.dwCheckPoint    = 0; 
            m_ServiceStatus.dwWaitHint      = 0;

			SetServiceStatus (m_ServiceStatusHandle,&m_ServiceStatus);

			stopScheduler();
             
            SetServiceStatus (m_ServiceStatusHandle,&m_ServiceStatus);
			break;
 
        case SERVICE_CONTROL_INTERROGATE: 
            break; 
    }      
    return; 
}

string GetServiceConfigValue(SC_HANDLE hServiceMgr, string szServiceName) 
{ 
    LPQUERY_SERVICE_CONFIG lpqscBuf; 
    LPSERVICE_DESCRIPTION lpqscBuf2;
    DWORD dwBytesNeeded; 
    BOOL bSuccess=TRUE;
 
    // Open a handle to the service. 
 
    SC_HANDLE schService = OpenService( 
        hServiceMgr,           // SCManager database 
        szServiceName.c_str(),     // name of service 
        SERVICE_QUERY_CONFIG);  // need QUERY access 
    if (schService == NULL)
    { 
		return "";
    }
 
    // Allocate a buffer for the configuration information.
 
    lpqscBuf = (LPQUERY_SERVICE_CONFIG) LocalAlloc( 
        LPTR, 4096); 
    if (lpqscBuf == NULL) 
    {
        return "";
    }
 
    lpqscBuf2 = (LPSERVICE_DESCRIPTION) LocalAlloc( 
        LPTR, 4096); 
    if (lpqscBuf2 == NULL) 
    {
        return "";
    }
 
    // Get the configuration information. 
 
    if (! QueryServiceConfig( 
        schService, 
        lpqscBuf, 
        4096, 
        &dwBytesNeeded) ) 
    {
        return "";
    }
 
    // Print the configuration information.
	string szRet = lpqscBuf->lpBinaryPathName;

    LocalFree(lpqscBuf); 
    LocalFree(lpqscBuf2); 

    return szRet;
}

string GetExecutableDir()
{
    // Get the executable file path
    TCHAR szFilePath[_MAX_PATH];
    ::GetModuleFileName(NULL, szFilePath, _MAX_PATH);
    string val = szFilePath;
    val = val.substr(0,val.find_last_of("\\"));
	return val;
}

int IsInstalled()
{
    BOOL bResult = FALSE;

    SC_HANDLE hSCMagager = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

    if (hSCMagager != NULL)
    {
        SC_HANDLE hSchService = ::OpenService(hSCMagager, SCHEDULER_SERVICE_NAME, SERVICE_QUERY_CONFIG);
        if (hSchService != NULL)
        {
            bResult = TRUE;
            ::CloseServiceHandle(hSchService);
        }
        ::CloseServiceHandle(hSCMagager);
    }
    return bResult;
}

bool Install()
{
    if (IsInstalled())
        return TRUE;

    SC_HANDLE hSCManager = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (hSCManager == NULL)
    {
        return FALSE;
    }

    // Get the executable file path
    TCHAR szFilePath[_MAX_PATH];
    ::GetModuleFileName(NULL, szFilePath, _MAX_PATH);

    SC_HANDLE hService = ::CreateService(
        hSCManager, SCHEDULER_SERVICE_NAME, SCHEDULER_SERVICE_DISPLAY_NAME,
        SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
        szFilePath, NULL, NULL, _T("RPCSS\0MySQL\0W3SVC\0"), NULL, NULL);

    if (hService == NULL)
    {
        ::CloseServiceHandle(hSCManager);
        return FALSE;
    }

    SERVICE_DESCRIPTION ServiceDescription = { SCHEDULER_SERVICE_DESCRIPTION };
    if( !ChangeServiceConfig2( hService, SERVICE_CONFIG_DESCRIPTION, &ServiceDescription ) )
    {
        //
        // Ignore failure on setting the description: not a critical error
        //
    }

    ::CloseServiceHandle(hService);
    ::CloseServiceHandle(hSCManager);
    return TRUE;
}

bool Uninstall()
{
    if (!IsInstalled())
        return TRUE;
    
    stopService();
    
    if (deleteService())
        return TRUE;

    return FALSE;
}

bool stopService()
{
    SC_HANDLE hSCManager = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

    if (hSCManager == NULL)
    {
        return FALSE;
    }


    SC_HANDLE hService = ::OpenService((SC_HANDLE)hSCManager, SCHEDULER_SERVICE_NAME, SERVICE_STOP | DELETE);

    if (hService == NULL)
    {
        ::CloseServiceHandle(hSCManager);
        return FALSE;
    }

    SERVICE_STATUS status;
    ::ControlService(hService, SERVICE_CONTROL_STOP, &status);
    
    ::CloseServiceHandle(hService);
    ::CloseServiceHandle(hSCManager);

    return TRUE;
}

bool deleteService()
{
    SC_HANDLE hSCManager = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

    if (hSCManager == NULL)
    {
        return FALSE;
    }

    SC_HANDLE hService = ::OpenService((SC_HANDLE)hSCManager, SCHEDULER_SERVICE_NAME, SERVICE_STOP | DELETE);

    if (hService == NULL)
    {
        ::CloseServiceHandle(hSCManager);
        return FALSE;
    }

    BOOL bDelete = ::DeleteService(hService);
    ::CloseServiceHandle(hService);
    ::CloseServiceHandle(hSCManager);

    bool bRet = bDelete == TRUE? true: false;       //To avoid compiler warning
    return bRet;
}

#else

//
//Unix implementation
//

bool stopService()
{
    return true;
}

bool deleteService()
{
    return true;
}


//--------------------------------------------------------------------------------------//
void cleanup(int signal)
{
	//SnapshotManager *snapmgr = new SnapshotManager();
	//snapmgr->waitForPendingJobs();

	stopScheduler();
}
#endif

//curl global intialization start
ACE_thread_t schThread_id()
{
    return ACE_OS::thr_self();
}

void sch_locking_callback(int mode, int type, const char *file, int line)
{
    if (mode & CRYPTO_LOCK)
    {
        lock_cs[type].acquire();
    }
    else
    {
        lock_cs[type].release();
    }
}

void schThread_cleanup(void)

{
    CRYPTO_set_locking_callback(NULL);
    delete[]lock_cs;
}

void schThread_setup(void)
{
    lock_cs=new ACE_Thread_Mutex[CRYPTO_num_locks()];
    if (0 == CRYPTO_get_locking_callback()) {
        CRYPTO_set_locking_callback((void (*)(int,int,const char *,int))sch_locking_callback);
        CRYPTO_set_id_callback((unsigned long (*)())schThread_id);
    }
    /* id callback defined */
}

void schGlobalInitialize()
{
    curl_global_init(CURL_GLOBAL_ALL);
    schThread_setup();
}

void schGlobalShutdown()
{
    curl_global_cleanup();
    schThread_cleanup();
}

//curl global intialization end
int main(int argc, char **argv)
{
	TerminateOnHeapCorruption();
	//calling init funtion of inmsafec library
	init_inm_safe_c_apis();
	schGlobalInitialize();
	#ifdef WIN32
        if(argc == 2)
        {
            switch(tolower(*(argv[1]+1)))
            {
            case 'i':
                Install();
                break;
            case 'u':
                Uninstall();
            }
            return 0;
        }
		
        SERVICE_TABLE_ENTRY DispatchTable[]={{SCHEDULER_SERVICE_NAME,ServiceMain},{NULL,NULL}};
		StartServiceCtrlDispatcher(DispatchTable);
	#else
		signal(SIGTERM, cleanup);
		startScheduler();
	#endif
	schGlobalShutdown();
	return 0;
}

void startScheduler()
{
	#ifdef WIN32
		SetCurrentDirectory(GetExecutableDir().c_str());
	#endif

    Logger::OpenLog();

	Logger::Log("main.cpp",__LINE__,0,0,"===> Scheduler Started <===");

	SCH_CONF::Initialize();

	Logger::Log(0,1,"API URL [%s]",SCH_CONF::GetCxApiUrl().c_str());

	if(!CheckConnectionHealth())
	{
		Logger::Log("main.cpp",__LINE__,0,0,"===> Scheduler Exiting <===");
		return;
	}

	ExecutionController *ec = ExecutionController::instance();
    
    if(ec == NULL)
    {
        Logger::Log(0,0,"Unable to start ExecutionController. Scheduler start failed");
		Logger::Log("main.cpp",__LINE__,0,0,"===> Scheduler Exiting <===");
        Logger::CloseLog();

        bRunning = false;
        stopService();

        return;
    }

	ExecutionEngine *ee	= ExecutionEngine::instance();

	ee->run();

	Logger::Log("main.cpp",__LINE__,0,0,"===> Scheduler Exiting <===");

	Logger::CloseLog();
}

void stopScheduler()
{
	Logger::Log("main.cpp",__LINE__,0,0,"STOPSCHEDULER");

    if(bRunning == false)
        return;
	else
		bRunning = false;

	ExecutionController *ec = ExecutionController::instance();
    
    if(ec != NULL)
    {
	    ec->stopController();       //Stop Execution Controller and the event generator
    }

    ExecutionEngine *ee = ExecutionEngine::instance(); 

    if(ee != NULL)
    {
	    ee->stopEngine();           //Stop Execution Engine
    }

	Logger::Log("main.cpp",__LINE__,0,0,"STOPSCHEDULER EXIT");
}

