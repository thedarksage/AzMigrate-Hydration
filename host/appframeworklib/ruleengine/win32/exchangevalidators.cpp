#include "exchangevalidators.h"
#include "host.h"
#include "util/common/util.h"
#include <sstream>
#include "system.h"
#include "portablehelpersmajor.h"




ExchangeHostNameValidator::ExchangeHostNameValidator(const std::string& name, 
                                                     const std::string& description, 
                                                     const ExchAppVersionDiscInfo & discInfo, 
                                                     const ExchangeMetaData& data, 
                                                     const std::string& hostname,
                                                     InmRuleIds ruleId)
:Validator(name, description, ruleId),
m_discInfo(discInfo),
metadata(data),
m_hostname(hostname)
{
}

ExchangeHostNameValidator::ExchangeHostNameValidator(const ExchAppVersionDiscInfo & discInfo, 
                                                     const ExchangeMetaData& data, 
                                                     const std::string& hostname,
                                                     InmRuleIds ruleId)
:Validator(ruleId),
m_discInfo(discInfo),
metadata(data),
m_hostname(hostname)
{
}

ExchangeHostNameValidator::ExchangeHostNameValidator(const std::string& name, 
                                                     const std::string& description, 
                                                     const ExchAppVersionDiscInfo & discInfo, 
                                                     const Exchange2k10MetaData& data, 
                                                     const std::string& hostname,
                                                     InmRuleIds ruleId)
:Validator(name, description, ruleId),
m_discInfo(discInfo),
metadata2(data),
m_hostname(hostname)
{
}

ExchangeHostNameValidator::ExchangeHostNameValidator(const ExchAppVersionDiscInfo & discInfo, 
                                                     const Exchange2k10MetaData& data, 
                                                     const std::string& hostname,
                                                     InmRuleIds ruleId)
:Validator(ruleId),
m_discInfo(discInfo),
metadata2(data),
m_hostname(hostname)
{
}

SVERROR ExchangeHostNameValidator::evaluate()
{
    SVERROR bRet = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    InmRuleStatus ruleStatus = INM_RULESTAT_NONE ;
    InmRuleErrorCode errorCode = INM_ERROR_NONE ;
    std::stringstream stream ;
    if(m_discInfo.m_appType == INM_APP_EXCH2003 || 
        m_discInfo.m_appType == INM_APP_EXCH2007 || 
        m_discInfo.m_appType == INM_APP_EXCHNAGE )
    {
        std::list<ExchangeStorageGroupMetaData>::const_iterator &storageGrpMetaDataIter = metadata.m_storageGrps.begin() ;
        while (storageGrpMetaDataIter != metadata.m_storageGrps.end() )
        {
            const ExchangeStorageGroupMetaData& storageGrpMetaData = *storageGrpMetaDataIter ;
            if( strcmpi(storageGrpMetaData.m_storageGrpName.c_str(), m_hostname.c_str()) == 0 )
            {
                stream << "Host name is matching with one of the storage group name" << std::endl ;
                ruleStatus = INM_RULESTAT_FAILED ;
                break ;
            }
            std::list<ExchangeMDBMetaData>::const_iterator mdbIter = storageGrpMetaData.m_mdbMetaDataList.begin() ;
            while( mdbIter != storageGrpMetaData.m_mdbMetaDataList.end() )
            {
                DebugPrintf(SV_LOG_DEBUG, "Mail Store Name %s\n", mdbIter->m_mdbName.c_str()) ;
                if( strcmpi(mdbIter->m_mdbName.c_str(), m_hostname.c_str()) == 0 )
                {
                    stream << "Host Name is matching with one of the mail store name under storage group " << storageGrpMetaData.m_storageGrpName.c_str() << std::endl ;
                    ruleStatus = INM_RULESTAT_FAILED ;
                    break ;
                }
                mdbIter ++ ;
            }
            if( mdbIter != storageGrpMetaData.m_mdbMetaDataList.end() )
            {
                ruleStatus = INM_RULESTAT_FAILED ;
                break ;
            }
            storageGrpMetaDataIter++ ;    
        }
    }
    else if(m_discInfo.m_appType == INM_APP_EXCH2010)
    {
        std::list<ExchangeMDBMetaData>::const_iterator mdbIter = metadata2.m_mdbMetaDataList.begin();
        while( mdbIter != metadata2.m_mdbMetaDataList.end() )
        {
            if( strcmpi(mdbIter->m_mdbName.c_str(), m_hostname.c_str()) == 0 )
            {
                stream << "Host name: "<< m_hostname << " matching with the Mail Store: " << mdbIter->m_mdbName << std::endl ;
                ruleStatus = INM_RULESTAT_FAILED ;
                break ;
            }
            mdbIter ++ ;
        }
    }

    if( strcmpi("InformationStore", m_hostname.c_str()) == 0 )
    {
        stream << "Host name: "<< m_hostname << " matching with \"InformationStore\" " << std::endl;
        ruleStatus = INM_RULESTAT_FAILED ;

    }
    if( strcmpi("Microsoft", m_hostname.c_str()) == 0 )
    {
        stream << "Host name: "<< m_hostname << " matching with \"Microsoft\" " << std::endl;
        ruleStatus = INM_RULESTAT_FAILED ;

    }
    if( strcmpi("Exchange", m_hostname.c_str()) == 0 )
    {
        stream << "Host name: "<< m_hostname << " matching with \"Exchange\" " << std::endl;
        ruleStatus = INM_RULESTAT_FAILED ;

    }
    if( strcmpi("Services", m_hostname.c_str()) == 0 )
    {
        stream << "Host name: "<< m_hostname << " matching with \"Services\" " << std::endl;
        ruleStatus = INM_RULESTAT_FAILED ;
    }
    if( strcmpi("Configuration", m_hostname.c_str()) == 0 )
    {
        stream << "Host name: "<< m_hostname << " matching with \"Configuration\" " << std::endl;
        ruleStatus = INM_RULESTAT_FAILED ;
    }
    if( strcmpi("Groups", m_hostname.c_str()) == 0 )
    {
        stream << "Host name: "<< m_hostname << " matching with \"Groups\" " << std::endl;
        ruleStatus = INM_RULESTAT_FAILED ;
    }
    if( strcmpi("Administrative", m_hostname.c_str()) == 0 )
    {
        stream << "Host name: "<< m_hostname << " matching with \"Administrative\" " << std::endl;
        ruleStatus = INM_RULESTAT_FAILED ;
    }
    if( ruleStatus != INM_RULESTAT_FAILED )
    {
        stream << m_hostname << " is not from the prohibited names list for exchange to failover/failback" <<std::endl ;
        setRuleExitCode(RULE_PASSED);
        setStatus( INM_RULESTAT_PASSED )  ;
    }
    else
    {
        setRuleExitCode(HOSTNAME_MATCH_ERROR);
        setStatus( INM_RULESTAT_FAILED )  ;
    }

	setRuleMessage(stream.str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR ExchangeHostNameValidator::fix()
{
    SVERROR bRet = SVS_FALSE ;
    return bRet ;
}
bool ExchangeHostNameValidator::canfix()
{
    bool bRet = false ;
    return bRet ;
}



ExchangeNestedMountPointValidator::ExchangeNestedMountPointValidator(const std::string& name, 
                                const std::string& description, 
                                const ExchAppVersionDiscInfo & discInfo, 
                                const ExchangeMetaData& data, 
                                InmRuleIds ruleId)
:
Validator(name, description, ruleId),
m_info(discInfo),
m_data(data)
{
}

ExchangeNestedMountPointValidator::ExchangeNestedMountPointValidator(const ExchAppVersionDiscInfo & discInfo, 
                                const ExchangeMetaData& data, 
                                InmRuleIds ruleId)
:
Validator(ruleId),
m_info(discInfo),
m_data(data)
{
}

ExchangeNestedMountPointValidator::ExchangeNestedMountPointValidator(const std::string& name, 
                                const std::string& description, 
                                const ExchAppVersionDiscInfo& discInfo, 
								const Exchange2k10MetaData& data, InmRuleIds ruleId)
:
Validator(name, description, ruleId),
m_info(discInfo),
m_data2(data)
{
}

ExchangeNestedMountPointValidator::ExchangeNestedMountPointValidator(const ExchAppVersionDiscInfo& discInfo, 
								const Exchange2k10MetaData& data, InmRuleIds ruleId)
:
Validator(ruleId),
m_info(discInfo),
m_data2(data)
{
}

SVERROR ExchangeNestedMountPointValidator::evaluate()
{
    SVERROR bRet = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    InmRuleErrorCode errorCode = INM_ERROR_NONE ;
    InmRuleStatus ruleStatus = INM_RULESTAT_NONE ;
    std::list<std::string>::const_iterator volIter;
    std::list<std::string>::const_iterator volIterEnd;
    std::stringstream stream ;
    if( this->m_info.m_appType == INM_APP_EXCH2003 || 
        this->m_info.m_appType == INM_APP_EXCH2007 || 
        this->m_info.m_appType == INM_APP_EXCHNAGE )
    {
        volIter = m_data.m_volumeList.begin() ;
        volIterEnd = m_data.m_volumeList.end();
    }
    else if( this->m_info.m_appType == INM_APP_EXCH2010 )
    {
        volIter = m_data2.m_volumePathNameList.begin() ;
        volIterEnd = m_data2.m_volumePathNameList.end();
    }	
    bool isNested = false ;
    while( volIter != volIterEnd )
    {
        size_t index1 = 3 ;
        char pszVolName[256] ;
        if( volIter->length() > 3 )
        {
            while( true )
            {
                size_t index2 = volIter->find_first_of("\\", index1) ;
				index2++;
				if( index2 != volIter->length() && index2 != std::string::npos )
                {
                    memset(pszVolName, '\0', 256) ;
                    std::string pathname = volIter->substr(0, index2-1) ;
                    DebugPrintf(SV_LOG_DEBUG, "path name %s index1:%d index2:%d\n", pathname.c_str(),index1, index2) ;
                    if( GetVolumePathName(pathname.c_str(), pszVolName, 256) == 0 )
					{
						stream << "Failed to get the volume name for path :"<<pathname;
						errorCode = GET_VOLUME_NAME_FAILED_ERROR ;
						ruleStatus = INM_RULESTAT_FAILED ;
						break ;
					}
					else
					{
						DebugPrintf(SV_LOG_DEBUG, "GetVolumePathName returned %s\n", pszVolName) ;
						if( isMointPointExist(pszVolName) )
						{
							stream << volIter->c_str() << " is a nested mount point" <<std::endl ;
							isNested = true ;
							errorCode = NESTED_MOUNT_POINT_ERROR ;
							ruleStatus = INM_RULESTAT_FAILED ;
							break ;
						}
					}          
                }
				else
				{
					break;
				}
				index1 = index2 ;
            }
        }
        volIter++ ;
    }
    if( isNested == true   )
    {
        setStatus(ruleStatus) ;
        setRuleExitCode(errorCode);
    }
    else
    {
        stream << "There are no nested mount points" ;
        setStatus(INM_RULESTAT_PASSED) ;
        setRuleExitCode(RULE_PASSED);
    }
    setRuleMessage(stream.str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR ExchangeNestedMountPointValidator::fix()
{
    SVERROR bRet = SVS_FALSE ;
    return bRet ;
}
bool ExchangeNestedMountPointValidator::canfix()
{
    bool bRet = false ;
    return bRet ;
}

bool ExchangeNestedMountPointValidator::isMointPointExist(const std::string & mountPoint)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool bIsMountPointFound = false;
	std::list<std::string>::iterator iterMountPoint;
	std::list<std::string>::iterator iterEnd;
	do
	{
		if( m_info.m_appType == INM_APP_EXCH2003 ||
			m_info.m_appType == INM_APP_EXCH2007 ||
			m_info.m_appType == INM_APP_EXCHNAGE)
		{
			iterMountPoint = m_data.m_volumeList.begin();
			iterEnd = m_data.m_volumeList.end();
		}
		else if( m_info.m_appType == INM_APP_EXCH2010)
		{
			iterMountPoint = m_data2.m_volumePathNameList.begin();
			iterEnd = m_data2.m_volumePathNameList.end();
		}
		else
		{
			DebugPrintf(SV_LOG_WARNING,"Un Supported Exchange application Version. Application Type %d\n",m_info.m_appType);
			break;
		}
		while(iterMountPoint != iterEnd)
		{
			if(strcmpi(mountPoint.c_str(),iterMountPoint->c_str()) == 0 )
			{
				bIsMountPointFound = true;
				break;
			}
			iterMountPoint++;
		}
	}while(false);
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return bIsMountPointFound;
}

ExchangeDataOnSysDrive::ExchangeDataOnSysDrive(const std::string& name, 
                        const std::string& description, 
                        const ExchAppVersionDiscInfo & discInfo, 
                        const ExchangeMetaData& data, 
                        const std::string& sysDrive,
                        InmRuleIds ruleId)
:
Validator(name, description, ruleId),
m_info(discInfo),
m_data(data),
m_sysDrive(sysDrive)
{
}

ExchangeDataOnSysDrive::ExchangeDataOnSysDrive(const ExchAppVersionDiscInfo & discInfo, 
                                               const ExchangeMetaData& data, 
                                               const std::string& sysDrive,
                                               InmRuleIds ruleId)
:
Validator(ruleId),
m_info(discInfo),
m_data(data),
m_sysDrive(sysDrive)
{
}

ExchangeDataOnSysDrive::ExchangeDataOnSysDrive(const std::string& name, 
                        const std::string& description, 
                        const ExchAppVersionDiscInfo & discInfo, 
                                               const Exchange2k10MetaData& data, 
                                               const std::string& sysDrive,
                                               InmRuleIds ruleId)
:
Validator(name, description, ruleId),
m_info(discInfo),
m_data2(data),
m_sysDrive(sysDrive)
{
}

ExchangeDataOnSysDrive::ExchangeDataOnSysDrive(const ExchAppVersionDiscInfo & discInfo, 
                                               const Exchange2k10MetaData& data, 
                                               const std::string& sysDrive,
                                               InmRuleIds ruleId)
:
Validator(ruleId),
m_info(discInfo),
m_data2(data),
m_sysDrive(sysDrive)
{
}

SVERROR ExchangeDataOnSysDrive::evaluate()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    InmRuleStatus ruleStatus = INM_RULESTAT_NONE ;
    InmRuleErrorCode errorCode = INM_ERROR_NONE ;
    std::stringstream stream ;

    if( m_sysDrive.length() == 2 )
    {
        m_sysDrive += "\\" ;
        }
        std::list<std::string>::const_iterator iterbegin; 
        std::list<std::string>::const_iterator iterend;
		bool bFlag = false;
		if(m_info.m_appType == INM_APP_EXCH2003 || 
            m_info.m_appType == INM_APP_EXCH2007 ||
            m_info.m_appType == INM_APP_EXCHNAGE )
		{
		iterbegin = find( m_data.m_volumeList.begin(), m_data.m_volumeList.end(), m_sysDrive) ;
			if( iterbegin != m_data.m_volumeList.end() )
			{
				errorCode = APPLICATION_DATA_ON_SYSTEM_DRIVE_ERROR ;
			}
 		}
		else if(m_info.m_appType == INM_APP_EXCH2010)
		{
		iterbegin = find( m_data2.m_volumePathNameList.begin(), m_data2.m_volumePathNameList.end(), m_sysDrive) ;
			if( iterbegin != m_data2.m_volumePathNameList.end() )
			{
				errorCode = APPLICATION_DATA_ON_SYSTEM_DRIVE_ERROR ;
			}
		}
        if( errorCode == APPLICATION_DATA_ON_SYSTEM_DRIVE_ERROR )
        {
            stream << "System Drive Letter " << m_sysDrive << std::endl ;
                stream << "Exchange Data is on System Drive" << std::endl ;
                ruleStatus = INM_RULESTAT_FAILED ;
        }
        else
        {
            stream << "System Drive Letter " << m_sysDrive << std::endl ;
                stream << "Exchange Data is not on System Drive" << std::endl ;
                ruleStatus = INM_RULESTAT_PASSED ;
                errorCode = RULE_PASSED ;
        }

    setRuleMessage(stream.str()) ;
    setRuleExitCode(errorCode) ;
    setStatus( ruleStatus ) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
SVERROR ExchangeDataOnSysDrive::fix()
{
    SVERROR bRet = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
bool ExchangeDataOnSysDrive::canfix()
{
    bool bRet = false ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

LcrStatusCheck::LcrStatusCheck(const std::string& name, 
                const std::string& description, 
                const ExchangeMetaData& exchMetaData,
                InmRuleIds ruleId)
:Validator(name, description, ruleId),
m_exchMetaData(exchMetaData)
{
}

LcrStatusCheck::LcrStatusCheck(const ExchangeMetaData& exchMetaData,
                InmRuleIds ruleId)
:Validator(ruleId),
m_exchMetaData(exchMetaData)
{
}

SVERROR LcrStatusCheck::evaluate()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_OK ;
    std::list<ExchangeStorageGroupMetaData>::const_iterator& storageGrpIter = m_exchMetaData.m_storageGrps.begin() ;
    std::stringstream stream ;
    InmRuleErrorCode errorCode = INM_ERROR_NONE ;
    InmRuleStatus ruleStatus = INM_RULESTAT_NONE ;
    while( storageGrpIter != m_exchMetaData.m_storageGrps.end() )
    {
        if( storageGrpIter->m_lcrEnabled )
        {
            stream << "Local Continuous Replication (LCR) is enabled for "<< storageGrpIter->m_storageGrpName << std::endl ;
            errorCode = LCR_ENABLED_ERROR ;
            ruleStatus = INM_RULESTAT_FAILED ;
            break ;
        }
        storageGrpIter++ ;
    }
    if( ruleStatus != INM_RULESTAT_FAILED  )
    {
		errorCode = RULE_PASSED ;
        ruleStatus = INM_RULESTAT_PASSED  ;
        stream << "No storage Group is having Local Continuous Replication Setting Enabled" ;
    }
    setRuleMessage(stream.str());
    setStatus(ruleStatus) ;
    setRuleExitCode(errorCode) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ; 
}

SVERROR LcrStatusCheck::fix()
{
    return SVS_FALSE ;
}
bool LcrStatusCheck::canfix()
{
    return false ;
}



CcrStatusCheck::CcrStatusCheck(const std::string& name, 
                                const std::string& description, 
                                const ExchAppVersionDiscInfo discoveryInfo,
                                InmRuleIds ruleId)
:Validator(name, description, ruleId),
m_discInfo(discoveryInfo)
{
}

CcrStatusCheck::CcrStatusCheck(const ExchAppVersionDiscInfo discoveryInfo,
                                InmRuleIds ruleId)
:Validator(ruleId),
m_discInfo(discoveryInfo)
{
}
SVERROR CcrStatusCheck::evaluate()
{
    SVERROR bRet = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::stringstream stream ;
    InmRuleStatus ruleStatus = INM_RULESTAT_NONE ;
    InmRuleErrorCode errorCode = INM_ERROR_NONE ;
    if( m_discInfo.m_clusterStorageType == 1 )
    {
        ruleStatus = INM_RULESTAT_FAILED ;
        errorCode = CCR_ENABLED_ERROR ;
        stream << "CCR is enabled on the machine.. Storage Groups May not mount if CCR is enabled\n" ;
    }
    else
    {
        stream << "CCR is Disabled" ;
        ruleStatus = INM_RULESTAT_PASSED ;
		errorCode = RULE_PASSED ;
    }
    setRuleMessage(stream.str());
    setStatus(ruleStatus) ;
    setRuleExitCode(errorCode) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
SVERROR CcrStatusCheck::fix()
{
    return SVS_FALSE ;
}
bool CcrStatusCheck::canfix()
{
    return false ;
}

void findAdDn(const std::string& str, std::string& ad, std::string& dc)
{
	std::string delims = "," ;
	std::string::size_type lastPos = str.find_first_not_of(delims, 0);
	std::string::size_type pos = str.find_first_of(delims, lastPos);
	while (std::string::npos != pos || std::string::npos != lastPos)
	{
		std::string token = str.substr(lastPos, pos - lastPos);
		if( strstr(token.c_str(), "CN=Exchange Administrative Group") != NULL )
		{
			ad = token ;
		}
		if( strstr(token.c_str(), "DC=") != NULL )
		{
			dc += token ;
			dc += "," ;
		}
		lastPos = str.find_first_not_of(delims, pos);
		pos = str.find_first_of(delims, lastPos);
	}

}


ExchangeAdminGroupEqualityValidator::ExchangeAdminGroupEqualityValidator(const std::string& name, 
									 const std::string& description, 
                                     const std::string & srcDn,
                                     const std::string& tgtDn,
                                     InmRuleIds ruleId)
:Validator(name, description, ruleId),
m_srcDn(srcDn),
m_tgtDn(tgtDn)
{
}

ExchangeAdminGroupEqualityValidator::ExchangeAdminGroupEqualityValidator( const std::string & srcDn,
                                                                         const std::string& tgtDn,
                                     InmRuleIds ruleId)
:Validator(ruleId),
m_srcDn(srcDn),
m_tgtDn(tgtDn)
{
}

SVERROR ExchangeAdminGroupEqualityValidator::evaluate()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;

	SVERROR bRet = SVS_FALSE ;
	std::stringstream resultStr ;
	InmRuleStatus ruleStatus = INM_RULESTAT_PASSED ;
    InmRuleErrorCode errorCode = INM_ERROR_NONE ;
	if(strcmpi(m_tgtDn.c_str(), "") != 0)
	{
		std::string srcAdminGrp, srcDc ;
		std::string tgtAdminGrp, tgtDc ;
		findAdDn(m_srcDn,srcAdminGrp, srcDc) ;
		findAdDn(m_tgtDn,tgtAdminGrp, tgtDc) ;
		if(strcmpi(srcAdminGrp.c_str(), tgtAdminGrp.c_str()) == 0)
		{
			resultStr << "Administrative Group Name is matching\n" ;
			resultStr << "Administrative Group Name :" << srcAdminGrp << "\n" ;
            errorCode = RULE_PASSED ;
            ruleStatus = INM_RULESTAT_PASSED ;
		}
		else
		{
			resultStr << "Administrative Group Name is not matching\n" ;
			resultStr << "Src Administrative Group Name :" << srcAdminGrp << "\n" ;
			resultStr << "Tgt Administrative Group Name :" << tgtAdminGrp << "\n" ;
            ruleStatus = INM_RULESTAT_FAILED ;
            errorCode = EXCHANGE_ADMINGROUP_MISMATCH_ERROR  ;
		}
	}
	else
	{
		resultStr << "Exchange not installed on target" ;
        ruleStatus = INM_RULESTAT_FAILED ;
        errorCode = EXCHANGE_ADMINGROUP_MISMATCH_ERROR  ;

	}
	setStatus(ruleStatus)  ;
    setRuleExitCode(errorCode) ;
    setRuleMessage(resultStr.str()) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet;
}

SVERROR ExchangeAdminGroupEqualityValidator::fix()
{
    return SVS_FALSE ;
}
bool ExchangeAdminGroupEqualityValidator::canfix()
{
    return false ;
}

ExchangeDCEqualityValidator::ExchangeDCEqualityValidator(const std::string& name, 
									 const std::string& description, 
                                     const std::string & srcDn,
                                     const std::string& tgtDn,
                                     InmRuleIds ruleId)
:Validator(name, description, ruleId),
m_srcDn(srcDn),
m_tgtDn(tgtDn)
{
}

ExchangeDCEqualityValidator::ExchangeDCEqualityValidator( const std::string & srcDn,
                                                         const std::string& tgtDn,
                                     InmRuleIds ruleId)
:Validator(ruleId),
m_srcDn(srcDn),
m_tgtDn(tgtDn)
{
}
SVERROR ExchangeDCEqualityValidator::evaluate()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;

	SVERROR bRet = SVS_FALSE ;
	std::stringstream resultStr ;
	InmRuleStatus ruleStatus = INM_RULESTAT_NONE ;
    InmRuleErrorCode errorCode = INM_ERROR_NONE ;

	if(strcmpi(m_tgtDn.c_str(), "") != 0)
	{
		std::string srcAdminGrp, srcDc ;
		std::string tgtAdminGrp, tgtDc ;
		findAdDn(m_srcDn,srcAdminGrp, srcDc) ;
		findAdDn(m_tgtDn,tgtAdminGrp, tgtDc) ;
		if(strcmpi(srcDc.c_str(),tgtDc.c_str()) == 0)
		{
			resultStr << "DC is matching\n" ;
			resultStr << "DC  Name :" << tgtDc << "\n" ;
            errorCode = RULE_PASSED ;
            ruleStatus = INM_RULESTAT_PASSED ;
		}
		else
		{
			resultStr << "DC is not matching\n" ;
			resultStr << "Src DC Name :" << srcDc << "\n" ;
			resultStr << "Tgt DC Name :" << tgtDc << "\n" ;
			ruleStatus = INM_RULESTAT_FAILED ;
            errorCode = EXCHANGE_DC_ERROR ;
		}
	}
	else
	{
		resultStr << "Exchange not installed on target" ;
        errorCode = EXCHANGE_DC_ERROR ;
        ruleStatus = INM_RULESTAT_FAILED ;
	}
	setStatus(ruleStatus) ;
    setRuleMessage(resultStr.str()) ;
    setRuleExitCode(errorCode) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet;
}

SVERROR ExchangeDCEqualityValidator::fix()
{
    return SVS_FALSE ;
}
bool ExchangeDCEqualityValidator::canfix()
{
    return false ;
}
/*
ExchangeMTAEqualityValidator::ExchangeMTAEqualityValidator(const std::string& name, 
									 const std::string& description, 
                                     bool & isSrcMTA,
                                     bool & isTgtMTA,
                                     InmRuleIds ruleId)
:Validator(name, description, ruleId),
m_isSrcMTA(isSrcMTA),
m_isTgtMTA(isTgtMTA)
{
}

ExchangeMTAEqualityValidator::ExchangeMTAEqualityValidator( bool & isSrcMTA,
                                                         bool & isTgtMTA,
														 InmRuleIds ruleId)
:Validator(ruleId),
m_isSrcMTA(isSrcMTA),
m_isTgtMTA(isTgtMTA)
{
}

SVERROR ExchangeMTAEqualityValidator::evaluate()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;

	SVERROR bRet = SVS_FALSE ;
	std::stringstream resultStr ;
	InmRuleStatus ruleStatus = INM_RULESTAT_NONE ;
    InmRuleErrorCode errorCode = INM_ERROR_NONE ;

	if( m_isSrcMTA == m_isTgtMTA )
	{
		resultStr << "MTA status of Target is matching with Source\n" ;
        errorCode = RULE_PASSED ;
        ruleStatus = INM_RULESTAT_PASSED ;
	}
	else
	{
		resultStr << "MTA status of Target is not matching with Source\n" ;
		ruleStatus = INM_RULESTAT_FAILED ;
        errorCode = EXCHANGE_MTA_MISMATCH_ERROR ;
	}
	setStatus(ruleStatus) ;
    setRuleMessage(resultStr.str()) ;
    setRuleExitCode(errorCode) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet;
}

SVERROR ExchangeMTAEqualityValidator::fix()
{
    return SVS_FALSE ;
}
bool ExchangeMTAEqualityValidator::canfix()
{
    return false ;
}
*/
SingleMailStoreCopyRule::SingleMailStoreCopyRule( const std::string& name, 
												const std::string& description, 
												const ExchAppVersionDiscInfo & discInfo, 
												const Exchange2k10MetaData& data,
												InmRuleIds ruleId )
:Validator(name, description, ruleId),
m_info(discInfo),
m_data2(data)
{
}

SingleMailStoreCopyRule::SingleMailStoreCopyRule( const ExchAppVersionDiscInfo & discInfo, 
												const Exchange2k10MetaData& data,
												InmRuleIds ruleId )
:Validator(ruleId),
m_info(discInfo),
m_data2(data)
{
}
SVERROR SingleMailStoreCopyRule::evaluate()
{
    SVERROR bRet = SVS_OK ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::stringstream resultStream;
	InmRuleStatus ruleStatus = INM_RULESTAT_PASSED ;
    InmRuleErrorCode errorCode = RULE_PASSED ;
	std::list<ExchangeMDBMetaData>::iterator Exchange2k10MetaDataIter;
	Exchange2k10MetaDataIter = m_data2.m_mdbMetaDataList.begin();
	while( Exchange2k10MetaDataIter != m_data2.m_mdbMetaDataList.end() )
	{
		if( Exchange2k10MetaDataIter->m_exch2k10MDBmetaData.m_exchangeMDBCopysList.size() > 1 )
		{
			resultStream << " MailStore: "<< Exchange2k10MetaDataIter->m_mdbName ;
			resultStream <<	" Having copies on: " ;
			std::list<std::string>::iterator copyHostListIter ;
			copyHostListIter = Exchange2k10MetaDataIter->m_exch2k10MDBmetaData.m_exchangeMDBCopysList.begin();
			while( copyHostListIter != Exchange2k10MetaDataIter->m_exch2k10MDBmetaData.m_exchangeMDBCopysList.end() )
			{
				resultStream << *copyHostListIter << ", " ;
				copyHostListIter++;
				ruleStatus = INM_RULESTAT_FAILED ;
				errorCode = DUPLICATE_MAILSTORE_COPY_ERROR ;
				bRet = SVS_FALSE;
			}
			resultStream << std::endl;
		}
		Exchange2k10MetaDataIter++;
	}
	if(resultStream.str().empty())
	{
		resultStream << "Non of the mail stores are having multiple copies." << std::endl;
	}
    setStatus(ruleStatus) ;
    setRuleExitCode(errorCode) ;
    setRuleMessage(resultStream.str()) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
SVERROR SingleMailStoreCopyRule::fix()
{
    SVERROR bRet = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
bool SingleMailStoreCopyRule::canfix()
{
    bool bRet = false ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SCRStatusCheck::SCRStatusCheck(const std::string& name, 
                                const std::string& description, 
                                const ExchangeMetaData& exchMetaData, 
                                InmRuleIds ruleId)
:Validator(name, description, ruleId),
m_exchMetaData(exchMetaData)
{

}

SCRStatusCheck::SCRStatusCheck(const ExchangeMetaData& exchMetaData, 
                                InmRuleIds ruleId)
:Validator(ruleId),
m_exchMetaData(exchMetaData)
{

}
SVERROR SCRStatusCheck::evaluate()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_OK ;
    std::list<ExchangeStorageGroupMetaData>::const_iterator& storageGrpIter = m_exchMetaData.m_storageGrps.begin() ;
    std::stringstream stream ;
    InmRuleStatus ruleStatus = INM_RULESTAT_NONE ;
    InmRuleErrorCode errorCode = INM_ERROR_NONE ;

    while( storageGrpIter != m_exchMetaData.m_storageGrps.end() )
    {
		if( !storageGrpIter->m_msExchStandbyCopyMachines.empty()  )
        {
            stream << "standby copy machine is : "<< storageGrpIter->m_msExchStandbyCopyMachines << std::endl ;
            stream << "Storage Group Name : " << storageGrpIter->m_storageGrpName  << std::endl ;
            stream << "Storage Groups May not mount after failover if SCR is enabeld " << std::endl ;
            ruleStatus = INM_RULESTAT_FAILED ;
            errorCode = SCR_ENABLED_ERROR ;
            break ;
        }
        storageGrpIter++ ;
    }
    if( storageGrpIter == m_exchMetaData.m_storageGrps.end() )
    {
        stream << "No storage Group is having SCR Setting Enabled" ;
        ruleStatus = INM_RULESTAT_PASSED  ;
        errorCode = RULE_PASSED ;
    }
    setStatus(ruleStatus) ;
    setRuleMessage(stream.str()) ;
    setRuleExitCode(errorCode) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ; 
}

SVERROR SCRStatusCheck::fix()
{
    return SVS_FALSE ;
}
bool SCRStatusCheck::canfix()
{
    return false ;
}