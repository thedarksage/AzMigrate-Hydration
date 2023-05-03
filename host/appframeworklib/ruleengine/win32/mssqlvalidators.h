#include "ruleengine/validator.h"
#include "mssql/mssqlmetadata.h"
#include "config/applicationsettings.h"
#include <map>
class MSSQLDBOnSystemDriveValidator : public Validator
{
    MSSQLInstanceMetaData m_instanceMetaData ;
    std::string m_sysDrive ;
    MSSQLDBOnSystemDriveValidator(const std::string& name, 
        const std::string& description,
        const MSSQLInstanceMetaData& instanceMetadata, 
        const std::string systemDrive,
        InmRuleIds ruleId) ;
public:
    MSSQLDBOnSystemDriveValidator(const MSSQLInstanceMetaData& instanceMetadata, 
        const std::string systemDrive,
        InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;
} ;

class MSSQLDbOnlineValidator : public Validator
{
    MSSQLInstanceMetaData m_instanceMetaData ;
    MSSQLDbOnlineValidator(const std::string& name, 
        const std::string& description,
        const MSSQLInstanceMetaData& instanceMetadata,
        InmRuleIds ruleId) ;
public:
    MSSQLDbOnlineValidator(const MSSQLInstanceMetaData& instanceMetadata,
        InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;
} ;


class DataRootEqualityValidator : public Validator
{
    std::string m_srcDataRoot ;
    std::string m_tgtDataRoot ;
    DataRootEqualityValidator(const std::string& name, 
        const std::string& description, 
        const std::string& srcDataRoot,
        const std::string& tgtDataRoot,
        InmRuleIds ruleId) ;
public:

    DataRootEqualityValidator(const std::string& srcDataRoot,
        const std::string& tgtDataRoot,
        InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;
} ;


class ApplicationInstanceNameCheck : public Validator
{
    std::string m_primaryServerInstanceName ;
    std::string m_secondaryServerInstanceName ;
    ApplicationInstanceNameCheck(const std::string& name, 
        const std::string& description, 
        std::string& ,
        std::string& ,
        InmRuleIds ruleId) ;
public:
    ApplicationInstanceNameCheck(std::string& ,
        std::string& ,
        InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;
} ;

class ApplicationInstallationVolumeCheck : public Validator
{
    ApplicationInstallationVolumeCheck(const std::string& name, 
        const std::string& description, 
        const std::map<std::string, std::string> &,
        const std::list<std::string> & srcDataVols,
        InmRuleIds ruleId) ;
    std::map<std::string, std::string> m_installMountPointMap ;
    std::list<std::string> m_srcDataVols ;
public:
    ApplicationInstallationVolumeCheck(const std::map<std::string, std::string> & , const std::list<std::string> & srcDataVols, InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;
} ;

