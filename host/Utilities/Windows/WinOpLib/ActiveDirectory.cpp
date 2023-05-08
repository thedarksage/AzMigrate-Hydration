#include "stdafx.h"
#include "ActiveDirectory.h"
#include "WMIHelper.h"
#include <Dsgetdc.h>
#include <Lm.h>
#include <atlconv.h>
#include "inmsafecapis.h"
#include "inmsafeint.h"
#include "inmageex.h"

using namespace std;
typedef struct _DS_REPSYNCALL_ERROR_STR_
{
	USHORT error;
	string strError;
}DS_REPSYNCALL_ERROR_STR;

DS_REPSYNCALL_ERROR_STR DsRepSyncCallError[]=
{
	{DS_REPSYNCALL_WIN32_ERROR_CONTACTING_SERVER,"The server %s can not be contacted\n"},
	{DS_REPSYNCALL_WIN32_ERROR_REPLICATING, "An error occurred during replication of the server %s\n"},
	{DS_REPSYNCALL_SERVER_UNREACHABLE,"The server %s is not reachable\n"}
};

int ActiveDirectoryMain(int argc, char* argv[])
{
    int iParsedOptions = 2;
	
	char szDCName[MAX_PATH] = {0};
	char szDomainName[MAX_PATH] = {0};
	char szUserName[MAX_PATH] = {0};
	char szPassword[MAX_PATH] = {0};
	
	bool bDoReplication = 0;
	bool bDoUpdateDnsServer = 0;
	bool bDoUpdateAllDnsServers = 0;
	bool bZoneUpdateFromDS = 0;
	char *pListOfDCsForDnsUpdate = NULL;
	char *pListOfZoneNames = NULL;
	char *pListOfNamingContexts = NULL;
	
	int OperationFlag = -1;
		
	if(argc == 2)
	{
		ActiveDirectory_usage();
		exit(1);
	}

	for (;iParsedOptions < argc; iParsedOptions++)
    {
        if((argv[iParsedOptions][0] == '-') || (argv[iParsedOptions][0] == '/'))
        {
			if(stricmp((argv[iParsedOptions]+1),OPTION_REPLICATE) == 0)
            {
				iParsedOptions++;
				if( (iParsedOptions >= argc) ||(argv[iParsedOptions][0] == '-') || (argv[iParsedOptions][0] == '/'))
				{
					printf("Incorrect argument passed\n");
					ActiveDirectory_usage();
					exit(1);
				}
				pListOfNamingContexts = argv[iParsedOptions];
				bDoReplication = true;
            }
			else if(stricmp((argv[iParsedOptions]+1),OPTION_USER) == 0)
            {
				iParsedOptions++;
				if( (iParsedOptions >= argc) ||(argv[iParsedOptions][0] == '-'))
				{
					printf("Incorrect argument passed\n");
					ActiveDirectory_usage();
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
					ActiveDirectory_usage();
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
					ActiveDirectory_usage();
					exit(1);
				}

				inm_strcpy_s(szDomainName,ARRAYSIZE(szDomainName),argv[iParsedOptions]);
            }
			else if(stricmp((argv[iParsedOptions]+1),OPTION_DOMAIN_CONTROLER_NAME) == 0)
            {
				iParsedOptions++;
				if( (iParsedOptions >= argc) ||(argv[iParsedOptions][0] == '-'))
				{
					printf("Incorrect argument passed\n");
					ActiveDirectory_usage();
					exit(1);
				}

				inm_strcpy_s(szDCName,ARRAYSIZE(szDCName),argv[iParsedOptions]);
            }
			else if(stricmp((argv[iParsedOptions]+1),OPTION_UPDATE_DNS_SERVER) == 0)
            {
				iParsedOptions++;
				if( (iParsedOptions >= argc) ||(argv[iParsedOptions][0] == '-'))
				{
					printf("Incorrect argument passed\n");
					ActiveDirectory_usage();
					exit(1);
				}
				bDoUpdateDnsServer = true;
				pListOfDCsForDnsUpdate  = argv[iParsedOptions];
            }
			else if(stricmp((argv[iParsedOptions]+1),OPTION_UPDATE_ALL_DNS_SERVERS) == 0)
            {
				bDoUpdateAllDnsServers = true;
            }
			else if(stricmp((argv[iParsedOptions]+1),OPTION_ZONE_UPDATE_FROM_DS) == 0)
            {
				iParsedOptions++;
				if( (iParsedOptions >= argc) ||(argv[iParsedOptions][0] == '-'))
				{
					printf("Incorrect argument passed\n");
					ActiveDirectory_usage();
					exit(1);
				}
				bZoneUpdateFromDS = true;
				pListOfZoneNames  = argv[iParsedOptions];
            }
			else
			{
				printf("Incorrect argument passed\n");
				ActiveDirectory_usage();
				exit(1);
			}
		}
		else
		{
			printf("Incorrect argument passed\n");
			ActiveDirectory_usage();
			exit(1);
		}
	}

	if(bDoUpdateDnsServer &&  bDoUpdateAllDnsServers)
	{
		printf("-UpdateAllDnsServer and -UpdateDnsServer can not be used together.\n");
		printf("Use either of it\n");
		ActiveDirectory_usage();
		exit(1);
	}

	if((pListOfZoneNames) && (!(bDoUpdateDnsServer || bDoUpdateAllDnsServers)))
	{
		printf("Incorrect argument passed\n");
		printf("-ZoneUpdateFromDs should be used either with -UpdateAllDnsServers or -UpdateDnsServer\n");
		ActiveDirectory_usage();
		exit(1);
	}

	if(bDoReplication && (bDoUpdateDnsServer || bDoUpdateAllDnsServers))
	{
		OperationFlag = OPERATION_REPLICATE_AND_DNS_UPDATE;
	}
	else if((bDoReplication ==0) && (bDoUpdateAllDnsServers || bDoUpdateDnsServer))
	{
		OperationFlag = OPERATION_DNS_UPDATE_FROM_DS;
	}
	else if((bDoReplication ==1) && (bDoUpdateAllDnsServers ==0) && (bDoUpdateDnsServer == 0))
	{
		OperationFlag = OPERATION_REPLICATE;
	}
	else
	{
		printf("Incorrect argument passed\n");
		ActiveDirectory_usage();
		exit(1); 
	}

 
	switch(OperationFlag)
	{
		case OPERATION_REPLICATE:
			DoADReplication(szDCName,pListOfNamingContexts,szDomainName,szUserName,szPassword);
			break;
		case OPERATION_DNS_UPDATE_FROM_DS:
			DoDnsUpdatefromDS(szDCName,szDomainName,szUserName,szPassword,bDoUpdateAllDnsServers,pListOfDCsForDnsUpdate,pListOfZoneNames);
			break;
		case OPERATION_REPLICATE_AND_DNS_UPDATE:
			DoReplicationAndDnsUpdatefromDS(szDCName,pListOfNamingContexts,szDomainName,szUserName,szPassword,bDoUpdateAllDnsServers,pListOfDCsForDnsUpdate,pListOfZoneNames);
			break;
		default:
			break;
	}

	return 0;
}
void ActiveDirectory_usage()
{	
	printf("Usage:\n");
	printf("Winop.exe  AD [[-replicate <NamingContext> ]|[-UpdateAllDnsServers |-UpdateDnsServer <DNS server name1,name2,name3,...>]] [-ZoneUpdateFromDs <ZoneName1,ZoneNam2...>][-dc <DomainControllerName>] [-domain <DomainName>] [-user <user name> -password <password> ]\n");
	
	printf("\n-replicate	Specify list of distinguished name of the naming contexts to\n");
	printf("\t\tsynchronize. Naming contexts should be separated by semicolon.\n"); 
	printf("\t\tDEFAULT key word can be used for configuration naming context. \n");
	printf("\t\tOther naming contexts are as below\n");
	printf("\t\t\"DC=MyDomain,DC=COM\"  for Domain\n");
	printf("\t\t\"CN=Configuration,DC=MyDomain,DC=COM for Configuration\n");
	printf("\t\t\"CN=Schema,CN=Configuration,DC=MyDomain,DC=COM\" for Schema\n");
	printf("\t\t\"DC=ForestDnsZones,DC=MyDomain,DC=COM\" for ForestDnsZones\n");
	printf("\t\t\"DC=DomainDnsZones,DC=MyDomain,DC=COM\" for DomainDnsZones\n");

	printf("\n-UpdateAllDnsServers	Use this switch to update all the DNS Servers's\n");
	printf("\t\tDNS zone from Directory Services. DNS Server must be\n");
	printf("\t\tinstegrated with Active Directory. This switch does not\n");
	printf("\t\ttake any parameter. It can be used with -replicate switch.\n");
	printf("\t\tWhen used with -replicate, replication happens first and\n");
	printf("\t\tthen DNS Zone update. By default, it updates all the Zones\n");
	printf("\t\tof all the DNS Servers in that domain that are DS integrated.\n");
	printf("\t\tIf -ZoneUpdateFromDs is used along with this switch, only\n");
	printf("\t\tspecified DNS Zones are updated from all the DNS Servers.\n");
	printf("\t\tThis switch can not be used with -UpdateDNSServer.\n");
	
	printf("\n-UpdateDnsServer    Use this switch to update specific DNS Servers'\n");
	printf("\t\tDNS zone from Directory Services. DNS Server has to be\n");
	printf("\t\tintegrated with Active Directory. It can be use along with\n"); 
	printf("\t\t-replicate switch. When used with -replicate, replication \n");
	printf("\t\thappens first and then DNS Zone update. By default, it updates\n"); 
	printf("\t\tall the Zones of the specified DNS Servers.\n");
	printf("\t\tIf -ZoneUpdateFromDs is used along with this, only specified\n");
	printf("\t\t DNS Zones are updated from specified DNS Servers. \n");
	printf("\t\tDNS Servers names are separated either by comma or semicolon.\n");
	printf("\t\tExample:  -UpdateDnsServer DC1,DC2,DC3\n");
	printf("\t\tSince DNS Server is integrated with Active Directory, name \n");
	printf("\t\tof the DNS Server is same as that of Domain Controller\n");

	printf("\nFollowing are all optional switches\n");

    printf("\n-dc \t\tSpecify the name of the domain controller from\n");
	printf("\t\twhere you want to replicate AD contents.\n");
	printf("\t\tif both -dc and -domain are specified, dc takes the precedence.\n");
	printf("\t\tif non of them is specified it will bing to global catalog\n");
	printf("\t\tserver in the forest of the local computer.\n");

	printf("\n-ZoneUpdateFromDs	Use this switch to update specific DNS zone.\n");
	printf("\t\t It should be used with either -UpdateAllDnsServers or\n");
	printf("\t\t-UpdateDnsServer. The zone must be integrated with DS.\n");
	printf("\t\tOtherwise update will not happen fot the zones. \n");
	printf("\t\tZone names are separated either by comma or semicolon\n");
	printf("\t\tExample:  -ZoneUpdateFromDs ZoneName1,ZoneName,ZoneName3\n");
	
	printf("\n-domain \tSpecify the name of the domain or server\n");
	printf("\t\twhose account database contains the user account.\n");
	printf("\t\t It is an optional parameter. It is used along with -user\n");
	printf("\t\tand -passsword. If -domain option is not used, local \n");
	printf("\t\tcomputer domain is used. \n");
			
	printf("\n-user \t\tSpecify the name of the user. This is the name\n");
	printf("\t\tof the user account to log on to.\n");
			
	printf("\n-password \tSpecify the plaintext password for the user \n");
	printf("\t\taccount specified in -user option.\n");

	printf("\nNOTE:DCs can be synchronized across site boundries only if they are connected\n");
	printf("by a synchronous (RPC) transport.\n");

	printf("\nExamples: \n");
	printf("Only replication example:\n");
	printf("WinOp.exe AD -replicate DEFAULT \n");
	printf("WinOp.exe AD -replicate \"DC=Schema,DC=Configuration,DC=mydomian,DC=COM\" \n");
	printf("WinOp.exe AD -replicate \"DC=Schema,DC=Configuration,DC=mydomian,DC=COM\" -dc SALES_DC\n");
	printf("WinOp.exe AD -replicate \"DC=mydomian,DC=COM\" -domain mydomain.com -user administrator -password mycred\n");
	printf("WinOp.exe AD -replicate DEFAULT;\"DC=Schema,DC=Configuration,DC=mydomian,DC=COM\" \n");
	printf("WinOp.exe AD -replicate \"DC=mydomian,DC=COM\";\"DC=Schema,DC=Configuration,DC=mydomian,DC=COM\" -dc SALES_DC\n");

	printf("\nOnly DNS Zone update example:\n");
	printf("WinOp.exe AD -UpdateAllDnsServers\n");
	printf("WinOp.exe AD -UpdateAllDnsServers -ZoneUpdateFromDs Inmange.net;DevZone\n");
	printf("WinOp.exe AD -UpdateDnsServer DC1,DC2,DC3\n");
	printf("WinOp.exe AD -UpdateDnsServer DC1,DC2,DC3 -ZoneUpdatefromDs Inmange.net;DevZone\n");

	printf("\nReplication and DNS zone update together:\n");
	printf("WinOp.exe AD -replicate DEFAULT -UpdateAllDnsServers\n");
	printf("WinOp.exe AD -replicate DEFAULT -UpdateDnsServer DC1,DC2,DC3\n");
	printf("WinOp.exe AD -replicate DEFAULT;\"DC=mydomian,DC=COM\" -UpdateDnsServer DC1,DC2,DC3\n");
	printf("WinOp.exe AD -replicate \"DC=mydomian,DC=COM\" -domain mydomain.com -user administrator -password mycred -UpdateDnsServer DC1,DC2\n");
	printf("WinOp.exe AD -replicate \"DC=mydomian,DC=COM\" -dc DC1 -domain mydomain.com -user administrator -password mycred -UpdateDnsServer DC1,DC2\n");
	printf("WinOp.exe AD -replicate \"DC=mydomian,DC=COM\" -dc DC1 -domain mydomain.com -user administrator -password mycred -UpdateAllDnsServers\n");
}

//Do AD replication
DWORD DoADReplication(char* szDCName, char* pListOfNamingContexts,char* szDomainName,char* szUserName,char* szPassword)
{
	HANDLE hDirectoryService = NULL;
	PDS_REPSYNCALL_ERRINFO* pDsError;
	RPC_AUTH_IDENTITY_HANDLE hAuthIdentity = NULL;
	DWORD dwRet = ERROR_SUCCESS;
	vector <const char*> vListOfNamingContexts;
	DWORD dwListOfNamingContexts = 0;

	_TCHAR* token;
	_TCHAR* seps = ";" ;
	
	token = _tcstok(pListOfNamingContexts,seps);
	bool bPrint = true;

	while( token != NULL )
	{
		if(IsDuplicateEntryPresent(vListOfNamingContexts,token))
		{
			if(bPrint)
			{
				printf("\nWARNING:Duplicate Naming Context entry found in passed arguments:\n");
				bPrint = false;
			}
			printf("Duplicate entry found for \"%s\" Naming Context, skipping the duplicate Naming Context.\n",token);
			token = _tcstok( NULL, seps );
		}
		else
		{
			vListOfNamingContexts.push_back(token );
			token = _tcstok( NULL, seps );
		}
	}

    dwRet = BindToDirectoryService(hDirectoryService,hAuthIdentity,szDCName,szDomainName,szUserName,szPassword);
	if(dwRet != ERROR_SUCCESS)
	{
		INM_DONE();
	}

	dwListOfNamingContexts = (DWORD) vListOfNamingContexts.size();

	for(DWORD dwIndex = 0; dwIndex < dwListOfNamingContexts; dwIndex++)
	{
		printf("\n\n[%d] Starting replication for \"%s\" Naming context\n",dwIndex+1, vListOfNamingContexts[dwIndex]);
		if(!stricmp(vListOfNamingContexts[dwIndex],"default"))
		{
			dwRet = DsReplicaSyncAll(hDirectoryService,NULL,DS_REPSYNCALL_PUSH_CHANGES_OUTWARD |DS_REPSYNCALL_CROSS_SITE_BOUNDARIES|
											DS_REPSYNCALL_ID_SERVERS_BY_DN,	SyncUpdateProc,NULL,&pDsError);
		}
		else
		{
			dwRet = DsReplicaSyncAll(hDirectoryService,vListOfNamingContexts[dwIndex],DS_REPSYNCALL_PUSH_CHANGES_OUTWARD |
											DS_REPSYNCALL_CROSS_SITE_BOUNDARIES | DS_REPSYNCALL_ID_SERVERS_BY_DN ,
											SyncUpdateProc,NULL,&pDsError);
		}

		if((dwRet== ERROR_SUCCESS) && !(pDsError))
		{
			printf("\nSucessfully replicated \"%s\" to all the servers\n",vListOfNamingContexts[dwIndex]);
		}
		else
		{
			printf("\nERROR: Fail to replicate \"%s\" to one or more servers.\n",vListOfNamingContexts[dwIndex]);
			LPVOID lpInmMessageBuffer;
			::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|	FORMAT_MESSAGE_FROM_SYSTEM, NULL, dwRet, 0,	
				(LPTSTR)&lpInmMessageBuffer, 0,	NULL);

			if(dwRet)
			{
				printf("Error:[%d] %s\n",dwRet,(LPCTSTR)lpInmMessageBuffer);
			}
		
			LocalFree(lpInmMessageBuffer);
		}

		if(pDsError)
		{
			LocalFree(pDsError);
			pDsError = NULL;
		}
	}	
INM_EXITD();
	
	DsUnBind(&hDirectoryService);
	if(hAuthIdentity)
	{
		DsFreePasswordCredentials(hAuthIdentity); 
	}

	return dwRet;
}

BOOL WINAPI SyncUpdateProc( LPVOID pData, PDS_REPSYNCALL_UPDATE pUpdate)
{
	switch(pUpdate->event)
	{
		printf("\n");
		case DS_REPSYNCALL_EVENT_ERROR:
			printf("\nAn error occurred.\n");
			if(pUpdate->pErrInfo)
			{
				printf("Source Server = %s\n",pUpdate->pErrInfo->pszSrcId);
				printf("Destination Server = %s\n",pUpdate->pErrInfo->pszSvrId);
				printf(DsRepSyncCallError[pUpdate->pErrInfo->error].strError.c_str(),pUpdate->pErrInfo->pszSvrId);
				LPVOID lpInmMessageBuffer;
				::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|	FORMAT_MESSAGE_FROM_SYSTEM,	NULL,pUpdate->pErrInfo->dwWin32Err,
					0,	(LPTSTR)&lpInmMessageBuffer,	0, NULL);

				printf("Error:[%d] %s\n",pUpdate->pErrInfo->dwWin32Err,(LPCTSTR)lpInmMessageBuffer);
				
				LocalFree(lpInmMessageBuffer);
				//printf("Win32 Error:%d\n",pUpdate->pErrInfo->dwWin32Err);
			}
	
			break;
		case DS_REPSYNCALL_EVENT_SYNC_STARTED: 
				printf("\nSynchronization of two servers has started");
				break;
		case DS_REPSYNCALL_EVENT_SYNC_COMPLETED:
				printf("\nSynchronization of two servers has just finished");
				break;
		case DS_REPSYNCALL_EVENT_FINISHED:
				printf("\nExecution of resync call is complete\n");
				break;
	}
	if(pUpdate->pSync)
	{
		printf("\n-------------------------------------------\n");
		printf("Source Server =  %s\n",pUpdate->pSync->pszSrcId); 
		printf("Destination Server = %s\n",pUpdate->pSync->pszDstId);
		printf("------------------------------------------\n");
	}
	return true;
}

// Bind with DS using DsBind or DsBindWithCred depending upon the parameter passed.
//if user, and password are NULL function will use DsBind else DsBindWithCred.
DWORD BindToDirectoryService(HANDLE &hDirectoryService,RPC_AUTH_IDENTITY_HANDLE &hAuthIdentity, char* szDCNameArg,char* szDomainNameArg,char* szUserNameArg,char* szPasswordArg)
{
	DWORD dwRet = ERROR_SUCCESS;
	char szUserName[MAX_PATH] ={0};
	char szDomainName[MAX_PATH] ={0};
	char szPassword[MAX_PATH] = {0};
	char szDCName[MAX_PATH] = {0};

	//check whether user and password are null, if yes use 
	//DsBind else DsBindWithCred
	if(szUserNameArg)
	{
		 inm_strcpy_s(szUserName,ARRAYSIZE(szUserName),szUserNameArg);
	}
	if(szPasswordArg)
	{
		inm_strcpy_s(szPassword,ARRAYSIZE(szPassword),szPasswordArg);
	}
	if(szDomainNameArg)
	{
		inm_strcpy_s(szDomainName,ARRAYSIZE(szDomainName),szDomainNameArg);
	}
	if(szDCNameArg)
	{
		inm_strcpy_s(szDCName,ARRAYSIZE(szDCName),szDCNameArg);
	}

	if(!(strlen (szUserName)|| strlen(szPassword)))
	{
		if((strlen(szDCName)== 0) && strlen (szDomainName))
		{
			dwRet = DsBind(NULL,szDomainName,&hDirectoryService);
		}
		else if(strlen(szDCName) && (strlen (szDomainName) ==0))
		{
			dwRet = DsBind(szDCName,NULL,&hDirectoryService);
		}
		else if(strlen(szDCName) && strlen (szDomainName))
		{
			dwRet = DsBind(szDCName,szDomainName,&hDirectoryService);
		}
		else
		{
			dwRet = DsBind(NULL,NULL,&hDirectoryService);
		}
		if(dwRet !=ERROR_SUCCESS)
		{
			printf("Error occured while binding to domain controller.\n");
			printf("Error ID: %d\n",dwRet);
			INM_DONE();
		}
	}
	else
	{
		if(strlen(szDomainName) == 0)
		{
			dwRet = DsMakePasswordCredentials(szUserName,NULL,szPassword,&hAuthIdentity);
		}
		else
		{
			dwRet = DsMakePasswordCredentials(szUserName,szDomainName,szPassword,&hAuthIdentity);
		}
		
		if(dwRet !=ERROR_SUCCESS)
		{
			printf("Error occured while constructing a credential handle.\n");
			printf("Error ID: %d\n",dwRet);
			INM_DONE();
		}
		if((strlen(szDCName)== 0) && strlen (szDomainName))
		{
			dwRet = DsBindWithCred(NULL,szDomainName,hAuthIdentity,&hDirectoryService);
		}
		else if(strlen(szDCName) && (strlen (szDomainName) ==0))
		{
			dwRet = DsBindWithCred(szDCName,NULL,hAuthIdentity,&hDirectoryService);
		}
		else if(strlen(szDCName) && strlen (szDomainName))
		{
			dwRet = DsBindWithCred(szDCName,szDomainName,hAuthIdentity,&hDirectoryService);
		}
		else
		{
			dwRet = DsBindWithCred(NULL,NULL,hAuthIdentity,&hDirectoryService);
		}

		if(dwRet !=ERROR_SUCCESS)
		{
			printf("Error occured while binding to domain controller using speicified credential\n");
			printf("Error ID: %d\n",dwRet);
			DsFreePasswordCredentials(hAuthIdentity); 
			hAuthIdentity = NULL;
			INM_DONE();
		}
	}
INM_EXITD()

	return dwRet;
}


DWORD DoDnsUpdatefromDS(_TCHAR* szDCName, _TCHAR* szDomainName,_TCHAR* szUserName,_TCHAR* szPassword,bool bDoUpdateAllDnsServers, _TCHAR* pListOfDCsForDnsUpdate, _TCHAR* pListOfZoneNames)
{
	DWORD dwRet = ERROR_SUCCESS;
	vector <const char*> vListOfDCs;
	vector <const char*> vListOfZoneNames;

	_TCHAR* token;
	_TCHAR* seps = ",;" ;
	bool bPrint = true;

	if(pListOfZoneNames)
	{
		token = _tcstok(pListOfZoneNames,seps);
			
		while( token != NULL )
		{
			if(IsDuplicateEntryPresent(vListOfZoneNames,token))
			{
				if(bPrint)
				{
					printf("\nWARNING:Duplicate ZoneName entry found in passed arguments:\n");
					bPrint = false;
				}
				printf("Duplicate entry found for %s zone, skipping the duplicate zone name.\n",token);
				token = _tcstok( NULL, seps );
			}
			else
			{
				vListOfZoneNames.push_back(token );
				token = _tcstok( NULL, seps );
			}
		}
	}
	if((!bDoUpdateAllDnsServers) && (pListOfDCsForDnsUpdate))
	{
		token = _tcstok(pListOfDCsForDnsUpdate,seps);

		bPrint = true;
		while( token != NULL )
		{
			if(IsDuplicateEntryPresent(vListOfDCs,token))
			{
				if(bPrint)
				{
					printf("\nWARNING:Duplicate DC name entry found in passed arguments:\n");
					bPrint = false;
				}
				printf("Duplicate entry found for %s DC, skipping the duplicate DC name.\n",token);
				token = _tcstok( NULL, seps );
			}
			else
			{
				vListOfDCs.push_back(token );
				token = _tcstok( NULL, seps );
			}
		}
	    dwRet = DoDnsUpdatefromDS(szDCName,szDomainName, szUserName, szPassword, bDoUpdateAllDnsServers, vListOfDCs,vListOfZoneNames);
	}
	else
	{
		dwRet = DoDnsUpdatefromDS(szDCName,szDomainName, szUserName, szPassword, bDoUpdateAllDnsServers, vListOfDCs,vListOfZoneNames);
	}

	return dwRet;
}

DWORD DoDnsUpdatefromDS(_TCHAR* szDCName, char* szDomainName,char* szUserName,char* szPassword,bool bDoUpdateAllDnsServers, vector<const char*> &vListOfDCsForDnsUpdate, vector<const char*> &vListOfZoneNames)
{
	HANDLE hDirectoryService = NULL;
	RPC_AUTH_IDENTITY_HANDLE hAuthIdentity = NULL;
	DWORD dwRet = ERROR_SUCCESS;

	//hDirectoryService and hAithIdentity are out parameter
	dwRet = BindToDirectoryService(hDirectoryService,hAuthIdentity,szDCName,szDomainName,szUserName,szPassword);
	if(dwRet != ERROR_SUCCESS)
	{
		INM_DONE();
	}
	//call overloaded DoDnsUpdateFromDS function
	dwRet = DoDnsUpdateFromDS(hDirectoryService,bDoUpdateAllDnsServers,vListOfDCsForDnsUpdate,vListOfZoneNames,szDomainName,szUserName,szPassword);
	if(dwRet != ERROR_SUCCESS)
	{
		INM_DONE();
	}
INM_EXITD()
	DsUnBind(&hDirectoryService);
	if(hAuthIdentity)
	{
		DsFreePasswordCredentials(hAuthIdentity); 
	}
	return dwRet;
}

//if bUpdateAllDNSServer is 1,vListOfDCForDnsUpdate is Ignored.
DWORD DoDnsUpdateFromDS(HANDLE hDirectoryService, bool bUpdateAllDNSServer, vector<const char*> &vListOfDCsForDnsUpdate,vector<const char*> &vListOfZoneNames,char* szDomainName,char* szUserName,char* szPwd)
{
	USES_CONVERSION;
	InmWMIHelper	wmiServer;
	wchar_t wzMachineName[MAX_PATH] ={0};
	DWORD dwRet = 0;
    DWORD dwIndex = 0;
	DWORD dwNumberOfDCsinTheList = 0;
	DWORD pInfoleveOut;
	
	PDOMAIN_CONTROLLER_INFO		pDomainInfo = NULL;
	DS_DOMAIN_CONTROLLER_INFO_2	*pDCInfo = NULL;
	
	vector<const char*> vUpdateSuceess;
	vector<const char*> vUpdateFailed;

	if(bUpdateAllDNSServer)
	{
		//Get DomainName
		dwRet  = DsGetDcName(0,0,0,0,DS_RETURN_DNS_NAME,&pDomainInfo);
		if(dwRet != ERROR_SUCCESS)
		{	
			printf("Failed to get list of Domain name.\n");
			printf("Error: %d",dwRet);
			INM_DONE();
		}
		
		//Get the list of Domain controllers
		dwRet = DsGetDomainControllerInfo(hDirectoryService,pDomainInfo->DomainName,2,&pInfoleveOut,(void**)&pDCInfo);
		if(dwRet != ERROR_SUCCESS)
		{	
			printf("Failed to get list of Domain controllers.\n");
			printf("Error: %d",dwRet);
			INM_DONE();
		}

		//Connect to each domain controller using WMI and execute UpdateDNSFromDS method
		//for MicrosotDNS_Zone class. this 
		for(dwIndex = 0; dwIndex < pInfoleveOut; dwIndex++)
		{
			memset(wzMachineName,0,sizeof(wzMachineName));
			mbstowcs(wzMachineName,pDCInfo[dwIndex].NetbiosName,strlen(pDCInfo[dwIndex].NetbiosName));
			printf("\n\nConnecting to DC %s.....\n",pDCInfo[dwIndex].NetbiosName);
		
			dwRet = wmiServer.ConnectWMI(wzMachineName,	szDomainName == NULL ? NULL : A2W(szDomainName),
				szUserName	 == NULL ? NULL : A2W(szUserName),szPwd	 == NULL ? NULL : A2W(szPwd));

			if(dwRet == ERROR_SUCCESS)
			{
				dwRet  = wmiServer.UpdateDnsFromDS(vListOfZoneNames);
				if(dwRet == ERROR_SUCCESS)
				{
					vUpdateSuceess.push_back(pDCInfo[dwIndex].NetbiosName);
				}
				else
				{
					vUpdateFailed.push_back(pDCInfo[dwIndex].NetbiosName);
				}
			}
			else
			{
				vUpdateFailed.push_back(pDCInfo[dwIndex].NetbiosName);
			}
		}
	}
	else
	{
		if(&vListOfDCsForDnsUpdate == NULL)
		{
			printf("Error:Invalid argument passed.\n");
			printf("List of Domain Controllers for DNS Zone update is NULL.\n");
		}
		else
		{
			dwNumberOfDCsinTheList = (DWORD)vListOfDCsForDnsUpdate.size();

			for(dwIndex = 0; dwIndex < dwNumberOfDCsinTheList; dwIndex++)
			{
				memset(wzMachineName,0,sizeof(wzMachineName));
				mbstowcs(wzMachineName,vListOfDCsForDnsUpdate[dwIndex],strlen(vListOfDCsForDnsUpdate[dwIndex]));
				printf("\n\nConnecting to DC %s......\n",vListOfDCsForDnsUpdate[dwIndex]);
			
				dwRet = wmiServer.ConnectWMI(wzMachineName,	szDomainName == NULL ? NULL : A2W(szDomainName), 
					szUserName	 == NULL ? NULL : A2W(szUserName),szPwd	 == NULL ? NULL : A2W(szPwd));

				if(dwRet == ERROR_SUCCESS)
				{
					dwRet = wmiServer.UpdateDnsFromDS(vListOfZoneNames);
					if(dwRet == ERROR_SUCCESS)
					{
						vUpdateSuceess.push_back(vListOfDCsForDnsUpdate[dwIndex]);
					}
					else
					{
						vUpdateFailed.push_back(vListOfDCsForDnsUpdate[dwIndex]);
					}
				}
				else
				{
					vUpdateFailed.push_back(vListOfDCsForDnsUpdate[dwIndex]);
				}
			}
		}
	}

	printf("\nSTATUS SUMMARY OF ZONE UPDATE\n");
	printf("==============================\n");
	DWORD dwUpdateSuccess = 0;
	DWORD dwUpdateFailed = 0;
	dwUpdateSuccess  = (DWORD)vUpdateSuceess.size();
	dwUpdateFailed =  (DWORD) vUpdateFailed.size();
	printf("Number of DCs are successfully updated = %d\n",dwUpdateSuccess );
	printf("Number of DCs are not successfully updated = %d\n",dwUpdateFailed );
	
	printf("\nFollowing DCs are successfully updated:\n");
	for(DWORD dwCount = 0; dwCount < dwUpdateSuccess; dwCount++)
	{
		printf("%s\n",vUpdateSuceess[dwCount]);
	}

	printf("\nFollowing DCs are partially or completely failed to update:\n");
	for(DWORD dwCount = 0; dwCount < dwUpdateFailed; dwCount++)
	{
		printf("%s\n",vUpdateFailed[dwCount]);
	}
	
INM_EXITD()	
	if(pDomainInfo)
	{
		NetApiBufferFree(pDomainInfo);
	}
	if (pDCInfo)
	{
		DsFreeDomainControllerInfo(2,pInfoleveOut,pDCInfo);
	}
	vUpdateSuceess.clear();
	vUpdateFailed.clear();
	return dwRet;
}


//if bUpdateAllDNSServer is 1,pListOfDCsForDnsUpdate is Ignored.
DWORD DoDnsUpdateFromDS(HANDLE hDirectoryService, bool bUpdateAllDNSServer, char* pListOfDCsForDnsUpdate,char* pListOfZoneNames,char* szDomainName,char* szUserName,char* szPassword)
{
	DWORD dwRet = 0;
    DWORD dwIndex = 0;
	vector <const char*> vListOfDCsForDnsUpdate;
	vector <const char*> vListOfZoneNames;
	bool bPrint = true;

	_TCHAR* token;
	_TCHAR* seps = ",;" ;


	if(pListOfZoneNames)
	{
		token = _tcstok(pListOfZoneNames,seps);
		while( token != NULL )
		{
			if(IsDuplicateEntryPresent(vListOfZoneNames,token))
			{
				if(bPrint)
				{
					printf("\nWARNING:Duplicate ZoneName entry found in passed arguments:\n");
					bPrint = false;
				}
				printf("Duplicate entry found for %s Zone, skipping the duplicate zone name.\n",token);
				token = _tcstok( NULL, seps );
			}
			else
			{
				vListOfZoneNames.push_back(token);
				token = _tcstok( NULL, seps );
			}
		}
	}
	
	if(pListOfDCsForDnsUpdate)
	{
			
		token = _tcstok(pListOfDCsForDnsUpdate,seps);
		bPrint = true;
		while( token != NULL )
		{
			if(IsDuplicateEntryPresent(vListOfDCsForDnsUpdate,token))
			{
				if(bPrint)
				{
					printf("\nWARNING:Duplicate DC name entry found in passed arguments:\n");
					bPrint = false;
				}
				printf("Duplicate entry found for %s DC, skipping duplicate DC name .\n",token);
				token = _tcstok( NULL, seps );
			}
			else
			{
				vListOfDCsForDnsUpdate.push_back(token);
				token = _tcstok( NULL, seps );
			}
		}
	}

	dwRet = DoDnsUpdateFromDS(hDirectoryService,bUpdateAllDNSServer,vListOfDCsForDnsUpdate,vListOfZoneNames,szDomainName,szUserName,szPassword);
	return dwRet;
}

DWORD DoReplicationAndDnsUpdatefromDS(char* szDCName, char* szListOfNamingContexts,char* szDomainName,char* szUserName,char* szPassword,bool bDoUpdateAllDnsServers, char* pListOfDCsForDnsUpdate,char* pListOfZoneNames)
{
	HANDLE hDirectoryService = NULL;
	PDS_REPSYNCALL_ERRINFO* pDsError;
	RPC_AUTH_IDENTITY_HANDLE hAuthIdentity = NULL;
	DWORD dwRet = 0;

	vector <const char*> vListOfNamingContexts;
	DWORD dwListOfNamingContexts = 0;

	_TCHAR* token;
	_TCHAR* seps = ";" ;
	if(!szListOfNamingContexts)
	{
		printf("ERROR: No naming contexts of directory partition is passed");
		dwRet = ERROR_INVALID_DATA;
		INM_DONE();
	}
	
	char *pListOfNamingContexts = NULL; 
    size_t contextslen;
    INM_SAFE_ARITHMETIC(contextslen = InmSafeInt<size_t>::Type(strlen(szListOfNamingContexts))+1, INMAGE_EX(strlen(szListOfNamingContexts)))
	pListOfNamingContexts = (char*)calloc(contextslen, sizeof(char));
	inm_strcpy_s(pListOfNamingContexts,(strlen(szListOfNamingContexts)+1),szListOfNamingContexts);

	token = _tcstok(pListOfNamingContexts,seps);
	bool bPrint = true;

	while( token != NULL )
	{
		if(IsDuplicateEntryPresent(vListOfNamingContexts,token))
		{
			if(bPrint)
			{
				printf("\nWARNING:Duplicate Naming Context entry found in passed arguments:\n");
				bPrint = false;
			}
			printf("Duplicate entry found for \"%s\" Naming Context, skipping the duplicate Naming Context.\n",token);
			token = _tcstok( NULL, seps );
		}
		else
		{
			vListOfNamingContexts.push_back(token );
			token = _tcstok( NULL, seps );
		}
	}

	dwRet = BindToDirectoryService(hDirectoryService,hAuthIdentity,szDCName,szDomainName,szUserName,szPassword);
	if(dwRet != ERROR_SUCCESS)
	{
		INM_DONE();
	}

	dwListOfNamingContexts = (DWORD) vListOfNamingContexts.size();

	for(DWORD dwIndex = 0; dwIndex < dwListOfNamingContexts; dwIndex++)
	{
		printf("\n\n[%d] Starting replication for \"%s\" Naming context\n",dwIndex+1,vListOfNamingContexts[dwIndex]);
		if(!stricmp(vListOfNamingContexts[dwIndex],"default"))
		{
			dwRet = DsReplicaSyncAll(hDirectoryService,NULL,DS_REPSYNCALL_PUSH_CHANGES_OUTWARD |DS_REPSYNCALL_CROSS_SITE_BOUNDARIES|
											DS_REPSYNCALL_ID_SERVERS_BY_DN,
											SyncUpdateProc,NULL,&pDsError);
		}
		else
		{
			dwRet = DsReplicaSyncAll(hDirectoryService,vListOfNamingContexts[dwIndex],DS_REPSYNCALL_PUSH_CHANGES_OUTWARD |
											DS_REPSYNCALL_CROSS_SITE_BOUNDARIES |	DS_REPSYNCALL_ID_SERVERS_BY_DN ,
											SyncUpdateProc,NULL,&pDsError);
		}

		if((dwRet== ERROR_SUCCESS) && !(pDsError))
		{
			printf("\nSucessfully replicated \"%s\" to all the servers\n",vListOfNamingContexts[dwIndex]);
		}
		else
		{
			
			printf("\nERROR: Fail to replicate \"%s\" to one or more servers.\n",vListOfNamingContexts[dwIndex]);
			LPVOID lpInmMessageBuffer;
			::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|	FORMAT_MESSAGE_FROM_SYSTEM,	NULL, dwRet,	0,
			(LPTSTR)&lpInmMessageBuffer, 0,	NULL);

			if(dwRet)
			{
				printf("Error:[%d] %s\n",dwRet,(LPCTSTR)lpInmMessageBuffer);
			}
		
			
			LocalFree(lpInmMessageBuffer);
		}

		if(pDsError)
		{
			LocalFree(pDsError);
			pDsError = NULL;
		}
	}	
	//DNS update is required as after replication is done DNS is not updated
	//By default after 5 minutes DNS update the recrod from Directory Service
	//Since we want to immediate update we need to use WMI to do that
	//UpdateFromDS method is aailable in MicrosoftDNS_Zone class.
	if(bDoUpdateAllDnsServers || pListOfDCsForDnsUpdate)
	{
		printf("\nStarting DNS update from Directory services\n");
		printf("--------------------------------------------\n");
		dwRet = DoDnsUpdateFromDS(hDirectoryService,bDoUpdateAllDnsServers,pListOfDCsForDnsUpdate,pListOfZoneNames,szDomainName,szUserName,szPassword);
	}

    
INM_EXITD();
	if(hAuthIdentity)
	{
		DsFreePasswordCredentials(hAuthIdentity); 
	}
	if(pListOfNamingContexts)
	{
		free(pListOfNamingContexts);
	}
	DsUnBind(&hDirectoryService);
	return dwRet;
		
}

bool IsDuplicateEntryPresent(vector<const char *>& cont,_TCHAR* token)
{
	bool ret = false;
	for(unsigned int i = 0; i < cont.size();i++)
	{
		if(!stricmp(cont[i],token))
		{
			ret = true;
			break;
		}
	}
	return ret;
}

