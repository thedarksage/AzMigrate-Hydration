#ifndef __SQL_DISCOVERY__H
#define __SQL_DISCOVERY__H

#include <list>
#include <map>
#include <string>
#include <boost/shared_ptr.hpp>
#include "appglobals.h"
#include "wmi/sqlwmi.h"
#include "mssqlmetadata.h"
#include "ado/sqlado.h"
#include <list>
#include "hostdiscovery.h"
#include <sstream>
#include "service.h"

class WMISQLInterface ;

struct MSSQLInstanceDiscoveryInfo
{
    InmProtectedAppType m_appType ;
    std::list<InmService> m_services ; //instance level services
    std::list<InmProcess> m_processes ; //instance level processes
    std::string m_version;
    std::string m_edition ;
    std::string m_instanceName ;
    std::string m_installPath ;
	std::string m_errorLogPath;
    std::string m_dataDir ;
    SV_UINT m_isClustered ;
    std::string m_virtualSrvrName ;
    std::string m_dumpDir ;
    std::map<std::string, std::string> m_properties ;
    std::string summary() ;
    std::string errStr;
    long errCode;
    MSSQLInstanceDiscoveryInfo()
    {
        errCode = 0 ;
    }
} ;

struct MSSQLVersionDiscoveryInfo
{
    InmProtectedAppType m_type ;
    std::list<MSSQLInstanceDiscoveryInfo> m_sqlInstanceDiscoveryInfo ;
    std::list<InmService> m_services ; //version level services
    std::list<InmProcess> m_processes ; //version level processes 
    std::string summary() ;
    std::string errStr;
    long errCode;
    MSSQLVersionDiscoveryInfo()
    {
        errCode = 0 ;
    } ;
} ;

struct MSSQLDiscoveryInfo
{
    std::list<MSSQLVersionDiscoveryInfo> m_sqlVersionDiscoveryInfo ;
    std::list<InmService> m_services ; //application level services
    std::list<InmProcess> m_processes ; //application level processes
    std::string summary() ;
    void clean()
    {
        m_sqlVersionDiscoveryInfo.clear();
        m_services.clear();
        m_processes.clear();
    }
    std::string errStr;
    long errCode;
    MSSQLDiscoveryInfo()
    {   
        errCode = 0 ;
    }
} ;

class MSSQLDiscoveryInfoImpl
{
	std::list<std::string> m_passiveIntanceNameList;
   // boost::shared_ptr<SQLAdo> m_SQLAdoImplPtr ;
    SVERROR discoverMSSQLApplevelsvcInfo(std::list<InmService>& svcList) ;
	SVERROR discoverMSSQLVersionLevelsvcInfo(InmProtectedAppType appType,std::list<InmService>& serviceList) ;
    SVERROR discoverMSSQLInstanceLevelsvcInfo(InmProtectedAppType appType, std::string InstanceName, std::list<InmService>& serviceList) ;
	SVERROR findPassiveInstanceNameList();
	SVERROR setInstancestatusInfo( std::map<std::string, MSSQLInstanceDiscoveryInfo>& instanceNameToInfoMap );
	SVERROR fillSqlVersionLevelInfo( std::map<std::string, MSSQLInstanceDiscoveryInfo>& instanceNameToInfoMap, MSSQLVersionDiscoveryInfo& sqlVersionDiscoveryInfo );
	std::string m_strActiveNodeName;
public:
	SVERROR getActiveNodeName(std::string &);
    static void prepareSupportedSQLVersionsList(std::list<InmProtectedAppType>& list) ;
    SVERROR discoverMSSQLInstanceInfo(MSSQLInstanceDiscoveryInfo& instanceDiscoveryInfo, MSSQLInstanceMetaData& metaData) ;
	SVERROR discoverMSSQLAppDiscoveryInfo(MSSQLDiscoveryInfo&, const std::list<InmProtectedAppType>& m_supportedSqlVersionsList) ;
    static boost::shared_ptr<MSSQLDiscoveryInfoImpl> m_instancePtr ;
    static boost::shared_ptr<MSSQLDiscoveryInfoImpl> getInstance() ;
	MSSQLDBType getSQLDataBaseTypeFromString(std::string);
	SVERROR getInstancenameListFromMap( std::map<std::string, MSSQLInstanceDiscoveryInfo>&, std::list<std::string>& );
  //bool isSQL2000ServicesRunning();
  SVERROR getSQLInstancesFromRegistry(std::map<std::string, MSSQLInstanceDiscoveryInfo> &instanceNameToInfoMap, InmProtectedAppType);
	bool queryRegistryValue(HKEY &hKey,std::string ValueName,DWORD &dwType,DWORD &dwSize,char** PerfData);
	SVERROR getSQLInstInfoFromRegistry(std::map<std::string, MSSQLInstanceDiscoveryInfo> &instanceNameToInfoMap,const std::string& host,REGSAM RegKeyAccess, InmProtectedAppType);		
	SVERROR enumSqlInstances(std::vector<std::string> &SQLInstances, REGSAM RegKeyAccess);
};
#endif


