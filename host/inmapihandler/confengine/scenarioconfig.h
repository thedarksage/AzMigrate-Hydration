#ifndef SCENARIO_CONFIG_H
#define SCENARIO_CONFIG_H
#include <boost/shared_ptr.hpp>

#include "confschema.h"
#include "planobj.h"
#include "appscenarioobject.h"
#include "scenariodetails.h"
#include "consistencypolicyobj.h"
#include "pairinfoobj.h"
#include "pairsobj.h"
#include "scenarioretentionpolicyobj.h"
#include "scenarioretentionwindowobj.h"
#include "replicaionoptionsobj.h"
#include "autoresyncobj.h"
#include "svtypes.h"
#include "util.h"



enum UPDATE_PAIROBJECT { UPDATE_RPO_THRESHOLD=1, UPDATE_RETENTION_POLICY=2 } ; //SHOULD BE 4

class ScenarioConfig;
typedef boost::shared_ptr<ScenarioConfig> ScenarioConfigPtr ;

class ScenarioConfig
{
    ConfSchema::Group* m_PlanGrp ;
    ConfSchema::PlanObject m_planAttrNamesObj ;
    ConfSchema::AppScenarioObject m_scenarioAttrNamesObj ;
    ConfSchema::ScenraionDetailsObject m_scenarioDetailsAttrNamesObj ;
    ConfSchema::ConsistencyPolicyObject m_consistencyPolicyAttrNamesObj ;
    ConfSchema::PairInfoObject m_pairInfoAttrNamesObj ;
    ConfSchema::PairsObject m_pairAttrNamesObj ;
    ConfSchema::ScenarioRetentionPolicyObject m_scenarioRetentionPolicyAttrNamesObj ;
    ConfSchema::ScenarioRetentionWindowObject m_scenarioretwindowAttrNamesObj ;
    ConfSchema::ReplicationOptionsObject m_replicationOptionsAttrNamesObj ;
    ConfSchema::AutoResyncObject m_autoresyncOptionAttrNamesObj ;
    static ScenarioConfigPtr m_scenarioConfigPtr ;
    bool m_isModified ;
    
    void loadScenarioConfig() ;
    
    bool isModified();
    
    void CreatePlanObject() ;

    void CreateScenarioObject(ConfSchema::Group* scenarioGrp) ;

    void CreateScenarioDetailsObject(ConfSchema::Group* scenarioDetailsGrp,
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
                                     const ReplicationOptions& replicationOptions) ;

    void PopulateConsistencyOptions(ConfSchema::Object& scenarioDetailsObj, 
                                        const ConsistencyOptions& consistencyOptions) ;
     void PopulateAutoResyncOptions(ConfSchema::Object& replicationOptionsObj, 
                                        const AutoResyncOptions& autoresyncOptions) ;

    void PopulateReplicationOptions(ConfSchema::Object& pairInfoObject, 
                                    const ReplicationOptions& replicationOptions, bool fromRequest) ;

    void PopulatePairInfo(ConfSchema::Object& scenarioDetailsObj,
                          const std::map<std::string, PairDetails>& pairDetailsMap,
                          const ReplicationOptions& replicationOptions) ;

    void PopulateProtectedVolumesInfo(ConfSchema::Object& scenarioDetailsObj,
                                      const std::string& repositoryName,
                                      const std::map<std::string, PairDetails>& pairDetailsMap) ;

    void PopulateExclusionVolumesInfo(ConfSchema::Object& scenarioDetailsObj,
                                      const std::list<std::string>& exclusionVols) ;

    void PopulateSourceIdInfo(ConfSchema::Object& scenarioDetailsObj, 
                                const std::string& srcHostId) ;

    void PopulatePairs( ConfSchema::Object& pairInfoObject, 
                        const std::map<std::string, PairDetails>& pairDetailsMap) ;

    void PopulateRetetionPolicy( ConfSchema::Object& pairObject,
                                 const RetentionPolicy& retPolicy) ;

    void PopulateRetentionWindows( ConfSchema::Object& retentionPolicyObj, 
                                   const std::list<ScenarioRetentionWindow>& retentionWindows) ;

    bool GetProtectionScenarioObject(ConfSchema::Object** scenarioObject) ;
    
    bool GetScenarioDetailsObject(ConfSchema::Object** scenarioDetailsObject) ;

    bool GetPairObjectByTgtVolume(const std::string& volumeName,
                                  ConfSchema::Object** pairObject) ;

    bool GetPairObjectBySrcVolume(const std::string& volumeName,
                                  ConfSchema::Object** pairObject) ;

    bool GetPairInfoObj(ConfSchema::Object** pairInfoObj) ;
    
    bool GetRetentionPolicy(ConfSchema::Object* pairObject, RetentionPolicy& retentionPolicy) ;

    ScenarioConfig() ;

    bool GetPlanObject(ConfSchema::Object** planObj) ;

    bool GetConsistencyObject(ConfSchema::Object** consistencyObj) ;

    bool GetExcludedVolsAttrGrp(ConfSchema::Object& scenarioDetailsObj,
                                ConfSchema::AttributeGroup** attrGrp) ;

    bool GetProtectedVolsAttrGrp(ConfSchema::Object& scenarioDetailsObj,
                                ConfSchema::AttributeGroup** attrGrp) ;

    void AddPairs( ConfSchema::Group& pairsGrp, 
                    const std::map<std::string, PairDetails>& pairDetailsMap) ;
public:
    void CreateProtectionScenario(const std::string& recoveryFeature,
                                  const std::string& actionOnNewVolumes,
                                  const std::string& srcHostId,
                                  const std::string& tgtHostId,
                                  const std::string& srcHostName,
                                  const std::string& tgtHostName,
                                  const std::string& backupRepoName,
                                  const std::list<std::string>& excludedVols,
                                  const ConsistencyOptions& consistencyOptions,
                                  const std::map<std::string, PairDetails>& pairDetailsMap,
                                  const ReplicationOptions& replicationOptions) ;
    
    bool ProtectionScenarioAvailable() ;
    
    bool GetExecutionStatus(std::string& executionStatus) ;

    void SetExecutionStatus(const std::string& executionStatus) ;

    bool IsAlreadyProtected(const std::string& volumeName) ;

    bool GetExcludedVolumes(std::list<std::string>& volumes) ;   

    bool GetProtectedVolumes(std::list<std::string>& volumes) ;

    bool GetRetentionPolicy(const std::string& volumeName, RetentionPolicy& retentionPolicy) ;

   // bool GetPairObj(const std::string& volumeName, RetentionPolicy& retentionPolicy) ;
    bool GetPairsObjects( ConfSchema::Object_t** pairObjects ) ;

    bool UpdateVolumeProvisioningFinalStatus(const std::string& targetVolumeName, 
                                        const std::string& mountPoint) ;
	bool UpdateVolumeProvisioningFailureStatus(const std::string& srcVolName) ;
												
    bool UpdatePairObjects( std::string& rpoThreshold, RetentionPolicy& retentionPolicy, bool bRPOThreshold, bool bRetentionPolicy  ) ;
    int GetProtectionScenarioId() ;
    bool PopulateConsistencyOptions(const ConsistencyOptions& consistencyOptions) ;
    static ScenarioConfigPtr GetInstance() ;

    bool GetPlanName(std::string& planName) ; 

    bool GetPlanId(std::string& planId) ;

    SV_UINT GetBatchSize() ;
	void SetBatchSize(SV_UINT batchsize ) ;

    bool GetPairDetails(std::map<std::string, PairDetails>& pairDetailsMap) ;

    bool GetPairDetails(const std::string& srcVolName, PairDetails& pairDetails) ;

    bool CanTriggerAutoResync() ;

    void GetSourceTargetVoumeMapping(std::list <std::string> &list , std::map<std::string, std::string>& srcTgtVolMap, bool returnProvisionedAlone = false ) ;

	//void GetSourceTargetVoumeMappingForList(std::list <std::string> &list , std::map<std::string, std::string>& srcTgtVolMap, bool returnProvisionedAlone = false );
    
    void ClearPlan() ;

    bool GetConsistencyOptions(ConsistencyOptions& consistencyOptions) ;

    bool GetReplicationOptions(ReplicationOptions& replicationOptions) ;

    bool GetRecoveryFeaturePolicy(std::string& recoveryFeaturePolicy) ;
    bool PopulateRecoveryFeaturePolicy( const std::string& recoveryFeaturePolicy )  ;

    bool GetActionOnNewVolumeDiscovery(std::string& action) ;

    bool GetRpoThreshold(SV_UINT& rpoThreshold) ;
    bool PopulateRPOThreshold( std::string& rpoThreshold ) ;

    bool GetRetentionPolicy(RetentionPolicy& retPolicy) ;

    void RemoveVolumeFromExclusionList(const std::string& volName) ;

    void AddVolumeToProtectedVolumesList(const std::string& volName) ;

    bool AddPairs(const std::map<std::string, PairDetails> pairDetailsMap) ;

    std::string GetSourceHostId() ;

    std::string GetTargetHostId() ;

    void ChangePaths( const std::string& existingRepo, const std::string& newRepo )  ;

	bool removePairs(std::list <std::string> &list );
	bool removevolumesFromRepo (std::list <std::string> &list );
	bool SetTargetVolumeUnProvisioningStatus(std::list <std::string> &sourceList );
	bool GetTargetVolumeUnProvisioningStatus(std::list <std::string> &sourceList );
	bool SetSystemPausedState(std::string &pausedState);
	std::string GetSystemPausedReason() ;
	void SetSystemPausedReason(const std::string& pausereason) ;
	bool GetSystemPausedState(std::string& systemPausedState);
	bool GetTargetVolumeProvisioningStatus(std::list <std::string> &sourceList );
	bool GetVolumeProvisioningStatus(const std::string &sourceVolume , std::string &volumeProvisioningStatus) ; 
	bool PopulateReplicationOptions (const ReplicationOptions & replicationOptions); 
	bool GetReplicationOptionsObj(ConfSchema::Object** replicationOptionsObj) ;
	void ModifyRetentionBaseDir(const std::string& existRepoBase, const std::string& newRepoBase) ;
	void GetTargetVolumeProvisioningFailedVolumes(std::list <std::string> &list, std::list <std::string> &tagetprovisionFailedVolumes); 
	bool removePairs(std::string &volumeName ); 
	bool removevolumeFromRepo (std::string &volumeName ); 
	void GetSourceTargetVoumeMapping(std::string &sourceVolume , std::map<std::string, std::string>& srcTgtVolMap); 
	void SetExecutionStatustoLeastVolumeProvisioningStatus( ); 
	void UpdateIOParameters(const spaceCheckParameters& spacecheckparams) ;
	void GetIOParameters(spaceCheckParameters& spacecheckparams) ;
	bool GetTargetVolume(const std::string& sourceVolume,
												std::string& targetName) ; 
} ;
bool GetSystemPausedState(int &systemPaused);
#endif
