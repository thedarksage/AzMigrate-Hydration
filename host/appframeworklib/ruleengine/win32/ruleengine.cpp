#include "ruleengine.h"
#include "ruleengine/validator.h"
#include "rulefactory.h"


RuleEnginePtr RuleEngine::m_ruleEnginePtr ;


RuleEnginePtr RuleEngine::getInstance()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    if( m_ruleEnginePtr.get() == NULL )
    {
        m_ruleEnginePtr.reset( new RuleEngine() ) ;
        Validator::CreateNameDescriptionLookupMap();
		Validator::CreateErrorLookupMap();
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return m_ruleEnginePtr ;
}


std::list<ValidatorPtr> RuleEngine::getSystemRules(enum SCENARIO_TYPE scenarioType,
												   enum CONTEXT_TYPE contextType,
												   const std::string& appTye,
												   bool bDnsFailover,
                                                   std::string srchostnameforPermissionChecks,
                                                   const std::string& tgthostnameforPermissionChecks,
                                                   bool bAtTarget,
												   bool isLocalContext)
{
    return RuleFactory::getSystemRules(scenarioType, 
                                                            contextType, 
															appTye,
															bDnsFailover,
                                                            srchostnameforPermissionChecks, 
                                                            tgthostnameforPermissionChecks, 
                                                            bAtTarget, 
                                                            isLocalContext) ;
}

std::list<ValidatorPtr> RuleEngine::getMSSQLInstanceRules(const MSSQLInstanceMetaData& instanceMetaData, 
															enum SCENARIO_TYPE scenarioType,
															enum CONTEXT_TYPE contextType,														  
															const std::string& sysDrive,
															const std::string &strVacpOptions, 
															bool isLocalContext)
{
	return RuleFactory::getMSSQLInstanceRules(instanceMetaData, scenarioType, contextType, sysDrive, strVacpOptions, isLocalContext) ;
}
std::list<ValidatorPtr> RuleEngine::getMSSQLVersionLevelRules(InmProtectedAppType m_type, 
															  enum SCENARIO_TYPE scenarioType,
															  enum CONTEXT_TYPE contextType,															  
															  bool isLocalContext)
{   
    return RuleFactory::getMSSQLVersionLevelRules(m_type, scenarioType, contextType, isLocalContext) ;
}
void RuleEngine::RunMSSQLPreRequisiteChecks(std::list<ValidatorPtr>& mssqlPreReqValidatorsList )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<ValidatorPtr>::const_iterator validatorIter = mssqlPreReqValidatorsList.begin();
    while( validatorIter != mssqlPreReqValidatorsList.end() )
    {
        if( (*validatorIter)->evaluate() != SVS_OK )
        {
            DebugPrintf(SV_LOG_ERROR, "Validation Failed\n") ;
            if( (*validatorIter)->fix() != SVS_OK )
            {
                std::string notes ;
                std::cout<<notes << std::endl ;
            }
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "Validation Passed\n") ;
        }
        validatorIter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

std::list<ValidatorPtr> RuleEngine::getFileServerRules(enum SCENARIO_TYPE scenarioType,
													   enum CONTEXT_TYPE contextType,
													   bool isLocalContext)
{
    return RuleFactory::getFileServerRules(scenarioType, contextType, isLocalContext);
}
std::list<ValidatorPtr> RuleEngine::getFileServerInstanceLevelRules( const FileServerInstanceMetaData& instanceMetaData,
																	enum SCENARIO_TYPE scenarioType,
																	enum CONTEXT_TYPE contextType,
																	const std::string& sysDrive,
																	const std::string& strVacpOptions,
																	bool isLocalContext )
{
	return RuleFactory::getFileServerInstanceLevelRules(instanceMetaData, scenarioType, contextType, sysDrive, strVacpOptions, isLocalContext) ;
}
std::list<ValidatorPtr> RuleEngine::getBESRules(bool isLocalContext)
{
    return RuleFactory::getBESRules(isLocalContext);
}
std::list<ValidatorPtr> RuleEngine::getNormalProtectionSourceRules(const std::string& strVacpOptions, bool isLocalContext)
{
	return RuleFactory::getNormalProtectionSourceRules(strVacpOptions, isLocalContext);
}
SVERROR RuleEngine::RunRule(ValidatorPtr validator)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;

    if( validator->evaluate() != SVS_OK )
    {
        DebugPrintf(SV_LOG_DEBUG, "Validation Failed and Trying to fix it.\n") ;
        validator->fix() ;
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "Validation Passed\n") ;
        bRet = SVS_OK ;
    }
    DebugPrintf(SV_LOG_DEBUG, "Rule Name         :%s\n", Validator::GetNameAndDescription(validator->getRuleId()).second.c_str()) ;
    DebugPrintf(SV_LOG_DEBUG, "Rule Description  :%s\n", Validator::GetNameAndDescription(validator->getRuleId()).first.c_str()) ;
    DebugPrintf(SV_LOG_DEBUG, "Rule Status       :%d\n", validator->getStatus()) ;
    DebugPrintf(SV_LOG_DEBUG, "Rule Status Desc  :%s\n", validator->statusToString().c_str()) ;
    DebugPrintf(SV_LOG_DEBUG, "Rule errorCode  :%d \n", validator->getRuleExitCode()) ;
    DebugPrintf(SV_LOG_DEBUG, "Rule Message :%s \n", validator->getRuleMessage().c_str()) ;
    DebugPrintf(SV_LOG_DEBUG, "Rule error Descrption :%s \n", Validator::getErrorString( validator->getRuleExitCode() ).c_str()) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
SVERROR RuleEngine::RunRules(std::list<ValidatorPtr>& validations)
{
	SVERROR bRet = SVS_FALSE ;
	std::list<ValidatorPtr>::iterator validateIter = validations.begin() ;
	while(validateIter != validations.end() )
	{
		bRet = RunRule(*validateIter) ;
		validateIter ++ ;
	}
	return bRet;
}
std::list<ValidatorPtr> RuleEngine::getExchangeRules(const ExchAppVersionDiscInfo& info, 
                                                     const ExchangeMetaData& metadata,
													 enum SCENARIO_TYPE scenarioType,
													 enum CONTEXT_TYPE contextType,
                                                     const std::string& systemDrive,
                                                      /*const*/ std::string &vacpCommand,
                                                     const std::string& hostname,
                                                     bool isLocalContext)
{
    return RuleFactory::getExchangeRules(info, metadata, scenarioType, contextType, systemDrive,vacpCommand,hostname,isLocalContext) ;
}
std::list<ValidatorPtr> RuleEngine::getExchange2010Rules(const ExchAppVersionDiscInfo& info, 
                                                         const Exchange2k10MetaData& metadata,
														 enum SCENARIO_TYPE scenarioType,
														 enum CONTEXT_TYPE contextType,
                                                         const std::string& systemDrive,
														 std::string &vacpCommand,
                                                         const std::string& hostname,
                                                         bool isLocalContext)
{
    return RuleFactory::getExchange2010Rules(info, metadata, scenarioType, contextType, systemDrive, vacpCommand, hostname, isLocalContext) ;
}
std::list<ValidatorPtr> RuleEngine::getTargetRedinessRules(const ExchangeTargetReadinessCheckInfo& srcInfo, 
                                                           const ExchangeTargetReadinessCheckInfo& tgtInfo,
															enum SCENARIO_TYPE scenarioType,
															enum CONTEXT_TYPE contextType,
															const SV_ULONGLONG& totalNumberOfPairs,
                                                           bool isLocalContext)
{
    return RuleFactory::getTargetRedinessRules(srcInfo, tgtInfo, scenarioType, contextType, totalNumberOfPairs, isLocalContext) ;
}
std::list<ValidatorPtr> RuleEngine::getMSSQLTargetRedinessRules(MSSQLTargetReadinessCheckInfo& srcData, 
																MSSQLTargetReadinessCheckInfo& tgtData,
																enum SCENARIO_TYPE scenarioType,
																enum CONTEXT_TYPE contextType,
																const SV_ULONGLONG& totalNumberOfPairs,
																bool isLocalContext)
{
    return RuleFactory::getMSSQLTargetRedinessRules(srcData, tgtData, scenarioType, contextType, totalNumberOfPairs, isLocalContext) ;
}

std::list<ValidatorPtr> RuleEngine::getFileServerTargetRedinessRules( FileServerTargetReadinessCheckInfo& srcData,
																	 enum SCENARIO_TYPE scenarioType,
																	 enum CONTEXT_TYPE contextType,
																	 const SV_ULONGLONG& totalNumberOfPairs,
																	 bool isLocalContext )
{
	return RuleFactory::getFileServerTargetRedinessRules(srcData, scenarioType, contextType, totalNumberOfPairs, isLocalContext) ;
}

std::list<ValidatorPtr> RuleEngine::getNormalProtectionTargetRules(const TargetReadinessCheckInfo &tgtInfo,
																   const SV_ULONGLONG& totalNumberOfPairs,
																	bool isLocalContext)
{
	return RuleFactory::getNormalProtectionTargetRules(tgtInfo, totalNumberOfPairs, isLocalContext) ;
}
//extern "C" __declspec(dllexport) RuleEngine* getRuleEngine() ;
RuleEngine* getRuleEngine() 
{
    RuleEnginePtr ptr = RuleEngine::getInstance() ;
    return ptr.get() ;
}
