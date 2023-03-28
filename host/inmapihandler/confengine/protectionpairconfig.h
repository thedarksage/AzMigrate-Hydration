#ifndef PROTECTIONPAIR_CONFIG_H
#define PROTECTIONPAIR_CONFIG_H
#include <string>
#include <list>
#include <boost/shared_ptr.hpp>

#include "volumegroupsettings.h" 
#include "protectionpairobject.h"
#include "confschema.h"
#include "confschemamgr.h"
#include "xmlizeretentioninformation.h"
#include "xmlizeinitialsettings.h"
#include "xmlizevolumegroupsettings.h"
#include "retlogpolicyobject.h"
#include "scenarioconfig.h"
#include "retwindowobject.h"
#include "retevtpolicyobject.h"
#include "inmsdkutil.h"
class ProtectionPairConfig ;
typedef boost::shared_ptr<ProtectionPairConfig> ProtectionPairConfigPtr ;
class ProtectionPairConfig
{
    bool m_isModified ;
    ConfSchema::ProtectionPairObject m_proPairAttrNamesObj ;
    ConfSchema::RetLogPolicyObject m_retLogPolicyAttrNamesObj ;
    ConfSchema::RetWindowObject m_retWindowAttrNamesObj ;
    ConfSchema::RetEventPoliciesObject m_retevtPolicyAttrNamesObj ;
    ConfSchema::Group* m_ProtectionPairGrp ;
    static ProtectionPairConfigPtr m_protectionPairConfigPtr ;
    void loadProtectionPairGrp() ;

    bool isModified() ;

    bool GetProtectionPairObj(const std::string& srcDeviceName,
                              const std::string& tgtDeviceName,
                              ConfSchema::Object** protectionPairObj,
                              const std::string& srcHostId = "",
                              const std::string& tgtHostId = "") ;

    bool GetProtectionPairObjByTgtDevName(const std::string& tgtDevName,
                                          ConfSchema::Object** protectionPairObj) ;

    bool IsValidJobId(ConfSchema::Object* protectionPairObj, 
                      const std::string& jobId) ;

    bool GetPairInResyncQueuedState(ConfSchema::Object** protectionPairObj) ;

    bool IsPairInStepTwo(ConfSchema::Object* protectionPairObj) ;

    bool IsPairInDiffSync(ConfSchema::Object* protectionPairObj) ;

    void SetResyncRequired(const std::string& srcDevName,
                           const std::string& errMessage,
                           int resyncSource) ;
    
    bool GetRetentionLogPolicyObject(ConfSchema::Object* protectionPairObj,
                                     ConfSchema::Object** retLogPolicyObj) ;

    void UpdateCurrentLogSize(ConfSchema::Object* protectionPairObj,
                              SV_ULONGLONG diskSpace) ;

    void UpdateVolumeFreeSpace(const std::string& volName,
                               SV_LONGLONG freeSpace) ;

    void GetCleanUpOptionsString( const std::string & cleanUpActionStr,
                                  std::string &cleanUpOptions ) ;

    void RestartResyncForPair( ConfSchema::Object* protectionPairObj, bool queued ) ;

    void PopulateProtectionPairAttrs( const PairDetails& pairDetails,
                                      ConfSchema::Object& protectionPairObj,
                                      bool Queued) ;

    bool IsAutoResyncAllowed( ConfSchema::Object* protectionPairObj  ) ;

    
    void PopualateRetentionEventPolicies(ConfSchema::Object& defaultRetentionWindowObj, 
        const std::string& retentionPath, const std::string& retentionVolume, const  std::string &srcVolumeName, SV_UINT tagCount = 1, bool bExisting = false ) ;

    void PopulateRetentionWindowObj(ConfSchema::Object& dailyWindowObj, 
                                    const RetentionPolicy& retPolicy,
                                    const ScenarioRetentionWindow& retWindow,
                                    const std::string& previousEndTime , bool bExisting = false ) ;

    void PopulateDefaultRetentionWindowObj(ConfSchema::Object& defaultWindowObj,
                                           const RetentionPolicy& retPolicy,  bool bExisting = false ) ;

    void PopulateRetentionWindowObjects(ConfSchema::Object& retentionLogObject,
                                        const PairDetails& pairDetails) ;

    void PopulateRetentionInformation(const PairDetails& pairDetails, 
                                      ConfSchema::Object& protectionPairObj) ;
    ProtectionPairConfig() ;

    int PairsInResync() ;

    bool IsRestartResyncAllowed(ConfSchema::Object* protectionPairObj) ;

    bool ShouldQueued(ConfSchema::Object* protectionPairObj, int batchSize) ;

    bool IsResyncRequiredSetToYes(ConfSchema::Object* protectionPairObj) ;

    void MarkPairForDeletionPending(ConfSchema::Object* protectionPairObj) ;

    bool IsCleanupDone( ConfSchema::Object* protectionPairObj) ;

    std::string getPairStatus(ConfSchema::Object& protectionPairObj) ;

    std::string GetCurrentRPO(ConfSchema::Object& protectionPairObj)  ;
	bool IsValidRPO(ConfSchema::Object* protectionPairObj) ;
	std::string GetResyncProgress(ConfSchema::Object& protectionPairObj) ;
	std::string GetEtlForResync(ConfSchema::Object& protectionPairObj) ;

	void GetResyncStartAndEndTime(ConfSchema::Object& protectionPairObj,
                                std::string& resyncStartTime,
								std::string& resyncEndTime) ;
    std::string GetResyncRequiredState(ConfSchema::Object& protectionPairObj) ;
	std::string GetPauseExtendedReason(ConfSchema::Object& protectionPairObj) ;
	void EnableAutoResync(ConfSchema::Object& protectionPairObj) ;
	void DisableAutoResync(ConfSchema::Object& protectionPairObj) ;
	bool IsAutoResyncDisabled(ConfSchema::Object& protectionPairObj) ;
    bool GetRetentionEventPolicies(ConfSchema::Object* retPolObject,
                                   std::list<ConfSchema::Object>& retEvntPolicyObjs) ;

    bool GetRetentionPath(ConfSchema::Object* protectionPairObj, std::string& retentionPath) ;
    bool GetSourceVolumeRetentionPathAndVolume( ConfSchema::Object* protectionPairObj, std::string& srcVolumeName, std::string& retentionVolume, std::string& retentionPath ) ;

    bool IsResyncReqdAfterResynStartTagTime( SV_ULONGLONG resyncStartTagTime,
                                             const std::string& resyncReqdMsg,
                                             SV_ULONGLONG& resyncRequiredTs, 
                                                              bool& bInNano) ;

    std::string GenerateCatalogPath( const std::string& retentionVolume,
                                     const std::string& srcVolumeName) ;
	
public:
	bool MoveOperationInProgress() ;
	bool GetRetentionPath(std::map<std::string, std::string>& srcVolRetentionPathMap) ;
	void PauseProtectionByTgtVolume(const std::string& tgtdevname) ;
	void ResumeProtectionByTgtVolume(const std::string& tgtdevname) ;

	void PauseProtection(CDP_DIRECTION direction, const std::string& device) ;
	void PauseTrack(CDP_DIRECTION direction,const std::string& device) ;
    void NotValidRPO(const std::string& srcDevName) ;
    bool PerformPendingResumeTracking() ;
    bool UpdatePairWithRetention( const std::string& srcVolume,
                                 const std::string& retentionvolume,
                                 const std::string& retentionpath,
                                 const std::string& catalogpath,
                                 const RetentionPolicy& retentionpolicy) ;
    void ResetValues(std::string& srcVol,
                      std::string& tgtVol,
                      VOLUME_SETTINGS::PAIR_STATE pairState,
                      bool resyncRequiredFlag,
                      int resyncRequiredCause,
                      int syncMode,
                      std::string& jobId,
                      SV_ULONGLONG resyncStartTagTime,
                      SV_ULONGLONG resyncStartSeqNo,
                      SV_ULONGLONG resyncEndTagTime,
                      SV_ULONGLONG resyncEndSeqNo,
                      std::string& retentionPath) ;
    bool GetProtectionPairObjBySrcDevName(const std::string& srcDevName,
                                          ConfSchema::Object** protectionPairObj) ;

    bool IsPairInQueuedState(ConfSchema::Object* protectionPairObj) ;

    bool IsPairInStepOne(ConfSchema::Object* protectionPairObj) ;

    bool IsPairExists(const std::string& srcDevName,
                      const std::string& tgtDevName,
                      const std::string& srcHostId = "",
                      const std::string& tgtHostId = "") ;


    bool IsPairInStepOne(const std::string& srcDevName,
                         const std::string& tgtDevName,
                         const std::string& srcHostId = "",
                         const std::string& tgtHostId = "",
                         const std::string& jobId = "") ;


    bool IsPairInStepTwo(const std::string& srcDevName,
                         const std::string& tgtDevName,
                         const std::string& srcHostId = "",
                         const std::string& tgtHostId = "",
                         const std::string& jobId = "") ;

    bool IsPairInDiffSync(const std::string& srcDevName,
                         const std::string& tgtDevName = "",
                         const std::string& srcHostId = "",
                         const std::string& tgtHostId = "",
                         const std::string& jobId = "") ;

    
    void UpdateStartTimeForFastSync(const std::string& srcDeviceName,
                                    const std::string& tgtDeviceName,
                                    const std::string& timeStamp,
                                    const std::string& sequenceNo,
                                    const std::string& jobId) ;

    bool GetResyncEndTagTimeAndSeqNo(const std::string& srcDeviceName,
                                  const std::string& tgtDeviceName,
                                  const std::string& jobId,
                                  std::string& resyncEndTagTime,
                                  std::string& resyncEndSeqNo) ;

    bool GetResyncStartTagTimeAndSeqNo(const std::string& srcDeviceName,
                                       const std::string& tgtDeviceName,
                                       const std::string& jobId,
                                       std::string& resyncEndTagTime,
                                       std::string& resyncEndSeqNo) ;
    bool MovePairFromStepOneToTwo(const std::string& srcDeviceName,
                                  const std::string& tgtDeviceName,
                                  SV_LONGLONG srcVolumeCapacity,
                                  const std::string& jobId) ;

    bool SetResyncStartTagTimeAndSeqNo(const std::string& srcDevName,
                                       const std::string& tgtDevName,
                                       const std::string& timeStamp,
                                       const std::string& seqNo,
                                       const std::string& jobId,
                                       const std::string& srcHostId = "",
                                       const std::string& tgtHostId = "") ;

    bool SetResyncEndTagTimeAndSeqNo(const std::string& srcDevName,
                                     const std::string& tgtDevName,
                                     const std::string& timeStamp,
                                     const std::string& seqNo,
                                     const std::string& jobId,
                                     const std::string& srcHostId = "",
                                     const std::string& tgtHostId = "") ;



    bool UpdateLastResyncOffset( const std::string& srcDevName,
                                 const std::string& tgtDevName,
                                 const std::string& jobId,
                                 SV_LONGLONG offset,
								 SV_LONGLONG fsunusedbytes,
                                 const std::string& srcHostId = "",
                                 const std::string& tgtHostId = "") ;

    bool UpdateResyncProgress( const std::string& srcDevName,
                               const std::string& tgtDevName,
                               const std::string& jobId,
                               const std::string& bytes,
                               const std::string& matched,
                               const std::string& srcHostId = "",
                               const std::string& tgtHostId = "" ) ;

    void UpdatePendingChanges(const std::string& srcDevName,
                              SV_LONGLONG pendingChangesInBytes,
                              double rpo) ;

    void SetResyncRequiredBySource(const std::string& srcDevName,
                                   const std::string& errMessage) ;

    void SetResyncRequiredByTarget(const std::string& srcDevName,
                                   const std::string& errMessage) ;

    void UpdateQuasiEndState(const std::string& tgtDevName) ;

    void UpdateReplState( const std::string& srcDevName, VOLUME_SETTINGS::SYNC_MODE syncMode) ;

	void UpdatePairState( const std::string& srcDevName,VOLUME_SETTINGS::PAIR_STATE pairState);

    void UpdateCDPSummary(const std::string& tgtDevName,
                          const ParameterGroup& cdpSummaryPg) ;

    void GetRetentionWindowDetailsByTgtDevName(const std::string& tgtDevName,
                                               ParameterGroup& pg) ;

    void UpdateCleanUpAction(const std::string& srcDevName,
                             const std::string& cleanupAction) ;

    void UpdatePauseReplicationStatus( const std::string& devName,
                                       int hostType,
                                       const std::string& pauseString ) ;

    bool IsPauseAllowed(const std::string& srcDevName,
                        const std::string& tgtDevName = "" ,
                        const std::string& srcHostId = "",
                        const std::string& tgtHostId = "") ;

    bool IsResumeAllowed(const std::string& srcDevName,
                         const std::string& tgtDevName = "",
                         const std::string& srcHostId = "",
                         const std::string& tgtHostId = "") ;

    bool IsPauseTrackingAllowed(const std::string& srcDevName,
                                const std::string& tgtDevName = "",
                                const std::string& srcHostId = "",
                                const std::string& tgtHostId = "") ;

    bool IsResumeTrackingAllowed(const std::string& srcDevName,
                                 const std::string& tgtDevName = "",
                                 const std::string& srcHostId = "",
                                 const std::string& tgtHostId = "") ;

    bool GetPairState( const std::string& srcDevName,
                       VOLUME_SETTINGS::PAIR_STATE& pairState,
                       const std::string& tgtDevName = "",
                       const std::string& srcHostId = "",
                       const std::string& tgtHostId = "") ;
    void GetPairState( ConfSchema::Object* protectionPairObj, VOLUME_SETTINGS::PAIR_STATE& pairState ) ;

	bool IsResyncProtectionAllowed (ConfSchema::Object* protectionPairObj, 
									VOLUME_SETTINGS::PAIR_STATE& pairState,
									bool isManualResync);

	bool IsResyncProtectionAllowed (const std::string& srcDevName,
									VOLUME_SETTINGS::PAIR_STATE& pairState,
									bool isManualResync);


	bool IsResyncProtectionAllowed (VOLUME_SETTINGS::PAIR_STATE& pairState, bool AutoResyncDisabled);

    bool PauseProtection(const std::string& srcDevName,
                         const bool directPause = false,
						 const std::string& pauseReason = "",
						 const std::string& tgtDevName = "",
                         const std::string& srcHostId = "",
                         const std::string& tgtHostId = "") ;

    bool PauseTracking(const std::string& srcDevName,
                       bool isRescueMode,
                       const std::string& tgtDevName = "" ,
                       const std::string& srcHostId = "",
                       const std::string& tgtHostId = "") ;

    bool ResumeTracking( const std::string& srcDevName,
                         int batchSize,
						 bool isRescueMode,
                         const std::string& tgtDevName = "" ,
                         const std::string& srcHostId = "",
                         const std::string& tgtHostId = "") ;
	bool ResumeProtection( ) ;
    bool ResumeProtection( const std::string& srcDevName,
                           const std::string& tgtDevName = "",
                           const std::string& srcHostId = "",
                           const std::string& tgtHostId = "") ;

    void RestartResyncForPair( const std::string& srcDevName,
                               int batchSize,
                               const std::string& tgtDevName = "",
                               const std::string& srcHostId = "",
                               const std::string& tgtHostId = "") ;

    void AddToProtection( const PairDetails& pairDetails, int batchSize) ;
    bool UpdatePairsWithRetention( RetentionPolicy& retentionPolicy ) ;

    void GetProtectedVolumes( std::list<std::string>& protectedVolumes) ;
    void GetProtectedTargetVolumes( std::list<std::string>& protectedVolumes) ;
    void DeletePair( const std::string& srcDevName,
                     const std::string& tgtDevName,
                     const std::string& srcHostId = "",
                     const std::string& tgtHostId = "") ;

    void PerformAutoResync(int batchSize) ;

    bool GetRetentionWindowDetailsByTgtVolName(const std::string& tgtDevName,
                                               std::string& logsFrom,
                                               std::string& logsTo,
                                               std::string& logsFromUtc,
                                               std::string& logsToUtc,
                                               const std::string& srcHostId = "",
                                               const std::string& tgtHostId = "") ;

    bool GetRetentionWindowDetailsBySrcVolName(const std::string& srcDevName,
                                               std::string& logsFrom,
                                               std::string& logsTo,
                                               std::string& logsFromUtc,
                                               std::string& logsToUtc,
                                               const std::string& srcHostId = "",
                                               const std::string& tgtHostId = "") ;

    bool GetPairInsertionTime(const std::string& srcDevName,
                              SV_LONGLONG& pairInsertionTime) ;

    bool GetRPOInSeconds(const std::string& srcDevName,
                         double& rpoInSec) ;

    void GetRPOInSeconds(ConfSchema::Object& protectionPairObj, double& rpoInSec) ;
    std::string ConvertRPOToHumanReadableformat( double& nRpoinSec ) ;

    bool IsRestartResyncAllowed(const std::string& srcDevName,
                                const std::string& tgtDevName,
                                const std::string& srcHostId,
                                const std::string& tgtHostId) ;

    bool IsResyncRequiredSetToYes(const std::string& srcDevName,
                                  const std::string& tgtDevName = "" ,
                                  const std::string& srcHostId = "" ,
                                  const std::string& tgtHostId = "") ;

	void MarkPairsForDeletionPending(std::list <std::string> &list) ;

    bool IsCleanupDoneForPairs( ) ;

    void ClearPairs() ;

    void GetCurrentRPO(const std::string& srcVolName,
                       std::string& rpoInHumanRdblFmt,
                       const std::string& tgtVolName = "",
                       const std::string& srcHostId= "",
                       const std::string& tgtHostId = "") ;

    void GetPairState(const std::string& srcDevName,
                      std::string& PairState,
                      const std::string& tgtDevName = "",
                      const std::string& srcHostId = "",
                      const std::string& tgtHostId = "") ;

    void GetRepositoryNameAndPath(const std::string& srcVolName,
                                  std::string& RepositoryName,
                                  std::string& RepositoryPath,
                                  const std::string& tgtVolName = "",
                                  const std::string& srcHostId = "",
                                  const std::string& tgtHostId = "") ;

    void GetResyncProgress(const std::string& srcDevName,
                           std::string& resyncProgress,
                           const std::string& tgtDevName = "",
                           const std::string& srcHostId = "",
                           const std::string& tgtHostId = "") ;

	void GetEtlForResync( const std::string& srcDevName,
                         std::string& resyncProgress,
                         const std::string& tgtDevName = "",
                         const std::string& srcHostId = "",
                         const std::string& tgtHostId = "") ;

    void GetResyncRequiredState(const std::string& srcDevName,
                                std::string& resyncRequiredState,
                                const std::string& tgtDevName = "",
                                const std::string& srcHostId = "",
                                const std::string& tgtHostId = "") ;
	void GetPauseExtendedReason(const std::string& srcDevName,
                                std::string& pauseReason,
                                const std::string& tgtDevName = "",
                                const std::string& srcHostId = "",
                                const std::string& tgtHostId = "") ;
	void DisableAutoResync(const std::string& srcDevName,
						   const std::string& tgtDevName = "",
						   const std::string& srcHostId = "",
						   const std::string& tgtHostId = "") ;

	void EnableAutoResync(const std::string& srcDevName,
						   const std::string& tgtDevName = "",
						   const std::string& srcHostId = "",
						   const std::string& tgtHostId = "") ;
	bool IsAutoResyncDisabled(const std::string& srcDevName,
							const std::string& tgtDevName = "",
							const std::string& srcHostId = "",
							const std::string& tgtHostId = "") ;

	void GetResyncStartAndEndTime(const std::string& srcDevName,
                                std::string& resyncStartTime,
								std::string& resyncEndTime,
                                const std::string& tgtDevName = "",
                                const std::string& srcHostId = "",
                                const std::string& tgtHostId = "") ;

	void GetResyncStartAndEndTimeInHRF(const std::string& srcDevName,
                                std::string& resyncStartTime,
								std::string& resyncEndTime,
                                const std::string& tgtDevName = "",
                                const std::string& srcHostId = "",
                                const std::string& tgtHostId = "") ;

    bool GetTargetVolumeBySrcVolumeName(const std::string& srcDevName,
                                        std::string& tgtDevName,
                                        const std::string& srcHostId = "",
                                        const std::string& tgtHostId = "") ;

    bool GetRetentionPathBySrcDevName(const std::string& srcDevName,
                                      std::string& retentionPath,
                                      const std::string& srcHostId = "",
                                      const std::string& tgtHostId = "") ;

    bool GetRetentionPathByTgtDevName(const std::string& tgtDevName,
                                      std::string& retentionPath,
                                      const std::string& srcHostId = "",
                                      const std::string& tgtHostId = "") ;

    bool MarkPairAsResized(const std::string& srcDevName,
                           const std::string& tgtDevName = "" ,
                           const std::string& srcHostId = "",
                           const std::string& tgtHostId = "") ;

    bool IsPairMarkedAsResized(const std::string& srcDevName,
                           const std::string& tgtDevName = "" ,
                           const std::string& srcHostId = "",
                           const std::string& tgtHostId = "") ;

    bool IsProtectedAsSrcVolume(const std::string& srcDevName,
                                 const std::string& srcHostId = "",
                                 const std::string& tgtHostId = "") ;

    bool IsProtectedAsTgtVolume(const std::string& tgtDevName,
                                 const std::string& srcHostId = "",
                                 const std::string& tgtHostId = "") ;

    static ProtectionPairConfigPtr GetInstance() ;

    bool UpdateRetentionObject( ConfSchema::Object &retentionPolicyObj, 
                                const RetentionPolicy& retentionPolicy, 
                                const std::string& srcVolumeName, 
                                const std::string& retentionVolume, 
                                const std::string& retentionPath,
                                const std::string& catalogpath="") ;
    
    bool GetRetentionWindowObjectForUpdate( std::string windowType, 
                                                             ConfSchema::Group &retentionWindowGroup, 
                                                             ConfSchema::Object** retentionWindowObject ) ;
    void GetProtectedPairs( std::map<std::string, std::string>& pairs ) ;

    void GetCurrentLogSizeBySrcDevName(const std::string& srcDevName,
                                       SV_ULONGLONG& logSize,
                                       const std::string& srcHostId = "",
                                       const std::string& tgtHostId = "") ;

    void UpdateCDPRetentionDiskUsage( const std::string& tgtVolume, SV_ULONGLONG startTs, 
                                      SV_ULONGLONG endTs, SV_ULONGLONG sizeOnDisk, 
                                      SV_ULONGLONG spaceSavedByCompression, 
                                      SV_ULONGLONG spaceSavedBySparsePolicy, SV_UINT numFiles) ;

    void ChangePaths( const std::string& existingRepo, const std::string& newRepo )  ;
	void GetBackupProtectionRemoveList(std::list <std::string> &list);
	void ClearPairs(std::list<std::string> &list);
	int getProtectionPairObjectCount() const ;
	bool IsDeletionInProgress( const std::string &volname) ;
	bool IsDeletionInProgress(ConfSchema::Object* protectionPairObj);
	void GetVolumesMarkedForDeletion(std::list <std::string> &list);
	bool GetSourceListFromTargetList (std::list<std::string> &targetVolumes , std::list<std::string> &sourceVolumes);
	void GetTargetVolumes( std::list<std::string>& tgtVolumes) ;
	bool isAllProtectedVolumesPaused (); 
	SV_ULONGLONG GetCDPStoreSizeByProtectedVolumes (std::list<std::string> &protectedVolumes );
	void ActOnQueuedPair() ;
	void ActOnStepOnePair();
	SV_INT GetNumberOfPairsProgressingInStepOne();
	bool SetQueuedState(const std::string& srcVolume, PAIR_QUEUED_STATUS) ;
	void ModifyRetentionBaseDir(const std::string& existRepoBase, const std::string& newRepoBase) ;
	void MarkPairsForDeletionPending(std::string &volumeName); 
	bool isPotectedVolumeObjectListExists (const std::list<std::string> &srcVolumeList); 
	bool isVolumeExists(const std::string& srcVolumeName); 
	bool isVolumePaused (const std::string &volumeName ); 
	void validateAndActOnResizedVolume(const std::string &resizedVolume); 
	void doStopFilteringAndRestartResync (const std::string &resizedVolume); 
	void RestartResyncForPair (const std::string &resizedVolume); 
	bool IncrementVolumeResizeActionCount(const std::string& srcVolumeName); 
	bool GetVolumeResizeActionCount(const std::string& srcVolumeName , int &volumeResizeActionCount); 
} ;
#endif
