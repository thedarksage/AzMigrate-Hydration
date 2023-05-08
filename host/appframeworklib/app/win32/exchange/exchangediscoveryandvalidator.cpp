#include <boost/lexical_cast.hpp>
#include <list>
#include "exchangediscoveryandvalidator.h"
#include "config/appconfigurator.h"
#include "ruleengine.h"
#include "util/common/util.h"
#include "discovery/discoverycontroller.h"
#include "system.h"
#include "host.h"
#include "appscheduler.h"

extern bool GetVolumePath(const std::string& filePath, std::string& volPath);

ExchangeDiscoveryAndValidator::ExchangeDiscoveryAndValidator(const ApplicationPolicy& policy)
:DiscoveryAndValidator(policy)
{
	m_ExchangeAppPtr.reset(new ExchangeApplication());
}

void ExchangeDiscoveryAndValidator::convertToExchangeDiscoveryInfo(const ExchAppVersionDiscInfo& exchDiscoveryInfo, 
                                                                const ExchangeMetaData& exchMetadata, 
                                                                ExchangeDiscoveryInfo exchdiscoveryInfo
																)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
	bool bProtected = false; 
	if(m_policyId != 0)
	{
		bProtected = configuratorPtr->isThisProtected(m_scenarioId);
	}
	DiscoveryControllerPtr discCtrlPtr = DiscoveryController::getInstance() ;
    exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ApplicationVersion", exchDiscoveryInfo.m_version)) ;
	exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ApplicationEdition", exchDiscoveryInfo.m_edition)) ;
    exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ApplicationCompatibiltiNo", "10000")) ;
    exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("InstallPath", exchDiscoveryInfo.m_installPath )) ;
    exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ApplicationInstanceName", exchDiscoveryInfo.m_cn)) ;
    if( exchDiscoveryInfo.m_isClustered )
    {
        exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("IsClustered", "1")) ;
        exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("VirtualServerName", exchDiscoveryInfo.m_cn));
    }
    else
    {
        exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("IsClustered", "0")) ; 
        exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("VirtualServerName", exchDiscoveryInfo.m_cn));
    }

    exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ServerRole", exchDiscoveryInfo.ServerCurrentRoleToString())) ;
    exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ServerType", exchDiscoveryInfo.ServerRoleToString())) ;
    exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("DistinguishedName", exchDiscoveryInfo.m_dn)) ; 
    if(!bProtected)
	{
		exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ErrorCode",boost::lexical_cast<std::string>(exchDiscoveryInfo.m_errCode))) ;            
		exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ErrorString",exchDiscoveryInfo.m_errString)) ;
	}
	else
	{
		exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ErrorCode", boost::lexical_cast<std::string>(IN_PROTECTION))) ;            
		exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ErrorString", "This is protected")) ;
	}
    
	std::list<InmService>::const_iterator svcIter = exchDiscoveryInfo.m_services.begin() ;
    while( svcIter != exchDiscoveryInfo.m_services.end() )
    {
        InmServiceProperties svcProperties ;
        svcProperties.m_serviceProperties.insert(std::make_pair("ServiceName", svcIter->m_serviceName)) ;
        svcProperties.m_serviceProperties.insert(std::make_pair("StartupType", svcIter->typeAsStr())) ;
        svcProperties.m_serviceProperties.insert(std::make_pair("ServiceStatus", svcIter->statusAsStr())) ;
        svcProperties.m_serviceProperties.insert(std::make_pair("LogonUser", svcIter->m_logonUser)) ;
        svcProperties.m_serviceProperties.insert(std::make_pair("Dependencies", svcIter->m_dependencies)) ;
        exchdiscoveryInfo.m_services.insert(std::make_pair(svcIter->m_serviceName, svcProperties)) ;
        svcIter ++ ;
    }
    if(!bProtected && (exchMetadata.errCode != DISCOVERY_SUCCESS) )
    {
        std::map<std::string, std::string>::iterator discErrStrIter,discErrorCodeIter ;
        discErrStrIter = exchdiscoveryInfo.m_InstanceInformation.find("ErrorString") ;
        discErrorCodeIter = exchdiscoveryInfo.m_InstanceInformation.find("ErrorCode") ;
		DiscoveryControllerPtr discoveryController = DiscoveryController::getInstance();
		std::string clusterID = discoveryController->m_hostLevelDiscoveryInfo.m_clusterInfo.m_clusterID;
        if( exchDiscoveryInfo.m_isClustered && clusterID.compare("") == 0 )
        {
            discErrorCodeIter->second = boost::lexical_cast<std::string>(DISCOVERY_FAIL) ;
        }
        else
        {
            if( boost::lexical_cast<int>(discErrorCodeIter->second) == DISCOVERY_SUCCESS )
            {
                discErrorCodeIter->second = boost::lexical_cast<std::string>(DISCOVERY_METADATANOTAVAILABLE) ;
            }
            else
            {
                discErrorCodeIter->second = boost::lexical_cast<std::string>(exchMetadata.errCode) ;
            }
        }
        discErrStrIter->second += exchMetadata.errString ;
    }
    std::list<ExchangeStorageGroupMetaData> storageGrpList = exchMetadata.m_storageGrps ;
    std::list<ExchangeStorageGroupMetaData>::iterator storageGrpIter = storageGrpList.begin() ;
    while(!bProtected && (storageGrpIter != storageGrpList.end()) )
    {
        StorageGroup sg ;
		std::string sgSystemVolume;
        sg.m_StorageGrpProperties.insert(std::make_pair("StorageGroupName", storageGrpIter->m_storageGrpName)) ;
        sg.m_StorageGrpProperties.insert(std::make_pair("TransactionLogPath", storageGrpIter->m_logFilePath)) ;
		sg.m_StorageGrpProperties.insert(std::make_pair("StorageGroupSystemPath", storageGrpIter->m_systemPath)) ;
		sg.m_StorageGrpProperties.insert(std::make_pair("StorageGroupSystemVolume", storageGrpIter->m_systemVolumePath)) ;
        sg.m_StorageGrpProperties.insert(std::make_pair("OnlineStatus", "ONLINE")) ;
            
        if( storageGrpIter->m_lcrEnabled )
        {
            sg.m_StorageGrpProperties.insert(std::make_pair("LCRStatus", "ENABLED")) ;
        }
        else
        {
            sg.m_StorageGrpProperties.insert(std::make_pair("LCRStatus", "DISABLED")) ;
        }
        if( storageGrpIter->m_msExchStandbyCopyMachines.compare("") == 0 )
        {
            sg.m_StorageGrpProperties.insert(std::make_pair("SCRStatus", "DISABLED")) ;
        }
        else
        {
            sg.m_StorageGrpProperties.insert(std::make_pair("SCRStatus", "ENABLED")) ;
        }
        const std::string& clusterStorageType = exchDiscoveryInfo.ClusterStorageTypeToStr() ;
        if( clusterStorageType.compare("Disabled")== 0 )
        {
            sg.m_StorageGrpProperties.insert(std::make_pair("CCRSTATUS", "DISABLED")  ) ;
            sg.m_StorageGrpProperties.insert(std::make_pair("ClusterStorageType", clusterStorageType)) ;
        }
        else
        {
            sg.m_StorageGrpProperties.insert(std::make_pair("CCRSTATUS", "DISABLED")) ;
            sg.m_StorageGrpProperties.insert(std::make_pair("ClusterStorageType", "N.A.")) ;
        }
	    std::string tempString1;
	    tempString1 = storageGrpIter->m_logVolumePath;
	    sanitizeVolumePathName(tempString1);
        sg.m_volumes.push_back( tempString1 ) ;
        std::list<ExchangeMDBMetaData>::iterator mdbIter = storageGrpIter->m_mdbMetaDataList.begin()  ;
        while( mdbIter != storageGrpIter->m_mdbMetaDataList.end() )
        {
            MailBox mb ;
            mb.m_mailBoxProperties.insert(std::make_pair("MailBoxName", mdbIter->m_mdbName)) ;
            mb.m_mailBoxProperties.insert(std::make_pair("OnlineStatus", "ONLINE")) ;
            mb.m_mailBoxProperties.insert(std::make_pair("EdbFilePath", mdbIter->m_edbFilePath)) ;
            if( mdbIter->m_type == EXCHANGE_PRIV_MDB )
            {
                mb.m_mailBoxProperties.insert(std::make_pair("MailStoreType", "PRIVATE")) ;
                mb.m_mailBoxProperties.insert(std::make_pair("PublicFolderDatabaseMailServerName", mdbIter->m_defaultPublicFolderDBs_MailBoxServerName)) ;
                mb.m_mailBoxProperties.insert(std::make_pair("PublicFolderDatabaseStorageGroup", mdbIter->m_defaultPublicFolderDBs_StorageGRPName)) ;
                mb.m_mailBoxProperties.insert(std::make_pair("PublicFolderDatabaseName", mdbIter->m_defaultPublicFolderDBName)) ;
            }
            else if( mdbIter->m_type == EXCHANGE_PUB_MDB )
            {
                mb.m_mailBoxProperties.insert(std::make_pair("MailStoreType", "PUBLIC")) ;
            }

		    std::string tempString;
            tempString = mdbIter->m_edbVol ;
		    sanitizeVolumePathName(tempString);
            mb.mailstoreFiles.insert( std::make_pair(mdbIter->m_edbFilePath,  tempString)) ;
            if( mdbIter->m_streamingDB.compare("") != 0 )
            {
                tempString = mdbIter->m_streamingVol ;
		        sanitizeVolumePathName(tempString);
                mb.mailstoreFiles.insert( std::make_pair(mdbIter->m_streamingDB,  tempString)) ;
            }
            mb.m_mailBoxProperties.insert(std::make_pair("ErrorCode", boost::lexical_cast<std::string>(mdbIter->errCode))) ;
            mb.m_mailBoxProperties.insert(std::make_pair("ErrorString", mdbIter->errString)) ;
            sg.m_mailBoxes.insert(std::make_pair(mdbIter->m_mdbName, mb)) ;
            mdbIter++ ;
        }
        sg.m_StorageGrpProperties.insert(std::make_pair("ErrorCode",boost::lexical_cast<std::string> (storageGrpIter->errCode))) ;
        sg.m_StorageGrpProperties.insert(std::make_pair("ErrorString",storageGrpIter->errString)) ;
        exchdiscoveryInfo.m_storageGrpMap.insert(std::make_pair(storageGrpIter->m_storageGrpName, sg)) ;
        storageGrpIter ++ ;
    }
    fillPolicyInfoInfo(exchdiscoveryInfo.m_policyInfo);
    discCtrlPtr->m_disCoveryInfo->exchInfo.push_back(exchdiscoveryInfo) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ExchangeDiscoveryAndValidator::convertToExchange2010DiscoveryInfo(const ExchAppVersionDiscInfo& exchVerDiscoveryInfo, 
                                                                const Exchange2k10MetaData& exch2010Metadata, 
                                                                ExchangeDiscoveryInfo exchdiscoveryInfo)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
	bool bProtected = false;
	if(m_policyId)
	{
		bProtected = configuratorPtr->isThisProtected(m_scenarioId);
	}
	DiscoveryControllerPtr discCtrlPtr = DiscoveryController::getInstance() ;
    exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ApplicationVersion", exchVerDiscoveryInfo.m_version)) ;
	exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ApplicationEdition", exchVerDiscoveryInfo.m_edition)) ;
    exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ApplicationCompatibiltiNo", "10000")) ;
    exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("InstallPath", exchVerDiscoveryInfo.m_installPath )) ;
    exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ApplicationInstanceName", exchVerDiscoveryInfo.m_cn)) ;
    exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ServerRole", exchVerDiscoveryInfo.ServerCurrentRoleToString())) ;
    exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ServerType", exchVerDiscoveryInfo.ServerRoleToString())) ;
    exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("DistinguishedName", exchVerDiscoveryInfo.m_dn)) ; 
    if(bProtected)
	{
		exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ErrorCode", boost::lexical_cast<std::string>(IN_PROTECTION))) ;            
		exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ErrorString", "This is protected")) ;
	}
	else
	{
		exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ErrorCode", boost::lexical_cast<std::string>(exchVerDiscoveryInfo.m_errCode))) ;            
		exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ErrorString", exchVerDiscoveryInfo.m_errString)) ;
	}
	std::string DAGName = "";
	std::string hostName = Host::GetInstance().GetHostName();
	if( !exch2010Metadata.m_mdbMetaDataList.empty() && 
		strcmpi(exch2010Metadata.m_mdbMetaDataList.begin()->m_exch2k10MDBmetaData.m_dagName.c_str(), hostName.c_str()))
	{
		DAGName = exch2010Metadata.m_mdbMetaDataList.begin()->m_exch2k10MDBmetaData.m_dagName;
		exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("DataAvailabilityGroup", DAGName)) ; 
	}	
	std::list<ExchangeMDBMetaData>::const_iterator exch2k10Iter = exch2010Metadata.m_mdbMetaDataList.begin();
	if( exch2k10Iter != exch2010Metadata.m_mdbMetaDataList.end() )
	{
		exchdiscoveryInfo.m_exchange2010Data.m_dagParticipants = exch2k10Iter->m_exch2k10MDBmetaData.m_participantsServersList;
	}
    if(!bProtected && (exch2010Metadata.errCode != DISCOVERY_SUCCESS) )
    {
        std::map<std::string, std::string>::iterator discErrStrIter,discErrorCodeIter ;
        discErrStrIter = exchdiscoveryInfo.m_InstanceInformation.find("ErrorString") ;
        discErrorCodeIter = exchdiscoveryInfo.m_InstanceInformation.find("ErrorCode") ;
        if(  boost::lexical_cast<int>(discErrorCodeIter->second) == DISCOVERY_SUCCESS )
        {
            discErrorCodeIter->second = boost::lexical_cast<std::string>(DISCOVERY_METADATANOTAVAILABLE) ;
        }
        else
        {
            discErrorCodeIter->second = boost::lexical_cast<std::string>(exch2010Metadata.errCode) ;
        }
        discErrStrIter->second += exch2010Metadata.errString ;
    }
	while(!bProtected && (exch2k10Iter != exch2010Metadata.m_mdbMetaDataList.end()) )
	{
		MailBox mb;
		mb.m_mailBoxProperties.insert(std::make_pair("MailBoxName", exch2k10Iter->m_mdbName ));
		mb.m_mailBoxProperties.insert(std::make_pair("OnlineStatus","ONLINE" ));
		mb.m_mailBoxProperties.insert(std::make_pair("TransactionLogPath", exch2k10Iter->m_exch2k10MDBmetaData.m_msExchESEParamLogFilePath ));
		mb.m_mailBoxProperties.insert(std::make_pair("EdbFilePath", exch2k10Iter->m_edbFilePath ));
		mb.m_mailBoxProperties.insert(std::make_pair("GUID", exch2k10Iter->m_exch2k10MDBmetaData.m_guid));
		mb.m_mailBoxProperties.insert(std::make_pair("MountInfo", exch2k10Iter->m_exch2k10MDBmetaData.m_mountInfo));
		if( exch2k10Iter->m_type == EXCHANGE_PRIV_MDB )
		{
			mb.m_mailBoxProperties.insert(std::make_pair("MailStoreType", "PRIVATE"));
			mb.m_mailBoxProperties.insert(std::make_pair("PublicFolderDatabaseMailServerName", exch2k10Iter->m_defaultPublicFolderDBs_MailBoxServerName ));
			mb.m_mailBoxProperties.insert(std::make_pair("PublicFolderDatabaseName", exch2k10Iter->m_defaultPublicFolderDBName));
		}
		else if( exch2k10Iter->m_type == EXCHANGE_PUB_MDB )
		{
			mb.m_mailBoxProperties.insert( std::make_pair("MailStoreType", "PUBLIC"));					
		}
		std::string tempString;
        tempString = exch2k10Iter->m_edbVol ;
        sanitizeVolumePathName(tempString);
        mb.mailstoreFiles.insert( std::make_pair(exch2k10Iter->m_edbFilePath,  tempString)) ;

        tempString = exch2k10Iter->m_exch2k10MDBmetaData.m_msExchESEParamLogFileVol ;
        sanitizeVolumePathName(tempString);
        mb.mailstoreFiles.insert( std::make_pair(exch2k10Iter->m_exch2k10MDBmetaData.m_msExchESEParamLogFilePath,  tempString)) ;

		if( strcmpi(DAGName.c_str(), hostName.c_str() ))
		{
			mb.m_copyHosts = exch2k10Iter->m_exch2k10MDBmetaData.m_exchangeMDBCopysList;
		}
        mb.m_mailBoxProperties.insert(std::make_pair("ErrorCode", boost::lexical_cast<std::string>(exch2k10Iter->errCode))) ;
		mb.m_mailBoxProperties.insert(std::make_pair("ErrorString", exch2k10Iter->errString)) ;
		//If any one mailstore metadata disocevery has failed then make the entire discovery status as failed for the appplication.
		if(exch2k10Iter->errCode != DISCOVERY_SUCCESS)
		{
			exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ErrorCode", boost::lexical_cast<std::string>(DISCOVERY_FAIL)));
			exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ErrorString","Metadata discovery failed."));
		}
		exchdiscoveryInfo.m_exchange2010Data.m_mailBoxes.insert(std::make_pair(exch2k10Iter->m_mdbName, mb));
		exch2k10Iter++;
	}
    fillPolicyInfoInfo(exchdiscoveryInfo.m_policyInfo);
    discCtrlPtr->m_disCoveryInfo->exchInfo.push_back(exchdiscoveryInfo) ;	

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

SVERROR ExchangeDiscoveryAndValidator::ConvertToCxFormat(const std::list<std::string>& InstanceNameList)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
	DiscoveryControllerPtr discCtrlPtr = DiscoveryController::getInstance() ;
	//DiscoveryInfo discoveryInfo ;
	//fillDiscoveredSystemInfo(*(discCtrlPtr->m_disCoveryInfo));
    std::map<std::string, ExchangeMetaData> exchMetadataMap ;
	std::map<std::string, Exchange2k10MetaData> exch2k10MetaDataMap ;
    std::map<std::string, ExchAppVersionDiscInfo> exchDiscoveryInfoMap ;
    std::list<std::string>::const_iterator instanceNameIter = InstanceNameList.begin() ;

    while( instanceNameIter != InstanceNameList.end() )
    {
		DebugPrintf(SV_LOG_DEBUG, "instance name %s\n", instanceNameIter->c_str()) ;
        if( m_ExchangeAppPtr->m_exchDiscoveryInfoMap.find(*instanceNameIter) != m_ExchangeAppPtr->m_exchDiscoveryInfoMap.end() )
		{        
			exchDiscoveryInfoMap.insert(*(m_ExchangeAppPtr->m_exchDiscoveryInfoMap.find(*instanceNameIter))) ;
			if( m_ExchangeAppPtr->m_exchDiscoveryInfoMap.find(*instanceNameIter)->second.m_appType != INM_APP_EXCH2010 && 
				m_ExchangeAppPtr->m_exchMetaDataMap.find(*instanceNameIter) != m_ExchangeAppPtr->m_exchMetaDataMap.end() )
			{
				exchMetadataMap.insert(*(m_ExchangeAppPtr->m_exchMetaDataMap.find(*instanceNameIter))) ;
			}
            else if( m_ExchangeAppPtr->m_exchDiscoveryInfoMap.find(*instanceNameIter)->second.m_appType == INM_APP_EXCH2010 && 
					 m_ExchangeAppPtr->m_exch2k10MetaDataMap.find(*instanceNameIter) != m_ExchangeAppPtr->m_exch2k10MetaDataMap.end() )
			{
				exch2k10MetaDataMap.insert(*(m_ExchangeAppPtr->m_exch2k10MetaDataMap.find(*instanceNameIter)));	
			}
		}
        instanceNameIter++ ;
    }
    std::map<std::string, ExchangeMetaData>::const_iterator exchMetadataMapIter  = exchMetadataMap.begin() ;
    while( exchMetadataMapIter != exchMetadataMap.end() )
    {
        ExchangeDiscoveryInfo exchdiscoveryInfo ;
        const ExchangeMetaData& exchMetadata = exchMetadataMapIter->second ;
        const ExchAppVersionDiscInfo& exchDiscoveryInfo = exchDiscoveryInfoMap.find(exchMetadataMapIter->first)->second ;
		convertToExchangeDiscoveryInfo(exchDiscoveryInfo, exchMetadata, exchdiscoveryInfo) ;
        exchMetadataMapIter++ ;
    }

    std::map<std::string, Exchange2k10MetaData>::const_iterator exch2010MetadataMapIter  = exch2k10MetaDataMap.begin() ;
    while( exch2010MetadataMapIter != exch2k10MetaDataMap.end() )
    {
        ExchangeDiscoveryInfo exchdiscoveryInfo ;
        const Exchange2k10MetaData& exch2k10Metadata = exch2010MetadataMapIter->second ;
        const ExchAppVersionDiscInfo& exchDiscoveryInfo = exchDiscoveryInfoMap.find(exch2010MetadataMapIter->first)->second ;
		convertToExchange2010DiscoveryInfo(exchDiscoveryInfo, exch2k10Metadata, exchdiscoveryInfo) ;
        exch2010MetadataMapIter++ ;
    }

    bRet = SVS_OK ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ; 
}

SVERROR ExchangeDiscoveryAndValidator::Discover(bool shouldUpdateCx)
{
	SVERROR bRet = SVS_FALSE ;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
	//getHostLevelDiscoveryInfo(m_hostDiscoveryInfo) ;
	std::list<std::string> instanceNameList;
	try
	{
		if(m_policyId == 0 )
		{
			m_ExchangeAppPtr.reset(new ExchangeApplication(false));
		}
		else
		{
			if(m_policyType.compare("Discovery") == 0)
			{
				SV_ULONGLONG instanceId = AppScheduler::getInstance()->getInstanceId(m_policyId);
				configuratorPtr->PolicyUpdate(m_policyId,instanceId,"InProgress","");
			}
			getDiscoveryInstanceList(instanceNameList);
			if(instanceNameList.empty())
			{
				m_ExchangeAppPtr.reset(new ExchangeApplication(false));
			}
			else
			{
				m_ExchangeAppPtr.reset( new ExchangeApplication(instanceNameList, false) ) ;
			}
		}
		if( m_ExchangeAppPtr->isInstalled() )
		{
			if(m_ExchangeAppPtr->discoverApplication() != SVS_OK)
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to discover Exchange application. Unable to proceed Exchange disovery.") ;
				bRet = SVS_FALSE;
			}
			if( m_ExchangeAppPtr->discoverMetaData() == SVS_OK)
			{
				bRet = SVS_OK;
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to discover Exchange application Metadata. Unable to proceed Exchange disovery.") ;
				bRet = SVS_FALSE;
			}
			if( shouldUpdateCx )
			{
				instanceNameList = m_ExchangeAppPtr->getInstances() ;
				ConvertToCxFormat(instanceNameList) ;
			}
		}
	}
	catch(...)
	{
		DebugPrintf(SV_LOG_ERROR, "Unhandled Exception occurred while discovering Exchange application. Unable to proceed Exchange disovery.") ;
		if( shouldUpdateCx )
		{
			DiscoveryControllerPtr discCtrlPtr = DiscoveryController::getInstance() ;
			ExchangeDiscoveryInfo exchdiscoveryInfo;
			fillPolicyInfoInfo(exchdiscoveryInfo.m_policyInfo);
			exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ErrorCode",boost::lexical_cast<std::string>(DISCOVERY_FAIL))) ;            
			exchdiscoveryInfo.m_InstanceInformation.insert(std::make_pair("ErrorString","Unhandled Exception occurred while discovering Exchange application. Unable to proceed Exchange disovery.")) ;
			discCtrlPtr->m_disCoveryInfo->exchInfo.push_back(exchdiscoveryInfo) ;
		}
		bRet = SVS_FALSE ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet ;
}


SVERROR ExchangeDiscoveryAndValidator::PerformReadinessCheckAtSource()
{
    SVERROR retStatus = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SV_ULONGLONG instanceId = AppScheduler::getInstance()->getInstanceId(m_policyId);
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
	try
	{
		if( IsItActiveNode() )
		{
			ReadinessCheck readyCheck;
			std::list<ValidatorPtr> validations ;
			std::list<ReadinessCheck> readinessChecksList;
			RuleEnginePtr enginePtr = RuleEngine::getInstance() ;
			ReadinessCheckInfo readinessCheckInfo ;
			DiscoveryControllerPtr discCtrlPtr = DiscoveryController::getInstance() ;
			std::list<std::string> activeInstances ;

			configuratorPtr->PolicyUpdate(m_policyId,instanceId,"InProgress","Source Readiness Check is in progress");

			m_ExchangeAppPtr.reset(new ExchangeApplication( false ) ) ;
			if( m_ExchangeAppPtr->isInstalled() )
			{
				if( m_ExchangeAppPtr->discoverApplication() == SVS_OK)
				{
					if(m_ExchangeAppPtr->discoverMetaData() == SVS_OK)
					{
						if(getReadinessDiscoveryInstanceList(activeInstances) == SVS_OK)
						{
							std::list<std::string>::const_iterator instanceNameIter = activeInstances.begin() ;
							std::string systemDrive ;
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
								std::list<InmRuleIds> failedRules ;
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
							if( m_scenarioType != SCENARIO_FASTFAILBACK )
							{
								while( instanceNameIter != activeInstances.end() )
								{
									if( m_ExchangeAppPtr->m_exchDiscoveryInfoMap.find(*instanceNameIter) != m_ExchangeAppPtr->m_exchDiscoveryInfoMap.end() )
									{
										const ExchAppVersionDiscInfo &appVerInfo = m_ExchangeAppPtr->m_exchDiscoveryInfoMap[*instanceNameIter] ;
										if( appVerInfo.m_appType != INM_APP_EXCH2010 )
										{
											ExchangeMetaData& exchMetaData = m_ExchangeAppPtr->m_exchMetaDataMap[*instanceNameIter];
											validations = enginePtr->getExchangeRules(appVerInfo,exchMetaData, m_scenarioType, m_context, systemDrive, m_appVacpOptions, Host::GetInstance().GetHostName());
										}
										else if( appVerInfo.m_appType == INM_APP_EXCH2010 )
										{
											Exchange2k10MetaData& exch2k10MetaData = m_ExchangeAppPtr->m_exch2k10MetaDataMap[*instanceNameIter];
											validations = enginePtr->getExchange2010Rules(appVerInfo, exch2k10MetaData, m_scenarioType, m_context, systemDrive, m_appVacpOptions, Host::GetInstance().GetHostName());
										}
										std::list<ValidatorPtr>::iterator validateIter = validations.begin() ;
										std::list<InmRuleIds> failedRules ;
										while(validateIter != validations.end() )
										{
											(*validateIter)->setFailedRules(failedRules) ;
											retStatus = enginePtr->RunRule(*validateIter) ;
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
									instanceNameIter++ ;
								}
							}
							readinessCheckInfo.propertiesList.insert(std::make_pair("PolicyId", boost::lexical_cast<std::string>(m_policyId))) ;
							readinessCheckInfo.propertiesList.insert(std::make_pair("InstanceId",boost::lexical_cast<std::string>(instanceId)));
							readinessCheckInfo.validationsList = readinessChecksList ;
							discCtrlPtr->m_ReadinesscheckInfoList.push_back(readinessCheckInfo);
						}
						else
						{
							DebugPrintf(SV_LOG_ERROR,"Failed to get instancelist from readiness data. Can not perform source readiness check.\n");
							configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Failed","Failed to get instancelist from readiness data. Can not perform source readiness check.");
						}
					}
					else
					{
						DebugPrintf(SV_LOG_ERROR,"Exchange metadata discovery failed. Can not perform Source Readiness Check");
						configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Failed","Exchange metadata discovery failed. Can not perform Source Readiness Check");
						retStatus = SVS_FALSE;
					}
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR,"Exchange Application discovery failed. Can not perform Source Readiness Check");
					configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Failed","Exchange Application discovery failed. Can not perform Source Readiness Check");
					retStatus = SVS_FALSE;
				}
			}
			else
			{
				DebugPrintf(SV_LOG_INFO,"Exchange is not installed. Can not perform Source Readiness Check for Exchange Application.");
				retStatus = SVS_OK ;
			}
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG, "Wont process the policy %ld as this is passive node\n", m_policyId) ;
			retStatus = SVS_OK ;
		}
	}
	catch(std::exception& ex)
	{
		std::string errString = "Got exception";
		errString += ex.what();
		errString += ". Can not process Source Readiness Check\n";
		DebugPrintf(SV_LOG_ERROR, errString.c_str() ) ;
		configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Failed",errString.c_str());
		retStatus = SVS_FALSE ;
	}
	catch(...)
	{
		DebugPrintf(SV_LOG_DEBUG, "Got an unhandled exception. Unable to process Source Readiness Check.\n" );
		configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Failed","Got an unhandled exception during Source Readiness Check.");
		retStatus = SVS_FALSE ;
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return retStatus ;
}

SVERROR ExchangeDiscoveryAndValidator::PerformReadinessCheckAtTarget()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR bRet = SVS_FALSE ;
	AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
	SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
	try
	{
		if( IsItActiveNode() )
		{
			DiscoveryControllerPtr discCtrlPtr = DiscoveryController::getInstance() ;
			ExchangeTargetReadinessCheckInfo srcInfo;
			ExchangeTargetReadinessCheckInfo tgtInfo;
			std::list<ReadinessCheck> readinessChecksList;
			std::list<ReadinessCheckInfo> readinesscheckInfos; 

			configuratorPtr->PolicyUpdate(m_policyId,instanceId,"InProgress","Target Readiness Check is in progress");
			
			std::string marshalledSrcReadinessCheckString = m_policy.m_policyData ;
			AppAgentConfiguratorPtr appConfiguratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;

			DebugPrintf( SV_LOG_DEBUG, "Target Readiness Info: '%s' \n", marshalledSrcReadinessCheckString.c_str()) ;
			srcInfo = unmarshal<ExchangeTargetReadinessCheckInfo>(marshalledSrcReadinessCheckString) ;

			std::string tgtVirtualServerName ;

			if(srcInfo.m_properties.find("TgtVirtualServerName") != srcInfo.m_properties.end())
			{
				tgtVirtualServerName = srcInfo.m_properties.find("TgtVirtualServerName")->second ;
				if( tgtVirtualServerName.compare("0") == 0 )
				{
					tgtVirtualServerName = "" ;
					m_ExchangeAppPtr.reset( new ExchangeApplication(false) ) ;
				}
				else
				{
					m_ExchangeAppPtr.reset( new ExchangeApplication(tgtVirtualServerName, false) ) ;
				}
				if(m_context != CONTEXT_BACKUP)
				{
					bool bDiscovery = false;
					std::string errMessage;
					if( m_ExchangeAppPtr->isInstalled() )
					{
						if(m_ExchangeAppPtr->discoverApplication() == SVS_OK)
						{
							//Metadata is not required to process Target Readiness checks.
							/*if(m_ExchangeAppPtr->discoverMetaData() == SVS_OK)
							{
								bDiscovery = true;
							}
							else
							{
								errMessage = "Exchange Application metadata discovery failed. Can not process Target Readiness Check.\n";
								DebugPrintf(SV_LOG_ERROR, errMessage.c_str());
							}*/
							bDiscovery = true;
						}
						else
						{
							errMessage = "Exchange Application discovery failed. Can not process Target Readiness Check.\n";
							DebugPrintf(SV_LOG_ERROR, errMessage.c_str());
						}
					}
					else
					{
						errMessage = "Exchange Application is not installed. Can not process Target Readiness Check.\n";
						DebugPrintf(SV_LOG_ERROR, errMessage.c_str());
					}
					//In case of HA/DR the exchange discovery is required to process readiness check. 
					//If discory fails then agent can not process readiness check.
					if(!bDiscovery)
					{
						bRet = SVS_FALSE;
						configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Failed",errMessage.c_str());
						return bRet;
					}
				}
				ExchAppVersionDiscInfo appVerInfo ;
				if( tgtVirtualServerName.compare("") != 0 )
				{
					if( m_ExchangeAppPtr->m_exchDiscoveryInfoMap.find(tgtVirtualServerName) != m_ExchangeAppPtr->m_exchDiscoveryInfoMap.end() )
					{
						appVerInfo = m_ExchangeAppPtr->m_exchDiscoveryInfoMap.find(tgtVirtualServerName)->second;
					}
					else
					{
						DebugPrintf(SV_LOG_ERROR, "Didnt find the exchange information for the virtual server name %s\n", tgtVirtualServerName.c_str()) ;
					}
				}
				else
				{
					if( m_ExchangeAppPtr->m_exchDiscoveryInfoMap.size() )
					{
						appVerInfo = m_ExchangeAppPtr->m_exchDiscoveryInfoMap.begin()->second ;
					}
				}
				tgtInfo.m_properties.insert(std::make_pair("Dn",appVerInfo.m_dn));
				tgtInfo.m_properties.insert(std::make_pair("Version",appVerInfo.m_version));
				tgtInfo.m_properties.insert(std::make_pair("Edition",appVerInfo.m_edition));
				/*if(strcmp(m_policy.m_policyProperties.find("ApplicationType")->second.c_str(), "EXCHANGE") == 0 ||
					strcmp(m_policy.m_policyProperties.find("ApplicationType")->second.c_str(), "EXCHANGE2003") == 0)
				{
					if(appVerInfo.m_isMta && appVerInfo.m_isClustered)
						tgtInfo.m_properties.insert(std::make_pair("IsMTA","YES"));
					else
						tgtInfo.m_properties.insert(std::make_pair("IsMTA","NO"));
				}
				else
				{
					tgtInfo.m_properties.insert(std::make_pair("IsMTA","NO"));
				}*/

				std::list<ValidatorPtr> validations ;
				std::list<ValidatorPtr>::iterator validateIter;
				std::list<InmRuleIds> failedRules ;
				RuleEnginePtr enginePtr = RuleEngine::getInstance() ;
				std::string systemDrive ;
				getEnvironmentVariable("SystemDrive", systemDrive) ;
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
						bRet = enginePtr->RunRule(*validateIter) ;
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
				if( m_scenarioType != SCENARIO_FASTFAILBACK )
				{
					validations = enginePtr->getTargetRedinessRules(srcInfo, tgtInfo, m_scenarioType, m_context, m_totalNumberOfPairs) ;
					failedRules.clear() ;
					validateIter = validations.begin() ;
					while(validateIter != validations.end() )
					{
						(*validateIter)->setFailedRules(failedRules) ;
						bRet = enginePtr->RunRule(*validateIter) ;
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
				ReadinessCheckInfo readinessCheckInfo ;
		        
				instanceId = schedulerPtr->getInstanceId(m_policyId);
				readinessCheckInfo.propertiesList.insert(std::make_pair("PolicyId", boost::lexical_cast<std::string>(m_policyId))) ;
				readinessCheckInfo.propertiesList.insert(std::make_pair("InstanceId",boost::lexical_cast<std::string>(instanceId)));
				readinessCheckInfo.validationsList = readinessChecksList ;

				discCtrlPtr->m_ReadinesscheckInfoList.push_back(readinessCheckInfo) ;

				bRet = SVS_OK ;
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "TgtVirtualServerName property is missing in the readiness check data. Can not process Readiness check.\n") ;
				configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Failed","TgtVirtualServerName property is missing in the readiness check data \
																			 . Can not process Readiness check.");
				bRet = SVS_FALSE ;
			}
		}
		else
		{
			DebugPrintf(SV_LOG_INFO, "Wont process the policy %ld as this is passive node\n", m_policyId) ;
			bRet = SVS_OK ;
		}
	}
	catch(std::exception& ex)
    {
		std::string errString = "Got exception";
		errString += ex.what();
		errString += ". Can not process Target Readiness Check\n";
		DebugPrintf(SV_LOG_ERROR, errString.c_str() ) ;
		configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Failed",errString.c_str());
	    bRet = SVS_FALSE ;
    }
    catch(...)
    {
        DebugPrintf(SV_LOG_DEBUG, "Got an unhandled exception. Unable to process Target Readiness Check.\n" );
		configuratorPtr->PolicyUpdate(m_policyId,instanceId,"Failed","Got an unhandled exception during Target Readiness Check.");
	    bRet = SVS_FALSE ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ExchangeDiscoveryAndValidator::IsItActiveNode()
{
    bool process = false ;
    if( isClusterNode() )
    {
    	ExchangeApplicationPtr m_exchApp;
        m_exchApp.reset( new ExchangeApplication(false) ) ;
        if( m_exchApp->isInstalled() )
        {
            std::string instanceName ;
            m_exchApp->discoverApplication() ;
            std::list<std::string> activeInstances = m_exchApp->getActiveInstances() ;
            std::map<std::string, std::string> propsMap = m_policy.m_policyProperties ;
            if( propsMap.find("VirtualServerName") != propsMap.end() )
            {
                instanceName = propsMap.find("VirtualServerName")->second ;
            }
            else if( propsMap.find("InstanceName") != propsMap.end() )
            {
                instanceName = propsMap.find("InstanceName")->second ;
            }
            if( isfound(activeInstances, instanceName) )
            {
                process = true ;
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG, "virtual server %s is not active on the current  node\n", instanceName.c_str()) ;
            }
        }
    }
    else
    {
        process = true ;
    }
	return process;
}




