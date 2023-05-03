#pragma once
#include <string>
#include <vector>
#include <ntsecapi.h>
#include <winsvc.h>

#define INM_USE_DEBUG_LOG_VIEWER	0
#define INM_USE_DEBUG_LOG			1
#define INM_FX_SERVICE_NAME			"frsvc"
#define INM_LOCAL_SYSTEM_ACC		"LocalSystem"
#define INM_SAM_REG_FAILOVER_KEY	"SAM\\SV Systems\\Failover"
#define INM_SVSYSTEMS_REGKEY		"SOFTWARE\\SV Systems"
#define INM_FILEREPL_REGKEY			"SOFTWARE\\SV Systems\\FileReplicationAgent"

struct CX_SETTINGS{
	bool bShowCxSettingsPage;
	bool bCxUseHttps;
	bool bCxNatHostEnabled;
	bool bCxNatIpEnabled;
	CString inmCxPort;
	CString cxIP;
	CString cx_NatIP;
	CString cx_NatHostName;
	CString cx_Passphrase;

	CX_SETTINGS()
		:bCxUseHttps(false),
		bShowCxSettingsPage(true),
		bCxNatHostEnabled(false),
		bCxNatIpEnabled(false),
		inmCxPort(_T("")),
		cxIP(_T("")),
		cx_NatIP(_T("")),
		cx_NatHostName(_T("")),
		cx_Passphrase(_T(""))
	{
	}
};

struct AGENT_SETTINGS {
	CString vxAgentCacheDir;

	AGENT_SETTINGS()
		:vxAgentCacheDir(_T(""))
	{
	}
};

struct LOGGING_SETTINGS {	
	int vxLocalLogLevel;
	int vxRemoteLogLevel;
	int fxLogLevel;	

	LOGGING_SETTINGS()
		:vxLocalLogLevel(0),
		vxRemoteLogLevel(0),
		fxLogLevel(0)
	{
	}
};

struct LOGON_SETTINGS {
	CString user_name;
	CString user_password;
	bool bAppAgentUserCheck;
	bool bFxLocalSystemAct;
	bool bFxUserAct;

	LOGON_SETTINGS()
		: bAppAgentUserCheck(false),
		bFxLocalSystemAct(false),
		bFxUserAct(false),
		user_name(_T("")),
		user_password(_T(""))
	{ }
};

enum SERVICE_STARTUP_TYPE
{
	AUTOMATIC_DELAYED_START = 2,
	AUTOMATIC = 2,
	MANUAL = 3,
	DISABLED = 4,
};

class CInmConfigData
{
	static CInmConfigData m_sConfData;

	CInmConfigData(void) { }

	friend class CInMageConfigPropertySheet;

	//Structure objects. Each objects will represent each tab in host config ui
	CX_SETTINGS m_cxSettings;
	AGENT_SETTINGS m_vxAgentSettings;
	LOGGING_SETTINGS m_agentLoggingSettings;
	LOGON_SETTINGS m_agentLogonSettings;

	CString m_hostConfigErrMsg;
	DWORD m_winApiLastError;

	//Read config functions
	BOOL ReadCXSettings();
	BOOL ReadCXSettingsFromRegistry();
	BOOL SaveCXSettings();
	BOOL ReadVxAgentSettings();
	BOOL ReadAgentLoggingSettings();
	BOOL ReadAgentLogonSettings();
	BOOL ReadFxConfiguration(std::string &logonAccName, bool &bLocalSysAccount);

	//Save config functions
	BOOL RestartInMageScoutService(const std::string &inmScoutServiceName);
	BOOL UpdateCXSettingsToRegistry(const CX_SETTINGS &cxSettings);
	BOOL UpdateCXSettingsToCxPsConf(const CX_SETTINGS &cxSettings);
	BOOL IsScoutServiceRestartRequired(const CX_SETTINGS &cxSettings);
	BOOL SaveVxAgentSettings();
	BOOL SaveAgentLoggingSettings();
	BOOL SaveAgentLogonSettings();
	BOOL VerifyScoutAgentServiceUserLogon(const std::string& userName, const std::string& domain, const std::string& password, HANDLE &agentLogonUserToken);
	int GrantLogOnRightsToUser(const char *inmUserAccName);
	HRESULT GetAgentUserAccountSid(LPCWSTR inmUserAccountName,PSID * inmUserAccountSid);
	void InitializeInmLsaString(PLSA_UNICODE_STRING inmLsaUnicodeString,LPWSTR inmLsaString);
	BOOL UpdateFxConfiguration(bool bLocalSysAccount);
	BOOL ValidatePassphrase();

	BOOL ReadCredentials(std::string &strDomainName,std::string &strUserName,std::string &strPassword,std::string &errorMsg);
	BOOL GetServiceConfiguration(const std::string &serviceName, LPQUERY_SERVICE_CONFIG  &lpSrvcConfig);

	bool m_isGlobalDataChanged;
	bool m_isAgentDataChanged;
	bool m_isLoggingDataChanged;
	bool m_isLogonDataChanged;
	BOOL m_bPassphraseValidated;

public:
	
	UINT scoutAgentInstallCode;
	bool m_bEnableHttps;

	std::string GetAgentInstallationPath();
	bool IsAgentRegistered();

	//Thread Procedures to read and save data
	static UINT ConfigReadThreadProc(LPVOID);
	static UINT ConfigSaveThreadProc(LPVOID);
	
	static CInmConfigData& GetInmConfigInstance();

	const CX_SETTINGS& GetCxSettings() const
	{
		return m_cxSettings;
	}

	const AGENT_SETTINGS& GetVxAgentSettings() const
	{
		return m_vxAgentSettings;
	}

	const LOGGING_SETTINGS& GetAgentLoggingSettings() const
	{
		return m_agentLoggingSettings;
	}

	const LOGON_SETTINGS& GetAgentLogonSettings() const
	{
		return m_agentLogonSettings;
	}

	CString GetConfigErrorMsg() const
	{
		return m_hostConfigErrMsg;
	}
	void SetConfigErrorMsg(CString errMsg)
	{
		m_hostConfigErrMsg = errMsg;
	}
	std::string GetWinApiErrorMsg() const
	{
		std::string errStr = "";
		LPVOID lpHostConfigErrMsg = NULL;
		if(FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, m_winApiLastError, 0, (LPTSTR)&lpHostConfigErrMsg, 0, NULL))
		{
			errStr = reinterpret_cast<char*>(&lpHostConfigErrMsg);
			LocalFree(lpHostConfigErrMsg);
		}
		return errStr;
	}
	void SetWinApiErrorMsg(DWORD winApiErr)
	{
		m_winApiLastError = winApiErr;
	}
	void SetGlobalDataChangeStatus(bool bChanged)
	{
		m_isGlobalDataChanged = bChanged;
	}
	void SetAgentDataChangeStatus(bool bChanged)
	{
		m_isAgentDataChanged = bChanged;
	}
	void SetLoggingDataChangeStatus(bool bChanged)
	{
		m_isLoggingDataChanged = bChanged;
	}
	void SetLogonDataChangeStatus(bool bChanged)
	{
		m_isLogonDataChanged = bChanged;
	}
	bool IsPassphraseValidated(){
		return m_bPassphraseValidated;
	}
};