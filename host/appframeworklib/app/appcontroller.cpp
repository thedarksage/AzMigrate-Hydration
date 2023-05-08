#include "appglobals.h"
#include "host.h"
#include <list>
#include <boost/lexical_cast.hpp>
#include <time.h>
#ifdef SV_WINDOWS
#include "app/win32/mssql/mssqlapplication.h"
#endif
#include "appcontroller.h"
#include "controller.h"
#include "appexception.h"
#include "service.h"
#include "util/common/util.h"
#include "config/appconfigvalueobj.h"
#include "appcommand.h"
#include "Consistency/tagdiscovery.h"
#ifdef SV_WINDOWS
#include <shlwapi.h>
#include <winerror.h>
#include "ClusterOperation.h"
#include "system.h"
#include "ADInterface.h"
#endif

FailoverJobs AppController::m_FailoverJobs;
ACE_Recursive_Thread_Mutex AppController::m_lock; 

void AppController::Init()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    m_ProtectionSettingsChanged = false ;
    bIsClusterNodeActive = false;
    AppAgentConfiguratorPtr appConfiguratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    try
    {
        if(appConfiguratorPtr->UnserializeFailoverJobs(m_FailoverJobs) != SVS_OK )
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to unserialize the failover jobs Info\n") ;
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "Previously saved failover jobs information loaded to memory\n ") ;
        }
    }
    catch(std::exception &ex)
    {
        DebugPrintf(SV_LOG_INFO,"Exception caught while unserializing failoverjobs info.Ignoring it safely.%s\n",ex.what());
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

ACE_SHARED_MQ& AppController::Queue()
{
    return m_MsgQueue ;
}

void AppController::PostMsg(int priority, SV_ULONG policyId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ACE_Message_Block *mb = NULL ;	


    mb = new ACE_Message_Block(0, (int) policyId);

    if( mb == NULL )
    {
        throw AppException("Failed to create message block\n") ;
    }
    mb->msg_priority(priority) ;
    m_MsgQueue.enqueue_prio(mb) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void AppController::ProcessRequests()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    while( !Controller::getInstance()->QuitRequested(1) )
    {
        ACE_Message_Block *mb;
        ACE_Time_Value waitTime = ACE_OS::gettimeofday ();
        int waitSeconds = 5; //Wait Max 5 seconds while dequeuing message block.
        waitTime.sec (waitTime.sec () + waitSeconds) ; 
        if (-1 == Queue().dequeue_head(mb, &waitTime)) 
        {
            if (errno == EWOULDBLOCK || errno == ESHUTDOWN) 
            {
                continue;
            }
            break;
        }
        m_policyId = mb->msg_type() ;
        mb->release() ;
        DebugPrintf(SV_LOG_DEBUG,"AppController got policyId %ld\n",m_policyId);
        AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
        AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
        if(!configuratorPtr->getApplicationPolicies(m_policyId, "",m_policy))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to get Applicaion policy for policy Id %ld\n",m_policyId );
            schedulerPtr->UpdateTaskStatus(m_policyId, TASK_STATE_FAILED) ;
        }
        else
        {
			//bool bCaughtException = false;
            try
            {
#ifdef SV_WINDOWS
                if(isClusterNode())
                {
                    if(!IsItActiveNode())
                    {
                        DebugPrintf(SV_LOG_DEBUG,"Current node is not Active node\n");
                        configuratorPtr->RemovePolicyFromCache(m_policyId);
                        schedulerPtr->UpdateTaskStatus(m_policyId, TASK_STATE_COMPLETED) ;
                        bIsClusterNodeActive = false;
                        continue;
                    }
                    else
                    {
                        bIsClusterNodeActive = true;
                        DebugPrintf(SV_LOG_DEBUG,"Current node is Active node.Running policy \n");   
                    }
                }
#endif            
                std::string policyType ;
                if( configuratorPtr->getApplicationPolicyType(m_policyId, policyType) == SVS_OK )
                {
                    if( policyType.compare("ProductionServerPlannedFailover") == 0 ||
                        policyType.compare("DRServerPlannedFailover") == 0 ||
                        policyType.compare("DRServerPlannedFailoverRollbackVolumes") == 0 ||
                        policyType.compare("DRServerPlannedFailoverSetupApplicationEnvironment") == 0 ||
                        policyType.compare("DRServerPlannedFailoverStartApplication") == 0 ||
                        policyType.compare("DRServerUnPlannedFailover") == 0  ||
                        policyType.compare("DRServerUnPlannedFailoverRollbackVolumes") == 0 ||
                        policyType.compare("DRServerUnPlannedFailoverSetupApplicationEnvironment") == 0 ||
                        policyType.compare("DRServerUnPlannedFailoverStartApplication") == 0 ||
                        policyType.compare("ProductionServerPlannedFailback") == 0 ||
                        policyType.compare("ProductionServerPlannedFailbackRollbackVolumes") == 0 ||
                        policyType.compare("ProductionServerPlannedFailbackSetupApplicationEnvironment") == 0 ||
                        policyType.compare("ProductionServerPlannedFailbackStartApplication") == 0 ||
                        policyType.compare("DRServerPlannedFailback") == 0 ||
                        policyType.compare("ProductionServerFastFailback") == 0 ||
                        policyType.compare("ProductionServerFastFailbackSetupApplicationEnvironment") == 0 ||
                        policyType.compare("ProductionServerFastFailbackStartApplication") == 0 ||
                        policyType.compare("DRServerFastFailback") == 0)
                    {
                        if(schedulerPtr->isPolicyEnabled(m_policyId))
                        {
                            SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);
                            configuratorPtr->PolicyUpdate(m_policyId,instanceId,"InProgress","");
                        }
                        bool isSettingRequied = isFailoverInformationRequired(m_policyId);
                        if(isSettingRequied)
                        {
                            GetFailoverInformation();
                            UpdateConfigFiles();
                            CreateAppConfFile();
                            UpdateFailoverJobsToCx();
                        }
                        RunFailoverJob();
                        schedulerPtr->UpdateTaskStatus(m_policyId, TASK_STATE_COMPLETED) ;

                    }
                    else
                    {
                        ProcessMsg(m_policyId) ;
                    }
                }
            }
            catch(FileNotFoundException& ex)
            {
				//bCaughtException = true;
                schedulerPtr->UpdateTaskStatus(m_policyId, TASK_STATE_FAILED) ;
                DebugPrintf(SV_LOG_ERROR,"Caught Exception: %s\n", ex.what());
            }
            catch(SettingsNotFound& ex)
            {
				//bCaughtException = true;
                schedulerPtr->UpdateTaskStatus(m_policyId, TASK_STATE_FAILED) ;
                DebugPrintf(SV_LOG_ERROR,"Caught Exception: %s\n", ex.what());
            }
            catch(UnknownKeyException& ex)
            {
				//bCaughtException = true;
                schedulerPtr->UpdateTaskStatus(m_policyId, TASK_STATE_FAILED) ;
                DebugPrintf(SV_LOG_ERROR,"Caught Exception: %s\n", ex.what());
            }
            catch(AppException& exception)
            {
				//bCaughtException = true;
                schedulerPtr->UpdateTaskStatus(m_policyId, TASK_STATE_FAILED) ;
                DebugPrintf(SV_LOG_ERROR, "Application Controller failed ..caught exception %s\n", exception.to_string().c_str()) ;
            }
            catch(const char*  ex)
            {
				//bCaughtException = true;
                schedulerPtr->UpdateTaskStatus(m_policyId, TASK_STATE_FAILED) ;
                DebugPrintf(SV_LOG_ERROR, "Application Controller failed ..caught exception %s\n", ex) ;
            }
            catch(std::exception& ex)
            {
				//bCaughtException = true;
                schedulerPtr->UpdateTaskStatus(m_policyId, TASK_STATE_FAILED) ;
                DebugPrintf(SV_LOG_ERROR, "AppController::ProcessRequests caught exception %s\n", ex.what()) ;
            }    
			//if(bCaughtException)
			//{
			//	SV_ULONGLONG instanceID = schedulerPtr->getInstanceId(m_policyId);
			//	configuratorPtr->PolicyUpdate(m_policyId,instanceID,"Failed","Caught Exception while processing the policy.");
			//}
        }
        Controller::getInstance()->setProcessRequests(true) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void AppController::GetFailoverInformation()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AppLocalConfigurator appLocalconfigurator ;

    std::map<std::string, std::string>& propsMap = m_policy.m_policyProperties ;
    GetValueFromMap(propsMap,"ScenarioId",m_FailoverInfo.m_ScenarioId,false);
    GetValueFromMap(propsMap,"ApplicationType",m_FailoverInfo.m_AppType,false);
    GetValueFromMap(propsMap,"SolutionType",m_FailoverInfo.m_SolutionType,false);
    GetValueFromMap(propsMap,"InstanceName",m_FailoverInfo.m_InstanceName) ;
    GetValueFromMap(propsMap,"PolicyType",m_FailoverInfo.m_PolicyType,false);
    if(m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailover") == 0 ||
          m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailover") == 0 ||
          m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailoverRollbackVolumes") == 0 ||
          m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailoverSetupApplicationEnvironment") == 0 ||
          m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailoverStartApplication") == 0 ||
          m_FailoverInfo.m_PolicyType.compare("DRServerUnPlannedFailover") == 0 ||
          m_FailoverInfo.m_PolicyType.compare("DRServerUnPlannedFailoverRollbackVolumes") == 0 ||
          m_FailoverInfo.m_PolicyType.compare("DRServerUnPlannedFailoverSetupApplicationEnvironment") == 0 ||
          m_FailoverInfo.m_PolicyType.compare("DRServerUnPlannedFailoverStartApplication") == 0)
    {
        m_FailoverInfo.m_FailoverType = "Failover";
    }
    else if(m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailback") == 0 ||
          m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailbackRollbackVolumes") == 0 ||
          m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailbackSetupApplicationEnvironment") == 0 ||
          m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailbackStartApplication") == 0 ||
          m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailback") == 0  ||
          m_FailoverInfo.m_PolicyType.compare("ProductionServerFastFailback") == 0 ||
          m_FailoverInfo.m_PolicyType.compare("ProductionServerFastFailbackSetupApplicationEnvironment") == 0 ||
          m_FailoverInfo.m_PolicyType.compare("ProductionServerFastFailbackStartApplication") == 0 ||
		  m_FailoverInfo.m_PolicyType.compare("DRServerFastFailback") == 0)
    {
        m_FailoverInfo.m_FailoverType = "Failback";
    }
    GetValueFromMap(propsMap,"RecoveryExecutor",m_FailoverInfo.m_RecoveryExecutor) ;
    GetValueFromMap(propsMap,"PreScript",m_FailoverInfo.m_Prescript) ;
    GetValueFromMap(propsMap,"PostScript",m_FailoverInfo.m_Postscript) ;
    GetValueFromMap(propsMap,"CustomScript",m_FailoverInfo.m_Customscript) ;
    GetValueFromMap(propsMap,"DNSFailover",m_FailoverInfo.m_DNSFailover) ;
    if(m_FailoverInfo.m_DNSFailover.empty() == true)
    {
        m_FailoverInfo.m_DNSFailover = "0";
    }
    GetValueFromMap(propsMap,"ADFailover",m_FailoverInfo.m_ADFailover) ;
    if(m_FailoverInfo.m_ADFailover.empty() == true)
    {
        m_FailoverInfo.m_ADFailover = "0";
    }
    GetValueFromMap(propsMap,"ConvertToCluster",m_FailoverInfo.m_ConvertToCluster);
    if(m_FailoverInfo.m_ConvertToCluster.empty() == true)
    {
        m_FailoverInfo.m_ConvertToCluster = "0";
    }
    GetValueFromMap(propsMap,"startApplicationService",m_FailoverInfo.m_startApplicationService);
    if(m_FailoverInfo.m_startApplicationService.empty() == true)
    {
        m_FailoverInfo.m_startApplicationService = "0";
    }
    GetValueFromMap(propsMap,"RepointAllPrivateStores",m_FailoverInfo.m_RepointAllStores);
    if(m_FailoverInfo.m_RepointAllStores.empty() == true)
    {
        m_FailoverInfo.m_RepointAllStores = "0";
    }
    GetValueFromMap(propsMap,"ExtraOptions",m_FailoverInfo.m_ExtraOptions);
    GetValueFromMap(propsMap,"ConsistencyCmd",m_FailoverInfo.m_VacpCmdLine) ;

    if(m_FailoverInfo.m_PolicyType.compare("ProductionServerFastFailback") == 0 || 
        // m_FailoverInfo.m_PolicyType.compare("ProductionServerFastFailbackSetupApplicationEnvironment") == 0 ||
        m_FailoverInfo.m_PolicyType.compare("ProductionServerFastFailbackStartApplication") == 0 ||
        //m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailoverSetupApplicationEnvironment") == 0 ||
        m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailoverStartApplication") == 0 ||
        //m_FailoverInfo.m_PolicyType.compare("DRServerUnPlannedFailoverSetupApplicationEnvironment") == 0 ||
        m_FailoverInfo.m_PolicyType.compare("DRServerUnPlannedFailoverStartApplication") == 0 ||
        //m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailbackSetupApplicationEnvironment") == 0 ||
        m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailbackStartApplication") == 0 ||
        m_FailoverInfo.m_PolicyType.compare("DRServerFastFailback") == 0)
    {
        GetValueFromMap(propsMap,"ProductionServerName", m_FailoverInfo.m_ProductionServerName) ;
        GetValueFromMap(propsMap,"ProductionVirtualServerName",m_FailoverInfo.m_ProductionVirtualServerName) ;
        GetValueFromMap(propsMap,"ProductionServerIP",m_FailoverInfo.m_ProductionServerIp);
        GetValueFromMap(propsMap,"ProductionVirtualServerIP",m_FailoverInfo.m_ProductionVirtualServerIp);
        GetValueFromMap(propsMap,"DRServerName", m_FailoverInfo.m_DRServerName);
        GetValueFromMap(propsMap,"DRServerIP",m_FailoverInfo.m_DRServerIp);
        GetValueFromMap(propsMap,"DRVirtualServerName", m_FailoverInfo.m_DRVirtualServerName) ;
        GetValueFromMap(propsMap,"DRServerVirtualServerIP",m_FailoverInfo.m_DRVirtualServerIp);

    }
    else
    {
        if(GetProtectionSettings() == SVS_FALSE)
        {
            throw AppException("Protection settings not found in application policies");
        }
        std::map<std::string,std::string>& protectionSettingsPropertyMap = m_AppProtectionSettings.m_properties; 
        GetValueFromMap(protectionSettingsPropertyMap,"ProductionServerName", m_FailoverInfo.m_ProductionServerName,false) ;
        GetValueFromMap(protectionSettingsPropertyMap,"DRServerName", m_FailoverInfo.m_DRServerName,false);
        GetValueFromMap(protectionSettingsPropertyMap,"ProductionVirtualServerName",m_FailoverInfo.m_ProductionVirtualServerName) ;
        GetValueFromMap(protectionSettingsPropertyMap,"DRVirtualServerName", m_FailoverInfo.m_DRVirtualServerName) ;
        GetValueFromMap(protectionSettingsPropertyMap,"ProductionServerIP",m_FailoverInfo.m_ProductionServerIp);
        GetValueFromMap(protectionSettingsPropertyMap,"DRServerIP",m_FailoverInfo.m_DRServerIp);
        GetValueFromMap(protectionSettingsPropertyMap,"ProductionVirtualServerIP",m_FailoverInfo.m_ProductionVirtualServerIp);
        GetValueFromMap(protectionSettingsPropertyMap,"DRServerVirtualServerIP",m_FailoverInfo.m_DRVirtualServerIp);
        GetValueFromMap(protectionSettingsPropertyMap,"Direction",m_FailoverInfo.m_ProtecionDirection);
#ifdef SV_WINDOWS
		if( m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailover") == 0 ||
			m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailback") == 0 )
        {
            m_FailoverInfo.m_ProductionServerName = Host::GetInstance().GetHostName();
            DebugPrintf(SV_LOG_DEBUG, "ProductionServer name  %s\n", m_FailoverInfo.m_ProductionServerName.c_str()) ;
        }
		if(	m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailback") == 0 ||
			m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailover") == 0 ||
			m_FailoverInfo.m_PolicyType.compare("DRServerUnPlannedFailover") == 0 )
		{
			m_FailoverInfo.m_DRServerName = Host::GetInstance().GetHostName();
            DebugPrintf(SV_LOG_DEBUG, "DRServer name  %s\n", m_FailoverInfo.m_DRServerName.c_str()) ;
		}
#endif
    }
    if(GetFailoverData()== SVS_FALSE)
    {
        throw AppException("Failed to get target policy data");
    }

    GetValueFromMap(m_FailoverData.m_properties,"isOracleRac",m_FailoverInfo.m_IsOracleRac) ;
    GetValueFromMap(m_FailoverData.m_properties,"TagName",m_FailoverInfo.m_TagName) ;
    GetValueFromMap(m_FailoverData.m_properties,"TagType",m_FailoverInfo.m_TagType) ;

    if( m_FailoverInfo.m_VacpCmdLine.empty() == false )
    {
        size_t pos = std::string::npos;
        if((pos =m_FailoverInfo.m_VacpCmdLine.find(" -a ") ) != std::string::npos ||
            (pos =m_FailoverInfo.m_VacpCmdLine.find(" -w ") ) != std::string::npos)
        {
            size_t pos1 = m_FailoverInfo.m_VacpCmdLine.find_first_of(" ",pos+4);
            if(pos1 != std::string::npos)
            {
                m_FailoverInfo.m_VacpCmdLine.erase(pos,pos1-pos);
            }
            else if (pos1 == std::string::npos)
            {
                m_FailoverInfo.m_VacpCmdLine.erase(pos,m_FailoverInfo.m_VacpCmdLine.size()-pos);
            }
        }
        
        std::list<std::string>::iterator volumeIter = m_AppProtectionSettings.appVolumes.begin();
        std::string volumesStr;
        while(volumeIter != m_AppProtectionSettings.appVolumes.end())
        {
            sanitizeVolumePathName(*volumeIter);
#ifdef SV_WINDOWS
            if((*volumeIter).size() == 1)
            {
                volumesStr += *volumeIter + ":;";
            }
            else
            {
                volumesStr += *volumeIter + ";";
            }
#else
            volumesStr += *volumeIter + ",";
#endif
            volumeIter++;
        }
        if(volumesStr.empty() == false)
        {
#ifndef SV_WINDOWS
            pos = m_FailoverInfo.m_VacpCmdLine.find("-v");
            if(pos != std::string::npos)
            {
                size_t pos1 = m_FailoverInfo.m_VacpCmdLine.find_first_of(" ",pos+3);
                if(pos1 != std::string::npos)
                {
                    m_FailoverInfo.m_VacpCmdLine.erase(pos,pos1-pos);
                }
            }
#endif
#ifdef SV_WINDOWS
			volumesStr = "\"" + volumesStr + "\"";
#endif
            m_FailoverInfo.m_VacpCmdLine += " -v " + volumesStr;
        }

        pos = m_FailoverInfo.m_VacpCmdLine.find(" -t ");
        if(pos != std::string::npos)
        {
            size_t pos1 = m_FailoverInfo.m_VacpCmdLine.find_first_of(" ",pos+4);
            if(pos1 != std::string::npos)
            {
                m_FailoverInfo.m_VacpCmdLine.erase(pos,pos1-pos);
            }
        }
        m_FailoverInfo.m_VacpCmdLine += " -t " + m_FailoverInfo.m_TagName;
        pos = m_FailoverInfo.m_VacpCmdLine.find(" -s ");
        if(pos != std::string::npos)
        {
            m_FailoverInfo.m_VacpCmdLine.erase(pos,3); //removing -s   
        }
        DebugPrintf(SV_LOG_DEBUG,"Vacp Command Line for Policy %ld is %s\n",m_policyId,m_FailoverInfo.m_VacpCmdLine.c_str());
    }
    GetValueFromMap(m_FailoverData.m_properties,"RecoveryPointType",m_FailoverInfo.m_RecoveryPointType);
    m_FailoverInfo.m_AppConfFilePath = appLocalconfigurator.getCacheDirectory();
    m_FailoverInfo.m_AppConfFilePath += ACE_DIRECTORY_SEPARATOR_CHAR ;
    std::string srcHostName;
    getSourceHostNameForFileCreation(srcHostName);
    m_FailoverInfo.m_AppConfFilePath +=  srcHostName;
    m_FailoverInfo.m_AppConfFilePath +=  "_" + m_FailoverInfo.m_AppType + ".conf";
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
SVERROR AppController::GetProtectionSettings(const std::string& scenarioId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR retValue = SVS_FALSE;
    std::string scenarioIdStr;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
    configuratorPtr->getAppProtectionPolicies(m_AppProtectionsettingsMap);
    if(!scenarioId.empty())
    {
        scenarioIdStr = scenarioId;
    }
    else
    {
        scenarioIdStr = m_FailoverInfo.m_ScenarioId;
    }
    if(m_AppProtectionsettingsMap.find(scenarioIdStr) != m_AppProtectionsettingsMap.end())
    {
        m_AppProtectionSettings = m_AppProtectionsettingsMap.find(scenarioIdStr)->second;
        retValue = SVS_OK;
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG,"ProtectionSettingsId %s not found \n",scenarioIdStr.c_str());
        DebugPrintf(SV_LOG_DEBUG,"Failed to get Application settings for the policyId %ld\n",m_policyId);
        if(scenarioId.empty())
        {
            throw SettingsNotFound("Failed  to fetch Application protection setttings");
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return retValue;
}

SVERROR AppController::GetPairsOfState(std::string scenarioId,PAIR_STATE pairState,std::map<std::string, std::string> & pairs)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retStatus = SVS_OK;

	std::string strPairState;
	switch(pairState)
	{
	case PAIR_STATE_RESYNC_I  : strPairState = "Resync1"; break;
	case PAIR_STATE_RESYNC_II : strPairState = "Resync2"; break;
	case PAIR_STATE_DIFF_SYNC : strPairState = "Diff";    break;
	default:
		DebugPrintf(SV_LOG_ERROR,"Invalid pair state specified\n");
		return SVS_FALSE;
	}

	
    if(GetProtectionSettings(scenarioId) == SVS_OK)
    {
		pairs.clear();

        std::map<std::string, ReplicationPair>::iterator pairsIter = m_AppProtectionSettings.Pairs.begin();
        while(pairsIter != m_AppProtectionSettings.Pairs.end())
        {
            std::map<std::string, std::string>& replicationPairInfoMap = pairsIter->second.m_properties;
            std::map<std::string, std::string>::iterator replicationPairInfoMapIter = replicationPairInfoMap.find("PairState");
            if(replicationPairInfoMapIter != replicationPairInfoMap.end())
            {
                if(replicationPairInfoMapIter->second.compare(strPairState) == 0)
                {
                    if(replicationPairInfoMap.find("RemoteMountPoint") != replicationPairInfoMap.end())
                    {
                        pairs.insert(std::make_pair(pairsIter->first,replicationPairInfoMap.find("RemoteMountPoint")->second));
                    }
                }
            }
            pairsIter++;
        }
    }
    else
    {
		DebugPrintf(SV_LOG_ERROR,"Protection Settings for the scenario-id %s not found\n",scenarioId.c_str());
		retStatus = SVS_FALSE;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return retStatus;;
}

SVERROR AppController::GetFailoverData()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE;
    std::string marshalledTgtFailoverData = m_policy.m_policyData ;
    try
    {
        DebugPrintf(SV_LOG_DEBUG,"Failover Data : %s \n",marshalledTgtFailoverData.c_str());
        m_FailoverData = unmarshal<FailoverPolicyData>(marshalledTgtFailoverData) ;
        std::list<std::string>::iterator iter = m_FailoverData.m_InstancesList.begin();
        while(iter != m_FailoverData.m_InstancesList.end())
        {
            DebugPrintf(SV_LOG_DEBUG,"Got %s instance from cx \n",iter->c_str());
            iter++;
        }
        bRet = SVS_OK;
    }   
    catch ( ContextualException& ce )
    {
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s.\n",
            FUNCTION_NAME, ce.what());
        DebugPrintf(SV_LOG_ERROR,"Got unmarshal exception for %s\n",marshalledTgtFailoverData.c_str());
    }
    catch(std::exception ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to unmarshal FailoverPolicyData %s\n for the policy %ld", ex.what(),m_policyId) ;
        DebugPrintf(SV_LOG_ERROR,"Got unmarshal exception for %s\n",marshalledTgtFailoverData.c_str());
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet;
}


void AppController::BuildDNSCommand(std::string& dnsCmd)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::stringstream cmdStream;
    AppLocalConfigurator localconfigurator;

    cmdStream <<"\"";
    cmdStream << localconfigurator.getInstallPath();
    cmdStream << "\\";
    cmdStream << "dns.exe";
    cmdStream <<"\"";
    cmdStream <<" -failback ";
    if((m_FailoverInfo.m_ProductionVirtualServerName.empty() == false && 
        m_FailoverInfo.m_DRVirtualServerName.empty() == false) ||
        (m_FailoverInfo.m_ProductionVirtualServerName.empty() == false && 
        m_FailoverInfo.m_DRVirtualServerName.empty() == true) )

    {
        cmdStream <<" -host ";
        cmdStream <<m_FailoverInfo.m_ProductionVirtualServerName;
        cmdStream <<" -ip ";
        cmdStream <<m_FailoverInfo.m_ProductionVirtualServerIp;
    }
    else if(m_FailoverInfo.m_ProductionVirtualServerName.empty() == true && 
        m_FailoverInfo.m_DRVirtualServerName.empty() == true)
    {
        cmdStream <<" -host ";
        cmdStream <<m_FailoverInfo.m_ProductionServerName;
        cmdStream <<" -ip ";
        cmdStream <<m_FailoverInfo.m_ProductionServerIp;
    }
    dnsCmd = cmdStream.str();
    DebugPrintf(SV_LOG_DEBUG,"DNS failback command is %s\n",dnsCmd.c_str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void AppController::UpdateFailoverJobsToCx()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AppAgentConfiguratorPtr appConfiguratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    PrePareFailoverJobs();
    appConfiguratorPtr->SerializeFailoverJobs(m_FailoverJobs);
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);
    if( !schedulerPtr->isPolicyEnabled(m_policyId) )
    {
        FailoverJob failoverJob = m_FailoverJobs.m_FailoverJobsMap.find(m_policyId)->second;    
        DebugPrintf(SV_LOG_DEBUG,"Updating FailoverJob information for poliyId %ld to Cx\n",m_policyId);
        appConfiguratorPtr->UpdateFailoverJobs(failoverJob,m_policyId,instanceId);
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void AppController::SetCustomVolumeInfoToConfigValueObj()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AppConfigValueObjectPtr configObj = AppConfigValueObject::getInstance() ;
    configObj->setVacpCommand(m_FailoverInfo.m_VacpCmdLine);

    std::list<std::string>::iterator customVolumeIter = m_AppProtectionSettings.CustomVolume.begin();
    while( customVolumeIter != m_AppProtectionSettings.CustomVolume.end())
    {
        configObj->setCustomVolume(*customVolumeIter);
        std::map<std::string, ReplicationPair>::iterator pairIter = m_AppProtectionSettings.Pairs.find(*customVolumeIter);
        if(pairIter != m_AppProtectionSettings.Pairs.end())
        {
            std::string remoteVolumeStr;
            GetValueFromMap(pairIter->second.m_properties,"RemoteMountPoint",remoteVolumeStr);
            configObj->setCustomProtectedPair(pairIter->first,remoteVolumeStr);
        }
        customVolumeIter++;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void AppController::SetAppVolumeInfoToConfigValueObj()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AppConfigValueObjectPtr configObj = AppConfigValueObject::getInstance() ;
    std::list<std::string>::iterator appVolumeIter = m_AppProtectionSettings.appVolumes.begin();
    while( appVolumeIter != m_AppProtectionSettings.appVolumes.end())
    {
        configObj->setApplicationVolume(*appVolumeIter);
        appVolumeIter++;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
void AppController::SetRecoveryPointInfoToConfigValueObj()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;   
    AppConfigValueObjectPtr configObj = AppConfigValueObject::getInstance() ;
    if(m_FailoverInfo.m_RecoveryPointType.compare("CUSTOM") == 0 )
    {
        RecoveryOptions recoveryOptions;
        RecoveryPoints recoveryPoints;

        std::map<std::string, RecoveryPoints>::iterator RecoverypointMapIter = m_FailoverData.m_recoveryPoints.begin();
        while(RecoverypointMapIter != m_FailoverData.m_recoveryPoints.end())
        {
            recoveryPoints = RecoverypointMapIter->second;
            recoveryOptions.m_VolumeName = RecoverypointMapIter->first;
            GetValueFromMap(recoveryPoints.m_properties,"TagName",recoveryOptions.m_TagName);
            GetValueFromMap(recoveryPoints.m_properties,"RecoveryPointType",recoveryOptions.m_RecoveryPointType);
            if(!recoveryOptions.m_TagName.empty())
            {   
                GetValueFromMap(recoveryPoints.m_properties,"TagType",recoveryOptions.m_TagType);
            }
            GetValueFromMap(recoveryPoints.m_properties,"Timestamp",recoveryOptions.m_TimeStamp); 
            configObj->setRecoveryOptions(recoveryOptions);
            RecoverypointMapIter++;
        }
    }
    else if(m_FailoverInfo.m_RecoveryPointType.compare("TAG") == 0)
    {
        GetValueFromMap(m_FailoverData.m_properties,"TagName",m_FailoverInfo.m_TagName) ;
        configObj->setTagName(m_FailoverInfo.m_TagName);
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;

}
void AppController::SetReplPairStatusInfoToConfigValueObj()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	AppConfigValueObjectPtr configObj = AppConfigValueObject::getInstance();
	std::map<std::string, ReplicationPair>::iterator iterPairs = m_AppProtectionSettings.Pairs.begin();
	while(iterPairs != m_AppProtectionSettings.Pairs.end())
	{
		std::string volume, status;
		GetValueFromMap(iterPairs->second.m_properties,"RemoteMountPoint",volume);
		GetValueFromMap(iterPairs->second.m_properties,"PairState",status);

		if(!volume.empty() && !status.empty())
			configObj->setPairStatus(volume,status);
		
		iterPairs++;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
void AppController::CreateAppConfFile()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    if(m_FailoverInfo.m_AppType.compare("ORACLE") == 0 || m_FailoverInfo.m_AppType.compare("BULK") == 0 || m_FailoverInfo.m_AppType.compare("DB2") == 0)
    {
        return;
    }

    AppConfigValueObject::createInstance() ;
    AppConfigValueObjectPtr configObj = AppConfigValueObject::getInstance() ;
    configObj->ClearLists();
    if(strstr(m_FailoverInfo.m_AppType.c_str(),"SQL"))
    {
#ifdef SV_WINDOWS
        SetSQLInstanceInfoToConfigValueObj();
#endif
    }
    SetCustomVolumeInfoToConfigValueObj();
    SetAppVolumeInfoToConfigValueObj();
    SetRecoveryPointInfoToConfigValueObj();
	SetReplPairStatusInfoToConfigValueObj();
    CreateSectionAndKeysOfConfFile(m_FailoverInfo.m_AppConfFilePath);
    configObj->UpdateConfFile(m_FailoverInfo.m_AppConfFilePath);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
#ifdef SV_WINDOWS
void AppController::SetSQLInstanceInfoToConfigValueObj()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AppConfigValueObjectPtr configObj = AppConfigValueObject::getInstance() ;
    std::string PrefixName;
    getSourceHostNameForFileCreation(PrefixName);
    if(m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailover") == 0 || 
        m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailback") == 0 )
    {
        std::list<std::string> instanceNameList;
        instanceNameList = m_FailoverData.m_InstancesList;
        std::list<std::string>::iterator listIter = instanceNameList.begin();
        while( listIter != instanceNameList.end())
        {
            std::string SQLInstanceStr;
            if((*listIter).compare("MSSQLSERVER") == 0)
            {
                SQLInstanceStr = PrefixName;
            }
            else if((*listIter).compare(PrefixName) == 0)
            {
                SQLInstanceStr = *listIter;
            }
            else
            {
                SQLInstanceStr = PrefixName + "\\" + *listIter ;
            }
            configObj->setSQLInstanceNameList(SQLInstanceStr);
            listIter++;
        }
    }
    else
    {
        std::map<std::string,SourceSQLInstanceInfo>::iterator discIter = m_SrcDiscInfo.m_srcsqlInstanceInfo.begin();
        while( discIter != m_SrcDiscInfo.m_srcsqlInstanceInfo.end())
        {
            std::string SQLInstanceStr;
            if(discIter->first.compare("MSSQLSERVER") == 0)
            {
                SQLInstanceStr = PrefixName;
            }
            else if(PrefixName.compare(discIter->first) == 0)
            {
                SQLInstanceStr = discIter->first ;
            }
            else
            {
                SQLInstanceStr = PrefixName + "\\" + discIter->first;
            }
            configObj->setSQLInstanceNameList(SQLInstanceStr);
            discIter++;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
#endif

void  AppController::CreateSectionAndKeysOfConfFile(std::string const & confFile)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::ofstream out;
    out.open(confFile.c_str(),std::ios::trunc);
    if (!out.is_open()) 
    {
        std::stringstream stream;
        DebugPrintf(SV_LOG_ERROR,"Failed to open %s\n",confFile.c_str());
        stream<<"Failed to open conf file :"<<confFile;
        throw FileNotFoundException(confFile);
    }
    out<<"[vacpoptions]"<<std::endl;
    out<<"performfullbackup="<<std::endl;
    out<<"serverip="<<std::endl;
    out<<"tagname="<<std::endl;
    out<<"writerinstance="<<std::endl;
    out<<"serverdevice="<<std::endl;
    out<<"vacpcommand="<<std::endl;
    out<<"volumeguid="<<std::endl;
    out<<"providerguid="<<std::endl;
    out<<"timeout="<<std::endl;
    out<<"serverport="<<std::endl;
    out<<"context="<<std::endl;
    out<<"crashconsistency="<<std::endl;
    out<<"application="<<std::endl;
    out<<"applicationvolumes="<<std::endl;
    out<<"customvolumes="<<std::endl;
    out<<"synchronoustag="<<std::endl;
    out<<"remotetag="<<std::endl;
    out<<"skipdrivercheck="<<std::endl;
    out<<"[sqlinstancenames]"<<std::endl;
    out<<"instances="<<std::endl;
    out<<"[customvolumeinformation]"<<std::endl;
    out<<"pairs="<<std::endl;
    out<<"[recoverypoint]"<<std::endl;
    out<<"latestcommontime="<<std::endl;
    AppConfigValueObjectPtr configObj = AppConfigValueObject::getInstance() ;
    std::map<std::string,RecoveryOptions> recoveryOptionsMap = configObj->getRecoveryOptions(); 
    for(unsigned int i = 0; i < recoveryOptionsMap.size();i++)
    {
        std::string str = boost::lexical_cast<std::string>(i+1);
        out<<"[" << "recoveryoptions.vol" << str <<"]"<<std::endl;
        out<<"volumename = "<<std::endl;
        out<<"tagname = "<<std::endl;
        out<<"tagtype = "<<std::endl;
        out<<"recoverypointtype = "<<std::endl;
        out<<"timestamp = "<<std::endl;

    }
	out<<"[replicationpairstatus]"<<std::endl;
    out.close();
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

SVERROR AppController::RunFailoverJob()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR retVal = SVS_FALSE;
    AppCommandPtr appCmdPtr;
    bool shouldRun = false;
    bool bIsRunFailed= false;
    DebugPrintf(SV_LOG_DEBUG, "In RunFailoverJob %ld\n", m_policyId) ;
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);
    AppAgentConfiguratorPtr appConfiguratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    bool policyEnabled = schedulerPtr->isPolicyEnabled(m_policyId) ;
    bool bRunFailoverSteps = isFailoverInformationRequired(m_policyId);
    if(policyEnabled)
    {
        Controller::getInstance()->setProcessRequests(false) ;
        if(!bRunFailoverSteps)
        {
            DebugPrintf(SV_LOG_DEBUG,"In ClusterConstructionSteps\n");
            shouldRun = true ;
        }
        else if(m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailover") == 0 ||
              m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailoverRollbackVolumes") == 0 ||
              m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailback") == 0 ||
              m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailbackRollbackVolumes") == 0)
        {
            TagDiscover tagDiscover;
            std::list<std::string>volumeList = m_AppProtectionSettings.CustomVolume;
            std::list<std::string>::iterator volIter = m_AppProtectionSettings.appVolumes.begin();
            while( volIter != m_AppProtectionSettings.appVolumes.end())
            {
                volumeList.push_back(*volIter);
                volIter++;
            }
            if(tagDiscover.isTagReached(volumeList,m_FailoverInfo.m_TagName))
            {
                shouldRun = true ;
                DebugPrintf(SV_LOG_DEBUG,"Tag %s Reached\n",m_FailoverInfo.m_TagName.c_str());
            }
        }
        else
        {
            shouldRun = true ;
        }
    }
    if(shouldRun && policyEnabled)
    {
        FailoverJob& failoverJob = m_FailoverJobs.m_FailoverJobsMap.find(m_policyId)->second;
        schedulerPtr->UpdateTaskStatus(m_policyId,TASK_STATE_STARTED);
        if( failoverJob.m_JobState == TASK_STATE_NOT_STARTED )
        {
            failoverJob.m_StartTime = convertTimeToString(time(NULL));
        }
#ifdef SV_WINDOWS
		//Reset the user handle so that the user token is upto-date
		//and the AppCommand will use this token to spwan the process.
		ControllerPtr controllerPtr = Controller::getInstance();
		controllerPtr->resetUserHandle();
#endif
        if( failoverJob.m_JobState != TASK_STATE_COMPLETED || failoverJob.m_JobState != TASK_STATE_FAILED )
        {
            std::list<FailoverCommand>::iterator cmdIter = failoverJob.m_FailoverCommands.begin();
            failoverJob.m_JobState = TASK_STATE_STARTED ;
            while(cmdIter != failoverJob.m_FailoverCommands.end() )
            {                
                if((cmdIter->m_GroupId != CLUSTER_RECONSTRUCTION && cmdIter->m_GroupId != START_APP_CLUSTER) 
                    && cmdIter->m_JobState == TASK_STATE_NOT_STARTED)
                {
                    std::string outputFileName = getOutputFileName(m_policyId,cmdIter->m_GroupId);
                    std::stringstream stream;
                    stream << "LogFilePath is "<<outputFileName<<std::endl;
                    WriteStringIntoFile(outputFileName,stream.str());
                    appCmdPtr.reset(new AppCommand(cmdIter->m_Command,0,outputFileName));
                    cmdIter->m_JobState = TASK_STATE_STARTED;  
                    cmdIter->m_InstanceId = instanceId;
                    time_t tempTime = time(NULL);
                    cmdIter->m_StartTime = convertTimeToString(tempTime);
                    std::string op;
					void *h = NULL;
#ifdef SV_WINDOWS
					if(cmdIter->m_bUseUserToken)
					{
						h = controllerPtr->getDuplicateUserHandle();
					}
#endif
                    if(appCmdPtr->Run(cmdIter->m_ExitCode,op, Controller::getInstance()->m_bActive,"", h) == SVS_OK)
                    {
                        tempTime = time(NULL);
                        cmdIter->m_EndTime = convertTimeToString(tempTime);
                        if( cmdIter->m_ExitCode != 0 )
                        {
                            DebugPrintf(SV_LOG_WARNING, "Command %s failed with exit Code %ld OutPut %s\n", 
                                cmdIter->m_Command.c_str(), cmdIter->m_ExitCode,op.c_str()) ;
                            cmdIter->m_JobState = TASK_STATE_FAILED;
                        }
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_DEBUG,"Failed to run the command %s\n",cmdIter->m_Command.c_str());
                        cmdIter->m_JobState = TASK_STATE_FAILED;
                    }
#ifdef SV_WINDOWS
					if(h)
					{
						CloseHandle(h);
					}
#endif
                    appConfiguratorPtr->SerializeFailoverJobs(m_FailoverJobs);
                    if( cmdIter->m_JobState == TASK_STATE_FAILED )
                    {
                        bIsRunFailed = true;
                        break ;
                    }
                    else
                    {
                        cmdIter->m_JobState = TASK_STATE_COMPLETED;
                        DebugPrintf(SV_LOG_DEBUG,"Successfully ran %s\n",cmdIter->m_Command.c_str());
                    }
                }
#ifdef SV_WINDOWS
                if(cmdIter->m_GroupId == START_APP_CLUSTER)
                {
                    if(m_FailoverInfo.m_ConvertToCluster.compare("0") ==0 &&
                       m_FailoverInfo.m_startApplicationService.compare("1") ==0)
                    {
						if(StartAppServices() == true)
						{
							cmdIter->m_ExitCode = 0;
							cmdIter->m_JobState = TASK_STATE_COMPLETED;
							DebugPrintf(SV_LOG_DEBUG,"Successfully started the sevices \n");
						}
						else
						{
							cmdIter->m_ExitCode = 1;
							cmdIter->m_JobState = TASK_STATE_FAILED;
							DebugPrintf(SV_LOG_DEBUG,"Failed to start the sevices \n");
							bIsRunFailed = true;
						}
					}
                }
                if(cmdIter->m_GroupId == CLUSTER_RECONSTRUCTION )
                {
                    if(RunClusterReconstructionSteps(failoverJob))
                    {
                        DebugPrintf(SV_LOG_DEBUG,"Cluster Reconstruction Completed\n");
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_DEBUG,"Cluster Reconstruction Failed\n");
                        bIsRunFailed = true;
                        break ;
                    }
                }
#endif
                cmdIter++;
            }
            if( !Controller::getInstance()->QuitRequested(1) )
            {
                if(!bIsRunFailed)
                {
                    if (m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailover") == 0 ||
                          m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailoverRollbackVolumes") == 0 ||
                          m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailoverSetupApplicationEnvironment") == 0 ||
                          m_FailoverInfo.m_PolicyType.compare("DRServerPlannedFailoverStartApplication") == 0 ||
                        m_FailoverInfo.m_PolicyType.compare("DRServerUnPlannedFailover") == 0 ||
                          m_FailoverInfo.m_PolicyType.compare("DRServerUnPlannedFailoverRollbackVolumes") == 0 ||
                          m_FailoverInfo.m_PolicyType.compare("DRServerUnPlannedFailoverSetupApplicationEnvironment") == 0 ||
                          m_FailoverInfo.m_PolicyType.compare("DRServerUnPlannedFailoverStartApplication") == 0 ||
                          m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailback") == 0 ||
                          m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailbackRollbackVolumes") == 0 ||
                          m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailbackSetupApplicationEnvironment") == 0 ||
                          m_FailoverInfo.m_PolicyType.compare("ProductionServerPlannedFailbackStartApplication") == 0)
                    {
                        MakeAppServicesAutoStart();
                    }
                    failoverJob.m_JobState =  TASK_STATE_COMPLETED;
                    failoverJob.m_EndTime = convertTimeToString(time(NULL));   
                    schedulerPtr->UpdateTaskStatus(m_policyId,TASK_STATE_COMPLETED);
                }
                else
                {
                    failoverJob.m_JobState =  TASK_STATE_FAILED;
                    failoverJob.m_EndTime = convertTimeToString(time(NULL));
                    schedulerPtr->UpdateTaskStatus(m_policyId,TASK_STATE_FAILED);
                }
            }
        }
        appConfiguratorPtr->SerializeFailoverJobs(m_FailoverJobs);
        if( !Controller::getInstance()->QuitRequested(1) )
        {
            appConfiguratorPtr->UpdateFailoverJobs(failoverJob,m_policyId,instanceId);
            RemoveFailoverJobFromCache(m_policyId) ;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG,"Policy %ld is not yet enabled\n", m_policyId);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return retVal;
}

void AppController::AddFailoverJob(SV_ULONG policyId, const FailoverJob& failoverJob)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AutoGuard lock( m_lock ) ; 	
    if(m_FailoverJobs.m_FailoverJobsMap.find(policyId) != m_FailoverJobs.m_FailoverJobsMap.end())
    {
        if(m_FailoverJobs.m_FailoverJobsMap.find(policyId)->second.m_JobState != TASK_STATE_NOT_STARTED &&
            m_FailoverJobs.m_FailoverJobsMap.find(policyId)->second.m_JobState != TASK_STATE_COMPLETED &&
            m_FailoverJobs.m_FailoverJobsMap.find(policyId)->second.m_JobState != TASK_STATE_FAILED)
        {
            DebugPrintf(SV_LOG_DEBUG,"Job already started so not updating into failoverjob map\n");
        }
        else
        {
            m_FailoverJobs.m_FailoverJobsMap.find(policyId)->second = failoverJob;
        }
    }
    else
    {
        m_FailoverJobs.m_FailoverJobsMap.insert(std::make_pair(policyId,failoverJob));
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void AppController::RemoveFailoverJobFromCache(SV_ULONG policyId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AutoGuard lock( m_lock ) ; 	
    std::map<SV_ULONG, FailoverJob>::iterator failoverJobIter ;
    failoverJobIter = m_FailoverJobs.m_FailoverJobsMap.find(policyId) ;
    if( failoverJobIter != m_FailoverJobs.m_FailoverJobsMap.end() )
    {
        m_FailoverJobs.m_FailoverJobsMap.erase( failoverJobIter ) ;
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "The Failover Job with policy id %ld doesnt exist in failoverjobs\n", policyId) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
FailoverJobs AppController::getFailoverJobsMap()
{
    return m_FailoverJobs;
}

std::string AppController::GetTagType(const std::string& appType)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::string tagType;
    if(appType.compare("EXCHANGE2003") == 0)
    {
        tagType = "EXCHANGE";
    }
    else if(appType.compare("EXCHANGE2007") == 0 || 
        appType.compare("EXCHANGE2010") == 0)
    {
        tagType = "ExchangeIS";
    }
    else if(appType.compare("SQL") == 0)
    {
        tagType = "SQL";
    }
    else if(appType.compare("SQL2005") == 0)
    {
        tagType = "SQL2005";
    }
    else if(appType.compare("SQL2008") == 0)
    {
        tagType = "SQL2008";
    }
	else if(appType.compare("SQL2012") == 0)
    {
        tagType = "SQL2012";
    }
    else if(appType.compare("ORACLE") == 0)
    {
        tagType = "ORACLE";
    }
    else
    {
        tagType = "FS";
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return tagType;
}
#ifdef SV_WINDOWS
bool AppController::RunClusterReconstructionSteps(FailoverJob& failoverJob)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SV_ULONG exitCode = 1;
    bool bRet = true;
    std::list<FailoverCommand>::iterator failoverCmd = failoverJob.m_FailoverCommands.begin();
    while(failoverCmd != failoverJob.m_FailoverCommands.end())
    {
        if( failoverCmd->m_GroupId == CLUSTER_RECONSTRUCTION )
        {
            break;
        }
        failoverCmd++;
    }
    AppAgentConfiguratorPtr appConfiguratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    std::string outputFileName = getOutputFileName(m_policyId,failoverCmd->m_GroupId);
    ClusterOp clusterOpObj;

    if( failoverCmd->m_State == CLUSTER_RECONSTRUCTION_NOTSTARTED )
    {
        
        if(isPairRemoved() == true)
        {
            failoverCmd->m_State = CLUSTER_RECONSTRUCTION_IN_PROGRESS ;
            PreClusterReconstruction() ;
            appConfiguratorPtr->SerializeFailoverJobs(m_FailoverJobs);
            if(clusterOpObj.constructCluster(outputFileName,exitCode) != true || exitCode != 0)
            {
                failoverCmd->m_State = CLSTER_RECONSTRUCTION_FAILED;
                failoverCmd->m_JobState = TASK_STATE_FAILED;
                DebugPrintf(SV_LOG_ERROR,"Failed to reconstruct the cluster exitCode is %ld\n",exitCode);
				bRet = false;
            }
            else
            {
                if( !Controller::getInstance()->QuitRequested(1) )
                {
                    DebugPrintf(SV_LOG_DEBUG,"Quit signal request\n");
                    Controller::getInstance()->Stop();
                }
                return bRet ;
            }
        }
		{
			DebugPrintf(SV_LOG_WARNING, "Paires are not yet removed. Can not reconstruct cluster.");
			bRet = false;
		}
    }
    else
    {
        std::string outputStr;
        std::stringstream stream;
        std::map<std::string, std::string>& propsMap = m_policy.m_policyProperties ;
        GetValueFromMap(propsMap,"ApplicationType",m_FailoverInfo.m_AppType,false);
        if(strstr(m_FailoverInfo.m_AppType.c_str(),"SQL") ||
            strstr(m_FailoverInfo.m_AppType.c_str(),"FILESERVER") )
        {
            InmServiceStatus ClusSvcStatus ;
            getServiceStatus("ClusSvc", ClusSvcStatus);
            if( ClusSvcStatus != INM_SVCSTAT_RUNNING )
            {
				int clusSvcTimeOut = 0;
                DebugPrintf(SV_LOG_DEBUG, "Cluster service is not running,Trying to start service\n") ;
                //stream << "Cluster service is not running,Trying to start the service"<<std::endl;
				//StrService("ClusSvc");
				do //Cluster service need to be available for making resources online. Otherwise the resources may not come online.
				{
					if (StrService("ClusSvc") == SVS_FALSE)
					{
						if(clusSvcTimeOut > 900)
						{
							DebugPrintf(SV_LOG_DEBUG, "Retry time out. Failed to start ClusSvc service\n");
							break;
						}
						//stream<<"Failed to start ClusSvc service"<<std::endl;
						DebugPrintf(SV_LOG_DEBUG, "Failed to start ClusSvc service. Retrying to start the service.\n");
						clusSvcTimeOut += 5;
					}
					else
					{
						DebugPrintf(SV_LOG_DEBUG, "Successfully started the ClusSvc service\n");
						//stream<<"Successfully started the ClusSvc service"<<std::endl;
						break;
					}
				}while(!Controller::getInstance()->QuitRequested(5));
            }
            if(clusterOpObj.revertResourcesToOriginalState(outputStr) != SVS_OK)
			{
				outputStr += "\n Failed to revert the cluster resources to original state. \n \
							 Please online failed cluster resources manually\n";
				bRet = false;
			}

            std::string startAppOutputFileName = getOutputFileName(m_policyId,START_APP_CLUSTER);
			bool bStartAppClstr = true;
            std::stringstream startAppInfoStream;
            std::string errLog;
            if(strstr(m_FailoverInfo.m_AppType.c_str(),"SQL"))
            {
                std::list<std::string> sqlServerAnalysisResourceTypeList;
				if( getResourcesByType("", "Generic Service", sqlServerAnalysisResourceTypeList) == SVS_OK )
                {
                    std::list<std::string>::iterator listIter = sqlServerAnalysisResourceTypeList.begin();
                    while(listIter != sqlServerAnalysisResourceTypeList.end())
                    {
                        errLog.clear();
                        if(clusterOpObj.onlineClusterResources(*listIter,errLog) == SVS_OK)
                        {
                            startAppInfoStream << "Successfully brought the Cluster Resource " ;
                            startAppInfoStream << (*listIter);
                            startAppInfoStream << " online \n";
                        }
                        else
                        {
                            startAppInfoStream << errLog;
							bStartAppClstr = false;
                        }
                        listIter++;
                    }
                }
				else
				{
					startAppInfoStream << "Failed to retrieve the cluster Generic Service resource information" << std::endl;
					bStartAppClstr = false;
				}
                std::list<std::string> sqlServerResourceTypeList;
                if( getResourcesByType("", "SQL Server", sqlServerResourceTypeList) == SVS_OK )
                {
					if(sqlServerResourceTypeList.empty())
					{
						startAppInfoStream << "No resrource found of type: SQL Server" << std::endl;
					}
                    std::list<std::string>::iterator listIter =sqlServerResourceTypeList.begin();
                    while(listIter != sqlServerResourceTypeList.end())
                    {
                        errLog.clear();
                        if(clusterOpObj.onlineClusterResources(*listIter,errLog) == SVS_OK)
                        {
                            startAppInfoStream << "Successfully brought the Cluster Resource " ;
                            startAppInfoStream << (*listIter);
                            startAppInfoStream << " online \n";
                        }
                        else
                        {
                            startAppInfoStream << errLog;
							bStartAppClstr = false;
                        }
                        listIter++;
                    }
                }
				else
				{
					startAppInfoStream << "Failed to retrieve the cluster SQL Server resource information" << std::endl;
					bStartAppClstr = false;
				}
                std::list<std::string> sqlServerAgentResourceTypeList;
                if( getResourcesByType("", "SQL Server Agent", sqlServerAgentResourceTypeList) == SVS_OK )
                {
					if(sqlServerAgentResourceTypeList.empty())
					{
						startAppInfoStream << "No resrource found of type: SQL Server Agent" << std::endl;
					}
                    std::list<std::string>::iterator listIter =sqlServerAgentResourceTypeList.begin();
                    while(listIter != sqlServerAgentResourceTypeList.end())
                    {
                        errLog.clear();
                        if(clusterOpObj.onlineClusterResources(*listIter,errLog) == SVS_OK)
                        {
                            startAppInfoStream << "Successfully brought the Cluster Resource " ;
                            startAppInfoStream << (*listIter);
                            startAppInfoStream << " online \n";
                        }
                        else
                        {
                            startAppInfoStream << errLog;
							bStartAppClstr = false;
                        }
                        listIter++;
                    }
                }
				else
				{
					startAppInfoStream << "Failed to retrieve the cluster SQL Server resource information" << std::endl;
					bStartAppClstr = false;
				}
            }
            else if(strstr(m_FailoverInfo.m_AppType.c_str(),"FILESERVER"))
            {
                std::list<std::string> FileServerResourceTypeList;
                if( getResourcesByType("", "Network Name", FileServerResourceTypeList) == SVS_OK )
                {
					if(FileServerResourceTypeList.empty())
					{
						startAppInfoStream << "No resrource found of type: Network Name" << std::endl;
					}
                    std::list<std::string>::iterator listIter =FileServerResourceTypeList.begin();
                    while(listIter != FileServerResourceTypeList.end())
                    {
                        if(clusterOpObj.onlineClusterResources(*listIter,errLog) == SVS_OK)
                        {
                            startAppInfoStream << "Successfully brought the Cluster Resource " ;
                            startAppInfoStream << (*listIter);
                            startAppInfoStream << " online \n";
                        }
                        else
                        {
                            startAppInfoStream << errLog;
							bStartAppClstr = false;
                        }
                        listIter++;
                    }
                }
				else
				{
					startAppInfoStream << "Failed to retrieve the cluster Network Name resource information" << std::endl;
					bStartAppClstr = false;
				}
				OSVERSIONINFOEX osVersionInfo ;
				getOSVersionInfo(osVersionInfo) ;
				if(osVersionInfo.dwMajorVersion == 6) //making this condition true for windows 2008 sp1,sp2 and R2
				{
					FileServerResourceTypeList.clear();
					if( getResourcesByType("", "File Server", FileServerResourceTypeList) == SVS_OK )
					{
						if(FileServerResourceTypeList.empty())
						{
							startAppInfoStream << "No resrource found of type: File Server" << std::endl;
						}
						std::list<std::string>::iterator listIter =FileServerResourceTypeList.begin();
						while(listIter != FileServerResourceTypeList.end())
						{
							if(clusterOpObj.onlineClusterResources(*listIter,errLog) == SVS_OK)
							{
								startAppInfoStream << "Successfully brought the Cluster Resource " ;
								startAppInfoStream << (*listIter);
								startAppInfoStream << " online \n";
							}
							else
							{
								startAppInfoStream << errLog;
								bStartAppClstr = false;
							}
							listIter++;
						}
					}
					else
					{
						startAppInfoStream << "Failed to retrieve the cluster File Server resource information" << std::endl;
						bStartAppClstr = false;
					}
				}
            }
			//Bug#17019
			std::list<FailoverCommand>::iterator startAppServicesCmd = failoverJob.m_FailoverCommands.begin();
			while(startAppServicesCmd != failoverJob.m_FailoverCommands.end())
			{
				if(startAppServicesCmd->m_GroupId == START_APP_CLUSTER)
				{
					startAppServicesCmd->m_JobState = TASK_STATE_COMPLETED;
					if(bStartAppClstr)
					{
						startAppServicesCmd->m_ExitCode = 0;
					}
					else
					{
						startAppServicesCmd->m_ExitCode = 1;
						startAppInfoStream << "\nINFORMATION: Some of the cluster resources are not online. Please make them online manually.\n" ;
					}
					DebugPrintf(SV_LOG_DEBUG,"Updating the task start_cluster_App_Services log file: %s.\n",startAppOutputFileName.c_str());
					WriteStringIntoFile(startAppOutputFileName,startAppInfoStream.str());

					break;
				}
				startAppServicesCmd++;
			}
		}
        else
        {
            outputStr =  "\n Please make sure all the passive nodes up and resources are online to make application available\n";
        }

		stream << outputStr;

		/* bug#23393: Svagent struck at stopping state for service stop request in cluster unplanned failover where cx is down.
		
		In previous release we added svagent restart login after cluster construction in failover/failback to avoid the issues
		in cluster registration with CX. But now, the service restart is causing the issues. Hence removing the earlier 
		workround to avoid the not responding issue. With this change the cluster registration issue should be addressed in 
		svagent.*/

		//Restarting the svagent service to create/update the SOFTWARE\\SV Systems\\ClusterTracking key in registry.
		//doing this to report the cluster information to CX by svagent.
		//HKEY hKey = NULL;
		//LONG lResult;
		//HKEY hResult;
		//LPTSTR lpSubKey = "SOFTWARE\\SV Systems\\ClusterTracking";
		//if(RegConnectRegistry(NULL,HKEY_LOCAL_MACHINE,&hKey) == ERROR_SUCCESS)
		//{
		//	int time = 0;
		//	do
		//	{
		//		if(time%120 == 0)
		//		{
		//			if(ReStartSvc("svagents") == SVS_FALSE)
		//			{
		//				DebugPrintf(SV_LOG_DEBUG, "Failed to restart the svagent service\n");
		//				stream<<"INFORMTION: Failed to restart the svagent service. It may cause cluster registration issues with CX.\
		//						Please restart svagent service manually to avoid this kind of issues."<<std::endl;
		//				break;
		//			}
		//			else
		//			{
		//				DebugPrintf(SV_LOG_DEBUG, "Successfully restarted svagent service\n");
		//				//stream<<"Successfully restarted svagent service"<<std::endl;
		//			}
		//		}
		//		lResult = RegOpenKeyEx (HKEY_LOCAL_MACHINE, lpSubKey, 0, KEY_READ, &hResult);
		//		if(ERROR_SUCCESS != lResult)
		//		{
		//			if (lResult == ERROR_FILE_NOT_FOUND)
		//			{
		//				if(time > 360)
		//				{
		//					stream<<"INFORMTION: Please restart svagent service manually if this cluster node is not registered with CX."<<std::endl;
		//					break;
		//				}
		//				DebugPrintf(SV_LOG_ERROR, "Failed to get HKLM\\%s key, ERROR: [%ld], will retry after 10 sec\n", lpSubKey, lResult);
		//			}
		//			else
		//			{
		//				DebugPrintf(SV_LOG_ERROR, "Failed to open HKLM\\%s key, ERROR: [%ld]\n", lpSubKey, lResult);
		//				stream<<"INFORMTION: Please restart svagent service manually if this cluster node is not registered with CX."<<std::endl;
		//				break;
		//			}
		//		}
		//		else
		//		{
		//			DebugPrintf(SV_LOG_DEBUG, "ClusterTracking registry key updated successfully\n");
		//			stream<<"ClusterTracking registry key updated successfully"<<std::endl;
		//			break;
		//		}
		//		time = time + 10;
		//	}while(!Controller::getInstance()->QuitRequested(10));
		//}
		//else
		//{
		//	DebugPrintf(SV_LOG_INFO, "\nFailed to connect to local machines registry\n");
		//	stream<<"INFORMTION: Please restart svagent service manually if this cluster node is not registered with CX."<<std::endl;
  //      }

		if(bRet)
			failoverCmd->m_ExitCode = 0;
		else
			failoverCmd->m_ExitCode = 1;
        failoverCmd->m_State = CLUSTER_RECONSTRUCTION_COMPLETED;
        failoverCmd->m_JobState = TASK_STATE_COMPLETED;
        WriteStringIntoFile(outputFileName,stream.str());
    }
    appConfiguratorPtr->SerializeFailoverJobs(m_FailoverJobs);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet;
}
#endif

void AppController::getSourceHostNameForFileCreation(std::string& srcFileName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;

    if(m_FailoverInfo.m_FailoverType.compare("Failover") == 0)
    {
        if(m_FailoverInfo.m_ProductionVirtualServerName.empty() == false)
        {
            srcFileName = m_FailoverInfo.m_ProductionVirtualServerName; 
        }
        else
        {
            srcFileName = m_FailoverInfo.m_ProductionServerName;
        }
    }
    else 
    {
        if(m_FailoverInfo.m_DRVirtualServerName.empty() == false)
        {
            srcFileName = m_FailoverInfo.m_DRVirtualServerName;
        }
        else
        {
            srcFileName = m_FailoverInfo.m_DRServerName;
        }
    }
    DebugPrintf(SV_LOG_DEBUG,"SourceHostNameForFileCreation %s\n",srcFileName.c_str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

bool AppController::isFailoverInformationRequired(SV_ULONG policyId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = true;
    if(m_FailoverJobs.m_FailoverJobsMap.size())
    {
        std::map<SV_ULONG, FailoverJob>::iterator failoverJobMapIter = m_FailoverJobs.m_FailoverJobsMap.find(policyId);
        if(failoverJobMapIter !=  m_FailoverJobs.m_FailoverJobsMap.end())
        {
            FailoverJob& failoverJob = failoverJobMapIter->second;
            std::list<FailoverCommand>::iterator failoverCommadsIter =  failoverJob.m_FailoverCommands.begin();
            while(failoverCommadsIter != failoverJob.m_FailoverCommands.end())
            {
                if( failoverCommadsIter->m_GroupId == CLUSTER_RECONSTRUCTION )
                {
                    if(failoverCommadsIter->m_State == CLUSTER_RECONSTRUCTION_IN_PROGRESS)
                    {
                        bRet = false;
                        DebugPrintf(SV_LOG_DEBUG,"FailoverJob already started and in cluster construction state\n");
                    }
                    break;
                }
                failoverCommadsIter++;
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet;
}

bool AppController::checkPairsInDiffsync(const std::string& scenarioId,std::string& outputFileName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = true;
    std::stringstream stream;
    std::map<std::string,std::string> pairDetails;
    if(GetProtectionSettings(scenarioId) == SVS_OK)
    {
        std::map<std::string, ReplicationPair>::iterator pairsIter = m_AppProtectionSettings.Pairs.begin();
        while(pairsIter != m_AppProtectionSettings.Pairs.end())
        {
            std::map<std::string, std::string>& replicationPairInfoMap = pairsIter->second.m_properties;
            std::map<std::string, std::string>::iterator replicationPairInfoMapIter = replicationPairInfoMap.find("PairState");
            if(replicationPairInfoMapIter != replicationPairInfoMap.end())
            {
                if(replicationPairInfoMapIter->second.compare("Diff") != 0)
                {
                    if(replicationPairInfoMap.find("RemoteMountPoint") != replicationPairInfoMap.end())
                    {
                        pairDetails.insert(std::make_pair(pairsIter->first,replicationPairInfoMap.find("RemoteMountPoint")->second));
                        bRet = false;
                    }
                }
            }
            pairsIter++;
        }
        if(bRet == false)
        {
            stream << "The following Pairs ";
            std::map<std::string,std::string>::iterator pairDetailsIter = pairDetails.begin();
            while(pairDetailsIter != pairDetails.end())
            {
                stream << pairDetailsIter->first;
                stream << " -> ";
                stream << pairDetailsIter->second;
                stream << "  ";
                pairDetailsIter++;
            }
            stream << "are not in diff sync";
        }   
    }
    else
    {
        stream << "Protection Settings for consistency policy not found ";       
        bRet = false;
    }
    WriteStringIntoFile(outputFileName,stream.str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet;
}


bool AppController::getDiffSyncVolumesList(const std::string& scenarioId, std::string& outputFileName, std::list<std::string>& diffSyncVolumeList)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::stringstream stream;
    bool allPairsInDiff = true;
    if(GetProtectionSettings(scenarioId) == SVS_OK)
    {
        std::map<std::string, std::string> diffpairsMap, nonDiffPairsMap;
        std::map<std::string, ReplicationPair>::iterator pairsIter = m_AppProtectionSettings.Pairs.begin();
        while(pairsIter != m_AppProtectionSettings.Pairs.end())
        {
            std::map<std::string, std::string>& replicationPairInfoMap = pairsIter->second.m_properties;
            std::map<std::string, std::string>::iterator replicationPairInfoMapIter = replicationPairInfoMap.find("PairState");
            if(replicationPairInfoMapIter != replicationPairInfoMap.end())
            {
                if(replicationPairInfoMapIter->second.compare("Diff") == 0)
                {
                    if(replicationPairInfoMap.find("RemoteMountPoint") != replicationPairInfoMap.end())
                    {
                        diffpairsMap.insert(std::make_pair(pairsIter->first,replicationPairInfoMap.find("RemoteMountPoint")->second));
                        diffSyncVolumeList.push_back(pairsIter->first);
                    }
                }
                else
                {
                    if(replicationPairInfoMap.find("RemoteMountPoint") != replicationPairInfoMap.end())
                    {
                        nonDiffPairsMap.insert(std::make_pair(pairsIter->first,replicationPairInfoMap.find("RemoteMountPoint")->second));
                    }
                    allPairsInDiff = false;
                }
            }
            pairsIter++;
        }
        stream << "The following Pairs ";
        std::map<std::string,std::string>::iterator diffPairDetailsIter = diffpairsMap.begin();
        while(diffPairDetailsIter != diffpairsMap.end())
        {
            stream << diffPairDetailsIter->first;
            stream << " -> ";
            stream << diffPairDetailsIter->second;
            stream << "  ";
            diffPairDetailsIter++;
        }
        stream << "are in diff sync" << std::endl;

        stream << "The following Pairs ";
        std::map<std::string,std::string>::iterator nonDiffPairDetailsIter = nonDiffPairsMap.begin();
        while(nonDiffPairDetailsIter != nonDiffPairsMap.end())
        {
            stream << nonDiffPairDetailsIter->first;
            stream << " -> ";
            stream << nonDiffPairDetailsIter->second;
            stream << "  ";
            nonDiffPairDetailsIter++;
        }
        stream << "are not in diff sync" << std::endl;
    }
    else
    {
        stream << "Protection Settings for consistency policy not found ";       
    }
    WriteStringIntoFile(outputFileName,stream.str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return allPairsInDiff;
}
bool AppController::isPairRemoved()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false;
      if(m_FailoverInfo.m_PolicyType.compare("DRServerUnPlannedFailover") == 0 ||
          m_FailoverInfo.m_PolicyType.compare("DRServerUnPlannedFailoverRollbackVolumes") == 0 ||
          m_FailoverInfo.m_PolicyType.compare("DRServerUnPlannedFailoverSetupApplicationEnvironment") == 0 ||
          m_FailoverInfo.m_PolicyType.compare("DRServerUnPlannedFailoverStartApplication") == 0)
    {
        bRet = true;
    }
    else
    {
        while( !bRet && !Controller::getInstance()->QuitRequested(10) )
        {
            if(GetProtectionSettings(m_FailoverInfo.m_ScenarioId) != SVS_OK)
            {
                bRet = true;
                DebugPrintf(SV_LOG_DEBUG,"Pairs removed\n");
                break;
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG,"Pairs not yet removed\n");
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet;
}

void AppController::BuildWinOpCommand(std::string& winOpCmd, bool bConsiderAllZones)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
#ifdef SV_WINDOWS
	std::string namingContexts, dnsServerName;
	TCHAR buffer[256] = TEXT("");
	DWORD dwSize = sizeof(buffer);

	if (!GetComputerNameEx((COMPUTER_NAME_FORMAT)3, buffer, &dwSize))
	{
		DebugPrintf(SV_LOG_ERROR,"GetComputerNameEx failed. Error code: %d. Cosidering only DEFAULT zone\n", GetLastError());
		namingContexts = "DEFAULT";
	}
	else 
	{
		DebugPrintf("Fully qualified domain name: %s\n",buffer);
		size_t firstDot = 0, foundDot = 0;
		std::string domainName, domainDN, hostName = std::string(buffer);
		if( (firstDot = hostName.find_first_of('.', 0)) != std::string::npos )
		{
			domainName.assign(hostName.c_str(), firstDot+1, hostName.length()-1-firstDot);			
			domainDN = "DC=" + domainName;
			while ( (foundDot = domainDN.find_first_of('.', 0)) != std::string::npos )			
				domainDN.replace(foundDot, 1, ",DC=");
			hostName.assign(hostName.c_str(), firstDot);
		}
		
		std::string domainDnsZones, forestDnsZone;
		
		domainDnsZones = "DC=DomainDnsZones,";
		domainDnsZones += domainDN;

		forestDnsZone = "DC=ForestDnsZones,";
		forestDnsZone += domainDN;

		if(bConsiderAllZones)
		{
			namingContexts += std::string("DEFAULT") + ";";
			namingContexts += domainDN + ";";
		}
		else
		{
			ADInterface IAd;
			IAd.getDnsServerName(dnsServerName);
		}
		namingContexts += domainDnsZones + ";";
		namingContexts += forestDnsZone;
	}

    std::stringstream cmdStream;
    AppLocalConfigurator localconfigurator;
    cmdStream <<"\"";
    cmdStream << localconfigurator.getInstallPath();
    cmdStream << "\\";
    cmdStream << "WinOp.exe";
    cmdStream <<"\"";
    cmdStream <<" AD ";
    cmdStream <<" -replicate ";
	cmdStream <<"\""<<namingContexts.c_str()<<"\"";
    cmdStream <<" -UpdateAllDnsServers ";
	if(!dnsServerName.empty())
		cmdStream <<" -DC " <<dnsServerName.c_str();
    winOpCmd = cmdStream.str();
#endif
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void AppController::BuildClusReConstructionCommand(std::string& clusConstructionCmdStr)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::stringstream cmdStream;
    AppLocalConfigurator localconfigurator;
    cmdStream << "\"";
    cmdStream << localconfigurator.getInstallPath();
    cmdStream << "\\";
    cmdStream << "ClusUtil.exe";
    cmdStream << "\"";
    cmdStream << " -prepare StandaloneToCluster:";
	cmdStream << Host::GetInstance().GetHostName();
	cmdStream << " -force";
	clusConstructionCmdStr = cmdStream.str();
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
