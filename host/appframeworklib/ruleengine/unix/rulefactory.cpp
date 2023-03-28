#include "rulefactory.h"
#include "Oraclevalidators.h"
#include "Db2validators.h"
#include "inmagevalidator.h"
#include <list>

std::list<ValidatorPtr> RuleFactory::getNormalProtectionSourceRules(  const std::string& strVacpOptions,
																	  bool isLocalContext)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<ValidatorPtr> preReqvalidatorList ;
    ValidatorPtr validatorPtr ;
    if( isLocalContext )
    {
		std::list<std::string> emptyList;
        validatorPtr.reset( new VolTagIssueValidator( emptyList, strVacpOptions, CONSISTENCY_TAG_CHECK )) ;
        preReqvalidatorList.push_back(validatorPtr) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return preReqvalidatorList ;
	
}

std::list<ValidatorPtr> RuleFactory::getNormalProtectionTargetRules(const TargetReadinessCheckInfo &tgtInfo,
                                                                    const SV_ULONGLONG &totalNumberOfPairs,
																	bool isLocalContext)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    
	std::list<ValidatorPtr> preReqvalidatorList;
	ValidatorPtr validatorPtr ;

    if (isLocalContext)
    {
        validatorPtr.reset(new CacheDirCapacityCheckValidator(CACHE_DIR_CAPACITY_CHECK)) ;
        preReqvalidatorList.push_back(validatorPtr) ;
        validatorPtr.reset(new CMMinSpacePerPairCheckValidator(totalNumberOfPairs, CM_MIN_SPACE_PER_PAIR_CHECK)) ;
        preReqvalidatorList.push_back(validatorPtr) ;

    if (!tgtInfo.m_volCapacities.empty())
    {
        validatorPtr.reset(new VolumeCapacityCheckValidator(tgtInfo.m_volCapacities, VOLUME_CAPACITY_CHECK)) ;
        preReqvalidatorList.push_back(validatorPtr) ;
    }
    }

    if (!tgtInfo.m_tgtVolumeInfo.empty())
    {
        validatorPtr.reset(new TargetVolumeValidator(tgtInfo.m_tgtVolumeInfo, TARGET_VOLUME_IN_USE_CHECK)) ;
        preReqvalidatorList.push_back(validatorPtr) ;
    }

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return preReqvalidatorList ;
}

std::list<ValidatorPtr> RuleFactory::getOracleProtectionSourceRules(const OracleAppVersionDiscInfo& appInfo, const std::string strVacpoptions, bool isLocalContext)
{

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<ValidatorPtr> preReqvalidatorList ;
    ValidatorPtr ptr ;

    ptr.reset(new OracleDatabaseRunValidator(appInfo, DB_INSTALL_CHECK)) ;
    preReqvalidatorList.push_back(ptr) ;

    if( isLocalContext )
    {
        ptr.reset(new OracleConsistencyValidator(appInfo, CONSISTENCY_TAG_CHECK)) ;
        preReqvalidatorList.push_back(ptr) ;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return preReqvalidatorList ;
}

std::list<ValidatorPtr> RuleFactory::getDb2ProtectionSourceRules(const Db2AppInstanceDiscInfo& appInfo, std::string instanceName, std::string appDatabaseName, const std::string strVacpoptions)
{

   DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
   std::list<ValidatorPtr> preReqvalidatorList ;
   ValidatorPtr ptr ;

   ptr.reset(new Db2DatabaseRunValidator(appInfo,instanceName, appDatabaseName, DB_ACTIVE_CHECK)) ;
   preReqvalidatorList.push_back(ptr) ;

   ptr.reset(new Db2ConsistencyValidator(appInfo, DB2_CONSISTENCY_TAG_CHECK)) ;
   preReqvalidatorList.push_back(ptr) ;

   DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
   return preReqvalidatorList ;
}

std::list<ValidatorPtr> RuleFactory::getOracleProtectionTargetRules(OracleTargetReadinessCheckInfo &readinessCheckData, 
                                                                    const SV_ULONGLONG &totalNumberOfPairs, 
                                                                    bool isLocalContext)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<ValidatorPtr> preReqvalidatorList ;
    ValidatorPtr validatorPtr ;

    if (isLocalContext)
    {
        validatorPtr.reset(new CacheDirCapacityCheckValidator(CACHE_DIR_CAPACITY_CHECK)) ;
        preReqvalidatorList.push_back(validatorPtr) ;
        validatorPtr.reset(new CMMinSpacePerPairCheckValidator(totalNumberOfPairs, CM_MIN_SPACE_PER_PAIR_CHECK)) ;
        preReqvalidatorList.push_back(validatorPtr) ;

        if (!readinessCheckData.m_volCapacities.empty())
    {
            validatorPtr.reset(new VolumeCapacityCheckValidator(readinessCheckData.m_volCapacities, VOLUME_CAPACITY_CHECK)) ;
        preReqvalidatorList.push_back(validatorPtr) ;
    }
    }

    if (!readinessCheckData.m_tgtVolumeInfo.empty())
    {
        validatorPtr.reset(new TargetVolumeValidator(readinessCheckData.m_tgtVolumeInfo, TARGET_VOLUME_IN_USE_CHECK)) ;
        preReqvalidatorList.push_back(validatorPtr) ;
    }


    if (!readinessCheckData.m_oracleTgtInstances.empty())
    {
        validatorPtr.reset(new OracleDatabaseShutdownValidator(readinessCheckData.m_oracleTgtInstances, DB_SHUTDOWN_CHECK)) ;
        preReqvalidatorList.push_back(validatorPtr) ;
    }

    if (!readinessCheckData.m_oracleSrcInstances.empty())
    {
        validatorPtr.reset(new OracleDatabaseConfigValidator(readinessCheckData.m_oracleSrcInstances, readinessCheckData.m_tgtVolumeInfo, DB_CONFIG_CHECK)) ;
        preReqvalidatorList.push_back(validatorPtr) ;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return preReqvalidatorList ;
}

std::list<ValidatorPtr> RuleFactory::getDb2ProtectionTargetRules(Db2TargetReadinessCheckInfo &readinessCheckData,
                                                                            const SV_ULONGLONG &totalNumberOfPairs,
                                                                            bool isLocalContext)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<ValidatorPtr> preReqvalidatorList ;
    ValidatorPtr validatorPtr ;

    if (isLocalContext)
    {
       validatorPtr.reset(new CacheDirCapacityCheckValidator(CACHE_DIR_CAPACITY_CHECK)) ;
       preReqvalidatorList.push_back(validatorPtr) ;
       validatorPtr.reset(new CMMinSpacePerPairCheckValidator(totalNumberOfPairs, CM_MIN_SPACE_PER_PAIR_CHECK)) ;
       preReqvalidatorList.push_back(validatorPtr) ;

       if (!readinessCheckData.m_volCapacities.empty())
       {
           validatorPtr.reset(new VolumeCapacityCheckValidator(readinessCheckData.m_volCapacities, VOLUME_CAPACITY_CHECK)) ;
           preReqvalidatorList.push_back(validatorPtr) ;
       }
    }
    if (!readinessCheckData.m_tgtVolumeInfo.empty())
    {
       validatorPtr.reset(new TargetVolumeValidator(readinessCheckData.m_tgtVolumeInfo, TARGET_VOLUME_IN_USE_CHECK)) ;
       preReqvalidatorList.push_back(validatorPtr) ;
    }

    if (!readinessCheckData.m_db2TgtInstances.empty())
    {
       validatorPtr.reset(new Db2DatabaseShutdownValidator(readinessCheckData.m_db2TgtInstances, DB2_SHUTDOWN_CHECK)) ;
       preReqvalidatorList.push_back(validatorPtr) ;
    }
    if (!readinessCheckData.m_db2SrcInstances.empty())
    {
       validatorPtr.reset(new Db2DatabaseConfigValidator(readinessCheckData.m_db2SrcInstances, readinessCheckData.m_tgtVolumeInfo, DB2_CONFIG_CHECK)) ;
       preReqvalidatorList.push_back(validatorPtr) ;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return preReqvalidatorList ;
}

std::list<ValidatorPtr> RuleFactory::getPrismProtectionSourceRules(SourceReadinessCheckInfo& srcReadinessCheckInfo, bool isLocalContext)
{

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<ValidatorPtr> preReqvalidatorList ;
    ValidatorPtr validatorPtr ;

    validatorPtr.reset(new ApplianceTargetLunCheckValidator(srcReadinessCheckInfo.m_atLunCheckInfo.m_applianceTargetLUNNumber,
                                        srcReadinessCheckInfo.m_atLunCheckInfo.m_applianceTargetLUNName,
                                        srcReadinessCheckInfo.m_atLunCheckInfo.m_physicalInitiatorPWWNs,
                                        srcReadinessCheckInfo.m_atLunCheckInfo.m_applianceTargetPWWNs, 
                                        DUMMY_LUNZ_CHECK)) ;
    preReqvalidatorList.push_back(validatorPtr) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return preReqvalidatorList ;
}
