#include "stdafx.h"
#include "hostconfigwxcommon.h"
#include "InMageLogonSettings.h"
#include "InMageSecurity.h"
#include <objsel.h>

IMPLEMENT_DYNAMIC(CInMageLogonSettings, CPropertyPage)

CInMageLogonSettings::CInMageLogonSettings()
	: CPropertyPage(CInMageLogonSettings::IDD)
	, m_agentSvcLogonName(_T(""))
	, m_agentSvcLogonPwd(_T(""))
	, m_fxAgentGroupBox(0)
	, m_useUserAccount(0)
{
	m_bChangeAttempted = false;
}

CInMageLogonSettings::~CInMageLogonSettings()
{
}

void CInMageLogonSettings::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_USER_NAME, m_agentSvcLogonName);
	DDX_Text(pDX, IDC_EDIT_PASSWORD, m_agentSvcLogonPwd);
	DDX_Radio(pDX, IDC_RADIO_FX_LOCAL_SYSTEM, m_fxAgentGroupBox);
	DDX_Check(pDX, IDC_CHECK_APPAGENT_USER, m_useUserAccount);
	DDX_Control(pDX, IDC_STATIC_APPAGENT_USER, m_appAgentGroupBox);
	DDX_Control(pDX, IDC_CHECK_APPAGENT_USER, m_appAgentUserCheckBox);
	DDX_Control(pDX, IDC_STATIC_APPAGENT_NOTE, m_appAgentNoteStaticText);
	DDX_Control(pDX, IDC_STATIC_FXAGENT_USER, m_fxAgentGroupBoxCtrl);
	DDX_Control(pDX, IDC_RADIO_FX_LOCAL_SYSTEM, m_fxLocalSysRadioButton);
	DDX_Control(pDX, IDC_RADIO_FX_USER_ACOUNT, m_fxUserAccRadioButton);
}


BEGIN_MESSAGE_MAP(CInMageLogonSettings, CPropertyPage)
	ON_EN_CHANGE(IDC_EDIT_USER_NAME, &CInMageLogonSettings::OnEnChangeEditUserName)
	ON_EN_CHANGE(IDC_EDIT_PASSWORD, &CInMageLogonSettings::OnEnChangeEditPassword)
	ON_BN_CLICKED(IDC_RADIO_FX_LOCAL_SYSTEM, &CInMageLogonSettings::OnBnClickedRadioFxLocalSystem)
	ON_BN_CLICKED(IDC_RADIO_FX_USER_ACOUNT, &CInMageLogonSettings::OnBnClickedRadioFxUserAcount)
	ON_BN_CLICKED(IDC_BT_USER_SEARCH, &CInMageLogonSettings::OnBnClickedBtUserSearch)
	ON_BN_CLICKED(IDC_CHECK_APPAGENT_USER, &CInMageLogonSettings::OnBnClickedCheckAppagentUser)
	ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

/********************************************
* Event handler for logon user name change	*
* Enable Apply button if change attempted	*
********************************************/
void CInMageLogonSettings::OnEnChangeEditUserName()
{
	CString strCurrDir;
	GetDlgItem(IDC_EDIT_USER_NAME)->GetWindowText(strCurrDir);
	if(strCurrDir.CompareNoCase(m_agentSvcLogonName))
	{
		m_bChangeAttempted = true;
		SetModified();
	}
}

/************************************************
* Event handler for logon user password change	*
* Enable Apply button if change attempted		*
************************************************/
void CInMageLogonSettings::OnEnChangeEditPassword()
{
	if(GetDlgItem(IDC_EDIT_PASSWORD)->GetWindowTextLength())
	{
		m_bChangeAttempted = true;
		SetModified();
	}
}

/************************************************
* Event handler for FX logon user option change	*
* Handle case if Local system is selected		*
* Enable Apply button if change attempted		*
************************************************/
void CInMageLogonSettings::OnBnClickedRadioFxLocalSystem()
{
	//0 is the index of FX Local System Account Radio button
	if( 0 == m_fxAgentGroupBox)
	{
		if(((CButton*)GetDlgItem(IDC_RADIO_FX_LOCAL_SYSTEM))->GetCheck() != BST_CHECKED )
		{
			m_bChangeAttempted = true;
			SetModified();
		}
	}
	else
	{
		if(((CButton*)GetDlgItem(IDC_RADIO_FX_LOCAL_SYSTEM))->GetCheck() == BST_CHECKED )
		{
			m_bChangeAttempted = true;
			SetModified();
		}
	}
	ValidateUserConfigGroup();
}

/************************************************
* Event handler for FX logon user option change	*
* Handle case if Use account is selected		*
* Enable Apply button if change attempted		*
************************************************/
void CInMageLogonSettings::OnBnClickedRadioFxUserAcount()
{
	if( 1 == m_fxAgentGroupBox)
	{
		if(((CButton*)GetDlgItem(IDC_RADIO_FX_USER_ACOUNT))->GetCheck() != BST_CHECKED )
		{
			m_bChangeAttempted = true;
			SetModified();
		}
	}
	else
	{
		if(((CButton*)GetDlgItem(IDC_RADIO_FX_USER_ACOUNT))->GetCheck() == BST_CHECKED )
		{
			m_bChangeAttempted = true;
			SetModified();
		}
	}
	ValidateUserConfigGroup();
}

/****************************************************
* Event handler for user search button is clicked	*
* Enable Apply button if change attempted			*
****************************************************/
void CInMageLogonSettings::OnBnClickedBtUserSearch()
{
	CString agentSvcLogonName;
	if(ShowUserPickDlg(agentSvcLogonName))
	{
		//Trim the un-wanted part from the string which looks like "WinNT://domain[/computer]/user-name"
		//We don't need the part after 2nd '/' from last.
		int slashCount = 0;
		for(int pos = agentSvcLogonName.GetLength()-1; pos >= 0; pos--)
		{
			if(agentSvcLogonName[pos] == '/')
				slashCount++;
			if(slashCount > 1)
			{
				agentSvcLogonName.Delete(0,pos+1);
				break;
			}
		}
		agentSvcLogonName.Replace("/","\\");
		m_agentSvcLogonName = agentSvcLogonName;
		if(CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode != SCOUT_FX)
			m_useUserAccount = ((CButton*)GetDlgItem(IDC_CHECK_APPAGENT_USER))->GetCheck();
		if(CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode != SCOUT_VX)
			m_fxAgentGroupBox = ((CButton*)GetDlgItem(IDC_RADIO_FX_USER_ACOUNT))->GetCheck();
		m_bChangeAttempted = true;
		SetModified();
		UpdateData(false);
	}
}

/********************************************************
* This function will display a user picked dialog box	*
* and display selected user in host config ui			*
********************************************************/
bool CInMageLogonSettings::ShowUserPickDlg(CString & scoutSvcLogonUserName)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n", __FUNCTION__);
	bool bUserSelected = false;

	CoInitialize(NULL);
	do
	{
		IDsObjectPicker *inmSvcLogonUserDlg = NULL;
		HRESULT hrRet = CoCreateInstance(CLSID_DsObjectPicker,0,CLSCTX_INPROC_SERVER,IID_IDsObjectPicker,(void**)&inmSvcLogonUserDlg);
		if(FAILED(hrRet))
		{
			CString errMsg;
			errMsg.Format(_T("Cloud't open User Picker Dialog. Error %ld"),hrRet);
			DebugPrintf(SV_LOG_ERROR,"%s\n", errMsg.GetString());
			AfxMessageBox(errMsg);
			//Add debug log for clear error information
			break;
		}

		DSOP_SCOPE_INIT_INFO usrListScopeInitInfos[1] = { 0 };
		usrListScopeInitInfos[0].cbSize = sizeof(usrListScopeInitInfos);
		usrListScopeInitInfos[0].flType = 
			DSOP_SCOPE_TYPE_GLOBAL_CATALOG |
			DSOP_SCOPE_TYPE_TARGET_COMPUTER |
			DSOP_SCOPE_TYPE_ENTERPRISE_DOMAIN |
			DSOP_SCOPE_TYPE_UPLEVEL_JOINED_DOMAIN |
			DSOP_SCOPE_TYPE_DOWNLEVEL_JOINED_DOMAIN |
			DSOP_SCOPE_TYPE_EXTERNAL_UPLEVEL_DOMAIN |
			DSOP_SCOPE_TYPE_EXTERNAL_DOWNLEVEL_DOMAIN |
			DSOP_SCOPE_TYPE_USER_ENTERED_UPLEVEL_SCOPE |
			DSOP_SCOPE_TYPE_USER_ENTERED_DOWNLEVEL_SCOPE;
		usrListScopeInitInfos[0].flScope =
			DSOP_SCOPE_FLAG_STARTING_SCOPE |
			DSOP_SCOPE_FLAG_WANT_PROVIDER_WINNT |
			DSOP_SCOPE_FLAG_DEFAULT_FILTER_USERS |
			DSOP_SCOPE_FLAG_DEFAULT_FILTER_GROUPS;
		usrListScopeInitInfos[0].FilterFlags.flDownlevel = DSOP_DOWNLEVEL_FILTER_USERS;
		usrListScopeInitInfos[0].FilterFlags.Uplevel.flBothModes = DSOP_FILTER_USERS;

		DSOP_INIT_INFO usrDlgInitInfo;
		ZeroMemory(&usrDlgInitInfo,sizeof(usrDlgInitInfo));
		usrDlgInitInfo.cbSize = sizeof(usrDlgInitInfo);
		usrDlgInitInfo.aDsScopeInfos = usrListScopeInitInfos;
		usrDlgInitInfo.cDsScopeInfos = 1;
		usrDlgInitInfo.flOptions = DSOP_FLAG_SKIP_TARGET_COMPUTER_DC_CHECK;
		
		hrRet = inmSvcLogonUserDlg->Initialize(&usrDlgInitInfo);
		if(FAILED(hrRet))
		{
			CString errMsg;
			errMsg.Format(_T("Error initializing User Picker Dialog. Error %ld"),hrRet);
			DebugPrintf(SV_LOG_ERROR,"%s\n", errMsg.GetString());
			AfxMessageBox(errMsg);
			inmSvcLogonUserDlg->Release();
			//Add debug log for clear error information
			break;
		}

		IDataObject *inmSvcSelUserData = NULL;
		hrRet = inmSvcLogonUserDlg->InvokeDialog(m_hWnd,&inmSvcSelUserData);
		if(FAILED(hrRet))
		{
			CString errMsg;
			errMsg.Format(_T("Error Opening User Picker Dialog. Error %ld"),hrRet);
			DebugPrintf(SV_LOG_ERROR,"%s\n", errMsg.GetString());
			AfxMessageBox(errMsg);
			inmSvcLogonUserDlg->Release();
			//Add debug log for clear error information
			break;
		}
		else if(inmSvcSelUserData)
		{
			STGMEDIUM inmUserStgMedium;
			
			FORMATETC inmUserFmtData;
			inmUserFmtData.cfFormat = RegisterClipboardFormat(CFSTR_DSOP_DS_SELECTION_LIST);
			inmUserFmtData.ptd = 0;
			inmUserFmtData.dwAspect = DVASPECT_CONTENT;
			inmUserFmtData.lindex = -1;
			inmUserFmtData.tymed = TYMED_HGLOBAL;

			hrRet = inmSvcSelUserData->GetData(&inmUserFmtData,&inmUserStgMedium);
			if(FAILED(hrRet))
			{
				CString errMsg;
				errMsg.Format(_T("Error Reading selected user-name from Picker Dialog. Error %ld"),hrRet);
				DebugPrintf(SV_LOG_ERROR,"%s\n", errMsg.GetString());
				AfxMessageBox(errMsg);
			}
			else
			{
				PDS_SELECTION_LIST pUsrSelectionList = (PDS_SELECTION_LIST)GlobalLock(inmUserStgMedium.hGlobal);
				if(pUsrSelectionList)
				{
					for(ULONG iUser = 0; iUser < pUsrSelectionList->cItems; iUser++)
						scoutSvcLogonUserName.Format("%S",pUsrSelectionList->aDsSelection[iUser].pwzADsPath);
					GlobalUnlock(inmUserStgMedium.hGlobal);
				}
				else
				{
					CString errMsg;
					errMsg.Format(_T("Got no user-name from Picker Dialog. Please retry !"));
					DebugPrintf(SV_LOG_ERROR,"%s\n", errMsg.GetString());
					AfxMessageBox(errMsg);
				}
				ReleaseStgMedium(&inmUserStgMedium);
				bUserSelected = true;
			}
		}
		
		inmSvcLogonUserDlg->Release();
	}while(false);
	CoUninitialize();
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", __FUNCTION__);
	return bUserSelected;
}

/************************************************************************************
* Based on scout installation code display controls when Logon tab is initialized	*
************************************************************************************/
BOOL CInMageLogonSettings::OnInitDialog()
{
	BOOL bInitLogonSettings = CPropertyPage::OnInitDialog();

	if(CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode == SCOUT_VX)
	{
		//Hide Fx logon user group box
		m_fxAgentGroupBoxCtrl.ShowWindow(SW_HIDE);
		m_fxLocalSysRadioButton.ShowWindow(SW_HIDE);
		m_fxUserAccRadioButton.ShowWindow(SW_HIDE);
	}
	else if(CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode == SCOUT_FX)
	{
		//Hide App agent logon user group box
		m_appAgentGroupBox.ShowWindow(SW_HIDE);
		m_appAgentUserCheckBox.ShowWindow(SW_HIDE);
		m_appAgentNoteStaticText.ShowWindow(SW_HIDE);

		//Move Fx logon user group box upward
		m_fxAgentGroupBoxCtrl.MoveWindow(17,129,350,90);
		m_fxLocalSysRadioButton.MoveWindow(25,150,150,35);
		m_fxUserAccRadioButton.MoveWindow(25,180,135,35);
	}
	return bInitLogonSettings;
}

/************************************************************
* Checks whether the config data is really changed or not	*
************************************************************/
bool CInMageLogonSettings::ValidateAgentConfigChange()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n", __FUNCTION__);
	if(!m_bChangeAttempted)
	{
		const LOGON_SETTINGS& agentLogonSettings = CInmConfigData::GetInmConfigInstance().GetAgentLogonSettings();
		
		if(agentLogonSettings.bAppAgentUserCheck != (m_useUserAccount == BST_CHECKED))
		{
			m_bChangeAttempted = true;
			DebugPrintf(SV_LOG_DEBUG, "Use user account state changed\n");
			DebugPrintf(SV_LOG_DEBUG, "Old state = %d\n",agentLogonSettings.bAppAgentUserCheck);
			DebugPrintf(SV_LOG_DEBUG, "New state = %d\n",m_useUserAccount);
			return m_bChangeAttempted;
		}

		if(agentLogonSettings.bFxLocalSystemAct)
		{
			if(0 != m_fxAgentGroupBox)
			{
				m_bChangeAttempted = true;
				return m_bChangeAttempted;
			}
		}
		else if(agentLogonSettings.bFxUserAct)
		{
			if(0 == m_fxAgentGroupBox)
			{
				m_bChangeAttempted = true;
				return m_bChangeAttempted;
			}
		}

		if(agentLogonSettings.user_name.CompareNoCase(m_agentSvcLogonName))
		{
			m_bChangeAttempted = true;
			DebugPrintf(SV_LOG_DEBUG, "User name changed.\n");
			DebugPrintf(SV_LOG_DEBUG, "Old User name = %s\n", agentLogonSettings.user_name.GetString());
			DebugPrintf(SV_LOG_DEBUG, "New User name = %s\n", m_agentSvcLogonName.GetString());
			return m_bChangeAttempted;
		}

		if(agentLogonSettings.user_password.CompareNoCase(m_agentSvcLogonPwd))
		{
			m_bChangeAttempted = true;
			DebugPrintf(SV_LOG_DEBUG, "User password changed.\n");
			return m_bChangeAttempted;
		}

		//No data change found 
		m_bChangeAttempted = false;
		DebugPrintf(SV_LOG_DEBUG,"No change attempted in Log On tab.\n");
	}
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", __FUNCTION__);
	return m_bChangeAttempted;
}

/************************************************************************************
* Do config data exchange from control variable to structure to persist the data	*
************************************************************************************/
BOOL CInMageLogonSettings::SyncAgentConfigData(LOGON_SETTINGS &agentLogonSettings)
{
	if(agentLogonSettings.bAppAgentUserCheck != (m_useUserAccount ? true : false))
	{
		if(m_useUserAccount == BST_CHECKED)
		{
			if(m_agentSvcLogonName.CompareNoCase("LocalSystem") == 0)
			{
				AfxMessageBox("Provide a user name.", MB_OK|MB_ICONERROR);
				DebugPrintf(SV_LOG_ERROR, "Use user account is selected but the user name is LocalSystem\n");
				return FALSE;
			}
			else if(m_agentSvcLogonName.IsEmpty())
			{
				AfxMessageBox("Logon user name cannot be empty.", MB_OK|MB_ICONERROR);
				DebugPrintf(SV_LOG_ERROR, "Logon user name cannot be empty.\n");
				return FALSE;
			}
			else if(m_agentSvcLogonPwd.IsEmpty())
			{
				AfxMessageBox("Logon user password cannot be empty.", MB_OK|MB_ICONERROR);
				DebugPrintf(SV_LOG_ERROR, "Password field is empty.\n");
				return FALSE;
			}
		}
	}
	if(agentLogonSettings.bFxUserAct != (m_fxAgentGroupBox ? true : false))
	{
		if(1 == m_fxAgentGroupBox)
		{
			if(m_agentSvcLogonName.CompareNoCase("LocalSystem") == 0)
			{
				AfxMessageBox("Provide a user name.", MB_OK|MB_ICONERROR);
				DebugPrintf(SV_LOG_ERROR, "Use user account is selected but the user name is LocalSystem\n");
				return FALSE;
			}
			else if(m_agentSvcLogonName.IsEmpty())
			{
				AfxMessageBox("Logon user name cannot be empty.", MB_OK|MB_ICONERROR);
				DebugPrintf(SV_LOG_ERROR, "Logon user name cannot be empty.\n");
				return FALSE;
			}
			else if(m_agentSvcLogonPwd.IsEmpty())
			{
				AfxMessageBox("Logon user password cannot be empty.", MB_OK|MB_ICONERROR);
				DebugPrintf(SV_LOG_ERROR, "Password field is empty.\n");
				return FALSE;
			}
		}
	}
	agentLogonSettings.user_name = m_agentSvcLogonName;
	agentLogonSettings.user_password = m_agentSvcLogonPwd;
	agentLogonSettings.bAppAgentUserCheck = (m_useUserAccount == BST_CHECKED);

	if(0 == m_fxAgentGroupBox)
	{
		agentLogonSettings.bFxUserAct = false;
		agentLogonSettings.bFxLocalSystemAct = true;
	}
	else if(1 == m_fxAgentGroupBox)
	{
		agentLogonSettings.bFxUserAct = true;
		agentLogonSettings.bFxLocalSystemAct = false;
	}
	return TRUE;
}

/********************************************************************
* Do config data exchange from persisted data to control variables	*
********************************************************************/
void CInMageLogonSettings::SyncAgentConfigData()
{
	const LOGON_SETTINGS& agentLogonSettings = CInmConfigData::GetInmConfigInstance().GetAgentLogonSettings();
	
	m_agentSvcLogonName = agentLogonSettings.user_name;
	m_agentSvcLogonPwd = "";

	if(agentLogonSettings.bAppAgentUserCheck)
		m_useUserAccount = BST_CHECKED;
	else
		m_useUserAccount = BST_UNCHECKED;

	if(agentLogonSettings.bFxLocalSystemAct)
		m_fxAgentGroupBox = 0;
	else if(agentLogonSettings.bFxUserAct)
		m_fxAgentGroupBox = 1;
}

/********************************************************************
* Update and validate the config data when tab/page lose focus		*
********************************************************************/
BOOL CInMageLogonSettings::OnKillActive()
{
	UpdateData();
	ValidateAgentConfigChange();
	if(!m_bChangeAttempted)
		SetModified(FALSE);
	return CPropertyPage::OnKillActive();
}

/************************************************
* Event handler for App agent logon user change	*
* Enable Apply button if change attempted		*
************************************************/
void CInMageLogonSettings::OnBnClickedCheckAppagentUser()
{
	int userAccountCheckStatus = ((CButton*)GetDlgItem(IDC_CHECK_APPAGENT_USER))->GetCheck();

	if( userAccountCheckStatus != m_useUserAccount)
	{
		m_bChangeAttempted = true;
		SetModified();
	}
	ValidateUserConfigGroup();
}

/************************************************************************
* Validate the user configuration option when tab/page becomes active	*
************************************************************************/
BOOL CInMageLogonSettings::OnSetActive()
{
	ValidateUserConfigGroup();

	return CPropertyPage::OnSetActive();
}

/************************************************************
* Enable or disable user configuration group box controls	*
************************************************************/
void CInMageLogonSettings::EnableUserConfigGroup(bool bEnable)
{
	if(!bEnable)
	{
		GetDlgItem(IDC_EDIT_USER_NAME)->SetWindowText("");
		GetDlgItem(IDC_EDIT_PASSWORD)->SetWindowText("");
	}
	GetDlgItem(IDC_EDIT_USER_NAME)->EnableWindow(bEnable);
	GetDlgItem(IDC_EDIT_PASSWORD)->EnableWindow(bEnable);
	GetDlgItem(IDC_BT_USER_SEARCH)->EnableWindow(bEnable);
	GetDlgItem(IDC_ASTERIC)->ShowWindow(bEnable);
	GetDlgItem(IDC_ASTERIC2)->ShowWindow(bEnable);
	GetDlgItem(IDC_ASTERIC4)->ShowWindow(bEnable);
	GetDlgItem(IDC_MANDATORYINDICATION)->ShowWindow(bEnable);
	m_bShowMandatoryFields = bEnable;
}

/************************************************************
* Validate App agent and FX user option to enable/disable 	*
* user configuration group box controls						*
************************************************************/
void CInMageLogonSettings::ValidateUserConfigGroup(void)
{
	EnableUserConfigGroup(
		((CButton*)GetDlgItem(IDC_CHECK_APPAGENT_USER))->GetCheck() == BST_CHECKED ||
		((CButton*)GetDlgItem(IDC_RADIO_FX_USER_ACOUNT))->GetCheck() == BST_CHECKED );
}

bool CInMageLogonSettings::IsAgentConfigChanged()
{
	//ValidateAgentConfigChange();
	return m_bChangeAttempted;
}

void CInMageLogonSettings::ResetConfigChangeStatus(bool bStatus)
{
	m_bChangeAttempted = bStatus;
}

HBRUSH CInMageLogonSettings::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{   
   HBRUSH hBrushHandle = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);  
   if(m_bShowMandatoryFields)
   {
	   if((pWnd->GetDlgCtrlID() == IDC_ASTERIC) || (pWnd->GetDlgCtrlID() == IDC_ASTERIC2) || (pWnd->GetDlgCtrlID() == IDC_ASTERIC4))   
	   {        
		  pDC->SetTextColor(RGB(127, 0, 0)); 
	   }
   }
   return hBrushHandle;
}