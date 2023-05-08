#include "mssqlvalidators.h"
#include "system.h"
#include <list>
#include <string>
#include <atlbase.h>
#include <sstream>
#include "service.h"
#include "util/common/util.h"
#include "mssql/mssqlapplication.h"

MSSQLDBOnSystemDriveValidator::MSSQLDBOnSystemDriveValidator(const std::string& name, 
                                                             const std::string& description,
                                                             const MSSQLInstanceMetaData& instanceMetadata,
                                                             const std::string systemDrive,
                                                             InmRuleIds ruleId)
                                                             :Validator(name, description, ruleId),
                                                             m_instanceMetaData(instanceMetadata )

{
}

MSSQLDBOnSystemDriveValidator::MSSQLDBOnSystemDriveValidator(const MSSQLInstanceMetaData& instanceMetadata,
                                                             const std::string systemDrive,
                                                             InmRuleIds ruleId)
                                                             :Validator(ruleId),
                                                             m_instanceMetaData(instanceMetadata ),
                                                             m_sysDrive(systemDrive)
{
}

bool MSSQLDBOnSystemDriveValidator::canfix()
{
    DebugPrintf(SV_LOG_DEBUG, "ENETERD %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    return bRet ;
}

SVERROR MSSQLDBOnSystemDriveValidator::fix()
{
    DebugPrintf(SV_LOG_DEBUG, "ENETERD %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR MSSQLDBOnSystemDriveValidator::evaluate()
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
    }
    else
    {
        bRet = SVS_OK ;
        if( m_sysDrive.length() == 2 )
        {
            m_sysDrive.append("\\") ;
        }
        std::map<std::string, MSSQLDBMetaData>::const_iterator dbIter = m_instanceMetaData.m_dbsMap.begin() ;
        stream << " System Volume is : " << m_sysDrive << std::endl ;
        stream << " Instance Name : " << m_instanceMetaData.m_instanceName << std::endl << std::endl;
        stream << " Databases that have their files on system drive: " << std::endl ;
        int index = 0 ;
        while( dbIter != m_instanceMetaData.m_dbsMap.end() )
        {
			if(MSSQL_DBTYPE_TEMP != dbIter->second.m_dbType)
			{
				std::list<std::string>::const_iterator strIter ;
				strIter = std::find(dbIter->second.m_volumes.begin(), dbIter->second.m_volumes.end(), m_sysDrive) ;

				if( strIter != dbIter->second.m_volumes.end() )
				{
					stream << ++index <<". " << dbIter->second.m_dbName << std::endl ;
					status = INM_RULESTAT_FAILED ;
					ruleStatus = APPLICATION_DATA_ON_SYSTEM_DRIVE_ERROR;
				}
			}
            dbIter++ ;
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


MSSQLDbOnlineValidator::MSSQLDbOnlineValidator(const MSSQLInstanceMetaData& instanceMetadata,
                                               InmRuleIds ruleId)
                                               :Validator(ruleId),
                                               m_instanceMetaData(instanceMetadata)
{


}

MSSQLDbOnlineValidator::MSSQLDbOnlineValidator(const std::string& name, 
                                               const std::string& description,
                                               const MSSQLInstanceMetaData& instanceMetadata,
                                               InmRuleIds ruleId)
                                               :Validator(name, description, ruleId),
                                               m_instanceMetaData(instanceMetadata)
{

}
SVERROR MSSQLDbOnlineValidator::evaluate()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_OK ;
    InmRuleStatus status = INM_RULESTAT_NONE ;
    InmRuleErrorCode ruleStatus = RULE_STAT_NONE;
    std::stringstream stream ;
    std::string dependentRule ;
    if( isDependentRuleFailed(dependentRule) )
    {
        stream <<  "Skipping this rule as dependent rule : " << dependentRule << " failed " << std::endl;
        status = INM_RULESTAT_SKIPPED ;
    }
    else
    {
        std::map<std::string, MSSQLDBMetaData>::const_iterator dbIter = m_instanceMetaData.m_dbsMap.begin() ;
        stream << " Instance Name : " << m_instanceMetaData.m_instanceName << std::endl << std::endl;
        stream << " Databases that are offline: " << std::endl ;
        int index = 0 ;
        while( dbIter != m_instanceMetaData.m_dbsMap.end() )
        {
            if( dbIter->second.m_dbOnline == false )
            {
                stream << ++index <<". " << dbIter->second.m_dbName << std::endl ;
                status = INM_RULESTAT_FAILED ;
                ruleStatus = SQL_DB_ONLINE_ERROR;
            }
            dbIter++ ;
        }
        if( status != INM_RULESTAT_FAILED )
        {
            stream << "NONE" << std::endl ;
            status = INM_RULESTAT_PASSED ;
            ruleStatus = RULE_PASSED;

        }
        else
        {
            stream << "\n\n The above datbase(s) are not online\n" << std::endl ;
        }

    }

    setStatus(status) ;
    setRuleMessage(stream.str()) ;
    setRuleExitCode(ruleStatus);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
SVERROR MSSQLDbOnlineValidator::fix()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    if( canfix() )
    {

    }
    else
    {
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;

}
bool MSSQLDbOnlineValidator::canfix()
{
    return false;
}



DataRootEqualityValidator::DataRootEqualityValidator(const std::string& name, 
                                                     const std::string& description, 
                                                     const std::string& srcDataRoot,
                                                     const std::string& tgtDataRoot,
                                                     InmRuleIds ruleId)
                                                     :Validator(name, description, ruleId),
                                                     m_srcDataRoot(srcDataRoot),
                                                     m_tgtDataRoot(tgtDataRoot)
{

}

DataRootEqualityValidator::DataRootEqualityValidator(const std::string& srcDataRoot,
                                                     const std::string& tgtDataRoot,
                                                     InmRuleIds ruleId)
                                                     :Validator(ruleId),
                                                     m_srcDataRoot(srcDataRoot),
                                                     m_tgtDataRoot(tgtDataRoot)
{

}

bool DataRootEqualityValidator::canfix()
{
    return false ;
}

SVERROR DataRootEqualityValidator::fix()
{
    return SVS_FALSE ;
}

SVERROR DataRootEqualityValidator::evaluate()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    InmRuleStatus status ;
    std::stringstream resultStram ;
    InmRuleErrorCode ruleStatus = INM_ERROR_NONE ;
    std::string dependentRule ;
    if( isDependentRuleFailed(dependentRule) )
    {
        resultStram <<  "Skipping this rule as dependent rule : " << dependentRule << " failed " << std::endl;
        status = INM_RULESTAT_SKIPPED ;
    }
    else
    {
		if(strcmpi(m_srcDataRoot.c_str(), m_tgtDataRoot.c_str()) == 0)
        {
            resultStram << "Source And Target MSSQL Instance has same data root "<<std::endl ;
            resultStram << "Data Root: " << m_tgtDataRoot << std::endl ;
            status = INM_RULESTAT_PASSED;
            bRet = SVS_OK ;
            ruleStatus = RULE_PASSED;
        }
        else
        {
			if(strcmpi(m_tgtDataRoot.c_str(), "") == 0)
            {
                resultStram << "Source And Target MSSQL Instance has Differnet DataRoot"<< std::endl;
                resultStram << " Source DataRoot: " << m_srcDataRoot << std::endl ;
                resultStram << " Target DataRoot: "<< m_tgtDataRoot << std::endl ;
            }
            else
            {
                resultStram << "MSSQL Server instance not installed on target machine " << std::endl;
                resultStram << " Source DataRoot: " << m_srcDataRoot << std::endl ;
            }
            status = INM_RULESTAT_FAILED;
            ruleStatus = SQL_DATA_ROOT_ERROR;
        }
    }
    setRuleExitCode(ruleStatus);
    setRuleMessage(resultStram.str());
    setStatus(status);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

ApplicationInstanceNameCheck::ApplicationInstanceNameCheck(const std::string& name, 
                                                           const std::string& description, 
                                                           std::string& primaryServerInstanceName,
                                                           std::string& secondaryServerInstanceName,
                                                           InmRuleIds ruleId)
                                                           :Validator(name, description, ruleId),
                                                           m_primaryServerInstanceName(primaryServerInstanceName),
                                                           m_secondaryServerInstanceName(secondaryServerInstanceName)
{
}

ApplicationInstanceNameCheck::ApplicationInstanceNameCheck(std::string& primaryServerInstanceName,
                                                           std::string& secondaryServerInstanceName,
                                                           InmRuleIds ruleId)
                                                           :Validator(ruleId),
                                                           m_primaryServerInstanceName(primaryServerInstanceName),
                                                           m_secondaryServerInstanceName(secondaryServerInstanceName)
{
}

bool ApplicationInstanceNameCheck::canfix()
{
    return false ;
}

SVERROR ApplicationInstanceNameCheck::fix()
{
    return SVS_FALSE ;
}

SVERROR ApplicationInstanceNameCheck::evaluate()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    InmRuleStatus status ;
    std::stringstream resultStram ;
    DebugPrintf(SV_LOG_DEBUG, "Primary NAme: %s\n", m_primaryServerInstanceName.c_str());
    if( strcmpi(m_primaryServerInstanceName.c_str(), m_secondaryServerInstanceName.c_str()) == 0 )
    {
        resultStram << " MSSQL server instance is available in target host " << std::endl ;
        resultStram << "Instance Name: " << m_primaryServerInstanceName << std::endl ;
        status = INM_RULESTAT_PASSED;
        setRuleExitCode(RULE_PASSED);
        bRet = SVS_OK ;
    }
    else
    {

        resultStram << " Primary Server Instance Name is not Found in this machine "<< std::endl;
        resultStram << " Instance Name: " << m_primaryServerInstanceName << std::endl ;
        status = INM_RULESTAT_FAILED;
        setRuleExitCode(SQL_INSTANCENAME_MISMATCH_ERROR);
    }
    setStatus(status) ;
    setRuleMessage(resultStram.str()) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
ApplicationInstallationVolumeCheck::ApplicationInstallationVolumeCheck(const std::string& name, 
                                                                       const std::string& description, 
                                                                       const std::map<std::string, std::string> & instalMountPointMap,
                                                                       const std::list<std::string> & srcDataVols,
                                                                       InmRuleIds ruleId)
                                                                       :m_installMountPointMap(instalMountPointMap),
                                                                       m_srcDataVols(srcDataVols),
                                                                       Validator(name, description, ruleId)

{

}
ApplicationInstallationVolumeCheck::ApplicationInstallationVolumeCheck(const std::map<std::string, std::string> & instalMountPointMap,
                                                                       const std::list<std::string> & srcDataVols,
                                                                       InmRuleIds ruleId)
                                                                       :m_installMountPointMap(instalMountPointMap),
                                                                       m_srcDataVols(srcDataVols),
                                                                       Validator(ruleId)
{

}
SVERROR ApplicationInstallationVolumeCheck::evaluate()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    InmRuleStatus status ;
    std::stringstream resultStram ;
    InmRuleErrorCode ruleErrorCode ;
    MSSQLApplicationPtr sqlAppPtr( new MSSQLApplication() ) ;
    if( sqlAppPtr->isInstalled() )
    {
        sqlAppPtr->discoverApplication() ;
        std::map<std::string, std::string>::const_iterator installMountPointIter = m_installMountPointMap.begin() ;
        while( installMountPointIter  != m_installMountPointMap.end() )
        {
            MSSQLInstanceDiscoveryInfo discInfo ;
            if( sqlAppPtr->getDiscoveredDataByInstance(installMountPointIter->first, discInfo) == SVS_OK )
            {
                char installVolume[1024] ;
                if( GetVolumePathName( discInfo.m_installPath.c_str(), installVolume, 1024) == TRUE )
                {
                    if( strcmpi(installVolume, installMountPointIter->second.c_str() ) == 0 )
                    {
                        resultStram << "source and target install volumes are " << discInfo.m_installPath <<std::endl ;
                        status = INM_RULESTAT_PASSED ;
                        ruleErrorCode = RULE_PASSED ;
                        bRet = SVS_OK ;
                    }
                    else
                    {
                        if( std::find( m_srcDataVols.begin() , m_srcDataVols.end(), installVolume ) == m_srcDataVols.end() )
                        {
                            resultStram << "source install volumes are " << discInfo.m_installPath << std::endl ;
                            resultStram << "targetinstall volumes are " << discInfo.m_installPath ;
                            status = INM_RULESTAT_PASSED ;
                            ruleErrorCode = RULE_PASSED ;
                            bRet = SVS_OK ;
                        }
                    }
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "SVGetVolumePathName failed with error for ", discInfo.m_installPath.c_str()) ;
                    status = INM_RULESTAT_FAILED;
                    ruleErrorCode = SQL_INSTALLPATH_ERROR ;
                    resultStram << "SVGetVolumePathName failed with error for " << discInfo.m_installPath << std::endl;
                    break ;
                }
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to get the discovery information for the instance %s\n", installMountPointIter->first.c_str()) ;
                status = INM_RULESTAT_FAILED;
                ruleErrorCode = SQL_INSTALLPATH_ERROR ;
                resultStram << "Failed to get the discovery information for the instance " << installMountPointIter->first << std::endl;
                break ;
            }
            installMountPointIter  ++ ;
        }
    }
    else
    {
        status = INM_RULESTAT_FAILED;
        ruleErrorCode = SQL_INSTALLPATH_ERROR ;
        resultStram << "Failed to find sql installation on local machine " << std::endl;
    }
    setRuleExitCode(ruleErrorCode) ;
    setStatus(status) ;
    setRuleMessage(resultStram.str()) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
SVERROR ApplicationInstallationVolumeCheck::fix()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return SVS_FALSE ;
}

bool ApplicationInstallationVolumeCheck::canfix()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return false; 
} 
