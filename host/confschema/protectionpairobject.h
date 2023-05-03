#ifndef PROTECTIONPAIR__OBJECT__H__
#define PROTECTIONPAIR__OBJECT__H__

/* TODO: should not be using these names finally */

namespace ConfSchema
{
    class ProtectionPairObject
    {
        const char *m_pName;
        const char *m_pIdAttrName;
        const char *m_pSrcNameAttrName;
        const char *m_pTgtNameAttrName;
		const char *m_pRepositoryNameAttrName;
        const char *m_pReplStateAttrName;
        const char *m_pResyncStartTagTimeAttrName;
        const char *m_pResyncEndTagTimeAttrName;
        const char *m_pResyncSetTimeStampAttrName;
        const char *m_pPairStateAttrName;
        const char *m_pCurrentRPOTimeAttrName;
        const char *m_pResyncStartSeqAttrName;
        const char *m_pResyncEndSeqAttrName;
        const char *m_pResyncCounterAttrName;
        const char *m_pShouldResyncAttrName;
        const char *m_pLastOffsetAttrName;
		const char *m_pFsUnusedBytes ;
        const char *m_pShouldResyncSetFromAttrName;
        const char *m_pResyncSetCXtimeStamp;
        const char *m_pLogsFromAttrName;
        const char *m_pLogsFromUTCAttrName;
        const char *m_pLogsToAttrName;
        const char *m_pLogsTOUTCAttrName;
        const char *m_pAutoResumeAttrName;
        const char *m_pFullSyncBytesSentAttrName;
        const char *m_pQuasiDiffStarttimeAttrName;
        const char *m_pQuasiDiffEndtimeAttrName;
        const char *m_pAutoResyncStartTimeAttrName;
        const char *m_pExecutionStateAttrName;
        const char *m_pIsMarkedForDeletionAttrName;
        const char *m_pPauseReasonAttrName;
		const char *m_pPauseExtendedReasonAttrName ;
        const char *m_pRestartResyncCounterAttrName;
        const char *m_pCleanupActionAttrName;
        const char *m_pMaintenanceActionAttrName;
        const char *m_pReplicationCleanupOptionAttrName;
        const char *m_pPendingChangesSizeAttrName;
        const char *m_pFastResyncMatchedAttrName;
        const char *m_pFastResyncUnMatchedAttrName;
        const char *m_pJobIdAttrName ;
        const char *m_pResyncStartTime ;
        const char *m_pResyncEndTime ;
        const char *m_pVolumeResize ;
		const char *m_pVolumeResizeActionCount ;
        const char *m_pResizePolicyTimeStamp ;
        const char *m_pSourceVolumeCapacity ;
        const char *m_pSourceVolumeRawSize ;
		const char *m_pMarkedForDeletion;
		const char *m_pResumeTrackingByService ;
		const char *m_pValidRPO ;
		const char *m_pAutoResyncDisabled ;
    public:
        
        ProtectionPairObject();
        const char * GetName(void);
        const char * GetIdAttrName(void);
        const char * GetSrcNameAttrName(void);
        const char * GetTgtNameAttrName(void);
		const char * GetRepositoryNameAttrName(void);
        const char * GetReplStateAttrName(void);
        const char * GetResyncStartTagTimeAttrName(void);
        const char * GetResyncEndTagTimeAttrName(void);
        const char * GetResyncSetTimeStampAttrName(void);
        const char * GetPairStateAttrName(void);
        const char * GetCurrentRPOTimeAttrName(void);
        const char * GetResyncStartSeqAttrName(void);
        const char * GetResyncEndSeqAttrName(void);
        const char * GetResyncCounterAttrName(void);
        const char * GetShouldResyncAttrName(void);
        const char * GetLastOffsetAttrName(void);
		const char * GetFsUnusedBytes(void) ;
        const char * GetShouldResyncSetFromAttrName(void);
        const char * GetResyncSetCXtimeStampAttrName(void);
        const char * GetLogsFromAttrName(void);
        const char * GetLogsFromUTCAttrName(void);
        const char * GetLogsToAttrName(void);
        const char * GetLogsTOUTCAttrName(void);
        const char * GetAutoResumeAttrName(void);
        const char * GetFullSyncBytesSentAttrName(void);
        const char * GetQuasiDiffStarttimeAttrName(void);
        const char * GetQuasiDiffEndtimeAttrName(void);
        const char * GetAutoResyncStartTimeAttrName(void);
        const char * GetExecutionStateAttrName(void);
        const char * GetIsMarkedForDeletionAttrName(void);
        const char * GetPauseReasonAttrName(void);
		const char * GetPauseExtendedReasonAttrName(void) ;
        const char * GetRestartResyncCounterAttrName(void);
        const char * GetCleanupActionAttrName(void);
        const char * GetMaintenanceActionAttrName(void);
        const char * GetReplicationCleanupOptionAttrName(void);
        const char * GetPendingChangesSizeAttrName(void);
        const char * GetFastResyncMatchedAttrName(void);
        const char * GetFastResyncUnMatchedAttrName(void);
        const char * GetJobIdAttrName(void) ;
        const char * GetResyncStartTime(void) ;
        const char * GetResyncEndTime(void) ;
        const char * GetPauseCountAttrName(void) ;
        const char * GetVolumeResizedAttrName(void) ;
		const char * GetVolumeResizedActionCountAttrName(void) ;
        const char * GetResizePolicyTSAttrName(void) ;
        const char * GetSrcVolCapacityAttrName(void) ;
        const char * GetSrcVolRawSizeAttrName(void) ;
		const char * GetMarkedForDeletionAttrName (void);
		const char * GetResumeTrackingByService(void) ;
		const char * GetIsValidRPOAttrName(void) ;
		const char * GetAutoResyncDisabledAttrName(void) ;
    };
	inline const char * ProtectionPairObject::GetAutoResyncDisabledAttrName(void)
    {   
        return m_pAutoResyncDisabled;
    }
    inline const char * ProtectionPairObject::GetFastResyncMatchedAttrName(void)
    {   
        return m_pFastResyncMatchedAttrName;
    }
    inline const char * ProtectionPairObject::GetFastResyncUnMatchedAttrName(void)
    {   
        return m_pFastResyncUnMatchedAttrName;
    }

    inline const char * ProtectionPairObject::GetPendingChangesSizeAttrName(void)
    {   
        return m_pPendingChangesSizeAttrName;
    }
    inline const char * ProtectionPairObject::GetReplicationCleanupOptionAttrName(void)
    {   
        return m_pReplicationCleanupOptionAttrName;
    }   
    inline const char * ProtectionPairObject::GetMaintenanceActionAttrName(void)
    {   
        return m_pMaintenanceActionAttrName;
    }
    inline const char * ProtectionPairObject::GetCleanupActionAttrName(void)
    {   
        return m_pCleanupActionAttrName;
    }
    inline const char * ProtectionPairObject::GetRestartResyncCounterAttrName(void)
    {   
        return m_pRestartResyncCounterAttrName;
    }
    inline const char * ProtectionPairObject::GetPauseReasonAttrName(void)
    {   
        return m_pPauseReasonAttrName;
    }
	inline const char * ProtectionPairObject::GetPauseExtendedReasonAttrName(void)
    {   
        return m_pPauseExtendedReasonAttrName;
    }

    inline const char * ProtectionPairObject::GetJobIdAttrName(void)
    {
        return m_pJobIdAttrName ;
    }
    inline const char * ProtectionPairObject::GetIsMarkedForDeletionAttrName(void)
    {   
        return m_pIsMarkedForDeletionAttrName;
    }
    inline const char * ProtectionPairObject::GetExecutionStateAttrName(void)
    {   
        return m_pExecutionStateAttrName;
    }
    inline const char * ProtectionPairObject::GetAutoResyncStartTimeAttrName(void)
    {   
        return m_pAutoResyncStartTimeAttrName;
    }
    inline const char * ProtectionPairObject::GetQuasiDiffEndtimeAttrName(void)
    {   
        return m_pQuasiDiffEndtimeAttrName;
    }
    inline const char * ProtectionPairObject::GetQuasiDiffStarttimeAttrName(void)
    {   
        return m_pQuasiDiffStarttimeAttrName;
    }
    
    inline const char * ProtectionPairObject::GetFullSyncBytesSentAttrName(void)
    {
        return m_pFullSyncBytesSentAttrName;
    }

    inline const char * ProtectionPairObject::GetAutoResumeAttrName(void)
    {
        return m_pAutoResumeAttrName;
    }

    inline const char * ProtectionPairObject::GetName(void)
    {
        return m_pName;
    }
         
    inline const char * ProtectionPairObject::GetIdAttrName(void)
    {
        return m_pIdAttrName;
    }

    inline const char * ProtectionPairObject::GetSrcNameAttrName(void)
    {
        return m_pSrcNameAttrName;
    }

    inline const char * ProtectionPairObject::GetTgtNameAttrName(void)
    {
        return m_pTgtNameAttrName;
    }

    inline const char * ProtectionPairObject::GetReplStateAttrName(void)
    {
        return m_pReplStateAttrName;
    }
    inline const char * ProtectionPairObject::GetResyncStartTagTimeAttrName(void)
    {
        return m_pResyncStartTagTimeAttrName;
    }
    inline const char * ProtectionPairObject::GetResyncEndTagTimeAttrName(void)
    {
        return m_pResyncEndTagTimeAttrName;
    }
    inline const char * ProtectionPairObject::GetResyncSetTimeStampAttrName(void)
    {
        return m_pResyncSetTimeStampAttrName;
    }
    inline const char * ProtectionPairObject::GetPairStateAttrName(void)
    {
        return m_pPairStateAttrName;
    }
    inline const char * ProtectionPairObject::GetCurrentRPOTimeAttrName(void)
    {
        return m_pCurrentRPOTimeAttrName;
    }
    inline const char * ProtectionPairObject::GetResyncStartSeqAttrName(void)
    {
        return m_pResyncStartSeqAttrName;
    }
    inline const char * ProtectionPairObject::GetResyncEndSeqAttrName(void)
    {
        return m_pResyncEndSeqAttrName;
    }
    inline const char * ProtectionPairObject::GetResyncCounterAttrName(void)
    {
        return m_pResyncCounterAttrName;
    }
    inline const char * ProtectionPairObject::GetShouldResyncAttrName(void)
    {
        return m_pShouldResyncAttrName;
    }
    inline const char * ProtectionPairObject::GetLastOffsetAttrName(void)
    {
        return m_pLastOffsetAttrName;
    }
	inline const char * ProtectionPairObject::GetFsUnusedBytes(void)
    {
        return m_pFsUnusedBytes;
    }
	
    inline const char * ProtectionPairObject::GetShouldResyncSetFromAttrName(void)
    {
        return m_pShouldResyncSetFromAttrName;
    }
    inline const char * ProtectionPairObject::GetResyncSetCXtimeStampAttrName(void)
    {
        return m_pResyncSetCXtimeStamp;
    }
    inline const char * ProtectionPairObject::GetLogsFromAttrName(void)
    {
        return m_pLogsFromAttrName;
    }
    inline const char * ProtectionPairObject::GetLogsFromUTCAttrName(void)
    {
        return m_pLogsFromUTCAttrName;
    }
    inline const char * ProtectionPairObject::GetLogsToAttrName(void)
    {
        return m_pLogsToAttrName;
    }
    inline const char * ProtectionPairObject::GetLogsTOUTCAttrName(void)
    {
        return m_pLogsTOUTCAttrName;
    }
	inline const char * ProtectionPairObject::GetRepositoryNameAttrName(void)
	{
		return m_pRepositoryNameAttrName;
	}
    inline const char * ProtectionPairObject::GetResyncStartTime(void)
    {
        return m_pResyncStartTime ;
    }
    inline const char * ProtectionPairObject::GetResyncEndTime(void)
    {
        return m_pResyncEndTime ;
    }
    inline const char * ProtectionPairObject::GetVolumeResizedAttrName(void)
    {
        return m_pVolumeResize ;
    }
	inline const char * ProtectionPairObject::GetVolumeResizedActionCountAttrName(void)
    {
        return m_pVolumeResizeActionCount ;
    }
    inline const char * ProtectionPairObject::GetResizePolicyTSAttrName(void)
    {
        return m_pResizePolicyTimeStamp ;
    }
    inline const char * ProtectionPairObject::GetSrcVolCapacityAttrName(void)
    {
        return m_pSourceVolumeCapacity ;
    }
    inline const char * ProtectionPairObject::GetSrcVolRawSizeAttrName(void)
    {
        return m_pSourceVolumeRawSize ;
    }
	inline const char * ProtectionPairObject::GetMarkedForDeletionAttrName(void)
    {   
        return m_pMarkedForDeletion;
    }
	inline const char * ProtectionPairObject::GetResumeTrackingByService(void)
	{
		return m_pResumeTrackingByService ;
	}
	inline const char * ProtectionPairObject::GetIsValidRPOAttrName(void)
	{	
		return m_pValidRPO ;
	}
}

#endif /* PROTECTIONPAIR__OBJECT__H__ */