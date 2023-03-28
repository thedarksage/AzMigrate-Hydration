#ifndef __METADATA__H
#define __METADATA__H

#include <boost/shared_ptr.hpp>
#include <map>
#ifndef _WINDOWS_
#include <Windows.h>
#endif
#include <string>
#include <list>
#include <string>
#include "ruleengine/validator.h"
#include "appglobals.h"
#include <sstream>
#include <map>

enum MSSQLDBType
{
	MSSQL_DBTYPE_NONE,
	MSSQL_DBTYPE_SYSTEM,
	MSSQL_DBTYPE_MASTER,
	MSSQL_DBTYPE_MSDB,
	MSSQL_DBTYPE_TEMP,
	MSSQL_DBTYPE_MODEL,
	MSSQL_DBTYPE_USER
};
struct MSSQLDBMetaData
{
    std::string m_dbName ;
    std::list<std::string> m_dbFiles ;
    std::list<std::string> m_volumes ;
    bool m_dbOnline ;
	MSSQLDBType m_dbType;
    std::map<std::string, std::string> m_dbProeprtiesMap ; //do not consider this now.
    std::string summary() ;
    std::string errorString;
    long errCode;
} ;

struct MSSQLInstanceMetaData
{
	std::string m_version ;
	std::string m_installPath ;
    std::string m_instanceName ;
	std::list<std::string> m_volumesList ;
    std::map<std::string, MSSQLDBMetaData> m_dbsMap ;
    std::string summary(std::list<ValidatorPtr>& list) ;
    std::string errorString;
    long errCode;
} ;

struct MSSQLMetaData
{
    std::map<std::string, MSSQLInstanceMetaData> m_serverInstanceMap ;
    void clean()
    {
        m_serverInstanceMap.clear();
    }
} ;


typedef boost::shared_ptr<MSSQLInstanceMetaData> MSSQLInstanceMetaDataPtr ;
typedef boost::shared_ptr<MSSQLMetaData> MSSQLMetaDataPtr ;
#endif
