#ifndef __RULE__ENGINE__H
#define __RULE__ENGINE__H

#include "boost/shared_ptr.hpp"

#include "appglobals.h"
#include "ruleengine/validator.h"
#include "applicationsettings.h"
#include "OracleDiscovery.h"
#include "Db2Discovery.h"

class RuleEngine ;
typedef boost::shared_ptr<RuleEngine> RuleEnginePtr ;

class RuleEngine
{
public:
    static RuleEnginePtr getInstance() ;
    static RuleEnginePtr m_ruleEnginePtr ;
	std::list<ValidatorPtr> getNormalProtectionSourceRules(const std::string& strVacpOptions, 
														   bool isLocalContext=true);
	std::list<ValidatorPtr> getNormalProtectionTargetRules(const TargetReadinessCheckInfo &tgtInfo,
                                                           const SV_ULONGLONG& totalNumberOfPairs,
														   bool isLocalContext=true);	
														   
	std::list<ValidatorPtr> getOracleProtectionSourceRules(const OracleAppVersionDiscInfo& appInfo, const std::string strVacpoptions, bool isLocalContext = true);
    std::list<ValidatorPtr> getDb2ProtectionSourceRules(const Db2AppInstanceDiscInfo& appInfo, std::string instanceName,std::string appDatabaseName, const std::string strVacpoptions);
    std::list<ValidatorPtr> getOracleProtectionTargetRules(OracleTargetReadinessCheckInfo& readinessCheckInfo, 
                                                            const SV_ULONGLONG& totalNumberOfPairs, 
                                                            bool isLocalContext=true);	
    std::list<ValidatorPtr> getDb2ProtectionTargetRules(Db2TargetReadinessCheckInfo& readinessCheckInfo,const SV_ULONGLONG& totalNumberOfPairs,bool isLocalContext=true);
	
	SVERROR RunRule(ValidatorPtr mssqlPreReqValidatorsList) ;
	SVERROR RunRules(std::list<ValidatorPtr>& validations);

	std::list<ValidatorPtr> getPrismProtectionSourceRules(SourceReadinessCheckInfo& srcReadinessCheckInfo, bool isLocalContext=true);
														   
} ;

#endif
