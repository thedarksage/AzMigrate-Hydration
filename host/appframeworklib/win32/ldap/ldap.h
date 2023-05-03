#ifndef __LDAP__H
#define __LDAP__H
#ifndef _WINDOWS_
#include <Windows.h>
#endif
#include <Winldap.h>
#include <string>
#include <list>
#include <map>
#include "appglobals.h"

#define MAX_LDAP_SEARCH_ATTEMPTS 10
#define APPAGENT_LDAP_SEARCH_TIMEOUT 120
#define EXCHANGE_SERVICE_PRINCIPAL_NAME "servicePrincipalName" 
class ldapConnection
{
    LDAP* m_ldap ;
    std::string m_dnsname ;
	std::string m_configDN;
	bool m_bBinded;
    SVERROR InitSession() ;
    SVERROR InitSSLSession() ;
    SVERROR UnBind() ;
    SVERROR GetOption(int option, void* buffer) ;    
	LDAP* getLdapConnectionCustom(ULONG ulSearchSizeLimit, LDAP_TIMEVAL ltConnTimeout, bool bBind=true);
	bool retryLdapConnection();
public:
    ~ldapConnection() ;
    SVERROR SetOption(int option, void* buffer) ;
    ldapConnection(const std::string&) ;
    SVERROR init() ;
    SVERROR Bind(const std::string&) ;
    SVERROR ConnectToServer() ;
    SVERROR GetNamingContexts(std::list<std::string>& namingContexts) ;
    SVERROR Modify(ULONG operation, const std::string& userDn, const std::string& attrname, const std::list<std::string>& modValsList) ;
    SVERROR Search(const std::string& base, ULONG scope, const std::string& filter, const std::list<std::string>& attrs, std::list<std::multimap<std::string, std::string> >& resultSet) ;
	SVERROR getNamingContexts(std::list<std::string>& namingContexts) ;
	SVERROR getConfigurationDn(std::string&) ;
	SVERROR checkADPrivileges(const std::string& szSourceHost, std::string& statusString );
} ;
#endif