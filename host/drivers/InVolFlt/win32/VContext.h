#ifndef _INMAGE_VCONTEXT_H__
#define _INMAGE_VCONTEXT_H__

#include "mscs.h"
#include "DataFileWriterMgr.h"
#include "Profiler.h"

//Fix for Bug 27337
#include "fsvolumeoffsetinfo.h"

// forward declarations
class Registry;

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
    ecDSDBCdeviceShutdown = 3
} etDSDBCdeviceState, *petDSDBCdeviceState;



typedef enum _etContextType
{
    ecCompletionContext = 1,

    ecVolumeContext = 2
} etContextType, *petContextType;


// This has to be different from DEVICE_CONTEXT as the Device Could be deleted
// before we are done with information in DEVICE_SPECIFIC_DIRTY_BLOCK_CONTEXT

#define VCF_FILTERING_STOPPED           0x00000001
#define VCF_READ_ONLY                   0x00000002
#define VCF_DATA_CAPTURE_DISABLED       0x00000004
#define VCF_DATA_FILES_DISABLED         0x00000008

#define VCF_OPEN_BITMAP_FAILED          0x00000010

#define VCF_REMOVED_FROM_LIST           0x00000100
#define VCF_GUID_OBTAINED               0x00000200
#define VCF_VOLUME_LETTER_OBTAINED      0x00000400
#define VCF_CV_FS_UNMOUTNED             0x00000800

#define VCF_OPEN_BITMAP_REQUESTED       0x00001000
#define VCF_BITMAP_READ_DISABLED        0x00002000
#define VCF_BITMAP_WRITE_DISABLED       0x00004000
#define VCF_PAGEFILE_WRITES_IGNORED     0x00008000

#define VCF_VOLUME_ON_CLUS_DISK         0x00010000
#define VCF_VOLUME_ON_BASIC_DISK        0x00020000
// This flag is set when only for cluster volumes, when the disk is acquired by SCSI ACQUIRE
#define VCF_CLUSTER_VOLUME_ONLINE       0x00040000
// This flag is set when offline status is returned for a write sent down to target disk.
// On Win2K we see writes succeed for a little time window between we see DISK RELEASE and
// Clus disk changes the status of volume to offline.
#define VCF_CLUSDSK_RETURNED_OFFLINE    0x00080000

#define VCF_IGNORE_BITMAP_CREATION          0x00100000
#define VCF_DATALOG_DIRECTORY_VALIDATED     0x00200000
#define VCF_CLEAR_BITMAP_ON_OPEN            0x00400000
// Flag to save Filtering State on opening Bitmap
#define VCF_SAVE_FILTERING_STATE_TO_BITMAP  0x00800000
#define VCF_DEVICE_SURPRISE_REMOVAL         0x01000000
#define VCF_VCONTEXT_FIELDS_PERSISTED       0x02000000

//Flag to check lock on a volume. We have taken 0x08000000 value which is
//again redefined at another location for VSF_READ_ONLY variable. Since
//the ulFlag variable is 32 bit and we have almost saturated it and we didn't
//want to create another flag specifically for this purpose, 0x08000000 was
//chosen that least conflicts. Bugzilla id: 26376
#define VCF_CV_FS_LOCKED					0x08000000

// VOLUME_OUT_OF_SYNC_TIMESTAMP is in formation MM:DD:YYYY::HH:MM:SS:XXX(MilliSeconds) 0x18 + NULL
// Size is in BYTES for storing WSTRING including NULL. rounding to be in 4 byte boundary
#define MAX_VOLUME_OUT_OF_SYNC_TIMESTAMP_SIZE_IN_WCHARS     0x1A
#define MIN_VOLUME_OUT_OF_SYNC_TIMESTAMP_SIZE_IN_WCHARS     0x18

#define MAX_UNIQUE_VOLUME_NAME_SIZE_IN_WCHARS    0x32
#define MIN_UNIQUE_VOLUME_NAME_SIZE_IN_WCHARS    0x30

// This macro tells if a volume is on a cluster disk.
#define IS_CLUSTER_VOLUME(pVC)      (pVC->ulFlags & VCF_VOLUME_ON_CLUS_DISK)
// This macro tells if a volume is on a non cluster disk.
#define IS_NOT_CLUSTER_VOLUME(pVC)  (!IS_CLUSTER_VOLUME(pVC))
// This macro tells if volume is online.
// If volume is not on a cluster disk it is online or if it is on cluster disk it has to be online.
#define IS_VOLUME_ONLINE(pVC)       (IS_NOT_CLUSTER_VOLUME(pVC) || (pVC->ulFlags & VCF_CLUSTER_VOLUME_ONLINE))

// This macro tells if volume is offline
// If volume is on cluster disk and not online, the volume is considered as offline.
#define IS_VOLUME_OFFLINE(pVC)      (IS_CLUSTER_VOLUME(pVC) && (!(pVC->ulFlags & VCF_CLUSTER_VOLUME_ONLINE)))

// This macro tells if cluster volume is online
#define IS_CLUSTER_VOLUME_ONLINE(pVC)  (pVC->ulFlags & VCF_CLUSTER_VOLUME_ONLINE)

#define MAX_BITMAP_OPEN_ERRORS_TO_STOP_FILTERING    0x0004
// Exponential back off 1, 2, 4, 8, 16, 32, 64, 128 
#define MAX_DELAY_FOR_BITMAP_FILE_OPEN_IN_SECONDS   300 // 5 * 60 Sec = 5 Minutes
#define MIN_DELAY_FOR_BIMTAP_FILE_OPEN_IN_SECONDS   1   // 1 second

#define INMAGE_MAX_TAG_LENGTH 96

typedef struct _VOLUME_GUID
{
    struct _VOLUME_GUID *NextGUID;
    WCHAR           GUID[GUID_SIZE_IN_CHARS + 1];   // 36 + 1
}VOLUME_GUID, *PVOLUME_GUID;

// The last bucket has to be always zero.
#define VC_DEFAULT_IO_SIZE_BUCKET_0     0x200    // 512
#define VC_DEFAULT_IO_SIZE_BUCKET_1     0x400    // 1K
#define VC_DEFAULT_IO_SIZE_BUCKET_2     0x800    // 2K
#define VC_DEFAULT_IO_SIZE_BUCKET_3     0x1000   // 4K
#define VC_DEFAULT_IO_SIZE_BUCKET_4     0x2000   // 8K
#define VC_DEFAULT_IO_SIZE_BUCKET_5     0x4000   // 16K
#define VC_DEFAULT_IO_SIZE_BUCKET_6     0x10000  // 64K
#define VC_DEFAULT_IO_SIZE_BUCKET_7     0x40000  // 256K
#define VC_DEFAULT_IO_SIZE_BUCKET_8     0x100000 // 1M
#define VC_DEFAULT_IO_SIZE_BUCKET_9     0x400000 // 4M
#define VC_DEFAULT_IO_SIZE_BUCKET_10    0x800000 // 8M
#define VC_DEFAULT_IO_SIZE_BUCKET_11    0x0      // > 8M

//#define VC_DEFAULT_IO_SIZE_BUCKET_0     0x1000   // 4K
//#define VC_DEFAULT_IO_SIZE_BUCKET_1     0x2000   // 8K
//#define VC_DEFAULT_IO_SIZE_BUCKET_2     0x4000   // 16K
//#define VC_DEFAULT_IO_SIZE_BUCKET_3     0x8000   // 32K
//#define VC_DEFAULT_IO_SIZE_BUCKET_4     0x10000  // 64K
//#define VC_DEFAULT_IO_SIZE_BUCKET_5     0x20000  // 128K
//#define VC_DEFAULT_IO_SIZE_BUCKET_6     0x40000  // 256K
//#define VC_DEFAULT_IO_SIZE_BUCKET_7     0x80000  // 512K
//#define VC_DEFAULT_IO_SIZE_BUCKET_8     0x100000 // 1M
//#define VC_DEFAULT_IO_SIZE_BUCKET_9     0x0  // > 1M

// Added to Persist VolumeContext Fields
#define   VC_LAST_PERSISTENT_VALUES       L"LastPersistentValues"
#define   PREV_END_TIMESTAMP              0x00
#define   PREV_END_SEQUENCENUMBER         0x01
#define   PREV_SEQUENCEID                 0x02
#define   SOURCE_TARGET_LAG               0x03
#define   MAX_PREV_TIMESTAMP              0xFFFFFFFFFFFFFFFF
#define   MAX_PREV_SEQNUMBER              0xFFFFFFFFFFFFFFFF
#define   MAX_SEQ_ID                      0xFFFFFFFF

// VolumeContext is allocated as a single chunck for
// 1. Structure
// 2. Buffer for DriverParameters.
typedef struct _VOLUME_CONTEXT
{
    // ListEntry is used to link all the VOLUME_CONTEXT structure in the list
    // mainited by DRIVER_CONTEXT
    LIST_ENTRY      ListEntry;
    etContextType   eContextType;
    LONG            lRefCount;
    ULONG           ulFlags;
    PDEVICE_OBJECT      FilterDeviceObject; 
    PDEVICE_OBJECT      TargetDeviceObject;
    etDSDBCdeviceState  eDSDBCdeviceState;
    etCaptureMode       eCaptureMode;
    etWriteOrderState   ePrevWOState;
    etWriteOrderState   eWOState;
    ULONG               FileWriterThreadPriority;
    bool                bNotify;
    bool                bResyncRequired;
    bool                bResyncIndicated; // ResyncRequired flag sent to user mode process
    bool                bReserved;
    LONG                lAdditionalGUIDs;

    CHANGE_LIST     ChangeList;
    PKDIRTY_BLOCK   pDBPendingTransactionConfirm;
    KSPIN_LOCK      Lock;
    // This is used to synchrnozie open and close of bitmapfile
    // This is also used in data filtering mode to reserve memory.
    KMUTEX          Mutex;  
    KMUTEX          BitmapOpenCloseMutex;
    // This event is used to synchrnoize user mode access to volume
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

    ULONG           ulDiskNumber;
#if (NTDDI_VERSION >= NTDDI_VISTA)
    etPartStyle  PartitionStyle;
    union{
        ULONG           ulDiskSignature;
        GUID            DiskId;
    };
#else 
    ULONG           ulDiskSignature;
#endif

    


    WCHAR           wGUID[GUID_SIZE_IN_CHARS + 1];   // 36 + 1
    WCHAR           wDriveLetter[3];
    PVOLUME_GUID    GUIDList;
    CHAR            GUIDinANSI[GUID_SIZE_IN_CHARS + 1];   // 36 + 1
    CHAR            DriveLetter[3];
    UNICODE_STRING  VolumeParameters;
    UNICODE_STRING  DataLogDirectory;
    WCHAR           BufferForOutOfSyncTimeStamp[MAX_VOLUME_OUT_OF_SYNC_TIMESTAMP_SIZE_IN_WCHARS];
    
    RTL_BITMAP      DriveLetterBitMap;
    ULONG           BufferForDriveLetterBitMap;
    LONG            lNumBitmapOpenErrors;
    LARGE_INTEGER   liFirstBitmapOpenErrorAtTickCount;
    LARGE_INTEGER   liLastBitmapOpenErrorAtTickCount;

    LIST_ENTRY      ReservedDataBlockList;
    LONG            lDataBlocksInUse;
    LONG            lDataBlocksQueuedToFileWrite;
    ULONG           ulDataBlocksReserved;
    // ulcbDataAllocated is only used to decide if more data buffers have to be allocated
    // for this volume. This does not imply data is available for use. For example data 
    // could have been allocated, but not locked to be used.
    ULONG           ulcbDataAllocated;
    // ulcbDataReserved is amount of data buffers that are reserved for IO that has been
    // seen going down.
    ULONG           ulcbDataReserved;
    // ulcbDataFree is amount of memory that is allocated for this volume and not reserved,
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
    UNICODE_STRING  UniqueVolumeName;
    WCHAR           BufferForUniqueVolumeName[MAX_UNIQUE_VOLUME_NAME_SIZE_IN_WCHARS];

    UNICODE_STRING  BitmapFileName;
    WCHAR           BufferForBitmapFileName[MAX_LOG_PATHNAME];
    ULONG           ulBitmapGranularity;    // This is hint to BitmapAPI::Open if file is being created
    ULONG           ulBitmapOffset;         // This is hint to BitmapAPI::Open if raw volume is being used for bitmap
    LONGLONG        llVolumeSize;          // This is used to validate the write offset and write size.
    ULONGLONG NumChangeNodeInDataWOSCreated;
    ULONGLONG NumChangeNodeInDataWOSDrained;

    // The following three fields are used to register PnP notification for File System unmount.
    PFILE_OBJECT    LogVolumeFileObject;
    PDEVICE_OBJECT  LogVolumeDeviceObject;

	PFILE_OBJECT    BootVolumeNotifyFileObject;
	PDEVICE_OBJECT  BootVolumeNotifyDeviceObject;
    PVOID           PnPNotificationEntry;

    PBASIC_VOLUME   pBasicVolume;

    struct _VOLUME_BITMAP      *pVolumeBitmap;  // protected by Mutex
    CDataFileWriterManager::VOLUME_NODE *VolumeFileWriter;

    PRKEVENT        DBNotifyEvent;
#define INSTANCES_OF_MEM_USAGE_TRACKED  10
    ULONG           ulMuCurrentInstance;
    ULONGLONG       ullDiskUsageArray[INSTANCES_OF_MEM_USAGE_TRACKED];
    ULONG           ulMemAllocatedArray[INSTANCES_OF_MEM_USAGE_TRACKED];
    ULONG           ulMemReservedArray[INSTANCES_OF_MEM_USAGE_TRACKED];
    ULONG           ulMemFreeArray[INSTANCES_OF_MEM_USAGE_TRACKED];
    ULONG           ulTotalMemFreeArray[INSTANCES_OF_MEM_USAGE_TRACKED];
    LARGE_INTEGER   liCaptureModeChangeTS[INSTANCES_OF_MEM_USAGE_TRACKED];
#define MAX_VC_IO_SIZE_BUCKETS          12
    ULONG           ulIoSizeArray[MAX_VC_IO_SIZE_BUCKETS];
    ULONGLONG       ullIoSizeCounters[MAX_VC_IO_SIZE_BUCKETS];
#define INSTANCES_OF_WO_STATE_TRACKED   10
    ULONG               ulWOSChangeInstance;
    etWriteOrderState   eOldWOState[INSTANCES_OF_WO_STATE_TRACKED];
    etWriteOrderState   eNewWOState[INSTANCES_OF_WO_STATE_TRACKED];
    LARGE_INTEGER       liWOstateChangeTS[INSTANCES_OF_WO_STATE_TRACKED];
    ULONGLONG           ullcbMDChangesPendingAtWOSchange[INSTANCES_OF_WO_STATE_TRACKED];
    ULONGLONG           ullcbBChangesPendingAtWOSchange[INSTANCES_OF_WO_STATE_TRACKED];
    ULONGLONG           ullcbDChangesPendingAtWOSchange[INSTANCES_OF_WO_STATE_TRACKED];
    ULONG               ulNumSecondsInWOS[INSTANCES_OF_WO_STATE_TRACKED];
    etWOSChangeReason   eWOSChangeReason[INSTANCES_OF_WO_STATE_TRACKED];

    struct _VOLUME_PROFILE  VolumeProfile;
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
    LARGE_INTEGER   liDisMountNotifyTimeStamp;
    LARGE_INTEGER   liDisMountFailNotifyTimeStamp;
    ULONGLONG       ullNetWritesSeenInBytes;
    ULONGLONG       ullUnaccountedBytes;
    LARGE_INTEGER   liTickCountAtNotifyEventSet;
#define MAX_NOTIFY_LATENCY_LOG_SIZE 10
#define MAX_DISTRIBUTION_FOR_NOTIFY_LATENCY 10
// latency between setting of notification and S2 retreiving dirty block
    ValueDistribution   *LatencyDistForNotify;
    ValueLog            *LatencyLogForNotify;
#define MAX_S2_DATA_RETRIEVAL_LATENCY_LOG_SIZE  12
#define MAX_DISTRIBUTION_FOR_S2_DATA_RETRIEVAL_LATENCY  12
    ValueDistribution   *LatencyDistForS2DataRetrieval;
    ValueLog            *LatencyLogForS2DataRetrieval;
#define MAX_COMMIT_LATENCY_LOG_SIZE  10
#define MAX_DISTRIBUTION_FOR_COMMIT_LATENCY  10
    ValueDistribution   *LatencyDistForCommit;
    ValueLog            *LatencyLogForCommit;
    bool                bDataLogLimitReached;

    // For "First file missing after Resync Start" issue
    ULONGLONG ResynStartTimeStamp;
    bool bResyncStartReceived;

    //this flag is used to push the dirty block to other list before bitmap is open
    bool                bQueueChangesToTempQueue;
    bool                IsVolumeCluster;
    bool                IsVolumeUsingAddDevice;
    ULONGLONG           ullcbMinFreeDataToDiskLimit;
    ULARGE_INTEGER      ulPendingTSOTransactionID;
    TIME_STAMP_TAG_V2   PendingTsoFirstChangeTS;
    TIME_STAMP_TAG_V2   PendingTsoLastChangeTS;
    ULONG               ulPendingTsoDataSource;
    etWriteOrderState   ulPendingTsoWOState;
    ULONG ulMaxDataSizePerDataModeDirtyBlock;
    ULONGLONG IoctlDiskCopyDataCount;
    ULONGLONG IoctlDiskCopyDataCountSuccess;
    ULONGLONG IoctlDiskCopyDataCountFailure;
    //PreAllocate WorkQueueEntry fo resync
    PWORK_QUEUE_ENTRY pReSyncWorkQueueEntry;
    bool bReSyncWorkQueueRef;
    // Added for debugging same tag seen twice issue
    UCHAR               LastTag[INMAGE_MAX_TAG_LENGTH];
    ULONG               ulTagsinWOState;
    ULONG               ulTagsinNonWOSDrop;
    // This field indicate End TimeStamp of DirtyBlock in Data/Metadata WO state
    ULONGLONG           EndTimeStampofDB;
    // Cache the MD DB's First Change Timestamp to use if this DB is pending, this TS may update as part of performance
    // changes
    ULONGLONG           MDFirstChangeTS;
    PDEVICE_OBJECT      NTFSDevObj;
    LARGE_INTEGER       liStartFilteringTimeStampByUser;
    LARGE_INTEGER       liGetDirtyBlockTimeStamp;
    LARGE_INTEGER       liCommitDirtyBlockTimeStamp;
	LARGE_INTEGER       liVolumeContextCreationTS;
    //Fix for Bug 27337
	FSINFOWRAPPER   	FsInfoWrapper;
} VOLUME_CONTEXT, *PVOLUME_CONTEXT;

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
    50,
    100,
    1000, // Milli second
    10000, // 10 Milli seconds
    100000,
    1000000, // 1 Second
    10000000, // 10 Seconds
    60000000, // 1 Minute
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
typedef struct _COMPLETION_CONTEXT
{
    LIST_ENTRY      ListEntry;
    etContextType   eContextType;
    struct _VOLUME_CONTEXT*        VolumeContext;
    LONGLONG                       llOffset;
    ULONG                          ulLength;    
    LONGLONG                    llLength;
    PIO_COMPLETION_ROUTINE CompletionRoutine;
    PVOID Context;
    UCHAR Control;    
}COMPLETION_CONTEXT, *PCOMPLETION_CONTEXT;

BOOLEAN
CanOpenBitmapFile(PVOLUME_CONTEXT   VolumeContext, BOOLEAN bLoseChanges);

VOID
SetBitmapOpenFailDueToLossOfChanges(PVOLUME_CONTEXT VolumeContext, bool bLockAcquired);

VOID
LogBitmapOpenSuccessEvent(PVOLUME_CONTEXT VolumeContext);

VOID
SetDBallocFailureError(PVOLUME_CONTEXT VolumeContext);

VOID
ProcessVContextWorkItems(PWORK_QUEUE_ENTRY pWorkItem);

VOID
FlushAndCloseBitmapFile(PVOLUME_CONTEXT pVolumeContext);

bool
IsDataCaptureEnabledForThisVolume(PVOLUME_CONTEXT VolumeContext);

bool
CanSwitchToDataCaptureMode(PVOLUME_CONTEXT pVolumeContext, bool bClearDiff = false);

bool
NeedWritingToDataFile(PVOLUME_CONTEXT VolumeContext);

bool
IsDataFilesEnabledForThisVolume(PVOLUME_CONTEXT VolumeContext);

NTSTATUS
InitializeDataLogDirectory(PVOLUME_CONTEXT VolumeContext, PWSTR pDataLogDirectory, bool bCustom = false);

NTSTATUS
VCAddKernelTag(
    PVOLUME_CONTEXT VolumeContext,
    PVOID           pTag,
    USHORT          usTagLen,
    PDATA_BLOCK     *ppDataBlock
    );

NTSTATUS
AddUserDefinedTags(
    PVOLUME_CONTEXT VolumeContext,
    PUCHAR          SequenceOfTags,
    PTIME_STAMP_TAG_V2 *ppTimeStamp,
    PLARGE_INTEGER  pliTickCount,
    PGUID           pTagGUID,
    ULONG           ulTagVolumeIndex
    );

/*-----------------------------------------------------------------------------
 * Functions related to volume context processing
 *-----------------------------------------------------------------------------
 */

BOOLEAN
AddVCWorkItemToList(
    etWorkItem          eWorkItem,
    PVOLUME_CONTEXT     pVolumeContext,
    ULONG               ulAdditionalInfo,
    BOOLEAN             bOpenBitmapIfRequired,
    PLIST_ENTRY         ListHead
    );

NTSTATUS
InMageFltSaveAllChanges(PVOLUME_CONTEXT   pVolumeContext, BOOLEAN bWaitRequied, PLARGE_INTEGER pliTimeOut, bool bMarkVolumeOffline = false);

/*-----------------------------------------------------------------------------
 * Function : LoadVolumeCfgFromRegistry
 *
 * Args -
 *  pVolumeContext - Points to volume whose configuration is read from registry
 *
 * Notes 
 *  This function uses VolumeParameters field in VOLUME_CONTEXT. VolumeParamets
 *  specifies the registry key under which the volume configuration is stored.
 *  
 *  As this function does registry operations, IRQL should be < DISPATCH_LEVEL
 *-----------------------------------------------------------------------------
 */
NTSTATUS
LoadVolumeCfgFromRegistry(PVOLUME_CONTEXT pVolumeContext);

NTSTATUS
FillBitmapFilenameInVolumeContext(PVOLUME_CONTEXT pVolumeContext);

NTSTATUS
OpenVolumeParametersRegKey(PVOLUME_CONTEXT pVolumeContext, Registry **ppVolumeParam);

/*-----------------------------------------------------------------------------
 * Function : AddVolumeLetterToRegVolumeCfg
 *
 * Args -
 *  pVolumeContext - Points to volume for which drive letter has to be added.
 *
 * Notes 
 *  This function uses VolumeParameters field in VOLUME_CONTEXT. VolumeParamets
 *  specifies the registry key under which the volume configuration is stored.
 *  
 *  As this function does registry operations, IRQL should be < DISPATCH_LEVEL
 *-----------------------------------------------------------------------------
 */
NTSTATUS
AddVolumeLetterToRegVolumeCfg(PVOLUME_CONTEXT pVolumeContext);

NTSTATUS
SetDWORDInRegVolumeCfg(PVOLUME_CONTEXT pVolumeContext, PWCHAR pValueName, ULONG ulValue, bool bFlush=0);

NTSTATUS
SetStringInRegVolumeCfg(PVOLUME_CONTEXT pVolumeContext, PWCHAR pValueName, PUNICODE_STRING pValue);

/*---------------------------------------------------------------------------------------------
 *  Functions releated to Allocate, Reference and Dereference VolumeContext
 *---------------------------------------------------------------------------------------------
 */
PVOLUME_CONTEXT
AllocateVolumeContext();

PVOLUME_CONTEXT
ReferenceVolumeContext(PVOLUME_CONTEXT   pVolumeContext);

VOID
DereferenceVolumeContext(PVOLUME_CONTEXT   pVolumeContext);

/*-----------------------------------------------------------------------------
 * Functions related to DataBlocks and DataFiltering
 *-----------------------------------------------------------------------------
 */

bool
VCSetWOState(PVOLUME_CONTEXT VolumeContext, bool bServiceInitiated, char *FunctionName);

void
VCSetWOStateToBitmap(PVOLUME_CONTEXT VolumeContext, bool bServiceInitiated, etWOSChangeReason eWOSChangeReason);

void
VCSetCaptureModeToMetadata(PVOLUME_CONTEXT VolumeContext, bool bServiceInitiated);

void
VCSetCaptureModeToData(PVOLUME_CONTEXT VolumeContext);

VOID
VCAdjustCountersForLostDataBytes(
    PVOLUME_CONTEXT VolumeContext,
    PDATA_BLOCK *ppDataBlock,
    ULONG ulBytesLost,
    bool bAllocate = false,
    bool bCanLock = false);

VOID
VCAdjustCountersToUndoReserve(
    PVOLUME_CONTEXT VolumeContext,
    ULONG   ulBytes);

PDATA_BLOCK
VCGetReservedDataBlock(PVOLUME_CONTEXT VolumeContext);

VOID
VCFreeUsedDataBlockList(PVOLUME_CONTEXT VolumeContext, bool bVCLockAcquired);

VOID
VCFreeReservedDataBlockList(PVOLUME_CONTEXT VolumeContext);

VOID
VCAddToReservedDataBlockList(PVOLUME_CONTEXT VolumeContext, PDATA_BLOCK DataBlock);

VOID
VCFreeAllDataBuffers(PVOLUME_CONTEXT    VolumeContext, bool bVCLockAcquired);

VOID
AddDirtyBlockToVolumeContext(
    PVOLUME_CONTEXT     VolumeContext,
    PKDIRTY_BLOCK       DirtyBlock,
    ULONG               ulDataSource,
    bool                bContinuation
    );

VOID
AddDirtyBlockToVolumeContext(
    PVOLUME_CONTEXT     VolumeContext,
    PKDIRTY_BLOCK       DirtyBlock,
    PTIME_STAMP_TAG_V2  *ppTimeStamp,
    PLARGE_INTEGER      pliTickCount,
    PDATA_BLOCK         *ppDataBlock
    );

VOID
AddLastChangeTimestampToCurrentNode(PVOLUME_CONTEXT VolumeContext, PDATA_BLOCK *ppDataBlock, bool bAllocate = false);

VOID
PrependChangeNodeToVolumeContext(PVOLUME_CONTEXT VolumeContext, PCHANGE_NODE ChangeNode);

PCHANGE_NODE
GetChangeNodeFromChangeList(PVOLUME_CONTEXT VolumeContext, bool bFromHead = true);

PKDIRTY_BLOCK
GetCurrentDirtyBlockFromChangeList(PVOLUME_CONTEXT VolumeContext);

VOID
QueueChangeNodesToSaveAsFile(PVOLUME_CONTEXT VolumeContext,bool bLockAcquired);

PCHANGE_NODE
GetChangeNodeToSaveAsFile(PVOLUME_CONTEXT VolumeContext);

ULONG
CurrentDataSource(PVOLUME_CONTEXT VolumeContext);

//
// GetNextVolumeGUID would match GUID from input parameter VolumeGUID.
// Search is started with primary GUID in VolumeContext and continues in
// GUIDList. If a matching GUID is found, it would return the next GUID in list
// by copying the next GUID into VolumeGUID input buffer.
// If matching GUID is the last GUID (i.e, no next GUID) it returns STATUS_NOT_FOUND
// If matching GUID is not found in list, then first/primary GUID is copied into
// VolumeGUID and STATUS_SUCCESS is returned.
// If next GUID is returned Status returned is STATUS_SUCCESS
// If next GUID is not present Status returned is STATUS_NOT_FOUND
// 
NTSTATUS
GetNextVolumeGUID(PVOLUME_CONTEXT VolumeContext, PWCHAR VolumeGUID);

VOID 
ClearVolumeStats(PVOLUME_CONTEXT pVolumeContext);

VOID
SetBitmapOpenError(PVOLUME_CONTEXT  VolumeContext, bool bLockAcquired, NTSTATUS Status);

void
MoveUnupdatedDirtyBlocks(PVOLUME_CONTEXT pVolumeContext);

void
InitializeClusterDiskAcquireParams(PVOLUME_CONTEXT pVolumeContext);

NTSTATUS
GetVContextFieldsFromRegistry(PVOLUME_CONTEXT pVolumeContext);

NTSTATUS
PersistVContextFields(PVOLUME_CONTEXT pVolumeContext);

void
UpdateVolumeContextWithValuesReadFromBitmap(BOOLEAN bVolumeInSync, BitmapPersistentValues BitmapPersistentValue);

BOOLEAN
ValidateDataWOSTransition(PVOLUME_CONTEXT pVolumeContext);

typedef NTSTATUS    (*DispatchEntry)( IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);

#endif // _INMAGE_VCONTEXT_H__

