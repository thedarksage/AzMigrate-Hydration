#include "mssql/mssqlapplication.h"
#include "rulefactory.h"
#include "systemvalidators.h"
#include "mssqlvalidators.h"
#include "fileservervalidators.h"
#include "inmagevalidator.h"
#include "system.h"
#include <list>
#include "exchangevalidators.h"
#include "clusterutil.h"
#include "service.h"
#include "discoveryandvalidator.h"

std::list<ValidatorPtr>RuleFactory::getSystemRules(enum SCENARIO_TYPE scenarioType,
                                                   enum CONTEXT_TYPE contextType,
												   const std::string& appType,
												   bool bDnsFailover,
                                                   std::string srchostnameforPermissionChecks,
												   const std::string& tgthostnameforPermissionChecks,
												   bool bAtTarget,
                                                   bool isLocalContext)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<ValidatorPtr> preReqvalidatorList ;

    ValidatorPtr validatorPtr ;
    if( isLocalContext )
    {
        std::list<std::string> serviceNames ;
        std::list<InmService> services ;
        InmService svagents("svagents") ;
        QuerySvcConfig(svagents) ;
        services.push_back(svagents) ;

        if( scenarioType != SCENARIO_FASTFAILBACK )
        {
            InmService vss("VSS") ;
            if( !bAtTarget )
            {
            	QuerySvcConfig(vss) ;
                services.push_back(vss) ;
				validatorPtr.reset(new VSSRollupCheckValidator(VSS_ROLEUP_CHECK)) ;
				preReqvalidatorList.push_back(validatorPtr) ;
            }
        }
        std::list<InmRuleIds> dependentRules ;
        if(scenarioType != SCENARIO_FAILBACK)
        {
            validatorPtr.reset(new ServiceValidator(services, INM_SVCSTAT_RUNNING, SERVICE_STATUS_CHECK)) ;
            preReqvalidatorList.push_back(validatorPtr) ;

            validatorPtr.reset(new InmNATIPValidator(CONFIGURED_IP_CHECK)) ;
            preReqvalidatorList.push_back(validatorPtr) ;
        }

        if( srchostnameforPermissionChecks.compare("") == 0 ) 
        {
            srchostnameforPermissionChecks = Host::GetInstance().GetHostName();
        }
		if(bDnsFailover)
		{
			if(scenarioType == SCENARIO_FASTFAILBACK)
			{
				if(!bAtTarget)
				{
					validatorPtr.reset(new DNSAvailabilityValidator(DNS_AVAILABILITY_CHECK)) ;
					//validatorPtr->setDependentRules(dependentRules) ;
					preReqvalidatorList.push_back(validatorPtr) ;
					validatorPtr.reset(new DNSUpdatePrivilageValidator(srchostnameforPermissionChecks, DNS_UPDATE_CHECK)) ;
					dependentRules.clear() ;
					dependentRules.push_back(DNS_AVAILABILITY_CHECK) ;
					validatorPtr->setDependentRules(dependentRules) ;
					preReqvalidatorList.push_back(validatorPtr) ;
				}
			}
			else if(scenarioType == SCENARIO_FAILOVER || scenarioType == SCENARIO_FAILBACK)
			{
				if(bAtTarget)
				{
					validatorPtr.reset(new DNSAvailabilityValidator(DNS_AVAILABILITY_CHECK)) ;
					//validatorPtr->setDependentRules(dependentRules) ;
					preReqvalidatorList.push_back(validatorPtr) ;
					validatorPtr.reset(new DNSUpdatePrivilageValidator(srchostnameforPermissionChecks, DNS_UPDATE_CHECK)) ;
					dependentRules.clear() ;
					dependentRules.push_back(DNS_AVAILABILITY_CHECK) ;
					validatorPtr->setDependentRules(dependentRules) ;
					preReqvalidatorList.push_back(validatorPtr) ;
				}
			}
			else
			{
				validatorPtr.reset(new DNSAvailabilityValidator(DNS_AVAILABILITY_CHECK)) ;
				//validatorPtr->setDependentRules(dependentRules) ;
				preReqvalidatorList.push_back(validatorPtr) ;
				validatorPtr.reset(new DNSUpdatePrivilageValidator(srchostnameforPermissionChecks, DNS_UPDATE_CHECK)) ;
				dependentRules.clear() ;
				dependentRules.push_back(DNS_AVAILABILITY_CHECK) ;
				validatorPtr->setDependentRules(dependentRules) ;
				preReqvalidatorList.push_back(validatorPtr) ;
			}
		}

		if(appType.find("SQL") == std::string::npos)
		{
			validatorPtr.reset(new ADUpdatePrivilageValidator(srchostnameforPermissionChecks, AD_UPDATE_CHECK)) ;
			if(bDnsFailover)
			{
				if(scenarioType == SCENARIO_FAILBACK || scenarioType == SCENARIO_FASTFAILBACK)
				{
					if(!bAtTarget)
					{
						dependentRules.clear() ;
						dependentRules.push_back(DNS_AVAILABILITY_CHECK) ;
						validatorPtr->setDependentRules(dependentRules) ;
					}
				}
				else if(scenarioType == SCENARIO_FAILOVER)
				{
					if(bAtTarget)
					{
						dependentRules.clear() ;
						dependentRules.push_back(DNS_AVAILABILITY_CHECK) ;
						validatorPtr->setDependentRules(dependentRules) ;
					}
				}
				else
				{
					dependentRules.clear() ;
					dependentRules.push_back(DNS_AVAILABILITY_CHECK) ;
					validatorPtr->setDependentRules(dependentRules) ;
				}
			}
			preReqvalidatorList.push_back(validatorPtr) ;
		}
        validatorPtr.reset(new DCAvailabilityValidator(DC_AVAILABILITY_CHECK)) ;
        preReqvalidatorList.push_back(validatorPtr) ;

        validatorPtr.reset(new HostDCCheckValidator(HOST_DC_CHECK)) ;
        dependentRules.clear() ;
        dependentRules.push_back(DC_AVAILABILITY_CHECK) ;
        validatorPtr->setDependentRules(dependentRules) ;
        preReqvalidatorList.push_back(validatorPtr) ;

		if(bAtTarget && bDnsFailover)
		{
			if(scenarioType == SCENARIO_FAILOVER)
			{
				validatorPtr.reset(new IPEqualityValidator(srchostnameforPermissionChecks, tgthostnameforPermissionChecks, false, IP_MISMATCH_CHECK)) ;
				preReqvalidatorList.push_back(validatorPtr) ;	
			}
			else if(scenarioType == SCENARIO_FAILBACK)
			{
				validatorPtr.reset(new IPEqualityValidator(srchostnameforPermissionChecks, tgthostnameforPermissionChecks, true, IP_MISMATCH_CHECK)) ;
				preReqvalidatorList.push_back(validatorPtr) ;	
			}
		}
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return preReqvalidatorList ;
}
std::list<ValidatorPtr>RuleFactory::getMSSQLInstanceRules(const MSSQLInstanceMetaData& instance, 
                                                          enum SCENARIO_TYPE scenarioType,
                                                          enum CONTEXT_TYPE contextType,
                                                          const std::string& sysdrive,
                                                          const std::string& strVacpOptions,
                                                          bool isLocalContext )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<ValidatorPtr> preReqvalidatorList ;
    ValidatorPtr validatorPtr ;
    std::string svcName ;
    std::list<InmRuleIds> depdendentRules ;

    if( isLocalContext )
    {
        if( instance.m_instanceName.compare("MSSQLSERVER") != 0 )
        {
            svcName = "MSSQL$" ;
            svcName += instance.m_instanceName ;
        }
        else
        {
            svcName = "MSSQLSERVER" ;
        }
        InmService serverSvc(svcName) ;
        QuerySvcConfig(serverSvc) ;

        validatorPtr.reset( new ServiceValidator(serverSvc, INM_SVCSTAT_RUNNING, SERVICE_STATUS_CHECK)) ;
        preReqvalidatorList.push_back(validatorPtr) ;
    }
    validatorPtr.reset( new MSSQLDbOnlineValidator(instance, SQL_DB_STATUS_CHECK)) ;
    depdendentRules.clear() ;
    depdendentRules.push_back(SERVICE_STATUS_CHECK) ;
    validatorPtr->setDependentRules(depdendentRules) ;
    preReqvalidatorList.push_back(validatorPtr) ;
	
    if(contextType == CONTEXT_HADR)
	{
		validatorPtr.reset( new MSSQLDBOnSystemDriveValidator(instance, sysdrive, SYSREM_DRIVE_DATA_CHECK) ) ;
		depdendentRules.clear() ;
		depdendentRules.push_back(SERVICE_STATUS_CHECK) ;
		depdendentRules.push_back(SQL_DB_STATUS_CHECK) ;
		validatorPtr->setDependentRules(depdendentRules) ;
		preReqvalidatorList.push_back(validatorPtr) ;
	}
	if( isLocalContext )
    {
        validatorPtr.reset( new VolTagIssueValidator(instance.m_volumesList, strVacpOptions, CONSISTENCY_TAG_CHECK)) ;
        depdendentRules.clear() ;
        depdendentRules.push_back(SERVICE_STATUS_CHECK) ;
        depdendentRules.push_back(SQL_DB_STATUS_CHECK) ;
        validatorPtr->setDependentRules(depdendentRules) ;
        preReqvalidatorList.push_back(validatorPtr) ;

        if(contextType == CONTEXT_HADR)
		{
			validatorPtr.reset( new PageFileVoumeCheck(instance.m_volumesList,PAGE_FILE_VOLUME_CHECK)) ;
			depdendentRules.clear() ;
			depdendentRules.push_back(SERVICE_STATUS_CHECK) ;
			depdendentRules.push_back(SQL_DB_STATUS_CHECK) ;
			validatorPtr->setDependentRules(depdendentRules) ;
			preReqvalidatorList.push_back(validatorPtr) ;
		}

    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return preReqvalidatorList ;
}
std::list<ValidatorPtr>RuleFactory::getMSSQLVersionLevelRules(InmProtectedAppType type, 
                                                              enum SCENARIO_TYPE scenarioType,
                                                              enum CONTEXT_TYPE contextType,															  
                                                              bool isLocalContext)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;

    std::list<ValidatorPtr> preReqvalidatorList ;
    ValidatorPtr validatorPtr;
    if( isLocalContext )
    {
        if(type != INM_APP_MSSQL2000)
        {
            InmService serverSvc("SQLWriter") ;
            QuerySvcConfig(serverSvc) ;

            validatorPtr.reset( new ServiceValidator(serverSvc, INM_SVCSTAT_RUNNING, SERVICE_STATUS_CHECK)) ;
            preReqvalidatorList.push_back(validatorPtr) ;			
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return preReqvalidatorList ;
}


std::list<ValidatorPtr> RuleFactory::getExchangeRules(const ExchAppVersionDiscInfo& info, 
                                                      const ExchangeMetaData& metadata,
                                                      enum SCENARIO_TYPE scenarioType,
                                                      enum CONTEXT_TYPE contextType,
                                                      const std::string& systemDrive,
                                                      /*const*/ std::string & strVacpoptions,
                                                      const std::string& hostname,                                             
                                                      bool isLocalContext)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<ValidatorPtr> preReqvalidatorList ;
    ValidatorPtr ptr ;
    if(contextType == CONTEXT_HADR)
    {
        if( isLocalContext )
        {
            ptr.reset(new ExchangeNestedMountPointValidator(info, metadata, NESTED_MOUNTPOINT_CHECK )) ;
            preReqvalidatorList.push_back(ptr) ;
        }    
        ptr.reset(new ExchangeHostNameValidator(info ,metadata, hostname, HOST_NAME_CHECK)) ;
        preReqvalidatorList.push_back(ptr) ;

        ptr.reset(new ExchangeDataOnSysDrive(info, metadata, systemDrive, SYSREM_DRIVE_DATA_CHECK)) ;
        preReqvalidatorList.push_back(ptr) ;

        ptr.reset(new LcrStatusCheck(metadata,LCR_ENABLED_CHECK)) ;
        preReqvalidatorList.push_back(ptr) ;

        ptr.reset(new CcrStatusCheck(info, CCR_ENABLED_CHECK)) ;
        preReqvalidatorList.push_back(ptr) ;

        ptr.reset(new SCRStatusCheck(metadata, SCR_ENABLED_CHECK)) ;
        preReqvalidatorList.push_back(ptr) ;
    }
    if( isLocalContext )
    {
        // ptr.reset( new VolTagIssueValidator(metadata.m_volumeList, CONSISTENCY_TAG_CHECK)) ;
        ptr.reset( new VolTagIssueValidator(metadata.m_volumeList,strVacpoptions, CONSISTENCY_TAG_CHECK)) ;
        preReqvalidatorList.push_back(ptr) ;
        if(contextType == CONTEXT_HADR)
		{
			ptr.reset( new PageFileVoumeCheck(metadata.m_volumeList,PAGE_FILE_VOLUME_CHECK)) ;
			preReqvalidatorList.push_back(ptr) ;
		}
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return preReqvalidatorList ;
}
std::list<ValidatorPtr> RuleFactory::getExchange2010Rules(const ExchAppVersionDiscInfo& info, 
                                                          const Exchange2k10MetaData& metadata,
                                                          enum SCENARIO_TYPE scenarioType,
                                                          enum CONTEXT_TYPE contextType,
                                                          const std::string& systemDrive,
                                                          std::string & strVacpoptions,
                                                          const std::string& hostname, 
                                                          bool isLocalContext)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<ValidatorPtr> preReqvalidatorList ;
    ValidatorPtr ptr ;
    if(contextType == CONTEXT_HADR)
    {
        if( isLocalContext )
        {
            ptr.reset(new ExchangeNestedMountPointValidator(info, metadata, NESTED_MOUNTPOINT_CHECK)) ;
            preReqvalidatorList.push_back(ptr) ;
        }    
        ptr.reset(new ExchangeHostNameValidator(info, metadata, hostname, HOST_NAME_CHECK)) ;
        preReqvalidatorList.push_back(ptr) ;

        ptr.reset(new ExchangeDataOnSysDrive( info, metadata, systemDrive,SYSREM_DRIVE_DATA_CHECK)) ;
        preReqvalidatorList.push_back(ptr) ;

        ptr.reset(new SingleMailStoreCopyRule(info, metadata, SINGLE_MAILSTORE_COPY_CHECK)) ;
        preReqvalidatorList.push_back(ptr) ;
        //strVacpoptions +=  std::string(" -w ExchangeIS");
    }
    if( isLocalContext )
    {
        ptr.reset( new VolTagIssueValidator(metadata.m_volumePathNameList, strVacpoptions, CONSISTENCY_TAG_CHECK)) ;
        preReqvalidatorList.push_back(ptr) ;
        if(contextType == CONTEXT_HADR)
		{
			ptr.reset( new PageFileVoumeCheck(metadata.m_volumePathNameList,PAGE_FILE_VOLUME_CHECK)) ;
			preReqvalidatorList.push_back(ptr) ;
		}
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return preReqvalidatorList ;
}
std::list<ValidatorPtr> RuleFactory::getFileServerRules(enum SCENARIO_TYPE scenarioType,
                                                        enum CONTEXT_TYPE contextType, 
                                                        bool isLocalContext)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;

    std::list<ValidatorPtr> preReqvalidatorList;
    ValidatorPtr ptr;
    if( isLocalContext)
    {
        InmService lanmanservice("lanmanserver"); 
        QuerySvcConfig(lanmanservice) ;
        ptr.reset(new ServiceValidator(lanmanservice, INM_SVCSTAT_RUNNING, SERVICE_STATUS_CHECK ));
        preReqvalidatorList.push_back(ptr) ;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return preReqvalidatorList ;
}
std::list<ValidatorPtr> RuleFactory::getFileServerInstanceLevelRules( const FileServerInstanceMetaData& instanceMetaData,
                                                                     enum SCENARIO_TYPE scenarioType,
                                                                     enum CONTEXT_TYPE contextType,
                                                                     const std::string& sysDrive,
                                                                     const std::string& strVacpOptions,
                                                                     bool isLocalContext )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<ValidatorPtr> preReqvalidatorList ;
    ValidatorPtr validatorPtr ;
    std::string svcName ;
    std::list<InmRuleIds> depdendentRules ;
	if(contextType == CONTEXT_HADR)
	{
		validatorPtr.reset( new FileShareOnSystemDriveValidator( instanceMetaData, sysDrive, SYSREM_DRIVE_DATA_CHECK ) ) ;
		validatorPtr->setDependentRules(depdendentRules) ;
		preReqvalidatorList.push_back(validatorPtr) ;
	}

    if( isLocalContext )
    {
        validatorPtr.reset( new VolTagIssueValidator( instanceMetaData.m_volumes, strVacpOptions, CONSISTENCY_TAG_CHECK )) ;
        depdendentRules.clear() ;
        depdendentRules.push_back(SERVICE_STATUS_CHECK) ;
        validatorPtr->setDependentRules(depdendentRules) ;
        preReqvalidatorList.push_back(validatorPtr) ;
		if(contextType == CONTEXT_HADR)
		{
			validatorPtr.reset( new PageFileVoumeCheck(instanceMetaData.m_volumes,PAGE_FILE_VOLUME_CHECK)) ;
			depdendentRules.clear() ;
			depdendentRules.push_back(SERVICE_STATUS_CHECK) ;
			validatorPtr->setDependentRules(depdendentRules) ;
			preReqvalidatorList.push_back(validatorPtr) ;
		}
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return preReqvalidatorList ;
}

std::list<ValidatorPtr> RuleFactory::getBESRules(bool isLocalContext)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<ValidatorPtr> preReqvalidatorList;
    //TO DO ....NO Rules for Now
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return preReqvalidatorList ;
}

std::list<ValidatorPtr> RuleFactory::getNormalProtectionSourceRules(  const std::string& strVacpOptions,
                                                                    bool isLocalContext)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<ValidatorPtr> preReqvalidatorList ;
    ValidatorPtr validatorPtr ;
    if( isLocalContext )
    {
        std::list<std::string> emptyList;
        validatorPtr.reset( new VolTagIssueValidator( emptyList, strVacpOptions, CONSISTENCY_TAG_CHECK )) ;
        preReqvalidatorList.push_back(validatorPtr) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return preReqvalidatorList ;

}
std::list<ValidatorPtr> RuleFactory::getTargetRedinessRules(const ExchangeTargetReadinessCheckInfo& srcInfo, 
                                                            const ExchangeTargetReadinessCheckInfo& tgtInfo,
                                                            enum SCENARIO_TYPE scenarioType,
                                                            enum CONTEXT_TYPE contextType,
															const SV_ULONGLONG& totalNumberOfPairs,
                                                            bool isLocalContext)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<ValidatorPtr> preReqvalidatorList ;
    ValidatorPtr validatorPtr ;

    std::string srcExchVersion, tgtExchVersion,srcDn, tgtDn, srcEdition, tgtEdition;
	bool isSrcMTA = false;
	bool isTgtMTA = false;
    //Get the Source Volumes capacity
    //Get the Target Volume Capacity
	std::list<std::map<std::string, std::string>> mapVolCapacities ;
	mapVolCapacities = srcInfo.m_volCapacities ;


	if ( srcInfo.m_properties.find("Version") != srcInfo.m_properties.end() )
    {
        srcExchVersion = srcInfo.m_properties.find("Version")->second ;
    }

    if ( tgtInfo.m_properties.find("Version") != tgtInfo.m_properties.end() )
    {
        tgtExchVersion = tgtInfo.m_properties.find("Version")->second ;
    }

    if ( srcInfo.m_properties.find("Dn") != srcInfo.m_properties.end() )
    {
        srcDn = srcInfo.m_properties.find("Dn")->second ;
    }

    if ( tgtInfo.m_properties.find("Dn") != tgtInfo.m_properties.end() )
    {
        tgtDn = tgtInfo.m_properties.find("Dn")->second ;
    }

	if ( srcInfo.m_properties.find("Edition") != srcInfo.m_properties.end() )
    {
        srcEdition = srcInfo.m_properties.find("Edition")->second ;
    }

    if ( tgtInfo.m_properties.find("Edition") != tgtInfo.m_properties.end() )
    {
        tgtEdition = tgtInfo.m_properties.find("Edition")->second ;
    }

	if ( srcInfo.m_properties.find("IsMTA") != srcInfo.m_properties.end() )
    {
		DebugPrintf(SV_LOG_DEBUG, "Source MTA value = %s\t", srcInfo.m_properties.find("IsMTA")->second.c_str()) ;
		if("YES" == (srcInfo.m_properties.find("IsMTA")->second))
		{
			isSrcMTA = true;
		}
    }

	if ( tgtInfo.m_properties.find("IsMTA") != tgtInfo.m_properties.end() )
    {
		DebugPrintf(SV_LOG_DEBUG, "Target MTA value = %s\n", tgtInfo.m_properties.find("IsMTA")->second.c_str()) ;
		if("YES" == (tgtInfo.m_properties.find("IsMTA")->second))
		{
			isTgtMTA = true;
		}
    }
	DebugPrintf(SV_LOG_DEBUG, "Source MTA = %d \t Target MTA = %d\n", isSrcMTA, isTgtMTA);

    if(contextType == CONTEXT_HADR)
    {
		validatorPtr.reset(new VersionEqualityValidator (srcExchVersion, tgtExchVersion, VERSION_MISMATCH_CHECK, srcEdition, tgtEdition)) ;
        preReqvalidatorList.push_back(validatorPtr) ;

        validatorPtr.reset(new ExchangeAdminGroupEqualityValidator(srcDn, tgtDn, ADMINISTRATIVE_GROUPS_MISMATCH_CHECK)) ;
        preReqvalidatorList.push_back(validatorPtr) ;

        validatorPtr.reset(new ExchangeDCEqualityValidator(srcDn, tgtDn, DOMAIN_MISMATCH_CHECK)) ;
        preReqvalidatorList.push_back(validatorPtr) ;

		/*validatorPtr.reset(new ExchangeMTAEqualityValidator(isSrcMTA, isTgtMTA, MTA_MISMATCH_CHECK)) ;
        preReqvalidatorList.push_back(validatorPtr) ;*/
    }
    if(contextType != CONTEXT_BACKUP)
    {
		validatorPtr.reset(new VolumeCapacityCheckValidator(mapVolCapacities, VOLUME_CAPACITY_CHECK)) ;
        preReqvalidatorList.push_back(validatorPtr) ;
    }
	validatorPtr.reset(new CacheDirCapacityCheckValidator(CACHE_DIR_CAPACITY_CHECK)) ;
    preReqvalidatorList.push_back(validatorPtr) ;

	validatorPtr.reset(new CMMinSpacePerPairCheckValidator(totalNumberOfPairs, CM_MIN_SPACE_PER_PAIR_CHECK)) ;
    preReqvalidatorList.push_back(validatorPtr) ;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return preReqvalidatorList ;
}

std::list<ValidatorPtr> RuleFactory::getMSSQLTargetRedinessRules(MSSQLTargetReadinessCheckInfo& srcData, 
                                                                 MSSQLTargetReadinessCheckInfo& tgtData,
                                                                 enum SCENARIO_TYPE scenarioType,
                                                                 enum CONTEXT_TYPE contextType,
																 const SV_ULONGLONG& totalNumberOfPairs,
                                                                 bool isLocalContext)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<ValidatorPtr> preReqvalidatorList ;
    ValidatorPtr validatorPtr ;

    std::string srcDataRoot, tgtDataRoot ;
    std::string srcSqlVersion, tgtSqlVersion ;
    std::map<std::string, std::map<std::string, std::string>>::iterator srcmssqlInstanceTgtReadinessCheckInfoIter ;
    srcmssqlInstanceTgtReadinessCheckInfoIter = srcData.m_sqlInstanceTgtReadinessCheckInfo.begin() ;
	std::list<std::map<std::string, std::string>> mapVolCapacities ;
	mapVolCapacities = srcData.m_volCapacities ;

    while (  srcmssqlInstanceTgtReadinessCheckInfoIter != srcData.m_sqlInstanceTgtReadinessCheckInfo.end() )
    {
        std::list<InmRuleIds> dependentRuleIds ;
        std::string srcInstanceName ;
        std::string tgtInstanceName ;
        std::string srcInstallationVolume ;
        srcInstanceName = srcmssqlInstanceTgtReadinessCheckInfoIter->first ;
		const std::map<std::string, std::string> srcSQLInstanceTgtReadinessCheckInfo = srcmssqlInstanceTgtReadinessCheckInfoIter->second; 
        std::string srcVersion, srcDataRoot, tgtVersion, tgtDataRoot ;
		if( srcSQLInstanceTgtReadinessCheckInfo.find("SrcVersion") != srcSQLInstanceTgtReadinessCheckInfo.end())
        {
			srcVersion = srcSQLInstanceTgtReadinessCheckInfo.find("SrcVersion")->second ;
        }
        if( srcSQLInstanceTgtReadinessCheckInfo.find("SrcDataRoot") != srcSQLInstanceTgtReadinessCheckInfo.end())
        {
            srcDataRoot = srcSQLInstanceTgtReadinessCheckInfo.find("SrcDataRoot")->second ;
        }
		if( srcSQLInstanceTgtReadinessCheckInfo.find("InstallationVolume") != srcSQLInstanceTgtReadinessCheckInfo.end())
        {
            srcInstallationVolume = srcSQLInstanceTgtReadinessCheckInfo.find("InstallationVolume")->second ;
        }
        if( tgtData.m_sqlInstanceTgtReadinessCheckInfo.find(srcInstanceName) != tgtData.m_sqlInstanceTgtReadinessCheckInfo.end() )
        {
			const std::map<std::string, std::string>&  tgtSQLInstanceTgtReadinessCheckInfo = tgtData.m_sqlInstanceTgtReadinessCheckInfo.find(srcInstanceName)->second ;

			if( tgtSQLInstanceTgtReadinessCheckInfo.find("SrcVersion") != tgtSQLInstanceTgtReadinessCheckInfo.end())
            {
                tgtVersion = tgtSQLInstanceTgtReadinessCheckInfo.find("SrcVersion")->second ;
            }

            if( tgtSQLInstanceTgtReadinessCheckInfo.find("SrcDataRoot") != tgtSQLInstanceTgtReadinessCheckInfo.end())
            {
                tgtDataRoot = tgtSQLInstanceTgtReadinessCheckInfo.find("SrcDataRoot")->second ;
            }
            tgtInstanceName = srcInstanceName ;
        }
        bool skipDependentChecks = false ;
        if( tgtInstanceName.compare("") == 0 )
        {
            skipDependentChecks = true  ;
        }
        if(contextType == CONTEXT_HADR)
        {
            validatorPtr.reset(new ApplicationInstanceNameCheck(srcInstanceName, tgtInstanceName, SQL_INSTANCE_CHECK)) ;
            preReqvalidatorList.push_back(validatorPtr) ;

            validatorPtr.reset(new VersionEqualityValidator (srcVersion, tgtVersion, VERSION_MISMATCH_CHECK)) ;
            dependentRuleIds.clear() ;
            dependentRuleIds.push_back(SQL_INSTANCE_CHECK);
            validatorPtr->setDependentRules(dependentRuleIds) ;
            preReqvalidatorList.push_back(validatorPtr) ;

            validatorPtr.reset(new DataRootEqualityValidator(srcDataRoot, tgtDataRoot, SQL_DATAROOT_CHECK)) ;
            dependentRuleIds.clear() ;
            dependentRuleIds.push_back(SQL_INSTANCE_CHECK);
            validatorPtr->setDependentRules(dependentRuleIds) ;
            preReqvalidatorList.push_back(validatorPtr) ;

            if( srcInstallationVolume.compare("") != 0 )
            {
                std::list<std::string> srcVols ;
				std::list<std::map<std::string, std::string> >::const_iterator srcCapIter = mapVolCapacities.begin();
				while( srcCapIter != mapVolCapacities.end() )
                {
					srcVols.push_back(srcCapIter->find("srcVolName")->second ); 
                    srcCapIter ++ ;
                }
                std::map<std::string, std::string> instalvolMap ;
                instalvolMap.insert(std::make_pair(srcInstanceName, srcInstallationVolume)) ;
                validatorPtr.reset(new ApplicationInstallationVolumeCheck(instalvolMap, srcVols,  SQL_INSTALL_VOLUME_CHECK)) ;
                preReqvalidatorList.push_back( validatorPtr ) ;
            }
        }
        if(contextType != CONTEXT_BACKUP)
        {
			validatorPtr.reset(new VolumeCapacityCheckValidator(mapVolCapacities, VOLUME_CAPACITY_CHECK)) ;
            dependentRuleIds.clear() ;
            dependentRuleIds.push_back(SQL_INSTANCE_CHECK);
            validatorPtr->setDependentRules(dependentRuleIds) ;
            preReqvalidatorList.push_back(validatorPtr) ;
        }
        srcmssqlInstanceTgtReadinessCheckInfoIter++ ;
    }
	validatorPtr.reset(new CacheDirCapacityCheckValidator(CACHE_DIR_CAPACITY_CHECK)) ;
    preReqvalidatorList.push_back(validatorPtr) ;
	validatorPtr.reset(new CMMinSpacePerPairCheckValidator(totalNumberOfPairs, CM_MIN_SPACE_PER_PAIR_CHECK)) ;
    preReqvalidatorList.push_back(validatorPtr) ;
	if(contextType == CONTEXT_HADR)
	{
		validatorPtr.reset(new TargetVolumeAvaialabilityValiadator(srcData.m_tempDbVols, TARGET_VOLUME_AVAILABILITY_CHECK)) ;
		preReqvalidatorList.push_back(validatorPtr) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return preReqvalidatorList ;

}
std::list<ValidatorPtr> RuleFactory::getFileServerTargetRedinessRules( FileServerTargetReadinessCheckInfo& srcData,
                                                                      enum SCENARIO_TYPE scenarioType,
                                                                      enum CONTEXT_TYPE contextType,
																	  const SV_ULONGLONG& totalNumberOfPairs,
                                                                      bool isLocalContext )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<ValidatorPtr> preReqvalidatorList ;
    ValidatorPtr validatorPtr ;
	const std::list<std::map<std::string, std::string>>& mapVolCapacities = srcData.m_volCapacities;

    if(contextType != CONTEXT_BACKUP)
    {
		validatorPtr.reset(new VolumeCapacityCheckValidator(mapVolCapacities, VOLUME_CAPACITY_CHECK)) ;
        preReqvalidatorList.push_back(validatorPtr) ;
    }
	validatorPtr.reset(new CacheDirCapacityCheckValidator(CACHE_DIR_CAPACITY_CHECK)) ;
    preReqvalidatorList.push_back(validatorPtr) ;
	validatorPtr.reset(new CMMinSpacePerPairCheckValidator(totalNumberOfPairs, CM_MIN_SPACE_PER_PAIR_CHECK)) ;
    preReqvalidatorList.push_back(validatorPtr) ;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return preReqvalidatorList ;
}
std::list<ValidatorPtr> RuleFactory::getNormalProtectionTargetRules(const TargetReadinessCheckInfo &tgtInfo,
																	const SV_ULONGLONG& totalNumberOfPairs,
                                                                    bool isLocalContext)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;

    std::list<ValidatorPtr> preReqvalidatorList;
    ValidatorPtr validatorPtr ;
    validatorPtr.reset(new CacheDirCapacityCheckValidator(CACHE_DIR_CAPACITY_CHECK)) ;
    preReqvalidatorList.push_back(validatorPtr) ;
	validatorPtr.reset(new CMMinSpacePerPairCheckValidator(totalNumberOfPairs, CM_MIN_SPACE_PER_PAIR_CHECK)) ;
    preReqvalidatorList.push_back(validatorPtr) ;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return preReqvalidatorList ;
}
