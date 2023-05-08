#include "fileservervalidators.h"
#include <sstream>
FileShareOnSystemDriveValidator::FileShareOnSystemDriveValidator(const std::string& name, 
                                                             const std::string& description,
                                                             const FileServerInstanceMetaData& instanceMetadata,
                                                             const std::string systemDrive,
                                                             InmRuleIds ruleId)
                                                             :Validator(name, description, ruleId),
                                                             m_instanceMetaData(instanceMetadata )

{
}

FileShareOnSystemDriveValidator::FileShareOnSystemDriveValidator(const FileServerInstanceMetaData& instanceMetadata,
                                                             const std::string systemDrive,
                                                             InmRuleIds ruleId)
                                                             :Validator(ruleId),
                                                             m_instanceMetaData(instanceMetadata ),
                                                             m_sysDrive(systemDrive)
{
}


bool FileShareOnSystemDriveValidator::canfix()
{
    DebugPrintf(SV_LOG_DEBUG, "ENETERD %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    return bRet ;
}

SVERROR FileShareOnSystemDriveValidator::fix()
{
    DebugPrintf(SV_LOG_DEBUG, "ENETERD %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR FileShareOnSystemDriveValidator::evaluate()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n", FUNCTION_NAME) ;
    std::string SystemDriveLetter ;
    SVERROR bRet = SVS_FALSE ;
    std::stringstream stream ;
    InmRuleStatus status = INM_RULESTAT_NONE ;
    InmRuleErrorCode ruleStatus = RULE_STAT_NONE;
    std::string dependentRule ;
    if( isDependentRuleFailed(dependentRule) )
    {
        stream <<  "Skipping this rule as dependent rule : " << dependentRule << " failed " << std::endl;
        status = INM_RULESTAT_SKIPPED ;
		DebugPrintf(SV_LOG_DEBUG, "Skipping this rule as dependent rule : %s is failed \n", dependentRule.c_str());
	}
    else
    {
        bRet = SVS_OK ;
        if( m_sysDrive.length() == 2 )
        {
            m_sysDrive.append("\\") ;
        }
		std::map<std::string, FileShareInfo>::iterator shareInfoIter = m_instanceMetaData.m_shareInfo.begin() ;
        stream << " System Volume is : " << m_sysDrive << std::endl ;
        stream << " File Shares that have their files on system drive: " << std::endl ;
        int index = 1 ;
        while( shareInfoIter != m_instanceMetaData.m_shareInfo.end() )
        {
			if( strcmpi( m_sysDrive.c_str(), shareInfoIter->second.m_mountPoint.c_str() ) == 0 )
            {
				stream << ++index <<". " << shareInfoIter->second.m_shareName << std::endl ;
                status = INM_RULESTAT_FAILED ;
                ruleStatus = APPLICATION_DATA_ON_SYSTEM_DRIVE_ERROR;
            }
            shareInfoIter++ ;
        }
        if( status != INM_RULESTAT_FAILED )
        {
            stream << "NONE" << std::endl ;
            status = INM_RULESTAT_PASSED ;
            ruleStatus = RULE_PASSED;

        }
    }
    setStatus(status) ;
    setRuleMessage(stream.str());
    setRuleExitCode(ruleStatus);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}


