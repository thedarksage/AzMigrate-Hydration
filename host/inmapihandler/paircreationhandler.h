#ifndef PAIR__CREATION__HANDLER__H_
#define PAIR__CREATION__HANDLER__H_
#include "Handler.h"
#include "confschema.h"
#include "protectionpairobject.h"
#include "volumesobject.h"
#include "apppolicyobject.h"
#include "policyinstanceobj.h"
#include "inmapihandlerdefs.h"
#include "appscenarioobject.h"
#include "planobj.h"
#include "pairsobj.h"
#include "scenarioretentionpolicyobj.h"
#include "retevtpolicyobject.h"
#include "retwindowobject.h"
#include "retlogpolicyobject.h"
#include "replicaionoptionsobj.h"
#include "autoresyncobj.h"
#include "scenariodetails.h"
#include "pairinfoobj.h"
#include "scenarioretentionwindowobj.h"
#include "configurator2.h"

enum ReplicationStatus
{
    RESYNC_STEP1 = 1,
    RESYNC_STEP2,
    DIFF_SYNC
};

enum PairState
{
    ACTIVE = 0,
    PAUSE_PENDING,
    PAUSED,
    TRACKING_PAUSE_PENDING,
    TRACKING_PAUSED,
    DELETED
};

enum ReplicationCleanUpOptions
{
    TARGET_CLEANUP_PENDING,
    TARGET_CLEANUP_COMPLETED
};


class PairCreationHandler :
	public Handler
{
public:
	PairCreationHandler(void);
	~PairCreationHandler(void);
	virtual INM_ERROR process(FunctionInfo& f);
	virtual INM_ERROR validate(FunctionInfo& f);
    /* TODO: why are these publics ? */
    /*       compare ignore case through 
     *       c++. not stricmp */
    bool CreatePairs(SV_UINT batchSize);
	bool DeletePairs();
	bool CheckForPendingResumeTracking() ;
    bool DeleteCorruptedConfigFiles() ;
	INM_ERROR ReconstructRepo(const std::string& agentGuid) ;
    void ReconstrProtectionPairConfig( ) ;
    void RecoverTheCatalogPathFromFs( std::string& catalogpath ) ;
    INM_ERROR GetPairPropertiesFromSettings(const std::string& srcVol,
                                            const InitialSettings& settings,
                                            std::string& targetVolume,
                                            bool& resync,
                                            std::string& jobId,
                                            std::string& retentionPath,
                                            SV_ULONGLONG& resyncStartTagTime,
                                            SV_ULONGLONG& resyncStartSeqNo,
                                            SV_ULONGLONG& resyncEndTagTime,
                                            SV_ULONGLONG& resyncEndSeqNo) ;
    void GetSourceVolumes(std::list<std::string>& volumes) ;
    void ReconstructPolicyConfig() ;
	bool TriggerAutoResync();
	INM_ERROR EnableVolumeUnprovisioningPolicy(FunctionInfo & f);
    bool DetectVolumeResize() ;
	bool GetResizedVolume(std::string &resizedVolumeName); 
	INM_ERROR PendingEvents(FunctionInfo &f );
	INM_ERROR MonitorEvents(FunctionInfo &f );
	bool ThresholdExceededAlert() ;
    Configurator* m_Configurator ;
    void LoadAllConfigurationFiles() ;
private:
    void CreatePairsIfRequired() ;
    void updatePairStateFromLocalSettings(HOST_VOLUME_GROUP_SETTINGS& hvs) ;
	std::list <std::string> getVolumesFromVolumeUnprovisioningPolicy (FunctionInfo &f);


    ConfSchema::Group *m_pPlanGroup;
    ConfSchema::PlanObject m_PlanObj;

    ConfSchema::Group *m_pAppScenarioGroup;
    ConfSchema::AppScenarioObject m_AppScenarionObj;

    ConfSchema::Group *m_pPairGroup;
    ConfSchema::PairsObject m_PairObject;

    ConfSchema::Group *m_pProtectionPairGroup;
    ConfSchema::ProtectionPairObject m_ProtectionPairObj;
	
	ConfSchema::ApplicationPolicyObject m_PolicyObj;
	ConfSchema::AppPolicyInstanceObject m_PolicyInstanceObj;

    ConfSchema::ScenarioRetentionPolicyObject m_RetentionPolicyObj;
    ConfSchema::RetLogPolicyObject m_RetLogPolicyObj;
    ConfSchema::RetWindowObject m_RetenWndObj;
    ConfSchema::RetEventPoliciesObject m_RetenEventPoliciesObj;
    ConfSchema::ReplicationOptionsObject m_ReplicationOptionsObj;
    ConfSchema::AutoResyncObject m_AutoResyncObject;
    ConfSchema::ScenraionDetailsObject m_ScenarioDetailsObj;
    ConfSchema::PairInfoObject m_PairInfoObj;
    ConfSchema::ScenarioRetentionWindowObject m_RetentionWindowObj;
    time_t m_CatalogPathUniqId;

};


#endif /* PAIR__CREATION__HANDLER__H_ */