#include "common.h"
#include "ServiceManager.h"

#define RUN_JOBS "\"perl C:\\home\\svsystems\\bin\\systemjobs.pl run_jobs\""

using namespace std;

LPSTR CX_SERVICE_NAME ="sysjobsvc";
LPSTR CX_SERVICE_DISPLAY_NAME = "InMage System Job Service";
LPSTR CX_SERVICE_DESCRIPTION = "InMage System Job Service for Windows Cx";

int SpawnAProcess(const char* pszImageName);
extern void LogEvent( LPCTSTR pFormat, ... );

extern HANDLE ghSvcStopEvent;
extern SERVICE_STATUS          gSvcStatus;
extern SERVICE_STATUS_HANDLE ServiceStatusHandle;
extern CRITICAL_SECTION g_cs;
extern ServiceManager svcMgr;
ServiceManager::ServiceManager(void)
{
  m_bStopFlag = false;
}
void ServiceManager::SetStopFlag()
{
  //EnterCriticalSection(&g_cs);
  m_bStopFlag = true;
  //LeaveCriticalSection(&g_cs);
}
ServiceManager::~ServiceManager(void)
{
}

int ServiceManager::CommandLineHelp()
{
  int nRet = ERROR_USE_PROPER_COMMANDLINE_ARGS;
  cout<<endl<<"\tCxService.exe [{-install -domain <domain name>\
			  -user <username> -password <pswd>}\
			  | -uninstall |-spawn <path and name of binary>]"<<endl;
  cout<<endl<<"\t\t-install  :Install the service."<<endl;
  cout<<endl<<"\t\t-uninstall:Un-Install the service."<<endl;
  cout<<endl<<"\t\t-spawn     :Spawn the binary from the given path."<<endl;
  return nRet;
}
BOOL ServiceManager::IsInstalled()
{
    BOOL bResult = FALSE;

    SC_HANDLE hSCMagager = ::OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );

    if ( hSCMagager != NULL )
    {
        SC_HANDLE hSchService = OpenService(hSCMagager, CX_SERVICE_NAME, SERVICE_QUERY_CONFIG);
        if ( hSchService != NULL )
        {
            bResult = TRUE;
            ::CloseServiceHandle( hSchService );
        }
        ::CloseServiceHandle( hSCMagager );
    }
    return bResult;
}
void ServiceManager::SvcInit(DWORD argc, LPTSTR *argv)
{
    // Create an event. The control handler function, SvcCtrlHandler,
    // signals this event when it receives the stop control code.
    ghSvcStopEvent = CreateEvent(
                         NULL,    // default security attributes
                         TRUE,    // manual reset event
                         FALSE,   // not signaled
                         NULL);   // no name

    if ( ghSvcStopEvent == NULL)
    {
        ReportSvcStatus( SERVICE_STOPPED, NO_ERROR, 0 );
        return;
    }
    // Report running status when initialization is complete.
    ReportSvcStatus( SERVICE_RUNNING, NO_ERROR, 0 );
	//Keep Spawning the same process until the service stops.
	DWORD dwThreadId = 0;
	DWORD dwWait = 0;
	DWORD dwError = 0;
	int nRet = SpawnAProcess(RUN_JOBS);
	if(nRet == 0) 
	  {
		LogEvent("Successfully spawned the process RUN_JOBS");
	  }
	  else
	  {
		LogEvent("Failed to spawn the process RUN_JOBS");
	  }
	/*do{
	  Sleep(10000);
	}while(!m_bStopFlag);*/
	dwWait = WaitForSingleObject(ghSvcStopEvent,INFINITE);
	char buffer_1[ ] = "C:\\home\\svsystems\\var\\system_job.pid"; 
    char *lpStr1;
    lpStr1 = buffer_1;
	if(WAIT_OBJECT_0 == dwWait)
	{
		BOOL bIsProcTerminated = TerminateProcess(svcMgr.pi.hProcess,3000);
		if(PathFileExists(lpStr1)) {
			if(DeleteFile(lpStr1)) {
				LogEvent("Successfully Deleted system_job.pid file");
			}else {
				LogEvent("Failed to Deleted system_job.pid file");
			}
		}else {
			LogEvent("system_job.pid file does not exists");
		}
	}
	    
	ReportSvcStatus( SERVICE_STOPPED, NO_ERROR, 0 );
	
    return;
}

VOID ServiceManager::ReportSvcStatus( DWORD dwCurrentState,
                      DWORD dwWin32ExitCode,
                      DWORD dwWaitHint)
{
    static DWORD dwCheckPoint = 1;

    gSvcStatus.dwCurrentState = dwCurrentState;
    gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
    gSvcStatus.dwWaitHint = dwWaitHint;

    if (dwCurrentState == SERVICE_START_PENDING)
	{
        gSvcStatus.dwControlsAccepted = 0;
	}
    else
	{
	  gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
	}

    if ( (dwCurrentState == SERVICE_RUNNING) ||
           (dwCurrentState == SERVICE_STOPPED) )
	{
        gSvcStatus.dwCheckPoint = 0;
	}
    else
	{
	  gSvcStatus.dwCheckPoint = dwCheckPoint++;
	}

    // Report the status of the service to the SCM.
    SetServiceStatus( ServiceStatusHandle, &gSvcStatus );
}

bool ServiceManager::Install()
{
    bool bRet ;
	DWORD dwErr = 0;
    if ( IsInstalled() )
    {
        bRet = true;
    }
    else
    {
        SC_HANDLE hSCManager = ::OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );
        if ( hSCManager == NULL )
        {
            LogEvent("Could not open SCManager");
            bRet = false;
        }
        else
        {
            TCHAR szFilePath[_MAX_PATH];
            ::GetModuleFileName( NULL, szFilePath, _MAX_PATH );
			std::string strDomUserName = "";
			std::string strSeparator = "\\";
			strDomUserName = m_strServiceDomainName + strSeparator + m_strServiceUserName;

            SC_HANDLE hService = ::CreateService(
                hSCManager, CX_SERVICE_NAME, CX_SERVICE_DISPLAY_NAME,
                SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
                SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
                szFilePath, NULL, NULL, NULL, NULL/*strDomUserName.c_str()*/, ""/*m_strServicePassword.c_str()*/ );

            if ( hService == NULL )
            {
			    dwErr = GetLastError();
				cout<<endl<<"GetLastError = "<<dwErr<<endl;
				LogEvent("Service Creation failed...");
                ::CloseServiceHandle( hSCManager );
                bRet = false;
            }
            else
            {
                SERVICE_DESCRIPTION ServiceDescription = { CX_SERVICE_DESCRIPTION };
                BOOL bVal = ChangeServiceConfig2( hService, SERVICE_CONFIG_DESCRIPTION, &ServiceDescription );
                ::CloseServiceHandle( hService );
                ::CloseServiceHandle( hSCManager );
				bVal == TRUE?bRet=true :bRet = false;
            }
        }
    }
    return bRet ;
}

bool ServiceManager::UnInstall()
{
    if ( !IsInstalled() )
        return true;
    stopService();
    if ( deleteService() )
        return true;

    return false;
}
bool ServiceManager::stopService()
{
	DWORD dwErr = 0;
    SC_HANDLE hSCManager = ::OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );
	LogEvent("Inside Stop Service");
    if ( hSCManager == NULL )
    {
	  dwErr = GetLastError();
	  cout<<endl<<"GetLastError = "<<dwErr<<endl;
        return false;
    }
    SC_HANDLE hService = OpenService( (SC_HANDLE)hSCManager, CX_SERVICE_NAME, SERVICE_STOP | DELETE );

    if ( hService == NULL )
    {
	  dwErr = GetLastError();
	  cout<<endl<<"GetLastError = "<<dwErr<<endl;
        ::CloseServiceHandle( hSCManager );
        return false;
    }
	
    SERVICE_STATUS status;
    ::ControlService( hService, SERVICE_CONTROL_STOP, &status );
    
    ::CloseServiceHandle( hService );
    ::CloseServiceHandle( hSCManager );

    return true;
}


bool ServiceManager::deleteService()
{
	DWORD dwErr = 0;
    SC_HANDLE hSCManager = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

    if ( hSCManager == NULL )
    {
        return FALSE;
    }
    SC_HANDLE hService = OpenService( (SC_HANDLE)hSCManager, CX_SERVICE_NAME, SERVICE_STOP | DELETE );

    if ( hService == NULL )
    {
	  dwErr = GetLastError();
	  cout<<endl<<"GetLastError = "<<dwErr<<endl;
        ::CloseServiceHandle( hSCManager );
        return FALSE;
    }

    BOOL bDelete = ::DeleteService( hService );
    ::CloseServiceHandle( hService );
    ::CloseServiceHandle( hSCManager );

    bool bRet = bDelete == TRUE? true: false;
    return bRet;
}

bool ServiceManager::GetFlagStatus()
{
	return m_bStopFlag;
}
int SpawnAProcess(const char* pszImageName)
{
  int nRet = 0;
  DWORD dwWait = 0;
  if(pszImageName)
  {
	//Create a separate thread and spawn the given process from the specified path
	//Let the thread wait for the exit value of the process.
	HANDLE pHandles[2] = {0, 0};	
	svcMgr.InitializeProcessHandles();
	int i = 0;

	BOOL bIsProcCreated = CreateProcess(0,
										(LPSTR)RUN_JOBS,
										  0,
										  0,
										  0,
										  0,
										  0,
										  0,
										  &(svcMgr.si),
										  &(svcMgr.pi));
	
	DWORD dwErr = GetLastError();
	
	/*
	BOOL bIsProcCreated1 = CreateProcess(0,
										(LPSTR)MONITOR_JOBS,
										  0,
										  0,
										  0,
										  0,
										  0,
										  0,
										  &(svcMgr.si1),
										  &(svcMgr.pi1));
	
	DWORD dwErr1 = GetLastError();
	*/
	if(!bIsProcCreated)
	{
	  if((i % 5) == 0)
	  {
		LogEvent("Failed to spawn the process Perlproc.exe. Error = %d",dwErr);
	  }
	  i++;
	}
	else
	{
		//dwWait = WaitForSingleObject(pi.hProcess,INFINITE);
		pHandles[0] = ghSvcStopEvent;
		pHandles[1] = svcMgr.pi.hProcess;
		//pHandles[2] = svcMgr.pi1.hProcess;
		LogEvent("Called Stop");		
		dwWait = WaitForMultipleObjects(3,pHandles,FALSE,INFINITE);
		//if((dwWait - WAIT_OBJECT_0) == 0)//if the Stop event is generated, but Perl process is still running
		LogEvent("PID IS %d",svcMgr.pi.dwProcessId);
		if(true == svcMgr.GetFlagStatus())
		{
			//Stop Services is initiated from Services.msc. So we should kill the perl process
			BOOL bIsProcTerminated = TerminateProcess(svcMgr.pi.hProcess,3000);
			//BOOL bIsProcTerminated1 = TerminateProcess(svcMgr.pi1.hProcess,3000);
			//if(bIsProcTerminated && bIsProcTerminated1)
			if(bIsProcTerminated)
			{
				LogEvent("Since Stop Services is initiated from Services.msc,perl process is terminated forcibly.");
			}
		}
	   LogEvent("Successfully spawned and exited the process Perlproc.exe. Error = %d",dwErr);
	}
  }
  else
  {
	nRet = ERROR_NULL_PROCESS_NAME_GIVEN_TO_SPAWN;
  }
  return nRet;
}

void LogEvent( LPCTSTR pFormat, ... )
{
  TCHAR    chMsg[256] = {0};
	HANDLE  hEventSource;
	LPTSTR  lpszStrings[1];
	va_list pArg;
    va_start(pArg, pFormat);
	_vstprintf( chMsg, pFormat, pArg );
	va_end(pArg);
    lpszStrings[0] = chMsg;

	hEventSource = RegisterEventSource( NULL, CX_SERVICE_NAME );
	if ( hEventSource != NULL ) 
    {
        ReportEvent( hEventSource,
					 EVENTLOG_INFORMATION_TYPE,
					 0,
		      		 0,
					 NULL, 
					 1,
					 0,
					 (LPCTSTR*) &lpszStrings[0],
					 NULL );
		DeregisterEventSource( hEventSource );
	}
} 

void ServiceManager::SetDomainName(const char* pszDomainName)
{
  m_strServiceDomainName = pszDomainName;
}
void ServiceManager::SetUserName(const char* pszUserName)
{
  m_strServiceUserName = pszUserName;
}
void ServiceManager::SetPassword(const char* pszPassword)
{
  m_strServicePassword = pszPassword;
}

HANDLE ServiceManager::GetRunJobsHandle()
{
	return pi.hProcess;
}
HANDLE ServiceManager::GetMonitroJobsHandle()
{
	return pi1.hProcess;
}
void ServiceManager::InitializeProcessHandles()
{
	ZeroMemory( &(svcMgr.si), sizeof(svcMgr.si) );
    (svcMgr.si).cb = sizeof(svcMgr.si);
    ZeroMemory( &(svcMgr.pi), sizeof(svcMgr.pi) );

	//ZeroMemory( &(svcMgr.si1), sizeof(svcMgr.si1) );
    //(svcMgr.si1).cb = sizeof(svcMgr.si1);
    //ZeroMemory( &(svcMgr.pi1), sizeof(svcMgr.pi1) );
}