#ifndef __EXCHANGE_LDAP_HELPER_H
#define __EXCHANGE_LDAP_HELPER_H
#include <boost/shared_ptr.hpp>
#include <map>
#include <list>

#include "appglobals.h"
#include "ldap.h"
#include "exchange/exchangemetadata.h"

class ExchangeLdapHelper
{
    std::string m_configDn ;
    std::string m_dnsname ;
    boost::shared_ptr<ldapConnection> m_ldapConnection ;
    //SVERROR getNamingContexts(std::list<std::string>& namingContexts) ;
    SVERROR bindToDn() ;
    //SVERROR getConfigurationDn() ;
    void prepareExchangeServerAttrList(std::list<std::string>& attrList) ;
    void prepareStorageGrpAttrList(std::list<std::string>& attrList) ;
    void prepareMdbattrList(std::list<std::string>& attrList) ;
public:
    SVERROR findExchangeServerCN(const std::string& hostname, std::list<std::string> & exchangeServerCnList) ;
    SVERROR getExchangeServerAttributes(const std::string& hostname, std::multimap<std::string, std::string>& attrList) ;
    SVERROR getStorageGroupAttributes(const std::string& hostname, const std::string& storageGrpName, std::multimap<std::string, std::string>& attrlist) ;
    SVERROR getMDBAttributes(const std::string& hostname, const std::string storageGrpName, ExchangeMdbType mdbType, const std::string mdbName, std::multimap<std::string, std::string>& attrlist) ;
	SVERROR get2010MDBAttributes(const std::string& hostname, ExchangeMdbType mdbType, const std::string mdbName, std::multimap<std::string, std::string>& attrMap);
    bool isExchangeInstalled(const std::string& hostName) ;
    bool isExchangeInstalled(std::list<std::string>& exchHosts) ;
    SVERROR getStorageGroups(const std::string& hostname, std::list<std::string>& storageGroupList) ;
    SVERROR getMDBs(const std::string& hostname, const std::string& storageGroupName, ExchangeMdbType mdbType, std::list<std::string>& Mdbs) ;
    SVERROR get2010MDBs(const std::string& hostname, std::list<std::string>& Mdbs) ;    
    SVERROR init(const std::string& dnsname) ;
	SVERROR get2010MDBCopyHosts( std::string& mdbName, std::list<std::string>& copyHostNameList );
	SVERROR getDAGParticipantsSereversList( std::string& DAGName, std::list<std::string>& participantsServersList );
	SVERROR getServerMaxMailstores(const std::string & serverDN, std::string & nMaxMailstores);
	std::string getEdition(const std::string& maxMailstores, InmProtectedAppType appType);
} ;
#endif