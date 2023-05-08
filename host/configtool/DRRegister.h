#ifndef DRREGISTER__H
#define DRREGISTER__H

#include <sstream>
#include <windows.h>

#include <boost/filesystem.hpp>

#include "extendedlengthpath.h"

#define AZURE_SITE_RECOVERY_PROVIDER_PATH "Program Files\\Microsoft Azure Site Recovery Provider"
#define CSPS_SERVER_BIN_PATH "home\\svsystems\\bin"

#define SETMARSPROXY_EXE "SetMarsProxy.exe"
#define DRCONFIGURATOR_EXE "DRConfigurator.exe"
#define CDPCLI_EXE "cdpcli.exe"

#define	BYPASS_PROXY "Bypass"
#define CUSTOM_PROXY "Custom"
#define CUSTOM_WITH_AUTHENTICATION_PROXY "CustomWithAuthentication"

enum DrRegReturnCode
{
	/// <summary>
	/// Setup was successful and we need a reboot
	/// </summary>
	SuccessfulNeedReboot = 3010,
};

class DRRegister
{
public:
    DRRegister(){}
	bool Register(const std::string& proxytype, const std::string& cspsinstallpath, std::string& hostname, const std::string& credentialsFile, DWORD& exitcode, std::stringstream& errmsg, const std::string& proxyaddress = "", const std::string& proxyport = "", const std::string& proxyusername = "", const std::string& proxypassword = "");

	bool SetMarsProxy(const std::string& proxytype, const std::string& cspsinstallpath, const std::string& proxyaddress, const std::string& proxyport, const std::string& proxyusername = "", const std::string& proxypassword = "");

    std::string GetHostName();
	bool GetAgentInstallPath(std::string& agentInstallPath, std::stringstream& errmsg);
	bool GetSystemDrv(std::string& sysdrv);
	bool IsMARSAgentInstalled();
	bool GetDraRegisteredVaultName(std::string& vaultName);
private:
    bool FileExist(const std::string& Name)
    {
        extendedLengthPath_t extName(ExtendedLengthPath::name(Name));
        return boost::filesystem::exists(extName);
    }
};



#endif //DRREGISTER__H