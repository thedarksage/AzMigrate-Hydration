#ifndef MSSQL_APPLICATION_H
#define MSSQL_APPLICATION_H
#include "app/application.h"
#include "mssqlmetadata.h"
#include "sqldiscovery.h"

class MSSQLApplication : public Application
{
    std::list<InmProtectedAppType> m_supportedSqlVersionsList  ;
public:
    virtual bool isInstalled() ;
    virtual SVERROR discoverApplication() ;
	virtual SVERROR discoverMetaData() ;
	void dumpMSSQLAppDiscoverdInfo(MSSQLDiscoveryInfo&);
	std::string getSQLDataBaseNameFromType(MSSQLDBType);
    void clean();
    MSSQLMetaData m_sqlmetaData ;
    MSSQLDiscoveryInfo m_mssqlDiscoveryInfo ;
    MSSQLDiscoveryInfo discoveryInfo()
    {
        return m_mssqlDiscoveryInfo ;
    }
	SVERROR getInstanceNameList(std::list<std::string>&);
    SVERROR getActiveInstanceNameList(std::list<std::string>& instanceNameList) ;
	SVERROR getDiscoveredDataByInstance(const std::string&, MSSQLInstanceDiscoveryInfo&);
	SVERROR getDiscoveredMetaDataByInstance(const std::string&, MSSQLInstanceMetaData&);
    SVERROR getInstanceNamesByVersion(std::map<std::string, std::list<std::string> >&) ;
    MSSQLApplication()
    {
        
    }
    MSSQLApplication(std::list<InmProtectedAppType> versions)
    {
        m_supportedSqlVersionsList = versions ;
    }
    MSSQLApplication(InmProtectedAppType version)
    {
        m_supportedSqlVersionsList.push_back(version) ;
    }
} ;
typedef boost::shared_ptr<MSSQLApplication> MSSQLApplicationPtr ;

#endif