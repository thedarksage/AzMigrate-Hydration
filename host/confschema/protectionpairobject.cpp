#include "protectionpairobject.h"

namespace ConfSchema
{
    ProtectionPairObject::ProtectionPairObject()
    {
        /* TODO: all these below are hard coded (taken from
         *       schema document)                      
         *       These have to come from some external
         *       source(conf file ?) ; Also curretly we use string constants 
         *       so that any instance of volumesobject gives 
         *       same addresses; 
         *       Not using std::string members and static volumesobject 
         *       for now since calling constructors of statics across 
         *       libraries is causing trouble (no constructors getting 
         *       called) */
        m_pName = "ProtectionPairs";
        m_pIdAttrName = "Id";
        m_pSrcNameAttrName = "sourceDevice";
        m_pTgtNameAttrName = "targetDevice";
		m_pRepositoryNameAttrName = "repositoryName";
        m_pReplStateAttrName = "replicationState";
        m_pResyncStartTagTimeAttrName = "resyncStartTagTime";
        m_pResyncEndTagTimeAttrName = "resyncEndTagTime";
        m_pResyncSetTimeStampAttrName = "resyncSettimestamp";
        m_pPairStateAttrName = "pairState";
        m_pCurrentRPOTimeAttrName = "currentRPOTime";
        m_pResyncStartSeqAttrName = "resyncStartTagTimeSequence";
        m_pResyncStartTime = "resyncStartTime" ;
        m_pResyncEndTime = "resyncEndTime" ;
        m_pResyncEndSeqAttrName = "resyncEndTagTimeSequence";
        m_pResyncCounterAttrName = "resyncCounter";
        m_pShouldResyncAttrName = "shouldResync";
        m_pLastOffsetAttrName = "lastResyncOffset";
        m_pShouldResyncSetFromAttrName = "shouldResyncSetFrom";
        m_pResyncSetCXtimeStamp = "resyncSetCxtimestamp";
        m_pLogsFromAttrName = "logsFrom";
        m_pLogsFromUTCAttrName = "logsFromUTC";
        m_pLogsToAttrName = "logsTo";
        m_pLogsTOUTCAttrName = "logsToUTC";
        m_pAutoResumeAttrName = "autoResume";
        m_pFullSyncBytesSentAttrName = "fullSyncBytesSent";
        m_pQuasiDiffStarttimeAttrName = "quasidiffStarttime";
        m_pQuasiDiffEndtimeAttrName = "quasidiffEndtime";
        m_pAutoResyncStartTimeAttrName = "autoResyncStartTime";
        m_pExecutionStateAttrName = "executionState";
        m_pIsMarkedForDeletionAttrName = "IsMarkedForDeletion";
        m_pPauseReasonAttrName = "pauseReason";
		m_pPauseExtendedReasonAttrName = "pauseExtendedReason";
        m_pJobIdAttrName = "jobId" ;
        m_pRestartResyncCounterAttrName ="restartResyncCounter";
        m_pCleanupActionAttrName = "cleanupAction";
        m_pMaintenanceActionAttrName = "maintenanceAction";
        m_pReplicationCleanupOptionAttrName = "replicationCleanupOption";
        m_pPendingChangesSizeAttrName = "PendingChangesSize";
        m_pFastResyncMatchedAttrName = "fastResyncMatched";
        m_pFastResyncUnMatchedAttrName = "fastResyncUnMatched" ;
        m_pVolumeResize = "VolumeResized" ;
		m_pVolumeResizeActionCount = "VolumeResizedActionCount" ;
        m_pResizePolicyTimeStamp = "ResizePolicyTimeStamp" ;
        m_pSourceVolumeCapacity = "SourceVolumeCapacity" ;
        m_pSourceVolumeRawSize = "SourceVolumeRawSize" ;
		m_pMarkedForDeletion = "MarkedForDeletion";
		m_pResumeTrackingByService = "ResumeTrackingByService" ;
		m_pFsUnusedBytes = "FsUnUsedBytes" ;
		m_pValidRPO = "IsRPOValid" ;
		m_pAutoResyncDisabled = "IsAutoResyncDisabled" ;
    }
}
