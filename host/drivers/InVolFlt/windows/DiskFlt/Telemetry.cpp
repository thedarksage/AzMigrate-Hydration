
#include "Common.h" 
#include "FltFunc.h"
#include "VContext.h"
#include "misc.h"


void InDskFltGetDiskCaptureAndWOState(PDEV_CONTEXT pDevContext, PULONGLONG pCaptureWOState)
{
    ULONGLONG ullCaptureWOState = *pCaptureWOState;

    ullCaptureWOState |= pDevContext->ePrevWOState;
    ullCaptureWOState = ullCaptureWOState << 2;
    ullCaptureWOState |= pDevContext->eWOState;
    ullCaptureWOState = ullCaptureWOState << 2;
    ullCaptureWOState |= pDevContext->eCaptureMode;

    SET_FLAG(ullCaptureWOState, DBS_FLAGS_SET_BY_DRIVER);

    *pCaptureWOState = ullCaptureWOState;
}

void InDskFltGetDiskBlendedState(
    _In_     PDEV_CONTEXT            pDevContext,
    _In_     ULONGLONG               ullTagState,
    _In_     etTagStateTriggerReason tagStateReason,
    _Out_    PULONGLONG              pDiskBlendedState
    )
    /*
    Description : This function retrieve the various flags across various driver structures to get the cumulative
    status for easy querying on the telemetry and this value is retrieves in all cases

    Arguments :
    pDevContext       -  Pointer to Per - Disk Device Context
    ullTagState       -  Committed/Revoked/Dropped
    tagStateReason    -  This is the trigger reason for the current tag state
    pDiskBlendedState -  Cumulative value

    IRQL requirement : The caller is expected to take spinlock except the ecTagStatusIOCTLFailure scenario 
    where the values are not per-disk. In this case, lock is optional.

    Return Value : None
    */
{
    if (NULL == pDiskBlendedState)
        return;

    ULONGLONG ullDiskBlendedState = *pDiskBlendedState;

    SET_FLAG(ullDiskBlendedState, DBS_FLAGS_SET_BY_DRIVER);

    if (ecServiceRunning != DriverContext.eServiceState)
        SET_FLAG(ullDiskBlendedState, DBS_SERVICE_STOPPED);

    if (NULL == DriverContext.ProcessStartIrp)
        SET_FLAG(ullDiskBlendedState, DBS_S2_STOPPED);

    if (NoRebootMode == DriverContext.eDiskFilterMode)
        SET_FLAG(ullDiskBlendedState, DBS_DRIVER_NOREBOOT_MODE);

    if ((DriverContext.ulMaxNonPagedPool) && (DriverContext.lNonPagedMemoryAllocated >= 0)) {
        if ((ULONG)DriverContext.lNonPagedMemoryAllocated > DriverContext.ulMaxNonPagedPool) {
            SET_FLAG(ullDiskBlendedState, DBS_MAX_NONPAGED_POOL_LIMIT_HIT);
        }
    }

    // This could be Low NP Memory condition or Low Memory condition
    if (1 == DriverContext.bLowMemoryCondition) {
        SET_FLAG(ullDiskBlendedState, DBS_LOW_MEMORY_CONDITION);
    }

    if (NULL != pDevContext) {
        // Set diff sync throttle state
        if (MAXLONGLONG == pDevContext->replicationStats.llDiffSyncThrottleEndTime)
            SET_FLAG(ullDiskBlendedState, DBS_DIFF_SYNC_THROTTLE);

        if (true == pDevContext->bResyncRequired)
            SET_FLAG(ullDiskBlendedState, DBS_DRIVER_RESYNC_REQUIRED);

        if (TEST_FLAG(pDevContext->DiskTelemetryInfo.ullDiskBlendedState, DBS_FILTERING_STOPPED_BY_KERNEL)) {
            SET_FLAG(ullDiskBlendedState, DBS_FILTERING_STOPPED_BY_KERNEL);
        }

        if (TEST_FLAG(pDevContext->DiskTelemetryInfo.ullDiskBlendedState, DBS_FILTERING_STOPPED_BY_USER)) {
            SET_FLAG(ullDiskBlendedState, DBS_FILTERING_STOPPED_BY_USER);
        }

        if (ecTagStatusDropped == ullTagState) {
            if (ecBitmapWrite == tagStateReason) {
                SET_FLAG(ullDiskBlendedState, DBS_BITMAP_WRITE);
            }
            else if (ecFilteringStopped == tagStateReason) {
                SET_FLAG(ullDiskBlendedState, DBS_FILTERING_STOPPED);
            }
            else if (ecClearDiffs == tagStateReason) {
                SET_FLAG(ullDiskBlendedState, DBS_CLEAR_DIFFERENTIALS);
            }
            else if (ecNonPagedPoolLimitHitMDMode == tagStateReason) {
                SET_FLAG(ullDiskBlendedState, DBS_NPPOOL_LIMIT_HIT_MD_MODE);
            }
        }

        if (ecTagStatusRevoked == ullTagState) {
            if (ecRevokeTimeOut == tagStateReason) {
                SET_FLAG(ullDiskBlendedState, DBS_TAG_REVOKE_TIMEOUT);
            } 
            else if (ecRevokeCancelIrpRoutine == tagStateReason) {
                SET_FLAG(ullDiskBlendedState, DBS_TAG_REVOKE_CANCELIRP);
            } 
            else if (ecRevokeCommitIOCTL == tagStateReason) {
                SET_FLAG(ullDiskBlendedState, DBS_TAG_REVOKE_COMMITIOCTL);
            } 
            else if (ecRevokeLocalCrash == tagStateReason) {
                SET_FLAG(ullDiskBlendedState, DBS_TAG_REVOKE_LOCALCC);
            }
            else if (ecRevokeDistrubutedCrashCleanupTag == tagStateReason) {
                SET_FLAG(ullDiskBlendedState, DBS_TAG_REVOKE_DCCLEANUPTAG);
            }
            else if (ecRevokeDistrubutedCrashInsertTag == tagStateReason) {
                SET_FLAG(ullDiskBlendedState, DBS_TAG_REVOKE_DCINSERTIOCTL);
            }
            else if (ecRevokeDistrubutedCrashReleaseTag == tagStateReason) {
                SET_FLAG(ullDiskBlendedState, DBS_TAG_REVOKE_DCRELEASEIOCTL);
            } 
            else if (ecRevokeAppTagInsertIOCTL == tagStateReason) {
                SET_FLAG(ullDiskBlendedState, DBS_TAG_REVOKE_APPINSERTIOCTL);
            }

        }
    }

    *pDiskBlendedState = ullDiskBlendedState;
    return;
}

void InDskFltTelemetryLogTagHistory(
_In_     PKDIRTY_BLOCK pDirtyBlock,
_Inout_  PDEV_CONTEXT  pDevContext,
_In_     ULONGLONG     ullTagState,
_In_     etTagStateTriggerReason tagStateReason,
_In_     ULONG         MessageType
)
    /*
    Description : This routine logs the telemetry data as part of CommitDB/Dropped/Revoked after the
    tag is successfully inserted.
    For tag committed case, Compare stats are the live stats from device context
    For tag dropped/revoked case, Compare starts are the last successfull stats captured in device context

    Arguments :
    pDirtyBlock -  Pointer to the Tag DirtyBlock
    pDevContext -  Pointer to Per - Disk Device Context
    ullTagState -  Committed/Revoked/Dropped

    IRQL requirement : The caller is expected to take spinlock

    Return Value : None
    */
{
    if ((NULL == pDirtyBlock) || (NULL == pDevContext)) {
        return;
    }

    PTAG_HISTORY pTagHistory = pDirtyBlock->pTagHistory;

    if (NULL == pTagHistory)
        return;

    SetMessageTypeFeature(&MessageType);

    ULONGLONG ullDiskBlendedState = 0;
    InDskFltGetDiskBlendedState(pDevContext, ullTagState, tagStateReason, &ullDiskBlendedState);

    ULONGLONG ullCaptureWOState = 0;
    InDskFltGetDiskCaptureAndWOState(pDevContext, &ullCaptureWOState);

    ++pDevContext->ullGroupingID;

    InDskFltWriteEvent(INDSKFLT_INFO_TELEMETRYLOG_TAG_INSERT,
        pTagHistory->liTagRequestTime.QuadPart,
        pTagHistory->TagType,
        pTagHistory->TagMarkerGUID,
        pTagHistory->usTotalNumDisk,
        pTagHistory->usNumProtectedDisk,
        (USHORT)pTagHistory->ulNumberOfDevicesTagsApplied,
        pTagHistory->ulIoCtlCode, //IOCTLcode single..set in the caller
        pTagHistory->TagStatus,
        (ULONG)STATUS_NOT_SUPPORTED,
        pDevContext->ullLastCommittedTagTimeStamp,
        pDevContext->ullLastCommittedTagSequeneNumber,
        pDevContext->LastCommittedTimeStamp,
        pDevContext->LastCommittedSequenceNumber,
        pDirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
        pDirtyBlock->TagTimeStampOfLastChange.ullSequenceNumber,
        pTagHistory->liTagInsertSystemTimeStamp.QuadPart,
        pTagHistory->liTagCompleteTimeStamp.QuadPart, //revoke/commitDB
        DriverContext.timeJumpInfo.llExpectedTime,
        DriverContext.timeJumpInfo.llCurrentTime,
        pDevContext->wDevID,
        ullDiskBlendedState,
        ullTagState,
        pDevContext,
        pDevContext->ullGroupingID,
        false);

    PTAG_DISK_REPLICATION_STATS pCurrentTagInsertStats;
    pCurrentTagInsertStats = &pTagHistory->CurrentTagInsertStats;

    InDskFltWriteEvent(INDSKFLT_INFO_TELEMETRYLOG_CURRENT_SET,
        pCurrentTagInsertStats->ullTotalChangesPending,
        pCurrentTagInsertStats->ullTrackedBytes,
        pCurrentTagInsertStats->ullDrainDBCount,
        pCurrentTagInsertStats->ullRevertedDBCount,
        pCurrentTagInsertStats->ullCommitDBCount,
        pCurrentTagInsertStats->ullDrainedDataInBytes,
        pCurrentTagInsertStats->NwLatBckt1,
        pCurrentTagInsertStats->NwLatBckt2,
        pCurrentTagInsertStats->NwLatBckt3,
        pCurrentTagInsertStats->NwLatBckt4,
        pCurrentTagInsertStats->NwLatBckt5,
        pCurrentTagInsertStats->ullCommitDBFailCount,
        pDevContext,
        pDevContext->ullGroupingID,
        false);

    PTAG_DISK_REPLICATION_STATS pPrevTagInsertStats;
    pPrevTagInsertStats = &pTagHistory->PrevTagInsertStats;

    // Fill the Prev Metrics from Dev Context 
    InDskFltWriteEvent(INDSKFLT_INFO_TELEMETRYLOG_PREVIOUS_SET,
        pTagHistory->liLastTagInsertSystemTime.QuadPart,
        pPrevTagInsertStats->ullTotalChangesPending,
        pPrevTagInsertStats->ullTrackedBytes,
        pPrevTagInsertStats->ullDrainDBCount,
        pPrevTagInsertStats->ullRevertedDBCount,
        pPrevTagInsertStats->ullCommitDBCount,
        pPrevTagInsertStats->ullDrainedDataInBytes,
        pPrevTagInsertStats->NwLatBckt1,
        pPrevTagInsertStats->NwLatBckt2,
        pPrevTagInsertStats->NwLatBckt3,
        pPrevTagInsertStats->NwLatBckt4,
        pPrevTagInsertStats->NwLatBckt5,
        pPrevTagInsertStats->ullCommitDBFailCount,
        pDevContext,
        pDevContext->ullGroupingID,
        false);

    InDskFltWriteEvent(INDSKFLT_INFO_TELEMETRYLOG_GENERIC_SET2,
        pTagHistory->liLastSuccessInsertSystemTime.QuadPart,
        ullCaptureWOState,
        pDevContext->lNumChangeToMetaDataWOState,
        pDevContext->lNumChangeToMetaDataWOStateOnUserRequest,
        pDevContext->lNumChangeToBitmapWOState,
        pDevContext->lNumChangeToBitmapWOStateOnUserRequest,
        pDevContext->DiskTelemetryInfo.LastWOSTime.QuadPart,
        pDevContext->ChangeList.ullcbMetaDataChangesPending,
        pDevContext->replicationStats.llDiffSyncThrottleStartTime,
        pDevContext->replicationStats.llDiffSyncThrottleEndTime,
        pDirtyBlock->pTagHistory->FirstGetDbTimeOnDrainBlk.QuadPart,
        0,
        0,
        pDevContext->llIoCounterNonPassiveLevel,
        ((pDevContext->IoCounterWithPagingIrpSet) + (pDevContext->IoCounterWithSyncPagingIrpSet)),
        pDevContext->IoCounterWithNullFileObject,
        pDevContext,
        pDevContext->ullGroupingID,
        false);

    TAG_DISK_REPLICATION_STATS StatsOnSuccessfulCommitDB = { 0 };

    // For committed DB, Need current stats from dev context
    // Get Last success stats in case of tag revoke and drop
    if (ecTagStatusTagCommitDBSuccess == ullTagState) {
        GetTagDiskReplicationStatsFromDevContext(&StatsOnSuccessfulCommitDB, pDevContext);
    }
    else {
        StatsOnSuccessfulCommitDB = pTagHistory->TagInsertSuccessStats;
    }

    PNON_WOSTATE_STATS pLastNonWOSTransitionStats = &pDevContext->DiskTelemetryInfo.LastNonWOSTransitionStats;
    InDskFltWriteEvent(INDSKFLT_INFO_TELEMETRYLOG_NONWOS_TRANSITION,
        pLastNonWOSTransitionStats->eWOSChangeReason,
        pLastNonWOSTransitionStats->eMetaDataModeReason,
        pLastNonWOSTransitionStats->ullDiskBlendedState,
        pLastNonWOSTransitionStats->lNonPagedMemoryAllocated,
        pLastNonWOSTransitionStats->liNonPagedLimitReachedTSforLastTime.QuadPart,
        pLastNonWOSTransitionStats->ulNonPagedPoolAllocFail,
        pLastNonWOSTransitionStats->ulMemAllocated,
        pLastNonWOSTransitionStats->ulMemReserved,
        pLastNonWOSTransitionStats->ulMemFree,
        pLastNonWOSTransitionStats->ulDataBlocksInFreeDataBlockList,
        pLastNonWOSTransitionStats->ulDataBlocksInLockedDataBlockList,
        pLastNonWOSTransitionStats->ulMaxLockedDataBlockCounter,
        pLastNonWOSTransitionStats->eNewWOState,
        pLastNonWOSTransitionStats->ulNumSecondsInWOS,
        pLastNonWOSTransitionStats->liWOstateChangeTS.QuadPart,
        pDevContext,
        pDevContext->ullGroupingID,
        false);

    InDskFltWriteEvent(INDSKFLT_INFO_TELEMETRYLOG_SUCCESS_SET,
        StatsOnSuccessfulCommitDB.ullTotalChangesPending,
        StatsOnSuccessfulCommitDB.ullTrackedBytes,
        StatsOnSuccessfulCommitDB.ullDrainDBCount,
        StatsOnSuccessfulCommitDB.ullRevertedDBCount,
        StatsOnSuccessfulCommitDB.ullCommitDBCount,
        StatsOnSuccessfulCommitDB.ullDrainedDataInBytes,
        StatsOnSuccessfulCommitDB.NwLatBckt1,
        StatsOnSuccessfulCommitDB.NwLatBckt2,
        StatsOnSuccessfulCommitDB.NwLatBckt3,
        StatsOnSuccessfulCommitDB.NwLatBckt4,
        StatsOnSuccessfulCommitDB.NwLatBckt5,
        StatsOnSuccessfulCommitDB.ullCommitDBFailCount,
        pDevContext,
        pDevContext->ullGroupingID,
        false);

    //telemetry enhancements added as part of 1708(9.12)
    InDskFltWriteEvent(INDSKFLT_INFO_TELEMETRYLOG_GENERIC_SET3,
        DriverContext.ullDrvProdVer,                                  // DrvProdVer
        pDevContext->ullCounterTSODrained,                            // DrainedTSOCount
        pDevContext->ullTotalIoCount,                                 // TrackedChurnCount
        pDevContext->licbReturnedToServiceInDataMode.QuadPart,        // DrainedDataInDMCurr
        pDevContext->ullcbDataAlocMissed,                             // DataAllocMissedInBytes
        pCurrentTagInsertStats->ullTotalCommitLatencyInDM,            // TotalCommitLatencyInDMCurr
        StatsOnSuccessfulCommitDB.ullTotalCommitLatencyInDM,          // TotalCommitLatencyInDMCmp
        pDevContext->ullTotalCommitLatencyInNonDataMode,              // TotalCommitLatencyInNonDM
        pDevContext,
        pDevContext->ullGroupingID,
        false);

    InDskFltWriteEvent(INDSKFLT_INFO_TELEMETRYLOG_GENERIC_SET1,
        (pDevContext->ulOutOfSyncErrorCode) ? (pDevContext->ulOutOfSyncErrorCode) : (pDevContext->ulLastOutOfSyncErrorCode),
        (pDevContext->liOutOfSyncTimeStamp.QuadPart) ? (pDevContext->liOutOfSyncTimeStamp.QuadPart) : (pDevContext->liLastOutOfSyncTimeStamp.QuadPart),
        pDevContext->liResyncStartTimeStamp.QuadPart,
        pDevContext->liResyncEndTimeStamp.QuadPart,
        pDevContext->liClearDiffsTimeStamp.QuadPart,
        pDevContext->liStartFilteringTimeStamp.QuadPart,
        pDevContext->liDevContextCreationTS.QuadPart,
        pDevContext->liGetDirtyBlockTimeStamp.QuadPart,
        pDevContext->liCommitDirtyBlockTimeStamp.QuadPart,
        DriverContext.globalTelemetryInfo.liDriverLoadTime.QuadPart,
        pDevContext->ulFlags,
        DriverContext.globalTelemetryInfo.liLastS2StartTime.QuadPart,
        DriverContext.globalTelemetryInfo.liLastS2StopTime.QuadPart,
        DriverContext.globalTelemetryInfo.liLastAgentStartTime.QuadPart,
        DriverContext.globalTelemetryInfo.liLastAgentStopTime.QuadPart,
        DriverContext.globalTelemetryInfo.ullMemAllocFailCount,
        MessageType,
        pDevContext,
        pDevContext->ullGroupingID,
        true);

    return;
}

void InDskFltTelemetryInsertTagFailure(
    _Inout_   PDEV_CONTEXT           pDevContext,
    _In_      PTAG_TELEMETRY_COMMON  pTagCommonAttribs,
    _In_     ULONG         MessageType
)
/*
Description : This routine logs the telemetry data in case of Tag insert failure

Arguments :
DevContext - Pointer to Per-Disk Device Context, Previous Stats should be filled with Current Stats
pTagCommonAttribs - Pointer to common attributes got from the various Consistency Tag IOCTLs

IRQL requirement : The caller is expected to take spinlock

Return Value : None

*/
{
    if (NULL == pDevContext) {
        return;
    }

    ++pDevContext->ullGroupingID;

    ULONGLONG ullDiskBlendedState = 0;
    InDskFltGetDiskBlendedState(pDevContext, pTagCommonAttribs->ullTagState, ecNotApplicable, &ullDiskBlendedState);

    ULONGLONG ullCaptureWOState = 0;
    InDskFltGetDiskCaptureAndWOState(pDevContext, &ullCaptureWOState);

    SetMessageTypeFeature(&MessageType);

    InDskFltWriteEvent(INDSKFLT_INFO_TELEMETRYLOG_INSERT_FAILURE,
        pTagCommonAttribs->liTagRequestTime.QuadPart,
        pTagCommonAttribs->TagType,
        pTagCommonAttribs->TagMarkerGUID,
        pTagCommonAttribs->usTotalNumDisk,
        pTagCommonAttribs->usNumProtectedDisk,
        (USHORT)pTagCommonAttribs->ulNumberOfDevicesTagsApplied,
        pTagCommonAttribs->ulIoCtlCode, //IOCTLcode single..set in the caller
        pTagCommonAttribs->TagStatus,
        pTagCommonAttribs->GlobaIOCTLStatus,
        pDevContext->ullLastCommittedTagTimeStamp,
        pDevContext->ullLastCommittedTagSequeneNumber,
        pTagCommonAttribs->liTagInsertSystemTimeStamp.QuadPart,
        DriverContext.timeJumpInfo.llExpectedTime,
        DriverContext.timeJumpInfo.llCurrentTime,
        pDevContext->wDevID,
        ullDiskBlendedState,
        pTagCommonAttribs->ullTagState,
        pDevContext,
        pDevContext->ullGroupingID,
        false);

    TAG_DISK_REPLICATION_STATS CurrentTagInsertStats;
    PTAG_DISK_REPLICATION_STATS pPrevTagInsertStats;
    PTAG_DISK_REPLICATION_STATS pTagInsertSuccessStats;

    pPrevTagInsertStats = &pDevContext->DiskTelemetryInfo.PrevTagInsertStats;

    // Fill the Prev Metrics from Dev Context 
    InDskFltWriteEvent(INDSKFLT_INFO_TELEMETRYLOG_PREVIOUS_SET,
        pDevContext->DiskTelemetryInfo.liLastTagInsertSystemTime.QuadPart,
        pPrevTagInsertStats->ullTotalChangesPending,
        pPrevTagInsertStats->ullTrackedBytes,
        pPrevTagInsertStats->ullDrainDBCount,
        pPrevTagInsertStats->ullRevertedDBCount,
        pPrevTagInsertStats->ullCommitDBCount,
        pPrevTagInsertStats->ullDrainedDataInBytes,
        pPrevTagInsertStats->NwLatBckt1,
        pPrevTagInsertStats->NwLatBckt2,
        pPrevTagInsertStats->NwLatBckt3,
        pPrevTagInsertStats->NwLatBckt4,
        pPrevTagInsertStats->NwLatBckt5,
        pPrevTagInsertStats->ullCommitDBFailCount,
        pDevContext,
        pDevContext->ullGroupingID,
        false);

    GetTagDiskReplicationStatsFromDevContext(&CurrentTagInsertStats, pDevContext);
    // Fill the Current Metrics from the CurrentTagInsertStats fields
    InDskFltWriteEvent(INDSKFLT_INFO_TELEMETRYLOG_CURRENT_SET,
        CurrentTagInsertStats.ullTotalChangesPending,
        CurrentTagInsertStats.ullTrackedBytes,
        CurrentTagInsertStats.ullDrainDBCount,
        CurrentTagInsertStats.ullRevertedDBCount,
        CurrentTagInsertStats.ullCommitDBCount,
        CurrentTagInsertStats.ullDrainedDataInBytes,
        CurrentTagInsertStats.NwLatBckt1,
        CurrentTagInsertStats.NwLatBckt2,
        CurrentTagInsertStats.NwLatBckt3,
        CurrentTagInsertStats.NwLatBckt4,
        CurrentTagInsertStats.NwLatBckt5,
        CurrentTagInsertStats.ullCommitDBFailCount,
        pDevContext,
        pDevContext->ullGroupingID,
        false);

    pTagInsertSuccessStats = &pDevContext->DiskTelemetryInfo.TagInsertSuccessStats;

    InDskFltWriteEvent(INDSKFLT_INFO_TELEMETRYLOG_SUCCESS_SET,
        pTagInsertSuccessStats->ullTotalChangesPending,
        pTagInsertSuccessStats->ullTrackedBytes,
        pTagInsertSuccessStats->ullDrainDBCount,
        pTagInsertSuccessStats->ullRevertedDBCount,
        pTagInsertSuccessStats->ullCommitDBCount,
        pTagInsertSuccessStats->ullDrainedDataInBytes,
        pTagInsertSuccessStats->NwLatBckt1,
        pTagInsertSuccessStats->NwLatBckt2,
        pTagInsertSuccessStats->NwLatBckt3,
        pTagInsertSuccessStats->NwLatBckt4,
        pTagInsertSuccessStats->NwLatBckt5,
        pTagInsertSuccessStats->ullCommitDBFailCount,
        pDevContext,
        pDevContext->ullGroupingID,
        false);


    InDskFltWriteEvent(INDSKFLT_INFO_TELEMETRYLOG_GENERIC_SET2,
        pDevContext->DiskTelemetryInfo.liLastSuccessInsertSystemTime.QuadPart,
        ullCaptureWOState,
        pDevContext->lNumChangeToMetaDataWOState,
        pDevContext->lNumChangeToMetaDataWOStateOnUserRequest,
        pDevContext->lNumChangeToBitmapWOState,
        pDevContext->lNumChangeToBitmapWOStateOnUserRequest,
        pDevContext->DiskTelemetryInfo.LastWOSTime.QuadPart,
        pDevContext->ChangeList.ullcbMetaDataChangesPending,
        pDevContext->replicationStats.llDiffSyncThrottleStartTime,
        pDevContext->replicationStats.llDiffSyncThrottleEndTime,
        0, // This field is not applicable as tag is not inserted
        0,
        0,
        pDevContext->llIoCounterNonPassiveLevel,
        ((pDevContext->IoCounterWithPagingIrpSet) + (pDevContext->IoCounterWithSyncPagingIrpSet)),
        pDevContext->IoCounterWithNullFileObject,
        pDevContext,
        pDevContext->ullGroupingID,
        false);

    //telemetry enhancements added as part of 1708(9.12)
    InDskFltWriteEvent(INDSKFLT_INFO_TELEMETRYLOG_GENERIC_SET3,
        DriverContext.ullDrvProdVer,                                  // DrvProdVer
        pDevContext->ullCounterTSODrained,                            // DrainedTSOCount
        pDevContext->ullTotalIoCount,                                 // TrackedChurnCount
        pDevContext->licbReturnedToServiceInDataMode.QuadPart,        // DrainedDataInDMCurr
        pDevContext->ullcbDataAlocMissed,                             // DataAllocMissedInBytes
        CurrentTagInsertStats.ullTotalCommitLatencyInDM,              // TotalCommitLatencyInDMCurr
        pTagInsertSuccessStats->ullTotalCommitLatencyInDM,            // TotalCommitLatencyInDMCmp
        pDevContext->ullTotalCommitLatencyInNonDataMode,              // TotalCommitLatencyInNonDM
        pDevContext,
        pDevContext->ullGroupingID,
        false);


    PNON_WOSTATE_STATS pLastNonWOSTransitionStats = &pDevContext->DiskTelemetryInfo.LastNonWOSTransitionStats;

    InDskFltWriteEvent(INDSKFLT_INFO_TELEMETRYLOG_NONWOS_TRANSITION,
        pLastNonWOSTransitionStats->eWOSChangeReason,
        pLastNonWOSTransitionStats->eMetaDataModeReason,
        pLastNonWOSTransitionStats->ullDiskBlendedState,
        pLastNonWOSTransitionStats->lNonPagedMemoryAllocated,
        pLastNonWOSTransitionStats->liNonPagedLimitReachedTSforLastTime.QuadPart,
        pLastNonWOSTransitionStats->ulNonPagedPoolAllocFail,
        pLastNonWOSTransitionStats->ulMemAllocated,
        pLastNonWOSTransitionStats->ulMemReserved,
        pLastNonWOSTransitionStats->ulMemFree,
        pLastNonWOSTransitionStats->ulDataBlocksInFreeDataBlockList,
        pLastNonWOSTransitionStats->ulDataBlocksInLockedDataBlockList,
        pLastNonWOSTransitionStats->ulMaxLockedDataBlockCounter,
        pLastNonWOSTransitionStats->eNewWOState,
        pLastNonWOSTransitionStats->ulNumSecondsInWOS,
        pLastNonWOSTransitionStats->liWOstateChangeTS.QuadPart,
        pDevContext,
        pDevContext->ullGroupingID,
        false);

    InDskFltWriteEvent(INDSKFLT_INFO_TELEMETRYLOG_GENERIC_SET1,
        (pDevContext->ulOutOfSyncErrorCode) ? (pDevContext->ulOutOfSyncErrorCode) : (pDevContext->ulLastOutOfSyncErrorCode),
        (pDevContext->liOutOfSyncTimeStamp.QuadPart) ? (pDevContext->liOutOfSyncTimeStamp.QuadPart) : (pDevContext->liLastOutOfSyncTimeStamp.QuadPart),
        pDevContext->liResyncStartTimeStamp.QuadPart,
        pDevContext->liResyncEndTimeStamp.QuadPart,
        pDevContext->liClearDiffsTimeStamp.QuadPart,
        pDevContext->liStartFilteringTimeStamp.QuadPart,
        pDevContext->liDevContextCreationTS.QuadPart,
        pDevContext->liGetDirtyBlockTimeStamp.QuadPart,
        pDevContext->liCommitDirtyBlockTimeStamp.QuadPart,
        DriverContext.globalTelemetryInfo.liDriverLoadTime.QuadPart,
        pDevContext->ulFlags,
        DriverContext.globalTelemetryInfo.liLastS2StartTime.QuadPart,
        DriverContext.globalTelemetryInfo.liLastS2StopTime.QuadPart,
        DriverContext.globalTelemetryInfo.liLastAgentStartTime.QuadPart,
        DriverContext.globalTelemetryInfo.liLastAgentStopTime.QuadPart,
        DriverContext.globalTelemetryInfo.ullMemAllocFailCount,
        MessageType,
        pDevContext,
        pDevContext->ullGroupingID,
        true);

    pDevContext->DiskTelemetryInfo.PrevTagInsertStats = CurrentTagInsertStats;
    pDevContext->DiskTelemetryInfo.liLastTagInsertSystemTime.QuadPart = pTagCommonAttribs->liTagInsertSystemTimeStamp.QuadPart;

    return;
}

NTSTATUS InDskFltTelemetryFillHistoryOnTagInsert(
    _Out_   PKDIRTY_BLOCK pDirtyBlock,
    _Inout_ PDEV_CONTEXT pDevContext,
    _In_    PTAG_TELEMETRY_COMMON pTagCommonAttribs
)
/*
Description : This function takes the snapshot of various fields from device context at the time of tag insertion and sent to telemetry 
                as part of Commit DB IOCTL

Arguments :
pDirtyBlock - pTagHistory which is part of pDirtyBlock carries the snapshot of the tag common attribs and device context
DevContext - Pointer to Per-Disk Device Context, Previous Stats should be filled with Current Stats
pTagCommonAttribs - Pointer to common attributes got from the various Consistency Tag IOCTLs

Return Value : STATUS_NO_MEMORY - Failed to allocate Non Paged pool
*/
{
    PTAG_HISTORY       pTagHistory = NULL;
    TAG_DISK_REPLICATION_STATS CurrentTagInsertStats;
    NTSTATUS           Status = STATUS_SUCCESS;
    LARGE_INTEGER liTagInsertSystemTimeStamp;

    KeQuerySystemTime(&liTagInsertSystemTimeStamp);

    pTagHistory = (TAG_HISTORY*)ExAllocatePoolWithTag(NonPagedPool, sizeof(TAG_HISTORY), TAG_TELEMETRY_INFO);

    if (NULL == pTagHistory) {
        InDskFltWriteEventWithDevCtxt(INFLTDRV_ERROR_TAGHISTORY_ALLOCATION_FAILED, pDevContext);
        Status = STATUS_NO_MEMORY;
        goto Cleanup;
    } else {
        // This is freed along with Tag buffer whereever it is getting freed
        RtlZeroMemory(pTagHistory, sizeof(TAG_HISTORY));
        pDirtyBlock->pTagHistory = pTagHistory;
    }

    pTagHistory->liTagRequestTime.QuadPart = pTagCommonAttribs->liTagRequestTime.QuadPart;
    pTagHistory->liLastTagInsertSystemTime.QuadPart = pDevContext->DiskTelemetryInfo.liLastTagInsertSystemTime.QuadPart;
    pTagHistory->liTagInsertSystemTimeStamp.QuadPart = liTagInsertSystemTimeStamp.QuadPart;
    pTagHistory->liLastSuccessInsertSystemTime.QuadPart = pDevContext->DiskTelemetryInfo.liLastSuccessInsertSystemTime.QuadPart;

    pTagHistory->TagType = pTagCommonAttribs->TagType;
    RtlCopyMemory(&pTagHistory->TagMarkerGUID[0], &pTagCommonAttribs->TagMarkerGUID[0], GUID_SIZE_IN_CHARS + 1);
    pTagHistory->usNumProtectedDisk = pTagCommonAttribs->usNumProtectedDisk;
    pTagHistory->usTotalNumDisk = pTagCommonAttribs->usTotalNumDisk;
    pTagHistory->ulNumberOfDevicesTagsApplied = pTagCommonAttribs->ulNumberOfDevicesTagsApplied;
    pTagHistory->ulNumberOfDevicesTagsApplied++;
    pTagHistory->TagStatus = STATUS_SUCCESS;
    pTagHistory->ulIoCtlCode = pTagCommonAttribs->ulIoCtlCode;
    pTagHistory->ullTagState = ecTagStatusInsertSuccess;

Cleanup:
    GetTagDiskReplicationStatsFromDevContext(&CurrentTagInsertStats, pDevContext);

    if (NULL != pTagHistory) {
        pTagHistory->CurrentTagInsertStats = CurrentTagInsertStats;
        pTagHistory->PrevTagInsertStats = pDevContext->DiskTelemetryInfo.PrevTagInsertStats;
        // On succesful commmit DB, replace these fields with the Current Metrics picked from the devcontext
        // If the tag is dropped, we have all the data to log
        pTagHistory->TagInsertSuccessStats = pDevContext->DiskTelemetryInfo.TagInsertSuccessStats;
    }
    pDevContext->DiskTelemetryInfo.liLastTagInsertSystemTime.QuadPart = liTagInsertSystemTimeStamp.QuadPart;
    pDevContext->DiskTelemetryInfo.liLastSuccessInsertSystemTime.QuadPart = liTagInsertSystemTimeStamp.QuadPart;

    pDevContext->DiskTelemetryInfo.PrevTagInsertStats = CurrentTagInsertStats;
    pDevContext->DiskTelemetryInfo.TagInsertSuccessStats = CurrentTagInsertStats;

    return Status;
}

void GetTagDiskReplicationStatsFromDevContext(
    _Out_    PTAG_DISK_REPLICATION_STATS pCurrentTagInsertStats,
    _In_     PDEV_CONTEXT pDevContext
)
/*
Description : This routine captures key statistics of the replicated disk for assessing the tag health

Arguments :
pCurrentTagInsertStats - Pointer to the some of the key replication stats structure
DevContext - Pointer to Per-Disk Device Context, Previous Stats should be filled with Current Stats

Return Value : None
*/
{
    if ((NULL == pCurrentTagInsertStats) || (NULL == pDevContext)) {
        return;
    }
    ULONGLONG   ullCommitLatencyDistribution[USER_MODE_NOTIFY_LATENCY_DISTRIBUTION_BUCKETS] = { 0 };
    ULONG       ulValidBucketIndex = TELEMETRY_VALID_COMMIT_LATENCY_BUCKET_INDEX;
    ULONG       ulFirstIndex = 0;

    pCurrentTagInsertStats->ullTotalChangesPending = pDevContext->ChangeList.ullcbTotalChangesPending;
    pCurrentTagInsertStats->ullTrackedBytes = pDevContext->ullTotalTrackedBytes;
    pCurrentTagInsertStats->ullDrainDBCount = pDevContext->DiskTelemetryInfo.ullDrainDBCount;
    pCurrentTagInsertStats->ullRevertedDBCount = pDevContext->DiskTelemetryInfo.ullRevertedDBCount;
    pCurrentTagInsertStats->ullCommitDBCount = pDevContext->DiskTelemetryInfo.ullCommitDBCount;
    pCurrentTagInsertStats->ullDrainedDataInBytes = pDevContext->ulicbReturnedToService.QuadPart;

    //GetDistribution to get the 10 such stats
    //Copy the only required fields to CurrentTagInsertStats
    if (pDevContext->LatencyDistForCommit) {
        pDevContext->LatencyDistForCommit->GetDistribution((PULONGLONG)ullCommitLatencyDistribution, USER_MODE_NOTIFY_LATENCY_DISTRIBUTION_BUCKETS);
    }

    //Get the appropriate fields from the distribution set
    pCurrentTagInsertStats->NwLatBckt1 = ullCommitLatencyDistribution[ulValidBucketIndex];

    // To avoid compatibility issues with user space programs, set the first 5 buckets to same value and add just first bucket to the 
    // the one where valid bucket starts
    pCurrentTagInsertStats->NwLatBckt1 += ullCommitLatencyDistribution[ulFirstIndex];
    pCurrentTagInsertStats->NwLatBckt2 = ullCommitLatencyDistribution[++ulValidBucketIndex];
    pCurrentTagInsertStats->NwLatBckt3 = ullCommitLatencyDistribution[++ulValidBucketIndex];
    pCurrentTagInsertStats->NwLatBckt4 = ullCommitLatencyDistribution[++ulValidBucketIndex];
    pCurrentTagInsertStats->NwLatBckt5 = ullCommitLatencyDistribution[++ulValidBucketIndex];
    pCurrentTagInsertStats->ullCommitDBFailCount = pDevContext->DiskTelemetryInfo.ullCommitDBFailCount;
    pCurrentTagInsertStats->ullTotalCommitLatencyInDM = pDevContext->ullTotalCommitLatencyInDataMode;

    return;
}

void InDskFltCaptureLastNonWOSTransitionStats(
    _In_     ULONG ulIndex,
    _Inout_  PDEV_CONTEXT pDevContext,
    _In_     etWOSChangeReason eWOSChangeReason,
    _In_     etCModeMetaDataReason eMetaDataModeReason
)
/*
Description : This function captures the various memory stats and history related to WO state right at the time of transition to 
              Non WO state
Arguments :
pDirtyBlock - Pointer to the Tag DirtyBlock
pDevContext - Pointer to Per - Disk Device Context

IRQL requirement : The caller is expected to take spinlock

Return Value : NTSTATUS
*/
{
    ASSERT(NULL != pDevContext);
    if (NULL == pDevContext)
        return;

    PNON_WOSTATE_STATS pLastNonWOStateHistory = &pDevContext->DiskTelemetryInfo.LastNonWOSTransitionStats;

    if (NULL == pLastNonWOStateHistory) {
        return;
    } else {
        pLastNonWOStateHistory->liWOstateChangeTS = pDevContext->liWOstateChangeTS[ulIndex];
        pDevContext->DiskTelemetryInfo.LastWOSTime = pLastNonWOStateHistory->liWOstateChangeTS;
        pLastNonWOStateHistory->ulNumSecondsInWOS = pDevContext->ulNumSecondsInWOS[ulIndex];
        pLastNonWOStateHistory->eOldWOState = pDevContext->eOldWOState[ulIndex];
        pLastNonWOStateHistory->eNewWOState = pDevContext->eNewWOState[ulIndex];
        pLastNonWOStateHistory->ullcbDChangesPendingAtWOSchange = pDevContext->ullcbDChangesPendingAtWOSchange[ulIndex];
        pLastNonWOStateHistory->ullcbMDChangesPendingAtWOSchange = pDevContext->ullcbMDChangesPendingAtWOSchange[ulIndex];
        pLastNonWOStateHistory->eWOSChangeReason = eWOSChangeReason;
        pLastNonWOStateHistory->ulMemAllocated = pDevContext->ulcbDataAllocated;
        pLastNonWOStateHistory->ulMemReserved = pDevContext->ulcbDataReserved;
        pLastNonWOStateHistory->ulMemFree = pDevContext->ulcbDataFree;
        pLastNonWOStateHistory->ulDataBlocksInFreeDataBlockList = DriverContext.ulDataBlocksInFreeDataBlockList;
        pLastNonWOStateHistory->ulDataBlocksInLockedDataBlockList = DriverContext.ulDataBlocksInLockedDataBlockList;
        pLastNonWOStateHistory->ulMaxLockedDataBlockCounter = DriverContext.MaxLockedDataBlockCounter;

        ULONGLONG ullBlendedState = 0;
        InDskFltGetDiskBlendedState(pDevContext, ecTagStatusMaxEnum, ecNotApplicable, &ullBlendedState);

        pLastNonWOStateHistory->ullDiskBlendedState = ullBlendedState;
        pLastNonWOStateHistory->lNonPagedMemoryAllocated = DriverContext.lNonPagedMemoryAllocated;
        pLastNonWOStateHistory->liNonPagedLimitReachedTSforLastTime.QuadPart = DriverContext.liNonPagedLimitReachedTSforLastTime.QuadPart;
        pLastNonWOStateHistory->ulNonPagedPoolAllocFail = pDevContext->ulNumMemoryAllocFailures;
        pLastNonWOStateHistory->eMetaDataModeReason = eMetaDataModeReason;
    }
    return;
}

void InDskFltGetTagMarkerGUID(
    _In_  PUCHAR pTagBuffer,
    _Out_ PCHAR  pTagMarkerGUID,
    _In_  ULONG  guidLength,
    _In_  ULONG  tagInputBufferLength
    )
/*
Description : This function retrives the Tag Marker GUID from the TagBuffer received from vacp

Arguments :
pTagBuffer     -  Pointer to the TagBuffer received as part of IOCTL input buffer and validation of the buffer is sucessful
pTagMarkerGUID -  Pointer to Tag Marker GUID in the TAG_TELEMETRY_COMMON structure
tagInputBufferLength - Tag length of opaque tag buffer
IRQL requirement : None

Return Value : None
*/
{
    ASSERT(NULL != pTagMarkerGUID);
    ASSERT(NULL != pTagBuffer);

    ULONG TagGUIDHdrLength = 0;
    ULONG ulMaxStreamRecHdrLength = sizeof(STREAM_REC_HDR_8B);
    KIRQL   oldIrql = 0;
    ULONG ulMinLength = sizeof(USHORT) + ulMaxStreamRecHdrLength + guidLength;

    if (tagInputBufferLength > ulMinLength) {
        pTagBuffer = pTagBuffer + sizeof(USHORT);
        PSTREAM_REC_HDR_4B pTagGUIDHdr = (PSTREAM_REC_HDR_4B)pTagBuffer;

        if ((pTagGUIDHdr->ucFlags & STREAM_REC_FLAGS_LENGTH_BIT) == STREAM_REC_FLAGS_LENGTH_BIT) {
            TagGUIDHdrLength = sizeof(STREAM_REC_HDR_8B);
        } else {
            TagGUIDHdrLength = sizeof(STREAM_REC_HDR_4B);
        }

        pTagBuffer = pTagBuffer + TagGUIDHdrLength;
        RtlCopyMemory(pTagMarkerGUID, pTagBuffer, guidLength);
        pTagMarkerGUID[guidLength] = '\0';

        // Check if it is failback tag
        // If yes until this IRP is completed no new request can be served for
        // commit tag notification
        KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
        if (NULL != DriverContext.TagNotifyContext.pTagCommitNotifyIrp) {
            if (guidLength == RtlCompareMemory(DriverContext.TagNotifyContext.TagGUID, pTagMarkerGUID, guidLength)) {
                if (!DriverContext.TagNotifyContext.bTimerSet ||
                    (TRUE == KeCancelTimer(&DriverContext.TagNotifyProtocolTimer))) {
                    // If timer has not started, execute this.
                    // check if another validation is in progress
                    if (!DriverContext.TagNotifyContext.isCommitNotifyTagInsertInProcess) {
                        DriverContext.TagNotifyContext.isCommitNotifyTagInsertInProcess = true;
                    }
                    DriverContext.TagNotifyContext.bTimerSet = FALSE;
                }
            }
        }
        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
    }
}

void InDskFltTelemetryTagIOCTLFailure(
    _In_      PTAG_TELEMETRY_COMMON  pTagCommonAttribs,
    _In_      NTSTATUS               Status,
    _In_      ULONG                  MessageType
    )
{
    if (NULL == pTagCommonAttribs)
        return;

    SetMessageTypeFeature(&MessageType);
    ULONGLONG ullDiskBlendedState = 0;
    InDskFltGetDiskBlendedState(NULL, ecTagStatusIOCTLFailure, ecNotApplicable, &ullDiskBlendedState);

    InDskFltWriteEvent(INDSKFLT_INFO_TELEMETRYLOG_IOCTL_FAILURE,
        pTagCommonAttribs->liTagRequestTime.QuadPart,
        pTagCommonAttribs->TagType,
        pTagCommonAttribs->TagMarkerGUID,
        pTagCommonAttribs->usTotalNumDisk,
        pTagCommonAttribs->usNumProtectedDisk,
        (USHORT)pTagCommonAttribs->ulNumberOfDevicesTagsApplied,
        pTagCommonAttribs->ulIoCtlCode,
        Status,
        DriverContext.timeJumpInfo.llExpectedTime,
        DriverContext.timeJumpInfo.llCurrentTime,
        ullDiskBlendedState,
        ecTagStatusIOCTLFailure,
        MessageType,
        &DriverContext,
        ++DriverContext.globalTelemetryInfo.ullGroupingID,
        true);

}

/*
Input Tag Stream Structure for disk devices
_____________________________________________________________________________________________________________________________________________________________
|  Context   |             |          |                |                 |    |              |              |         |          |     |         |           |
| GUID(UCHAR)|   Flags     |timeout   | No. of GUIDs(n)|  Volume GUID1   | ...| Volume GUIDn | Tag Len (k)  |Tag1 Size| Tag1 data| ... |Tagk Size| Tagk Data |
|_(36 Bytes)_|____(4 Bytes)|_(4 Bytes)|_ (4 Bytes)_____|___(36 Bytes)____|____|______________|___(4 bytes)__|(2 bytes)|______________________________________|

*/

bool InDskFltAppGetTagMarkerGUID(
    _In_  PUCHAR pTagBuffer,
    _Out_ PCHAR  pTagMarkerGUID,
    _In_  ULONG  guidLength,
    _In_  ULONG  tagInputBufferLength
    )
/*
Description : This function retrives the Tag Marker GUID from the TagBuffer received from vacp

Arguments :
pTagBuffer           -  Pointer to the TagBuffer received as part of IOCTL input buffer and validation of the buffer is sucessful
pTagMarkerGUID       -  Pointer to Tag Marker GUID in the TAG_TELEMETRY_COMMON structure
guidlength           -  Length of Marker GUID
tagInputBufferLength -  Length of input IOCTL
IRQL requirement : None

Return Value : bool
*/
{
    ULONG  ulBytesParsed = 0;
    ULONG  ulNumberOfDevices = 0;
    ULONG  ulNumberOfTags = 0;
    PUCHAR pTagData;
    ULONG  ultagDataLength = 0;

    if ((NULL == pTagBuffer) || (tagInputBufferLength < (GUID_SIZE_IN_CHARS + (3 * sizeof(ULONG)))))
        return false;
    ulBytesParsed = GUID_SIZE_IN_CHARS + (2 * sizeof(ULONG));
    ulNumberOfDevices = *(PULONG)(pTagBuffer + ulBytesParsed);
    ulBytesParsed += sizeof(ULONG);
    if (0 == ulNumberOfDevices)
        return false;

    if (tagInputBufferLength < (ulBytesParsed + (ulNumberOfDevices * GUID_SIZE_IN_BYTES)))
        return false;
    ulBytesParsed += (ulNumberOfDevices * GUID_SIZE_IN_BYTES);

    if (tagInputBufferLength < ulBytesParsed + sizeof(ULONG))
        return false;
    ulNumberOfTags = *(PULONG)(pTagBuffer + ulBytesParsed);
    ulBytesParsed += sizeof(ULONG);
    if (0 == ulNumberOfTags)
        return false;

    pTagData = pTagBuffer + ulBytesParsed;
    ultagDataLength = tagInputBufferLength - ulBytesParsed;

    InDskFltGetTagMarkerGUID(pTagData, pTagMarkerGUID, guidLength, ultagDataLength);

    return true;
}

inline void SetMessageTypeFeature(
    _Inout_ ULONG *MessageType
    )
/*
Description : This function sets the feature bit to intrepret the existing 
column, if there is any change in functionality of column.

Arguments :
MessageType - Indicates the life cycle of tag and user-kernel tag IOCTL failure
*/
{
    ASSERT(NULL != MessageType);

    *MessageType |= TELEMETRY_WINDOWS_MSG_TYPE;
    *MessageType |= TELEMETRY_FLAGS_COMMIT_LATENCY_BUCKETS;

    return;
}
