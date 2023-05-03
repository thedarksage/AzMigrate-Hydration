#include <list>
#include "Oraclediscoveryandvalidator.h"
#include "config/appconfigurator.h"
#include <boost/lexical_cast.hpp>
#include "ruleengine/unix/ruleengine.h"
#include "util/common/util.h"
#include "discovery/discoverycontroller.h"
//#include "util/system.h"
#include <boost/lexical_cast.hpp>
#include "appscheduler.h"

OracleDiscoveryAndValidator::OracleDiscoveryAndValidator(const ApplicationPolicy& policy)
:DiscoveryAndValidator(policy)
{
}

void OracleDiscoveryAndValidator::convertToOracleDiscoveryInfo(const OracleAppDiscoveryInfo& oracleDiscoveryInfo, 
        OracleDiscoveryInfo& oraclediscoveryInfo )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    DiscoveryControllerPtr discCtrlPtr = DiscoveryController::getInstance() ;

    oraclediscoveryInfo.m_oracleDBInstanceInfoMap.clear();
    oraclediscoveryInfo.m_oracleClusterInfoMap.clear();
    oraclediscoveryInfo.m_oracleUnregisterInfoMap.clear();
    oraclediscoveryInfo.m_dbOracleAppDevInfo.clear();


    // there should be only one cluster info
    std::map<std::string, OracleClusterDiscInfo>::const_iterator clusterIter  = oracleDiscoveryInfo.m_dbOracleClusterDiscInfo.begin();

    OracleClusterInfo oracleClusterInfo;

    if (clusterIter != oracleDiscoveryInfo.m_dbOracleClusterDiscInfo.end())
    {
        oracleClusterInfo.m_clusterName = (clusterIter->second).m_clusterName;
        oracleClusterInfo.m_nodeName = (clusterIter->second).m_nodeName;
        oracleClusterInfo.m_otherNodes = (clusterIter->second).m_otherNodes;

        oraclediscoveryInfo.m_oracleClusterInfoMap.insert(std::make_pair(oracleClusterInfo.m_clusterName, oracleClusterInfo));
    }

    std::map<std::string, OracleAppDevDiscInfo>::const_iterator appdevIter = oracleDiscoveryInfo.m_dbOracleAppDevDiscInfo.begin();

    for(; appdevIter!= oracleDiscoveryInfo.m_dbOracleAppDevDiscInfo.end(); appdevIter++)
    {
        DebugPrintf(SV_LOG_DEBUG, "APP DEVS TO REPORT --> %s\n", appdevIter->first.c_str());

        OracleAppDevInfo oracleAppDevInfo;

        oracleAppDevInfo.m_devNames = (appdevIter->second).m_devNames;
     
        oraclediscoveryInfo.m_dbOracleAppDevInfo.insert(std::make_pair(appdevIter->first, oracleAppDevInfo));
    }

    std::map<std::string, OracleUnregisterDiscInfo>::const_iterator unregisterIter = oracleDiscoveryInfo.m_dbUnregisterDiscInfo.begin();

    for(; unregisterIter != oracleDiscoveryInfo.m_dbUnregisterDiscInfo.end(); unregisterIter++)
    {
        DebugPrintf(SV_LOG_DEBUG, "DB NAME TO UNREGISTER --> %s\n",unregisterIter->first.c_str());

        OracleUnregisterInfo oracleUnregisterInfo;

        oracleUnregisterInfo.m_dbName = unregisterIter->first;
     
        oraclediscoveryInfo.m_oracleUnregisterInfoMap.insert(std::make_pair(unregisterIter->first, oracleUnregisterInfo));
    }

    std::map<std::string, OracleAppVersionDiscInfo>::const_iterator discoveryIter = oracleDiscoveryInfo.m_dbOracleDiscInfo.begin();

    for(; discoveryIter != oracleDiscoveryInfo.m_dbOracleDiscInfo.end(); discoveryIter++)
    {
        DebugPrintf(SV_LOG_DEBUG, "DB NAME TO REPORT --> %s\n",discoveryIter->first.c_str());

        OracleDBInstanceInfo oracleDBInstanceInfo;

        oracleDBInstanceInfo.m_dbProperties.insert(std::make_pair("DBInstallPath", (discoveryIter->second).m_dbInstallPath));

        if((discoveryIter->second).m_isCluster == "true")
        {
            oracleDBInstanceInfo.m_dbProperties.insert(std::make_pair("IsClustered", "1")) ;
            oracleDBInstanceInfo.m_dbProperties.insert(std::make_pair("ClusterName", (discoveryIter->second).m_clusterName));
            oracleDBInstanceInfo.m_otherClusterNodes = (discoveryIter->second).m_otherClusterNodes;
        }
        else
            oracleDBInstanceInfo.m_dbProperties.insert(std::make_pair("IsClustered", "0")) ;

        if((discoveryIter->second).m_recoveryLogEnabled)
        {
            oracleDBInstanceInfo.m_dbProperties.insert(std::make_pair("RecoveryLogEnabled", "1"));
        }
        else
            oracleDBInstanceInfo.m_dbProperties.insert(std::make_pair("RecoveryLogEnabled", "0"));

        oracleDBInstanceInfo.m_dbProperties.insert(std::make_pair("NodeName", (discoveryIter->second).m_nodeName)) ;

        oracleDBInstanceInfo.m_dbProperties.insert(std::make_pair("DatabaseVersion", (discoveryIter->second).m_dbVersion)) ;

        oracleDBInstanceInfo.m_volumeInfo = (discoveryIter->second).m_diskView;
        oracleDBInstanceInfo.m_filterDevices = (discoveryIter->second).m_filterDeviceList;

        oracleDBInstanceInfo.m_files = (discoveryIter->second).m_filesMap;

        oraclediscoveryInfo.m_oracleDBInstanceInfoMap.insert(std::make_pair(discoveryIter->first, oracleDBInstanceInfo));
    }

    //fillPolicyInfoInfo(oraclediscoveryInfo.m_policyInfo);
    //oraclediscoveryInfo.m_policyInfo.insert(std::make_pair("ErrorCode", "0")) ;
    //oraclediscoveryInfo.m_policyInfo.insert(std::make_pair("ErrorString", "Oracle application discovery complete"));

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

SVERROR OracleDiscoveryAndValidator::PostDiscoveryInfoToCx()
{
    SVERROR bRet = SVS_FALSE ;

    DebugPrintf(SV_LOG_DEBUG, "ENTERED OracleDiscoveryAndValidator::%s\n", FUNCTION_NAME);
    DiscoveryControllerPtr discCtrlPtr = DiscoveryController::getInstance() ;
    DiscoveryInfo discoverInfo ;
    OracleDiscoveryInfo oracleDiscInfo;
    OracleAppDiscoveryInfo discoveryInfo;

    fillPolicyInfoInfo(oracleDiscInfo.m_policyInfo);	
    m_OracleAppPtr->getOracleAppDiscoveryInfo(discoveryInfo);
    convertToOracleDiscoveryInfo(discoveryInfo, oracleDiscInfo) ;

    oracleDiscInfo.m_policyInfo.insert(std::make_pair("ErrorCode", "0")) ;
    oracleDiscInfo.m_policyInfo.insert(std::make_pair("ErrorString", "Oracle application discovery complete"));
    discCtrlPtr->m_disCoveryInfo->oraclediscoveryInfo.push_back(oracleDiscInfo);

    bRet = SVS_OK ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ; 
}

SVERROR OracleDiscoveryAndValidator::Discover(bool shouldUpdateCx)
{
    SVERROR bRet = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED OracleDiscoveryAndValidator::%s\n", FUNCTION_NAME) ;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
    AppLocalConfigurator localconfigurator;
    do
    {
        if (!localconfigurator.shouldDiscoverOracle())
        {
            DebugPrintf(SV_LOG_DEBUG, "OracleDiscovery is disabled in drscout configuration.\n");
            bRet = SVS_OK;
            break;
        }
        m_OracleAppPtr.reset( new OracleApplication() ) ;

        if( m_OracleAppPtr->init() != SVS_OK)
        {
            DebugPrintf(SV_LOG_DEBUG, "Nothing to discover. \n") ;
            bRet = SVS_OK ;
        }
        else
        {
            if(m_policyId != 0)
            {
                SV_ULONGLONG instanceId = AppScheduler::getInstance()->getInstanceId(m_policyId);
                configuratorPtr->PolicyUpdate(m_policyId,instanceId,"InProgress","");

                //This is to clean up the appwizard.conf file which contains the unregistered db info
                DebugPrintf(SV_LOG_DEBUG,"Received PolicyId :%d\n",m_policyId);
                std::string policyData =  m_policy.m_policyData ;
                DebugPrintf(SV_LOG_DEBUG,"Received PolicyData: %s\n",policyData.c_str());
                if( 0 == strlen(policyData.c_str()) )
                {
                   std::string appwizardfile = localconfigurator.getInstallPath() + "/etc/appwizard.conf";
                   std::ofstream out;
                   out.open(appwizardfile.c_str(), std::ios::out);
                   if (!out)
                   {
                      DebugPrintf(SV_LOG_ERROR,"Failed to open %s file\n", appwizardfile.c_str());
                      DebugPrintf(SV_LOG_ERROR,"appwizard.conf file doesnot exist\n");
                   }
                   else
                   {
                      DebugPrintf(SV_LOG_DEBUG,"Emptying the contents of the file %s\n",appwizardfile.c_str());
                      out << " ";
                      out.close();
                   }
                }
            }

            if (m_OracleAppPtr->discoverApplication() == SVS_OK)
            {
                if( shouldUpdateCx )
                {
                    PostDiscoveryInfoToCx();
                }

                bRet = SVS_OK;
            }
            else
            {
                OracleDiscoveryInfo oracleDiscInfo;
                fillPolicyInfoInfo(oracleDiscInfo.m_policyInfo);
                oracleDiscInfo.m_policyInfo.insert(std::make_pair("ErrorCode", "1")) ;
                oracleDiscInfo.m_policyInfo.insert(std::make_pair("ErrorString", "Oracle application discovery failed"));

                DiscoveryControllerPtr discCtrlPtr = DiscoveryController::getInstance() ;
                discCtrlPtr->m_disCoveryInfo->oraclediscoveryInfo.push_back(oracleDiscInfo);
            }
        }
    }while (0);
    DebugPrintf(SV_LOG_DEBUG, "EXITED OracleDiscoveryAndValidator::%s\n", FUNCTION_NAME) ;
    return bRet ;
}


SVERROR OracleDiscoveryAndValidator::PerformReadinessCheckAtSource()
{
    SVERROR bRet = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;

    ReadinessCheck readyCheck;
    std::list<ValidatorPtr> validations ;
    std::list<ReadinessCheck> readinessChecksList;
    RuleEnginePtr enginePtr = RuleEngine::getInstance() ;
    ReadinessCheckInfo readinessCheckInfo ;
    DiscoveryControllerPtr discCtrlPtr = DiscoveryController::getInstance() ;
    bool isLocalContext = false;    

    m_OracleAppPtr.reset( new OracleApplication() ) ;
    m_OracleAppPtr->discoverApplication();

    SV_ULONGLONG instanceId = AppScheduler::getInstance()->getInstanceId(m_policyId);

    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
    configuratorPtr->PolicyUpdate(m_policyId,instanceId,"InProgress","");

    std::list<InmRuleIds> failedRules ;
    bool tagVerifyRule = false;

    DebugPrintf(SV_LOG_DEBUG, "App instanceName : '%s' \n", m_appInstanceName.c_str()) ;

    OracleAppVersionDiscInfo appVerInfo;
    if( m_OracleAppPtr->m_oracleDiscoveryInfoMap.find(m_appInstanceName) != m_OracleAppPtr->m_oracleDiscoveryInfoMap.end() )
    {
        appVerInfo = m_OracleAppPtr->m_oracleDiscoveryInfoMap[m_appInstanceName] ;
        DebugPrintf(SV_LOG_DEBUG, "App instanceName '%s' found.\n", m_appInstanceName.c_str()) ;
    }

    if ((m_scenarioType == SCENARIO_PROTECTION) && (m_scheduleType != 1))
    {
        isLocalContext = true;    
    }
    validations = enginePtr->getOracleProtectionSourceRules(appVerInfo, m_appVacpOptions, isLocalContext);

    std::list<ValidatorPtr>::iterator validateIter = validations.begin() ;
    while(validateIter != validations.end() )
    {
        /*
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
        */

        (* validateIter)->setFailedRules(failedRules) ;
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

    if ((m_solutionType.compare("PRISM") == 0) && (m_scenarioType == SCENARIO_PROTECTION) && (m_scheduleType != 1))
    {

        std::string marshalledSrcReadinessCheckString = m_policy.m_policyData ;

        DebugPrintf( SV_LOG_DEBUG, "Policy Data : '%s' \n", marshalledSrcReadinessCheckString.c_str()) ;

        try
        {
            SourceReadinessCheckInfo srcReadinessCheckInfo;
            std::map<std::string, std::string> readinessData = unmarshal<std::map<std::string, std::string> >(marshalledSrcReadinessCheckString);

            if( readinessData.find( "dummyLunCheck" ) !=  readinessData.end() )
            {
                ApplianceTargetLunCheckInfo atLunzCheckInfo;

                try
                {
                    std::string marshalledDummyLunzCheckInfo = readinessData.find( "dummyLunCheck" )->second;

                    DebugPrintf( SV_LOG_DEBUG, "Marshalled Dummy Lunz data : '%s'\n", marshalledDummyLunzCheckInfo.c_str()) ;

                    atLunzCheckInfo = unmarshal<ApplianceTargetLunCheckInfo>(marshalledDummyLunzCheckInfo );

                    srcReadinessCheckInfo.m_atLunCheckInfo = atLunzCheckInfo;
                }
                catch(std::exception& ex)
                {
                    DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling dummyLunz %s\n", ex.what()) ;
                    return SVS_FALSE ;
                }
                catch(...)
                {
                    DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling dummyLunz\n") ;
                    return SVS_FALSE ;
                }

                validations = enginePtr->getPrismProtectionSourceRules(srcReadinessCheckInfo) ;
                
                validateIter = validations.begin() ;
                while(validateIter != validations.end() )
                {
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
            else
            {
                DebugPrintf(SV_LOG_ERROR, "Dummy lunz information is not found in oracle source readiness check data.\n") ;
                return SVS_FALSE ;
            }
        }
        catch(std::exception& ex)
        {
            DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling readiness check%s\n", ex.what()) ;
            return SVS_FALSE ;
        }
        catch(...)
        {
            DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling readiness check\n") ;
            return SVS_FALSE ;
        }
    }

	if(!readinessChecksList.empty())
    {
        readinessCheckInfo.propertiesList.insert(std::make_pair("PolicyId", boost::lexical_cast<std::string>(m_policyId))) ;
        readinessCheckInfo.propertiesList.insert(std::make_pair("InstanceId",boost::lexical_cast<std::string>(instanceId)));
        readinessCheckInfo.validationsList = readinessChecksList ;
        discCtrlPtr->m_ReadinesscheckInfoList.push_back(readinessCheckInfo);
    }

    bRet = SVS_OK ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR OracleDiscoveryAndValidator::PerformReadinessCheckAtTarget()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_OK;
    bool isLocalContext = true;    
    
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    DiscoveryControllerPtr discCtrlPtr = DiscoveryController::getInstance() ;
    std::list<ReadinessCheck> readinessChecksList;
    std::list<ReadinessCheckInfo> readinesscheckInfos; 

    OracleTargetReadinessCheckInfo oracleTgtReadinessCheckInfo;
    RuleEnginePtr enginePtr = RuleEngine::getInstance() ;
    ReadinessCheckInfo readinessCheckInfo ;
    std::list<ValidatorPtr> validations ;

    m_OracleAppPtr.reset( new OracleApplication() ) ;
    m_OracleAppPtr->discoverApplication();

    readinessChecksList.clear() ;
    std::string marshalledTgtReadinessCheckString = m_policy.m_policyData ;

    AppAgentConfiguratorPtr appConfiguratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
    try
    {
        DebugPrintf( SV_LOG_DEBUG, "Target Readiness Info: '%s' \n", marshalledTgtReadinessCheckString.c_str()) ;

        std::map<std::string, std::string> targetData = unmarshal<std::map<std::string, std::string> >(marshalledTgtReadinessCheckString);

        if( targetData.find( "isTargetServer" ) !=  targetData.end() )
        {
            std::string targetServer = targetData.find("isTargetServer")->second;

            DebugPrintf( SV_LOG_DEBUG, "Target Server : '%s' \n", targetServer.c_str()) ;

            if (targetServer.compare("NO") == 0)
            { 
                isLocalContext = false;
            }
        }

        if ((m_scenarioType == SCENARIO_PROTECTION) && (m_scheduleType != 1))
        {
            if (m_context == CONTEXT_HADR)
            {
                if( targetData.find( "srcDbInfo" ) !=  targetData.end() )
                {
                    try
                    {
                        std::string marshalledDbInfoString = targetData.find("srcDbInfo")->second;

                        DebugPrintf( SV_LOG_DEBUG, "Source DB Info: '%s' \n", marshalledDbInfoString.c_str()) ;

                        std::list<std::map<std::string, std::string> > dbInfo = unmarshal<std::list<std::map<std::string, std::string> > >(marshalledDbInfoString);
                        oracleTgtReadinessCheckInfo.m_oracleSrcInstances = dbInfo;
                    }
                    catch(std::exception& ex)
                    {
                        DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling src dbinfo : %s\n", ex.what()) ;
                        return SVS_FALSE ;
                    }
                    catch(...)
                    {
                        DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling src dbinfo\n") ;
                        return SVS_FALSE ;
                    }
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "src dbinfo is missing target readiness data\n") ;
                    return SVS_FALSE ;
                }
            }

            if( targetData.find( "tgtDbInfo" ) !=  targetData.end() )
            {
                try
                {
                    std::string marshalledDbInfoString = targetData.find("tgtDbInfo")->second;

                    DebugPrintf( SV_LOG_DEBUG, "Target DB Info: '%s' \n", marshalledDbInfoString.c_str()) ;

                    std::list<std::map<std::string, std::string> > dbInfo = unmarshal<std::list<std::map<std::string, std::string> > >(marshalledDbInfoString);
                    std::list<std::map<std::string, std::string> >::iterator dbInfoIter = dbInfo.begin();
                    std::list<std::map<std::string, std::string> > tgtDbInfo;
                    while(dbInfoIter != dbInfo.end())
                    {
                        std::string dbInstanceName = dbInfoIter->find("InstanceName")->second;
                        std::map<std::string, std::string> dbInstanceInfo;
                        dbInstanceInfo.insert(std::make_pair("dbName", dbInstanceName));

                        if( m_OracleAppPtr->m_oracleDiscoveryInfoMap.find(dbInstanceName) != m_OracleAppPtr->m_oracleDiscoveryInfoMap.end() )
                        {
                            DebugPrintf(SV_LOG_DEBUG, "App instance '%s' online.\n", dbInstanceName.c_str()) ;
                            dbInstanceInfo.insert(std::make_pair("Status", "Online"));
                        }
                        else
                        {
                            DebugPrintf(SV_LOG_DEBUG, "App instance '%s' online.\n", dbInstanceName.c_str()) ;
                            dbInstanceInfo.insert(std::make_pair("Status", "Offline"));
                        }

                        tgtDbInfo.push_back(dbInstanceInfo);

                        dbInfoIter++;
                    }

                    oracleTgtReadinessCheckInfo.m_oracleTgtInstances = tgtDbInfo;
                }
                catch(std::exception& ex)
                {
                    DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling dbinfo : %s\n", ex.what()) ;
                    return SVS_FALSE ;
                }
                catch(...)
                {
                    DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling dbinfo\n") ;
                    return SVS_FALSE ;
                }
            }

            if( targetData.find( "tgtVolInfo" ) !=  targetData.end() )
            {
                try
                {
                    std::string marshalledVolInfoString = targetData.find("tgtVolInfo")->second;

                    DebugPrintf( SV_LOG_DEBUG, "Volume Info: '%s' \n", marshalledVolInfoString.c_str()) ;
                    std::list<std::map<std::string, std::string> > volInfo = unmarshal<std::list<std::map<std::string, std::string> > >(marshalledVolInfoString);
                    oracleTgtReadinessCheckInfo.m_tgtVolumeInfo = volInfo;
                }
                catch(std::exception& ex)
                {
                    DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling volinfo : %s\n", ex.what()) ;
                    return SVS_FALSE ;
                }
                catch(...)
                {
                    DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling volinfo\n") ;
                    return SVS_FALSE ;
                }
            }
            
            if( targetData.find( "volumeCapacities" ) !=  targetData.end() )
            {
                try
                {
                    std::string marshalledVolCapString = targetData.find("volumeCapacities")->second;

                    DebugPrintf( SV_LOG_DEBUG, "Capacities Info: '%s' \n", marshalledVolCapString.c_str()) ;
                    std::list<std::map<std::string, std::string> > capInfo = unmarshal<std::list<std::map<std::string, std::string> > >(marshalledVolCapString);
                    oracleTgtReadinessCheckInfo.m_volCapacities= capInfo;
                }
                catch(std::exception& ex)
                {
                    DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling capacities info : %s\n", ex.what()) ;
                    return SVS_FALSE ;
                }
                catch(...)
                {
                    DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling capacities info\n") ;
                    return SVS_FALSE ;
                }
            }
        }
    }
    catch(std::exception& ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling target readiness info: %s\n", ex.what()) ;
        return SVS_FALSE ;
    }
    catch(...)
    {
        DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling target readiness info\n") ;
        return SVS_FALSE ;
    }

    SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
    configuratorPtr->PolicyUpdate(m_policyId,instanceId,"InProgress","");

    validations = enginePtr->getOracleProtectionTargetRules(oracleTgtReadinessCheckInfo, m_totalNumberOfPairs, isLocalContext);

    std::list<ValidatorPtr>::iterator validateIter = validations.begin() ;
    std::list<InmRuleIds> failedRules ;
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

    readinessCheckInfo.propertiesList.insert(std::make_pair("PolicyId", boost::lexical_cast<std::string>(m_policyId))) ;
    readinessCheckInfo.propertiesList.insert(std::make_pair("InstanceId",boost::lexical_cast<std::string>(instanceId)));
    readinessCheckInfo.validationsList = readinessChecksList ;

    discCtrlPtr->m_ReadinesscheckInfoList.push_back(readinessCheckInfo) ;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR OracleDiscoveryAndValidator::UnregisterDB()
{
    SVERROR bRet = SVS_OK ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED OracleDiscoveryAndValidator::%s\n", FUNCTION_NAME) ;
    std::string marshalledUnregisterDBInfo = m_policy.m_policyData ;

    AppAgentConfiguratorPtr appConfiguratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    SV_ULONG policyId = boost::lexical_cast<SV_ULONG>(m_policy.m_policyProperties.find("PolicyId")->second);
    DebugPrintf( SV_LOG_DEBUG, "Policy Id received : %d\n",policyId);
    SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(policyId);
    std::string outputFileName = getOutputFileName(policyId);
    schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_STARTED) ;
    AppLocalConfigurator localconfigurator ;
    std::string installPath = localconfigurator.getInstallPath();
    appConfiguratorPtr->PolicyUpdate(policyId,instanceId,"InProgress","InProgress");
    std::stringstream stream;
    try
    {
       DebugPrintf( SV_LOG_DEBUG, "Unregistered DB Info :%s\n",marshalledUnregisterDBInfo.c_str());
       UnregisterInfo unregInfo = unmarshal<UnregisterInfo>(marshalledUnregisterDBInfo);
       std::string appwizardfile = installPath + "/etc/appwizard.conf";
    
       DebugPrintf( SV_LOG_DEBUG, "Unregistered DBs are : \n");
       std::ifstream in;
       std::ofstream out;
       std::string readline;
       std::string newfile = "tmp/appwizard.con";

       in.open(appwizardfile.c_str(), std::ios::in);
       out.open(newfile.c_str(),std::ios::out);

       if (!in)
       {
          DebugPrintf(SV_LOG_ERROR,"Failed to open %s file\n", appwizardfile.c_str());
          bRet = SVS_FALSE;
       }
      else
      {
         if(!out)
         {
           DebugPrintf(SV_LOG_ERROR,"Failed to open %s file\n", newfile.c_str());
           bRet = SVS_FALSE;
         }
         else
         {
          while(out.good())
          {
            getline(in,readline);
            std::string dbname=readline.substr(readline.find_first_of(":")+1);
            DebugPrintf( SV_LOG_DEBUG," Existing Oracle Databases : %s\n",dbname.c_str());

            std::list<std::string>::iterator dbitr = unregInfo.m_InstancesList.begin();
            std::string newline;
            while(dbitr != unregInfo.m_InstancesList.end())
            {
               if(strcmpi((*dbitr).c_str(),dbname.c_str()) == 0)
               {
                 DebugPrintf( SV_LOG_DEBUG," Oracle Database '%s' is already existing in appwizard.conf \n",dbname.c_str());
                 newline = "OracleDatabase=NO:" + (*dbitr);
               }
               else
               {
                 newline = readline ;
               }
               dbitr++;
            }
            out << newline << std::endl;
          }
          out.close();
          in.close();
        }
      }
    }
    catch ( ContextualException& ce )
    {
       DebugPrintf(SV_LOG_ERROR,"\n%s encountered exception %s.\n", FUNCTION_NAME, ce.what());
       DebugPrintf(SV_LOG_ERROR,"Got unmarshal exception for %s\n",marshalledUnregisterDBInfo.c_str());
    }
    catch(std::exception ex)
    {
       DebugPrintf(SV_LOG_ERROR, "Failed to unmarshal UnregisterApplication Data %s\n for the policy %ld", ex.what(),m_policyId) ;
       DebugPrintf(SV_LOG_ERROR,"Got unmarshal exception for %s\n",marshalledUnregisterDBInfo.c_str());
    }
    if(bRet == SVS_OK)
    {
       stream << "Oracle Database Unregistration is successful." << std::endl;
       WriteStringIntoFile(outputFileName, stream.str());
       appConfiguratorPtr->UpdateUnregisterApplicationInfo(policyId,instanceId,"Success",std::string("Unregistration of Oracle database is Completed"));
    }
    else
    {
       stream << "Oracle Database Unregistration Failed." << std::endl;
       WriteStringIntoFile(outputFileName, stream.str());
       appConfiguratorPtr->UpdateUnregisterApplicationInfo(policyId,instanceId,"Failed",std::string("Unregistration of Oracle database is not successful"));
    }
    schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_COMPLETED) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED OracleDiscoveryAndValidator::%s\n", FUNCTION_NAME) ;
    return bRet ;
}

