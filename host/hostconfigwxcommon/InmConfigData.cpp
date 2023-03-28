#include "stdafx.h"
#include <winsvc.h>
#include "InmConfigData.h"
#include "LocalConfigurator.h"
#include "inmageex.h"
#include "globs.h"
#include "InMageSecurity.h"
#include "mq.h"
#include "genpassphrase.h"
#include "hostconfigprocessmgr.h"
#include "csgetfingerprint.h"
using namespace std;

#define DEFAULT_HTTPS_PORT "443"
#define DEFAULT_HTTP_PORT "80"


CInmConfigData CInmConfigData::m_sConfData;

CInmConfigData& CInmConfigData::GetInmConfigInstance()
{
	return m_sConfData;
}

/********************************************************************
* Worker thread to read config data from persisted location and		*
* update host config UI. Post success/failure message to host config*
********************************************************************/
UINT CInmConfigData::ConfigReadThreadProc(LPVOID lpWnd)
{
	CWnd *pConfigPropertySheet = (CWnd*)lpWnd;

	BOOL bReadAgentSettings = TRUE;
	//Reading & updating CInmConfigData logic starts here
	BOOL bReadGlobalSettings = CInmConfigData::GetInmConfigInstance().ReadCXSettings();
	if(CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode != SCOUT_FX)
		bReadAgentSettings = CInmConfigData::GetInmConfigInstance().ReadVxAgentSettings();
	BOOL bReadLoggingSettings = CInmConfigData::GetInmConfigInstance().ReadAgentLoggingSettings();
	BOOL bReadLogonSettings = CInmConfigData::GetInmConfigInstance().ReadAgentLogonSettings();

	//Post result to main window
	if(bReadGlobalSettings && bReadAgentSettings && bReadLoggingSettings && bReadLogonSettings)
		pConfigPropertySheet->PostMessage(CONFIG_READ_SUCCESS);
	else
		pConfigPropertySheet->PostMessage(CONFIG_READ_FAILED);
	return 0;
}

/****************************************************************************
* Worker thread to save config data from host config UI and persist			*
* data in registry/drscout.conf. Post success/failure message to host config*
****************************************************************************/
UINT CInmConfigData::ConfigSaveThreadProc(LPVOID lpWnd)
{
	CWnd *pConfigPropertySheet = (CWnd*)lpWnd;

	BOOL bSaveAgentSettings = TRUE;
	BOOL bSaveGlobalSettings = TRUE;
	BOOL bSaveLoggingSettings = TRUE;
	BOOL bSaveLogonSettings = TRUE;
	CInmConfigData::GetInmConfigInstance().m_bPassphraseValidated = TRUE;

	do
	{		
	//Saving logic starts here
	if(CInmConfigData::GetInmConfigInstance().m_isGlobalDataChanged)
		{
			bSaveGlobalSettings = CInmConfigData::GetInmConfigInstance().ValidatePassphrase();
			if (FALSE == bSaveGlobalSettings){
				CInmConfigData::GetInmConfigInstance().m_bPassphraseValidated = FALSE;
				break;
			}
		bSaveGlobalSettings = CInmConfigData::GetInmConfigInstance().SaveCXSettings();
		}
	if(CInmConfigData::GetInmConfigInstance().m_isAgentDataChanged)
	{
		if(CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode != SCOUT_FX)
			bSaveAgentSettings = CInmConfigData::GetInmConfigInstance().SaveVxAgentSettings();
	}

	if(CInmConfigData::GetInmConfigInstance().m_isLoggingDataChanged)
		bSaveLoggingSettings = CInmConfigData::GetInmConfigInstance().SaveAgentLoggingSettings();

	if(CInmConfigData::GetInmConfigInstance().m_isLogonDataChanged)
		bSaveLogonSettings = CInmConfigData::GetInmConfigInstance().SaveAgentLogonSettings();
	} while (false);
	//Post result to main window
	if(bSaveGlobalSettings && bSaveAgentSettings && bSaveLoggingSettings && bSaveLogonSettings)
		pConfigPropertySheet->PostMessage(CONFIG_SAVE_SUCCESS);
	else
		pConfigPropertySheet->PostMessage(CONFIG_SAVE_FAILED);
	return 0;
}

/****************************************************************************
* Based on agent installation code, host config ui will read CX settings	*
* either from registry or drscout.conf file									*
****************************************************************************/
BOOL CInmConfigData::ReadCXSettings()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	BOOL bInitConfigPropertySheet = TRUE;
	try
	{			
		if(CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode == SCOUT_FX)
		{
			bInitConfigPropertySheet = ReadCXSettingsFromRegistry();
		}
		else
		{		
			LocalConfigurator localConfig;
			//CX_SETTINGS
			HTTP_CONNECTION_SETTINGS httpConSettings = localConfig.getHttp();		
			m_cxSettings.cxIP = CString(httpConSettings.ipAddress, _countof(httpConSettings.ipAddress));
			m_cxSettings.inmCxPort.Format(_T("%d"),httpConSettings.port);
			m_cxSettings.bCxUseHttps = localConfig.IsHttps();
			m_cxSettings.cx_NatHostName = CString(localConfig.getConfiguredHostname().c_str());
			m_cxSettings.cx_NatIP = CString(localConfig.getConfiguredIpAddress().c_str());
			if(0 == m_cxSettings.cx_NatIP.Compare("0"))
				m_cxSettings.cx_NatIP = _T("0.0.0.0");
			m_cxSettings.bCxNatHostEnabled = localConfig.getUseConfiguredHostname();
			m_cxSettings.bCxNatIpEnabled = localConfig.getUseConfiguredIpAddress();
			
		}
		string passphrase = securitylib::getPassphrase();
		m_cxSettings.cx_Passphrase = CString(passphrase.c_str());
	}
	catch ( ContextualException& ce )
	{
		bInitConfigPropertySheet = FALSE;
		SetConfigErrorMsg(ce.what());
		DebugPrintf(SV_LOG_ERROR, "Caught contextual exception = %s\n", ce.what());
	}
	catch( std::exception const& e )
	{
		bInitConfigPropertySheet = FALSE;
		SetConfigErrorMsg(e.what());
		DebugPrintf(SV_LOG_ERROR, "Caught exception = %s\n", e.what());
	}
	catch ( ... )
	{
		bInitConfigPropertySheet = FALSE;
		SetConfigErrorMsg(_T("Encountered unknown exception."));
		DebugPrintf(SV_LOG_ERROR, "Encountered unknown exception.\n");
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
	return bInitConfigPropertySheet;
}

/****************************************************************************
* Based on agent installation code, host config ui will save CX settings	*
* either to registry or drscout.conf file or both							*
* Restarts InMage scout services if either cxip or port number changes		*
****************************************************************************/
BOOL CInmConfigData::SaveCXSettings()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	BOOL rv = TRUE;
	bool isMasterTarget = false;
	if (scoutAgentInstallCode != SCOUT_FX)
	{
		LocalConfigurator lc;
		if (strcmp(MASTER_TARGET_ROLE, lc.getAgentRole().c_str()) == 0)
		{
			DebugPrintf(SV_LOG_DEBUG, "Agent role is MasterTarget\n");
			//Update settings to cxps conf
			if (!UpdateCXSettingsToCxPsConf(m_cxSettings))
			{
				rv = FALSE;
			}
			isMasterTarget = true;
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG, "Agent role is Agent\n");
		}
	}
	if(rv && scoutAgentInstallCode != SCOUT_FX)
	{
		try
		{
			LocalConfigurator localConfig;
			//CX_SETTINGS
			HTTP_CONNECTION_SETTINGS httpConSettings;
			strcpy_s(httpConSettings.ipAddress, _countof(httpConSettings.ipAddress), m_cxSettings.cxIP);
			httpConSettings.port = atoi(m_cxSettings.inmCxPort.GetString());
			localConfig.setHttp(httpConSettings);
			localConfig.setHttps(m_cxSettings.bCxUseHttps);
			localConfig.setConfiguredHostname(m_cxSettings.cx_NatHostName.GetString());
			localConfig.setConfiguredIpAddress(m_cxSettings.cx_NatIP.GetString());
			localConfig.setUseConfiguredHostname(m_cxSettings.bCxNatHostEnabled);
			localConfig.setUseConfiguredIpAddress(m_cxSettings.bCxNatIpEnabled);
		}
		catch ( ContextualException& ce )
		{
			rv = FALSE;
			SetConfigErrorMsg(ce.what());
			DebugPrintf(SV_LOG_ERROR, "Caught contextual exception = %s\n", ce.what());
		}
		catch( std::exception const& e )
		{
			rv = FALSE;
			SetConfigErrorMsg(e.what());
			DebugPrintf(SV_LOG_ERROR, "Caught exception = %s\n", e.what());
		}
		catch ( ... )
		{
			rv = FALSE;
			SetConfigErrorMsg("Encountered unknown exception.\n");
			DebugPrintf(SV_LOG_ERROR, "Encountered unknown exception.\n");
		}
	}
	if (rv)
	{
		BOOL bRestartScoutService = IsScoutServiceRestartRequired(m_cxSettings);
		//Update settings to registry
		if (!UpdateCXSettingsToRegistry(m_cxSettings))
		{
			rv = FALSE;
		}
		else
		{
			securitylib::writePassphrase(m_cxSettings.cx_Passphrase.GetString());
			m_cxSettings.cx_Passphrase.Delete(0, m_cxSettings.cx_Passphrase.GetLength());

			if (bRestartScoutService)
			{
				bool frsvcStart = true;
				bool svagentsSvcStart = true;
				bool appSvcStart = true;
				bool cxpsSvcStart = true;
				//Restart VX, FX , Application and cxpsprocessserver services
				if (scoutAgentInstallCode != SCOUT_VX)
				{
					LPQUERY_SERVICE_CONFIG lpSrvcConfig;
					BOOL bfunRetVal = GetServiceConfiguration(INM_FX_SERVICE_NAME, lpSrvcConfig);
					if (bfunRetVal)
					{
						//0x00000003 refers to Manual Start. Apart from this we expect StartType to be automatic
						//If Service Start Type is manual and if it is running, it has to be restarted by user.
						if ( SERVICE_STARTUP_TYPE::MANUAL != lpSrvcConfig->dwStartType )
						{
							if (!RestartInMageScoutService(INM_FX_SERVICE_NAME))
							{
								frsvcStart = false;
								DebugPrintf(SV_LOG_ERROR, "Failed to restart FX Agent service.Please restart manually to effect the changes\n");
							}
						}
					}
					else
					{
						frsvcStart = false;
					}
				}
				if (scoutAgentInstallCode != SCOUT_FX)
				{
					if (!RestartInMageScoutService("svagents"))
					{
						svagentsSvcStart = false;
						SetConfigErrorMsg("Failed to restart VX Agent service.\nPlease restart manually to effect the changes");
						DebugPrintf(SV_LOG_ERROR, "Failed to restart VX Agent service.Please restart manually to effect the changes\n");
					}

					if (!RestartInMageScoutService("InMage Scout Application Service"))
					{
						appSvcStart = false;
						SetConfigErrorMsg("Failed to restart Scout Application service.\nPlease restart manually to effect the changes");
						DebugPrintf(SV_LOG_ERROR, "Failed to restart Scout Application Agent service.Please restart manually to effect the changes\n");
					}
					if (isMasterTarget)
					{
						if (!RestartInMageScoutService("cxprocessserver"))
						{
							cxpsSvcStart = false;
							SetConfigErrorMsg("Failed to restart cxprocessserver service.\nPlease restart manually to effect the changes");
							DebugPrintf(SV_LOG_ERROR, "Failed to restart cxprocessserver service.Please restart manually to effect the changes\n");
						}
					}
				}
				if (!frsvcStart || !svagentsSvcStart || !appSvcStart || !cxpsSvcStart)
				{
					rv = FALSE;
				}
			}
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
	return rv;
}

/********************************************************
* Based on agent installation code, host config ui will *
* save CX settings to specific registry	hives			*
********************************************************/
BOOL CInmConfigData::UpdateCXSettingsToRegistry(const CX_SETTINGS &cxSettings)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	BOOL rv = TRUE;
	CString errStr;
	HKEY svSysKey;
	HKEY fileReplKey;

	LSTATUS svSysRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, INM_SVSYSTEMS_REGKEY, 0, KEY_READ | KEY_WRITE, &svSysKey);
	if(svSysRet != ERROR_SUCCESS)
	{
		SetWinApiErrorMsg(GetLastError());
		errStr.Format("Failed to open registry key to update global settings.\nErrorCode = %u", svSysRet);
		SetConfigErrorMsg(errStr);
		return FALSE;
	}

	//Update ServerHttpPort
	DWORD cxPort_Dw = atoi(cxSettings.inmCxPort.GetString());
	svSysRet = RegSetValueEx(svSysKey, SV_SERVER_HTTP_PORT_VALUE_NAME, 0, REG_DWORD, reinterpret_cast<const BYTE*> (&cxPort_Dw), sizeof(DWORD));
	if(svSysRet != ERROR_SUCCESS)
	{
		SetWinApiErrorMsg(GetLastError());
		errStr.Format("Failed to update Http port.\nErrorCode = %u", svSysRet);
		SetConfigErrorMsg(errStr);
		RegCloseKey(svSysKey);
		return FALSE;
	}

	//Update ServerName
	string cxip = cxSettings.cxIP.GetString();
	svSysRet = RegSetValueEx(svSysKey, SV_SERVER_NAME_VALUE_NAME, 0, REG_SZ, reinterpret_cast<const BYTE*> (cxip.c_str()), (lstrlen(cxip.c_str())+1)*sizeof(TCHAR));
	if(svSysRet != ERROR_SUCCESS)
	{
		SetWinApiErrorMsg(GetLastError());
		errStr.Format("Failed to update CX ip.\nErrorCode = %u", svSysRet);
		SetConfigErrorMsg(errStr);
		RegCloseKey(svSysKey);
		return FALSE;
	}
	
	////Update Https
	DWORD useHttps = cxSettings.bCxUseHttps ? TRUE : FALSE;
	svSysRet = RegSetValueEx(svSysKey, SV_HTTPS, 0, REG_DWORD, reinterpret_cast<const BYTE*> (&useHttps), sizeof(DWORD));
	if(svSysRet != ERROR_SUCCESS)
	{
		SetWinApiErrorMsg(GetLastError());
		errStr.Format("Failed to update Https state.\nErrorCode = %u", svSysRet);
		SetConfigErrorMsg(errStr);
		RegCloseKey(svSysKey);
		return FALSE;
	}

	RegCloseKey(svSysKey);

	if(scoutAgentInstallCode != SCOUT_VX)
	{
		LSTATUS fileReplRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, INM_FILEREPL_REGKEY, 0, KEY_READ | KEY_WRITE, &fileReplKey);

		if(fileReplRet != ERROR_SUCCESS)
		{
			SetWinApiErrorMsg(GetLastError());
			errStr.Format("Failed to open registry key to update global settings.\nErrorCode = %u", svSysRet);
			SetConfigErrorMsg(errStr);
			return FALSE;
		}
		//Update UseConfiguredHostname
		DWORD natHost = cxSettings.bCxNatHostEnabled ? TRUE : FALSE;
		fileReplRet = RegSetValueEx(fileReplKey, SV_USE_CONFIGURED_HOSTNAME, 0, REG_DWORD, reinterpret_cast<const BYTE*> (&natHost), sizeof(DWORD));
		if(fileReplRet != ERROR_SUCCESS)
		{
			SetWinApiErrorMsg(GetLastError());
			errStr.Format("Failed to update NAT host enable.\nErrorCode = %u", fileReplRet);
			SetConfigErrorMsg(errStr);
			RegCloseKey(fileReplKey);
			return FALSE;
		}

		//Update UseConfiguredIP
		DWORD natIp = cxSettings.bCxNatIpEnabled ? TRUE : FALSE;
		fileReplRet = RegSetValueEx(fileReplKey, SV_USE_CONFIGURED_IP, 0, REG_DWORD, reinterpret_cast<const BYTE*> (&natIp), sizeof(DWORD));
		if(fileReplRet != ERROR_SUCCESS)
		{
			SetWinApiErrorMsg(GetLastError());
			errStr.Format("Failed to update NAT IP enable.\nErrorCode = %u", fileReplRet);
			SetConfigErrorMsg(errStr);
			RegCloseKey(fileReplKey);
			return FALSE;
		}

		//Update ConfiguredHostname
		string hostName = cxSettings.cx_NatHostName.GetString();
		fileReplRet = RegSetValueEx(fileReplKey, SV_CONFIGURED_HOSTNAME, 0, REG_SZ, reinterpret_cast<const BYTE*> (hostName.c_str()), (lstrlen(hostName.c_str())+1)*sizeof(TCHAR));
		if(fileReplRet != ERROR_SUCCESS)
		{
			SetWinApiErrorMsg(GetLastError());
			errStr.Format("Failed to update Nat host name.\nErrorCode = %u", fileReplRet);
			SetConfigErrorMsg(errStr);
			RegCloseKey(fileReplKey);
			return FALSE;
		}

		//Update ConfiguredIP
		string natIpStr = cxSettings.cx_NatIP.GetString();
		fileReplRet = RegSetValueEx(fileReplKey, SV_CONFIGURED_IP, 0, REG_SZ, reinterpret_cast<const BYTE*> (natIpStr.c_str()), (lstrlen(natIpStr.c_str())+1)*sizeof(TCHAR));
		if(fileReplRet != ERROR_SUCCESS)
		{
			SetWinApiErrorMsg(GetLastError());
			errStr.Format("Failed to update NAT ip.\nErrorCode = %u", fileReplRet);
			SetConfigErrorMsg(errStr);
			RegCloseKey(fileReplKey);
			return FALSE;
		}
		RegCloseKey(fileReplKey);
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
	return rv;
}

/********************************************************
* Based on agent installation code, host config ui will *
* save CX settings to cxps.conf file		*
********************************************************/
BOOL CInmConfigData::UpdateCXSettingsToCxPsConf(const CX_SETTINGS &cxSettings)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	bool rv = TRUE;

	LocalConfigurator lc;

	std::string tmpPath = lc.getInstallPath() + std::string("\\") + "transport";

	//std::string tmpFile = tmpPath;
	//tmpFile += ACE_DIRECTORY_SEPARATOR_CHAR;
	//tmpFile += "tmpLog.log";

	std::string conffile = tmpPath + std::string("\\") + "cxps.conf";

	std::string cmd = std::string("\"") + lc.getInstallPath() + std::string("\\") + "transport" + std::string("\\") + "cxpscli.exe" + std::string("\"");
	cmd += std::string(" ") + "--conf" + std::string(" ") + std::string("\"") + conffile + std::string("\"");
	cmd += std::string(" ") + "--csip" + std::string(" ") + m_cxSettings.cxIP.GetString();

	if (m_cxSettings.bCxUseHttps)
	{
		cmd += std::string(" ") + "--cssecure" + std::string(" ") + "yes";
		cmd += std::string(" ") + "--cssslport" + std::string(" ") + m_cxSettings.inmCxPort.GetString();
	}
	if (!m_cxSettings.bCxUseHttps)
	{
		cmd += std::string(" ") + "--cssecure" + std::string(" ") + "no";
		cmd += std::string(" ") + "--csport" + std::string(" ") + m_cxSettings.inmCxPort.GetString();
	}


	DebugPrintf(SV_LOG_DEBUG, "ENTERED Command : %s\n", cmd.c_str());

	
	std::string errmsg;

	if (!execProcUsingPipe(cmd, errmsg))
	{
		rv = false;
		SetConfigErrorMsg(errmsg.c_str());
		DebugPrintf(SV_LOG_ERROR, "Failed to Execute cxpscli process.%s\n", errmsg.c_str());
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
	return rv;
}

/****************************************************
* Generic function to restart InMage Scout services	*
****************************************************/
BOOL CInmConfigData::RestartInMageScoutService(const std::string &inmScoutServiceName)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	BOOL bConfig = TRUE;
	CString errStr;
	SC_HANDLE inmageServiceHandle, inmSvcManagerHandle;
	
	//Get Service Control Manager(SCM) handle
	inmSvcManagerHandle = OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS);
	if(inmSvcManagerHandle == NULL)
	{
		SetConfigErrorMsg("Failed to open service control manager.");
		SetWinApiErrorMsg(GetLastError());
		DebugPrintf(SV_LOG_ERROR, "Failed to open service control manager.%s\n");
		return FALSE;
	}
	
	//Get service handle
	inmageServiceHandle = ::OpenService(inmSvcManagerHandle, inmScoutServiceName.c_str(), SERVICE_START | SERVICE_STOP | SERVICE_QUERY_STATUS);
	if(inmageServiceHandle == NULL)
	{
		errStr.Format("Failed to get service handle for the service = %s.", inmScoutServiceName.c_str());
		SetConfigErrorMsg(errStr);
		SetWinApiErrorMsg(GetLastError());

		//Release SCM handle and return false
		::CloseServiceHandle(inmSvcManagerHandle);
		return FALSE;
	}

	SERVICE_STATUS serviceStatus;
	bConfig = ::QueryServiceStatus(inmageServiceHandle, (LPSERVICE_STATUS) &serviceStatus);

	if(serviceStatus.dwCurrentState == SERVICE_RUNNING)
	{
		bConfig = ::ControlService(inmageServiceHandle,SERVICE_CONTROL_STOP, &serviceStatus);
		// Wait for service to stop
		int retry = 1;
		SERVICE_STATUS_PROCESS inmageServiceStatus;
		DWORD inmSvcActualSize = 0;
		do
		{
			Sleep(1000);
		}while(::QueryServiceStatusEx(inmageServiceHandle,SC_STATUS_PROCESS_INFO,(LPBYTE)&inmageServiceStatus,sizeof(inmageServiceStatus),&inmSvcActualSize) &&(SERVICE_STOPPED != inmageServiceStatus.dwCurrentState)&& (retry++ <= 600));
		if (::QueryServiceStatusEx(inmageServiceHandle, SC_STATUS_PROCESS_INFO, (LPBYTE)&inmageServiceStatus, sizeof(inmageServiceStatus),&inmSvcActualSize) && SERVICE_STOPPED != inmageServiceStatus.dwCurrentState) 
		{
			SetConfigErrorMsg("Failed to stop service in timely fashion");
			SetWinApiErrorMsg(GetLastError());

			//Release service handles
			::CloseServiceHandle(inmageServiceHandle);
			::CloseServiceHandle(inmSvcManagerHandle);
			return FALSE;
		}
	}

	//Start the service
	bConfig = ::StartService(inmageServiceHandle, NULL, NULL);
	if(!bConfig)
	{			
		errStr.Format("Failed to start service.\nPlease start %s the service manually.", inmScoutServiceName.c_str());
		SetConfigErrorMsg(errStr);
		SetWinApiErrorMsg(GetLastError());
	}
	//Release service handles
	::CloseServiceHandle(inmageServiceHandle);
	::CloseServiceHandle(inmSvcManagerHandle);
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
	return bConfig;
}

/********************************************************
* Based on agent installation code, host config ui will *
* read CX settings from specific registry hive			*
********************************************************/
BOOL CInmConfigData::ReadCXSettingsFromRegistry()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	BOOL rv = TRUE;
	CString errStr;
	HKEY svSysKey;
	HKEY fileReplKey;

	char cxIP[MAX_PATH];
	char confHostName[MAX_PATH];
	char confIp[MAX_PATH];
	DWORD cxSettingsValueSize;
	DWORD cxSettingsValueType;
	DWORD cxSettingsDwordValue;

	LSTATUS svSysRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, INM_SVSYSTEMS_REGKEY, 0, KEY_READ, &svSysKey);
	
	if(svSysRet != ERROR_SUCCESS)
	{
		SetWinApiErrorMsg(GetLastError());
		errStr.Format("Failed to open registry key to read global settings.\nErrorCode = %u", svSysRet);
		SetConfigErrorMsg(errStr);
		return FALSE;
	}

	//Read ServerHttpPort
	cxSettingsValueSize = sizeof(DWORD);
	cxSettingsValueType = REG_DWORD;
	svSysRet = RegQueryValueEx(svSysKey, SV_SERVER_HTTP_PORT_VALUE_NAME, 0, &cxSettingsValueType, (LPBYTE) &cxSettingsDwordValue, &cxSettingsValueSize);
	if(svSysRet != ERROR_SUCCESS)
	{
		SetWinApiErrorMsg(GetLastError());
		errStr.Format("Failed to read Http port.\nErrorCode = %u", svSysRet);
		SetConfigErrorMsg(errStr);
		RegCloseKey(svSysKey);
		return FALSE;
	}
	m_cxSettings.inmCxPort.Format(_T("%d"), cxSettingsDwordValue);

	//Read ServerName
	cxSettingsValueSize = MAX_PATH;
	cxSettingsValueType = REG_SZ;
	svSysRet = RegQueryValueEx(svSysKey, SV_SERVER_NAME_VALUE_NAME, 0, &cxSettingsValueType, (LPBYTE)&cxIP, &cxSettingsValueSize);
	if(svSysRet == ERROR_SUCCESS)
	{
		m_cxSettings.cxIP = cxIP;
	}
	if(svSysRet != ERROR_SUCCESS)
	{
		SetWinApiErrorMsg(GetLastError());
		errStr.Format("Failed to read CX ip.\nErrorCode = %u", svSysRet);
		SetConfigErrorMsg(errStr);
		RegCloseKey(svSysKey);
		return FALSE;
	}
	
	//Read Https	
	cxSettingsValueSize = sizeof(DWORD);
	cxSettingsValueType = REG_DWORD;
	svSysRet = RegQueryValueEx(svSysKey, SV_HTTPS, 0, &cxSettingsValueType, (LPBYTE) &cxSettingsDwordValue, &cxSettingsValueSize);
	if(svSysRet == ERROR_FILE_NOT_FOUND)
		m_cxSettings.bCxUseHttps = false;
	else if(svSysRet == ERROR_SUCCESS)		
		m_cxSettings.bCxUseHttps = cxSettingsDwordValue ? true : false;
	else
	{
		SetWinApiErrorMsg(GetLastError());
		errStr.Format("Failed to read Https state.\nErrorCode = %u", svSysRet);
		SetConfigErrorMsg(errStr);
		RegCloseKey(svSysKey);
		return FALSE;
	}
	RegCloseKey(svSysKey);

	if(scoutAgentInstallCode != SCOUT_VX)
	{
		LSTATUS fileReplRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, INM_FILEREPL_REGKEY, 0, KEY_READ, &fileReplKey);

		if(fileReplRet != ERROR_SUCCESS)
		{
			SetWinApiErrorMsg(GetLastError());
			errStr.Format("Failed to open registry key to read global settings.\nErrorCode = %u", svSysRet);
			SetConfigErrorMsg(errStr);
			return FALSE;
		}
		//Read UseConfiguredHostname
		cxSettingsValueSize = sizeof(DWORD);
		cxSettingsValueType = REG_DWORD;
		fileReplRet = RegQueryValueEx(fileReplKey, SV_USE_CONFIGURED_HOSTNAME, 0, &cxSettingsValueType, (LPBYTE) (&cxSettingsDwordValue), &cxSettingsValueSize);
		if(fileReplRet != ERROR_SUCCESS)
		{
			SetWinApiErrorMsg(GetLastError());
			errStr.Format("Failed to read NAT host enable.\nErrorCode = %u", fileReplRet);
			SetConfigErrorMsg(errStr);
			RegCloseKey(fileReplKey);
			return FALSE;
		}
		m_cxSettings.bCxNatHostEnabled = cxSettingsDwordValue ? true : false;

		//Read UseConfiguredIP
		cxSettingsValueSize = sizeof(DWORD);
		cxSettingsValueType = REG_DWORD;
		fileReplRet = RegQueryValueEx(fileReplKey, SV_USE_CONFIGURED_IP, 0, &cxSettingsValueType, (LPBYTE) &cxSettingsDwordValue, &cxSettingsValueSize);
		if(fileReplRet != ERROR_SUCCESS)
		{
			SetWinApiErrorMsg(GetLastError());
			errStr.Format("Failed to read NAT IP enable.\nErrorCode = %u", fileReplRet);
			SetConfigErrorMsg(errStr);
			RegCloseKey(fileReplKey);
			return FALSE;
		}
		m_cxSettings.bCxNatIpEnabled = cxSettingsDwordValue ? true : false;

		//Read ConfiguredHostname
		cxSettingsValueSize = MAX_PATH;
		cxSettingsValueType = REG_SZ;
		fileReplRet = RegQueryValueEx(fileReplKey, SV_CONFIGURED_HOSTNAME, 0, &cxSettingsValueType, (LPBYTE)&confHostName, &cxSettingsValueSize);
		if((fileReplRet == ERROR_SUCCESS) || (fileReplRet == ERROR_MORE_DATA))
		{
			m_cxSettings.cx_NatHostName = confHostName;
		}
		if(fileReplRet != ERROR_SUCCESS)
		{
			SetWinApiErrorMsg(GetLastError());
			errStr.Format("Failed to read Nat host name.\nErrorCode = %u", fileReplRet);
			SetConfigErrorMsg(errStr);
			RegCloseKey(fileReplKey);
			return FALSE;
		}

		//Read ConfiguredIP
		cxSettingsValueSize = MAX_PATH;
		cxSettingsValueType = REG_SZ;
		fileReplRet = RegQueryValueEx(fileReplKey, SV_CONFIGURED_IP, 0, &cxSettingsValueType, (LPBYTE)&confIp, &cxSettingsValueSize);
		if((fileReplRet == ERROR_SUCCESS) || (fileReplRet == ERROR_MORE_DATA))
		{
			m_cxSettings.cx_NatIP = confIp;
			if(0 == m_cxSettings.cx_NatIP.Compare("0"))
				m_cxSettings.cx_NatIP = "0.0.0.0";
		}
		if(fileReplRet != ERROR_SUCCESS)
		{
			SetWinApiErrorMsg(GetLastError());
			errStr.Format("Failed to read NAT ip.\nErrorCode = %u", fileReplRet);
			SetConfigErrorMsg(errStr);
			RegCloseKey(fileReplKey);
			return FALSE;
		}
		RegCloseKey(fileReplKey);
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
	return rv;
}

/************************************************************************
* Checks whether InMage Scout service restart is really required		*
* Condition: Restart service only if either CX ip or port is changed	*
************************************************************************/
BOOL CInmConfigData::IsScoutServiceRestartRequired(const CX_SETTINGS &cxSettings)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	BOOL bRestartScoutService = FALSE;
	CString errStr;
	HKEY svSysKey;

	char cxIp[MAX_PATH];
	DWORD cxSettingsValueSize;
	DWORD cxSettingsValueType;
	DWORD cxSettingsDwordValue;

	int existingPort = 0;
	CString existingIp;
	bool existingHttps = false;

	try
	{
		LSTATUS svSysRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, INM_SVSYSTEMS_REGKEY, 0, KEY_READ, &svSysKey);
		
		if(svSysRet != ERROR_SUCCESS)
		{
			SetWinApiErrorMsg(GetLastError());
			errStr.Format("Failed to open registry key to read global settings.\nErrorCode = %u", svSysRet);
			SetConfigErrorMsg(errStr);
			DebugPrintf(SV_LOG_ERROR, "Failed to open registry key to read global settings. ErrorCode = %u", svSysRet);
			DebugPrintf(SV_LOG_ERROR, "Error description = %s", GetWinApiErrorMsg().c_str());
			return FALSE;
		}

		//Read ServerHttpPort
		cxSettingsValueSize = sizeof(DWORD);
		cxSettingsValueType = REG_DWORD;
		svSysRet = RegQueryValueEx(svSysKey, SV_SERVER_HTTP_PORT_VALUE_NAME, 0, &cxSettingsValueType, (LPBYTE) &cxSettingsDwordValue, &cxSettingsValueSize);
		if(svSysRet != ERROR_SUCCESS)
		{
			SetWinApiErrorMsg(GetLastError());
			errStr.Format("Failed to read Http port.\nErrorCode = %u", svSysRet);
			SetConfigErrorMsg(errStr);
			DebugPrintf(SV_LOG_ERROR, "Failed to read http port number. ErrorCode = %u", svSysRet);
			DebugPrintf(SV_LOG_ERROR, "Error description = %s", GetWinApiErrorMsg().c_str());		
			RegCloseKey(svSysKey);
			return FALSE;
		}
		existingPort = cxSettingsDwordValue;

		//Read ServerName
		cxSettingsValueSize = MAX_PATH;
		cxSettingsValueType = REG_SZ;
		svSysRet = RegQueryValueEx(svSysKey, SV_SERVER_NAME_VALUE_NAME, 0, &cxSettingsValueType, (LPBYTE)&cxIp, &cxSettingsValueSize);
		if(svSysRet == ERROR_SUCCESS)
		{
			existingIp = cxIp;
		}
		if(svSysRet != ERROR_SUCCESS)
		{
			SetWinApiErrorMsg(GetLastError());
			errStr.Format("Failed to read CX ip.\nErrorCode = %u", svSysRet);
			SetConfigErrorMsg(errStr);
			DebugPrintf(SV_LOG_ERROR, "Failed to read configuration server ip. ErrorCode = %u", svSysRet);
			DebugPrintf(SV_LOG_ERROR, "Error description = %s", GetWinApiErrorMsg().c_str());
			RegCloseKey(svSysKey);
			return FALSE;
		}

		//Read Https
		cxSettingsValueSize = sizeof(DWORD);
		cxSettingsValueType = REG_DWORD;
		svSysRet = RegQueryValueEx(svSysKey, SV_HTTPS, 0, &cxSettingsValueType, (LPBYTE) &cxSettingsDwordValue, &cxSettingsValueSize);
		if(svSysRet == ERROR_FILE_NOT_FOUND)
			cxSettingsDwordValue = 0;
		else if(svSysRet != ERROR_SUCCESS)
		{
			SetWinApiErrorMsg(GetLastError());
			errStr.Format("Failed to read Https enabled status.\nErrorCode = %u", svSysRet);
			SetConfigErrorMsg(errStr);
			RegCloseKey(svSysKey);
			return FALSE;
		}
		existingHttps = cxSettingsDwordValue ? true : false;

		RegCloseKey(svSysKey);

		string passphrase = securitylib::getPassphrase();

		if((existingIp.CompareNoCase(cxSettings.cxIP) != 0 ) ||
				(existingPort != atoi(cxSettings.inmCxPort.GetString())) || 
				(existingHttps != cxSettings.bCxUseHttps) ||
				(cxSettings.cx_Passphrase.Compare(passphrase.c_str()) != 0))
		{
			DebugPrintf(SV_LOG_DEBUG, "Service restart is required\n");
			bRestartScoutService = TRUE;
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG, "Service restart not required\n");
		}
	}
	catch ( ContextualException& ce )
	{
		bRestartScoutService = FALSE;
		DebugPrintf(SV_LOG_DEBUG, "Encountered contextual exception = %s\n",ce.what());
		SetConfigErrorMsg(ce.what());
	}
	catch( std::exception const& e )
	{
		bRestartScoutService = FALSE;
		DebugPrintf(SV_LOG_DEBUG, "Encountered exception = %s\n",e.what());
		SetConfigErrorMsg(e.what());
	}
	catch ( ... )
	{
		bRestartScoutService = FALSE;
		DebugPrintf(SV_LOG_DEBUG, "Encountered unknown exception.\n");
		SetConfigErrorMsg(_T("Encountered unknown exception."));
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
	return bRestartScoutService;
}

/********************************************
* Reads VX settings from drscout.conf file	*
********************************************/
BOOL CInmConfigData::ReadVxAgentSettings()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	BOOL bReadVxAgentSetting = TRUE;

	try
	{
		LocalConfigurator localConfig;
		//AGENT_SETTINGS
		m_vxAgentSettings.vxAgentCacheDir = CString(localConfig.getCacheDirectory().c_str());
	}
	catch ( ContextualException& ce )
	{
		bReadVxAgentSetting = FALSE;
		DebugPrintf(SV_LOG_DEBUG, "Encountered contextual exception = %s\n",ce.what());
		SetConfigErrorMsg(ce.what());
	}
	catch( std::exception const& e )
	{
		bReadVxAgentSetting = FALSE;
		DebugPrintf(SV_LOG_DEBUG, "Encountered exception = %s\n",e.what());
		SetConfigErrorMsg(e.what());
	}
	catch ( ... )
	{
		bReadVxAgentSetting = FALSE;
		DebugPrintf(SV_LOG_DEBUG, "Encountered unknown exception.\n");
		SetConfigErrorMsg(_T("Encountered unknown exception."));
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
	return bReadVxAgentSetting;
}

/****************************************
* Save VX settings to drscout.conf file	*
****************************************/
BOOL CInmConfigData::SaveVxAgentSettings()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	BOOL bSaveVxAgentSetting = TRUE;

	try
	{
		LocalConfigurator localConfig;
		//AGENT_SETTINGS
		localConfig.setCacheDirectory(m_vxAgentSettings.vxAgentCacheDir.GetString());
		localConfig.setDiffTargetCacheDirectoryPrefix(m_vxAgentSettings.vxAgentCacheDir.GetString());
	}
	catch ( ContextualException& ce )
	{
		bSaveVxAgentSetting = FALSE;
		DebugPrintf(SV_LOG_DEBUG, "Encountered contextual exception = %s\n",ce.what());
		SetConfigErrorMsg(ce.what());
	}
	catch( std::exception const& e )
	{
		bSaveVxAgentSetting = FALSE;
		DebugPrintf(SV_LOG_DEBUG, "Encountered exception = %s\n",e.what());
		SetConfigErrorMsg(e.what());
	}
	catch ( ... )
	{
		bSaveVxAgentSetting = FALSE;
		DebugPrintf(SV_LOG_DEBUG, "Encountered unknown exception.\n");
		SetConfigErrorMsg(_T("Encountered unknown exception."));
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
	return bSaveVxAgentSetting;
}

/****************************************************
* Reads VX logging settings from drscout.conf file	*
* Reads FX logging settings from registry			*
****************************************************/
BOOL CInmConfigData::ReadAgentLoggingSettings()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	BOOL bReadLoggingSettings = TRUE;
	CString errStr;	

	if(scoutAgentInstallCode != SCOUT_VX)
	{
		HKEY svSysKey;
		DWORD cxSettingsValueSize;
		DWORD cxSettingsValueType;
		DWORD cxSettingsDwordValue;
		LSTATUS svSysRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, INM_SVSYSTEMS_REGKEY, 0, KEY_READ, &svSysKey);
	
		if(svSysRet != ERROR_SUCCESS)
		{
			SetWinApiErrorMsg(GetLastError());
			errStr.Format("Failed to open registry key to read logging settings.\nErrorCode = %u", svSysRet);
			SetConfigErrorMsg(errStr);
			DebugPrintf(SV_LOG_ERROR, "Failed to open registry key to read logging settings. ErrorCode = %u\n", svSysRet);
			DebugPrintf(SV_LOG_ERROR, "Error description = %s\n", GetWinApiErrorMsg().c_str());
			return FALSE;
		}

		//Read DebugLogLevel
		cxSettingsValueSize = sizeof(DWORD);
		cxSettingsValueType = REG_DWORD;
		svSysRet = RegQueryValueEx(svSysKey, SV_DEBUG_LOG_LEVEL_VALUE_NAME, 0, &cxSettingsValueType, (LPBYTE) &cxSettingsDwordValue, &cxSettingsValueSize);
		if(svSysRet != ERROR_SUCCESS)
		{
			SetWinApiErrorMsg(GetLastError());
			errStr.Format("Failed to read debug log level.\nErrorCode = %u", svSysRet);
			SetConfigErrorMsg(errStr);
			RegCloseKey(svSysKey);
			DebugPrintf(SV_LOG_ERROR, "Failed to read debug log level. ErrorCode = %u\n", svSysRet);
			DebugPrintf(SV_LOG_ERROR, "Error description = %s\n", GetWinApiErrorMsg().c_str());
			return FALSE;
		}
		RegCloseKey(svSysKey);
		m_agentLoggingSettings.fxLogLevel = cxSettingsDwordValue;
	}
	if(scoutAgentInstallCode != SCOUT_FX)
	{		
		try
		{
			LocalConfigurator localConfig;
			m_agentLoggingSettings.vxLocalLogLevel = (int)localConfig.getLogLevel();
			m_agentLoggingSettings.vxRemoteLogLevel = localConfig.getRemoteLogLevel();
		}
		catch ( ContextualException& ce )
		{
			bReadLoggingSettings = FALSE;
			SetConfigErrorMsg(ce.what());
			DebugPrintf(SV_LOG_DEBUG, "Encountered contextual exception = %s\n",ce.what());
		}
		catch( std::exception const& e )
		{
			bReadLoggingSettings = FALSE;
			SetConfigErrorMsg(e.what());
			DebugPrintf(SV_LOG_DEBUG, "Encountered exception = %s\n",e.what());
		}
		catch ( ... )
		{
			bReadLoggingSettings = FALSE;
			SetConfigErrorMsg(_T("Encountered unknown exception."));
			DebugPrintf(SV_LOG_DEBUG, "Encountered unknown exception.\n");
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
	return bReadLoggingSettings;
}

/****************************************************************
* VX logging settings will be persisted to drscout.conf file	*
* FX logging settings will be stored in registry				*
****************************************************************/
BOOL CInmConfigData::SaveAgentLoggingSettings()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	BOOL bSaveLoggingSettings = TRUE;
	CString errStr;	

	if(scoutAgentInstallCode != SCOUT_VX)
	{
		HKEY svSysKey;
		LSTATUS svSysRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, INM_SVSYSTEMS_REGKEY, 0, KEY_READ | KEY_WRITE, &svSysKey);
	
		if(svSysRet != ERROR_SUCCESS)
		{
			SetWinApiErrorMsg(GetLastError());
			errStr.Format("Failed to open registry key to write logging settings.\nErrorCode = %u", svSysRet);
			SetConfigErrorMsg(errStr);
			DebugPrintf(SV_LOG_ERROR, "Failed to open registry key to write logging settings. ErrorCode = %u\n", svSysRet);
			DebugPrintf(SV_LOG_ERROR, "Error description = %s\n", GetWinApiErrorMsg().c_str());			
			return FALSE;
		}

		//Update DebugLogLevel
		svSysRet = RegSetValueEx(svSysKey, SV_DEBUG_LOG_LEVEL_VALUE_NAME, 0, REG_DWORD, reinterpret_cast<const BYTE*> (&m_agentLoggingSettings.fxLogLevel), sizeof(DWORD));
		if(svSysRet != ERROR_SUCCESS)
		{
			SetWinApiErrorMsg(GetLastError());
			errStr.Format("Failed to update debug log level.\nErrorCode = %u", svSysRet);
			SetConfigErrorMsg(errStr);
			RegCloseKey(svSysKey);
			DebugPrintf(SV_LOG_ERROR, "Failed to update debug log level. ErrorCode = %u\n", svSysRet);
			DebugPrintf(SV_LOG_ERROR, "Error description = %s\n", GetWinApiErrorMsg().c_str());			
			return FALSE;
		}
		
		RegCloseKey(svSysKey);
	}
	if(scoutAgentInstallCode != SCOUT_FX)
	{		
		try
		{
			LocalConfigurator localConfig;
			localConfig.setLogLevel((SV_LOG_LEVEL)m_agentLoggingSettings.vxLocalLogLevel);
			localConfig.setRemoteLogLevel(m_agentLoggingSettings.vxRemoteLogLevel);
		}
		catch ( ContextualException& ce )
		{
			bSaveLoggingSettings = FALSE;
			SetConfigErrorMsg(ce.what());
			DebugPrintf(SV_LOG_DEBUG, "Encountered contextual exception = %s\n",ce.what());
		}
		catch( std::exception const& e )
		{
			bSaveLoggingSettings = FALSE;
			SetConfigErrorMsg(e.what());
			DebugPrintf(SV_LOG_DEBUG, "Encountered exception = %s\n",e.what());
		}
		catch ( ... )
		{
			bSaveLoggingSettings = FALSE;
			SetConfigErrorMsg(_T("Encountered unknown exception."));
			DebugPrintf(SV_LOG_DEBUG, "Encountered unknown exception.\n");
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
	return bSaveLoggingSettings;
}

/********************************************************************
* Reads logon user credentials from registry if FX is not installed	*
* Else reads FX logon user credentials from service manager			*
********************************************************************/
BOOL CInmConfigData::ReadAgentLogonSettings()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	BOOL bReadLogonSettings = TRUE;
	string domainName, domainUserName, domainPassword;
	domainName = domainUserName = domainPassword = "";
	string errorMsg;

	if(CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode != SCOUT_FX)
	{
		//Read user logon configuration for Application service from registry
		bReadLogonSettings = ReadCredentials(domainName, domainUserName, domainPassword, errorMsg);		
		if(bReadLogonSettings)
		{
			m_agentLogonSettings.user_name.Format("%s\\%s",domainName.c_str(),domainUserName.c_str()); 
			m_agentLogonSettings.user_password = ""; //do not display password
			m_agentLogonSettings.bAppAgentUserCheck = true;
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG, "%s\n", errorMsg.c_str());
			m_agentLogonSettings.user_name = "";
			m_agentLogonSettings.user_password = "";
			m_agentLogonSettings.bAppAgentUserCheck = false;
			bReadLogonSettings = true;
		}
	}

	if(CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode != SCOUT_VX)
	{
		//Read user logon configuration for Fx service from SCM
		string fxLogonAccName = "";
		bool bLocalSysAcc = true;
		bReadLogonSettings = ReadFxConfiguration(fxLogonAccName, bLocalSysAcc);
		if(bReadLogonSettings)
		{
			m_agentLogonSettings.bFxLocalSystemAct = bLocalSysAcc;
			m_agentLogonSettings.bFxUserAct = !bLocalSysAcc ;

			if(0 == m_agentLogonSettings.user_name.GetLength())
				m_agentLogonSettings.user_name = fxLogonAccName.c_str();
		}
		else
		{
			//SetConfigErrorMsg("Failed to read Fx configuration.");
			m_agentLogonSettings.bFxLocalSystemAct = true;
			m_agentLogonSettings.bFxUserAct = false;
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
	return bReadLogonSettings;
}

/********************************************************
* Reads FX logon user credentials from service manager	*
********************************************************/
BOOL CInmConfigData::ReadFxConfiguration(std::string &logonAccName, bool &bLocalSysAccount)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	BOOL bConfig = TRUE;
	CString errStr;

	SC_HANDLE scoutServiceHandle, scoutServiceMgrHandle;
	LPQUERY_SERVICE_CONFIG querySvcConfig;
	LPCTSTR fxServiceName = INM_FX_SERVICE_NAME;
	DWORD dwActualBytes;
	DWORD lastError;
	
	//Get Service Control Manager(SCM) handle
	scoutServiceMgrHandle = OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS);
	if(scoutServiceMgrHandle == NULL)
	{
		errStr.Format("Failed to open service control manager. ErrorCode = %d", GetLastError());
		SetConfigErrorMsg(errStr);
		SetWinApiErrorMsg(GetLastError());
		DebugPrintf(SV_LOG_ERROR, "Failed to open service control manager. ErrorCode = %u\n", GetLastError());
		DebugPrintf(SV_LOG_ERROR, "Error description = %s\n", GetWinApiErrorMsg().c_str());
		return FALSE;
	}
	
	//Get Fx service handle
	scoutServiceHandle = ::OpenService(scoutServiceMgrHandle, fxServiceName, SERVICE_QUERY_CONFIG);
	if(scoutServiceHandle == NULL)
	{
		errStr.Format("Failed to get Fx service handle. ErrorCode = %d", GetLastError());
		SetConfigErrorMsg(errStr);
		SetWinApiErrorMsg(GetLastError());

		//Release SCM handle and return false
		::CloseServiceHandle(scoutServiceMgrHandle);
		DebugPrintf(SV_LOG_ERROR, "Failed to get FX service handle. ErrorCode = %u\n", GetLastError());
		DebugPrintf(SV_LOG_ERROR, "Error description = %s\n", GetWinApiErrorMsg().c_str());		
		return FALSE;
	}
	
	//Get service configuration information
	//Pass the buffer size as zero to get actual size of data for buffer allocation
	bConfig = ::QueryServiceConfig(scoutServiceHandle, NULL, 0, &dwActualBytes);

	if(!bConfig)
	{
		lastError = GetLastError();
		if(ERROR_INSUFFICIENT_BUFFER == lastError)
		{
			DWORD byteCount = dwActualBytes;

			//Allocate memory
			querySvcConfig = (LPQUERY_SERVICE_CONFIG) LocalAlloc(LPTR, byteCount);
			bConfig = ::QueryServiceConfig(scoutServiceHandle, querySvcConfig, byteCount, &dwActualBytes);
			if(TRUE == bConfig)
			{
				logonAccName = querySvcConfig->lpServiceStartName;
				if(strcmpi(logonAccName.c_str(), INM_LOCAL_SYSTEM_ACC) == 0)
				{
					bLocalSysAccount = true;
					logonAccName = ""; //if Local system, no need to display user name
				}
				else
					bLocalSysAccount = false;
			}

			//Free memory allocated using LocalAlloc
			LocalFree(querySvcConfig);
		}
		else
		{
			errStr.Format("Failed to query Fx service configuration. ErrorCode = %d", GetLastError());
			SetConfigErrorMsg(errStr);
			SetWinApiErrorMsg(lastError);

			//Release service handles and return false
			::CloseServiceHandle(scoutServiceHandle);
			::CloseServiceHandle(scoutServiceMgrHandle);
			DebugPrintf(SV_LOG_ERROR, "Failed to query FX service configuration. ErrorCode = %u\n", GetLastError());
			DebugPrintf(SV_LOG_ERROR, "Error description = %s\n", GetWinApiErrorMsg().c_str());
			return FALSE;
		}
	}

	//Release service handles
	::CloseServiceHandle(scoutServiceHandle);
	::CloseServiceHandle(scoutServiceMgrHandle);
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
	return bConfig;
}

/************************************************************************************
* Stores logon user name in registry for App agent and restarts App agent service	*
* Changes the logon user for FX service and restarts FX service						*
* Grants logon permission before updating user configuration						*
************************************************************************************/
BOOL CInmConfigData::SaveAgentLogonSettings()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	BOOL bSaveLogonSettings = TRUE;
	string userName = "";
	string domainName = "";
	string password = "";

	if(CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode != SCOUT_FX)
	{
		if(m_agentLogonSettings.bAppAgentUserCheck)
		{
			if(!m_agentLogonSettings.user_password.IsEmpty())
			{
				std::string agentLogonUser = m_agentLogonSettings.user_name.GetString();
				
				if(GrantLogOnRightsToUser(agentLogonUser.c_str()) == -1)
				{
					//AfxMessageBox("Failed to grant logon rights");
					return FALSE;
				}			
				domainName = agentLogonUser.substr(0,agentLogonUser.find_first_of('\\'));
				userName = agentLogonUser.substr(agentLogonUser.find_first_of('\\')+1);
				password = m_agentLogonSettings.user_password.GetString();

				//Check if given credentials are true. If true, update registry
				HANDLE agentLogonUserToken = NULL;
				bSaveLogonSettings = VerifyScoutAgentServiceUserLogon(userName, domainName, password, agentLogonUserToken);
				if(bSaveLogonSettings)
				{
					//Terminate the impersonation and release the handle.
					RevertToSelf();
					if(agentLogonUserToken != NULL)
						CloseHandle(agentLogonUserToken);
					string encryptionKey;
					string encryptedPassword, errorMsg;
					bSaveLogonSettings = EncryptCredentials(domainName, userName, password, encryptionKey, encryptedPassword, true, errorMsg);
					securitylib::secureClearPassphrase(encryptionKey);
					
					//Restart Application service
					if(bSaveLogonSettings)
					{
						bSaveLogonSettings = RestartInMageScoutService("InMage Scout Application Service");
						if(!bSaveLogonSettings)
						{
							SetConfigErrorMsg("Failed to restart InMage Scout Application service.\nPlease restart manually.");							
							DebugPrintf(SV_LOG_ERROR, "Failed to restart InMage Scout Application service.\n");
							return FALSE;
						}
					}
					else
					{
						SetConfigErrorMsg("Failed to save logon credentials.");
						DebugPrintf(SV_LOG_ERROR, "Failed to save logon credentials.\n");
						DebugPrintf(SV_LOG_ERROR, "ErrorMessage = %s\n", errorMsg.c_str());
						return FALSE;
					}
				}
				else
				{
					return FALSE;
				}
			}
		}
		else
		{
			//Handled case:
			//if "Use User Account" option is unchecked and registry is having user logon credentials.
			//delete registry key and restart application service
			HKEY keyRes;
			LSTATUS regStatus = RegOpenKeyEx(HKEY_LOCAL_MACHINE,INM_SAM_REG_FAILOVER_KEY,0,KEY_READ,&keyRes);
			if(regStatus == ERROR_SUCCESS)
			{
				regStatus = RegDeleteKey(HKEY_LOCAL_MACHINE,INM_SAM_REG_FAILOVER_KEY);
				bSaveLogonSettings = RestartInMageScoutService("InMage Scout Application Service");
			}	
		}
	}
	if(CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode != SCOUT_VX)
		bSaveLogonSettings = UpdateFxConfiguration(m_agentLogonSettings.bFxLocalSystemAct);
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
	return bSaveLogonSettings;
}

/************************************************************
* Validates user configuration provided in host config ui 	*
* How to validate: Logon user and try to get a token		*
*************************************************************/
BOOL CInmConfigData::VerifyScoutAgentServiceUserLogon(const std::string& userName, const std::string& domain, const std::string& password, HANDLE &agentLogonUserToken)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	BOOL bLoggedOn = FALSE;
	CString errStr;

	if(userName.empty() == false && domain.empty() == false && password.empty() == false)
	{
		//Logon user provided in host config ui
		if(LogonUser(userName.c_str(), domain.c_str(), password.c_str(), /*LOGON32_LOGON_INTERACTIVE*/LOGON32_LOGON_SERVICE, LOGON32_PROVIDER_DEFAULT, &agentLogonUserToken) == TRUE)
		{
			//Check for a user token using the user name
			if(ImpersonateLoggedOnUser(agentLogonUserToken))
			{
				bLoggedOn = TRUE;
			}
			else
			{
				errStr.Format("Error occured during Impersonate LoggedOnUser. ErrorCode = %d",GetLastError());
				SetConfigErrorMsg(errStr);
				SetWinApiErrorMsg(GetLastError());
				DebugPrintf(SV_LOG_ERROR, "Error occured during Impersonate LoggedOnUser. ErrorCode = %u\n", GetLastError());
				DebugPrintf(SV_LOG_ERROR, "Error description = %s\n", GetWinApiErrorMsg().c_str());
				bLoggedOn = FALSE;
			}
		}
		else
		{
			errStr.Format("Error occured during LogOnUser. ErrorCode = %d",GetLastError());
			SetConfigErrorMsg(errStr);
			SetWinApiErrorMsg(GetLastError());
			DebugPrintf(SV_LOG_ERROR, "Error occured during LogOnUser. ErrorCode = %u\n", GetLastError());
			DebugPrintf(SV_LOG_ERROR, "Error description = %s\n", GetWinApiErrorMsg().c_str());
			bLoggedOn = FALSE;
		}
	}
	else
	{
		agentLogonUserToken = NULL;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
	return bLoggedOn;
}

/************************************
* Grants logon permission for user	*
************************************/
int CInmConfigData::GrantLogOnRightsToUser(const char *inmUserAccName)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	CString errStr;
	USES_CONVERSION;	
	HRESULT hr = MQ_ERROR;
	static SECURITY_QUALITY_OF_SERVICE inmSecurityQos = { sizeof SECURITY_QUALITY_OF_SERVICE, SecurityImpersonation, SECURITY_DYNAMIC_TRACKING, FALSE };
	static LSA_OBJECT_ATTRIBUTES inmLsaObjAtt = { sizeof LSA_OBJECT_ATTRIBUTES, NULL, NULL, 0, NULL, &inmSecurityQos };
	NTSTATUS inmNtStat;
	LSA_HANDLE inmLsaHandle;

	//Open Security policy
	if(inmNtStat = LsaOpenPolicy( NULL, &inmLsaObjAtt, GENERIC_READ | GENERIC_EXECUTE |POLICY_LOOKUP_NAMES | POLICY_CREATE_ACCOUNT, &inmLsaHandle )!= ERROR_SUCCESS)
	{
	   errStr.Format("Unable to open policy object. ErrorCode = %d",GetLastError());
	   SetConfigErrorMsg(errStr);
	   SetWinApiErrorMsg(GetLastError());
	   DebugPrintf(SV_LOG_ERROR, "Unable to open policy object. ErrorCode = %u\n", GetLastError());
	   DebugPrintf(SV_LOG_ERROR, "Error description = %s\n", GetWinApiErrorMsg().c_str());
	   return -1;
	}
	PSID inmUserAccountSid;
	std::wstring inmAgentSvcAccName;
	inmAgentSvcAccName = A2W(inmUserAccName);
	
	//Get the  SID corresponding to account Name
	hr = GetAgentUserAccountSid(inmAgentSvcAccName.c_str(),&inmUserAccountSid);
	if(MQ_OK != hr)
	{
		// Free memory allocated for SID.
		if(inmUserAccountSid != NULL) 
           delete inmUserAccountSid;
		return -1;
	}
	
	LPWSTR inmAgentSvcPriv = L"SeServiceLogonRight";        // privilege to grant (Unicode)
    LSA_UNICODE_STRING inmAgentSvcPrivStr;

    //Create a LSA_UNICODE_STRING for the privilege name.
    InitializeInmLsaString(&inmAgentSvcPrivStr, inmAgentSvcPriv);

    // grant the privilege
	if((inmNtStat = LsaAddAccountRights( inmLsaHandle, inmUserAccountSid,&inmAgentSvcPrivStr ,1))!= ERROR_SUCCESS)
	{
	   errStr.Format("Unable to assign privileges to user account. ErrorCode = %d",GetLastError());
	   SetConfigErrorMsg(errStr);
	   SetWinApiErrorMsg(GetLastError());
	}

	// Close the policy handle and Free memory allocated for SID.
    LsaClose(inmLsaHandle);
    if(inmUserAccountSid != NULL) 
	{
		delete inmUserAccountSid;
		inmUserAccountSid = NULL;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
	return 0;
}

/************************
* Gets user account SID	*
************************/
HRESULT CInmConfigData::GetAgentUserAccountSid(LPCWSTR inmUserAccountName,PSID * inmUserAccountSid) 
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	CString errStr;
	HRESULT hr = MQ_OK;
	USES_CONVERSION;
	// Validate the input parameters.
	if (inmUserAccountName == NULL || inmUserAccountSid == NULL)
    {
		SetConfigErrorMsg("The user account name is empty");
		DebugPrintf(SV_LOG_ERROR, "The user account name is empty.\n");
		return MQ_ERROR_INVALID_PARAMETER;
	}

	// Create buffers that may be large enough.
	// If a buffer is too small, the count parameter will be set to the size needed.
	const DWORD INM_INITIAL_SIZE = 32;
	DWORD inmSidCountBytes = 0;

	DWORD inmDwSidBufferSize = INM_INITIAL_SIZE;
	DWORD inmUserDomainName = 0;

	DWORD inmDomainNameBufSize = INM_INITIAL_SIZE;
	WCHAR *inmDomainNameW = NULL;

	SID_NAME_USE inmSidType;
	DWORD inmDwErrorCode = 0;


	// Create buffers for the SID and the domain name.
	*inmUserAccountSid = (PSID) new BYTE[inmDwSidBufferSize];
	if (*inmUserAccountSid == NULL)
	{
		SetConfigErrorMsg("Insufficient resource to allocate memory for SID");
		DebugPrintf(SV_LOG_ERROR, "Insufficient resource to allocate memory for SID.\n");
		return MQ_ERROR_INSUFFICIENT_RESOURCES;
	}
	memset(*inmUserAccountSid, 0, inmDwSidBufferSize);
	inmDomainNameW = new WCHAR[inmDomainNameBufSize];
	if (inmDomainNameW == NULL)
	{
		SetConfigErrorMsg("Insufficient resource to allocate memory for domain name");
		DebugPrintf(SV_LOG_ERROR, "Insufficient resource to allocate memory for domain name.\n");
		return MQ_ERROR_INSUFFICIENT_RESOURCES;
	}
	memset(inmDomainNameW, 0, inmDomainNameBufSize*sizeof(WCHAR));

	// Obtain the SID for the account name passed.
	for ( ; ; )
	{
		// Set the count variables to the buffer sizes and retrieve the SID.
		inmSidCountBytes = inmDwSidBufferSize;
		inmUserDomainName = inmDomainNameBufSize;
		if (LookupAccountNameW(NULL,inmUserAccountName,*inmUserAccountSid,&inmSidCountBytes,inmDomainNameW,&inmUserDomainName,&inmSidType))
		{
			if (IsValidSid(*inmUserAccountSid) == FALSE)
			{
				SetConfigErrorMsg("The security identifier is not valid.");
				DebugPrintf(SV_LOG_ERROR, "The security identifier is not valid.\n");
			}
			break;
		}
		inmDwErrorCode = GetLastError();

		// Check if one of the buffers was too small.
		if (inmDwErrorCode == ERROR_INSUFFICIENT_BUFFER)
		{
			if (inmSidCountBytes > inmDwSidBufferSize)
			{
				// Reallocate memory for the SID buffer.
				//wprintf(L"The SID buffer was too small. It will be reallocated.\n");
				FreeSid(*inmUserAccountSid);
				*inmUserAccountSid = (PSID) new BYTE[inmSidCountBytes];
				if (*inmUserAccountSid == NULL)
				{	
					SetConfigErrorMsg("Insufficient resource to reallocate memory for SID");
					DebugPrintf(SV_LOG_ERROR, "Insufficient resource to reallocate memory for SID.\n");
					return MQ_ERROR_INSUFFICIENT_RESOURCES;
				}
				memset(*inmUserAccountSid, 0, inmSidCountBytes);
				inmDwSidBufferSize = inmSidCountBytes;
			}
			if (inmUserDomainName > inmDomainNameBufSize)
			{
				// Reallocate memory for the domain name buffer.
				// wprintf(L"The domain name buffer was too small. It will be reallocated.\n");
				delete [] inmDomainNameW;
				inmDomainNameW = new WCHAR[inmUserDomainName];
				if (inmDomainNameW == NULL)
				{
					SetConfigErrorMsg("Insufficient resource to reallocate memory for domain name");
					DebugPrintf(SV_LOG_ERROR, "Insufficient resource to reallocate memory for domain name.\n");
					return MQ_ERROR_INSUFFICIENT_RESOURCES;
				}
				memset(inmDomainNameW, 0, inmUserDomainName*sizeof(WCHAR));
				inmDomainNameBufSize = inmUserDomainName;
			}
		}
		else
		{
			errStr.Format("Unable to get the security identifier for given user account. ErrorCode = %d",GetLastError());
			SetConfigErrorMsg(errStr);
			SetWinApiErrorMsg(GetLastError());
			DebugPrintf(SV_LOG_ERROR, "Unable to get the security identifier for given user account.\n");
			DebugPrintf(SV_LOG_ERROR, "Error description = %s", GetWinApiErrorMsg().c_str());
			hr = HRESULT_FROM_WIN32(inmDwErrorCode);
			break;
		}
	}
	delete [] inmDomainNameW;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
	return hr; 
}

/************************
* Initializes LSA string*
************************/
void CInmConfigData::InitializeInmLsaString(PLSA_UNICODE_STRING inmLsaUnicodeString,LPWSTR inmLsaString)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	DWORD inmLsaStringLength;
	if (inmLsaString == NULL) 
	{
		inmLsaUnicodeString->Buffer = NULL;
		inmLsaUnicodeString->Length = 0;
		inmLsaUnicodeString->MaximumLength = 0;
		return;
	}
	inmLsaStringLength = wcslen(inmLsaString);
	inmLsaUnicodeString->Buffer = inmLsaString;
	inmLsaUnicodeString->Length = (USHORT) inmLsaStringLength * sizeof(WCHAR);
	inmLsaUnicodeString->MaximumLength=(USHORT)(inmLsaStringLength+1) * sizeof(WCHAR);
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
}

/*
GetServiceStartupType

Fetches Service Configuration. 

Input	: Takes serivceName as argument.

Output	: Returns Service Configuration.
*/
BOOL CInmConfigData::GetServiceConfiguration(const std::string &serviceName, LPQUERY_SERVICE_CONFIG &lpSrvcConfig)
{
	DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__ );
	BOOL bfunRetVal = TRUE;

	SC_HANDLE serviceHandle, serviceMgrHandle;
	DWORD dwActualBytes;
	DWORD lastError;

	do
	{
		//Get Service Control Manager(SCM) handle
		serviceMgrHandle = OpenSCManager(NULL, NULL, STANDARD_RIGHTS_READ);
		if (serviceMgrHandle == NULL)
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to open service control manager. ErrorCode = %u\n", GetLastError());
			DebugPrintf(SV_LOG_ERROR, "Error description = %s\n", GetWinApiErrorMsg().c_str());
			bfunRetVal = FALSE;
			break;
		}

		//Get service handle
		serviceHandle = ::OpenService(serviceMgrHandle, serviceName.c_str(), SERVICE_QUERY_CONFIG);
		if (serviceHandle == NULL)
		{
			//Release SCM handle and return false
			::CloseServiceHandle(serviceMgrHandle);
			DebugPrintf(SV_LOG_ERROR, "Failed to get handle for service %s. ErrorCode = %u\n", serviceName.c_str(), GetLastError());
			DebugPrintf(SV_LOG_ERROR, "Error description = %s\n", GetWinApiErrorMsg().c_str());
			bfunRetVal = FALSE;
			break;
		}

		//Get service configuration information
		//Pass the buffer size as zero to get actual size of data for buffer allocation
		bool bConfig = ::QueryServiceConfig(serviceHandle, NULL, 0, &dwActualBytes);

		if (!bConfig)//bConfig Will be false as above query will always fail as buffer size sent is 0.
		{
			lastError = GetLastError();
			if (ERROR_INSUFFICIENT_BUFFER == lastError)
			{
				DWORD byteCount = dwActualBytes;

				//Allocate memory
				lpSrvcConfig = (LPQUERY_SERVICE_CONFIG)LocalAlloc(LPTR, byteCount);
				bConfig = ::QueryServiceConfig(serviceHandle, lpSrvcConfig, byteCount, &dwActualBytes);
				if (!bConfig)
				{
					if (ERROR_ACCESS_DENIED == GetLastError())
					{
						DebugPrintf(SV_LOG_ERROR, "The service handle does not have the SERVICE_QUERY_CONFIG access right.\n");
					}
					else if (ERROR_INSUFFICIENT_BUFFER == GetLastError())
					{
						DebugPrintf(SV_LOG_ERROR, "There is  buffer size allocated for lpServiceConfig is less than required.\n");
					}
					else if (ERROR_INVALID_HANDLE == GetLastError())
					{
						DebugPrintf(SV_LOG_ERROR, "The handle used for query is invalid.\n");
					}
				}
				else
				{
					DebugPrintf(SV_LOG_DEBUG, "Successfully queried configuration information for service %s.\n", serviceName.c_str());
				}
			}
			else
			{
				//Release service handles and return false
				::CloseServiceHandle(serviceHandle);
				::CloseServiceHandle(serviceMgrHandle);
				DebugPrintf(SV_LOG_ERROR, "Failed to query configuration for service %s. ErrorCode = %u\n", serviceName.c_str(), GetLastError());
				DebugPrintf(SV_LOG_ERROR, "Error description = %s\n", GetWinApiErrorMsg().c_str());
				bfunRetVal = FALSE;
				break;
			}
		}

		//Release service handles
		::CloseServiceHandle(serviceHandle);
		::CloseServiceHandle(serviceMgrHandle);
	} while (0);

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
	return bfunRetVal;
}

/************************************************************
* Updates FX service logon and restarts service if required	*
************************************************************/
BOOL CInmConfigData::UpdateFxConfiguration(bool bLocalSysAccount)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	BOOL bConfig = TRUE;
	CString errStr;
	SC_HANDLE scoutServiceHandle, scoutServiceMgrHandle;
	DWORD dwFxServiceStartupType;
	LPCTSTR lpszServiceName = INM_FX_SERVICE_NAME;
	
	bool bIsLocalAccount = false;
	
	//Get Service Control Manager(SCM) handle
	scoutServiceMgrHandle = OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS);
	if(scoutServiceMgrHandle == NULL)
	{
		errStr.Format("Failed to open service control manager. ErrorCode = %d", GetLastError());
		SetConfigErrorMsg(errStr);
		SetWinApiErrorMsg(GetLastError());
		DebugPrintf(SV_LOG_ERROR, "Failed to open service control manager. ErrorCode = %u\n", GetLastError());
		DebugPrintf(SV_LOG_ERROR, "Error description = %s\n", GetWinApiErrorMsg().c_str());
		return FALSE;
	}
	
	//Get Fx service handle
	scoutServiceHandle = ::OpenService(scoutServiceMgrHandle, lpszServiceName, SERVICE_ALL_ACCESS);//SERVICE_CHANGE_CONFIG | SERVICE_QUERY_CONFIG);
	if(scoutServiceHandle == NULL)
	{
		errStr.Format("Failed to get Fx service handle. ErrorCode = %d",GetLastError());
		SetConfigErrorMsg(errStr);
		SetWinApiErrorMsg(GetLastError());
		DebugPrintf(SV_LOG_ERROR, "Failed to get Fx service handle. ErrorCode = %u\n", GetLastError());
		DebugPrintf(SV_LOG_ERROR, "Error description = %s\n", GetWinApiErrorMsg().c_str());
		
		//Release SCM handle and return false
		::CloseServiceHandle(scoutServiceMgrHandle);
		return FALSE;
	}
	//Get service configuration information
	//Pass the buffer size as zero to get actual size of data for buffer allocation
	LPQUERY_SERVICE_CONFIG querySvcConfig;
	DWORD dwActualBytes;
	DWORD lastError;
	bConfig = ::QueryServiceConfig(scoutServiceHandle, NULL, 0, &dwActualBytes);
	if(!bConfig)
	{
		lastError = GetLastError();
		if(ERROR_INSUFFICIENT_BUFFER == lastError)
		{
			DWORD byteCount = dwActualBytes;

			//Allocate memory
			querySvcConfig = (LPQUERY_SERVICE_CONFIG) LocalAlloc(LPTR, byteCount);
			bConfig = ::QueryServiceConfig(scoutServiceHandle, querySvcConfig, byteCount, &dwActualBytes);
			if(TRUE == bConfig)
			{
				if(strcmpi(querySvcConfig->lpServiceStartName, INM_LOCAL_SYSTEM_ACC) == 0)
					bIsLocalAccount = true;

				dwFxServiceStartupType = querySvcConfig->dwStartType;
			}

			//Free memory allocated using LocalAlloc
			LocalFree(querySvcConfig);
		}
		else
		{
			errStr.Format("Failed to query existing Fx service configuration. Fx service logon user will remain same.\nErrorCode = %d",lastError);
			SetConfigErrorMsg(errStr);
			SetWinApiErrorMsg(lastError);
			DebugPrintf(SV_LOG_ERROR, "query existing Fx service configuration. ErrorCode = %u\n", lastError);
			DebugPrintf(SV_LOG_ERROR, "Error description = %s\n", GetWinApiErrorMsg().c_str());
			//Release service handles and return false
			::CloseServiceHandle(scoutServiceHandle);
			::CloseServiceHandle(scoutServiceMgrHandle);
			return FALSE;
		}
	}
	
	if(bIsLocalAccount == bLocalSysAccount)
	{
		//Release service handles and return false
		::CloseServiceHandle(scoutServiceHandle);
		::CloseServiceHandle(scoutServiceMgrHandle);
		return TRUE;
	}
	
	//Update service configuration information
	string svcStartName = GetAgentLogonSettings().user_name.GetString();
	string svcPassword = GetAgentLogonSettings().user_password.GetString();
	if(bLocalSysAccount && !bIsLocalAccount) //Local system is selected, but Fx is running under user account
	{
		svcStartName = "LocalSystem";
		svcPassword = "";
	}
	else if(!bLocalSysAccount && bIsLocalAccount) //User account is selected, but Fx is running under Local system
	{
		string domainName = "";
		string userName = "";
		string password = "";
		string errorMsg = "";
		bConfig = ReadCredentials(domainName, userName, password, errorMsg);
		if(bConfig)
		{
			svcStartName = domainName + "\\" + userName;
			svcPassword = password;
		}
		else
		{
			domainName = svcStartName.substr(0,svcStartName.find_first_of('\\'));
			userName = svcStartName.substr(svcStartName.find_first_of('\\')+1);
			password = svcPassword;
		}
		if(GrantLogOnRightsToUser(svcStartName.c_str()) == -1)
		{				
			//Release service handles and return false
			::CloseServiceHandle(scoutServiceHandle);
			::CloseServiceHandle(scoutServiceMgrHandle);
			return FALSE;
		}	
		HANDLE agentLogonUserToken = NULL;

		bConfig = VerifyScoutAgentServiceUserLogon(userName, domainName, password, agentLogonUserToken);
		if(bConfig)
		{
			//Terminate the impersonation and release the handle.
			RevertToSelf();
			if(agentLogonUserToken != NULL)
				CloseHandle(agentLogonUserToken);
		}
		else
		{
			//Release service handles and return false
			::CloseServiceHandle(scoutServiceHandle);
			::CloseServiceHandle(scoutServiceMgrHandle);
			return FALSE;
		}		
	}	
	bConfig = ::ChangeServiceConfig(scoutServiceHandle, SERVICE_NO_CHANGE,SERVICE_NO_CHANGE,SERVICE_ERROR_NORMAL,
									NULL, NULL, NULL, NULL, svcStartName.c_str(), svcPassword.c_str(), NULL);
	
	//Release service handles
	::CloseServiceHandle(scoutServiceHandle);
	::CloseServiceHandle(scoutServiceMgrHandle);

	//Query status, stop and start Fx service
	if(bConfig)
	{
		if ( SERVICE_STARTUP_TYPE::MANUAL != dwFxServiceStartupType )
		{
			bConfig = RestartInMageScoutService(INM_FX_SERVICE_NAME);
		}
	}
	else
	{
		errStr.Format("Failed to update Fx service configuration. ErrorCode = %d", GetLastError());
		SetConfigErrorMsg(errStr);
		SetWinApiErrorMsg(GetLastError());
		DebugPrintf(SV_LOG_ERROR, "Failed to update Fx service configuration. ErrorCode = %u\n", GetLastError());
		DebugPrintf(SV_LOG_ERROR, "Error description = %s\n", GetWinApiErrorMsg().c_str());
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
	return bConfig;
}
BOOL CInmConfigData::ReadCredentials(std::string &strDomainName,std::string &strUserName,std::string &strPassword,std::string &errorMsg)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	BOOL bResult = TRUE;
	
	DWORD algId;
	bResult = DecryptCredentials(strDomainName, strUserName, strPassword, algId, errorMsg);
	if(bResult)
	{
		if(algId == RC4)
		{
			DebugPrintf(SV_LOG_DEBUG, "The encryption algorithm found as RC4. Upgrading to AES256 algorithm...\n"); 
			string encryptionKey;
			string encryptedPassword;
			bResult = EncryptCredentials(strDomainName, strUserName, strPassword, encryptionKey, encryptedPassword, true, errorMsg); 
			if(!bResult)
			{
				DebugPrintf(SV_LOG_ERROR, "%s\n", errorMsg.c_str());
			}
			securitylib::secureClearPassphrase(encryptionKey);
		}		
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "%s\n", errorMsg.c_str());
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
	return bResult;
}

BOOL CInmConfigData::ValidatePassphrase()
{
	BOOL rc = TRUE;
	try{		
		if (m_cxSettings.cx_Passphrase.IsEmpty())
		{
			DebugPrintf(SV_LOG_ERROR, "Passphrase cannot be empty.\n");
			SetConfigErrorMsg("Passphrase cannot be empty.");
			rc = FALSE;
		}
		else if (m_cxSettings.cx_Passphrase.GetLength() < 16)
		{
			DebugPrintf(SV_LOG_ERROR, "Passphrase length should be atleast 16 characters.\n");
			SetConfigErrorMsg("Passphrase length should be atleast 16 characters.");
			rc = FALSE;
		}
		else
		{
			//string existingPassphrase = securitylib::getPassphrase();
			//if ((m_cxSettings.cx_Passphrase.Compare(existingPassphrase.c_str()) != 0))
			{
				string reply, hostId;
				bool useSsl = m_cxSettings.bCxUseHttps;
				bool verifyPassphraseOnly = false;

				if (securitylib::csGetFingerprint(reply, hostId, std::string(m_cxSettings.cx_Passphrase.GetString()), std::string(m_cxSettings.cxIP.GetString()), std::string(m_cxSettings.inmCxPort.GetString()), verifyPassphraseOnly, useSsl, true))
				{
					DebugPrintf(SV_LOG_ERROR, "Passphrase validation successful.\n");
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR, "Passphrase validation failed.\n");
					DebugPrintf(SV_LOG_ERROR, "%s.\n", reply.c_str());
					string errorMsg = reply;
					reply += string("\n\nIf you forgot passphrase, follow the steps below to get the passphrase.\n1. Login to CS and open the command prompt.\n2. Navigate to CS installation path(Eg: C:\\home\\svsystems\\bin).\n3. Execute the command \"genpassphrase.exe -n\"");
					SetConfigErrorMsg(reply.c_str());
					return FALSE;
				}
			}
		}
	}
	catch (ContextualException& cexp)
	{
		DebugPrintf(SV_LOG_ERROR, "Caught contextual exception = %s\n", cexp.what());
		SetConfigErrorMsg(cexp.what());
		rc = FALSE;
	}
	catch (std::exception const& exp)
	{
		DebugPrintf(SV_LOG_ERROR, "Caught exception = %s\n", exp.what());
		SetConfigErrorMsg(exp.what());
		rc = FALSE;
	}
	catch (...)
	{
		DebugPrintf(SV_LOG_ERROR, "Encountered unknown exception.\n");
		SetConfigErrorMsg("Encountered unknown exception.");
		rc = FALSE;
	}
	return rc;
}

std::string CInmConfigData::GetAgentInstallationPath()
{	
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	string agentInstallPath = "";
	try{

		LocalConfigurator lc;
		agentInstallPath = lc.getInstallPath();
	}
	catch (ContextualException& cexp)
	{
		DebugPrintf(SV_LOG_ERROR, "Caught contextual exception = %s\n", cexp.what());
	}
	catch (std::exception const& exp)
	{
		DebugPrintf(SV_LOG_ERROR, "Caught exception = %s\n", exp.what());
	}
	catch (...)
	{
		DebugPrintf(SV_LOG_ERROR, "Encountered unknown exception.\n");
	}
	return agentInstallPath;
}

bool CInmConfigData::IsAgentRegistered()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	bool bRet = false;
	try{

		LocalConfigurator lc;
		HTTP_CONNECTION_SETTINGS connSettings = lc.getHttp();
		CString cxIP = CString(connSettings.ipAddress, _countof(connSettings.ipAddress));		
		std::string passphrase = securitylib::getPassphrase();
		
		if (passphrase.empty()||(0 == cxIP.Compare("0.0.0.0")))
			return bRet;

		securitylib::secureClearPassphrase(passphrase);
		bRet = true;
	}
	catch (ContextualException& cexp)
	{
		DebugPrintf(SV_LOG_ERROR, "Caught contextual exception = %s\n", cexp.what());
	}
	catch (std::exception const& exp)
	{
		DebugPrintf(SV_LOG_ERROR, "Caught exception = %s\n", exp.what());
	}
	catch (...)
	{
		DebugPrintf(SV_LOG_ERROR, "Encountered unknown exception.\n");
	}
	return bRet;
}