///
/// @file abhaiflt.h
/// Defines shared control codes for use in sentinel .
///
#ifndef __INVOLFLT__H__
#define __INVOLFLT__H__
#include <DrvDefines.h>
#include <tchar.h>
#include <DriverTags.h>

#define INMAGE_FILTER_DOS_DEVICE_NAME   _T("\\\\.\\InMageFilter")
#define INMAGE_DEVICE_NAME              L"\\Device\\InMageFilter"

#define PARAMETERS_KEY                      L"\\Parameters"
#define KERNEL_TAGS_KEY                     L"KernelTags"
#define DEFAULT_LOG_DIRECTORY_VALUE         L"\\SystemRoot\\InMageVolumeLogs\\"

// These are parameters global for all volumes.
#define DB_HIGH_WATERMARK_SERVICE_NOT_STARTED   L"DirtyBlockHighWaterMarkServiceNotStarted"
#define DB_MAX_COALESCED_METADATA_CHANGE_SIZE	L"MaxCoalescedMetadataChangeSize"
#define DB_LOW_WATERMARK_SERVICE_RUNNING        L"DirtyBlockLowWaterMarkServiceRunning"
#define DB_VALIDATION_LEVEL	L"ValidationCheckLevel"
#define DB_HIGH_WATERMARK_SERVICE_RUNNING       L"DirtyBlockHighWaterMarkServiceRunning"
#define DB_HIGH_WATERMARK_SERVICE_SHUTDOWN      L"DirtyBlockHighWaterMarkServiceShutdown"
#define DB_TO_PURGE_HIGH_WATERMARK_REACHED      L"DirtyBlocksToPurgeWhenHighWaterMarkIsReached"
#define MAXIMUM_BITMAP_BUFFER_MEMORY            L"MaximumBitmapBufferMemory"
/*#define BITMAP_8K_GRANULARITY_SIZE              L"Bitmap8KGranularitySize"
#define BITMAP_16K_GRANULARITY_SIZE             L"Bitmap16KGranularitySize"
#define BITMAP_32K_GRANULARITY_SIZE             L"Bitmap32KGranularitySize"*/
#define BITMAP_512K_GRANULARITY_SIZE            L"Bitmap512KGranularitySize"
#define DEFAULT_LOG_DIRECTORY                   L"DefaultLogDirectory"
#define DATA_LOG_DIRECTORY                      L"DataLogDirectory"
#define DATA_BUFFER_SIZE                        L"DataBufferSize"
#define DATA_BLOCK_SIZE                         L"DataBlockSize"
#define DATA_POOL_SIZE                          L"DataPoolSize"
#define DATASIZE_LIMIT_PER_DIRTY_BLOCK          L"DataSizeLimitPerDirtyBlock"
#define DATASIZE_LIMIT_PER_VOLUME               L"DataSizeLimitPerVolume"
#define LOCKED_DATA_BLOCKS_FOR_USE_AT_DISPATCH_IRQL  L"LockedDataBlocksForUseAtDispatchIRQL"
#define VOLUME_DATA_FILTERING_FOR_NEW_VOLUMES   L"VolumeDataFilteringForNewVolumes"
#define VOLUME_DATA_FILES_FOR_NEW_VOLUMES       L"VolumeDataFilesForNewVolumes"
#define VOLUME_DATA_SIZE_LIMIT                  L"VolumeDataSizeLimit"
#define VOLUME_FILTERING_DISABLED_FOR_NEW_VOLUMES           L"VolumeFilteringDisabledForNewVolumes"
#define VOLUME_FILTERING_DISABLED_FOR_NEW_CLUSTER_VOLUMES   L"VolumeFilteringDisabledForNewClusterVolumes"
#define NUMBER_OF_THREADS_PER_FILE_WRITER       L"NumberOfThreadsPerFileWriter"
#define NUMBER_OF_COMMON_FILE_WRITERS           L"NumberOfCommonFileWriters"
#define FREE_THRESHOLD_FOR_FILE_WRITE           L"FreeThresholdForFileWrite"
#define VOLUME_THRESHOLD_FOR_FILE_WRITE         L"VolumeThresholdForFileWrite"
#define TIME_IN_SECONDS_BETWEEN_NO_MEMORY_LOG_EVENTS   L"TimeInSecondsBetweenNoMemoryLogEvents"
#define MAX_WAIT_TIME_IN_SECONDS_ON_FAIL_OVER   L"MaxWaitTimeInSecondsOnFailOver"
#define WORKER_THREAD_PRIORITY                  L"WorkerThreadPriority"
#define MAX_NON_PAGED_POOL_IN_MB                L"MaxNonPagedPoolInMB"
#define MAX_WAIT_TIME_FOR_IRP_COMPLETION_IN_MINUTES        L"MaxWaitTimeForIrpCompletionInMinutes"
#define DATA_ALLOCATION_LIMIT_IN_PERCENTAGE L"DataAllocationLimitInPercentage"
#define FREE_AND_LOCKED_DATA_BLOCKS_COUNT_IN_PERCENTAGE L"TotalFreeAndLockedDataBlocksLimitInPercentage"

#define DEFAULT_TIME_IN_SECONDS_BETWEEN_NO_MEMORY_LOG_EVENTS    3600  // 60 minutes
#define DEFAULT_MAX_TIME_IN_SECONDS_ON_FAIL_OVER                120   // 2 minute

#define DEFAULT_VOLUME_FILTERING_DISABLED_FOR_NEW_VOLUMES           0x01
// Filtering state is now stored on the Bitmap header for cluster volumes
#define DEFAULT_VOLUME_FILTERING_DISABLED_FOR_NEW_CLUSTER_VOLUMES   0x01

// This value is present at both Parameters section and Parameters\Volume section
#define VOLUME_DATA_CAPTURE                     L"VolumeDataFiltering"
#define VOLUME_DATA_FILES                       L"VolumeDataFiles"
#define VOLUME_DATA_TO_DISK_LIMIT_IN_MB         L"VolumeDataToDiskLimitInMB"
#define VOLUME_MIN_FREE_DATA_TO_DISK_LIMIT_IN_MB         L"VolumeMinFreeDataToDiskLimitInMB"
#define VOLUME_FILE_WRITER_THREAD_PRIORITY      L"VolumeFileWriterThreadPriority"
#define VOLUME_DATA_NOTIFY_LIMIT_IN_KB          L"VolumeDataNotifyLimitInKB"

#define KERNEL_TAGS_ENABLED                     L"KernelTagsEnabled"
#define DEFAULT_KERNEL_TAGS_ENABLED             0x00

#define MODE_CHANGE_TAG_DESC_FOR_BITMAP_TO_DATA         L"ModeChangeTagDescForBitmapToData"
#define MODE_CHANGE_TAG_DESC_FOR_METADATA_TO_DATA       L"ModeChangeTagDescForMetaDataToData"
#define MODE_CHANGE_TAG_DESC_FOR_BITMAP_TO_METADATA     L"ModeChangeTagDescForBitmapToMetaData"
#define MODE_CHANGE_TAG_DESC_FOR_DATA_TO_METADATA       L"ModeChangeTagDescForDataToMetaData"
#define MODE_CHANGE_TAG_DESC_FOR_METADATA_TO_BITMAP     L"ModeChangeTagDescForMetaDataToBitmap"
#define MODE_CHANGE_TAG_DESC_FOR_DATA_TO_BITMAP         L"ModeChangeTagDescForDataToBitmap"

// These are ANSI Strings.
#define MODE_CHANGE_TAG_DESC_FOR_BITMAP_TO_DATA_DEFAULT_VALUE         "Filtering mode change from bitmap to data"
#define MODE_CHANGE_TAG_DESC_FOR_METADATA_TO_DATA_DEFAULT_VALUE       "Filtering mode change from meta-data to data"
#define MODE_CHANGE_TAG_DESC_FOR_BITMAP_TO_METADATA_DEFAULT_VALUE     "Filtering mode change from bitmap to meta-data"
#define MODE_CHANGE_TAG_DESC_FOR_DATA_TO_METADATA_DEFAULT_VALUE       "Filtering mode change from data to meta-data"
#define MODE_CHANGE_TAG_DESC_FOR_METADATA_TO_BITMAP_DEFAULT_VALUE     "Filtering mode change from meta-data to bitmap"
#define MODE_CHANGE_TAG_DESC_FOR_DATA_TO_BITMAP_DEFAULT_VALUE         "Filtering mode change from data to bitmap"

// This is limit of data per dirty block.
//#define SERVICE_REQUEST_DATA_SIZE_LIMIT         L"ServiceRequestDataSizeLimit"
#define MAX_DATA_SIZE_PER_DATA_MODE_DIRTY_BLOCK         L"MaxDataSizePerDataModeDirtyBlock"
#define MAX_DATA_SIZE_PER_NON_DATA_MODE_DIRTY_BLOCK     L"MaxDataSizePerNonDataModeDirtyBlock"
#define MAX_DATA_SIZE_PER_DATA_MODE_DIRTY_BLOCK_IN_MB 16
#define MIN_DATA_SIZE_PER_DATA_MODE_DIRTY_BLOCK_IN_MB 1

#define MAX_DATA_SIZE_PER_NON_DATA_MODE_DIRTY_BLOCK_IN_MB 64
#define MIN_DATA_SIZE_PER_NON_DATA_MODE_DIRTY_BLOCK_IN_MB 1

// Mapped page writer thread has a priority of 17
// LOW_REALTIME_PRIORITY is 16. So setting priority to LOW_REALTIME_PRIORITY does not help.
#define DEFAULT_VOLUME_FILE_WRITERS_THREAD_PRIORITY     (19)    // LOW_REALTIME_PRIORITY + 3
#define MINIMUM_VOLUME_FILE_WRITERS_THREAD_PRIORITY     (9)
#define DEFAULT_NUMBER_OF_THREADS_FOR_EACH_FILE_WRITER  0x00
#define DEFAULT_NUMBER_OF_COMMON_FILE_WRITERS           0x01
#define DEFAULT_FREE_THREASHOLD_FOR_FILE_WRITE          20      // 20% = 12
#define DEFAULT_VOLUME_THRESHOLD_FOR_FILE_WRITE         25      // 25% = 8
//#define DEFAULT_BITMAP_8K_GRANULARITY_SIZE              (520)   // in GBytes
//#define DEFAULT_BITMAP_16K_GRANULARITY_SIZE             (1030)  // in GBytes
//#define DEFAULT_BITMAP_32K_GRANULARITY_SIZE             (2060)  // in GBytes
#define DEFAULT_BITMAP_512K_GRANULARITY_SIZE            (512)   // in GBytes

#define DEFAULT_LOG_DIR_FOR_CLUSDISKS           L"\\InMageClusterLogs\\"
#define DEFAULT_DATA_LOG_DIR_FOR_CLUSDISKS      L"\\InMageClusterLogs\\DataLogs\\"
#define DEFAULT_WORKER_THREAD_PRIORITY     (12)    // Matching UnMount thread priority

// These are parameters per volume
#define VOLUME_LETTER                           L"VolumeLetter"                 //REG_STRING
#define VOLUME_FILTERING_DISABLED               L"VolumeFilteringDisabled"      //REG_DWORD
#define VOLUME_BITMAP_READ_DISABLED             L"VolumeBitmapReadDisabled"     //REG_DWORD
#define VOLUME_BITMAP_WRITE_DISABLED            L"VolumeBitmapWriteDisabled"    //REG_DWORD
#define VOLUME_PAGEFILE_WRITES_IGNORED          L"VolumePagefileWritesIgnored"  //REG_DWORD
#define VOLUME_BITMAP_GRANULARITY               L"VolumeBitmapGranularity"      //REG_DWORD
#define VOLUME_BITMAP_OFFSET                    L"VolumeBitmapOffset"           //REG_DWORD
#define VOLUME_RESYNC_REQUIRED                  L"VolumeResyncRequired"
#define VOLUME_OUT_OF_SYNC_COUNT                L"VolumeOutOfSyncCount"
#define VOLUME_OUT_OF_SYNC_TIMESTAMP            L"VolumeOutOfSyncTimestamp"     //FORMAT MM:DD:YYYY::HH:MM:SS:XXX(Milli Seconds)
#define VOLUME_OUT_OF_SYNC_TIMESTAMP_IN_GMT     L"VolumeOutOfSyncTimestampInGMT"     //FORMAT LongLong
#define VOLUME_OUT_OF_SYNC_ERRORCODE            L"VolumeOutOfSyncErrorCode"
#define VOLUME_OUT_OF_SYNC_ERROR_DESCRIPTION    L"VolumeOutOfSyncErrorDescription"
#define VOLUME_OUT_OF_SYNC_ERRORSTATUS          L"VolumeOutOfSyncErrorStatus"
#define VOLUME_LOG_PATH_NAME                    L"LogPathName"
#define VOLUME_DATALOG_DIRECTORY                L"VolumeDataLogDirectory"
#define VOLUME_READ_ONLY                        L"VolumeReadOnly"
#define VOLUME_FILE_WRITER_ID                   L"VolumeFileWriterId"

#define VOLUME_BITMAP_GRANULARITY_DEFAULT_VALUE         (0x1000)
#define VOLUME_BITMAP_OFFSET_DEFAULT_VALUE              (0)

#define VOLUME_FILTERING_DISABLED_SET                   (1)
#define VOLUME_FILTERING_DISABLED_UNSET                 (0)

#define VOLUME_DATA_CAPTURE_SET                         (1)
#define VOLUME_DATA_CAPTURE_UNSET                       (0)
// This value enables and disables data capture for all volumes.
// If this is set to 0, data capture is disabled for all volumes
#define DEFAULT_VOLUME_DATA_CAPTURE                     (1)     // FALSE

#define VOLUME_DATA_FILES_SET                           (1)
#define VOLUME_DATA_FILES_UNSET                         (0)
#define DEFAULT_VOLUME_DATA_FILES                       (0)     // FALSE

#define VOLUME_BITMAP_READ_DISABLED_SET                 (1)
#define VOLUME_BITMAP_READ_DISABLED_UNSET               (0)
#define VOLUME_BITMAP_READ_DISABLED_DEFAULT_VALUE       (0)     // FALSE


#define VOLUME_BITMAP_WRITE_DISABLED_SET                (0)
#define VOLUME_BITMAP_WRITE_DISABLED_UNSET              (1)
#define VOLUME_BITMAP_WRITE_DISABLED_DEFAULT_VALUE      (0)     // FALSE

#define VOLUME_PAGEFILE_WRITES_IGNORED_SET              (1)
#define VOLUME_PAGEFILE_WRITES_IGNORED_UNSET            (0)
#define VOLUME_PAGEFILE_WRITES_IGNORED_DEFAULT_VALUE    (1)     // TRUE

#define VOLUME_READ_ONLY_SET                            (1)
#define VOLUME_READ_ONLY_UNSET                          (0)
#define VOLUME_READ_ONLY_DEFAULT_VALUE                  (0)     // FALSE

#define DEFAULT_VOLUME_RESYNC_REQUIRED_VALUE            (0)     // FALSE
#define VOLUME_RESYNC_REQUIRED_SET                      (1)     // TRUE

#define DEFAULT_VOLUME_OUT_OF_SYNC_COUNT                (0)
#define DEFAULT_VOLUME_OUT_OF_SYNC_ERRORCODE            (0)
#define DEFAULT_VOLUME_OUT_OF_SYNC_ERRORSTATUS          (0)

#define DEFAULT_VOLUME_DATA_TO_DISK_LIMIT_IN_MB             (512)       // 512 MB
#define DEFAULT_VOLUME_MIN_FREE_DATA_TO_DISK_LIMIT_IN_MB    (128)       // 128 MB
#ifdef _WIN64
#define DEFAULT_VOLUME_DATA_NOTIFY_LIMIT_IN_KB    (0x2000)      // 8 MB
#define DEFAULT_MAX_NON_PAGED_POOL_IN_MB          (32)          // 32 MB
#else
#define DEFAULT_VOLUME_DATA_NOTIFY_LIMIT_IN_KB    (4072)        // 4 MB - 24K (for stream header overhead)
#define DEFAULT_MAX_NON_PAGED_POOL_IN_MB          (12)          // 12 MB
#endif // _WIN64

#define MAX_DATA_SIZE_PER_VOL_PERC 80

                                                                // 
// Error Numbers logged to registry when volume is out of sync.
#define ERROR_TO_REG_OUT_OF_MEMORY_FOR_DIRTY_BLOCKS         0x0001
#define ERROR_TO_REG_BITMAP_READ_ERROR                      0x0002
#define ERROR_TO_REG_BITMAP_WRITE_ERROR                     0x0003
#define ERROR_TO_REG_BITMAP_OPEN_ERROR                      0x0004
#define ERROR_TO_REG_BITMAP_OPEN_FAIL_CHANGES_LOST          0x0005
//Fix for bug 25726
#define ERROR_TO_REG_MISSED_PNP_VOLUME_REMOVE               0x0006
#define ERROR_TO_REG_DESCRIPTION_IN_EVENT_LOG               0x0007

#define ERROR_TO_REG_MAX_ERROR                              ERROR_TO_REG_DESCRIPTION_IN_EVENT_LOG

/*------------------------------------------------------------------------------
 * This array is defined in DirtyBlock.cpp changes to have to be made at both
 * the places
 *------------------------------------------------------------------------------
WCHAR *ErrorToRegErrorDescriptions[ERROR_TO_REG_MAX_ERROR] = {
    NULL,
    L"Out of NonPagedPool for dirty blocks",
    L"Bit map read failed with Status %#x",
    L"Bit map write failed with Status %#x",
    L"Bit map open failed with Status %#x",
    L"Bit map open failed and could not write changes",
    L"See event viewer for error description"
    };
*/

typedef enum _etTagStatus
{
    // etTagStatusCommited is a successful state
    ecTagStatusCommited = 0,
    // etTagStatus is initialized with ecTagStatusPending.
    ecTagStatusPending = 1,
    // etTagStatusDeleted - Tag is deleted due to StopFiltering or ClearDiffs
    ecTagStatusDeleted = 2,
    // etTagStatusDropped - Tag is dropped due to write in Bitmap file.
    ecTagStatusDropped = 3,
    // ecTagStatusInvalidGUID
    ecTagStatusInvalidGUID = 4,
    // ecTagStatusFilteringStopped is returned if volume filtering is stopped
    ecTagStatusFilteringStopped = 5,
    // ecTagStatusUnattempted - fail to generate tag
    //  for example - 
    // volume filtering is stopped for some another volume in the volume list,
    //          so no tag is added to this volume, and ecTagStatusUnattempted is returned.
    // GUID is invalid for some another volume in the volume list,
    //          so no tag is added to this volume, and ecTagStatusUnattempted is returned.
    ecTagStatusUnattempted = 6,
    // ecTagStatusFailure - Some error occurs while adding tags. Such as
    // memory allocation failure, ..
    ecTagStatusFailure = 7,
    ecTagStatusMaxEnum = 8
} etTagStatus, *petTagStatus;

typedef enum _etServiceState
{
    ecServiceUnInitiliazed = 0,
// state is set to ecServiceNotStarted by driver at initialization 
// time, this state is removed once the driver receives shutdown notify
// IRP.
    ecServiceNotStarted = 1,
// state is set to ecServiceRunning if driver receives the shutdown IRP.
// This tells us that service has started and registered with driver.
    ecServiceRunning = 2,
// state is set to ecServiceShutdown if driver receives a cancel of 
// SHUTDOWN IRP. This tells that service has shutdown for some reason.
    ecServiceShutdown = 3,
    // This is used as the size of array if some state has to be stored for each
    // service state, and what to be indexed on service state instead of switching
    // on state value.
    ecServiceMaxStates = 4,
} etServiceState, *petServiceState;

typedef enum _etVBitmapState {
    ecVBitmapStateUnInitialized = 0,
    ecVBitmapStateOpened = 1,   // Set to this state as soon as the bitmap is opened.
    ecVBitmapStateReadStarted = 2,        // Set to this state when worker routine is queued for first read.
    ecVBitmapStateReadPaused = 3,
    ecVBitmapStateAddingChanges = 4,
    ecVBitmapStateReadCompleted = 5,
    ecVBitmapStateClosed = 6,
    ecVBitmapStateReadError = 7,
    ecVBitmapStateInternalError = 8
} etVBitmapState, *petVBitmapState;

// TODO: This needs to be removed
typedef enum _etFilteringMode
{
    ecFilteringModeUninitialized = 0,
    ecFilteringModeStartedNoIO = 1,
    ecFilteringModeBitmap = 2,
    ecFilteringModeMetaData = 3,
    ecFilteringModeData = 4
} etFilteringMode, *petFilteringMode;

typedef enum _etCaptureMode
{
    ecCaptureModeUninitialized = 0,
    ecCaptureModeMetaData = 1,
    ecCaptureModeData = 2,
} etCaptureMode, *petCaptureMode;

typedef enum _etWriterOrderState
{
    ecWriteOrderStateUnInitialized = 0,
    ecWriteOrderStateBitmap = 1,
    ecWriteOrderStateMetadata = 2,
    ecWriteOrderStateData = 3
} etWriteOrderState, *petWriteOrderState;

typedef enum _etWOSChangeReason
{
    ecWOSChangeReasonUnInitialized = 0,
    eCWOSChangeReasonServiceShutdown = 1,
    ecWOSChangeReasonBitmapChanges = 2,
    ecWOSChangeReasonBitmapNotOpen = 3,
    ecWOSChangeReasonBitmapState = 4,
    ecWOSChangeReasonCaptureModeMD = 5,
    ecWOSChangeReasonMDChanges = 6,
    ecWOSChangeReasonDChanges = 7,
}etWOSChangeReason, *petWOSChangeReason;

typedef enum ShudownMarker {
    Uninitialized = 0,
    CleanShutdown = 1,
    DirtyShutdown = 2,
}ShutdownMarker, *pShutdownMarker;;   

//
// TODO: consider using IoRegisterDeviceInterface(); also see Q262305.
//
#define VOLUME_LETTER_IN_CHARS                   2
#define VOLUME_NAME_SIZE_IN_CHARS               44
#define VOLUME_NAME_SIZE_IN_BYTES              0x5A

// Device types in the range of 32768+ are allowed for non MS use; similarly for functions 2048-4095
#define FILE_DEVICE_INMAGE   0x00008001
#define VOLSNAPCONTROLTYPE                              0x00000053 // 'S'
#define IOCTL_INMAGE_PROCESS_START_NOTIFY       CTL_CODE( FILE_DEVICE_INMAGE, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define IOCTL_INMAGE_SERVICE_SHUTDOWN_NOTIFY    CTL_CODE( FILE_DEVICE_INMAGE, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS )

// This IOCTL can be sent on control device object name InMageFilter or can be sent on Volume Object.
// When sent on Volume device the bitmap to be cleared is implicit on the volume device the IOCTL is sent the bitmap is cleared
// When sent on Control device the bitmap volumes GUID has to be sent in InputBuffer.
// GUID format is a2345678-b234-c234-d234-e23456789012
#define IOCTL_INMAGE_CLEAR_DIFFERENTIALS        CTL_CODE( FILE_DEVICE_INMAGE, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS )

// This IOCTL can be sent on control device object name InMageFilter or can be sent on Volume Object.
// When sent on Volume device, the device to stop filtering is implicit and the device on which the IOCTL 
// is received is stop filtering
// When sent on Control device the volumes GUID has to be sent in InputBuffer.
// GUID format is a2345678-b234-c234-d234-e23456789012
// This IOCTL is considered as nop if IOCTL Is stop filtering and volume is already stopped from filtering
// Similarly for Start filtering if volume is actively fitlering START filtering is considered as nop.
#define IOCTL_INMAGE_STOP_FILTERING_DEVICE       CTL_CODE( FILE_DEVICE_INMAGE, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define IOCTL_INMAGE_START_FILTERING_DEVICE      CTL_CODE( FILE_DEVICE_INMAGE, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS )

// This IOCTL can be sent on control device object name InMageFilter or can be sent on Volume Object.
// When sent on Volume device, the driver sends data regarding the volume on which the IOCTL is received
// When sent on Control device, driver sends data regarding all volumes.
#define IOCTL_INMAGE_GET_VOLUME_STATS           CTL_CODE( FILE_DEVICE_INMAGE, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS )

#define IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS     CTL_CODE( FILE_DEVICE_INMAGE, 0x806, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS  CTL_CODE( FILE_DEVICE_INMAGE, 0x807, METHOD_BUFFERED, FILE_ANY_ACCESS )

// This IOCTL can be sent only on control device object name InMageFilter.
// InputBuffer is VOLUME_FLAGS_INPUT and OutputBuffer is VOLUME_FLAGS_OUTPUT
// Output buffer is optional, if present driver returns current volume flags after the current bit operation.
#define IOCTL_INMAGE_SET_VOLUME_FLAGS           CTL_CODE( FILE_DEVICE_INMAGE, 0x808, METHOD_BUFFERED, FILE_ANY_ACCESS )
// This IOCTL can be sent only on control device object name InMageFilter.
// InputBuffer is optional, if present driver contains GUID of the volume.
#define IOCTL_INMAGE_CLEAR_VOLUME_STATS         CTL_CODE( FILE_DEVICE_INMAGE, 0x809, METHOD_BUFFERED, FILE_ANY_ACCESS )

// These IOCTL are sent only on control device object name InMageFilter
// Input is DiskSignature of length 4 bytes, output is NULL
#define IOCTL_INMAGE_MSCS_DISK_ADDED_TO_CLUSTER     CTL_CODE( FILE_DEVICE_INMAGE, 0x80A, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define IOCTL_INMAGE_MSCS_DISK_REMOVED_FROM_CLUSTER CTL_CODE( FILE_DEVICE_INMAGE, 0x80B, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define IOCTL_INMAGE_MSCS_DISK_ACQUIRE          CTL_CODE( FILE_DEVICE_INMAGE, 0x80C, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define IOCTL_INMAGE_MSCS_DISK_RELEASE          CTL_CODE( FILE_DEVICE_INMAGE, 0x80D, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define IOCTL_INMAGE_MSCS_GET_ONLINE_DISKS          CTL_CODE( FILE_DEVICE_INMAGE, 0x854, METHOD_BUFFERED, FILE_ANY_ACCESS )

/**
 * This IOCTL returns
 * STATUS_NOT_FOUND - if TAG_VOLUME_INPUT_FLAGS_NO_ERROR_IF_GUID_NOT_FOUND specifed and input GUID is not found.
 */
#define IOCTL_INMAGE_TAG_VOLUME                 CTL_CODE( FILE_DEVICE_INMAGE, 0x80E, METHOD_BUFFERED, FILE_ANY_ACCESS )
/**
 * This IOCTL is used to set Dirty block notification event on a given volume. Input buffer for this IOCTL contains
 * GUID of the volume, and notification handle. If IOCTL is sent on volume device instead of control device, GUID is
 * ignored. Output buffer is NULL.
 * */
#define IOCTL_INMAGE_SET_DIRTY_BLOCK_NOTIFY_EVENT   CTL_CODE( FILE_DEVICE_INMAGE, 0x80F, METHOD_BUFFERED, FILE_ANY_ACCESS )

#define IOCTL_INMAGE_SET_DATA_FILE_THREAD_PRIORITY  CTL_CODE( FILE_DEVICE_INMAGE, 0x810, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define IOCTL_INMAGE_SET_IO_SIZE_ARRAY              CTL_CODE( FILE_DEVICE_INMAGE, 0x811, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define IOCTL_INMAGE_SET_DATA_SIZE_TO_DISK_LIMIT    CTL_CODE( FILE_DEVICE_INMAGE, 0x812, METHOD_BUFFERED, FILE_ANY_ACCESS )

#define IOCTL_INMAGE_SET_DRIVER_FLAGS               CTL_CODE( FILE_DEVICE_INMAGE, 0x813, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define IOCTL_INMAGE_SET_DATA_NOTIFY_LIMIT          CTL_CODE( FILE_DEVICE_INMAGE, 0x814, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define IOCTL_INMAGE_SET_DRIVER_CONFIGURATION       CTL_CODE( FILE_DEVICE_INMAGE, 0x815, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define IOCTL_INMAGE_SET_RESYNC_REQUIRED            CTL_CODE( FILE_DEVICE_INMAGE, 0x816, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define IOCTL_INMAGE_GET_DATA_MODE_INFO             CTL_CODE( FILE_DEVICE_INMAGE, 0x817, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define IOCTL_INMAGE_RESYNC_START_NOTIFICATION      CTL_CODE( FILE_DEVICE_INMAGE, 0x818, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define IOCTL_INMAGE_RESYNC_END_NOTIFICATION        CTL_CODE( FILE_DEVICE_INMAGE, 0x819, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define IOCTL_INMAGE_SET_WORKER_THREAD_PRIORITY     CTL_CODE( FILE_DEVICE_INMAGE, 0x81A, METHOD_BUFFERED, FILE_ANY_ACCESS )

#define IOCTL_INMAGE_GET_VERSION                    CTL_CODE( FILE_DEVICE_INMAGE, 0x820, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define IOCTL_INMAGE_CUSTOM_COMMAND                 CTL_CODE( FILE_DEVICE_INMAGE, 0x821, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define IOCTL_INMAGE_GET_VOLUME_WRITE_ORDER_STATE   CTL_CODE( FILE_DEVICE_INMAGE, 0x822, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define IOCTL_INMAGE_GET_VOLUME_SIZE                CTL_CODE( FILE_DEVICE_INMAGE, 0x823, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define IOCTL_INMAGE_GET_VOLUME_FLAGS               CTL_CODE( FILE_DEVICE_INMAGE, 0x824, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define IOCTL_INMAGE_SYNC_TAG_VOLUME                CTL_CODE( FILE_DEVICE_INMAGE, 0x825, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define IOCTL_INMAGE_GET_TAG_VOLUME_STATUS          CTL_CODE( FILE_DEVICE_INMAGE, 0x826, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define IOCTL_INMAGE_GET_ADDITIONAL_VOLUME_STATS    CTL_CODE( FILE_DEVICE_INMAGE, 0x827, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define IOCTL_INMAGE_GET_GLOBAL_TIMESTAMP           CTL_CODE( FILE_DEVICE_INMAGE, 0x828, METHOD_BUFFERED, FILE_ANY_ACCESS )

#define IOCTL_INMAGE_SET_DEBUG_INFO                 CTL_CODE( FILE_DEVICE_INMAGE, 0x850, METHOD_BUFFERED, FILE_ANY_ACCESS )

#if DBG
#define IOCTL_INMAGE_ADD_DIRTY_BLOCKS_TEST          CTL_CODE( FILE_DEVICE_INMAGE, 0x852, METHOD_BUFFERED, FILE_ANY_ACCESS )
#endif // DBG
#define IOCTL_INMAGE_SET_MIN_FREE_DATA_SIZE_TO_DISK_LIMIT    CTL_CODE( FILE_DEVICE_INMAGE, 0x853, METHOD_BUFFERED, FILE_ANY_ACCESS )

#define IOCTL_INMAGE_GET_LCN CTL_CODE( FILE_DEVICE_INMAGE, 0x855, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_VOLSNAP_COMMIT_SNAPSHOT CTL_CODE(VOLSNAPCONTROLTYPE, 4, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define VOLUME_FLAG_IGNORE_PAGEFILE_WRITES  0x00000001
#define VOLUME_FLAG_DISABLE_BITMAP_READ     0x00000002
#define VOLUME_FLAG_DISABLE_BITMAP_WRITE    0x00000004
#define VOLUME_FLAG_READ_ONLY               0x00000008

#define VOLUME_FLAG_DISABLE_DATA_CAPTURE    0x00000010
#define VOLUME_FLAG_FREE_DATA_BUFFERS       0x00000020
#define VOLUME_FLAG_DISABLE_DATA_FILES      0x00000040
#define VOLUME_FLAG_VCFIELDS_PERSISTED      0x00000080
#define VOLUME_FLAG_ENABLE_SOURCE_ROLLBACK  0x00000100
#define VOLUME_FLAG_ONLINE                  0x00000200
#define VOLUME_FLAG_DATA_LOG_DIRECTORY      0x00000400
#define VOLUME_FLAGS_VALID                  0x000007FF

#define SHUTDOWN_NOTIFY_FLAGS_ENABLE_DATA_FILTERING 0x00000001
#define SHUTDOWN_NOTIFY_FLAGS_ENABLE_DATA_FILES     0x00000002

typedef struct _SHUTDOWN_NOTIFY_INPUT
{
    ULONG   ulFlags;        // 0x00
    // 64BIT : Keep structure size as multiple of 8 bytes
    ULONG   ulReserved;     // 0x04
                            // 0x08
} SHUTDOWN_NOTIFY_INPUT, *PSHUTDOWN_NOTIFY_INPUT;

#define PROCESS_START_NOTIFY_INPUT_FLAGS_DATA_FILES_AWARE   0x0001
#define PROCESS_START_NOTIFY_INPUT_FLAGS_64BIT_PROCESS      0x0002

#define UDIRTY_BLOCK_MAX_DATA_LOG_DIRECTORY 0x400
//UDIRTY_BLOCK_MAX_FILE_NAME -2(//)-37(GUID + 1)-2(//)-50(pre_completed_diff 32 >> 50)-80(20 * 4, 20 is string for 64bit number)-2(conituetag)-594(ExtraBuffer)

typedef struct _PROCESS_START_NOTIFY_INPUT
{
    ULONG   ulFlags;        // 0x00
    // 64BIT : Keep structure size as multiple of 8 bytes
    ULONG   ulReserved;     // 0x04
                            // 0x08
} PROCESS_START_NOTIFY_INPUT, *PPROCESS_START_NOTIFY_INPUT;

typedef struct _VOLUME_FLAGS_INPUT
{
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS + 2];     // 0x00
    // if eOperation is BitOpSet the bits in ulVolumeFlags will be set
    // if eOperation is BitOpReset the bits in ulVolumeFlags will be unset
    etBitOperation  eOperation;                     // 0x4C
    ULONG   ulVolumeFlags;                          // 0x50
    struct {
        USHORT  usLength;                                   // 0x54 Filename length in bytes not including NULL
        WCHAR   FileName[UDIRTY_BLOCK_MAX_DATA_LOG_DIRECTORY];  // 0x56
    } DataFile;
    // 64BIT : Keep structure size as multiple of 8 bytes
    ULONG   ulReserved[2];                             // 0x856
    CHAR    cReserved[2];                              // 0x86E
} VOLUME_FLAGS_INPUT, *PVOLUME_FLAGS_INPUT;            // 0x870

typedef struct _VOLUME_FLAGS_OUTPUT
{
    ULONG   ulVolumeFlags;      //  0x00
    // 64BIT : Keep structure size as multiple of 8 bytes
    ULONG   ulReserved;         // 0x04
                                // 0x08
} VOLUME_FLAGS_OUTPUT, *PVOLUME_FLAGS_OUTPUT;

#define DRIVER_FLAG_DISABLE_DATA_FILES                  0x00000001
#define DRIVER_FLAG_DISABLE_DATA_FILES_FOR_NEW_VOLUMES  0x00000002
#define DRIVER_FLAGS_VALID                              0x00000003

typedef struct _DRIVER_FLAGS_INPUT
{
    // if eOperation is BitOpSet the bits in ulFlags will be set
    // if eOperation is BitOpReset the bits in ulFlags will be unset
    etBitOperation  eOperation;     // 0x00
    ULONG   ulFlags;                // 0x04
                                    // 0x08
} DRIVER_FLAGS_INPUT, *PDRIVER_FLAGS_INPUT;

typedef struct _DRIVER_FLAGS_OUTPUT
{
    ULONG   ulFlags;        // 0x00
    // 64BIT : Keep structure size as multiple of 8 bytes
    ULONG   ulReserved;     // 0x04
                            // 0x08
} DRIVER_FLAGS_OUTPUT, *PDRIVER_FLAGS_OUTPUT;


//
// From our ioctl, we return the dirty blocks in a fixed length structure
// If this does not exhaust our dirty blocks (which we accumulate in a queue), the sentinel fetches more.
//
#define MAX_UDIRTY_CHANGES 1024

//We are keeping the total number of changes in the Dirty block as 1024
//This will increase the memory use per dirty block by 8K
//Due to this, per io time stamp changes are done only for WIN64
#define MAX_UDIRTY_CHANGES_V2 1024


typedef struct _DISK_CHANGE
{
    LARGE_INTEGER   ByteOffset;             // 0x00
    ULONG           Length;                 // 0x08
    USHORT          usBufferIndex;          // 0x0C
    USHORT          usNumberOfBuffers;      // 0x0E
                                            // 0x10
} DISK_CHANGE, DiskChange, *PDISK_CHANGE;

#define DB_FLAGS_COMMIT_FAILED      0x0001



/*

Tag Structure
 _________________________________________________________________________
|                   |              |                        | Padding     |
| Tag Header        |  Tag Size    | Tag Data               | (4Byte      |
|__(4 / 8 Bytes)____|____(2 Bytes)_|__(Tag Size Bytes)______|__ Alignment)|

Tag Size doesnot contain the padding.
But the length in Tag Header contains the total tag length including padding.
i.e. Tag length in header = Tag Header size + 2 bytes Tag Size + Tag Data Length  + Padding
*/

#define TAG_VOLUME_INPUT_FLAGS_ATOMIC_TO_VOLUME_GROUP           0x0001
// If this flag is set. IOCTL is returned immediately with out
// waiting for tag to commit
#define TAG_VOLUME_INPUT_FLAGS_NO_WAIT_FOR_COMMIT               0x0002
// If this flag is set. IOCTL ignores the volume if GUID Is not found
// If this flag is not set. IOCTL returns error if volume GUID Is not found
#define TAG_VOLUME_INPUT_FLAGS_NO_ERROR_IF_GUID_NOT_FOUND       0x0004
// If this flag is set. IOCTL ignores the volume if filtering is not on
// If this flag is not set. IOCTL returns error if volume filtering is not on
#define TAG_VOLUME_INPUT_FLAGS_NO_ERROR_IF_FILTERING_STOPPED    0x0008
// If this flag is set. driver waits till IOCTL_VOLSNAP_COMMIT_SNAPSHOT to be
// received from VSS system module to apply the tag to volumes.
#define TAG_VOLUME_INPUT_FLAGS_DEFER_TILL_COMMIT_SNAPSHOT    0x0010

typedef struct _TAG_VOLUME_STATUS_OUTPUT_DATA
{
    etTagStatus Status;
    ULONG       ulNoOfVolumes;
    etTagStatus VolumeStatus[1];
} TAG_VOLUME_STATUS_OUTPUT_DATA, *PTAG_VOLUME_STATUS_OUTPUT_DATA;

#define SIZE_OF_TAG_VOLUME_STATUS_OUTPUT_DATA(x) ((ULONG)FIELD_OFFSET(TAG_VOLUME_STATUS_OUTPUT_DATA, VolumeStatus[x]))

typedef struct _TAG_VOLUME_STATUS_INPUT_DATA
{
    GUID        TagGUID;
} TAG_VOLUME_STATUS_INPUT_DATA, *PTAG_VOLUME_STATUS_INPUT_DATA;

/*
Input Tag Stream Structure
 _______________________________________________________________________________________________________________________________________________
|            |           |                |                 |    |              |              |         |          |     |         |           |
|  TagGUID   |   Flags   | No. of GUIDs(n)|  Volume GUID1   | ...| Volume GUIDn | Tag Len (k)  |Tag1 Size| Tag1 data| ... |Tagk Size| Tagk Data |
|_(16 Bytes)_|_(4 Bytes)_|____(4 Bytes)__ |____(36 Bytes)___|____|______________|__(4 Bytes)___|_2 Bytes_|__________|_____|_________|___________|

*/
typedef struct _SYNC_TAG_INPUT_DATA
{
    GUID        TagGUID;
    ULONG       ulFlags;
    ULONG       ulNumberOfVolumeGUIDs;
    // an array of wGUID[GUID_SIZE_IN_CHARS]
    // ULONG    ulNumberOfTags;
    // an array of Tag Structures (USHORT usTagSize, BYTE TagData[usTagSize]
} SYNC_TAG_INPUT_DATA, *PSYNC_TAG_INPUT_DATA;

#ifndef INVOFLT_STREAM_FUNCTIONS
#define INVOFLT_STREAM_FUNCTIONS

typedef struct _STREAM_REC_HDR_4B
{
    USHORT      usStreamRecType;    // 0x00
    UCHAR       ucFlags;            // 0x02
    UCHAR       ucLength;           // 0x03 Length includes size of this header too.
                                    // 0x04
} STREAM_REC_HDR_4B, *PSTREAM_REC_HDR_4B;

typedef struct _STREAM_REC_HDR_8B
{
    USHORT      usStreamRecType;    // 0x00
    UCHAR       ucFlags;            // 0x02 STREAM_REC_FLAGS_LENGTH_BIT bit is set for this record.
    UCHAR       ucReserved;         // 0x03
    ULONG       ulLength;           // 0x04 Length includes size of this header too.
                                    // 0x08
} STREAM_REC_HDR_8B, *PSTREAM_REC_HDR_8B;

#define FILL_STREAM_HEADER_4B(pHeader, Type, Len)           \
{                                                           \
    ASSERT((ULONG)Len < (ULONG)0xFF);                       \
    ((PSTREAM_REC_HDR_4B)pHeader)->usStreamRecType = Type;  \
    ((PSTREAM_REC_HDR_4B)pHeader)->ucFlags = 0;             \
    ((PSTREAM_REC_HDR_4B)pHeader)->ucLength = Len;          \
}

#define FILL_STREAM_HEADER_8B(pHeader, Type, Len)           \
{                                                           \
    ASSERT((ULONG)Len > (ULONG)0xFF);                       \
    ((PSTREAM_REC_HDR_8B)pHeader)->usStreamRecType = Type;  \
    ((PSTREAM_REC_HDR_8B)pHeader)->ucFlags = STREAM_REC_FLAGS_LENGTH_BIT;             \
    ((PSTREAM_REC_HDR_8B)pHeader)->ucReserved = 0;          \
    ((PSTREAM_REC_HDR_8B)pHeader)->ulLength = Len;          \
}

#define STREAM_REC_FLAGS_LENGTH_BIT         0x01

#define GET_STREAM_LENGTH(pHeader)                              \
    ( (((PSTREAM_REC_HDR_4B)pHeader)->ucFlags & STREAM_REC_FLAGS_LENGTH_BIT) ?      \
                (((PSTREAM_REC_HDR_8B)pHeader)->ulLength) :                         \
                (((PSTREAM_REC_HDR_4B)pHeader)->ucLength))


#endif



#define FILL_STREAM_HEADER(pHeader, Type, Len)                  \
{                                                               \
    if((ULONG)Len > (ULONG)0xFF) {                              \
        ((PSTREAM_REC_HDR_8B)pHeader)->usStreamRecType = Type;  \
        ((PSTREAM_REC_HDR_8B)pHeader)->ucFlags = STREAM_REC_FLAGS_LENGTH_BIT;             \
        ((PSTREAM_REC_HDR_8B)pHeader)->ucReserved = 0;          \
        ((PSTREAM_REC_HDR_8B)pHeader)->ulLength = Len;          \
    } else {                                                    \
        ((PSTREAM_REC_HDR_4B)pHeader)->usStreamRecType = Type;  \
        ((PSTREAM_REC_HDR_4B)pHeader)->ucFlags = 0;             \
        ((PSTREAM_REC_HDR_4B)pHeader)->ucLength = (UCHAR)(Len);   \
    }                                                           \
}

#define FILL_STREAM(pHeader, Type, Len, pData)                  \
{                                                               \
    if((ULONG)Len > (ULONG)0xFF) {                              \
        ((PSTREAM_REC_HDR_8B)pHeader)->usStreamRecType = Type;  \
        ((PSTREAM_REC_HDR_8B)pHeader)->ucFlags = STREAM_REC_FLAGS_LENGTH_BIT;             \
        ((PSTREAM_REC_HDR_8B)pHeader)->ucReserved = 0;          \
        ((PSTREAM_REC_HDR_8B)pHeader)->ulLength = Len;          \
        RtlCopyMemory(((PUCHAR)pHeader) + sizeof(PSTREAM_REC_HDR_8B), pData, Len);  \
    } else {                                                    \
        ((PSTREAM_REC_HDR_4B)pHeader)->usStreamRecType = Type;  \
        ((PSTREAM_REC_HDR_4B)pHeader)->ucFlags = 0;             \
        ((PSTREAM_REC_HDR_4B)pHeader)->ucLength = (UCHAR)Len;   \
        RtlCopyMemory(((PUCHAR)pHeader) + sizeof(PSTREAM_REC_HDR_4B), pData, Len);  \
    }                                                           \
}



#define STREAM_REC_SIZE(pHeader)                              \
    ( (((PSTREAM_REC_HDR_4B)pHeader)->ucFlags & STREAM_REC_FLAGS_LENGTH_BIT) ?      \
                (((PSTREAM_REC_HDR_8B)pHeader)->ulLength) :                         \
                (((PSTREAM_REC_HDR_4B)pHeader)->ucLength))
#define STREAM_REC_TYPE(pHeader)    ((pHeader->usStreamRecType & TAG_TYPE_MASK) >> 0x14)
#define STREAM_REC_ID(pHeader)  (((PSTREAM_REC_HDR_4B)pHeader)->usStreamRecType)
#define STREAM_REC_HEADER_SIZE(pHeader) ( (((PSTREAM_REC_HDR_4B)pHeader)->ucFlags & STREAM_REC_FLAGS_LENGTH_BIT) ?  sizeof(STREAM_REC_HDR_8B) : sizeof(STREAM_REC_HDR_4B) )
#define STREAM_REC_DATA_SIZE(pHeader)   (STREAM_REC_SIZE(pHeader) - STREAM_REC_HEADER_SIZE(pHeader))
#define STREAM_REC_DATA(pHeader)    ((PUCHAR)pHeader + STREAM_REC_HEADER_SIZE(pHeader))

#define GET_ALIGNMENT_BOUNDARY(x,alignBoundary) ((((x) + alignBoundary -1 )/alignBoundary)*alignBoundary)

#define COMPUTE_HEADER_SIZE_FROM_STREAM_DATA_SIZE(Len)  ((Len + sizeof(STREAM_REC_HDR_4B)) < 0xFF ? sizeof(STREAM_REC_HDR_4B) : sizeof(STREAM_REC_HDR_8B))

// Common structure name used earlier but as part of supporting Global Sequence number for 32-bit systems
// it has made to use V2 structures and kept other structure for backward compatiablility.
typedef struct _TIME_STAMP_TAG_V2
{
    STREAM_REC_HDR_4B    Header;
    ULONG                Reserved;
    ULONGLONG   ullSequenceNumber;
    ULONGLONG   TimeInHundNanoSecondsFromJan1601;
} TIME_STAMP_TAG_V2, *PTIME_STAMP_TAG_V2;

typedef struct _TIME_STAMP_TAG
{
    STREAM_REC_HDR_4B   Header;                     // 0x00
    ULONG       ulSequenceNumber;                   // 0x04
    ULONGLONG   TimeInHundNanoSecondsFromJan1601;   // 0x08
                                                    // 0x10
} TIME_STAMP_TAG, *PTIME_STAMP_TAG;

#define INVOLFLT_DATA_SOURCE_UNDEFINED  0x00
#define INVOLFLT_DATA_SOURCE_BITMAP     0x01
#define INVOLFLT_DATA_SOURCE_META_DATA  0x02
#define INVOLFLT_DATA_SOURCE_DATA       0x03

typedef struct _DATA_SOURCE_TAG
{
    STREAM_REC_HDR_4B   Header;         // 0x00
    ULONG       ulDataSource;           // 0x04
                                        // 0x08
} DATA_SOURCE_TAG, *PDATA_SOURCE_TAG;

#define UDIRTY_BLOCK_FLAG_START_OF_SPLIT_CHANGE     0x00000001
#define UDIRTY_BLOCK_FLAG_PART_OF_SPLIT_CHANGE      0x00000002
#define UDIRTY_BLOCK_FLAG_END_OF_SPLIT_CHANGE       0x00000004
#define UDIRTY_BLOCK_FLAG_DATA_FILE                 0x00000008
#define UDIRTY_BLOCK_FLAG_SVD_STREAM                0x00000010
#define UDIRTY_BLOCK_FLAG_TSO_FILE                  0x00000020

#define UDIRTY_BLOCK_FLAG_VOLUME_RESYNC_REQUIRED    0x80000000

#define UDIRTY_BLOCK_HEADER_SIZE    0x200           // 512 Bytes
#define UDIRTY_BLOCK_MAX_ERROR_STRING_SIZE  0x80    // 128 Bytes
#define UDIRTY_BLOCK_TAGS_SIZE      0xE00   // 7 * 512 Bytes
// UDIRTY_BLOCK_MAX_FILE_NAME is UDIRTY_BLOCK_TAGS_SIZE / sizeof(WCHAR) - 1(for length field)
#define UDIRTY_BLOCK_MAX_FILE_NAME  0x6FF   // (0xE00 /2) - 1
    // uHeader is .5 KB and uTags is 3.5 KB, uHeader + uTags = 4KB

// Common structure name used earlier but as part of supporting Global Sequence number for 32-bit systems
// it has made to use V2 structures and kept other structure for backward compatiablility.
typedef struct _UDIRTY_BLOCK_V2
{

    union {
        struct {
        ULARGE_INTEGER    uliTransactionID;
        ULARGE_INTEGER    ulicbChanges;
        ULONG             cChanges;
        ULONG             ulTotalChangesPending;
        ULARGE_INTEGER    ulicbTotalChangesPending;
        ULONG             ulFlags;
        ULONG             ulSequenceIDforSplitIO;
        ULONG             ulBufferSize;
        USHORT            usMaxNumberOfBuffers;
        USHORT            usNumberOfBuffers;
        ULONG             ulcbChangesInStream;
        ULONG             ulcbBufferArraySize;
        // resync flags
        ULONG             ulOutOfSyncCount;
        UCHAR             ErrorStringForResync[UDIRTY_BLOCK_MAX_ERROR_STRING_SIZE]; 
        ULONG             ulOutOfSyncErrorCode;
        LARGE_INTEGER     liOutOfSyncTimeStamp;
        etWriteOrderState eWOState;  
        ULONG             ulDataSource; 
        /* This is actually a pointer to memory and not an array of pointers. 
         * It contains Changes in linear memorylocation.
         */
#ifdef _WIN64
        PVOID               *ppBufferArray;
#else
        PVOID               *ppBufferArray;
        ULONG               Reserved;
#endif 
        ULONGLONG         ullPrevEndSequenceNumber;
        ULONGLONG         ullPrevEndTimeStamp;
        ULONG             ulPrevSequenceIDforSplitIO;
        
        } Hdr;
        UCHAR  BufferReservedForHeader[UDIRTY_BLOCK_HEADER_SIZE];
    } uHdr;

    // Start of Markers
    union {
        struct {
            STREAM_REC_HDR_4B   TagStartOfList;
            STREAM_REC_HDR_4B   TagPadding;
            TIME_STAMP_TAG_V2   TagTimeStampOfFirstChange;
            TIME_STAMP_TAG_V2   TagTimeStampOfLastChange;
            DATA_SOURCE_TAG     TagDataSource;
            STREAM_REC_HDR_4B   TagEndOfList;
        } TagList;
        struct {
            USHORT   usLength; // Filename length in bytes not including NULL
            WCHAR    FileName[UDIRTY_BLOCK_MAX_FILE_NAME];
        } DataFile;
        UCHAR  BufferForTags[UDIRTY_BLOCK_TAGS_SIZE];
    } uTagList;

    //DISK_CHANGE     Changes[ MAX_DIRTY_CHANGES ];
    ULONGLONG ChangeOffsetArray[MAX_UDIRTY_CHANGES_V2];  
    ULONG ChangeLengthArray[MAX_UDIRTY_CHANGES_V2];  
    ULONG TimeDeltaArray[MAX_UDIRTY_CHANGES_V2];
    ULONG SequenceNumberDeltaArray[MAX_UDIRTY_CHANGES_V2];
} UDIRTY_BLOCK_V2, *PUDIRTY_BLOCK_V2; //, DIRTY_BLOCKS, *PDIRTY_BLOCKS;

typedef struct _UDIRTY_BLOCK
{
    union {
        struct {
            ULARGE_INTEGER  uliTransactionID;                                       // Offset 0x00
            ULARGE_INTEGER  ulicbChanges;                                           // Offset 0x08
            ULONG           cChanges;                                               // Offset 0x10
            // ulTotalChangesPending does not include changes from this dirty block.
            ULONG           ulTotalChangesPending;                                  // Offset 0x14
            ULONG           ulFlags;                                                // Offset 0x18
            ULONG           ulSequenceIDforSplitIO;                                 // Offset 0x1C
            UCHAR           ErrorStringForResync[UDIRTY_BLOCK_MAX_ERROR_STRING_SIZE];   // Offset 0x20
            ULONG           ulOutOfSyncCount;                                       // Offset 0xA0
            ULONG           ulOutOfSyncErrorCode;                                   // Offset 0xA4
            // ulicbTotalChangesPending does not include changes from this dirty block.
            ULARGE_INTEGER  ulicbTotalChangesPending;                               // Offset 0xA8
            LARGE_INTEGER   liOutOfSyncTimeStamp;                                   // Offset 0xB0                                                                                    // 
            ULONG           ulBufferSize;                                           // Offset 0xB8
            USHORT          usMaxNumberOfBuffers;                                   // Offset 0xBC
            USHORT          usNumberOfBuffers;                                      // Offset 0xBE
            ULONG           ulcbBufferArraySize;                                    // Offset 0xC0
            ULONG           ulcbChangesInStream;                                    // Offset 0xC4
            etWriteOrderState   eWOState;                                               // Offset 0xC8
            ULONG               ulDataSource;                                           // Offset 0xCC
            PVOID               *ppBufferArray;                                        // Offset 0xD0
            ULONG               ulPrevEndSequenceNumber;                                 // Offset 0xD4
            ULONGLONG           ullPrevEndTimeStamp;                                     // Offset 0xD8
            ULONG               ulPrevSequenceIDforSplitIO;                              // Offset 0xE0
                                                                                         // Offset 0xE4
        } Hdr;
        UCHAR BufferReservedForHeader[UDIRTY_BLOCK_HEADER_SIZE];
    } uHdr;

    // Start of Markers
    union {
        struct {
            STREAM_REC_HDR_4B   TagStartOfList;                 // 0x200
            STREAM_REC_HDR_4B   TagPadding;                     // 0x204
            TIME_STAMP_TAG      TagTimeStampOfFirstChange;      // 0x208
            TIME_STAMP_TAG      TagTimeStampOfLastChange;       // 0x218
            DATA_SOURCE_TAG     TagDataSource;                  // 0x228
            STREAM_REC_HDR_4B   TagEndOfList;                   // 0x230
                                                                // 0x234
        } TagList;
        struct {
            USHORT  usLength;                                   // 0x200 Filename length in bytes not including NULL
            WCHAR   FileName[UDIRTY_BLOCK_MAX_FILE_NAME];       // 0x202
        } DataFile;
        UCHAR BufferForTags[UDIRTY_BLOCK_TAGS_SIZE];
    } uTagList;

    LONGLONG        ChangeOffsetArray[MAX_UDIRTY_CHANGES];      // 0x1000
    ULONG           ChangeLengthArray[MAX_UDIRTY_CHANGES];
} UDIRTY_BLOCK, *PUDIRTY_BLOCK; //, DIRTY_BLOCKS, *PDIRTY_BLOCKS;

#define VSF_BITMAP_READ_NOT_STARTED             0x00000001
#define VSF_BITMAP_READ_IN_PROGRESS             0x00000002
#define VSF_BITMAP_READ_PAUSED                  0x00000004
#define VSF_BITMAP_READ_COMPLETE                0x00000008
#define VSF_BITMAP_READ_ERROR                   0x00000010
#define VSF_BITMAP_WRITE_ERROR                  0x00000020
#define VSF_BITMAP_NOT_OPENED                   0x00000040

#define VSF_VOLUME_ON_BASIC_DISK                0x00040000
#define VSF_VOLUME_ON_CLUS_DISK                 0x00080000
#define VSF_CLUSTER_VOLUME_ONLINE               0x00100000
#define VSF_PAGEFILE_WRITES_IGNORED             0x00200000
#define VSF_VCFIELDS_PERSISTED                  0x00400000
#define VSF_SURPRISE_REMOVED                    0x00800000
#define VSF_CLUSTER_VOLUME                      0x01000000
#define VSF_DATA_NOTIFY_SET                     0x02000000
#define VSF_DATA_FILES_DISABLED                 0x04000000
#define VSF_READ_ONLY                           0x08000000
#define VSF_FILTERING_STOPPED                   0x80000000
#define VSF_BITMAP_WRITE_DISABLED               0x40000000
#define VSF_BITMAP_READ_DISABLED                0x20000000
#define VSF_DATA_CAPTURE_DISABLED               0x10000000


#define VOLUME_STATS_DATA_MAJOR_VERSION         0x0003
#define VOLUME_STATS_DATA_MINOR_VERSION         0x0000

#define VOLUMES_DM_MAJOR_VERSION                0x0001
#define VOLUMES_DM_MINOR_VERSION                0x0000

// Keep this a multiple of 2 for 8 byte alignment in VOLUME_STATS structure
#define USER_MODE_MAX_IO_BUCKETS                    12
#define USER_MODE_INSTANCES_OF_MEM_USAGE_TRACKED    10

typedef struct _VOLUME_STATS
{
    WCHAR               VolumeGUID[GUID_SIZE_IN_CHARS];     // 0x00
    WCHAR               DriveLetter;                        // 0x48
    USHORT              usReserved;                         // 0x4A
    ULONG               ulVolumeFlags;                      // 0x4C

    etCaptureMode       eCaptureMode;                       // 0x50
    etWriteOrderState   eWOState;                           // 0x54
    etWriteOrderState   ePrevWOState;                       // 0x58
    etVBitmapState      eVBitmapState;                      // 0x5C
                                                            // 
    LONG                lAdditionalGUIDs;                   // 0x60
    ULONG               ulAdditionalGUIDsReturned;          // 0x64
                                                            // 
    ULARGE_INTEGER      ulVolumeSize;                       // 0x68
    ULARGE_INTEGER      uliChangesReturnedToService;        // 0x70
    ULARGE_INTEGER      uliChangesReadFromBitMap;           // 0x78
    ULARGE_INTEGER      uliChangesWrittenToBitMap;          // 0x80

    ULARGE_INTEGER      ulicbChangesQueuedForWriting;       // 0x88
    ULARGE_INTEGER      ulicbChangesWrittenToBitMap;        // 0x90
    ULARGE_INTEGER      ulicbChangesReadFromBitMap;         // 0x98
    ULARGE_INTEGER      ulicbChangesReturnedToService;      // 0xA0
    ULARGE_INTEGER      ulicbChangesPendingCommit;          // 0xA8
    ULARGE_INTEGER      ulicbChangesReverted;               // 0xB0
    ULARGE_INTEGER      ulicbChangesInBitmap;               // 0xB8
    ULONGLONG           ullDataFilesReturned;               // 0xC0
    ULONGLONG           ullcbTotalChangesPending;           // 0xC8
    ULONGLONG           ullcbBitmapChangesPending;          // 0xD0
    ULONGLONG           ullcbMetaDataChangesPending;        // 0xD8
    ULONGLONG           ullcbDataChangesPending;            // 0xE0

    ULONG               ulChangesPendingCommit;             // 0xE8
    ULONG               ulChangesReverted;                  // 0xEC
    ULONG               ulChangesQueuedForWriting;          // 0xF0
    ULONG               ulDataFilesPending;                 // 0xF4

    LONG                lDirtyBlocksInQueue;                // 0xF8
    ULONG               ulTotalChangesPending;              // 0xFC
    ULONG               ulBitmapChangesPending;             // 0x100
    ULONG               ulMetaDataChangesPending;           // 0x104

    ULONG               ulDataChangesPending;                       // 0x108                                                            
    LONG                lNumChangeToMetaDataWOState;                // 0x10C
    LONG                lNumChangeToMetaDataWOStateOnUserRequest;   // 0x110
    LONG                lNumChangeToDataWOState;                    // 0x114

    LONG                lNumChangeToBitmapWOState;                  // 0x118
    LONG                lNumChangeToBitmapWOStateOnUserRequest;     // 0x11C
    LONG                lNumChangeToDataCaptureMode;                // 0x120
    LONG                lNumChangeToMetaDataCaptureMode;            // 0x124
    LONG                lNumChangeToMetaDataCaptureModeOnUserRequest;  //0x128
    ULONG               ulNumSecondsInDataCaptureMode;              // 0x12C
                                                                    
    ULONG               ulNumSecondsInMetadataCaptureMode;          // 0x130
    ULONG               ulNumSecondsInCurrentCaptureMode;           // 0x134
    ULONG               ulNumSecondsInDataWOState;                  // 0x138
    ULONG               ulNumSecondsInMetaDataWOState;              // 0x13C
    ULONG               ulNumSecondsInBitmapWOState;                // 0x140
    ULONG               ulNumSecondsInCurrentWOState;               // 0x144

    LONG                lNumBitmapOpenErrors;                       // 0x148
    LONG                lNumBitMapWriteErrors;                      // 0x14C
    LONG                lNumBitMapReadErrors;                       // 0x150
    LONG                lNumBitMapClearErrors;                      // 0x154
    ULONG               ulNumMemoryAllocFailures;                   // 0x158
    ULONG               ulNumberOfTagPointsDropped;                 // 0x15C


    ULONG               ulcbDataNotifyThreshold;                    // 0x160
    ULONG               ulcbDataBufferSizeAllocated;                // 0x164
    ULONG               ulcbDataBufferSizeFree;                     // 0x168
    ULONG               ulcbDataBufferSizeReserved;                 // 0x16C

    ULONGLONG           ullcbDataToDiskLimit;                       // 0x170
    ULONGLONG           ullcbMinFreeDataToDiskLimit;                // 0x178
    ULONGLONG           ullcbDiskUsed;                              // 0x180

    ULONG               ulDataFilesReverted;                        // 0x188
    ULONG               ulConfiguredPriority;                       // 0x18C
    ULONG               ulEffectivePriority;                        // 0x190
    ULONG               ulWriterId;                                 // 0x194

    ULONG               ulIoSizeArray[USER_MODE_MAX_IO_BUCKETS];            // 0x198
    ULONG               ulIoSizeReadArray[USER_MODE_MAX_IO_BUCKETS];        // 0x1C8
    ULONGLONG           ullIoSizeCounters[USER_MODE_MAX_IO_BUCKETS];        // 0x1F8
    ULONGLONG           ullIoSizeReadCounters[USER_MODE_MAX_IO_BUCKETS];    // 0x258


    ULONG               ulOutOfSyncCount;                                   // 0x2B8
    ULONG               ulOutOfSyncErrorCode;                               // 0x2BC
    ULONG               ulOutOfSyncErrorStatus;                             // 0x2C0
    ULONG               ulMuInstances;                                      // 0x2C4

    ULONG               ulMAllocatedAtOutOfDataMode[USER_MODE_INSTANCES_OF_MEM_USAGE_TRACKED];  // 0x2C8
    ULONG               ulMReservedAtOutOfDataMode[USER_MODE_INSTANCES_OF_MEM_USAGE_TRACKED];   // 0x2F0
    ULONG               ulMFreeInVCAtOutOfDataMode[USER_MODE_INSTANCES_OF_MEM_USAGE_TRACKED];   // 0x218
    ULONG               ulMFreeAtOutOfDataMode[USER_MODE_INSTANCES_OF_MEM_USAGE_TRACKED];       // 0x340
    ULONGLONG           ullDuAtOutOfDataMode[USER_MODE_INSTANCES_OF_MEM_USAGE_TRACKED];         // 0x368
    LARGE_INTEGER       liCaptureModeChangeTS[USER_MODE_INSTANCES_OF_MEM_USAGE_TRACKED];        // 0x3B8

    WCHAR               ErrorStringForResync[UDIRTY_BLOCK_MAX_ERROR_STRING_SIZE];               // 0x408

    // TimeInHundNanoSecondsFromJan1601
    LONGLONG            llLastFlushTime;                                    // 0x508
    LARGE_INTEGER       liOutOfSyncTimeStamp;                               // 0x510
    LARGE_INTEGER       liOutOfSyncResetTimeStamp;                          // 0x518
    LARGE_INTEGER       liOutOfSyncIndicatedTimeStamp;                      // 0x520
    LARGE_INTEGER       liLastOutOfSyncTimeStamp;                           // 0x528
    LARGE_INTEGER       liResyncStartTimeStamp;                             // 0x530
    LARGE_INTEGER       liResyncEndTimeStamp;                               // 0x538
    LARGE_INTEGER       liStartFilteringTimeStamp;                          // 0x540
    LARGE_INTEGER       liStopFilteringTimeStamp;                           // 0x548
    LARGE_INTEGER       liClearDiffsTimeStamp;                              // 0x550
    LARGE_INTEGER       liClearStatsTimeStamp;                              // 0x558
    LARGE_INTEGER       liDeleteBitmapTimeStamp;                            // 0x560
    LARGE_INTEGER       liDisMountNotifyTimeStamp;                          // 0x568
    LARGE_INTEGER       liDisMountFailNotifyTimeStamp;                      // 0x570      
    LARGE_INTEGER       liMountNotifyTimeStamp;                             // 0x578

    ULONG               ulFlushCount;                                       // 0x580
#define USER_MODE_INSTANCES_OF_WO_STATE_TRACKED   10
    ULONG               ulWOSChInstances;                                   // 0x584
    etWriteOrderState   eOldWOState[USER_MODE_INSTANCES_OF_WO_STATE_TRACKED];                   // 0x588
    etWriteOrderState   eNewWOState[USER_MODE_INSTANCES_OF_WO_STATE_TRACKED];                   // 0x5B0
    etWOSChangeReason   eWOSChangeReason[USER_MODE_INSTANCES_OF_WO_STATE_TRACKED];              // 0x5D8
    ULONG               ulNumSecondsInWOS[USER_MODE_INSTANCES_OF_WO_STATE_TRACKED];             // 0x600
    LARGE_INTEGER       liWOstateChangeTS[USER_MODE_INSTANCES_OF_WO_STATE_TRACKED];             // 0x628
    ULONGLONG           ullcbMDChangesPendingAtWOSchange[USER_MODE_INSTANCES_OF_WO_STATE_TRACKED];  // 0x678
    ULONGLONG           ullcbBChangesPendingAtWOSchange[USER_MODE_INSTANCES_OF_WO_STATE_TRACKED];   // 0x6C8
    ULONGLONG           ullcbDChangesPendingAtWOSchange[USER_MODE_INSTANCES_OF_WO_STATE_TRACKED];   // 0x718

    ULONGLONG           liLastCommittedTimeStampReadFromStore; // 0x768
    ULONGLONG           liLastCommittedSequenceNumberReadFromStore; // 0x770
    ULONG               liLastCommittedSplitIoSeqIdReadFromStore; // 0x778
    ULONG               ulReserved; // 0x77C

    // This is reserved space so that later additions would not
    // change the size of the structure. padding to mulitple of 8
    ULONGLONG           ullCacheHit, ullCacheMiss;//0x788
    ULONG               ulTagsinWOState;
    ULONG               ulTagsinNonWOSDrop;
    LARGE_INTEGER       liStartFilteringTimeStampByUser;
	LARGE_INTEGER       liGetDBTimeStamp;
	LARGE_INTEGER       liCommitDBTimeStamp;
	LARGE_INTEGER       liVolumeContextCreationTS;
    ULONG               ulDataBlocksReserved;
    ULONG               ulReserved1;
    ULONGLONG           Reserved[8];
                                              // 0x800
} VOLUME_STATS, *PVOLUME_STATS;

#define VSDF_SERVICE_DISABLED_DATA_MODE                 0x0001
#define VSDF_DRIVER_DISABLED_DATA_CAPTURE               0x0002
#define VSDF_DRIVER_FAILED_TO_ALLOCATE_DATA_POOL        0x0004
#define VSDF_SERVICE2_DID_NOT_REGISTER_FOR_DATA_MODE    0x0008
#define VSDF_SERVICE_DISABLED_DATA_FILES                0x0010
#define VSDF_DRIVER_DISABLED_DATA_FILES                 0x0020
#define VSDF_SERVICE2_DID_NOT_REGISTER_FOR_DATA_FILES   0x0040

typedef struct _VOLUME_STATS_DATA
{
    USHORT          usMajorVersion;             // 0x00
    USHORT          usMinorVersion;             // 0x02
    ULONG           ulTotalVolumes;             // 0x04
    ULONG           ulVolumesReturned;          // 0x08
    LONG            lDirtyBlocksAllocated;      // 0x0C

    etServiceState  eServiceState;              // 0x10
    ULONG           ulDriverFlags;              // 0x14
    ULONGLONG       ullDataPoolSizeAllocated;    // 0x18
    ULONGLONG       ullDataPoolSizeFree;         // 0x20

    ULONGLONG       ullDataPoolSizeFreeAndLocked;    // 0x28
    ULONG           ulNumMemoryAllocFailures;   // 0x30
    LONG            lBitmapWorkItemsAllocated;  // 0x34
    LONG            lChangeBlocksAllocated;     // 0x38
    LONG            lChangeNodesAllocated;      // 0x3c
    LONG            lNonPagedMemoryAllocated;   // 0x40
    ULONG           AsyncTagIOCTLCount;                       // 0x44    
    LARGE_INTEGER   liNonPagedLimitReachedTSforFirstTime;   // 0x48
    LARGE_INTEGER   liNonPagedLimitReachedTSforLastTime;    // 0x50
    ULONG           ulNonPagedMemoryLimitInMB;  // 0x58
    // This is reserved space so that later additions would not
    // change the size of the structure. padding to mulitple of 8
    LONG            lTotalAdditionalGUIDs;           // 0x5C
    ULONGLONG       ullPersistedTimeStampAfterBoot;      // 0x60
    ULONGLONG       ullPersistedSequenceNumberAfterBoot; // 0x68
    UCHAR           LastShutdownMarker;                  // 0x70
    BOOLEAN         PersistentRegistryCreated;           // 0x71
    UCHAR           ucReserved[0x06];                    // 0x72
    ULONGLONG       ulDaBInOrphanedMappedDBList;         // 0x78
    ULONG           LockedDataBlockCounter;              // 0x7c
    ULONG           MaxLockedDataBlockCounter;           // 0x80
    ULONG           MappedDataBlockCounter;              // 0x84
    ULONG           MaxMappedDataBlockCounter;           // 0x88
                                                         // 0x90
} VOLUME_STATS_DATA, *PVOLUME_STATS_DATA;

typedef struct _VOLUME_DM_DATA
{
    WCHAR               VolumeGUID[GUID_SIZE_IN_CHARS];     // 0x00
    WCHAR               DriveLetter;                        // 0x48
    USHORT              usReserved;                         // 0x4A
    etWriteOrderState   eWOState;                           // 0x4C
    
    etWriteOrderState   ePrevWOState;                       // 0x50
    LONG        lNumChangeToMetaDataWOState;                // 0x54
    LONG        lNumChangeToMetaDataWOStateOnUserRequest;   // 0x58
    LONG        lNumChangeToDataWOState;                    // 0x5C
                                                            // 
    LONG        lNumChangeToBitmapWOState;                  // 0x60
    LONG        lNumChangeToBitmapWOStateOnUserRequest;     // 0x64
    LONG        lNumChangeToDataCaptureMode;                // 0x68
    LONG        lNumChangeToMetaDataCaptureMode;            // 0x6C

    LONG        lNumChangeToMetaDataCaptureModeOnUserRequest;  //0x70
    ULONG       ulNumSecondsInDataCaptureMode;              // 0x74
    ULONG       ulNumSecondsInMetadataCaptureMode;          // 0x78
    ULONG       ulNumSecondsInCurrentCaptureMode;           // 0x7C

    ULONG       ulNumSecondsInDataWOState;                  // 0x80
    ULONG       ulNumSecondsInMetaDataWOState;              // 0x84
    ULONG       ulNumSecondsInBitmapWOState;                // 0x88
    ULONG       ulNumSecondsInCurrentWOState;               // 0x8C

    etCaptureMode   eCaptureMode;
    LONG            lDirtyBlocksInQueue;
    ULONG           ulDataFilesPending;

    ULONG           ulTotalChangesPending;
    ULONG           ulMetaDataChangesPending;
    ULONG           ulBitmapChangesPending;
    ULONG           ulDataChangesPending;

    ULONGLONG       ullcbTotalChangesPending;
    ULONGLONG       ullcbMetaDataChangesPending;
    ULONGLONG       ullcbBitmapChangesPending;
    ULONGLONG       ullcbDataChangesPending;

#define USER_MODE_INSTANCES_OF_NOTIFY_LATENCY_TRACKED   10
#define USER_MODE_NOTIFY_LATENCY_DISTRIBUTION_BUCKETS   10
    ULONGLONG   ullNotifyLatencyDistribution[USER_MODE_NOTIFY_LATENCY_DISTRIBUTION_BUCKETS];
    ULONG       ulNotifyLatencyDistSizeArray[USER_MODE_NOTIFY_LATENCY_DISTRIBUTION_BUCKETS];
    ULONG       ulNotifyLatencyLogArray[USER_MODE_INSTANCES_OF_NOTIFY_LATENCY_TRACKED];
    ULONG       ulNotifyLatencyLogSize;
    ULONG       ulNotifyLatencyMinLoggedValue;
    ULONG       ulNotifyLatencyMaxLoggedValue;

#define USER_MODE_INSTANCES_OF_COMMIT_LATENCY_TRACKED   10
#define USER_MODE_COMMIT_LATENCY_DISTRIBUTION_BUCKETS   10
    ULONGLONG   ullCommitLatencyDistribution[USER_MODE_COMMIT_LATENCY_DISTRIBUTION_BUCKETS];
    ULONG       ulCommitLatencyDistSizeArray[USER_MODE_COMMIT_LATENCY_DISTRIBUTION_BUCKETS];
    ULONG       ulCommitLatencyLogArray[USER_MODE_INSTANCES_OF_COMMIT_LATENCY_TRACKED];
    ULONG       ulCommitLatencyLogSize;
    ULONG       ulCommitLatencyMinLoggedValue;
    ULONG       ulCommitLatencyMaxLoggedValue;

#define USER_MODE_INSTANCES_OF_S2_DATA_RETRIEVAL_LATENCY_TRACKED 12
#define USER_MODE_S2_DATA_RETRIEVAL_LATENCY_DISTRIBUTION_BUCKERS 12
    ULONGLONG   ullS2DataRetievalLatencyDistribution[USER_MODE_S2_DATA_RETRIEVAL_LATENCY_DISTRIBUTION_BUCKERS];
    ULONG       ulS2DataRetievalLatencyDistSizeArray[USER_MODE_S2_DATA_RETRIEVAL_LATENCY_DISTRIBUTION_BUCKERS];
    ULONG       ulS2DataRetievalLatencyLogArray[USER_MODE_INSTANCES_OF_S2_DATA_RETRIEVAL_LATENCY_TRACKED];
    ULONG       ulS2DataRetievalLatencyLogSize;
    ULONG       ulS2DataRetievalLatencyMinLoggedValue;
    ULONG       ulS2DataRetievalLatencyMaxLoggedValue;

    ULONG       ulMuInstances;
    ULONGLONG   ullDuAtOutOfDataMode[USER_MODE_INSTANCES_OF_MEM_USAGE_TRACKED];
    ULONG       ulMAllocatedAtOutOfDataMode[USER_MODE_INSTANCES_OF_MEM_USAGE_TRACKED];
    ULONG       ulMReservedAtOutOfDataMode[USER_MODE_INSTANCES_OF_MEM_USAGE_TRACKED];
    ULONG       ulMFreeInVCAtOutOfDataMode[USER_MODE_INSTANCES_OF_MEM_USAGE_TRACKED];
    ULONG       ulMFreeAtOutOfDataMode[USER_MODE_INSTANCES_OF_MEM_USAGE_TRACKED];
    LARGE_INTEGER liCaptureModeChangeTS[USER_MODE_INSTANCES_OF_MEM_USAGE_TRACKED];

} VOLUME_DM_DATA, *PVOLUME_DM_DATA;

typedef struct _VOLUMES_DM_DATA
{
    USHORT          usMajorVersion;             // 0x00
    USHORT          usMinorVersion;             // 0x02
    ULONG           ulTotalVolumes;             // 0x04
    ULONG           ulVolumesReturned;          // 0x08
    // This is reserved space so that later additions would not
    // change the size of the structure. padding to mulitple of 8
    ULONG           ulReserved[0x1D];           // 0x0C
    VOLUME_DM_DATA    VolumeArray[1];             // 0x80
} VOLUMES_DM_DATA, *PVOLUMES_DM_DATA;

#define COMMIT_TRANSACTION_FLAG_RESET_RESYNC_REQUIRED_FLAG  0x00000001

typedef struct _COMMIT_TRANSACTION
{
    WCHAR           VolumeGUID[GUID_SIZE_IN_CHARS];     // 0x00
    ULARGE_INTEGER  ulTransactionID;                    // 0x48
    ULONG           ulFlags;                            // 0x50
    ULONG           ulReserved;                         // 0x54
                                                        // 0x58
} COMMIT_TRANSACTION, *PCOMMIT_TRANSACTION;

typedef struct _VOLUME_DB_EVENT_INFO
{
    WCHAR       VolumeGUID[GUID_SIZE_IN_CHARS];     // 0x00
    HANDLE      hEvent;                             // 0x48
                                                    // 0x50 for 64Bit 0x4C for 32bit
} VOLUME_DB_EVENT_INFO, *PVOLUME_DB_EVENT_INFO;

typedef struct _VOLUME_DB_EVENT_INFO32
{
    WCHAR       VolumeGUID[GUID_SIZE_IN_CHARS];     // 0x00
    ULONG       hEvent;                             // 0x48
                                                    // 0x4C
} VOLUME_DB_EVENT_INFO32, *PVOLUME_DB_EVENT_INFO32;

#define DEBUG_INFO_FLAGS_RESET_MODULES  0x0001
#define DEBUG_INFO_FLAGS_SET_LEVEL_ALL  0x0002
#define DEBUG_INFO_FLAGS_SET_LEVEL      0x0004

typedef struct _DEBUG_INFO
{
    ULONG   ulDebugInfoFlags;       // 0x00
    ULONG   ulDebugLevelForAll;     // 0x04
    ULONG   ulDebugLevelForMod;     // 0x08
    ULONG   ulDebugModules;         // 0x0C
                                    // 0x10
} DEBUG_INFO, *PDEBUG_INFO;


#define SET_DATA_FILE_THREAD_PRIORITY_FLAGS_VALID_GUID  0x0001

typedef struct _SET_DATA_FILE_THREAD_PRIORITY_INPUT
{
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS + 2];     // 0x00
    ULONG   ulFlags;                                // 0x4C
    ULONG   ulThreadPriority;                       // 0x50
    ULONG   ulDefaultThreadPriority;                // 0x54
    ULONG   ulThreadPriorityForAllVolumes;          // 0x58
    ULONG   ulReserved;                             // 0x5C
                                                    // 0x60
} SET_DATA_FILE_THREAD_PRIORITY_INPUT, *PSET_DATA_FILE_THREAD_PRIORITY_INPUT;

typedef struct _SET_DATA_FILE_THREAD_PRIORITY_OUTPUT
{
    ULONG   ulFlags;                        // 0x00
    ULONG   ulErrorInSettingForVolume;      // 0x04
    ULONG   ulErrorInSettingDefault;        // 0x08
    ULONG   ulErrorInSettingForAllVolumes;  // 0x0C
                                            // 0x10
} SET_DATA_FILE_THREAD_PRIORITY_OUTPUT, *PSET_DATA_FILE_THREAD_PRIORITY_OUTPUT;

#define SET_WORKER_THREAD_PRIORITY_VALID_PRIORITY   0x0001

typedef struct _SET_WORKER_THREAD_PRIORITY
{
    ULONG   ulFlags;                                // 0x00
    ULONG   ulThreadPriority;                       // 0x04
                                                    // 0x08
} SET_WORKER_THREAD_PRIORITY, *PSET_WORKER_THREAD_PRIORITY;

#define SET_DATA_TO_DISK_SIZE_LIMIT_FLAGS_VALID_GUID  0x0001

typedef struct _SET_DATA_TO_DISK_SIZE_LIMIT_INPUT
{
    // Adding +2 for alignment and null terminator
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS + 2];     // 0x00
    ULONG   ulFlags;                                // 0x4C
    ULONG   ulDataToDiskSizeLimit;                  // 0x50
    ULONG   ulDefaultDataToDiskSizeLimit;           // 0x54
    ULONG   ulDataToDiskSizeLimitForAllVolumes;     // 0x58
    ULONG   ulReserved;                             // 0x5C
                                                    // 0x60
} SET_DATA_TO_DISK_SIZE_LIMIT_INPUT, *PSET_DATA_TO_DISK_SIZE_LIMIT_INPUT;

typedef struct _SET_DATA_TO_DISK_SIZE_LIMIT_OUTPUT
{
    ULONG   ulFlags;                        // 0x00
    ULONG   ulErrorInSettingForVolume;      // 0x04 STATUS_DEVICE_DOES_NOT_EXIST
    ULONG   ulErrorInSettingDefault;        // 0x08
    ULONG   ulErrorInSettingForAllVolumes;  // 0x0C
                                            // 0x10
} SET_DATA_TO_DISK_SIZE_LIMIT_OUTPUT, *PSET_DATA_TO_DISK_SIZE_LIMIT_OUTPUT;

#define SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_FLAGS_VALID_GUID  0x0001

typedef struct _SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_INPUT
{
    // Adding +2 for alignment and null terminator
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS + 2];     // 0x00
    ULONG   ulFlags;                                // 0x4C
    ULONG   ulMinFreeDataToDiskSizeLimit;                  // 0x50
    ULONG   ulDefaultMinFreeDataToDiskSizeLimit;           // 0x54
    ULONG   ulMinFreeDataToDiskSizeLimitForAllVolumes;     // 0x58
    ULONG   ulReserved;                             // 0x5C
                                                    // 0x60
} SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_INPUT, *PSET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_INPUT;

typedef struct _SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_OUTPUT
{
    ULONG   ulFlags;                        // 0x00
    ULONG   ulErrorInSettingForVolume;      // 0x04 STATUS_DEVICE_DOES_NOT_EXIST
    ULONG   ulErrorInSettingDefault;        // 0x08
    ULONG   ulErrorInSettingForAllVolumes;  // 0x0C
                                            // 0x10
} SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_OUTPUT, *PSET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_OUTPUT;


#define SET_DATA_NOTIFY_SIZE_LIMIT_FLAGS_VALID_GUID  0x0001

typedef struct _SET_DATA_NOTIFY_SIZE_LIMIT_INPUT
{
    // Adding +2 for alignment and null terminator
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS + 2];     // 0x00
    ULONG   ulFlags;                                // 0x4C
    ULONG   ulLimit;                                // 0x50
    ULONG   ulDefaultLimit;                         // 0x54
    ULONG   ulLimitForAllVolumes;                   // 0x58
    ULONG   ulReserved;                             // 0x5C
                                                    // 0x60
} SET_DATA_NOTIFY_SIZE_LIMIT_INPUT, *PSET_DATA_NOTIFY_SIZE_LIMIT_INPUT;

typedef struct _SET_DATA_NOTIFY_SIZE_LIMIT_OUTPUT
{
    ULONG   ulFlags;                        // 0x00
    ULONG   ulErrorInSettingForVolume;      // 0x04 STATUS_DEVICE_DOES_NOT_EXIST
    ULONG   ulErrorInSettingDefault;        // 0x08
    ULONG   ulErrorInSettingForAllVolumes;  // 0x0C
                                            // 0x10
} SET_DATA_NOTIFY_SIZE_LIMIT_OUTPUT, *PSET_DATA_NOTIFY_SIZE_LIMIT_OUTPUT;

typedef struct _SET_RESYNC_REQUIRED
{
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS + 2]; // 0x00
    ULONG   ulOutOfSyncErrorCode;               // 0x4C
    ULONG   ulOutOfSyncErrorStatus;             // 0x50
    ULONG   ulReserved;                         // 0x54
                                                // 0x58
} SET_RESYNC_REQUIRED, *PSET_RESYNC_REQUIRED;

#define SET_IO_SIZE_ARRAY_INPUT_FLAGS_VALID_GUID    0x0001
#define SET_IO_SIZE_ARRAY_READ_INPUT 0x0002
#define SET_IO_SIZE_ARRAY_WRITE_INPUT 0x0004

typedef struct _SET_IO_SIZE_ARRAY_INPUT
{
    // Adding +2 for alignment and null terminator
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS + 2];     // 0x00
    ULONG   ulFlags;                                // 0x4C
    ULONG   ulArraySize;                            // 0x50
    ULONG   ulReserved;                             // 0x54
    ULONG   ulIoSizeArray[USER_MODE_MAX_IO_BUCKETS];    // 0x058
                                                    // 0x88
} SET_IO_SIZE_ARRAY_INPUT, *PSET_IO_SIZE_ARRAY_INPUT;

#define DRIVER_CONFIG_FLAGS_HWM_SERVICE_NOT_STARTED                   0x00000001
#define DRIVER_CONFIG_FLAGS_HWM_SERVICE_RUNNING                       0x00000002
#define DRIVER_CONFIG_FLAGS_HWM_SERVICE_SHUTDOWN                      0x00000004
#define DRIVER_CONFIG_FLAGS_LWM_SERVICE_RUNNING                       0x00000008
#define DRIVER_CONFIG_FLAGS_CHANGES_TO_PURGE_ON_HWM                   0x00000010
#define DRIVER_CONFIG_FLAGS_TIME_BETWEEN_MEM_FAIL_LOG                 0x00000020
#define DRIVER_CONFIG_FLAGS_MAX_WAIT_TIME_ON_FAILOVER                 0x00000040
#define DRIVER_CONFIG_FLAGS_MAX_LOCKED_DATA_BLOCKS                    0x00000080
#define DRIVER_CONFIG_FLAGS_MAX_NON_PAGED_POOL                        0x00000100
#define DRIVER_CONFIG_FLAGS_MAX_WAIT_TIME_FOR_IRP_COMPLETION          0x00000200
#define DRIVER_CONFIG_FLAGS_DEFAULT_FILTERING_FOR_NEW_CLUSTER_VOLUMES 0x00000400
#define DRIVER_CONFIG_FLAGS_MAX_DATA_ALLOCATION_LIMIT                 0x00000800
#define DRIVER_CONFIG_FLAGS_AVAILABLE_DATA_BLOCK_COUNT                0x00001000
#define DRIVER_CONFIG_FLAGS_MAX_COALESCED_METADATA_CHANGE_SIZE	0x00002000
#define DRIVER_CONFIG_FLAGS_VALIDATION_LEVEL				0x00004000
#define DRIVER_CONFIG_DATA_POOL_SIZE                                  0x00008000
#define DRIVER_CONFIG_MAX_DATA_POOL_SIZE_PER_VOLUME                   0x00010000
#define DRIVER_CONFIG_FLAGS_VOL_THRESHOLD_FOR_FILE_WRITE              0x00020000
#define DRIVER_CONFIG_FLAGS_FREE_THRESHOLD_FOR_FILE_WRITE             0x00040000
#define DRIVER_CONFIG_FLAGS_DATA_FILTER_FOR_NEW_VOLUMES               0x00080000
#define DRIVER_CONFIG_FLAGS_FITERING_DISABLED_FOR_NEW_VOLUMES         0x00100000
#define DRIVER_CONFIG_FLAGS_ENABLE_DATA_CAPTURE	                      0x00200000
#define DRIVER_CONFIG_FLAGS_MAX_DATA_PER_NON_DATA_MODE_DIRTY_BLOCK    0x00400000
#define DRIVER_CONFIG_FLAGS_MAX_DATA_PER_DATA_MODE_DIRTY_BLOCK        0x00800000
#define DRIVER_CONFIG_FLAGS_DATA_LOG_DIRECTORY                        0x01000000
#define DRIVER_CONFIG_FLAGS_DATA_LOG_DIRECTORY_ALL                    0x03000000
#define DRIVER_CONFIG_FLAGS1_VALID                                    0x03FFFFFF


typedef struct _DRIVER_CONFIG
{
    ULONG   ulFlags1;                                    // 0x00
    ULONG   ulFlags2;                                    // 0x04
    ULONG   ulhwmServiceNotStarted;                      // 0x08
    ULONG   ulhwmServiceRunning;                         // 0x0C
    ULONG   ulhwmServiceShutdown;                        // 0x10
    ULONG   ullwmServiceRunning;                         // 0x14
    ULONG   ulChangesToPergeOnhwm;                       // 0x18
    ULONG   ulSecsBetweenMemAllocFailureEvents;          // 0x1C
    ULONG   ulSecsMaxWaitTimeOnFailOver;                 // 0x20
    ULONG   ulMaxLockedDataBlocks;                       // 0x24
    ULONG   ulMaxNonPagedPoolInMB;                       // 0x28
    ULONG   ulMaxWaitTimeForIrpCompletionInMinutes;      // 0x2c
    bool    bDisableVolumeFilteringForNewClusterVolumes; // 0x30
    UCHAR   ucReserved[3];                               // 0x31
// These tunables (in percentages) helps only to switch from Metadata to Data capture mode and does not have any other significance   
    ULONG   ulAvailableDataBlockCountInPercentage;       // 0x34
    ULONG   ulMaxDataAllocLimitInPercentage;             // 0x38
    ULONG   ulMaxCoalescedMetaDataChangeSize;	// 0x3c
    ULONG   ulValidationLevel;	// 0x40
    ULONG   ulDataPoolSize;     // 0x44
    ULONG   ulMaxDataSizePerVolume;                      // 0x48
    ULONG   ulReqNonPagePool;                            // 0x4c
    ULONG   ulDaBThresholdPerVolumeForFileWrite;         // 0x50
    ULONG   ulDaBFreeThresholdForFileWrite;              // 0x54
    bool    bEnableDataFilteringForNewVolumes;           // 0x58
    bool    bDisableVolumeFilteringForNewVolumes;        // 0x59 
    bool    bEnableDataCapture;                          // 0x5A
    UCHAR   ucReserved_1[1];                             // 0x5B
    ULONG   ulMaxDataSizePerNonDataModeDirtyBlock;       // 0x5C
    ULONG   ulMaxDataSizePerDataModeDirtyBlock;          // 0x60
    ULONG   ulError;                                     // 0x64
    struct {
        USHORT  usLength;                                   // 0x68 Filename length in bytes not including NULL
        WCHAR   FileName[UDIRTY_BLOCK_MAX_DATA_LOG_DIRECTORY];       // 0x6A
    } DataFile;
    ULONG   ulReserved[485];                              // 0x86A
    CHAR    cReserved[2];                                 //0xFFE
} DRIVER_CONFIG, *PDRIVER_CONFIG;                         //0x1000

#ifdef _CUSTOM_COMMAND_CODE_
typedef struct _DRIVER_CUSTOM_COMMAND
{
    ULONG   ulParam1;
    ULONG   ulParam2;
} DRIVER_CUSTOM_COMMAND, *PDRIVER_CUSTOM_COMMAND;
#endif // _CUSTOM_COMMAND_CODE_

// Version information stated with Driver Version 2.0.0.0
// This version has data stream changes
#define DRIVER_MAJOR_VERSION    0x02
// Version information with Driver Version 2.1.0.0 has Per IO Time Stamp changes
#define DRIVER_MINOR_VERSION    0x01
// Version information with Driver Version 2.1.1.0 has Write Order state changes
// Version information with Driver Version 2.1.3.0 has OOD changes
#define DRIVER_MINOR_VERSION2   0x03
#define DRIVER_MINOR_VERSION3   0x00

typedef struct _DRIVER_VERSION
{
    USHORT  ulDrMajorVersion;
    USHORT  ulDrMinorVersion;
    USHORT  ulDrMinorVersion2;
    USHORT  ulDrMinorVersion3;
    USHORT  ulPrMajorVersion;
    USHORT  ulPrMinorVersion;
    USHORT  ulPrMinorVersion2;
    USHORT  ulPrBuildNumber;
} DRIVER_VERSION, *PDRIVER_VERSION;

typedef struct _RESYNC_START_INPUT
{
    WCHAR       VolumeGUID[GUID_SIZE_IN_CHARS];     // 0x00
                                                    // 0x48
} RESYNC_START_INPUT, *PRESYNC_START_INPUT;

// Common structure name used earlier but as part of supporting Global Sequence number for 32-bit systems
// it has made to use V2 structures and kept other structure for backward compatiablility.
typedef struct _RESYNC_START_OUTPUT_V2
{
    ULONGLONG TimeInHundNanoSecondsFromJan1601;
    ULONGLONG ullSequenceNumber;
} RESYNC_START_OUTPUT_V2, *PRESYNC_START_OUTPUT_V2;

typedef struct _RESYNC_START_OUTPUT
{
    ULONGLONG   TimeInHundNanoSecondsFromJan1601;   // 0x00
    ULONG       ulSequenceNumber;                   // 0x08
    ULONG       ulReserved;                         // 0x0C
} RESYNC_START_OUTPUT, *PRESYNC_START_OUTPUT;

typedef struct _RESYNC_END_INPUT
{
    WCHAR       VolumeGUID[GUID_SIZE_IN_CHARS];     // 0x00
                                                    // 0x48
} RESYNC_END_INPUT, *PRESYNC_END_INPUT;

// Common structure name used earlier but as part of supporting Global Sequence number for 32-bit systems
// it has made to use V2 structures and kept other structure for backward compatiablility.
typedef struct _RESYNC_END_OUTPUT_V2
{
    ULONGLONG TimeInHundNanoSecondsFromJan1601;
    ULONGLONG ullSequenceNumber;
} RESYNC_END_OUTPUT_V2, *PRESYNC_END_OUTPUT_V2;

typedef struct _RESYNC_END_OUTPUT
{
    ULONGLONG   TimeInHundNanoSecondsFromJan1601;   // 0x00
    ULONG       ulSequenceNumber;                   // 0x08
    ULONG       ulReserved;                         // 0x0C
} RESYNC_END_OUTPUT, *PRESYNC_END_OUTPUT;


#define STOP_FILTERING_FLAGS_DELETE_BITMAP  0x0001
#define STOP_ALL_FILTERING_FLAGS            0x0002
typedef struct _STOP_FILTERING_INPUT
{
    WCHAR           VolumeGUID[GUID_SIZE_IN_CHARS];     // 0x00
    ULONG           ulFlags;                            // 0x48
    ULONG           ulReserved;                         // 0x4C
                                                        // 0x50
} STOP_FILTERING_INPUT, *PSTOP_FILTERING_INPUT;

#if (NTDDI_VERSION >= NTDDI_VISTA)
typedef enum {
    ecPartStyleMBR = 0,
    ecPartStyleGPT = 1,
    ecPartStyleUnknown = 0x0FA0,
} etPartStyle;
typedef struct _CDSKFL_INFO
{   
    etPartStyle   ePartitionStyle;
    ULONG         DeviceNumber;
    union {
        ULONG ulDiskSignature;
        GUID  DiskId;
    };
}CDSKFL_INFO, *PCDSKFL_INFO;
#endif
typedef struct _VOLUME_STATS_ADDITIONAL_INFO
{
    GUID      VolumeGuid;
    ULONGLONG ullTotalChangesPending;
    ULONGLONG ullOldestChangeTimeStamp;
    ULONGLONG ullDriverCurrentTimeStamp;
}VOLUME_STATS_ADDITIONAL_INFO, *PVOLUME_STATS_ADDITIONAL_INFO;

typedef struct StructGetDisks {
    bool IsVolumeAddedByEnumeration; // OUTPUT : true or false
    ULONG DiskSignaturesInputArrSize; // INPUT
    ULONG DiskSignaturesOutputArrSize; // OUTPUT
    ULONG DiskSignatures[1]; // INPUT and OUTPUT : INPUT : Disks known to incdsfl for which it already has mapping and these need not be processed/returned by involflt
} S_GET_DISKS;

#define FILE_NAME_SIZE_LCN 2048

typedef struct _GET_LCN {
    WCHAR FileName[FILE_NAME_SIZE_LCN]; //Max Filename can be FILE_NAME_SIZE_LCN - 1
    LARGE_INTEGER StartingVcn; 
    USHORT usFileNameLength; //Number of bytes
} GET_LCN, *PGET_LCN;

typedef struct _DRIVER_GLOBAL_TIMESTAMP
{
    ULONGLONG TimeInHundNanoSecondsFromJan1601;
    ULONGLONG ullSequenceNumber;
} DRIVER_GLOBAL_TIMESTAMP, *PDRIVER_GLOBAL_TIMESTAMP;
#endif // __INVOLFLT__H__
