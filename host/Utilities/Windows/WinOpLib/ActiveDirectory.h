#ifndef _ACTIVEDIRECTORY_H
#define _ACTIVEDIRECTORY_H
#include <rpc.h>
#include <ntdsapi.h>


#define OPTION_REPLICATE				"replicate"
#define OPTION_DOMAIN_CONTROLER_NAME	"dc"
#define OPTION_DOMAIN					"domain"
#define OPTION_USER						"user"
#define OPTION_PASSWORD					"password"
#define OPTION_UPDATE_DNS_SERVER		"UpdateDNSServer"
#define OPTION_UPDATE_ALL_DNS_SERVERS	"UpdateAllDNSServers"
#define OPTION_ZONE_UPDATE_FROM_DS		"ZoneUpdateFromDS"


enum emActiveDirectoryOperation
{
	OPERATION_REPLICATE = 0,
	OPERATION_DNS_UPDATE_FROM_DS,
	OPERATION_REPLICATE_AND_DNS_UPDATE
};

int ActiveDirectoryMain(int argc, char* argv[]);


//Do only AD replication
DWORD DoADReplication(char* szDCName, char* pListOfNamingContexts,char* szDomainName,char* szUserName,char* szPassword);

//Do only DNS Zone update from DS
DWORD DoDnsUpdatefromDS(char* szDCName, char* szDomainName,char* szUserName,char* szPassword,bool bDoUpdateAllDnsServers, char* pListOfDCsForDnsUpdate,char* pListOfZoneNames);
DWORD DoDnsUpdatefromDS(char* szDCName, char* szDomainName,char* szUserName,char* szPassword,bool bDoUpdateAllDnsServers, std::vector<const char*> &vListOfDCForDnsUpdate,std::vector<const char*> &vListOfZoneNames);
DWORD DoDnsUpdateFromDS(HANDLE hDirectoryService, bool bUpdateAllDNSServer, std::vector<const char*>& vListOfDCForDnsUpdate,std::vector<const char*>& vListOfZoneNames,char* szDomainName=NULL,char* szUserName=NULL,char* szPassword=NULL);
DWORD DoDnsUpdateFromDS(HANDLE hDirectoryService, bool bUpdateAllDNSServer, char* pListOfDCsForDnsUpdate,char* pListOfZoneNames,char* szDomainName=NULL,char* szUserName=NULL,char* szPassword=NULL);

//Do both AD replication and DNS Zone update
DWORD DoReplicationAndDnsUpdatefromDS(char* szDCName, char* pListOfNamingContexts,char* szDomainName,char* szUserName,char* szPassword,bool bDoUpdateAllDnsServers, char* pListOfDCsForDnsUpdate, char* pListOfZoneNames);

DWORD BindToDirectoryService(HANDLE &hDirectoryService,RPC_AUTH_IDENTITY_HANDLE &hAuthIdentity, char* szDCName,char* szDomainName,char* szUserName,char* szPassword);

BOOL WINAPI SyncUpdateProc( LPVOID pData, PDS_REPSYNCALL_UPDATE pUpdate);
bool IsDuplicateEntryPresent(std::vector<const char *>& cont,_TCHAR* token);

void ActiveDirectory_usage();

#endif //_ACTIVEDIRECTORY_H