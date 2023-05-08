#include "fileserverdiscovery.h"
#include "host.h" 
#ifndef _WINDOWS_
#include <Windows.h>
#endif
#include <lm.h>
#include <winsock.h>
#include <atlconv.h>
#include <set>
#include <sstream>
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include <ACE/File_Lock.h>
#include <boost/shared_array.hpp>
#include "portablehelpersmajor.h"
#include "system.h"
#include "config/applocalconfigurator.h"
#include "appcommand.h"
#include "util/common/util.h"
#include "service.h"
#include "controller.h"
#include "inmsafeint.h"
#include "inmageex.h"


#define INM_TYPE_BUFFER_LEN 1024

SVERROR FileServerDiscoveryImpl::discoverFileServer(std::map<std::string, FileServerInstanceDiscoveryData>& discoveryMap, std::map<std::string, FileServerInstanceMetaData>&instanceMap)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    
	SVERROR retErrorValue = SVS_OK;
	AppLocalConfigurator configurator ;
	std::string regExportPath = configurator.getApplicationPolicyLogPath();
	regExportPath = regExportPath + "FSExport.reg";
	exportRegistry( regExportPath );
	if( isClusterNode() )
	{
		OSVERSIONINFOEX osVersionInfo ;
		getOSVersionInfo(osVersionInfo) ;
		if(osVersionInfo.dwMajorVersion == 6) //making this condition true for windows 2008 sp1,sp2 and R2
		{
			retErrorValue = discoverW2k8ClusterShareInfo( discoveryMap, instanceMap );	
		}
		else
		{
			retErrorValue = discoverClusterShareInfo( discoveryMap, instanceMap );
		}
	}
	discoverNonClusterShareInfo( discoveryMap, instanceMap);
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retErrorValue;
}

SVERROR FileServerDiscoveryImpl::exportRegistry( const std::string& filePath )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retErrorValue = SVS_OK;
	std::stringstream cmdStream;
	cmdStream << "regedit.exe /S /E /A " << "\""<< filePath << "\""<< " " << LANMANSERVER_SHARE_KEY;
	std::string cmd = cmdStream.str();
	AppCommand appCmd( cmd, 120 );
	SV_ULONG exitCode;
	std::string outPut;
	HANDLE h = NULL;
	h = Controller::getInstance()->getDuplicateUserHandle();
	if( appCmd.Run( exitCode, outPut, Controller::getInstance()->m_bActive, "", h ) != SVS_OK )
	{
		DebugPrintf(SV_LOG_ERROR, "Failed To Export Registry key: %s to FilePath: %s\n", LANMANSERVER_SHARE_KEY, filePath.c_str() );
		retErrorValue = SVS_FALSE;
	}
	if(h)
	{
		CloseHandle(h);
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retErrorValue;
}

SVERROR FileServerDiscoveryImpl::getRegistryVersionData( const std::string& totalData, std::string& registryVersion )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retErrorValue = SVS_OK;
	std::string tempAllShareData = totalData;
	std::string::size_type index = tempAllShareData.find("[");
	if( index != std::string::npos )
	{
		registryVersion = tempAllShareData.substr(0, index);
	}
	else
	{
		DebugPrintf( SV_LOG_ERROR, "Failed to get the registry version \n" );
		retErrorValue = SVS_FALSE;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retErrorValue;	
}

SVERROR FileServerDiscoveryImpl::getShareData( const std::string& shareName, const std::string& totalData, std::string& sharesAscii )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retErrorValue = SVS_FALSE;
	std::string tempAllShareData = totalData;
	boost::algorithm::to_upper( tempAllShareData );
	std::string tempShareName = "\"";
	tempShareName += shareName;
	boost::algorithm::to_upper( tempShareName );
	tempShareName += "\"";
	std::string::size_type shareIndex = tempAllShareData.find(tempShareName);
	if( shareIndex != std::string::npos )
	{
		std::string::size_type colonIndex = tempAllShareData.find_first_of( "=", shareIndex );
		if( colonIndex != std::string::npos )
		{
			std::string::size_type index = tempAllShareData.find_first_of( "\"[", colonIndex );
			int no_ofElements = index - colonIndex - 1;
			sharesAscii = totalData.substr( colonIndex+1, no_ofElements );
			retErrorValue = SVS_OK;
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retErrorValue;	
}

SVERROR FileServerDiscoveryImpl::getSecurityData( const std::string& shareName, const std::string& totalData, std::string& securityAscii )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retErrorValue = SVS_FALSE;
	std::string tempAllShareData = totalData;
	boost::algorithm::to_upper( tempAllShareData );
	std::string tempShareName = "\"";
	tempShareName += shareName;
	boost::algorithm::to_upper( tempShareName );
	tempShareName += "\"";
	std::string::size_type shareIndex = tempAllShareData.find(tempShareName);
	if( shareIndex != std::string::npos )
	{
		shareIndex = tempAllShareData.find(tempShareName, shareIndex+1);
		std::string::size_type colonIndex = tempAllShareData.find_first_of( "=", shareIndex );
		if( colonIndex != std::string::npos )
		{
			std::string::size_type index = tempAllShareData.find_first_of( "\"", colonIndex );
			int no_ofElements;
			if( index != std::string::npos )
			{
				no_ofElements = index - colonIndex - 1;
			}
			else
			{
				no_ofElements = tempAllShareData.size() - colonIndex - 1;
			}
			securityAscii = totalData.substr( colonIndex+1, no_ofElements );
			retErrorValue = SVS_OK;
		}
	}	
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retErrorValue;	
}

SVERROR FileServerDiscoveryImpl::discoverNonClusterShareInfo(std::map<std::string, FileServerInstanceDiscoveryData>&discoveryMap, std::map<std::string, FileServerInstanceMetaData>&instanceMap)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retErrorValue = SVS_OK;
	NET_API_STATUS netAPIStatusValue;
	PSHARE_INFO_502 ptrSHARE_INFO_502, ptrSHARE_INFO_502_Buffer;
	DWORD dwEntriesRead = 0, dwTotalEntries = 0,dwresume = 0 ;
	std::string ipAddress = Host::GetInstance().GetIPAddress();
    FileServerInstanceDiscoveryData discoveryData ;
    FileServerInstanceMetaData instanceMetaData ;
	instanceMetaData.m_isClustered = false;
    USES_CONVERSION;
	std::string hostName = Host::GetInstance().GetHostName() ;
    boost::algorithm::to_lower(hostName) ;
	InmService lanmanService("lanmanserver");
	if(QuerySvcConfig(lanmanService) == SVS_OK && lanmanService.m_svcStatus != INM_SVCSTAT_RUNNING)
	{
		discoveryData.m_ErrorCode = DISCOVERY_METADATANOTAVAILABLE;
		discoveryData.m_ErrorString = "Failed to discover sharenames, becuase Lanaman Service is not runnning.";
		discoveryMap.insert(std::make_pair(hostName, discoveryData)) ;	

		instanceMetaData.m_ErrorCode = DISCOVERY_METADATANOTAVAILABLE;
		instanceMetaData.m_ErrorString = "Failed to discover sharenames, becuase Lanaman Service is not runnning.";
		instanceMap.insert(std::make_pair(hostName, instanceMetaData));
		DebugPrintf(SV_LOG_INFO, "Lanman service is not running. so not able to discover Local Shares. \n");
		retErrorValue = SVS_FALSE;
	}
	else
	{
		discoveryData.m_ips.push_back(ipAddress) ;					

		bool bfoundNonclusterShare = false ;
		std::string systemDrive ;
		getEnvironmentVariable("SystemDrive", systemDrive) ;
		if( systemDrive.size() == 2 )
		{
			systemDrive += "\\" ;
		}
		else if( systemDrive.size() == 1 )
		{
			systemDrive += ":\\" ;
		}

		try
		{
			do
			{
				netAPIStatusValue = NetShareEnum ( NULL, 502, (LPBYTE *) &ptrSHARE_INFO_502_Buffer, -1, (LPDWORD)&dwEntriesRead, (LPDWORD)&dwTotalEntries, (LPDWORD)&dwresume );
				if( netAPIStatusValue == ERROR_SUCCESS || netAPIStatusValue == ERROR_MORE_DATA )
				{
					ptrSHARE_INFO_502 = ptrSHARE_INFO_502_Buffer;
					DWORD dwIndex;

					for( dwIndex = 1; dwIndex <= dwEntriesRead; dwIndex++ )
					{
						if( ptrSHARE_INFO_502->shi502_type == STYPE_DISKTREE )
						{
							FileShareInfo sh_info;
							sh_info.m_absolutePath = W2A( ptrSHARE_INFO_502->shi502_path );  
							sh_info.m_shareName = W2A( ptrSHARE_INFO_502->shi502_netname );
							wchar_t wzVolumeorMountPtName[512];
							GetVolumePathNameW( (ptrSHARE_INFO_502->shi502_path), wzVolumeorMountPtName ,512 );
							sh_info.m_mountPoint =  W2A( wzVolumeorMountPtName );
							sh_info.m_UNCPath = "\\\\" + hostName + "\\" + sh_info.m_shareName  ;
							std::string shareName = sh_info.m_shareName;
							size_t found;
							found = shareName.find_last_of("$");
							size_t size = shareName.size();
							if( isAlreadyDiscovered(shareName, instanceMap) == false )
							{
								if( found == std::string::npos || found != size - 1 )
								{
									if( fillSharesAndSecurityDataFromReg( sh_info ) == SVS_OK )
									{
										if( strcmpi(sh_info.m_mountPoint.c_str(), systemDrive.c_str()) != 0 )
										{
											instanceMetaData.m_shareInfo.insert(make_pair(sh_info.m_shareName, sh_info)) ;
											if( find(instanceMetaData.m_volumes.begin(), instanceMetaData.m_volumes.end(), sh_info.m_mountPoint) == 
												instanceMetaData.m_volumes.end() )
											{
												instanceMetaData.m_volumes.push_back(sh_info.m_mountPoint) ;
											}
											bfoundNonclusterShare = true;
										}
									}
								}
							}
						}
						ptrSHARE_INFO_502++;
					}
					if( bfoundNonclusterShare )
					{
						instanceMap.insert(std::make_pair(hostName, instanceMetaData)) ;
						discoveryMap.insert(std::make_pair(hostName, discoveryData)) ;
						retErrorValue = SVS_OK;
					}
					else
					{
						discoveryData.m_ErrorCode = DISCOVERY_SUCCESS;
						discoveryData.m_ErrorString = "No shares are available on the local host.";
						discoveryMap.insert(std::make_pair(hostName, discoveryData)) ;	
						instanceMetaData.m_ErrorCode = DISCOVERY_SUCCESS;
						instanceMetaData.m_ErrorString = "No shares are available on the local host";
						instanceMap.insert(std::make_pair(hostName, instanceMetaData));
						DebugPrintf(SV_LOG_INFO, "Lanman service is not running. so not able to discover Local Shares. \n");
						retErrorValue = SVS_FALSE;
					}
					NetApiBufferFree( ptrSHARE_INFO_502_Buffer );
				}
				else 
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to enumarate shared Resources [ ERRORVALUE: %d ] \n", GetLastError() ) ;
					DebugPrintf(SV_LOG_ERROR, " Please Make sure to be running lanman service Because:");
					DebugPrintf(SV_LOG_INFO, "Supports file, print, and named-pipe sharing over the network for this computer. If this service is stopped, these functions will be unavailable. If this service is disabled, any services that explicitly depend on it will fail to start... \n" ) ;
					NetApiBufferFree( ptrSHARE_INFO_502_Buffer );
					retErrorValue = SVS_FALSE;
				}
			}while( netAPIStatusValue == ERROR_MORE_DATA );
		}
		catch(...)
		{
			DebugPrintf(SV_LOG_ERROR,"Got Exception while discovering non-cluster Shares\n");
			discoveryData.m_ErrorCode = DISCOVERY_FAIL;
			instanceMetaData.m_ErrorCode = DISCOVERY_FAIL;
			discoveryData.m_ErrorString = "Got Exception while discovering non-cluster Shares";
			discoveryMap.insert(std::make_pair(hostName, discoveryData)) ;
			instanceMetaData.m_ErrorString = "Got Exception while discovering non-cluster Shares";
			instanceMap.insert(std::make_pair(hostName, instanceMetaData));
			retErrorValue = SVS_FALSE;
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retErrorValue ;
}

SVERROR FileServerDiscoveryImpl::discoverClusterShareInfo(std::map<std::string, FileServerInstanceDiscoveryData>&discoveryMap, std::map<std::string, FileServerInstanceMetaData>&instanceMap)
{	
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	USES_CONVERSION;
	SVERROR retErrorValue = SVS_FALSE ;
	std::list<std::string> fileSharesResourceNames;
	std::stringstream query;
    CLUSTER_RESOURCE_STATE state;
	std::string hostName = Host::GetInstance().GetHostName();
    std::string groupName, ownerNodeName;
	try
	{
		if(  getResourcesByType("", "File Share", fileSharesResourceNames) == SVS_OK )
		{
			std::string clusterName, clusterIP;
			if( getClusterName( clusterName ) == true && getClusterIpAddress("", clusterIP ) == SVS_OK )
			{
				
				std::list<std::string>::iterator fileSharesResourceNamesIter = fileSharesResourceNames.begin();
				
				std::list<std::string> fileshareKeyList;
				fileshareKeyList.push_back("Sharename");
				fileshareKeyList.push_back("Path");

				std::list<std::string> networknameKeyList;
				networknameKeyList.push_back("Name");

				std::map< std::string, std::string> outputPropertyMap;
				
				std::list<std::string> dependedNameList ;
				
				while( fileSharesResourceNamesIter != fileSharesResourceNames.end() )
				{                
					bool bPassiveInstance = false;
					if( getResourceState("",*fileSharesResourceNamesIter, state, groupName, ownerNodeName ) == SVS_OK )
					{
						if( strcmpi(hostName.c_str(), ownerNodeName.c_str()) != 0)
						{
							/*fileSharesResourceNamesIter++;	
							continue;*/
							bPassiveInstance = true;
						}
					}
					else
					{
						DebugPrintf(SV_LOG_ERROR, "getResourceState failed\n") ;
					}
					std::string networkname ;
					DebugPrintf(SV_LOG_DEBUG, "FileShare ResourceName: %s \n",fileSharesResourceNamesIter->c_str() );
				
					// Step2) Finding the "ShareName" and "Path" (Private Properties) of the SHARED RESOURCE NAME.

					outputPropertyMap.clear();

					if( getResourcePropertiesMap( "", *fileSharesResourceNamesIter, fileshareKeyList, outputPropertyMap ) != SVS_OK || 
						outputPropertyMap.empty())
					{
						DebugPrintf(SV_LOG_ERROR, "Failed to get The Properties of resource %s \n", fileSharesResourceNamesIter->c_str() );
					}
					else
					{
						FileShareInfo shareInfo;
						if( outputPropertyMap.find("Sharename") != outputPropertyMap.end() )
						{	
							shareInfo.m_shareName = outputPropertyMap.find("Sharename")->second;
						}
						if( outputPropertyMap.find("Path") != outputPropertyMap.end() )
						{
							shareInfo.m_absolutePath = outputPropertyMap.find("Path")->second;
						}
						
						char wzVolumeorMountPtName[512];
						// PR#10815: Long Path support
                        SVGetVolumePathName(shareInfo.m_absolutePath.c_str(), wzVolumeorMountPtName, ARRAYSIZE(wzVolumeorMountPtName));
						shareInfo.m_mountPoint = wzVolumeorMountPtName ;

						// Step3) Finding the RESOURCE NAME of depended "Network Name" if any.

						dependedNameList.clear();
						std::list<std::string> ipaddresses ;
						if( getDependedResorceNameListOfType( *fileSharesResourceNamesIter, "Network Name", dependedNameList ) == SVS_OK && dependedNameList.empty() == false )
						{
							std::string dependedNetworkName = *( dependedNameList.begin());
							outputPropertyMap.clear();

							// Step4) Finding the Actual "Name" property of the caliculated Network Name resources's Resource Name.

							getResourcePropertiesMap( "",dependedNetworkName , networknameKeyList, outputPropertyMap ) ;
							
							if( outputPropertyMap.empty() == false )
							{
								networkname = outputPropertyMap.begin()->second;
							}
							dependedNameList.clear();
							query.str("");
							query << "SELECT * FROM MSCluster_ResourceToDependentResource where Antecedent = 'MSCluster_Resource.Name=\\'" << dependedNetworkName << "\\''";
							getDependencyList( query.str(), ipaddresses );                        
						}
						else if( dependedNameList.empty() )
						{
							DebugPrintf(SV_LOG_INFO, " File server: %s does not have any net work name dependency \n", fileSharesResourceNamesIter->c_str() );
							networkname = clusterName ;
							ipaddresses.push_back( clusterIP.c_str() ) ;
						}
						else
						{
							DebugPrintf(SV_LOG_ERROR, "Failed to get The depended resource names %s \n", fileSharesResourceNamesIter->c_str() );
						}
						//mapClusterShareInfo.insert(std::make_pair(shareInfo.m_shareName, shareInfo) );
						FileServerInstanceDiscoveryData discoveryData ;                     
						shareInfo.m_UNCPath = "\\\\" + networkname + "\\" + shareInfo.m_shareName ;
						if( discoveryMap.find(networkname) == discoveryMap.end() )
						{
							discoveryData.m_ips = ipaddresses ;
							discoveryMap.insert(std::make_pair(networkname, discoveryData)) ;
						}
						if( fillSharesAndSecurityDataFromReg( shareInfo ) == SVS_OK )
						{
							if( instanceMap.find(networkname) == instanceMap.end() )
							{
								FileServerInstanceMetaData instanceMetaData ;
									if( bPassiveInstance )
									{
										instanceMetaData.m_ErrorCode = PASSIVE_INSTANCE;
										instanceMetaData.m_ErrorString = "PASSIVE_INSTANCE";
									}
								instanceMetaData.m_shareInfo.insert(make_pair(shareInfo.m_shareName, shareInfo)) ;
								if( find(instanceMetaData.m_volumes.begin(), instanceMetaData.m_volumes.end(), 
									shareInfo.m_mountPoint) == instanceMetaData.m_volumes.end() )
								{
									instanceMetaData.m_volumes.push_back(shareInfo.m_mountPoint) ;
								}
								instanceMap.insert(std::make_pair(networkname, instanceMetaData)) ;
								retErrorValue = SVS_OK;
							}
							else
							{
								FileServerInstanceMetaData& instanceMetaData = instanceMap.find(networkname)->second ;
								if( bPassiveInstance )
								{
								instanceMetaData.m_ErrorCode = PASSIVE_INSTANCE;
								instanceMetaData.m_ErrorString = "PASSIVE_INSTANCE";
								}
								instanceMetaData.m_shareInfo.insert(make_pair(shareInfo.m_shareName, shareInfo)) ;
								if( find(instanceMetaData.m_volumes.begin(), instanceMetaData.m_volumes.end(), 
									shareInfo.m_mountPoint) == instanceMetaData.m_volumes.end() )
								{
									instanceMetaData.m_volumes.push_back(shareInfo.m_mountPoint) ;
								}
								retErrorValue = SVS_OK;
							}
						}
						else
						{
							DebugPrintf(SV_LOG_DEBUG, "Failed to get share's security info. \n");
							retErrorValue = SVS_FALSE;
						}
					}
					fileSharesResourceNamesIter++;	
				}
			}
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to get The resource names of File shares\n");
			retErrorValue = SVS_FALSE;
		}
	}
	catch(...)
	{
		DebugPrintf(SV_LOG_ERROR,"Got Exception while discovering cluster Shares\n");
		FileServerInstanceDiscoveryData discoveryData;
		FileServerInstanceMetaData instanceMetaData ;
		discoveryData.m_ErrorCode = DISCOVERY_FAIL;
		instanceMetaData.m_ErrorCode = DISCOVERY_FAIL;
		discoveryData.m_ErrorString = "Got Exception while discovering cluster Shares";
		discoveryMap.insert(std::make_pair(hostName, discoveryData)) ;
		instanceMetaData.m_ErrorString = "Got Exception while discovering cluster Shares";
		instanceMap.insert(std::make_pair(hostName, instanceMetaData));
		retErrorValue = SVS_FALSE;	
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retErrorValue ;
}

SVERROR FileServerDiscoveryImpl::discoverW2k8ClusterShareInfo(std::map<std::string, FileServerInstanceDiscoveryData>&discoveryMap, std::map<std::string, FileServerInstanceMetaData>&instanceMap)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	USES_CONVERSION;
	bool bPassiveInstance = false;
	std::map<std::string,bool> mapPassiveInstance;
	SVERROR retErrorValue = SVS_OK ;
	std::stringstream query;
	std::list<std::string> fileServerResourceNames;
	CLUSTER_RESOURCE_STATE state;
	std::string hostName = Host::GetInstance().GetHostName();
    std::string groupName, ownerNodeName;
	std::map<std::string, w2k8networkDetails> mapFsDskWithNetWork;

	OSVERSIONINFO osvi;
	ZeroMemory(&osvi,sizeof(OSVERSIONINFO));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	if(!GetVersionEx(&osvi))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to get the OS Version informaiton. Error %d\n",GetLastError());
	}

	try
	{
		if( getResourcesByType("", "File Server", fileServerResourceNames) == SVS_OK )
		{
			std::string clusterName, clusterIP;		
			if( getClusterName( clusterName ) == true && getClusterIpAddress("", clusterIP ) == SVS_OK )
			{			
				std::list<std::string>::iterator fileServerResourceNamesIter = fileServerResourceNames.begin();

				std::list<std::string> networknameKeyList;
				networknameKeyList.push_back("Name");

				std::list<std::string> phyDiskKeyList;
				phyDiskKeyList.push_back("Physical Disk");

				std::map< std::string, std::string> outputPropertyMap;
				
				std::list<std::string> dependedNameList;
				std::list<std::string> dependedPhyDskList;
				
				while( fileServerResourceNamesIter != fileServerResourceNames.end() )
				{ 
					bPassiveInstance = false;
					w2k8networkDetails w2k8NetIp;
					std::string networkname;
					std::string physicalDisk;
					if( getResourceState("",*fileServerResourceNamesIter, state, groupName, ownerNodeName ) == SVS_OK )
					{
						if( strcmpi(hostName.c_str(), ownerNodeName.c_str()) != 0)
						{
							bPassiveInstance = true;
						}
					}
					else
					{
						DebugPrintf(SV_LOG_ERROR, "getResourceState failed\n") ;
					}
					dependedNameList.clear();
					std::list<std::string> ipaddresses ;
					if( getDependedResorceNameListOfType( *fileServerResourceNamesIter, "Network Name", dependedNameList ) == SVS_OK && dependedNameList.empty() == false )
					{
						std::string dependedNetworkName = *( dependedNameList.begin());
						outputPropertyMap.clear();

						getResourcePropertiesMap( "",dependedNetworkName , networknameKeyList, outputPropertyMap);
						
						if( outputPropertyMap.find("Name") != outputPropertyMap.end() )
						{	
							networkname = outputPropertyMap.find("Name")->second;
						}
						if( getDependedResorceNameListOfType( dependedNetworkName, "IP Address", ipaddresses ) == SVS_OK && ipaddresses.empty() == false )
						{
							DebugPrintf(SV_LOG_INFO, "File server: %s Network name is: %s\n", fileServerResourceNamesIter->c_str(),  networkname.c_str());
							std::list<std::string>::iterator iterListIp;
							for(iterListIp = ipaddresses.begin(); iterListIp != ipaddresses.end(); iterListIp++)
							{
								DebugPrintf(SV_LOG_INFO, "\tIP Address is: %s\n", iterListIp->c_str());
							}
						}
						mapPassiveInstance[networkname] = bPassiveInstance;
						dependedNameList.clear();
					}
					else if( dependedNameList.empty() )
					{
						DebugPrintf(SV_LOG_INFO, "File Server: %s does not have any net work name dependency \n", fileServerResourceNamesIter->c_str() );
						networkname = clusterName;
						ipaddresses.push_back(clusterIP.c_str());
					}
					else
					{
						DebugPrintf(SV_LOG_ERROR, "Failed to get The depended resource names %s \n", fileServerResourceNamesIter->c_str() );
					}
					dependedPhyDskList.clear();
					switch(osvi.dwMinorVersion)
					{
					case 0: // Windows Server 2008
					case 1: // Windows Server 2008 R2
						retErrorValue = getDependedResorceNameListOfType( *fileServerResourceNamesIter, "Physical Disk", dependedPhyDskList );
						break;
					case 2: // Windows Server 2012
						retErrorValue = getResourcesByTypeFromGroup( *fileServerResourceNamesIter, "Physical Disk", dependedPhyDskList );
						break;
					default:
						DebugPrintf(SV_LOG_DEBUG,"Unknown minor version found: %d\n",osvi.dwMinorVersion);
						break;
					}
					if(retErrorValue == SVS_OK && dependedPhyDskList.empty() == false )
					{
						std::list<std::string>::iterator listIter = dependedPhyDskList.begin();
						for(;listIter != dependedPhyDskList.end(); listIter++)
						{
							physicalDisk = *(listIter);
							DebugPrintf(SV_LOG_INFO, "Clsuter Disk : %s \n", physicalDisk.c_str());
							w2k8NetIp.networkName = networkname;
							w2k8NetIp.ipAddress = ipaddresses;
							mapFsDskWithNetWork.insert(std::make_pair(physicalDisk, w2k8NetIp));
						}
					}
					fileServerResourceNamesIter++;
				}
			}

			std::list<std::string> strClusVolList;
			std::map<std::string, w2k8networkDetails> mapFsVolWithNetWork;
			retErrorValue = SVS_OK ;

			if(discoverFileServerVolumes(strClusVolList, mapFsDskWithNetWork, mapFsVolWithNetWork) != SVS_OK)
			{
				DebugPrintf(SV_LOG_DEBUG, "Failed to retrive the FileServer Volume information %s\n", FUNCTION_NAME) ;
				retErrorValue = SVS_FALSE;
			}

			if(DiscoverAllW2k8FileShares(strClusVolList, mapFsVolWithNetWork, discoveryMap, instanceMap, mapPassiveInstance) != SVS_OK)
			{
				DebugPrintf(SV_LOG_DEBUG, "Failed to retrive the FileShare information from the FileServer Volumes %s\n", FUNCTION_NAME) ;
				retErrorValue = SVS_FALSE;
			}
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG, "Failed to get the FileServer resources from cluster, No FileServer Resources are available in the Cluster\n") ;
			FileServerInstanceDiscoveryData discoveryData;
			FileServerInstanceMetaData instanceMetaData ;
			discoveryData.m_ErrorCode = DISCOVERY_SUCCESS;
			instanceMetaData.m_ErrorCode = DISCOVERY_SUCCESS;
			discoveryData.m_ErrorString = "No FileServer instances are available in W2K8 cluster";
			discoveryMap.insert(std::make_pair(hostName, discoveryData)) ;
			instanceMetaData.m_ErrorString = "No FileServer instances are available in W2K8 cluster";
			instanceMap.insert(std::make_pair(hostName, instanceMetaData));
			retErrorValue = SVS_OK;
		}
	}
	catch(...)
	{
		DebugPrintf(SV_LOG_ERROR,"Got Exception while discovering W2K8 cluster Shares\n");
		FileServerInstanceDiscoveryData discoveryData;
		FileServerInstanceMetaData instanceMetaData ;
		discoveryData.m_ErrorCode = DISCOVERY_FAIL;
		instanceMetaData.m_ErrorCode = DISCOVERY_FAIL;
		discoveryData.m_ErrorString = "Got Exception while discovering W2K8 cluster Shares";
		discoveryMap.insert(std::make_pair(hostName, discoveryData)) ;
		instanceMetaData.m_ErrorString = "Got Exception while discovering W2K8 cluster Shares";
		instanceMap.insert(std::make_pair(hostName, instanceMetaData));
		retErrorValue = SVS_FALSE;
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retErrorValue ;
}

SVERROR FileServerDiscoveryImpl::fillSharesAndSecurityDataFromReg( FileShareInfo& fileShareInfoObj )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retErrorValue = SVS_OK ;
	AppLocalConfigurator configurator ;
	std::string regExportPath = configurator.getApplicationPolicyLogPath();
	regExportPath = regExportPath + "FSExport.reg";
	std::string lockPath = regExportPath + ".lck";
    ACE_File_Lock lock(getLongPathName(lockPath.c_str()).c_str(),O_CREAT | O_RDWR, SV_LOCK_PERMISSIONS,0);
    lock.acquire_read();
	std::ifstream is;
	is.open( regExportPath.c_str(), std::ios::in | std::ios::binary );
	if( is.is_open() && is.good() )
	{
		is.seekg (0, std::ios::end);
		int length = is.tellg();
		is.seekg (0, std::ios::beg);
        size_t buflen;
        INM_SAFE_ARITHMETIC(buflen = InmSafeInt<int>::Type(length)+1, INMAGE_EX(length))
		boost::shared_array<char> buffer(new char[buflen]);
		is.read (buffer.get(), length);
        buffer[length] = '\0';
		lock.release();
		is.close();
		std::string totalData;
		totalData = buffer.get();
		std::string stuff_to_remove = "\n\r\\" ;
		std::string regVersion;
		if( getRegistryVersionData( totalData, regVersion ) == SVS_OK )
		{
			removeChars( stuff_to_remove, regVersion );
		}
		if( getShareData( fileShareInfoObj.m_shareName, totalData, fileShareInfoObj.m_sharesAscii ) == SVS_OK )
		{
			removeChars( stuff_to_remove, fileShareInfoObj.m_sharesAscii );
		}
		if( getSecurityData( fileShareInfoObj.m_shareName, totalData, fileShareInfoObj.m_securityAscii ) == SVS_OK )
		{
			removeChars( stuff_to_remove, fileShareInfoObj.m_securityAscii );
		}
	}
	else
	{
		DebugPrintf( SV_LOG_ERROR, "Failed to open the file: %s\n", regExportPath.c_str() );
		retErrorValue = SVS_FALSE;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retErrorValue ;
}

SVERROR FileServerDiscoveryImpl::generateRegistryFile( const std::string& filePath, const std::string& registryVersion, const std::map<std::string, FileShareInfo>& regInfoMap )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retErrorValue = SVS_OK ;
	std::stringstream writeStream;
	std::string tempregistryVersion;
	tempregistryVersion = registryVersion;
	writeStream	<< tempregistryVersion << std::endl;
	std::map<std::string, FileShareInfo>::const_iterator regInfoMapBegIter = regInfoMap.begin();
	std::map<std::string, FileShareInfo>::const_iterator regInfoMapEndIter = regInfoMap.end();
	if( regInfoMapBegIter != regInfoMapEndIter )
	{
		writeStream << SHARES_KEY << std::endl;
	}
	while( regInfoMapBegIter != regInfoMapEndIter )
	{
		std::string tempShareData ;
		tempShareData = regInfoMapBegIter->second.m_sharesAscii;
		tempShareData = "\"=" + tempShareData;
		tempShareData =  regInfoMapBegIter->first + tempShareData;
		tempShareData = "\"" + tempShareData;
		writeStream << tempShareData << std::endl;
		regInfoMapBegIter++;	
	}
	regInfoMapBegIter = regInfoMap.begin();
	if( regInfoMapBegIter != regInfoMapEndIter )
	{
		writeStream << SECURITY_KEY << std::endl;
	}
	while( regInfoMapBegIter != regInfoMapEndIter )
	{
		std::string tempsecurityData ;
		tempsecurityData = regInfoMapBegIter->second.m_securityAscii;
		tempsecurityData = "\"=" + tempsecurityData;
		tempsecurityData =  regInfoMapBegIter->first + tempsecurityData;
		tempsecurityData = "\"" + tempsecurityData;
		writeStream << tempsecurityData << std::endl;
		regInfoMapBegIter++;	
	}
	if( !writeStream.str().empty() )
	{
		std::ofstream os;
		os.open( filePath.c_str(), ofstream::out | ofstream::binary );
		if( os.is_open() && os.good() )
		{
			os << writeStream.str();
			os.flush();
			os.close();
		}
		else
		{
			DebugPrintf( SV_LOG_ERROR, "Failed to open the file: %s\n", filePath.c_str() );
			retErrorValue = SVS_FALSE;
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retErrorValue ;
}

std::list<std::pair<std::string, std::string> > FileShareInfo::getProperties() 
{
    std::list<std::pair<std::string, std::string> > properties ;
    properties.push_back(std::make_pair("Absolute Path", m_absolutePath)) ;
    properties.push_back(std::make_pair("Mount Point", m_mountPoint)) ;
    properties.push_back(std::make_pair("UNC Path", m_UNCPath)) ;
    return properties; 
}

bool FileServerDiscoveryImpl::isAlreadyDiscovered(std::string shareName, std::map<std::string, FileServerInstanceMetaData>& instanceMap)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::map<std::string, FileServerInstanceMetaData>::iterator instanceMapIter = instanceMap.begin();
    while(instanceMapIter != instanceMap.end())
    {
        std::map<std::string, FileShareInfo>::iterator shareInfoMapIter = instanceMapIter->second.m_shareInfo.begin();
        while(shareInfoMapIter != instanceMapIter->second.m_shareInfo.end())
        {
            if( strcmpi( shareName.c_str(), shareInfoMapIter->first.c_str() ) == 0)
            {
                return true;
            }
            shareInfoMapIter++;
        }
        instanceMapIter++;
    }
    return false;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

SVERROR FileServerDiscoveryImpl::DiscoverAllW2k8FileShares(std::list<std::string>& strClusVolList, std::map<std::string, w2k8networkDetails>& mapFsVolWithNetWork, std::map<std::string, FileServerInstanceDiscoveryData>&discoveryMap, std::map<std::string, FileServerInstanceMetaData>&instanceMap, std::map<std::string,bool>& mapPassiveInstance)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retErrorValue = SVS_OK;
	NET_API_STATUS netAPIStatusValue;
	PSHARE_INFO_503 ptrSHARE_INFO_503, ptrSHARE_INFO_503_Buffer;
	DWORD dwEntriesRead, dwTotalEntries,dwresume = 0 ;

	USES_CONVERSION;

	std::string systemDrive ;
	getEnvironmentVariable("SystemDrive", systemDrive) ;
	if( systemDrive.size() == 2 )
	{
		systemDrive += "\\" ;
	}
	else if( systemDrive.size() == 1 )
	{
		systemDrive += ":\\" ;
	}

	try
	{
		do
		{
			setSecurityEnablePriv();
			netAPIStatusValue = NetShareEnum(NULL, 503, (LPBYTE *) &ptrSHARE_INFO_503_Buffer, -1, &dwEntriesRead, &dwTotalEntries, &dwresume);
			if( netAPIStatusValue == ERROR_SUCCESS || netAPIStatusValue == ERROR_MORE_DATA )
			{
				ptrSHARE_INFO_503 = ptrSHARE_INFO_503_Buffer;
				DWORD dwIndex;
				//bool bFoundW2k8ClusterShare = false;

				for( dwIndex = 1; dwIndex <= dwEntriesRead; dwIndex++ )
				{
					if(ptrSHARE_INFO_503->shi503_type == STYPE_DISKTREE)
					{
						FileShareInfo fsinfo;
						std::string strBaseVolumeName;

						try				 
						{
							getBaseVolumeName(W2A(ptrSHARE_INFO_503->shi503_path),strBaseVolumeName);
							boost::algorithm::to_upper(strBaseVolumeName);
							DebugPrintf(SV_LOG_INFO,"Base Volume :%s.\n",strBaseVolumeName.c_str());
						}
						catch(std::string strException)
						{
							DebugPrintf(SV_LOG_ERROR,"Failed to get the base volume .Error :%s.\n",strException.c_str());
							retErrorValue = SVS_FALSE;
							break;
						}
						if(strcmpi(systemDrive.c_str(), strBaseVolumeName.c_str()) != 0)
						{
							std::list<std::string>::iterator listVolIter;
							for(listVolIter = strClusVolList.begin(); listVolIter != strClusVolList.end(); listVolIter++)
							{
								if(strcmpi(listVolIter->c_str(), strBaseVolumeName.c_str()) == 0)
								{
									std::string networkName;
									FileServerInstanceDiscoveryData discoveryData;
									if(mapFsVolWithNetWork.find(strBaseVolumeName) != mapFsVolWithNetWork.end())
									{
										networkName = mapFsVolWithNetWork.find(strBaseVolumeName)->second.networkName;
										DebugPrintf(SV_LOG_INFO,"NetWrok Name : %s\n",networkName.c_str());
										if( discoveryMap.find(networkName) == discoveryMap.end() )
										{
											discoveryMap.insert(std::make_pair(networkName, discoveryData)) ;
											discoveryData.m_ips =  mapFsVolWithNetWork.find(strBaseVolumeName)->second.ipAddress;
										}
									}
									fsinfo.m_shi503_type = ptrSHARE_INFO_503->shi503_type;
									if(ptrSHARE_INFO_503->shi503_passwd != NULL)
									{
										fsinfo.m_shi503_passwd = W2A(ptrSHARE_INFO_503->shi503_passwd);
									}
									
									fsinfo.m_shi503_max_uses = ptrSHARE_INFO_503->shi503_max_uses;
									fsinfo.m_shareName = W2A(ptrSHARE_INFO_503->shi503_netname);
									fsinfo.m_absolutePath = W2A(ptrSHARE_INFO_503->shi503_path);
									fsinfo.m_shi503_remark = W2A(ptrSHARE_INFO_503->shi503_remark);
									fsinfo.m_UNCPath = "\\\\" + networkName + "\\" + fsinfo.m_shareName  ;
									trimCharacters(strBaseVolumeName);
									fsinfo.m_mountPoint = strBaseVolumeName;
									fsinfo.m_sharesAscii = W2A(ptrSHARE_INFO_503->shi503_netname);
									if( isAlreadyDiscovered(fsinfo.m_shareName, instanceMap) == false )
									{
										if (IsValidSecurityDescriptor(ptrSHARE_INFO_503->shi503_security_descriptor))
										{
											if(getSecurityDescriptorString(fsinfo.m_absolutePath, fsinfo.m_securityAscii))
											{
												bool bPassiveInstance = false;
												if(mapPassiveInstance.find(networkName) != mapPassiveInstance.end())
													bPassiveInstance = mapPassiveInstance[networkName];
												if( instanceMap.find(networkName) == instanceMap.end() )
												{
													FileServerInstanceMetaData instanceMetaData ;
													if( bPassiveInstance )
													{
														instanceMetaData.m_ErrorCode = PASSIVE_INSTANCE;
														instanceMetaData.m_ErrorString = "PASSIVE_INSTANCE";
													}
													instanceMetaData.m_shareInfo.insert(make_pair(fsinfo.m_shareName, fsinfo)) ;
													if( find(instanceMetaData.m_volumes.begin(), instanceMetaData.m_volumes.end(), 
														fsinfo.m_mountPoint) == instanceMetaData.m_volumes.end() )
													{
														instanceMetaData.m_volumes.push_back(fsinfo.m_mountPoint) ;
													}
													instanceMap.insert(std::make_pair(networkName, instanceMetaData)) ;
													//bFoundW2k8ClusterShare = true;
													retErrorValue = SVS_OK;
												}
												else
												{
													FileServerInstanceMetaData& instanceMetaData = instanceMap.find(networkName)->second ;
													if( bPassiveInstance )
													{
														instanceMetaData.m_ErrorCode = PASSIVE_INSTANCE;
														instanceMetaData.m_ErrorString = "PASSIVE_INSTANCE";
													}
													instanceMetaData.m_shareInfo.insert(make_pair(fsinfo.m_shareName, fsinfo)) ;
													if( find(instanceMetaData.m_volumes.begin(), instanceMetaData.m_volumes.end(), 
														fsinfo.m_mountPoint) == instanceMetaData.m_volumes.end() )
													{
														instanceMetaData.m_volumes.push_back(fsinfo.m_mountPoint) ;
													} 
													//bFoundW2k8ClusterShare = true;
													retErrorValue = SVS_OK;
												}
											}
											else
											{
												DebugPrintf(SV_LOG_ERROR,"Failed to fetch the security Descriptor string.Exiting.\n");
												retErrorValue = SVS_FALSE;
											}
										}
										else
										{
											fsinfo.m_securityAscii = "";
										}									
									}
								}
							}
						}
					}
					ptrSHARE_INFO_503++;
				}
				/*if(!bFoundW2k8ClusterShare)
				{*/
					std::string groupName, ownerNodeName;
					std::list<std::string>::iterator listVolIter;
					for(listVolIter = strClusVolList.begin(); listVolIter != strClusVolList.end(); listVolIter++)
					{
						if(mapFsVolWithNetWork.find(*listVolIter) != mapFsVolWithNetWork.end())
						{
							std::string networkName;
							CLUSTER_RESOURCE_STATE state;
							networkName = mapFsVolWithNetWork.find(*listVolIter)->second.networkName;
							if( getResourceState("", networkName, state, groupName, ownerNodeName ) == SVS_OK )
							{						
								if( discoveryMap.find(networkName) == discoveryMap.end() )
								{
									FileServerInstanceDiscoveryData discoveryData;
									FileServerInstanceMetaData instanceMetaData ;
									bool bPassiveInstance = false;
									if(mapPassiveInstance.find(networkName) != mapPassiveInstance.end())
										bPassiveInstance = mapPassiveInstance[networkName];
									if(bPassiveInstance)
									{
										discoveryData.m_ips =  mapFsVolWithNetWork.find(*listVolIter)->second.ipAddress;
										discoveryData.m_ErrorCode = PASSIVE_INSTANCE;
										discoveryData.m_ErrorString = "PASSIVE_INSTANCE";
										discoveryMap.insert(std::make_pair(networkName, discoveryData)) ;
										instanceMetaData.m_ErrorCode = PASSIVE_INSTANCE;
										instanceMetaData.m_ErrorString = "PASSIVE_INSTANCE";
										instanceMap.insert(std::make_pair(networkName, instanceMetaData));
										retErrorValue = SVS_OK;
									}
									else
									{									
										if(state == 3 || state == 4 || state == -1 || state == 130)
										{
											discoveryData.m_ErrorCode = DISCOVERY_FAIL;
											instanceMetaData.m_ErrorCode = DISCOVERY_FAIL;
											discoveryData.m_ErrorString = "W2K8 FileServer Cluster is offline/unavailable/unknown";
											discoveryMap.insert(std::make_pair(networkName, discoveryData)) ;
											instanceMetaData.m_ErrorString = "W2K8 FileServer Cluster is offline/unavailable/unknown";
											instanceMap.insert(std::make_pair(networkName, instanceMetaData));
											DebugPrintf(SV_LOG_INFO, "Cluster FileServer Service is not ONLINE\n");
											retErrorValue = SVS_FALSE;
										}
										else
										{
											discoveryData.m_ips =  mapFsVolWithNetWork.find(*listVolIter)->second.ipAddress;
											discoveryData.m_ErrorCode = DISCOVERY_SUCCESS;
											discoveryData.m_ErrorString = "No shares are available on the W2K8 Cluster.";
											discoveryMap.insert(std::make_pair(networkName, discoveryData)) ;
											instanceMetaData.m_ErrorCode = DISCOVERY_SUCCESS;
											instanceMetaData.m_ErrorString = "No shares are available on the W2K8 Cluster.";
											instanceMap.insert(std::make_pair(networkName, instanceMetaData));
											retErrorValue = SVS_OK;
										}									
									}
								}
							}
							else
							{
								DebugPrintf(SV_LOG_ERROR, "getResourceState failed\n") ;
								retErrorValue = SVS_FALSE;
							}
						}
					}
				/*}*/
				// Free the allocated buffer.
				NetApiBufferFree(ptrSHARE_INFO_503_Buffer);
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"NetEnumShare api failed with Error Code :%d.\n",netAPIStatusValue);
				retErrorValue = SVS_FALSE;
			}
		}while( netAPIStatusValue == ERROR_MORE_DATA );
	}
	catch(...)
	{
		DebugPrintf(SV_LOG_ERROR,"Got Exception while discovering W2K8 cluster Shares\n");
		std::list<std::string>::iterator listVolIter;
		for(listVolIter = strClusVolList.begin(); listVolIter != strClusVolList.end(); listVolIter++)
		{
			if(mapFsVolWithNetWork.find(*listVolIter) != mapFsVolWithNetWork.end())
			{
				FileServerInstanceDiscoveryData discoveryData;
				FileServerInstanceMetaData instanceMetaData ;
				std::string networkName;
				networkName = mapFsVolWithNetWork.find(*listVolIter)->second.networkName;
				discoveryData.m_ErrorCode = DISCOVERY_FAIL;
				instanceMetaData.m_ErrorCode = DISCOVERY_FAIL;
				discoveryData.m_ErrorString = "Got Exception while discovering W2K8 cluster Shares";
				discoveryMap.insert(std::make_pair(networkName, discoveryData)) ;
				instanceMetaData.m_ErrorString = "Got Exception while discovering W2K8 cluster Shares";
				instanceMap.insert(std::make_pair(networkName, instanceMetaData));
			}
		}
		retErrorValue = SVS_FALSE;
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retErrorValue ;
}

void FileServerDiscoveryImpl::trimCharacters(std::string &trimString)
{
	using namespace boost;
	trim(trimString);
	trim_left_if(trimString,is_any_of("\""));
	trim_right_if(trimString,is_any_of("\""));
	trim_right_if(trimString,is_any_of("\\"));
}

void FileServerDiscoveryImpl::getBaseVolumeName(const std::string &strPath,std::string &BaseVolume)
{
	std::stringstream error_str;
	char str[512];
	if(!GetVolumePathName(strPath.c_str(),str,512))
	{
		error_str<<"GetVolumePathName failed with error code :%d";
		error_str<<GetLastError()<<std::endl;
		throw error_str.str();
	}
	else
	{
		BaseVolume = std::string(str);
	}
}

BOOL FileServerDiscoveryImpl::getSecurityDescriptorString(const std::string& dirName,std::string& sddlString)
{
		DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
		BOOL bResult = FALSE;
		LPTSTR lpszSDBuffer;
		PSECURITY_DESCRIPTOR psd         = NULL;
		PSID                  ppsidOwner = NULL;
		PSID                  ppsidGroup = NULL;
		PACL                  ppDacl = NULL;
		PACL				  ppACL = NULL;
		ULONG cbszSDBuffer			= 0;
		DWORD dwSidSize				= 0;
		DWORD dwErrorCode			= 0;
		DWORD cAce					= 0; 
		SECURITY_INFORMATION dwSecInfo	=0;
		ACCESS_ALLOWED_ACE * pOldAce = NULL;

		dwSecInfo					 =  DACL_SECURITY_INFORMATION |
										LABEL_SECURITY_INFORMATION |
										GROUP_SECURITY_INFORMATION |
										OWNER_SECURITY_INFORMATION  |
								        SACL_SECURITY_INFORMATION;

		//We are creating a specified Descriptor.The File share Descriptor we can use,but its lacking some values(SACL);
		DWORD dwReturn = GetNamedSecurityInfo(const_cast<char*>(dirName.c_str()),SE_FILE_OBJECT,dwSecInfo,&ppsidOwner,&ppsidGroup,&ppDacl,&ppACL,&psd);
		if(dwReturn == ERROR_SUCCESS)
		{
			if(! ConvertSecurityDescriptorToStringSecurityDescriptor (psd,SDDL_REVISION,dwSecInfo,&lpszSDBuffer,&cbszSDBuffer ))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to get the SDDL string.Error Code :[%d].\n",GetLastError());
			}
			else
			{
				sddlString = std::string(lpszSDBuffer);
				if(lpszSDBuffer != NULL)
						LocalFree((HLOCAL)lpszSDBuffer);

				bResult = TRUE;
			}
			if(psd)
				LocalFree(psd);
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to get the GetNamedSecurityInfo.Error Code :[%d].\n",dwReturn /*GetLastError()*/);
		}
		DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
		return bResult;
}

BOOL FileServerDiscoveryImpl::setSecurityEnablePriv()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	HANDLE hToken = NULL;
	SVERROR logonStatus = SVS_OK;
	/*readAndChangeServiceLogOn(hToken);*/
	logonStatus = revertToOldLogon();
	if(logonStatus == SVS_FALSE)
	{
		DebugPrintf(SV_LOG_DEBUG, "Failed to revert the old credentials\n") ;
	}
	BOOL result = FALSE;
	LUID luid;
	TOKEN_PRIVILEGES tkprivs;
	if(OpenProcessToken(GetCurrentProcess(), (TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY), &hToken))
	{
		if(LookupPrivilegeValue(NULL, "SeSecurityPrivilege", &luid))
		{
			tkprivs.PrivilegeCount = 1;
			tkprivs.Privileges[0].Luid = luid;
			tkprivs.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
			if(FALSE == AdjustTokenPrivileges(hToken, FALSE, &tkprivs, sizeof(tkprivs), NULL, NULL))
			{
				DebugPrintf(SV_LOG_ERROR,"AdjustTokenPrivileges failed.Error Code :[%d]\n",GetLastError());
			}
			else
			{
				result = TRUE;
			}
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"LookupPrivilegeValue failed.Error Code :[%d]\n",GetLastError());
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"OpenProcessToken failed.Error Code :[%d]",GetLastError());
	}
	
	CloseHandle(hToken);
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return result;
}

SVERROR FileServerDiscoveryImpl::discoverFileServerVolumes(std::list<std::string>& strClusVolList, std::map<std::string, w2k8networkDetails>& mapFsDiskWithNetWork, std::map<std::string, w2k8networkDetails>& mapFsVolWithNetWork)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retValue = SVS_FALSE;
	std::map<std::string,std::string> clusterDiskIdMap;
	std::string strResourceDiskFileName = "res.vbs";
	std::string strWmiClassName = "MSCluster_ResourceToDisk";
	do
	{
		if(createResourceDiskFile(strResourceDiskFileName,strWmiClassName) != SVS_OK)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to create Resource Disk File.\n");
			break;
		}

		if(executeVbsScriptForClusterDisk(strResourceDiskFileName) != SVS_OK)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to execute Resource script.\n");
			break;
		}
		if(prepareClusterDiskNameIdMap(clusterDiskIdMap) == SVS_FALSE)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to get the Cluster Disk Name Id map.\n");
			break;
		}
		std::map<std::string, std::string>::const_iterator iterClusDiskId = clusterDiskIdMap.begin();
		for(;iterClusDiskId != clusterDiskIdMap.end(); iterClusDiskId++)
		{
			DebugPrintf(SV_LOG_DEBUG,"DiskID : %s\t DiskName: %s\n", iterClusDiskId->first.c_str(), iterClusDiskId->second.c_str());
		}
		iterClusDiskId = clusterDiskIdMap.begin();
		while(iterClusDiskId != clusterDiskIdMap.end())
		{
			std::list<std::string> strVolNameList;
			if(getListOfvolumesOnClusterDisk(std::string(""),iterClusDiskId->first,strVolNameList) == SVS_OK)
			{
				std::list<std::string>::iterator iterVolList = strVolNameList.begin();
				while(iterVolList != strVolNameList.end())
				{
					strClusVolList.push_back(*iterVolList);
					if(mapFsDiskWithNetWork.find(iterClusDiskId->first) != mapFsDiskWithNetWork.end())
					{
						std::string strVolName = *iterVolList;
						FormatFsVol(strVolName);
						DebugPrintf(SV_LOG_INFO,"Cluster Volume: %s\n", strVolName.c_str());
						boost::algorithm::to_upper(strVolName);
						mapFsVolWithNetWork.insert(std::make_pair(strVolName, mapFsDiskWithNetWork.find(iterClusDiskId->first)->second));
					}
					iterVolList++;
				}
				retValue = SVS_OK;
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to find the Cluster Physical disk information.\n");
				retValue = SVS_FALSE;
				break;
			}
			iterClusDiskId++;
		}
#ifndef SCOUT_WIN2K_SUPPORT
		BOOST_FOREACH(std::string &strVolName,strClusVolList)
		{
			size_t index = strVolName.find("\\\\?\\Volume{");
			if(index != std::string::npos)
			{
				if(strVolName[strVolName.length()-1] != '\\')
					strVolName += "\\";
				FormatVolumeName(strVolName,false);
				char szVolumePath[MAX_PATH+1] = {0};
				DWORD dwVolPathSize = 0;
				if(!GetVolumePathNamesForVolumeName (strVolName.c_str(),szVolumePath,MAX_PATH, &dwVolPathSize))
				{
					DebugPrintf(SV_LOG_ERROR,"\n GetVolumePathNamesForVolumeName Failed with ErrorCode : [%d]",GetLastError());
					retValue = SVS_FALSE;
				} 
				else
				{
					strVolName = szVolumePath;
				}
			}
			FormatVolumeName(strVolName,false);
			boost::algorithm::to_upper(strVolName);
		}
#endif
	}while(0);
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retValue;
}

void FileServerDiscoveryImpl::FormatFsVol(std::string& strVolName)
{
#ifndef SCOUT_WIN2K_SUPPORT
	size_t index = strVolName.find("\\\\?\\Volume{");
	if(index != std::string::npos)
	{
		if(strVolName[strVolName.length()-1] != '\\')
			strVolName += "\\";
		FormatVolumeName(strVolName,false);
		char szVolumePath[MAX_PATH+1] = {0};
		DWORD dwVolPathSize = 0;
		if(!GetVolumePathNamesForVolumeName (strVolName.c_str(),szVolumePath,MAX_PATH, &dwVolPathSize))
		{
			DebugPrintf(SV_LOG_ERROR,"\n GetVolumePathNamesForVolumeName Failed with ErrorCode : [%d]",GetLastError());
			return;
		} 
		else
		{
			strVolName = szVolumePath;
		}
	}
	FormatVolumeName(strVolName, false);
	boost::algorithm::to_upper(strVolName);
#endif
}

SVERROR FileServerDiscoveryImpl::executeVbsScriptForClusterDisk(const std::string& strResrcDiskFileName)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retValue = SVS_FALSE;
	std::string strCmdToExecute = "cscript //nologo ";
	strCmdToExecute += strResrcDiskFileName;
	SV_UINT m_uTimeOut = 1000;
	SV_ULONG exitCode = 1;
	std::string strOuputFileName = generateLogFilePath(strCmdToExecute);
	AppCommand objAppCommand(strCmdToExecute,m_uTimeOut,strOuputFileName);
	HANDLE h = NULL;
	h = Controller::getInstance()->getDuplicateUserHandle();
	if(objAppCommand.Run(exitCode,m_stdOut,Controller::getInstance()->m_bActive, "", h) != SVS_OK)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to spawn script.\n");
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG,"Successfully spawned the script. \n");
		DebugPrintf(SV_LOG_DEBUG,"Script Executed = %s",m_stdOut.c_str());
		retValue = SVS_OK;
	}
	if(h)
	{
		CloseHandle(h);
	}
	ACE_OS::unlink(strResrcDiskFileName.c_str());
	ACE_OS::unlink(strOuputFileName.c_str());
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retValue;
}
SVERROR FileServerDiscoveryImpl::createResourceDiskFile(const std::string& strResourceDiskFileName,const std::string& strWMIClassName)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retValue = SVS_FALSE;
	ACE_HANDLE hHandle = ACE_INVALID_HANDLE;
	hHandle = ACE_OS::open(getLongPathName(strResourceDiskFileName.c_str()).c_str(), O_CREAT | O_RDWR );
	if(hHandle == ACE_INVALID_HANDLE)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to open %s for writing ",strResourceDiskFileName.c_str());
		return retValue;
	}
	boost::shared_ptr<void> hKeyGguard(static_cast<void*>(0), boost::bind(boost::type<int>(), &ACE_OS::close, hHandle));
	std::stringstream ostr;
	std::string strLocalHost = Host::GetInstance().GetHostName();

	ostr<<"On Error Resume Next\n";
	ostr<<"Const wbemFlagReturnImmediately = &h10\n";
	ostr<<"Const wbemFlagForwardOnly = &h20\n";
	ostr<<"arrComputers = Array(\""<<strLocalHost<<"\")\n";
	ostr<<"For Each strComputer In arrComputers\n";
	ostr<<"Set objWMIService = GetObject(\"winmgmts:\\\\\" & strComputer & \"\\root\\MSCluster\")\n";
	ostr<<"Set colItems = objWMIService.ExecQuery(\"SELECT * FROM "<<strWMIClassName<<"\")\n";
	ostr<<"For Each objItem In colItems\n";
	ostr<<" WScript.Echo \"GroupComponent: \" & objItem.GroupComponent\n";
	ostr<<" WScript.Echo \"PartComponent: \" & objItem.PartComponent\n";
	ostr<<" WScript.Echo \"INMAGE_TOKEN\"\n";
	ostr<<"		Next\n";
	ostr<<"Next\n";
	size_t bytesWrote = 0;
	size_t bytesRead = ostr.str().length();
	if(( bytesWrote = ACE_OS::write(hHandle, ostr.str().c_str(), bytesRead)) != bytesRead)
	{
		DebugPrintf(SV_LOG_ERROR, "Among %d bytes wrote only %d bytes\n", bytesRead, bytesWrote) ;
	}
	else
	{
		retValue = SVS_OK;
	}	
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retValue;
}


SVERROR FileServerDiscoveryImpl::prepareClusterDiskNameIdMap(std::map<std::string,std::string>& clusterDiskIdMap)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retValue = SVS_FALSE;
	std::list<std::string> tokenList;
	stringParser(tokenList);
	if(!tokenList.empty())
	{
		stringParser(tokenList,clusterDiskIdMap);
		if(clusterDiskIdMap.empty())
		{
			DebugPrintf(SV_LOG_ERROR,"Found empty map for Cluster Disk volumes.\n");
		}
		else
		{
			retValue = SVS_OK;
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retValue;
}

void FileServerDiscoveryImpl::stringParser(std::list<std::string>& outList)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::size_t index;
	std::string strToken = "INMAGE_TOKEN";
	index = m_stdOut.find(strToken);
	while(index != std::string::npos)
	{
		std::string extractedString = m_stdOut.substr(0,index);
		m_stdOut.erase(0,index+strToken.length()+1);
		outList.push_back(extractedString);
		index = m_stdOut.find(strToken);
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void FileServerDiscoveryImpl::stringParser(std::list<std::string> stringList,std::map<std::string,std::string>& tokenMap)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::size_t index;
	std::string token = "PartComponent";
	std::list<std::string>::iterator listIter = stringList.begin();
	while(listIter != stringList.end())
	{
		std::string strIterValue = *listIter;
		index = strIterValue.find(token);
		if(index != std::string::npos)
		{
			std::string stringFirst = strIterValue.substr(0,index-1);
			std::string stringSecond = strIterValue.substr(index,strIterValue.length());
			index = stringFirst.find("=");
			if(index != std::string::npos)
				stringFirst = stringFirst.substr(index+1,stringFirst.length());
			
			index = stringSecond.find("=");
			if(index != std::string::npos)
				stringSecond = stringSecond.substr(index+1,stringSecond.length());

			trimCharacters(stringFirst);
			trimCharacters(stringSecond);

			tokenMap.insert(std::make_pair(stringFirst,stringSecond));
		}
		listIter++;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void FileServerDiscoveryImpl::stringParser (std::list<std::string> stringList,std::map<std::string,std::list<std::string> >& partitionDiskIdMap)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;

	std::size_t index;
	std::string token = "PartComponent";
	std::list<std::string>::iterator listIter = stringList.begin();
	while(listIter != stringList.end())
	{
		std::string strIterValue = *listIter;
		index = strIterValue.find(token);
		if(index != std::string::npos)
		{
			std::string stringFirst = strIterValue.substr(0,index-1);
			std::string stringSecond = strIterValue.substr(index,strIterValue.length());
			index = stringFirst.find("=");
			if(index != std::string::npos)
				stringFirst = stringFirst.substr(index+1,stringFirst.length());
			
			index = stringSecond.find("=");
			if(index != std::string::npos)
				stringSecond = stringSecond.substr(index+1,stringSecond.length());
			
			trimCharacters(stringFirst);
			std::map<std::string,std::list<std::string> >::iterator iter = partitionDiskIdMap.find(stringFirst);
			if(iter != partitionDiskIdMap.end())
			{
				trimCharacters(stringSecond);
				iter->second.push_back(stringSecond);
				partitionDiskIdMap.insert(std::make_pair(stringFirst,iter->second));
			}
			else
			{
				std::list<std::string> strList;
				trimCharacters(stringSecond);
				strList.push_back(stringSecond);
				partitionDiskIdMap.insert(std::make_pair(stringFirst,strList));
			}
		}
		listIter++;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

SVERROR FileServerDiscoveryImpl::getListOfvolumesOnClusterDisk(const std::string &clusterName, const std::string &resName,std::list<std::string>& volList)
{	
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	USES_CONVERSION;
	HCLUSTER hInmCluster = NULL;
	HRESOURCE hInmResource = NULL;
	WCHAR* lpwClusterName = NULL;

	DWORD cbBuffSize1 = INM_TYPE_BUFFER_LEN;
	
	SVERROR retError = SVS_FALSE;
	
	DWORD cbLength,cbRequired,dwSignature,dwSyntax,cbPosition;
	cbLength = cbRequired = dwSignature = dwSyntax = cbPosition = 0;

	CLUSPROP_BUFFER_HELPER cbHelper;
	LPVOID lpszType = LocalAlloc( LPTR, 1024 );
	if( lpszType == NULL )
	{
		DebugPrintf( SV_LOG_ERROR, " Failed to allocate local memory to Buffer. Error Code: %d \n", GetLastError() );
		DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
		return 	retError;
	}
	if( clusterName.empty() == false )
	{
		lpwClusterName = A2W(clusterName.c_str()) ;
	}
	
	hInmCluster = OpenCluster( lpwClusterName );
	if( NULL != hInmCluster )
	{
		hInmResource = OpenClusterResource (hInmCluster,A2W(resName.c_str()));
		if(hInmResource != NULL)
		{
			DWORD bytesReturned;
			DWORD dwRet = ClusterResourceControl( hInmResource,
				NULL, CLUSCTL_RESOURCE_STORAGE_GET_DISK_INFO_EX , 
				NULL, NULL, lpszType, cbBuffSize1, &bytesReturned );
			if( dwRet == ERROR_MORE_DATA )
			{
				LocalFree( lpszType );
				cbBuffSize1 = bytesReturned;
				lpszType = LocalAlloc( LPTR, cbBuffSize1 );
				if( lpszType == NULL )
				{
					DebugPrintf( SV_LOG_ERROR, "Failed to allocate local memory to Buffer. Error Code: %d \n", GetLastError() );
				}
				dwRet = ClusterResourceControl( hInmResource,
					NULL, CLUSCTL_RESOURCE_STORAGE_GET_DISK_INFO_EX , 
					NULL, NULL, lpszType, cbBuffSize1, &bytesReturned );		
			}
			if(dwRet == ERROR_SUCCESS)
			{
				cbHelper.pb = (PBYTE)lpszType;
				while(1)
				{
					retError = SVS_OK; // make sure break
					if(cbHelper.pSyntax->dw == CLUSPROP_SYNTAX_PARTITION_INFO_EX)
					{
						volList.push_back(W2A(cbHelper.pPartitionInfoValueEx->szDeviceName));
					}
					else if(cbHelper.pSyntax->dw == CLUSPROP_SYNTAX_SCSI_ADDRESS)
					{
						//Required to support Cluster - cluster FileServer Failover
					}
					else if(cbHelper.pSyntax->dw == CLUSPROP_SYNTAX_DISK_SIGNATURE)
					{
						//Required to support Cluster - cluster FileServer Failover
					}
					else if(cbHelper.pSyntax->dw == CLUSPROP_SYNTAX_ENDMARK)
						break;
				
					cbLength = cbHelper.pValue->cbLength;				
					cbPosition +=  sizeof( CLUSPROP_VALUE ) + ALIGN_CLUSPROP( cbLength ); // Data and padding.
					if( ( cbPosition + sizeof( DWORD ) ) >= bytesReturned )
						break;
					cbHelper.pb += sizeof( CLUSPROP_VALUE ) + ALIGN_CLUSPROP( cbLength );
				}
			}
			else
			{
				DebugPrintf( SV_LOG_ERROR, "CLUSCTL_RESOURCE_STORAGE_GET_DISK_INFO_EX Failed with error code : %d\n",GetLastError());
			}
			CloseClusterResource(hInmResource);
		}
		else
		{
			DebugPrintf( SV_LOG_ERROR, "Failed to open cluster Resource Error Code : %d\n",GetLastError());
		}
		CloseCluster( hInmCluster );
	}
	else
	{
		DebugPrintf( SV_LOG_ERROR, "Failed to open cluster  Error Code : %d\n",GetLastError());
	}
	if(lpszType != NULL)
	{
		LocalFree((HLOCAL)lpszType);
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retError;
}
