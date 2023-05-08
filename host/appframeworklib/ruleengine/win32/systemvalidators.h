#ifndef __SYSTEM__VALIDATORS__H
#define __SYSTEM__VALIDATORS__H
#include "ruleengine/validator.h"
#include "appglobals.h"
#include "service.h"


class ServiceValidator : public Validator
{
    std::list<InmService> m_services ;
    InmServiceStatus m_status ;
    ServiceValidator(const std::string& name, 
                                const std::string& description,
                                const InmService service, 
                                InmServiceStatus serviceStatus, 
                                InmRuleIds ruleId) ;

    ServiceValidator::ServiceValidator(const std::string& name, 
                                                            const std::string& description,
                                                            const std::list<InmService> services, 
                                                            InmServiceStatus serviceStatus,
        InmRuleIds ruleId) ;


public:
    ServiceValidator(const InmService service, 
                                InmServiceStatus serviceStatus, 
                                InmRuleIds ruleId) ;
    
    ServiceValidator(const std::list<InmService> serviceNameList, 
        InmServiceStatus serviceStatus,
        InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;
} ;

class ProcessValidator : public Validator
{
    std::string m_processName ;
    InmProcessStatus m_status ; 
    ProcessValidator(const std::string& name, 
                                const std::string& description,
                                const std::string processName, 
                                InmProcessStatus processStat, 
                                InmRuleIds ruleId) ;
public:
    ProcessValidator(const std::string processName, InmProcessStatus processStat, InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;
} ;


class FirewallValidator : public Validator
{
    InmFirewallStatus m_currentStatus, m_requiredStatus ;
    FirewallValidator(const std::string& name, 
                                const std::string& description,
                                InmFirewallStatus currentStatus,  
                                InmFirewallStatus requiredStatus, 
                                InmRuleIds ruleId) ;
public:
    FirewallValidator(InmFirewallStatus currentStatus, 
                                InmFirewallStatus requiredStatus, 
                                InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;
} ;

class DynamicDNSUpdateValidator : public Validator
{
    InmDnsUpdateType m_currentUpdateType, m_requiredUpdateType ;
    DynamicDNSUpdateValidator(const std::string& name, 
                                                    const std::string& description, 
                                                    InmDnsUpdateType currentUpdateType, 
                                                    InmDnsUpdateType requiredUpdateType, 
                                                    InmRuleIds ruleId) ;
public:
    DynamicDNSUpdateValidator(InmDnsUpdateType currentUpdateType, 
                                                InmDnsUpdateType requiredUpdateType, 
                                                InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;
} ;



class DNSUpdatePrivilageValidator : public Validator
{
    DNSUpdatePrivilageValidator(const std::string& name, 
                                                    const std::string& description, 
                                                    const std::string & hostnameforPermissionChecks,
                                                    InmRuleIds ruleId) ;
    std::string m_hostnameforPermissionChecks ;
public:
    DNSUpdatePrivilageValidator(const std::string & hostnameforPermissionChecks, InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;
} ;

class ADUpdatePrivilageValidator : public Validator
{
    std::string m_hostnameforPermissionChecks ;
    ADUpdatePrivilageValidator(const std::string& name, 
                                                const std::string& description, 
                                                const std::string & hostnameforPermissionChecks, 
                                                InmRuleIds ruleId) ;
public:
    ADUpdatePrivilageValidator(const std::string & hostnameforPermissionChecks, InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;
} ;
class MultipleNwAdaptersRule:public Validator
{
    MultipleNwAdaptersRule(const std::string& name, 
                           const std::string& description,
                           InmRuleIds ruleId) ;
public:
    MultipleNwAdaptersRule(InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;

} ;

class DNSAvailabilityValidator : public Validator
{
	DNSAvailabilityValidator(const std::string& name, 
                                            const std::string& description, 
                                            InmRuleIds ruleId) ;
public:
    DNSAvailabilityValidator(InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;

};

class DCAvailabilityValidator : public Validator
{
	DCAvailabilityValidator(const std::string& name, 
                                        const std::string& description, 
                                        InmRuleIds ruleId) ;
public:
    DCAvailabilityValidator(InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;

};

class  HostDCCheckValidator: public Validator
{
	HostDCCheckValidator(const std::string& name, 
                                        const std::string& description, 
                                        InmRuleIds ruleId) ;
public:
    HostDCCheckValidator(InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;

};
class CacheDirCapacityCheckValidator : public Validator

{
    CacheDirCapacityCheckValidator(const std::string& name, 
                             const std::string& description, 
                             InmRuleIds ruleId) ;
    
public:
    CacheDirCapacityCheckValidator(InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;
} ;

class CMMinSpacePerPairCheckValidator : public Validator

{
    CMMinSpacePerPairCheckValidator(const std::string& name, 
                             const std::string& description, 
							 const SV_ULONGLONG& totalNumberOfPairs,
                             InmRuleIds ruleId) ;
	SV_ULONGLONG m_totalNumberOfPairs;
    
public:
    CMMinSpacePerPairCheckValidator(const SV_ULONGLONG& totalNumberOfPairs, InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;
} ;

class IPEqualityValidator : public Validator
{
	std::string m_dnsrecordName1;
	std::string m_dnsrecordName2;
	bool m_bShouldEqual;
	IPEqualityValidator(const std::string& name,
		const std::string& description, 
		const std::string& dnsrecordName1, 
		const std::string& dnsrecordName2,
		const bool& bShouldEqual,
		InmRuleIds ruleId);
public:
	IPEqualityValidator(const std::string& dnsrecordName1, 
						const std::string& dnsrecordName2,
						const bool& bShouldEqual,
						InmRuleIds ruleId);

	SVERROR evaluate() ;
	SVERROR fix() ;
	bool canfix() ;
};

class VSSRollupCheckValidator : public Validator
{
	VSSRollupCheckValidator(const std::string& name,
		const std::string& description, 
		InmRuleIds ruleId);
public:
	VSSRollupCheckValidator(InmRuleIds ruleId);

	SVERROR evaluate() ;
	SVERROR fix() ;
	bool canfix() ;
};

#endif

