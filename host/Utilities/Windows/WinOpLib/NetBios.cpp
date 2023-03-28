#include "stdafx.h"
#include <Lm.h>
#include "NetBios.h"
#include "inmsafecapis.h"



int NetBiosMain(int argc, char* argv[])
{
    int iParsedOptions = 2;
	
	char szMachineName[MAX_PATH] ={0};
	wchar_t wszMachineName[MAX_PATH] = {0};
	char szRemoteMachineName[MAX_PATH] ={0};
	wchar_t wszRemoteMachineName[MAX_PATH+2] = {0};
	char szDomainName[MAX_PATH] = {0};
	char szUserName[MAX_PATH] = {0};
	char szPassword[MAX_PATH] = {0};

	int OperationFlag = -1;
		
	if(argc == 2)
	{
		netbios_usage();
		exit(1);
	}

	for (;iParsedOptions < argc; iParsedOptions++)
    {
        if((argv[iParsedOptions][0] == '-') || (argv[iParsedOptions][0] == '/'))
        {
			if((stricmp((argv[iParsedOptions]+1),OPTION_ADD) == 0))
			{
				iParsedOptions++;
				if((iParsedOptions >= argc)||(argv[iParsedOptions][0] == '-') || (argv[iParsedOptions][0] == '/'))
				{
					printf("Incorrect argument passed\n");
					netbios_usage();
					exit(1);
				}
				inm_strcpy_s(szMachineName,ARRAYSIZE(szMachineName),argv[iParsedOptions]);
				mbstowcs(wszMachineName,szMachineName,strlen(szMachineName));
				OperationFlag = OPERATION_ADD;
            }
            else if(stricmp((argv[iParsedOptions]+1),OPTION_DELETE) == 0)
            {
				iParsedOptions++;
				if((iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') || (argv[iParsedOptions][0] == '/'))
				{
					printf("Incorrect argument passed\n");
					netbios_usage();
					exit(1);
				}
				inm_strcpy_s(szMachineName,ARRAYSIZE(szMachineName),argv[iParsedOptions]);
				mbstowcs(wszMachineName,szMachineName,strlen(szMachineName));
				OperationFlag = OPERATION_DELETE;

            }
			else if(stricmp((argv[iParsedOptions]+1),OPTION_CHANGE) == 0)
            {
				iParsedOptions++;
				if( (iParsedOptions >= argc) ||(argv[iParsedOptions][0] == '-')|| (argv[iParsedOptions][0] == '/'))
				{
					printf("Incorrect argument passed\n");
					netbios_usage();
					exit(1);
				}
				inm_strcpy_s(szMachineName,ARRAYSIZE(szMachineName),argv[iParsedOptions]);
				mbstowcs(wszMachineName,szMachineName,strlen(szMachineName));
				OperationFlag = OPERATION_CHANGE;
            }
			else if(stricmp((argv[iParsedOptions]+1),OPTION_REMOTE) == 0)
            {
				iParsedOptions++;
				if( (iParsedOptions >= argc) ||(argv[iParsedOptions][0] == '-'))
				{
					printf("Incorrect argument passed\n");
					netbios_usage();
					exit(1);
				}

				inm_strcpy_s(szRemoteMachineName,ARRAYSIZE(szRemoteMachineName),argv[iParsedOptions]);
				inm_wcscpy_s(wszRemoteMachineName,ARRAYSIZE(wszRemoteMachineName),L"\\\\");
				mbstowcs(wszRemoteMachineName+2,szRemoteMachineName,strlen(szRemoteMachineName));
            }
			else if(stricmp((argv[iParsedOptions]+1),OPTION_USER) == 0)
            {
				iParsedOptions++;
				if( (iParsedOptions >= argc) ||(argv[iParsedOptions][0] == '-'))
				{
					printf("Incorrect argument passed\n");
					netbios_usage();
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
					netbios_usage();
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
					netbios_usage();
					exit(1);
				}

				inm_strcpy_s(szDomainName,ARRAYSIZE(szDomainName),argv[iParsedOptions]);
            }
			else
			{
				printf("Incorrect argument passed\n");
				netbios_usage();
				exit(1);
			}
		}
		else
		{
			printf("Incorrect argument passed\n");
			netbios_usage();
			exit(1);
		}
	}

	switch(OperationFlag)
	{
		case OPERATION_ADD:
			AddComputerName(wszRemoteMachineName,wszMachineName,szDomainName,szUserName,szPassword);
			break;
		
		case OPERATION_DELETE:
			DeleteComputerName(wszRemoteMachineName,wszMachineName,szDomainName,szUserName,szPassword);
			break;
	
		case OPERATION_CHANGE:
			ChangeComputerName(wszRemoteMachineName,wszMachineName,szDomainName,szUserName,szPassword);
            break;
		default:
			break;
	}

	return 0;
}
void netbios_usage()
{	
	printf("Usage:\n");
	printf("Winop.exe  NETBIOS [-remote <Computer Name>] [-add <Name> |-delete <Name> | -changeto <Name> ] [-domain <DomainName> -user <user name> -password <password> ]\n");
	
	printf("\n-remote		When this option is specified, operation is\n");
	printf("		performed on that Machine. If not specified\n");
	printf("		operation is performed on local machine\n");
	
	printf("\n-add		This will add Netbios name to exisitng\n"); 
	printf("		Netbios name table.\n");
	
	printf("\n-delete		This will delete Netbios name from Netbios \n");
	printf("		name table\n");
	
	printf("\n-changeto	Use this option to change the Computer name.\n");
	printf("		It requires to reboot the machine. It also \n");
	

	printf("\nFollowing are optional flags.  They are used to perform remote or local\n");
	printf("operation where logged-on user does not have privilege to perfom NETBIOS operation.\n"); 
	
	printf("\n-domain		Specify the name of the domain or server\n");
	printf("		whose account database contains the user account.\n");
	printf("		It is an optional parameter. It is used along with -user\n");
	printf("		and -passsword. If -domain option is not used, local \n");
	printf("		computer domain is used. \n");
			
	printf("\n-user		Specify the name of the user. This is the name\n");
	printf("		of the user account to log on to.\n");
			
	printf("\n-password	Specify the plaintext password for the user \n");
	printf("		account specified in -user option.\n");
	
}

DWORD AddComputerName(wchar_t* wszRemoteMachineName, wchar_t* wszComputerName,char* szDomainName, char* szUserName, char* szPassword)
{
	DWORD ret;
	HANDLE hToken = NULL;
	if(szUserName)
	{
		ret = LogonUser(szUserName,	szDomainName, szPassword, LOGON32_LOGON_NEW_CREDENTIALS, LOGON32_PROVIDER_WINNT50,&hToken);

		if(hToken)
		{
			ImpersonateLoggedOnUser(hToken);
		}
	}
	
    ret = NetServerComputerNameAdd(wszRemoteMachineName,NULL,wszComputerName);
	if(NERR_Success == ret)
	{
		wprintf(L"Successfully added %s to NetBios name table.\n", wszComputerName);
		wprintf(L"NOTE: Add operation succeeds if the server name specified\n");
		wprintf(L"is added to at least one transport.\n");
	}
	else
	{
		wprintf(L"Error: Failed to add Computer Name. Win32 Error %d, return code %d\n", GetLastError(),ret);
	}
	
	RevertToSelf();
	if(hToken)
	{
		CloseHandle(hToken);
	}
	return ret;
}

DWORD DeleteComputerName(wchar_t* wszRemoteMachineName, wchar_t* wszComputerName, char* szDomainName, char* szUserName, char* szPassword)
{
	DWORD ret;
	HANDLE hToken = NULL;

	if(szUserName)
	{
		ret = LogonUser(szUserName,	szDomainName, szPassword, LOGON32_LOGON_NEW_CREDENTIALS, LOGON32_PROVIDER_WINNT50,&hToken);

		if(hToken)
		{
			ImpersonateLoggedOnUser(hToken);
		}
	}
	
	ret = NetServerComputerNameDel(wszRemoteMachineName,wszComputerName);
	if(NERR_Success == ret)
	{
		wprintf(L"Successfully deleted %s from NetBios name table", wszComputerName);
	}
	else
	{
		wprintf(L"Error: Failed to delete Computer Name:%s. Win32 Error %d\n",wszComputerName, ret);
	}
	RevertToSelf();
	if(hToken)
	{
		CloseHandle(hToken);
	}
	return ret;
}

DWORD ChangeComputerName(wchar_t* wszRemoteMachineName, wchar_t* wszComputerName,char* szDomainName, char* szUserName, char* szPassword)
{
    DWORD ret;
	HANDLE hToken = NULL;

	char szHostName[MAX_PATH] = {0};
	wchar_t wszHostName[MAX_PATH] = {0};
	gethostname(szHostName,MAX_PATH);
	mbstowcs(wszHostName,szHostName,MAX_PATH);

	if(szUserName)
	{
		ret = LogonUser(szUserName,	szDomainName, szPassword, LOGON32_LOGON_NEW_CREDENTIALS, LOGON32_PROVIDER_WINNT50,&hToken);

		if(hToken)
		{
			ImpersonateLoggedOnUser(hToken);
		}

	}
	
	ret = NetRenameMachineInDomain(wszRemoteMachineName,wszComputerName,NULL,NULL,NETSETUP_ACCT_CREATE);
	if(NERR_Success == ret)
	{
		wprintf(L"Successfully changed computer name from %s to %s", wszHostName,wszComputerName);
	}
	else
	{
		wprintf(L"Error: Failed to add Computer Name. Win32 Error %d", GetLastError());
	}

	RevertToSelf();
	if(hToken)
	{
		CloseHandle(hToken);
	}
	return ret;
}