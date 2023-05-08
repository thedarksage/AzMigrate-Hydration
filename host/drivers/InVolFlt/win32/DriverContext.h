#ifndef _INMAGE_DRIVER_CONTEXT_H_
#define _INMAGE_DRIVER_CONTEXT_H_

#include "mscs.h"
#include "WorkQueue.h"
#include "DirtyBlock.h"
#include "DataFileWriterMgr.h"
#include "Vcontext.h"

#define INMAGE_SYMLINK_NAME     L"\\DosDevices\\Global\\InMageFilter"

#define DEFAULT_DB_HIGH_WATERMARK_SERVICE_NOT_STARTED   0x1000  // 4K Changes.
// LWM is increased to 75% of HWM and Changes to Purge on HWM reached increased to 50%. 
// This change would read out the bitmap changes soon.
#ifdef _WIN64
#define DEFAULT_DB_HIGH_WATERMARK_SERVICE_RUNNING       0x10000  // 64K Changes
#define DEFAULT_DB_LOW_WATERMARK_SERVICE_RUNNING        0xC000   // 48K Changes
#define DEFAULT_DB_TO_PURGE_HIGH_WATERMARK_REACHED      0x8000   // 32K Changes
#else
#define DEFAULT_DB_HIGH_WATERMARK_SERVICE_RUNNING       0x8000  // 32K Changes
#define DEFAULT_DB_LOW_WATERMARK_SERVICE_RUNNING        0x6000  // 24K Changes
#define DEFAULT_DB_TO_PURGE_HIGH_WATERMARK_REACHED      0x4000  // 16K Changes
#endif

#define DEFAULT_MAX_COALESCED_METADATA_CHANGE_SIZE		0x100000 // 1MB


// 0 = No Checks; 1 = Add to Event Log; 2 or higher = Cause BugCheck/BSOD + Add to Event Log
// Below setting Default Validation Level to 2
#define DEFAULT_VALIDATION_LEVEL		1


// To disable thresholds set the high and low water marks to zero.
//#define DEFAULT_DB_LOW_WATERMARK_SERVICE_RUNNING        0x0000 
//#define DEFAULT_DB_HIGH_WATERMARK_SERVICE_RUNNING       0x0000 
#define DEFAULT_DB_HIGH_WATERMARK_SERVICE_SHUTDOWN      0x0400  // 1K Changes.

#define NON_PAGE_POOL_OVER_HEAD 1.2
// Default bitmap memory is set to satisfy disk space of 2 Terabytes.
// 2 * 1024 * 1024 * 1024 * 1024 = 2 Terabytes
// Number of 4K blocks in 2 Terabytes = 2 * 1024 * 1024 * 1024 * 1024 / 4 * 1024
// = 512 * 1024 * 1024 = 512 Meg blocks of 4K are in 2 Terabytes.
// Amount of memory required for bitmap for 512 Meg blocks = 512M / 8 (1 byte represents 8 blocks)
// = 64 MBytes of memory.
#define DEFAULT_MAXIMUM_BITMAP_BUFFER_MEMORY            (0x100000 * 65) // 65 Mbytes
#ifdef _WIN64
#define DEFAULT_DATA_POOL_SIZE                          (256)    // in MBytes
#define DEFAULT_MAX_LOCKED_DATA_BLOCKS                  (60)    // Number of data blocks
#else
#define DEFAULT_DATA_POOL_SIZE                          (64)    // in MBytes
#define DEFAULT_MAX_LOCKED_DATA_BLOCKS                  (30)    // Number of data blocks
#endif // _WIN64
#define DEFAULT_DATA_BUFFER_SIZE                        (4)     // in KBytes
#define DEFAULT_DATA_BLOCK_SIZE                         (1 * 1024)  // in KBytes = 1MB

#define DEFAULT_NUM_DATA_BLOCKS_PER_SERVICE_REQUEST     (4)     // Number of data blocks per dirty block
// (4 * 1024) In KBytes = 4MB
#define DEFAULT_MAX_DATA_SIZE_PER_DATA_MODE_DIRTY_BLOCK         (DEFAULT_NUM_DATA_BLOCKS_PER_SERVICE_REQUEST * DEFAULT_DATA_BLOCK_SIZE)
#define DEFAULT_MAX_DATA_SIZE_PER_NON_DATA_MODE_DIRTY_BLOCK     (64 * 1024)    //in KBytes = 64MB
#ifdef _WIN64
#define DEFAULT_DATA_DIRTY_BLOCKS_PER_VOLUME            (56)
#else
#define DEFAULT_DATA_DIRTY_BLOCKS_PER_VOLUME            (12)
#endif
#define MINIMUM_DATA_DIRTY_BLOCKS_PER_VOLUME            (3)

// (56 * 4)MB = 224MB for 64-bit
// (12 * 4)MB  = 48MB for 32-bit
#define DEFAULT_VOLUME_DATA_SIZE_LIMIT                  (DEFAULT_DATA_DIRTY_BLOCKS_PER_VOLUME * DEFAULT_MAX_DATA_SIZE_PER_DATA_MODE_DIRTY_BLOCK)
#define DEFAULT_MAX_WAIT_TIME_FOR_IRP_COMPLETION_IN_MINUTES        (30)

// This value enables and disables data filtering for new volumes.
// This value is saved for each volume under volume configuration.
#define DEFAULT_VOLUME_DATA_FILTERING_FOR_NEW_VOLUMES   (1)

// This value enables and disables data files for new volumes
// This value is saved for each volume under volume configuration
// under value VolumeDataFiles
#define DEFAULT_VOLUME_DATA_FILES_FOR_NEW_VOLUMES       (0)

// Default Max Limit to below which volume can be allowed to switch to Data Capture Mode (in percentage)
#define DEFAULT_MAX_DATA_ALLOCATION_LIMIT                75
// Minimum data blocks available in free and locked data block list to switch to Data Capture Mode (in percentage)
#define DEFAULT_FREE_AND_LOCKED_DATA_BLOCKS_COUNT        25

#define DC_FLAGS_DEVICE_CREATED                 0x00000001
#define DC_FLAGS_SYMLINK_CREATED                0x00000002
#define DC_FLAGS_DIRTYBLOCKS_POOL_INIT          0x00000100
#define DC_FLAGS_CHANGE_NODE_POOL_INIT          0x00000200
#define DC_FLAGS_WORKQUEUE_ENTRIES_POOL_INIT    0x00000400
#define DC_FLAGS_LIST_NODE_POOL_INIT            0x00000800
#define DC_FLAGS_SERVICE_STATE_CHANGED          0x00001000
#define DC_FLAGS_BITMAP_WORK_ITEM_POOL_INIT     0x00004000
#define DC_FLAGS_CHANGE_BLOCK_2K_POOL_INIT      0x00008000
#define DC_FLAGS_BITRUN_LENOFFSET_ITEM_POOL_INIT     0x00010000
#define DC_FLAGS_DATA_POOL_VARIABLES_INTIALIZED     0x00020000

#define DC_FLAGS_DATAPOOL_ALLOCATION_FAILED     0x80000000

//added for persistent timestamp
#define SHUTDOWN_MARKER                          L"ShutdownMarker"
#define LAST_PERSISTENT_VALUES                   L"LastPersistentValues"
#define LAST_GLOBAL_TIME_STAMP                   0x01
#define LAST_GLOBAL_SEQUENCE_NUMBER              0x02
#define SEQ_NO_BUMP                              0xB71B00 // Changes 12 Million from 1 as reg flush increased to 2 min from 1 sec
#define PERSISTENT_FLUSH_TIME_OUT                120 // Changed to 2Min from 1sec as lot of data was getting flushed on system vol because of reg flush
#define TIME_STAMP_BUMP                          (PERSISTENT_FLUSH_TIME_OUT * HUNDRED_NANO_SECS_IN_SEC)

typedef struct _BOOTVOL_NOTIFICATION_INFO
{
    #define         MAX_FAILURE 10
    volatile LONG 	bootVolumeDetected;
	KEVENT          bootNotifyEvent;
	PVOLUME_CONTEXT pBootVolumeContext;
	volatile LONG   failureCount;
	volatile LONG   reinitCount;
	volatile LONG   LockUnlockBootVolumeDisable;
} BOOTVOL_NOTIFICATION_INFO, PBOOTVOL_NOTIFICATION_INFO; 

typedef struct _DRIVER_CONTEXT
{
    PDRIVER_OBJECT  DriverObject;
    UNICODE_STRING  DriverImage;
    UNICODE_STRING  DriverParameters;
    UNICODE_STRING  DefaultLogDirectory;
    UNICODE_STRING  DataLogDirectory;
    WCHAR           ZeroGUID[GUID_SIZE_IN_CHARS];   // GUID_SIZE_IN_CHARS is 36
    etServiceState  eServiceState;
    ULONG           ulNumVolumes;
    ULONG           ulFlags;
    bool            bEnableDataCapture;
    BOOLEAN         bEnableDataFilteringForNewVolumes;
    bool            bServiceSupportsDataCapture;
    BOOLEAN         bKernelTagsEnabled;

    bool            bServiceSupportsDataFiles;
    bool            bS2SupportsDataFiles;
    bool            bS2is64BitProcess;
    bool            bEnableDataFiles;

    bool            bEnableDataFilesForNewVolumes;
    bool            bDisableVolumeFilteringForNewVolumes;
    bool            bDisableVolumeFilteringForNewClusterVolumes;
    bool            bEndian;

    ULONG           DBHighWaterMarks[ecServiceMaxStates]; // ecServiceMaxStates is 4

    ULONG           DBToPurgeWhenHighWaterMarkIsReached;
    ULONG           DBLowWaterMarkWhileServiceRunning;
    ULONG           ulDataToDiskLimitPerVolumeInMB;
    ULONG           FileWriterThreadPriority;

    // The following values limit the memory consumption of the bitmap buffers
    ULONG           MaximumBitmapBufferMemory;
    ULONG           CurrentBitmapBufferMemory;
/*    ULONG           Bitmap8KGranularitySize;
    ULONG           Bitmap16KGranularitySize; */


    ULONG           Bitmap512KGranularitySize;
    // Following values are related to data buffering.
    ULONG           ulDataBufferSize;
    ULONG           ulDataBlockSize;
    ULONGLONG       ullDataPoolSize;
  
    ULONG           ulDataBlockAllocated;
                    
    ULONG           ulMaxSetLockedDataBlocks;
    ULONG           ulMaxLockedDataBlocks;        
    ULONG           ulMaxDataSizePerVolume;

    ULONG           ulMaxDataSizePerDataModeDirtyBlock;
    ULONG           ulMaxDataSizePerNonDataModeDirtyBlock;
    
    ULONG           ulDataBlocksInLockedDataBlockList;
    ULONG           ulMaxNonPagedPool;

    ULONG           ulDataBlocksInFreeDataBlockList;
    ULONG           ulDataBlocksInOrphanedMappedDataBlockList;

    ULONG           ulMaxWaitTimeForIrpCompletion;
   	ULONG           rsvd;

    LIST_ENTRY      OrphanedMappedDataBlockList;
    LIST_ENTRY      LockedDataBlockList;
    LIST_ENTRY      FreeDataBlockList;
    // A separate lock from DriverContext.Lock is used for DataBlockLists to 
    // avoid deadlock. DriverContext lock is acquired before volume context lock.
    // Volume context is acquired while OrphanedMappedDataBlockList, 
    // FreeDataBlockList or LockedDataBlockList is managed. Using a separate 
    // lock for these lists from DriverContext.Lock would avoid deadlock.
    KSPIN_LOCK      DataBlockListLock;
    KSPIN_LOCK      DataBlockCounterLock;
    ULONG           LockedDataBlockCounter;
    ULONG           MaxLockedDataBlockCounter;
#if DBG
    ULONG           LockedErrorCounter;
#endif
    ULONG           MappedDataBlockCounter;
    ULONG           MaxMappedDataBlockCounter;

    KSPIN_LOCK      TimeStampLock;
    KSPIN_LOCK      MultiVolumeLock;
    KSPIN_LOCK      NoMemoryLogEventLock;
    LARGE_INTEGER   liLastDBallocFailAtTickCount;
    ULONG           ulSecondsBetweenNoMemoryLogEvents;
    ULONG           ulSecondsMaxWaitTimeOnFailOver;
    LONG            lAdditionalGUIDs;
    ULONG           ulNumMemoryAllocFailures;

    ULONGLONG       ullLastTimeStamp;
    ULONGLONG       ullLastPersistTimeStamp;
    ULONGLONG       ullPersistedTimeStampAfterBoot;
    BOOLEAN         PersistentRegistryCreatedFlag;
    CHAR            LastShutdownMarker;
    CHAR            CurrentShutdownMarker;
    ULONGLONG       ullTimeStampSequenceNumber;
    ULONGLONG       ullLastPersistSequenceNumber;
    ULONGLONG       ullPersistedSequenceNumberAfterBoot;
    ULONG           Reserved;

    // The following prevents flooding the system event log with messages
    ULONG           NbrRecentEventLogMessages;
    LARGE_INTEGER   EventLogTimePeriod;
    LARGE_INTEGER   liNonPagedLimitReachedTSforFirstTime;
    LARGE_INTEGER   liNonPagedLimitReachedTSforLastTime;

    //
    // Statistics
    // lDirtyBlocksAllocated is not used for controling code. This is used only
    // for debugging and statistics purposed to see how many blocks have been allocated
    tInterlockedLong        lDirtyBlocksAllocated;
    tInterlockedLong        lChangeNodesAllocated;
    tInterlockedLong        lChangeBlocksAllocated;
    tInterlockedLong        lBitmapWorkItemsAllocated;
    tInterlockedLong        lNonPagedMemoryAllocated;
    tInterlockedLong        lBitRunLengthOffsetItemsAllocated;

    PDEVICE_OBJECT          ControlDevice;
    NPAGED_LOOKASIDE_LIST   DirtyBlocksPool;
    NPAGED_LOOKASIDE_LIST   WorkQueueEntriesPool;
    NPAGED_LOOKASIDE_LIST   ListNodePool;
    NPAGED_LOOKASIDE_LIST   BitmapWorkItemPool;
    NPAGED_LOOKASIDE_LIST   BitRunLengthOffsetPool;
    NPAGED_LOOKASIDE_LIST   ChangeNodePool;
    NPAGED_LOOKASIDE_LIST   ChangeBlock2KPool;
    LIST_ENTRY              HeadForVolumeContext;
    LIST_ENTRY              HeadForBasicDisks;
    // HeadForTagNodes has TAG_NODEs and is protected by TagListLock
    LIST_ENTRY              HeadForTagNodes;
    KSPIN_LOCK              TagListLock;
    KSPIN_LOCK              Lock;

    // VolumeBitmaps are allocated from non paged pool.
    LIST_ENTRY              HeadForVolumeBitmaps;
    ULONG                   ulNumVolumeBitmaps;
    ULONG                   ulMinFreeDataToDiskLimitPerVolumeInMB;
    ULONG           MaxCoalescedMetaDataChangeSize;
    ULONG           ulValidationLevel;

    PIRP            ProcessStartIrp;
    // UserProcess is the process that has sent ProcessStartIrp.
    // Driver maps memory into UserProcess when in data mode.
    PEPROCESS       UserProcess;
    PIRP            ServiceShutdownIrp;
    KEVENT          ServiceThreadWakeEvent;
    KEVENT          ServiceThreadShutdownEvent;
    //Added for persistent time stamp
    KEVENT          PersistentThreadShutdownEvent;
    PKTHREAD        pServiceThread;
    PKTHREAD        pTimeSeqFlushThread; //added for persistent time stamp
    bool            PersistantValueFlushedToRegistry; //added to stop draining changes when we have written the values in the registry.
    WORK_QUEUE      WorkItemQueue;

    KMUTEX          WriteFilterMutex;
    IOBuffer        *WriteFilteringObject;

    // Fields related to kernel tags.
    PCHAR           BitmapToDataModeTag;
    PCHAR           MetaDataToDataModeTag;
    PCHAR           BitmapToMetaDataModeTag;
    PCHAR           DataToMetaDataModeTag;
//    PWCHAR          DataToBitmapModeTag;
//    PWCHAR          MetaDataToBitmapModeTag;
    USHORT          usBitmapToDataModeTagLen;
    USHORT          usMetaDataToDataModeTagLen;
    USHORT          usBitmapToMetaDataModeTagLen;
    USHORT          usDataToMetaDataModeTagLen;
//    USHORT          usDataToBitmapModeTagLen;
//    USHORT          usMetaDataToBitmapModeTagLen;
    ULONG           ulNumberOfThreadsPerFileWriter;
    ULONG           ulNumberOfCommonFileWriters;
    ULONG           ulDaBFreeThresholdForFileWrite;
    ULONG           ulDaBThresholdPerVolumeForFileWrite;
    ULONG           ulDataNotifyThresholdInKB;
    // This is calculated at boot time based on UDIRTY_BLOCK fields.
    ULONG           ulMaxTagBufferSize;
    PVOID               SystemTimeCallbackPointer;

    CDataFileWriterManager   *DataFileWriterManager;
    bool            IsDispatchEntryPatched;
    bool            IsClusterVolume;
    bool            IsVolumeAddedByEnumeration;
    PDRIVER_DISPATCH MajorFunction[IRP_MJ_MAXIMUM_FUNCTION + 1];
    PDRIVER_DISPATCH MajorFunctionForClusDrives[IRP_MJ_MAXIMUM_FUNCTION + 1];
    PDRIVER_OBJECT      pDriverObject , pDriverObjectForCluster ;
// These tunables (in percentages) helps only to switch from Metadata to Data capture mode and does not have any other significance   
    ULONG           ulMaxDataAllocLimitInPercentage;
    ULONG           ulAvailableDataBlockCountInPercentage;

#ifdef _CUSTOM_COMMAND_CODE_
    LIST_ENTRY      MemoryAllocatedList;
#endif // _CUSTOM_COMMAND_CODE_
#ifdef DBG
    LONG            PendingWrites;
    LONG            CompletedWrites;
#endif
    ULONG           ulNumberofAsyncTagIOCTLs;
    ULONG           osMajorVersion;
    ULONG           osMinorVersion;
    BOOLEAN         IsWindows8AndLater;
	BOOTVOL_NOTIFICATION_INFO bootVolNotificationInfo;
} DRIVER_CONTEXT, *PDRIVER_CONTEXT;

extern DRIVER_CONTEXT   DriverContext;

BOOLEAN
SetDriverContextFlag(ULONG  ulFlags);

VOID
UnsetDriverContextFlag(ULONG    ulFlags);

VOID
WakeServiceThread(BOOLEAN bDriverContextLockAcquired);

NTSTATUS
InitializeDriverContext(
    PDRIVER_OBJECT  DriverObject,
    PUNICODE_STRING RegistryPath
    );
VOID
CleanDriverContext();
VOID
GetPersistentValuesFromRegistry(Registry *pParametersKey);

PBASIC_DISK
GetBasicDiskFromDriverContext(ULONG ulDiskSignature, ULONG ulDiskNumber=0xffffffff);

PBASIC_DISK
AddBasicDiskToDriverContext(ULONG ulDiskSignature);

PBASIC_VOLUME
AddBasicVolumeToDriverContext(ULONG ulDiskSignature, struct _VOLUME_CONTEXT *VolumeContext);

PBASIC_VOLUME
GetBasicVolumeFromDriverContext(PWCHAR pGUID);

#if (NTDDI_VERSION >= NTDDI_VISTA)

PBASIC_DISK
GetBasicDiskFromDriverContext(GUID  Guid);

PBASIC_DISK
AddBasicDiskToDriverContext(GUID  Guid);

PBASIC_VOLUME
AddBasicVolumeToDriverContext(GUID  Guid, struct _VOLUME_CONTEXT *VolumeContext);

#endif
//added for the persistent time stamp change
typedef struct _PERSISTENT_VALUE
{
    ULONG PersistentValueType;
    ULONG Reserved;
    ULONGLONG Data;
}PERSISTENT_VALUE, *PPERSISTENT_VALUE;
typedef struct _PERSISTENT_INFO
{
    ULONG NumberOfInfo;
    CHAR   ShutdownMarker;//for marking the clean shutdown
    CHAR   Reserved[3];
    PERSISTENT_VALUE PersistentValues[1];
}PERSISTENT_INFO, *PPERSISTENT_INFO;

//
// Device Extension
//
typedef struct _DEVICE_EXTENSION {

    PDEVICE_OBJECT DeviceObject;

    //
    // Target Device Object
    //

    PDEVICE_OBJECT TargetDeviceObject;

    //
    // Physical device object
    //
    PDEVICE_OBJECT PhysicalDeviceObject;

	//
	//Fix for Bug 28568
	//
	IO_REMOVE_LOCK	 RemoveLock;

    //
	//
	//
	DEVICE_PNP_STATE  PreviousPnPState;

	//
	//
	//
	DEVICE_PNP_STATE DevicePnPState;

    //
    // must synchronize paging path notifications
    //
    KEVENT PagingPathCountEvent;
#if(_WIN32_WINNT > 0x0500)
    volatile long  PagingPathCount;
#else
    long  PagingPathCount;
#endif

    PFILE_OBJECT pagingFile; // track this so we can filter out writes to a pagefile

    //
    // As we may get multiple shutdown's, only respond to the first
    //
    BOOLEAN shutdownSeen;

    //
    // Dirty blocks tracked by this driver
    //

    _VOLUME_CONTEXT   *pVolumeContext;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

#define DEVICE_EXTENSION_SIZE sizeof(DEVICE_EXTENSION)

NTSTATUS
GetListOfVolumes(PLIST_ENTRY ListHead);

NTSTATUS
OpenDriverParametersRegKey(Registry **ppDriverParam);

NTSTATUS
SetStrInParameters(PWCHAR pValueName, PUNICODE_STRING pValue);

NTSTATUS
SetDWORDInParameters(PWCHAR pValueName, ULONG ulValue);

VOID
SetSystemTimeChangeNotification();

VOID
SystemTimeChangeNotification(
    IN PVOID CallbackContext,
    IN PVOID Argument1,
    IN PVOID Argument2
    );

bool
MaxNonPagedPoolLimitReached(bool bAllocating = true);

void SetMaxLockedDataBlocks();
ULONG GetFreeThresholdForFileWrite(PVOLUME_CONTEXT VolumeContext);
ULONG GetThresholdPerVolumeForFileWrite(PVOLUME_CONTEXT VolumeContext);
ULONGLONG ReqNonPagePoolMem();
ULONG ValidateDataBufferSize(ULONG ulDataBufferSize);
ULONG ValidateDataBlockSize(ULONG ulDataBlockSize);
ULONG ValidateMaxDataSizePerDataModeDirtyBlock(ULONG ulMaxDataSizePerDataModeDirtyBlock);
ULONG ValidateMaxDataSizePerNonDataModeDirtyBlock(ULONG ulMaxDataSizePerNonDataModeDirtyBlock);
ULONG ValidateMaxDataSizePerVolume(ULONG ulMaxDataSizePerVolume);
ULONG ValidateMaxSetLockedDataBlocks(ULONG ulMaxSetLockedDataBlocks);
ULONG ValidateFreeThresholdForFileWrite(ULONG ulDaBFreeThresholdForFileWrite);
ULONG ValidateThresholdPerVolumeForFileWrite(ULONG ulDaBThresholdPerVolumeForFileWrite);
ULONGLONG ValidateDataPoolSize(ULONGLONG ullDataPoolSize);
#endif // _INMAGE_DRIVER_CONTEXT_H_
