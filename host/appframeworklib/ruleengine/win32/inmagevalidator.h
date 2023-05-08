#ifndef INMAGE_VALIDATOR_H
#define INMAGE_VALIDATOR_H
#include "ruleengine/validator.h"

class InmNATIPValidator : public Validator
{
    bool m_natEnabled ;
    InmNATIPValidator(const std::string& name, 
        const std::string& description, 
        InmRuleIds ruleId) ;
public:
    InmNATIPValidator(InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;
} ;

class InmNATNameValidator : public Validator
{
    bool m_natEnabled ;
    InmNATNameValidator(const std::string& name, 
        const std::string& description,
        bool natEnabled,
        InmRuleIds ruleId) ;
public:
    InmNATNameValidator(bool natEnabled,
        InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;
} ;

class VolTagIssueValidator : public Validator
{
	std::string m_strVacpOptions;
    std::list<std::string> m_volList ;
    VolTagIssueValidator(const std::string& name, 
        const std::string& description,
	    const std::list<std::string>& volList,
        InmRuleIds ruleId) ;
public:
    VolTagIssueValidator(const std::list<std::string>& volList,
		const std::string strVacpOptions,
        InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    //void FormatVacpErrorCode( std::stringstream& stream, SV_ULONG& exitCode ) ;
    SVERROR fix() ;
    bool canfix() ;
} ;

class VersionEqualityValidator : public Validator
{
    std::string m_srcVersion ;
    std::string m_tgtVersion ;
	std::string m_srcEdition ;
	std::string m_tgtEdition ;
    VersionEqualityValidator(const std::string& name, 
        const std::string& description, 
        const std::string& srcVersion,
        const std::string& tgtVersion,
        InmRuleIds ruleId,
		const std::string& srcEdition = "",
		const std::string& tgtEdition = "") ;
public:
 VersionEqualityValidator(const std::string& srcVersion,
        const std::string& tgtVersion,
        InmRuleIds ruleId,
		const std::string& srcEdition = "",
		const std::string& tgtEdition = "") ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;
} ;

class DomainEqualityValidator : public Validator
{
    std::string m_srcDomain ;
    std::string m_tgtDomain ;
    DomainEqualityValidator(const std::string& name, 
        const std::string& description, 
        const std::string& srcDomain,
        const std::string& tgtDomain,
        InmRuleIds ruleId) ;
public:
    DomainEqualityValidator(const std::string& srcDomain,
        const std::string& tgtDomain,
        InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;

} ;

class VolumeCapacityCheckValidator : public Validator

{
	std::list<std::map<std::string, std::string>> m_VolCapacities ;
	VolumeCapacityCheckValidator(const std::string& name, 
                             const std::string& description, 
                             const std::list<std::map<std::string, std::string>> mapVolCapacities,
                             InmRuleIds ruleId) ;
    
public:
    VolumeCapacityCheckValidator(const std::list<std::map<std::string, std::string>> mapVolCapacities,
                             InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;
} ;

class TargetVolumeAvaialabilityValiadator: public Validator
{
    std::list<std::string> targetVolumeList ;
    TargetVolumeAvaialabilityValiadator( const std::string& name, 
        const std::string& description,
        const std::list<std::string> targetVolumes,
        InmRuleIds ruleId ) ;

public:
    TargetVolumeAvaialabilityValiadator( const std::list<std::string> targetVolumes,
        InmRuleIds ruleId ) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;
} ;

class PageFileVoumeCheck : public Validator
{
	std::string m_strVacpOptions;
    std::list<std::string> m_volList ;
    PageFileVoumeCheck(const std::string& name, 
        const std::string& description,
	    const std::list<std::string>& volList,
        InmRuleIds ruleId) ;
public:
    PageFileVoumeCheck(const std::list<std::string>& volList,InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;
} ;
#endif
