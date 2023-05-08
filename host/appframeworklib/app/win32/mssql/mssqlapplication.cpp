#include "mssqlapplication.h"
#include "ruleengine.h"
#include <iostream>
#include "mssqlexception.h"
#include "system.h"
bool MSSQLApplication::isInstalled()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    MSSQLDiscoveryInfoImpl mssqlDiscInfoimpl;
	boost::shared_ptr<WMISQLInterface> wmiinterfacePtr ;
    if( m_supportedSqlVersionsList.size() == 0 )
    {
        mssqlDiscInfoimpl.prepareSupportedSQLVersionsList(m_supportedSqlVersionsList) ;
    }
	std::list<InmProtectedAppType>::const_iterator supportedSqlVersionIter = m_supportedSqlVersionsList.begin() ;
    while( supportedSqlVersionIter != m_supportedSqlVersionsList.end() )
    {
		try
		{
			wmiinterfacePtr = WMISQLInterface::getWMISQLInterface(*supportedSqlVersionIter) ;
			if( wmiinterfacePtr->init() == SVS_OK )
			{
				DebugPrintf(SV_LOG_DEBUG, "SQL is installed on this host\n" ) ;
				DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
				return true ;
			}
			else
			{
			    DebugPrintf(SV_LOG_INFO, "Failed to initialize the WMI Interface for %d\n", *supportedSqlVersionIter) ;
				
			}
		}
		catch(...)
		{
			DebugPrintf(SV_LOG_ERROR, "Got Exception while initializing WMI Interface\n") ;
			throw "Got Exception while initializing WMI Interface";
		}
		supportedSqlVersionIter++;
	}
    DebugPrintf(SV_LOG_INFO, "SQL is not installed on this host\n" ) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return false;
}

SVERROR MSSQLApplication::discoverApplication()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    try
    {
        boost::shared_ptr<MSSQLDiscoveryInfoImpl> sqlDiscoveryInfoPtr = MSSQLDiscoveryInfoImpl::getInstance() ;
        if( sqlDiscoveryInfoPtr->discoverMSSQLAppDiscoveryInfo(m_mssqlDiscoveryInfo, m_supportedSqlVersionsList) == SVS_OK )
        {
            DebugPrintf(SV_LOG_DEBUG, "Successfully discovered the mssql application\n") ;
            bRet = SVS_OK ;
		}
        else
        {
            DebugPrintf(SV_LOG_WARNING, "Failed to discover the mssql application\n") ;
			bRet = SVS_FALSE;
			m_mssqlDiscoveryInfo.errCode = DISCOVERY_FAIL;
        }
		dumpMSSQLAppDiscoverdInfo(m_mssqlDiscoveryInfo);
    }
    catch(MssqlException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Exception while discovering the mssql application\n") ;
        DebugPrintf(SV_LOG_ERROR, "Exception Details : \n", ex.to_string().c_str()) ;
		m_mssqlDiscoveryInfo.errCode = DISCOVERY_FAIL;
		m_mssqlDiscoveryInfo.errStr = "Exception while discovering the mssql application";
		bRet = SVS_FALSE;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
SVERROR MSSQLApplication::discoverMetaData()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;

    try
    {
        boost::shared_ptr<MSSQLDiscoveryInfoImpl> sqlDiscoveryInfoPtr = MSSQLDiscoveryInfoImpl::getInstance() ;
        std::list<MSSQLVersionDiscoveryInfo>::iterator& mssqlVersionDiscoveryInfoIter = m_mssqlDiscoveryInfo.m_sqlVersionDiscoveryInfo.begin() ;
        while( mssqlVersionDiscoveryInfoIter != m_mssqlDiscoveryInfo.m_sqlVersionDiscoveryInfo.end() )
        {
            std::list<MSSQLInstanceDiscoveryInfo>::iterator mssqlInstanceDiscoveryInfoIter = mssqlVersionDiscoveryInfoIter->m_sqlInstanceDiscoveryInfo.begin() ;
            while( mssqlInstanceDiscoveryInfoIter != mssqlVersionDiscoveryInfoIter->m_sqlInstanceDiscoveryInfo.end() )
            {
                MSSQLInstanceMetaData metadata ;
                if(mssqlInstanceDiscoveryInfoIter->errCode == PASSIVE_INSTANCE)
                {
                    MSSQLInstanceMetaData metadata ;
                    metadata.errCode = PASSIVE_INSTANCE ;
                    metadata.m_instanceName = mssqlInstanceDiscoveryInfoIter->m_instanceName ;
                    metadata.errorString = "Passive Instance" ;
                    m_sqlmetaData.m_serverInstanceMap.insert( std::make_pair( mssqlInstanceDiscoveryInfoIter->m_instanceName, metadata)) ;
                }
                else if (mssqlInstanceDiscoveryInfoIter->errCode == DISCOVERY_FAIL)
                {
                    MSSQLInstanceMetaData metadata ;
                    metadata.errCode = DISCOVERY_FAIL ;
                    metadata.m_instanceName = mssqlInstanceDiscoveryInfoIter->m_instanceName ;
                    metadata.errorString = "Application related services not running " ;
                    m_sqlmetaData.m_serverInstanceMap.insert( std::make_pair( mssqlInstanceDiscoveryInfoIter->m_instanceName, metadata)) ;
                }
                else
                {
                    sqlDiscoveryInfoPtr->discoverMSSQLInstanceInfo(*mssqlInstanceDiscoveryInfoIter, metadata) ;
                    m_sqlmetaData.m_serverInstanceMap.insert( std::make_pair( mssqlInstanceDiscoveryInfoIter->m_instanceName, metadata)) ;
                }
                mssqlInstanceDiscoveryInfoIter++ ;
				bRet = SVS_OK ;
            }
            mssqlVersionDiscoveryInfoIter ++ ;
        }
    }
    catch(MssqlException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Exception while discovering the msq sql application\n") ;
        DebugPrintf(SV_LOG_ERROR, "Exception Details : \n", ex.to_string().c_str()) ;
		bRet = SVS_FALSE ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

std::string MSSQLApplication::getSQLDataBaseNameFromType(MSSQLDBType type )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::string dbName;
	if( type == MSSQL_DBTYPE_SYSTEM )
	{
		dbName = "SYSTEM";
	}
	else if( type == MSSQL_DBTYPE_MASTER )
	{
		dbName = "MASTER";
	}
	else if( type == MSSQL_DBTYPE_MODEL )
	{
		dbName = "MODEL";
	}
	else if( type == MSSQL_DBTYPE_MSDB )
	{
		dbName = "MSDB";
	}
	else if( type == MSSQL_DBTYPE_TEMP )
	{
		dbName = "TEMP";
	}
	else
	{
		dbName = "USER";
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return dbName;
}

SVERROR MSSQLApplication::getInstanceNamesByVersion(std::map<std::string, std::list<std::string> >& instancesMap)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retStatus = SVS_OK;
	std::list<MSSQLVersionDiscoveryInfo>::iterator MSSQLVersionDiscoveryInfoListIter = m_mssqlDiscoveryInfo.m_sqlVersionDiscoveryInfo.begin();
    std::list<std::string> sql2000Instances, sql2005Instances, sql2008Instances, sql2012Instances ;

	while(MSSQLVersionDiscoveryInfoListIter != m_mssqlDiscoveryInfo.m_sqlVersionDiscoveryInfo.end() )
	{
		std::list<MSSQLInstanceDiscoveryInfo>::iterator MSSQLInstanceDiscoveryInfoIter = MSSQLVersionDiscoveryInfoListIter->m_sqlInstanceDiscoveryInfo.begin();
        while(MSSQLInstanceDiscoveryInfoIter != MSSQLVersionDiscoveryInfoListIter->m_sqlInstanceDiscoveryInfo.end() )
		{
            switch( MSSQLInstanceDiscoveryInfoIter->m_appType )
            {
            case INM_APP_MSSQL2000 :
                sql2000Instances.push_back(MSSQLInstanceDiscoveryInfoIter->m_instanceName) ;
                break ;
            case INM_APP_MSSQL2005 :
                sql2005Instances.push_back(MSSQLInstanceDiscoveryInfoIter->m_instanceName) ;                
                break ;
            case INM_APP_MSSQL2008 :
                sql2008Instances.push_back(MSSQLInstanceDiscoveryInfoIter->m_instanceName) ;                
                break ;
			case INM_APP_MSSQL2012 :
				sql2012Instances.push_back(MSSQLInstanceDiscoveryInfoIter->m_instanceName) ;                
				break ;
            default :
                DebugPrintf(SV_LOG_ERROR, "The version of the sql instance is not proper\n") ;
            }
			MSSQLInstanceDiscoveryInfoIter++;
		}
		MSSQLVersionDiscoveryInfoListIter++;
	}
    if( sql2000Instances.size() )
    {
        instancesMap.insert(std::make_pair("SQL", sql2000Instances)) ;
    }
    if( sql2005Instances.size() )
    {
        instancesMap.insert(std::make_pair("SQL2005", sql2005Instances)) ;
    }
    if( sql2008Instances.size() )
    {
        instancesMap.insert(std::make_pair("SQL2008", sql2008Instances)) ;
    }
	if( sql2012Instances.size() )
    {
        instancesMap.insert(std::make_pair("SQL2012", sql2012Instances)) ;
    }
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus;
}
SVERROR MSSQLApplication::getInstanceNameList(std::list<std::string>& instanceNameList)
{
	SVERROR retStatus = SVS_OK;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::list<MSSQLVersionDiscoveryInfo>::iterator MSSQLVersionDiscoveryInfoListIter = m_mssqlDiscoveryInfo.m_sqlVersionDiscoveryInfo.begin();
	while(MSSQLVersionDiscoveryInfoListIter != m_mssqlDiscoveryInfo.m_sqlVersionDiscoveryInfo.end() )
	{
		std::list<MSSQLInstanceDiscoveryInfo>::iterator MSSQLInstanceDiscoveryInfoIter = MSSQLVersionDiscoveryInfoListIter->m_sqlInstanceDiscoveryInfo.begin();
		while(MSSQLInstanceDiscoveryInfoIter != MSSQLVersionDiscoveryInfoListIter->m_sqlInstanceDiscoveryInfo.end() )
		{
            DebugPrintf(SV_LOG_DEBUG, "Instance on this machine is %s\n", MSSQLInstanceDiscoveryInfoIter->m_instanceName.c_str()) ;
			instanceNameList.push_back(MSSQLInstanceDiscoveryInfoIter->m_instanceName);
			MSSQLInstanceDiscoveryInfoIter++;
		}
		MSSQLVersionDiscoveryInfoListIter++;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus;
}

SVERROR MSSQLApplication::getActiveInstanceNameList(std::list<std::string>& instanceNameList)
{
	SVERROR retStatus = SVS_OK;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::list<MSSQLVersionDiscoveryInfo>::iterator MSSQLVersionDiscoveryInfoListIter = m_mssqlDiscoveryInfo.m_sqlVersionDiscoveryInfo.begin();
	while(MSSQLVersionDiscoveryInfoListIter != m_mssqlDiscoveryInfo.m_sqlVersionDiscoveryInfo.end() )
	{
		std::list<MSSQLInstanceDiscoveryInfo>::iterator MSSQLInstanceDiscoveryInfoIter = MSSQLVersionDiscoveryInfoListIter->m_sqlInstanceDiscoveryInfo.begin();
		while(MSSQLInstanceDiscoveryInfoIter != MSSQLVersionDiscoveryInfoListIter->m_sqlInstanceDiscoveryInfo.end() )
		{
            if( MSSQLInstanceDiscoveryInfoIter->errCode == DISCOVERY_SUCCESS )
            {
                DebugPrintf(SV_LOG_DEBUG, "Active Instance on this machine is %s\n", MSSQLInstanceDiscoveryInfoIter->m_instanceName.c_str()) ;
			    instanceNameList.push_back(MSSQLInstanceDiscoveryInfoIter->m_instanceName);
            }
            MSSQLInstanceDiscoveryInfoIter++;

		}
		MSSQLVersionDiscoveryInfoListIter++;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus;
}

SVERROR MSSQLApplication::getDiscoveredDataByInstance(const std::string& instanceName, MSSQLInstanceDiscoveryInfo& instanceDiscInfo)
{
	SVERROR retStatus = SVS_OK;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bshouldExit = false;
	std::list<MSSQLVersionDiscoveryInfo>::iterator MSSQLVersionDiscoveryInfoListInter;
	MSSQLVersionDiscoveryInfoListInter = m_mssqlDiscoveryInfo.m_sqlVersionDiscoveryInfo.begin();
	while(MSSQLVersionDiscoveryInfoListInter != m_mssqlDiscoveryInfo.m_sqlVersionDiscoveryInfo.end())
	{
		std::list<MSSQLInstanceDiscoveryInfo>::iterator MSSQLInstanceDiscoveryInfoIter;
		MSSQLInstanceDiscoveryInfoIter = MSSQLVersionDiscoveryInfoListInter->m_sqlInstanceDiscoveryInfo.begin();
		while(MSSQLInstanceDiscoveryInfoIter != MSSQLVersionDiscoveryInfoListInter->m_sqlInstanceDiscoveryInfo.end())
		{
			if( strcmpi(instanceName.c_str(), MSSQLInstanceDiscoveryInfoIter->m_instanceName.c_str()) == 0)
			{
				instanceDiscInfo = *MSSQLInstanceDiscoveryInfoIter;
				bshouldExit = true;
				break;
			}
			MSSQLInstanceDiscoveryInfoIter++;
			if(bshouldExit)
			{
				break;
			}
		}
		MSSQLVersionDiscoveryInfoListInter++;
	}
	if(!bshouldExit)
	{
		retStatus = SVS_FALSE;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus;
}

SVERROR MSSQLApplication::getDiscoveredMetaDataByInstance(const std::string& instanceName, MSSQLInstanceMetaData& instanceMetaData)
{
	SVERROR retStatus = SVS_OK;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::map<std::string, MSSQLInstanceMetaData>::iterator instanceMetaDataInfoIter;
	instanceMetaDataInfoIter = m_sqlmetaData.m_serverInstanceMap.find(instanceName);
	if(instanceMetaDataInfoIter != m_sqlmetaData.m_serverInstanceMap.end())
	{
		instanceMetaData = instanceMetaDataInfoIter->second;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus;
}

void MSSQLApplication::clean()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    m_sqlmetaData.clean();
    m_mssqlDiscoveryInfo.clean();
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void MSSQLApplication::dumpMSSQLAppDiscoverdInfo(MSSQLDiscoveryInfo& mssql_discoveryinfo)
{	
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n\n", FUNCTION_NAME) ;
	
	DebugPrintf(SV_LOG_DEBUG,"#########################################\n");
	DebugPrintf(SV_LOG_DEBUG,"  MS-SQL APPLICATION LEVEL DICOVERY INFO\n");
	DebugPrintf(SV_LOG_DEBUG,"#########################################\n\n");

	DebugPrintf(SV_LOG_DEBUG,"  ---------------------------------------\n");
	DebugPrintf(SV_LOG_DEBUG,"   MS-SQL APPLICATION LEVEL Services INFO\n");
	DebugPrintf(SV_LOG_DEBUG,"  ---------------------------------------\n");

	std::list<InmService>::iterator appLevelServiceIter = mssql_discoveryinfo.m_services.begin();
	while(appLevelServiceIter != mssql_discoveryinfo.m_services.end() )
	{
		DebugPrintf(SV_LOG_DEBUG, "\tServiceName: %s\n", appLevelServiceIter->m_serviceName.c_str());
		DebugPrintf(SV_LOG_DEBUG, "\tService Status: %d\n", appLevelServiceIter->m_svcStatus);
		appLevelServiceIter++;
	}
	DebugPrintf(SV_LOG_DEBUG,"\n");
	DebugPrintf(SV_LOG_DEBUG,"  -------------------------------------------------\n");
	DebugPrintf(SV_LOG_DEBUG,"   MS-SQL APPLICATION LEVEL VERSION DISCOVERY INFO\n");
	DebugPrintf(SV_LOG_DEBUG,"  -------------------------------------------------\n");
	std::list<MSSQLVersionDiscoveryInfo>::iterator appLevelVersionDiscoveryInfoiter = mssql_discoveryinfo.m_sqlVersionDiscoveryInfo.begin();
	while(appLevelVersionDiscoveryInfoiter != mssql_discoveryinfo.m_sqlVersionDiscoveryInfo.end() )
	{
		DebugPrintf(SV_LOG_DEBUG,"\n");
		DebugPrintf(SV_LOG_DEBUG,"\t---------------------\n");
		DebugPrintf(SV_LOG_DEBUG,"\t MS-SQL APPLICATIION %d\n", appLevelVersionDiscoveryInfoiter->m_type);
		DebugPrintf(SV_LOG_DEBUG,"\t---------------------\n");
		
		DebugPrintf(SV_LOG_DEBUG,"\t  -------------\n");
		DebugPrintf(SV_LOG_DEBUG,"\t  Services INFO\n");
		DebugPrintf(SV_LOG_DEBUG,"\t  -------------\n");
		std::list<InmService>::iterator versionLevelServiceInfoIter = appLevelVersionDiscoveryInfoiter->m_services.begin();
		while(versionLevelServiceInfoIter != appLevelVersionDiscoveryInfoiter->m_services.end() )
		{
			DebugPrintf(SV_LOG_DEBUG, "\t\tServiceName: %s\n", versionLevelServiceInfoIter->m_serviceName.c_str());
			DebugPrintf(SV_LOG_DEBUG, "\t\tService Status: %d\n", versionLevelServiceInfoIter->m_svcStatus);
			versionLevelServiceInfoIter++;
		}

		DebugPrintf(SV_LOG_DEBUG,"\t  --------------\n");
		DebugPrintf(SV_LOG_DEBUG,"\t  INSTANCES INFO\n");
		DebugPrintf(SV_LOG_DEBUG,"\t  --------------\n");
		std::list<MSSQLInstanceDiscoveryInfo>::iterator versionLevelInstanceInfoiter = appLevelVersionDiscoveryInfoiter->m_sqlInstanceDiscoveryInfo.begin();
		while(versionLevelInstanceInfoiter != appLevelVersionDiscoveryInfoiter->m_sqlInstanceDiscoveryInfo.end() )
		{
			DebugPrintf(SV_LOG_DEBUG,"\t\t-----------------------\n");
			DebugPrintf(SV_LOG_DEBUG, "\t\tINSTANCE: %s\n", versionLevelInstanceInfoiter->m_instanceName.c_str());
			DebugPrintf(SV_LOG_DEBUG,"\t\t-----------------------\n");
			DebugPrintf(SV_LOG_DEBUG, "\t\t\tData Dir: %s\n", versionLevelInstanceInfoiter->m_dataDir.c_str());
			DebugPrintf(SV_LOG_DEBUG, "\t\t\tDump Dir: %s\n", versionLevelInstanceInfoiter->m_dumpDir.c_str());
			DebugPrintf(SV_LOG_DEBUG, "\t\t\tError Log Path: %s\n", versionLevelInstanceInfoiter->m_errorLogPath.c_str());
			versionLevelInstanceInfoiter++;
		}
		appLevelVersionDiscoveryInfoiter++;
	}
	DebugPrintf(SV_LOG_DEBUG,"\n");
	DebugPrintf(SV_LOG_DEBUG,"########################################\n");
	DebugPrintf(SV_LOG_DEBUG, "EXITED%s\n", FUNCTION_NAME) ;
}


