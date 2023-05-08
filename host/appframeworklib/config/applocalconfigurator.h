#ifndef APPLOCALCONFIGURATOR_H
#define APPLOCALCONFIGURATOR_H
#include "localconfigurator.h"

static const char SECTION_APPAGENT[] = "appagent" ;
static const char KEY_APPSETTINGS_CACHEPATH[] = "ApplicationSettingsCachePath" ;
static const char KEY_APPSETTINGS_POLLINGINTERVAL[] = "SettingsPollingInterval" ;
static const char KEY_APPDISCOVERY_INTERVAL[] = "ApplicationDiscoveryInterval" ;
static const char KEY_APPSCHEDULER_CACHEPATH[] = "ApplicationSchedulerCachePath" ;
static const char KEY_FAILOVERJOBS_CACHEPATH[] = "ApplicationFailoverJobsCachePath";
static const char KEY_EXCHANGE_SEARCHPATH[]    = "ExchangeSearchPath";
static const char KEY_EXCHANGE_SEARCH_ALREADY_BACKED_UP[]    = "ExchangeFilesAlreadyBackedUp";
static const char KEY_COMPRESS_VOLPACKS[] = "CompressVolPacks";
static const char KEY_CONSISTENCY_TAG_ISSUE_TIME_LIMIT[] = "ConsistencyTagIssueTimeLimit";
static const char KEY_ORACLE_DISCOVERY[] = "OracleDiscovery";
//TODO: Need to add Db2Discovery in drscout.conf
static const char KEY_DB2_DISCOVERY[] = "Db2Discovery";
static const char KEY_ARRAY_DISCOVERY[] = "ArrayDiscovery";
static const char KEY_MYSQL_DISCOVERY[] = "MSSqlDiscovery";
static const char KEY_MSEXCHANGE_DISCOVERY[] = "MSExchangeDiscovery";
static const char KEY_SUPPORTED_APPS[] = "SupportedApplications";
static const char KEY_REGISTER_APP_AGENT[] = "RegisterAppAgent";

class AppLocalConfigurator : public FileConfigurator
{
public:
    std::string getAppSettingsCachePath() const ;
    SV_UINT getAppSettingsPollingInterval() const ;
    SV_UINT getApplicationDiscoveryInterval() const ;
    std::string getAppSchedulerCachePath() const ;
    std::string getFailoverJobsCachePath() const ;
	std::string getExchangeSrcDataPaths() const;
    bool    shouldDiscoverOracle() const;
    bool    shouldDiscoverDb2() const;
    bool    shouldDiscoverArrays() const;
    bool    shouldDiscoverMySql() const;
    bool    shouldDiscoverMSExchange() const;
	SV_UINT getExchangeFilesAlreadyBackedUp() const;
	void setExchangeDataPath(const std::string &);
	void setExchangeFilesBackedUp(const std::string &);
	std::string getApplicatioAgentCachePath();
	std::string getApplicationPolicyLogPath();
    SV_UINT getCompressVolPacks() const;
    void setVolpackCompression(bool compression) const ;
	SV_UINT getConsistencyTagIssueTimeLimit() const;
	std::string getSupportedAppNames() const;
	bool getRegisterAppAgent() const;
} ;
#endif
