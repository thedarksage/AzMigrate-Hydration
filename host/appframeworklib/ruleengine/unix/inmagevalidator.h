#ifndef INMAGE_VALIDATOR_H
#define INMAGE_VALIDATOR_H
#include "ruleengine/validator.h"

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
    SVERROR fix() ;
    bool canfix() ;
} ;

class VolumeCapacityCheckValidator : public Validator

{
    std::list<std::map<std::string, std::string> > m_volCapacitiesInfo;
    VolumeCapacityCheckValidator(const std::string& name, 
                             const std::string& description, 
                             const std::list<std::map<std::string, std::string> > &volCapacitiesInfo,
                             InmRuleIds ruleId) ;
    
public:
    VolumeCapacityCheckValidator(const std::list<std::map<std::string, std::string> > &volCapacitiesInfo,
                             InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;
} ;

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
                             const SV_ULONGLONG& m_totalNumberOfPairs,
                             InmRuleIds ruleId) ;
    SV_ULONGLONG m_totalNumberOfPairs; 

public:
    CMMinSpacePerPairCheckValidator(const SV_ULONGLONG& m_totalNumberOfPairs, InmRuleIds ruleId) ;
    SV_ULONGLONG getDirectoryConsumedSpace(std::string& cachedir);
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;
} ;

class ApplianceTargetLunCheckValidator : public Validator
{

    std::string         m_atLunNumber;
    std::string         m_atLunName;
    std::list<std::string>    m_physicalInitiatorPwwns;
    std::list<std::string>    m_applianceTargetPwwns;

    ApplianceTargetLunCheckValidator(const std::string& name,
                        const std::string& description,
                        const std::string& atLunNumber,
                        const std::string&  atLunName,
                        const std::list<std::string>&    physicalInitiatorPwwns,
                        const std::list<std::string>&   applianceTargetPwwns,
                        InmRuleIds  ruleId);

public:
    ApplianceTargetLunCheckValidator(const std::string& atLunNumber,
                        const std::string&  atLunName,
                        const std::list<std::string>&    physicalInitiatorPwwns,
                        const std::list<std::string>&    applianceTargetPwwns,
                        InmRuleIds  ruleId);

    SVERROR evaluate();
    SVERROR fix();
    bool    canfix();
    static bool QuitRequested(int sec);

} ;

class TargetVolumeValidator : public Validator
{

    std::list<std::map<std::string, std::string> >    m_targetVolumeInfo;

    TargetVolumeValidator(const std::string& name,
                        const std::string& description,
                        const std::list<std::map<std::string, std::string> >&   targetVolumeInfo,
                        InmRuleIds  ruleId);

public:
    TargetVolumeValidator(const std::list<std::map<std::string, std::string> >&   targetVolumeInfo,
                        InmRuleIds  ruleId);

    SVERROR evaluate();
    SVERROR fix();
    bool    canfix();

} ;

#endif
