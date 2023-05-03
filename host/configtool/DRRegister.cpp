#include <exception>
#include <atlbase.h>

#include "DrRegister.h"
#include "servicemgr.h"
#include "logger.h"
#include "host.h"
#include "ProcessMgr.h"
#include "localconfigurator.h"
#include "csgetfingerprint.h"

bool DRRegister::GetAgentInstallPath(std::string& agentInstallPath, std::stringstream& errmsg)
{
	bool ret = true;
	try{
		LocalConfigurator lc;
		agentInstallPath = lc.getInstallPath();
	}
	catch (std::exception& e)
	{
		errmsg << "Could not find the agent installation path from drscout.conf file. Exception: " << e.what();

		DebugPrintf(SV_LOG_ERROR, "%s", errmsg.str().c_str());

		ret = false;
	}
	return ret;
}

bool DRRegister::IsMARSAgentInstalled()
{
	bool bRet = false;
	CRegKey  marsAgentKey;
	std::string marsAgentInstallPath;
	
	LSTATUS marsAgentStatus = marsAgentKey.Open(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows Azure Backup\\Setup", KEY_READ | KEY_WOW64_64KEY);
	if (ERROR_SUCCESS != marsAgentStatus)
	{
		DebugPrintf(SV_LOG_ERROR, "MarsAgent has not been installed.\n");
		return false;
	}
	else
	{
		char marsPath[BUFSIZ];
		DWORD dwCount = sizeof(marsPath);
		LSTATUS result = marsAgentKey.QueryStringValue("InstallPath", marsPath, &dwCount);
		if (ERROR_SUCCESS != result)
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to query MarsAgent InstallPath from Microsoft\\Windows Azure Backup\\Setup reg key.\n");
			RegCloseKey(marsAgentKey);

			return false;
		}
		else
		{
			marsAgentInstallPath = marsPath;
			if (!marsAgentInstallPath.empty() && marsAgentInstallPath.compare("C:\\Program Files\\Microsoft Azure Recovery Services Agent\\") == 0)
			{
				DebugPrintf(SV_LOG_DEBUG, "MarsAgent Installation Succeed.\n");
				bRet = true;
			}
			else if (!marsAgentInstallPath.empty())
			{
				DebugPrintf(SV_LOG_DEBUG, "MarsAgent Installed on custom path, it is not supported. installed Path = %s\n", marsAgentInstallPath.c_str());
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "MarsAgent InstallPath key value is empty.\n");
			}
		}
		RegCloseKey(marsAgentKey);
	}

	return bRet;
}

bool DRRegister::GetDraRegisteredVaultName(std::string& registeredVaultName)
{
	bool bRet = false;
	CRegKey  draRegKey;
	std::string vaultNameDetails;

	LSTATUS draRegStatus = draRegKey.Open(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Azure Site Recovery\\Registration", KEY_READ | KEY_WOW64_64KEY);
	if (ERROR_SUCCESS != draRegStatus)
	{
		DebugPrintf(SV_LOG_ERROR, "GetDraRegisteredVaultName::Configuration Server has not been registered with cloud service.\n");
		return false;
	}
	else
	{
		char vaultName[BUFSIZ];
		DWORD dwCount = sizeof(vaultName);
		LSTATUS result = draRegKey.QueryStringValue("ResourceName", vaultName, &dwCount);
		if (ERROR_SUCCESS != result)
		{
			DebugPrintf(SV_LOG_ERROR, "GetDraRegisteredVaultName::Failed to query ResourceName from Microsoft\\Azure Site Recovery\\Registration reg key.\n");
			RegCloseKey(draRegKey);

			return false;
		}
		else
		{
			registeredVaultName = vaultName;
			if (registeredVaultName.empty())
			{
				
				DebugPrintf(SV_LOG_ERROR, "GetDraRegisteredVaultName::DRA Registration, ResourceName key value is empty.\n");
			}
			else
			{
				DebugPrintf(SV_LOG_DEBUG, "GetDraRegisteredVaultName::Successfully retrieved key ResourceName.\n");
				bRet = true;
			}
		}
		RegCloseKey(draRegKey);
	}

	return bRet;
}

bool DRRegister::GetSystemDrv(std::string& sysdrv)
{
	bool haveSysDrv = true;

	const char* systemDrive = "%SystemDrive%";
	std::vector<char> buffer(260);
	size_t size = ExpandEnvironmentStrings(systemDrive, &buffer[0], (DWORD)buffer.size());
	if (size > buffer.size()) {
		buffer.resize(size + 1);
		size = ExpandEnvironmentStrings(systemDrive, &buffer[0], (DWORD)buffer.size());
		if (size > buffer.size()) {
			haveSysDrv = false;
		}
	}
	buffer[size] = '\0';
	if (haveSysDrv) {
		sysdrv.assign(&buffer[0]);
	}
	return haveSysDrv;
}
std::string DRRegister::GetHostName()
{

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
    return Host::GetInstance().GetHostName();
}

bool DRRegister::SetMarsProxy(const std::string& proxytype, const std::string& cspsinstallpath, const std::string& proxyaddress, const std::string& proxyport, const std::string& proxyusername, const std::string& proxypassword)
{
	bool bRet = false;

	if (proxytype.compare(CUSTOM_PROXY) == 0 || proxytype.compare(CUSTOM_WITH_AUTHENTICATION_PROXY) == 0)
	{

		std::string cmd;
		std::string cmdToLog;

		std::string formatedProxyAddress = proxyaddress;

		if (formatedProxyAddress.find_first_of("http://") == std::string::npos)
		{
			formatedProxyAddress = std::string("http://") + formatedProxyAddress;
		}

		cmd = "\"" + cspsinstallpath + "\\" + CSPS_SERVER_BIN_PATH + "\\" + SETMARSPROXY_EXE + "\"";
		cmd += " " + formatedProxyAddress + " " + proxyport;
		cmdToLog = cmd;


		if (proxytype.compare(CUSTOM_WITH_AUTHENTICATION_PROXY) == 0)
		{
			cmd += " " + proxyusername + " " + proxypassword;
			cmdToLog += " " + proxyusername + " ********** ";
		}

		DebugPrintf(SV_LOG_DEBUG, "Issuing Command : %s\n", cmdToLog.c_str());

		std::string out, emsg;
		bool isExecutedSussfully = true;

		try
		{

			if (!execProcUsingPipe(cmd, emsg, true, out))
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to set MARS Proxy.\n");
				if (!out.empty())
				{
					DebugPrintf(SV_LOG_ERROR, "Error: %s\n", out.c_str());
				}
			}
			else
			{
				DebugPrintf(SV_LOG_DEBUG, "Successfully set MARS Agent Proxy\n");
				if (!out.empty())
				   DebugPrintf(SV_LOG_DEBUG, "MARS Proxy settings command line output: %s\n", out.c_str());
				bRet = true;
			}
			
		}
		catch (std::exception exp)
		{
			DebugPrintf(SV_LOG_ERROR, "Exception occured, Exception Message: %s\n", exp.what());
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "Error: Unsupported Proxytype.\n");
	}
	return bRet;
}

bool DRRegister::Register(const std::string& proxytype, const std::string& cspsinstallpath, std::string& hostname, const std::string& credentialsFile, DWORD& exitcode, std::stringstream& errmsg, const std::string& proxyaddress, const std::string& proxyport, const std::string& proxyusername, const std::string& proxypassword)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	bool rv = FALSE;
	bool isDRARegisterSucceeded = false;

	bool isMarsProxySet = false;
	bool isMarsProxySettingRequire = false;
	std::string marsSettingErrmsg;

	if (IsMARSAgentInstalled() && (proxytype.compare(CUSTOM_PROXY) == 0 || proxytype.compare(CUSTOM_WITH_AUTHENTICATION_PROXY) == 0))
	{
		isMarsProxySettingRequire = true;
		if (SetMarsProxy(proxytype, cspsinstallpath, proxyaddress, proxyport, proxyusername, proxypassword))
		{
			isMarsProxySet = true;
		}
		else
		{
			marsSettingErrmsg = "Failed to set  Microsoft Azure Recovery Services Agent proxy.\n";
		}
	}
 
	if (FileExist(credentialsFile))
    {
		std::string sysDrv;
		if (!GetSystemDrv(sysDrv))
		{
			errmsg << "Could not determine SystemDrive.";
			DebugPrintf(SV_LOG_ERROR, "%s\n", errmsg.str().c_str());
			return false;
		}

		//start container registration
		//DRConfigurator.exe  /configureCloud  /ExportLocation <Path To Save Cert And Registry hive>  /CertificatePassword  <Password for PFX file>  /Credentials  <Vault Creds File path>
		std::string regContainerCmd = "\"" + sysDrv + "\\" + AZURE_SITE_RECOVERY_PROVIDER_PATH + "\\" + DRCONFIGURATOR_EXE + "\"";
		std::string cmdToLog = regContainerCmd + " /configureCloud  /ExportLocation  \"" + securitylib::getPrivateDir() + "\"  /CertificatePassword  *********** " + " /Credentials " + "\"" + credentialsFile + "\"";
		regContainerCmd += " /configureCloud  /ExportLocation  \"" + securitylib::getPrivateDir() + "\"  /CertificatePassword  " + securitylib::getPassphrase() + " /Credentials " + "\"" + credentialsFile + "\"";


		if (proxytype.compare(CUSTOM_PROXY) == 0)
		{
			regContainerCmd += std::string(" ") + "/proxyaddress" + " " + proxyaddress + " " + "/proxyport" + " " + proxyport;
			cmdToLog += std::string(" ") + "/proxyaddress" + " " + proxyaddress + " " + "/proxyport" + " " + proxyport;
		}

		if (proxytype.compare(CUSTOM_WITH_AUTHENTICATION_PROXY) == 0)
		{
			regContainerCmd += std::string(" ") + "/proxyaddress" + " " + proxyaddress + " " + "/proxyport" + " " + proxyport + " " + "/proxyusername" + " " + proxyusername + " " + "/proxypassword" + " " + proxypassword;
			cmdToLog += std::string(" ") + "/proxyaddress" + " " + proxyaddress + " " + "/proxyport" + " " + proxyport + " " + "/proxyusername" + " " + proxyusername + " " + "/proxypassword  *********";
		}

		DebugPrintf(SV_LOG_DEBUG, "ENTERED Command : %s\n", cmdToLog.c_str());
		std::stringstream containerErr;
		if (!ExecuteProc(regContainerCmd, exitcode, containerErr))
			errmsg << "Container registration got failed.\n\n";
		else if (exitcode != 0)
		{
			// adding error message if CS is registered with any other vault.
			if (exitcode == 10)
			{
				std::string registeredVaultName;
				if (!GetDraRegisteredVaultName(registeredVaultName))
				{
					errmsg << "Could not determine ResourceName from registry.";
					DebugPrintf(SV_LOG_ERROR, "%s\n", errmsg.str().c_str());
					return false;
				}
				else
				{
					errmsg << "Configuration Server registration failed. The Configuration Server is already registered with a different vault '" + registeredVaultName + "'."
						"If you need to register this Configuration Server to a new vault, then please uninstall all Azure Site Recovery Components first and then re-install the Configuration server from scratch."
						"Components that need to be uninstalled are\n"
						"1) Microsoft Azure Recovery Services Agent\n"
						"2) Microsoft Azure Site Recovery Master Target\Mobility Service\n"
						"3) Microsoft Azure Recovery Service Provider\n"
						"4) Microsoft Azure Site Recovery Configuration Server\Process Server\n"
						"5) Microsoft Azure Site Recovery Configuration Server dependencies.\n\n";
				}
			}
			else
			{
				errmsg << "Container registration got failed. Error code: " << exitcode << "\n\n";
			}
			
		}
			
		//end container registration
		else
		{
			//start DRA registration
			std::string cmd = "\"" + sysDrv + "\\" + AZURE_SITE_RECOVERY_PROVIDER_PATH + "\\" + DRCONFIGURATOR_EXE + "\"";
			cmd += " /r /friendlyname " + hostname + " /Credentials " + "\"" + credentialsFile + "\"";
			std::string draCmdToLog = cmd;

			if (proxytype.compare(CUSTOM_PROXY) == 0)
			{
				cmd += std::string(" ") + "/proxyaddress" + " " + proxyaddress + " " + "/proxyport" + " " + proxyport;
				draCmdToLog += std::string(" ") + "/proxyaddress" + " " + proxyaddress + " " + "/proxyport" + " " + proxyport;
			}

			if (proxytype.compare(CUSTOM_WITH_AUTHENTICATION_PROXY) == 0)
			{
				cmd += std::string(" ") + "/proxyaddress" + " " + proxyaddress + " " + "/proxyport" + " " + proxyport + " " + "/proxyusername" + " " + proxyusername + " " + "/proxypassword" + " " + proxypassword;
				draCmdToLog += std::string(" ") + "/proxyaddress" + " " + proxyaddress + " " + "/proxyport" + " " + proxyport + " " + "/proxyusername" + " " + proxyusername + " " + "/proxypassword" + "  ********";
			}

			DebugPrintf(SV_LOG_DEBUG, "ENTERED Command : %s\n", draCmdToLog.c_str());
			std::stringstream regDRAErr;
			if (!ExecuteProc(cmd, exitcode, regDRAErr))
			{
				errmsg << "Successfully completed the Container registration but failed in DRA registration.\n\n";
				DebugPrintf(SV_LOG_ERROR, "Failed to execute Command : %s\n", cmd.c_str());
			}
			else if (exitcode == 0 || exitcode == (int)SuccessfulNeedReboot)
			{
				isDRARegisterSucceeded = true;
				bool rebootrequired = false;
				if (exitcode == (int)SuccessfulNeedReboot)
				{
					rebootrequired = true;
				}
		
				//end DRA registration
				std::string agentInstalledPath;
				std::stringstream err;
				if (GetAgentInstallPath(agentInstalledPath, err))
				{
					//cdpcli.exe --registermt
					std::string regMTCmd = "\"" + agentInstalledPath + std::string("\\") + CDPCLI_EXE + "\"";
					regMTCmd += " --registermt";
					DebugPrintf(SV_LOG_DEBUG, "ENTERED Command : %s\n", regMTCmd.c_str());
					std::stringstream regMTErr;
					if (!ExecuteProc(regMTCmd, exitcode, regMTErr))
						errmsg << "Successfully completed the DRA registration and container registration but failed in MT registration.\n\n";
					else if (exitcode != 0)
						errmsg << "Successfully completed the DRA registration and container registration but failed in MT registration.Error code: " << exitcode << "\n\n";
					else
					{
						rv = true;
						if (rebootrequired)
						{
							DebugPrintf(SV_LOG_DEBUG, "returning reboot required exitcode.\n");
							exitcode = (int)SuccessfulNeedReboot;

						}
					}
				}
				else
				{
					errmsg << "Successfully completed the DRA registration and container registration but failed in MT registration. Error: " << err.str().c_str() << "\n\n";
					DebugPrintf(SV_LOG_ERROR, "%s\n", errmsg.str().c_str());
				}
			}
			else
			{
				errmsg << "DRA registration failed. Error logs are available at <SYSTEMDRIVE>\\ProgramData\\ASRLogs .\n\n";
				DebugPrintf(SV_LOG_ERROR, "%s\n", errmsg.str().c_str());
			}
		}
	}
	else
	{
		errmsg << "Credential file " << credentialsFile << " does not exist.\n";
		DebugPrintf(SV_LOG_ERROR, "%s\n", errmsg.str().c_str());
	}


	bool isMARSAgentServiceRestarted = true;

	if (isMarsProxySet || isDRARegisterSucceeded)
	{
		std::list<std::string> failedToRestartServices;
		std::list<std::string> services;
		services.push_back("svagents");
		services.push_back("obengine");
		boost::shared_ptr<ServiceMgr> cmgr(new ServiceMgr());
		std::stringstream err;
		if (!cmgr->RestartServices(services, failedToRestartServices, err))
		{
			isMARSAgentServiceRestarted = false;
			marsSettingErrmsg = err.str();/*"Failed to re-start Microsoft Azure Recovery Services Agent services. Please re-start the service using Windows Service Control Manager.\n"*/;
		}
	}
	if ((isMarsProxySettingRequire && !isMarsProxySet) || !isMARSAgentServiceRestarted)
	{
		errmsg << marsSettingErrmsg << "\n";
		DebugPrintf(SV_LOG_ERROR, "%s\n", errmsg.str().c_str()); 
		rv = false;
	}

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
    return rv;
}

