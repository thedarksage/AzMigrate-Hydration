#include "ClusterOperation.h"
#include "system_win.h"
#include "appexception.h"
#include "clusterutil.h"
#include "host.h"
#include "appcommand.h"
#include <boost/lexical_cast.hpp>
#include "serializationtype.h"
std::string ClusterOp::getClusUtilPath(bool &bBringOnline)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::string strClusItilAbsolutePath ;
    strClusItilAbsolutePath += m_objLocalConfigurator.getInstallPath();

    strClusItilAbsolutePath += "\\";
    OSVERSIONINFOEX osVersionInfo ;
    getOSVersionInfo(osVersionInfo) ;

    if(osVersionInfo.dwMajorVersion >= WIN2K8_SERVER_VER)
    {
        bBringOnline = true;
    }

    strClusItilAbsolutePath += "ClusUtil.exe";

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return strClusItilAbsolutePath;
}

bool ClusterOp::breakCluster(const std::string &outputFileName,SV_ULONG &exitCode)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRetValue = false;
    bool bOnline = false;
    std::string strCommnadToExecute ;

    strCommnadToExecute += std::string("\"");
    strCommnadToExecute += std::string(getClusUtilPath(bOnline));
    strCommnadToExecute += std::string("\"");
    strCommnadToExecute += m_strCommandToExecute;

    m_strCommandToExecute = std::string("");
    m_strCommandToExecute = strCommnadToExecute;


    DebugPrintf(SV_LOG_INFO,"\n The break  command to execute : %s\n",m_strCommandToExecute.c_str());

    AppCommand objAppCommand(m_strCommandToExecute,m_uTimeOut,outputFileName);
	HANDLE h = NULL;
	h = Controller::getInstance()->getDuplicateUserHandle();
    if(objAppCommand.Run(exitCode,m_stdOut, Controller::getInstance()->m_bActive, "", h ) != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to spawn clusutil.exe.\n");
    }
    else
    {
        DebugPrintf(SV_LOG_INFO,"Successfully spawned the clusutil.exe. \n");
        bRetValue = true;
    }
	if(h)
	{
		CloseHandle(h);
	}

    DebugPrintf(SV_LOG_INFO,"\n The exit code from cluster break process is : %d",exitCode);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRetValue;
}

bool ClusterOp::constructCluster(const std::string &outputFileName,SV_ULONG &exitCode)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bResult = false;
    bool bOnline = false;
    std::string strCommnadToExecute ;


    m_strCommandToExecute = std::string("");
    m_strCommandToExecute += std::string(" -prepare StandaloneToCluster:");
    m_strCommandToExecute +=  Host::GetInstance().GetHostName();
    m_strCommandToExecute += std::string(" -force");

    strCommnadToExecute += std::string("\"");
    strCommnadToExecute += std::string(getClusUtilPath(bOnline));
    strCommnadToExecute += std::string("\"");
    strCommnadToExecute += m_strCommandToExecute;

    m_strCommandToExecute = std::string("");
    m_strCommandToExecute = strCommnadToExecute;


    DebugPrintf(SV_LOG_INFO,"\n The cluster construct command to execute : %s\n",m_strCommandToExecute.c_str());
    AppCommand objAppCommand(m_strCommandToExecute,m_uTimeOut,outputFileName);
	HANDLE h = NULL;
	h = Controller::getInstance()->getDuplicateUserHandle();
    if(objAppCommand.Run(exitCode,m_stdOut, Controller::getInstance()->m_bActive, "", h) != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to spawn clusutil.exe.\n");
    }
    else
    {
        DebugPrintf(SV_LOG_INFO,"Successfully spawned the clusutil.exe. \n");
        bResult = true;
    }
	if(h)
	{
		CloseHandle(h);
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bResult;
}
SVERROR ClusterOp::offlineClusterResources(std::string &resourceName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    USES_CONVERSION;
    SVERROR retStatus = SVS_FALSE ;
    HCLUSTER hCluster = NULL;
    HRESOURCE hResource = NULL;
    HCLUSENUM hClusEnum = NULL;
    WCHAR* lpwClusterName = NULL;
    hCluster = OpenCluster( lpwClusterName );
    if( NULL != hCluster )
    {
        hResource = OpenClusterResource (hCluster,A2W(resourceName.c_str()));
        if(hResource != NULL)
        {
            DWORD dwResult = OfflineClusterResource(hResource);
            if((dwResult == ERROR_SUCCESS) || (dwResult == ERROR_IO_PENDING))
            {
                DebugPrintf(SV_LOG_INFO,"Successfully offlined the Cluster Resource %s\n",resourceName.c_str());
                retStatus = SVS_OK ;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR,"Failed to bring offline the Cluster Resource %s\n",resourceName.c_str());
                DebugPrintf(SV_LOG_ERROR,"Error Code :%d\n",GetLastError());
            }
            CloseClusterResource(hResource);
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to open cluster Resource %s.Error Code:%d\n",resourceName.c_str(),GetLastError());
        }
        CloseCluster( hCluster );
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to open handle to cluster.Error Code :%d\n",GetLastError());
    }
    if( lpwClusterName != NULL )
    {
        free( lpwClusterName );
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return retStatus;
}

SVERROR ClusterOp::getCLusterResourceInfo(std::string clusterName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR retStatus = SVS_FALSE ;
    HCLUSTER hCluster = NULL;
    HRESOURCE hResource = NULL;
    HCLUSENUM hClusEnum = NULL;

    WCHAR* lpwClusterName = NULL;
    DWORD dwIndex = 0;
    DWORD dwType;

    DWORD dwRetVal = ERROR_SUCCESS;

    DWORD dwcbBuffer = INM_BUFFER_LEN;
    WCHAR lpwBuffer[INM_BUFFER_LEN];

    DWORD dwcbTypeBuffer = INM_TYPE_BUFFER_LEN;
    WCHAR lpwTypeBuffer[INM_TYPE_BUFFER_LEN];

    USES_CONVERSION;
    if( clusterName.empty() == false )
        lpwClusterName = A2W(clusterName.c_str()) ;

    hCluster = OpenCluster( lpwClusterName );
    if( NULL != hCluster )
    {
        hClusEnum = ClusterOpenEnum( hCluster, CLUSTER_ENUM_RESOURCE );

        if( NULL != hClusEnum )
        {
            dwIndex = 0;	
            dwcbBuffer = INM_BUFFER_LEN;
            dwRetVal = ClusterEnum( hClusEnum, dwIndex, &dwType, lpwBuffer, &dwcbBuffer );
			DebugPrintf(SV_LOG_INFO,"ClusterEnum return value: %d\n",dwRetVal);

            while( ERROR_SUCCESS == dwRetVal )
            {
                RescoureInfo objRescoureInfo;
                hResource = OpenClusterResource( hCluster, lpwBuffer );

                if( NULL != hResource )
                {
                    dwcbTypeBuffer = INM_TYPE_BUFFER_LEN;
                    dwRetVal = ClusterResourceControl( 
                        hResource, 
                        NULL, 
                        CLUSCTL_RESOURCE_GET_RESOURCE_TYPE,
                        NULL,
                        NULL,
                        lpwTypeBuffer,
                        dwcbTypeBuffer,
                        NULL
                        );



                    if( dwRetVal!= ERROR_SUCCESS )
                    {
                        DebugPrintf( SV_LOG_ERROR, " ClusterResourceControl() Failed.Error Code: %d \n", GetLastError() );
						retStatus = SVS_FALSE;
                        break;
                    }
                    WCHAR lpznodeName[INM_TYPE_BUFFER_LEN], lpzgroupName[INM_TYPE_BUFFER_LEN];
                    DWORD cbNodeNameBuffSize = INM_TYPE_BUFFER_LEN, cbGroupNameBuffSize = INM_TYPE_BUFFER_LEN ;
                    objRescoureInfo.rescourceState =  GetClusterResourceState( hResource, lpznodeName, &cbNodeNameBuffSize, lpzgroupName, &cbGroupNameBuffSize );
                    objRescoureInfo.rescourceName = W2A(lpwBuffer);
                    objRescoureInfo.resourceType = W2A(lpwTypeBuffer);
                    objRescoureInfo.ownerNode = W2A(lpznodeName);
                    objRescoureInfo.groupName = W2A(lpzgroupName);


                    m_resouceInfoList.push_back(objRescoureInfo);
                    CloseClusterResource( hResource );		
                    dwIndex++;
                    dwcbBuffer = INM_BUFFER_LEN;
                    dwRetVal = ClusterEnum( hClusEnum, dwIndex, &dwType, lpwBuffer, &dwcbBuffer );
					DebugPrintf(SV_LOG_INFO,"ClusterEnum return value: %d\n",dwRetVal);
                    retStatus = SVS_OK;
                }
				else
				{
					DebugPrintf(SV_LOG_ERROR,"OpenClusterResource failed. Error code = %d\n",GetLastError());
					retStatus = SVS_FALSE;
					break;
				}
            }
            ClusterCloseEnum( hClusEnum );
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR,"ClusterOpenEnum failed.Error code = %d\n",GetLastError());
        }
        CloseCluster( hCluster );

    }
    else 
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to open cluste.Error code = %d.\n",GetLastError());
    }
    if( lpwClusterName != NULL )
    {
        free( lpwClusterName );
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return retStatus;
}


SVERROR ClusterOp::persistClusterResourceState()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR retStatus = SVS_FALSE;
    std::list<RescoureInfo> tempResourcesInfo;
    if(getCLusterResourceInfo(std::string("")) != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to get the resources information.\n");
        return SVS_FALSE;
    }
    std::list<RescoureInfo>::iterator iterBegin = m_resouceInfoList.begin();
    std::list<RescoureInfo>::iterator iterEnd = m_resouceInfoList.end();
    while(iterBegin != iterEnd)
    {
        RescoureInfo objRescoureInfo = *iterBegin;
        if((strcmpi(objRescoureInfo.resourceType.c_str(),"Physical Disk") == 0) || 
            (strcmpi(objRescoureInfo.groupName.c_str(),"Cluster Group") == 0))
        {
            DebugPrintf(SV_LOG_INFO,"Ignoring this resource : %s",objRescoureInfo.rescourceName.c_str());
            iterBegin++;
            continue;
        }
        else if(objRescoureInfo.rescourceState ==  ClusterResourceOnline) //Perform the offline operation iff Resource is online
        {

            offlineClusterResources(objRescoureInfo.rescourceName); //dont bother about return value since,anyway we will give a try to online this resource
        }

        iterBegin++;
    }
    persistClusterResourcesProp();
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return SVS_OK ;
}
SVERROR ClusterOp::persistClusterResourcesProp()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR retStatus = SVS_FALSE;
    AppLocalConfigurator configurator ;
    ESERIALIZE_TYPE type = configurator.getSerializerType();
    AppAgentConfiguratorPtr appConfiguratorPtr = AppAgentConfigurator::CreateAppAgentConfigurator(type);
    if(appConfiguratorPtr->perisitClusterResourceInfo(m_resouceInfoList) != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to persist the Cluster resource information\n");
    }
    else
    {
        DebugPrintf(SV_LOG_INFO,"Persisted the cluster resource information successfully.\n");
        retStatus = SVS_OK ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return retStatus;
}

SVERROR ClusterOp::revertResourcesToOriginalState(std::string& result)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR retStatus = SVS_OK;
    std::stringstream stream;
    AppLocalConfigurator configurator ;
    ESERIALIZE_TYPE type = configurator.getSerializerType();
    std::list<RescoureInfo> ListClusterResources;
    AppAgentConfiguratorPtr appConfiguratorPtr = AppAgentConfigurator::CreateAppAgentConfigurator(type);
    try
    {
        if(appConfiguratorPtr->UnserializeClusterResourceInfo(ListClusterResources) != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to unserialise the cluster resources\n");
            return SVS_FALSE;
        }
    }
    catch(AppException obj)
    {
        DebugPrintf(SV_LOG_INFO,"Exception caught %s\n",obj.what()); //
        return SVS_FALSE;
    }
    if(checkClusterResourceAvailable() == true)
    {
        std::string errorLog;
		std::map<std::string,RescoureInfo> OnlnResources;
		int nCycles = 0; //will attempt to online failed resources in n cycles
		do
		{
			nCycles++;
			std::list<RescoureInfo>::iterator iterBegin = ListClusterResources.begin();
			while(iterBegin != ListClusterResources.end())
			{
				if((*iterBegin).rescourceState == ClusterResourceOnline)
				{
					if(OnlnResources.find((*iterBegin).rescourceName) ==  OnlnResources.end())
					{
						if(onlineClusterResources((*iterBegin).rescourceName,errorLog) == SVS_OK)
						{
							stream << "Successfully brought the Cluster Resource " ;
							stream << (*iterBegin).rescourceName;
							stream << " online \n";
							OnlnResources.insert(std::make_pair((*iterBegin).rescourceName,*iterBegin));
						}
						else
						{
							if(nCycles > 3)
							{
								stream << errorLog;
								retStatus = SVS_FALSE;
							}
						}
					}
				}
				iterBegin++;
			}

			if(OnlnResources.size() == ListClusterResources.size() || nCycles > 3)
				break;

		}while(!Controller::getInstance()->QuitRequested(1));
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG,"Cluster resources not available\n");
        stream << "Failed to get Cluster resources information"<<std::endl;
		retStatus = SVS_FALSE;
    }
    //retStatus = SVS_OK; //explicitly set the value as true.
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    result = stream.str();
    return retStatus;
}
SVERROR ClusterOp::onlineClusterResources(const std::string& resourceName,std::string& errorLog)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::stringstream stream;
    unsigned int  timeOut = 0;
    USES_CONVERSION;
    SVERROR retStatus = SVS_FALSE ;
    HCLUSTER hCluster = NULL;
    HRESOURCE hResource = NULL;
    HCLUSENUM hClusEnum = NULL;
	int nRetryOpenClstr = 0;
	do
	{
		WCHAR* lpwClusterName = NULL;
		hCluster = OpenCluster( lpwClusterName );
		if( NULL != hCluster )
		{
			do 
			{
				hResource = OpenClusterResource (hCluster,A2W(resourceName.c_str()));
				if(hResource != NULL)
				{
					DebugPrintf(SV_LOG_DEBUG,"Successfully open the cluster resource %s \n",resourceName.c_str());
					break;
				}
				else
				{
					timeOut += 10 ;
					if( timeOut > 300 )
					{
						DebugPrintf(SV_LOG_WARNING, "Timed-out at 300 seconds while waiting for the resource %s\n", resourceName.c_str()) ;
						stream << "TimeOut at 300 seconds while waiting to OpenClusterResource successful\n";
						break;
					}
				}
			}while(!Controller::getInstance()->QuitRequested(10) );
			if(hResource != NULL)
			{
				timeOut = 0;
				do
				{
					DWORD dwResult = OnlineClusterResource(hResource);
					if((dwResult == ERROR_SUCCESS) || (dwResult == ERROR_IO_PENDING))
					{
						CLUSTER_RESOURCE_STATE clusResState;
						int nStatusRetry = 0;
						do
						{
							clusResState = GetClusterResourceState(hResource,NULL,NULL,NULL,NULL);
							if(clusResState == ClusterResourceOnline)
							{
								DebugPrintf(SV_LOG_DEBUG, "Successfuly brought the resource online.\n");
								retStatus = SVS_OK ;
								break;
							}
							else if(clusResState == ClusterResourceInitializing ||
								clusResState == ClusterResourcePending ||
								clusResState == ClusterResourceOnlinePending ||
								clusResState == ClusterResourceOfflinePending)
							{
								DebugPrintf(SV_LOG_INFO,"Resource is in the process of comming online. Status code: %d \n" ,clusResState);
							}
							else
							{
								stream << "Can not bring " << resourceName << " online at this moment." << std::endl
									<< "Current status: " << clusResState << std::endl;
								DebugPrintf(SV_LOG_ERROR,"Can not bring %s online at this moment. Current state %d. Error code: %d\n",
									resourceName.c_str(),clusResState,GetLastError());
								break;
							}
							if(40 < nStatusRetry++)
							{   //Time out period 120sec
								stream << "Resource online timeout . Current state: " << clusResState << std::endl;
								DebugPrintf(SV_LOG_WARNING, "Reached timeout limit for resource online.");
								//retStatus = SVS_OK ;
								break;
							}

							//::Sleep(3000);
						}while(!Controller::getInstance()->QuitRequested(3));
						//DebugPrintf(SV_LOG_INFO,"Successfully brought the Cluster Resource %s online\n",resourceName.c_str());
						//retStatus = SVS_OK ;
						break;
					}
					else
					{
						DebugPrintf(SV_LOG_DEBUG,"Retrying to Online the resource %s\n",resourceName.c_str());
						timeOut += 5 ;
						if( timeOut > 120  )
						{
							DebugPrintf(SV_LOG_ERROR,"Failed to bring the Cluster Resource %s online. Error Code is %ld\n",resourceName.c_str(),GetLastError());
							stream <<  "Failed to bring cluster resouce " << resourceName << " online. Error Code "<<GetLastError()<<std::endl;
							break;
						}
					}
				}while(!Controller::getInstance()->QuitRequested(5) );
				CloseClusterResource(hResource);
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to open cluster Resource %s.Error Code:%d\n",resourceName.c_str(),GetLastError());
				stream <<  "Failed to open cluster Resource " << resourceName << " online. Error Code "<<GetLastError()<<std::endl;
			}
			CloseCluster( hCluster );
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to open handle to cluster.Error Code :%d\n",GetLastError());
			nRetryOpenClstr++;
			if(10 < nRetryOpenClstr)
			{
				stream <<  "Failed to open handle to cluster. Error Code :"<< GetLastError() << std::endl
					   <<  "Can not bring "<< resourceName << " resource online." << std::endl;
			}
			else
			{
				if( lpwClusterName != NULL )
					free( lpwClusterName );
				continue;
			}
		}

		if( lpwClusterName != NULL )
		{
			free( lpwClusterName );
		}
		break;
	}while(!Controller::getInstance()->QuitRequested(6));
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    errorLog = stream.str();
    return retStatus;
}
SVERROR ClusterOp::findNodeState(const std::string& clusterName,const std::string& nodeName,bool& IsAlive)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR retError = SVS_FALSE; ;
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
    }
    else
    {
        hNode = NULL;
        hNode = OpenClusterNode( hCluster, A2W(nodeName.c_str()) );
        if( hNode == NULL )
        {
            DebugPrintf( SV_LOG_ERROR, "OpenClusterNode Failed. Name: %s .Error Code: %s \n ", nodeName.c_str() ,GetLastError() );
        }
        else
        {
            CLUSTER_NODE_STATE nodeState;
            nodeState = GetClusterNodeState(hNode);
            if(ClusterNodeStateUnknown != nodeState)
            {	
                if(ClusterNodeUp == nodeState)
                {
                    IsAlive = true;
                }
                else
                {
                    IsAlive = false;
                    DebugPrintf(SV_LOG_INFO,"Node %s is in state %d\n",nodeName.c_str(),nodeState);
                }
                retError = SVS_OK;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR,"The node %s is in unknown state.Error Code:%d",nodeName.c_str(),GetLastError());
            }
            CloseClusterNode(hNode);
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
bool ClusterOp::allClusterNodesUp(const std::string& clusterName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool areAllNodeUp = true;

    std::list<std::string> listOfNodes;
    if(getNodesPresentInCluster(clusterName,listOfNodes) == SVS_OK)
    {
        std::list<std::string>::iterator iter = listOfNodes.begin();
        bool bState = false;
        while(iter != listOfNodes.end())
        {
            std::string nodeName = *iter;
            if(findNodeState(clusterName,nodeName,bState) == SVS_OK)
            {
                if(bState == true)
                {
                    DebugPrintf(SV_LOG_INFO,"The node %s is on\n",nodeName.c_str());
                }
                else
                {
                    DebugPrintf(SV_LOG_INFO,"The node %s is off\n",nodeName.c_str());
                    areAllNodeUp = false;
                }
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR,"findNodeState failed to find node state in cluster.\n");
                areAllNodeUp = false ;
                break;
            }
            iter++;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR,"getNodesPresentInCluster failed to find all nodes present in cluster.\n");
        areAllNodeUp = false ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return areAllNodeUp ;

}

bool ClusterOp::BringW2k8ClusterDiskOnline(const std::string &outputFileName,SV_ULONG &exitCode)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false;
    bool bOnline = false;
    std::string strCommnadToExecute ;
    m_strCommandToExecute = std::string("");
    m_strCommandToExecute += std::string(" -prepare onlinedisk");

    strCommnadToExecute += std::string("\"");
    strCommnadToExecute += std::string(getClusUtilPath(bOnline));
    strCommnadToExecute += std::string("\"");
    strCommnadToExecute += m_strCommandToExecute;

    m_strCommandToExecute = std::string("");
    m_strCommandToExecute = strCommnadToExecute;


    DebugPrintf(SV_LOG_INFO,"\n The w2k8 cluster disk online  command to execute : %s\n",m_strCommandToExecute.c_str());
    AppCommand objAppCommand(m_strCommandToExecute,0,outputFileName);
	HANDLE h = NULL;
	h = Controller::getInstance()->getDuplicateUserHandle();
    if(objAppCommand.Run(exitCode,m_stdOut, Controller::getInstance()->m_bActive, "", h ) != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to spawn clusutil.exe.\n");
    }
    else
    {
        DebugPrintf(SV_LOG_INFO,"Successfully spawned the clusutil.exe. \n");
        bRet = true;
    }
	if(h)
	{
		CloseHandle(h);
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

bool ClusterOp::OfflineorOnlineResource(const std::string& resourceType, const std::string& appName,std::string m_VirtualServerName,bool bMakeOnline)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    USES_CONVERSION;
    WCHAR clustName[MAX_PATH];
    DWORD clustIndex = 0;
    DWORD nameLength = 0;
    DWORD type;
    DWORD status = ERROR_SUCCESS;
    bool result = false;
    HRESOURCE hResource = 0;
    std::string szVirtualServer = std::string(m_VirtualServerName);
    size_t seperatorSlash = std::string::npos;

    seperatorSlash = szVirtualServer.find_first_of('\\');
    if( seperatorSlash != std::string::npos )
    {
        szVirtualServer = szVirtualServer.assign(szVirtualServer, 0, seperatorSlash );
    }

    HCLUSTER hCluster = OpenCluster(0);
    if (0 == hCluster)
    {
        status = GetLastError();
        DebugPrintf(SV_LOG_ERROR,"OfflineorOnlineResource:OpenCluster failed Error Code is %ld \n",status);
        DebugPrintf(SV_LOG_DEBUG,"Probably not a cluster Configuration\n");
    }
    else
    {
        HCLUSENUM hClustEnum = ClusterOpenEnum(hCluster, CLUSTER_ENUM_RESOURCE);
        if (0 == hClustEnum) 
        {
            status = GetLastError();
            DebugPrintf(SV_LOG_ERROR,"OfflineorOnlineResource:ClusterOpenEnum failed Error Code is %ld \n",status);
        }
        else
        {
            do
            {
                nameLength = sizeof(clustName) / sizeof(WCHAR);
                
                status = ClusterEnum(hClustEnum, clustIndex, &type, clustName, &nameLength);                    
                if (ERROR_SUCCESS != status) 
                {
                    if (ERROR_NO_MORE_ITEMS != status) 
                    {
                        
                        DebugPrintf(SV_LOG_ERROR,"OfflineorOnlineResource:ClusterOpenEnum failed Error Code is %ld \n",status);
                    }
                }
                
                else
                {
                    hResource = OpenClusterResource(hCluster, clustName);
                    if (0 == hResource)
                    {
                        status = GetLastError();
                        if (ERROR_RESOURCE_NOT_FOUND != status) 
                        {
                            DebugPrintf(SV_LOG_ERROR,"OfflineorOnlineResource:OpenClusterResource failed Error Code is %ld \n",status);
                        }
                    }
                    else
                    {
                        bool bOffline = false;
                        bool bOnLine = false;
                        WCHAR *resName;
                        DWORD resNameLen = MAX_PATH;
                        DWORD bytesReturned;
                        DWORD cbBuffSize = MAX_PATH;
                        LPVOID lpszType;
                        lpszType = LocalAlloc( LPTR, cbBuffSize );

                        if( NULL == lpszType )
                        {											
                            status = GetLastError();
                            DebugPrintf(SV_LOG_ERROR,"OfflineorOnlineResource:Local buffer allocation failure Error Code is %ld \n",status);
                            break;
                        }

                        status = ClusterResourceControl(
                            hResource,             
                            NULL,		
                            CLUSCTL_RESOURCE_GET_RESOURCE_TYPE, 
                            NULL,            
                            0,               
                            (LPVOID)lpszType,
                            cbBuffSize,
                            &bytesReturned);

                        if( ERROR_MORE_DATA == status )
                        {
                            LocalFree( lpszType );
                            cbBuffSize = bytesReturned;

                            lpszType = LocalAlloc( LPTR, cbBuffSize );

                            if( NULL == lpszType )
                            {											
                                status = GetLastError();
                                DebugPrintf(SV_LOG_ERROR,"OfflineorOnlineResource:Local buffer allocation failure Error Code is %ld \n",status);
                                break;
                            }

                            status = ClusterResourceControl(
                                hResource,             
                                NULL,		
                                CLUSCTL_RESOURCE_GET_RESOURCE_TYPE, 
                                NULL,            
                                0,               
                                (LPVOID)lpszType,
                                cbBuffSize,
                                &bytesReturned);
                        }

                        if (ERROR_SUCCESS != status)
                        {
                            DebugPrintf(SV_LOG_ERROR,"OfflineorOnlineResource:ClusterResourceControl failed Error Code is %ld \n",status);
                            DebugPrintf(SV_LOG_ERROR,"Failed to get the resources of type: %s\n",resourceType.c_str() );
                        } 
                        else
                        {
                            if (_strcmpi(W2A((WCHAR*)lpszType), resourceType.c_str()) == 0)
                            {
                                LPWSTR pszPropertyVal = NULL;
                                cbBuffSize = MAX_PATH;
                                LPVOID lpOutBuffer = NULL;
                                LPWSTR pszPropertyName = NULL;
                                if( (strcmpi(appName.c_str(), "EXCHANGE") == 0))
                                {
                                    pszPropertyName = L"NetworkName";
                                }
                                else if( (_strcmpi(appName.c_str(), "SQL") == 0) )
                                {
                                    pszPropertyName = L"VirtualServerName";
                                }
                                else if( (_strcmpi(appName.c_str(), "FILESERVER") == 0) )
                                {
									pszPropertyName = L"Name";
                                } 
                                else
                                {
                                    DebugPrintf(SV_LOG_ERROR,"Invalid application type: %s\n",appName.c_str() );
                                    return false;
                                }

                                lpOutBuffer = LocalAlloc( LPTR, cbBuffSize );

                                if( NULL == lpOutBuffer )
                                {										
                                    status = GetLastError();
                                    DebugPrintf(SV_LOG_ERROR,"OfflineorOnlineResource:Local buffer allocation failure Error Code is %ld \n",status);
                                    break;
                                }

                                status = ClusterResourceControl(
                                    hResource,             
                                    NULL,		
                                    CLUSCTL_RESOURCE_GET_PRIVATE_PROPERTIES, 
                                    NULL,            
                                    0,               
                                    lpOutBuffer,									
                                    cbBuffSize,
                                    &bytesReturned);

                                if( ERROR_MORE_DATA == status )
                                {
                                    LocalFree( lpOutBuffer );
                                    cbBuffSize = bytesReturned;

                                    lpOutBuffer = LocalAlloc( LPTR, cbBuffSize );

                                    if( NULL == lpOutBuffer )
                                    {											
                                        status = GetLastError();
                                        DebugPrintf(SV_LOG_ERROR,"OfflineorOnlineResource:Local buffer allocation failure Error Code is %ld \n",status);
                                        break;
                                    }

                                    status = ClusterResourceControl(
                                        hResource,             
                                        NULL,		
                                        CLUSCTL_RESOURCE_GET_PRIVATE_PROPERTIES, 
                                        NULL,            
                                        0,               
                                        lpOutBuffer,									
                                        cbBuffSize,
                                        &bytesReturned);
                                }

                                if( ERROR_SUCCESS != status )
                                {									
                                    resName = (WCHAR*) LocalAlloc( LPTR, resNameLen );
                                    if( ERROR_SUCCESS == ResUtilGetResourceName(hResource, resName, &resNameLen ))
                                    {
                                        std::string resourceName = W2A(resName);
                                        DebugPrintf(SV_LOG_ERROR,"Failed to get the private properties of the resource : %s\n",resourceName.c_str());
                                    }
                                    else
                                    {
                                        DebugPrintf(SV_LOG_ERROR,"Failed to get the private properties of the resource of type : ",resourceType.c_str());
                                    }
                                    LocalFree( resName );
                                }
                                else
                                {
                                    status = ResUtilFindSzProperty(lpOutBuffer, cbBuffSize, (LPCWSTR) pszPropertyName, &pszPropertyVal);

                                    if( ERROR_SUCCESS != status )
                                    {
                                        resName = (WCHAR*) LocalAlloc( LPTR, resNameLen );
                                        if( ERROR_SUCCESS == ResUtilGetResourceName(hResource, resName, &resNameLen ))
                                        {
                                            std::string resourceName = W2A(resName);
											DebugPrintf(SV_LOG_ERROR,"Failed to parse the private properties of the resource: %s\n",resourceName.c_str());
                                        }
                                        else
                                        {
											DebugPrintf(SV_LOG_ERROR,"Failed to parse the private properties of the resource of type: %s\n",resourceType.c_str());
                                        }
                                        LocalFree( resName );
                                    }
                                    else
                                    {
										if(_strcmpi( szVirtualServer.c_str(), W2A(pszPropertyVal) ) == 0)
										{
											if(bMakeOnline == false)
											{
												if(GetClusterResourceState(hResource, NULL, NULL, NULL, NULL) == ClusterResourceOffline) 

												{
													result = true;
													bOffline = false;			
													DebugPrintf(SV_LOG_DEBUG,"The resource %s is already offline...\n",resourceType.c_str());
												}
												else
												{
													bOffline = true;
												}
											}
											if(bMakeOnline == true)
											{
												if(GetClusterResourceState(hResource, NULL, NULL, NULL, NULL) == ClusterResourceOnline) 
												{
													result = true; 
													bOnLine = false;
													DebugPrintf(SV_LOG_DEBUG,"The resource %s is already Online...\n",resourceType.c_str());
												}
												else
												{
													bOnLine = true;
												}
											}
										}
										else
										{
											bOffline = false;
											bOnLine = false;
										}
									}
									LocalFree( pszPropertyVal );
									LocalFree( lpOutBuffer );
								}

                                if(bOffline)
                                {									
                                    resName = (WCHAR*) LocalAlloc( LPTR, resNameLen );
                                    if( ERROR_SUCCESS == ResUtilGetResourceName(hResource, resName, &resNameLen ))
									{
										std::string resourceName = W2A(resName);
                                        DebugPrintf(SV_LOG_DEBUG,"Attempting to offline the cluster resource: %s\n",resourceName.c_str());
									}
                                    else
                                        DebugPrintf(SV_LOG_DEBUG,"Attempting to offline the resource of type: %s\n",resourceType.c_str());
                                    LocalFree( resName );											

                                    WCHAR nNameRes[MAX_PATH];
                                    bytesReturned = sizeof(nNameRes);

									int nOfflineRetry = 0;
                                    while (nOfflineRetry < RESOURCE_OFFLINE_ATTEMPTS) {
                                        status = OfflineClusterResource(hResource);
										nOfflineRetry++;
                                        if (ERROR_SUCCESS != status) {										
											unsigned int retry = RESOURCE_OFFLINE_ATTEMPTS;
                                            while(GetClusterResourceState(hResource, NULL, NULL, NULL, NULL) == ClusterResourceOfflinePending)
                                            {
                                                if(0 == --retry)
												{
                                                    DebugPrintf(SV_LOG_DEBUG,"Reached maximum waiting for the cluster resource: %s offile\n",resourceType.c_str());
													break;
												}
                                                else
                                                    DebugPrintf(SV_LOG_DEBUG,"AWaiting for 3 seconds for the cluster resource: %s to go offile\n",resourceType.c_str());
                                                Sleep(3000);
												
                                            }
                                        }									
                                        if( GetClusterResourceState(hResource, NULL, NULL, NULL, NULL) == ClusterResourceOffline)
                                        {
                                            DebugPrintf(SV_LOG_DEBUG,"Successfully Offlined Cluster Resource : %s\n",resourceType.c_str());
                                            result = true;
                                            break;
                                        }
										Sleep(3000);
                                    }
                                }

                                if(bOnLine)
                                {

                                    resName = (WCHAR*) LocalAlloc( LPTR, resNameLen );
                                    if( ERROR_SUCCESS == ResUtilGetResourceName(hResource, resName, &resNameLen ))
									{
										std::string resourceName = W2A(resName);
                                        DebugPrintf(SV_LOG_DEBUG,"Attempting to online the cluster resource: %s\n",resourceName.c_str());
									}
                                    else
                                        DebugPrintf(SV_LOG_DEBUG,"Attempting to online the resource of type: %s\n",resourceType.c_str());
                                    LocalFree( resName );											

                                    WCHAR nNameRes[MAX_PATH];
                                    bytesReturned = sizeof(nNameRes);

									int nOnlineRetry = 0;
                                    while (nOnlineRetry < RESOURCE_ONLINE_ATTEMPTS) {
                                        status = OnlineClusterResource(hResource);
										nOnlineRetry++;
                                        if (ERROR_SUCCESS != status) {										
											unsigned int retry = RESOURCE_ONLINE_ATTEMPTS;
                                            while(GetClusterResourceState(hResource, NULL, NULL, NULL, NULL) == ClusterResourceOnlinePending)
                                            {
                                                if(0 == --retry)
												{
                                                    DebugPrintf(SV_LOG_DEBUG,"Reached maximum waiting for the cluster resource: %s to come online\n",resourceType.c_str());
													break;
												}
                                                else
                                                    DebugPrintf(SV_LOG_DEBUG,"AWaiting for 3 seconds for the cluster resource: %s to come online\n",resourceType.c_str());
                                                Sleep(3000);
                                            }
                                        }									
                                        if( GetClusterResourceState(hResource, NULL, NULL, NULL, NULL) == ClusterResourceOnline)
                                        {
                                            DebugPrintf(SV_LOG_DEBUG,"Successfully Onlined Cluster Resource : %s\n",resourceType.c_str());
                                            result = true;
                                            break;
                                        }
										Sleep(3000);
                                    }
                                }
                            }
                        }
                        CloseClusterResource(hResource);
                    }
                    ++clustIndex;
                }
            } while (ERROR_SUCCESS == status && result == false);

            ClusterCloseEnum(hClustEnum);
        }
        CloseCluster(hCluster);
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return result;
}

SVERROR ClusterOp::getClusterResources(std::list<RescoureInfo>& resouceInfoList)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE;
    if(getCLusterResourceInfo(std::string("")) != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to get the resources information.\n");
    }
    else
    {
        resouceInfoList = m_resouceInfoList;
        bRet = SVS_OK;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet;
}

bool ClusterOp::checkClusterResourceAvailable()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<RescoureInfo> resouceInfoList;
    unsigned int timeOut = 0;
    bool bResourceAvailable = false;
    while( !Controller::getInstance()->QuitRequested(10) )
    {
        if(getClusterResources(resouceInfoList) == SVS_OK)
        {
            bResourceAvailable = true;
            break;
        }
        else
        {
            timeOut += 10 ;
            if( timeOut > 600 )
            {
                DebugPrintf(SV_LOG_WARNING, "Timed-out at 600 seconds while waiting for the resource \n") ;
                break;
            }
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return  bResourceAvailable;
}