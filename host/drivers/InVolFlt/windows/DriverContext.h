#pragma once


#include "global.h"
#include "WorkQueue.h"
#include "DirtyBlock.h"
#include "DataFileWriterMgr.h"
#include "DevContext.h"
#include "TagNode.h"

// Symbolic link for volume filter
#define INMAGE_SYMLINK_NAME                             L"\\DosDevices\\Global\\InMageFilter"

// Symbolic link for disk filter
#define INMAGE_DISK_FILTER_SYMLINK_NAME                 L"\\DosDevices\\Global\\InMageDiskFilter"

#define DISK_NAME_FORMAT                                L"\\Device\\Harddisk%lu\\Partition0"

#define DRIVER_STORVSC_NAME                             L"storvsc"
#define DRIVER_VMBUS_NAME                               L"vmbus"
#define DRIVER_START_KEY                                L"Start"

#define VENDOR_VMWARE                                   L"VMware"
#define VENDOR_MSFT                                     L"Msft"

/* Global Registry keys which are applicable of both the filters*/

// Location of global registry keys under HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\<drivername>
// applicable for all disks/volumes
#define PARAMETERS_KEY                                  L"\\Parameters"
// This key is helpful for debugging purpose
#define KERNEL_TAGS_KEY                                 L"KernelTags"

#define BOOT_DISK_KEY                                   L"BootDiskInfo"
// Default location for bitmap files for non-cluster devices, Installer sets the install path e.g. 
// C:\Program Files (x86)\Microsoft Azure Site Recovery\Application Data 

#ifdef VOLUME_FLT
#define DEFAULT_LOG_DIRECTORY_VALUE                     L"\\SystemRoot\\InMageVolumeLogs\\"
#else
#define DEFAULT_LOG_DIRECTORY_VALUE                     L"\\SystemRoot\\InMageLogs\\"
#endif

#define DEFAULT_LOG_DIR_FOR_CLUSDISKS                   L"\\InMageClusterLogs\\"
#define DEFAULT_DATA_LOG_DIR_FOR_CLUSDISKS              L"\\InMageClusterLogs\\DataLogs\\"

#define DEFAULT_BOOT_VENDOR_ID                          L"None"

// Registry keys for water marks
#define DB_HIGH_WATERMARK_SERVICE_NOT_STARTED           L"DirtyBlockHighWaterMarkServiceNotStarted"
#define DB_HIGH_WATERMARK_SERVICE_SHUTDOWN              L"DirtyBlockHighWaterMarkServiceShutdown"
#define DB_LOW_WATERMARK_SERVICE_RUNNING                L"DirtyBlockLowWaterMarkServiceRunning"
#define DB_HIGH_WATERMARK_SERVICE_RUNNING               L"DirtyBlockHighWaterMarkServiceRunning"
#define DB_TO_PURGE_HIGH_WATERMARK_REACHED              L"DirtyBlocksToPurgeWhenHighWaterMarkIsReached"

#define DB_MAX_COALESCED_METADATA_CHANGE_SIZE           L"MaxCoalescedMetadataChangeSize"
#define DB_VALIDATION_LEVEL                             L"ValidationCheckLevel"

#define MAXIMUM_BITMAP_BUFFER_MEMORY                    L"MaximumBitmapBufferMemory"
#define BITMAP_512K_GRANULARITY_SIZE                    L"Bitmap512KGranularitySize"

#define DEFAULT_LOG_DIRECTORY                           L"DefaultLogDirectory"
#define DATA_LOG_DIRECTORY                              L"DataLogDirectory"

#define PREV_BOOTDEV_VENDOR_ID                          L"VendorId"
#define PREV_BOOTDEV_ID                                 L"DeviceId"
#define PREV_BOOTDEV_BUS                                L"BusType"
#define DISABLE_NO_HYDRATION_WF                         L"DisableNoHydrationWF"

#define DATA_BUFFER_SIZE                                L"DataBufferSize"
#define DATA_BLOCK_SIZE                                 L"DataBlockSize"
#define DATA_POOL_SIZE                                  L"DataPoolSize"
#define DATASIZE_LIMIT_PER_DIRTY_BLOCK                  L"DataSizeLimitPerDirtyBlock"
#define DATASIZE_LIMIT_PER_VOLUME                       L"DataSizeLimitPerVolume"
#define LOCKED_DATA_BLOCKS_FOR_USE_AT_DISPATCH_IRQL     L"LockedDataBlocksForUseAtDispatchIRQL"


#define NUMBER_OF_THREADS_PER_FILE_WRITER               L"NumberOfThreadsPerFileWriter"
#define NUMBER_OF_COMMON_FILE_WRITERS                   L"NumberOfCommonFileWriters"
#define FREE_THRESHOLD_FOR_FILE_WRITE                   L"FreeThresholdForFileWrite"


#define TIME_IN_SECONDS_BETWEEN_NO_MEMORY_LOG_EVENTS    L"TimeInSecondsBetweenNoMemoryLogEvents"
#define MAX_WAIT_TIME_IN_SECONDS_ON_FAIL_OVER           L"MaxWaitTimeInSecondsOnFailOver"
#define WORKER_THREAD_PRIORITY                          L"WorkerThreadPriority"
#define MAX_NON_PAGED_POOL_IN_MB                        L"MaxNonPagedPoolInMB"
#define MAX_WAIT_TIME_FOR_IRP_COMPLETION_IN_MINUTES     L"MaxWaitTimeForIrpCompletionInMinutes"

#define DATA_ALLOCATION_LIMIT_IN_PERCENTAGE             L"DataAllocationLimitInPercentage"
#define FREE_AND_LOCKED_DATA_BLOCKS_COUNT_IN_PERCENTAGE L"TotalFreeAndLockedDataBlocksLimitInPercentage"

#define MAX_DATA_SIZE_PER_DATA_MODE_DIRTY_BLOCK         L"MaxDataSizePerDataModeDirtyBlock"
#define MAX_DATA_SIZE_PER_NON_DATA_MODE_DIRTY_BLOCK     L"MaxDataSizePerNonDataModeDirtyBlock"

#define MODE_CHANGE_TAG_DESC_FOR_BITMAP_TO_DATA         L"ModeChangeTagDescForBitmapToData"
#define MODE_CHANGE_TAG_DESC_FOR_METADATA_TO_DATA       L"ModeChangeTagDescForMetaDataToData"
#define MODE_CHANGE_TAG_DESC_FOR_BITMAP_TO_METADATA     L"ModeChangeTagDescForBitmapToMetaData"
#define MODE_CHANGE_TAG_DESC_FOR_DATA_TO_METADATA       L"ModeChangeTagDescForDataToMetaData"
#define MODE_CHANGE_TAG_DESC_FOR_METADATA_TO_BITMAP     L"ModeChangeTagDescForMetaDataToBitmap"
#define MODE_CHANGE_TAG_DESC_FOR_DATA_TO_BITMAP         L"ModeChangeTagDescForDataToBitmap"

#define RESYNC_ON_REBOOT                                L"ResyncOnReboot"

// These are ANSI Strings.
#define MODE_CHANGE_TAG_DESC_FOR_BITMAP_TO_DATA_DEFAULT_VALUE         "Filtering mode change from bitmap to data"
#define MODE_CHANGE_TAG_DESC_FOR_METADATA_TO_DATA_DEFAULT_VALUE       "Filtering mode change from meta-data to data"
#define MODE_CHANGE_TAG_DESC_FOR_BITMAP_TO_METADATA_DEFAULT_VALUE     "Filtering mode change from bitmap to meta-data"
#define MODE_CHANGE_TAG_DESC_FOR_DATA_TO_METADATA_DEFAULT_VALUE       "Filtering mode change from data to meta-data"
#define MODE_CHANGE_TAG_DESC_FOR_METADATA_TO_BITMAP_DEFAULT_VALUE     "Filtering mode change from meta-data to bitmap"
#define MODE_CHANGE_TAG_DESC_FOR_DATA_TO_BITMAP_DEFAULT_VALUE         "Filtering mode change from data to bitmap"

#define KERNEL_TAGS_ENABLED                             L"KernelTagsEnabled"
#define REBOOT_COUNTER                                  L"RebootCounter"

#define NUM_FS_FLUSH_THREADS                            L"FsFlushThreadsCount"
#define FS_FLUSH_TIMEOUT_IN_MS                          L"FsFlushTimeout"

#define DEVICE_ID_KEY                                   L"DeviceId"
#define BUS_TYPE_KEY                                    L"BusType"
#define VENDOR_ID_KEY                                   L"VendorId"
#define RESDISK_DETECTION_TIME_MS                       L"ResDiskDetectionTimeMS"
#define TAG_NOTIFY_TIMEOUT_SEC                          L"TagNotifyTimeoutSecs"
#define DEVICE_DRAIN_STATE                              L"DeviceDrainState"
#define CLUSTER_ATTRIBUTES                              L"ClusterAttributes"

// This value is present at both Parameters section and Parameters\Volume section
// Volume/Device prefix is added to indicate these keys are applicable for both global and local section
// Maintaining same registry key names for volume filter and modifying few to reflect the functionality
#ifdef VOLUME_FLT
#define DEVICE_DATAMODE_CAPTURE                         L"VolumeDataFiltering"
#define DATAMODE_CAPTURE_FOR_NEW_DEVICES                L"VolumeDataFilteringForNewVolumes"
#define DEVICE_DATA_FILES                               L"VolumeDataFiles"
#define DATA_FILES_FOR_NEW_DEVICES                      L"VolumeDataFilesForNewVolumes"
#define MAX_DATA_SIZE_LIMIT_PER_DEVICE                  L"VolumeDataSizeLimit"
// Fields required for data file support
#define DEVICE_DATA_TO_DISK_LIMIT_IN_MB                 L"VolumeDataToDiskLimitInMB"
#define DEVICE_MIN_FREE_DATA_TO_DISK_LIMIT_IN_MB        L"VolumeMinFreeDataToDiskLimitInMB"
#define DEVICE_FILE_WRITER_THREAD_PRIORITY              L"VolumeFileWriterThreadPriority"
#define DEVICE_THRESHOLD_FOR_FILE_WRITE                 L"VolumeThresholdForFileWrite"

#define DEVICE_DATA_NOTIFY_LIMIT_IN_KB                  L"VolumeDataNotifyLimitInKB"

#define DISABLE_FILTERING_FOR_NEW_DEVICES               L"VolumeFilteringDisabledForNewVolumes"
#define DISABLE_FILTERING_FOR_NEW_CLUSTER_DEVICES       L"VolumeFilteringDisabledForNewClusterVolumes"

#else
#define DEVICE_DATAMODE_CAPTURE                         L"DeviceDataModeCapture" 
#define DATAMODE_CAPTURE_FOR_NEW_DEVICES                L"DataModeCaptureForNewDevices"
#define DEVICE_DATA_FILES                               L"DeviceDataFiles"
#define DATA_FILES_FOR_NEW_DEVICES                      L"DataFilesForNewDevices"
#define MAX_DATA_SIZE_LIMIT_PER_DEVICE                  L"MaxDeviceDataSizeLimit"
// Fields required for supporting data files
#define DEVICE_DATA_TO_DISK_LIMIT_IN_MB                 L"DeviceDataToDiskLimitInMB"
#define DEVICE_MIN_FREE_DATA_TO_DISK_LIMIT_IN_MB        L"DeviceMinFreeDataToDiskLimitInMB"
#define DEVICE_FILE_WRITER_THREAD_PRIORITY              L"DeviceFileWriterThreadPriority"
#define DEVICE_THRESHOLD_FOR_FILE_WRITE                 L"DeviceThresholdForFileWrite"

#define DEVICE_DATA_NOTIFY_LIMIT_IN_KB                  L"DeviceDataNotifyLimitInKB"

#define DISABLE_FILTERING_FOR_NEW_DEVICES               L"DisableFilteringForNewDevices"
#define DISABLE_FILTERING_FOR_NEW_CLUSTER_DEVICES       L"DisableFilteringForNewClusterDevices"
#define FS_FLUSH_PRE_SHUTDOWN                           L"FsFlushPreShutdown"

#define DEVICE_DATA_OS_MAJOR_VER                        L"OsMajorVersion"
#define DEVICE_DATA_OS_MINOR_VER                        L"OsMinorVersion"

#define TOO_MANY_LAST_CHANCE_WRITES_HIT                 L"TooManyLastChanceWritesHit"

#define SYSTEM_STATE_FLAGS                              L"SystemStateFlags"

#endif
/* Default value which are set when the key is created, most of registry entries can be tuned from user space*/

#define DEFAULT_TIME_IN_SECONDS_BETWEEN_NO_MEMORY_LOG_EVENTS        3600  // 60 minutes
#define DEFAULT_MAX_TIME_IN_SECONDS_ON_FAIL_OVER                    120   // 2 minute

#define DEFAULT_DISABLE_FILTERING_FOR_NEW_DEVICES                   0x01
// Filtering state is now stored on the Bitmap header for cluster devices
#define DEFAULT_FILTERING_DISABLED_FOR_NEW_CLUSTER_DEVICES          0x01

#define DEFAULT_KERNEL_TAGS_ENABLED                                 0x00

// Mapped page writer thread has a priority of 17
// LOW_REALTIME_PRIORITY is 16. So setting priority to LOW_REALTIME_PRIORITY does not help.
#define DEFAULT_DEVICE_FILE_WRITERS_THREAD_PRIORITY                 (19)    // LOW_REALTIME_PRIORITY + 3
#define MINIMUM_DEVICE_FILE_WRITERS_THREAD_PRIORITY                 (9)
#define DEFAULT_NUMBER_OF_THREADS_FOR_EACH_FILE_WRITER              0x00
#define DEFAULT_NUMBER_OF_COMMON_FILE_WRITERS                       0x01
#define DEFAULT_FREE_THREASHOLD_FOR_FILE_WRITE                      20      // 20% = 12
#define DEFAULT_DEVICE_THRESHOLD_FOR_FILE_WRITE                     25      // 25% = 8
#define DEFAULT_BITMAP_512K_GRANULARITY_SIZE                        (512)   // in GBytes

#define DEFAULT_WORKER_THREAD_PRIORITY                              (12)

#define DEFAULT_DB_HIGH_WATERMARK_SERVICE_NOT_STARTED               0x1000  // 4K Changes.

// LWM is increased to 75% of HWM and Changes to Purge on HWM reached increased to 50%. 
// This change would read out the bitmap changes soon.
#ifdef _WIN64
#define DEFAULT_DB_HIGH_WATERMARK_SERVICE_RUNNING                   0x10000  // 64K Changes
#define DEFAULT_DB_LOW_WATERMARK_SERVICE_RUNNING                    0xC000   // 48K Changes
#define DEFAULT_DB_TO_PURGE_HIGH_WATERMARK_REACHED                  0x8000   // 32K Changes
#else
#define DEFAULT_DB_HIGH_WATERMARK_SERVICE_RUNNING                   0x8000  // 32K Changes
#define DEFAULT_DB_LOW_WATERMARK_SERVICE_RUNNING                    0x6000  // 24K Changes
#define DEFAULT_DB_TO_PURGE_HIGH_WATERMARK_REACHED                  0x4000  // 16K Changes
#endif

// To disable thresholds set the high and low water marks to zero.
//#define DEFAULT_DB_LOW_WATERMARK_SERVICE_RUNNING                  0x0000 
//#define DEFAULT_DB_HIGH_WATERMARK_SERVICE_RUNNING                 0x0000 
#define DEFAULT_DB_HIGH_WATERMARK_SERVICE_SHUTDOWN                  0x0400  // 1K Changes.

#define DEFAULT_MAX_COALESCED_METADATA_CHANGE_SIZE                  0x100000 // 1MB

// 0 = No Checks; 1 = Add to Event Log; 2 or higher = Cause BugCheck/BSOD + Add to Event Log
// Below setting Default Validation Level to 2
#define DEFAULT_VALIDATION_LEVEL                                    1

#define NON_PAGE_POOL_OVER_HEAD                                     1.2

// Default bitmap memory is set to satisfy disk space of 2 Terabytes.
// 2 * 1024 * 1024 * 1024 * 1024 = 2 Terabytes
// Number of 4K blocks in 2 Terabytes = 2 * 1024 * 1024 * 1024 * 1024 / 4 * 1024
// = 512 * 1024 * 1024 = 512 Meg blocks of 4K are in 2 Terabytes.
// Amount of memory required for bitmap for 512 Meg blocks = 512M / 8 (1 byte represents 8 blocks)
// = 64 MBytes of memory.
#define MAX_ALLOWED_DATA_POOL_IN_MB                                 4096
#define DEFAULT_MAXIMUM_BITMAP_BUFFER_MEMORY                        (0x100000 * 65) // 65 Mbytes
#ifdef _WIN64
#define DEFAULT_DATA_POOL_SIZE                                      (256)    // in MBytes
#define DEFAULT_MAX_LOCKED_DATA_BLOCKS                              (60)    // Number of data blocks
#else
#define DEFAULT_DATA_POOL_SIZE                                      (64)    // in MBytes
#define DEFAULT_MAX_LOCKED_DATA_BLOCKS                              (30)    // Number of data blocks
#endif // _WIN64
#define DEFAULT_DATA_BUFFER_SIZE                                    (4)     // in KBytes
#define DEFAULT_DATA_BLOCK_SIZE                                     (1 * 1024)  // in KBytes = 1MB

#define DEFAULT_NUM_DATA_BLOCKS_PER_SERVICE_REQUEST                 (4)     // Number of data blocks per dirty block
// (4 * 1024) In KBytes = 4MB
#define DEFAULT_MAX_DATA_SIZE_PER_DATA_MODE_DIRTY_BLOCK             (DEFAULT_NUM_DATA_BLOCKS_PER_SERVICE_REQUEST * DEFAULT_DATA_BLOCK_SIZE)
#define DEFAULT_MAX_DATA_SIZE_PER_NON_DATA_MODE_DIRTY_BLOCK         (64 * 1024)    //in KBytes = 64MB
#ifdef _WIN64
#define DEFAULT_DATA_DIRTY_BLOCKS_PER_DEV                           (56)
#else
#define DEFAULT_DATA_DIRTY_BLOCKS_PER_DEV                           (12)
#endif
#define MINIMUM_DATA_DIRTY_BLOCKS_PER_DEV                           (3)

// (56 * 4)MB = 224MB for 64-bit
// (12 * 4)MB  = 48MB for 32-bit
#define DEFAULT_MAX_DATA_SIZE_LIMIT_PER_DEVICE                      (DEFAULT_DATA_DIRTY_BLOCKS_PER_DEV * DEFAULT_MAX_DATA_SIZE_PER_DATA_MODE_DIRTY_BLOCK)
#define DEFAULT_MAX_WAIT_TIME_FOR_IRP_COMPLETION_IN_MINUTES         (30)

// This value enables and disables datamode capture for new device.
// This value is saved for each Device under Device configuration.
#define DEFAULT_DATAMODE_CAPTURE_FOR_NEW_DEVICES                     (1)

// This value enables and disables data files for new Device
// This value is saved for each volume under Device configuration
// under value DeviceDataFiles
#define DEFAULT_DATA_FILES_FOR_NEW_DEVICES                           (0)

// Default Max Limit to below which Device can be allowed to switch to Data Capture Mode (in percentage)
#define DEFAULT_MAX_DATA_ALLOCATION_LIMIT                            75
// Minimum data blocks available in free and locked data block list to switch to Data Capture Mode (in percentage)
#define DEFAULT_FREE_AND_LOCKED_DATA_BLOCKS_COUNT                    25

#define DEFAULT_DEVICE_DATA_TO_DISK_LIMIT_IN_MB                      (512)       // 512 MB
#define DEFAULT_DEVICE_MIN_FREE_DATA_TO_DISK_LIMIT_IN_MB             (128)       // 128 MB
#ifdef _WIN64
#define DEFAULT_DEVICE_DATA_NOTIFY_LIMIT_IN_KB                       (0x2000)      // 8 MB
#define DEFAULT_MAX_NON_PAGED_POOL_IN_MB                             (32)          // 32 MB
#else
#define DEFAULT_DEVICE_DATA_NOTIFY_LIMIT_IN_KB                       (4072)        // 4 MB - 24K (for stream header overhead)
#define DEFAULT_MAX_NON_PAGED_POOL_IN_MB                             (12)          // 12 MB
#endif // _WIN64

// This value enables and disables data capture for all devices.
// If this is set to 0, data capture is disabled for all devices
#define DEFAULT_DEVICE_DATAMODE_CAPTURE                              (1)
// Preallocates the nodes on driver load
#define DEFAULT_MAX_PAGE_FILE_NODES                                  (64)
#define DEFAULT_BOOT_COUNTER                                         0

#define DEFAULT_RESDISK_DETECTION_TIME_IN_MS                        (30*1000)       // 30 secs

#define MIN_NUM_FS_FLUSH_THREADS                                    (KeQueryActiveProcessorCount(NULL))
#define MAX_NUM_FS_FLUSH_THREADS                                    (2 * KeQueryActiveProcessorCount(NULL))
#define MIN_FS_FLUSH_TIMEOUT_IN_SEC                                 (60) // 60 sec
#define MAX_FS_FLUSH_TIMEOUT_IN_SEC                                 (5*60) // 300 sec
#define MAX_COUNT_FOR_LEARNIRP_FAILURE                               10

#define DC_FLAGS_DEVICE_CREATED                         0x00000001
#define DC_FLAGS_SYMLINK_CREATED                        0x00000002
#define DC_FLAGS_DIRTYBLOCKS_POOL_INIT                  0x00000100
#define DC_FLAGS_CHANGE_NODE_POOL_INIT                  0x00000200
#define DC_FLAGS_WORKQUEUE_ENTRIES_POOL_INIT            0x00000400
#define DC_FLAGS_LIST_NODE_POOL_INIT                    0x00000800
#define DC_FLAGS_SERVICE_STATE_CHANGED                  0x00001000
#define DC_FLAGS_BITMAP_WORK_ITEM_POOL_INIT             0x00004000
#define DC_FLAGS_CHANGE_BLOCK_2K_POOL_INIT              0x00008000
#define DC_FLAGS_BITRUN_LENOFFSET_ITEM_POOL_INIT        0x00010000
#define DC_FLAGS_DATA_POOL_VARIABLES_INTIALIZED         0x00020000
#define DC_FLAGS_POST_SHUT_DEVICE_CREATED               0x00040000
#define DC_FLAGS_COMPLETION_CONTEXT_POOL_INIT           0x00080000
#define DC_FLAGS_DATAPOOL_ALLOCATION_FAILED             0x80000000

//added for persistent timestamp
#define SHUTDOWN_MARKER                          L"ShutdownMarker"
#define LAST_PERSISTENT_VALUES                   L"LastPersistentValues"
#define LAST_GLOBAL_TIME_STAMP                   0x01
#define LAST_GLOBAL_SEQUENCE_NUMBER              0x02
#define SEQ_NO_BUMP                              0xB71B00 // Changes 12 Million from 1 as reg flush increased to 2 min from 1 sec
#define PERSISTENT_FLUSH_TIME_OUT                120 // Changed to 2Min from 1sec as lot of data was getting flushed on system vol because of reg flush
#define TOLERABLE_DELAY_TIMER_SCHEDULING         (180) // Expect max 180 sec delay than expected between time update calls
#define CLOCKJUMP_FOR_LOGGING                    (30) // Log if we see time jump of > 30 sec
#define LOGGING_DELAY_SCSI_DIRECT_CMD_SECS       (1800) // Every 30 mins
#define TIME_STAMP_BUMP                          (PERSISTENT_FLUSH_TIME_OUT * HUNDRED_NANO_SECS_IN_SEC)
#define CLUSTER_STATE_RESCAN_TIME_SEC            (30)

// Wait Objects for Low memory condiditions
#define INDSKFLT_WAIT_TERMINATE_THREAD              0  
#define INDSKFLT_WAIT_LOW_NON_PAGED_POOL_CONDITION  1  
#define INDSKFLT_WAIT_LOW_MEMORY_CONDITION          2  
#define MAX_INDSKFLT_WAIT_OBJECTS                   3   

// Max Wait Objects for tag
#define INDSKFLT_WAIT_RELEASE_WRITES                1  
#define MAX_INDSKFLT_WAIT_TAG_OBJECTS               2   

// Upper Limit for Data pool size
#define MAX_ALLOWED_DATA_POOL_IN_MB                              4096

typedef struct _DRIVER_TELEMETRY {
    ULONGLONG       ullFlags;
    LARGE_INTEGER   liDriverLoadTime;
    LARGE_INTEGER   liLastAgentStartTime;
    LARGE_INTEGER   liLastAgentStopTime;
    LARGE_INTEGER   liLastS2StartTime;
    LARGE_INTEGER   liLastS2StopTime;
    ULONGLONG       ullMemAllocFailCount;
    ULONGLONG       ullGroupingID;
    LARGE_INTEGER   liLastTagRequestTime;
    LARGE_INTEGER   liTagRequestTime;
}DRIVER_TELEMETRY, *PDRIVER_TELEMETRY;

typedef struct _BOOTVOL_NOTIFICATION_INFO
{
    #define         MAX_FAILURE 10
    volatile LONG 	bootVolumeDetected;
	KEVENT          bootNotifyEvent;
	PDEV_CONTEXT pBootDevContext;
	volatile LONG   failureCount;
	volatile LONG   reinitCount;
	volatile LONG   LockUnlockBootVolumeDisable;
} BOOTVOL_NOTIFICATION_INFO, *PBOOTVOL_NOTIFICATION_INFO; 
typedef struct _TIMEJUMP_INFO{
    LONGLONG        llExpectedTime;
    LONGLONG        llCurrentTime;
}TIMEJUMP_INFO;

typedef struct _VOLUME_NODE{
    LIST_ENTRY          ListEntry;
    UNICODE_STRING      VolumeName;
} VOLUME_NODE, *PVOLUME_NODE;

typedef struct _BOOT_DISK_INFORMATION
{
    WCHAR               wDeviceId[DEVID_LENGTH];
    UNICODE_STRING      usVendorId;
    ULONG32             BusType;
}BOOT_DISK_INFORMATION, *PBOOT_DISK_INFORMATION;

typedef struct _DRIVER_CONTEXT
{
    PDRIVER_OBJECT  DriverObject;
    UNICODE_STRING  DriverImage;
    UNICODE_STRING  DriverParameters;
    UNICODE_STRING  DefaultLogDirectory;
    UNICODE_STRING  DataLogDirectory;
    WCHAR           ZeroGUID[GUID_SIZE_IN_CHARS];   // GUID_SIZE_IN_CHARS is 36
    etServiceState  eServiceState;
    ULONG           ulNumDevs;
    ULONG           ulFlags;
    bool            bEnableDataCapture;
    BOOLEAN         bEnableDataModeCaptureForNewDevices;
    bool            bServiceSupportsDataCapture;
    BOOLEAN         bKernelTagsEnabled;

    bool            bServiceSupportsDataFiles;
    bool            bS2SupportsDataFiles;
    bool            bS2is64BitProcess;
    bool            bEnableDataFiles;

    bool            bEnableDataFilesForNewDevs;
    bool            bDisableFilteringForNewDevice;
#ifdef VOLUME_CLUSTER_SUPPORT
    bool            bDisableVolumeFilteringForNewClusterVolumes;
#endif
    bool            bEndian;

    ULONG           DBHighWaterMarks[ecServiceMaxStates]; // ecServiceMaxStates is 4

    ULONG           DBToPurgeWhenHighWaterMarkIsReached;
    ULONG           DBLowWaterMarkWhileServiceRunning;
    ULONG           ulDataToDiskLimitPerDevInMB;
    ULONG           FileWriterThreadPriority;

    // The following values limit the memory consumption of the bitmap buffers
    ULONG           MaximumBitmapBufferMemory;
    ULONG           CurrentBitmapBufferMemory;

    ULONG           Bitmap512KGranularitySize;

    // Following values are related to data buffering.
    ULONG           ulDataBufferSize;
    ULONG           ulDataBlockSize;
    ULONGLONG       ullDataPoolSize;
  
    ULONG           ulDataBlockAllocated;
                    
    ULONG           ulMaxSetLockedDataBlocks;
    ULONG           ulMaxLockedDataBlocks;        
    ULONG           ulMaxDataSizePerDev;

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
    // Global list to maintain Page File Nodes
    LIST_ENTRY      FreePageFileList;
    // A separate lock from DriverContext.Lock is used for DataBlockLists to 
    // avoid deadlock. DriverContext lock is acquired before Device context lock.
    // Device context is acquired while OrphanedMappedDataBlockList, 
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
    KSPIN_LOCK      MultiDeviceLock;
    KSPIN_LOCK      NoMemoryLogEventLock;
    LARGE_INTEGER   liLastDBallocFailAtTickCount;
    ULONG           ulSecondsBetweenNoMemoryLogEvents;
    ULONG           ulSecondsMaxWaitTimeOnFailOver;
#ifdef VOLUME_FLT
    LONG            lAdditionalGUIDs;
#endif
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
    LARGE_INTEGER   liLastScsiDirectCmdTS;
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
    // Device object to regsiter for Post Shutdown.
	PDEVICE_OBJECT          PostShutdownDO;
    NPAGED_LOOKASIDE_LIST   DirtyBlocksPool;
    NPAGED_LOOKASIDE_LIST   WorkQueueEntriesPool;
    NPAGED_LOOKASIDE_LIST   ListNodePool;
    NPAGED_LOOKASIDE_LIST   BitmapWorkItemPool;
    NPAGED_LOOKASIDE_LIST   BitRunLengthOffsetPool;
    NPAGED_LOOKASIDE_LIST   ChangeNodePool;
    NPAGED_LOOKASIDE_LIST   ChangeBlock2KPool;
    NPAGED_LOOKASIDE_LIST   CompletionContextPool;
    LIST_ENTRY              HeadForDevContext;
    LIST_ENTRY              HeadForBasicDisks;
    // HeadForTagNodes has TAG_NODEs and is protected by TagListLock
    LIST_ENTRY              HeadForTagNodes;
    KSPIN_LOCK              TagListLock;
    KSPIN_LOCK              Lock;
    // To add and remove nodes from global list
	KSPIN_LOCK              PageFileListLock;

    // DevBitmaps are allocated from non paged pool.
    LIST_ENTRY              HeadForDevBitmaps;
    ULONG                   ulNumDevBitmaps;
    ULONG                   ulMinFreeDataToDiskLimitPerDevInMB;
    ULONG           MaxCoalescedMetaDataChangeSize;
    ULONG           ulValidationLevel;

    BOOLEAN           bResyncOnReboot;
    PIRP            ProcessStartIrp;
    // UserProcess is the process that has sent ProcessStartIrp.
    // Driver maps memory into UserProcess when in data mode.
    PEPROCESS       UserProcess;
    PIRP            ServiceShutdownIrp;
    KEVENT          ServiceThreadWakeEvent;
    KEVENT          ServiceThreadShutdownEvent;
    KEVENT          StartFSFlushEvent;
    KEVENT          StopFSFlushEvent;

    //Added for persistent time stamp
    KEVENT          PersistentThreadShutdownEvent;
    PKTHREAD        pServiceThread;
    PKTHREAD        pTimeSeqFlushThread; //added for persistent time stamp
    bool            PersistantValueFlushedToRegistry; //added to stop draining changes when we have written the values in the registry.
    bool            bFlushFsPreShutdown;
    WORK_QUEUE      WorkItemQueue;

    KMUTEX          DiskRescanMutex;

    // Fields related to kernel tags.
    PCHAR           BitmapToDataModeTag;
    PCHAR           MetaDataToDataModeTag;
    PCHAR           BitmapToMetaDataModeTag;
    PCHAR           DataToMetaDataModeTag;
    USHORT          usBitmapToDataModeTagLen;
    USHORT          usMetaDataToDataModeTagLen;
    USHORT          usBitmapToMetaDataModeTagLen;
    USHORT          usDataToMetaDataModeTagLen;
    ULONG           ulNumberOfThreadsPerFileWriter;
    ULONG           ulNumberOfCommonFileWriters;
    ULONG           ulDaBFreeThresholdForFileWrite;
    ULONG           ulDaBThresholdPerDevForFileWrite;
    ULONG           ulDataNotifyThresholdInKB;
    // This is calculated at boot time based on UDIRTY_BLOCK fields.
    ULONG           ulMaxTagBufferSize;
    PVOID               SystemTimeCallbackPointer;

    CDataFileWriterManager   *DataFileWriterManager;
    bool            IsDispatchEntryPatched;
    bool            bNoRebootCompletionNotSet;  // Limit EventLog in case of Error
    bool            bNoRebootWrongDevObj;       // Limit EventLog in case of Error
    bool            bNoRebootWrongDispatchEntry;// Limit EventLog in case of Error
    PDRIVER_OBJECT  pTargetDriverObject;        // Driver Object of Patched Driver
    PDRIVER_DISPATCH TargetMajorFunction[IRP_MJ_MAXIMUM_FUNCTION + 1];
#ifdef VOLUME_NO_REBOOT_SUPPORT
    bool            IsVolumeAddedByEnumeration;
    PDRIVER_OBJECT  pDriverObject;
    PDRIVER_DISPATCH MajorFunction[IRP_MJ_MAXIMUM_FUNCTION + 1];
#endif
#ifdef VOLUME_CLUSTER_SUPPORT
    bool            IsClusterVolume;
#endif
#ifdef VOLUME_CLUSTER_SUPPORT
    PDRIVER_DISPATCH MajorFunctionForClusDrives[IRP_MJ_MAXIMUM_FUNCTION + 1];
    PDRIVER_OBJECT     pDriverObjectForCluster ;
#endif
    
// These tunable (in percentages) helps only to switch from Metadata to Data capture mode and does not have any other significance   
    ULONG           ulMaxDataAllocLimitInPercentage;
    ULONG           ulAvailableDataBlockCountInPercentage;

    ULONG               ulTagContextGUIDLength;
    UCHAR               TagContextGUID[GUID_SIZE_IN_CHARS];
    PIRP                pTagProtocolHoldIrp;

    KTIMER              TagProtocolTimer;
    KDPC                TagProtocolTimerDpc;
    PTAG_INFO           pTagInfo;

    KTIMER              TagNotifyProtocolTimer;
    KDPC                TagNotifyProtocolTimerDpc;

    etTagProtocolState  TagProtocolState;
    etTagType           TagType;
    NTSTATUS            TagStatus;
    LONGLONG            llNumQueuedIos;
    LONGLONG            llIoReleaseTimeInMicroSec;


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

    // Low non-paged pool event
    PKEVENT                     hLowNonPagedPoolCondition;

    // Low memory event
    PKEVENT                     hLowMemoryCondition;

    // Event to notify the low memory thread to exit.
    KEVENT                      hLowMemoryThreadShutdown;

    // Handle to Low Memory thread
    HANDLE                      hLowMemoryThread;

    // Variable to indicate Low Memory condition
    LONG                        bLowMemoryCondition;
    // Should be used as information
    ULONG                       ulNumOfFreePageNodes;
    ULONG                       ulNumOfAllocPageNodes;

    // App consistency protocol timer, DPC and state machine
    KTIMER                      AppTagProtocolTimer;
    KDPC                        AppTagProtocolTimerDpc;
	BOOLEAN                     bTimer;
	etDriverMode                eDiskFilterMode;
    LARGE_INTEGER               liStopFilteringAllTimestamp;
    ULONG                       ulBootCounter;
    ULONG                       ulNumFlushVolumeThreads;
    ULONG                       ulNumFlushVolumeThreadsInProgress;
    volatile LONG               lCntDevContextAllocs;
    volatile LONG               lCntDevContextFree;
    TIMEJUMP_INFO               timeJumpInfo;
    DRIVER_TELEMETRY            globalTelemetryInfo;
    PKTHREAD                    *pFlushVolumeCacheThreads;
    PKEVENT                      hFlushThreadTerminated;
    PULONG                      pProtectedDiskNumbers;
    ULONG                       ulNumProtectedDisks;
    LARGE_INTEGER               liFsFlushTimeout;
    LIST_ENTRY                  VolumeNameList;
    ULONGLONG                   ullDrvProdVer;
    ULONG                       PreShutdownDiskCount;
    ULONG                       MaxLogForLearnIrpFailures;

    VM_CX_SESSION_CONTEXT       vmCxSessionContext;

    TAG_COMMIT_NOTIFY_CONTEXT   TagNotifyContext;

    volatile bool               bTooManyLastChanceWritesHit;

    volatile LONG               SystemStateFlags;
    KMUTEX                      SystemStateRegistryMutex;

    volatile BOOLEAN            isSanWFActive;
    volatile BOOLEAN            isNoHydWFActive;

    KEVENT                      hFailoverDetectionOver;
    volatile BOOLEAN            isResourceDiskDetectionInProgress;

    bool                        isBootDeviceStarted;
    bool                        isFailoverDetected;
    BOOLEAN                     isResourceDiskDetectedOnFOVM;

    ULONG                       ulMaxResDiskDetectionTimeInMS;
    ULONG                       ulTagNotifyTimeoutInSecs;
    BOOT_DISK_INFORMATION       prevBootDiskInfo;
    BOOT_DISK_INFORMATION       bootDiskInfo;
} DRIVER_CONTEXT, *PDRIVER_CONTEXT;

extern DRIVER_CONTEXT   DriverContext;

VOID
InDskFltFlushVolumeCache(
    _In_    PVOID Context
);

NTSTATUS
InDskFltCreateFlushVolumeCachesThread();

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
#ifdef VOLUME_FLT
    volatile long  PagingPathCount;

    PFILE_OBJECT pagingFile; // track this so we can filter out writes to a pagefile

    //
    // As we may get multiple shutdown's, only respond to the first
    //
    BOOLEAN shutdownSeen;
#endif
    //
    // Dirty blocks tracked by this driver
    //

    _DEV_CONTEXT   *pDevContext;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

#define DEVICE_EXTENSION_SIZE sizeof(DEVICE_EXTENSION)
// Structure to hold page file info.
typedef struct _PAGE_FILE_NODE {
    LIST_ENTRY     ListEntry;
	_DEV_CONTEXT   *pDevContext; //No OP
	PFILE_OBJECT   pFileObject;
	volatile LONG  lRefCount;    //Gets Change in Usage Notification call
} PAGE_FILE_NODE, *PPAGE_FILE_NODE;


NTSTATUS
GetListOfDevs(PLIST_ENTRY ListHead);

NTSTATUS
OpenDriverParametersRegKey(Registry **ppDriverParam);

NTSTATUS
SetStrInParameters(PWCHAR pValueName, PUNICODE_STRING pValue);

NTSTATUS
SetDWORDInParameters(PWCHAR pValueName, ULONG ulValue);

NTSTATUS
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
ULONG GetFreeThresholdForFileWrite(PDEV_CONTEXT DevContext);
ULONG GetThresholdPerDevForFileWrite(PDEV_CONTEXT DevContext);
ULONGLONG ReqNonPagePoolMem();
ULONG ValidateDataBufferSize(ULONG ulDataBufferSize);
ULONG ValidateDataBlockSize(ULONG ulDataBlockSize);
ULONG ValidateMaxDataSizePerDataModeDirtyBlock(ULONG ulMaxDataSizePerDataModeDirtyBlock);
ULONG ValidateMaxDataSizePerNonDataModeDirtyBlock(ULONG ulMaxDataSizePerNonDataModeDirtyBlock);
ULONG ValidateMaxDataSizePerDev(ULONG ulMaxDataSizePerDev);
ULONG ValidateMaxSetLockedDataBlocks(ULONG ulMaxSetLockedDataBlocks);
ULONG ValidateFreeThresholdForFileWrite(ULONG ulDaBFreeThresholdForFileWrite);
ULONG ValidateThresholdPerDevForFileWrite(ULONG ulDaBThresholdPerDevForFileWrite);
ULONGLONG ValidateDataPoolSize(ULONGLONG ullDataPoolSize);

NTSTATUS
InDskFltQueryDriverAttribute(
    _In_    PCWSTR              wszDriverPath,
    _In_    PWCHAR              ParameterName,
    _Out_   PVOID               ParameterValue
);

NTSTATUS
InDskFltSetDriverAttribute(
_In_    PCWSTR              wszDriverPath,
_In_     PCWSTR ValueName,
_In_     ULONG  ValueType,
_In_opt_ PVOID  ValueData,
_In_     ULONG  ValueLength
);

VOID
QueryBootDisk();

NTSTATUS
InDskFltHandleSanAndNoHydWF();