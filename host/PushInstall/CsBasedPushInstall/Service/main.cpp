#pragma warning (disable :4786)

#include <windows.h>

#include <iostream>

#include <ace/ACE.h>
#include <ace/Init_ACE.h>

#include "../common/version.h"
#include "HandlerCurl.h"
#include "portablehelpersmajor.h"
#include "cdputil.h"
#include "inmsafecapis.h"
#include "logger.h"
#include "remoteapi.h"
#include "PushInstallService.h"
#include "terminateonheapcorruption.h"
#include "securityutils.h"


using namespace PI;

class CopyrightNotice {
public:
	CopyrightNotice()	{ std::cout << "\n" << INMAGE_COPY_RIGHT << "\n\n"; }
};

CopyrightNotice displayCopyrightNotice;

#define PUSHINSTALLER_SERVICE_NAME "InMage PushInstall"
#define PUSHINSTALLER_SERVICE_DISPLAY_NAME "InMage PushInstall"
#define PUSHINSTALLER_SERVICE_DESCRIPTION "PushInstaller service"

PushInstallService service;
SERVICE_STATUS m_ServiceStatus;
SERVICE_STATUS_HANDLE m_ServiceStatusHandle;
bool bRunning = true;

/*
 * LogEvent
 *
 * This function used for event logging.
 *
 * Return none.
*/

void LogEvent( LPCTSTR pFormat, ... )
{
    //DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    TCHAR    chMsg[256];
	HANDLE  hEventSource;
	LPTSTR  lpszStrings[1];
	va_list pArg;
    va_start(pArg, pFormat);
	_vstprintf( chMsg, pFormat, pArg );
	va_end(pArg);
    lpszStrings[0] = chMsg;

	hEventSource = RegisterEventSource( NULL, PUSHINSTALLER_SERVICE_NAME );
	if ( hEventSource != NULL ) 
    {
        ReportEvent( hEventSource, EVENTLOG_INFORMATION_TYPE, 0,
		      		0L, NULL, 1, 0, (LPCTSTR*) &lpszStrings[0],
			    	NULL );
		DeregisterEventSource( hEventSource );
	}
    //DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}



/*
 * stopService()
 *
 * This function will stop the pushinstaller service. 
 *
 * Return   true -- if stop successfull.
            false -- if stop fail.   
*/       

bool stopService()
{
    SC_HANDLE hSCManager = ::OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );

    if ( hSCManager == NULL )
    {
        return FALSE;
    }

	// PR#10815: Long Path support    
    SC_HANDLE hService = SVOpenService( (SC_HANDLE)hSCManager, PUSHINSTALLER_SERVICE_NAME, SERVICE_STOP | DELETE );

    if ( hService == NULL )
    {
        ::CloseServiceHandle( hSCManager );
        return FALSE;
    }

    SERVICE_STATUS status;
    ::ControlService( hService, SERVICE_CONTROL_STOP, &status );
    
    ::CloseServiceHandle( hService );
    ::CloseServiceHandle( hSCManager );

    return TRUE;
}

/*
 * deleteService()
 *
 * This function will delete the pushinstaller service. 
 *
 * Return   true -- if delete successfull.
            false -- if delete fail.   
*/

bool deleteService()
{
    SC_HANDLE hSCManager = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

    if ( hSCManager == NULL )
    {
        return FALSE;
    }

	// PR#10815: Long Path support
    SC_HANDLE hService = SVOpenService( (SC_HANDLE)hSCManager, PUSHINSTALLER_SERVICE_NAME, SERVICE_STOP | DELETE );

    if ( hService == NULL )
    {
        ::CloseServiceHandle( hSCManager );
        return FALSE;
    }

    BOOL bDelete = ::DeleteService( hService );
    ::CloseServiceHandle( hService );
    ::CloseServiceHandle( hSCManager );

    bool bRet = bDelete == TRUE? true: false;       //To avoid compiler warning
    return bRet;
}


/*
* IsInstalled
*
* This function tires to open the "pshInstaller Service", if successfully opened means already existed other wise not existed ..
*
* Return   true -- if service already existed .
false -- if service not existed .
*/

BOOL IsInstalled()
{
	BOOL bResult = FALSE;

	SC_HANDLE hSCMagager = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

	if (hSCMagager != NULL)
	{
		// PR#10815: Long Path support
		SC_HANDLE hSchService = SVOpenService(hSCMagager, PUSHINSTALLER_SERVICE_NAME, SERVICE_QUERY_CONFIG);
		if (hSchService != NULL)
		{
			bResult = TRUE;
			::CloseServiceHandle(hSchService);
		}
		::CloseServiceHandle(hSCMagager);
	}
	return bResult;
}


/*
* Install
*
* This function will perform the actual installation of pushinstaller service.
*
* Return   true -- if istallation successfull or already installed.
false -- if istallation failure.
*/

bool Install(const std::string & serviceUserName, const std::string & servicePasswd)
{

	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	bool bRet;
	if (IsInstalled())
	{
		bRet = TRUE;
	}
	else
	{
		SC_HANDLE hSCManager = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		if (hSCManager == NULL)
		{
			LogEvent(_T("Could not open SCManager"));
			DebugPrintf(SV_LOG_ERROR, "OpenSCManager failed. %d\n", GetLastError());
			bRet = FALSE;
		}
		else
		{
			// Get the executable file path
			TCHAR szFilePath[_MAX_PATH];
			::GetModuleFileName(NULL, szFilePath, _MAX_PATH);

			SC_HANDLE hService = ::CreateService(
				hSCManager, PUSHINSTALLER_SERVICE_NAME, PUSHINSTALLER_SERVICE_DISPLAY_NAME,
				SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
				SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
				szFilePath, NULL, NULL, _T("WINMGMT\0"), serviceUserName.c_str(), servicePasswd.c_str());

			if (hService == NULL)
			{
				LogEvent(_T("Service Creation failed..."));
				DebugPrintf(SV_LOG_ERROR, "Create Service Failed %d\n", GetLastError());
				::CloseServiceHandle(hSCManager);
				bRet = FALSE;
			}
			else
			{
				SERVICE_DESCRIPTION ServiceDescription = { PUSHINSTALLER_SERVICE_DESCRIPTION };
				ChangeServiceConfig2(hService, SERVICE_CONFIG_DESCRIPTION, &ServiceDescription);

				DWORD dwErrorServiceFailureActions = SetServiceFailureActionsRecomended(hService);
				if (ERROR_SUCCESS != dwErrorServiceFailureActions)
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to set service recovery options for the service - %s. Error = %lu \n", PUSHINSTALLER_SERVICE_NAME, dwErrorServiceFailureActions);
				}

				::CloseServiceHandle(hService);
				::CloseServiceHandle(hSCManager);
				bRet = TRUE;
			}
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
	return bRet;
}

/*
* Uninstall()
*
* This function will perform the uninstallation of pushinstaller service.
*
* Return   true -- if Uninstall successfull.
false -- if Uninstall fail.
*/

bool Uninstall()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	if (!IsInstalled())
		return TRUE;
	stopService();
	service.deletePushInstaller();
	if (deleteService())
		return TRUE;

	return FALSE;
}

/*
* startPushInstaller()
*
* This function will start pushinstaller process (actuall service code).
*
* Return None.
*/

void startPushInstaller()
{
	bRunning = true;
	try{
		if (!CDPUtil::InitQuitEvent()){
			DebugPrintf(SV_LOG_ERROR, "PushInstall Service: Unable to Initialize QuitEvent\n");
		}
	}
	catch (std::string const& e) {
		DebugPrintf(SV_LOG_FATAL, "PushInstall Service: exception %s\n", e.c_str());
	}
	catch (char const* e) {
		DebugPrintf(SV_LOG_FATAL, "PushInstall Service: exception %s\n", e);
	}
	catch (std::exception const& e) {
		DebugPrintf(SV_LOG_FATAL, "PushInstall Service: exception %s\n", e.what());
	}
	catch (...) {
		DebugPrintf(SV_LOG_FATAL, "PushInstall Service: exception...\n");
	}
	service.runPushInstaller();
}

/*
* stopPushInstaller()
*
* This function will stop pushinstaller process (actuall service code).
*
* Return None.
*/

void stopPushInstaller()
{
	if (bRunning == false)
		return;
	service.stopPushInstaller();
	bRunning = false;
}




/*
* ServiceCtrlHandler
*
* This function sets the status of the service based on the given opcode ..
*
* Return none.
*/

void WINAPI ServiceCtrlHandler(DWORD Opcode)
{
	switch (Opcode)
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
		m_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
		m_ServiceStatus.dwCheckPoint = 0;
		m_ServiceStatus.dwWaitHint = 0;
		SetServiceStatus(m_ServiceStatusHandle, &m_ServiceStatus);
		stopPushInstaller();
		break;

	case SERVICE_CONTROL_INTERROGATE:
		break;
	}
	return;
}


/*
* ServiceMain
*
* This function is the starting point of service execution..
*
* Return none.
*/

void WINAPI ServiceMain(DWORD argc, LPTSTR *argv)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	m_ServiceStatus.dwServiceType = SERVICE_WIN32;
	m_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
	m_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	m_ServiceStatus.dwWin32ExitCode = 0;
	m_ServiceStatus.dwServiceSpecificExitCode = 0;
	m_ServiceStatus.dwCheckPoint = 0;
	m_ServiceStatus.dwWaitHint = 0;

	// Registers the function ServiceCtrlHandler() to handle service control requests.

	m_ServiceStatusHandle = RegisterServiceCtrlHandler(PUSHINSTALLER_SERVICE_NAME, ServiceCtrlHandler);
	if (m_ServiceStatusHandle == (SERVICE_STATUS_HANDLE)0)
	{
		LogEvent(_T("Service Controle Handler's Registration failed"));
		return;
	}

	m_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
	m_ServiceStatus.dwCheckPoint = 0;
	m_ServiceStatus.dwWaitHint = 0;

	SetServiceStatus(m_ServiceStatusHandle, &m_ServiceStatus);
	startPushInstaller();
	return;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
}


/*
 * main()
 *
 * This is the entry point of pushinstaller project. 
 *
*/

int main( int argc, char **argv )
{
	try {

		TerminateOnHeapCorruption();
		init_inm_safe_c_apis();
		ACE::init();
		tal::GlobalInitialize();
		remoteApiLib::global_init();
		SetDaemonMode();

		std::string logPath = CsPushConfig::Instance()->logFolder();
		logPath += "\\PushInstallService.log";
		SetLogLevel(CsPushConfig::Instance()->logLevel());
		SetLogFileName(logPath.c_str());
		CDPUtil::InitQuitEvent();

		std::string serviceUserName;
		std::string servicePasswd;

		if (argc > 1)
		{
			switch (*(argv[1] + 1))
			{
			case 'i':
				if (argc < 5)
				{
					DebugPrintf(SV_LOG_ERROR, "additional -u <domain>\\user -p <password> should be provided when -i is specified\n");
					return -1;
				}
				if (argc == 6)
				{
					serviceUserName = argv[3];
					servicePasswd = argv[5];
				}
				else if (argc == 8)
				{
					serviceUserName = argv[7];
					serviceUserName += "\\";
					serviceUserName += argv[3];
					servicePasswd = argv[5];
				}
				Install(serviceUserName, servicePasswd);
				break;
			case 'u':
				Uninstall();
			default:
				return 0;

			}
		}
		SERVICE_TABLE_ENTRY DispatchTable[] = { { PUSHINSTALLER_SERVICE_NAME, ServiceMain }, { NULL, NULL } };
		StartServiceCtrlDispatcher(DispatchTable);

		remoteApiLib::global_exit();
		tal::GlobalShutdown();
	}
	catch (ErrorException & ee) {
		LogEvent("%s", ee.what());
	}
	catch (std::exception & e) {
		LogEvent("%s", e.what());
	}
	catch (...){
		LogEvent("unhandled exception");
	}
	return 0;
}

