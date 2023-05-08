#include "fileserverdiscoveryandvalidator.h"
#include "config/appconfigurator.h"
#include "discovery/discoverycontroller.h"
#include "util/common/util.h"
#include "controller.h"
#include "ruleengine.h"
#include "system.h"
#include <boost/lexical_cast.hpp>
#include <stdlib.h>

FSDiscoveryAndValidator::FSDiscoveryAndValidator(const ApplicationPolicy& policy)
: DiscoveryAndValidator(policy)
{
	m_FSApp.reset(new FileServerAplication());	
}

SVERROR FSDiscoveryAndValidator::fillFileServerDiscoveryData( std::string& networkName, FileServerInfo& fsInfo)
{
	SVERROR bRet = SVS_FALSE ;
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
	AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
	bool bProtected = false;
	if(m_policyId != 0)
	{
		bProtected = configuratorPtr->isThisProtected(m_scenarioId);
	}
	std::map<std::string, FileServerInstanceDiscoveryData>::iterator FileServerInstanceDiscoveryDataIter;
	FileServerInstanceDiscoveryDataIter = m_FSApp->m_fileShareInstanceDiscoveryMap.find(networkName);
	if( FileServerInstanceDiscoveryDataIter != m_FSApp->m_fileShareInstanceDiscoveryMap.end() )	
	{
		fsInfo.m_ipAddressList = FileServerInstanceDiscoveryDataIter->second.m_ips;
	}
	std::map<std::string, FileServerInstanceMetaData>::iterator FileServerInstanceMetaDataIter;
	FileServerInstanceMetaDataIter = m_FSApp->m_fileShareInstanceMetaDataMap.find(networkName);
	if( FileServerInstanceMetaDataIter != m_FSApp->m_fileShareInstanceMetaDataMap.end() )
	{
		fsInfo.m_InstanceInformation.insert( std::make_pair("ApplicationInstanceName", networkName) );
		if( FileServerInstanceMetaDataIter->second.m_isClustered )
		{
			fsInfo.m_InstanceInformation.insert(std::make_pair("IsClustered", "1"));
		}
		else
		{
			fsInfo.m_InstanceInformation.insert(std::make_pair("IsClustered", "0"));
		}
		fsInfo.m_InstanceInformation.insert(std::make_pair("Version", m_FSApp->m_registryVersion));
		if(!bProtected)
		{
			fsInfo.m_InstanceInformation.insert(std::make_pair("ErrorCode", boost::lexical_cast<std::string>(FileServerInstanceMetaDataIter->second.m_ErrorCode))); 
			fsInfo.m_InstanceInformation.insert(std::make_pair("ErrorString", FileServerInstanceMetaDataIter->second.m_ErrorString));  
		}
		else
		{
			fsInfo.m_InstanceInformation.insert(std::make_pair("ErrorCode", boost::lexical_cast<std::string>(IN_PROTECTION))); 
			fsInfo.m_InstanceInformation.insert(std::make_pair("ErrorString", "This is protected"));  
		}
		fsInfo.m_volumes.insert(fsInfo.m_volumes.end(), FileServerInstanceMetaDataIter->second.m_volumes.begin(), FileServerInstanceMetaDataIter->second.m_volumes.end() );

		std::map<std::string, FileShareInfo>::iterator fileShareInfoIter;
		fileShareInfoIter = FileServerInstanceMetaDataIter->second.m_shareInfo.begin();
		while( fileShareInfoIter != FileServerInstanceMetaDataIter->second.m_shareInfo.end() )
		{
			ShareProperties shareProps;
			shareProps.m_properties.insert(std::make_pair( "ShareName", fileShareInfoIter->second.m_shareName ));
			std::string tempVolume = fileShareInfoIter->second.m_mountPoint;
			sanitizeVolumePathName(tempVolume);			
			shareProps.m_properties.insert(std::make_pair( "MountPoint", tempVolume ));

			shareProps.m_properties.insert(std::make_pair( "AbsolutePath", fileShareInfoIter->second.m_absolutePath ));
			
			shareProps.m_properties.insert(std::make_pair( "Shares", fileShareInfoIter->second.m_sharesAscii )); 
			
			shareProps.m_properties.insert(std::make_pair( "Security", fileShareInfoIter->second.m_securityAscii ));

			if( isClusterNode() )
			{
				OSVERSIONINFOEX osVersionInfo ;
				getOSVersionInfo(osVersionInfo) ;
				if(osVersionInfo.dwMajorVersion == 6) //making this condition true for windows 2008 sp1,sp2 and R2
				{
					char shareuserbuf[sizeof(unsigned long)*8+1];
					char sharetypebuf[sizeof(unsigned long)*8+1];
					shareProps.m_properties.insert(std::make_pair( "ShareUsers", ultoa(fileShareInfoIter->second.m_shi503_max_uses,shareuserbuf, 16)));

					shareProps.m_properties.insert(std::make_pair( "SecurityPwd", fileShareInfoIter->second.m_shi503_passwd ));

					shareProps.m_properties.insert(std::make_pair( "ShareRemark", fileShareInfoIter->second.m_shi503_remark ));

					shareProps.m_properties.insert(std::make_pair( "ShareType", ultoa(fileShareInfoIter->second.m_shi503_type, sharetypebuf, 16)));
				}
			}

			fsInfo.m_shareProperties.insert(std::make_pair(fileShareInfoIter->second.m_shareName, shareProps) );
			fileShareInfoIter++;
		}
	}
	else
	{
		std::stringstream errorString;
		errorString << "Instance Name " << networkName << " not found";
		fsInfo.m_InstanceInformation.insert(std::make_pair("ErrorCode", "-1")); 
		fsInfo.m_InstanceInformation.insert(std::make_pair("ErrorString", errorString.str()));  
		DebugPrintf(SV_LOG_DEBUG, "The network name %s not found in the discovered info.\n", networkName.c_str());
	}
	bRet = SVS_OK; 
 	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet;	
}
void FSDiscoveryAndValidator::PostDiscoveryInfoToCx( std::list<std::string>& networkNameList)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DiscoveryControllerPtr discCtrlPtr = DiscoveryController::getInstance() ;
    //DiscoveryInfo discoverInfo ;
    //fillDiscoveredSystemInfo(*(discCtrlPtr->m_disCoveryInfo));
    std::list<std::string>::iterator networkNameListIter = networkNameList.begin();
    while(networkNameListIter != networkNameList.end() )
    {
		FileServerInfo fsInfo;
        fillPolicyInfoInfo(fsInfo.m_policyInfo);
		fillFileServerDiscoveryData(*networkNameListIter, fsInfo);  
		discCtrlPtr->m_disCoveryInfo->fileserverInfo.push_back(fsInfo);
        networkNameListIter++;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

SVERROR FSDiscoveryAndValidator::Discover( bool shouldUpdateCx )
{
    SVERROR retStatus = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
	std::map<std::string, std::list<std::string> > versionInstancesMap ;    
	std::list<std::string> networkNameList;
    
	try
	{
		if( m_FSApp->isInstalled() && !Controller::getInstance()->QuitRequested(1))
		{
			m_FSApp->discoverApplication() ;
			m_FSApp->getNetworkNameList(networkNameList);
			if( m_policyId != 0 )
			{
				getDiscoveryInstanceList(networkNameList) ;
			}
			if( !Controller::getInstance()->QuitRequested(1) )
			{
				if(m_policyId != 0)
				{
					SV_ULONGLONG instanceId = AppScheduler::getInstance()->getInstanceId(m_policyId);
					configuratorPtr->PolicyUpdate(m_policyId,instanceId,"InProgress","");
				}
				if( shouldUpdateCx )
				{
					PostDiscoveryInfoToCx(networkNameList) ;
				}
			}
			retStatus = SVS_OK;
		}
	}
	catch(...)
	{
		DebugPrintf(SV_LOG_ERROR, "Unhandled Exception occurred while discovering Fileshares. Unable to proceed FileServer disovery.") ;
		if( shouldUpdateCx )
		{
			DiscoveryControllerPtr discCtrlPtr = DiscoveryController::getInstance() ;
			FileServerInfo fsInfo;
			fillPolicyInfoInfo(fsInfo.m_policyInfo);
			fsInfo.m_InstanceInformation.insert(std::make_pair("ErrorCode", boost::lexical_cast<std::string>(DISCOVERY_FAIL))); 
			fsInfo.m_InstanceInformation.insert(std::make_pair("ErrorString", "Unhandled Exception occurred while discovering Fileshares. Unable to proceed FileServer disovery.")); 
			discCtrlPtr->m_disCoveryInfo->fileserverInfo.push_back(fsInfo);
		}
		retStatus = SVS_FALSE ;
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return retStatus;
}

SVERROR FSDiscoveryAndValidator::PerformReadinessCheckAtSource()
{
    SVERROR retStatus = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	SV_ULONGLONG instanceId = AppScheduler::getInstance()->getInstanceId(m_policyId);
	AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
	try
	{
		if( IsItActiveNode() )
		{
			DiscoveryControllerPtr discCtrlPtr = DiscoveryController::getInstance() ;
			ReadinessCheck readyCheck;
			RuleEnginePtr enginePtr = RuleEngine::getInstance() ;
			ReadinessCheckInfo readinessCheckInfo ;
			configuratorPtr->PolicyUpdate(m_policyId,instanceId,"InProgress","");
			if(m_FSApp->discoverApplication() == SVS_OK)
			{
				std::list<std::string> instanceNameList;
				if(getReadinessDiscoveryInstanceList(instanceNameList) == SVS_OK)
				{
					std::string systemDrive ;
					std::list<ReadinessCheck> readinessChecksList;
					std::list<ValidatorPtr>::iterator validateIter;
					std::list<ValidatorPtr> validations ;
					std::list<InmRuleIds> failedRules ;
					getEnvironmentVariable("SystemDrive", systemDrive) ;
					if(m_context == CONTEXT_HADR)
					{
						std::string srcHostName;
						if( m_policy.m_policyProperties.find("SrcVirtualServerName") != m_policy.m_policyProperties.end() )
						{
							srcHostName = m_policy.m_policyProperties.find("SrcVirtualServerName")->second ;
						}
						else if ( m_policy.m_policyProperties.find("SourceHostName") != m_policy.m_policyProperties.end() )
						{
							srcHostName = m_policy.m_policyProperties.find("SourceHostName")->second ;
						}

						bool bDnsFailover = true;
						if( m_policy.m_policyProperties.find("DNSFailover") != m_policy.m_policyProperties.end() )
						{
							if(boost::lexical_cast<bool>(m_policy.m_policyProperties.find("DNSFailover")->second) == true)
								bDnsFailover = true;
							else
								bDnsFailover = false;
						}
						validations = enginePtr->getSystemRules(m_scenarioType, m_context, m_applicationType, bDnsFailover, srcHostName) ;
						validateIter = validations.begin() ;
						while(validateIter != validations.end() )
						{
							(*validateIter)->setFailedRules(failedRules) ;
							enginePtr->RunRule(*validateIter) ;
							if( (*validateIter)->getStatus() != INM_RULESTAT_PASSED )
							{
								failedRules.push_back((*validateIter)->getRuleId()) ;
							}
							ReadinessCheck readyCheck ;
							(*validateIter)->convertToRedinessCheckStruct(readyCheck);
							readinessChecksList.push_back(readyCheck) ;
							validateIter ++ ;
						}
					}
					if(m_scenarioType != SCENARIO_FASTFAILBACK)
					{
						bool tagVerifyRule = false ;
						std::list<std::string>::const_iterator instanceIter = instanceNameList.begin() ;
						while( instanceIter != instanceNameList.end() )
						{
							std::list<ValidatorPtr> fsAppLevelValidations ;
							fsAppLevelValidations = enginePtr->getFileServerRules(m_scenarioType, m_context);
							std::list<ValidatorPtr>::iterator validateIter = fsAppLevelValidations.begin() ;
							std::list<InmRuleIds> failedRules ;
							while(validateIter != fsAppLevelValidations.end() )
							{
								(*validateIter)->setFailedRules(failedRules) ;
								enginePtr->RunRule(*validateIter) ;
								if( (*validateIter)->getStatus() != INM_RULESTAT_PASSED )
								{
									failedRules.push_back((*validateIter)->getRuleId()) ;
								}
								ReadinessCheck readyCheck ;
								(*validateIter)->convertToRedinessCheckStruct(readyCheck);
								readinessChecksList.push_back(readyCheck) ;
								validateIter ++ ;
							}
							FileServerInstanceMetaData instanceMetaData;
							if(m_FSApp->getDiscoveredMetaDataByInstance(*instanceIter, instanceMetaData) == SVS_OK)
							{
								validations = enginePtr->getFileServerInstanceLevelRules(instanceMetaData, m_scenarioType, m_context, systemDrive, m_appVacpOptions);
								std::list<ValidatorPtr>::iterator validateIter = validations.begin() ;
								std::list<InmRuleIds> failedRules ;
								while(validateIter != validations.end() )
								{
									if( (*validateIter)->getRuleId() == CONSISTENCY_TAG_CHECK)
									{
										if( tagVerifyRule )
										{
											validateIter ++ ;
											continue ;
										}
										else
										{
											tagVerifyRule = true ;
										}
									}
									(*validateIter)->setFailedRules(failedRules) ;
									enginePtr->RunRule(*validateIter) ;
									if( (*validateIter)->getStatus() != INM_RULESTAT_PASSED )
									{
										failedRules.push_back((*validateIter)->getRuleId()) ;
									}
									ReadinessCheck readyCheck ;
									(*validateIter)->convertToRedinessCheckStruct(readyCheck);
									readinessChecksList.push_back(readyCheck) ;
									validateIter ++ ;
								}
							}
							instanceIter++ ;
						}
					}
					readinessCheckInfo.propertiesList.insert(std::make_pair("PolicyId", boost::lexical_cast<std::string>(m_policyId))) ;
					readinessCheckInfo.propertiesList.insert(std::make_pair("InstanceId",boost::lexical_cast<std::string>(instanceId)));
					readinessCheckInfo.validationsList = readinessChecksList ;
					discCtrlPtr->m_ReadinesscheckInfoList.push_back(readinessCheckInfo) ;
					retStatus = SVS_OK ;
				}
				else
				{
					configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Failed","Failed to get the instancelist from CX. Unable to process the SoureReadinessCheck.");
					DebugPrintf(SV_LOG_ERROR,"Failed to get the instancelist from CX. Unable to process the SoureReadinessCheck.\n");
					retStatus = SVS_FALSE;
				}				
			}
			else
			{
				DebugPrintf(SV_LOG_DEBUG, "Failed on discovering FileShares... Unable to process Source Readiness Check.\n" );
				configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Failed","Failed on discovering FileShares during Source Readiness Check.");
				retStatus = SVS_FALSE;
			}
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG, "Wont process the policy %ld as this is passive node\n", m_policyId) ;
			retStatus = SVS_OK;
		}
	}
	catch(std::exception& ex)
	{
		DebugPrintf(SV_LOG_ERROR, "Got an exception %s. Unable to process Source Readiness Check.\n", ex.what());
		configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Failed","Got an exception during Source Readiness Check.");
		retStatus = SVS_FALSE ;
	}
	catch(...)
	{
		DebugPrintf(SV_LOG_DEBUG, "Got an unhandled exception. Unable to process Source Readiness Check.\n" );
		configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Failed","Got an unhandled exception during Source Readiness Check.");
		retStatus = SVS_FALSE ;
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return retStatus;
}

SVERROR FSDiscoveryAndValidator::PerformReadinessCheckAtTarget()
{
    SVERROR retStatus = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
    SV_ULONGLONG instanceId = AppScheduler::getInstance()->getInstanceId(m_policyId);
	try
	{
		if( IsItActiveNode() )
		{
			configuratorPtr->PolicyUpdate(m_policyId,instanceId,"InProgress","");
			DiscoveryControllerPtr discCtrlPtr = DiscoveryController::getInstance() ;
			std::list<ReadinessCheck> readinessChecksList;

			/*if(m_context != CONTEXT_BACKUP)
			{
				if( m_FSApp->discoverApplication() == SVS_FALSE )
				{
					DebugPrintf(SV_LOG_DEBUG, "Failed on discovering FileShares... Unable to process Target Readiness Check.\n" );
					configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Failed","Failed on discovering FileShares during Target Readiness Check.");
					DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
					return retStatus;
				}
			}*/ //Target readinesscheck don't need any discovery info.

			FileServerTargetReadinessCheckInfo srcFSTgtReadinessChekInfo;
			std::string marshalledSrcReadinessCheckString = m_policy.m_policyData ;
			DebugPrintf(SV_LOG_DEBUG, "The string which need to unmarshall is : %s \n", marshalledSrcReadinessCheckString.c_str()) ;
			try
			{
				srcFSTgtReadinessChekInfo = unmarshal<FileServerTargetReadinessCheckInfo>(marshalledSrcReadinessCheckString ) ;
				std::list<ValidatorPtr> validations ;
				std::list<ValidatorPtr>::iterator validateIter;
				std::list<InmRuleIds> failedRules ;
				RuleEnginePtr enginePtr = RuleEngine::getInstance() ;
				if(m_context == CONTEXT_HADR)
				{
					std::string tgthostname;
					std::string srcHostName;
					if( m_policy.m_policyProperties.find("SrcVirtualServerName") != m_policy.m_policyProperties.end() )
					{
						srcHostName = m_policy.m_policyProperties.find("SrcVirtualServerName")->second ;
					}
					else if ( m_policy.m_policyProperties.find("SourceHostName") != m_policy.m_policyProperties.end() )
					{
						srcHostName = m_policy.m_policyProperties.find("SourceHostName")->second ;
					}
					if( m_policy.m_policyProperties.find("TgtVirtualServerName") != m_policy.m_policyProperties.end() )
					{
						tgthostname = m_policy.m_policyProperties.find("TgtVirtualServerName")->second ;
					}
					else if ( m_policy.m_policyProperties.find("TargetHostName") != m_policy.m_policyProperties.end() )
					{
						tgthostname = m_policy.m_policyProperties.find("TargetHostName")->second ;
					}
					bool bDnsFailover = true;
					if( m_policy.m_policyProperties.find("DNSFailover") != m_policy.m_policyProperties.end() )
					{
						if(boost::lexical_cast<bool>(m_policy.m_policyProperties.find("DNSFailover")->second) == true)
							bDnsFailover = true;
						else
							bDnsFailover = false;
					}
					validations = enginePtr->getSystemRules(m_scenarioType, m_context, m_applicationType, bDnsFailover, srcHostName, tgthostname, true) ;
					validateIter = validations.begin() ;
					while(validateIter != validations.end() )
					{
						(*validateIter)->setFailedRules(failedRules) ;
						enginePtr->RunRule(*validateIter) ;
						if( (*validateIter)->getStatus() != INM_RULESTAT_PASSED )
						{
							failedRules.push_back((*validateIter)->getRuleId()) ;
						}
						ReadinessCheck readyCheck ;
						(*validateIter)->convertToRedinessCheckStruct(readyCheck);
						readinessChecksList.push_back(readyCheck) ;
						validateIter ++ ;
					}
				}
				if(m_scenarioType != SCENARIO_FASTFAILBACK)
				{
					failedRules.clear() ;
					validations = enginePtr->getFileServerTargetRedinessRules(srcFSTgtReadinessChekInfo, m_scenarioType, m_context, m_totalNumberOfPairs) ;
					validateIter = validations.begin() ;
					while(validateIter != validations.end() )
					{
						ReadinessCheck readyCheck ;
						(*validateIter)->setFailedRules(failedRules) ;
						enginePtr->RunRule(*validateIter) ;
						if( (*validateIter)->getStatus() != INM_RULESTAT_PASSED )
						{
							failedRules.push_back((*validateIter)->getRuleId()) ;
						}
						(*validateIter)->convertToRedinessCheckStruct(readyCheck);
						readinessChecksList.push_back(readyCheck) ;
						validateIter ++ ;
					}
				}
				AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
				ReadinessCheckInfo readinessCheckInfo ;
				readinessCheckInfo.propertiesList.insert(std::make_pair("PolicyId", boost::lexical_cast<std::string>(m_policyId))) ;
				readinessCheckInfo.propertiesList.insert(std::make_pair("InstanceId",boost::lexical_cast<std::string>(instanceId)));
				readinessCheckInfo.validationsList = readinessChecksList ;

				discCtrlPtr->m_ReadinesscheckInfoList.push_back(readinessCheckInfo) ;
			}
			catch(std::exception& ex)
			{
				DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling %s ... Unable to process Target Readiness Check.\n", ex.what()) ;
				configuratorPtr->PolicyUpdate(m_policyId, instanceId, "Failed", "Error while unmarshalling... Unable to process Target Readiness Check.");
				retStatus = SVS_FALSE ;
			}
			catch(...)
			{
				DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling... Unable to process Target Readiness Check.\n") ;
				configuratorPtr->PolicyUpdate(m_policyId, instanceId, "Failed", "Error while unmarshalling... Unable to process Target Readiness Check.");
				retStatus = SVS_FALSE ;
			}
		} 
		else
		{
			DebugPrintf(SV_LOG_DEBUG, "Wont process the policy %ld as this is passive node\n", m_policyId) ;
			retStatus = SVS_OK;
		}
	}
	catch(std::exception& ex)
	{
		DebugPrintf(SV_LOG_ERROR, "Got an exception %s. Unable to process Target Readiness Check.\n", ex.what());
		configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Failed","Got an exception during Target Readiness Check.");
		retStatus = SVS_FALSE ;
	}
	catch(...)
	{
		DebugPrintf(SV_LOG_DEBUG, "Got an unhandled exception. Unable to process Target Readiness Check.\n" );
		configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Failed","Got an unhandled exception during Target Readiness Check.");
		retStatus = SVS_FALSE ;
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return retStatus;
}

bool FSDiscoveryAndValidator::IsItActiveNode()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bProcess = false ;
    std::list<std::string> networkNameList;
	std::map<std::string, std::string> propsMap = m_policy.m_policyProperties ;
    if( isClusterNode() )
    {
		OSVERSIONINFOEX osVersionInfo ;
		getOSVersionInfo(osVersionInfo) ;
		if(osVersionInfo.dwMajorVersion == 6 /*&& (m_policy.m_policyProperties.find("PolicyType")->second.compare("TargetReadinessCheck") == 0)*/) //making this condition true for windows 2008 sp1,sp2 and R2
		{
			std::list<std::string> fileServerResourceNames;
			CLUSTER_RESOURCE_STATE state;
			std::string groupName, ownerNodeName;
			std::string hostName = Host::GetInstance().GetHostName();
			std::map<std::string, std::string>::iterator iInstanceName = propsMap.find("InstanceName");
			if(iInstanceName == propsMap.end())
			{
				DebugPrintf(SV_LOG_ERROR, "InstanceName property not found in the policy properties. Considering this node as passive node.\n");
			}
			else if( getResourcesByType("", "File Server", fileServerResourceNames) == SVS_OK )
			{
				DebugPrintf(SV_LOG_DEBUG, "Searching Instance Name: %s\n", iInstanceName->second.c_str());

				std::list<std::string> networknameKeyList;
				networknameKeyList.push_back("Name");
				
				std::list<std::string> dependentNameList;

				std::map< std::string, std::string> outputPropertyMap;
				
				std::list<std::string>::iterator fileServerResourceNamesIter = fileServerResourceNames.begin();
				while( fileServerResourceNamesIter != fileServerResourceNames.end() )
				{
					DebugPrintf(SV_LOG_DEBUG, "Verifying File Server Resource Name: %s\n", fileServerResourceNamesIter->c_str());

					dependentNameList.clear();
					outputPropertyMap.clear();
					if( getResourceState("",*fileServerResourceNamesIter, state, groupName, ownerNodeName ) == SVS_OK )
					{
						if( strcmpi(hostName.c_str(), ownerNodeName.c_str()) == 0)
						{
							if( getDependedResorceNameListOfType( *fileServerResourceNamesIter, "Network Name", dependentNameList ) == SVS_OK)
							{
								if(dependentNameList.empty())
								{
									DebugPrintf(SV_LOG_WARNING, "Network Name dependency is not configured for the FileServer Resource %s\n",
										fileServerResourceNamesIter->c_str());
								}
								else
								{
									std::string networkName;
									std::string dependentNetworkName = *(dependentNameList.begin());
									getResourcePropertiesMap( "",dependentNetworkName , networknameKeyList, outputPropertyMap);
									if(outputPropertyMap.find("Name") != outputPropertyMap.end())
										networkName = outputPropertyMap.find("Name")->second;

									if(0 == strcmpi(networkName.c_str(),iInstanceName->second.c_str()))
									{
										bProcess = true;
										break;
									}
								}
							}	
						}
					}
					else
					{
						DebugPrintf(SV_LOG_ERROR, "getResourceState failed. Considering this node as passive node for %s.\n",
							fileServerResourceNamesIter->c_str()) ;
					}
					fileServerResourceNamesIter++;
				}
			}
		}
		else
		{
			FileServerAplicationPtr fileSvrPtr;
			fileSvrPtr.reset( new FileServerAplication() );
			if( fileSvrPtr->isInstalled() )
			{
				fileSvrPtr->discoverApplication();
				fileSvrPtr->getActiveInstanceNameList(networkNameList);
				std::map<std::string, std::string>::iterator iterMap = propsMap.find("InstanceName");
				if(iterMap != propsMap.end())
				{
					if(isfound(networkNameList, iterMap->second))
					{
						bProcess = true;
						DebugPrintf(SV_LOG_INFO,"Network Name  %s  resides on local Host\n",iterMap->second.c_str());
					}
				}	
				else
				{
					DebugPrintf(SV_LOG_INFO,"\n This node is not active node \n");
				}
			}
			else
			{
				DebugPrintf( SV_LOG_ERROR, "\n File Server is Not installed.. \n" );
			}
		}
    }
    else
    {
        bProcess = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bProcess;
}
