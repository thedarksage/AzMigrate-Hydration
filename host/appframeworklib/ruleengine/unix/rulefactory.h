#ifndef __RULE__FACTORY__H
#define __RULE__FACTORY__H
#include <list>
#include "ruleengine/validator.h"
#include "oracle/OracleDiscovery.h"
#include "db2/Db2Discovery.h"

class RuleFactory
{
public:
	static std::list<ValidatorPtr> getNormalProtectionSourceRules( const std::string& strVacpOptions, bool isLocalContext=true);
	static std::list<ValidatorPtr> getNormalProtectionTargetRules(const TargetReadinessCheckInfo &tgtInfo,
                                                                  const SV_ULONGLONG& totalNumberOfPairs,
																  bool isLocalContext=true);
																  
	static std::list<ValidatorPtr> getOracleProtectionSourceRules(const OracleAppVersionDiscInfo& appInfo, const std::string strVacpoptions, bool isLocalContext = true);
    static std::list<ValidatorPtr> getDb2ProtectionSourceRules(const Db2AppInstanceDiscInfo& appInfo,std::string instanceName,std::string appDatabaseName, const std::string strVacpoptions);
    static std::list<ValidatorPtr> getOracleProtectionTargetRules(OracleTargetReadinessCheckInfo& readinessCheckInfo, 
                                                                  const SV_ULONGLONG& totalNumberOfPairs, 
                                                                  bool isLocalContext = true);
    static std::list<ValidatorPtr> getDb2ProtectionTargetRules(Db2TargetReadinessCheckInfo& readinessCheckInfo,
                                                               const SV_ULONGLONG& totalNumberOfPairs,
                                                               bool isLocalContext = true);

    static std::list<ValidatorPtr> getPrismProtectionSourceRules(SourceReadinessCheckInfo& srcReadinessCheckInfo, 
                                                                    bool isLocalContext=true);

} ;
#endif

