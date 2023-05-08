#ifndef _SPN_H
#define _SPN_H

#define OPTION_ADD			"add"
#define OPTION_DELETE		"delete"
#define OPTION_DELETEHOST	"deletehost"
#define OPTION_ADDHOST		"addhost"

enum emSpnOperation	{
	OPERATION_ADDSPN = 0,
	OPERATION_DELETESPN,
	OPERATION_ADDHOST,
	OPERATION_DELETEHOST
};
const std::string whiteSpaces( " \f\n\r\t\v" ); 

// ServicePrincipalName attributes required for authenticating an outlook client 2003 and above with an exchange server
#define EXCHANGE_SERVICE_PRINCIPAL_NAME		"servicePrincipalName"
#define EXCHANGE_HTTP						"HTTP"

void  trimRight( std::string& str, const std::string& trimChars = whiteSpaces );
void  trimLeft( std::string& str, const std::string& trimChars = whiteSpaces );
void  trim( std::string& str, const std::string& trimChars = whiteSpaces );

ULONG AddSpnEntries(std::map< std::string, std::list<std::string> > accServermapping, std::string domainName, std::string domainDN, const std::string& serverDomainName);
ULONG DeleteSpnEntries(std::map< std::string, std::list<std::string> > accServermapping, std::string domainName, std::string domainDN, const std::string& serverDomainName);
ULONG modifyEntriesInComputerAccount(bool operation, const std::string compAccount, const std::string serverNameParam,
									 LDAP* pLdapConnection, LDAPMessage* pSpnlist, std::string domainName, const std::string& serverDomainName);
ULONG AddHostSpn(std::map< std::string, std::list<std::string> > accServermapping, std::string domainName, std::string domainDN, const std::string& serverDomainName, ULONG& nEntries);
ULONG DeleteHostSpn(std::map< std::string, std::list<std::string> > accServermapping, std::string domainName, std::string domainDN, const std::string& serverDomainName,ULONG& nEntries);
ULONG modifyHostEntryInComputerAccount(bool operation, const std::string compAccount, const std::string serverNameParam,
									 LDAP* pLdapConnection, LDAPMessage* pSpnlist, std::string domainName, const std::string& serverDomainName);
int SpnMain(int argc, char* argv[]);
void spn_usage();

#endif //_MODIFYSPN_H