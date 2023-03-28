#include "stdafx.h"
#include "hostconfigwxcommon.h"
#include "InMageConfigPropertySheet.h"
#include <fstream>

IMPLEMENT_DYNAMIC(CInMageConfigPropertySheet, CPropertySheet)

CInMageConfigPropertySheet::CInMageConfigPropertySheet(UINT nIDCaption, CWnd* pParentWnd, UINT iSelectPage)
	:CPropertySheet(nIDCaption, pParentWnd, iSelectPage)
{

}

CInMageConfigPropertySheet::CInMageConfigPropertySheet(LPCTSTR pszCaption, CWnd* pParentWnd, UINT iSelectPage)
	:CPropertySheet(pszCaption, pParentWnd, iSelectPage)
{
	this->AddPage(&m_cxSettingsPage);
	if(CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode != SCOUT_FX)
		this->AddPage(&m_vxAgentSettingsPage);
	this->AddPage(&m_agentLoggingPage);	
	this->AddPage(&m_logonPage);
}

CInMageConfigPropertySheet::~CInMageConfigPropertySheet()
{
}


BEGIN_MESSAGE_MAP(CInMageConfigPropertySheet, CPropertySheet)
	ON_MESSAGE(CONFIG_READ_SUCCESS,&CInMageConfigPropertySheet::OnConfigReadSuccess)
	ON_MESSAGE(CONFIG_READ_FAILED, &CInMageConfigPropertySheet::OnConfigReadFailed)
	ON_MESSAGE(CONFIG_SAVE_SUCCESS,&CInMageConfigPropertySheet::OnConfigSaveSuccess)
	ON_MESSAGE(CONFIG_SAVE_FAILED, &CInMageConfigPropertySheet::OnConfigSaveFailed)
	ON_COMMAND(ID_APPLY_NOW,&CInMageConfigPropertySheet::OnApplyNow)
	ON_COMMAND(IDOK,&CInMageConfigPropertySheet::OnOK)
	ON_COMMAND(IDCANCEL,&CInMageConfigPropertySheet::OnCancel)
	//ON_COMMAND(IDHELP,&CInMageConfigPropertySheet::OnHelp)
	ON_WM_SYSCOMMAND()
END_MESSAGE_MAP()

/********************************************************************************
* Read configuration data from persisted location and update in host config ui	*
********************************************************************************/
BOOL CInMageConfigPropertySheet::OnInitDialog()
{
	BOOL bInitConfigPropertySheet = CPropertySheet::OnInitDialog();
	
	//Help will be available online. Hence disable help button
	CButton *configHelpButton;
	configHelpButton = reinterpret_cast<CButton *>(GetDlgItem(IDHELP));
	configHelpButton->ShowWindow(SW_HIDE);

	//Disable the page, and tab control while the worker thread is in progress.
	//Once the thread finishes restore them back.
	ShowBackEndStatus(INM_BACKEND_STATUS_INITIALIZING);
	EnabledWizardPages(false);

	AfxBeginThread(CInmConfigData::ConfigReadThreadProc,this);
	m_bAgentConfigThreadRunning = true;
	return bInitConfigPropertySheet;
}

/****************************************************
* Message handler for successful read operation		*
****************************************************/
LRESULT CInMageConfigPropertySheet::OnConfigReadSuccess(WPARAM, LPARAM)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n", __FUNCTION__);
	m_bAgentConfigThreadRunning = false;
	
	//Sync all config data in host config UI and set Global tab as active
	if(CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode != SCOUT_FX)
		m_vxAgentSettingsPage.SyncAgentConfigData();
	m_agentLoggingPage.SyncAgentConfigData();
	m_logonPage.SyncAgentConfigData();
	
	m_cxSettingsPage.SyncAgentConfigData();	
	m_cxSettingsPage.UpdateData(FALSE);
	SetActivePage(&m_cxSettingsPage);

	//Disable progress status text and enable controls
	ShowBackEndStatus(INM_BACKEND_STATUS_INITIALIZING,false);
	EnabledWizardPages();
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", __FUNCTION__);
	return 0;
}

/************************************************
* Message handler for failed read operation		*
************************************************/
LRESULT CInMageConfigPropertySheet::OnConfigReadFailed(WPARAM, LPARAM)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n", __FUNCTION__);
	m_bAgentConfigThreadRunning = false;

	//Disable progress status text and enable controls
	ShowBackEndStatus(INM_BACKEND_STATUS_INITIALIZING,false);
	EnabledWizardPages();

	//Failed to read the configuration from persistent location
	//Close the dialog after error prompt message.
	AfxMessageBox(CInmConfigData::GetInmConfigInstance().GetConfigErrorMsg(),MB_OK|MB_ICONERROR);
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", __FUNCTION__);
	EndDialog(IDOK);
	return 0;
}

/****************************************************
* Message handler for successful save operation		*
****************************************************/
LRESULT CInMageConfigPropertySheet::OnConfigSaveSuccess(WPARAM, LPARAM)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n", __FUNCTION__);
	m_bAgentConfigThreadRunning = false;	

	ShowBackEndStatus(INM_BACKEND_STATUS_SAVING,false);
	EnabledWizardPages();

	//reset m_bChangeAttempted to false
	m_cxSettingsPage.ResetConfigChangeStatus();
	if(CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode != SCOUT_FX)
		m_vxAgentSettingsPage.ResetConfigChangeStatus();
	m_agentLoggingPage.ResetConfigChangeStatus();
	m_logonPage.ResetConfigChangeStatus();
	CInmConfigData::GetInmConfigInstance().m_bEnableHttps = false;

	//Ok button was pressed without Apply button,
	//So exiting the wizard now after saving the changes.
	if(m_bSaveConfigAndExit)
		EndDialog(IDOK);
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", __FUNCTION__);
	return 0;
}

/****************************************************
* Message handler for failed save operation			*
****************************************************/
LRESULT CInMageConfigPropertySheet::OnConfigSaveFailed(WPARAM, LPARAM)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n", __FUNCTION__);
	m_bAgentConfigThreadRunning = false;

	ShowBackEndStatus(INM_BACKEND_STATUS_SAVING,false);
	EnabledWizardPages();

	AfxMessageBox(CInmConfigData::GetInmConfigInstance().GetConfigErrorMsg(),MB_OK|MB_ICONERROR);
	
	//Exit wizard only OK button is pressed
	if (CInmConfigData::GetInmConfigInstance().IsPassphraseValidated() && m_bSaveConfigAndExit)
		EndDialog(IDOK);
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", __FUNCTION__);
	return 0;
}

/********************************************************************
* This function will sync host config data from UI to background	*
* and start thread to persist config data to registry and			* 
* drscout.conf file													*
********************************************************************/
void CInMageConfigPropertySheet::SaveHostAgentConfig()
{
	bool bGlobalConfigChanged = m_cxSettingsPage.IsAgentConfigChanged();
	bool bAgentConfigChanged = false;
	bool bLoggingConfigChanged = m_agentLoggingPage.IsAgentConfigChanged();
	bool bLogonConfigChanged = m_logonPage.IsAgentConfigChanged();

	//1. Sync Global page data with config data.
	if(bGlobalConfigChanged)
	{
		BOOL bRet = m_cxSettingsPage.SyncAgentConfigData(CInmConfigData::GetInmConfigInstance().m_cxSettings);
		if(!bRet)
			return;
	}

	//2.Sync Agent page data with config data
	if(CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode != SCOUT_FX)
	{
		bAgentConfigChanged = m_vxAgentSettingsPage.IsAgentConfigChanged();
		if(bAgentConfigChanged)
		{
			BOOL bRet =	m_vxAgentSettingsPage.SyncAgentConfigData(CInmConfigData::GetInmConfigInstance().m_vxAgentSettings);
			if(!bRet)
				return;
		}
	}

	//3. Sync Logging page data with config data
	if(bLoggingConfigChanged)
		m_agentLoggingPage.SyncAgentConfigData(CInmConfigData::GetInmConfigInstance().m_agentLoggingSettings);
	
	//4. Sync Logon page data with config data
	if(bLogonConfigChanged)
	{
		BOOL bRet = m_logonPage.SyncAgentConfigData(CInmConfigData::GetInmConfigInstance().m_agentLogonSettings);	
		if(!bRet)
			return;
	}

	//5. Set backend status message and disable pages, and restore them back at OnConfigSaveSuccess/OnConfigSaveFailed
	ShowBackEndStatus(INM_BACKEND_STATUS_SAVING);
	EnabledWizardPages(false);

	//6. Set config data change status
	CInmConfigData::GetInmConfigInstance().SetGlobalDataChangeStatus(bGlobalConfigChanged);
	CInmConfigData::GetInmConfigInstance().SetAgentDataChangeStatus(bAgentConfigChanged);
	CInmConfigData::GetInmConfigInstance().SetLoggingDataChangeStatus(bLoggingConfigChanged);
	CInmConfigData::GetInmConfigInstance().SetLogonDataChangeStatus(bLogonConfigChanged);

	//7. Start the worker thread to save the configuration to persistent location.
	AfxBeginThread(CInmConfigData::ConfigSaveThreadProc,this);
	m_bAgentConfigThreadRunning = true;
}

/********************************************************************
* Display progress text when background thread work is in progress	*
********************************************************************/
void CInMageConfigPropertySheet::ShowBackEndStatus(INM_BACKEND_STATUS status, bool bShow)
{
	int show = bShow ? SW_SHOW : SW_HIDE;

	switch(status)
	{
	case INM_BACKEND_STATUS_INITIALIZING:
		m_cxSettingsPage.m_staticInitialize.ShowWindow(show);
		break;
	case INM_BACKEND_STATUS_SAVING:
		GetActivePage()->GetDlgItem(IDC_STATIC_IN_PROGRESS)->ShowWindow(show);
		break;
	default:
		//Log warning msg about unknown status value
		break;
	}
}

/********************************************************************
* Enable or disable controls in tab when background thread starts	*
* or finishes it's work												*
********************************************************************/
void CInMageConfigPropertySheet::EnabledWizardPages(bool bEnable)
{
	GetActivePage()->EnableWindow(bEnable);
	GetTabControl()->EnableWindow(bEnable);	
}

/************************************************
* Command handler for APPLY button				*
* Start saving host config data					*
************************************************/
void CInMageConfigPropertySheet::OnApplyNow()
{
	m_bSaveConfigAndExit = false;
	Default();
	SaveHostAgentConfig();		
}

/********************************************************
* Command handler for OK button							*
* Start saving host config data if not done and exit	*
********************************************************/
void CInMageConfigPropertySheet::OnOK()
{
	if(m_bAgentConfigThreadRunning)
	{
		AfxMessageBox(_T("Please wait, Updating Configuration Data is in progress..."),
		MB_OK|MB_ICONEXCLAMATION);
	}
	else
	{	
		GetActivePage()->UpdateData();
			
		//Validate the all pages to detect real changes.
		m_cxSettingsPage.ValidateAgentConfigChange();
		if(CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode != SCOUT_FX)
			m_vxAgentSettingsPage.ValidateAgentConfigChange();
		m_agentLoggingPage.ValidateAgentConfigChange();
		m_logonPage.ValidateAgentConfigChange();

		//Verify that is there any configuration change made, if so save the changes and exit.
		bool bConfigChanged_t = m_cxSettingsPage.IsAgentConfigChanged() || m_agentLoggingPage.IsAgentConfigChanged() || m_logonPage.IsAgentConfigChanged();
		if(CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode != SCOUT_FX)
			bConfigChanged_t = bConfigChanged_t || m_vxAgentSettingsPage.IsAgentConfigChanged();
		if(bConfigChanged_t)
		{
			m_bSaveConfigAndExit = true;
			GetDlgItem(ID_APPLY_NOW)->EnableWindow(FALSE);
			SaveHostAgentConfig();
		}
		else
		{
			CPropertySheet::EndDialog(IDOK);
		}		
	}
}

/************************************************************************
* Command handler for CANCEL button and ESC key pressed					*
* Confirm host config dismiss if any config data changes are detected	*
************************************************************************/
void CInMageConfigPropertySheet::OnCancel()
{
	if(m_bAgentConfigThreadRunning)
	{
		AfxMessageBox(_T("Please wait, Updating Configuration Data is in progress..."),
		MB_OK|MB_ICONEXCLAMATION);
	}
	else
	{
		std::string agentInstallDir = CInmConfigData::GetInmConfigInstance().GetAgentInstallationPath();

		std::string errorMessage = "If you close this wizard, you will not be able to register this server with Configuration Server.Click 'Yes' to close the wizard and 'No' to go back to the wizard. At a later point of time, you can register this server by invoking 'hostconfigwxcommon.exe' from the path ";
		errorMessage += agentInstallDir;
		if((CInmConfigData::GetInmConfigInstance().m_bEnableHttps) ||
			(!CInmConfigData::GetInmConfigInstance().IsAgentRegistered()))
		{	
			if (AfxMessageBox(errorMessage.c_str(), MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) == IDYES){
				CPropertySheet::EndDialog(IDCANCEL);
			}
			return;
		}
		//Updated the active page so that the DDX will happen.
		GetActivePage()->UpdateData();
		
		//Validate the all pages to detect real changes.
		//Note: Worker thread is not running because m_bAgentConfigThreadRunning is false.
		//      So we are free to access config data object ValidateAgentConfigChange().
		m_cxSettingsPage.ValidateAgentConfigChange();
		if(CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode != SCOUT_FX)
			m_vxAgentSettingsPage.ValidateAgentConfigChange();
		m_agentLoggingPage.ValidateAgentConfigChange();
		m_logonPage.ValidateAgentConfigChange();

		//Verify that is there any configuration change made, if so prompt the user to proceed for cancel.
		bool bConfigChanged_t = m_cxSettingsPage.IsAgentConfigChanged() || m_agentLoggingPage.IsAgentConfigChanged() || m_logonPage.IsAgentConfigChanged();
		if(CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode != SCOUT_FX)
			bConfigChanged_t = bConfigChanged_t || m_vxAgentSettingsPage.IsAgentConfigChanged();
		if(bConfigChanged_t)
		{
			if(AfxMessageBox(_T("Do you want to exit without saving changes ?"),MB_YESNO | MB_ICONQUESTION) == IDYES)
				CPropertySheet::EndDialog(IDCANCEL);
		}
		else
		{
			CPropertySheet::EndDialog(IDCANCEL);
		}
	}
}

/************************************************************************
* Command handler for property sheet control button	X					*
* Confirm host config dismiss if any config data changes are detected	*
************************************************************************/
void CInMageConfigPropertySheet::OnSysCommand(UINT nID, LPARAM lParam)
{
	if(SC_CLOSE == nID)
	{
		if(m_bAgentConfigThreadRunning)
		{
			AfxMessageBox(_T("Please wait, Updating Configuration Data is in progress..."),
			MB_OK|MB_ICONEXCLAMATION);
			//Exit without closing window.
			return;
		}
		else
		{
			std::string agentInstallDir = CInmConfigData::GetInmConfigInstance().GetAgentInstallationPath();

			std::string errorMessage = "If you close this wizard, you will not be able to register this server with Configuration Server.Click 'Yes' to close the wizard and 'No' to go back to the wizard. At a later point of time, you can register this server by invoking 'hostconfigwxcommon.exe' from the path ";
			errorMessage += agentInstallDir;
			if ((CInmConfigData::GetInmConfigInstance().m_bEnableHttps) ||
				(!CInmConfigData::GetInmConfigInstance().IsAgentRegistered()))
			{
				if (AfxMessageBox(errorMessage.c_str(), MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) == IDYES){
					CPropertySheet::EndDialog(IDCANCEL);
				}
				return;
			}
			//Updated the active page so that the DDX will happen.
			GetActivePage()->UpdateData();
			
			//Validate the all pages to detect real changes.
			//Note: Worker thread is not running because m_bAgentConfigThreadRunning is false.
			//      So we are free to access config data object ValidateAgentConfigChange().
			m_cxSettingsPage.ValidateAgentConfigChange();
			if(CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode != SCOUT_FX)
				m_vxAgentSettingsPage.ValidateAgentConfigChange();
			m_agentLoggingPage.ValidateAgentConfigChange();
			m_logonPage.ValidateAgentConfigChange();

			//Verify that is there any configuration change made, if so prompt the user to proceed for cancel.
			bool bConfigChanged_t = m_cxSettingsPage.IsAgentConfigChanged() || m_agentLoggingPage.IsAgentConfigChanged() || m_logonPage.IsAgentConfigChanged();
			if(CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode != SCOUT_FX)
				bConfigChanged_t = bConfigChanged_t || m_vxAgentSettingsPage.IsAgentConfigChanged();
			if(bConfigChanged_t)
			{
				if(AfxMessageBox(_T("Do you want to exit without saving changes ?"),MB_YESNO | MB_ICONQUESTION) == IDNO)
					return;
			}
		}
	}	
	CPropertySheet::OnSysCommand(nID, lParam);
}

/****************************
* Display host config help	*
****************************/
void CInMageConfigPropertySheet::OnHelp()
{
	//Check help file exists
	std::ifstream helpFstream;
	helpFstream.open("hostconfig.chm");		
	if(helpFstream.is_open() == true)
	{
		helpFstream.close();
		::HtmlHelp(this->m_hWnd, "hostconfig.chm",HH_DISPLAY_TOPIC,NULL);
	}
	else
		AfxMessageBox("Help file is missing.", MB_OK|MB_ICONWARNING); 
}