#ifndef CONFSETTINGS__H
#define CONFSETTINGS__H

#include <string>

enum{
    SV_OK,
    SV_FAIL
};

enum{
	SCOUT_VX_ISTALLATION_REG_KEY = 5
};

/*
std::string SV_AMETHYST_CONF_FILE_PATH = "";
std::string SV_PUSHINSTALL_CONF_FILE_PATH = "";
std::string SV_PS_CXPS_CONF_FILE_PATH = "";
std::string SV_PS_CXPS_CONF_PATH = "";
std::string SV_DRSCOUT_CONF_FILE_PATH = "";
std::string SV_AGENT_CXPS_CONF_FILE_PATH = "";
std::string SV_CSPS_INSTALL_PATH = "";
std::string SV_AGENT_INSTALL_PATH = "";

bool SV_MTAGENT_INSTALLED = false;
*/


/// \brief ConfPathsInformer: Informs path for various conf files
class ConfPathsInformer
{
public:
	static void InitializeInstance();

	std::string AmethystConfPath();

	std::string PIConfPath();

	std::string CXPSConfPathOnPS();

	std::string CXPSInstallPathOnPS();

	std::string DrScoutConfPath();

	std::string CXPSConfPathOnMT();

	std::string CSPSInstallPath();

	std::string MTInstallPath();

	bool IsMTInstalled();

private:
	ConfPathsInformer();

private:
	std::string m_amethyst_conf_file_path;

	std::string m_pushinstall_conf_file_path;

	std::string m_ps_cxps_conf_file_path;

	std::string m_ps_cxps_conf_path;

	std::string m_drscout_conf_file_path;

	std::string m_agent_cxps_conf_file_path;

	std::string m_csps_install_path;

	std::string m_agent_install_path;

	bool m_is_mt_installed;
};


#define SV_CSPS_INSTALLER_CONF_PATH "Microsoft Azure Site Recovery\\Config\\App.conf"
#define SV_CSPS_INSTALLDIRECTORY_KEY "INSTALLATION_PATH"
#define SV_AMETHYST_CONF_FILE "home\\svsystems\\etc\\amethyst.conf"
#define SV_PUSH_CONF_FILE "pushinstaller.conf"
#define SV_PS_CXPS_CONF_FILE "home\\svsystems\\transport\\cxps.conf"
#define SV_DRSCOUT_CONF_FILE "Application Data\\etc\\drscout.conf"
#define SV_AGENT_CXPS_CONF_FILE "transport\\cxps.conf"
#define SV_PS_CXPS_CONF_DIR "home\\svsystems\\transport"
#define EMPTY_STRING "Empty"
#define PROXY_PASSWORD "proxypassword"


#define INMAGECONFIG_API extern "C" __declspec(dllexport)

INMAGECONFIG_API void freeAllocatedBuf(unsigned char** allocatedBuff);
INMAGECONFIG_API int StopServices(char** err);
INMAGECONFIG_API int StartServices(char** err);
INMAGECONFIG_API int UpdatePropertyValues(char* amethystReqStream, char* pushReqStream, char* cxpsRequestStream, char** err);
INMAGECONFIG_API int UpdateConfFilesInCS(char* amethystReqStream, char* cxpsReqStream, char* azureCsStream, char** err);
INMAGECONFIG_API int UpdateHostGUID(char** err);
INMAGECONFIG_API int UpdateIPs(char** err);
INMAGECONFIG_API int GetValueFromAmethyst(char* key, char** value, char** err);
INMAGECONFIG_API int GetValueFromCXPSConf(char* key, char** value, char** err);
INMAGECONFIG_API int GetHostName(char** hostname, char** err);
INMAGECONFIG_API int DoDRRegister(char* cmdinput,char** processexitcode, char** err);
INMAGECONFIG_API int GetPassphrase(char** passphrase, char** err);
INMAGECONFIG_API int ProcessPassphrase(char* req, char** err);

#endif