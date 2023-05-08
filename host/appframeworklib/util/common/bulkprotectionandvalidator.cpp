#include "bulkprotectionandvalidator.h"
#include "config/appconfigurator.h"
#include "ruleengine.h"
#include "controller.h"
#include <boost/lexical_cast.hpp>
#include "util.h"

BulkProtectionAndValidator::BulkProtectionAndValidator(const ApplicationPolicy& policy)
: DiscoveryAndValidator(policy)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

SVERROR BulkProtectionAndValidator::PerformReadinessCheckAtSource()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retStatus = SVS_FALSE ;

	SV_ULONGLONG instanceId = AppScheduler::getInstance()->getInstanceId(m_policyId);
	AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
	bool bRunConsistencyRule = true;

	DiscoveryControllerPtr discCtrlPtr = DiscoveryController::getInstance() ;
	ReadinessCheck readyCheck;
	RuleEnginePtr enginePtr = RuleEngine::getInstance() ;
	ReadinessCheckInfo readinessCheckInfo ;
	std::list<ReadinessCheck> readinessChecksList;
	std::list<ValidatorPtr> validations ;

#ifdef SV_WINDOWS
	if(isClusterNode())
	{
		std::string volumePathsString;
		std::string::size_type index1 = m_appVacpOptions.find("-v");
		if(index1 == std::string::npos)
		{
			index1 =  m_appVacpOptions.find("-V");
		}
		if(index1 != std::string::npos)
		{
            index1 += 2 ; 
			std::string::size_type index2 = m_appVacpOptions.find("-", index1);
			if(index2 != std::string::npos)
			{
				int noOfElements = ( index2 - 1 )   - index1;
				volumePathsString = m_appVacpOptions.substr(index1, noOfElements);			
			}
			else
			{
				volumePathsString = m_appVacpOptions.substr(index1, m_appVacpOptions.size());
			}
		}
		else
		{
			//invalid...
		}
        trimSpaces( volumePathsString) ;
        DebugPrintf( SV_LOG_DEBUG, "Volumes: %s \n", volumePathsString.c_str() ) ;
		std::list<std::string> volumePathList = tokenizeString(volumePathsString, ";");
		std::list<std::string>::iterator volumePathListIter;
		if(volumePathList.empty() == false)
		{
			bRunConsistencyRule  = CheckVolumesAccess(volumePathList) ;			
		}
	}
#endif
    if( bRunConsistencyRule  )
    {
        configuratorPtr->PolicyUpdate(m_policyId,instanceId,"InProgress","");
    }

	validations = enginePtr->getNormalProtectionSourceRules(m_appVacpOptions) ;
	std::list<ValidatorPtr>::iterator validateIter = validations.begin() ;
	while(validateIter != validations.end())
	{
		ReadinessCheck readyCheck ;			
        if ( (*validateIter)->getRuleId() != CONSISTENCY_TAG_CHECK ||
               ( bRunConsistencyRule == true ) )
        {
            enginePtr->RunRule(*validateIter) ;
            (*validateIter)->convertToRedinessCheckStruct(readyCheck);
            readinessChecksList.push_back(readyCheck) ;			
        }		
		validateIter ++ ;
	}

#ifdef SV_UNIX
    if ((m_solutionType.compare("PRISM") == 0) && (m_scenarioType == SCENARIO_PROTECTION) && (m_scheduleType != 1))
    {

        std::string marshalledSrcReadinessCheckString = m_policy.m_policyData ;

        DebugPrintf( SV_LOG_DEBUG, "Policy Data : '%s' \n", marshalledSrcReadinessCheckString.c_str()) ;

        try
        {
            std::map<std::string, std::string> readinessData = unmarshal<std::map<std::string, std::string> >(marshalledSrcReadinessCheckString);

            if( readinessData.find( "dummyLunCheck" ) !=  readinessData.end() )
            {

                SourceReadinessCheckInfo srcReadinessCheckInfo;
                ApplianceTargetLunCheckInfo atLunzCheckInfo;

                try
                {
                    std::string marshalledDummyLunzCheckInfo = readinessData.find( "dummyLunCheck" )->second;

                    DebugPrintf( SV_LOG_DEBUG, "Marshalled Dummy Lunz : '%s'\n", marshalledDummyLunzCheckInfo.c_str()) ;

                    atLunzCheckInfo = unmarshal<ApplianceTargetLunCheckInfo>(marshalledDummyLunzCheckInfo );
                }
                catch(std::exception& ex)
                {
                    DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling dummyLunz %s\n", ex.what()) ;
                }
                catch(...)
                {
                    DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling\n") ;
                    return SVS_FALSE ;
                }

                srcReadinessCheckInfo.m_atLunCheckInfo = atLunzCheckInfo;
                validations = enginePtr->getPrismProtectionSourceRules(srcReadinessCheckInfo) ;
                
                validateIter = validations.begin() ;
                while(validateIter != validations.end() )
                {
                    enginePtr->RunRule(*validateIter) ;
                    ReadinessCheck readyCheck ;
                    (*validateIter)->convertToRedinessCheckStruct(readyCheck);
                    readinessChecksList.push_back(readyCheck) ;
                    validateIter ++ ;
                }
            }
        }
        catch(std::exception& ex)
        {
            DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling policy data %s\n", ex.what()) ;
        }
        catch(...)
        {
            DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling\n") ;
            return SVS_FALSE ;
        }

    }
#endif

	if(!readinessChecksList.empty())
	{
		readinessCheckInfo.propertiesList.insert(std::make_pair("PolicyId", boost::lexical_cast<std::string>(m_policyId))) ;
		readinessCheckInfo.propertiesList.insert(std::make_pair("InstanceId",boost::lexical_cast<std::string>(instanceId)));
		readinessCheckInfo.validationsList = readinessChecksList ;
		discCtrlPtr->m_ReadinesscheckInfoList.push_back(readinessCheckInfo) ;
        retStatus = SVS_OK ;
	}
    else
    {
        retStatus = SVS_FALSE;
    }

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus ;
}

SVERROR BulkProtectionAndValidator::PerformReadinessCheckAtTarget()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retStatus = SVS_FALSE ;
    bool isLocalContext = true;
    
	DiscoveryControllerPtr discCtrlPtr = DiscoveryController::getInstance() ;
    std::list<ReadinessCheck> readinessChecksList;
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    SV_ULONGLONG instanceId = schedulerPtr->getInstanceId(m_policyId);
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;

	configuratorPtr->PolicyUpdate(m_policyId,instanceId,"InProgress","");
    std::list<ReadinessCheckInfo> readinesscheckInfos; 

    TargetReadinessCheckInfo tgtReadinessCheckInfo;
    std::string marshalledTgtReadinessCheckString = m_policy.m_policyData ;
    AppAgentConfiguratorPtr appConfiguratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
    try
    {
        DebugPrintf( SV_LOG_DEBUG, "Target Readiness Info: '%s' \n", marshalledTgtReadinessCheckString.c_str()) ;

        std::map<std::string, std::string> targetData = unmarshal<std::map<std::string, std::string> >(marshalledTgtReadinessCheckString);

        if( targetData.find( "isTargetServer" ) !=  targetData.end() )
        {
            std::string targetServer = targetData.find("isTargetServer")->second;

            DebugPrintf( SV_LOG_DEBUG, "Target Server: '%s' \n", targetServer.c_str()) ;

            if (targetServer.compare("NO") == 0)
            {
                isLocalContext = false;
            }
        }

        if (m_scenarioType == SCENARIO_PROTECTION)
        {

            if( targetData.find( "tgtVolInfo" ) !=  targetData.end() )
            {
                try
                {
                    std::string marshalledVolInfoString = targetData.find("tgtVolInfo")->second;

                    DebugPrintf( SV_LOG_DEBUG, "Volume Info: '%s' \n", marshalledVolInfoString.c_str()) ;
                    std::list<std::map<std::string, std::string> > volInfo = unmarshal<std::list<std::map<std::string, std::string> > >(marshalledVolInfoString);
                    tgtReadinessCheckInfo.m_tgtVolumeInfo = volInfo;
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
                    tgtReadinessCheckInfo.m_volCapacities= capInfo;
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
        DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling %s\n", ex.what()) ;
        return SVS_FALSE ;
    }
    catch(...)
    {
        DebugPrintf(SV_LOG_ERROR, "Error while unmarshalling\n") ;
        return SVS_FALSE ;
    }
	
    std::list<ValidatorPtr> validations ;
	std::list<ValidatorPtr>::iterator validateIter;
	RuleEnginePtr enginePtr = RuleEngine::getInstance() ;
	validations = enginePtr->getNormalProtectionTargetRules(tgtReadinessCheckInfo, m_totalNumberOfPairs, isLocalContext) ;
	
	validateIter = validations.begin() ;
	while(validateIter != validations.end() )
	{
		enginePtr->RunRule(*validateIter) ;
		ReadinessCheck readyCheck ;
		(*validateIter)->convertToRedinessCheckStruct(readyCheck);
		readinessChecksList.push_back(readyCheck) ;
		validateIter ++ ;
	}
    
    ReadinessCheckInfo readinessCheckInfo ;
    readinessCheckInfo.propertiesList.insert(std::make_pair("PolicyId", boost::lexical_cast<std::string>(m_policyId))) ;
    readinessCheckInfo.propertiesList.insert(std::make_pair("InstanceId",boost::lexical_cast<std::string>(instanceId)));
    readinessCheckInfo.validationsList = readinessChecksList ;

    discCtrlPtr->m_ReadinesscheckInfoList.push_back(readinessCheckInfo) ;
	
	retStatus = SVS_OK ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus ;
}
