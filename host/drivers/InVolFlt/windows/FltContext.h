/*++

Copyright (c) Microsoft Corporation

Module Name:

    FltContext.h

Abstract:

    This module contains the type and function definitions specifically for Filter Context

Environment:

    Kernel mode

Revision History:

--*/

#pragma once

#include "global.h"
#include "Profiler.h"
#include "CxSession.h"
#include "TagCommitNotify.h"

typedef enum _etDSDBCdeviceState
{
// If device is in ecDSDBCdeviceOnline state, Device has just started so
// we need to read the bit map regarding this device
// and create the dirty block list.
    ecDSDBCdeviceOnline = 1,
// If device is in ecDSDBCdeviceOffline state, Device has been removed so
// we need to flush all the dirty blocks to bitmap.
    ecDSDBCdeviceOffline = 2,
// If device is in shutdown state, Device has received a shutdown IRP and
// In this state driver pends the shutdown till all the bitmap writes are
// completed.
    ecDSDBCdeviceShutdown = 3,
    // In Remove device Call Device Context is not removed immediately.
    // Following must be set post offlinestate to protect from recursion
    ecDSDBCdeviceDontProcess = 4
} etDSDBCdeviceState, *petDSDBCdeviceState;

// This has to be different from DEVICE_CONTEXT as the Device Could be deleted
// before we are done with information in DEVICE_SPECIFIC_DIRTY_BLOCK_CONTEXT

#define DCF_FILTERING_STOPPED               0x00000001
//Reserved                                  0x00000002 Flag is overlapped for VolFlt and Diskflt
#define DCF_DATA_CAPTURE_DISABLED           0x00000004
#define DCF_DATA_FILES_DISABLED             0x00000008
#define DCF_OPEN_BITMAP_FAILED              0x00000010
#define DCF_EXPLICIT_NONWO_NODRAIN          0x00000020
#define DCF_DEVID_OBTAINED                  0x00000040
#define DCF_OPEN_BITMAP_REQUESTED           0x00000080
#define DCF_BITMAP_READ_DISABLED            0x00000100 
#define DCF_BITMAP_WRITE_DISABLED           0x00000200
#define DCF_IGNORE_BITMAP_CREATION          0x00000400
#define DCF_DATALOG_DIRECTORY_VALIDATED     0x00000800
#define DCF_CLEAR_BITMAP_ON_OPEN            0x00001000
#define DCF_SURPRISE_REMOVAL                0x00002000
#define DCF_FIELDS_PERSISTED                0x00004000

//Bugzilla id: 26376
#define DCF_CV_FS_LOCKED		            0x00008000
#define DCF_DEVNUM_OBTAINED                 0x00010000
#define DCF_DEVSIZE_OBTAINED                0x00020000
#define DCF_DISKID_CONFLICT                 0x00040000
#define DCF_REMOVE_CLEANUP_PENDING          0x00080000
//Reserved                                  0x00100000 to 0x00800000 Flag is overlapped for VolFlt and Diskflt


// Set flag on PreShutdown and Remove Device
// Indicates not to page fault
#define DCF_DONT_PAGE_FAULT                 0x10000000

// Set flag on Device Power and Remove Device
#define DCF_LAST_IO                         0x20000000

// Indicates its bitmap storage device is Power downed/Removed
#define DCF_BITMAP_DEV_OFF                  0x40000000

// Error in Page file detection
#define DCF_PAGE_FILE_MISSED                0x80000000

// Exclude devices for page fault preoperation
#define EXCLUDE_NOPAGE_FAULT_PREOP          (DCF_DONT_PAGE_FAULT |\
                                             DCF_BITMAP_DEV_OFF  |\
                                             DCF_FILTERING_STOPPED)

// Exclude devices for post shutdown operation
#define EXCLUDE_POST_SHUTDOWN_OP            (DCF_BITMAP_DEV_OFF |\
                                             DCF_FILTERING_STOPPED)

// Include devices for post shutdown operation if flags are set
#define INCLUDE_POST_SHUTDOWN_OP            (DCF_DONT_PAGE_FAULT)

// Exclude devices for Bitmap Device OFF pre-operation
#define EXCLUDE_BITMAPOFF_PREOP             (DCF_BITMAP_DEV_OFF |\
                                             DCF_FILTERING_STOPPED)

#define DONT_READ_BITMAP                    (DCF_DONT_PAGE_FAULT | DCF_LAST_IO | DCF_BITMAP_DEV_OFF)
#define DONT_OPEN_BITMAP                    (DCF_DONT_PAGE_FAULT | DCF_BITMAP_DEV_OFF)
#define DONT_DRAIN                          (DCF_DONT_PAGE_FAULT |\
                                             DCF_EXPLICIT_NONWO_NODRAIN)

#define DONT_CAPTURE_IN_DATA                (DCF_DONT_PAGE_FAULT |\
                                             DCF_PAGE_FILE_MISSED |\
                                             DCF_EXPLICIT_NONWO_NODRAIN)
/* DTAP TODO
#define DONT_WRITE_BITMAP_FILE              (DCF_DONT_PAGE_FAULT | DCF_BITMAP_DEV_OFF)
#define DONT_CLOSE_BITMAP_FILE              (DCF_DONT_PAGE_FAULT)
#define DONT_WRITE_BITMAP                   (DCF_BITMAP_DEV_OFF)  //Aply to raw and file IO
#define DONT_CLOSE_BITMAP_RAW               (DCF_BITMAP_DEV_OFF)
#define DONT_BUFFER_RAW_IO                  (DCF_BITMAP_DEV_OFF)
*/

// Filtering should be enabled after learning both Device size and ID
#define IS_DEVINFO_OBTAINED(_f)             (((_f) & (DCF_DEVID_OBTAINED | DCF_DEVSIZE_OBTAINED)) == (DCF_DEVID_OBTAINED | DCF_DEVSIZE_OBTAINED))

// Store the Drive letter/Disk Number in form of string
#define DEVICE_NUM_LENGTH                   (12)

// Store the GUID/Signature in form of string
// Addional length is for NULL terminator
#define DEVID_LENGTH                        (GUID_SIZE_IN_CHARS + 1)




// VOLUME_OUT_OF_SYNC_TIMESTAMP is in formation MM:DD:YYYY::HH:MM:SS:XXX(MilliSeconds) 0x18 + NULL
// Size is in BYTES for storing WSTRING including NULL. rounding to be in 4 byte boundary
#define MAX_DEV_OUT_OF_SYNC_TIMESTAMP_SIZE_IN_WCHARS    0x1A

#define MAX_BITMAP_OPEN_ERRORS_TO_STOP_FILTERING        0x0004
// Exponential back off 1, 2, 4, 8, 16, 32, 64, 128 
#define MAX_DELAY_FOR_BITMAP_FILE_OPEN_IN_SECONDS       300 // 5 * 60 Sec = 5 Minutes
#define MIN_DELAY_FOR_BIMTAP_FILE_OPEN_IN_SECONDS       1   // 1 second
#define INMAGE_MAX_TAG_LENGTH                           96
#define INSTANCES_OF_MEM_USAGE_TRACKED                  10
#define MAX_DC_IO_SIZE_BUCKETS                          12
#define INSTANCES_OF_WO_STATE_TRACKED                   10
#define MAX_NOTIFY_LATENCY_LOG_SIZE                     10
#define MAX_DISTRIBUTION_FOR_NOTIFY_LATENCY             10
#define MAX_S2_DATA_RETRIEVAL_LATENCY_LOG_SIZE          12
#define MAX_DISTRIBUTION_FOR_S2_DATA_RETRIEVAL_LATENCY  12
#define MAX_COMMIT_LATENCY_LOG_SIZE                     10
#define MAX_DISTRIBUTION_FOR_COMMIT_LATENCY             10

// The last bucket has to be always zero.
#define DC_DEFAULT_IO_SIZE_BUCKET_0     0x200    // 512
#define DC_DEFAULT_IO_SIZE_BUCKET_1     0x400    // 1K
#define DC_DEFAULT_IO_SIZE_BUCKET_2     0x800    // 2K
#define DC_DEFAULT_IO_SIZE_BUCKET_3     0x1000   // 4K
#define DC_DEFAULT_IO_SIZE_BUCKET_4     0x2000   // 8K
#define DC_DEFAULT_IO_SIZE_BUCKET_5     0x4000   // 16K
#define DC_DEFAULT_IO_SIZE_BUCKET_6     0x10000  // 64K
#define DC_DEFAULT_IO_SIZE_BUCKET_7     0x40000  // 256K
#define DC_DEFAULT_IO_SIZE_BUCKET_8     0x100000 // 1M
#define DC_DEFAULT_IO_SIZE_BUCKET_9     0x400000 // 4M
#define DC_DEFAULT_IO_SIZE_BUCKET_10    0x800000 // 8M
#define DC_DEFAULT_IO_SIZE_BUCKET_11    0x0      // > 8M

// Added to Persist DevContext Fields
#define   DC_LAST_PERSISTENT_VALUES       L"LastPersistentValues"
#define   PREV_END_TIMESTAMP              0x00
#define   PREV_END_SEQUENCENUMBER         0x01
#define   PREV_SEQUENCEID                 0x02
#define   SOURCE_TARGET_LAG               0x03
#define   MAX_PREV_TIMESTAMP              0xFFFFFFFFFFFFFFFF
#define   MAX_PREV_SEQNUMBER              0xFFFFFFFFFFFFFFFF
#define   MAX_SEQ_ID                      0xFFFFFFFF

#define PARTMGR_REG_KEY                 L"Partmgr"

#define TELEMETRY_VALID_COMMIT_LATENCY_BUCKET_INDEX  5
typedef struct _NON_WOSTATE_STATS
{
    etWriteOrderState   eOldWOState;
    etWriteOrderState   eNewWOState;
    LARGE_INTEGER       liWOstateChangeTS;
    ULONGLONG           ullcbMDChangesPendingAtWOSchange;
    ULONGLONG           ullcbBChangesPendingAtWOSchange;
    ULONGLONG           ullcbDChangesPendingAtWOSchange;
    ULONG               ulNumSecondsInWOS;
    etWOSChangeReason   eWOSChangeReason;
    ULONG               ulMemAllocated;
    ULONG               ulMemReserved;
    ULONG               ulMemFree;
    ULONG               ulDataBlocksInFreeDataBlockList;
    ULONG               ulDataBlocksInLockedDataBlockList;
    ULONG               ulMaxLockedDataBlockCounter;
    ULONGLONG           ullDiskBlendedState;
    ULONG               lNonPagedMemoryAllocated;
    LARGE_INTEGER       liNonPagedLimitReachedTSforLastTime;
    ULONG               ulNonPagedPoolAllocFail;
    etCModeMetaDataReason   eMetaDataModeReason;
}NON_WOSTATE_STATS, *PNON_WOSTATE_STATS;

typedef struct _DISK_TELEMETRY
{
    ULONGLONG                   ullDrainDBCount; // Does not include DirtyBlocks failed when Drain Barrier
    ULONGLONG                   ullCommitDBCount;
    ULONGLONG                   ullCommitDBFailCount;
    ULONGLONG                   ullRevertedDBCount; // Includes TSO as well
    LARGE_INTEGER               liStopFilteringTimestampByUser;
    LARGE_INTEGER               liLastTagInsertSystemTime; // irrespective of success
    LARGE_INTEGER               liLastSuccessInsertSystemTime;
    ULONGLONG                   ullDiskBlendedState;
    TAG_DISK_REPLICATION_STATS  TagInsertSuccessStats;
    TAG_DISK_REPLICATION_STATS  PrevTagInsertStats; // Irrespective of success or failure to insert the tag, always carry previous
    NON_WOSTATE_STATS           LastNonWOSTransitionStats; // This state gives when the WO state to moved to MD or Bitmap last time
    LARGE_INTEGER               LastWOSTime;
    LONGLONG                    GetDbLatencyCnt; 
    LONGLONG                    WaitDbLatencyCnt;
}DISK_TELEMETRY, *PDISK_TELEMETRY; //DiskTelemetryInfo

// DevContext is allocated as a single chunck for
// 1. Structure
// 2. Buffer for DriverParameters.
// Structure need to be rededined to optimize memory consumption
typedef struct _FLT_CONTEXT
{
    // ListEntry is used to link all the DEV_CONTEXT structure in the list
    // mainited by DRIVER_CONTEXT
    LIST_ENTRY      ListEntry;
    IO_CSQ          CsqIrpQueue;
    LIST_ENTRY		HoldIrpQueue;
   BOOLEAN         bWritesHeldForEarlierBarrier;

    KEVENT          hReleaseWritesEvent;
    KEVENT          hReleaseWritesThreadShutdown;
    PVOID           pReleaseWritesThread;


   // Per device pege file list
    LIST_ENTRY      PageFileList;
    LONG            lRefCount;
    ULONG           ulFlags;
    PDEVICE_OBJECT      FilterDeviceObject; 
    PDEVICE_OBJECT      TargetDeviceObject;
    // Store bitmap device object on raw conversion
    PDEVICE_OBJECT      pBitmapDevObject;
    etDSDBCdeviceState  eDSDBCdeviceState;
    etCaptureMode       eCaptureMode;
    etWriteOrderState   ePrevWOState;
    etWriteOrderState   eWOState;
    ULONG               FileWriterThreadPriority;
    bool                bNotify;
    bool                bResyncRequired;
    bool                bResyncIndicated; // ResyncRequired flag sent to user mode process
    bool                bReserved;
    volatile LONG       bBarrierOn;
    CHANGE_LIST     ChangeList;
    PKDIRTY_BLOCK   pDBPendingTransactionConfirm;
    KSPIN_LOCK      Lock;
    KSPIN_LOCK      CsqSpinLock;
    PVOID           pClusterStateThread;
    // This is used to synchrnozie open and close of bitmapfile
    // This is also used in data filtering mode to reserve memory.
    KMUTEX          Mutex;  
    KMUTEX          BitmapOpenCloseMutex;
    // This event is used to synchrnoize user mode access to Device
    // This would be acquired so that multiple threads would not
    // symultaneously GetDBTrans, ClearDiffs, etc..
    KEVENT          SyncEvent;

    // Transaction Id is used to uniquely identify Dirty blocks per volume.
    // Service would use this ID to commit the operation of get dirty blocks.
    ULARGE_INTEGER  uliTransactionId;
    // These are the changes that have been already passed to service or written to bitmap
    ULARGE_INTEGER  uliChangesReturnedToService;
    ULARGE_INTEGER  uliChangesWrittenToBitMap;
    ULARGE_INTEGER  uliChangesReadFromBitMap;

    ULARGE_INTEGER  ulicbChangesQueuedForWriting;
    ULARGE_INTEGER  ulicbChangesWrittenToBitMap;
    ULARGE_INTEGER  ulicbChangesReadFromBitMap;
    // ulicbReturnedToService are changes that are returned to service and commited.
    // This counter does not include changes that are reverted Or changes that are pending commit.
    ULARGE_INTEGER  ulicbReturnedToService;
    ULARGE_INTEGER  uliDataFilesReturned;
    ULONGLONG       ullcbChangesPendingCommit;
    ULARGE_INTEGER  ulicbChangesReverted;
    LARGE_INTEGER   liTickCountAtLastWOStateChange;
    LARGE_INTEGER   liTickCountAtLastCaptureModeChange;
    LARGE_INTEGER   liLastFlushTime;

    // These are the changes pending in DirtyBlocks
    ULONG           ulChangesPendingCommit;
    ULONG           ulChangesQueuedForWriting;  // This is read only for user - User can not clear this stats
    ULONG           ulChangesReverted;
    ULONG           ulDataFilesReverted;

    ULONGLONG       ullDataFilesReturned;
    ULONG           ulWriterId;
    ULONG           ulcbDataNotifyThreshold;

    ULONG           ulNumberOfTagPointsDropped;
    LONG            lDirtyBlocksInPendingCommitQueue;
    LONG            lNumBitMapWriteErrors;
    LONG            lNumBitMapReadErrors;
    LONG            lNumBitMapClearErrors;  // Clear bit errors

    LONG            lNumChangeToMetaDataWOStateOnUserRequest;
    LONG            lNumChangeToMetaDataWOState;
    LONG            lNumChangeToDataWOState;
    LONG            lNumChangeToBitmapWOStateOnUserRequest;
    LONG            lNumChangeToBitmapWOState;

    LONG            lNumChangeToDataCaptureMode;
    LONG            lNumChangeToMetaDataCaptureMode;
    LONG            lNumChangeToMetaDataCaptureModeOnUserRequest;
    ULONG           ulNumSecondsInDataCaptureMode;
    ULONG           ulNumSecondsInMetadataCaptureMode;

    ULONG           ulNumSecondsInDataWOState;
    ULONG           ulNumSecondsInMetaDataWOState;
    ULONG           ulNumSecondsInBitmapWOState;
    // Store GUID/Signature in form of String
    WCHAR           wDevID[DEVID_LENGTH];
    // Store Volume letter/Disk Number in form of String as Information
    WCHAR           wDevNum[DEVICE_NUM_LENGTH];
#if DBG
    // Store Disk Info in form of String for Printf
    CHAR            chDevID[DEVID_LENGTH];
    CHAR            chDevNum[DEVICE_NUM_LENGTH];
#endif
    UNICODE_STRING  DevParameters;
    UNICODE_STRING  DataLogDirectory;
    WCHAR           BufferForOutOfSyncTimeStamp[MAX_DEV_OUT_OF_SYNC_TIMESTAMP_SIZE_IN_WCHARS];
    LONG            lNumBitmapOpenErrors;
    LARGE_INTEGER   liFirstBitmapOpenErrorAtTickCount;
    LARGE_INTEGER   liLastBitmapOpenErrorAtTickCount;

    LIST_ENTRY      ReservedDataBlockList;
    LONG            lDataBlocksInUse;
    LONG            lDataBlocksQueuedToFileWrite;
    ULONG           ulDataBlocksReserved;
    // ulcbDataAllocated is only used to decide if more data buffers have to be allocated
    // for this Device. This does not imply data is available for use. For example data 
    // could have been allocated, but not locked to be used.
    ULONG           ulcbDataAllocated;
    // ulcbDataReserved is amount of data buffers that are reserved for IO that has been
    // seen going down.
    ULONG           ulcbDataReserved;
    // ulcbDataFree is amount of memory that is allocated for this Device and not reserved,
    // this memory can be used for future IO with out allocating more data buffers for IO
    ULONG           ulcbDataFree;
    ULONG           ulNumPendingIrps;
    ULONG           ulFlushCount;
    // This number of bytes of data that is sent down with out reserving memory.
    ULONGLONG       ullcbDataAlocMissed;
    ULONGLONG       ullcbDataWrittenToDisk;
    ULONGLONG       ullcbDataToDiskLimit;
    // This name has GUID in form \\Device\Volume{....}
    // The buffer is kept null Terminated with NameLength 0x60 and MaxLen 0x64
    UNICODE_STRING  BitmapFileName;
    WCHAR           BufferForBitmapFileName[MAX_LOG_PATHNAME];
    ULONG           ulBitmapGranularity;    // This is hint to BitmapAPI::Open if file is being created
    ULONG           ulBitmapOffset;         // This is hint to BitmapAPI::Open if raw Device is being used for bitmap
    LONGLONG        llDevSize;              // This is used to validate the write offset and write size. Object can be Vol/Disk
    LONGLONG        llActualDeviceSize;     // Used for handling resize.
    ULONGLONG NumChangeNodeInDataWOSCreated;
    ULONGLONG NumChangeNodeInDataWOSDrained;
    PFILE_OBJECT    BootVolumeNotifyFileObject; //DTAP Required as volume even in disk. Recheck
    PDEVICE_OBJECT  BootVolumeNotifyDeviceObject; //DTAP Required as volume even in disk. Recheck
    PVOID           PnPNotificationEntry;
    struct _DEV_BITMAP      *pDevBitmap;  // protected by Mutex
    CDataFileWriterManager::DEV_NODE *DevFileWriter;

    PRKEVENT        DBNotifyEvent;
    ULONG           ulMuCurrentInstance;
    ULONGLONG       ullDiskUsageArray[INSTANCES_OF_MEM_USAGE_TRACKED];
    ULONG           ulMemAllocatedArray[INSTANCES_OF_MEM_USAGE_TRACKED];
    ULONG           ulMemReservedArray[INSTANCES_OF_MEM_USAGE_TRACKED];
    ULONG           ulMemFreeArray[INSTANCES_OF_MEM_USAGE_TRACKED];
    ULONG           ulTotalMemFreeArray[INSTANCES_OF_MEM_USAGE_TRACKED];
    LARGE_INTEGER   liCaptureModeChangeTS[INSTANCES_OF_MEM_USAGE_TRACKED];

    ULONGLONG       ullTotalTrackedBytes; // Stores the total size of writes to the device tracked by the driver.
    ULONG           ulIoSizeArray[MAX_DC_IO_SIZE_BUCKETS];
    ULONGLONG       ullIoSizeCounters[MAX_DC_IO_SIZE_BUCKETS];
    ULONG               ulWOSChangeInstance;
    etWriteOrderState   eOldWOState[INSTANCES_OF_WO_STATE_TRACKED];
    etWriteOrderState   eNewWOState[INSTANCES_OF_WO_STATE_TRACKED];
    LARGE_INTEGER       liWOstateChangeTS[INSTANCES_OF_WO_STATE_TRACKED];
    ULONGLONG           ullcbMDChangesPendingAtWOSchange[INSTANCES_OF_WO_STATE_TRACKED];
    ULONGLONG           ullcbBChangesPendingAtWOSchange[INSTANCES_OF_WO_STATE_TRACKED];
    ULONGLONG           ullcbDChangesPendingAtWOSchange[INSTANCES_OF_WO_STATE_TRACKED];
    ULONG               ulNumSecondsInWOS[INSTANCES_OF_WO_STATE_TRACKED];
    etWOSChangeReason   eWOSChangeReason[INSTANCES_OF_WO_STATE_TRACKED];

    struct _DEVICE_PROFILE  DeviceProfile;
    ULONGLONG       LastCommittedTimeStamp;
    ULONGLONG       LastCommittedTimeStampReadFromStore;
    ULONGLONG       TSOEndTimeStamp;
    ULONGLONG       LastCommittedSequenceNumber;
    ULONGLONG       TSOEndSequenceNumber;
    ULONGLONG       LastCommittedSequenceNumberReadFromStore;
    ULONG           LastCommittedSplitIoSeqId;
    ULONG           LastCommittedSplitIoSeqIdReadFromStore;
    ULONG           TSOSequenceId;
    ULONG           ulNumMemoryAllocFailures;
    ULONG           ulOutOfSyncCount;
    ULONG           ulOutOfSyncErrorCode;
    LARGE_INTEGER   liOutOfSyncTimeStamp;
    ULONG           ulOutOfSyncErrorStatus;
    ULONG           ulOutOfSyncIndicatedAtCount;
    LARGE_INTEGER   liOutOfSyncIndicatedTimeStamp;
    LARGE_INTEGER   liOutOfSyncResetTimeStamp;
    LARGE_INTEGER   liStartFilteringTimeStamp;
    LARGE_INTEGER   liStopFilteringTimeStamp;
    LARGE_INTEGER   liClearDiffsTimeStamp;
    LARGE_INTEGER   liClearStatsTimeStamp;
    LARGE_INTEGER   liResyncStartTimeStamp;
    LARGE_INTEGER   liResyncEndTimeStamp;
    LARGE_INTEGER   liLastOutOfSyncTimeStamp;
    LARGE_INTEGER   liDeleteBitmapTimeStamp;
    LARGE_INTEGER   liMountNotifyTimeStamp;
    LARGE_INTEGER   liDisMountNotifyTimeStamp; //DTAP required in BootVolumeDriverNotificationCallback
    LARGE_INTEGER   liDisMountFailNotifyTimeStamp; //DTAP Required in BootVolumeDriverNotificationCallback
    ULONGLONG       ullNetWritesSeenInBytes;
    ULONGLONG       ullUnaccountedBytes;
    LARGE_INTEGER   liTickCountAtNotifyEventSet;

// latency between setting of notification and S2 retreiving dirty block
    ValueDistribution   *LatencyDistForNotify;
    ValueLog            *LatencyLogForNotify;
    ValueDistribution   *LatencyDistForS2DataRetrieval;
    ValueLog            *LatencyLogForS2DataRetrieval;
    ValueDistribution   *LatencyDistForCommit;
    ValueLog            *LatencyLogForCommit;
    bool                bDataLogLimitReached;

    // For "First file missing after Resync Start" issue
    ULONGLONG ResynStartTimeStamp;
    bool bResyncStartReceived;

    //this flag is used to push the dirty block to other list before bitmap is open
    bool                bQueueChangesToTempQueue;
    ULONGLONG           ullcbMinFreeDataToDiskLimit;
    ULARGE_INTEGER      ulPendingTSOTransactionID;
    TIME_STAMP_TAG_V2   PendingTsoFirstChangeTS;
    TIME_STAMP_TAG_V2   PendingTsoLastChangeTS;
    ULONG               ulPendingTsoDataSource;
    etWriteOrderState   ulPendingTsoWOState;
    ULONG ulMaxDataSizePerDataModeDirtyBlock;
    //PreAllocate WorkQueueEntry fo resync
    PWORK_QUEUE_ENTRY pReSyncWorkQueueEntry;
    bool bReSyncWorkQueueRef;
    // Added for debugging same tag seen twice issue
    UCHAR               LastTag[INMAGE_MAX_TAG_LENGTH];
    ULONG               ulTagsinWOState;
    ULONG               ulTagsinNonWOSDrop;
    // Info
    ULONG               ulNumOfPageFiles;

    ULONG               ulOldOsMajorVersion;
    ULONG               ulOldOsMinorVersion;

    // This field indicate End TimeStamp of DirtyBlock in Data/Metadata WO state
    ULONGLONG           EndTimeStampofDB;
    // Cache the MD DB's First Change Timestamp to use if this DB is pending, this TS may update as part of performance
    // changes
    ULONGLONG           MDFirstChangeTS;
    PDEVICE_OBJECT      NTFSDevObj;
    LARGE_INTEGER       liStartFilteringTimeStampByUser;
    LARGE_INTEGER       liGetDirtyBlockTimeStamp;
    LARGE_INTEGER       liCommitDirtyBlockTimeStamp;
    LARGE_INTEGER       liDevContextCreationTS; //Common Code

    // debugging and statistics
    ULONG               ulRevokeTagDBCount;
    LARGE_INTEGER       liBootVolumeLockTS;
    LARGE_INTEGER       liBootVolumeUnLockTS;

    PWCHAR              CurrentHardwareIdName;
    PWCHAR              PrevHardwareIdName;

    ULONGLONG           ullTotalIoCount;
    ULONGLONG           ullTotalCommitLatencyInDataMode;
    ULONGLONG           ullTotalCommitLatencyInNonDataMode;
    LARGE_INTEGER       licbReturnedToServiceInDataMode;
    ULONG               ulMaxBitmapHdrWriteGroups;
} FLT_CONTEXT, *PFLT_CONTEXT; // Common Code for Volume and Disk

const ULONG NotifyLatencyBucketsInMicroSeconds[MAX_DISTRIBUTION_FOR_NOTIFY_LATENCY] = 
{
    10,
    100,
    1000, // Milli second
    10000, // 10 Milli seconds
    100000,
    1000000, // 1 Second
    10000000, // 10 Seconds
    30000000, // 30 Seconds
    60000000, // 1 Minute
    0,
};

const ULONG CommitLatencyBucketsInMicroSeconds[MAX_DISTRIBUTION_FOR_COMMIT_LATENCY] = 
{
    10,
    10, 
    10, 
    10,
    10,
    80000,  // 80ms
    150000, // 150ms
    250000, // 250ms
    500000, // 500ms
    0,
};

const ULONG S2DataRetrievalLatencyBucketsInMicroSeconds[MAX_DISTRIBUTION_FOR_S2_DATA_RETRIEVAL_LATENCY] =
{
    10,
    100,
    1000, // Milli second
    10000, // 10 Milli seconds
    100000,
    1000000, // 1 Second
    10000000, // 10 Seconds
    30000000, // 30 Seconds
    60000000, // 1 Minute
    600000000, // 10 Minutes
    1200000000, // 20 Minutes
    0,
};


typedef enum _etTagProtocolState
{
    ecTagNoneInProgress = 0,
    ecTagIoPaused,
    ecTagInsertTagStarted,
    ecTagInsertTagCompleted,
    ecTagIoResumed,
    ecTagCommitStarted,
    ecTagCleanup
} etTagProtocolState;

typedef enum _etTagProtocolPhase
{
    HoldWrites = 1,
    InsertTag,
    ReleaseWrites,
    CommitTag
} etTagProtocolPhase;

// MessageType : 1  - 14 reserved for crash consistency IOCTL failures
// MessageType : 15 - 18 Tag life cycle states
// MessageType : 19 onwards App consistency IOCTL failures
typedef enum _etMessageType {
    ecMsgUninitialized = 1,
    ecMsgCCInputBufferMismatch,
    ecMsgCCInvalidTagInputBuffer,
    ecMsgCompareExchangeTagStateFailure,
    ecMsgValidateAndBarrierFailure,
    ecMsgTagVolumeInSequenceFailure,
    ecMsgInputZeroTimeOut,
    ecMsgAllFilteredDiskFlagNotSet,
    ecMsgInFlightIO,
    ecMsgTagIrpCancelled,
    ecMsgInValidTagProtocolState,
    ecMsgTagInsertFailure = 15,
    ecMsgTagCommitDBSuccess, 
    ecMsgTagRevoked,
    ecMsgTagDropped,
    ecMsgPreCheckFailure,
    ecMsgAppInputBufferMismatch,
    ecMsgAppInvalidTagInputBuffer,
    ecMsgAppUnexpectedFlags,
    ecMsgAppInvalidInputDiskNum,
    ecMsgAppOutputBufferTooSmall,
    ecMsgAppTagInfoMemAllocFailure,
    ecMsgAppDeviceNotFound,             // at least one corresponding device mapping is not found out of the list of the devices sent by vacp
    ecMsgStatusNoMemory,
    ecMsgStatusUnexpectedPrecheckFlags,
}etMessageType, *petMessageType;

// This flag represents the MessageType originiated from windows driver
#define TELEMETRY_WINDOWS_MSG_TYPE                    0x08000000
// This flag represents feature bit : commit latency buckets have changed to
// lower values i.e. 80ms, 150ms, 250ms, 500ms, >500ms
#define TELEMETRY_FLAGS_COMMIT_LATENCY_BUCKETS        0x00001000
