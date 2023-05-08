#ifndef _NETBIOS_H
#define _NETBIOS_H

#define OPTION_ADD			"add"
#define OPTION_DELETE		"delete"
#define OPTION_CHANGE		"changeto"
#define OPTION_REMOTE		"remote"
#define OPTION_DOMAIN		"domain"
#define OPTION_USER			"user"
#define OPTION_PASSWORD		"password"

enum emNetBiosOperation
{
	OPERATION_ADD = 0,
	OPERATION_DELETE,
	OPERATION_CHANGE
};

int NetBiosMain(int argc, char* argv[]);
DWORD AddComputerName(wchar_t* wszRemoteMachineName, wchar_t* wszComputerName,char* szDomainName, char* szUserName, char* password);
DWORD DeleteComputerName(wchar_t* wszRemoteMachineName, wchar_t* wszComputerName, char* szDomainName, char* szUserName, char* password);
DWORD ChangeComputerName(wchar_t* wszRemoteMachineName, wchar_t* wszComputerName,char* szDomainName, char* szUserName, char* password);
void netbios_usage();

#endif //_NETBIOS_H