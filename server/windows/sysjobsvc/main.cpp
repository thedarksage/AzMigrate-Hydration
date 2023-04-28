#include "ServiceManager.h"
//#include "ProcSpawner.h"
#include "common.h"
#include <iostream>
#include <Windows.h>
#include "inmsafecapis.h"
#include "win32\terminateonheapcorruption.h"

const int ERROR_USE_PROPER_COMMANDLINE_ARGS				=	1001;
const int ERROR_NULL_PROCESS_NAME_GIVEN_TO_SPAWN		= 	1002;
const int ERROR_COULD_NOT_REGISTER_SERVICE_CTRL_HANDLER	=	1003;

CRITICAL_SECTION g_cs;
ServiceManager svcMgr;
SERVICE_STATUS ServiceStatus;
SERVICE_STATUS_HANDLE ServiceStatusHandle;
VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv);
void WINAPI ServiceCtrlHandler( DWORD Opcode );
extern int SpawnAProcess(const char* pszImageName);
void LogEvent( LPCTSTR pFormat, ... );
HANDLE ghSvcStopEvent;
SERVICE_STATUS          gSvcStatus;
using namespace std;

int main(int argc,const char* argv[])
{
	TerminateOnHeapCorruption();

   //calling init funtion of inmsafec library
	init_inm_safe_c_apis();

  int nRet = 0;
  bool bRet = false;
  bool bInstall = false;
  bool bDomain = false;
  bool bUserName = false;
  bool bPassword = false;
  bool bUnInstall = false;
  bool bSpawn = false;
  bool bExecute = false;
  std::string strProcess = "";
  InitializeCriticalSection(&g_cs);
  if(argc > 1)
  {
	do
	{
	  if(argc < 2 || argc > 8)
	  {
		nRet = svcMgr.CommandLineHelp();
		break;
	  }
	  for(int i = 1; i < argc; i++)
	  {
		if(0 == _stricmp(argv[i],"-install"))
		{
		  cout<<endl<<"Installing the Cx Service."<<endl;
		  bInstall = true;
		}
		/*else if(0 == _stricmp(argv[i],"-domain"))
		{
		  svcMgr.SetDomainName(argv[++i]);
		  bDomain = true;
		}
		else if(0 == _stricmp(argv[i],"-username"))
		{
		  svcMgr.SetUserName(argv[++i]);
		  bUserName = true;
		}
		else if(0 == _stricmp(argv[i],"-password"))
		{
		  svcMgr.SetPassword(argv[++i]);
		  bPassword = true;
		}*/
		else if(0 == _stricmp(argv[i],"-uninstall"))
		{
		  cout<<endl<<"Un-Installing the Cx Service."<<endl;
		  bUnInstall = true;
		}
		else if (0 == _stricmp(argv[i],"-spawn"))
		{
		  //cout<<endl<<"Spawning the process for "<<argv[++i]<<endl;
		  strProcess = argv[i];
		  bSpawn = true;
		}
	  }
	  if(bInstall /*&& bDomain && bUserName && bPassword*/ && !bUnInstall && !bSpawn && !bExecute)
	  {
		bRet = svcMgr.Install();
		return((bRet == true)?0:1);
	  }
	  else if(bUnInstall && !bInstall && !bDomain && !bUserName && !bPassword && !bSpawn && !bExecute)
	  {
		bRet = svcMgr.UnInstall();
		return((bRet == true)?0:1);
	  }
	  else if (bSpawn)
	  {
		 nRet = SpawnAProcess(strProcess.c_str());
		 return nRet;
	  }
	  else
	  {
		nRet = svcMgr.CommandLineHelp();
		return nRet;
	  }
	}while(false);
  }
  SERVICE_TABLE_ENTRY DispatchTable[] = 
  {
	{CX_SERVICE_NAME,ServiceMain},
	{NULL,NULL}
  };

  StartServiceCtrlDispatcher(DispatchTable);
  DWORD dwErr = GetLastError();
  cout<<endl<<"GetLastError = "<<dwErr<<endl;
  return nRet;
}

VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv)
{
  do
  {
	LogEvent("Entered ServiceMain");
	ServiceStatus.dwServiceType = SERVICE_WIN32;
	ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
	ServiceStatus.dwControlsAccepted = SERVICE_CONTROL_STOP|SERVICE_CONTROL_SHUTDOWN;
	ServiceStatus.dwWin32ExitCode = 0;
	ServiceStatus.dwServiceSpecificExitCode = 0;
	ServiceStatus.dwCheckPoint = 0;
	ServiceStatus.dwWaitHint = 0;

	//Register ServiceCtrlHandler function to handle service requests
	ServiceStatusHandle = RegisterServiceCtrlHandler(CX_SERVICE_NAME,(LPHANDLER_FUNCTION)ServiceCtrlHandler);
	if(ServiceStatusHandle == (SERVICE_STATUS_HANDLE)0)
	{
	  LogEvent("Service Control Handler's registration has failed.");
	  break;
	}
	ServiceStatus.dwCurrentState = SERVICE_RUNNING;
	ServiceStatus.dwCheckPoint = 0;
	ServiceStatus.dwWaitHint = 0;
	
	SetServiceStatus(ServiceStatusHandle,&ServiceStatus);
	svcMgr.SvcInit(argc,argv);
  }while(false);
}

void WINAPI ServiceCtrlHandler( DWORD Opcode )
{
	HANDLE pHandles[1] = {0};
	pHandles[0] = svcMgr.GetRunJobsHandle();
	//pHandles[2] = svcMgr.GetMonitroJobsHandle();
    switch( Opcode ) 
    { 
        case SERVICE_CONTROL_PAUSE: 
            ServiceStatus.dwCurrentState = SERVICE_PAUSED; 
            break; 
 
        case SERVICE_CONTROL_CONTINUE: 
            ServiceStatus.dwCurrentState = SERVICE_RUNNING; 
            break; 
 
        case SERVICE_CONTROL_SHUTDOWN:
        
        case SERVICE_CONTROL_STOP: 
			SetEvent(ghSvcStopEvent);
			svcMgr.SetStopFlag();
			ServiceStatus.dwWin32ExitCode = 0; 
            ServiceStatus.dwCurrentState  = SERVICE_STOPPED; 
            ServiceStatus.dwCheckPoint    = 0; 
            ServiceStatus.dwWaitHint      = 0; 
			WaitForMultipleObjects(1,pHandles,TRUE,INFINITE);
            SetServiceStatus ( ServiceStatusHandle, &ServiceStatus );

			break;
 
        case SERVICE_CONTROL_INTERROGATE: 
            break; 
    }      
    return; 
}

