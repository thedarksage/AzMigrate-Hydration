#include "MonitorHandler.h"
#include "inmsdkutil.h"
#include "inmstrcmp.h"
#include "util.h"
#include "confengine/protectionpairconfig.h"
#include "confengine/scenarioconfig.h"
#include "confengine/policyconfig.h"
#include "confengine/volumeconfig.h"
#include "confengine/protectionpairconfig.h"
#include "confengine/alertconfig.h"
#include "confengine/auditconfig.h"
#include "confengine/agentconfig.h"
#include "APIController.h"

#include "alertobject.h"
#include "auditobject.h"
#include "inmsdkutil.h"
#include "confengine/confschemareaderwriter.h"
#include "util.h"
#include "portablehelpersmajor.h"
#ifdef SV_WINDOWS
#include "appcommand.h"
#include "operatingsystem.h"
#endif
#include "volumeinfocollector.h"
#include "boost/algorithm/string/replace.hpp"
#include "service.h"
#include <iomanip>
#include "../inmsafecapis/inmsafecapis.h"

MonitorHandler::MonitorHandler(void)
{
}

MonitorHandler::~MonitorHandler(void)
{
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;

}
INM_ERROR MonitorHandler::process(FunctionInfo& request)
{
	Handler::process(request);
	INM_ERROR errCode = E_SUCCESS;
	if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"GetErrorLogPath") == 0)
	{
		errCode = GetErrorLogPath(request);
	}
    else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"ListVolumesWithProtectionDetails") == 0)
	{
		errCode = ListVolumesWithProtectionDetails(request);
	}
	else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"GetProtectionDetails") == 0)
	{
		errCode = GetProtectionDetails(request);
	}
	else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"DownloadAlerts") == 0)
	{
		errCode = DownloadAlerts(request);
	}
	else if (InmStrCmp<NoCaseCharCmp> (m_handler.functionName.c_str(), "DownloadAudit") == 0)
	{
		errCode = DownloadAudit (request);
	}
	else if (InmStrCmp<NoCaseCharCmp> (m_handler.functionName.c_str(), "HealthStatus") == 0)
	{
		errCode = HealthStatus (request);
	}
	else
	{
		//Through an exception that, the function is not supported.
	}
	return Handler::updateErrorCode(errCode);
}
INM_ERROR MonitorHandler::validate(FunctionInfo& request)
{
	INM_ERROR errCode = Handler::validate(request);
	if(E_SUCCESS != errCode)
		return errCode;

	//TODO: Write hadler specific validation here

	return errCode;
}
bool MonitorHandler::isAllProtectedVolumesPaused(std::list <std::string> &protectedVolumes)
{
	bool bRet = false;
	std::list<std::string>::iterator protectedVolumesIter = protectedVolumes.begin ();
	ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;

	while (protectedVolumesIter != protectedVolumes.end() )
	{
		VOLUME_SETTINGS::PAIR_STATE pairState ;
		protectionPairConfigPtr->GetPairState( *protectedVolumesIter, pairState ) ;		
		if (pairState == VOLUME_SETTINGS::PAUSE || pairState == VOLUME_SETTINGS::PAUSE_PENDING)
		{
			bRet = true;
		}
		else if (bRet == false)
		{
			bRet = false ;
			break;
		}
		protectedVolumesIter++;
	}
	return bRet;
}

bool MonitorHandler::GetErrorCodeFromSystemStatus(const std::string& status,
                                                  INM_ERROR& errCode,
                                                  std::string& errorMsg,
                                                  std::string& protectionStatus)
{
    bool bRet = true ;
    errCode = E_UNKNOWN ;
	std::list <std::string> protectedVolumes; 
	std::string systemPauseState ;
	ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
	scenarioConfigPtr->GetSystemPausedState (systemPauseState);
    if( InmStrCmp<NoCaseCharCmp>(status,VOL_PACK_PENDING) == 0 )
    {
        protectionStatus = VOL_PACK_PENDING ;
        errCode = E_SUCCESS ;
    }
	else if (InmStrCmp<NoCaseCharCmp>(status,VOL_PACK_INPROGRESS) == 0 )
	{
		protectionStatus = VOL_PACK_INPROGRESS ;
        errCode = E_SUCCESS ;
	}
	else if (InmStrCmp<NoCaseCharCmp>(status,VOL_PACK_SUCCESS) == 0 )
	{
		protectionStatus = VOL_PACK_SUCCESS ;
        errCode = E_SUCCESS ;
	}
    else if(InmStrCmp<NoCaseCharCmp>(status,"Active") == 0 )
    {
        protectionStatus = "Under Protection";
		if (systemPauseState == "1")
		{
			protectionStatus += ", ";
			protectionStatus += "Paused";
		}
        errCode = E_SUCCESS ;
    }
    else if(InmStrCmp<NoCaseCharCmp>(status,VOL_PACK_FAILED) == 0 )
    {
        protectionStatus = status;
        errorMsg = VOL_PACK_FAILED ;
        errCode = E_VOLPACK_PROVISION_FAIL ;
    }
    else if( InmStrCmp<NoCaseCharCmp>(status,SCENARIO_DELETION_PENDING) == 0 ||
        InmStrCmp<NoCaseCharCmp>(status,VOL_PACK_DELETION_PENDING) == 0 || 
        InmStrCmp<NoCaseCharCmp>(status,VOL_PACK_DELETION_INPROGRESS) == 0 || 
        InmStrCmp<NoCaseCharCmp>(status,VOL_PACK_DELETION_SUCCESS) == 0 )
    {
        protectionStatus = "Scenario Deletion Pending" ;
        errCode = E_SUCCESS ;
    }
    else if( InmStrCmp<NoCaseCharCmp>(status,VOL_PACK_DELETION_FAILED) == 0 || 
        InmStrCmp<NoCaseCharCmp>(status,SCENARIO_DELETION_FAILED) == 0)
    {
        protectionStatus = "Scenario Deletion Failed";
        if( InmStrCmp<NoCaseCharCmp>(status,VOL_PACK_DELETION_FAILED) == 0 )
        {
            errorMsg = VOL_PACK_DELETION_FAILED;
            errCode = E_VOLPACK_UNPROVISION_FAIL ;
        }
    }
    if( errCode == E_UNKNOWN ) 
    {
        bRet = false ;
    }
    return bRet ;
}
                 
void MonitorHandler::GetSystemStatusPg(INM_ERROR errCode,
                                       const std::string& errMsg,
                                       const std::string& protectionStatus,
                                       ParameterGroup& systemStatusPg)
{
    ValueType vt ;
    vt.value = boost::lexical_cast<std::string>(errCode) ;
    systemStatusPg.m_Params.insert(std::make_pair("ErrorCode", vt)) ;
    vt.value = errMsg ;
    systemStatusPg.m_Params.insert(std::make_pair("ErrorMessage", vt)) ;
    vt.value = protectionStatus ;
    systemStatusPg.m_Params.insert(std::make_pair("Protectionstatus", vt)) ;
}
void PopulatePgFromList(const std::string& prefix, 
                        int startIdx, 
                        const std::list<std::string>& list,
                        ParameterGroup& pg)
{
    std::list<std::string>::const_iterator listIter = list.begin() ;
    ValueType vt ;
    while( listIter != list.end() )
    {
        std::string key = prefix + boost::lexical_cast<std::string>(startIdx++) ;
        vt.value = *listIter ;
        pg.m_Params.insert(std::make_pair( key, vt) ) ;
        listIter++ ;
    }
}

void MonitorHandler::GetReplicationOptionsPg(ParameterGroup& replOptionsPg)
{
    ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
    ReplicationOptions replicationOptions ;
    if( scenarioConfigPtr->GetReplicationOptions(replicationOptions) )
    {
        ParameterGroup autoresyncoptionsPg ; 
        ValueType vt;
        vt.value = boost::lexical_cast<std::string>(replicationOptions.batchSize);
        replOptionsPg.m_Params.insert(std::make_pair("BatchResync",vt));
        vt.value = replicationOptions.autoResyncOptions.between ;
        autoresyncoptionsPg.m_Params.insert(std::make_pair("Between", vt)) ;
        vt.value = boost::lexical_cast<std::string>(replicationOptions.autoResyncOptions.waitTime) ;
        vt.value += " Minutes" ;
        autoresyncoptionsPg.m_Params.insert(std::make_pair("WaitTime",vt));
        replOptionsPg.m_ParamGroups.insert(std::make_pair("AutoResyncPolicy", autoresyncoptionsPg)) ;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to get the replication options\n") ;
    }
}

void MonitorHandler::GetConsistencyOptions(ParameterGroup& consistencyOptionsPg)
{
        
    ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
    ConsistencyOptions consistencyOptions ;
    if( scenarioConfigPtr->GetConsistencyOptions(consistencyOptions) )
    {
        ValueType vt ;
        vt.value = consistencyOptions.exception ;
        consistencyOptionsPg.m_Params.insert(std::make_pair( "Exception", vt)) ;
        vt.value = consistencyOptions.interval ;
        consistencyOptionsPg.m_Params.insert(std::make_pair("ConsistencySchedule",vt));
        vt.value = consistencyOptions.tagType ;
        consistencyOptionsPg.m_Params.insert(std::make_pair("ConsistencyLevel",vt));
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to get the consistency options\n") ;
    }
}

void MonitorHandler::GetExcludedVolumesPg(ParameterGroup& excludeVolumesPg)
{
    ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
    std::list<std::string> excludedVolumes ;
    if( scenarioConfigPtr->GetExcludedVolumes(excludedVolumes) )
    {
        PopulatePgFromList("VolumeName", 0, excludedVolumes, excludeVolumesPg) ;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to get the excluded volumes\n") ;
    }

}

void MonitorHandler::GetNonProtectedVolumesPg(ParameterGroup& nonprotectedVolumePg)
{
    ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
    VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
    std::list<std::string> nonprotectedVolumes  ;
    std::list<std::string> protectedVolumes ;
    std::list<std::string> excludedVolumes ;
    scenarioConfigPtr->GetExcludedVolumes(excludedVolumes) ;
    if( scenarioConfigPtr->GetProtectedVolumes(protectedVolumes) )
    {
        volumeConfigPtr->GetNonProtectedVolumes(protectedVolumes, excludedVolumes, nonprotectedVolumes) ;
        PopulatePgFromList("VolumeName", 0, nonprotectedVolumes, nonprotectedVolumePg) ;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to get the protected volumes\n") ;
    }
}

void MonitorHandler::GetProtectedVolumesPg(ParameterGroup& protectedVolumePg)
{
    ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
    std::list<std::string> protectedVolumes ;
    if( scenarioConfigPtr->GetProtectedVolumes(protectedVolumes) )
    {
        PopulatePgFromList("VolumeName", 0, protectedVolumes, protectedVolumePg) ;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to get the protected volumes\n") ;
    }
}
void MonitorHandler::GetRetentionPolicyPg(ParameterGroup& pg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
    RetentionPolicy  retPolicy ;
    if( scenarioConfigPtr->GetRetentionPolicy(retPolicy) )
    {
        ValueType vt ;
        vt.value = retPolicy.duration ;
        pg.m_Params.insert(std::make_pair("Duration", vt)) ;
        vt.value = retPolicy.retentionType ;
        pg.m_Params.insert(std::make_pair("RetentionType", vt)) ;
        std::list<ScenarioRetentionWindow>::const_iterator begin = retPolicy.retentionWindows.begin() ;
        ParameterGroup retWindowPg ;
        
        while( begin != retPolicy.retentionWindows.end() )
        {
            const ScenarioRetentionWindow& retWindow = *begin ;
            vt.value = boost::lexical_cast<std::string>(retWindow.count) ;
            ParameterGroup windowPg ; 
            windowPg.m_Params.insert(std::make_pair("Count",vt));
            vt.value = retWindow.duration ;
            windowPg.m_Params.insert(std::make_pair("Duration",vt)) ;
            vt.value = retWindow.granularity ;
            windowPg.m_Params.insert(std::make_pair("Granularity",vt));
            retWindowPg.m_ParamGroups.insert(std::make_pair( retWindow.retentionWindowId, windowPg)) ;
            begin++ ;
        }
        pg.m_ParamGroups.insert(std::make_pair( "RetentionWindow", retWindowPg)) ;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Unable to get Retention Policy information\n") ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
void MonitorHandler::GetLogRotationSettingsPg(ParameterGroup& logrotationSettingsPg)
{
	LocalConfigurator lc ;
	SV_UINT fullbackupInterval = 0 ;
	bool fullbackupRequired = false ;
	if( lc.IsFullBackupRequired() )
	{
		fullbackupRequired = true ;
		fullbackupInterval = lc.GetFullBackupInterval() ;
	}
	ValueType vt ;
	vt.value = boost::lexical_cast<std::string>(fullbackupInterval) ;
	logrotationSettingsPg.m_Params.insert(std::make_pair( "FullBackupIntervalInSec", vt)) ;
	vt.value = "Yes" ;
	if( !fullbackupRequired )
	{
		vt.value = "No" ;
	}
	logrotationSettingsPg.m_Params.insert(std::make_pair( "RequiredFullBackup", vt)) ;
}
void MonitorHandler::GetIOParameters(ParameterGroup& ioparamsPg)
{
	ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
	spaceCheckParameters spacecheckparams ;
	scenarioConfigPtr->GetIOParameters(spacecheckparams) ;
	ValueType vt ;
	std::stringstream stream ;
	stream << std::setprecision(2) << spacecheckparams.compressionBenifits ;
	vt.value = stream.str(); 
	ioparamsPg.m_Params.insert(std::make_pair("CompressionBenefits", vt)) ;
	stream.str(""); 
	stream << std::setprecision(2) << spacecheckparams.changeRate ;
	vt.value = stream.str() ;
	ioparamsPg.m_Params.insert(std::make_pair("ChangeRate", vt)) ;
}
void MonitorHandler::GetConfigureProtectionPolicyPg(ParameterGroup& pg)
{
    ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
    ParameterGroup replOptionsPg ;
	ParameterGroup ioparamsPg ;
    GetIOParameters(ioparamsPg) ;
	pg.m_ParamGroups.insert(std::make_pair( "IOParameters", ioparamsPg)) ;
    GetReplicationOptionsPg(replOptionsPg) ;
    ParameterGroup consistencyOptionsPg ;
    GetConsistencyOptions(consistencyOptionsPg) ;
    pg.m_ParamGroups.insert(std::make_pair( "ConsistencyPolicy", consistencyOptionsPg)) ;
    pg.m_ParamGroups.insert(std::make_pair("ReplicationOptions", replOptionsPg)) ;
    ParameterGroup excludeVolumesPg ;
    GetExcludedVolumesPg(excludeVolumesPg) ;
    pg.m_ParamGroups.insert(std::make_pair( "ExclusionList", excludeVolumesPg)) ;
    ParameterGroup retentionPolicyPg ;
    GetRetentionPolicyPg(retentionPolicyPg) ;
    pg.m_ParamGroups.insert(std::make_pair( "RetentionPolicy", retentionPolicyPg)) ;
    ParameterGroup nonprotectedVolumePg ;
    GetNonProtectedVolumesPg(nonprotectedVolumePg) ;
    pg.m_ParamGroups.insert(std::make_pair( "Non-protected VolumeList", nonprotectedVolumePg)) ;

    ParameterGroup protectedVolumePg, logRotationSettingsPg ;
    GetProtectedVolumesPg(protectedVolumePg) ;
    pg.m_ParamGroups.insert(std::make_pair( "Protected VolumeList",protectedVolumePg)) ;
	GetLogRotationSettingsPg(logRotationSettingsPg) ;
	pg.m_ParamGroups.insert(std::make_pair( "FullBackupSettings", logRotationSettingsPg)) ;
    std::string recoveryFeaturePolicy ;
    if( scenarioConfigPtr->GetRecoveryFeaturePolicy(recoveryFeaturePolicy) )
    {
        ValueType vt ;
        vt.value = recoveryFeaturePolicy ;
        pg.m_Params.insert(std::make_pair("RecoveryFeaturePolicy", vt)) ;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to get the recovery feauture policy") ;
    }
    std::string actionOnNewVolumesDiscovery ;
    if( scenarioConfigPtr->GetActionOnNewVolumeDiscovery(actionOnNewVolumesDiscovery) )
    {
        ValueType vt ;
        vt.value = actionOnNewVolumesDiscovery ;
        pg.m_Params.insert(std::make_pair("NewVolumeDiscovered",vt));
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Failed know action on new volumes discovery") ;
    }
    SV_UINT rpoThreshold ;
    if( scenarioConfigPtr->GetRpoThreshold(rpoThreshold) )
    {
        ValueType vt ;
        vt.value = boost::lexical_cast<std::string>(rpoThreshold) ;
        pg.m_Params.insert(std::make_pair("RPOPolicy",vt));
    }

}

void MonitorHandler::GetVolumeInformationFromScenario(const std::string& srcVolume,
                                                      ParameterGroup& volumePg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
    VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
    std::string repoName, repoPath ;
	std::string volumeProvisioningStatus; 
    volumeConfigPtr->GetRepositoryNameAndPath(repoName, repoPath) ;
    ValueType vt ;
    scenarioConfigPtr->GetVolumeProvisioningStatus (srcVolume,volumeProvisioningStatus);
    vt.value = volumeProvisioningStatus ;
    volumePg.m_Params.insert(std::make_pair("PairState", vt)) ;
    
    vt.value = repoName ;
    volumePg.m_Params.insert(std::make_pair("RepositoryName", vt)) ;
    
    vt.value = repoPath ;
    volumePg.m_Params.insert(std::make_pair("RepositoryPath", vt)) ;
    
    vt.value = srcVolume ;
    volumePg.m_Params.insert(std::make_pair("VolumeName", vt)) ;
    
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void MonitorHandler::GetRetentionSettingsPg(const std::string& srcVolume, 
                                                ParameterGroup& retentionSetttingsPg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ParameterGroup retentionActualsPg, totalTimeRangePg ;
    ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
    std::string logsFrom, logsTo, logsFromUtc, logsToUtc ;

    protectionPairConfigPtr->GetRetentionWindowDetailsBySrcVolName(srcVolume, logsFrom, logsTo, logsFromUtc, logsToUtc) ;
    std::string starttime, endtime ;
    ValueType vt ;
    vt.value = "N/A" ;
    if( !logsFrom.empty() )
    {
        convertTimeStampToCXformat(logsFrom, starttime) ;
        vt.value = starttime ;
    }
    totalTimeRangePg.m_Params.insert(std::make_pair( "StartTime", vt)) ;
    vt.value = "N/A" ;
    if( !logsTo.empty() )
    {
        convertTimeStampToCXformat(logsTo, endtime) ;
        vt.value = endtime ;
    }
    totalTimeRangePg.m_Params.insert(std::make_pair( "EndTime", vt)) ;
    SV_ULONGLONG logSize ;
    protectionPairConfigPtr->GetCurrentLogSizeBySrcDevName(srcVolume, logSize) ;
    vt.value = boost::lexical_cast<std::string>(logSize) + " (Bytes)" ;
    retentionActualsPg.m_Params.insert(std::make_pair( "SizeOccupied", vt)) ;
    retentionActualsPg.m_ParamGroups.insert(std::make_pair( "TotalTimeRange", totalTimeRangePg)) ;
    retentionSetttingsPg.m_ParamGroups.insert(std::make_pair( "RetentionActuals", retentionActualsPg)) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void MonitorHandler::GetVolumeInformationFromProtection(const std::string& srcVolume,
                                                        ParameterGroup& volumePg )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
    std::string currentRpo, pairState, resyncProgress, resyncReqdState, repoPath, repoName ;
	std::string etlresync,pauseReason ;
	VolumeConfigPtr volumeConfigptr = VolumeConfig::GetInstance() ;
    protectionPairConfigPtr->GetCurrentRPO(srcVolume, currentRpo) ;
	std::string retentionpath ;
	protectionPairConfigPtr->GetRetentionPathBySrcDevName(srcVolume, retentionpath) ;
    protectionPairConfigPtr->GetPairState(srcVolume, pairState) ;
    protectionPairConfigPtr->GetRepositoryNameAndPath(srcVolume, repoName, repoPath) ;
    protectionPairConfigPtr->GetResyncProgress(srcVolume, resyncProgress) ;
	protectionPairConfigPtr->GetEtlForResync(srcVolume, etlresync) ;
    protectionPairConfigPtr->GetResyncRequiredState(srcVolume, resyncReqdState) ;
	protectionPairConfigPtr->GetPauseExtendedReason(srcVolume, pauseReason) ;
	std::string resyncStartTime, resyncEndTime ;
	protectionPairConfigPtr->GetResyncStartAndEndTimeInHRF(srcVolume, resyncStartTime, resyncEndTime) ;
    ValueType vt ;
    vt.value = currentRpo ;
    volumePg.m_Params.insert(std::make_pair("CurrentRPO", vt)) ;
    vt.value = pairState ;
    volumePg.m_Params.insert(std::make_pair("PairState", vt)) ;
    vt.value = repoName ;
    volumePg.m_Params.insert(std::make_pair("RepositoryName", vt)) ;
    vt.value = repoPath ;
    volumePg.m_Params.insert(std::make_pair("RepositoryPath", vt)) ;
    vt.value = resyncProgress ;
    volumePg.m_Params.insert(std::make_pair("ResyncProgress", vt)) ;
    vt.value = pauseReason ;
    volumePg.m_Params.insert(std::make_pair("PauseReason", vt)) ;
	vt.value = retentionpath ;
    volumePg.m_Params.insert(std::make_pair("RetentionPath", vt)) ;
	vt.value = resyncReqdState ;
	volumePg.m_Params.insert(std::make_pair("ResyncRequiredState",  vt)) ;
    vt.value = etlresync ;
    volumePg.m_Params.insert(std::make_pair("ResyncEtl", vt)) ;
    vt.value = srcVolume ;
    volumePg.m_Params.insert(std::make_pair("VolumeName", vt)) ;
	protectionPairConfigPtr->GetResyncStartAndEndTimeInHRF(srcVolume, resyncStartTime, resyncEndTime) ;
    vt.value = resyncStartTime ;
	volumePg.m_Params.insert(std::make_pair("ResyncStartTimeHRF",  vt)) ;
    vt.value = resyncEndTime ;
	volumePg.m_Params.insert(std::make_pair("ResyncEndTimeHRF",  vt)) ;
    protectionPairConfigPtr->GetResyncStartAndEndTime(srcVolume, resyncStartTime, resyncEndTime) ;
    vt.value = resyncStartTime ;
	volumePg.m_Params.insert(std::make_pair("ResyncStartTime",  vt)) ;
    vt.value = resyncEndTime ;
	volumePg.m_Params.insert(std::make_pair("ResyncEndTime",  vt)) ;
	if( volumeConfigptr->IsBootVolume(srcVolume) )
	{
		vt.value = "BootVolume" ;
	}
	else if( volumeConfigptr->IsSystemVolume(srcVolume) )
	{
		vt.value = "BootVolume" ;
	}
	else
	{
		vt.value = "General" ;
	}
	volumePg.m_Params.insert(std::make_pair("VolumeUsedAs",  vt)) ;
	ParameterGroup retentionSettingsPg ; 
	GetRetentionSettingsPg(srcVolume, retentionSettingsPg) ;
    volumePg.m_ParamGroups.insert(std::make_pair( "RetentionSettings", retentionSettingsPg)) ;
}


void MonitorHandler::GetVolumeLevelProtectionInfo(ParameterGroup& responsePg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
    ScenarioConfigPtr scenarionConfigPtr = ScenarioConfig::GetInstance() ;
    std::list<std::string> protectedVolumesFromScenario, protectedVolumesFromProtection ;
    protectionPairConfigPtr->GetProtectedVolumes(protectedVolumesFromProtection) ;
    if( scenarionConfigPtr->GetProtectedVolumes(protectedVolumesFromScenario) )
    {
        std::list<std::string>::const_iterator volIterFromScenario ;
        volIterFromScenario = protectedVolumesFromScenario.begin() ;
        std::string executionStatus ;
        scenarionConfigPtr->GetExecutionStatus(executionStatus) ;
        while( volIterFromScenario != protectedVolumesFromScenario.end() )
        {
            ParameterGroup volumeInfoPg ;
			bool proceed = false ;
            if( std::find( protectedVolumesFromProtection.begin(), protectedVolumesFromProtection.end(),
                            *volIterFromScenario) == protectedVolumesFromProtection.end() )
            {
				GetVolumeInformationFromScenario(*volIterFromScenario, volumeInfoPg) ;
				proceed = true ;
            }
            else
            {
                GetVolumeInformationFromProtection(*volIterFromScenario, volumeInfoPg) ;
                proceed = true ;                
            }
            if( proceed )
            {
                responsePg.m_ParamGroups.insert(std::make_pair(*volIterFromScenario, volumeInfoPg)) ;
            }
            volIterFromScenario++ ;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to get the protected volumes from the protection pair store\n") ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
INM_ERROR MonitorHandler::GetProtectionDetails(FunctionInfo& request)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    INM_ERROR errCode = E_UNKNOWN;
	LocalConfigurator  lc ;
	std::string actualbasepath, newbasepath, agentguid ;
	if( request.m_RequestPgs.m_Params.find( "RepositoryPath" ) != request.m_RequestPgs.m_Params.end() )
	{
		newbasepath = request.m_RequestPgs.m_Params.find( "RepositoryPath" )->second.value ;
	}
	else
	{
		try
		{
			newbasepath = lc.getRepositoryLocation() ;
		}
		catch(ContextualException& ex)
		{
			return E_NO_REPO_CONFIGERED ;
		}
	}
	if( request.m_RequestPgs.m_Params.find( "AgentGUID" ) != request.m_RequestPgs.m_Params.end() )
	{
		agentguid = request.m_RequestPgs.m_Params.find( "AgentGUID" )->second.value;
	}
	else
	{
		agentguid = lc.getHostId() ;
	}
	std::string repoPathForHost = constructConfigStoreLocation( getRepositoryPath(newbasepath),  agentguid) ;
	std::string lockPath = getLockPath(repoPathForHost) ;
	RepositoryLock repoLock(lockPath) ;
	repoLock.Acquire() ;
	ConfSchemaReaderWriter::CreateInstance( request.m_RequestFunctionName, getRepositoryPath(lc.getRepositoryLocation()),  repoPathForHost, false) ;
	std::string repositoryname ;
    ScenarioConfigPtr scenarionConfigPtr = ScenarioConfig::GetInstance() ;
	SV_UINT rpoThreshold ;
    scenarionConfigPtr->GetRpoThreshold(rpoThreshold) ;
    ProtectionPairConfigPtr protectionPairConfig = ProtectionPairConfig::GetInstance() ;
	std::list<std::string> protectedvolumes ;
	protectionPairConfig->GetProtectedVolumes(protectedvolumes) ;
	VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
	volumeConfigPtr->GetRepositoryNameAndPath(repositoryname, actualbasepath) ;
	if( volumeConfigPtr->IsValidRepository() )
	{
		if( scenarionConfigPtr->ProtectionScenarioAvailable() )
		{
			std::string executionStatus ;
			if( scenarionConfigPtr->GetExecutionStatus(executionStatus) )
			{
				std::string protectionStatus ;
				std::string errMsg ;
				GetErrorCodeFromSystemStatus(executionStatus, errCode, errMsg, protectionStatus) ;
				ParameterGroup systemStatusPg ;
				GetSystemStatusPg(errCode, errMsg, protectionStatus, systemStatusPg) ;
				request.m_ResponsePgs.m_ParamGroups.insert(std::make_pair( "SystemStatus", systemStatusPg)) ;
				if( executionStatus == "Active" || 
					executionStatus == VOL_PACK_INPROGRESS || 
					executionStatus == VOL_PACK_SUCCESS || 
					executionStatus == VOL_PACK_PENDING ||
					executionStatus == VOL_PACK_FAILED || 
					executionStatus == SCENARIO_DELETION_PENDING)
				{
					ParameterGroup configuredprotectionPoliciesPg ;
					GetConfigureProtectionPolicyPg(configuredprotectionPoliciesPg) ;
					request.m_ResponsePgs.m_ParamGroups.insert(std::make_pair( "Configured Protection Policy",configuredprotectionPoliciesPg )) ;
				}
				GetVolumeLevelProtectionInfo(request.m_ResponsePgs) ;

			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to get the execution status") ;
			}
		}
		else
		{
			errCode = E_NO_PROTECTIONS ;
		}
	}
	else
	{
		errCode = E_INVALID_REPOSITORY ;
	}
	repoLock.Release() ;
	std::list<std::string>::const_iterator protectedVolIter = protectedvolumes.begin() ;
	while( protectedvolumes.end() != protectedVolIter )
	{
		DebugPrintf(SV_LOG_DEBUG, "Protected Volume %s\n",protectedVolIter->c_str()) ;
		ParameterGroupsIter_t volgrpIter = request.m_ResponsePgs.m_ParamGroups.find( *protectedVolIter ) ;
		if(  volgrpIter != request.m_ResponsePgs.m_ParamGroups.end() )
		{
			DebugPrintf(SV_LOG_DEBUG, "actual  base path %s, new base path %s\n", actualbasepath.c_str(), newbasepath.c_str()) ;
			ParameterGroup availableRestorePointsPg ;
			ParameterGroup availableRecoveryRangesPg ;
			std::map<SV_ULONGLONG, CDPEvent>  volumeRestorePoints ;
			std::map<SV_ULONGLONG, CDPTimeRange> volumeRecoveryRanges ;
			int restorePointCount = 5 ;
			if( volgrpIter->second.m_Params.find( "RetentionPath" ) != volgrpIter->second.m_Params.end() )
			{
				std::string retentionpath = volgrpIter->second.m_Params.find( "RetentionPath" )->second.value ;
				
				GetAvailableRestorePointsPg(actualbasepath, newbasepath, retentionpath, rpoThreshold, availableRestorePointsPg, volumeRestorePoints, restorePointCount) ;
				GetAvailableRecoveryRangesPg(actualbasepath, newbasepath, retentionpath, rpoThreshold, availableRecoveryRangesPg, volumeRecoveryRanges, restorePointCount) ;
				volgrpIter->second.m_ParamGroups.insert(std::make_pair( "AvailableRecoveryRanges", 
															availableRecoveryRangesPg)) ;

				volgrpIter->second.m_ParamGroups.insert(std::make_pair( "AvailableRestorePoints", 
															availableRestorePointsPg)) ;
			}
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG, "Protected Volume %s is not found in the response pgs\n",protectedVolIter->c_str()) ;
		}
		protectedVolIter++ ;
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return errCode ;
}
void copyDirectoryContents(std::string& srcDir,std::string& tgtDir,bool brecursive)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;	

	SVMakeSureDirectoryPathExists(tgtDir.c_str());
	if(boost::filesystem::exists(srcDir))
	{
		boost::filesystem::path dir(srcDir);
		boost::filesystem::directory_iterator dir_iter(srcDir), dir_end;
		for(;dir_iter != dir_end; ++dir_iter)
		{ 
			if(boost::filesystem::is_regular_file(*dir_iter))
			{
				std::string tgtfile = tgtDir+"\\"+(*dir_iter).leaf();
				boost::filesystem::path srcloc((*dir_iter));
				boost::filesystem::path tgtloc(tgtfile);
				boost::filesystem::copy_file(srcloc,tgtloc,boost::filesystem::copy_option::overwrite_if_exists);
			}
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}
void copyfile(const std::string& srcFile, const std::string& tgtDir)
{
    if( boost::filesystem::is_regular_file(srcFile) )
    {
        SVMakeSureDirectoryPathExists(tgtDir.c_str());
	    std::string tgtfile = tgtDir+"\\"+ srcFile.substr(srcFile.find_last_of(ACE_DIRECTORY_SEPARATOR_STR_A) + 1) ;
        boost::filesystem::path srcloc((srcFile));
        boost::filesystem::path tgtloc(tgtfile);
        boost::filesystem::copy_file(srcloc,tgtloc,boost::filesystem::copy_option::overwrite_if_exists);
    }
}

void MonitorHandler::CollectDriveStatistics( const std::string& volumeName, 
                                             const std::string& logDir)
{
#ifdef SV_WINDOWS
    SV_ULONG dwExitCode = 0 ;
    std::string logfile ;
    std::string stdOutput ;
    bool bProcessActive = true;
    std::string tgtVol ;
    std::string srcVol = volumeName ; 
    
    logfile = logDir ;
    logfile += ACE_DIRECTORY_SEPARATOR_CHAR ;
    logfile += "ntfsInfo.log" ;
    std::string ntfsinfoStr = "fsutil fsinfo ntfsinfo " ;
    ntfsinfoStr += volumeName ;
    if( volumeName.length() == 1 )
    {
        ntfsinfoStr += ":" ;
    }
    else
    {
        ntfsinfoStr += "\\" ;
    }
    AppCommand ntfsinfoCmd (ntfsinfoStr, 0, logfile) ;
    stdOutput = "" ;
    ntfsinfoCmd.Run(dwExitCode,stdOutput, bProcessActive )  ;
    

    logfile = logDir ;
    logfile += ACE_DIRECTORY_SEPARATOR_CHAR ;
    logfile += "volumeinfo.log" ;
    std::string volumeinfoStr = "fsutil fsinfo volumeinfo " ;
    volumeinfoStr += volumeName ;
    if( volumeName.length() == 1 )
    {
        volumeinfoStr += ":" ;
    }
    else
    {
        volumeinfoStr += "\\" ;
    }
    AppCommand volumeinfoCmd (volumeinfoStr, 0, logfile) ;
    stdOutput = "" ;
    volumeinfoCmd.Run(dwExitCode,stdOutput, bProcessActive )  ;

    logfile = logDir ;
    logfile += ACE_DIRECTORY_SEPARATOR_CHAR ;
    logfile += "statistics.log" ;
    std::string statisticsStr = "fsutil fsinfo statistics " ;
    statisticsStr += volumeName ;
    if( volumeName.length() == 1 )
    {
        statisticsStr += ":" ;
    }
    else
    {
        statisticsStr += "\\" ;
    }
    AppCommand statisticsCmd (statisticsStr, 0, logfile) ;
    stdOutput = "" ;
    statisticsCmd.Run(dwExitCode,stdOutput, bProcessActive )  ;
#endif
}
#ifdef SV_WINDOWS
void CollectHotFixAndInstalledInfo(const std::string& tgtPath)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::string hotfixfilePath = tgtPath ;
	std::string installedProductsPath = tgtPath ;
    hotfixfilePath+= ACE_DIRECTORY_SEPARATOR_CHAR ;
    hotfixfilePath+= "agentlogs" ;
    hotfixfilePath+= ACE_DIRECTORY_SEPARATOR_CHAR ;
	hotfixfilePath+= "hotfix.log" ;
	
	installedProductsPath += ACE_DIRECTORY_SEPARATOR_CHAR ;
    installedProductsPath += "agentlogs" ;
    installedProductsPath += ACE_DIRECTORY_SEPARATOR_CHAR ;
	installedProductsPath += "installProds.log" ;
	std::list<std::string> hotfixes ;
	GetInstalledPatches(hotfixes);
	std::list<InstalledProduct> installedProds ;
	GetInstalledProducts(installedProds) ;

    if( hotfixes.size() )
	{
		
		std::ofstream out;
		out.open(hotfixfilePath.c_str(), std::ios::trunc);
		if( out.is_open() )
		{
			out << "List of hotfixes" << std::endl ;
			std::list<std::string>::const_iterator hotfixIter = hotfixes.begin() ;
			int i = 0 ;
			while( hotfixes.end() != hotfixIter )
			{
				out << hotfixIter->c_str() << std::endl; 
				hotfixIter++ ;
			}
			out.close() ;
		}
	}
	if( installedProds.size() )
	{
		std::ofstream out;
		out.open(installedProductsPath.c_str(), std::ios::trunc);
		if( out.is_open() )
		{
			out << "List of installed products" << std::endl << std::endl ;
			std::list<InstalledProduct>::const_iterator installedProdIter = installedProds.begin() ;
			while( installedProds.end() != installedProdIter )
			{
				out << "Install Location : " << installedProdIter->installLocation ;
				out << "Name :" << installedProdIter->name << std::endl;
				out << "Package Name : " << installedProdIter->pkgname << std::endl;
				out << "Vendor : " << installedProdIter->vendor << std::endl;
				out << "Version : " << installedProdIter->version << std::endl << std::endl ;
				installedProdIter++ ;
			}
			out.close() ;
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
#endif
void MonitorHandler::CollectStatistics(const std::string& tgtPath)
{
#ifdef SV_WINDOWS
	ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
    CollectHotFixAndInstalledInfo(tgtPath) ;
	std::list<std::string> protectedVolumes ;
    SV_ULONG dwExitCode = 0 ;
    std::string stdOutput ;
    bool bProcessActive = true;
    protectionPairConfigPtr->GetProtectedVolumes( protectedVolumes ) ;
    std::list<std::string>::const_iterator VolIter = protectedVolumes.begin() ;
    LocalConfigurator configurator ;
    std::string logDir = tgtPath ;
    logDir += ACE_DIRECTORY_SEPARATOR_CHAR ;
    logDir += "agentlogs" ;
    std::string logfile = logDir ;
    logfile += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
    logfile += "server_stats.log" ;
    std::string serverStatsStr ;
    serverStatsStr = "net statistics server" ;
    AppCommand serverStatsCmd(serverStatsStr, 0, logfile) ;
    serverStatsCmd.Run(dwExitCode,stdOutput, bProcessActive)  ;
    
    std::string workstationStatsStr ;
    logfile = logDir ;
    logfile += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
    logfile += "workstation_stats.log" ;
    workstationStatsStr = "net statistics workstation" ;
    AppCommand workStationStatsCmd(workstationStatsStr, 0, logfile) ;
    stdOutput = "" ;
    workStationStatsCmd.Run(dwExitCode,stdOutput, bProcessActive)  ;

    logfile = logDir ;
    logfile += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
    logfile += "regexport.log" ;
    
    std::string regExportStr = "regedit.exe /S /E /A ";
    regExportStr += "\"" ;
    regExportStr += logDir ;
    regExportStr += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
    regExportStr += "services_involflt.reg" ;
    regExportStr += "\"" ;
    regExportStr += " " ;
    regExportStr += "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Class\\{71A27CDD-812A-11D0-BEC7-08002BE2092F}" ;
    AppCommand regExportCmd1( regExportStr, 0, logfile) ;
    stdOutput = "" ;
    regExportCmd1.Run( dwExitCode,stdOutput, bProcessActive) ;
    
    regExportStr = "regedit.exe /S /E /A ";
    regExportStr += "\"" ;
    regExportStr += logDir ;
    regExportStr += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
    regExportStr += "71A27CDD-812A-11D0-BEC7-08002BE2092F.reg" ;
    regExportStr += "\"" ;
    regExportStr += " " ;
    regExportStr += "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\services\\involflt" ;
    AppCommand regExportCmd2( regExportStr, 0, logfile) ;
    stdOutput = "" ;
    regExportCmd2.Run( dwExitCode,stdOutput, bProcessActive) ;
    
    regExportStr = "regedit.exe /S /E /A ";
    regExportStr += "\"" ;
    regExportStr += logDir ;
    regExportStr += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
    regExportStr += "invirvol.reg" ;
    regExportStr += "\"" ;
    regExportStr += " " ;
    regExportStr += "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\services\\invirvol" ;
    AppCommand regExportCmd3( regExportStr, 0, logfile) ;
    stdOutput = "" ;
    regExportCmd3.Run( dwExitCode,stdOutput, bProcessActive) ;
    
	regExportStr = "regedit.exe /S /E /A ";
    regExportStr += "\"" ;
    regExportStr += logDir ;
    regExportStr += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
    regExportStr += "incdskfl.reg" ;
    regExportStr += "\"" ;
    regExportStr += " " ;
    regExportStr += "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\services\\incdskfl" ;
    AppCommand regExportCmd4( regExportStr, 0, logfile) ;
    stdOutput = "" ;
    regExportCmd4.Run( dwExitCode,stdOutput, bProcessActive) ;

	std::string drvcdStatsStr = configurator.getInstallPath() ;
	drvcdStatsStr += ACE_DIRECTORY_SEPARATOR_CHAR ;
	drvcdStatsStr += "drvutil.exe" ;
	drvcdStatsStr += " --cd " ;
	logfile = logDir ;
	logfile += ACE_DIRECTORY_SEPARATOR_CHAR ;
	logfile += "involflt_cd.log" ;
	AppCommand drvStatsCdCmd (drvcdStatsStr, 0, logfile) ;
	stdOutput = "" ;
	drvStatsCdCmd.Run(dwExitCode,stdOutput, bProcessActive )  ;
    
	std::string drvgdvStatsStr = configurator.getInstallPath() ;
	drvgdvStatsStr += ACE_DIRECTORY_SEPARATOR_CHAR ;
	drvgdvStatsStr += "drvutil.exe" ;
	drvgdvStatsStr += " --gdv " ;
	logfile = logDir ;
	logfile += ACE_DIRECTORY_SEPARATOR_CHAR ;
	logfile += "involflt_gdv.log" ;
	AppCommand drvStatsGdvCmd (drvgdvStatsStr, 0, logfile) ;
	stdOutput = "" ;
	drvStatsGdvCmd.Run(dwExitCode,stdOutput, bProcessActive )  ;

    while( VolIter != protectedVolumes.end() )
    {
        ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
        std::string tgtVol ;
        protectionPairConfigPtr->GetTargetVolumeBySrcVolumeName( VolIter->c_str(), tgtVol ) ;
        std::string volLogDir = logDir ;
        std::string srcVol = VolIter->c_str() ;
        volLogDir += ACE_DIRECTORY_SEPARATOR_CHAR ;
        boost::algorithm::replace_all( srcVol, ":", "_") ;
        boost::algorithm::replace_all( srcVol, "\\", "_") ;
        volLogDir += srcVol ;
        srcVol = VolIter->c_str(); 
        SVMakeSureDirectoryPathExists( volLogDir.c_str()) ;
        std::string listEventsStr = configurator.getInstallPath();
        listEventsStr += ACE_DIRECTORY_SEPARATOR_CHAR ;
        listEventsStr += "cdpcli.exe --listevents --vol=" ;
        listEventsStr += tgtVol ;
        logfile = volLogDir ;
        logfile += ACE_DIRECTORY_SEPARATOR_CHAR ;
        logfile += "cdp_events.log" ;
        stdOutput = "" ;
        AppCommand listeventscmd (listEventsStr, 0, logfile) ;
        listeventscmd.Run(dwExitCode,stdOutput, bProcessActive )  ;
        std::string showSummaryStr  = configurator.getInstallPath();;
        showSummaryStr += ACE_DIRECTORY_SEPARATOR_CHAR ;
        showSummaryStr += "cdpcli.exe --showsummary --vol=" ;
        showSummaryStr += tgtVol ;
        showSummaryStr += " --verbose" ;
        logfile = volLogDir ;
        logfile += ACE_DIRECTORY_SEPARATOR_CHAR ;
        logfile += "cdp_summary.log" ;
        AppCommand showSummaryCmd (showSummaryStr, 0, logfile ) ;
        stdOutput = "" ;
        showSummaryCmd.Run(dwExitCode,stdOutput, bProcessActive)  ;

        std::string drvStatsStr = configurator.getInstallPath() ;
        drvStatsStr += ACE_DIRECTORY_SEPARATOR_CHAR ;
        drvStatsStr += "drvutil.exe" ;
        drvStatsStr += " --ps " ;
        drvStatsStr += srcVol ;
        logfile = volLogDir ;
        logfile += ACE_DIRECTORY_SEPARATOR_CHAR ;
        logfile += "involflt_psstats.log" ;
        AppCommand drvStatsCmd (drvStatsStr, 0, logfile) ;
        stdOutput = "" ;
        drvStatsCmd.Run(dwExitCode,stdOutput, bProcessActive )  ;
        stdOutput = "" ;
        std::string dataModeStatsStr = configurator.getInstallPath() ;
        dataModeStatsStr += ACE_DIRECTORY_SEPARATOR_CHAR ;
        dataModeStatsStr += "drvutil.exe" ;
        dataModeStatsStr += " --dm " ;
        dataModeStatsStr += srcVol ;
        logfile = volLogDir ;
        logfile += ACE_DIRECTORY_SEPARATOR_CHAR ;
        logfile += "involflt_dmstats.log" ;
        AppCommand dataModeStatsCmd (drvStatsStr, 0, logfile) ;
        dataModeStatsCmd.Run(dwExitCode,stdOutput, bProcessActive )  ;
        CollectDriveStatistics( VolIter->c_str(), volLogDir ) ;
        VolIter++ ;
    }
    try
    {
		std::string repoDrive = configurator.getRepositoryLocation() ;
		if( !(repoDrive.length() > 2 && repoDrive[0] == '\\' && repoDrive[1] == '\\' ) )
		{
			std::string volLogDir = logDir ;
			std::string srcVol = repoDrive ;
			volLogDir += ACE_DIRECTORY_SEPARATOR_CHAR ;
			boost::algorithm::replace_all( srcVol, ":", "_") ;
			boost::algorithm::replace_all( srcVol, "\\", "_") ;
			volLogDir += srcVol ;
			srcVol = repoDrive ; 
			SVMakeSureDirectoryPathExists( volLogDir.c_str()) ;
			CollectDriveStatistics( repoDrive, volLogDir ) ;
		}
    }
    catch(...)
    {
    }
#endif
}
void MonitorHandler::SaveEvents(const std::string& tgtPath)
{
#ifdef SV_WINDOWS
    BOOL (WINAPI * ProcAdd)(
  __in_opt  HANDLE Session,
  __in      LPCWSTR Path,
  __in      LPCWSTR Query,
  __in      LPCWSTR TargetFilePath,
  __in      DWORD Flags);
    DWORD status = ERROR_SUCCESS;
    HINSTANCE hinstLib; 
    LPWSTR pTargetLogFile1 = L"system_eventlog.evtx";
    LPWSTR pQuery1 = L"<QueryList><Query Id=\"0\" Path=\"System\"><Select Path=\"System\">*[System[Provider[@Name='backupagent' or @Name='invirvol' or @Name='InVolFlt' or @Name='volmgr' or @Name='disk' or @Name='Volsnap' or @Name='VSS']]]</Select></Query></QueryList>";
    LPWSTR pTargetLogFile2 = L"application_eventlog.evtx";
    LPWSTR pQuery2 = L"<QueryList><Query Id=\"0\" Path=\"Application\"><Select Path=\"Application\">*[System[Provider[@Name='backupagent' or @Name='volmgr' or @Name='Volsnap' or @Name='VSS']]]</Select></Query></QueryList>";
    hinstLib = LoadLibrary(TEXT("Wevtapi.dll")); 
    if (hinstLib != NULL) 
    {
        *(FARPROC *)&ProcAdd = GetProcAddress(hinstLib, "EvtExportLog");
        if (NULL != ProcAdd) 
        {
            std::string tgtFile, eventlogsdir ;
            eventlogsdir = tgtPath ;
            eventlogsdir += ACE_DIRECTORY_SEPARATOR_CHAR ;
            eventlogsdir += "EventLogs" ;
            SVMakeSureDirectoryPathExists( eventlogsdir.c_str() ) ;
            if( (ProcAdd) (NULL, NULL, pQuery1, pTargetLogFile1, 0x1) )
             {
                 if( boost::filesystem::exists( "system_eventlog.evtx") )
                 {
                     ACE_OS::sleep(2) ;
                     tgtFile = eventlogsdir ;
                     tgtFile += ACE_DIRECTORY_SEPARATOR_CHAR ;
                     tgtFile += "system_eventlog.evtx" ;
					 if (boost::filesystem::exists(tgtFile.c_str()))
						boost::filesystem::remove( tgtFile.c_str()) ;
                     boost::filesystem::copy_file( "system_eventlog.evtx",tgtFile.c_str()) ;
                     boost::filesystem::remove( "system_eventlog.evtx") ;
                 }
             }
             if( (ProcAdd)(NULL, NULL, pQuery2, pTargetLogFile2, 0x1) )
             {
                 if( boost::filesystem::exists( "application_eventlog.evtx") )
                 {

					ACE_OS::sleep(2) ;
					tgtFile = eventlogsdir;
					tgtFile += ACE_DIRECTORY_SEPARATOR_CHAR ;
					tgtFile += "application_eventlog.evtx" ;
					if (boost::filesystem::exists(tgtFile.c_str()))
						boost::filesystem::remove( tgtFile.c_str()) ;
					boost::filesystem::copy_file( "application_eventlog.evtx",tgtFile.c_str()) ; 
					boost::filesystem::remove( "application_eventlog.evtx") ;
                 }
             }
        }
    }
       
#endif
}
INM_ERROR MonitorHandler::GetErrorLogPath(FunctionInfo& request)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    INM_ERROR errCode = E_UNKNOWN ;
	std::string repoName,repoPath;
	VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance(); 
	volumeConfigPtr->GetRepositoryNameAndPath(repoName,repoPath);
    if( volumeConfigPtr->isRepositoryAvailable(repoName)) 
    {
		LocalConfigurator localconfig;
		std::string repoBaseDir = getConfigStorePath(); 
	    

        std::string BaseTargetDir ;
		std::string backupLogspath;
        BaseTargetDir = repoBaseDir +  "BackupLogs";
		backupLogspath = BaseTargetDir;
        
        std::string srcDir, tgtDir ;
        srcDir = repoBaseDir + "Logs" ;
        tgtDir = BaseTargetDir + ACE_DIRECTORY_SEPARATOR_STR_A + "Logs" ;
        copyDirectoryContents(srcDir,tgtDir,false);

        srcDir = localconfig.getInstallPath();;
		srcDir += ACE_DIRECTORY_SEPARATOR_STR_A;
		srcDir += "Application Data";
		srcDir += ACE_DIRECTORY_SEPARATOR_STR_A;
		srcDir += "ApplicationPolicyLogs";
        tgtDir = BaseTargetDir + ACE_DIRECTORY_SEPARATOR_STR_A + "ApplicationPolicyLogs" ;
        copyDirectoryContents(srcDir,tgtDir,false);

        srcDir = localconfig.getInstallPath();;
		srcDir += ACE_DIRECTORY_SEPARATOR_STR_A;
		srcDir += "Application Data";
		srcDir += ACE_DIRECTORY_SEPARATOR_STR_A;
		srcDir += "etc";
		tgtDir = BaseTargetDir + ACE_DIRECTORY_SEPARATOR_STR_A + "etc";
        copyDirectoryContents(srcDir,tgtDir,false);
        
        srcDir = repoBaseDir + ACE_DIRECTORY_SEPARATOR_STR_A + "configstore" ;
        tgtDir = BaseTargetDir + ACE_DIRECTORY_SEPARATOR_STR_A + "configstore" ;
        copyDirectoryContents(srcDir,tgtDir,false);
        

        std::string srcFile;
        tgtDir = BaseTargetDir + ACE_DIRECTORY_SEPARATOR_STR_A + "agentlogs" ;
        srcFile = localconfig.getInstallPath() + ACE_DIRECTORY_SEPARATOR_STR_A + "cdpcli.log" ;
        copyfile( srcFile, tgtDir) ;
        
        srcFile = localconfig.getInstallPath() + ACE_DIRECTORY_SEPARATOR_STR_A + "backupagent.log" ;
        copyfile( srcFile, tgtDir) ;
		
		srcFile = localconfig.getInstallPath() + ACE_DIRECTORY_SEPARATOR_STR_A + "ProductLicense.log" ;
        copyfile( srcFile, tgtDir) ;
		
		srcFile = localconfig.getInstallPath() + ACE_DIRECTORY_SEPARATOR_STR_A + "Host.log" ;
        copyfile( srcFile, tgtDir) ;

        srcFile = localconfig.getInstallPath() + ACE_DIRECTORY_SEPARATOR_STR_A + "bxadmin.log" ;
        copyfile( srcFile, tgtDir) ;
 
		srcFile = localconfig.getInstallPath() + ACE_DIRECTORY_SEPARATOR_STR_A + "SSEMonitor.log" ;
        copyfile( srcFile, tgtDir) ;

		srcFile = localconfig.getInstallPath() + ACE_DIRECTORY_SEPARATOR_STR_A + "SSEMonitor_0.log" ;
        copyfile( srcFile, tgtDir) ;

        srcFile = localconfig.getInstallPath() + ACE_DIRECTORY_SEPARATOR_STR_A + "cdpexplorer.log" ;
        copyfile( srcFile, tgtDir) ;

        srcFile = localconfig.getInstallPath() + ACE_DIRECTORY_SEPARATOR_STR_A + "cdpexplorer.audit" ;
        copyfile( srcFile, tgtDir) ;
        
        srcFile = localconfig.getInstallPath() + ACE_DIRECTORY_SEPARATOR_STR_A + "cdpmgr.log" ;
        copyfile( srcFile, tgtDir) ;

		srcFile = localconfig.getInstallPath() + ACE_DIRECTORY_SEPARATOR_STR_A + "ProtectionManager.log" ;
        copyfile( srcFile, tgtDir) ;

		srcFile = localconfig.getInstallPath() + ACE_DIRECTORY_SEPARATOR_STR_A + "ProtectionManager_0.log" ;
        copyfile( srcFile, tgtDir) ;

		srcFile = localconfig.getInstallPath() + ACE_DIRECTORY_SEPARATOR_STR_A + "RescueUSB.log" ;
        copyfile( srcFile, tgtDir) ;

		srcFile = localconfig.getInstallPath() + ACE_DIRECTORY_SEPARATOR_STR_A + "RescueUSB_0.log" ;
        copyfile( srcFile, tgtDir) ;

		srcFile = localconfig.getInstallPath() + ACE_DIRECTORY_SEPARATOR_STR_A + "RescueCD.log" ;
        copyfile( srcFile, tgtDir) ;

		srcFile = localconfig.getInstallPath() + ACE_DIRECTORY_SEPARATOR_STR_A + "RescueCD_0.log" ;
        copyfile( srcFile, tgtDir) ;

		srcFile = localconfig.getInstallPath() + ACE_DIRECTORY_SEPARATOR_STR_A + "SSEUpdater.log" ;
        copyfile( srcFile, tgtDir) ;

		srcFile = localconfig.getInstallPath() + ACE_DIRECTORY_SEPARATOR_STR_A + "inmsdk.log" ;
		copyfile( srcFile, tgtDir) ;

		srcFile = localconfig.getInstallPath() + ACE_DIRECTORY_SEPARATOR_STR_A + "ProtectionManager.log.1" ;
		if (boost::filesystem::exists(srcFile))
			copyfile( srcFile, tgtDir) ;

#ifdef SV_WINDOWS
		std::string scoutMailRecoveryPath ; 
		if ( ReadScoutMailRegistryKey (scoutMailRecoveryPath) == true )
		{
			std::string smrConfPath = scoutMailRecoveryPath +  "\\AppData\\etc\\smrscout.conf" ;
			if (boost::filesystem::exists(smrConfPath))
			{
				DebugPrintf(SV_LOG_DEBUG, "File: %s exist\n", smrConfPath.c_str()) ;
				FileConfigurator configurator(smrConfPath);
				std::string section = "Logging" ,key = "LogPath" ;
				std::string smrLogPath = configurator.getSMRInfoFromSMRConf(section,key);
				srcFile = smrLogPath + ACE_DIRECTORY_SEPARATOR_STR_A + "InMageSMRServer.log" ;
				if (boost::filesystem::exists(srcFile))
					copyfile( srcFile, tgtDir) ;

				srcFile = smrLogPath + ACE_DIRECTORY_SEPARATOR_STR_A + "ScoutMailRecovery.log" ;
				if (boost::filesystem::exists(srcFile))
					copyfile( srcFile, tgtDir) ;

				srcFile = smrLogPath + ACE_DIRECTORY_SEPARATOR_STR_A + "InMageApplication.log" ;
				if (boost::filesystem::exists(srcFile))
					copyfile( srcFile, tgtDir) ;
			}
			else
			{
				DebugPrintf(SV_LOG_DEBUG, "File: %s not found\n", smrConfPath.c_str()) ;
			}
		}
#endif

        SaveEvents( BaseTargetDir ) ;
        CollectStatistics( BaseTargetDir ) ;
        std::string strZipCommand = localconfig.getArchiveToolPath() + " "  ;
        strZipCommand += BaseTargetDir + " " ;
        strZipCommand += BaseTargetDir + ".zip" ;

        SV_ULONG dwExitCode = 1 ;
#ifdef SV_WINDOWS
		if( BaseTargetDir[0] != '\\' && BaseTargetDir[1] != '\\' )
		{
			std::string strLogFile = localconfig.getInstallPath() + ACE_DIRECTORY_SEPARATOR_STR_A ;
			strLogFile += "inmwinzip.log" ;
			AppCommand zipCommand( strZipCommand, 0, strLogFile ) ;
			std::string stdoutput ;

			bool bProcessActive = true;
			zipCommand.Run( dwExitCode, stdoutput, bProcessActive, "", false ) ;
		}
		else 
		{
			/* Copy from CIFS Path to Installation directory recursively */
			std::string destinationPath = localconfig.getInstallPath() + ACE_DIRECTORY_SEPARATOR_STR_A ;
			destinationPath += "BackupLogs";
			copyDir (backupLogspath ,destinationPath);  

			std::string strLogFile = localconfig.getInstallPath() + ACE_DIRECTORY_SEPARATOR_STR_A ;
			strLogFile += "inmwinzip.log" ; 
			/*Prepare Zip command */ 	
			strZipCommand = localconfig.getArchiveToolPath() + " "  ;
			strZipCommand += "\"" + destinationPath + "\"" + " " + "\"" + destinationPath + ".zip" + "\"";
			AppCommand zipCommand( strZipCommand, 0, strLogFile ) ;
			std::string stdoutput ;
			bool bProcessActive = true;
			/*Execute the zip command*/ 
			zipCommand.Run( dwExitCode, stdoutput, bProcessActive, "", false ) ;
			BaseTargetDir = destinationPath;
		}
#endif
        errCode = E_SUCCESS ;
        ValueType vt ;
                
         if( dwExitCode == 0 )
             vt.value = BaseTargetDir+ ".zip" ;
         else
            vt.value = BaseTargetDir ;
        request.m_ResponsePgs.m_Params.insert(std::make_pair("ErrorLogPathName",vt));
    }
    else
    {
        errCode = E_NO_PROTECTIONS ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return errCode ;
}

void MonitorHandler::GetVolumeLevelInformationForLVWPD(ParameterGroup& responsePg, 
                                                       const std::list<std::string>& protectedVolumes)
{
    std::list<std::string>::const_iterator volIter = protectedVolumes.begin() ;
    ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
    ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
    VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
    std::map<std::string, volumeProperties> volumePropertiesMap ;
    std::string repoName, repoPath ;
    ParameterGroup vollevelprotectionPg ;
    
    if( volumeConfigPtr->GetVolumeProperties(volumePropertiesMap) )
    {
        volumeConfigPtr->GetRepositoryNameAndPath(repoName, repoPath) ;
        while( volIter != protectedVolumes.end() )
        {
            ParameterGroup volPg ;
            ParameterGroup srcVolNamePg, volRestorePointsPg, ProtectionDetailsPg;
            ParameterGroup tgtHostIdPg ;
            if( volumePropertiesMap.find( *volIter ) != volumePropertiesMap.end() )
            {
                volumeProperties& protectedVolumeProperties = volumePropertiesMap.find(*volIter)->second ;
                ValueType vt ;
                vt.value = *volIter ;
                volPg.m_Params.insert(std::make_pair( "VolumeName", vt)) ;
                std::string mntpnt = *volIter ;
                if( mntpnt.length() == 1 )
                {
                    mntpnt += ":\\" ;
                }
                vt.value = mntpnt ;
                volPg.m_Params.insert(std::make_pair("VolumeMountPoint", vt)) ;
                std::string volumeUsedAs = "General" ;
                if( protectedVolumeProperties.isSystemVolume.compare("1") == 0 || 
                    protectedVolumeProperties.isBootVolume.compare("1") == 0 )
                {
                    volumeUsedAs = "BootVolume" ;
                }
                vt.value = volumeUsedAs ;
                volPg.m_Params.insert(std::make_pair("VolumeUsedAs", vt)) ;
                vt.value = "Fixed" ;
				volPg.m_Params.insert(std::make_pair("VolumeType", vt)) ;
                vt.value = protectedVolumeProperties.fileSystemType ;
				volPg.m_Params.insert(std::make_pair("FileSystemType", vt)) ;
                vt.value = protectedVolumeProperties.capacity ;
                volPg.m_Params.insert(std::make_pair("Capacity", vt)) ;
                vt.value = protectedVolumeProperties.volumeLabel ;
                volPg.m_Params.insert( std::make_pair( "VolumeLabel", vt ) ) ;
                vt.value = protectedVolumeProperties.rawSize ;
                volPg.m_Params.insert( std::make_pair( "RawCapacity", vt ) ) ;
                std::string tgtVolume ;
                protectionPairConfigPtr->GetTargetVolumeBySrcVolumeName(*volIter, tgtVolume) ;
                vt.value = tgtVolume ;
                ProtectionDetailsPg.m_Params.insert(std::make_pair("TargetVolume", vt)) ;
                std::string pairState ;
                protectionPairConfigPtr->GetPairState(*volIter, pairState) ;
                vt.value = pairState ;
                ProtectionDetailsPg.m_Params.insert(std::make_pair("PairState",vt));
                std::string retentionPath ;

                protectionPairConfigPtr->GetRetentionPathBySrcDevName(*volIter, retentionPath) ;
                vt.value = retentionPath ;
                ProtectionDetailsPg.m_Params.insert(std::make_pair("RetentionDataPath", vt)) ;
                    
                SV_UINT rpoThreshold ;
                scenarioConfigPtr->GetRpoThreshold(rpoThreshold) ;
                vt.value = repoName ;
                ProtectionDetailsPg.m_Params.insert(std::make_pair("RepositoryName", vt)) ;
                if( repoPath.length() == 1 )
                {
                    repoPath += ":\\" ;
                }
                vt.value = repoPath ;
                ProtectionDetailsPg.m_Params.insert(std::make_pair("RepositoryPath", vt)) ;
                tgtHostIdPg.m_ParamGroups.insert(std::make_pair("ProtectionDetails", ProtectionDetailsPg)) ;
				volRestorePointsPg.m_ParamGroups.insert(std::make_pair(AgentConfig::GetInstance()->getHostId(), tgtHostIdPg)) ;
                srcVolNamePg.m_ParamGroups.insert(std::make_pair( "VolumeRestorePoints", volRestorePointsPg)) ;
                srcVolNamePg.m_ParamGroups.insert( std::make_pair("VolumeDetails", volPg)) ;
                vollevelprotectionPg.m_ParamGroups.insert(std::make_pair( *volIter, srcVolNamePg)) ;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to get the volume information for %s\n",volIter->c_str()) ;
            }
            volIter++ ;
        }
        responsePg.m_ParamGroups.insert(std::make_pair("VolumeLevelProtectionInfo", vollevelprotectionPg )) ;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to get the volume details information\n") ;
    }
}

INM_ERROR MonitorHandler::ListVolumesWithProtectionDetails(FunctionInfo& request)
{
	LocalConfigurator lc ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	INM_ERROR errCode = E_UNKNOWN;
	std::string actualbasepath, newbasepath, agentguid ;
	if( request.m_RequestPgs.m_Params.find( "RepositoryPath" ) != request.m_RequestPgs.m_Params.end() )
	{
		newbasepath = request.m_RequestPgs.m_Params.find( "RepositoryPath" )->second.value ;
	}
	else
	{
		try
		{
			newbasepath = lc.getRepositoryLocation() ;
		}
		catch(ContextualException& ex)
		{
			return E_NO_REPO_CONFIGERED ;
		}
	}
	if( request.m_RequestPgs.m_Params.find( "AgentGUID" ) != request.m_RequestPgs.m_Params.end() )
	{
		agentguid = request.m_RequestPgs.m_Params.find( "AgentGUID" )->second.value;
	}
	else
	{
		agentguid = lc.getHostId() ;
	}
	std::string repoPathForHost = constructConfigStoreLocation( getRepositoryPath(newbasepath),  agentguid) ;
	std::string lockPath = getLockPath(repoPathForHost) ;
	RepositoryLock repoLock(lockPath) ;
	repoLock.Acquire() ;
    ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
	VolumeConfigPtr volumeconfigptr = VolumeConfig::GetInstance() ;
	SV_UINT rpothreshold ;
    scenarioConfigPtr->GetRpoThreshold(rpothreshold) ;
    
	std::string repoName ;
	std::list<std::string> protectedVolumes ;
	volumeconfigptr->GetRepositoryNameAndPath( repoName, actualbasepath) ;
    if( scenarioConfigPtr->ProtectionScenarioAvailable() )
    {
        errCode = E_SUCCESS ;
        ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
        protectionPairConfigPtr->GetProtectedVolumes(protectedVolumes) ;
        ParameterGroup commonRecoveryInfoPg ;
        GetVolumeLevelInformationForLVWPD(request.m_ResponsePgs, protectedVolumes) ;
    }
    else
    {
        errCode = E_NO_PROTECTIONS ;
    }
	repoLock.Release() ;
	std::list<std::string>::const_iterator protectedvoliter = protectedVolumes.begin() ;
	std::map< std::string, std::map<SV_ULONGLONG, CDPEvent> > volumeRestorePointsMap ;
	std::map<std::string, std::map<SV_ULONGLONG, CDPTimeRange> > volumeRecoveryRangeMap ;
	ParameterGroupsIter_t volumelevelprotectioninfoIter ;
	volumelevelprotectioninfoIter = request.m_ResponsePgs.m_ParamGroups.find( "VolumeLevelProtectionInfo" ) ;
	while( protectedvoliter != protectedVolumes.end() && 
			volumelevelprotectioninfoIter != request.m_ResponsePgs.m_ParamGroups.end() )
	{
		std::map<SV_ULONGLONG, CDPEvent>  volumeRestorePoints ;
		std::map<SV_ULONGLONG, CDPTimeRange> volumeRecoveryRanges ;
		ParameterGroupsIter_t volgrpIter = volumelevelprotectioninfoIter->second.m_ParamGroups.find( *protectedvoliter ) ;
		if( volgrpIter != volumelevelprotectioninfoIter->second.m_ParamGroups.end() )
		{
			ParameterGroupsIter_t volrestorepointpgIter = volgrpIter->second.m_ParamGroups.find( "VolumeRestorePoints" ) ;
			if( volrestorepointpgIter != volgrpIter->second.m_ParamGroups.end() )
			{
				ParameterGroupsIter_t tgthostidagentgrpiter = volrestorepointpgIter->second.m_ParamGroups.find( agentguid ) ;
				if( tgthostidagentgrpiter != volrestorepointpgIter->second.m_ParamGroups.end() )
				{
					ParameterGroupsIter_t protectiondetailspgiter = tgthostidagentgrpiter->second.m_ParamGroups.find( "ProtectionDetails" ) ;
					ParameterGroup restorePointsPg, recoveryRangesPg ;
					if( protectiondetailspgiter != tgthostidagentgrpiter->second.m_ParamGroups.end() )
					{
						std::string retentionpath ;
						if( protectiondetailspgiter->second.m_Params.find( "RetentionDataPath" ) !=
								protectiondetailspgiter->second.m_Params.end() )
						{
							retentionpath = protectiondetailspgiter->second.m_Params.find( "RetentionDataPath" )->second.value ;
						}
						if( !retentionpath.empty() )
						{
							GetAvailableRestorePointsPg( actualbasepath, newbasepath, retentionpath, rpothreshold, restorePointsPg, volumeRestorePoints,  0) ;
							GetAvailableRecoveryRangesPg(  actualbasepath, newbasepath, retentionpath, rpothreshold, recoveryRangesPg, volumeRecoveryRanges, 0 ) ;
							volumeRestorePointsMap.insert( std::make_pair( *protectedvoliter, volumeRestorePoints ) ) ;
							volumeRecoveryRangeMap.insert( std::make_pair( *protectedvoliter, volumeRecoveryRanges ) ) ;
						}
					}
					tgthostidagentgrpiter->second.m_ParamGroups.insert(std::make_pair( "RestorePoints" , restorePointsPg)) ;
					tgthostidagentgrpiter->second.m_ParamGroups.insert(std::make_pair( "RecoveryRanges" , recoveryRangesPg )) ;
				}
			}
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "parametergroup for %s doesn't exist\n", protectedvoliter->c_str()) ;
		}
		protectedvoliter++ ;
	}
	
	FillCommonRestorePointInfo(request.m_ResponsePgs, volumeRestorePointsMap, volumeRecoveryRangeMap) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return errCode;
}
void MonitorHandler::CheckRepositoryValidity( const std::string& repoDevice, const std::string& sparsefile)
{
    std::stringstream sparsepartfile;
    sparsepartfile << sparsefile << SPARSE_PARTFILE_EXT << 0;
    ACE_stat s ;
    if( sv_stat( getLongPathName(sparsepartfile.str().c_str()).c_str(), &s ) < 0 )
    {
        VolumeSummaries_t volumeSummaries;
        VolumeDynamicInfos_t volumeDynamicInfos;
        VolumeInfoCollector volumeCollector;
        volumeCollector.GetVolumeInfos(volumeSummaries, volumeDynamicInfos, false); 
        VolumeSummaries_t::const_iterator volSummaryIterB = volumeSummaries.begin() ;
        VolumeSummaries_t::const_iterator volSummaryIterE = volumeSummaries.end() ;
        std::string repoDevicePath = repoDevice ;
        std::string newRepoDevice ;
        if( repoDevicePath.length() == 1 )
        {
            repoDevicePath += ":" ;
        }
        repoDevicePath += "\\" ;

        while( volSummaryIterB != volSummaryIterE )
        {
            std::string newsparsefilepath ;
            std::string volume = volSummaryIterB->name ;
            if( volume.length() == 1 )
            {
                volume += ":\\" ;
            }
            newsparsefilepath = volume + sparsepartfile.str().substr( repoDevicePath.length() ) ;
            if( sv_stat( getLongPathName(newsparsefilepath.c_str()).c_str(), &s ) == 0 )
            {
                newRepoDevice = volume ;
                if( newRepoDevice.length() > 3 && newRepoDevice[newRepoDevice.length() - 1 ] == '\\' )
                {
                    newRepoDevice.erase( newRepoDevice.length() - 1 ) ;
                }
                if( newRepoDevice.length() <= 3 )
                {
                    newRepoDevice = newRepoDevice[0] ;
                }
            }
            if( !newRepoDevice.empty() )
            {
                break ;
            }
            volSummaryIterB++ ;
        }
        std::string alertMsg ;
        if( !newRepoDevice.empty() )
        {
            alertMsg = "The backup location device drive letter changed to " ;
            alertMsg += newRepoDevice ;
            alertMsg += ". It might have happened due to server reboot. ";
            alertMsg += repoDevice ;
            AlertConfigPtr alertConfigPtr = AlertConfig::GetInstance() ;
            alertConfigPtr->AddAlert("FATAL", "CX", SV_ALERT_SIMPLE, SV_ALERT_MODULE_RESYNC, alertMsg) ;
        }
    }
}

INM_ERROR MonitorHandler::DownloadAlerts(FunctionInfo& request)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	INM_ERROR errCode = E_SUCCESS;
    LocalConfigurator configurator ;
	bool loadgrps = true ;
	ConfSchema::Group dummyalertgrp ;
	if( request.m_RequestPgs.m_Params.find("ProcessWithoutlock") != request.m_RequestPgs.m_Params.end() )
	{
		loadgrps = false ;
	}
    AlertConfigPtr alertConfig = AlertConfig::GetInstance(loadgrps) ;
	if( !loadgrps )
	{
		alertConfig->SetAlertGrp(dummyalertgrp) ;
	}
	InmServiceStatus status = INM_SVCSTAT_NONE;
	getServiceStatus( "backupagent", status) ;
	bool accessibleRepo = true ;
	std::string repoLocation = configurator.getRepositoryLocation() ;
	if( status == INM_SVCSTAT_STOPPED)
	{
		alertConfig->AddTmpAlert("CRITICAL", "CX", SV_ALERT_SIMPLE, SV_ALERT_MODULE_RESYNC, 
						"Backupagent is not running, Please start it for backup to continue.") ;
	}
	int agentRepoAccessError ;
	SV_LONG agentRepoLastAccessTime ;
	std::string errmsg ;
	GetAgentRepositoryAccess( agentRepoAccessError, agentRepoLastAccessTime,errmsg ) ;
	if(	agentRepoAccessError != -1 && agentRepoAccessError != 0 )
	{
		if(!errmsg.empty())	
		{
			std::string repoAccessFailedAlert = "Backupagent service is failing to access the backup location " + repoLocation + ". " + errmsg ;
			alertConfig->AddTmpAlert("CRITICAL", "CX", SV_ALERT_SIMPLE, SV_ALERT_MODULE_RESYNC, 
						repoAccessFailedAlert) ;
		}
	}
	if( GetInMageRebootPendingKey())
	{
		alertConfig->AddTmpAlert("CRITICAL", "CX", SV_ALERT_SIMPLE, SV_ALERT_MODULE_RESYNC, 
						"The recent installed update requires system reboot for correct functioning of the product.") ;
	}
	std::string alertMsg ;
    try
    {
        getConfigStorePath() ;
    }
    catch(TransactionException& transactionException)
    {
        accessibleRepo = false ;
		alertMsg = "The backup location device " ;
        alertMsg +=  repoLocation ;
        alertMsg += " is not accessible. " ;
        alertMsg += "Please make sure the device is available and accessible." ;
    }
	if( repoLocation.length() > 2 && repoLocation[0] == '\\' && repoLocation[1] == '\\' )
	{
		alertMsg.clear() ;
		INM_ERROR err = E_SUCCESS ;
		AccessRemoteShare(repoLocation, err, alertMsg) ;
		if( err != E_SUCCESS )
		{
			alertMsg = getErrorMessage( err ) ;
		}
	}
	if( !alertMsg.empty() )
	{
		alertConfig->AddTmpAlert("FATAL", "CX", SV_ALERT_SIMPLE, SV_ALERT_MODULE_RESYNC, alertMsg) ;
	}
	VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
	
	if( accessibleRepo && !volumeConfigPtr->IsValidRepository() )
	{
		alertConfig->AddTmpAlert("FATAL", "CX", SV_ALERT_SIMPLE, SV_ALERT_MODULE_RESYNC, repoLocation + " is not a valid backup location" ) ;
	}
    int counter = configurator.getVirtualVolumesId();
    int novols = 1;
    while(counter != 0) 
    {
        char regData[26];
		inm_sprintf_s(regData, ARRAYSIZE(regData), "VirVolume%d", counter);

        std::string data = configurator.getVirtualVolumesPath(regData);

        std::string sparsefilename;
        std::string volume;

        if (ParseSparseFileNameAndVolPackName(data, sparsefilename, volume))
        {
            CheckRepositoryValidity( configurator.getRepositoryLocation(), sparsefilename ) ; 
            break ;
        }
		counter--;
    }
    ConfSchema::Group& alertGroup = alertConfig->GetAlertGroup( ) ;
    ConfSchema::Group& tmpAlertGroup = *(alertConfig->m_tmpAlertGroup) ;
    errCode = polulateAlertResponse( request, alertGroup, tmpAlertGroup ) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return errCode;
}


INM_ERROR MonitorHandler::polulateAlertResponse(FunctionInfo& request,ConfSchema::Group& alertGroup,ConfSchema::Group &tmpAlertGroup)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	INM_ERROR errCode = E_UNKNOWN;
	std::string Lastdownloadedtimestamp,deleteOld;
	bool validDate = true;
	std::vector<std::string> deleteAlerts;
	Parameters_t::const_iterator paramIter 	= request.m_RequestPgs.m_Params.find("Lastdownloadedtimestamp"); // Bug 17056 changed LastDownloadedTimestamp to Lastdownloadedtimestamp
	ConfSchema::AlertObjcet m_AlertObj ;
	if(paramIter!=request.m_RequestPgs.m_Params.end())
	{
		Lastdownloadedtimestamp = paramIter->second.value;
	}
	paramIter 	= request.m_RequestPgs.m_Params.find("DeleteOld");
	if(paramIter != request.m_RequestPgs.m_Params.end())
	{
		deleteOld = paramIter->second.value;
	}

	if(!Lastdownloadedtimestamp.empty())
	{
		std::string year, month, date, hour, min, sec ;
		/*year = Lastdownloadedtimestamp.substr(0, 4) ;
		month = Lastdownloadedtimestamp.substr(5, 2) ;
		date = Lastdownloadedtimestamp.substr(8, 2) ;
		hour = Lastdownloadedtimestamp.substr(11, 2) ;
		min = Lastdownloadedtimestamp.substr(14, 2) ;
		sec = Lastdownloadedtimestamp.substr(17,2) ;
		*/
		month = Lastdownloadedtimestamp.substr(0, 2) ;
		date =  Lastdownloadedtimestamp.substr(3, 2) ;
		year =  Lastdownloadedtimestamp.substr(6, 4) ;
		hour =  Lastdownloadedtimestamp.substr(11, 2) ;
		min =   Lastdownloadedtimestamp.substr(14, 2) ;
		sec =  Lastdownloadedtimestamp.substr(17, 2) ;

		validDate = dateValidator (year,month,date,hour,min,sec);
		if (validDate == false )
		{
			request.m_Message = "Invalid Lastdownloadedtimestamp Paramater in requestxml (yyyy-mm-dd hh-mm-ss)";
			errCode = E_WRONG_PARAM;
		}
		Lastdownloadedtimestamp = year+month+date+hour+min+sec;

	}	
	
	int count = 0;
    ConfSchema::Object_t downloadedAlerts ;
	ConfSchema::ObjectsReverseIter_t alertObjIter = alertGroup.m_Objects.rbegin();
	while(alertObjIter != alertGroup.m_Objects.rend())
	{
		std::string alerttime;
		ParameterGroup pg;
		ValueType vt;
		ConfSchema::AttrsIter_t attrIter = alertObjIter->second.m_Attrs.find(m_AlertObj.GetAlertTimestampAttrName());
		if(attrIter != alertObjIter->second.m_Attrs.end())
		{
			alerttime = attrIter->second;
			std::string year, month, date, hour, min, sec ;
			year = alerttime.substr(1, 4) ;
			month = alerttime.substr(6, 2) ;
			date = alerttime.substr(9, 2) ;
			hour = alerttime.substr(12, 2) ;
			min = alerttime.substr(15, 2) ;
			sec = alerttime.substr(18,2) ;
			alerttime = year+month+date+hour+min+sec;
			bool process = false ;
			if( Lastdownloadedtimestamp.empty() )
			{
				process = true ;
			}
			else if(boost::lexical_cast<SV_ULONGLONG>(alerttime) > boost::lexical_cast<SV_ULONGLONG>(Lastdownloadedtimestamp))
			{
				process = true ;
			}
			if( process )
			{
				if(attrIter!=alertObjIter->second.m_Attrs.end())
				{
					vt.value = month + "-" + date + "-" + year + " " + hour + ":" + min + ":" + sec ;
					pg.m_Params.insert(std::make_pair("Timestamp",vt));
				}
				attrIter = alertObjIter->second.m_Attrs.find(m_AlertObj.GetCountAttrName());
				if(attrIter!=alertObjIter->second.m_Attrs.end())
				{
					vt.value = attrIter->second;
					pg.m_Params.insert(std::make_pair("OccuranceCount",vt));
				}
				attrIter = alertObjIter->second.m_Attrs.find(m_AlertObj.GetLevelAttrName());
				if(attrIter!=alertObjIter->second.m_Attrs.end())
				{
					vt.value = attrIter->second;
					pg.m_Params.insert(std::make_pair("Level",vt));
				}
				attrIter = alertObjIter->second.m_Attrs.find(m_AlertObj.GetMessageAttrName());
				if(attrIter!=alertObjIter->second.m_Attrs.end())
				{
					vt.value = attrIter->second;
					pg.m_Params.insert(std::make_pair("Message",vt));
				}
				std::string alertno = "Alert["+ boost::lexical_cast<std::string>(count++) + "]" ; // Replace AlertID with Alert Bug 16942
				request.m_ResponsePgs.m_ParamGroups.insert(std::make_pair(alertno,pg));
                downloadedAlerts.insert( *alertObjIter ) ;
			}
		}
		alertObjIter++;
	}
	if ( InmStrCmp<NoCaseCharCmp>(deleteOld,"Yes") == 0 || deleteOld.empty()) 
	{
		ConfSchema::ObjectsIter_t downloadedAlertIter = downloadedAlerts.begin() ;
		while( downloadedAlertIter != downloadedAlerts.end() )
		{
			alertGroup.m_Objects.erase( downloadedAlertIter->first ) ;
			downloadedAlertIter++ ;
		}
	}
	alertObjIter = tmpAlertGroup.m_Objects.rbegin();
	while(alertObjIter != tmpAlertGroup.m_Objects.rend())
	{
		std::string alerttime;
		ParameterGroup pg;
		ValueType vt;
		ConfSchema::AttrsIter_t attrIter = alertObjIter->second.m_Attrs.find(m_AlertObj.GetAlertTimestampAttrName());
		if(attrIter != alertObjIter->second.m_Attrs.end())
		{
			alerttime = attrIter->second;
			std::string year, month, date, hour, min, sec ;
			year = alerttime.substr(1, 4) ;
			month = alerttime.substr(6, 2) ;
			date = alerttime.substr(9, 2) ;
			hour = alerttime.substr(12, 2) ;
			min = alerttime.substr(15, 2) ;
			sec = alerttime.substr(18,2) ;
			alerttime = year+month+date+hour+min+sec;
			bool process = false ;
			if( Lastdownloadedtimestamp.empty() )
			{
				process = true ;
			}
			else if(boost::lexical_cast<SV_ULONGLONG>(alerttime) > boost::lexical_cast<SV_ULONGLONG>(Lastdownloadedtimestamp))
			{
				process = true ;
			}
			if( process )
			{
				if(attrIter!=alertObjIter->second.m_Attrs.end())
				{
					vt.value = month + "-" + date + "-" + year + " " + hour + ":" + min + ":" + sec ;
					pg.m_Params.insert(std::make_pair("Timestamp",vt));
				}
				attrIter = alertObjIter->second.m_Attrs.find(m_AlertObj.GetCountAttrName());
				if(attrIter!=alertObjIter->second.m_Attrs.end())
				{
					vt.value = attrIter->second;
					pg.m_Params.insert(std::make_pair("OccuranceCount",vt));
				}
				attrIter = alertObjIter->second.m_Attrs.find(m_AlertObj.GetLevelAttrName());
				if(attrIter!=alertObjIter->second.m_Attrs.end())
				{
					vt.value = attrIter->second;
					pg.m_Params.insert(std::make_pair("Level",vt));
				}
				attrIter = alertObjIter->second.m_Attrs.find(m_AlertObj.GetMessageAttrName());
				if(attrIter!=alertObjIter->second.m_Attrs.end())
				{
					vt.value = attrIter->second;
					pg.m_Params.insert(std::make_pair("Message",vt));
				}
				std::string alertno = "Alert["+ boost::lexical_cast<std::string>(count++) + "]" ; // Replace AlertID with Alert Bug 16942
				request.m_ResponsePgs.m_ParamGroups.insert(std::make_pair(alertno,pg));
			}
		}
		alertObjIter++;
	}
	tmpAlertGroup.m_Objects.clear();
	if( request.m_RequestPgs.m_Params.find("ProcessWithoutlock") == request.m_RequestPgs.m_Params.end() )
	{
		ConfSchemaReaderWriterPtr confschemardrwrtr = ConfSchemaReaderWriter::GetInstance() ;
		confschemardrwrtr->Sync() ;
	}
    
	errCode = E_SUCCESS;
	
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	return errCode;
}


INM_ERROR MonitorHandler::HealthStatus(FunctionInfo& request)
{
	INM_ERROR errCode = E_SUCCESS ;
	ParameterGroup repositoryPg, agenthealthpg ;
	ValueType vt ;
	int agentRepoAccessError ;
	SV_LONG agentRepoLastAccessTime ;
	vt.value = boost::lexical_cast<std::string>(GetAgentHeartBeatTime()) ;
	agenthealthpg.m_Params.insert(std::make_pair( "HeartBeatTimeInGMT", vt)) ;
	std::string errmsg ;
	GetAgentRepositoryAccess( agentRepoAccessError, agentRepoLastAccessTime,errmsg ) ;
	vt.value = boost::lexical_cast<std::string>(agentRepoLastAccessTime) ;
	agenthealthpg.m_Params.insert(std::make_pair( "LastSuccessfulRepoAccessTimeInGMT", vt)) ;
	vt.value = boost::lexical_cast<std::string>(GetBookMarkSuccessTimeStamp()) ;
	agenthealthpg.m_Params.insert(std::make_pair( "LastSuccessfulBookMarkTimeInGMT", vt)) ;
	vt.value = getErrorMessage((INM_ERROR)agentRepoAccessError) ;
	agenthealthpg.m_Params.insert(std::make_pair( "RepositoryAccessReason", vt)) ;
	INM_ERROR error ;
	errmsg = "" ; 
	AccessRemoteShare(error, errmsg) ;
	if( error == E_SUCCESS )
	{
		UpdateRepositoryAccess() ;
	}
	InmServiceStatus status = INM_SVCSTAT_NONE;
	getServiceStatus( "backupagent", status) ;
	if( status != INM_SVCSTAT_RUNNING )
	{
#ifdef SV_WINDOWS
		vt.value = GetServiceStopReason() ;
#endif
	}
	else
	{
		vt.value = "" ;
	}
	//agenthealthpg.m_Params.insert(std::make_pair( "ServiceStopReason", vt)) ;
	vt.value = getErrorMessage(error) ;
	repositoryPg.m_Params.insert(std::make_pair( "RepositoryAccessReason", vt)) ; 
	vt.value = boost::lexical_cast<std::string>(GetRepositoryAccessTime()) ;
	repositoryPg.m_Params.insert(std::make_pair( "LastSuccessfulRepoAccessTimeInGMT", vt)) ;
	LocalConfigurator lc ;
	vt.value = "Not Configured" ;
	try
	{
		vt.value = lc.getRepositoryLocation() ;
	}
	catch(ContextualException& cex)
	{
	}
	repositoryPg.m_Params.insert(std::make_pair( "RepositoryPath", vt)) ;
	request.m_ResponsePgs.m_ParamGroups.insert(std::make_pair( "RepositoryHealth", repositoryPg)) ;
	request.m_ResponsePgs.m_ParamGroups.insert(std::make_pair( "AgentHealth", agenthealthpg)) ;
	return E_SUCCESS ;
}
INM_ERROR MonitorHandler::DownloadAudit(FunctionInfo& request)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	INM_ERROR errCode = E_SUCCESS;
    AuditConfigPtr auditConfig = AuditConfig::GetInstance() ;
    ConfSchema::Group& auditGroup = auditConfig->GetAuditGroup( ) ;
    errCode = polulateAuditResponse( request, auditGroup ) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return errCode;
}


INM_ERROR MonitorHandler::polulateAuditResponse(FunctionInfo& request,ConfSchema::Group& auditGroup)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	INM_ERROR errCode = E_UNKNOWN;
	std::string Lastdownloadedtimestamp;
	bool validDate = true;
	Parameters_t::const_iterator paramIter 	= request.m_RequestPgs.m_Params.find("Lastdownloadedtimestamp"); // Bug 17056 changed LastDownloadedTimestamp to Lastdownloadedtimestamp
	ConfSchema::AuditObject m_AuditObj ;
	ConfSchema::Object_t downloadedAudits ;
	if(paramIter!=request.m_RequestPgs.m_Params.end())
	{
		Lastdownloadedtimestamp = paramIter->second.value;
	}
	if(!Lastdownloadedtimestamp.empty())
	{
		std::string year, month, date, hour, min, sec ;
		year = Lastdownloadedtimestamp.substr(0, 4) ;
		month = Lastdownloadedtimestamp.substr(5, 2) ;
		date = Lastdownloadedtimestamp.substr(8, 2) ;
		hour = Lastdownloadedtimestamp.substr(11, 2) ;
		min = Lastdownloadedtimestamp.substr(14, 2) ;
		sec = Lastdownloadedtimestamp.substr(17,2) ;
		
		validDate = dateValidator (year,month,date,hour,min,sec);
		if (validDate == false )
		{
			request.m_Message = "Invalid Lastdownloadedtimestamp Paramater in requestxml (yyyy-mm-dd hh-mm-ss)";
			errCode = E_WRONG_PARAM;
		}
		Lastdownloadedtimestamp = year+month+date+hour+min+sec;

	}	
	int count = 0;
    ConfSchema::Object_t downloadedAudit ;
	ConfSchema::ObjectsReverseIter_t auditObjIter = auditGroup.m_Objects.rbegin();
	while(auditObjIter != auditGroup.m_Objects.rend())
	{
		std::string audittime;
		ParameterGroup pg;
		ValueType vt;
		ConfSchema::AttrsIter_t attrIter = auditObjIter->second.m_Attrs.find(m_AuditObj.GetAuditTimestampAttrName());
		if(attrIter != auditObjIter->second.m_Attrs.end())
		{
			audittime = attrIter->second;
			std::string year, month, date, hour, min, sec ;
			year = audittime.substr(0, 4) ;
			month = audittime.substr(5, 2) ;
			date = audittime.substr(8, 2) ;
			hour = audittime.substr(11, 2) ;
			min = audittime.substr(14, 2) ;
			sec = audittime.substr(17,2) ;
			audittime = year+month+date+hour+min+sec;
			bool process = false;
			if( Lastdownloadedtimestamp.empty() )
			{
				process = true ;
			}
			else if(boost::lexical_cast<SV_ULONGLONG>(audittime) > boost::lexical_cast<SV_ULONGLONG>(Lastdownloadedtimestamp))
			{
				process = true ;
			}
			
			if( process )
			{
				if(attrIter!=auditObjIter->second.m_Attrs.end())
				{
					vt.value = month + "-" + date + "-" + year + " " + hour + "-" + min + "-" + sec ;
					pg.m_Params.insert(std::make_pair("Timestamp",vt));
				}
				attrIter = auditObjIter->second.m_Attrs.find(m_AuditObj.GetMessageAttrName());
				if(attrIter!=auditObjIter->second.m_Attrs.end())
				{
					vt.value = attrIter->second;
					pg.m_Params.insert(std::make_pair("Message",vt));
				}
				std::string auditno = "Audit["+ boost::lexical_cast<std::string>(count++) + "]" ; // Replace AlertID with Alert Bug 16942
				request.m_ResponsePgs.m_ParamGroups.insert(std::make_pair(auditno,pg));
                downloadedAudits.insert( *auditObjIter ) ;
			}
		}
		auditObjIter++;
	}
    ConfSchemaReaderWriterPtr confschemardrwrtr = ConfSchemaReaderWriter::GetInstance() ;
    confschemardrwrtr->Sync() ;
	errCode = E_SUCCESS;
	
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	return errCode;
}
