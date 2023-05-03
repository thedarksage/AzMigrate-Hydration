#ifndef FAILOVER_COMMAND_UTIL_H
#define FAILOVER_COMMAND_UTIL_H

#include "applicationsettings.h"
#include "config/appconfigurator.h"
#include "Consistency/tagdiscovery.h"
#include "config/applocalconfigurator.h"

class FailoverCommandUtil
{
public:
    ApplicationSettings m_AppSettings;
    std::string m_appType;
    std::map<SV_ULONG,ApplicationPolicy> m_DisabledAppPolicies;
	std::map<SV_ULONG,bool> m_PoliciesWithResyncPair;
    void GetApplicationSettings();
    SV_ULONG GetNoOfPolicies();
    SVERROR GetProtectionSettings(SV_ULONG policyId,AppProtectionSettings& appProtecionSettings);
    SVERROR GetFailoverData(SV_ULONG policyId,FailoverPolicyData& failoverData);
    void GetDisabledPolices();
    void ShowDisabledPolices();
    SVERROR EnablePolicy(SV_ULONG policyId);
    SVERROR GetRecoverOptions(std::list<std::string>& volumeList,FailoverPolicyData& failoverData);
	bool CopySettingsForRerun(SV_ULONG depPolicyId = 0);
    ApplicationSettings ReadSettings (std::string const & filename);
    bool CopyCacheFile(std::string const & SourceFile, std::string const & DestinationFile);
    bool ValidatePolyId(SV_ULONG policyId);
    void SerializeAppSettings();
    void ChooseLastestCommonTag(std::map<std::string, std::vector<CDPEvent> > eventInfoMap,FailoverPolicyData& failoverData);
    void ChooseLastestCommonTime(std::map< std::string, std::vector<CDPTimeRange> > timeRangeInfoMap,FailoverPolicyData& failoverData);
    SVERROR LastestCommonTag(std::vector<CDPEvent>& cdpEvents,std::string volumeName,FailoverPolicyData& failoverData);
    SVERROR LastestCommonTime(std::vector<CDPTimeRange>& cdpTimeRanges,std::string volumeName,FailoverPolicyData& failoverData);
    int GetRecoveryType(std::list<std::string>& volumeList);
    SVERROR ChooseCustomRecovery(std::list<std::string>& volList,FailoverPolicyData& failoverData);
    SVERROR PrintLogFile(std::string outputFileName);
    void monitorFailoverExecution(SV_ULONG policyId);
    void getFailoverJobs(FailoverJobs& FailoverJobs);
    void PrintSummary(FailoverPolicyData& failoverData);
	SVERROR StopSVAgent();
	SVERROR StartSVAgent();
};
#endif
