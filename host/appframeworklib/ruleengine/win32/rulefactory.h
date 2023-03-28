#ifndef __RULE__FACTORY__H
#define __RULE__FACTORY__H
#include <list>
#include "ruleengine/validator.h"

#include "exchange/exchangediscovery.h"
#include "exchange/exchangemetadata.h"
#include "mssql/mssqlmetadata.h"
#include "fileserver/fileserverdiscovery.h"

class RuleFactory
{
public:
    static std::list<ValidatorPtr>getSystemRules(enum SCENARIO_TYPE scenarioType,
												 enum CONTEXT_TYPE contextType,
												 const std::string&,
												 bool bDnsFailover,
                                                 std::string srchostnameforPermissionChecks,
                                                 const std::string& tgthostnameforPermissionChecks,
                                                 bool bAtTarget,
												 bool isLocalContext=true) ;
    static std::list<ValidatorPtr>getMSSQLInstanceRules(const MSSQLInstanceMetaData&, 
														enum SCENARIO_TYPE scenarioType,
														enum CONTEXT_TYPE contextType,
                                                        const std::string& sysDrive,
														const std::string& strVacpOptions,
                                                        bool isLocalContext=true) ;
	static std::list<ValidatorPtr>getMSSQLVersionLevelRules(InmProtectedAppType m_type, 
															enum SCENARIO_TYPE scenarioType,
															enum CONTEXT_TYPE contextType,		
															bool isLocalContext=true) ;
    static std::list<ValidatorPtr> getExchangeRules(const ExchAppVersionDiscInfo& info, 
                                                                            const ExchangeMetaData& metadata, 
																			enum SCENARIO_TYPE scenarioType,
																			enum CONTEXT_TYPE contextType,
                                                                            const std::string& systemDrive,
																			/*const*/ std::string &strVacpOptions,
                                                                            const std::string& hostname,
                                                                            bool isLocalContext=true) ;
	static std::list<ValidatorPtr> getExchange2010Rules(const ExchAppVersionDiscInfo& info, 
                                                                                const Exchange2k10MetaData& metadata,
																				enum SCENARIO_TYPE scenarioType,
																				enum CONTEXT_TYPE contextType,
                                                                                const std::string& systemDrive,
                                                                                std::string &strVacpOptions,
																				const std::string& hostname,
                                                                                bool isLocalContext=true) ;
	static std::list<ValidatorPtr> getFileServerRules(enum SCENARIO_TYPE scenarioType,
														enum CONTEXT_TYPE contextType,
														bool isLocalContext=true);
	static std::list<ValidatorPtr> getFileServerInstanceLevelRules( const FileServerInstanceMetaData& instanceMetaData,
																	enum SCENARIO_TYPE scenarioType,
																	enum CONTEXT_TYPE contextType,
																	const std::string& sysDrive,
																	const std::string& strVacpOptions,
																	bool isLocalContext = true );
    static std::list<ValidatorPtr> getBESRules( bool isLocalContext=true);
	static std::list<ValidatorPtr> getNormalProtectionSourceRules( const std::string& strVacpOptions, bool isLocalContext=true);
	static std::list<ValidatorPtr> getTargetRedinessRules(const ExchangeTargetReadinessCheckInfo& srcInfo, 
                                                                                const ExchangeTargetReadinessCheckInfo& tgtInfo, 
																				enum SCENARIO_TYPE scenarioType,
																				enum CONTEXT_TYPE contextType,
																				const SV_ULONGLONG& totalNumberOfPairs,
                                                                                bool isLocalContext=true) ;
	static std::list<ValidatorPtr> getMSSQLTargetRedinessRules(MSSQLTargetReadinessCheckInfo& srcData, 
																MSSQLTargetReadinessCheckInfo& tgtData,
																enum SCENARIO_TYPE scenarioType,
																enum CONTEXT_TYPE contextType,
																const SV_ULONGLONG& totalNumberOfPairs,
																bool isLocalContext=true);
	static std::list<ValidatorPtr> getFileServerTargetRedinessRules( FileServerTargetReadinessCheckInfo& srcData,
																		enum SCENARIO_TYPE scenarioType,
																		enum CONTEXT_TYPE contextType,
																		const SV_ULONGLONG& totalNumberOfPairs,
																		bool isLocalContext=true );
	static std::list<ValidatorPtr> getNormalProtectionTargetRules(const TargetReadinessCheckInfo &tgtInfo,
																	const SV_ULONGLONG& totalNumberOfPairs,
																  bool isLocalContext=true);

} ;
#endif
