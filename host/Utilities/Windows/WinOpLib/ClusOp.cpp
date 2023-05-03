#include "stdafx.h"
#include<iostream>
#include <vector>
#include "ClusOp.h"

////**************************************************************************************************
                                           
////ClusOp is tool to access,modify,delete or create clusters on a node.It's also used to deal with
////the various Cluster objects like nodes,groups,networks and virtual servers.
////**************************************************************************************************/

/*List of Include Files*/

/*Link this module with the following libraries*/
#pragma comment(lib, "ClusApi.lib" )  //Cluster API Library
#pragma comment(lib, "ResUtils.lib" ) //Utility Library


ClusOp::ClusOp()
{
	m_bIsCluster = true;
	hCluster	= NULL;
	hNode		= NULL;
	ClusDocEx_DEFAULT_CB  = 256 * sizeof( WCHAR );
	dwIndex	= 0;
	cbNameSize = 0;
	cbResultSize = 0;
	lpcbResultSize = NULL;
	memset((void*)szName,0x00,512 * sizeof(WCHAR));

	cbOutBufferSize = 8192;//MCHECK a huge number to capture lists
	memset((void*)bOutBuffer,0x00,cbOutBufferSize);
	lpOutBuffer = (LPVOID)bOutBuffer;
	dwEnum	= ERROR_SUCCESS;
	cbEntrySize = 0;
	cchNameSize	= 0;
	cbNameAlloc	= ClusDocEx_DEFAULT_CB;
	dwType	= CLUSTER_NETWORK_ENUM_ALL;	
	hClusEnum = NULL;
	hResource = NULL;
	strVirtualServerName = "";
}

ClusOp::~ClusOp()
{
	if(hNode != NULL)
	{	
		CloseClusterNode(hNode);
	}
	if(hCluster != NULL)
	{	
		CloseCluster(hCluster);
	}
}

bool ClusOp::DetermineActiveness(LPWSTR lpszGroupName)
{
	USES_CONVERSION;
	bool status = false;
	bool clusterState = false;
	//Using the Resource Group Name, get the Handle to the Resource Group
	if(NULL == hCluster)
	{
		cout<<endl<<endl<<"[ERROR] Cluster Handle is corrupted.";
		return false;
	}
	//Open Cluster Node
	string strCompName;		
	DWORD cchNameSize =	MAX_COMPUTERNAME_LENGTH + 1;
	DWORD cchNodeName = MAX_PATH * sizeof(WCHAR);

	BOOL bComp = GetComputerName((LPSTR)strCompName.c_str(),&cchNodeName);
	if(FALSE == bComp)
	{
		cout<<endl<<endl<<"[ERROR]Could not get the name of the local computer.";
		return false;
	}
	LPWSTR lpszNodeName = (LPWSTR) LocalAlloc(LPTR,cchNameSize * sizeof(WCHAR));
	if(lpszNodeName != NULL)
	{	
		if(MultiByteToWideChar(CP_ACP, 0, strCompName.c_str(), -1, lpszNodeName, MAX_PATH) == 0)
		{
			cout<<endl<<endl<<"[Error] Could not convert the Node Name to Wide Character set.";			
			return false;
		}
		
		hNode = OpenClusterNode(hCluster,lpszNodeName);
		if(hNode == NULL)
		{	
			cout<<endl<<endl<<"[ERROR] Handle to the Node (hNode) is NULL.";
			return false;
		}
		else// Check if the Node is an Active Node or Passive Node of a Cluster
		{			
			CLUSTER_NODE_STATE clusNodeState;
			clusNodeState = GetClusterNodeState(hNode);
			
			//If Node is Up
			if (clusNodeState == ClusterNodeUp)
			{	
			}
			else
			{
				status = false;
				cout<<endl<<endl<<"[Error]: Current Node is not a part of the Cluster.";
				return false;
			}
		}
	}
	else
	{
		//Cleanup the Node
		cout<<endl<<endl<<"[ERROR]: Node name is Null.";
	}

	HGROUP hGroup = OpenClusterGroup(hCluster,lpszGroupName);
	if(NULL == hGroup)
	{
		cout<<endl<<endl<<"[ERROR]: Invalid Cluster Group Name.";
		return false;
	}
	else
	{	
		//cout<<endl<<endl<<"The Handle to the Cluster's Resource Group is :"<<hGroup;
		LocalFree(lpszNodeName);
		LPWSTR lpszGroupNodeName  =	(LPWSTR) LocalAlloc(LPTR,cchNameSize * sizeof(WCHAR));
		if(lpszGroupNodeName != NULL)
		{
			CLUSTER_GROUP_STATE cgs = GetClusterGroupState(hGroup,lpszGroupNodeName,&cchNameSize);
			switch(cgs)
			{
				case ClusterGroupStateUnknown:
				{
					cout<<endl<<endl<<"\tCluster Group State is Unknown.";
					status = false;
					break;
				}
				case ClusterGroupOnline:
				{
					cout<<endl<<endl<<"\tCluster Group State is Online.";					
					break;
				}
				case ClusterGroupOffline:
				{
					cout<<endl<<endl<<"\tCluster Group State is Offline.";
					status = false;
					break;
				}
				case ClusterGroupFailed:
				{
					cout<<endl<<endl<<"\tCluster Group State is Failed.";
					break;
				}
				case ClusterGroupPartialOnline:
				{
					cout<<endl<<endl<<"\tCluster Group State is PartialOnline.";
					break;
				}
				case ClusterGroupPending:
				{
					cout<<endl<<endl<<"\tCluster Group State is Pending.";
					break;
				}
			}			
			cout<<endl<<endl<<"\tCluster Resource Group's Owner Node: \t"<<W2A(lpszGroupNodeName);

			if(TRUE == bComp)
			{
				cout<<endl<<endl<<"\tThe Host Name of the Node          :\t"<<strCompName.c_str();
		
				if(stricmp(strCompName.c_str(),W2A(lpszGroupNodeName)) == 0)
				{
					//cout<<endl<<endl<<"Active Node";
					status = true;
				}
				else
				{
					//cout<<endl<<endl<<"Inactive Node.";
					status = false;
				}
			}
			LocalFree(lpszGroupNodeName);
		}
		else
		{
			cout<<endl<<endl<<"[ERROR]: Could not allocate memory to store Computer Name.";
		}
	}
	return status;
}

int ClusOp::CheckClustActiveness(string strVirSer)
{
	int nResult = ERROR_SUCCESS;
	bool bStatus = FindResGroupForVServer(strVirSer,CLUSTER_NETWORK_NAME_RESOURCE_TYPE,CLUSTER_RESOURCE_NETOWRK_NAME_PROPERTY);
	if(bStatus == true)
	{
		bStatus = DetermineActiveness(szName);
	}
	if(true == bStatus)
	{
		nResult = 0;
	}
	else
	{
		if(false == m_bIsCluster)
		{
			nResult = -1;
		}
		else
		{
			nResult = 1;
		}
	}
	return nResult;
}

/*
Name		: FindResGroupForVServer
Description : Find the group in which Network Name resides.
			  e.g:	bFlag = processCluster(	strHost,
									CLUSTER_NETWORK_NAME_RESOURCE_TYPE,
									CLUSTER_RESOURCE_NETOWRK_NAME_PROPERTY,
									FIND_VIRTUAL_SERVER_GROUP);
*/
bool ClusOp ::FindResGroupForVServer(const string &strHost,const string resourceType,const string strPropertyToBeFetched)
{
	USES_CONVERSION;
	strVirtualServerName = strHost;    
    HGROUP    hGroup   = NULL;
    HRESOURCE hResource = NULL;
    HCLUSENUM hEnum;
    HGROUPENUM hGroupEnum;
	bool result = false;
    
	DWORD status = ERROR_SUCCESS;
    DWORD chName;
    DWORD chNameGroup;    
    WCHAR szNameGroup[9000];
    DWORD dwType;
    DWORD dwIndex  = 0;
    DWORD dwIndexGroup = 0;
    DWORD dwIndexResource = 0;
    DWORD cbNameSize  = 0;
    DWORD    dwCluster          = ERROR_SUCCESS;
    DWORD    dwResult           = ERROR_SUCCESS;
    DWORD    dwResultGroup      = ERROR_SUCCESS;
    DWORD    dwResultResource   = ERROR_SUCCESS;
    DWORD    cchNameSize        = MAX_COMPUTERNAME_LENGTH + 1;
    LPWSTR   lpszClusterName    = (LPWSTR) LocalAlloc(LPTR,cchNameSize * sizeof( WCHAR));
	LPWSTR   lpszClusterGrpName = (LPWSTR) LocalAlloc(LPTR,(strHost.length()) * sizeof( WCHAR));

	if(lpszClusterName != NULL )
    {

		if(MultiByteToWideChar(CP_ACP, 0, strHost.c_str(), -1, lpszClusterName, MAX_PATH) == 0)
		{
			cout<<"\n[ERROR]:  MultiByteToWideChar Failed.Cannot Proceed further.";
			cout<<"\t[Error :]"<<GetLastError();
			return false;
		}
        
		hCluster = OpenCluster(lpszClusterName );
		if(hCluster == NULL)
		{
            printf("\n\tCluster not found..");
			printf("\tError:[%d]",GetLastError());
			m_bIsCluster = false;
			return false;
        }
		else
		{
            hEnum = ClusterOpenEnum(hCluster,CLUSTER_ENUM_GROUP);
            if(hEnum == NULL)
			{
               printf("\n\tclusteropenenum failed");
     		   printf("\tError:[%d]",GetLastError());
            }
			else
			{
               while( dwResult == ERROR_SUCCESS )
               {
                   chName = sizeof (szName);
				   dwType = 4;// CLUSTER_ENUM_GROUP
                   dwResult = ClusterEnum(hEnum,dwIndex,&dwType, szName,&chName);
                   if(dwResult == ERROR_SUCCESS)
				   {
                      	//found Groups						
						//-----------------------GROUP START--------------------
                        hGroup = OpenClusterGroup(hCluster,szName);
                        if(hGroup == NULL){
                               printf("\n\tUnable to open the group");
     				   		   printf("\tError:[%d]",GetLastError());
						       result = false;
							   break;
                         }
						 else
						 {
							//cout<<endl<<endl<<"OpenClusterGroup = "<<W2A(szName);							
                            hGroupEnum = ClusterGroupOpenEnum(hGroup,CLUSTER_GROUP_ENUM_ALL);
                            if(hGroupEnum == NULL)
							{
                                printf("\n\tClusterGroupOpenEnum failed");
								printf("\tError:[%d]",GetLastError());
								result = false;
								break;
                            }
							else
							{
                               dwResultGroup = ERROR_SUCCESS;
                               dwIndexGroup = 0;
                               while( dwResultGroup == ERROR_SUCCESS )
                               {
                                   chNameGroup = sizeof (szNameGroup);
                                   dwResultGroup = ClusterGroupEnum(hGroupEnum,dwIndexGroup,&dwType, szNameGroup,&chNameGroup);
                                   if(dwResultGroup == ERROR_SUCCESS)
                                   {
                                        hResource = OpenClusterResource(hCluster,szNameGroup);
                                         if(hResource == NULL)
									     {
											 //printf("\n\tError:Unable to open resource %s and the error is %d",szNameGroup,GetLastError());
											 //result = false;
											 break;
                                          }
										  else
										  {
										    	bool bOffline ;
												WCHAR *resName;
												DWORD resNameLen = MAX_PATH;
												DWORD bytesReturned;
												DWORD cbBuffSize = 512;
												LPVOID lpszType;
												lpszType = LocalAlloc( LPTR, cbBuffSize );

												if( NULL == lpszType )
												{											
													status = GetLastError();
													cout << "[ERROR][" << status << "] Local buffer allocation failure...\n";	
													result = false;
													break;
												}
	    										status = ClusterResourceControl(hResource,	NULL,CLUSCTL_RESOURCE_GET_RESOURCE_TYPE, NULL,0,(LPVOID)lpszType,cbBuffSize,&bytesReturned);
												if(ERROR_MORE_DATA == status )
												{
													LocalFree( lpszType );

													cbBuffSize = bytesReturned;
			
													lpszType = LocalAlloc( LPTR, cbBuffSize );
													if( NULL == lpszType )
													{											
														status = GetLastError();
														cout << "[ERROR][" << status << "] Local buffer allocation failure...\n";	
														result = false;
														break;
													}
													status = ClusterResourceControl(hResource,NULL,CLUSCTL_RESOURCE_GET_RESOURCE_TYPE,NULL,0,(LPVOID)lpszType,cbBuffSize,&bytesReturned);
												}
												if (ERROR_SUCCESS != status)
												{
													cout << "[ERROR][" << status << "] ApplicationFailover::OfflineResource:ClusterResourceControl failed.\n" 
														<< "\nFailed to get the resources of type: " << resourceType.c_str() << std::endl;
													result = false;
													break;
												} 
												else
												{	
													if(strcmpi(W2A((WCHAR*)lpszType), resourceType.c_str()) == 0)
													{
														bOffline = true;								
														LPWSTR pszPropertyVal = NULL;
														LPWSTR pszShareName = NULL;
														cbBuffSize = bytesReturned;//512;
														LPVOID lpOutBuffer = NULL;
														LPWSTR pszPropertyName = NULL;
														pszPropertyName = L"NetworkName";
														lpOutBuffer = LocalAlloc( LPTR, cbBuffSize );
														if(NULL == lpOutBuffer )
														{										
															status = GetLastError();
															cout << "[ERROR][" << status << "] Local buffer allocation failure...\n";
															result = false;
															break;
														}
														status = ClusterResourceControl(hResource, NULL,CLUSCTL_RESOURCE_GET_PRIVATE_PROPERTIES, NULL,0,lpOutBuffer,cbBuffSize,&bytesReturned);
														if( ERROR_MORE_DATA == status )
														{
															LocalFree( lpOutBuffer );
															cbBuffSize = bytesReturned;
														
															lpOutBuffer = LocalAlloc( LPTR, cbBuffSize );
															if( NULL == lpOutBuffer )
															{											
																status = GetLastError();
																cout << "[ERROR][" << status << "] Local buffer allocation failure...\n";	
																result = false;
																break;
															}
															status = ClusterResourceControl(hResource,NULL,CLUSCTL_RESOURCE_GET_PRIVATE_PROPERTIES,NULL,0,lpOutBuffer,cbBuffSize,&bytesReturned);
														}
														if(ERROR_SUCCESS != status)
														{	
															cout << "[ERROR][" << status << "] Failed to get the private properties of the resource of type : " 
																<< resourceType.c_str() << endl;
															LocalFree( resName );
														}
														else //if status = ERROR_SUCCESS
														{			
															resName = (WCHAR*) LocalAlloc( LPTR, resNameLen ); 
															status = ResUtilFindSzProperty(lpOutBuffer, cbBuffSize, A2W(strPropertyToBeFetched.c_str()) ,&pszPropertyVal);
														
															if(ERROR_SUCCESS != status )
															{
																if(ERROR_INVALID_DATA == status)
																{
																	cout<<"\n invalid data...\n";
																}
																if(ERROR_NOT_ENOUGH_MEMORY == status)
																{
																	cout<<"\n not enough Memory.\n";
																}

																if(ERROR_FILE_NOT_FOUND == status)
																{
																	cout<<"\n File not found....\n";
																}
															}
															else
															{
																if(((strcmpi(W2A((WCHAR*)pszPropertyVal), strVirtualServerName.c_str()) == 0)) ||
																	(strcmpi(W2A(resName),strVirtualServerName.c_str())== 0))
																{	
																	ClusterResourceGroup = szName;
																	cout<<endl<<endl<<"\tVirtual server "<<"[" <<strVirtualServerName.c_str()<<"]"<< " resides in Group :"<<W2A(ClusterResourceGroup.c_str());
																	result = true;
																	dwResult = 1;
																	break;//This break is vital, otherwise,the tool will give wrong result.
																}													
															}
															LocalFree( resName );
															LocalFree( lpOutBuffer );
														}//end of else case for (if ERROR_SUCCESS == status)
													}//if( ERROR_MORE_DATA == status )
												}//end of if(strcmpi(W2A((WCHAR*)lpszType), resourceType.c_str()) == 0)
												CloseClusterResource(hResource);
											}//end of status == ERROR_SUCCESS for CLUSCTL_RESOURCE_GET_RESOURCE_TYPE
                                        }
										else if(dwResultGroup == ERROR_NO_MORE_ITEMS)
										{
                                            //printf("\n\t\tno more items ");
                                        }
										else if( dwResultGroup == ERROR_MORE_DATA)
										{
                                            //printf("\n\t\tmore data");
                                        }
										else
										{
                                            printf("\n\t\t\tdwResult empty");
                                        }
                                        dwIndexGroup++;
                                    }//while
                                }//else
								
                            }//else
//-------------------------GROUP END---------------------
                        }
                        dwIndex++;
                    }//while
					
				}
				ClusterCloseEnum(hEnum);
         }
		dwResult = GetLastError();
		LocalFree( lpszClusterName );
    }
    SetLastError( dwResult );
	return result;
}
void ClusOp::ClusOp_Usage()
{
	printf("Usage:\n");
	printf("Winop.exe Cluster -CheckActiveNode <virtual server name>");
}

int ClusOpMain(int argc, char* argv[])
{
	ClusOp co;
	int nStatus = -1;
	int iParsedOptions = 2;
	string strVirtualServerName;	
	int nArgCount = argc;
	if(nArgCount <=2)
	{
		co.ClusOp_Usage();
		return -2;//Invalid Usage
	}
	for (;iParsedOptions < nArgCount; iParsedOptions++)
    {
        if((argv[iParsedOptions][0] == '-') || (argv[iParsedOptions][0] == '/'))
        {
			if(stricmp((argv[iParsedOptions]+1),OPTION_CHECK_ACTIVE_NODE) == 0)
			{
				iParsedOptions++;
				strVirtualServerName = argv[iParsedOptions];
				break;
			}
			else
			{
				printf("Incorrect argument passed\n");
				co.ClusOp_Usage();
				return -2;
			}
		}
		else
		{
			printf("Incorrect argument passed\n");
			co.ClusOp_Usage();
			return -2;
		}
	}

	cout<<endl<<"\tChecking if the current Node is Active/Passive Node of the cluster...\n\n";
	//for(int i = 1; i < (nArgCount); i++)
	//{
		//strVirtualServerName = argv[i];
		//cout<<endl<<"************************************************************************\n";
		cout<<endl<<"\tFor Virtual Server: "<<strVirtualServerName.c_str();
		nStatus = co.CheckClustActiveness(strVirtualServerName);
		if(nStatus == 0)
		{
			cout<<endl<<endl<<"\tCluster Node is an ACTIVE Node.";
		}
		else if(nStatus == 1)
		{
			cout<<endl<<endl<<"\tCluster Node is a PASSIVE Node.";
		}
		else if(nStatus = -1)
		{
			cout<<endl<<endl<<"\t The node is not a part of any Cluster.";
		}
		else if (nStatus = -2)
		{
			cout<<endl<<endl<<"\t Invalid usage.";
		}
		//cout<<endl<<"************************************************************************\n";
	//}
	return nStatus;
}
