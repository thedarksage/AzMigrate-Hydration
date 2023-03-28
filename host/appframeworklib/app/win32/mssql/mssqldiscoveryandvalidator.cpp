#include <boost/lexical_cast.hpp>
#include "mssqldiscoveryandvalidator.h"
#include "config/appconfigurator.h"
#include "util/common/util.h"
#include "service.h"
#include "ruleengine.h"
#include "system.h"
#include "controller.h"

SQLDiscoveryAndValidator::SQLDiscoveryAndValidator(const ApplicationPolicy& policy)
: DiscoveryAndValidator(policy)
{

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    m_SqlApp.reset(new MSSQLApplication);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}
SVERROR SQLDiscoveryAndValidator::fillSQLDiscData(const std::string& instanceName, 
					std::map<std::string, std::string>& instnceInfoMap, const bool& bProtected)
{
    SVERROR retStatus = SVS_OK ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    MSSQLInstanceDiscoveryInfo instanceDiscInfo;
    if(m_SqlApp->getDiscoveredDataByInstance(instanceName, instanceDiscInfo) == SVS_OK)
    {
        instnceInfoMap.insert(std::make_pair("ApplicationInstanceName", instanceDiscInfo.m_instanceName));
        instnceInfoMap.insert(std::make_pair("ApplicationVersion", instanceDiscInfo.m_version));
        instnceInfoMap.insert(std::make_pair("ApplicationEdition", instanceDiscInfo.m_edition));
        instnceInfoMap.insert(std::make_pair("ApplicationCompatibiltiNo", "1000000"));
        instnceInfoMap.insert(std::make_pair("InstallPath", instanceDiscInfo.m_installPath));
        instnceInfoMap.insert(std::make_pair("DataRoot", instanceDiscInfo.m_dataDir));
        if(instanceDiscInfo.m_isClustered || instanceDiscInfo.m_virtualSrvrName.compare("") != 0)
        {
            instnceInfoMap.insert(std::make_pair("IsClustered", "1"));
            instnceInfoMap.insert(std::make_pair("VirtualServerName", instanceDiscInfo.m_virtualSrvrName));
        }
        else
        {
            instnceInfoMap.insert(std::make_pair("IsClustered", "0"));
            instnceInfoMap.insert(std::make_pair("VirtualServerName", instanceDiscInfo.m_virtualSrvrName));
        }        

		DiscoveryControllerPtr discoveryController = DiscoveryController::getInstance();
		std::string clusterID = discoveryController->m_hostLevelDiscoveryInfo.m_clusterInfo.m_clusterID;

        if( instanceDiscInfo.m_version.compare("") == 0 || 
            ( clusterID.compare("") == 0 && instanceDiscInfo.m_isClustered ) )
        {
            instnceInfoMap.insert(std::make_pair("ErrorCode",boost::lexical_cast<std::string>(-1)));
            instnceInfoMap.insert(std::make_pair("ErrorCode","The discovery information is not available"));
        }
        else
        {
	        if(bProtected)
	        {
	            instnceInfoMap.insert(std::make_pair("ErrorString", "This is protected"));
	            instnceInfoMap.insert(std::make_pair("ErrorCode",boost::lexical_cast<std::string>(IN_PROTECTION)));
	        }
	        else
	        {
	            instnceInfoMap.insert(std::make_pair("ErrorString", instanceDiscInfo.errStr));
	            instnceInfoMap.insert(std::make_pair("ErrorCode",boost::lexical_cast<std::string>(instanceDiscInfo.errCode)));
			}
		}
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);	
    return retStatus;
}
SVERROR SQLDiscoveryAndValidator::fillSQLDiscMetaData(const std::string& instanceName, std::map<std::string, MSSQLDBMetaDataInfo>& sqlMetaDatInfoMap,const bool& bProtected )
{
    SVERROR retStatus = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);	
    MSSQLInstanceMetaData instanceMetaData;
    if( m_SqlApp->getDiscoveredMetaDataByInstance(instanceName, instanceMetaData) == SVS_OK )
    {
        std::map<std::string, MSSQLDBMetaData>::iterator MSSQLDBMetaDataIter = instanceMetaData.m_dbsMap.begin();
        std::string tempString;

        while(MSSQLDBMetaDataIter != instanceMetaData.m_dbsMap.end())
        {	
            MSSQLDBMetaDataInfo metaDataInfo;
            if(bProtected)
            {
                metaDataInfo.m_properties.insert(std::make_pair("ErrorString", "This is protected"));
                metaDataInfo.m_properties.insert(std::make_pair("ErrorString",boost::lexical_cast<std::string>(IN_PROTECTION)));
            }
            else 
            {
                if(MSSQLDBMetaDataIter->second.m_dbOnline)
                {
                    metaDataInfo.m_properties.insert(std::make_pair("Status", "ONLINE"));	
                }
                else
                {
                    metaDataInfo.m_properties.insert(std::make_pair("Status", "OFFLINE"));	
                }
                metaDataInfo.m_properties.insert(std::make_pair("DbType", boost::lexical_cast<std::string>(MSSQLDBMetaDataIter->second.m_dbType)));	
                std::list<std::string>::iterator volIter = MSSQLDBMetaDataIter->second.m_volumes.begin();
                while(volIter != MSSQLDBMetaDataIter->second.m_volumes.end())
                {
                    tempString = *volIter;
                    sanitizeVolumePathName(tempString);
                    metaDataInfo.m_dbVolumes.push_back(tempString);
                    volIter++;
                }
                metaDataInfo.m_properties.insert(std::make_pair("ErrorCode",boost::lexical_cast<std::string>(MSSQLDBMetaDataIter->second.errCode)));
                metaDataInfo.m_properties.insert(std::make_pair("ErrorString",boost::lexical_cast<std::string>(MSSQLDBMetaDataIter->second.errorString)));
            }
            metaDataInfo.m_LogFilesList = MSSQLDBMetaDataIter->second.m_dbFiles;
            sqlMetaDatInfoMap.insert(std::make_pair(MSSQLDBMetaDataIter->first, metaDataInfo));  
            MSSQLDBMetaDataIter++;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);	
    return retStatus;
}
void SQLDiscoveryAndValidator::PostDiscoveryInfoToCx(std::list<std::string>& instanceNameList)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DiscoveryControllerPtr discCtrlPtr = DiscoveryController::getInstance() ;
    //DiscoveryInfo discoverInfo ;
    //fillDiscoveredSystemInfo(*(discCtrlPtr->m_disCoveryInfo));
    std::list<std::string>::iterator instanceNameListIter = instanceNameList.begin();
	AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
	bool bProtected = false;
	if(m_policyId != 0)
	{
		bProtected = configuratorPtr->isThisProtected(m_scenarioId);
	}  
	while(instanceNameListIter != instanceNameList.end() )
    {
        SQLDiscoveryInfo sqlDiscInfo;
        fillPolicyInfoInfo(sqlDiscInfo.m_policyInfo);	
        fillSQLDiscData(*instanceNameListIter, sqlDiscInfo.m_InstanceInformation, bProtected);  
        fillSQLDiscMetaData(*instanceNameListIter, sqlDiscInfo.m_sqlMetaDatInfo, bProtected);
        discCtrlPtr->m_disCoveryInfo->sqlInfo.push_back(sqlDiscInfo);

        instanceNameListIter++;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

SVERROR SQLDiscoveryAndValidator::Discover(bool shouldUpdateCx)
{
    SVERROR retStatus = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
    std::list<std::string> instanceNameList;
	
    InmProtectedAppType appType =INM_APP_NONE ;
	if( m_policy.m_policyProperties.find("ApplicationType") != m_policy.m_policyProperties.end() )
	{
		std::string appTypeStr = m_policy.m_policyProperties.find("ApplicationType")->second ;
		if( appTypeStr.compare("SQL2000") == 0 )
		{
			appType = INM_APP_MSSQL2000 ;
		}
		else if( appTypeStr.compare("SQL2005") == 0 )
		{
			appType = INM_APP_MSSQL2005 ;
		}
		else if( appTypeStr.compare("SQL2008") == 0 )
		{
			appType = INM_APP_MSSQL2008 ;
		}
		else if( appTypeStr.compare("SQL2012") == 0 )
		{
			appType = INM_APP_MSSQL2012 ;
		}
	}
	if( INM_APP_NONE == appType )
	{
		m_SqlApp.reset(new MSSQLApplication()) ;
	}
	else
	{
		m_SqlApp.reset(new MSSQLApplication(appType)) ;
	}
	try
	{
		if( m_SqlApp->isInstalled() && !Controller::getInstance()->QuitRequested(1))
		{
			if((m_policyId != 0) && (m_policyType.compare("Discovery") == 0))
			{
				SV_ULONGLONG instanceId = AppScheduler::getInstance()->getInstanceId(m_policyId);
				configuratorPtr->PolicyUpdate(m_policyId,instanceId,"InProgress","");
			}
			if(m_SqlApp->discoverApplication() == SVS_OK)
			{
				m_SqlApp->getInstanceNameList(instanceNameList);
				if( m_policyId != 0 )
				{
					getDiscoveryInstanceList(instanceNameList) ;
				}
				if( !Controller::getInstance()->QuitRequested(1))
				{
					/*Meta data is not required for TargetReadinessCheck*/
					if(m_policyType.compare("TargetReadinessCheck") != 0)
					{
						if(m_SqlApp->discoverMetaData() == SVS_OK)
						{
							retStatus = SVS_OK;
						}
						else
						{
							DebugPrintf(SV_LOG_ERROR,"MS-SQL Application Metadata discovery failed. Policy ID: %d", m_policyId);
							retStatus = SVS_FALSE;
						}
					}
					else
					{
						DebugPrintf(SV_LOG_INFO,"Skipping MS-SQL Application Metadata discovery as it is not required for Target Readiness Check.\
												Policy ID: %d", m_policyId);
						retStatus = SVS_OK;
					}
					if( shouldUpdateCx )
					{
						PostDiscoveryInfoToCx(instanceNameList) ;
					}
				}	
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"MS-SQL Application discovery failed. Policy ID: %d", m_policyId);
				retStatus = SVS_FALSE;
			}
		}
	}
	catch(...)
	{
		DebugPrintf(SV_LOG_ERROR, "Unhandled Exception occurred while discovering MSSQL application. Unable to proceed MSSQL disovery.") ;
		if( shouldUpdateCx )
		{
			DiscoveryControllerPtr discCtrlPtr = DiscoveryController::getInstance() ;
			SQLDiscoveryInfo sqlDiscInfo;
			fillPolicyInfoInfo(sqlDiscInfo.m_policyInfo);
			sqlDiscInfo.m_InstanceInformation.insert(std::make_pair("ErrorCode",boost::lexical_cast<std::string>(DISCOVERY_FAIL))) ;            
			sqlDiscInfo.m_InstanceInformation.insert(std::make_pair("ErrorString","Unhandled Exception occurred while discovering MSSQL application. Unable to proceed MSSQL disovery.")) ;
			discCtrlPtr->m_disCoveryInfo->sqlInfo.push_back(sqlDiscInfo);
		}
		retStatus = SVS_FALSE ;
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return retStatus;
}

SVERROR SQLDiscoveryAndValidator::PerformReadinessCheckAtSource()
{
    SVERROR retStatus = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	SV_ULONGLONG instanceId = AppScheduler::getInstance()->getInstanceId(m_policyId);
	AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
	try
	{
		if( IsItActiveNode() )
		{
			configuratorPtr->PolicyUpdate(m_policyId,instanceId,"InProgress","Source Readiness Check is in progress");
			DiscoveryControllerPtr discCtrlPtr = DiscoveryController::getInstance() ;
			RuleEnginePtr enginePtr = RuleEngine::getInstance() ;
			ReadinessCheckInfo readinessCheckInfo ;

			if(Discover(false) == SVS_OK)
			{				
				std::list<std::string> instanceNameList;     
				if(getReadinessDiscoveryInstanceList(instanceNameList) == SVS_OK)
				{
					std::string systemDrive ;
					std::list<ReadinessCheck> readinessChecksList;
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
						std::list<ValidatorPtr>::iterator validateIter = validations.begin() ;

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
							MSSQLInstanceDiscoveryInfo instanceDiscdata;
							if( m_SqlApp->getDiscoveredDataByInstance(*instanceIter, instanceDiscdata) == SVS_OK)
							{
								std::list<ValidatorPtr> sqlVersionLevelValidations ;
								sqlVersionLevelValidations = enginePtr->getMSSQLVersionLevelRules(instanceDiscdata.m_appType, m_scenarioType, m_context);
								std::list<ValidatorPtr>::iterator validateIter = sqlVersionLevelValidations.begin();
								while(validateIter != sqlVersionLevelValidations.end())
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
							MSSQLInstanceMetaData instanceMetaData;
							if(m_SqlApp->getDiscoveredMetaDataByInstance(*instanceIter, instanceMetaData) == SVS_OK)
							{
								instanceMetaData.m_instanceName = *instanceIter ;
								validations = enginePtr->getMSSQLInstanceRules(instanceMetaData, m_scenarioType, m_context, systemDrive,m_appVacpOptions);
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
					configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Failed","Failed to get the SQL instancelist from CX. Unable to process the SoureReadinessCheck.");
					DebugPrintf(SV_LOG_ERROR,"Failed to get the SQL instancelist from CX. Unable to process the SoureReadinessCheck.\n");
					retStatus = SVS_FALSE;
				}
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to discover MSQSQL Application... Unable to process Source Readiness Check.\n" );
				configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Failed","Failed on discovering MSSQL application during Source Readiness Check.");
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
		DebugPrintf(SV_LOG_ERROR, "Got an exception %s. Unable to process Source Readiness Check.\n", ex.what());
		configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Failed","Got an exception during Source Readiness Check.");
		retStatus = SVS_FALSE ;
	}
	catch(...)
	{
		configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Failed","Got an unhandled exception. Unable to process Source Readiness Check.");
		DebugPrintf(SV_LOG_ERROR, "Got an unhandled exception during Source Readiness Check.\n" );			
		retStatus = SVS_FALSE ;
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return retStatus;
}

SVERROR SQLDiscoveryAndValidator::PerformReadinessCheckAtTarget()
{
    SVERROR retStatus = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
	SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);
	AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
	try
	{
		if( IsItActiveNode() )
		{
			DiscoveryControllerPtr discCtrlPtr = DiscoveryController::getInstance() ;
			std::list<ReadinessCheck> readinessChecksList;
			std::list<std::string> instanceNameList;
			configuratorPtr->PolicyUpdate(m_policyId,instanceId,"InProgress","");
			if(m_context != CONTEXT_BACKUP)
			{
				if(Discover(false) == SVS_FALSE)
				{
					DebugPrintf(SV_LOG_ERROR, "Failed on discovering MSQSQL instance metadata... Unable to process Target Readiness Check.\n" );
					configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Failed","Failed on discovering MSSQL instance metadata during Target Readiness Check.");
					DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
					return retStatus;
				}
			}
			std::list<ReadinessCheckInfo> readinesscheckInfos; 
			MSSQLTargetReadinessCheckInfo srcSqlTgtReadinessChekInfo, tgtSqlTgtReadinessCheckInfo ;
			std::string marshalledSrcReadinessCheckString = m_policy.m_policyData ;
			DebugPrintf(SV_LOG_DEBUG, "The string which need to unmarshall is : %s \n", marshalledSrcReadinessCheckString.c_str());
			try
			{
				srcSqlTgtReadinessChekInfo = unmarshal<MSSQLTargetReadinessCheckInfo>(marshalledSrcReadinessCheckString ) ;
				instanceNameList = srcSqlTgtReadinessChekInfo.m_InstancesList;
				std::list<std::string>::iterator iter = instanceNameList.begin();
				while(iter != instanceNameList.end())
				{
					DebugPrintf(SV_LOG_DEBUG,"Got %s instance from cx \n",iter->c_str());
					iter++;
				}
				std::list<ValidatorPtr> validations ;
				std::list<InmRuleIds> failedRules ;
				std::list<ValidatorPtr>::iterator validateIter;
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
					std::map<std::string, std::map<std::string, std::string>> sqlInstanceTgtReadinessCheckInfoMap ;
					sqlInstanceTgtReadinessCheckInfoMap = srcSqlTgtReadinessChekInfo.m_sqlInstanceTgtReadinessCheckInfo ;
					std::map<std::string, std::map<std::string, std::string>>::const_iterator sqlInstanceTgtReadinessCheckInfoIter ;
					sqlInstanceTgtReadinessCheckInfoIter = sqlInstanceTgtReadinessCheckInfoMap.begin() ;
					while( sqlInstanceTgtReadinessCheckInfoIter != sqlInstanceTgtReadinessCheckInfoMap.end() )
					{
						const std::string instanceName = sqlInstanceTgtReadinessCheckInfoIter->first ;			
						const std::map<std::string, std::string>& sqlSrcInstanceTgtReadinessCheckInfo = sqlInstanceTgtReadinessCheckInfoIter->second  ;
						if(isfound(instanceNameList, instanceName) )
						{
							std::list<MSSQLVersionDiscoveryInfo>::iterator MSSQLVersionDiscoveryInfoListInter;
							MSSQLVersionDiscoveryInfoListInter = m_SqlApp->m_mssqlDiscoveryInfo.m_sqlVersionDiscoveryInfo.begin();
							while(MSSQLVersionDiscoveryInfoListInter != m_SqlApp->m_mssqlDiscoveryInfo.m_sqlVersionDiscoveryInfo.end())
							{
								std::list<MSSQLInstanceDiscoveryInfo>::iterator MSSQLInstanceDiscoveryInfoIter;
								MSSQLInstanceDiscoveryInfoIter = MSSQLVersionDiscoveryInfoListInter->m_sqlInstanceDiscoveryInfo.begin();
								while(MSSQLInstanceDiscoveryInfoIter != MSSQLVersionDiscoveryInfoListInter->m_sqlInstanceDiscoveryInfo.end())
								{
									if( strcmpi(instanceName.c_str(), MSSQLInstanceDiscoveryInfoIter->m_instanceName.c_str()) == 0)
									{
										std::map<std::string, std::string> sqlTgtInstanceTgtReadinessCheckInfo ; 
											if(MSSQLInstanceDiscoveryInfoIter->errCode != DISCOVERY_FAIL)
											{
												sqlTgtInstanceTgtReadinessCheckInfo.insert(std::make_pair("SrcDataRoot", MSSQLInstanceDiscoveryInfoIter->m_dataDir));
												sqlTgtInstanceTgtReadinessCheckInfo.insert(std::make_pair("SrcVersion", MSSQLInstanceDiscoveryInfoIter->m_version)) ;
											}
											else
											{
												DebugPrintf(SV_LOG_DEBUG,"Taking TargetReadiness info from cx\n");
												std::string tgtDataRoot;
												std::string tgtVersion;
												if(sqlSrcInstanceTgtReadinessCheckInfo.find("TgtDataRoot") != sqlSrcInstanceTgtReadinessCheckInfo.end())
												{
													tgtDataRoot = sqlSrcInstanceTgtReadinessCheckInfo.find("TgtDataRoot")->second;
												}
												if(sqlSrcInstanceTgtReadinessCheckInfo.find("TgtVersion") != sqlSrcInstanceTgtReadinessCheckInfo.end())
												{
													tgtVersion = sqlSrcInstanceTgtReadinessCheckInfo.find("TgtVersion")->second;
												}
												DebugPrintf(SV_LOG_DEBUG,"Target DataRoot is %s\n",tgtDataRoot.c_str());
												DebugPrintf(SV_LOG_DEBUG,"Target Version is %s\n",tgtVersion.c_str());
												sqlTgtInstanceTgtReadinessCheckInfo.insert(std::make_pair("SrcDataRoot", tgtDataRoot));
												sqlTgtInstanceTgtReadinessCheckInfo.insert(std::make_pair("SrcVersion", tgtVersion));
											}
											tgtSqlTgtReadinessCheckInfo.m_sqlInstanceTgtReadinessCheckInfo.insert(std::make_pair(instanceName,sqlTgtInstanceTgtReadinessCheckInfo));
										break;
									}
									MSSQLInstanceDiscoveryInfoIter++;
								}
								if( MSSQLInstanceDiscoveryInfoIter != MSSQLVersionDiscoveryInfoListInter->m_sqlInstanceDiscoveryInfo.end() )
								{
									break ;
								}
								MSSQLVersionDiscoveryInfoListInter++;
							}
						}
						sqlInstanceTgtReadinessCheckInfoIter++ ;
					}
					failedRules.clear() ;
					validations = enginePtr->getMSSQLTargetRedinessRules(srcSqlTgtReadinessChekInfo, tgtSqlTgtReadinessCheckInfo, m_scenarioType, m_context, m_totalNumberOfPairs) ;
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
				instanceId = schedulerPtr->getInstanceId(m_policyId);
				ReadinessCheckInfo readinessCheckInfo ;
				readinessCheckInfo.propertiesList.insert(std::make_pair("PolicyId", boost::lexical_cast<std::string>(m_policyId))) ;
				readinessCheckInfo.propertiesList.insert(std::make_pair("InstanceId",boost::lexical_cast<std::string>(instanceId)));
				readinessCheckInfo.validationsList = readinessChecksList ;
				discCtrlPtr->m_ReadinesscheckInfoList.push_back(readinessCheckInfo) ;
				retStatus = SVS_OK;
			}
			catch(std::exception& ex)
			{
				DebugPrintf(SV_LOG_ERROR, "Got an exception %s while unmarshalling. Unable to process Target Readiness Check.\n", ex.what());
				configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Failed","Got an exception while unmarshalling during Target Readiness Check.");
				retStatus = SVS_FALSE ;
			}
			catch(...)
			{
				DebugPrintf(SV_LOG_ERROR, "Got an exception while unmarshalling. Unable to process Target Readiness Check.\n");
				configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Failed","Got an exception while unmarshalling during Target Readiness Check.");
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
		DebugPrintf(SV_LOG_ERROR, "Got an unhandled exception. Unable to process Target Readiness Check.\n" );
		configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Failed","Got an unhandled exception during Target Readiness Check.");
		retStatus = SVS_FALSE ;
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    retStatus = SVS_OK ;
    return retStatus;
}

bool SQLDiscoveryAndValidator::IsItActiveNode()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bIsActiveNode = false;
    std::string strAciveNodeName;
    std::list<std::string> instanceNameList;
    std::map<std::string, std::string>& propsMap = m_policy.m_policyProperties ;

    std::string strLocalHost = Host::GetInstance().GetHostName();
    MSSQLApplicationPtr m_SqlApp ;
    m_SqlApp.reset( new MSSQLApplication() );
    if( isClusterNode() )
    {
        if(m_SqlApp->isInstalled())
        {
            m_SqlApp->discoverApplication();
            m_SqlApp->getActiveInstanceNameList(instanceNameList);
            std::map<std::string, std::string>::iterator iterMap = propsMap.find("InstanceName");
            if(iterMap != propsMap.end())
            {
                if(isfound(instanceNameList,iterMap->second))
                {
                    bIsActiveNode = true;
                    DebugPrintf(SV_LOG_INFO,"Instance Name  %s  resides on local Host :%s\n",iterMap->second.c_str(),strLocalHost.c_str());
                }
            }	
            else
            {
                DebugPrintf(SV_LOG_INFO,"\n This node is not active node %s\n",strLocalHost.c_str());
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR,"\n SQL is not installed on this node. \n");
        }
    }
    else
    {
        bIsActiveNode = true;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bIsActiveNode;
}
