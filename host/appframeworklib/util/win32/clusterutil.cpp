#include "clusterutil.h"
#include <stdio.h>
#ifndef _WINDOWS_
#include <Windows.h>
#endif
#include <clusapi.h>
#include <atlconv.h>
#include <list>
#include <map>
#include <sstream>
#include <boost/shared_ptr.hpp>
#include "registry.h"
#include "wmi/wmi.h"
#include "host.h"
#include "registry.h"
#include "service.h"
#include "inmsafeint.h"
#include "inmageex.h"
#define INM_BUFFER_LEN 1024
#define INM_TYPE_BUFFER_LEN 1024
#include <boost/lexical_cast.hpp>
bool isClusterNode()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRet = false ;
	NODECLUSTERSTATUS status ;
	if( getClusterStatus(status) == SVS_OK )
	{
		if( status == CLUSTER_STATE_RUNNING )
		{
			bRet = true ;
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet ;
}

bool isClusterStateOk()
{
    std::string clusterName ;
    if( !getClusterName(clusterName) )
    {
        DebugPrintf(SV_LOG_DEBUG, "This node is not a cluster node\n") ;
        return true ;
    }
    InmServiceStatus ClusSvcStatus ;
    getServiceStatus("ClusSvc", ClusSvcStatus);
    if( ClusSvcStatus == INM_SVCSTAT_NOTINSTALLED )
    {
        DebugPrintf(SV_LOG_DEBUG, "Cluster service is not installed..\n") ;
        return true ;
    }
    if(ClusSvcStatus !=  INM_SVCSTAT_RUNNING)
    {
        DebugPrintf(SV_LOG_DEBUG,"ClusSvc services is not running\n");
        return false;
    }
    if( isClusterNode() )
    {
        DebugPrintf(SV_LOG_DEBUG, "This node is part of cluster %s\n", clusterName.c_str()) ;
        return true ;
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "Cluster name is %s but current node not belongs cluster now\n", clusterName.c_str()) ;
        return false ;
    }
}

SVERROR getClusterStatus( NODECLUSTERSTATUS& status, std::string nodeName )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR returnValue = SVS_OK;
	LPCWSTR lpszInmNodeName = NULL;
	USES_CONVERSION;
	DWORD state;
	if( nodeName.empty() == false )
	{
		lpszInmNodeName = A2W(nodeName.c_str());
	}
	if( GetNodeClusterState(lpszInmNodeName, &state) != ERROR_SUCCESS )
	{
        DWORD error = GetLastError() ;
        DebugPrintf(SV_LOG_ERROR, "GetNodeClusterState failed %ld\n ", error);
        throw "GetNodeClusterState Failed";
		returnValue = SVS_FALSE;
	}
	if(state == ClusterStateNotInstalled)
	{
		status = CLUSTER_STATE_NOT_INSTALLED;
		DebugPrintf(SV_LOG_DEBUG, "Node Cluster State: The Cluster service is not installed on the node.\n ");
	}
	else if(state == ClusterStateNotConfigured)
	{
		status = CLUSTER_STATE_NOT_CONFIGURED;
		DebugPrintf(SV_LOG_DEBUG, "Node Cluster State: The Cluster service is installed on the node but has not yet been configured.\n ");
	}
	else if(state == ClusterStateNotRunning)
	{
		status = CLUSTER_STATE_NOT_RUNNING;
		DebugPrintf(SV_LOG_DEBUG, "Node Cluster State: The Cluster service is installed and configured on the node but is not currently running.\n ");
	}
	else
	{
		status = CLUSTER_STATE_RUNNING;
		DebugPrintf(SV_LOG_DEBUG, "Node Cluster State: The Cluster service is installed, configured, and running on the node.\n ");
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return returnValue;
}
static SVERROR ClusterResource_Control( HRESOURCE hResource, DWORD dwControlCode, LPVOID& lpOutBuffer, DWORD& cbBuffSize, DWORD& bytesReturned )
{
	//DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retError = SVS_FALSE;
	DWORD dwClusterResourceControlResult = ERROR_SUCCESS;
    lpOutBuffer = NULL ;
	lpOutBuffer = LocalAlloc( LPTR, cbBuffSize );
	if( lpOutBuffer == NULL )
	{
		DebugPrintf( SV_LOG_ERROR, " Failed to allocate local memory to Buffer. Error Code: %d \n", GetLastError() );
	}
    else
    {
	    dwClusterResourceControlResult = ClusterResourceControl( hResource,
		    NULL, 
		    dwControlCode, 
		    NULL, 
		    NULL, 
		    lpOutBuffer,
		    cbBuffSize,
		    &bytesReturned
		    );
	    if( dwClusterResourceControlResult == ERROR_MORE_DATA )
	    {
            if( lpOutBuffer )
            {
		        LocalFree( lpOutBuffer );
                lpOutBuffer = NULL ;
            }
		    cbBuffSize = bytesReturned;
		    lpOutBuffer = LocalAlloc( LPTR, cbBuffSize );
		    DebugPrintf( SV_LOG_DEBUG, " Error More Data. Increasing Size of Buffer to %d. \n ", bytesReturned );
		    if( lpOutBuffer == NULL )
		    {
			    DebugPrintf( SV_LOG_ERROR, "LocalAlloc Failed. Error Code: %d \n", GetLastError() );
		    }
            else
            {
		        dwClusterResourceControlResult = ClusterResourceControl( hResource,
			        NULL, 
			        dwControlCode, 
			        NULL, 
			        NULL, 
			        lpOutBuffer,
			        cbBuffSize,
			        &bytesReturned
			        );
            }
	    }
    }
	if( dwClusterResourceControlResult == ERROR_SUCCESS )
	{
		retError = SVS_OK;
	}
    else
    {
		DebugPrintf( SV_LOG_ERROR, " ClusterResourceControl() Failed.Error Code: %d \n", GetLastError() );
    }
	//DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retError ;
}

/* 
clusterName --> [__in__] name of the cluster to open. or can pass empty string
resourceTypeName --> [__in__] The Type of resource of which to get the all the resource name ..
resourceNameList --> [__Out__] contains the all the vailable resource names resulting from the operation.
*/

SVERROR getResourcesByType(const std::string& clusterName, const std::string& resourceTypeName, std::list<std::string>& resourceNameList ) 
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retError = SVS_OK ;
	HCLUSTER hCluster = NULL;
	HCLUSENUM hClusterEnum = NULL;
	HRESOURCE hResource = NULL;
	WCHAR* lpwClusterName = NULL;
	USES_CONVERSION;
	if( clusterName.empty() == false )
	{
		lpwClusterName = A2W(clusterName.c_str()) ;
	}
	hCluster = OpenCluster( lpwClusterName );
	if( hCluster == NULL )
	{
		DebugPrintf( SV_LOG_ERROR, "OpenCluster Failed.Error Code: %d \n", GetLastError() );
		retError = SVS_FALSE;
	}
	else
	{
		hClusterEnum = ClusterOpenEnum( hCluster, CLUSTER_ENUM_RESOURCE );
		if( hClusterEnum == NULL )
		{
			DebugPrintf( SV_LOG_ERROR, "ClusterOpenEnum Failed.Error Code: %d \n", GetLastError() );
			retError = SVS_FALSE;
		}
		else
		{
			DWORD dwClusterEnumIndex = 0;
			DWORD dwClusterEnumResult = ERROR_SUCCESS;
			DWORD dwObjectType; // can be a type of resource/node/group..
			WCHAR lpwObjectName[INM_TYPE_BUFFER_LEN];
			DWORD lpcchName = INM_TYPE_BUFFER_LEN; 

			while( dwClusterEnumResult == ERROR_SUCCESS )
			{
				lpcchName = 1024;
				dwClusterEnumResult = ClusterEnum( hClusterEnum, dwClusterEnumIndex, &dwObjectType, lpwObjectName, &lpcchName );
				if( dwClusterEnumResult == ERROR_NO_MORE_ITEMS )
				{
					DebugPrintf( SV_LOG_DEBUG, " No More Objects in the Enum.\n " );
				}
				else if( dwClusterEnumResult == ERROR_MORE_DATA )
				{
					DebugPrintf( SV_LOG_ERROR, " Error More Data.\n " );
				}
				else if( dwClusterEnumResult == ERROR_SUCCESS )
				{
					if( dwObjectType == CLUSTER_ENUM_RESOURCE )
					{
						hResource = NULL;
						hResource = OpenClusterResource( hCluster, lpwObjectName );
						if( hResource == NULL )
						{
							DebugPrintf( SV_LOG_ERROR, "OpenClusterResource Failed. Name: %s .Error Code: %d \n ", lpwObjectName ,GetLastError() );
						}
						else
						{
							LPVOID lpszType1;
							DWORD cbBuffSize1 = INM_TYPE_BUFFER_LEN;
							DWORD bytesReturned1 ;
							SVERROR retControlResource1 = SVS_OK;
							retControlResource1 = ClusterResource_Control( hResource, 
								CLUSCTL_RESOURCE_GET_RESOURCE_TYPE, 
								lpszType1, 
								cbBuffSize1, 
								bytesReturned1 );
							if( retControlResource1 != SVS_OK )
							{
								DebugPrintf( SV_LOG_ERROR, " controlControlResource() Failed \n" );
								retError = SVS_FALSE;
							    CloseClusterResource( hResource );
                                hResource = NULL ;
								break;
							}
							USES_CONVERSION;
							if( lpszType1 != NULL &&  strcmpi( W2A( (WCHAR*)lpszType1 ), resourceTypeName.c_str() ) == 0  )
							{
								resourceNameList.push_back( W2A(lpwObjectName) );
							}
							LocalFree( lpszType1 );
                            if( hResource )
                            {
							    CloseClusterResource( hResource );
                                hResource = NULL ;
                            }
						}
					}
				}
				else
				{
					DebugPrintf( SV_LOG_ERROR, " ClusterEnum() failed. Error Code: %d \n", GetLastError() );
				}
				dwClusterEnumIndex++;
			}
            if( hClusterEnum )
            {
			    ClusterCloseEnum( hClusterEnum );
                hClusterEnum = NULL ;
            }
		}
		CloseCluster( hCluster );
	}
	if(lpwClusterName != NULL)
	{
		free(lpwClusterName);
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retError ;
}

/* 
clusterName --> [__in__] name of the cluster to open. or can pass empty string
resourceName --> [__in__] The Name of resource of which private properties to find ..
propertyKeyList --> [__in__] The list of property names to find from the given resources .
propertyMap --> [__Out__] contains the property name value pairs.
*/

SVERROR getResourcePropertiesMap( std::string clusterName, std::string resourceName, std::list<std::string>& propertyKeyList, std::map< std::string, std::string>&  propertyMap )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retError = SVS_OK ;
	HCLUSTER hCluster = NULL;
	HCLUSENUM hClusterEnum = NULL;
	HRESOURCE hResource = NULL;
	WCHAR* lpwClusterName = NULL;
	USES_CONVERSION;
	if( clusterName.empty() == false )
		lpwClusterName = A2W(clusterName.c_str()) ;

	hCluster = OpenCluster( lpwClusterName );
	if( hCluster == NULL )
	{
		DebugPrintf( SV_LOG_ERROR, " Failed to open the cluster.Error Code: %d \n", GetLastError() );
		retError = SVS_FALSE;
	}
	else
	{
		hResource = NULL;
		LPCWSTR lpcresourceName = A2W( resourceName.c_str());
		hResource = OpenClusterResource( hCluster, lpcresourceName );	
		if( hResource == NULL )
		{
			DebugPrintf( SV_LOG_ERROR, " OpenClusterResource Failed for %s, Error %ld\n", resourceName.c_str(),GetLastError() );
			retError = SVS_FALSE;
		}
		else
		{
			LPVOID lpszType = NULL ;
			DWORD cbBuffSize = INM_TYPE_BUFFER_LEN;
			DWORD bytesReturned ;
			SVERROR retControlResource = SVS_OK;
			retControlResource = ClusterResource_Control( hResource, 
				CLUSCTL_RESOURCE_GET_PRIVATE_PROPERTIES,
				lpszType, 
				cbBuffSize, 
				bytesReturned );

			if( retControlResource != SVS_OK )
			{
				DebugPrintf( SV_LOG_ERROR, " ClusterResource_Control Failed \n" );
				retError = SVS_FALSE;
			}
			else
			{
				LPWSTR pszPropertyValue = NULL;
				std::string szpropertyName, szpropertyValue;
				LPCWSTR lpcpropertyName ;
				std::list<std::string>::iterator propertyKeyListIter = propertyKeyList.begin();
				while( propertyKeyListIter != propertyKeyList.end() )
				{
					szpropertyName = *(propertyKeyListIter);
					lpcpropertyName = A2W(szpropertyName.c_str());
					szpropertyValue.clear();
					if( ResUtilFindSzProperty( lpszType, cbBuffSize, lpcpropertyName, &pszPropertyValue) == ERROR_SUCCESS  )
                    {
                        szpropertyValue = W2A( pszPropertyValue);
					    propertyMap.insert( std::make_pair(szpropertyName, szpropertyValue) );
					    if( pszPropertyValue != NULL )
                        {
                            LocalFree (pszPropertyValue) ;
                            pszPropertyValue = NULL ;
                        }
                    }
					propertyKeyListIter++;
				}
			}
			LocalFree( lpszType );		
			CloseClusterResource( hResource );
		}
		CloseCluster( hCluster );
	}
	if(lpwClusterName != NULL)
	{
		free(lpwClusterName);
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retError ;
}

/* 
dependentResourceName --> [__in__] name of the resource for which to find the dependencies.
dependedResourceType --> [__in__] The Type of dependency names to to find..
dependedResourceNameList --> [__Out__] contains the list of depended resource name list of give type.
*/

SVERROR getDependedResorceNameListOfType( std::string dependentResourceName, std::string dependedResourceType, std::list<std::string>& dependedResourceNameList )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retError = SVS_OK ;
	HRESOURCE hResource = NULL;
	USES_CONVERSION;
	LPCWSTR lpszResourceName = A2W(dependentResourceName.c_str());
	LPCWSTR lpszResourceType = A2W(dependedResourceType.c_str());
	hResource = ResUtilGetResourceNameDependency( lpszResourceName, lpszResourceType );
	if( hResource == NULL )
	{
		DebugPrintf(SV_LOG_ERROR, "The resource %s does not have any resourcedependency of type %s \n", dependentResourceName.c_str(), dependedResourceType.c_str() );
		dependedResourceNameList.clear();
	}
	else
	{
		LPVOID lpszName;
		DWORD cbBuffSize = INM_TYPE_BUFFER_LEN;
		DWORD bytesReturned ;
		SVERROR retControlResource = SVS_OK;
		retControlResource = ClusterResource_Control( hResource, 
			CLUSCTL_RESOURCE_GET_NAME,
			lpszName, 
			cbBuffSize, 
			bytesReturned );
		if( retControlResource != SVS_OK ) 
		{
			retError = SVS_FALSE ;
		}
		else
		{
			dependedResourceNameList.push_back( W2A((WCHAR*)lpszName) );
		}
		CloseClusterResource(hResource);
		if(lpszName != NULL)
		{
			LocalFree(lpszName);
		}	
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retError ;
}

SVERROR getClusterIpAddress(const std::string& clustername, std::string& clusterIpAddress )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retError = SVS_OK ;
	if( isClusterNode() == false )
	{
		retError = SVS_FALSE ;
	}
	clusterIpAddress.clear();
	std::list<std::string> propertyKeyList;
	propertyKeyList.push_back("Address");
	std::map<std::string, std::string> propertyMap;
	retError = getResourcePropertiesMap(clustername, "Cluster Ip Address", propertyKeyList, propertyMap);
	if( retError == SVS_OK && propertyMap.empty() == false )
	{
		std::map<std::string, std::string>::iterator propertyMapIter = propertyMap.find("Address");
		clusterIpAddress = propertyMapIter->second ;
		DebugPrintf(SV_LOG_DEBUG, "Cluster Ip Address is %s\n", clusterIpAddress.c_str() ) ;
	}
	else 
	{
		DebugPrintf( SV_LOG_ERROR, " Failed to Find The cluster Ip Address to which this local machine belongs\n" );
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retError ;
}

SVERROR getResourceState( std::string clusterName, std::string resourceName, CLUSTER_RESOURCE_STATE& state, std::string& groupName, std::string& nodeName )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retStatus = SVS_OK ;
	HCLUSTER hCluster = NULL;
	HRESOURCE hResource = NULL;
	WCHAR* lpwClusterName = NULL;
	USES_CONVERSION;
	if( clusterName.empty() == false )
		lpwClusterName = A2W(clusterName.c_str()) ;

	hCluster = OpenCluster( lpwClusterName );
	if( hCluster == NULL )
	{
		DebugPrintf( SV_LOG_ERROR, " Failed to open the cluster.Error Code: %d \n", GetLastError() );
		retStatus = SVS_FALSE;
	}
	else
	{
		hResource = NULL;
		LPCWSTR lpcresourceName = A2W( resourceName.c_str());
		hResource = OpenClusterResource( hCluster, lpcresourceName );	
		if( hResource == NULL )
		{
			DebugPrintf( SV_LOG_ERROR, " OpenClusterResource Failed for %s. Error %ld\n", resourceName.c_str(), GetLastError() );
			retStatus = SVS_FALSE;
		}
		else
		{
			WCHAR lpznodeName[INM_TYPE_BUFFER_LEN], lpzgroupName[INM_TYPE_BUFFER_LEN];

			DWORD cbNodeNameBuffSize = INM_TYPE_BUFFER_LEN, cbGroupNameBuffSize = INM_TYPE_BUFFER_LEN ;
			state = GetClusterResourceState( hResource, lpznodeName, &cbNodeNameBuffSize, lpzgroupName, &cbGroupNameBuffSize );
			if(lpzgroupName != NULL && lpznodeName != NULL)
			{
				groupName = W2A((WCHAR*)lpzgroupName) ;
				nodeName = W2A((WCHAR*)lpznodeName);
			}
			CloseClusterResource(hResource);
		}
		CloseCluster(hCluster);
	}
	if(lpwClusterName != NULL)
	{
		free(lpwClusterName);
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus;
}

SVERROR getDependencyList( std::string query, std::list<std::string>& depndentList )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retStatus = SVS_OK ;
	USES_CONVERSION ;
	boost::shared_ptr<WMIConnector> wmiConnectorPtr ;
	if( wmiConnectorPtr.get() == NULL )
	{
		wmiConnectorPtr.reset( new WMIConnector() ) ;
	}
	if( wmiConnectorPtr->ConnectToServer( Host::GetInstance().GetHostName(), "root\\MSCluster") != SVS_OK )
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to connect to root\\MSCluster on localhost\n") ;
	}
	else
	{
		IEnumWbemClassObject* pEnumerator = NULL ;
		DebugPrintf(SV_LOG_DEBUG, "Query is %s\n", query.c_str());
		if( wmiConnectorPtr->execQuery( query, &pEnumerator ) == SVS_OK )
		{
			IWbemClassObject *pclsObj = NULL ;
			ULONG uReturn = 0;
			if( pEnumerator == NULL )
			{
				DebugPrintf(SV_LOG_ERROR, "No Output from the Query..\n") ;
			}
			while (pEnumerator)
			{
				HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
				if( SUCCEEDED(hr) )
				{
                    if(0 == uReturn)
				    {
					    DebugPrintf(SV_LOG_DEBUG, "BREAK\n");
					    break;
				    }
				    VARIANT vtProp;
				    hr = pclsObj->Get(L"Dependent", 0, &vtProp, 0, 0);
				    if( SUCCEEDED(hr) )
				    {
					    std::string dependent = W2A(vtProp.bstrVal) ;
					    DebugPrintf(SV_LOG_DEBUG, "Dependent is %s\n", dependent.c_str()) ;	
					    //VariantClear(&vtProp);
					    size_t index = dependent.find_first_of("\"");

					    std::string subStr = dependent.substr( index+1, (dependent.size()-(index + 2)) );
					    std::list<std::string> propKeyList;
					    propKeyList.push_back("Address");
					    std::map<std::string, std::string> propertyMap;
					    getResourcePropertiesMap("", subStr, propKeyList, propertyMap); 
					    if( propertyMap.empty() == false )
					    {
						    subStr = propertyMap.begin()->second;							
					    }
					    depndentList.push_back(subStr);
				    }
				    else
				    {
					    DebugPrintf(SV_LOG_ERROR, "Get Failed\n") ;
				    }
					VariantClear(&vtProp);
					pclsObj->Release();
                }
			}
			if( pEnumerator != NULL )
			{
				pEnumerator->Release();
			}
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to run the WMI Query. Query is %s\n", query.c_str()) ;
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus;
}
SVERROR getClusterInfo( ClusterInformation& cluster_Info)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retStatus = SVS_OK ;
	cluster_Info.m_nodeName = Host::GetInstance().GetHostName() ;
	if( !getClusterName( cluster_Info.m_clusterName ))
	{
		retStatus = SVS_FALSE ;
	}
	if( !GetNameId( cluster_Info.m_clusterID ) )
	{
		retStatus = SVS_FALSE ;
	}
    if( isClusterNode() )
    {
	    std::list<std::string> networksResourceNames;
	    if( getResourcesByType("", "Network Name", networksResourceNames) == SVS_OK )
	    {
		    std::list<std::string>::iterator networksResourceNamesIter = networksResourceNames.begin();
		    while( networksResourceNamesIter != networksResourceNames.end() )
		    {
			    std::list<std::string> propertyKeyList;
			    propertyKeyList.push_back("Name");
			    std::map<std::string, std::string> propertyMap;
			    if( getResourcePropertiesMap("", *networksResourceNamesIter, propertyKeyList, propertyMap) == SVS_OK && 
				    propertyMap.empty() == false )
			    {
				    std::map<std::string, std::string>::iterator propertyMapIter = propertyMap.find("Name");
				    std::string netWorkName = propertyMapIter->second ;
				    std::stringstream query;
				    NetworkResourceInfo nwresource_Info ;
				    query << "SELECT * FROM MSCluster_ResourceToDependentResource where Antecedent = 'MSCluster_Resource.Name=\\'" << *networksResourceNamesIter << "\\''";
				    if( getDependencyList( query.str(), nwresource_Info.m_dependendentIpList ) == SVS_OK )
				    {
					    if( getResourceState( "", *networksResourceNamesIter, nwresource_Info.m_resInfo.m_resourceStatus, nwresource_Info.m_resInfo.m_ownerGroup, nwresource_Info.m_resInfo.m_ownerNodeName ) == SVS_OK )
					    {
						    cluster_Info.m_networkInfoMap.insert( std::make_pair( netWorkName, nwresource_Info ));
					    }
					    else
					    {
						    DebugPrintf(SV_LOG_ERROR, "getResourceState() failed.\n");
					    }
				    }
				    else
				    {
					    DebugPrintf(SV_LOG_ERROR, "getDependencyList() failed.\n");
				    }
			    }
			    networksResourceNamesIter++;
		    }
        }
		if( getClusterQuorumInfo(cluster_Info) != SVS_OK)
		{
			retStatus = SVS_FALSE;
		}
		if( getNodesPresentInCluster("",cluster_Info.m_nodes) != SVS_OK)
		{
			retStatus = SVS_FALSE;
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus;
}


SVERROR getNodesPresentInCluster(const std::string& clusterName, std::list<std::string>& resourceNameList)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retError = SVS_OK ;
	HCLUSTER hCluster = NULL;
	HCLUSENUM hClusterEnum = NULL;
	HNODE hNode = NULL;
	WCHAR* lpwClusterName = NULL;
	USES_CONVERSION;
	if( clusterName.empty() == false )
	{
		lpwClusterName = A2W(clusterName.c_str()) ;
	}
	hCluster = OpenCluster( lpwClusterName );
	if( hCluster == NULL )
	{
		DebugPrintf( SV_LOG_ERROR, "OpenCluster Failed.Error Code: %d \n", GetLastError() );
		retError = SVS_FALSE;
	}
	else
	{
		hClusterEnum = ClusterOpenEnum( hCluster, CLUSTER_ENUM_NODE );
		if( hClusterEnum == NULL )
		{
			DebugPrintf( SV_LOG_ERROR, "ClusterOpenEnum Failed.Error Code: %d \n", GetLastError() );
			retError = SVS_FALSE;
		}
		else
		{
			DWORD dwClusterEnumIndex = 0;
			DWORD dwClusterEnumResult = ERROR_SUCCESS;
			DWORD dwObjectType = CLUSTER_ENUM_NODE; // can be a type of resource/node/group..
			WCHAR lpwObjectName[INM_TYPE_BUFFER_LEN];
			DWORD lpcchName = INM_TYPE_BUFFER_LEN; 

			while( dwClusterEnumResult == ERROR_SUCCESS )
			{
				lpcchName = 1024;
				dwClusterEnumResult = ClusterEnum( hClusterEnum, dwClusterEnumIndex, &dwObjectType, lpwObjectName, &lpcchName );
				if( dwClusterEnumResult == ERROR_NO_MORE_ITEMS )
				{
					DebugPrintf( SV_LOG_DEBUG, " No More Objects in the Enum.\n " );
				}
				else if( dwClusterEnumResult == ERROR_MORE_DATA )
				{
					DebugPrintf( SV_LOG_ERROR, " Error More Data.\n " );
				}
				else if( dwClusterEnumResult == ERROR_SUCCESS )
				{
					if( dwObjectType == CLUSTER_ENUM_NODE )
					{
						hNode = NULL;
						hNode = OpenClusterNode( hCluster, lpwObjectName );
						if( hNode == NULL )
						{
							DebugPrintf( SV_LOG_ERROR, "OpenClusterNode Failed. Name: %s .Error Code: %s \n ", lpwObjectName ,GetLastError() );
						}
						else
						{
							LPVOID lpszType1;
							DWORD cbBuffSize1 = INM_TYPE_BUFFER_LEN;
							DWORD bytesReturned1 ;
							SVERROR retControlResource1 = SVS_OK;
							retControlResource1 = ClusterNode_Control( hNode, 
								CLUSCTL_NODE_GET_RO_COMMON_PROPERTIES, 
								lpszType1, 
								cbBuffSize1, 
								bytesReturned1 );
							if( retControlResource1 != SVS_OK )
							{
								DebugPrintf( SV_LOG_ERROR, " controlControlResource() Failed \n" );
								retError = SVS_FALSE;
								break;
							}
							USES_CONVERSION;
							LPWSTR pszNodeName = NULL;
							if( ERROR_SUCCESS == ResUtilFindSzProperty (lpszType1,bytesReturned1,L"NodeName",&pszNodeName))
							 {
								if(pszNodeName != NULL)
								{
									resourceNameList.push_back( W2A(pszNodeName));	
								}							
							}
							LocalFree( lpszType1 );
							CloseClusterNode( hNode );
						}
					}
				}
				else
				{
					DebugPrintf( SV_LOG_ERROR, " ClusterEnum() failed. Error Code: %d \n", GetLastError() );
				}
				dwClusterEnumIndex++;
			}
			ClusterCloseEnum( hClusterEnum );
		}
		CloseCluster( hCluster );
	}
	if(lpwClusterName != NULL)
	{
		free(lpwClusterName);
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retError ;
}


static SVERROR ClusterNode_Control( HNODE hResource,  DWORD dwControlCode, LPVOID& lpOutBuffer, DWORD& cbBuffSize, DWORD& bytesReturned )
{
	//DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retError = SVS_OK;
	DWORD dwClusterResourceControlResult = ERROR_SUCCESS;
    lpOutBuffer = NULL ;
	lpOutBuffer = LocalAlloc( LPTR, cbBuffSize );
	if( lpOutBuffer == NULL )
	{
		DebugPrintf( SV_LOG_ERROR, " Failed to allocate local memory to Buffer. Error Code: %d \n", GetLastError() );
		retError = SVS_FALSE;
	}
	dwClusterResourceControlResult = ClusterNodeControl( hResource,
		NULL, 
		dwControlCode, 
		NULL, 
		NULL, 
		lpOutBuffer,
		cbBuffSize,
		&bytesReturned
		);
	if( dwClusterResourceControlResult == ERROR_MORE_DATA )
	{
		dwClusterResourceControlResult = ERROR_SUCCESS;
		LocalFree( lpOutBuffer );
        lpOutBuffer = NULL ;
		cbBuffSize = bytesReturned;
		lpOutBuffer = LocalAlloc( LPTR, cbBuffSize );
		DebugPrintf( SV_LOG_DEBUG, " Error More Data. Increasing Size of Buffer to %d. \n ", bytesReturned );
		if( lpOutBuffer == NULL )
		{
			DebugPrintf( SV_LOG_ERROR, "LocalAlloc Failed. Error Code: %d \n", GetLastError() );
			retError = SVS_FALSE;
		}
		dwClusterResourceControlResult = ClusterNodeControl( hResource,
			NULL, 
			dwControlCode, 
			NULL, 
			NULL, 
			lpOutBuffer,
			cbBuffSize,
			&bytesReturned
			);
	}

	if( dwClusterResourceControlResult!= ERROR_SUCCESS )
	{
		DebugPrintf( SV_LOG_ERROR, " ClusterNodeControl() Failed.Error Code: %d \n", GetLastError() );
		retError = SVS_FALSE;
	}
	return retError ;
}

bool GetNameId(std::string& id)
{
    bool bret = false ;
    if( getStringValue("Cluster", "ClusterInstanceID", id) != SVS_OK)
    {
        if( getStringValue("Cluster", "ClusterNameResource", id) == SVS_OK )
        {
            bret = true ;
        }
    }
    else
    {
        bret = true ;
    }
    return bret ;
}

bool getClusterName( std::string& name)
{
    bool bret = false ;
    if( getStringValue("Cluster", "ClusterName", name) == SVS_OK )
    {
        bret = true ;
    }
    return bret ;
}
/*
	Gets the list of resources of specified resourceTypeName from the group to which the resoruceName belongs.
Params:
	[in]	: Any resource name which is used to identify resource group
	[in]	: the type of resources to be filtered
	[out]	: If success then this list will contain the list of resource of specified resoruce type from the same group.
	[in_opt]: Name of the cluster. And its optional.
*/
SVERROR getResourcesByTypeFromGroup(
									const std::string & resourceName,
									const std::string& resoruceTypeName,
									std::list<std::string> & ResourceNameList,
									const std::string& clusterName
									)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	SVERROR ret = SVS_OK;
	USES_CONVERSION;
	HCLUSTER	hClus = NULL;
	HRESOURCE	hRes  = NULL;
	HGROUP		hGrp  = NULL;
	do{
		if(clusterName.empty())
			hClus = OpenCluster(NULL);
		else
			hClus = OpenCluster(A2W(clusterName.c_str()));
		if(NULL == hClus)
		{
			DebugPrintf(SV_LOG_DEBUG,"Failed to open the cluster. Error %d\n",GetLastError());
			ret = SVE_FAIL;
			break;
		}
		
		hRes = OpenClusterResource(hClus,A2W(resourceName.c_str()));
		if(NULL == hRes)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to get handle to the resoruce %s. Error %d\n",
				resourceName.c_str(),
				GetLastError());
			ret = SVE_FAIL;
			break;
		}
		
		wchar_t szGroupName[256];
		DWORD cchGroupName = 256;
		if(GetClusterResourceState(hRes,NULL,NULL,szGroupName,&cchGroupName) ==
			ClusterResourceStateUnknown )
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to get group name of the resoruce %s. Error %d\n",
				resourceName.c_str(),
				GetLastError());
			ret = SVE_FAIL;
			break;
		}

		hGrp = OpenClusterGroup(hClus,szGroupName);
		if(NULL == hGrp)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to open the cluster group %s",W2A(szGroupName));
			ret = SVE_FAIL;
			break;
		}
		DWORD dwType = CLUSTER_GROUP_ENUM_CONTAINS;
		HGROUPENUM hGrpEnum = ClusterGroupOpenEnum(hGrp,dwType);
		if(NULL == hGrpEnum)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to enumerate group %s\n.",W2A(szGroupName));
			ret = SVE_FAIL;
			break;
		}
		DWORD cchAllocName = 256;
		DWORD cbAllocName = cchAllocName * sizeof(WCHAR);
		LPWSTR lpszResourceName = (LPWSTR)LocalAlloc(LPTR,cbAllocName);
		DWORD dwEnumCount = ClusterGroupGetEnumCount(hGrpEnum);
		DWORD dwResult = ERROR_SUCCESS;
		for(DWORD dwIndex=0; (dwIndex < dwEnumCount) && (ERROR_SUCCESS == dwResult); dwIndex++)
		{
			cchAllocName = cbAllocName/sizeof(WCHAR);
			DWORD dwResult = ClusterGroupEnum(
				hGrpEnum,
				dwIndex,
				&dwType,
				lpszResourceName,
				&cchAllocName);
			if(dwResult == ERROR_MORE_DATA)
			{
				LocalFree(lpszResourceName);
				cchAllocName++; //For null character.
				cbAllocName = cchAllocName*sizeof(WCHAR);
				lpszResourceName = (LPWSTR)LocalAlloc(LPTR,cbAllocName);
				DWORD dwResult = ClusterGroupEnum(
				hGrpEnum,
				dwIndex,
				&dwType,
				lpszResourceName,
				&cchAllocName);
				if(ERROR_SUCCESS == dwResult)
				{
					if(ResourceTypeEqual(lpszResourceName,resoruceTypeName,hClus))
						ResourceNameList.push_back(W2A(lpszResourceName));
				}
				else
				{
					DebugPrintf(SV_LOG_DEBUG,"ClusterGroupEnum failure. Error %d\n",dwResult);
					ret = SVE_FAIL;
				}
			}
			else if(ERROR_SUCCESS == dwResult)
			{
				if(ResourceTypeEqual(lpszResourceName,resoruceTypeName,hClus))
						ResourceNameList.push_back(W2A(lpszResourceName));
			}
			else if(ERROR_NO_MORE_ITEMS != dwResult)
			{
				DebugPrintf(SV_LOG_DEBUG,"ClusterGroupEnum failure. Error %d\n",dwResult);
				ret = SVE_FAIL;
			}
		}
		LocalFree(lpszResourceName);
		ClusterGroupCloseEnum(hGrpEnum);
	} while(false);
	
	if(hGrp)
		CloseClusterGroup(hGrp);
	if(hRes)
		CloseClusterResource(hRes);
	if(hClus)
		CloseCluster(hClus);
	
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return ret;
}
/*
	Verifies sepcified resoruce is of speciried resource-type or not.
Parama:
	[in]	: Name of the resoruce
	[in]	: Resource type
	[in_opt]: Cluster Handle
Return Value:
	If the specified resource is of specifid type then the return value is TRUE, otherwise FALASE.
*/
BOOL ResourceTypeEqual(const std::wstring & resourceName, const std::string & resourceType, HCLUSTER hClus)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	BOOL bEqual = FALSE, bCloseClus = FALSE;
	USES_CONVERSION;
	if(NULL == hClus)
	{
		hClus = OpenCluster(NULL);
		if(NULL == hClus)
		{
			DebugPrintf(SV_LOG_DEBUG,"Failed to open cluster. Error %d\n",GetLastError());
			return bEqual;
		}
		bCloseClus = TRUE;
	}
	
	HRESOURCE hRes = OpenClusterResource(hClus,resourceName.c_str());
	if(hRes)
	{
		bEqual = ResUtilResourceTypesEqual(A2W(resourceType.c_str()),hRes);
		CloseClusterResource(hRes);
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG,"Failed to open cluster resoruce. Error %d\n",GetLastError());
	}

	if(bCloseClus)
		CloseCluster(hClus);
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return bEqual;
}


/*
 Description:
 Helper function to execute the resource control code. Caller is responsible for freeing the memory allocated to lpOutBuff using LocalFree() on success.

 Parameters:
 [In]	  hRes
 [In]	  dwControlCode	-> Cluster resoruce control code
 [Out]	  lpOutBuff		-> Resulting out buffer for the control code. User need to clear the buffer on success
 [Out]	  bytesReturned	-> size of the resulting buffer in bytes
 [Opt In] lpInBuff		-> Input buffer to the cluster control code
 [Opt In] cbhInBuff		-> Size of input buffer in bytes. this can be ignored if lpInBuff is NULL

 Return Values:
 ERROR_SUCCESS  on success, otherwise a win 32 error code.
*/
DWORD ClusUtil_ClusterResourceControl(
									  HRESOURCE hRes,
									  const DWORD dwControlCode,
									  LPVOID *lpOutBuff,
									  DWORD & bytesReturned,
									  LPVOID lpInBuff,
									  DWORD cbInBuff
									  )
{
	DWORD dwRet = ERROR_SUCCESS,
		  cbOutBufferSize= MAX_PATH;
	if(NULL == hRes)
		return ERROR_INVALID_HANDLE;

	*lpOutBuff = (LPVOID)LocalAlloc(LPTR,cbOutBufferSize);
	if(NULL == *lpOutBuff)
	{
		DebugPrintf(SV_LOG_FATAL,"Error. Out of memory\n");
		return ERROR_OUTOFMEMORY;
	}

	dwRet = ClusterResourceControl(hRes,
									NULL,
									dwControlCode,
									lpInBuff,
									cbInBuff,
									*lpOutBuff,
									cbOutBufferSize,
									&bytesReturned);
	if(ERROR_MORE_DATA == dwRet)
	{
		LocalFree(*lpOutBuff);
		cbOutBufferSize = bytesReturned;
		*lpOutBuff = (LPVOID)LocalAlloc(LPTR,cbOutBufferSize);
		if(NULL == *lpOutBuff)
		{
			DebugPrintf(SV_LOG_FATAL,"Error: Out of memory\n");
			return ERROR_OUTOFMEMORY;
		}
		dwRet = ClusterResourceControl(hRes,
									NULL,
									dwControlCode,
									lpInBuff,
									cbInBuff,
									*lpOutBuff,
									cbOutBufferSize,
									&bytesReturned);
		if(ERROR_SUCCESS != dwRet)
		{
			LocalFree(*lpOutBuff);
			bytesReturned = 0;
			*lpOutBuff = NULL;
			DebugPrintf(SV_LOG_ERROR,"ClusterResourceControl filed. Error %d.\n",dwRet);
		}
	}
	else if(ERROR_SUCCESS != dwRet)
	{
		LocalFree(*lpOutBuff);
		bytesReturned = 0;
		*lpOutBuff = NULL;
		DebugPrintf(SV_LOG_ERROR,"ClusterResourceControl filed. Error %d.\n",dwRet);
	}
	return dwRet;
}
/*
Description:
  Enumarates the specified cluster group and gets the specified content type objects from in the group.
  Contents include resoruces nodes or both.
Params:
  [In     ] hCluster    -> A valid handle to the cluster.
  [In     ] grpName     -> A valid group name in the cluster.
  [Out    ] grpContents -> On success, it will be filled with specified content type(s)
  [Opt Out] dwType      -> Type of contents of a groups. Allowed options are:
								CLUSTER_GROUP_ENUM_ALL		3
								CLUSTER_GROUP_ENUM_NODE		2
								CLUSTER_GROUP_ENUM_CONTAINS	1
*/
DWORD GetClusterGroupContents(HCLUSTER hCluster,
							  const std::wstring & grpName,
							  std::vector<std::wstring>& grpContents,
							  DWORD dwType
							  )
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	DWORD dwRet = ERROR_SUCCESS;
	if(NULL == hCluster)
		return ERROR_INVALID_HANDLE;
	if(grpName.empty())
		return ERROR_INVALID_PARAMETER;

	HGROUP hGroup = OpenClusterGroup(hCluster,grpName.c_str());
	if(NULL == hGroup)
	{
		dwRet = GetLastError();
		DebugPrintf(SV_LOG_ERROR,"Failed to open cluster group. Error %d\n",dwRet);
		DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
		return dwRet;
	}

	HGROUPENUM hGrpEnum = ClusterGroupOpenEnum(hGroup,dwType);
	if(NULL == hGrpEnum)
	{
		dwRet = GetLastError();
		DebugPrintf(SV_LOG_ERROR,"Failed to open cluster group enum. Error %d\n",dwRet);
		DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
		CloseClusterGroup(hGroup);
		return dwRet;
	}

	DWORD dwEnumRet = ERROR_SUCCESS,
		  dwEnumType,
		  dwcchResName,
		  dwResNameBuffLen = MAX_PATH;
	PWCHAR szResName = new (std::nothrow) WCHAR[dwResNameBuffLen];
	if(NULL == szResName)
	{
		ClusterGroupCloseEnum(hGrpEnum);
		CloseClusterGroup(hGroup);
		DebugPrintf(SV_LOG_FATAL,"Error Out of memory\n");
		DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
		return ERROR_OUTOFMEMORY;
	}

	for(DWORD dwIndex = 0; ; dwIndex++)
	{
		dwcchResName = dwResNameBuffLen;
		dwEnumRet = ClusterGroupEnum(hGrpEnum,dwIndex,&dwEnumType,szResName,&dwcchResName);
		if(ERROR_MORE_DATA == dwEnumRet)
		{
            INM_SAFE_ARITHMETIC(dwcchResName+=InmSafeInt<int>::Type(1), INMAGE_EX(dwcchResName)) // additional bit is required for null character
			dwResNameBuffLen = dwcchResName;
			delete [] szResName;
			szResName = new (std::nothrow) WCHAR[dwResNameBuffLen];
			if(NULL == szResName)
			{
				ClusterGroupCloseEnum(hGrpEnum);
				CloseClusterGroup(hGroup);
				DebugPrintf(SV_LOG_FATAL,"Error Out of memory\n");
				DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
				return ERROR_OUTOFMEMORY;
			}
			if((dwEnumRet = ClusterGroupEnum(hGrpEnum,dwIndex,&dwEnumType,szResName,&dwcchResName))
				!= ERROR_SUCCESS ) break;
		}
		else if(ERROR_SUCCESS == dwEnumRet)
		{
			if(dwType != CLUSTER_GROUP_ENUM_ALL && dwType != dwEnumType)
				DebugPrintf(SV_LOG_WARNING,"Got Unrequested type of object\n");
			else
				grpContents.push_back(szResName);

		}
		else { break ;}
	}
	delete [] szResName;
	if(ERROR_NO_MORE_ITEMS != dwEnumRet)
	{
		dwRet = dwEnumRet;
	}
	ClusterGroupCloseEnum(hGrpEnum);
	CloseClusterGroup(hGroup);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return dwRet;
}

/*Description:
	connect to the cluster and gets all the cluster object-names of type specified by dwClusEnumType in the cluster.
  
  Prams:
  [In]  hCluster       -> A valid cluster handle
  [In]  dwClusEnumType -> Cluster enumeration type i.e CLUSTER_ENUM type
  [Out] objects	       -> On success, filled with object names of requested type available in the cluster
  
  Return code:
    ERROR_SUCCESS on success, otherwise a win32 error code.
*/
DWORD GetClusterEnumObjects(HCLUSTER hCluster,
							const DWORD dwClusEnumType,
							std::vector<std::wstring> & objects
							)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	DWORD dwRet = ERROR_SUCCESS;
	
	if(NULL == hCluster)
		return ERROR_INVALID_HANDLE;

	HCLUSENUM hObjEnum = NULL;
	if((hObjEnum = ClusterOpenEnum(hCluster,dwClusEnumType)) == NULL)
	{
		dwRet = GetLastError();
		DebugPrintf(SV_LOG_ERROR,"Failed to open cluster enum. error %d\n",dwRet);
		DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
		return dwRet;
	}

	DWORD dwEnumResult = ERROR_SUCCESS;
	DWORD dwEnumType, cchObjName, objNameBuffLen = MAX_PATH;
	PWCHAR wszObjName = new (std::nothrow) WCHAR [objNameBuffLen];
	if(NULL == wszObjName)
	{
		DebugPrintf(SV_LOG_FATAL,"Error Out of memory\n");
		DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
		ClusterCloseEnum(hObjEnum);
		return ERROR_OUTOFMEMORY;
	}

	objects.clear();

	for(DWORD dwIndex = 0; ; dwIndex++)
	{
		cchObjName = objNameBuffLen;
		dwEnumResult = ClusterEnum(hObjEnum,dwIndex,&dwEnumType,wszObjName,&cchObjName);
		if(ERROR_MORE_DATA == dwEnumResult)
		{
            INM_SAFE_ARITHMETIC(cchObjName+=InmSafeInt<int>::Type(1), INMAGE_EX(cchObjName))// additional bit is required for null character
			objNameBuffLen = cchObjName;
			delete [] wszObjName;
			wszObjName = new (std::nothrow) WCHAR[objNameBuffLen];
			if(NULL == wszObjName)
			{
				DebugPrintf(SV_LOG_FATAL,"Error Out of memory\n");
				DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
				ClusterCloseEnum(hObjEnum);
				return ERROR_OUTOFMEMORY;
			}
			if((dwEnumResult = ClusterEnum(hObjEnum,dwIndex,&dwEnumType,wszObjName,&cchObjName)
				) != ERROR_SUCCESS) break;
		}
		else if(ERROR_SUCCESS == dwEnumResult)
		{
			if(dwClusEnumType == dwEnumType)
				objects.push_back(wszObjName);
			else
				DebugPrintf(SV_LOG_WARNING,"Got unrequested object type\n");
		}
		else { break; }
	}
	if(ERROR_NO_MORE_ITEMS != dwEnumResult)
	{
		dwRet = dwEnumResult;
	}

	delete [] wszObjName;
	ClusterCloseEnum(hObjEnum);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return dwRet;
}

SVERROR getClusterQuorumInfo(ClusterInformation &clusInfo)
{
	USES_CONVERSION;
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	SVERROR ret = SVS_OK;
	HCLUSTER hCluster = OpenCluster(NULL);
	if(NULL == hCluster)
	{
		ret = SVE_FAIL;
		DebugPrintf(SV_LOG_ERROR,"Failed to connect to cluster. Error %d",GetLastError());
	}
	else
	{
		OSVERSIONINFO OsInfo;
		memset(&OsInfo,0,sizeof(OSVERSIONINFO));
		OsInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

		WCHAR szResourceName[MAX_PATH],szDeviceName[MAX_PATH];
		DWORD dwMaxQuorumLogSize = 0, dwRet,
			cchResourceName,cchDeviceName;
		cchResourceName = cchDeviceName = MAX_PATH;

		dwRet = GetClusterQuorumResource(hCluster,
			szResourceName,
			&cchResourceName,
			szDeviceName,
			&cchDeviceName,
			&dwMaxQuorumLogSize);
		if(ERROR_SUCCESS == dwRet)
		{
			clusInfo.m_quorumInfo.resource = W2A(szResourceName);
			DebugPrintf(SV_LOG_INFO,"Quorum Resource: %s\n",
				clusInfo.m_quorumInfo.resource.c_str());

			GetVersionEx(&OsInfo);

			if(OsInfo.dwMajorVersion >= 6)
			{
				switch(dwMaxQuorumLogSize)
				{
				case CLUS_NODE_MAJORITY_QUORUM:
					clusInfo.m_quorumInfo.type = CLUSTER_QUORUM_TYPE_NODE_MAJORITY;
					break;
				case CLUS_LEGACY_QUORUM:
					clusInfo.m_quorumInfo.path = W2A(szDeviceName);
					clusInfo.m_quorumInfo.type = CLUSTER_QUORUM_TYPE_NO_MAJORITY;
					break;
				case CLUS_HYBRID_QUORUM:
					{
						ClusterResourceOp resOp(hCluster,szResourceName);
						if(resOp.isOpened())
						{
							ClusterResourceInformation resInfo;
							resOp.GetResourceInformation(resInfo);
							if(0 == resInfo.type.compare("Physical Disk"))
							{
								clusInfo.m_quorumInfo.type = 
									CLUSTER_QUORUM_TYPE_NODE_DISK_MAJORITY;
								clusInfo.m_quorumInfo.path = W2A(szDeviceName);
							}
							else if(0 == resInfo.type.compare("File Share Witness") &&
								resInfo.otherProp.find("SharePath") != resInfo.otherProp.end())
							{
								clusInfo.m_quorumInfo.path = 
									resInfo.otherProp.find("SharePath")->second;
								clusInfo.m_quorumInfo.type = 
									CLUSTER_QUORUM_TYPE_NODE_FS_MAJORITY;
							}
							else
							{
								DebugPrintf(SV_LOG_WARNING,"Unkown quorum resoruce type:  %s. %s\n",
									resInfo.type.c_str(),
									resInfo.errorMsg.c_str());
								DebugPrintf(SV_LOG_ERROR,"Could not identify quorum type.\n");
								ret = SVE_FAIL;
							}
						}
						else
						{
							ret = SVE_FAIL;
							DebugPrintf(SV_LOG_ERROR,"Failed to get quorum resource props. Error %d\n",
								resOp.GetError());
						}
					}
					break;
				}
			}
			else
			{
				//Win2k3 cluster will use following resource types as quorum
				//	1. Physical Disk
				//	2. Majority Node Set
				//  3. Local Quorum
				clusInfo.m_quorumInfo.path = W2A(szDeviceName);
				ClusterResourceOp resOp(hCluster,szResourceName);
				if(resOp.isOpened())
				{
					ClusterResourceInformation resInfo;
					resOp.GetResourceInformation(resInfo);
					clusInfo.m_quorumInfo.type = resInfo.type;
				}
				else
				{
					ret = SVE_FAIL;
					DebugPrintf(SV_LOG_ERROR,"Failed to get quorum resource props. Error %d\n",
						resOp.GetError());
				}
			}
		}
		else
		{
			ret = SVE_FAIL;
			DebugPrintf(SV_LOG_ERROR,"Failed to get quorum resource information. Error %d\n",dwRet);
		}
		CloseCluster(hCluster);
	}
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return ret;
}

//ClusterGroupOp class member functions
void ClusterGroupOp::FillClusterGroupDetails(const std::wstring & grpName, ClusterGroupInformation & info)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	HGROUP hGroup = OpenClusterGroup(m_hCluster,grpName.c_str());
	if(NULL == hGroup)
	{
		DWORD dwError = GetLastError();
		char szError[16];
		info.errorMsg += "Group open error ";
		info.errorMsg += ultoa(dwError,szError,10);
		info.errorMsg += ";";
		DebugPrintf(SV_LOG_DEBUG, "Failed to open the cluster group. Error %d\n",dwError);
		DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
		return ;
	}
	USES_CONVERSION;
	
	WCHAR wszGroupId[MAX_PATH];
	DWORD dwGuidLen = MAX_PATH,
		dwRet = ClusterGroupControl(hGroup,
		NULL,
		CLUSCTL_GROUP_GET_ID,
		NULL,
		0,
		(LPVOID)wszGroupId,
		dwGuidLen,
		&dwGuidLen);
	if(ERROR_SUCCESS != dwRet)
	{
		char szError[16];
		info.errorMsg += "ClusterGroup Id error ";
		info.errorMsg += ultoa(dwRet,szError,10);
		info.errorMsg += ";";
		DebugPrintf(SV_LOG_DEBUG, "ClusterGroupControl failed. Error %d\n", dwRet) ;
		DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
		return ;
	}
	info.id = W2A(wszGroupId);

	WCHAR szNodeName[MAX_COMPUTERNAME_LENGTH+1];
	DWORD mxNodeNameLen = MAX_COMPUTERNAME_LENGTH +1;
	info.state = GetClusterGroupState(hGroup,szNodeName,&mxNodeNameLen);
	if(info.state < 0)
	{
		dwRet = GetLastError();
		char szError[16];
		info.errorMsg += "GetClusterGroupState error ";
		info.errorMsg += ultoa(dwRet,szError,10);
		info.errorMsg += ";";
		DebugPrintf(SV_LOG_DEBUG, "GetClusterGroupState failed. Error %d\n", dwRet) ;
	}
	else
	{
		info.ownerNode = W2A(szNodeName);
	}

	CloseClusterGroup(hGroup);
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

DWORD ClusterGroupOp::GetClusGroupsInfo(std::vector<ClusterGroupInformation> & ClusGroups)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	USES_CONVERSION;

	if(!isConnected())
	{
		DebugPrintf(SV_LOG_DEBUG, "Cluster handle is not valid. Error %d\n", m_dwError) ;
		DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
		return m_dwError;
	}

	std::vector<std::wstring> grps;
	DWORD dwRet = GetClusterGroups(grps);
	if(ERROR_SUCCESS != dwRet)
	{
		DebugPrintf(SV_LOG_DEBUG, "Failed to get clsuter groups. Error %d\n", dwRet) ;
		DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
		return dwRet;
	}

	for(DWORD i=0; i < grps.size(); i++)
	{
		ClusterGroupInformation grpInfo;
		grpInfo.name = W2A(grps[i].c_str());

		FillClusterGroupDetails(grps[i],grpInfo);
		
		FillClusterGroupNodes(grps[i], grpInfo);

		FillClusterGroupResourcesInfo(grps[i], grpInfo);

		ClusGroups.push_back(grpInfo); 
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return ERROR_SUCCESS;
}

DWORD ClusterGroupOp::GetClusterGroups(std::vector<std::wstring> & groups)
{
	return GetClusterEnumObjects(
		m_hCluster, 
		CLUSTER_ENUM_GROUP, 
		groups);
}	

void ClusterGroupOp::FillClusterGroupNodes(const std::wstring & grpName,
						   ClusterGroupInformation & info)
{
	std::vector<std::wstring> nodes;
	DWORD dwRet = GetClusterGroupContents(
		m_hCluster,
		grpName,
		nodes,
		CLUSTER_GROUP_ENUM_NODES);
	if(ERROR_SUCCESS != dwRet)
	{
		char szError[16];
		info.errorMsg += "OwnersNodes error ";
		info.errorMsg += ultoa(dwRet,szError,10);
		info.errorMsg += ";";
		DebugPrintf(SV_LOG_ERROR, "Failed to get prefered owners of the group. Error %d\n", dwRet) ;
		return ;
	}

	USES_CONVERSION;
	for(size_t i=0; i<nodes.size(); i++)
		info.prefOwners.push_back(W2A(nodes[i].c_str()));
}

void ClusterGroupOp::FillClusterGroupResourcesInfo(const std::wstring & grpName,
						   ClusterGroupInformation & info)
{
	std::vector<std::wstring> resources;
	DWORD dwRet = GetClusterGroupContents(
		m_hCluster,
		grpName,
		resources,
		CLUSTER_GROUP_ENUM_CONTAINS);
	if(ERROR_SUCCESS == dwRet)
	{
		for(size_t i=0; i < resources.size(); i++)
		{
			ClusterResourceOp res(m_hCluster,resources[i]);
			ClusterResourceInformation resInfo;
			res.GetResourceInformation(resInfo);
			info.resources.push_back(resInfo);
		}
	}
	else
	{
		char szError[16];
		info.errorMsg += "Group Resources error ";
		info.errorMsg += ultoa(dwRet,szError,10);
		info.errorMsg += ";";
		DebugPrintf(SV_LOG_ERROR, "Failed to get resources of the group. Error %d\n", dwRet) ;
	}
}

//ClusterResource
void ClusterResourceOp::FillDiskInfo(ClusterResourceInformation &info)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	USES_CONVERSION;
	DWORD dwRet = ERROR_SUCCESS;
	CLUSPROP_BUFFER_HELPER cbh;
	LPVOID lpDiskInfoBuff = NULL;
	DWORD cbDiskInfoBuff = 0;
	dwRet = ClusUtil_ClusterResourceControl(m_hRes,
		CLUSCTL_RESOURCE_STORAGE_GET_DISK_INFO_EX,&lpDiskInfoBuff,cbDiskInfoBuff);
	if(ERROR_INVALID_FUNCTION == dwRet)
	{
		dwRet = ClusUtil_ClusterResourceControl(m_hRes,
		CLUSCTL_RESOURCE_STORAGE_GET_DISK_INFO,&lpDiskInfoBuff,cbDiskInfoBuff);
	}
	if(ERROR_SUCCESS != dwRet)
	{
		char szError[16];
		info.errorMsg += "DinskInfo error ";
		info.errorMsg += ultoa(dwRet,szError,10);
		info.errorMsg += ";";
		DebugPrintf(SV_LOG_ERROR, "Failed to get disk resource partition info. Error %d\n", dwRet) ;
		DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
		return ;
	}

	cbh.pb = (PBYTE)lpDiskInfoBuff;

	DWORD cbPos = 0;
	std::string strPartInfo;
	while(CLUSPROP_SYNTAX_ENDMARK != cbh.pSyntax->dw)
	{
		char szValue[64];
		std::string strValue;
		switch(cbh.pValue->Syntax.dw)
		{
		case CLUSPROP_SYNTAX_DISK_SIGNATURE:
			info.otherProp["Signature"] =
				ultoa(cbh.pDiskSignatureValue->dw,szValue,16);
			break;
		case CLUSPROP_SYNTAX_DISK_GUID:
			info.otherProp["GUID"] = W2A(cbh.pStringValue->sz);
			break;
		case CLUSPROP_SYNTAX_SCSI_ADDRESS:
			strValue = itoa((int)cbh.pScsiAddressValue->PortNumber,szValue,10);
			strValue += ",";
			strValue += itoa((int)cbh.pScsiAddressValue->PathId,szValue,10);
			strValue += ",";
			strValue += itoa((int)cbh.pScsiAddressValue->TargetId,szValue,10);
			strValue += ",";
			strValue += itoa((int)cbh.pScsiAddressValue->Lun,szValue,10);
			info.otherProp["SCSIAddress"] = strValue;
			break;
		case CLUSPROP_SYNTAX_DISK_NUMBER:
			info.otherProp["Disk"] = ultoa(cbh.pDiskNumberValue->dw,szValue,10);
			break;
		case CLUSPROP_SYNTAX_DISK_SIZE:
			info.otherProp["Size"] = _ui64toa(cbh.pULargeIntegerValue->li.QuadPart,szValue,10);
			break;
		case CLUSPROP_SYNTAX_PARTITION_INFO:
		case CLUSPROP_SYNTAX_PARTITION_INFO_EX:
			strPartInfo += W2A(cbh.pPartitionInfoValue->szDeviceName);
			if(lstrlenW(cbh.pPartitionInfoValue->szVolumeLabel) > 0)
			{
				strPartInfo += ",";
				strPartInfo += W2A(cbh.pPartitionInfoValue->szVolumeLabel);
			}
			strPartInfo += ",";
			if(CLUSPROP_SYNTAX_PARTITION_INFO_EX ==
				cbh.pPartitionInfoValue->Syntax.dw)
				strPartInfo += 
				_ui64toa(cbh.pPartitionInfoValueEx->TotalSizeInBytes.QuadPart, szValue, 10);
			strPartInfo += ";";
			break;
		default:
			//Log warning message about unknown value entry.
			break;
		}

		cbPos += ClusHlpr_ListEntrySize(cbh.pValue->cbLength);
		if(cbPos + sizeof(DWORD) > cbDiskInfoBuff)
			break;
		
		cbh.pb += ClusHlpr_ListEntrySize(cbh.pValue->cbLength);
	}
	
	info.otherProp["PartitionInfo"] = strPartInfo;

	LocalFree(lpDiskInfoBuff);
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
void ClusterResourceOp::FillResourceType(ClusterResourceInformation &info)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	USES_CONVERSION;
	DWORD dwRet,bytesReturned;
	std::string type;

	WCHAR *szResType = NULL;
	dwRet = ClusUtil_ClusterResourceControl(
		m_hRes,
		CLUSCTL_RESOURCE_GET_RESOURCE_TYPE,
		(LPVOID*)&szResType,
		bytesReturned
		);
	if(ERROR_SUCCESS == dwRet)
	{
		info.type = W2A(szResType);
		LocalFree((LPVOID)szResType);
	}
	else
	{
		char szError[16];
		info.errorMsg += "ResourceType error ";
		info.errorMsg += ultoa(dwRet, szError, 10);
		info.errorMsg += ";";
		DebugPrintf(SV_LOG_ERROR, "Failed to get Resource type. error %d\n", dwRet) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ClusterResourceOp::FillResourceId(ClusterResourceInformation &info)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	USES_CONVERSION;
	DWORD dwRet,bytesReturned;
	std::string type;

	WCHAR *szResId = NULL;
	dwRet = ClusUtil_ClusterResourceControl(
		m_hRes,
		CLUSCTL_RESOURCE_GET_ID,
		(LPVOID*)&szResId,
		bytesReturned
		);
	if(ERROR_SUCCESS == dwRet)
	{
		info.id = W2A(szResId);
		LocalFree((LPVOID)szResId);
	}
	else
	{
		char szError[16];
		info.errorMsg += "ResourceId error ";
		info.errorMsg += ultoa(dwRet, szError, 10);
		info.errorMsg += ";";
		DebugPrintf(SV_LOG_ERROR, "Failed to get Resource Id. Error %d\n", dwRet) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ClusterResourceOp::FillResourcePrivateDetails(ClusterResourceInformation & info)
{
	if(0 == info.type.compare("Physical Disk"))
	{
		FillDiskInfo(info);
		return;
	}
	else if(0 == info.type.compare("Network Name"))
	{
		FillNetworkNameInfo(info);
		return;
	}
	else if(0 == info.type.compare("IP Address"))
	{
		FillIPAddressInfo(info);
		return;
	}
	else if(0 == info.type.compare("File Share Witness"))
	{
		FillFileShareWitnessInfo(info);
	}
}

void ClusterResourceOp::FillIPAddressInfo(ClusterResourceInformation &info)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	DWORD dwRet = 0;
	do
	{
		std::string propValue;
		dwRet = GetSzProperty(
			CLUSCTL_RESOURCE_GET_PRIVATE_PROPERTIES,
			"Address",
			propValue);
		if(ERROR_SUCCESS != dwRet)
			break;
		else
			info.otherProp["Address"] =  propValue;

		dwRet = GetSzProperty(
			CLUSCTL_RESOURCE_GET_PRIVATE_PROPERTIES,
			"Network",
			propValue);
		if(ERROR_SUCCESS != dwRet)
			break;
		else
			info.otherProp["Netword"] =  propValue;
	}while(false);
	if(ERROR_SUCCESS != dwRet)
	{
		char szError[16];
		info.errorMsg += "IPAddress prop error ";
		info.errorMsg += ultoa(dwRet, szError, 10);
		info.errorMsg += ";";
		DebugPrintf(SV_LOG_ERROR, "Failed to get IPAddress resource property. Error %d\n", dwRet) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ClusterResourceOp::FillNetworkNameInfo(ClusterResourceInformation &info)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	DWORD dwRet = 0;
	do
	{
		std::string propValue;
		dwRet = GetSzProperty(
			CLUSCTL_RESOURCE_GET_PRIVATE_PROPERTIES,
			"Name",
			propValue);
		if(ERROR_SUCCESS != dwRet)
			break;
		else
			info.otherProp["NetworkName"] =  propValue;
	}while(false);
	if(ERROR_SUCCESS != dwRet)
	{
		char szError[16];
		info.errorMsg += "NetworkName prop error ";
		info.errorMsg += ultoa(dwRet, szError, 10);
		info.errorMsg += ";";
		DebugPrintf(SV_LOG_ERROR, "Failed to get NetworkName resource property. Error %d\n", dwRet) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

DWORD ClusterResourceOp::GetSzProperty(DWORD dwControlCode,
											  const std::string & szPropName,
											  std::string & szValue)
{
	USES_CONVERSION;
	LPVOID lpOutBuf		= NULL;
	DWORD bytesReturned = 0,
		  dwRet			= 0;
	dwRet = ClusUtil_ClusterResourceControl(m_hRes,
		dwControlCode,
		&lpOutBuf
		,bytesReturned);

	if(ERROR_SUCCESS != dwRet)
		return dwRet;

	LPWSTR pszPropValue = NULL;

	dwRet = ResUtilFindSzProperty(lpOutBuf,
		bytesReturned,
		A2W(szPropName.c_str()),
		&pszPropValue);

	if(ERROR_SUCCESS == dwRet)
	{
		szValue = W2A(pszPropValue);
		LocalFree(pszPropValue);
	}

	LocalFree(lpOutBuf);
	
	return dwRet;
}
void ClusterResourceOp::FillResourceDependencies(ClusterResourceInformation & info)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	USES_CONVERSION;
	DWORD dwRet = ERROR_SUCCESS;
	do
	{
		HRESENUM hResEnum = ClusterResourceOpenEnum(m_hRes,CLUSTER_RESOURCE_ENUM_DEPENDS);
		if(NULL == hResEnum)
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to open ClusterResourceEnum. Error %d\n", GetLastError()) ;
			break;
		}
		DWORD dwType, cchName, dwszNameLen = MAX_PATH;
		LPWSTR lpszName = new (std::nothrow) WCHAR[dwszNameLen];
		for(DWORD dwIndex=0; ;dwIndex++)
		{
			cchName = dwszNameLen;
			dwRet = ClusterResourceEnum(hResEnum,dwIndex,&dwType,lpszName,&cchName);
			if(ERROR_MORE_DATA == dwRet)
			{
                INM_SAFE_ARITHMETIC(cchName+=InmSafeInt<int>::Type(1), INMAGE_EX(cchName))
				dwszNameLen = cchName;
				delete [] lpszName;
				LPWSTR lpszName = new (std::nothrow) WCHAR[dwszNameLen];
				dwRet = ClusterResourceEnum(hResEnum,dwIndex,&dwType,lpszName,&cchName);
			}
			if(ERROR_SUCCESS != dwRet)
				break;
			if(CLUSTER_RESOURCE_ENUM_DEPENDS == dwType)
				info.dependencies.push_back(W2A(lpszName));
		}
		delete [] lpszName;
		ClusterResourceCloseEnum(hResEnum);
	}while(false);
	if(ERROR_NO_MORE_ITEMS != dwRet && ERROR_SUCCESS != dwRet)
	{
		char szError[16];
		info.errorMsg += "ResourceDepends error ";
		info.errorMsg += ultoa(dwRet, szError, 10);
		info.errorMsg += ";";
		DebugPrintf(SV_LOG_ERROR, "Failed to get resource dependencies. Error %d\n", dwRet) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ClusterResourceOp::FillFileShareWitnessInfo(ClusterResourceInformation &info)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	DWORD dwRet = 0;
	do
	{
		std::string propValue;
		dwRet = GetSzProperty(
			CLUSCTL_RESOURCE_GET_PRIVATE_PROPERTIES,
			"SharePath",
			propValue);
		if(ERROR_SUCCESS != dwRet)
			break;
		else
			info.otherProp["SharePath"] =  propValue;
	}while(false);
	if(ERROR_SUCCESS != dwRet)
	{
		char szError[16];
		info.errorMsg += "SharePath prop error ";
		info.errorMsg += ultoa(dwRet, szError, 10);
		info.errorMsg += ";";
		DebugPrintf(SV_LOG_ERROR, "Failed to get FileShareWitness resource proprty. Error %d\n", dwRet) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
