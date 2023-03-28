#include <boost/algorithm/string.hpp>
#include "controller.h"
#include "sqldiscovery.h"
#ifndef _WINDOWS_
#include <Windows.h>
#endif
#include <atlbase.h>
#include "service.h"
#include "clusterutil.h"
#include "host.h"
#include "util/common/util.h"
#include "portablehelpersmajor.h"
#include <sstream>
#include <locale>
#define INC_OLE2
#define UNICODE
#define _UNICODE
#include <boost/lexical_cast.hpp>


typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
boost::shared_ptr<MSSQLDiscoveryInfoImpl> MSSQLDiscoveryInfoImpl::m_instancePtr ;

void MSSQLDiscoveryInfoImpl::prepareSupportedSQLVersionsList(std::list<InmProtectedAppType>& list)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    list.push_back(INM_APP_MSSQL2000) ;
    list.push_back(INM_APP_MSSQL2005) ;
    list.push_back(INM_APP_MSSQL2008) ;
	list.push_back(INM_APP_MSSQL2012) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

SVERROR MSSQLDiscoveryInfoImpl::fillSqlVersionLevelInfo( std::map<std::string, MSSQLInstanceDiscoveryInfo>& instanceNameToInfoMap, MSSQLVersionDiscoveryInfo& sqlVersionDiscoveryInfo )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR  retStatus = SVS_OK ;
	std::map<std::string, MSSQLInstanceDiscoveryInfo>::iterator instanceNameToInfoBeginMap = instanceNameToInfoMap.begin();
	std::map<std::string, MSSQLInstanceDiscoveryInfo>::iterator instanceNameToInfoEndMap = instanceNameToInfoMap.end();
	while( instanceNameToInfoBeginMap != instanceNameToInfoEndMap )
    {
		sqlVersionDiscoveryInfo.m_sqlInstanceDiscoveryInfo.push_back(instanceNameToInfoBeginMap->second);
		instanceNameToInfoBeginMap++;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return retStatus ;
}

SVERROR MSSQLDiscoveryInfoImpl::discoverMSSQLAppDiscoveryInfo(MSSQLDiscoveryInfo& sqlDiscoveryInfo, const std::list<InmProtectedAppType>& supportedSqlVersionsList)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR  bRet = SVS_OK ;
    boost::shared_ptr<WMISQLInterface> wmiinterfacePtr ;
	findPassiveInstanceNameList();
    std::list<InmProtectedAppType>::const_iterator supportedSqlVersionIter = supportedSqlVersionsList.begin() ;
	std::list<std::string> skippedInstanceNameListFor2008;
    while( supportedSqlVersionIter != supportedSqlVersionsList.end() )
    {
			std::map<std::string, MSSQLInstanceDiscoveryInfo> instanceNameToInfoMap;
			
			if( *supportedSqlVersionIter != INM_APP_MSSQL2008 )
			{
				skippedInstanceNameListFor2008.clear();
			}
			if(*supportedSqlVersionIter == INM_APP_MSSQL2000 )
			{
				getSQLInstancesFromRegistry(instanceNameToInfoMap, *supportedSqlVersionIter);						
			}
			else
			{
				wmiinterfacePtr = WMISQLInterface::getWMISQLInterface(*supportedSqlVersionIter) ;
				bool discoveredInstances = false ;
				if( (wmiinterfacePtr->init() == SVS_OK ) &&
					( wmiinterfacePtr->getSQLInstanceInfo(instanceNameToInfoMap, skippedInstanceNameListFor2008) == SVS_OK ) )
				{
						discoveredInstances = true ;
				}
				if(discoveredInstances == false )
				{
					getSQLInstancesFromRegistry(instanceNameToInfoMap, *supportedSqlVersionIter );
				}
			}
			if( *supportedSqlVersionIter == INM_APP_MSSQL2005 )
			{
				getInstancenameListFromMap(instanceNameToInfoMap, skippedInstanceNameListFor2008);
			}
			if( m_passiveIntanceNameList.empty() == false )
			{
				setInstancestatusInfo(instanceNameToInfoMap);
			}
			
			MSSQLVersionDiscoveryInfo sqlVersionDiscoveryInfo ;
			fillSqlVersionLevelInfo(instanceNameToInfoMap, sqlVersionDiscoveryInfo);
			sqlVersionDiscoveryInfo.m_type = *supportedSqlVersionIter ;
			discoverMSSQLVersionLevelsvcInfo(sqlVersionDiscoveryInfo.m_type,sqlVersionDiscoveryInfo.m_services);
			sqlDiscoveryInfo.m_sqlVersionDiscoveryInfo.push_back(sqlVersionDiscoveryInfo) ;
          
			supportedSqlVersionIter++ ;
    }
    discoverMSSQLApplevelsvcInfo(sqlDiscoveryInfo.m_services) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}


SVERROR MSSQLDiscoveryInfoImpl::discoverMSSQLApplevelsvcInfo(std::list<InmService>& serviceList)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR  bRet = SVS_OK ;
    InmService writerSvc("SQLWriter") ;
    QuerySvcConfig(writerSvc) ;
    serviceList.push_back(writerSvc) ;
    InmService browserSvc("SQLBrowser") ;
    QuerySvcConfig(browserSvc) ;
    serviceList.push_back(browserSvc) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
SVERROR MSSQLDiscoveryInfoImpl::discoverMSSQLVersionLevelsvcInfo(InmProtectedAppType appType,std::list<InmService>& serviceList)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_OK ;
	switch(appType)
	{
	case INM_APP_MSSQL2000:
		{
			InmService olap("MSSQLServerOLAPService");
			QuerySvcConfig(olap);
			serviceList.push_back(olap);

            InmService msftesqlService("msftesql-Exchange") ;
			QuerySvcConfig(msftesqlService);
			serviceList.push_back(msftesqlService);

            InmService adHelperSvc("MSSQLServerADHelper");
			QuerySvcConfig(adHelperSvc);
			serviceList.push_back(adHelperSvc);

        }
		break;
	case INM_APP_MSSQL2005:
		{
			InmService MsDtsSvc("MsDtsServer");
			QuerySvcConfig(MsDtsSvc);
			serviceList.push_back(MsDtsSvc);
			InmService adHelperSvc("MSSQLServerADHelper");
			QuerySvcConfig(adHelperSvc);
			serviceList.push_back(adHelperSvc);
		}
		break;
	case INM_APP_MSSQL2008:
		{
			InmService MsDtsSvc("MsDtsServer100");
			QuerySvcConfig(MsDtsSvc);
			serviceList.push_back(MsDtsSvc);
			InmService adHelperSvc("MSSQLServerADHelper100");
			QuerySvcConfig(adHelperSvc);
			serviceList.push_back(adHelperSvc);
		}
		break;
		case INM_APP_MSSQL2012:
		{
			InmService MsDtsSvc("MsDtsServer110");
			QuerySvcConfig(MsDtsSvc);
			serviceList.push_back(MsDtsSvc);
		}
		break;
	
	case INM_APP_EXCHNAGE:
		break;
	case INM_APP_MSSQL:
		break;
	case INM_APP_NONE:
		break;
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
SVERROR MSSQLDiscoveryInfoImpl::discoverMSSQLInstanceLevelsvcInfo(InmProtectedAppType appType, std::string instanceName, std::list<InmService>& serviceList)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR  bRet = SVS_OK ;
    std::string svcName ;
    if( instanceName.compare("MSSQLSERVER") != 0 )
    {
        svcName = "MSSQL$" ;
        svcName += instanceName.c_str() ;
    }
    else
    {
        svcName = "MSSQLSERVER" ;
    }

    InmService serverSvc(svcName) ;
    QuerySvcConfig(serverSvc) ;
    serviceList.push_back(serverSvc) ;
    if( instanceName.compare("MSSQLSERVER") != 0 )
    {
        svcName = "SQLAgent$" ;
        svcName += instanceName ;
    }
    else
    {
        svcName = "SQLSERVERAGENT" ;
    }

    InmService agentsvc(svcName) ;
    QuerySvcConfig(agentsvc) ;
    serviceList.push_back(agentsvc) ;

    if( appType != INM_APP_MSSQL2000 )
    {
        if( instanceName.compare("MSSQLSERVER") != 0 )
        {
	        svcName = "ReportServer$" ;
            svcName += instanceName ;
        }
        else
        {
            svcName = "ReportServer" ;
        }
        InmService reportsvc(svcName) ;
        QuerySvcConfig(reportsvc) ;
        serviceList.push_back(reportsvc) ;

        if( instanceName.compare("MSSQLSERVER") != 0 )
        {
	        svcName = "MSOLAP$" ;
            svcName += instanceName ;
        }
        else
        {
            svcName = "MSSQLServerOLAPService" ;
        }
        InmService msolapsvc(svcName) ;
        QuerySvcConfig(msolapsvc) ;
        serviceList.push_back(msolapsvc) ;

        if( instanceName.compare("MSSQLSERVER") != 0 )
        {

	        svcName = "MSFTESQL$" ;
            svcName += instanceName ;
        }
        else
        {
            svcName = "msftesql" ;
        }
        InmService msftesqlsvc(svcName) ;
        QuerySvcConfig(msftesqlsvc) ;
        serviceList.push_back(msftesqlsvc) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR MSSQLDiscoveryInfoImpl::discoverMSSQLInstanceInfo(MSSQLInstanceDiscoveryInfo& instanceDiscoveryInfo, MSSQLInstanceMetaData& metaData)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR  bRet = SVS_FALSE ;
    std::stringstream stream;
    SQLAdo *sqlAdoPtr = SQLAdo::getSQLAdoImpl(instanceDiscoveryInfo.m_appType);
   // m_SQLAdoImplPtr = SQLAdo::getSQLAdoImpl(instanceDiscoveryInfo.m_appType);
    std::stringstream sqlInstancefqn ;
    
    std::string hostname = Host::GetInstance().GetHostName() ;
    if( instanceDiscoveryInfo.m_isClustered )
    {
        sqlInstancefqn << instanceDiscoveryInfo.m_virtualSrvrName ;
    }
    else
    {
        sqlInstancefqn << hostname ;
    }
    if( instanceDiscoveryInfo.m_instanceName.compare("MSSQLSERVER") != 0 )
    {
        sqlInstancefqn << '\\' ;
        sqlInstancefqn << instanceDiscoveryInfo.m_instanceName ;
    }

    metaData.m_instanceName = instanceDiscoveryInfo.m_instanceName ;
    metaData.m_version = instanceDiscoveryInfo.m_version ;
    metaData.m_installPath = instanceDiscoveryInfo.m_installPath ;
    bool discoveryFail = false ;

	HANDLE hUser = NULL;
	if(instanceDiscoveryInfo.m_appType == INM_APP_MSSQL2012)
	{
		hUser = Controller::getInstance()->getDuplicateUserHandle();
		if(hUser)
		{
			if(ImpersonateLoggedOnUser(hUser))
			{
				DebugPrintf(SV_LOG_DEBUG,"Successfully impersonated the configured user.\n");
			}
			else
			{
				DebugPrintf(SV_LOG_WARNING,"Logon failure. MS SQL Discovery may fail. Error %d\n", GetLastError());
			}
		}
		else
		{
			DebugPrintf(SV_LOG_WARNING,"Domain User is not configured. MS SQL Discovery may fail.\n");
		}
	}
    sqlAdoPtr->init() ;
    if( sqlAdoPtr->connect(sqlInstancefqn.str(), "master") == SVS_OK )
    {
        //m_SQLAdoImplPtr->getServerProperties(instanceDiscoveryInfo.m_properties) ;
        std::list<std::string> dbNamesList ;
        if( sqlAdoPtr->getDbNames(dbNamesList) != SVS_OK )
        {
            DebugPrintf(SV_LOG_ERROR, "Unable to get the db names for the instance %\n", sqlInstancefqn.str().c_str()) ;
            stream<< "Failed to get db names for the instance: "<<sqlInstancefqn.str();
            discoveryFail = true ;
        }
        else
        {
            std::list<std::string>::const_iterator dbNameIter  = dbNamesList.begin() ;            
            while( dbNameIter != dbNamesList.end() )
            {
                MSSQLDBMetaData dbMetaData ;
                if( sqlAdoPtr->discoverDatabase((*dbNameIter), dbMetaData) != SVS_OK )
                {
                    DebugPrintf(SV_LOG_ERROR, "Unable to get the databas files for the database %s\n", (*dbNameIter).c_str()) ;
					dbMetaData.m_dbFiles.push_back("NOT FOUND");
					dbMetaData.m_volumes.push_back("NOT FOUND");
                    stream << "Failed to get db files for the database: " << (*dbNameIter);
                    discoveryFail = true ;

				}
                else
                {
                    std::list<std::string>::iterator dbFilesListIter = dbMetaData.m_dbFiles.begin() ;
                    while( dbFilesListIter != dbMetaData.m_dbFiles.end() )
                    {
                        trimSpaces(*dbFilesListIter) ;
                        int statRetVal = 0 ;
                        ACE_stat stat ;
                        std::stringstream errorstream ;
                        if( (statRetVal = sv_stat( getLongPathName(dbFilesListIter->c_str()).c_str(), &stat )) != 0 )
                        {
                            DebugPrintf(SV_LOG_WARNING, "Unable to stat for %s. This can be ignored if the volume that contains the file/volume is not available\n", dbFilesListIter->c_str()) ;
                            errorstream << "Unable to stat "   << dbFilesListIter->c_str() << " This can be ignored if the volume that contains the file/volume is not available " << std::endl ;
                            discoveryFail = true ;
                        }
                        CHAR driveLetter[256] ;
						// PR#10815: Long Path support
                        if (!discoveryFail && SVGetVolumePathName((*dbFilesListIter).c_str(), driveLetter, ARRAYSIZE(driveLetter)) == FALSE)
                        {
                            errorstream << "SVGetVolumePathName for "   << dbFilesListIter->c_str() << " failed with error " << GetLastError() ;
                            discoveryFail = true ;
                        }
                        if(  !discoveryFail && (statRetVal = sv_stat( getLongPathName(dbFilesListIter->c_str()).c_str(), &stat )) != 0 )
                        {
                            DebugPrintf(SV_LOG_WARNING, "Unable to stat for %s. This can be ignored if the volume that contains the file/volume is not available\n", dbFilesListIter->c_str()) ;
                            errorstream << "Unable to stat "   << dbFilesListIter->c_str() << " This can be ignored if the volume that contains the file/volume is not available " << std::endl ;
                            discoveryFail = true ;
                        }
                        if( discoveryFail )
                        {
                            dbMetaData.errorString =  errorstream.str() ;
                            discoveryFail = true  ;
                            break ;
                        }

                        if( find(metaData.m_volumesList.begin(), metaData.m_volumesList.end(), driveLetter) == 
                            metaData.m_volumesList.end() )
                        {
                            metaData.m_volumesList.push_back(driveLetter) ;
                            DebugPrintf(SV_LOG_DEBUG, "Volume name %s inserted for database %s\n", driveLetter, (*dbNameIter).c_str()) ;
                        }

                        if( find(dbMetaData.m_volumes.begin(), dbMetaData.m_volumes.end(), driveLetter) == 
                            dbMetaData.m_volumes.end() )
                        {
                            dbMetaData.m_volumes.push_back( driveLetter ) ;
                        }
                        dbFilesListIter ++ ;
                    }
                }
				dbMetaData.m_dbType = getSQLDataBaseTypeFromString(*dbNameIter);
				metaData.m_dbsMap.insert(std::make_pair((*dbNameIter), dbMetaData)) ;
                dbNameIter++ ;
            }
        }
    }
    else
    {
        DebugPrintf(SV_LOG_WARNING, "Failed connect to sql server instance <--> %s\n", instanceDiscoveryInfo.m_instanceName.c_str()) ;
        discoveryFail = true ;
        stream <<"Failed to connect SQL server instance: "<< instanceDiscoveryInfo.m_instanceName;
    }
	if(hUser)
	{
		RevertToSelf();
		CloseHandle(hUser);
	}

    if( !discoveryFail )
    {
        instanceDiscoveryInfo.errCode = DISCOVERY_SUCCESS;
        metaData.errCode = DISCOVERY_SUCCESS ;
        stream << " Successfully Discovered :" <<std::endl;
        stream << "Instance Name:"<<instanceDiscoveryInfo.m_instanceName <<std::endl;
        stream << "Instance Version:"<<instanceDiscoveryInfo.m_version<<std::endl;
		bRet = SVS_OK;
    }
    else
    {
        if( instanceDiscoveryInfo.errCode == DISCOVERY_SUCCESS )
        {
            instanceDiscoveryInfo.errCode = DISCOVERY_METADATANOTAVAILABLE ;
        }
        else
        {
        	instanceDiscoveryInfo.errCode = DISCOVERY_FAIL ;
        }
        metaData.errCode = DISCOVERY_FAIL ;
    }
    instanceDiscoveryInfo.errStr = stream.str();
    discoverMSSQLInstanceLevelsvcInfo(instanceDiscoveryInfo.m_appType, instanceDiscoveryInfo.m_instanceName, instanceDiscoveryInfo.m_services) ;
    if(sqlAdoPtr != NULL)
	{
		delete sqlAdoPtr;
	}
    
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
boost::shared_ptr<MSSQLDiscoveryInfoImpl> MSSQLDiscoveryInfoImpl::getInstance()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED%s\n", FUNCTION_NAME) ;
    if( m_instancePtr.get() == NULL )
    {
        m_instancePtr.reset( new MSSQLDiscoveryInfoImpl() ) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED%s\n", FUNCTION_NAME) ;
    return m_instancePtr ;
}

std::string MSSQLInstanceDiscoveryInfo::summary()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED%s\n", FUNCTION_NAME) ;
    std::stringstream stream ;
    stream<<"\n\t\tMicrosoft SQL Server Instance Information\n\n" ;
    stream << "\t\t\tInstance Name\t\t:" << m_instanceName <<std::endl ;
    stream << "\t\t\tVersion\t\t\t: " << m_version << std::endl ;
    stream << "\t\t\tEdtion\t\t\t: " << m_edition << std::endl ;
    stream << "\t\t\tInstall Path\t\t: " << m_installPath << std::endl ; 
	stream << "\t\t\tErrorLog Path\t\t: " << m_errorLogPath << std::endl;
    std::list<InmService>::iterator iter = m_services.begin() ;
    stream << "\n\t\t\tMicrosoft SQL Services Status\n\n" ;
	while( iter != m_services.end() )
    {
        std::list<std::pair<std::string, std::string> > properties = iter->getProperties() ;
        std::list<std::pair<std::string, std::string> >::iterator propIter = properties.begin() ;
        while( propIter != properties.end() )
        {
            stream << "\t\t\t\t" << propIter->first << ":\t\t\t" <<propIter->second <<std::endl;
            propIter++ ;
        }
        stream << std::endl ;
        iter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED%s\n", FUNCTION_NAME) ;
    return stream.str();
}

std::string MSSQLDiscoveryInfo::summary()
{
    std::stringstream stream ;
    std::list<InmService>::iterator sqlServiceIter = m_services.begin();
    while( sqlServiceIter != m_services.end())
    {
        if( sqlServiceIter->m_svcStatus == INM_SVCSTAT_NOTINSTALLED )
        {
            return stream.str();;
        }
        sqlServiceIter++;
    }
    stream << "\n\n\n\t Microsoft SQL Server Discovery Information\n" ;
    stream << "\n\n\t\tSQL Writer and SQL Browser Services Status\n\n" ;
    std::list<InmService>::iterator serviceIter = m_services.begin() ;
    while( serviceIter != m_services.end() )
    {
        std::list<std::pair<std::string, std::string> > properties = serviceIter->getProperties() ;
        std::list<std::pair<std::string, std::string> >::iterator propIter = properties.begin() ;
        while( propIter != properties.end() )
        {
            stream << "\t\t\t" << propIter->first << ":\t\t" <<propIter->second <<std::endl;
            propIter++ ;
        }
        stream << std::endl ;
        serviceIter++ ;
    }
 	return stream.str();
}


std::string MSSQLVersionDiscoveryInfo::summary()
{
	std::stringstream stream;    
	stream << "\n\n\t\tMicrosoft SQL Server - Version level services"<<std::endl<<std::endl;
	std::list<InmService>::iterator iter = m_services.begin();
	while(iter != m_services.end())
	{
        std::list<std::pair<std::string, std::string> > properties = iter->getProperties() ;
        std::list<std::pair<std::string, std::string> >::iterator propIter = properties.begin() ;
        while( propIter != properties.end() )
        {
            stream << "\t\t\t" << propIter->first << ":\t\t\t" <<propIter->second <<std::endl;
            propIter++ ;
        }
        stream << std::endl ;
		iter++;
	}
	return stream.str();
}

MSSQLDBType MSSQLDiscoveryInfoImpl::getSQLDataBaseTypeFromString( std::string dbName )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	MSSQLDBType dbType;
    boost::algorithm::to_lower(dbName);
	if( dbName.find("reportserver") == 0 )
	{
		dbType = MSSQL_DBTYPE_SYSTEM;
	}
	else if( dbName.compare("master") == 0 )
	{
		dbType = MSSQL_DBTYPE_MASTER;
	}
	else if( dbName.compare("model") == 0 )
	{
		dbType = MSSQL_DBTYPE_MODEL;
	}
	else if( dbName.compare("msdb") == 0 )
	{
		dbType = MSSQL_DBTYPE_MSDB;
	}
	else if( dbName.compare("tempdb") == 0 )
	{
		dbType = MSSQL_DBTYPE_TEMP;
	}
	else
	{
		dbType = MSSQL_DBTYPE_USER;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return dbType;
}
SVERROR MSSQLDiscoveryInfoImpl::findPassiveInstanceNameList()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retStatus = SVS_OK;
    m_passiveIntanceNameList.clear();
	std::list<std::string> resourceNameList ;
    //m_passiveIntanceNameList.clear() ;
	if( isClusterNode() == true )
	{
		std::string resourceName, groupName, ownerNodeName, instanceName;
		CLUSTER_RESOURCE_STATE state;
		std::string hostName = Host::GetInstance().GetHostName();
		if( getResourcesByType("", "SQL Server", resourceNameList) == SVS_OK )
		{
			std::list<std::string>::iterator iterName =  resourceNameList.begin() ;
			while( iterName != resourceNameList.end() )
			{
				resourceName = *iterName;
				if( getResourceState("",resourceName, state, groupName, ownerNodeName ) == SVS_OK )
				{
                    DebugPrintf(SV_LOG_DEBUG, "HostName :%s, ownerNode :%s\n", hostName.c_str(), ownerNodeName.c_str()) ;
					if( strcmpi(hostName.c_str(), ownerNodeName.c_str()) != 0)
					{
						/*instanceName = resourceName.substr(resourceName.find_first_of("(") + 1, resourceName.find_last_not_of(")") - resourceName.find_first_of("(")) ;
                        trimSpaces(instanceName);
                        if(resourceName.find_first_of("(") == std::string::npos)
                        {
                            m_passiveIntanceNameList.push_back("MSSQLSERVER");
                            DebugPrintf(SV_LOG_DEBUG,"MSSQLSERVER is passive\n");
                        }
                        else
                        {
                            m_passiveIntanceNameList.push_back(instanceName);
                            DebugPrintf(SV_LOG_DEBUG,"%s is passive\n", instanceName.c_str());
                        }*/
						std::list<std::string> propList;
						propList.push_back("InstanceName");
						std::map<std::string,std::string> propsMap;
						if( getResourcePropertiesMap("",resourceName,propList,propsMap) == SVS_OK &&
							propsMap.find("InstanceName") != propsMap.end() )
						{
							m_passiveIntanceNameList.push_back(propsMap.find("InstanceName")->second);
                            DebugPrintf(SV_LOG_DEBUG,"%s is passive\n", propsMap.find("InstanceName")->second.c_str());
						}
						else
						{
							DebugPrintf(SV_LOG_ERROR,"Failed to get the instnace name property of %s\n",resourceName.c_str());
						}
                    }
					else
					{
						m_strActiveNodeName = ownerNodeName;
					}
					
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR, "getResourceState failed\n") ;
				}
				iterName++;
			}
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "getResourcesByType Failed\n") ;
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus;
}
SVERROR MSSQLDiscoveryInfoImpl::getActiveNodeName(std::string &strActiveNode)
{
	SVERROR bRet = SVS_FALSE;
	if(findPassiveInstanceNameList() == SVS_OK)
	{
		if(!m_strActiveNodeName.empty())
		{
			strActiveNode = m_strActiveNodeName;
			bRet = true;
		}
	}
	else
		DebugPrintf(SV_LOG_ERROR,"Failed to get the active node name from !");

	return bRet;
}

SVERROR MSSQLDiscoveryInfoImpl::setInstancestatusInfo( std::map<std::string, MSSQLInstanceDiscoveryInfo>& instanceNameToInfoMap )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR  retStatus = SVS_OK ;
	std::list<std::string>::iterator passiveInstancenameListBeginIter = m_passiveIntanceNameList.begin();
	std::list<std::string>::iterator passiveInstancenameEndListIter = m_passiveIntanceNameList.end();
	std::map<std::string, MSSQLInstanceDiscoveryInfo>::iterator instanceNameToInfoMapIter;
	std::map<std::string, MSSQLInstanceDiscoveryInfo>::iterator instanceNameToInfoMapEndIter = instanceNameToInfoMap.end() ;
	while( passiveInstancenameListBeginIter != passiveInstancenameEndListIter )
	{
		instanceNameToInfoMapIter = instanceNameToInfoMap.begin();
		while( instanceNameToInfoMapIter != instanceNameToInfoMapEndIter )
		{
			if( strcmpi( instanceNameToInfoMapIter->first.c_str(), passiveInstancenameListBeginIter->c_str() ) == 0 )
			{
				DebugPrintf( SV_LOG_DEBUG, "Setting the Instance Name as Passive: %s \n", instanceNameToInfoMapIter->first.c_str() );
				instanceNameToInfoMapIter->second.errCode = PASSIVE_INSTANCE;
				instanceNameToInfoMapIter->second.errStr = "PASSIVE_INSTANCE";
				break;
			}
			instanceNameToInfoMapIter++;
		}

		/*instanceNameToInfoMapIter = instanceNameToInfoMap.find(*passiveInstancenameListBeginIter);
		if( instanceNameToInfoMapIter != instanceNameToInfoMapEndIter )
		{
			DebugPrintf( SV_LOG_DEBUG, "Setting the Instance Name as Passive: %s \n", instanceNameToInfoMapIter->first.c_str() );
			instanceNameToInfoMapIter->second.errCode = PASSIVE_INSTANCE;
			instanceNameToInfoMapIter->second.errStr = "PASSIVE_INSTANCE";
		}*/
		passiveInstancenameListBeginIter++;
	}

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return retStatus ;
}
SVERROR MSSQLDiscoveryInfoImpl::getInstancenameListFromMap( std::map<std::string, MSSQLInstanceDiscoveryInfo>& instanceNameToInfoMap, std::list<std::string>& skippedInstanceNameListFor2008)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR  retStatus = SVS_OK ;
	std::map<std::string, MSSQLInstanceDiscoveryInfo>::iterator instanceNameToInfoBeginMap = instanceNameToInfoMap.begin();
	std::map<std::string, MSSQLInstanceDiscoveryInfo>::iterator instanceNameToInfoEndMap = instanceNameToInfoMap.end();
	while( instanceNameToInfoBeginMap != instanceNameToInfoEndMap )
	{
		skippedInstanceNameListFor2008.push_back(instanceNameToInfoBeginMap->first);
		instanceNameToInfoBeginMap++;
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return retStatus ;
}

/*bool MSSQLDiscoveryInfoImpl::isSQL2000ServicesRunning()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool bRet = false;
		std::map<std::string, MSSQLInstanceDiscoveryInfo> sql2000InstInfoMap;
		InmProtectedAppType appType=INM_APP_MSSQL2000;
		if(getSQLInstancesFromRegistry(sql2000InstInfoMap, appType) == SVS_OK)
    {
        InmServiceStatus serviceStatus = INM_SVCSTAT_NONE; 
        std::map<std::string,MSSQLInstanceDiscoveryInfo>::iterator sql2000InstInfoMapIter = sql2000InstInfoMap.begin();
        while(sql2000InstInfoMapIter != sql2000InstInfoMap.end())
        {
            std::string serviceName = sql2000InstInfoMapIter->first;;
						if(serviceName.compare("MSSQLSERVER")!=0)
            {
							
              serviceName = "MSSQL$"+serviceName;
            }
            
            getServiceStatus(serviceName, serviceStatus);
            if( serviceStatus == INM_SVCSTAT_RUNNING )
            {
                DebugPrintf(SV_LOG_DEBUG, "%s service status is running..\n",serviceName.c_str()) ;
                bRet = true;
                break;
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG, "%s service status is not runnning..\n",serviceName.c_str()) ;
            }
            sql2000InstInfoMapIter++;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bRet;
}*/

SVERROR MSSQLDiscoveryInfoImpl::getSQLInstancesFromRegistry(std::map<std::string, MSSQLInstanceDiscoveryInfo> &instanceNameToInfoMap, InmProtectedAppType appType)
{
   DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ; 		
   REGSAM samDesired=KEY_ALL_ACCESS;
   BOOL bIsWow64 = FALSE;
   std::string host = Host::GetInstance().GetHostName();
   
   LPFN_ISWOW64PROCESS fnIsWow64Process = (LPFN_ISWOW64PROCESS) GetProcAddress(GetModuleHandle(TEXT("kernel32")),"IsWow64Process");
   DebugPrintf(SV_LOG_DEBUG,"Discovering SQL Instances\n");
   if (NULL != fnIsWow64Process)
   {
       if (!fnIsWow64Process(GetCurrentProcess(),&bIsWow64))
       {
           DWORD lastError = GetLastError();
           DebugPrintf(SV_LOG_ERROR," Failed to determine if the host is a 32 bit or a 64 bit platform,Error is %ld \n",lastError);
           return SVE_FAIL;
       }
   }
   if(bIsWow64 == TRUE)
   {
       samDesired = KEY_WOW64_64KEY | KEY_ALL_ACCESS;
       
       if(getSQLInstInfoFromRegistry(instanceNameToInfoMap,host,samDesired,appType)==SVE_FAIL)
       {
           DebugPrintf(SV_LOG_ERROR,"Failed to get SQL Instance Names\n");
           return SVE_FAIL;
       }
   }

   samDesired=KEY_ALL_ACCESS;
   if(getSQLInstInfoFromRegistry(instanceNameToInfoMap,host,samDesired,appType)==SVE_FAIL)
   {
       DebugPrintf(SV_LOG_ERROR,"Failed to get SQL Instance Names\n");
       return SVE_FAIL;
   }
   /*if(instanceNameToInfoMap.empty())
   {
       DebugPrintf(SV_LOG_ERROR,"Couldn't find SQL Server with the name\n");
       return SVE_FAIL;
   }*/
	 DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
   return SVS_OK;  		
    
}

bool MSSQLDiscoveryInfoImpl::queryRegistryValue(HKEY &hKey,std::string ValueName,DWORD &dwType,DWORD &dwSize,char** PerfData)
{
	DWORD lstatus=0;
	bool result=false;
	
	lstatus=RegQueryValueExA(hKey,ValueName.c_str(), NULL, &dwType, NULL,&dwSize);	
	if(lstatus==ERROR_SUCCESS)
	{
		*PerfData = (char*) malloc(dwSize);
		if (*PerfData == NULL)
		 {
			DebugPrintf(SV_LOG_ERROR,"Unable to allocate a memory of size %lu on the Heap!\n",dwSize);
			return false;
		 }
		 if(RegQueryValueExA(hKey,ValueName.c_str(), NULL, &dwType, (LPBYTE)*PerfData,&dwSize) == ERROR_SUCCESS)
		 {
			 DebugPrintf(SV_LOG_DEBUG, "Successfully read the value of %s from the registry\n",ValueName.c_str()) ;
			 result=true;
		 }
	}
	else
	{
		if(lstatus==ERROR_FILE_NOT_FOUND)
		{
			result=false;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to query the registry value of %s. [ERROR CODE]:%lu \n",ValueName.c_str(),lstatus);
			result=false;
		}
	}
	
	return result;	
}


SVERROR MSSQLDiscoveryInfoImpl::getSQLInstInfoFromRegistry(std::map<std::string, MSSQLInstanceDiscoveryInfo> &instanceNameToInfoMap,const std::string& host,REGSAM RegKeyAccess, InmProtectedAppType appType)
{
	SVERROR result = SVS_OK;
	HKEY hKey;
	DWORD dwType=REG_SZ;
	DWORD dwSize;
	DWORD lstatus=0;
	char* PerfData;
	std::string subKey;
	std::vector<std::string>ALLInstanceNames;

	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	result=enumSqlInstances(ALLInstanceNames,RegKeyAccess);
	if(result==SVS_OK && ALLInstanceNames.empty()==false)
	{	
		HKEY ClusKey;
		std::vector<std::string>::iterator ALLInstancesIter = ALLInstanceNames.begin();
		bool isSQL2000Instance;
		std::string instanceName="";

		for(;ALLInstancesIter != ALLInstanceNames.end();ALLInstancesIter++)
		{
			isSQL2000Instance=true;
			instanceName= *ALLInstancesIter;
			std::string virtualServer="";
			std::string intermediateInstanceName="";
			std::string versionStr="";
			std::string edition="";
			std::string dataRoot="";
			std::string installPath="";
			std::string InstnaceErrMsg = "Successfully discovered the instance " + instanceName;
			InmProtectedAppType SQLVersion = INM_APP_NONE;

			lstatus=RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SOFTWARE\\Microsoft\\Microsoft SQL Server\\Instance Names\\SQL",0,RegKeyAccess,&hKey);
			if(lstatus==ERROR_SUCCESS)
			{
				if(queryRegistryValue(hKey,(*ALLInstancesIter),dwType,dwSize,&PerfData)==true)
				{
					intermediateInstanceName=std::string(PerfData);
					subKey ="SOFTWARE\\Microsoft\\Microsoft SQL Server\\"+intermediateInstanceName+"\\Setup";
					isSQL2000Instance=false;
					free(PerfData);
				}		
			}
			RegCloseKey(hKey);
			if(isSQL2000Instance)
			{
				if(instanceName.compare("MSSQLSERVER")==0)
				{
					subKey="SOFTWARE\\Microsoft\\MSSQLSERVER\\Setup";
				}
				else
				{
					subKey="SOFTWARE\\Microsoft\\Microsoft SQL Server\\"+instanceName+"\\Setup";
				}				
			}
			
			if(RegOpenKeyExA(HKEY_LOCAL_MACHINE,subKey.c_str(),0,RegKeyAccess,&hKey)==ERROR_SUCCESS)
			{
				//Determining the version of a sqlserver instance
				if(queryRegistryValue(hKey,"Patchlevel",dwType,dwSize,&PerfData)==true)
				{
					versionStr=std::string(PerfData);
					std::string version=strtok(PerfData,".");					
					if( version.compare("10")==0 )
						SQLVersion=INM_APP_MSSQL2008;
					else if(version.compare("11")==0 )
						SQLVersion=INM_APP_MSSQL2012;
					else if(version.compare("9")==0 )
						SQLVersion=INM_APP_MSSQL2005;					
					else if( version.compare("8")==0)
						SQLVersion=INM_APP_MSSQL2000;
					free(PerfData);
				}	
				else
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to determine the Version of SQL Instance: %s from the registry\n",instanceName.c_str());
					//RegCloseKey(hKey);
					result=SVE_FAIL;
					InstnaceErrMsg = "Failed to determine the Version of SQL Instance: " + instanceName + " from the registry";
					//break;
				}		
				//Determinig the Edition of SQLServer instance
				if((result == SVS_OK) && (queryRegistryValue(hKey,"Edition",dwType,dwSize,&PerfData)==true))
				{
					edition=std::string(PerfData);				
					free(PerfData);
				}	
				else
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to determine the Edition of SQL Instance: %s from the registry\n",instanceName.c_str());
					//RegCloseKey(hKey);
					result=SVE_FAIL;
					//break;
					InstnaceErrMsg = "Failed to determine the Edition of SQL Instance: " + instanceName +" from the registry";
				}
				//Determinig the Data Root of SQLServer instance
				if((result == SVS_OK) && (queryRegistryValue(hKey,"SQLDataRoot",dwType,dwSize,&PerfData)==true))
				{
					dataRoot=std::string(PerfData);				
					free(PerfData);
				}	
				else
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to determine the Data Root of SQL Instance: %s from the registry\n",instanceName.c_str());
					//RegCloseKey(hKey);
					result=SVE_FAIL;
					//break;
					InstnaceErrMsg = "Failed to determine the Data Root of SQL Instance: " + instanceName + " from the registry";
				}
				//Determinig the Install path of SQLServer instance
				if((result == SVS_OK) && (queryRegistryValue(hKey,"SQLPath",dwType,dwSize,&PerfData)==true))
				{
					installPath=std::string(PerfData);				
					free(PerfData);
				}	
				else
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to determine the Install path of SQL Instance: %s from the registry\n",instanceName.c_str());
					//RegCloseKey(hKey);
					result=SVE_FAIL;
					//break;
					InstnaceErrMsg = "Failed to determine the Install path of SQL Instance: " + instanceName + " from the registry";
				}
				RegCloseKey(hKey);
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to determine Installation Information of the SQL Instance: %s\n",instanceName.c_str());
				result=SVE_FAIL;
				InstnaceErrMsg = "Failed to determine Installation Information of the SQL Instance: " + instanceName;
				//break;
			}
			if(SQLVersion==appType)
			{
				MSSQLInstanceDiscoveryInfo instanceDiscInfo;
				instanceDiscInfo.m_instanceName=instanceName;
				instanceDiscInfo.m_version=versionStr;
				instanceDiscInfo.m_edition=edition;
				instanceDiscInfo.m_dataDir=dataRoot;
				if(result == SVS_OK)
				{
					instanceDiscInfo.errCode=DISCOVERY_SUCCESS;
					instanceDiscInfo.errStr="Successfully discovered Information of SQL Instance";
				}
				else
				{
					instanceDiscInfo.errCode = DISCOVERY_FAIL;
					instanceDiscInfo.errStr = InstnaceErrMsg;
				}
				instanceDiscInfo.m_appType=	SQLVersion;
				instanceDiscInfo.m_installPath=installPath;
				
				if(SQLVersion==INM_APP_MSSQL2012 || SQLVersion==INM_APP_MSSQL2008 || SQLVersion==INM_APP_MSSQL2005)
				{
					subKey ="SOFTWARE\\Microsoft\\Microsoft SQL Server\\"+intermediateInstanceName+"\\Cluster";
				}
				else	
				{
					if((*ALLInstancesIter)=="MSSQLSERVER")
					{
						subKey="SOFTWARE\\Microsoft\\MSSQLSERVER\\Cluster";
					}
					else
					{
						subKey="SOFTWARE\\Microsoft\\Microsoft SQL Server\\"+(*ALLInstancesIter)+"\\Cluster";
					}
				}	 
				//Checking for given Sqlserver is cluster virtual server or not
				if(RegOpenKeyExA(HKEY_LOCAL_MACHINE,subKey.c_str(),0,RegKeyAccess,&ClusKey)!=ERROR_SUCCESS)
				{
					instanceDiscInfo.m_isClustered= 0;
					instanceDiscInfo.m_virtualSrvrName=	"";	
					DebugPrintf(SV_LOG_DEBUG,"SQL Instance Name: %s\n", instanceName.c_str());
				}
				else
				{
					if(queryRegistryValue(ClusKey,"ClusterName",dwType,dwSize,&PerfData)==true)
					{
						virtualServer = std::string(PerfData);
						free(PerfData);
						instanceDiscInfo.m_isClustered= 1;
						instanceDiscInfo.m_virtualSrvrName=	virtualServer;				
						DebugPrintf(SV_LOG_DEBUG,"Virtual Server Name: %s\n", virtualServer.c_str());				
					}
				}//end of else
				RegCloseKey(ClusKey);
				instanceNameToInfoMap[instanceName]=instanceDiscInfo;
				if(result != SVS_OK)
				{//Stop gathering information
					break;
				}
			}//end of if block
		}//end of for loop
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return result;
}

SVERROR MSSQLDiscoveryInfoImpl::enumSqlInstances(std::vector<std::string> &SQLInstances,REGSAM RegKeyAccess)
{
	SVERROR result = SVS_OK;
	HKEY hKey;
	DWORD dwType=REG_MULTI_SZ;
	DWORD dwSize;
	DWORD lstatus=0;
	char* PerfData;
	
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;

	//opening registry
	lstatus=RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SOFTWARE\\Microsoft\\Microsoft SQL Server",0,RegKeyAccess,&hKey);
	if(lstatus==ERROR_SUCCESS)
	{
		//querying registry to get SQL instance names
		if(queryRegistryValue(hKey,"InstalledInstances",dwType,dwSize,&PerfData)==true)
		{
			std::string str,str1 ;
			for(size_t i=0;i<dwSize;i++)
			{
				if(PerfData[i] !=  '\0')
				{
					str += PerfData[i];
				}
				else
				{
					str1 = str;
					if(!str1.empty())
					{
						SQLInstances.push_back(str1);
					}
					str.clear();
				}
			}
			free(PerfData);
		}
	}
	else if(lstatus==ERROR_FILE_NOT_FOUND)
	{
		DebugPrintf(SV_LOG_INFO, "Couldn't find HKLM\\SOFTWARE\\Microsoft\\Microsoft SQL Server\n") ;
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to open registry key HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Microsoft SQL Server. [Error]:%l\n",lstatus);
		result= SVE_FAIL;
	}
	RegCloseKey(hKey);	
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return result;
}