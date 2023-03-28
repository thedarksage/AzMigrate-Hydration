#ifndef _SYSTEM_H
#define _SYSTEM_H


#define OPTION_SHUTDOWN					"shutdown"
#define OPTION_RESTART					"restart"
#define OPTION_DISABLEDRIVER			"disabledriver"
#define OPTION_ENABLEDRIVER				"Enabledriver"
#define OPTION_DOMAIN					"domain"
#define OPTION_USER						"user"
#define OPTION_PASSWORD					"password"
#define OPTION_FORCE					"force"
#define OPTION_SYSTEM					"sys"
#define OPTION_SERVICE					"service"
#define OPTION_START					"start"
#define OPTION_STOP						"stop"


enum emSystemOperation
{
	OPERATION_SHUTDOWN = 0,
	OPERATION_RESTART,
	OPERATION_DISABLEDRIVER,
	OPERATION_ENABLEDRIVER,
	OPERATION_START_SERVICE,
	OPERATION_STOP_SERVICE
};
enum emServiceStartType
{
    SVCTYPE_NONE,
    SVCTYPE_MANUAL,
    SVCTYPE_DISABLED,
    SVCTYPE_AUTO
} ;

int SystemMain(int argc, char* argv[]);
void System_usage();

DWORD ShutdownSystems(char* pListOfSystemsTobeShutDown, bool bForce  = 0, char* szDomainName = NULL,char* szUserName = NULL,char* szPassword=NULL);
DWORD RestartSystems(char* pListOfSystemsTobeRestarted, bool bForce ,char* szDomainName,char* szUserName,char* szPassword);
DWORD DisableDrivers(char* pListOfDriversTobeDisabled, char* pListOfSystems, char* szDomainName,char* szUserName,char* szPassword);
DWORD EnableDrivers(char* pListOfDriversTobeEnabled, char* pListOfSystems, char* szDomainName,char* szUserName,char* szPassword);
DWORD StartServices(char* pListOfServicesTobeStarted, char* pListOfSystems, char* szDomainName,char* szUserName,char* szPassword);
DWORD StopServices(char* pListOfServicesTobeStopped, char* pListOfSystems, char* szDomainName,char* szUserName,char* szPassword);
DWORD changeServiceType(const std::string& svcName, emServiceStartType serviceType);

void DisplayTOD(__in LPCWSTR lpCName=NULL);
bool SetShutDownPrivilege();

#endif //_SYSTEM_H