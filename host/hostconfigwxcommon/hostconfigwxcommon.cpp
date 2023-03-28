#include "stdafx.h"
#include "hostconfigwxcommon.h"
#include "hostconfigwxcommonDlg.h"
#include "InMageConfigPropertySheet.h"
#include "inmsafecapis.h"
#include "terminateonheapcorruption.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(ChostconfigwxcommonApp, CWinAppEx)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()

ChostconfigwxcommonApp::ChostconfigwxcommonApp()
{
}

ChostconfigwxcommonApp theApp;

/************************************************************
* This function is the main entry of hostconfigwxcommon	UI	*
************************************************************/
BOOL ChostconfigwxcommonApp::InitInstance()
{
	TerminateOnHeapCorruption();
    init_inm_safe_c_apis();
	//Contains information on classes need to register for controls
	//used in InMage host config UI
	INITCOMMONCONTROLSEX hostConfigInitCtrls;
	hostConfigInitCtrls.dwSize = sizeof(hostConfigInitCtrls);	
	hostConfigInitCtrls.dwICC = ICC_WIN95_CLASSES;

	//Used to register classes for controls used in host config UI
	InitCommonControlsEx(&hostConfigInitCtrls);	
	CWinAppEx::InitInstance();

	//Used to add OLE controls in InMage host config UI
	AfxEnableControlContainer();

	//This name will be used as a caption to AfxMessageBox displayed by host config UI
	//First free the app name that was allocated by MFC before InitInstance
	//App name should be dynamically allocated on heap using _tcsdup. Failing to do so will cause heap corruption
	free((void*)m_pszAppName);
	m_pszAppName = _tcsdup(_T("Host Agent Config"));

	StartLogging();
	//Get Scout installation code from registry
	//4=Only FX, 5=Only VX, 9=Both FX and VX, 8=Oracle agent
	CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode = GetScoutAgentInstallCode();

	if((CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode != SCOUT_VX) &&
		(CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode != SCOUT_FX) &&
		(CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode != SCOUT_BOTH))
	{
		DebugPrintf(SV_LOG_DEBUG,"InMage agent is not installed.\n");
		AfxMessageBox("InMage Agent is not installed",MB_OK|MB_ICONERROR);
		EndLogging();
		return FALSE;
	}
	CInmConfigData::GetInmConfigInstance().m_bEnableHttps = false;
	if('\0' != m_lpCmdLine[0])
	{
		if(0 == strcmpi(m_lpCmdLine, "/enablehttps"))
		{
			CInmConfigData::GetInmConfigInstance().m_bEnableHttps = true;
		}
	}
	//Create a property sheet object and provide property sheet handle to main window.
	CInMageConfigPropertySheet hostConfigSheet(_T("Host Agent Config"));
	m_pMainWnd = &hostConfigSheet;

	//Create a modal dialog of host config property sheet
	//Call will block here until host config is dismissed with OK or CANCEL
	INT_PTR hostConfigSheetResult = hostConfigSheet.DoModal();
	
	if (hostConfigSheetResult == IDOK)
	{
		//InMage config property sheet will be dimissed
	}
	else if (hostConfigSheetResult == IDCANCEL)
	{		
		//InMage config property sheet will be dimissed
	}	
	EndLogging();
	return FALSE;
}

/************************************************************************************
* This function will read registry entry to get list of InMage products installed	*
************************************************************************************/
UINT ChostconfigwxcommonApp::GetScoutAgentInstallCode()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	UINT scoutAgentInstallCode = 0;
	HKEY scoutAgentKey;

	//Check for FX agent installation
	LSTATUS scoutAgentKeyStatus = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\InMage Systems\\Installed Products\\4", 0, KEY_READ, &scoutAgentKey);
	if(scoutAgentKey != NULL)
	{
		DebugPrintf(SV_LOG_DEBUG,"FX agent is installed.\n");
		scoutAgentInstallCode += SCOUT_FX;
		RegCloseKey(scoutAgentKey);
	}

	//Check for VX agent installation
	scoutAgentKeyStatus = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\InMage Systems\\Installed Products\\5", 0, KEY_READ, &scoutAgentKey);
	if(scoutAgentKey != NULL)
	{
		DebugPrintf(SV_LOG_DEBUG,"VX agent is installed.\n");
		scoutAgentInstallCode += SCOUT_VX;
		RegCloseKey(scoutAgentKey);
	}

	//Check for Oracle agent installation
	scoutAgentKeyStatus = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\InMage Systems\\Installed Products\\8", 0, KEY_READ, &scoutAgentKey);
	if(scoutAgentKey != NULL)
	{
		//In Oracle agent, only FX will be installed.
		DebugPrintf(SV_LOG_DEBUG,"Oracle agent is installed.\n");
		scoutAgentInstallCode = SCOUT_FX;
		RegCloseKey(scoutAgentKey);
	}
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
	return scoutAgentInstallCode;
}
void ChostconfigwxcommonApp::StartLogging()
{
	SetHostConfigLogFileName("HostAgentConfig.log");
}
void ChostconfigwxcommonApp::EndLogging()
{
	CloseHostConfigLog();
}