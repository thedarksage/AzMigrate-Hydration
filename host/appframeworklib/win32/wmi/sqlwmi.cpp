#include "sqlwmi.h"
#include "host.h"
#include <string>
#include <sstream>
#include <algorithm>
#include <atlconv.h>
#include <boost/algorithm/string.hpp>

SVERROR WMISQLInterface::init()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR  bRet = SVS_FALSE ;
    if( m_init == false )
    {
        if( m_wmiConnectorPtr.get() == NULL )
        {
            m_wmiConnectorPtr.reset( new WMIConnector() ) ;
        }

        m_init = true ;
        bRet = SVS_OK ;

    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "WMI SQL Interface already initialized\n") ;
        bRet = SVS_OK ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

boost::shared_ptr<WMISQLInterface>WMISQLInterface::getWMISQLInterface(InmProtectedAppType type)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    boost::shared_ptr<WMISQLInterface> wmiInterfacePtr ;
    switch( type )
    {
    case INM_APP_MSSQL2000:
        wmiInterfacePtr.reset( new WMISQL2000Impl() ) ;
        break ;
    case INM_APP_MSSQL2005:
    case INM_APP_MSSQL2008:
	case INM_APP_MSSQL2012:
        wmiInterfacePtr.reset( new WMISQLImpl(type) ) ;
        break ;
    default:
        DebugPrintf(SV_LOG_ERROR, "Un-supported version of sql server\n") ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return wmiInterfacePtr ;
}

SVERROR WMISQL2000Impl::init()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    std::string hostname = Host::GetInstance().GetHostName() ;

    if( WMISQLInterface::init() == SVS_OK )
    {
        std::string wmiProvider = "root\\MicrosoftSQLServer" ;
        if( m_wmiConnectorPtr->ConnectToServer(hostname, wmiProvider)  == 0 )
        {
            DebugPrintf(SV_LOG_DEBUG, "Successfully connect to %s\n on localhost\n", wmiProvider.c_str()) ;
            bRet = SVS_OK ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR WMISQL2000Impl::getSQLRegistrySettings( const std::string& instanceName, MSSQLInstanceDiscoveryInfo& instanceDiscoverInfo )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR  bRet = SVS_FALSE ;
    USES_CONVERSION ;
    std::list<std::string> servceNameList;
    std::stringstream query ;
    query << "SELECT SQLDataRoot, SQLRootPath, ErrorLogPath FROM MSSQL_RegistrySetting WHERE " << "SettingID LIKE " ;
    if( instanceName.compare("MSSQLSERVER") == 0 )
        query << "'(LOCAL)'" ;
    else
        query << "'%" << instanceName << "%'" ;

    DebugPrintf( SV_LOG_DEBUG, "WQL Query is: %s \n", query.str().c_str() );
    IEnumWbemClassObject* pEnumerator = NULL ;
    if( m_wmiConnectorPtr->execQuery(query.str(), &pEnumerator) == SVS_OK )
    {
        IWbemClassObject *pclsObj = NULL ;
        ULONG uReturn = 0;
        while (pEnumerator)
        {
            HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
            if( SUCCEEDED(hr) )
            {
                if(0 == uReturn)
                {
                    break;
                }
                VARIANT vtPropDataRoot;
                HRESULT hr1 = pclsObj->Get(L"SQLDataRoot", 0, &vtPropDataRoot, 0, 0);
                std::string dataDir = W2A(vtPropDataRoot.bstrVal) ;
                VariantClear(&vtPropDataRoot);	
                if(!FAILED(hr1))
                {
                    instanceDiscoverInfo.m_dataDir = dataDir;					
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "Failed to get Poperty: SQLDataRoot\n");
                }

                VARIANT vtPropRootPath;
                HRESULT hr2 = pclsObj->Get(L"SQLRootPath", 0, &vtPropRootPath, 0, 0);
                std::string rootPath = W2A(vtPropRootPath.bstrVal) ;
                VariantClear(&vtPropRootPath);	
                if(!FAILED(hr2))
                {
                    instanceDiscoverInfo.m_installPath = rootPath;					
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "Failed to get Poperty: SQLRootPath\n");
                }

                VARIANT vtPropErrLogPath;
                HRESULT hr3 = pclsObj->Get(L"ErrorLogPath", 0, &vtPropErrLogPath, 0, 0);
                std::string errLogPath = W2A(vtPropErrLogPath.bstrVal) ;
                VariantClear(&vtPropErrLogPath);	
                if(!FAILED(hr3))
                {
                    instanceDiscoverInfo.m_errorLogPath = errLogPath;					
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "Failed to get Poperty: ErrorLogPath\n");
                }
                pclsObj->Release();
            }
            else
            {
				DebugPrintf(SV_LOG_WARNING, "Failed in IWbemClassObject next. hr = %08X return = %ld\n",hr, uReturn);
				break;
            }

        }
        pEnumerator->Release();
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to execute the query %s\n", query.str().c_str()) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
SVERROR WMISQL2000Impl::getSQLInstanceEdition( std::string versionString, std::string& edition )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR  bRet = SVS_FALSE ;
    versionString.erase( versionString.find_last_not_of("\n") + 1) ; //erase all trailing spaces
    versionString.erase(0 , versionString.find_first_not_of("\n") ) ; //erase all leading spaces

    DebugPrintf(SV_LOG_DEBUG, "Version String is %s\n", versionString.c_str()) ;
    versionString = versionString.substr(versionString.find_last_of("\n")) ;
    versionString.erase( versionString.find_last_not_of(" ") + 1) ; //erase all trailing spaces
    versionString.erase(0 , versionString.find_first_not_of(" ") ) ; //erase all leading spaces
    versionString.erase(0 , versionString.find_first_not_of("\t") ) ; //erase all leading spaces
    versionString.erase(0 , versionString.find_first_not_of("\n") ) ; //erase all leading spaces
    DebugPrintf(SV_LOG_DEBUG, "New Version String is %s\n", versionString.c_str()) ;
    edition = versionString ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
SVERROR WMISQL2000Impl::getSQLInstanceInfo( std::map<std::string, MSSQLInstanceDiscoveryInfo>& instanceNameToInfoMap, std::list<std::string>& skippedInstanceNameList )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR  bRet = SVS_FALSE ;
    USES_CONVERSION ;
    std::list<std::string> servceNameList;
    std::stringstream query ;
    query << "SELECT NAME, VersionString, Clustered FROM MSSQL_SQLServer WHERE NAME != NULL";
    DebugPrintf( SV_LOG_DEBUG, "WQL Query is: %s \n", query.str().c_str() );
    IEnumWbemClassObject* pEnumerator = NULL ;
    if( m_wmiConnectorPtr->execQuery(query.str(), &pEnumerator) == SVS_OK )
    {
        IWbemClassObject *pclsObj = NULL ;
        ULONG uReturn = 0;
        while (pEnumerator)
        {
            HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
            if( SUCCEEDED(hr) )
            {
                if(0 == uReturn)
                {
                    break;
                }
                VARIANT vtPropName;
                HRESULT hr1 = pclsObj->Get(L"Name", 0, &vtPropName, 0, 0);
                if(!FAILED(hr1))
                {
                    std::string serviceName = "";
                    std::string nameVal = W2A(vtPropName.bstrVal) ;
                    VariantClear(&vtPropName);	            
                    if( nameVal.compare("(LOCAL)") == 0 )
                    {
                        serviceName = "MSSQLSERVER" ;
                    }
                    else
                    {
                        serviceName  = nameVal.substr(nameVal.find_first_of("\\") + 1 ) ;
                        serviceName = "MSSQL$" + serviceName ;
                    }
                    servceNameList.push_back(serviceName);
                    DebugPrintf(SV_LOG_DEBUG, "The SQL ServiceName is %S\n", serviceName.c_str()) ;
                    std::string instanceName = "MSSQLSERVER" ;
                    if( serviceName.compare(instanceName) != 0 )
                    {
                        instanceName = serviceName.substr(serviceName.find_first_of("$") + 1 ) ;
                    }
                    DebugPrintf(SV_LOG_DEBUG, "Instance Name:%s \n", instanceName.c_str()) ;
                    if( instanceNameToInfoMap.find(instanceName) == instanceNameToInfoMap.end() )
                    {
                        MSSQLInstanceDiscoveryInfo instanceDiscInfo;
                        instanceDiscInfo.m_instanceName = instanceName;
                        instanceNameToInfoMap.insert( std::make_pair(instanceName, instanceDiscInfo) );				
                    }
                    else
                    {
                        instanceNameToInfoMap.find(instanceName)->second.m_instanceName = instanceName;
                    }
                    VARIANT vtPropVersionString;
                    HRESULT hr2 = pclsObj->Get(L"VersionString", 0, &vtPropVersionString, 0, 0);
                    if(!FAILED(hr2))
                    {
                        std::string versionStringVal = W2A(vtPropVersionString.bstrVal) ;
                        std::string versionStringValForEdition = versionStringVal;
                        VariantClear(&vtPropName);	
                        DebugPrintf(SV_LOG_DEBUG, "Version string %s\n", versionStringVal.c_str()) ;
                        versionStringVal = versionStringVal.substr(versionStringVal.find_first_of("-") + 2 , versionStringVal.find_first_of("(") - versionStringVal.find_first_of("-") - 3) ;
                        DebugPrintf(SV_LOG_DEBUG, "Version Value %s\n", versionStringVal.c_str()) ;
                        std::string editionvalue;
                        getSQLInstanceEdition(versionStringValForEdition, editionvalue);
                        if( instanceNameToInfoMap.find(instanceName) == instanceNameToInfoMap.end() )
                        {
                            MSSQLInstanceDiscoveryInfo instanceDiscInfo;
                            instanceDiscInfo.m_appType = INM_APP_MSSQL2000;
                            instanceDiscInfo.m_version = versionStringVal;
                            instanceDiscInfo.m_edition = editionvalue;
                            instanceNameToInfoMap.insert( std::make_pair(instanceName, instanceDiscInfo) );				
                        }
                        else
                        {
                            instanceNameToInfoMap.find(instanceName)->second.m_appType = INM_APP_MSSQL2000;
                            instanceNameToInfoMap.find(instanceName)->second.m_version = versionStringVal;
                            instanceNameToInfoMap.find(instanceName)->second.m_edition = editionvalue;
                        }
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_ERROR, "Failed to get Poperty: VersionString\n");
                    }
                    VARIANT vtPropClusterd;
                    HRESULT hr3 = pclsObj->Get(L"Clustered", 0, &vtPropClusterd, 0, 0);
                    if(!FAILED(hr3))
                    {
                        SV_UINT clusterValue;
                        clusterValue = vtPropClusterd.boolVal ;
                        VariantClear(&vtPropName);	
                        DebugPrintf(SV_LOG_DEBUG, "Clustered property is %d\n", clusterValue) ;
                        if( instanceNameToInfoMap.find(instanceName) == instanceNameToInfoMap.end() )
                        {
                            MSSQLInstanceDiscoveryInfo instanceDiscInfo;
                            instanceDiscInfo.m_isClustered = clusterValue;
                            instanceNameToInfoMap.insert( std::make_pair(instanceName, instanceDiscInfo) );				
                        }
                        else
                        {
                            instanceNameToInfoMap.find(instanceName)->second.m_isClustered = clusterValue;
                        }
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_ERROR, "Failed to get Poperty: Clustered\n");
                    }
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "Failed to get Poperty: Name\n");
                }
                pclsObj->Release();
            }
            else
            {
                DebugPrintf(SV_LOG_WARNING, "Failed in IWbemClassObject next. hr = %08X return = %ld\n",hr, uReturn);
				break;
            }
            pEnumerator->Release();
        }         
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to execute the query %s\n", query.str().c_str()) ;
    }
    std::map<std::string, MSSQLInstanceDiscoveryInfo>::iterator instanceNameToInfoMapBegIter = instanceNameToInfoMap.begin();
    std::map<std::string, MSSQLInstanceDiscoveryInfo>::iterator instanceNameToInfoMapEndIter = instanceNameToInfoMap.end();
    while( instanceNameToInfoMapBegIter != instanceNameToInfoMapEndIter )
    {
        getSQLRegistrySettings(instanceNameToInfoMapBegIter->first, instanceNameToInfoMapBegIter->second );
        instanceNameToInfoMapBegIter++;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

WMISQLImpl::WMISQLImpl( const InmProtectedAppType& type )
{
    m_appType = type ;
}

SVERROR WMISQLImpl::init()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR  bRet = SVS_FALSE ;
    if( WMISQLInterface::init() == SVS_OK )
    {
        std::string hostname = Host::GetInstance().GetHostName() ;
        std::string wmiProvider ;
        if( m_appType == INM_APP_MSSQL2008 )
        {
            wmiProvider = "root\\Microsoft\\SqlServer\\ComputerManagement10" ;
        }
		else if( m_appType == INM_APP_MSSQL2012 ) 
		{
            wmiProvider = "root\\Microsoft\\SqlServer\\ComputerManagement11" ;
        }
        else
        {
            wmiProvider = "root\\Microsoft\\SqlServer\\ComputerManagement" ;
        }        
        if( m_wmiConnectorPtr->ConnectToServer(hostname, 
            wmiProvider)  == 0 )
        {
            DebugPrintf(SV_LOG_DEBUG, "Successfully connect to %s on localhost\n", wmiProvider.c_str()) ;
            bRet = SVS_OK ;
        }
        else
        {
            DebugPrintf(SV_LOG_WARNING, "Failed to connect to %s on localhost\n", wmiProvider.c_str()) ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR WMISQLImpl::getSQLInstanceInfo( std::map<std::string, MSSQLInstanceDiscoveryInfo>& instanceNameToInfoMap, std::list<std::string>& skippedInstanceNameList )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR  bRet = SVS_FALSE ;
    std::list<std::string> servceNameList;
    std::string hostname = Host::GetInstance().GetHostName() ;    
    std::stringstream query ;
    USES_CONVERSION ;
    query << "Select ServiceName, PropertyName, PropertyStrValue, PropertyNumValue from SqlServiceAdvancedProperty where ( PropertyName='VSNAME' OR PropertyName='STARTUPPARAMETERS' OR PropertyName='INSTALLPATH' OR PropertyName='DATAPATH' OR PropertyName='DUMPDIR' OR PropertyName='DUMPDIR' OR PropertyName='SKUNAME' OR PropertyName='VERSION' OR PropertyName='CLUSTERED' )";
    if( m_appType == INM_APP_MSSQL2008 )
    {
        std::list<std::string>::iterator skippedInstanceNameListBegIter = skippedInstanceNameList.begin();
        std::list<std::string>::iterator skippedInstanceNameListEndIter = skippedInstanceNameList.end();
        if( skippedInstanceNameListBegIter != skippedInstanceNameListEndIter )
        {
            query << " AND (";
            do
            {
                std::string ServiceName = *skippedInstanceNameListBegIter;
                if( strcmpi(ServiceName.c_str(), "MSSQLSERVER") != 0 )
                {
                    ServiceName = "MSSQL$" + ServiceName;				
                }
                query << " ServiceName != '"<< ServiceName <<"'";
                skippedInstanceNameListBegIter++;
                if( skippedInstanceNameListBegIter != skippedInstanceNameListEndIter )
                {
                    query << " AND";
                }
            }while( skippedInstanceNameListBegIter != skippedInstanceNameListEndIter );
            query << " )";
        }
    }
    size_t discInfoMapSize = instanceNameToInfoMap.size() ;
    DebugPrintf( SV_LOG_DEBUG, "WQL Query is: %s \n", query.str().c_str() );
    IEnumWbemClassObject* pEnumerator = NULL ;
    if( m_wmiConnectorPtr->execQuery(query.str(), &pEnumerator) == SVS_OK )
    {
        IWbemClassObject *pclsObj = NULL ;
        ULONG uReturn = 0;
        while (pEnumerator)
        {
            HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
            if( SUCCEEDED(hr) )
            {
                if(0 == uReturn)
                {
                    pclsObj = NULL ;
                    break;
                }
                VARIANT vtPropServiceName;
                HRESULT hr1 = pclsObj->Get(L"ServiceName", 0, &vtPropServiceName, 0, 0);
                if( !FAILED(hr1) )
                {
                    std::string ServiceName = "";
                    if( vtPropServiceName.bstrVal != NULL )
                    {
                        ServiceName = W2A(vtPropServiceName.bstrVal);
                    }
                    servceNameList.push_back(ServiceName);
                    VariantClear(&vtPropServiceName);
                    boost::algorithm::to_upper(ServiceName);
                    if( strcmpi(ServiceName.c_str(), "MSSQLSERVER") == 0 || ServiceName.find("MSSQL$", 0) == 0 )
                    {
                        DebugPrintf(SV_LOG_DEBUG, "Service Name: %s \n",ServiceName.c_str() );
                        std::string instanceName = "MSSQLSERVER" ;
                        if( strcmpi(ServiceName.c_str(), "MSSQLSERVER") != 0 )
                        {
                            instanceName = ServiceName.substr(ServiceName.find_first_of("$") + 1 ) ;
                        }
                        if( instanceNameToInfoMap.find(instanceName) == instanceNameToInfoMap.end() )
                        {
                            MSSQLInstanceDiscoveryInfo instanceDiscInfo;
                            instanceDiscInfo.m_instanceName = instanceName;
                            instanceNameToInfoMap.insert( std::make_pair(instanceName, instanceDiscInfo) );				
                        }
                        else
                        {
                            instanceNameToInfoMap.find(instanceName)->second.m_instanceName = instanceName;
                        }


                        DebugPrintf(SV_LOG_DEBUG, "Instance Name:%s \n", instanceName.c_str()) ;

                        std::string PropertyName = "";
                        VARIANT vtProp;
                        HRESULT hr2 = pclsObj->Get(L"PropertyName", 0, &vtProp, 0, 0);
                        if( !FAILED(hr2) && vtProp.bstrVal != NULL )
                        {
                            PropertyName = W2A(vtProp.bstrVal);
                            DebugPrintf(SV_LOG_DEBUG, "PropertyName is %s \n", PropertyName.c_str());
                            if( strcmpi( PropertyName.c_str(), "VERSION") == 0 )
                            {
                                VARIANT vtProp;
                                HRESULT hr3 = pclsObj->Get(L"PropertyStrValue", 0, &vtProp, 0, 0);
                                if( !FAILED(hr3) && vtProp.bstrVal != NULL )
                                {
                                    std::string versionValue = W2A(vtProp.bstrVal);
                                    if( instanceNameToInfoMap.find(instanceName) == instanceNameToInfoMap.end() )
                                    {
                                        MSSQLInstanceDiscoveryInfo instanceDiscInfo;
                                        instanceDiscInfo.m_appType = m_appType;
                                        instanceDiscInfo.m_version = versionValue;
                                        instanceNameToInfoMap.insert( std::make_pair(instanceName, instanceDiscInfo) );				
                                    }
                                    else
                                    {
                                        instanceNameToInfoMap.find(instanceName)->second.m_appType = m_appType;
                                        instanceNameToInfoMap.find(instanceName)->second.m_version = versionValue;
                                    }
                                    DebugPrintf(SV_LOG_DEBUG, "versionValue is %s \n", versionValue.c_str());
                                }
                                VariantClear(&vtProp);
                            }
                            else if( strcmpi( PropertyName.c_str(), "SKUNAME") == 0)
                            {
                                VARIANT vtProp;
                                HRESULT hr3 = pclsObj->Get(L"PropertyStrValue", 0, &vtProp, 0, 0);
                                if( !FAILED(hr3) && vtProp.bstrVal != NULL )
                                {
                                    std::string editionValue = W2A(vtProp.bstrVal);
                                    if( instanceNameToInfoMap.find(instanceName) == instanceNameToInfoMap.end() )
                                    {
                                        MSSQLInstanceDiscoveryInfo instanceDiscInfo;
                                        instanceDiscInfo.m_edition = editionValue;
                                        instanceNameToInfoMap.insert( std::make_pair(instanceName, instanceDiscInfo) );				
                                    }
                                    else
                                    {
                                        instanceNameToInfoMap.find(instanceName)->second.m_edition = editionValue;
                                    }
                                    DebugPrintf(SV_LOG_DEBUG, "Edition is %s \n", editionValue.c_str());
                                }
                                VariantClear(&vtProp);
                            }
                            else if( strcmpi( PropertyName.c_str(), "DATAPATH") == 0 )
                            {
                                VARIANT vtProp;
                                HRESULT hr3 = pclsObj->Get(L"PropertyStrValue", 0, &vtProp, 0, 0);
                                if( !FAILED(hr3) && vtProp.bstrVal != NULL )
                                {
                                    std::string dataDir = W2A(vtProp.bstrVal);
                                    if( instanceNameToInfoMap.find(instanceName) == instanceNameToInfoMap.end() )
                                    {
                                        MSSQLInstanceDiscoveryInfo instanceDiscInfo;
                                        instanceDiscInfo.m_dataDir = dataDir;
                                        instanceNameToInfoMap.insert( std::make_pair(instanceName, instanceDiscInfo) );				
                                    }
                                    else
                                    {
                                        instanceNameToInfoMap.find(instanceName)->second.m_dataDir = dataDir;
                                    }
                                    DebugPrintf(SV_LOG_DEBUG, "DataPath  is %s \n", dataDir.c_str());
                                }
                                VariantClear(&vtProp);
                            }
                            else if( strcmpi( PropertyName.c_str(), "DUMPDIR") == 0 )
                            {
                                VARIANT vtProp;
                                HRESULT hr3 = pclsObj->Get(L"PropertyStrValue", 0, &vtProp, 0, 0);
                                if( !FAILED(hr3) && vtProp.bstrVal != NULL )
                                {
                                    std::string dumpDir = W2A(vtProp.bstrVal);
                                    if( instanceNameToInfoMap.find(instanceName) == instanceNameToInfoMap.end() )
                                    {
                                        MSSQLInstanceDiscoveryInfo instanceDiscInfo;
                                        instanceDiscInfo.m_dumpDir = dumpDir;
                                        instanceNameToInfoMap.insert( std::make_pair(instanceName, instanceDiscInfo) );				
                                    }
                                    else
                                    {
                                        instanceNameToInfoMap.find(instanceName)->second.m_dumpDir = dumpDir;
                                    }
                                    DebugPrintf(SV_LOG_DEBUG, "DUMPDIR is %s \n", dumpDir.c_str());
                                }
                                VariantClear(&vtProp);
                            }
                            else if( strcmpi( PropertyName.c_str(), "INSTALLPATH") == 0 )
                            {
                                VARIANT vtProp;
                                HRESULT hr3 = pclsObj->Get(L"PropertyStrValue", 0, &vtProp, 0, 0);
                                if( !FAILED(hr3) && vtProp.bstrVal != NULL )
                                {
                                    std::string installPath = W2A(vtProp.bstrVal);
                                    if( instanceNameToInfoMap.find(instanceName) == instanceNameToInfoMap.end() )
                                    {
                                        MSSQLInstanceDiscoveryInfo instanceDiscInfo;
                                        instanceDiscInfo.m_installPath = installPath;
                                        instanceNameToInfoMap.insert( std::make_pair(instanceName, instanceDiscInfo) );				
                                    }
                                    else
                                    {
                                        instanceNameToInfoMap.find(instanceName)->second.m_installPath = installPath;
                                    }
                                    DebugPrintf(SV_LOG_DEBUG, "INSTALLPATH is %s \n", installPath.c_str());
                                }
                                VariantClear(&vtProp);
                            }
                            else if( strcmpi( PropertyName.c_str(), "STARTUPPARAMETERS") == 0 )
                            {
                                VARIANT vtProp;
                                HRESULT hr3 = pclsObj->Get(L"PropertyStrValue", 0, &vtProp, 0, 0);
                                if( !FAILED(hr3) && vtProp.bstrVal != NULL )
                                {
                                    std::string startuppatams = W2A(vtProp.bstrVal);
                                    /*=========
                                    PropertyStrValue value looks like "-d master.mdf path; -e ErrorLog path; -l mastlog.ldf path".
                                    But we want only ErrorLog path, so we need to subSting the 'PropertyStrValue' value...
                                    ===========*/
                                    size_t strtPos,endPos;
                                    strtPos = startuppatams.find_first_of(';');
                                    strtPos += 3;
                                    endPos = startuppatams.find_last_of(';');
                                    std::string errorLogPath = startuppatams.substr(strtPos,endPos-strtPos);
                                    if( instanceNameToInfoMap.find(instanceName) == instanceNameToInfoMap.end() )
                                    {
                                        MSSQLInstanceDiscoveryInfo instanceDiscInfo;
                                        instanceDiscInfo.m_errorLogPath = errorLogPath;
                                        instanceNameToInfoMap.insert( std::make_pair(instanceName, instanceDiscInfo) );				
                                    }
                                    else
                                    {
                                        instanceNameToInfoMap.find(instanceName)->second.m_errorLogPath = errorLogPath;
                                    }
                                    DebugPrintf(SV_LOG_DEBUG, "Error Log Path: is %s \n", errorLogPath.c_str());
                                }
                                VariantClear(&vtProp);
                            }
                            else if( strcmpi( PropertyName.c_str(), "VSNAME") == 0 )
                            {
                                VARIANT vtProp;
                                HRESULT hr3 = pclsObj->Get(L"PropertyStrValue", 0, &vtProp, 0, 0);
                                if( !FAILED(hr3) && vtProp.bstrVal != NULL )
                                {
                                    std::string virtualSrvrName = W2A(vtProp.bstrVal);
                                    if( instanceNameToInfoMap.find(instanceName) == instanceNameToInfoMap.end() )
                                    {
                                        MSSQLInstanceDiscoveryInfo instanceDiscInfo;
                                        instanceDiscInfo.m_virtualSrvrName = virtualSrvrName;
                                        instanceNameToInfoMap.insert( std::make_pair(instanceName, instanceDiscInfo) );				
                                    }
                                    else
                                    {
                                        instanceNameToInfoMap.find(instanceName)->second.m_virtualSrvrName = virtualSrvrName;
                                    }
                                    DebugPrintf(SV_LOG_DEBUG, "VSNAME is %s \n", virtualSrvrName.c_str());
                                }
                                VariantClear(&vtProp);
                            }
                            else if( strcmpi( PropertyName.c_str(), "CLUSTERED") == 0 )
                            {
                                VARIANT vtProp;
                                HRESULT hr3 = pclsObj->Get(L"PropertyNumValue", 0, &vtProp, 0, 0);
                                if( !FAILED(hr3) )
                                {
                                    bool isClustered = vtProp.iVal ;
                                    if( instanceNameToInfoMap.find(instanceName) == instanceNameToInfoMap.end() )
                                    {
                                        MSSQLInstanceDiscoveryInfo instanceDiscInfo;
                                        instanceDiscInfo.m_isClustered = isClustered;
                                        instanceNameToInfoMap.insert( std::make_pair(instanceName, instanceDiscInfo) );				
                                    }
                                    else
                                    {
                                        instanceNameToInfoMap.find(instanceName)->second.m_isClustered = isClustered;
                                    }
                                    DebugPrintf(SV_LOG_DEBUG, "The SQL Instance Clustered Property is %d\n", isClustered) ;
                                }
                                VariantClear(&vtProp);							
                            }
                        }
                        VariantClear(&vtProp);
                    }
                }
                pclsObj->Release();
            }
            else
            {
				DebugPrintf(SV_LOG_WARNING, "Failed in IWbemClassObject next. hr = %08X return = %ld\n",hr, uReturn);
				break;
            }
        }
        pEnumerator->Release();
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to execute the query %s\n", query.str().c_str()) ;
    }
    if( discInfoMapSize == instanceNameToInfoMap.size() )
    {
        DebugPrintf(SV_LOG_DEBUG, "No sql Instance Informaiton is found\n") ;
    }
    else
    {
        bRet = SVS_OK ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}