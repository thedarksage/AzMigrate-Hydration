/* 
 * @file frsvc.cpp 
 * 
 * Implements file replication service.that implements file
 * replication. @bug Need some performance counters.
 *
 * Note: Proxy/Stub Information
 * To build a separate proxy/stub DLL, 
 * run nmake -f serviceps.mk in the project directory.
 * 
 * Author				:
 * Revision Information	:
*/

/*******************************Include Headers*******************************/

#include "stdafx.h"	/*defines CServiceModule*/
#include "resource.h"
#include <initguid.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include "hostagenthelpers.h"	/* getagentparamsfromregistry, unregisterhost*/
#include "hostagenthelpers_ported.h"
#include "frsvc_i.h"
#include "frsvc_i.c"
#include <stdio.h>
#include <Windows.h>
#include <winbase.h>
#include "inmmessage.h"
#include <string>
#include <iostream>
#include <fstream>
#include "..\Log\logger.h"
#include "portablehelpersmajor.h"
#include "curlwrapper.h"
#include "terminateonheapcorruption.h"
#include "hostrecoverymanager.h"

/*******************************Constant declarations*************************/

const char * COMMAND_CONTEXT_PASSWORD = "1";
const char * COMMAND_CONTEXT_GROUP = "2";
const char * FILE_PASSWORD = "";
const char * FILE_GROUP = "group";
const char * MASK_CONTEXT = "3";

/****************************Declare Namespaces*******************************/

using namespace std;

/***************************** Global declarations****************************/

char SV_ROOT_KEY[]  = "SOFTWARE\\SV Systems";
CServiceModule _Module;
extern bool bCX_Not_Reachable;

/*****************************Non-Member Function Definitions*****************/

BEGIN_OBJECT_MAP(ObjectMap)
END_OBJECT_MAP()

/*
 * FindOneOf
 *
 * Function find the second string in the first one.
 *
 * p1	pointer to the first string.
 * p2	pointer to the second string.
 *
 * Return Value SVE_FAIL or SVS_OK
*/
LPCTSTR FindOneOf(LPCTSTR Inmp1, LPCTSTR Inmp2)
{
	DebugPrintf( "ENTER FindOneOf()\n");

	while (Inmp1 != NULL && *Inmp1 != NULL) 
	{
		LPCTSTR lInmp = Inmp2;
		while (lInmp != NULL && *lInmp != NULL)
		{
			if (*Inmp1 == *lInmp)
				return CharNext(Inmp1);
			lInmp = CharNext(lInmp);
		}
		Inmp1 = CharNext(Inmp1);
	}

	DebugPrintf( "EXIT FindOneOf()\n" );
	return NULL;
}

//
//SVERROR WriteStringIntoFile(const std::string& fileName,const std::string& str)
//{
//    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
//    SVERROR retValue = SVS_OK;
//    std::ofstream out;
//    out.open(fileName.c_str(),std::ios::app);
//    if (!out.is_open()) 
//    {
//        DebugPrintf(SV_LOG_ERROR,"Failed to createFile %s\n",fileName.c_str());
//    }
//    else
//    {
//        out<<str;
//    }
//    out.close();
//    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
//    return retValue;
//}

/*************************Member Function Definitions*************************/

/*UpdateServer has to be written*/


/*
 * CServiceModule::UpdateJobState
 *
 * This function updates CX with call to OnFRServiceStop.php"
 *
 * status	agent status information to update.
 *
 * Return Value SVE_FAIL or SVS_OK
*/
SVERROR CServiceModule::UpdateJobState()
{
    /* place holder function */
	return SVS_OK;
}

/* Although some of these functions are big they are declared inline since
they are only used once*/

/*
 * CServiceModule::RegisterServer
 *
 * This function registers agent with the CX server.
 *
 * bRegTypeLib	-- > Value indicating if the service is registred type lib.
 *
 * bService		--> Value indicating if the service is started.
 *
 * Return Value SVE_FAIL or SVS_OK
*/
inline HRESULT CServiceModule::RegisterServer(BOOL InmbRegTypeLib, BOOL InmbService)
{
	DebugPrintf("ENTER CServiceModule::RegisterServer()\n");
	// Creating  service
	HRESULT lInmhr = (Install() == TRUE) ? SVS_OK : SVE_FAIL;
	DebugPrintf("EXIT CServiceModule::RegisterServer()\n");
	return lInmhr;
}

/*
 * CServiceModule::RegisterServer
 *
 * This function unregisters the agent from CX server.
 *
 * Return Value SVE_FAIL or SVS_OK
*/
inline HRESULT CServiceModule::UnregisterServer()
{
	DebugPrintf( "ENTER CServiceModule::UnregisterServer()\n" );
	DebugPrintf( "calling Uninstall \n" );
	Uninstall();
	DebugPrintf( "EXIT CServiceModule::UnregisterServer()\n" );
	return S_OK;
}

/*
 * CServiceModule::Init
 *
 * This function unregisters the agent from CX server.
 *
 * Return Value SVE_FAIL or SVS_OK
*/
inline void CServiceModule::Init(_ATL_OBJMAP_ENTRY* Inmp, HINSTANCE Inmh,
		UINT InmnServiceNameID, const GUID* Inmplibid)
{
	DebugPrintf( "ENTER CServiceModule::Init()\n" );

	m_InmbService = TRUE;
	m_bShouldStop = false;

	LoadString(Inmh, InmnServiceNameID, m_InmszServiceName,
		sizeof(m_InmszServiceName) / sizeof(TCHAR));
	LoadString(Inmh, IDS_SERVICELONGNAME, m_InmszServiceLongName,
		sizeof(m_InmszServiceLongName) / sizeof(TCHAR));
	LoadString(Inmh, IDS_SERVICEDESCRIPTION, m_InmszServiceDescription,
		sizeof(m_InmszServiceDescription) / sizeof(TCHAR));

	
	m_InmhServiceStatus = NULL;
	m_Inmstatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	m_Inmstatus.dwCurrentState = SERVICE_STOPPED;
	m_Inmstatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
	m_Inmstatus.dwWin32ExitCode = 0;
	m_Inmstatus.dwServiceSpecificExitCode = 0;
	m_Inmstatus.dwCheckPoint = 0;
	m_Inmstatus.dwWaitHint = 0;

	if( m_agent.Init( NULL ).failed() ) {
		DebugPrintf( "FAILED Couldn't init CFileReplicationAgent\n" );
	}

	DebugPrintf( "EXIT CServiceModule::Init()\n" );
}



/*
 * CServiceModule::IsInstalled
 *
 * This function checks with the SCManager if the FX service is installed.
 *
 * Return Value LONG value of CComModule::Unlock().
*/
BOOL CServiceModule::IsInstalled()
{
	DebugPrintf( "ENTER CServiceModule::IsInstalled()\n" );

	BOOL lInmbres = FALSE;
	SC_HANDLE lInmhSCM = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

	if (lInmhSCM != NULL) {
		// PR#10815: Long Path support
		SC_HANDLE lInmhService = ::SVOpenService(lInmhSCM, m_InmszServiceName,SERVICE_QUERY_CONFIG);
		if (lInmhService != NULL) {
			lInmbres = TRUE;
			::CloseServiceHandle(lInmhService);
		}
		::CloseServiceHandle(lInmhSCM);
	}

	DebugPrintf( "EXIT CServiceModule::IsInstalled()\n" );
	return lInmbres;
}

/*
 * CServiceModule::Install
 *
 * This function uses  SCManager to install FX service.
 *
 * Return Value LONG value of CComModule::Unlock().
*/
inline BOOL CServiceModule::Install()
{
	DebugPrintf( "ENTER CServiceModule::Install()\n" );

	if (IsInstalled())
	{
		return TRUE;
	}

	SC_HANDLE lInmhSCM = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (lInmhSCM == NULL) 
	{
		MessageBox(NULL, _T("Couldn't open service manager"),
			m_InmszServiceName, MB_OK);
		return FALSE;
	}

	
	TCHAR lInmszFilePath[_MAX_PATH];
	::GetModuleFileName(NULL, lInmszFilePath, _MAX_PATH);

	TCHAR lInmquotedPath[_MAX_PATH];
	_stprintf(lInmquotedPath, _T("\"%s\""), lInmszFilePath);

	SC_HANDLE lInmhService = ::CreateService(lInmhSCM, m_InmszServiceName, m_InmszServiceLongName,SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
		SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
		lInmquotedPath, NULL, NULL, _T("RPCSS\0"), NULL, NULL);

	if (lInmhService == NULL) 
	{
		::CloseServiceHandle(lInmhSCM);
		MessageBox(NULL, _T("Couldn't create service"), m_InmszServiceName, MB_OK);
		return FALSE;
	}

	SERVICE_DESCRIPTION ServiceDescription = { m_InmszServiceDescription };
	if( !ChangeServiceConfig2( lInmhService, SERVICE_CONFIG_DESCRIPTION,&ServiceDescription ) ) {
		//
		// Ignore failure on setting the description: not a critical error
		//
	}

	//Set Service recovery options
	if ( SetServiceFailureActionsRecomended ( lInmhService ) ) {
		//
		// Ignore failure on setting service recovery options
		//
	}

	::CloseServiceHandle(lInmhService);
	::CloseServiceHandle(lInmhSCM);
	DebugPrintf( "EXIT CServiceModule::Install()\n" );
	return TRUE;
}

/*
 * CServiceModule::Uninstall
 *
 * This function uses  SCManager to install FX service.
 *
 * Return Value LONG value of CComModule::Unlock().
*/
inline BOOL CServiceModule::Uninstall()
{
	DebugPrintf( "ENTER CServiceModule::Uninstall()\n" );
	if (!IsInstalled())
	{
		return TRUE;
	}

	SV_HOST_AGENT_PARAMS AgentParams;
	ZeroFill( AgentParams );
	SVERROR rc = GetAgentParamsFromRegistry( SV_ROOT_KEY, &AgentParams,
		SV_AGENT_FILE_REPLICATION );
	if( rc.failed() ) {
		DebugPrintf( "FAILED Couldn't get agent params from registry\n" );
		return( FALSE );
	}
	
	SC_HANDLE lInmhSCM = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

	if (lInmhSCM == NULL) {
		MessageBox(NULL, _T("Couldn't open service manager"), m_InmszServiceName,
			MB_OK);
		return FALSE;
	}

	// PR#10815: Long Path support
	SC_HANDLE lInmhService = ::SVOpenService(lInmhSCM, m_InmszServiceName,	SERVICE_STOP | DELETE);

	if (lInmhService == NULL) 
	{
		::CloseServiceHandle(lInmhSCM);
		MessageBox(NULL, _T("Couldn't open service"), m_InmszServiceName, MB_OK);
		return FALSE;
	}
	SERVICE_STATUS lInmstatus;
	::ControlService(lInmhService, SERVICE_CONTROL_STOP, &lInmstatus);

	BOOL lInmbDelete = ::DeleteService(lInmhService);
	::CloseServiceHandle(lInmhService);
	::CloseServiceHandle(lInmhSCM);
	DebugPrintf( "EXIT CServiceModule::Uninstall()\n" );
	if (lInmbDelete)
		return TRUE;

	MessageBox(NULL, _T("Service could not be deleted"), m_InmszServiceName, MB_OK);
	return FALSE;
}

/*
 * CServiceModule::LogEvent
 *
 * This function used for event logging.
 *
 * Return none.
*/
void CServiceModule::LogEvent(LPCTSTR pFormat, ...)
{
	DebugPrintf( "ENTER CServiceModule::LogEvent()\n" );

	TCHAR    lInmchMsg[256];
	HANDLE  lInmhEventSource;
	LPTSTR  lInmlpszStrings[1];
	va_list lInmpArg;

	va_start(lInmpArg, pFormat);
	_vstprintf(lInmchMsg, pFormat, lInmpArg);
	va_end(lInmpArg);

	lInmlpszStrings[0] = lInmchMsg;

	if (m_InmbService) 
	{
	
		lInmhEventSource = RegisterEventSource(NULL, m_InmszServiceName);
		if (lInmhEventSource != NULL)
		{
			
			ReportEvent(lInmhEventSource, EVENTLOG_INFORMATION_TYPE, 0,
				MSG_INMAGE_INFORMATION, NULL, 1, 0, (LPCTSTR*) &lInmlpszStrings[0],
				NULL);
			DeregisterEventSource(lInmhEventSource);
		}
	}
	else
	{
		
		_putts(lInmchMsg);
	}
	DebugPrintf( "EXIT CServiceModule::LogEvent()\n" );
}

/*
 * CServiceModule::LogEvent
 *
 * This function used for service starting.
 *
 * Return none.
*/
inline void CServiceModule::Start()
{
	DebugPrintf( "ENTER CServiceModule::Start()\n" );
	SERVICE_TABLE_ENTRY lInmst[] = {{ m_InmszServiceName, _ServiceMain },
		{ NULL, NULL }
	};
	if (m_InmbService && !::StartServiceCtrlDispatcher(lInmst))
	{
		m_InmbService = FALSE;
	}
	if (m_InmbService == FALSE)
		Run();
	DebugPrintf( "EXIT CServiceModule::Start()\n" );
}

/*
 * CServiceModule::ServiceMain
 *
 * This function used for service starting.
 *
 * Return none.
*/
inline void CServiceModule::ServiceMain(DWORD ,LPTSTR*)
{
	DebugPrintf( "ENTER CServiceModule::ServiceMain()\n" );
	
	m_Inmstatus.dwCurrentState = SERVICE_START_PENDING;
	m_InmhServiceStatus = RegisterServiceCtrlHandler(m_InmszServiceName, _Handler);

	if (m_InmhServiceStatus == NULL) 
	{
		LogEvent(_T("Handler not installed"));
		return;
	}
	//Setting service status
	SetServiceStatus(SERVICE_START_PENDING);
//setting following variables
	m_Inmstatus.dwWin32ExitCode = S_OK;
	m_Inmstatus.dwCheckPoint = 0;
	m_Inmstatus.dwWaitHint = 0;

	//calling Run Funtion
	Run();
//Setting service status stopped
	SetServiceStatus(SERVICE_STOPPED);
	DebugPrintf( "EXIT CServiceModule::ServiceMain()\n" );
	CloseDebug();
}

/*
 * CServiceModule::Handler
 *
 * This function handles various service event messages.
 *
 * Return none.
*/
inline void CServiceModule::Handler(DWORD InmdwOpcode)
{
	
	DebugPrintf( "ENTER CServiceModule::Handler()\n" );
	switch (InmdwOpcode)
	{
	case SERVICE_CONTROL_STOP:
		DebugPrintf( "Received  SERVICE_CONTROL_STOP\n" );
		m_bShouldStop = true;
		UpdateJobState();
		SetServiceStatus(SERVICE_STOP_PENDING);
		//
		// Get ourself to quit
		//
		if(bCX_Not_Reachable==true)
		{
			DebugPrintf( "bCX_Not_Reachable==true \n");
			DebugPrintf( "setting   SERVICE_STOPPED\n" );
			SetServiceStatus(SERVICE_STOPPED);
			m_agent.Shutdown();
			WSACleanup();
		}
		else
		{
			DebugPrintf( "entered PostThreadMessage\n" );
			PostThreadMessage(dwThreadID, WM_QUIT, 0, 0);
		}
		break;
	case SERVICE_CONTROL_PAUSE:
		DebugPrintf( "Received SERVICE_CONTROL_PAUSE\n" );
		break;
	case SERVICE_CONTROL_CONTINUE:
		DebugPrintf( "Received SERVICE_CONTROL_CONTINUE\n" );
		break;
	case SERVICE_CONTROL_INTERROGATE:
		DebugPrintf( "Received SERVICE_CONTROL_INTERROGATE\n" );
		break;
	case SERVICE_CONTROL_SHUTDOWN:
		m_bShouldStop = true;
		DebugPrintf( "Received SERVICE_CONTROL_SHUTDOWN\n" );
		SetServiceStatus(SERVICE_STOP_PENDING);
		PostThreadMessage(dwThreadID, WM_QUIT, 0, 0);
		break;
	default:
		LogEvent(_T("Bad service request"));
	}
	DebugPrintf( "EXIT CServiceModule::Handler()\n" );
}

/*
 * CServiceModule::_ServiceMain
 *
 * This function used for service starting.
 *
 * Return none.
*/
void WINAPI CServiceModule::_ServiceMain(DWORD InmdwArgc, LPTSTR* InmlpszArgv)
{
	DebugPrintf( "ENTER CServiceModule::_ServiceMain()\n" );
	_Module.ServiceMain(InmdwArgc, InmlpszArgv);
	DebugPrintf( "EXIT CServiceModule::_ServiceMain()\n" );
}

/*
 * CServiceModule::_Handler
 *
 * This function used for service starting.
 *
 * Return none.
*/
void WINAPI CServiceModule::_Handler(DWORD InmdwOpcode)
{
	DebugPrintf( "ENTER CServiceModule::_Handler()\n" );
	_Module.Handler(InmdwOpcode); 
	DebugPrintf( "EXIT CServiceModule::_Handler()\n" );
}

/*
 * CServiceModule::SetServiceStatus
 *
 * This function used for setting the agent status at cx.
 *
 * Return none.
*/
void CServiceModule::SetServiceStatus(DWORD InmdwState)
{
	DebugPrintf( "ENTER CServiceModule::SetServiceStatus()\n" );

	m_Inmstatus.dwCurrentState = InmdwState;
	// BoundsChecker: uncomment the line below
	// m_Inmstatus.dwWaitHint = 4000000000;
	::SetServiceStatus(m_InmhServiceStatus, &m_Inmstatus);

	DebugPrintf( "EXIT CServiceModule::SetServiceStatus()\n" );
}

/*
 * CServiceModule::Run
 *
 * This function used for setting the agent status at cx.
 *
 * Return none.
*/
void CServiceModule::Run()
{
	//__asm int 3;
	DebugPrintf( "ENTER CServiceModule::Run()\n" );
	_Module.dwThreadID = GetCurrentThreadId();


	LogEvent(_T("Service started"));
	if (m_InmbService)
		SetServiceStatus(SERVICE_RUNNING);

	WSADATA WsaData;
	ZeroFill( WsaData );

	if( WSAStartup( MAKEWORD( 1, 0 ), &WsaData ) ) {
		DebugPrintf( "FAILED Couldn't start Winsock, err %d\n", WSAGetLastError());
	}

	try {

		while (!m_bShouldStop && HostRecoveryManager::IsRecoveryInProgress(std::bind1st(std::mem_fun(&CServiceModule::QuitRequested), this)))
		{
			DebugPrintf("Recovery is in progress. Wait for 2sec and check recovery status again.\n");
			//
			// Wait for 2sec and check recovery progress again.
			//
			ACE_OS::sleep(2);
		}

		//
		// If its a test failover VM then exit the service.
		//
		if (m_bShouldStop)
		{
			DebugPrintf("Stop signal has recieved. Service will be stoped\n");
		}
		else if (IsItTestFailoverVM())
		{
			DebugPrintf("This is a Test Failover Machine. Service will be stoped\n");
		}
		else
		{
			m_agent.Run();
			m_agent.Shutdown();
		}
	}
	catch (const HostRecoveryException& exp)
	{
		DebugPrintf("Caught exception. Service will be stoped. Exception details: %s\n", exp.what());
	}
	catch (const std::exception& exp)
	{
		DebugPrintf("Caught exception. Service will be stoped. Exception details: %s\n", exp.what());
	}
	catch (const std::string& exp)
	{
		DebugPrintf("Caught exception. Service will be stoped. Exception details: %s\n", exp.c_str());
	}
	catch (...)
	{
		DebugPrintf("Caught exception. Service will be stoped\n");
	}

	WSACleanup();



	CoUninitialize();
	LogEvent(_T("Service stopped"));
	DebugPrintf( "EXIT CServiceModule::Run()\n" );
}

bool CServiceModule::QuitRequested(int waitTimeSeconds)
{
    if(m_bShouldStop)
        return true;

    if(waitTimeSeconds)
    {
        ACE_OS::sleep(waitTimeSeconds);
    }

    return m_bShouldStop;
}

/*
 * _tWinMain
 *
 * Every time the filerep service is started on windows,
 * create /etc/{group,passwd} files that contain the SIDs for
 * the domains users. This is to allow inmsync to properly replicate
 * Windows ACLs (bug 386).
 *
 * Return SVS_OK or SVS_FAIL.
*/
extern "C" int WINAPI _tWinMain(HINSTANCE InmhInstance,
	HINSTANCE , LPTSTR InmlpCmdLine, int )
{
	TerminateOnHeapCorruption();

	DebugPrintf( "ENTER _tWinMain()\n" );

	InmlpCmdLine = GetCommandLine(); 
	_Module.Init(ObjectMap, InmhInstance, IDS_SERVICENAME, &LIBID_SERVICELib);
	_Module.m_InmbService = TRUE;
	TCHAR szTokens[] = _T("-/");

	LPCTSTR lpszToken = FindOneOf(InmlpCmdLine, szTokens);
	while (lpszToken != NULL) {
		if (lstrcmpi(lpszToken, _T("UnregServer"))==0)
			return _Module.UnregisterServer();

		
		
		if (lstrcmpi(lpszToken, _T("Service"))==0)
			return _Module.RegisterServer(TRUE, TRUE);

		lpszToken = FindOneOf(lpszToken, szTokens);
	}

		

	_Module.Start();

	
	DebugPrintf( "EXIT _tWinMain()\n" );
	return _Module.m_Inmstatus.dwWin32ExitCode;
}

