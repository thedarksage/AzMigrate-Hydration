#ifndef DISCOVERY_VALIDATOR_H
#define DISCOVERY_VALIDATOR_H
#include "appglobals.h"
#include "applicationsettings.h"
#include <boost/shared_ptr.hpp>

// enum SCENARIO_TYPE
// {
	// SCENARIO_PROTECTION = 1,
	// SCENARIO_FAILOVER,
	// SCENARIO_FAILBACK,
	// SCENARIO_FASTFAILBACK
// };

// enum  CONTEXT_TYPE  
// {
	// CONTEXT_HADR = 1,
	// CONTEXT_BACKUP,
	// CONTEXT_BULK
// };

class DiscoveryAndValidator 
{

public:
	//TODO : Need to make them private and use via get functions
	SV_ULONG m_policyId ;
	std::string m_policyType ;
	SCENARIO_TYPE m_scenarioType; //Protection, Failover, Fast-Failback
	CONTEXT_TYPE   m_context; // HA-DR, Backup, Bulk
    SV_ULONGLONG m_totalNumberOfPairs; //number of pairs
	std::string m_appInstanceName ;
	std::string m_appInstanceId ;
	std::string m_appVacpOptions ;
	ApplicationPolicy m_policy ;
    std::string m_applicationType ;
    std::string m_solutionType ;
    SV_ULONG m_scheduleType ;
	DiscoveryAndValidator(const ApplicationPolicy&) ;
    void init() ;
	virtual SVERROR Process();
    virtual SVERROR Discover(bool shouldUpdateCx) = 0;
    virtual SVERROR PerformReadinessCheckAtSource() = 0;
    virtual SVERROR PerformReadinessCheckAtTarget() = 0;
    virtual SVERROR UnregisterDB() { return SVS_OK; };
    SVERROR fillDiscoveredSystemInfo(DiscoveryInfo&);
    SVERROR fillPolicyInfoInfo(std::map<std::string, std::string>&);
} ;
typedef boost::shared_ptr<DiscoveryAndValidator> DiscoveryAndValidatorPtr ;


#endif
