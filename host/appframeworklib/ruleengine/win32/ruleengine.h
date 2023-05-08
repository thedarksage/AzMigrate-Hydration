#ifndef __RULE__ENGINE__H
#define __RULE__ENGINE__H

#include "boost/shared_ptr.hpp"

#include "appglobals.h"
#include "ruleengine/validator.h"

#include "mssql/mssqlmetadata.h"
#include "exchange/exchangediscovery.h"
#include "exchange/exchangemetadata.h"
#include "fileserver/fileserverdiscovery.h"
#include "config/applicationsettings.h"

class RuleEngine ;
typedef boost::shared_ptr<RuleEngine> RuleEnginePtr ;

class RuleEngine
{
public:
    void RunMSSQLPreRequisiteChecks(const MSSQLInstanceMetaData& instanceMetaData) ;
    static RuleEnginePtr getInstance() ;
    static RuleEnginePtr m_ruleEnginePtr ;
    std::list<ValidatorPtr> getSystemRules(enum SCENARIO_TYPE scenarioType,
										   enum CONTEXT_TYPE contextType,
										   const std::string& appTye = "",
										   bool bDnsFailover = true,
                                           std::string srchostnameforPermissionChecks = "" ,
                                           const std::string& tgthostnameforPermissionChecks = "" ,
                                           bool bAtTarget = false,
										   bool isLocalContext=true) ;
	std::list<ValidatorPtr> getMSSQLInstanceRules(const MSSQLInstanceMetaData& instanceMetaData,
													enum SCENARIO_TYPE scenarioType,
													enum CONTEXT_TYPE contextType,
													const std::string& sysDrive,
													const std::string& strVacpOptions,
													bool isLocalContext=true) ;
    std::list<ValidatorPtr> getMSSQLVersionLevelRules(InmProtectedAppType m_type, 
													  enum SCENARIO_TYPE scenarioType,
													  enum CONTEXT_TYPE contextType,
                                                      bool isLocalContext=true) ;
    std::list<ValidatorPtr> getExchangeRules(const ExchAppVersionDiscInfo & info, 
                                                                    const ExchangeMetaData& metadata,
																	enum SCENARIO_TYPE scenarioType,
																	enum CONTEXT_TYPE contextType,
                                                                    const std::string& systemDrive,
																	/*const*/ std::string &vacpCommand,
																	const std::string& hostname,
                                                                    bool isLocalContext=true) ;
	std::list<ValidatorPtr> getExchange2010Rules(const ExchAppVersionDiscInfo & info, 
                                                                    const Exchange2k10MetaData& metadata, 
																	enum SCENARIO_TYPE scenarioType,
																	enum CONTEXT_TYPE contextType,
                                                                    const std::string& systemDrive,
																	std::string &vacpCommand,
																	const std::string& hostname,
                                                                    bool isLocalContext=true) ;
	std::list<ValidatorPtr> getFileServerRules( enum SCENARIO_TYPE scenarioType,
												enum CONTEXT_TYPE contextType,
												bool isLocalContext=true );
	std::list<ValidatorPtr> getFileServerInstanceLevelRules( const FileServerInstanceMetaData& instanceMetaData,
																enum SCENARIO_TYPE scenarioType,
																enum CONTEXT_TYPE contextType,
																const std::string& sysDrive,
																const std::string& strVacpOptions,
																bool isLocalContext=true );
	std::list<ValidatorPtr> getBESRules(bool isLocalContext=true);
	std::list<ValidatorPtr> getNormalProtectionSourceRules(const std::string& strVacpOptions, 
														   bool isLocalContext=true);

	std::list<ValidatorPtr> getTargetRedinessRules(const ExchangeTargetReadinessCheckInfo& srcInfo, 
                                                                                const ExchangeTargetReadinessCheckInfo& tgtInfo,
																				enum SCENARIO_TYPE scenarioType,
																				enum CONTEXT_TYPE contextType,
																				const SV_ULONGLONG& totalNumberOfPairs,
                                                                                bool isLocalContext=true);
	std::list<ValidatorPtr> getMSSQLTargetRedinessRules(MSSQLTargetReadinessCheckInfo& srcData, 
                                                                                        MSSQLTargetReadinessCheckInfo& tgtData,
																						enum SCENARIO_TYPE scenarioType,
																						enum CONTEXT_TYPE contextType,
																						const SV_ULONGLONG& totalNumberOfPairs,
                                                                                        bool isLocalContext=true);
	std::list<ValidatorPtr> getFileServerTargetRedinessRules( FileServerTargetReadinessCheckInfo& srcData,
																enum SCENARIO_TYPE scenarioType,
																enum CONTEXT_TYPE contextType,
																const SV_ULONGLONG& totalNumberOfPairs,
 															  bool isLocalContext=true );
	std::list<ValidatorPtr> getNormalProtectionTargetRules(const TargetReadinessCheckInfo &tgtInfo,
															const SV_ULONGLONG& totalNumberOfPairs,
														   bool isLocalContext=true);

	SVERROR RunRule(ValidatorPtr mssqlPreReqValidatorsList) ;
	SVERROR RunRules(std::list<ValidatorPtr>& validations);

    void RunMSSQLPreRequisiteChecks(std::list<ValidatorPtr>& mssqlPreReqValidatorsList ) ;
} ;

#endif
