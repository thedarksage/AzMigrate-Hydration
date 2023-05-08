#pragma once

#include "global.h"
#include "Profiler.h"
#include "BitmapAPI.h"
#include "DevContext.h"

#ifdef VOLUME_FLT
// These are parameters per volume
#define VOLUME_LETTER                           L"VolumeLetter"                 //REG_STRING
#define DEVICE_FILTERING_DISABLED               L"VolumeFilteringDisabled"      //REG_DWORD
#define DEVICE_BITMAP_READ_DISABLED             L"VolumeBitmapReadDisabled"     //REG_DWORD
#define DEVICE_BITMAP_WRITE_DISABLED            L"VolumeBitmapWriteDisabled"    //REG_DWORD
#define DEVICE_BITMAP_GRANULARITY               L"VolumeBitmapGranularity"      //REG_DWORD
#define DEVICE_BITMAP_OFFSET                    L"VolumeBitmapOffset"           //REG_DWORD
#define DEVICE_RESYNC_REQUIRED                  L"VolumeResyncRequired"
#define DEVICE_OUT_OF_SYNC_COUNT                L"VolumeOutOfSyncCount"
#define DEVICE_OUT_OF_SYNC_TIMESTAMP            L"VolumeOutOfSyncTimestamp"     //FORMAT MM:DD:YYYY::HH:MM:SS:XXX(Milli Seconds)
#define DEVICE_OUT_OF_SYNC_TIMESTAMP_IN_GMT     L"VolumeOutOfSyncTimestampInGMT"     //FORMAT LongLong
#define DEVICE_OUT_OF_SYNC_ERRORCODE            L"VolumeOutOfSyncErrorCode"
#define DEVICE_OUT_OF_SYNC_ERROR_DESCRIPTION    L"VolumeOutOfSyncErrorDescription"
#define DEVICE_OUT_OF_SYNC_ERRORSTATUS          L"VolumeOutOfSyncErrorStatus"
#define DEVICE_BITMAPLOG_PATH_NAME              L"LogPathName"
#define DEVICE_DATAFILELOG_DIRECTORY            L"VolumeDataLogDirectory"
#define DEVICE_READ_ONLY                        L"VolumeReadOnly"
#define DEVICE_FILE_WRITER_ID                   L"VolumeFileWriterId"
#define VOLUME_PAGEFILE_WRITES_IGNORED          L"VolumePagefileWritesIgnored"  //REG_DWORD
#else
//per-device registry keys for disk filter 
#define DEVICE_FILTERING_DISABLED               L"DeviceFilteringDisabled"      //REG_DWORD
#define DEVICE_BITMAP_READ_DISABLED             L"DeviceBitmapReadDisabled"     //REG_DWORD
#define DEVICE_BITMAP_WRITE_DISABLED            L"DeviceBitmapWriteDisabled"    //REG_DWORD

#define DEVICE_BITMAP_GRANULARITY               L"DeviceBitmapGranularity"      //REG_DWORD
#define DEVICE_BITMAP_OFFSET                    L"DeviceBitmapOffset"           //REG_DWORD
#define DEVICE_RESYNC_REQUIRED                  L"DeviceResyncRequired"
#define DEVICE_OUT_OF_SYNC_COUNT                L"DeviceOutOfSyncCount"
#define DEVICE_OUT_OF_SYNC_TIMESTAMP            L"DeviceOutOfSyncTimestamp"     //FORMAT MM:DD:YYYY::HH:MM:SS:XXX(Milli Seconds)
#define DEVICE_OUT_OF_SYNC_TIMESTAMP_IN_GMT     L"DeviceOutOfSyncTimestampInGMT"     //FORMAT LongLong
#define DEVICE_OUT_OF_SYNC_ERRORCODE            L"DeviceOutOfSyncErrorCode"
#define DEVICE_OUT_OF_SYNC_ERROR_DESCRIPTION    L"DeviceOutOfSyncErrorDescription"
#define DEVICE_OUT_OF_SYNC_ERRORSTATUS          L"DeviceOutOfSyncErrorStatus"
#define DEVICE_BITMAPLOG_PATH_NAME              L"BitmapLogPathName"
#define DEVICE_DATAFILELOG_DIRECTORY            L"DeviceDataFileLogDirectory"
#define DEVICE_READ_ONLY                        L"DeviceReadOnly"
#define DEVICE_FILE_WRITER_ID                   L"DeviceFileWriterId"
#define HARDWARE_ID_KEY_NAME                    L"HardwareId"
#define DEVICE_BITMAP_MAX_WRITEGROUPS_IN_HEADER L"DeviceBitmapMaxWriteGroups"         //REG_DWORD
#define DEVICE_BUS_TYPE                         L"BusType"
#endif

#define DEVICE_BITMAP_GRANULARITY_DEFAULT_VALUE         (0x1000)
#define DEVICE_BITMAP_OFFSET_DEFAULT_VALUE              (0)

#define DEVICE_FILTERING_DISABLED_SET                   (1)
#define DEVICE_FILTERING_DISABLED_UNSET                 (0)

#define DEVICE_DATA_CAPTURE_SET                         (1)
#define DEVICE_DATA_CAPTURE_UNSET                       (0)

#define DEVICE_DATA_FILES_SET                           (1)
#define DEVICE_DATA_FILES_UNSET                         (0)
#define DEFAULT_DEVICE_DATA_FILES                       (0)     // FALSE

#define DEVICE_BITMAP_READ_DISABLED_SET                 (1)
#define DEVICE_BITMAP_READ_DISABLED_UNSET               (0)
#define DEVICE_BITMAP_READ_DISABLED_DEFAULT_VALUE       (0)     // FALSE


#define DEVICE_BITMAP_WRITE_DISABLED_SET                (1)
#define DEVICE_BITMAP_WRITE_DISABLED_UNSET              (0)
#define DEVICE_BITMAP_WRITE_DISABLED_DEFAULT_VALUE      (0)     // FALSE

#define VOLUME_PAGEFILE_WRITES_IGNORED_SET              (1)
#define VOLUME_PAGEFILE_WRITES_IGNORED_UNSET            (0)
#define VOLUME_PAGEFILE_WRITES_IGNORED_DEFAULT_VALUE    (1)     // TRUE

#define DEVICE_READ_ONLY_SET                            (1)
#define DEVICE_READ_ONLY_UNSET                          (0)
#define DEVICE_READ_ONLY_DEFAULT_VALUE                  (0)     // FALSE

#define DEFAULT_DEVICE_RESYNC_REQUIRED_VALUE            (0)     // FALSE
#define DEVICE_RESYNC_REQUIRED_SET                      (1)     // TRUE
#define DEVICE_RESYNC_REQUIRED_UNSET                    (0)

#define DEFAULT_DEVICE_OUT_OF_SYNC_COUNT                (0)
#define DEFAULT_DEVICE_OUT_OF_SYNC_ERRORCODE            (0)
#define DEFAULT_DEVICE_OUT_OF_SYNC_ERRORSTATUS          (0)

#define DEFAULT_FS_FLUSH_PRE_SHUTDOWN                   (1)
#define MAX_WRITE_GROUPS_IN_BITMAP_HEADER_DEFAULT_VALUE (31)

BOOLEAN
CanOpenBitmapFile(PDEV_CONTEXT   DevContext, BOOLEAN bLoseChanges);

VOID
SetBitmapOpenFailDueToLossOfChanges(PDEV_CONTEXT DevContext, bool bLockAcquired);

VOID
LogBitmapOpenSuccessEvent(PDEV_CONTEXT DevContext);

VOID
SetDBallocFailureError(PDEV_CONTEXT DevContext);

VOID
ProcessDevContextWorkItems(PWORK_QUEUE_ENTRY pWorkItem);

VOID
FlushAndCloseBitmapFile(PDEV_CONTEXT pDevContext);

bool
IsDataCaptureEnabledForThisDev(PDEV_CONTEXT DevContext);

bool
CanSwitchToDataCaptureMode(PDEV_CONTEXT pDevContext, bool bClearDiff = false);

bool
NeedWritingToDataFile(PDEV_CONTEXT DevContext);

bool
IsDataFilesEnabledForThisDev(PDEV_CONTEXT DevContext);

NTSTATUS
InitializeDataLogDirectory(PDEV_CONTEXT DevContext, PWSTR pDataLogDirectory, bool bCustom = false);

NTSTATUS
VCAddKernelTag(
    PDEV_CONTEXT    DevContext,
    PVOID           pTag,
    USHORT          usTagLen
    );

NTSTATUS
AddUserDefinedTags(
    PDEV_CONTEXT    DevContext,
    PUCHAR          SequenceOfTags,
    PTIME_STAMP_TAG_V2 *ppTimeStamp,
    PLARGE_INTEGER  pliTickCount,
    PGUID           pTagGUID,
    ULONG           ulTagDevIndex
    );


NTSTATUS
AddUserDefinedTagsForDiskDevice(
    __in PDEV_CONTEXT    DevContext,
    __in PUCHAR          SequenceOfTags,
    __in PTAG_TELEMETRY_COMMON pTagCommonAttribs,
    __in bool           isCommitNotifyTagRequest
);

NTSTATUS
AddTagsInDBHeader(
    __in PDEV_CONTEXT    DevContext,
    __in PUCHAR          pSequenceOfTags,
    __in PTAG_TELEMETRY_COMMON pTagCommonAttribs,
    __in bool           isCommitNotifyTagRequest
);
/*-----------------------------------------------------------------------------
 * Functions related to Device context processing
 *-----------------------------------------------------------------------------
 */

BOOLEAN
AddVCWorkItemToList(
    etWorkItem          eWorkItem,
    PDEV_CONTEXT        pDevContext,
    ULONG               ulAdditionalInfo,
    BOOLEAN             bOpenBitmapIfRequired,
    PLIST_ENTRY         ListHead
    );

NTSTATUS
OpenDeviceParametersRegKey(PDEV_CONTEXT pDevContext, Registry **ppVolumeParam);

NTSTATUS
SetDWORDInRegVolumeCfg(PDEV_CONTEXT pDevContext, PWCHAR pValueName, ULONG ulValue, bool bFlush=0);

NTSTATUS
SetStringInRegVolumeCfg(PDEV_CONTEXT pDevContext, PWCHAR pValueName, PUNICODE_STRING pValue);

/*---------------------------------------------------------------------------------------------
 *  Functions releated to Allocate, Reference and Dereference DevContext
 *---------------------------------------------------------------------------------------------
 */
PDEV_CONTEXT
AllocateDevContext();

PDEV_CONTEXT
ReferenceDevContext(PDEV_CONTEXT   pDevContext);

VOID
DereferenceDevContext(PDEV_CONTEXT   pDevContext);

/*-----------------------------------------------------------------------------
 * Functions related to DataBlocks and DataFiltering
 *-----------------------------------------------------------------------------
 */

bool
VCSetWOState(PDEV_CONTEXT DevContext, bool bServiceInitiated, char *FunctionName, etCModeMetaDataReason eMetaDataModeReason);

void
VCSetWOStateToBitmap(PDEV_CONTEXT DevContext, bool bServiceInitiated, etWOSChangeReason eWOSChangeReason, etCModeMetaDataReason eMetaDataModeReason);

void
VCSetCaptureModeToMetadata(PDEV_CONTEXT DevContext, bool bServiceInitiated);

void
VCSetCaptureModeToData(PDEV_CONTEXT DevContext);

VOID
VCAdjustCountersForLostDataBytes(
    PDEV_CONTEXT DevContext,
    PDATA_BLOCK *ppDataBlock,
    ULONG ulBytesLost,
    bool bAllocate = false,
    bool bCanLock = false);

VOID
VCAdjustCountersToUndoReserve(
    PDEV_CONTEXT DevContext,
    ULONG   ulBytes);

PDATA_BLOCK
VCGetReservedDataBlock(PDEV_CONTEXT DevContext);

VOID
VCFreeUsedDataBlockList(PDEV_CONTEXT DevContext, bool bVCLockAcquired);

VOID
VCFreeReservedDataBlockList(PDEV_CONTEXT DevContext);

VOID
VCAddToReservedDataBlockList(PDEV_CONTEXT DevContext, PDATA_BLOCK DataBlock);

VOID
VCFreeAllDataBuffers(PDEV_CONTEXT    DevContext, bool bVCLockAcquired);

VOID
AddDirtyBlockToDevContext(
    PDEV_CONTEXT        DevContext,
    PKDIRTY_BLOCK       DirtyBlock,
    ULONG               ulDataSource,
    bool                bContinuation
    );

VOID
AddDirtyBlockToDevContext(
    PDEV_CONTEXT        DevContext,
    PKDIRTY_BLOCK       DirtyBlock,
    PTIME_STAMP_TAG_V2  *ppTimeStamp,
    PLARGE_INTEGER      pliTickCount,
    PDATA_BLOCK         *ppDataBlock
    );

VOID
AddLastChangeTimestampToCurrentNode(PDEV_CONTEXT DevContext, PDATA_BLOCK *ppDataBlock, bool bAllocate = false);

VOID
PrependChangeNodeToDevContext(PDEV_CONTEXT DevContext, PCHANGE_NODE ChangeNode);

PCHANGE_NODE
GetChangeNodeFromChangeList(PDEV_CONTEXT DevContext, bool bFromHead = true);

PKDIRTY_BLOCK
GetCurrentDirtyBlockFromChangeList(PDEV_CONTEXT DevContext);

VOID
QueueChangeNodesToSaveAsFile(PDEV_CONTEXT DevContext,bool bLockAcquired);

PCHANGE_NODE
GetChangeNodeToSaveAsFile(PDEV_CONTEXT DevContext);

ULONG
CurrentDataSource(PDEV_CONTEXT DevContext);

VOID 
ClearDevStats(PDEV_CONTEXT pDevContext);

VOID
SetBitmapOpenError(PDEV_CONTEXT  DevContext, bool bLockAcquired, NTSTATUS Status);

void
MoveUnupdatedDirtyBlocks(PDEV_CONTEXT pDevContext);

NTSTATUS
GetDeviceContextFieldsFromRegistry(PDEV_CONTEXT pDevContext);

NTSTATUS
PersistFDContextFields(PDEV_CONTEXT pDevContext, LONG *ErrorStatus);

#if DBG
BOOLEAN
ValidateDataWOSTransition(PDEV_CONTEXT pDevContext);
#endif

typedef NTSTATUS    (*DispatchEntry)( IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);


NTSTATUS
SetDevOutOfSync(
    _DEV_CONTEXT      *pDevContext,
    ULONG             ulOutOfSyncErrorCode,
    NTSTATUS          Status,
    bool              bLockAcquired
    );

void
ResetDevOutOfSync(_DEV_CONTEXT *DevContext, bool bClear);

NTSTATUS
QueueWorkerRoutineForSetDevOutOfSync(_DEV_CONTEXT  *pDevContext, ULONG ulOutOfSyncErrorCode, NTSTATUS StatusToLog, bool bLockAcquired);

NTSTATUS
LoadDeviceConfigFromRegistry(PDEV_CONTEXT pDevContext);

VOID
SetDevCntFlag (
    __in    PDEV_CONTEXT pDevContext,
    __in    ULONG        ulFlags,
    __in    bool         bDCLockAcquired
    );

bool
AddDeviceContextToList (
    __in    PDEV_CONTEXT pDevContext,
    __in    ULONG        ulExcludeAnySet,
    __in    ULONG        ulIncludeAllSet,
    __in    PLIST_ENTRY  pListHead,
    __in    bool         bDCLockAcquired,
    __in    bool         bPreShutdown = false
    );

tagLogRecoveryState
CheckResyncDetected (
    __in    PDEV_CONTEXT pDevContext,
    __in    bool         bDCLockAcquired
    );

NTSTATUS
ChangeModeToRawBitmap (
    __in    PDEV_CONTEXT pDevContext,
	__in    LONG         *ErrorStatus
    );

PPAGE_FILE_NODE 
FindPageFileNodeInDevCnt (
    __in    PDEV_CONTEXT pDevContext,
    __in    PFILE_OBJECT pFileObject,
    __in    bool         bDCLockAcquired
    );

void
RemovePageFileObject (
    __in    PDEV_CONTEXT pDevContext,
    __in    PFILE_OBJECT pFileObject
    );

void
AddPageFileObject (
    __in    PDEV_CONTEXT pDevContext,
    __in    PFILE_OBJECT pFileObject
    );

void
FreeAllPageFileNodeToGlobalList (
    __in    PDEV_CONTEXT pDevContext,
    __in    bool         bDCLockAcquired
    );

void
RemoveDeviceContextFromGlobalList(
    __in    PDEV_CONTEXT    pDevContext
    );

void InDskFltCaptureLastNonWOSTransitionStats(
    _In_     ULONG ulIndex, 
    _Inout_  PDEV_CONTEXT DevContext, 
    _In_     etWOSChangeReason eWOSChangeReason,
    _In_     etCModeMetaDataReason eMetaDataModeReason);

void GetTagDiskReplicationStatsFromDevContext(
    _Out_    PTAG_DISK_REPLICATION_STATS pCurrentTagInsertStats,
    _In_     PDEV_CONTEXT pDevContext
);

NTSTATUS InDskFltTelemetryFillHistoryOnTagInsert(
    _Out_   PKDIRTY_BLOCK pDirtyBlock, 
    _Inout_ PDEV_CONTEXT pDevContext, 
    _In_    PTAG_TELEMETRY_COMMON pTagCommonAttribs);

// These flags are added to represent the state of various driver states from global and per-disk context
// for telemetry purposes
#define  DBS_DIFF_SYNC_THROTTLE                           0x0000000000000001
#define  DBS_SERVICE_STOPPED                              0x0000000000000002
#define  DBS_S2_STOPPED                                   0x0000000000000004
#define  DBS_DRIVER_NOREBOOT_MODE                         0x0000000000000008
#define  DBS_DRIVER_RESYNC_REQUIRED                       0x0000000000000010
#define  DBS_FILTERING_STOPPED_BY_USER                    0x0000000000000020
#define  DBS_FILTERING_STOPPED_BY_KERNEL                  0x0000000000000040
#define  DBS_FILTERING_STOPPED                            0x0000000000000080
#define  DBS_CLEAR_DIFFERENTIALS                          0x0000000000000100
#define  DBS_BITMAP_WRITE                                 0x0000000000000200
#define  DBS_NPPOOL_LIMIT_HIT_MD_MODE                     0x0000000000000400
#define  DBS_MAX_NONPAGED_POOL_LIMIT_HIT                  0x0000000000000800
#define  DBS_LOW_MEMORY_CONDITION                         0x0000000000001000
#define  DBS_SPLIT_IO_FAILED                              0x0000000000002000 // Used by Linux Driver
#define  DBS_ORPHAN                                       0x0000000000004000 // Used by Linux Driver
// Reserved Field
#define  DBS_TAG_REVOKE_TIMEOUT                           0x0000000000010000
#define  DBS_TAG_REVOKE_CANCELIRP                         0x0000000000020000
#define  DBS_TAG_REVOKE_COMMITIOCTL                       0x0000000000040000
#define  DBS_TAG_REVOKE_LOCALCC                           0x0000000000080000
#define  DBS_TAG_REVOKE_DCCLEANUPTAG                      0x0000000000100000
#define  DBS_TAG_REVOKE_DCINSERTIOCTL                     0x0000000000200000
#define  DBS_TAG_REVOKE_DCRELEASEIOCTL                    0x0000000000400000
#define  DBS_TAG_REVOKE_APPINSERTIOCTL                    0x0000000000800000
#define  DBS_FLAGS_SET_BY_DRIVER                          0x0400000000000000

void InDskFltTelemetryLogTagHistory(
    _In_     PKDIRTY_BLOCK pDirtyBlock,
    _Inout_  PDEV_CONTEXT  pDevContext,
    _In_     ULONGLONG     ullTagState,
    _In_     etTagStateTriggerReason tagStateReason,
    _In_     ULONG         MessageType
);

FORCEINLINE BOOLEAN
DirtyBlockReadyForDrain(PDEV_CONTEXT DevContext)
{
    PCHANGE_NODE    ChangeNode = NULL;

    ASSERT(NULL != DevContext);

    if (IsListEmpty(&DevContext->ChangeList.Head)) {
        return FALSE;
    }

    ChangeNode = (PCHANGE_NODE)DevContext->ChangeList.Head.Flink;

    // If tag is pending return false
    if (TEST_ALL_FLAGS(ChangeNode->DirtyBlock->ulFlags, KDIRTY_BLOCK_FLAG_CONTAINS_TAGS | KDIRTY_BLOCK_FLAG_TAGS_PENDING)) {
        return FALSE;
    }

    // Notify Event will be set when Either
    // 1. There is one pending closed dirty block Or,
    // 2. Total changes pending > Thresold (8MB default)
    return (
        (0 != ChangeNode->DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601) ||
        (DevContext->ChangeList.ullcbTotalChangesPending >= DevContext->ulcbDataNotifyThreshold)
        );
}

void ResetDeviceId(
    IN PDEV_CONTEXT pDevContext,
    IN bool bResetIfFiteringStopped
);