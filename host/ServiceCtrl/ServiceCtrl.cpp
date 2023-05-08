#include <stdio.h>
#include <windows.h>
#include <atlbase.h>
#include "globs.h"
#include "hostagenthelpers.h"
#include "portablehelpersmajor.h"
#include "inmsafecapis.h"
#include "terminateonheapcorruption.h"

const char *SERVICES_KEY_VALUE = "SYSTEM\\CurrentControlSet\\Services";

void DeletePreservedKey(char *ServiceName)
{
	CRegKey cregkey;
	DWORD dwResult = ERROR_SUCCESS;
	char KeyName[256];

	do
	{
		dwResult = cregkey.Open( HKEY_LOCAL_MACHINE, SV_VXAGENT_VALUE_NAME);
		if( ERROR_SUCCESS != dwResult )
		{
			//printf("Failed to open registry for key %s\n", SV_VXAGENT_VALUE_NAME );
			break;
		}

		inm_strcpy_s(KeyName,ARRAYSIZE(KeyName), "Service");
		inm_strcat_s(KeyName, ARRAYSIZE(KeyName),ServiceName);
		inm_strcat_s(KeyName,ARRAYSIZE(KeyName), "State");

		dwResult = cregkey.DeleteValue(KeyName);
		if( ERROR_SUCCESS != dwResult )
		{
			//printf("Failed to delete the temporary key\n");
			break;
		}
	} while (FALSE);
}

DWORD PreserveState(char *ServiceName, DWORD & Type, char retrieve)
{
	CRegKey cregkey;
	DWORD dwResult = ERROR_SUCCESS;
	char KeyName[256];

	do
	{
		dwResult = cregkey.Open( HKEY_LOCAL_MACHINE, SV_VXAGENT_VALUE_NAME);
		if( ERROR_SUCCESS != dwResult )
		{
			//printf("Failed to open registry for key %s\n", SV_VXAGENT_VALUE_NAME );
			break;
		}

		inm_strcpy_s(KeyName,ARRAYSIZE(KeyName), "Service");
		inm_strcat_s(KeyName,ARRAYSIZE(KeyName), ServiceName);
		inm_strcat_s(KeyName,ARRAYSIZE(KeyName), "State");

		if(retrieve)
		{
			dwResult = cregkey.QueryDWORDValue(KeyName, Type);
			if( ERROR_SUCCESS != dwResult )
			{
			//	printf("Failed to query registry value %s\n", KeyName );
				break;
			}
		}
		else
		{
			dwResult = cregkey.SetDWORDValue(KeyName, Type);
			if( ERROR_SUCCESS != dwResult )
			{
			//	printf("Failed to set registry value %s\n", KeyName );
				break;
			}
		}
	} while (FALSE);

	return dwResult;
}

DWORD GetServiceStartType(char *ServiceName, DWORD & StartType)
{
	CRegKey cregkey;
	DWORD dwResult = ERROR_SUCCESS;
	char Service[256];
	
	do
	{
		inm_strcpy_s(Service,ARRAYSIZE(Service), SERVICES_KEY_VALUE);
		inm_strcat_s(Service,ARRAYSIZE(Service), "\\");
		inm_strcat_s(Service, ARRAYSIZE(Service),ServiceName);

        dwResult = cregkey.Open( HKEY_LOCAL_MACHINE, Service);
		if( ERROR_SUCCESS != dwResult )
		{
			//printf("Failed to open registry for services\n");
			break;
		}	

		dwResult = cregkey.QueryDWORDValue( "Start", StartType);
        if( ERROR_SUCCESS != dwResult )
        {
            //printf("Failed to open the registry %s for service %s\n", Service, ServiceName);
			break;
        }
		//printf("Service start type %d\n", StartType);
			
	} while (FALSE);

	return dwResult;
}

DWORD SetServiceStartType(char *ServiceName, DWORD StartType)
{
	CRegKey cregkey;
	DWORD dwResult = ERROR_SUCCESS;
	char Service[256];
	
	do
	{
        inm_strcpy_s(Service, ARRAYSIZE(Service), SERVICES_KEY_VALUE);
        inm_strcat_s(Service, ARRAYSIZE(Service), "\\");
        inm_strcat_s(Service, ARRAYSIZE(Service), ServiceName);

        dwResult = cregkey.Open( HKEY_LOCAL_MACHINE, Service);
		if( ERROR_SUCCESS != dwResult )
		{
		//	printf("Failed to open registry for services\n");
			break;
		}

		dwResult = cregkey.SetDWORDValue("Start", StartType);
		if( ERROR_SUCCESS != dwResult )
		{
		//	printf("Failed to set registry value for service %s\n", ServiceName );
			break;
		}
	} while (FALSE);

	return dwResult;
}


BOOL WaitForServiceToReachState(SC_HANDLE hService, DWORD dwDesiredState,
                                 SERVICE_STATUS* pss, DWORD dwMilliseconds) {
 
    DWORD dwLastState, dwLastCheckPoint;
    BOOL  fFirstTime = TRUE; // Don't compare state & checkpoint the first time through
    BOOL  fServiceOk = TRUE;
    DWORD dwTimeout = GetTickCount() + dwMilliseconds;
 
    // Loop until the service reaches the desired state,
    // an error occurs, or we timeout
    while  (TRUE) {
       // Get current state of service
       fServiceOk = QueryServiceStatus(hService, pss);
 
       // If we can't query the service, we're done
       if (!fServiceOk) break;
 
       // If the service reaches the desired state, we're done
       if (pss->dwCurrentState == dwDesiredState) break;
 
       // If we timed-out, we're done
       if ((dwMilliseconds != INFINITE) && (dwTimeout > GetTickCount())) {
          SetLastError(ERROR_TIMEOUT); 
          break;
       }
 
       // If this is our first time, save the service's state & checkpoint
       if (fFirstTime) {
          dwLastState = pss->dwCurrentState;
          dwLastCheckPoint = pss->dwCheckPoint;
          fFirstTime = FALSE;
       } else {
          // If not first time & state has changed, save state & checkpoint
          if (dwLastState != pss->dwCurrentState) {
             dwLastState = pss->dwCurrentState;
             dwLastCheckPoint = pss->dwCheckPoint;
          } else {
             // State hasn't change, check that checkpoint is increasing
             if (pss->dwCheckPoint > dwLastCheckPoint) {
                // Checkpoint has increased, save checkpoint
                dwLastCheckPoint = pss->dwCheckPoint;
             } else {
                // Checkpoint hasn't increased, service failed, we're done!
                fServiceOk = FALSE; 
                break;
             }
          }
       }
       // We're not done, wait the specified period of time
       Sleep(pss->dwWaitHint);
    }
 
    // Note: The last SERVICE_STATUS is returned to the caller so
    // that the caller can check the service state and error codes.
    return(fServiceOk);
 }

HRESULT InstallCheckService(char *ServiceName)
{
	HRESULT hr = S_OK;
	BOOL fService = TRUE;

	do
	{
		// Open the SCM and the desired service.
		SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
		if(!hSCM)
		{
			hr = HRESULT_FROM_WIN32( GetLastError() );
			//printf("Unable to connect to SC Manager to stop service %s\n", ServiceName);
			break;
		}

		// PR#10815: Long Path support
		SC_HANDLE hService = SVOpenService(hSCM, ServiceName, SERVICE_STOP | SERVICE_QUERY_STATUS );
		if(!hService)
		{
			hr = HRESULT_FROM_WIN32( GetLastError() );
			//printf("Unable to open the service %s\n", ServiceName);
			CloseServiceHandle(hSCM);
			break;
		}

		SERVICE_STATUS ss;
		fService = QueryServiceStatus(hService, &ss);

		if(!fService)
		{
			hr = HRESULT_FROM_WIN32( GetLastError() );
			//printf("Unable to query the service %s\n", ServiceName);
			CloseServiceHandle(hService);
			CloseServiceHandle(hSCM);
			break;
		}
		//printf("Current state of service is %d\n", ss.dwCurrentState);
		if(ss.dwCurrentState & SERVICE_STOPPED)
		{
			//The service is already stopped, return success.
			hr = S_OK;
		}
		else if(!ControlService(hService, SERVICE_CONTROL_STOP, &ss))
		{
			//printf("Unable to issue stop request to service %s", ServiceName);
			hr = E_FAIL;
			CloseServiceHandle(hService);
			CloseServiceHandle(hSCM);
			break;
		} else {
			WaitForServiceToReachState(hService, SERVICE_STOP, &ss, 15000);
		}

		DWORD dwResult = ERROR_SUCCESS, StartType = 0;
		dwResult = GetServiceStartType(ServiceName, StartType);
		if(ERROR_SUCCESS != dwResult)
		{
			//printf("Failed to get start type for the service %s\n", ServiceName);
			hr = E_FAIL;
			CloseServiceHandle(hService);
			CloseServiceHandle(hSCM);
			break;
		}

		if(StartType == SERVICE_AUTO_START)
		{
			PreserveState(ServiceName, StartType,0);
			SetServiceStartType(ServiceName, SERVICE_DEMAND_START);
		}

		// Close the service and the SCM.
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCM);

	} while (FALSE);

	return hr;
}

void UnInstallCheckService(char *ServiceName)
{
	HRESULT hr = S_OK;
	DWORD StartType = 0;
	DWORD dwResult = ERROR_SUCCESS;

	dwResult = PreserveState(ServiceName, StartType, 1);
	if(ERROR_SUCCESS != dwResult)
	{
		return;
	}
	
	SetServiceStartType(ServiceName, StartType);
	DeletePreservedKey(ServiceName);
	return ;
}

int main(int argc, char *argv[])
{
	TerminateOnHeapCorruption();
	//calling init funtion of inmsafec library
	init_inm_safe_c_apis();
	std::string strOSVersion;
	char W2kString[] = "Microsoft Windows 2000";
	char forceflag[] = "force";
	
    GetOperatingSystemInfo(strOSVersion);

    if (0 == _strnicmp(strOSVersion.c_str(), W2kString, strlen(W2kString)))
	{
		if(strcmp(argv[2], "stop")==0)
		{
			if((argc == 4) && (0 == _stricmp(argv[3],forceflag)))
			{
				InstallCheckService(argv[1]);
			}
			else
			{
				printf("\"Distributed Link Tracking Client\" service will now be stopped.\n");
				printf("\t This is a non-essential service that interferes with volume access.\n");
				printf("\t It is highly recommended that this service be turned off.\n\n");

				printf("Press \"y\" or \"Y\" to continue stopping this service...\n");
				printf("Press \"n\" or \"N\" to proceed without stopping the service....\n");

				char ch;
				GetConsoleYesOrNoInput(ch);
				switch (ch)
				{
				case 'Y':
				case 'y':
					InstallCheckService(argv[1]);
					break;
				case 'N':
				case 'n':
					/*	printf("\nWARNING: Snapshot/Recovery may not succeed since you have chosen to not to stop\n");
					printf("this service which interferes with snapshot/recovery process\n");
					printf("\nPress enter to continue....\n");
					fflush(stdin);
					getchar();
					*/
					break;
				}
			}
		}
		else if(strcmp(argv[2],"restore")==0)
			UnInstallCheckService(argv[1]);
	}

	return 0;
}