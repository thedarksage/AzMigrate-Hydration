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

std::list<ValidatorPtr> RuleEngine::getNormalProtectionSourceRules(const std::string& strVacpOptions, bool isLocalContext)
{
	return RuleFactory::getNormalProtectionSourceRules(strVacpOptions, isLocalContext);
}

std::list<ValidatorPtr> RuleEngine::getNormalProtectionTargetRules(const TargetReadinessCheckInfo &tgtInfo,
                                                                    const SV_ULONGLONG& totalNumberOfPairs,
																	bool isLocalContext)
{
	return RuleFactory::getNormalProtectionTargetRules(tgtInfo, totalNumberOfPairs, isLocalContext) ;
}

std::list<ValidatorPtr> RuleEngine::getOracleProtectionSourceRules(const OracleAppVersionDiscInfo& appInfo, const std::string strVacpoptions, bool isLocalContext)
{

	return RuleFactory::getOracleProtectionSourceRules(appInfo, strVacpoptions, isLocalContext);

}

std::list<ValidatorPtr> RuleEngine::getDb2ProtectionSourceRules(const Db2AppInstanceDiscInfo& appInfo,std::string instanceName,std::string appDatabaseName, const std::string strVacpoptions)
{
    return RuleFactory::getDb2ProtectionSourceRules(appInfo, instanceName, appDatabaseName, strVacpoptions);
}

std::list<ValidatorPtr> RuleEngine::getOracleProtectionTargetRules(OracleTargetReadinessCheckInfo &readinessCheckData, 
                                                                   const SV_ULONGLONG &totalNumberOfPairs, 
                                                                   bool isLocalContext)
{

    return RuleFactory::getOracleProtectionTargetRules(readinessCheckData, totalNumberOfPairs, isLocalContext);

}

std::list<ValidatorPtr> RuleEngine::getDb2ProtectionTargetRules(Db2TargetReadinessCheckInfo &readinessCheckData,
                                                                           const SV_ULONGLONG &totalNumberOfPairs,
                                                                           bool isLocalContext)
{
   return RuleFactory::getDb2ProtectionTargetRules(readinessCheckData, totalNumberOfPairs, isLocalContext);
}

std::list<ValidatorPtr> RuleEngine::getPrismProtectionSourceRules(SourceReadinessCheckInfo& srcReadinessCheckInfo, bool isLocalContext)
{

    return RuleFactory::getPrismProtectionSourceRules(srcReadinessCheckInfo, isLocalContext);

}

RuleEngine* getRuleEngine() 
{
    RuleEnginePtr ptr = RuleEngine::getInstance() ;
    return ptr.get() ;
}

