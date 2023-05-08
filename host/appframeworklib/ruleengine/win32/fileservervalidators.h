#ifndef __FILESERVERVALIDATOR_H__
#define __FILESERVERVALIDATOR_H__
#include "ruleengine/validator.h"
#include "fileserver/fileserverdiscovery.h"

class FileShareOnSystemDriveValidator : public Validator
{
    FileServerInstanceMetaData m_instanceMetaData ;
    std::string m_sysDrive ;
    FileShareOnSystemDriveValidator(const std::string& name, 
									const std::string& description,
									const FileServerInstanceMetaData& instanceMetadata, 
									const std::string systemDrive,
									InmRuleIds ruleId) ;
public:
    FileShareOnSystemDriveValidator(const FileServerInstanceMetaData& instanceMetadata, 
									const std::string systemDrive,
									InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;
} ;


#endif
