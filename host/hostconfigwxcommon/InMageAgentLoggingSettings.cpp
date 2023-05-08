#include "stdafx.h"
#include "hostconfigwxcommon.h"
#include "InMageAgentLoggingSettings.h"

#include <sstream>

int const SV_MAX_LOCAL_LOG_LEVEL = 7;
int const SV_MAX_REMOTE_LOG_LEVEL = 3;

IMPLEMENT_DYNAMIC(CInMageAgentLoggingSettings, CPropertyPage)

CInMageAgentLoggingSettings::CInMageAgentLoggingSettings()
	: CPropertyPage(CInMageAgentLoggingSettings::IDD)
	, m_vxLocalLoglevel(_T(""))
	, m_vxRemoteLoglevel(_T(""))
	, m_fxLoglevel(_T(""))
{
	m_bChangeAttempted = false;
}

CInMageAgentLoggingSettings::~CInMageAgentLoggingSettings()
{
}

void CInMageAgentLoggingSettings::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_SPIN_LOCAL_LOG_LEVEL, m_spLocalLoglevel);
	DDX_Control(pDX, IDC_SPIN_REMOTE_LOG_LEVEL, m_spRemoteLoglevel);
	DDX_Control(pDX, IDC_SPIN_FX_LOG_LEVEL, m_spFxLoglevel);
	DDX_Text(pDX, IDC_EDIT_LOCAL_LOG_LEVEL, m_vxLocalLoglevel);
	DDV_MaxChars(pDX, m_vxLocalLoglevel, 1);
	DDX_Text(pDX, IDC_EDIT_REMOTE_LOG_LEVEL, m_vxRemoteLoglevel);
	DDV_MaxChars(pDX, m_vxRemoteLoglevel, 1);
	DDX_Text(pDX, IDC_EDIT_FX_LOG_LEVEL, m_fxLoglevel);
	DDV_MaxChars(pDX, m_fxLoglevel, 1);
	DDX_Control(pDX, IDC_VX_LOGGING_GROUPBOX, m_vxLogGroupBox);
	DDX_Control(pDX, IDC_FX_LOGGING_GROUPBOX, m_fxLogGroupBox);
	DDX_Control(pDX, IDC_STATIC_LOCAL_LOGGING, m_vxLocalStaticText);
	DDX_Control(pDX, IDC_STATIC_REMOTE_LOG_LEVEL, m_vxRemoteStaticText);
	DDX_Control(pDX, IDC_EDIT_LOCAL_LOG_LEVEL, m_vxLocalEditCtrl);
	DDX_Control(pDX, IDC_EDIT_REMOTE_LOG_LEVEL, m_vxRemoteEditCtrl);
	DDX_Control(pDX, IDC_STATIC_FX_LOG_LEVEL, m_fxLogStaticText);
	DDX_Control(pDX, IDC_EDIT_FX_LOG_LEVEL, m_fxLogEditCtrl);
}


BEGIN_MESSAGE_MAP(CInMageAgentLoggingSettings, CPropertyPage)
	ON_EN_CHANGE(IDC_EDIT_LOCAL_LOG_LEVEL, &CInMageAgentLoggingSettings::OnEnChangeEditLocalLogLevel)
	ON_EN_CHANGE(IDC_EDIT_REMOTE_LOG_LEVEL, &CInMageAgentLoggingSettings::OnEnChangeEditRemoteLogLevel)
	ON_EN_CHANGE(IDC_EDIT_FX_LOG_LEVEL, &CInMageAgentLoggingSettings::OnEnChangeEditFxLogLevel)
END_MESSAGE_MAP()

/****************************************************
* Event handler for VX local log level changes		*
* Enable Apply button if change attempted			*
****************************************************/
void CInMageAgentLoggingSettings::OnEnChangeEditLocalLogLevel()
{
	UINT modifiedLocalLogLevel = GetDlgItemInt(IDC_EDIT_LOCAL_LOG_LEVEL);	
	if(modifiedLocalLogLevel != m_vxLocalLoglevel)
	{
		m_bChangeAttempted = true;
		SetModified();
	}
}

/****************************************************
* Event handler for VX remote log level changes		*
* Enable Apply button if change attempted			*
****************************************************/
void CInMageAgentLoggingSettings::OnEnChangeEditRemoteLogLevel()
{
	UINT modifiedRemoteLogLevel = GetDlgItemInt(IDC_EDIT_REMOTE_LOG_LEVEL);	
	if(modifiedRemoteLogLevel != m_vxRemoteLoglevel)
	{
		m_bChangeAttempted = true;
		SetModified();
	}
}

/****************************************************
* Event handler for FX log level changes			*
* Enable Apply button if change attempted			*
****************************************************/
void CInMageAgentLoggingSettings::OnEnChangeEditFxLogLevel()
{
	UINT modifiedFxLogLevel = GetDlgItemInt(IDC_EDIT_FX_LOG_LEVEL);	
	if(modifiedFxLogLevel != m_fxLoglevel)
	{
		m_bChangeAttempted = true;
		SetModified();
	}
}

/****************************************************************************************
* Based on scout installation code display controls when Logging tab is initialized		*
****************************************************************************************/
BOOL CInMageAgentLoggingSettings::OnInitDialog()
{
	BOOL bInitLogSettings = CPropertyPage::OnInitDialog();

	if(CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode == SCOUT_VX)
	{
		//Hide FX logging group box and it's controls
		m_fxLogGroupBox.ShowWindow(SW_HIDE);
		m_fxLogStaticText.ShowWindow(SW_HIDE);
		m_fxLogEditCtrl.ShowWindow(SW_HIDE);
		m_spFxLoglevel.ShowWindow(SW_HIDE);
	}
	else if(CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode == SCOUT_FX)
	{
		//Hide VX logging group box and it's controls
		m_vxLogGroupBox.ShowWindow(SW_HIDE);
		m_vxLocalStaticText.ShowWindow(SW_HIDE);
		m_vxRemoteStaticText.ShowWindow(SW_HIDE);
		m_vxLocalEditCtrl.ShowWindow(SW_HIDE);
		m_vxRemoteEditCtrl.ShowWindow(SW_HIDE);
		m_spLocalLoglevel.ShowWindow(SW_HIDE);
		m_spRemoteLoglevel.ShowWindow(SW_HIDE);

		//Move FX logging group box upward
		m_fxLogGroupBox.MoveWindow(15,40,350,70);
		m_fxLogStaticText.MoveWindow(25,70,65,30);
		m_fxLogEditCtrl.MoveWindow(95,65,65,22);
		m_spFxLoglevel.MoveWindow(158,65,17,22);
	}

	//Set range for spin control
	if(CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode != SCOUT_FX)
	{		
		m_spLocalLoglevel.SetRange(0,SV_MAX_LOCAL_LOG_LEVEL);
		m_spRemoteLoglevel.SetRange(0,SV_MAX_REMOTE_LOG_LEVEL);
	}
	if(CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode != SCOUT_VX)
			m_spFxLoglevel.SetRange(0,SV_MAX_LOCAL_LOG_LEVEL);

	return bInitLogSettings;
}

/********************************************************************
* Update and validate the config data when tab/page lose focus		*
********************************************************************/
BOOL CInMageAgentLoggingSettings::OnKillActive()
{
	UpdateData();
	ValidateAgentConfigChange();
	if(!m_bChangeAttempted)
		SetModified(FALSE);
	return CPropertyPage::OnKillActive();
}

/********************************************************************
* Checks whether the config data is really changed or not			*
********************************************************************/
bool CInMageAgentLoggingSettings::ValidateAgentConfigChange()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n", __FUNCTION__);
	if(!m_bChangeAttempted)
	{
		const LOGGING_SETTINGS& agentLogSettings = CInmConfigData::GetInmConfigInstance().GetAgentLoggingSettings();
		bool bNoChange = true;

		if(CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode != SCOUT_FX)
		{
			bNoChange = (agentLogSettings.vxLocalLogLevel == atoi(m_vxLocalLoglevel.GetString())) && (agentLogSettings.vxRemoteLogLevel == atoi(m_vxRemoteLoglevel.GetString()));
			if(!bNoChange)
			{
				DebugPrintf(SV_LOG_DEBUG,"Old Vx local log level = %d.\n", agentLogSettings.vxLocalLogLevel);
				DebugPrintf(SV_LOG_DEBUG,"New Vx local log level = %d.\n", atoi(m_vxLocalLoglevel.GetString()));
				DebugPrintf(SV_LOG_DEBUG,"Old Vx remote log level = %d.\n", agentLogSettings.vxRemoteLogLevel);
				DebugPrintf(SV_LOG_DEBUG,"New Vx remote log level = %d.\n", atoi(m_vxRemoteLoglevel.GetString()));
			}
		}

		if(CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode != SCOUT_VX)
		{
			bNoChange = bNoChange && (agentLogSettings.fxLogLevel == atoi(m_fxLoglevel.GetString()));
			if(!bNoChange)
			{
				DebugPrintf(SV_LOG_DEBUG,"Old Fx log level = %d.\n", agentLogSettings.fxLogLevel);
				DebugPrintf(SV_LOG_DEBUG,"New Fx log level = %d.\n", atoi(m_fxLoglevel.GetString()));
			}
		}

		if(bNoChange)
		{
			DebugPrintf(SV_LOG_DEBUG,"No change attempted in Logging tab.\n");
			m_bChangeAttempted = false;
		}
		else
		{
			m_bChangeAttempted = true;
			DebugPrintf(SV_LOG_DEBUG,"Change attempted in Logging tab.\n");
		}
	}
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", __FUNCTION__);
	return m_bChangeAttempted;
}

/************************************************************************************
* Do config data exchange from control variable to structure to persist the data	*
************************************************************************************/
void CInMageAgentLoggingSettings::SyncAgentConfigData(LOGGING_SETTINGS &agentLogSettings)
{
	if(CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode != SCOUT_FX)
	{
		agentLogSettings.vxLocalLogLevel = atoi(m_vxLocalLoglevel.GetString());
		if(agentLogSettings.vxLocalLogLevel > SV_MAX_LOCAL_LOG_LEVEL)
		{
			agentLogSettings.vxLocalLogLevel = SV_MAX_LOCAL_LOG_LEVEL;
			m_vxLocalLoglevel.Format(_T("%d"),SV_MAX_LOCAL_LOG_LEVEL);
			std::stringstream localLogLevelText;
			localLogLevelText << SV_MAX_LOCAL_LOG_LEVEL;
			m_vxLocalEditCtrl.SetWindowText(localLogLevelText.str().c_str());
		}
		else if(agentLogSettings.vxLocalLogLevel == 0)
		{
			m_vxLocalLoglevel.Format(_T("%d"),agentLogSettings.vxLocalLogLevel);
			m_vxLocalEditCtrl.SetWindowText("0");
		}
		agentLogSettings.vxRemoteLogLevel = atoi(m_vxRemoteLoglevel.GetString());
		if(agentLogSettings.vxRemoteLogLevel > SV_MAX_REMOTE_LOG_LEVEL)
		{
			agentLogSettings.vxRemoteLogLevel = SV_MAX_REMOTE_LOG_LEVEL;
			m_vxRemoteLoglevel.Format(_T("%d"),SV_MAX_REMOTE_LOG_LEVEL);
			std::stringstream remoteLogLevelText;
			remoteLogLevelText << SV_MAX_REMOTE_LOG_LEVEL;
			m_vxRemoteEditCtrl.SetWindowText(remoteLogLevelText.str().c_str());
		}
		else if(agentLogSettings.vxRemoteLogLevel == 0)
		{
			m_vxRemoteLoglevel.Format(_T("%d"),agentLogSettings.vxRemoteLogLevel);
			m_vxRemoteEditCtrl.SetWindowText("0");
		}
	}

	if(CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode != SCOUT_VX)
	{
		agentLogSettings.fxLogLevel = atoi(m_fxLoglevel.GetString());
		if(agentLogSettings.fxLogLevel > SV_MAX_LOCAL_LOG_LEVEL)
		{
			agentLogSettings.fxLogLevel = SV_MAX_LOCAL_LOG_LEVEL;
			m_fxLoglevel.Format(_T("%d"),SV_MAX_LOCAL_LOG_LEVEL);
			std::stringstream localLogLevelText;
			localLogLevelText << SV_MAX_LOCAL_LOG_LEVEL;
			m_fxLogEditCtrl.SetWindowText(localLogLevelText.str().c_str());
		}
		else if(agentLogSettings.fxLogLevel == 0)
		{
			m_fxLoglevel.Format(_T("%d"),agentLogSettings.fxLogLevel);
			m_fxLogEditCtrl.SetWindowText("0");
		}
	}
}

/********************************************************************
* Do config data exchange from persisted data to control variables	*
********************************************************************/
void CInMageAgentLoggingSettings::SyncAgentConfigData()
{
	const LOGGING_SETTINGS &agentLoggingSettings = CInmConfigData::GetInmConfigInstance().GetAgentLoggingSettings();

	if(CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode != SCOUT_FX)
	{
		m_vxLocalLoglevel.Format(_T("%d"),agentLoggingSettings.vxLocalLogLevel);
		m_vxRemoteLoglevel.Format(_T("%d"),agentLoggingSettings.vxRemoteLogLevel);
	}

	if(CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode != SCOUT_VX)
	{
		m_fxLoglevel.Format(_T("%d"),agentLoggingSettings.fxLogLevel);
	}
}

bool CInMageAgentLoggingSettings::IsAgentConfigChanged()
{
	//ValidateAgentConfigChange();
	return m_bChangeAttempted;
}

void CInMageAgentLoggingSettings::ResetConfigChangeStatus(bool bStatus)
{
	m_bChangeAttempted = bStatus;
}

