#ifndef __SQL__WMI__H
#define __SQL__WMI__H
#include "appglobals.h"
#include "mssql/sqldiscovery.h"
#include "wmi.h"
struct MSSQLInstanceDiscoveryInfo ;
class WMISQLInterface
{
    bool m_init ;
public:
    boost::shared_ptr<WMIConnector> m_wmiConnectorPtr ;
    WMISQLInterface() 
    {
        m_init = false ;
    }
    virtual SVERROR init() ;
    static boost::shared_ptr<WMISQLInterface>getWMISQLInterface(InmProtectedAppType) ;
    virtual SVERROR getSQLInstanceInfo( std::map<std::string, MSSQLInstanceDiscoveryInfo>&, std::list<std::string>& ) = 0;
} ;

class WMISQL2000Impl : public WMISQLInterface
{
	SVERROR getSQLInstanceEdition(std::string versionStringValForEdition, std::string& edition) ;
	SVERROR getSQLRegistrySettings( const std::string& instanceName, MSSQLInstanceDiscoveryInfo& instanceDiscoverInfoMap );
public:
    SVERROR init() ;
    SVERROR getSQLInstanceInfo( std::map<std::string, MSSQLInstanceDiscoveryInfo>&, std::list<std::string>& );
} ;


class WMISQLImpl : public WMISQLInterface
{
    InmProtectedAppType m_appType ;
public:
    WMISQLImpl(const InmProtectedAppType&) ;
    SVERROR init() ;
    SVERROR getSQLInstanceInfo( std::map<std::string, MSSQLInstanceDiscoveryInfo>&, std::list<std::string>& );
} ;

#endif
