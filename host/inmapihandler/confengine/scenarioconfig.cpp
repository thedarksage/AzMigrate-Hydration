#include <boost/lexical_cast.hpp>
#include "confschemamgr.h"
#include "scenarioconfig.h"
#include "portable.h"
#include "portablehelpers.h"
#include "confschemamgr.h"
#include "InmsdkGlobals.h"
#include "util.h"
#include "confschemareaderwriter.h"
#include "volumeconfig.h"
#include "auditconfig.h"

ScenarioConfigPtr ScenarioConfig::m_scenarioConfigPtr ;
ScenarioConfigPtr ScenarioConfig::GetInstance() 
{
    if( !m_scenarioConfigPtr )
    {
        m_scenarioConfigPtr.reset( new ScenarioConfig() ) ;
    }
    m_scenarioConfigPtr->loadScenarioConfig() ;
    return m_scenarioConfigPtr ;
}

ScenarioConfig::ScenarioConfig()
{
    m_isModified = false ;
    m_PlanGrp = NULL ;
}

void ScenarioConfig::loadScenarioConfig()
{
    ConfSchemaReaderWriterPtr confSchemaReaderWriter = ConfSchemaReaderWriter::GetInstance() ;
    m_PlanGrp = confSchemaReaderWriter->getGroup( m_planAttrNamesObj.GetName() ) ;
}

bool ScenarioConfig::PopulateConsistencyOptions(const ConsistencyOptions& consistencyOptions)
{
    bool bRet = false ;
    ConfSchema::Object* scenarioDetailsObject = NULL ;
    if( GetScenarioDetailsObject(&scenarioDetailsObject) )
    {
        PopulateConsistencyOptions( *scenarioDetailsObject, consistencyOptions ) ;
        bRet = true ;
    }
    return bRet ;
}

void ScenarioConfig::PopulateConsistencyOptions(ConfSchema::Object& scenarioDetailsObj, 
                                                const ConsistencyOptions& consistencyOptions)
{
    ConfSchema::Object consistencyOptionsObj ;
    ConfSchema::Attrs_t& consistencyAttrs = consistencyOptionsObj.m_Attrs ;
    consistencyAttrs.insert(std::make_pair( m_consistencyPolicyAttrNamesObj.GetExceptionAttrName(), 
        consistencyOptions.exception)) ;
    consistencyAttrs.insert(std::make_pair( m_consistencyPolicyAttrNamesObj.GetIntervalAttrName(), 
        consistencyOptions.interval)) ;
    consistencyAttrs.insert(std::make_pair( m_consistencyPolicyAttrNamesObj.GetTagNameAttrName(), 
        consistencyOptions.tagName)) ;
    consistencyAttrs.insert(std::make_pair( m_consistencyPolicyAttrNamesObj.GetTagTypeAttrName(), 
        consistencyOptions.tagType)) ;
    ConfSchema::Group consistencyGrp ;
    consistencyGrp.m_Objects.insert( std::make_pair("Consistency Options",
        consistencyOptionsObj )) ;
    scenarioDetailsObj.m_Groups.erase( m_consistencyPolicyAttrNamesObj.GetName() );
    scenarioDetailsObj.m_Groups.insert( std::make_pair(m_consistencyPolicyAttrNamesObj.GetName(), consistencyGrp)) ;    
}


void ScenarioConfig::PopulateRetentionWindows( ConfSchema::Object& retentionPolicyObj, 
                                              const std::list<ScenarioRetentionWindow>& retentionWindows)
{
    ConfSchema::Group RetentionWindowGrp ;
    std::list<ScenarioRetentionWindow>::const_iterator retentionWindowIter = retentionWindows.begin() ;
    AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
    while( retentionWindowIter != retentionWindows.end() )
    {
        ConfSchema::Object retentionWindowObj ;
        ConfSchema::Attrs_t& retentionWindowAttrs = retentionWindowObj.m_Attrs ;
        retentionWindowAttrs.insert( std::make_pair( m_scenarioretwindowAttrNamesObj.GetRetentionWindowIdAttrName(),
            retentionWindowIter->retentionWindowId)) ;

        retentionWindowAttrs.insert( std::make_pair( m_scenarioretwindowAttrNamesObj.GetdurationAttrName(),
            retentionWindowIter->duration)) ;
        auditConfigPtr->LogAudit( retentionWindowIter->retentionWindowId + " retention window set/changed to " + retentionWindowIter->duration ) ;
        retentionWindowAttrs.insert( std::make_pair( m_scenarioretwindowAttrNamesObj.GetGranualarityAttrName(),
            retentionWindowIter->granularity)) ;
        retentionWindowAttrs.insert( std::make_pair( m_scenarioretwindowAttrNamesObj.GetCountAttrName(),
            boost::lexical_cast<std::string>(retentionWindowIter->count))) ;
        RetentionWindowGrp.m_Objects.insert(std::make_pair( retentionWindowIter->retentionWindowId, retentionWindowObj)) ;
        retentionWindowIter++ ;
    }
    retentionPolicyObj.m_Groups.insert(std::make_pair( m_scenarioretwindowAttrNamesObj.GetName(), RetentionWindowGrp)) ;
}

void ScenarioConfig::PopulateRetetionPolicy( ConfSchema::Object& pairObject,
                                            const RetentionPolicy& retPolicy)
{
    ConfSchema::Group retentionPolicyGrp ;
    ConfSchema::Object retentionPolicyObj ;
    ConfSchema::Attrs_t& retentionPolicyAttrs = retentionPolicyObj.m_Attrs ;
    retentionPolicyAttrs.insert( std::make_pair( m_scenarioRetentionPolicyAttrNamesObj.GetDurationAttrName(), 
        retPolicy.duration)) ;
    retentionPolicyAttrs.insert( std::make_pair( m_scenarioRetentionPolicyAttrNamesObj.GetRetentionTypeAttrName(), 
        retPolicy.retentionType)) ;
    retentionPolicyAttrs.insert( std::make_pair( m_scenarioRetentionPolicyAttrNamesObj.GetSizeAttrName(), 
        retPolicy.size)) ;
    AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
    auditConfigPtr->LogAudit( "Default Retention Duration is set/changed to " + retPolicy.duration + " for " + 
        pairObject.m_Attrs.find( m_pairAttrNamesObj.GetSourceVolumeAttrName())->second) ;
    PopulateRetentionWindows( retentionPolicyObj, retPolicy.retentionWindows) ;
    retentionPolicyGrp.m_Objects.insert(std::make_pair( "RetentionPolicyObject", 
        retentionPolicyObj)) ;
    pairObject.m_Groups.insert(std::make_pair( m_scenarioRetentionPolicyAttrNamesObj.GetName(), 
        retentionPolicyGrp)) ;
}

void ScenarioConfig::AddPairs( ConfSchema::Group& pairsGrp, 
                              const std::map<std::string, PairDetails>& pairDetailsMap) 
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::map<std::string, PairDetails>::const_iterator pairDetailIter ;
    pairDetailIter = pairDetailsMap.begin() ;

    while( pairDetailIter != pairDetailsMap.end() )
    {
        ConfSchema::Object PairObj ;
        ConfSchema::Attrs_t& pairObjAttrs = PairObj.m_Attrs ;
        pairObjAttrs.insert( std::make_pair( m_pairAttrNamesObj.GetMoveRetentionPathAttrName(), "")) ;
        pairObjAttrs.insert( std::make_pair( m_pairAttrNamesObj.GetProcessServerAttrName(), "")) ;

        pairObjAttrs.insert( std::make_pair( m_pairAttrNamesObj.GetRetentionPathAttrName(), 
            pairDetailIter->second.retentionPath)) ;
        pairObjAttrs.insert( std::make_pair( m_pairAttrNamesObj.GetRetentionVolumeAttrName(), 
            pairDetailIter->second.retentionVolume)) ;
        pairObjAttrs.insert( std::make_pair( m_pairAttrNamesObj.GetRpoThresholdAttrName(),
            boost::lexical_cast<std::string>(pairDetailIter->second.rpoThreshold))) ;
        pairObjAttrs.insert( std::make_pair( m_pairAttrNamesObj.GetSourceVolumeAttrName(), 
            pairDetailIter->first)) ;
        pairObjAttrs.insert( std::make_pair( m_pairAttrNamesObj.GetTargetVolumeAttrName(), 
            pairDetailIter->second.targetVolume)) ;
        pairObjAttrs.insert( std::make_pair( m_pairAttrNamesObj.GetVolumeProvisioningStatusAttrName(), 
            VOL_PACK_PENDING)) ;
        pairObjAttrs.insert( std::make_pair( m_pairAttrNamesObj.GetSrcCapacityAttrName(), 
            boost::lexical_cast<std::string>(pairDetailIter->second.srcVolCapacity))) ;
        pairObjAttrs.insert( std::make_pair( m_pairAttrNamesObj.GetSrcRawSizeAttrName(), 
            boost::lexical_cast<std::string>(pairDetailIter->second.srcVolRawSize))) ;

        PopulateRetetionPolicy( PairObj, pairDetailIter->second.retPolicy) ;
        pairsGrp.m_Objects.insert(std::make_pair(
            pairDetailIter->first + "<->" + pairDetailIter->second.targetVolume, PairObj)) ;
        pairDetailIter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ScenarioConfig::PopulatePairs( ConfSchema::Object& pairInfoObject, 
                                   const std::map<std::string, PairDetails>& pairDetailsMap) 
{
    std::map<std::string, PairDetails>::const_iterator pairDetailsIter = pairDetailsMap.begin() ;
    ConfSchema::Group pairsGrp ;
    while( pairDetailsIter != pairDetailsMap.end() )
    {
        ConfSchema::Object PairObj ;
        ConfSchema::Attrs_t& pairObjAttrs = PairObj.m_Attrs ;
        pairObjAttrs.insert( std::make_pair( m_pairAttrNamesObj.GetMoveRetentionPathAttrName(), "")) ;
        pairObjAttrs.insert( std::make_pair( m_pairAttrNamesObj.GetProcessServerAttrName(), "")) ;

        pairObjAttrs.insert( std::make_pair( m_pairAttrNamesObj.GetRetentionPathAttrName(), 
            pairDetailsIter->second.retentionPath)) ;
        pairObjAttrs.insert( std::make_pair( m_pairAttrNamesObj.GetRetentionVolumeAttrName(), 
            pairDetailsIter->second.retentionVolume)) ;
        pairObjAttrs.insert( std::make_pair( m_pairAttrNamesObj.GetRpoThresholdAttrName(),
            boost::lexical_cast<std::string>(pairDetailsIter->second.rpoThreshold))) ;
        pairObjAttrs.insert( std::make_pair( m_pairAttrNamesObj.GetSourceVolumeAttrName(), 
            pairDetailsIter->first)) ;
        pairObjAttrs.insert( std::make_pair( m_pairAttrNamesObj.GetTargetVolumeAttrName(), 
            pairDetailsIter->second.targetVolume)) ;
        pairObjAttrs.insert( std::make_pair( m_pairAttrNamesObj.GetVolumeProvisioningStatusAttrName(), 
            VOL_PACK_PENDING)) ;
        pairObjAttrs.insert( std::make_pair( m_pairAttrNamesObj.GetSrcCapacityAttrName(), 
            boost::lexical_cast<std::string>(pairDetailsIter->second.srcVolCapacity))) ;
        pairObjAttrs.insert( std::make_pair( m_pairAttrNamesObj.GetSrcRawSizeAttrName(), 
            boost::lexical_cast<std::string>(pairDetailsIter->second.srcVolRawSize))) ;

        PopulateRetetionPolicy( PairObj, pairDetailsIter->second.retPolicy) ;
        pairsGrp.m_Objects.insert( std::make_pair( pairDetailsIter->first + "<->" + pairDetailsIter->second.targetVolume, PairObj)) ;
        pairDetailsIter++ ;
    }
    pairInfoObject.m_Groups.insert(std::make_pair( m_pairAttrNamesObj.GetName(), pairsGrp)) ;
}

void ScenarioConfig::PopulateAutoResyncOptions(ConfSchema::Object& replicationOptionsObj, const AutoResyncOptions& autoresyncOptions)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTER %s\n", FUNCTION_NAME) ;
    ConfSchema::Group autoResyncOptionsGrp ;
    ConfSchema::Object autoResyncOptionsObj ;
    ConfSchema::Attrs_t& autoresyncOptionsAttrs = autoResyncOptionsObj.m_Attrs ;
    autoresyncOptionsAttrs.insert( std::make_pair( m_autoresyncOptionAttrNamesObj.GetBetweenAttrName(), 
        autoresyncOptions.between)) ;
    autoresyncOptionsAttrs.insert( std::make_pair( m_autoresyncOptionAttrNamesObj.GetStateAttrName(),
        autoresyncOptions.state)) ;
    autoresyncOptionsAttrs.insert( std::make_pair( m_autoresyncOptionAttrNamesObj.GetWaitTimeAttrName(),
        boost::lexical_cast<std::string>(autoresyncOptions.waitTime))) ;
    autoResyncOptionsGrp.m_Objects.insert(std::make_pair( "ReplicationOptions", 
        autoResyncOptionsObj)) ;
    replicationOptionsObj.m_Groups.insert(std::make_pair( m_autoresyncOptionAttrNamesObj.GetName(), autoResyncOptionsGrp)) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

bool ScenarioConfig::PopulateReplicationOptions (const ReplicationOptions & replicationOptions)
{
	DebugPrintf (SV_LOG_DEBUG, "ENTER %s \n", FUNCTION_NAME );
    bool bRet = false ;
	bool fromRequest = true;
    ConfSchema::Object* pairInfoObj ;
    if( GetPairInfoObj(&pairInfoObj) )
	{
		PopulateReplicationOptions( *pairInfoObj, replicationOptions,fromRequest) ;
		bRet = true ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

void ScenarioConfig::PopulateReplicationOptions(ConfSchema::Object& pairInfoObject,const ReplicationOptions& replicationOptions , bool fromRequest = false) 
{
	DebugPrintf (SV_LOG_DEBUG, "ENTER %s \n", FUNCTION_NAME );
    ConfSchema::Group replicationOptionsGrp ;
    ConfSchema::Object replicationOptionsObj ;
    ConfSchema::Attrs_t& replicationOptionsAttrs = replicationOptionsObj.m_Attrs ;
    replicationOptionsAttrs.insert(std::make_pair( m_replicationOptionsAttrNamesObj.GetBatchResyncAttrName(), 
        boost::lexical_cast<std::string>(replicationOptions.batchSize))) ;
    PopulateAutoResyncOptions(replicationOptionsObj, replicationOptions.autoResyncOptions) ;
    replicationOptionsGrp.m_Objects.insert(std::make_pair("AutoResyncOptions", 
        replicationOptionsObj)) ;
    pairInfoObject.m_Groups.insert(std::make_pair( m_replicationOptionsAttrNamesObj.GetName(), replicationOptionsGrp)) ;

	if (fromRequest == true )
	{
		pairInfoObject.m_Groups.erase(m_replicationOptionsAttrNamesObj.GetName()) ;
		pairInfoObject.m_Groups.insert(std::make_pair( m_replicationOptionsAttrNamesObj.GetName(), replicationOptionsGrp)) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return;
}
void ScenarioConfig::PopulatePairInfo(ConfSchema::Object& scenarioDetailsObj,
                                      const std::map<std::string, PairDetails>& pairDetailsMap,
                                      const ReplicationOptions& replicationOptions)
{
    ConfSchema::Object pairInfoObject ;
    ConfSchema::Attrs_t& pairInfoAttrs = pairInfoObject.m_Attrs ;
    pairInfoAttrs.insert( std::make_pair( m_pairInfoAttrNamesObj.GetTargetHostIdAttrName(), "tgtHostId")) ;
    PopulatePairs(pairInfoObject, pairDetailsMap) ;
    PopulateReplicationOptions(pairInfoObject,replicationOptions) ;
    ConfSchema::Group pairInfoGroup ;
    pairInfoGroup.m_Objects.insert(std::make_pair( "PairInfoObject", pairInfoObject)) ;

    scenarioDetailsObj.m_Groups.insert( std::make_pair( m_pairInfoAttrNamesObj.GetName(), pairInfoGroup )) ;

}


void ScenarioConfig::PopulateProtectedVolumesInfo(ConfSchema::Object& scenarioDetailsObj,
                                                  const std::string& repositoryName,
                                                  const std::map<std::string, PairDetails>& pairDetailsMap)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    //Create attribute group with repository name and insert to BackupRepositories attribute group
    //repository name attribute group contain the protected volumes.
    ConfSchema::AttributeGroup backupRepoNameAttrGrp ;
    ConfSchema::AttributeGroup backupRepositoryAttrGrp ;
    std::map<std::string, PairDetails>::const_iterator pairDetailIter = pairDetailsMap.begin() ;
    int i = 0 ;
    std::string auditmsg ;
    auditmsg = "Initiating the protection of the volumes : " ;
    while( pairDetailIter != pairDetailsMap.end() )
    {
        backupRepoNameAttrGrp.m_Attrs.insert(std::make_pair("SourceVolume" + 
            boost::lexical_cast<std::string>(++i), pairDetailIter->first)) ;
        auditmsg += pairDetailIter->first ;
        auditmsg += ", " ;
        pairDetailIter++ ;
    }
    AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;

    auditConfigPtr->LogAudit( auditmsg ) ;

    backupRepositoryAttrGrp.m_AttrbGroups.insert(std::make_pair( repositoryName, backupRepoNameAttrGrp) ) ;

    scenarioDetailsObj.m_ObjAttrbGrps.m_AttrbGroups.insert(std::make_pair("BackupRepositories", 
        backupRepositoryAttrGrp)) ;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ScenarioConfig::PopulateExclusionVolumesInfo(ConfSchema::Object& scenarioDetailsObj,
                                                  const std::list<std::string>& exclusionVols)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::AttributeGroup excludedVolsAttrGrp;
    //excluded attribute group contains the excluded volumes
    std::list<std::string>::const_iterator excludedVolIter = exclusionVols.begin() ;
    int i = 0 ;
    std::string auditmsg ;
    if( exclusionVols.size() )
    {
        auditmsg = "The volumes excluded from protection are : " ;
    }

    while( excludedVolIter != exclusionVols.end() )
    {
        excludedVolsAttrGrp.m_Attrs.insert( std::make_pair( "Volume" + 
            boost::lexical_cast<std::string>(++i), *excludedVolIter)) ;
        auditmsg += *excludedVolIter ;
        auditmsg += "," ;
        excludedVolIter++ ;
    }
    if( exclusionVols.size() )
    {
        AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
        auditConfigPtr->LogAudit( auditmsg ) ;
    }
    scenarioDetailsObj.m_ObjAttrbGrps.m_AttrbGroups.insert(std::make_pair("excludedVolumes", excludedVolsAttrGrp)) ;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ScenarioConfig::PopulateSourceIdInfo(ConfSchema::Object& scenarioDetailsObj, const std::string& srcHostId)
{
    ConfSchema::AttributeGroup srcIdAttrGrp ;
    srcIdAttrGrp.m_Attrs.insert(std::make_pair("hostId1", srcHostId)) ;
    scenarioDetailsObj.m_ObjAttrbGrps.m_AttrbGroups.insert(std::make_pair("sourceId", srcIdAttrGrp)) ;
}

void ScenarioConfig::CreateScenarioDetailsObject(ConfSchema::Group* scenarioDetailsGrp,
                                                 const std::string& recoveryFeature,
                                                 const std::string& actionOnNewVolumes,
                                                 const std::string& srcHostId,
                                                 const std::string& tgtHostId,
                                                 const std::string& srcHostName,
                                                 const std::string& tgtHostName,
                                                 const std::string& backupRepoName,
                                                 const std::list<std::string>& excludedVols,
                                                 const ConsistencyOptions& consistencyOptions,
                                                 const std::map<std::string, PairDetails>& pairDetailsMap,
                                                 const ReplicationOptions& replicationOptions)
{
    ConfSchema::Object scenarioDetailsObj ;
    ConfSchema::Attrs_t& scenarioDetailsObjAttrs = scenarioDetailsObj.m_Attrs ;
    scenarioDetailsObjAttrs.insert( std::make_pair( m_scenarioDetailsAttrNamesObj.GetActionOnNewVolumeDiscoveryAttrName(), actionOnNewVolumes)) ;
    scenarioDetailsObjAttrs.insert( std::make_pair( m_scenarioDetailsAttrNamesObj.GetRecoveryFeatrureAttrName(), recoveryFeature)) ;
    scenarioDetailsObjAttrs.insert( std::make_pair( m_scenarioDetailsAttrNamesObj.GetRunReadinessCheckAttrName(), "No")) ;
    scenarioDetailsObjAttrs.insert( std::make_pair( m_scenarioDetailsAttrNamesObj.GetSoulutionTypeAttrName(), "Host")) ;
    scenarioDetailsObjAttrs.insert( std::make_pair( m_scenarioDetailsAttrNamesObj.GetSrcHostId(), srcHostId)) ;
    scenarioDetailsObjAttrs.insert( std::make_pair( m_scenarioDetailsAttrNamesObj.GetSrcHostName(), srcHostName)) ;
    scenarioDetailsObjAttrs.insert( std::make_pair( m_scenarioDetailsAttrNamesObj.GetTargetHostId(), tgtHostId)) ;
    scenarioDetailsObjAttrs.insert( std::make_pair( m_scenarioDetailsAttrNamesObj.GetTgtHostName(), tgtHostName)) ;


    PopulateSourceIdInfo(scenarioDetailsObj, srcHostId) ;
    PopulateProtectedVolumesInfo(scenarioDetailsObj, backupRepoName, pairDetailsMap) ;
    PopulateExclusionVolumesInfo(scenarioDetailsObj, excludedVols) ;
    PopulatePairInfo(scenarioDetailsObj, pairDetailsMap, replicationOptions) ;
    PopulateConsistencyOptions(scenarioDetailsObj, consistencyOptions) ;
    scenarioDetailsGrp->m_Objects.insert(std::make_pair( "Protectoin Scenario Details", 
        scenarioDetailsObj)) ;
}

void ScenarioConfig::CreateScenarioObject(ConfSchema::Group* scenarioGrp)
{
    ConfSchema::Object scenarioObj ;
    ConfSchema::Attrs_t& scenarioObjAttrs = scenarioObj.m_Attrs ;
    scenarioObjAttrs.insert( std::make_pair(m_scenarioAttrNamesObj.GetApplicationTypeAttrName(), "System")) ;
    scenarioObjAttrs.insert( std::make_pair(m_scenarioAttrNamesObj.GetExecutionStatusAttrName(),VOL_PACK_PENDING)) ;
    scenarioObjAttrs.insert( std::make_pair(m_scenarioAttrNamesObj.GetIsPrimaryAttrName(), "1")) ;
    scenarioObjAttrs.insert( std::make_pair(m_scenarioAttrNamesObj.GetReferenceIdAttrName(), "")) ;
    scenarioObjAttrs.insert( std::make_pair(m_scenarioAttrNamesObj.GetScenarioDescriptionAttrName(), "")) ;
    scenarioObjAttrs.insert( std::make_pair(m_scenarioAttrNamesObj.GetScenarioIdAttrName(), boost::lexical_cast<std::string>(time(NULL)))) ;
    scenarioObjAttrs.insert( std::make_pair(m_scenarioAttrNamesObj.GetScenarioNameAttrName(), "Backup Protection")) ;
    scenarioObjAttrs.insert( std::make_pair(m_scenarioAttrNamesObj.GetScenarionVersionAttrName(), "1.0")) ;
    scenarioObjAttrs.insert( std::make_pair(m_scenarioAttrNamesObj.GetScenarioTypeAttrName(), "Protection")) ;
    scenarioObjAttrs.insert( std::make_pair(m_scenarioAttrNamesObj.GetIsSystemPausedAttrName(), "0")) ;
    scenarioObj.m_Groups.insert( std::make_pair( m_scenarioDetailsAttrNamesObj.GetName(), ConfSchema::Group())) ;
    scenarioGrp->m_Objects.insert( std::make_pair( "Protection Scenario", scenarioObj) ) ;
}


void ScenarioConfig::CreatePlanObject()
{
    ConfSchema::Object planObj ;
    ConfSchema::Attrs_t& planAttrs = planObj.m_Attrs ;
    planAttrs.insert( std::make_pair( m_planAttrNamesObj.GetPlandIdAttrName(), "ID_"+ boost::lexical_cast<std::string>(time(NULL)))) ;
    planAttrs.insert( std::make_pair( m_planAttrNamesObj.GetPlanNameAttrName(), "Plan_" + boost::lexical_cast<std::string>(time(NULL)))) ;

    planObj.m_Groups.insert( std::make_pair(m_scenarioAttrNamesObj.GetName(), ConfSchema::Group())) ;

    m_PlanGrp->m_Objects.insert( std::make_pair("PlanObject", planObj)) ;
}

void ScenarioConfig::CreateProtectionScenario(const std::string& recoveryFeature,
                                              const std::string& actionOnNewVolumes,
                                              const std::string& srcHostId,
                                              const std::string& tgtHostId,
                                              const std::string& srcHostName,
                                              const std::string& tgtHostName,
                                              const std::string& backupRepoName,
                                              const std::list<std::string>& excludedVols,
                                              const ConsistencyOptions& consistencyOptions,
                                              const std::map<std::string, PairDetails>& pairDetailsMap,
                                              const ReplicationOptions& replicationOptions)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;

    //loadScenarioConfig() ;
    CreatePlanObject() ;
    ConfSchema::Object& planObj = m_PlanGrp->m_Objects.begin()->second ;
    ConfSchema::Group* scenarioGrpPtr ;

    scenarioGrpPtr = &(planObj.m_Groups.find( m_scenarioAttrNamesObj.GetName() )->second) ;

    CreateScenarioObject(scenarioGrpPtr) ;
    ConfSchema::Object& scenarioObj = scenarioGrpPtr->m_Objects.begin()->second ;

    ConfSchema::Group* scenarioDetailsGrpPtr ;
    scenarioDetailsGrpPtr = &(scenarioObj.m_Groups.find(m_scenarioDetailsAttrNamesObj.GetName())->second) ;

    CreateScenarioDetailsObject( scenarioDetailsGrpPtr, recoveryFeature, 
        actionOnNewVolumes, srcHostId, 
        tgtHostId, srcHostName, 
        tgtHostName,
        backupRepoName,
        excludedVols,
        consistencyOptions,
        pairDetailsMap,
        replicationOptions) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

bool ScenarioConfig::isModified()
{
    return m_isModified ;
}

bool ScenarioConfig::GetProtectionScenarioObject(ConfSchema::Object** scenarioObject)
{
    bool bRet = false ;
    if( m_PlanGrp->m_Objects.size() )
    {
        ConfSchema::Object& planObj = m_PlanGrp->m_Objects.begin()->second ;
        ConfSchema::Group* scenarioGrpPtr ;
        scenarioGrpPtr = &(planObj.m_Groups.find( m_scenarioAttrNamesObj.GetName() )->second) ;
        if( scenarioGrpPtr->m_Objects.size() )
        {
            *scenarioObject = &(scenarioGrpPtr->m_Objects.begin()->second) ;
            bRet = true ;
        }
    }
    return bRet ;
}

bool ScenarioConfig::ProtectionScenarioAvailable()
{
    bool bRet = false ;

    //loadScenarioConfig() ;
    ConfSchema::Object* scenarioObject = NULL ;
    if( GetProtectionScenarioObject(&scenarioObject) )
    {
        bRet = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
void ScenarioConfig::SetExecutionStatus(const std::string& executionStatus)
{
    //loadScenarioConfig() ;
    ConfSchema::Object* scenarioObject = NULL ;
    if( GetProtectionScenarioObject(&scenarioObject) )
    {
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = scenarioObject->m_Attrs.find( m_scenarioAttrNamesObj.GetExecutionStatusAttrName() ) ;
        DebugPrintf(SV_LOG_DEBUG, "Updating the scenario Status %s to %s\n", attrIter->second.c_str(), 
            executionStatus.c_str()) ;
        attrIter->second = executionStatus;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}


bool ScenarioConfig::GetExecutionStatus(std::string& executionStatus)
{
    bool bRet = false ;
    //loadScenarioConfig() ;
    ConfSchema::Object* scenarioObject = NULL ;
    if( GetProtectionScenarioObject(&scenarioObject) )
    {
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = scenarioObject->m_Attrs.find( m_scenarioAttrNamesObj.GetExecutionStatusAttrName() ) ;
        executionStatus = attrIter->second ;
        bRet = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ScenarioConfig::GetScenarioDetailsObject(ConfSchema::Object** scenarioDetailsObject)
{
    ConfSchema::Object* scenarioObject = NULL ;
    bool bRet = false ;
    if( GetProtectionScenarioObject(&scenarioObject) )
    {
        ConfSchema::GroupsIter_t groupIter ;
        groupIter = scenarioObject->m_Groups.find( m_scenarioDetailsAttrNamesObj.GetName()) ;
        if( groupIter != scenarioObject->m_Groups.end() )
        {
            ConfSchema::Group& scenarioDetailsGrp = groupIter->second ;
            if( scenarioDetailsGrp.m_Objects.size() )
            {
                *scenarioDetailsObject = &(scenarioDetailsGrp.m_Objects.begin()->second) ;
                bRet = true ;
            }
        }
    }
    return bRet ;
}

bool ScenarioConfig::GetPairObjectByTgtVolume(const std::string& volumeName,
                                              ConfSchema::Object** pairObject)
{
    bool bRet = false ;
    ConfSchema::Object* scenarioDetailsObject = NULL ;
    if( GetScenarioDetailsObject(&scenarioDetailsObject) )
    {
        ConfSchema::GroupsIter_t groupIter ;
        groupIter = scenarioDetailsObject->m_Groups.find( m_pairInfoAttrNamesObj.GetName() ) ;
        if( groupIter != scenarioDetailsObject->m_Groups.end() )
        {
            ConfSchema::Group& pairInfoGrp = groupIter->second ;
            ConfSchema::Object& pairInfoObj = pairInfoGrp.m_Objects.begin()->second ;
            groupIter = pairInfoObj.m_Groups.find( m_pairAttrNamesObj.GetName() ) ;
            if( groupIter != pairInfoObj.m_Groups.end() )
            {
                ConfSchema::Group& pairsGrp = groupIter->second ;
                ConfSchema::ObjectsIter_t pairObjIter = find_if( pairsGrp.m_Objects.begin(),
                    pairsGrp.m_Objects.end(),
                    ConfSchema::EqAttr(m_pairAttrNamesObj.GetTargetVolumeAttrName(),
                    volumeName.c_str())) ;
                if( pairObjIter != pairsGrp.m_Objects.end() )
                {
                    bRet = true ;
                    *pairObject = &(pairObjIter->second) ;
                }
            }
        }
    }
    return bRet ;
}

bool ScenarioConfig::UpdatePairObjects( std::string& rpoThreshold, RetentionPolicy& retentionPolicy, bool bRPOThreshold, bool bRetentionPolicy )
{
    bool bRet = false ;
    ConfSchema::Object* scenarioDetailsObject = NULL ;
    if( GetScenarioDetailsObject(&scenarioDetailsObject) )
    {
        ConfSchema::Object_t* pairObjects ;
        if( GetPairsObjects( &pairObjects )  )
        {
            ConfSchema::ObjectsIter_t objectsIter = pairObjects->begin() ;
            while( objectsIter != pairObjects->end() )
            {
                ConfSchema::Object* pairObject = &(objectsIter->second) ;
                if( bRPOThreshold )
                {                                    
                    pairObject->m_Attrs.erase( m_pairAttrNamesObj.GetRpoThresholdAttrName()) ;
                    pairObject->m_Attrs.insert( std::make_pair( m_pairAttrNamesObj.GetRpoThresholdAttrName(), rpoThreshold ) ) ;
                }
                if( bRetentionPolicy )
                {
                    pairObject->m_Groups.erase( m_scenarioRetentionPolicyAttrNamesObj.GetName() ) ;                                   
                    PopulateRetetionPolicy(  *pairObject, retentionPolicy ) ;                                   
                }
                objectsIter++ ; 
            }
            AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
            if( bRPOThreshold )
            {
                auditConfigPtr->LogAudit("RPO threshold is changed to " + rpoThreshold ) ;
            }
        }
    }
    return bRet ;
}

bool ScenarioConfig::GetPairsObjects( ConfSchema::Object_t** pairObjects ) 
{
    bool bRet = false ;
    ConfSchema::Object* scenarioDetailsObject = NULL ;
    if( GetScenarioDetailsObject(&scenarioDetailsObject) )
    {
        ConfSchema::GroupsIter_t groupIter ;
        groupIter = scenarioDetailsObject->m_Groups.find( m_pairInfoAttrNamesObj.GetName() ) ;
        if( groupIter != scenarioDetailsObject->m_Groups.end() )
        {
            ConfSchema::Group& pairInfoGrp = groupIter->second ;
            ConfSchema::Object& pairInfoObj = pairInfoGrp.m_Objects.begin()->second ;
            groupIter = pairInfoObj.m_Groups.find( m_pairAttrNamesObj.GetName() ) ;
            if( groupIter != pairInfoObj.m_Groups.end() )
            {
                ConfSchema::Group& pairsGrp = groupIter->second ;
                *pairObjects = & (pairsGrp.m_Objects) ;    
                bRet = true ;
            }
        }
    }
    return bRet ;
}
//making this function to return first pair object if no source volume is given
bool ScenarioConfig::GetPairObjectBySrcVolume(const std::string& volumeName,
                                              ConfSchema::Object** pairObject)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* scenarioDetailsObject = NULL ;
    if( GetScenarioDetailsObject(&scenarioDetailsObject) )
    {
        ConfSchema::GroupsIter_t groupIter ;
        groupIter = scenarioDetailsObject->m_Groups.find( m_pairInfoAttrNamesObj.GetName() ) ;
        if( groupIter != scenarioDetailsObject->m_Groups.end() )
        {
            ConfSchema::Group& pairInfoGrp = groupIter->second ;
            ConfSchema::Object& pairInfoObj = pairInfoGrp.m_Objects.begin()->second ;
            groupIter = pairInfoObj.m_Groups.find( m_pairAttrNamesObj.GetName() ) ;
            if( groupIter != pairInfoObj.m_Groups.end() )
            {
                ConfSchema::Group& pairsGrp = groupIter->second ;
                if( volumeName.length() > 0 )
                {   
                    ConfSchema::ObjectsIter_t pairObjIter = find_if( pairsGrp.m_Objects.begin(),
                        pairsGrp.m_Objects.end(),
                        ConfSchema::EqAttr(m_pairAttrNamesObj.GetSourceVolumeAttrName(),
                        volumeName.c_str())) ;
                    if( pairObjIter != pairsGrp.m_Objects.end() )
                    {
                        bRet = true ;
                        *pairObject = &(pairObjIter->second) ;
                    }
                }
                else
                {
                    if( pairsGrp.m_Objects.empty() == false ) 
                    {
                        bRet = true ;
                        *pairObject = & (pairsGrp.m_Objects.begin()->second) ;
                    }
                }
            }
        }
    }
    return bRet ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

bool ScenarioConfig::IsAlreadyProtected(const std::string& volumeName)
{
    bool bRet = false ;
    ConfSchema::Object* pairObj ;
    if( GetPairObjectBySrcVolume(volumeName, &pairObj) )
    {
        bRet = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;

}

bool ScenarioConfig::GetProtectedVolumes(std::list<std::string>& volumes)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTER %s\n", FUNCTION_NAME) ;
    ConfSchema::Object* scenarioDetailsObject = NULL ;
    bool bRet = false ;
    if( GetScenarioDetailsObject(&scenarioDetailsObject) )
    {
        ConfSchema::ConstAttrGrpIter_t  attrGrpIter ;
        attrGrpIter = scenarioDetailsObject->m_ObjAttrbGrps.m_AttrbGroups.find( "BackupRepositories" ) ;
        if( attrGrpIter != scenarioDetailsObject->m_ObjAttrbGrps.m_AttrbGroups.end() )
        {
            attrGrpIter = attrGrpIter->second.m_AttrbGroups.begin() ;
            ConfSchema::ConstAttrsIter_t attrIter = attrGrpIter->second.m_Attrs.begin() ;
            bRet = true ;
            while( attrIter != attrGrpIter->second.m_Attrs.end() )
            {
                volumes.push_back(attrIter->second) ;
                attrIter++ ;
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ScenarioConfig::GetExcludedVolumes(std::list<std::string>& volumes)
{
    bool bRet = false ;
    ConfSchema::Object* scenarioDetailsObject = NULL ;
    if( GetScenarioDetailsObject(&scenarioDetailsObject) )
    {
        ConfSchema::ConstAttrGrpIter_t  attrGrpIter ;
        attrGrpIter = scenarioDetailsObject->m_ObjAttrbGrps.m_AttrbGroups.find( "excludedVolumes" ) ;
        if( attrGrpIter != scenarioDetailsObject->m_ObjAttrbGrps.m_AttrbGroups.end() )
        {
            bRet = true ;
            ConfSchema::ConstAttrsIter_t attrIter = attrGrpIter ->second.m_Attrs.begin() ;
            while( attrIter != attrGrpIter->second.m_Attrs.end() )
            {
                volumes.push_back(attrIter->second) ;
                attrIter++ ;
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ScenarioConfig::GetRetentionPolicy(ConfSchema::Object* pairObject, RetentionPolicy& retentionPolicy)
{
    bool bRet = false ;
    ConfSchema::GroupsIter_t groupIter ;
    if( !pairObject )
    {
        throw "Invalid Pair Object" ;
    }
    groupIter = pairObject->m_Groups.find( m_scenarioRetentionPolicyAttrNamesObj.GetName() ) ;
    if( groupIter->second.m_Objects.size() )
    {
        bRet = true ;
        ConfSchema::Object& retentionPolicyObj = groupIter->second.m_Objects.begin()->second;
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = retentionPolicyObj.m_Attrs.find( m_scenarioRetentionPolicyAttrNamesObj.GetDurationAttrName() );
        retentionPolicy.duration = attrIter->second ;
        attrIter = retentionPolicyObj.m_Attrs.find( m_scenarioRetentionPolicyAttrNamesObj.GetRetentionTypeAttrName() ) ;
        retentionPolicy.retentionType = attrIter->second ;
        attrIter = retentionPolicyObj.m_Attrs.find( m_scenarioRetentionPolicyAttrNamesObj.GetSizeAttrName() ) ;
        retentionPolicy.size = attrIter->second ;
        groupIter = retentionPolicyObj.m_Groups.find( m_scenarioretwindowAttrNamesObj.GetName() ) ;
        if( groupIter != retentionPolicyObj.m_Groups.end() )
        {
            ConfSchema::ObjectsIter_t retentionWindowObjIter ; 
            retentionWindowObjIter = groupIter->second.m_Objects.begin() ;
            while( retentionWindowObjIter != groupIter->second.m_Objects.end() )
            {   
                ScenarioRetentionWindow retentionWindow ;
                ConfSchema::Attrs_t& retentionWindowAttrs = retentionWindowObjIter->second.m_Attrs ;
                attrIter = retentionWindowAttrs.find( m_scenarioretwindowAttrNamesObj.GetCountAttrName()) ;
                retentionWindow.count = boost::lexical_cast<int>(attrIter->second) ;
                attrIter = retentionWindowAttrs.find( m_scenarioretwindowAttrNamesObj.GetdurationAttrName()) ;
                retentionWindow.duration = attrIter->second ;
                attrIter = retentionWindowAttrs.find( m_scenarioretwindowAttrNamesObj.GetGranualarityAttrName()) ;
                retentionWindow.granularity = attrIter->second ;
                attrIter = retentionWindowAttrs.find( m_scenarioretwindowAttrNamesObj.GetRetentionWindowIdAttrName()) ;
                retentionWindow.retentionWindowId = attrIter->second ;
                retentionPolicy.retentionWindows.push_back( retentionWindow ) ;
                retentionWindowObjIter++ ;
            }
        }
    }
    return bRet ;
}

bool ScenarioConfig::GetRetentionPolicy(const std::string& volumeName, RetentionPolicy& retentionPolicy)
{
    bool bRet = false ;
    ConfSchema::Object* pairObj ;
    if( GetPairObjectBySrcVolume(volumeName, &pairObj) )
    {
        ConfSchema::GroupsIter_t groupIter ;
        groupIter = pairObj->m_Groups.find( m_scenarioRetentionPolicyAttrNamesObj.GetName() ) ;
        if( groupIter->second.m_Objects.size() )
        {
            bRet = true ;
            ConfSchema::Object& retentionPolicyObj = groupIter->second.m_Objects.begin()->second;
            ConfSchema::AttrsIter_t attrIter ;
            attrIter = retentionPolicyObj.m_Attrs.find( m_scenarioRetentionPolicyAttrNamesObj.GetDurationAttrName() );
            retentionPolicy.duration = attrIter->second ;
            attrIter = retentionPolicyObj.m_Attrs.find( m_scenarioRetentionPolicyAttrNamesObj.GetRetentionTypeAttrName() ) ;
            retentionPolicy.retentionType = attrIter->second ;
            attrIter = retentionPolicyObj.m_Attrs.find( m_scenarioRetentionPolicyAttrNamesObj.GetSizeAttrName() ) ;
            retentionPolicy.size = attrIter->second ;
            groupIter = retentionPolicyObj.m_Groups.find( m_scenarioretwindowAttrNamesObj.GetName() ) ;
            if( groupIter != retentionPolicyObj.m_Groups.end() )
            {
                ConfSchema::ObjectsIter_t retentionWindowObjIter ; 
                retentionWindowObjIter = groupIter->second.m_Objects.begin() ;
                while( retentionWindowObjIter != groupIter->second.m_Objects.end() )
                {   
                    ScenarioRetentionWindow retentionWindow ;
                    ConfSchema::Attrs_t& retentionWindowAttrs = retentionWindowObjIter->second.m_Attrs ;
                    attrIter = retentionWindowAttrs.find( m_scenarioretwindowAttrNamesObj.GetCountAttrName()) ;
                    retentionWindow.count = boost::lexical_cast<int>(attrIter->second) ;
                    attrIter = retentionWindowAttrs.find( m_scenarioretwindowAttrNamesObj.GetdurationAttrName()) ;
                    retentionWindow.duration = attrIter->second ;
                    attrIter = retentionWindowAttrs.find( m_scenarioretwindowAttrNamesObj.GetGranualarityAttrName()) ;
                    retentionWindow.granularity = attrIter->second ;
                    attrIter = retentionWindowAttrs.find( m_scenarioretwindowAttrNamesObj.GetRetentionWindowIdAttrName()) ;
                    retentionWindow.retentionWindowId = attrIter->second ;
                    retentionPolicy.retentionWindows.push_back( retentionWindow ) ;
                    retentionWindowObjIter++ ;
                }
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ScenarioConfig::UpdateVolumeProvisioningFinalStatus(const std::string& targetName,
														const std::string& mountPoint)
{
    bool bRet = false ;
    ConfSchema::Object* pairObject ;
    if( GetPairObjectByTgtVolume(targetName, &pairObject) )
    {
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = pairObject->m_Attrs.find(m_pairAttrNamesObj.GetTargetVolumeAttrName()) ;
        attrIter->second = mountPoint ;
        attrIter = pairObject->m_Attrs.find(m_pairAttrNamesObj.GetVolumeProvisioningStatusAttrName()) ;
        attrIter->second = VOL_PACK_SUCCESS ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ScenarioConfig::GetTargetVolume(const std::string& sourceVolume,
										std::string& targetName)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* pairObject ;
    if( GetPairObjectBySrcVolume(sourceVolume, &pairObject) )
    {
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = pairObject->m_Attrs.find(m_pairAttrNamesObj.GetTargetVolumeAttrName()) ;
		targetName = attrIter->second ; 
		DebugPrintf (SV_LOG_DEBUG, " target Name is %s \n",targetName.c_str() ); 
		bRet = true; 
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
bool ScenarioConfig::UpdateVolumeProvisioningFailureStatus(const std::string& srcVolName )
{
    bool bRet = false ;
    ConfSchema::Object* pairObject ;
    if( GetPairObjectBySrcVolume(srcVolName, &pairObject) )
    {
        ConfSchema::AttrsIter_t attrIter ;
		attrIter = pairObject->m_Attrs.find(m_pairAttrNamesObj.GetVolumeProvisioningStatusAttrName()) ;
        attrIter->second = VOL_PACK_FAILED ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ScenarioConfig::SetTargetVolumeUnProvisioningStatus(std::list <std::string> &sourceList )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    std::list <std::string>::iterator sourceListIter = sourceList.begin ();
    while (sourceListIter != sourceList.end())
    {
        ConfSchema::Object* pairObject ;
        std::string sourceString = *sourceListIter;
        if( GetPairObjectBySrcVolume(sourceString, &pairObject) )
        {
            ConfSchema::AttrsIter_t attrIter ;
            attrIter = pairObject->m_Attrs.find(m_pairAttrNamesObj.GetSourceVolumeAttrName()) ;
            if (attrIter->second == sourceString ) 
            {
                attrIter = pairObject->m_Attrs.find(m_pairAttrNamesObj.GetVolumeProvisioningStatusAttrName()) ;
                attrIter->second = VOL_PACK_DELETION_INPROGRESS ;
                bRet = true; 
            }
        }
        sourceListIter ++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

int ScenarioConfig::GetProtectionScenarioId()
{
    bool bRet = false ;
    int scenarioId ;
    ConfSchema::Object* scenarioObject = NULL ;
    if( GetProtectionScenarioObject(&scenarioObject) )
    {
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = scenarioObject->m_Attrs.find( m_scenarioAttrNamesObj.GetScenarioIdAttrName() ) ;
        scenarioId = boost::lexical_cast<int>(attrIter->second) ;

    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return scenarioId ;
}
bool ScenarioConfig::GetPlanObject(ConfSchema::Object** planObj)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    if( m_PlanGrp->m_Objects.size() )
    {
        *planObj = &(m_PlanGrp->m_Objects.begin()->second) ;
        bRet = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}


bool ScenarioConfig::GetPlanName(std::string& planName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* planObject = NULL ;
    if( GetPlanObject(&planObject) )
    {
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = planObject->m_Attrs.find( m_planAttrNamesObj.GetPlanNameAttrName() ) ;
        planName = attrIter->second ;
        bRet = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}


bool ScenarioConfig::GetPlanId(std::string& planId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* planObject = NULL ;
    if( GetPlanObject(&planObject) )
    {
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = planObject->m_Attrs.find( m_planAttrNamesObj.GetPlandIdAttrName() ) ;
        planId = attrIter->second ;
        bRet = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ScenarioConfig::GetPairInfoObj(ConfSchema::Object** pairInfoObj)
{
    bool bRet = false ;
    *pairInfoObj = NULL ;
    ConfSchema::Object* scenarioDetailsObj ;
    if( GetScenarioDetailsObject(&scenarioDetailsObj) )
    {
        ConfSchema::Group& pairInfoGrp = scenarioDetailsObj->m_Groups.find( m_pairInfoAttrNamesObj.GetName() )->second ;
        if( pairInfoGrp.m_Objects.size() )
        {
            *pairInfoObj = &(pairInfoGrp.m_Objects.begin()->second) ;
            bRet = true ;
        }
    }
    return bRet ;
}


bool ScenarioConfig::GetReplicationOptionsObj(ConfSchema::Object** replicationOptionsObj)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* pairInfoObj ;

    if( GetPairInfoObj(&pairInfoObj) )
    {
        ConfSchema::Group& replicationOptionsGrp = pairInfoObj->m_Groups.find( m_replicationOptionsAttrNamesObj.GetName() )->second ;
        if( replicationOptionsGrp.m_Objects.size() )
        {
            *replicationOptionsObj = &(replicationOptionsGrp.m_Objects.begin()->second) ;
            bRet = true ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SV_UINT ScenarioConfig::GetBatchSize()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SV_UINT batchsize = 0 ;
    ConfSchema::Object* replicationOptionsObj ;
    if( GetReplicationOptionsObj( &replicationOptionsObj ) )
    {
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = replicationOptionsObj->m_Attrs.find( m_replicationOptionsAttrNamesObj.GetBatchResyncAttrName() ) ;
        batchsize = boost::lexical_cast<SV_UINT>(attrIter->second) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return batchsize ;
}
void ScenarioConfig::SetBatchSize(SV_UINT batchsize )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::Object* replicationOptionsObj ;
    if( GetReplicationOptionsObj( &replicationOptionsObj ) )
    {
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = replicationOptionsObj->m_Attrs.find( m_replicationOptionsAttrNamesObj.GetBatchResyncAttrName() ) ;
		attrIter->second = boost::lexical_cast<std::string>(batchsize) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

bool ScenarioConfig::GetPairDetails(const std::string& srcVolName, PairDetails& pairDetails)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* pairObject ;
    if( GetPairObjectBySrcVolume( srcVolName, &pairObject) && 
        GetRetentionPolicy(pairObject, pairDetails.retPolicy) )
    {
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = pairObject->m_Attrs.find( m_pairAttrNamesObj.GetSourceVolumeAttrName()) ;
        pairDetails.srcVolumeName = attrIter->second ;
        attrIter = pairObject->m_Attrs.find( m_pairAttrNamesObj.GetRetentionPathAttrName()) ;
        pairDetails.retentionPath = attrIter->second ;
        attrIter = pairObject->m_Attrs.find( m_pairAttrNamesObj.GetRetentionVolumeAttrName()) ;
        pairDetails.retentionVolume = attrIter ->second ;
        attrIter = pairObject->m_Attrs.find( m_pairAttrNamesObj.GetRpoThresholdAttrName()) ;
        pairDetails.rpoThreshold = boost::lexical_cast<int>(attrIter->second) ;
        attrIter = pairObject->m_Attrs.find( m_pairAttrNamesObj.GetTargetVolumeAttrName()) ;
        pairDetails.targetVolume = attrIter->second ;
        attrIter = pairObject->m_Attrs.find( m_pairAttrNamesObj.GetSrcCapacityAttrName() ); 
        pairDetails.srcVolCapacity = boost::lexical_cast<SV_LONGLONG>(attrIter->second) ;
        attrIter = pairObject->m_Attrs.find( m_pairAttrNamesObj.GetSrcRawSizeAttrName()) ;
        pairDetails.srcVolRawSize = boost::lexical_cast<SV_LONGLONG>(attrIter->second) ;
        attrIter = pairObject->m_Attrs.find( m_pairAttrNamesObj.GetVolumeProvisioningStatusAttrName() ) ;
        pairDetails.volumeProvisioningStatus = attrIter->second ;
        bRet = true ;
    }
    return bRet ;
}

bool ScenarioConfig::GetPairDetails(std::map<std::string, PairDetails>& pairDetailsMap)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    std::list<std::string> protectedVolumes ;
    if( GetProtectedVolumes(protectedVolumes) )
    {
        std::list<std::string>::const_iterator protectedVolIter = protectedVolumes.begin() ;
        while( protectedVolIter != protectedVolumes.end() )
        {
            PairDetails pairDetails ;
            if( GetPairDetails(*protectedVolIter, pairDetails) )
            {
                bRet = true ;
                pairDetailsMap.insert(std::make_pair( *protectedVolIter, pairDetails) ) ;
            }
            else
            {
                bRet = false ;
                DebugPrintf(SV_LOG_ERROR, "Failed to get the pair details for %s from scenario\n",protectedVolIter->c_str()) ;
                break ;
            }
            protectedVolIter++ ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

/*
TODO :not complete yet
*/
bool ScenarioConfig::CanTriggerAutoResync()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* replicationOptionsObj ;
    if( GetReplicationOptionsObj(&replicationOptionsObj) )
    {
        ConfSchema::GroupsIter_t autoresyncGrpIter ;
        autoresyncGrpIter = replicationOptionsObj->m_Groups.find( m_autoresyncOptionAttrNamesObj.GetName() ) ;
        if( autoresyncGrpIter != replicationOptionsObj->m_Groups.end() )
        {
            ConfSchema::Group& autoResyncGrp = autoresyncGrpIter->second ;
            if( autoResyncGrp.m_Objects.size() )
            {
                ConfSchema::Object& autoResyncObj = autoResyncGrp.m_Objects.begin()->second ;
                ConfSchema::AttrsIter_t attrIter ;
                attrIter = autoResyncObj.m_Attrs.find( m_autoresyncOptionAttrNamesObj.GetBetweenAttrName()) ;
                if( attrIter != autoResyncObj.m_Attrs.end() )
                {
                    time_t startTime, endTime ;
                    GetExcludeFromAndTo( attrIter->second, startTime, endTime ) ;
                    if( startTime != endTime ) 
                    {
                        time_t currentTime ;
                        time( &currentTime ) ;
                        time_t rawtime;
                        time ( &rawtime );
                        struct tm * timeinfo;
                        timeinfo = localtime ( &rawtime );
                        timeinfo->tm_min = 0;
                        timeinfo->tm_hour = 0;
                        timeinfo->tm_sec = 0;
                        startTime += mktime( timeinfo ) ;
                        endTime +=  mktime( timeinfo ) ;
                        if( currentTime >= startTime && currentTime <= endTime )
                            bRet = true ;
                    }
                }                    
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

void ScenarioConfig::GetTargetVolumeProvisioningFailedVolumes(std::list <std::string> &list, std::list <std::string> &tagetprovisionFailedVolumes)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<std::string>::iterator listIter = list.begin();
    std::map<std::string, PairDetails> pairDetailsMap ;
    if( GetPairDetails(pairDetailsMap) )
    {
        std::map<std::string, PairDetails>::const_iterator pairDetailIter ;
        while ( listIter  != list.end () )
        {
            pairDetailIter = pairDetailsMap.begin() ;
            while( pairDetailIter != pairDetailsMap.end())
            {
                if (pairDetailIter->second.srcVolumeName.compare(*listIter) == 0)
                {
                    std::string sourceDevice  = *listIter;
                    DebugPrintf ( SV_LOG_DEBUG , "pairDetailIter->second.srcVolumeName is %s \n",pairDetailIter->second.srcVolumeName.c_str()) ; 
                    DebugPrintf ( SV_LOG_DEBUG , "source Device is %s \n",sourceDevice.c_str() ); 
                    bool bAdd = true ;
					std::string volumeProvisioningStatus = pairDetailIter->second.volumeProvisioningStatus ; 
                    if( volumeProvisioningStatus == "Target Volume Provisioning Failed" )
                    {
						tagetprovisionFailedVolumes.push_back(sourceDevice);                     
                    }
                }
                pairDetailIter++ ; 
            }
            listIter ++ ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ScenarioConfig::GetSourceTargetVoumeMapping(std::string &sourceVolume , std::map<std::string, std::string>& srcTgtVolMap)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::map<std::string, PairDetails> pairDetailsMap ;
	if( GetPairDetails(pairDetailsMap) )
	{
		std::map<std::string, PairDetails>::const_iterator pairDetailIter ;
		pairDetailIter = pairDetailsMap.begin() ;
		while( pairDetailIter != pairDetailsMap.end())
		{
			if (pairDetailIter->second.srcVolumeName.compare(sourceVolume) == 0)
			{
				DebugPrintf ( SV_LOG_DEBUG , "pairDetailIter->second.srcVolumeName is %s \n",pairDetailIter->second.srcVolumeName.c_str()) ; 
				DebugPrintf ( SV_LOG_DEBUG , "source Volume is %s \n",sourceVolume.c_str() ); 
				bool bAdd = true ;
				std::string volumeProvisioningStatus = pairDetailIter->second.volumeProvisioningStatus ; 
				if( volumeProvisioningStatus == "Target Volume Provisioning Successful" )
				{
					bAdd = false ;                    
				}
				if( bAdd )
					srcTgtVolMap.insert(std::make_pair( pairDetailIter->second.srcVolumeName,
					pairDetailIter->second.targetVolume)) ;
			}
			pairDetailIter++ ; 
		}
	}
	else
	{
		throw "Cannot get the Mapping of source and target volumes" ;
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ScenarioConfig::GetSourceTargetVoumeMapping(std::list <std::string> &list , std::map<std::string, std::string>& srcTgtVolMap, bool returnProvisionedAlone )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<std::string>::iterator listIter = list.begin();
    std::map<std::string, PairDetails> pairDetailsMap ;
	if (list.size() > 0 )
	{
		if( GetPairDetails(pairDetailsMap) )
		{
			std::map<std::string, PairDetails>::const_iterator pairDetailIter ;
			while ( listIter  != list.end () )
			{
				pairDetailIter = pairDetailsMap.begin() ;
				while( pairDetailIter != pairDetailsMap.end())
				{
					if (pairDetailIter->second.srcVolumeName.compare(*listIter) == 0)
					{
						std::string sourceDevice  = *listIter;
						DebugPrintf ( SV_LOG_DEBUG , "pairDetailIter->second.srcVolumeName is %s \n",pairDetailIter->second.srcVolumeName.c_str()) ; 
						DebugPrintf ( SV_LOG_DEBUG , "source Device is %s \n",sourceDevice.c_str() ); 
						bool bAdd = true ;
						std::string volumeProvisioningStatus = pairDetailIter->second.volumeProvisioningStatus ; 
						if( returnProvisionedAlone && !(volumeProvisioningStatus == "Target Volume Provisioning Successful" || volumeProvisioningStatus == "Target Volume Provisioning Failed" ) )
						{
							bAdd = false ;                    
						}
						if( bAdd )
							srcTgtVolMap.insert(std::make_pair( pairDetailIter->second.srcVolumeName,
							pairDetailIter->second.targetVolume)) ;
					}
					pairDetailIter++ ; 
				}
				listIter ++ ;
			}
		}
		else
		{
			throw "Cannot get the Mapping of source and target volumes" ;
		}
	}
	else 
	{
		DebugPrintf (SV_LOG_DEBUG, "List size is %d \n", list.size()); 
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

bool ScenarioConfig::removePairs(std::string &volumeName )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool bRet = false;
    int count = 0;
    ConfSchema::Object_t* pairObjects ;
    if( GetPairsObjects( &pairObjects )  )
    {
		ConfSchema::ObjectsIter_t pairObjIter = find_if( pairObjects->begin(),
												pairObjects->end(),
												ConfSchema::EqAttr(m_pairAttrNamesObj.GetSourceVolumeAttrName(),
												volumeName.c_str())) ;
		if (pairObjIter != pairObjects->end())
        {
            pairObjects->erase(pairObjIter);
			bRet = true;
        }
				
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bRet; 
}


bool ScenarioConfig::removePairs(std::list <std::string> &list )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool bRet = false;
    int count = 0;
    ConfSchema::Object_t* pairObjects ;
    std::list <std::string>::iterator listIter = list.begin();
    if( GetPairsObjects( &pairObjects )  )
    {
        while (listIter  != list.end() )
        {
			std::string volumeName = *listIter;
			ConfSchema::ObjectsIter_t pairObjIter = find_if( pairObjects->begin(),
													pairObjects->end(),
													ConfSchema::EqAttr(m_pairAttrNamesObj.GetSourceVolumeAttrName(),
													volumeName.c_str())) ;
			if (pairObjIter != pairObjects->end())
                {
                    pairObjects->erase(pairObjIter);
                    count ++;
                }
				
            listIter++; 
        }
    }
    if (count = list.size () )
        bRet = true;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bRet; 
}


void ScenarioConfig::ClearPlan()
{
    m_PlanGrp->m_Objects.clear() ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;

}

bool ScenarioConfig::GetConsistencyObject(ConfSchema::Object** consistencyObj)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::Object* scenarioDetailsObj ;
    bool bRet = false ;
    if( GetScenarioDetailsObject( &scenarioDetailsObj) )
    {
        ConfSchema::GroupsIter_t grpIter ;
        grpIter = scenarioDetailsObj->m_Groups.find( m_consistencyPolicyAttrNamesObj.GetName() ) ;
        if( grpIter != scenarioDetailsObj->m_Groups.end() )
        {
            ConfSchema::Group& consistencyGrp = grpIter ->second ;
            if( consistencyGrp.m_Objects.size() )
            {
                *consistencyObj = &(consistencyGrp.m_Objects.begin()->second) ;
                bRet = true ;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "No Object available in %s Group\n",
                    m_consistencyPolicyAttrNamesObj.GetName()) ;
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Unable to find the group %s\n", 
                m_consistencyPolicyAttrNamesObj.GetName()) ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ScenarioConfig::GetConsistencyOptions(ConsistencyOptions& consistencyOptions)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* consistencyObject ;
    GetConsistencyObject(&consistencyObject) ;
    ConfSchema::AttrsIter_t attrIter ;
    attrIter = consistencyObject->m_Attrs.find( m_consistencyPolicyAttrNamesObj.GetExceptionAttrName()) ;
    consistencyOptions.exception = attrIter->second ;
    attrIter = consistencyObject->m_Attrs.find( m_consistencyPolicyAttrNamesObj.GetIntervalAttrName() ) ;
    consistencyOptions.interval = attrIter->second ;
    attrIter = consistencyObject->m_Attrs.find( m_consistencyPolicyAttrNamesObj.GetTagNameAttrName() );
    consistencyOptions.tagName = attrIter->second ;
    attrIter = consistencyObject->m_Attrs.find( m_consistencyPolicyAttrNamesObj.GetTagTypeAttrName()) ;
    consistencyOptions.tagType = attrIter->second ;
    bRet = true ;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ScenarioConfig::GetReplicationOptions(ReplicationOptions& replicationOptions)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* replicationOptionsObject ;
    GetReplicationOptionsObj(&replicationOptionsObject) ;
    ConfSchema::AttrsIter_t attrIter ;
    attrIter = replicationOptionsObject->m_Attrs.find( m_replicationOptionsAttrNamesObj.GetBatchResyncAttrName() ) ;
    replicationOptions.batchSize = boost::lexical_cast<int>(attrIter->second) ;
    ConfSchema::GroupsIter_t grpIter ;
    grpIter = replicationOptionsObject->m_Groups.find( m_autoresyncOptionAttrNamesObj.GetName() ) ;

    if( grpIter !=  replicationOptionsObject->m_Groups.end() )
    {
        ConfSchema::Group& autoResyncGrp = grpIter->second ;
        if( autoResyncGrp.m_Objects.size() )
        {
            ConfSchema::Object& autoresyncOptions = autoResyncGrp.m_Objects.begin()->second ;
            attrIter = autoresyncOptions.m_Attrs.find( m_autoresyncOptionAttrNamesObj.GetBetweenAttrName() ) ;
            replicationOptions.autoResyncOptions.between = attrIter->second ;
            attrIter = autoresyncOptions.m_Attrs.find( m_autoresyncOptionAttrNamesObj.GetWaitTimeAttrName() ) ;
            replicationOptions.autoResyncOptions.waitTime = boost::lexical_cast<int>(attrIter ->second) ;
            bRet = true ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ScenarioConfig::PopulateRecoveryFeaturePolicy( const std::string& recoveryFeaturePolicy ) 
{
    bool bRet = false ;
    ConfSchema::Object* scenarioDetailsObj ;
    if( GetScenarioDetailsObject(&scenarioDetailsObj) )
    {
        bRet = true ;
        scenarioDetailsObj->m_Attrs.erase( m_scenarioDetailsAttrNamesObj.GetRecoveryFeatrureAttrName() ) ;
        scenarioDetailsObj->m_Attrs.insert( std::make_pair( m_scenarioDetailsAttrNamesObj.GetRecoveryFeatrureAttrName(), recoveryFeaturePolicy ) ) ;
        AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
        auditConfigPtr->LogAudit("Setting the recovery feature policy as " + recoveryFeaturePolicy ) ;
    }
    return bRet ;
}

bool ScenarioConfig::GetRecoveryFeaturePolicy(std::string& recoveryFeaturePolicy)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* scenarioDetailsObj ;
    if( GetScenarioDetailsObject(&scenarioDetailsObj) )
    {
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = scenarioDetailsObj->m_Attrs.find( m_scenarioDetailsAttrNamesObj.GetRecoveryFeatrureAttrName() ) ;
        recoveryFeaturePolicy = attrIter ->second ;
        bRet = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ScenarioConfig::GetActionOnNewVolumeDiscovery(std::string& action)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* scenarioDetailsObj ;
    if( GetScenarioDetailsObject(&scenarioDetailsObj) )
    {
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = scenarioDetailsObj->m_Attrs.find( m_scenarioDetailsAttrNamesObj.GetActionOnNewVolumeDiscoveryAttrName() ) ;
        action = attrIter ->second ;
        bRet = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ScenarioConfig::PopulateRPOThreshold( std::string& rpoThreshold ) 
{
    bool bRet = false ;
    ConfSchema::Object* scenarioDetailsObj ;
    if( GetScenarioDetailsObject(&scenarioDetailsObj) )
    {
        std::map<std::string, PairDetails> pairDetailsMap ;

        GetPairDetails(   pairDetailsMap   )  ;

    }
    return bRet ;
}

bool ScenarioConfig::GetRpoThreshold(SV_UINT& rpoThreshold)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::map<std::string, PairDetails> pairDetailsMap ;
    bool bRet = false ;
    if( GetPairDetails(   pairDetailsMap   ) )
    {
        if( pairDetailsMap.size() ) 
        {
            bRet = true ;
            PairDetails& pairDetails = pairDetailsMap.begin()->second ;
            rpoThreshold = pairDetails.rpoThreshold ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ScenarioConfig::GetRetentionPolicy(RetentionPolicy& retPolicy)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::map<std::string, PairDetails> pairDetailsMap ;
    bool bRet = false ;
    if( GetPairDetails(   pairDetailsMap   ) )
    {
        if( pairDetailsMap.size() ) 
        {
            bRet = true ;
            PairDetails& pairDetails = pairDetailsMap.begin()->second ;
            retPolicy = pairDetails.retPolicy ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
bool ScenarioConfig::GetExcludedVolsAttrGrp(ConfSchema::Object& scenarioDetailsObj,
                                            ConfSchema::AttributeGroup** attrGrp)
{
    bool bRet = false ;
    ConfSchema::AttrGrpIter_t attrGrpIter = scenarioDetailsObj.m_ObjAttrbGrps.m_AttrbGroups.find( "excludedVolumes") ;
    if( attrGrpIter != scenarioDetailsObj.m_ObjAttrbGrps.m_AttrbGroups.end() )
    {
        *attrGrp = &(attrGrpIter->second ) ;
        bRet = true ;
    }
    return bRet ;
}

bool ScenarioConfig::GetProtectedVolsAttrGrp(ConfSchema::Object& scenarioDetailsObj,
                                             ConfSchema::AttributeGroup** attrGrp)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::AttrGrpIter_t attrGrpIter = scenarioDetailsObj.m_ObjAttrbGrps.m_AttrbGroups.find( "BackupRepositories") ;
    if( attrGrpIter != scenarioDetailsObj.m_ObjAttrbGrps.m_AttrbGroups.end() )
    {
        if( attrGrpIter->second.m_AttrbGroups.size() )
        {
            bRet = true ;
            *attrGrp = &(attrGrpIter->second.m_AttrbGroups.begin()->second) ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME); 
    return bRet ;
}

void ScenarioConfig::RemoveVolumeFromExclusionList(const std::string& volName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::Object* scenarioDetailsObj ;
    if( GetScenarioDetailsObject(&scenarioDetailsObj) )
    {
        ConfSchema::AttributeGroup* attrGrp ;
        if( GetExcludedVolsAttrGrp(*scenarioDetailsObj, &attrGrp) )
        {
            ConfSchema::ConstAttrsIter_t attrIter = attrGrp->m_Attrs.begin() ;
            while( attrIter != attrGrp->m_Attrs.end() )
            {
                if( attrIter->second.compare(volName) == 0 )
                {
                    attrGrp->m_Attrs.erase( attrIter->first ) ;
                    break ;
                }
                attrIter ++ ;
            }

        }

    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME); 

}
void ScenarioConfig::AddVolumeToProtectedVolumesList(const std::string& volName)
{
    ConfSchema::Object* scenarioDetailsObj ;
    if( GetScenarioDetailsObject(&scenarioDetailsObj) )
    {
        ConfSchema::AttributeGroup* attrGrp ;
        if( GetProtectedVolsAttrGrp(*scenarioDetailsObj, &attrGrp) )
        {
            ConfSchema::ConstAttrsIter_t attrIter = attrGrp->m_Attrs.begin() ;

            attrGrp->m_Attrs.insert(std::make_pair(volName, volName)) ;
            AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
            auditConfigPtr->LogAudit( "Starting protection for " + volName ) ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME); 

}

bool ScenarioConfig::AddPairs(const std::map<std::string, PairDetails> pairDetailsMap)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* pairInfoObject ;
    if( GetPairInfoObj(&pairInfoObject) )
    {
        ConfSchema::GroupsIter_t grpIter = pairInfoObject->m_Groups.find( m_pairAttrNamesObj.GetName() ) ;
        if( grpIter != pairInfoObject->m_Groups.end() )
        {
            ConfSchema::Group& pairsGrp = grpIter->second ;
            AddPairs( pairsGrp, pairDetailsMap)  ;
            bRet = true ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

void ScenarioConfig::ChangePaths( const std::string& existingRepo, const std::string& newRepo ) 
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::Object_t* pairObjects ;
    if( GetPairsObjects( &pairObjects )  )
    {
        ConfSchema::ObjectsIter_t pairObjIter = pairObjects->begin() ;
        while( pairObjIter != pairObjects->end() )
        {
            ConfSchema::Object& pairObject = pairObjIter->second ;
            ConfSchema::AttrsIter_t attrIter = pairObject.m_Attrs.find( m_pairAttrNamesObj.GetRetentionPathAttrName() ) ;
            std::string currentretPath = attrIter->second ;
            std::string newRetepath = newRepo ;
            newRetepath += currentretPath.substr( existingRepo.length() ) ;
            attrIter->second = newRetepath ;
            /*attrIter = pairObject.m_Attrs.find( m_pairAttrNamesObj.GetTargetVolumeAttrName() ) ;
            std::string targetPath = attrIter->second ;
            std::string newTargetPath = newRepo ;
            newTargetPath += targetPath.substr( existingRepo.length() ) ;
            attrIter->second = newTargetPath ;
            */
            attrIter = pairObject.m_Attrs.find( m_pairAttrNamesObj.GetRetentionVolumeAttrName() ) ;
            attrIter->second = newRepo ;
            pairObjIter++ ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
bool ScenarioConfig::removevolumesFromRepo (std::list <std::string> &list )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<std::string>::iterator listIter = list.begin() ; 
    int count = 0 ;
    bool bRet = false ;
    ConfSchema::Object* scenarioDetailsObj ;
    if( GetScenarioDetailsObject(&scenarioDetailsObj) )
    {
        ConfSchema::AttrGrpIter_t attrGrpIter = scenarioDetailsObj->m_ObjAttrbGrps.m_AttrbGroups.find( "BackupRepositories") ;
        if( attrGrpIter != scenarioDetailsObj->m_ObjAttrbGrps.m_AttrbGroups.end() )
        {
            attrGrpIter = attrGrpIter->second.m_AttrbGroups.begin() ;
            while (listIter != list.end() )
            {
                ConfSchema::AttrsIter_t attrIter = attrGrpIter->second.m_Attrs.begin() ;
                while ( attrIter != attrGrpIter->second.m_Attrs.end() )
                {	
                    if (attrIter->second == *listIter)
                    {
                        attrGrpIter->second.m_Attrs.erase(attrIter ++);
                        count ++ ;
                    }
                    else
                    {
                        ++ attrIter;
                    }
                }
                listIter ++ ;
            }
        }
    }
    if (count == list.size())
        bRet = true; 
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ScenarioConfig::removevolumeFromRepo (std::string &volumeName )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* scenarioDetailsObj ;
    if( GetScenarioDetailsObject(&scenarioDetailsObj) )
    {
        ConfSchema::AttrGrpIter_t attrGrpIter = scenarioDetailsObj->m_ObjAttrbGrps.m_AttrbGroups.find( "BackupRepositories") ;
        if( attrGrpIter != scenarioDetailsObj->m_ObjAttrbGrps.m_AttrbGroups.end() )
        {
            attrGrpIter = attrGrpIter->second.m_AttrbGroups.begin() ;
            ConfSchema::AttrsIter_t attrIter = attrGrpIter->second.m_Attrs.begin() ;
            while ( attrIter != attrGrpIter->second.m_Attrs.end() )
            {	
				if (attrIter->second == volumeName)
                {
                    attrGrpIter->second.m_Attrs.erase(attrIter ++);
					bRet = true;
					break ;
                }
                else
                {
                    ++ attrIter;
                }
            }
		}
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
bool ScenarioConfig::GetTargetVolumeUnProvisioningStatus(std::list <std::string> &sourceList )
{
    bool bRet = true ;
    std::list <std::string>::iterator sourceListIter = sourceList.begin ();
    if (sourceList.size () == 0 )
        bRet = false ; 
    while (sourceListIter != sourceList.end())
    {
        ConfSchema::Object* pairObject ;
        std::string sourceString = *sourceListIter;
        if( GetPairObjectBySrcVolume(sourceString, &pairObject) )
        {
            ConfSchema::AttrsIter_t attrIter ;
            attrIter = pairObject->m_Attrs.find(m_pairAttrNamesObj.GetSourceVolumeAttrName()) ;
            if (attrIter->second == sourceString ) 
            {
                attrIter = pairObject->m_Attrs.find(m_pairAttrNamesObj.GetVolumeProvisioningStatusAttrName()) ;
				if ( !( attrIter->second == VOL_PACK_SUCCESS || attrIter->second == VOL_PACK_FAILED ) ) 
                //if ( attrIter->second != VOL_PACK_SUCCESS ) 
                {
                    bRet = false; 
                    break; 
                }
            }
        }
        sourceListIter ++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
bool ScenarioConfig::SetSystemPausedState(std::string &pausedState)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false;
    ConfSchema::Object* scenarioObject = NULL ;
    if( GetProtectionScenarioObject(&scenarioObject) )
    {
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = scenarioObject->m_Attrs.find( m_scenarioAttrNamesObj.GetIsSystemPausedAttrName() ) ;
        if (attrIter != scenarioObject->m_Attrs.end () )
        {
            DebugPrintf(SV_LOG_DEBUG, "Updating the system paused state to 1 \n") ;
            attrIter->second = pausedState;
            bRet = true;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet;
}
void ScenarioConfig::SetSystemPausedReason(const std::string& pausereason)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::Object* scenarioObject = NULL ;
    if( GetProtectionScenarioObject(&scenarioObject) )
    {
        ConfSchema::AttrsIter_t attrIter ;
		scenarioObject->m_Attrs.insert(std::make_pair( m_scenarioAttrNamesObj.GetSystemPauseReasonAttrName(), "" )) ;
        attrIter = scenarioObject->m_Attrs.find( m_scenarioAttrNamesObj.GetSystemPauseReasonAttrName() ) ;
		attrIter->second = pausereason ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
std::string ScenarioConfig::GetSystemPausedReason()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::string pausereason ;
    ConfSchema::Object* scenarioObject = NULL ;
    if( GetProtectionScenarioObject(&scenarioObject) )
    {
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = scenarioObject->m_Attrs.find( m_scenarioAttrNamesObj.GetSystemPauseReasonAttrName() ) ;
		if( attrIter != scenarioObject->m_Attrs.end() )
		{
			pausereason = attrIter->second ;
		}
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return pausereason ;
}
bool ScenarioConfig::GetSystemPausedState(std::string& systemPausedStatus)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* scenarioObject = NULL ;
    if( GetProtectionScenarioObject(&scenarioObject) )
    {
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = scenarioObject->m_Attrs.find( m_scenarioAttrNamesObj.GetIsSystemPausedAttrName() ) ;
        if (attrIter != scenarioObject->m_Attrs.end() ) 
        {
            systemPausedStatus = attrIter->second ;
            bRet = true ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool GetSystemPausedState(int &systemPaused)
{	
    bool bRet = false;
    std::string systemPausedState;
    ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
    if( scenarioConfigPtr->ProtectionScenarioAvailable() )
    {
        if ( scenarioConfigPtr->GetSystemPausedState (systemPausedState) ) 
        {
            systemPaused = boost::lexical_cast <int> (systemPausedState);
            bRet = true;
        }
        else 
        {
            bRet = false;
        }
    }
    return bRet;
}

bool ScenarioConfig::GetTargetVolumeProvisioningStatus(std::list <std::string> &sourceList )
{
    bool bRet = false ;
    int count = 0;
    std::list <std::string>::iterator sourceListIter = sourceList.begin ();
    if (sourceList.size () == 0 )
        bRet = false ; 
    while (sourceListIter != sourceList.end())
    {
        ConfSchema::Object* pairObject ;
        std::string sourceString = *sourceListIter;
        if( GetPairObjectBySrcVolume(sourceString, &pairObject) )
        {
            ConfSchema::AttrsIter_t attrIter ;
            attrIter = pairObject->m_Attrs.find(m_pairAttrNamesObj.GetSourceVolumeAttrName()) ;
            if (attrIter->second == sourceString ) 
            {
                attrIter = pairObject->m_Attrs.find(m_pairAttrNamesObj.GetVolumeProvisioningStatusAttrName()) ;
                if ( attrIter->second == VOL_PACK_SUCCESS || attrIter->second == VOL_PACK_FAILED) 
                {
                    count ++;
                }
            }
        }
        sourceListIter ++ ;
    }
    if (sourceList.size() == count)
        bRet = true;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ScenarioConfig::GetVolumeProvisioningStatus(const std::string &sourceVolume , std::string &volumeProvisioningStatus)
{
    bool bRet = false ;
    ConfSchema::Object* pairObject ;
    if( GetPairObjectBySrcVolume(sourceVolume, &pairObject) )
    {
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = pairObject->m_Attrs.find(m_pairAttrNamesObj.GetVolumeProvisioningStatusAttrName()) ;
        volumeProvisioningStatus = attrIter->second;
        bRet = true;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}


void ScenarioConfig::ModifyRetentionBaseDir(const std::string& existRepoBase,
											const std::string& newRepoBase)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::string newRepoBaseDrive, newRepoBasePath ;
	std::string existingRepoBaseDrive, existingRepoBasePath ;
	newRepoBaseDrive = newRepoBase ;
	newRepoBasePath = newRepoBase ;
	if( newRepoBase.length() == 1 )
	{
		newRepoBasePath += ":\\" ;
	}
	existingRepoBaseDrive = existRepoBase ;
	existingRepoBasePath = existRepoBase ;
	if( existRepoBase.length() == 1 )
	{
		existingRepoBasePath += ":\\" ;
	}
	if( newRepoBasePath[newRepoBasePath.length() - 1 ] != '\\' )
	{
		newRepoBasePath += '\\' ;
	}
	if( existingRepoBasePath[existingRepoBasePath.length() - 1 ] != '\\' )
	{
		existingRepoBasePath += '\\' ;
	}
	ConfSchema::Object_t* pairObjects ;
    if( GetPairsObjects( &pairObjects )  )
	{
		ConfSchema::ObjectsIter_t pairObjIter = pairObjects->begin() ;
        while( pairObjIter != pairObjects->end() )
        {
			ConfSchema::AttrsIter_t attrIter ;
			attrIter = pairObjIter->second.m_Attrs.find( m_pairAttrNamesObj.GetRetentionVolumeAttrName() ) ;
			attrIter->second = newRepoBaseDrive ; 
			attrIter = pairObjIter->second.m_Attrs.find( m_pairAttrNamesObj.GetRetentionPathAttrName() ) ;
			attrIter->second = newRepoBasePath + attrIter->second.substr( existingRepoBasePath.length() ) ;
			pairObjIter++ ; 
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ScenarioConfig::SetExecutionStatustoLeastVolumeProvisioningStatus( )
{
	std::string executionStatus ; 
	GetExecutionStatus(executionStatus) ;
	DebugPrintf(SV_LOG_DEBUG, "ENTER %s\n", FUNCTION_NAME) ;
	std::list <std::string> backupRepositoriesVolumes;
	GetProtectedVolumes (backupRepositoriesVolumes);
	std::list <std::string>::iterator backupRepositoriesVolumesIter = backupRepositoriesVolumes.begin(); 

	std::string volumeProvisioningStatus ;
	while (backupRepositoriesVolumesIter != backupRepositoriesVolumes.end())
	{
		std::string volumeName = backupRepositoriesVolumesIter->c_str();
		GetVolumeProvisioningStatus (volumeName,volumeProvisioningStatus);	
		if (volumeProvisioningStatus == VOL_PACK_PENDING)
		{
			executionStatus = VOL_PACK_PENDING ;
			break ;
		}
		else if (volumeProvisioningStatus == VOL_PACK_INPROGRESS )
		{
			executionStatus = VOL_PACK_INPROGRESS; 
		}
		else if (volumeProvisioningStatus == VOL_PACK_SUCCESS && executionStatus != VOL_PACK_INPROGRESS )
		{
			executionStatus = VOL_PACK_SUCCESS; 
		}
		backupRepositoriesVolumesIter ++; 
	}
	SetExecutionStatus(executionStatus) ;
	DebugPrintf (SV_LOG_DEBUG, "executionStatus is %s \n ",executionStatus.c_str());
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return;
}

void ScenarioConfig::UpdateIOParameters(const spaceCheckParameters& spacecheckparams)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	ConfSchema::Object* protectionscenarioObj ;
	GetProtectionScenarioObject(&protectionscenarioObj) ;
	if( protectionscenarioObj->m_Groups.find( "IOParameters") != protectionscenarioObj->m_Groups.end() )
	{
		ConfSchema::Group& ioparamsGrp = protectionscenarioObj->m_Groups.find( "IOParameters")->second ;
		if( ioparamsGrp.m_Objects.size() )
		{
			ConfSchema::AttrsIter_t attrIter; 
			ConfSchema::Object& ioparamsObj = ioparamsGrp.m_Objects.begin()->second ;
			attrIter = ioparamsObj.m_Attrs.find("CompressionRatio") ;
			attrIter->second = boost::lexical_cast<std::string>(spacecheckparams.compressionBenifits) ;
			attrIter = ioparamsObj.m_Attrs.find("ChangeRate") ;
			attrIter->second = boost::lexical_cast<std::string>(spacecheckparams.changeRate) ;
		}
	}
	else
	{
		ConfSchema::Object ioparamsObj ;
		ioparamsObj.m_Attrs.insert(std::make_pair( "CompressionRatio", boost::lexical_cast<std::string>(spacecheckparams.compressionBenifits))) ;
		ioparamsObj.m_Attrs.insert(std::make_pair( "ChangeRate", boost::lexical_cast<std::string>(spacecheckparams.changeRate))) ;
		ConfSchema::Group ioparamsGrp ;
		ioparamsGrp.m_Objects.insert(std::make_pair("1", ioparamsObj)) ;
		protectionscenarioObj->m_Groups.insert( std::make_pair("IOParameters", ioparamsGrp)) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXIETD %s\n", FUNCTION_NAME) ;
}

void ScenarioConfig::GetIOParameters(spaceCheckParameters& spacecheckparams)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	ConfSchema::Object* protectionscenarioObj ;
	spacecheckparams.compressionBenifits = 0.2 ;
	spacecheckparams.changeRate = 0.05 ;
	if( GetProtectionScenarioObject(&protectionscenarioObj) )
	{
		if( protectionscenarioObj->m_Groups.find( "IOParameters") != protectionscenarioObj->m_Groups.end() )
		{
			ConfSchema::Group& ioparamsGrp = protectionscenarioObj->m_Groups.find( "IOParameters")->second ;
			if( ioparamsGrp.m_Objects.size() )
			{
				ConfSchema::AttrsIter_t attrIter; 
				ConfSchema::Object& ioparamsObj = ioparamsGrp.m_Objects.begin()->second ;
				attrIter = ioparamsObj.m_Attrs.find("CompressionRatio") ;
				spacecheckparams.compressionBenifits = boost::lexical_cast<double>(attrIter->second) ;
				attrIter = ioparamsObj.m_Attrs.find("ChangeRate") ;
				spacecheckparams.changeRate = boost::lexical_cast<double>(attrIter->second) ;
			}
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXIETD %s\n", FUNCTION_NAME) ;
}
