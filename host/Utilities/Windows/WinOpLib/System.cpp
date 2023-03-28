#include "stdafx.h"
#include "System.h"
#include "ActiveDirectory.h"
#include <lm.h>
#include <time.h>
#include "inmsafecapis.h"
#include "inmsafeint.h"
#include "inmageex.h"

#pragma comment(lib, "netapi32.lib")
using namespace std;

#define SERVICE_REG_KEY		"SYSTEM\\CurrentControlSet\\Services"
#define START_REG_VALUE		"Start"

int SystemMain(int argc, char* argv[])
{
    int iParsedOptions = 2;
	
	char szDomainName[MAX_PATH] = {0};
	char szUserName[MAX_PATH] = {0};
	char szPassword[MAX_PATH] = {0};
	
	bool bShutdown = 0;
	bool bRestart = 0;
	bool bLogOff = 0;
	bool bDisableDrivers = 0;
	bool bEnableDrivers = 0;
	bool bForce = 0;
	bool bStartService = 0;
	bool bStopService = 0;
	char *pListOfSystemsTobeShutDown = NULL;
	char *pListOfSystemsTobeRestarted = NULL;
	char *pListOfDriversTobeDisabled = NULL;
	char *pListOfDriversTobeEnabled = NULL;
	char *pListOfServicesTobeStarted = NULL;
	char *pListOfServicesTobeStopped = NULL;
	char *pListOfSystems = NULL;

	
	int OperationFlag = -1;
		
	if(argc == 2)
	{
		System_usage();
		exit(1);
	}

	for (;iParsedOptions < argc; iParsedOptions++)
    {
        if((argv[iParsedOptions][0] == '-') || (argv[iParsedOptions][0] == '/'))
        {
			if(stricmp((argv[iParsedOptions]+1),OPTION_SHUTDOWN) == 0)
            {
				iParsedOptions++;
				if( (iParsedOptions >= argc) ||(argv[iParsedOptions][0] == '-') || (argv[iParsedOptions][0] == '/'))
				{
					printf("Incorrect argument passed\n");
					System_usage();
					exit(1);
				}
				pListOfSystemsTobeShutDown = argv[iParsedOptions];
				bShutdown = true;
            }
			else if(stricmp((argv[iParsedOptions]+1),OPTION_RESTART) == 0)
            {
				iParsedOptions++;
				if( (iParsedOptions >= argc) ||(argv[iParsedOptions][0] == '-'))
				{
					printf("Incorrect argument passed\n");
					System_usage();
					exit(1);
				}
				pListOfSystemsTobeRestarted = argv[iParsedOptions];
				bRestart = true;
            }
			else if(stricmp((argv[iParsedOptions]+1),OPTION_ENABLEDRIVER) == 0)
            {
				iParsedOptions++;
				if( (iParsedOptions >= argc) ||(argv[iParsedOptions][0] == '-'))
				{
					printf("Incorrect argument passed\n");
					System_usage();
					exit(1);
				}
				pListOfDriversTobeEnabled = argv[iParsedOptions];
				bEnableDrivers  = true;
            }
			else if(stricmp((argv[iParsedOptions]+1),OPTION_DISABLEDRIVER) == 0)
            {
				iParsedOptions++;
				if( (iParsedOptions >= argc) ||(argv[iParsedOptions][0] == '-'))
				{
					printf("Incorrect argument passed\n");
					System_usage();
					exit(1);
				}
				pListOfDriversTobeDisabled = argv[iParsedOptions];
				bDisableDrivers = true;
            }
			else if(stricmp((argv[iParsedOptions]+1),OPTION_START) == 0)
            {
				iParsedOptions++;
				if( (iParsedOptions >= argc) ||(argv[iParsedOptions][0] == '-'))
				{
					printf("Incorrect argument passed\n");
					System_usage();
					exit(1);
				}
				pListOfServicesTobeStarted = argv[iParsedOptions];
				bStartService = true;
            }
			else if(stricmp((argv[iParsedOptions]+1),OPTION_STOP) == 0)
            {
				iParsedOptions++;
				if( (iParsedOptions >= argc) ||(argv[iParsedOptions][0] == '-'))
				{
					printf("Incorrect argument passed\n");
					System_usage();
					exit(1);
				}
				pListOfServicesTobeStopped = argv[iParsedOptions];
				bStopService = true;
            }
			else if(stricmp((argv[iParsedOptions]+1),OPTION_SYSTEM) == 0)
            {
				iParsedOptions++;
				if( (iParsedOptions >= argc) ||(argv[iParsedOptions][0] == '-'))
				{
					printf("Incorrect argument passed\n");
					System_usage();
					exit(1);
				}
				pListOfSystems = argv[iParsedOptions];
            }
			else if(stricmp((argv[iParsedOptions]+1),OPTION_FORCE) == 0)
            {
				iParsedOptions++;
				bForce= true;
            }
			else if(stricmp((argv[iParsedOptions]+1),OPTION_USER) == 0)
            {
				iParsedOptions++;
				if( (iParsedOptions >= argc) ||(argv[iParsedOptions][0] == '-'))
				{
					printf("Incorrect argument passed\n");
					System_usage();
					exit(1);
				}

				inm_strcpy_s(szUserName,ARRAYSIZE(szUserName),argv[iParsedOptions]);
            }
			else if(stricmp((argv[iParsedOptions]+1),OPTION_PASSWORD) == 0)
            {
				iParsedOptions++;
				if( (iParsedOptions >= argc) ||(argv[iParsedOptions][0] == '-'))
				{
					printf("Incorrect argument passed\n");
					System_usage();
					exit(1);
				}

				inm_strcpy_s(szPassword,ARRAYSIZE(szPassword),argv[iParsedOptions]);
            }
			else if(stricmp((argv[iParsedOptions]+1),OPTION_DOMAIN) == 0)
            {
				iParsedOptions++;
				if( (iParsedOptions >= argc) ||(argv[iParsedOptions][0] == '-'))
				{
					printf("Incorrect argument passed\n");
					System_usage();
					exit(1);
				}

				inm_strcpy_s(szDomainName,ARRAYSIZE(szDomainName),argv[iParsedOptions]);
            }
			else
			{
				printf("Incorrect argument passed\n");
				System_usage();
				exit(1);
			}
		}
		else
		{
			printf("Incorrect argument passed\n");
			System_usage();
			exit(1);
		}
	}


	DWORD dwReturn = NULL;
	if(bDisableDrivers)
	{
		dwReturn = DisableDrivers(pListOfDriversTobeDisabled, pListOfSystems , szDomainName, szUserName, szPassword);
	}
	if(bEnableDrivers)
	{
		dwReturn = EnableDrivers(pListOfDriversTobeEnabled, pListOfSystems, szDomainName, szUserName, szPassword);
	}
	if(bStartService)
	{
		StartServices(pListOfServicesTobeStarted, pListOfSystems, szDomainName, szUserName, szPassword);
	}
	if(bStopService)
	{
		StopServices(pListOfServicesTobeStopped, pListOfSystems, szDomainName, szUserName, szPassword);
	}
	if(bShutdown)
	{
		dwReturn = ShutdownSystems(pListOfSystemsTobeShutDown, bForce, szDomainName, szUserName, szPassword);
	}
	if(bRestart)
	{
		dwReturn = RestartSystems(pListOfSystemsTobeRestarted,bForce, szDomainName, szUserName, szPassword);
	}
		

	return 0;
}

void System_usage()
{	
	printf("Usage:\n");
	printf("Winop.exe  SYSTEM [-shutdown <System1,System2,...>] [-restart <System1,System2,...>][-logoff <System1,System2,...>] [-disabledriver <System1,System2,...>][-force] [[-domain <DomainName>] [-user <user name> -password <password> ]]\n");

}

DWORD ShutdownSystems(char* pListOfSystemsTobeShutDown, bool bForce, char* szDomainName,char* szUserName,char* szPassword)
{

	DWORD ret = true;
	HANDLE hToken = NULL;
	if(szUserName)
	{
		ret = LogonUser(szUserName,	szDomainName, szPassword, LOGON32_LOGON_NEW_CREDENTIALS, LOGON32_PROVIDER_WINNT50,&hToken);

		if(hToken)
		{
			ImpersonateLoggedOnUser(hToken);
		}
	}
	SetShutDownPrivilege();

	DWORD dwRet = ERROR_SUCCESS;
	vector <const char*> vListOfSystems;
	
	_TCHAR* token;
	_TCHAR* seps = ",;" ;
	bool bPrint = true;
	if(pListOfSystemsTobeShutDown)
	{
		token = _tcstok(pListOfSystemsTobeShutDown,seps);
			
		while( token != NULL )
		{
			if(IsDuplicateEntryPresent(vListOfSystems,token))
			{
				if(bPrint)
				{
					printf("\nWARNING:Duplicate System  entry found in passed arguments:\n");
					bPrint = false;
				}
				printf("Duplicate entry found for %s system, skipping the duplicate system name.\n",token);
				token = _tcstok( NULL, seps );
			}
			else
			{
				vListOfSystems.push_back(token );
				token = _tcstok( NULL, seps );
			}
		}
	}

	char szSystemName[MAX_PATH] = {0};
	for(unsigned int i = 0; i < vListOfSystems.size(); i++)
	{
		ZeroMemory(szSystemName,MAX_PATH);
		inm_sprintf_s(szSystemName, ARRAYSIZE(szSystemName), "\\\\%s", vListOfSystems[i]);
		short nRetry = 0;
		do
		{
			ret = InitiateSystemShutdownEx(szSystemName,"Shutdown is Initialted by WinOp.exe",0,bForce,FALSE,SHTDN_REASON_MAJOR_APPLICATION |SHTDN_REASON_FLAG_PLANNED);
			if(ret)
			{
				printf("Successfully initiated shutdown for the [%s] system\n",vListOfSystems[i]);
				break;
			}
			else
			{
				DWORD dwError = GetLastError();
				printf("Failed initiated shutdown for the [%s] system\n",vListOfSystems[i]);
				printf("Win32 Error:[%d]\n",dwError);
				if(  nRetry < 20 &&
					(ERROR_ACCESS_DENIED == dwError ||
					ERROR_NOT_READY == dwError)
					)
				{
					printf("Will retry shutdown after 45sec ... \n");
					//Bug#17966,18212. Due to clock sync issue Kerberos authanticaton is failing with error 5.
					//For information showing the times on DC and local machines for the first retry.
					if(ERROR_ACCESS_DENIED == dwError && 0 == nRetry)
					{
						NET_API_STATUS nStatus = 0;
						LPCWSTR lpPDCName = NULL;
						nStatus = NetGetDCName(NULL,NULL,(LPBYTE*)&lpPDCName);
						if(NERR_Success == nStatus)
						{
							cout << "DC Time: ";
							DisplayTOD(lpPDCName);
							NetApiBufferFree((LPVOID)lpPDCName);

							cout << "Local Time: ";
						    DisplayTOD(NULL);
						}
					}
					Sleep(45 * 1000);
					nRetry++;
				}
				else
				{
					ret = false;
					break;
				}
			}
		}while(1);
	}

	if(hToken)
	{
		RevertToSelf();
		CloseHandle(hToken);
	}
	return ret;
}

DWORD RestartSystems(char* pListOfSystemsTobeRestarted,bool bForce, char* szDomainName,char* szUserName,char* szPassword)
{

	DWORD ret = true;
	HANDLE hToken = NULL;
	if(szUserName)
	{
		ret = LogonUser(szUserName,	szDomainName, szPassword, LOGON32_LOGON_NEW_CREDENTIALS, LOGON32_PROVIDER_WINNT50,&hToken);

		if(hToken)
		{
			ImpersonateLoggedOnUser(hToken);
		}
	}
	SetShutDownPrivilege();

	DWORD dwRet = ERROR_SUCCESS;
	vector <const char*> vListOfSystems;
	
	_TCHAR* token;
	_TCHAR* seps = ",;" ;
	bool bPrint = true;
	if(pListOfSystemsTobeRestarted)
	{
		token = _tcstok(pListOfSystemsTobeRestarted,seps);
			
		while( token != NULL )
		{
			if(IsDuplicateEntryPresent(vListOfSystems,token))
			{
				if(bPrint)
				{
					printf("\nWARNING:Duplicate System  entry found in passed arguments:\n");
					bPrint = false;
				}
				printf("Duplicate entry found for %s system, skipping the duplicate system name.\n",token);
				token = _tcstok( NULL, seps );
			}
			else
			{
				vListOfSystems.push_back(token );
				token = _tcstok( NULL, seps );
			}
		}
	}

	char szSystemName[MAX_PATH] = {0};
	for(unsigned int i = 0; i < vListOfSystems.size(); i++)
	{
		ZeroMemory(szSystemName,MAX_PATH);
		inm_sprintf_s(szSystemName, ARRAYSIZE(szSystemName), "\\\\%s", vListOfSystems[i]);
		if(InitiateSystemShutdownEx(szSystemName,"Restart is Initialted by WinOp.exe",0,bForce,TRUE,SHTDN_REASON_MAJOR_APPLICATION |SHTDN_REASON_FLAG_PLANNED))
		{
			printf("Successfully initiated restart for the [%s] system\n",vListOfSystems[i]);
		}
		else
		{
			printf("Failed initiated restart for the [%s] system\n",vListOfSystems[i]);
			printf("Win32 Error:[%d]\n",GetLastError());
		}
	}

	if(hToken)
	{
		RevertToSelf();
		CloseHandle(hToken);
	}
	return ret;
	
}

DWORD DisableDrivers(char* pListOfDriversTobeDisabled, char* pListOfSystems, char* szDomainName,char* szUserName,char* szPassword)
{
	DWORD dwRet = TRUE;
	HANDLE hToken = NULL;
	HKEY hkResult = NULL;
	DWORD dwBufSize = 4;
	DWORD dwStartRegValue = 4;
	DWORD dwType = REG_DWORD;
	
	char* pSystems  = NULL;
	char* pDriversTobeDisabled  = NULL;
	vector <const char*> vListOfDrivers;
	vector <const char*> vListOfSystems;
	
	_TCHAR* token;
	_TCHAR* seps = ",;";

	if(szUserName)
	{
		dwRet = LogonUser(szUserName,	szDomainName, szPassword, LOGON32_LOGON_NEW_CREDENTIALS, LOGON32_PROVIDER_WINNT50,&hToken);

		if(hToken)
		{
			ImpersonateLoggedOnUser(hToken);
		}
	}

 ;
	bool bPrint = true;
	if(pListOfSystems)
	{
        size_t systemslen;
        INM_SAFE_ARITHMETIC(systemslen = InmSafeInt<size_t>::Type(strlen(pListOfSystems)) +1, INMAGE_EX(strlen(pListOfSystems)))
		pSystems = new char[systemslen];
		inm_strcpy_s(pSystems,(strlen(pListOfSystems) +1),pListOfSystems);

		token = _tcstok(pSystems,seps);
			
		while( token != NULL )
		{
			if(IsDuplicateEntryPresent(vListOfSystems,token))
			{
				if(bPrint)
				{
					printf("\nWARNING:Duplicate System  entry found in passed arguments:\n");
					bPrint = false;
				}
				printf("Duplicate entry found for %s system, skipping the duplicate system name.\n",token);
				token = _tcstok( NULL, seps );
			}
			else
			{
				vListOfSystems.push_back(token );
				token = _tcstok( NULL, seps );
			}
		}
	}
	else
	{
		char szComputerName[MAX_PATH+1] = {0};
		DWORD dwSize = MAX_PATH;
		GetComputerNameEx(ComputerNamePhysicalNetBIOS,szComputerName,&dwSize);
		vListOfSystems.push_back(szComputerName);
	}

	bPrint = true;
	if(pListOfDriversTobeDisabled)
	{
        size_t driversdisablelen;
        INM_SAFE_ARITHMETIC(driversdisablelen = InmSafeInt<size_t>::Type(strlen(pListOfDriversTobeDisabled)) +1, INMAGE_EX(strlen(pListOfDriversTobeDisabled)))
		pDriversTobeDisabled = new char[driversdisablelen];
		inm_strcpy_s(pDriversTobeDisabled,(strlen(pListOfDriversTobeDisabled) +1),pListOfDriversTobeDisabled);

		token = _tcstok(pDriversTobeDisabled,seps);
			
		while( token != NULL )
		{
			if(IsDuplicateEntryPresent(vListOfDrivers,token))
			{
				if(bPrint)
				{
					printf("\nWARNING:Duplicate diver entry found in passed arguments:\n");
					bPrint = false;
				}
				printf("Duplicate entry found for %s driver, skipping the duplicate driver name.\n",token);
				token = _tcstok( NULL, seps );
			}
			else
			{
				vListOfDrivers.push_back(token );
				token = _tcstok( NULL, seps );
			}
		}
	}

	char szSubKey[MAX_PATH] = {0};
	char szSystemName[MAX_PATH+2] = {0};
	for(unsigned int i = 0; i < vListOfSystems.size(); i++)
	{
		HKEY hKey = NULL;
		ZeroMemory(szSystemName,MAX_PATH);
		inm_sprintf_s(szSystemName, ARRAYSIZE(szSystemName), "\\\\%s", vListOfSystems[i]);

		if(RegConnectRegistry(szSystemName,HKEY_LOCAL_MACHINE,&hKey) == ERROR_SUCCESS)
		{
			printf("\nConnected to [%s] registry\n", vListOfSystems[i]);
		}	
		else
		{
			printf("\nFailed to connect [%s] registry\n",vListOfSystems[i]);
			continue;
		}

		for(unsigned int j = 0; j < vListOfDrivers.size(); j++)
		{
			ZeroMemory(szSubKey,MAX_PATH);
			inm_sprintf_s(szSubKey, ARRAYSIZE(szSubKey), "%s\\%s", SERVICE_REG_KEY, vListOfDrivers[j]);

			if(RegOpenKeyEx(hKey,szSubKey,0,KEY_SET_VALUE,&hkResult) == ERROR_SUCCESS)
			{
				if(RegSetValueEx(hkResult,START_REG_VALUE,NULL,dwType,(BYTE*)&dwStartRegValue,dwBufSize) == ERROR_SUCCESS)
				{
					printf("Disable the [%s] driver\n",vListOfDrivers[j]);
				}
				else
				{
					printf("Error: Failed to disable the [%s] driver\n",vListOfDrivers[j]);
					//dwRet = false;
				}
			}
			else
			{
				printf("ERROR:[%d] Failed to open [%s] registry\n", GetLastError(), szSubKey);
			}

			if(hkResult)
			{
				RegCloseKey(hkResult);
				hkResult = NULL;
			}
		}
		if(hKey)
		{
			RegCloseKey(hKey);
			hKey = NULL;
		}
	}

	if(hToken)
	{
		RevertToSelf();
		CloseHandle(hToken);
	}

	if(pSystems )
	{
		delete []pSystems;
	}
	if(pDriversTobeDisabled)
	{
		delete []pDriversTobeDisabled;
	}

	return dwRet;
}

DWORD EnableDrivers(char* pListOfDriversTobeEnabled, char* pListOfSystems, char* szDomainName,char* szUserName,char* szPassword)
{

	DWORD dwRet = TRUE;
	HANDLE hToken = NULL;
	HKEY hkResult = NULL;
	DWORD dwBufSize = 4;
	DWORD dwStartRegValue = 1;
	DWORD dwType = REG_DWORD;

	_TCHAR* token;
	_TCHAR* seps = ",;" ;

	char* pSystems  = NULL;
	char* pDriversTobeEnabled  = NULL;
	vector <const char*> vListOfDrivers;
	vector <const char*> vListOfSystems;

	if(szUserName)
	{
		dwRet = LogonUser(szUserName,	szDomainName, szPassword, LOGON32_LOGON_NEW_CREDENTIALS, LOGON32_PROVIDER_WINNT50,&hToken);

		if(hToken)
		{
			ImpersonateLoggedOnUser(hToken);
		}
	}
	
	
	bool bPrint = true;
	if(pListOfSystems)
	{
        size_t systemslen;
        INM_SAFE_ARITHMETIC(systemslen = InmSafeInt<size_t>::Type(strlen(pListOfSystems)) +1, INMAGE_EX(strlen(pListOfSystems)))
		pSystems = new char[systemslen];
		inm_strcpy_s(pSystems,(strlen(pListOfSystems) +1),pListOfSystems);

		token = _tcstok(pSystems,seps);
			
		while( token != NULL )
		{
			if(IsDuplicateEntryPresent(vListOfSystems,token))
			{
				if(bPrint)
				{
					printf("\nWARNING:Duplicate System  entry found in passed arguments:\n");
					bPrint = false;
				}
				printf("Duplicate entry found for %s system, skipping the duplicate system name.\n",token);
				token = _tcstok( NULL, seps );
			}
			else
			{
				vListOfSystems.push_back(token );
				token = _tcstok( NULL, seps );
			}
		}
	}
	else
	{
		char szComputerName[MAX_PATH+1] = {0};
		DWORD dwSize = MAX_PATH;
		GetComputerNameEx(ComputerNamePhysicalNetBIOS,szComputerName,&dwSize);
		vListOfSystems.push_back(szComputerName);
	}

	bPrint = true;
	if(pListOfDriversTobeEnabled)
	{
        size_t driversenablelen;
        INM_SAFE_ARITHMETIC(driversenablelen = InmSafeInt<size_t>::Type(strlen(pListOfDriversTobeEnabled)) +1, INMAGE_EX(strlen(pListOfDriversTobeEnabled)))
		pDriversTobeEnabled = new char[driversenablelen];
		inm_strcpy_s(pDriversTobeEnabled,(strlen(pListOfDriversTobeEnabled) +1),pListOfDriversTobeEnabled);

		token = _tcstok(pDriversTobeEnabled ,seps);
			
		while( token != NULL )
		{
			if(IsDuplicateEntryPresent(vListOfDrivers,token))
			{
				if(bPrint)
				{
					printf("\nWARNING:Duplicate diver entry found in passed arguments:\n");
					bPrint = false;
				}
				printf("Duplicate entry found for %s driver, skipping the duplicate driver name.\n",token);
				token = _tcstok( NULL, seps );
			}
			else
			{
				vListOfDrivers.push_back(token );
				token = _tcstok( NULL, seps );
			}
		}
	}

	char szSubKey[MAX_PATH] = {0};
	char szSystemName[MAX_PATH+2] = {0};
	for(unsigned int i = 0; i < vListOfSystems.size(); i++)
	{
		HKEY hKey = NULL;
		ZeroMemory(szSystemName,MAX_PATH);
		inm_sprintf_s(szSystemName, ARRAYSIZE(szSystemName), "\\\\%s", vListOfSystems[i]);

		if(RegConnectRegistry(szSystemName,HKEY_LOCAL_MACHINE,&hKey) == ERROR_SUCCESS)
		{
			printf("\nConnected to [%s] registry\n", vListOfSystems[i]);
		}	
		else
		{
			printf("\nFailed to connect [%s] registry\n",vListOfSystems[i]);
			continue;
		}

		for(unsigned int j = 0; j < vListOfDrivers.size(); j++)
		{
			ZeroMemory(szSubKey,MAX_PATH);
			inm_sprintf_s(szSubKey, ARRAYSIZE(szSubKey), "%s\\%s", SERVICE_REG_KEY, vListOfDrivers[j]);

			if(RegOpenKeyEx(hKey,szSubKey,0,KEY_SET_VALUE,&hkResult) == ERROR_SUCCESS)
			{
				if(RegSetValueEx(hkResult,START_REG_VALUE,NULL,dwType,(BYTE*)&dwStartRegValue,dwBufSize) == ERROR_SUCCESS)
				{
					printf("Enable the [%s] driver\n",vListOfDrivers[j]);
				}
				else
				{
					printf("Error: Failed to Enable the [%s] driver\n",vListOfDrivers[j]);
					//dwRet = false;
				}
			}

			if(hkResult)
			{
				RegCloseKey(hkResult);
				hkResult = NULL;
			}
		}
		if(hKey)
		{
			RegCloseKey(hKey);
			hKey = NULL;
		}
	}		

	if(hToken)
	{
		RevertToSelf();
		CloseHandle(hToken);
	}

	if(pSystems )
	{
		delete []pSystems;
	}

	if(pDriversTobeEnabled)
	{
		delete []pDriversTobeEnabled;
	}

	return dwRet;
}


DWORD StartServices(char* pListOfServicesTobeStarted, char* pListOfSystems, char* szDomainName,char* szUserName,char* szPassword)
{

	DWORD dwRet = TRUE;
	HANDLE hToken = NULL;
	
	_TCHAR* token;
	_TCHAR* seps = ",;" ;

	char* pSystems = NULL;
	char* pServicesTobeStarted = NULL;
	vector <const char*> vListOfServices;
	vector <const char*> vListOfSystems;

	if(szUserName)
	{
		dwRet = LogonUser(szUserName,	szDomainName, szPassword, LOGON32_LOGON_NEW_CREDENTIALS, LOGON32_PROVIDER_WINNT50,&hToken);

		if(hToken)
		{
			ImpersonateLoggedOnUser(hToken);
		}
	}
	
	bool bPrint = true;
	if(pListOfSystems)
	{
        size_t systemslen;
        INM_SAFE_ARITHMETIC(systemslen = InmSafeInt<size_t>::Type(strlen(pListOfSystems)) +1, INMAGE_EX(strlen(pListOfSystems)))
		pSystems = new char[systemslen];
		inm_strcpy_s(pSystems,(strlen(pListOfSystems) +1),pListOfSystems);

		token = _tcstok(pSystems,seps);
			
		while( token != NULL )
		{
			if(IsDuplicateEntryPresent(vListOfSystems,token))
			{
				if(bPrint)
				{
					printf("\nWARNING:Duplicate System  entry found in passed arguments:\n");
					bPrint = false;
				}
				printf("Duplicate entry found for %s system, skipping the duplicate system name.\n",token);
				token = _tcstok( NULL, seps );
			}
			else
			{
				vListOfSystems.push_back(token );
				token = _tcstok( NULL, seps );
			}
		}
	}
	else
	{
		char szComputerName[MAX_PATH+1] = {0};
		DWORD dwSize = MAX_PATH;
		GetComputerNameEx(ComputerNamePhysicalNetBIOS,szComputerName,&dwSize);
		vListOfSystems.push_back(szComputerName);
	}

	bPrint = true;
	if(pListOfServicesTobeStarted)
	{
        size_t servicesstartlen;
        INM_SAFE_ARITHMETIC(servicesstartlen = InmSafeInt<size_t>::Type(strlen(pListOfServicesTobeStarted)) +1, INMAGE_EX(strlen(pListOfServicesTobeStarted)))
		pServicesTobeStarted = new char[servicesstartlen];
		inm_strcpy_s(pServicesTobeStarted,(strlen(pListOfServicesTobeStarted) +1),pListOfServicesTobeStarted);

		token = _tcstok(pServicesTobeStarted,seps);
			
		while( token != NULL )
		{
			if(IsDuplicateEntryPresent(vListOfServices,token))
			{
				if(bPrint)
				{
					printf("\nWARNING:Duplicate service name entry found in passed arguments:\n");
					bPrint = false;
				}
				printf("Duplicate entry found for %s service, skipping the duplicate service name.\n",token);
				token = _tcstok( NULL, seps );
			}
			else
			{
				vListOfServices.push_back(token );
				token = _tcstok( NULL, seps );
			}
		}
	}

	char szSubKey[MAX_PATH] = {0};
	char szSystemName[MAX_PATH+2] = {0};
	for(unsigned int i = 0; i < vListOfSystems.size(); i++)
	{
		SC_HANDLE hSCM = NULL;
		HKEY hKey = NULL;
		ZeroMemory(szSystemName,MAX_PATH);
		inm_sprintf_s(szSystemName, ARRAYSIZE(szSystemName), "\\\\%s", vListOfSystems[i]);

		hSCM = OpenSCManager(szSystemName,NULL,SC_MANAGER_ALL_ACCESS);
		if(hSCM)
		{
			printf("\nConnected to [%s] SCM\n", vListOfSystems[i]);
		}	
		else
		{
			printf("\nError:[%d] Failed to connect [%s] SCM\n",GetLastError(),vListOfSystems[i]);
			continue;
		}

		for(unsigned int j = 0; j < vListOfServices.size(); j++)
		{
			SC_HANDLE hService = NULL;
			hService = OpenService(hSCM,vListOfServices[j],SC_MANAGER_ALL_ACCESS|SERVICE_ALL_ACCESS);

			if(!hService)
			{
				printf("Error:[%d] Failed to open [%s] service \n",GetLastError(),vListOfServices[j]);
				//dwRet = false;
			}

			if(StartService(hService,NULL,NULL))
			{
				printf("Successfully started [%s] service\n",vListOfServices[j]);
			}
			else
			{
				if(GetLastError() == ERROR_SERVICE_ALREADY_RUNNING)
				{
					printf("[%s] service is already running\n",vListOfServices[j]);
				}
				else
				{
					printf("Error: [%d] Failed to start [%s] service\n",GetLastError(),vListOfServices[j]);
				}
				
			}
			if(hService)
			{
				CloseServiceHandle(hService);
				hService = NULL;
			}
		}
		if(hSCM)
		{
			CloseServiceHandle(hSCM);
			hSCM = NULL;
		}
	}		

	if(hToken)
	{
		RevertToSelf();
		CloseHandle(hToken);
	}

	if(pSystems )
	{
		delete []pSystems;
	}

	if(pServicesTobeStarted)
	{
		delete []pServicesTobeStarted;
	}
	return dwRet;

}


DWORD StopServices(char* pListOfServicesTobeStopped, char* pListOfSystems, char* szDomainName,char* szUserName,char* szPassword)
{
	DWORD dwRet = TRUE;
	HANDLE hToken = NULL;
	
	_TCHAR* token;
	_TCHAR* seps = ",;" ;

	char* pSystems = NULL;
	char* pServicesTobeStopped = NULL;
	vector <const char*> vListOfServices;
	vector <const char*> vListOfSystems;

	if(szUserName)
	{
		dwRet = LogonUser(szUserName,	szDomainName, szPassword, LOGON32_LOGON_NEW_CREDENTIALS, LOGON32_PROVIDER_WINNT50,&hToken);

		if(hToken)
		{
			ImpersonateLoggedOnUser(hToken);
		}
	}
	
	bool bPrint = true;
	if(pListOfSystems)
	{
        size_t systemslen;
        INM_SAFE_ARITHMETIC(systemslen = InmSafeInt<size_t>::Type(strlen(pListOfSystems)) +1, INMAGE_EX(strlen(pListOfSystems)))
		pSystems = new char[systemslen];
		inm_strcpy_s(pSystems,(strlen(pListOfSystems) +1),pListOfSystems);

		token = _tcstok(pSystems,seps);
			
		while( token != NULL )
		{
			if(IsDuplicateEntryPresent(vListOfSystems,token))
			{
				if(bPrint)
				{
					printf("\nWARNING:Duplicate System  entry found in passed arguments:\n");
					bPrint = false;
				}
				printf("Duplicate entry found for %s system, skipping the duplicate system name.\n",token);
				token = _tcstok( NULL, seps );
			}
			else
			{
				vListOfSystems.push_back(token );
				token = _tcstok( NULL, seps );
			}
		}
	}
	else
	{
		char szComputerName[MAX_PATH+1] = {0};
		DWORD dwSize = MAX_PATH;
		GetComputerNameEx(ComputerNamePhysicalNetBIOS,szComputerName,&dwSize);
		vListOfSystems.push_back(szComputerName);
	}

	bPrint = true;
	if(pListOfServicesTobeStopped)
	{
        size_t servicesstoplen;
        INM_SAFE_ARITHMETIC(servicesstoplen = InmSafeInt<size_t>::Type(strlen(pListOfServicesTobeStopped)) +1, INMAGE_EX(strlen(pListOfServicesTobeStopped)))
		pServicesTobeStopped = new char[servicesstoplen];
		inm_strcpy_s(pServicesTobeStopped,(strlen(pListOfServicesTobeStopped) +1),pListOfServicesTobeStopped);

		token = _tcstok(pListOfServicesTobeStopped,seps);
			
		while( token != NULL )
		{
			if(IsDuplicateEntryPresent(vListOfServices,token))
			{
				if(bPrint)
				{
					printf("\nWARNING:Duplicate service name entry found in passed arguments:\n");
					bPrint = false;
				}
				printf("Duplicate entry found for %s service, skipping the duplicate service name.\n",token);
				token = _tcstok( NULL, seps );
			}
			else
			{
				vListOfServices.push_back(token );
				token = _tcstok( NULL, seps );
			}
		}
	}

	char szSubKey[MAX_PATH] = {0};
	char szSystemName[MAX_PATH+2] = {0};
	for(unsigned int i = 0; i < vListOfSystems.size(); i++)
	{
		SC_HANDLE hSCM = NULL;
		HKEY hKey = NULL;
		ZeroMemory(szSystemName,MAX_PATH);
		inm_sprintf_s(szSystemName, ARRAYSIZE(szSystemName), "\\\\%s", vListOfSystems[i]);

		hSCM = OpenSCManager(szSystemName,NULL,SC_MANAGER_ALL_ACCESS);
		if(hSCM)
		{
			printf("\nConnected to [%s] SCM\n", vListOfSystems[i]);
		}	
		else
		{
			printf("\nError:[%d] Failed to connect [%s] SCM\n",GetLastError(),vListOfSystems[i]);
			continue;
		}

		for(unsigned int j = 0; j < vListOfServices.size(); j++)
		{
			SC_HANDLE hService = NULL;
			hService = OpenService(hSCM,vListOfServices[j],SC_MANAGER_ALL_ACCESS|SERVICE_ALL_ACCESS);

			if(!hService)
			{
				printf("Error:[%d] Failed to open [%s] service \n",GetLastError(),vListOfServices[j]);
				//dwRet = false;
			}

			SERVICE_STATUS_PROCESS  serviceStatusProcess;
			SERVICE_STATUS serviceStatus;
			DWORD actualSize = 0;
			if(ControlService(hService,SERVICE_CONTROL_STOP,&serviceStatus))
			{
				int retry = 1;
				printf("Waiting for the service [%s] to stop\n",vListOfServices[j]);
				do 
				{
                    Sleep(1000); 
				} while (::QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&serviceStatusProcess, sizeof(serviceStatusProcess), &actualSize) && SERVICE_STOPPED != serviceStatusProcess.dwCurrentState && retry++ <= 600);
				
				if (QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&serviceStatusProcess, sizeof(serviceStatusProcess), &actualSize) && SERVICE_STOPPED == serviceStatusProcess.dwCurrentState) 
				{
					printf("Successfully stopped the service: [%s]\n",vListOfServices[j]);
				}
			}
			else
			{
				if(GetLastError() == ERROR_SERVICE_NOT_ACTIVE)
				{
					printf("[%s] service is already stopped\n",vListOfServices[j]);
				}
				else
				{
					printf("Error: [%d] Failed to stop [%s] service\n",GetLastError(),vListOfServices[j]);
				}
				
			}
			if(hService)
			{
				CloseServiceHandle(hService);
				hService = NULL;
			}
		}
		if(hSCM)
		{
			CloseServiceHandle(hSCM);
			hSCM = NULL;
		}
	}		

	if(hToken)
	{
		RevertToSelf();
		CloseHandle(hToken);
	}


	if(pSystems )
	{
		delete []pSystems;
	}

	if(pServicesTobeStopped)
	{
		delete []pServicesTobeStopped;
	}
	return dwRet;

}


bool SetShutDownPrivilege() 
{
	TOKEN_PRIVILEGES tokenPrivileges; 
	HANDLE hProcessToken; 
  
    //Get a Token for current   process. 
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hProcessToken)) 
	{
		printf("OpenProcessToken Failed %ld\n",GetLastError());
		return(FALSE); 
	}
 
   // Get the  locally unique identifier for the shutdown privilege. It used on a specified system to locally represent the specified privilege name. 
    if(LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME , &tokenPrivileges.Privileges[0].Luid) == FALSE)
   {
       printf("LookupPrivilegeValue Failed %ld\n", GetLastError() ) ;
       return FALSE ;
   }
   else
   {
       tokenPrivileges.PrivilegeCount = 1;  
       tokenPrivileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED; 
     
       // Get the shutdown privilege of current process. 
		if( AdjustTokenPrivileges(hProcessToken, FALSE, &tokenPrivileges, 0, (PTOKEN_PRIVILEGES)NULL, 0) == FALSE )
       {
            printf("AdjustTokenPrivileges Failed for SE_SHUTDOWN_NAME %ld\n", GetLastError()) ;
            return FALSE ;
       }
	   printf("AdjustTokenPrivileges last error %ld for SE_SHUTDOWN_NAME\n", GetLastError()) ;
   }
 
   if( LookupPrivilegeValue(NULL, SE_REMOTE_SHUTDOWN_NAME, &tokenPrivileges.Privileges[0].Luid) == FALSE )
   {
       printf("LookupPrivilegeValue Failed %ld\n", GetLastError() ) ;
       return FALSE; 
   }
   else
   {
       tokenPrivileges.PrivilegeCount = 1;  
       tokenPrivileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED; 
     
       // Get the shutdown privilege of the current process. 
       if( AdjustTokenPrivileges(hProcessToken, FALSE, &tokenPrivileges, 0,(PTOKEN_PRIVILEGES)NULL, 0) == FALSE )
       {
            printf("AdjustTokenPrivileges Failed for SE_REMOTE_SHUTDOWN_NAME %ld\n", GetLastError()) ;
            return FALSE ;
       }
	   printf("AdjustTokenPrivileges last error %ld for SE_REMOTE_SHUTDOWN_NAME\n", GetLastError()) ;
   }
   return TRUE;
}

DWORD changeServiceType(const std::string& svcName, emServiceStartType serviceType)
{
	DWORD dwRet = TRUE;

    SC_HANDLE hService;
	SC_HANDLE hSCManager;
    DWORD dwServiceType ;
    bool isProperType = true  ;
    switch( serviceType )
    {
        case SVCTYPE_MANUAL :
            dwServiceType = SERVICE_DEMAND_START ;
            break ;
        case SVCTYPE_DISABLED :
            dwServiceType = SERVICE_DISABLED ;
            break ;
        case SVCTYPE_AUTO :
            dwServiceType = SERVICE_AUTO_START ;
            break ;
        default :
            isProperType = false ;
    }
    if( isProperType )
    {
		// Opean and get the handle of the local SCM database. 
		hSCManager = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS);  
 		if(hSCManager)
		{
			hService = OpenService( hSCManager, svcName.c_str(),SERVICE_CHANGE_CONFIG);   
			if (NULL != hService)
			{ 
				if(!ChangeServiceConfig(hService, SERVICE_NO_CHANGE, dwServiceType,  SERVICE_NO_CHANGE, NULL,NULL,NULL,	NULL, NULL, NULL, NULL))            
				{
					printf("ChangeServiceConfig failed (%d)\n", GetLastError());
					dwRet = FALSE;
				}
				else
				{
					printf("ChangeServiceConfig succeeded");
				}
			}
			else
			{
				printf("openservice() failed with Error code :%d",GetLastError());
				dwRet = FALSE;
			}
		}
		else 
		{
			printf("OpenSCManager() failed with Error code :%d",GetLastError());
			dwRet = FALSE;
		}
		CloseServiceHandle(hService); 
		CloseServiceHandle(hSCManager);
	}
	else
	{
		printf("Inavlid service option.No Change is done to service start type.\n");
	}
    return dwRet ;
}

void DisplayTOD(LPCWSTR lpCName)
{
	LPTIME_OF_DAY_INFO pTimeOfDayBuff = 0;
	NET_API_STATUS nStatus = NetRemoteTOD(lpCName,(LPBYTE*)&pTimeOfDayBuff);
	if(NERR_Success == nStatus)
	{
		time_t elapsedTime = pTimeOfDayBuff->tod_elapsedt;
		struct tm * local_time = 0;
		char szFormatedTime[MAX_PATH];
		
		local_time = localtime(&elapsedTime);
		asctime_s(szFormatedTime,MAX_PATH,local_time);
		
		cout << szFormatedTime <<endl ;
		
		NetApiBufferFree(pTimeOfDayBuff);
	}
}
