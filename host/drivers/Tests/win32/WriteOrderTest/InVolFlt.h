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
#define DB_LOW_WATERMARK_SERVICE_RUNNING        L"DirtyBlockLowWaterMarkServiceRunning"
#define DB_HIGH_WATERMARK_SERVICE_RUNNING       L"DirtyBlockHighWaterMarkServiceRunning"
#define DB_HIGH_WATERMARK_SERVICE_SHUTDOWN      L"DirtyBlockHighWaterMarkServiceShutdown"
#define DB_TO_PURGE_HIGH_WATERMARK_REACHED      L"DirtyBlocksToPurgeWhenHighWaterMarkIsReached"
#define MAXIMUM_BITMAP_BUFFER_MEMORY            L"MaximumBitmapBufferMemory"
#define BITMAP_8K_GRANULARITY_SIZE              L"Bitmap8KGranularitySize"
#define BITMAP_16K_GRANULARITY_SIZE             L"Bitmap16KGranularitySize"
#define BITMAP_32K_GRANULARITY_SIZE             L"Bitmap32KGranularitySize"
#define DEFAULT_LOG_DIRECTORY                   L"DefaultLogDirectory"
#define DATA_BUFFER_SIZE                        L"DataBufferSize"
#define DATA_BLOCK_SIZE                         L"DataBlockSize"
#define DATA_POOL_SIZE                          L"DataPoolSize"
#define DATA_POOL_CHUNK_SIZE                    L"DataPoolChunkSize"
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

#define DEFAULT_TIME_IN_SECONDS_BETWEEN_NO_MEMORY_LOG_EVENTS    3600  // 60 minutes

#define DEFAULT_VOLUME_FILTERING_DISABLED_FOR_NEW_VOLUMES           0x01
#define DEFAULT_VOLUME_FILTERING_DISABLED_FOR_NEW_CLUSTER_VOLUMES   0x00

// This value is present at both Parameters section and Parameters\Volume section
#define VOLUME_DATA_FILTERING                   L"VolumeDataFiltering"
#define VOLUME_DATA_FILES                       L"VolumeDataFiles"
#define VOLUME_DATA_TO_DISK_LIMIT_IN_MB         L"VolumeDataToDiskLimitInMB"
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
#define SERVICE_REQUEST_DATA_SIZE_LIMIT         L"ServiceRequestDataSizeLimit"

// Mapped page writer thread has a priority of 17
// LOW_REALTIME_PRIORITY is 16. So setting priority to LOW_REALTIME_PRIORITY does not help.
#define DEFAULT_VOLUME_FILE_WRITERS_THREAD_PRIORITY     (19)    // LOW_REALTIME_PRIORITY + 3
#define MINIMUM_VOLUME_FILE_WRITERS_THREAD_PRIORITY     (9)
#define DEFAULT_NUMBER_OF_THREADS_FOR_EACH_FILE_WRITER  0x00
#define DEFAULT_NUMBER_OF_COMMON_FILE_WRITERS           0x01
#define DEFAULT_FREE_THREASHOLD_FOR_FILE_WRITE          20      // 20% = 12
#define DEFAULT_VOLUME_THRESHOLD_FOR_FILE_WRITE         25      // 25% = 8
#define DEFAULT_BITMAP_8K_GRANULARITY_SIZE              (520)   // in GBytes
#define DEFAULT_BITMAP_16K_GRANULARITY_SIZE             (1030)   // in GBytes
#define DEFAULT_BITMAP_32K_GRANULARITY_SIZE             (2060)   // in GBytes

#define DEFAULT_LOG_DIR_FOR_CLUSDISKS           L"\\InMageVolumeLogs\\"
#define DEFAULT_DATA_LOG_DIR_FOR_CLUSDISKS      L"\\InMageVolumeLogs\\"
#define DEFAULT_LOGDIR_PREFIX_FOR_CLUSTER_DISKS L"DataLog-"

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
#define VOLUME_LOG_PATH_NAME                    L"LogPathName"
#define VOLUME_DATALOG_DIRECTORY                L"VolumeDataLogDirectory"
#define VOLUME_READ_ONLY                        L"VolumeReadOnly"
#define VOLUME_FILE_WRITER_ID                   L"VolumeFileWriterId"

#define VOLUME_BITMAP_GRANULARITY_DEFAULT_VALUE         (0x1000)
#define VOLUME_BITMAP_OFFSET_DEFAULT_VALUE              (0)

#define VOLUME_FILTERING_DISABLED_SET                   (1)
#define VOLUME_FILTERING_DISABLED_UNSET                 (0)

#define VOLUME_DATA_FILTERING_SET                       (1)
#define VOLUME_DATA_FILTERING_UNSET                     (0)
// This value enables and disables data filitering for all volumes.
// If this is set to 0, data filtering is disabled for all volumes
#define DEFAULT_VOLUME_DATA_FILTERING                   (1)     // FALSE

#define VOLUME_DATA_FILES_SET                           (1)
#define VOLUME_DATA_FILES_UNSET                         (0)
#define DEFAULT_VOLUME_DATA_FILES                       (1)     // TRUE

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

#define DEFAULT_VOLUME_DATA_TO_DISK_LIMIT_IN_MB         (512)   // 512 MB
#define DEFAULT_VOLUME_DATA_NOTIFY_LIMIT_IN_KB          (512)   // 512 KB
                                                                // 
// Error Numbers logged to registry when volume is out of sync.
#define ERROR_TO_REG_OUT_OF_MEMORY_FOR_DIRTY_BLOCKS         0x0001
#define ERROR_TO_REG_BITMAP_READ_ERROR                      0x0002
#define ERROR_TO_REG_BITMAP_WRITE_ERROR                     0x0003
#define ERROR_TO_REG_BITMAP_OPEN_ERROR                      0x0004
#define ERROR_TO_REG_BITMAP_OPEN_FAIL_CHANGES_LOST          0x0005
#define ERROR_TO_REG_DESCRIPTION_IN_EVENT_LOG               0x0006
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

typedef enum _etFilteringMode
{
    ecFilteringModeUninitialized = 0,
    ecFilteringModeStartedNoIO = 1,
    ecFilteringModeBitmap = 2,
    ecFilteringModeMetaData = 3,
    ecFilteringModeData = 4
} etFilteringMode, *petFilteringMode;


//
// TODO: consider using IoRegisterDeviceInterface(); also see Q262305.
//
#define VOLUME_LETTER_IN_CHARS                   2
#define VOLUME_NAME_SIZE_IN_CHARS               44
#define VOLUME_NAME_SIZE_IN_BYTES              0x5A

// Device types in the range of 32768+ are allowed for non MS use; similarly for functions 2048-4095
#define FILE_DEVICE_INMAGE   0x00008001
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

#define IOCTL_INMAGE_SET_DRIVER_FLAGS           CTL_CODE( FILE_DEVICE_INMAGE, 0x813, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define IOCTL_INMAGE_SET_DATA_NOTIFY_LIMIT      CTL_CODE( FILE_DEVICE_INMAGE, 0x814, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define IOCTL_INMAGE_SET_DRIVER_CONFIGURATION   CTL_CODE( FILE_DEVICE_INMAGE, 0x815, METHOD_BUFFERED, FILE_ANY_ACCESS )

#define IOCTL_INMAGE_SET_DEBUG_INFO             CTL_CODE( FILE_DEVICE_INMAGE, 0x850, METHOD_BUFFERED, FILE_ANY_ACCESS )

#if DBG
#define IOCTL_INMAGE_ADD_DIRTY_BLOCKS_TEST      CTL_CODE( FILE_DEVICE_INMAGE, 0x852, METHOD_BUFFERED, FILE_ANY_ACCESS )
#endif // DBG

#define VOLUME_FLAG_IGNORE_PAGEFILE_WRITES  0x00000001
#define VOLUME_FLAG_DISABLE_BITMAP_READ     0x00000002
#define VOLUME_FLAG_DISABLE_BITMAP_WRITE    0x00000004
#define VOLUME_FLAG_READ_ONLY               0x00000008

#define VOLUME_FLAG_DISABLE_DATA_FILTERING  0x00000010
#define VOLUME_FLAG_FREE_DATA_BUFFERS       0x00000020
#define VOLUME_FLAG_DISABLE_DATA_FILES      0x00000040
#define VOLUME_FLAGS_VALID                  0x0000007F

#define SHUTDOWN_NOTIFY_FLAGS_ENABLE_DATA_FILTERING 0x00000001
#define SHUTDOWN_NOTIFY_FLAGS_ENABLE_DATA_FILES     0x00000002

typedef struct _SHUTDOWN_NOTIFY_INPUT
{
    ULONG   ulFlags;
} SHUTDOWN_NOTIFY_INPUT, *PSHUTDOWN_NOTIFY_INPUT;

#define PROCESS_START_NOTIFY_INPUT_FLAGS_DATA_FILES_AWARE   0x0001

typedef struct _PROCESS_START_NOTIFY_INPUT
{
    ULONG   ulFlags;
} PROCESS_START_NOTIFY_INPUT, *PPROCESS_START_NOTIFY_INPUT;

typedef struct _VOLUME_FLAGS_INPUT
{
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS + 1];
    // if eOperation is BitOpSet the bits in ulVolumeFlags will be set
    // if eOperation is BitOpReset the bits in ulVolumeFlags will be unset
    etBitOperation  eOperation;
    ULONG   ulVolumeFlags;
} VOLUME_FLAGS_INPUT, *PVOLUME_FLAGS_INPUT;

typedef struct _VOLUME_FLAGS_OUTPUT
{
    ULONG   ulVolumeFlags;
} VOLUME_FLAGS_OUTPUT, *PVOLUME_FLAGS_OUTPUT;

#define DRIVER_FLAG_DISABLE_DATA_FILES                  0x00000001
#define DRIVER_FLAG_DISABLE_DATA_FILES_FOR_NEW_VOLUMES  0x00000002
#define DRIVER_FLAGS_VALID                              0x00000003

typedef struct _DRIVER_FLAGS_INPUT
{
    // if eOperation is BitOpSet the bits in ulFlags will be set
    // if eOperation is BitOpReset the bits in ulFlags will be unset
    etBitOperation  eOperation;
    ULONG   ulFlags;
} DRIVER_FLAGS_INPUT, *PDRIVER_FLAGS_INPUT;

typedef struct _DRIVER_FLAGS_OUTPUT
{
    ULONG   ulFlags;
} DRIVER_FLAGS_OUTPUT, *PDRIVER_FLAGS_OUTPUT;

typedef struct _MSCS_VOLUME
{
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS + 1];
    ULONG   ulDriveLetterBitMap;
} MSCS_VOLUME, *PMSCS_VOLUME;

typedef struct _MSCS_VOLUME_IN_DISK
{
    ULONG   ulNumVolumes;
    ULONG   ulDiskSignature;
    ULONG   ulcbRequiredSize;
    ULONG   ulcbSizeReturned;
    MSCS_VOLUME VolumeArray[1];
} MSCS_VOLUMES_IN_DISK, *PMSCS_VOLUMES_IN_DISK;

#define VSF_BITMAP_READ_NOT_STARTED             0x00000001
#define VSF_BITMAP_READ_IN_PROGRESS             0x00000002
#define VSF_BITMAP_READ_PAUSED                  0x00000004
#define VSF_BITMAP_READ_COMPLETE                0x00000008
#define VSF_BITMAP_READ_ERROR                   0x00000010
#define VSF_BITMAP_WRITE_ERROR                  0x00000020
#define VSF_BITMAP_NOT_OPENED                   0x00000040

#define VSF_DATA_NOTIFY_SET                     0x02000000
#define VSF_DATA_FILES_DISABLED                 0x04000000
#define VSF_READ_ONLY                           0x08000000
#define VSF_FILTERING_STOPPED                   0x80000000
#define VSF_BITMAP_WRITE_DISABLED               0x40000000
#define VSF_BITMAP_READ_DISABLED                0x20000000
#define VSF_DATA_MODE_DISABLED                  0x10000000


#define VOLUME_STATS_DATA_MAJOR_VERSION         0x0002
#define VOLUME_STATS_DATA_MINOR_VERSION         0x0000

#define USER_MODE_MAX_IO_BUCKETS                    12

typedef struct _VOLUME_STATS
{
    WCHAR               VolumeGUID[GUID_SIZE_IN_CHARS];
    ULARGE_INTEGER      uliChangesReturnedToService;
    ULARGE_INTEGER      uliChangesReadFromBitMap;
    ULARGE_INTEGER      uliChangesWrittenToBitMap;

    ULARGE_INTEGER      ulicbChangesQueuedForWriting;
    ULARGE_INTEGER      ulicbChangesWrittenToBitMap;
    ULARGE_INTEGER      ulicbChangesReadFromBitMap;
    ULARGE_INTEGER      ulicbChangesReturnedToService;
    ULARGE_INTEGER      ulicbChangesPendingCommit;
    ULARGE_INTEGER      ulicbChangesReverted;
    ULARGE_INTEGER      ulicbChangesInBitmap;
    ULARGE_INTEGER      ulicbPendingChanges;

    ULONG               ulPendingChanges;
    ULONG               ulChangesPendingCommit;
    ULONG               ulChangesQueuedForWriting;
    LONG                lDirtyBlocksInQueue;

    LONG                lNumBitMapWriteErrors;
    ULONG               ulVolumeFlags;
    ULONG               ulChangesReverted;
    etVBitmapState      eVBitmapState;

    ULARGE_INTEGER      ulVolumeSize;

    WCHAR               DriveLetter;
    USHORT              usReserved;
    LONG                lNumBitmapOpenErrors;
    ULONG               ulNumberOfTagPointsDropped;
    etFilteringMode     eFilteringMode;

    LONG                lNumChangeToMetaDataFltMode;    // TODO:
    LONG                lNumChangeToMetaDataFltModeOnUserRequest;   // TODO:
    LONG                lNumChangeToDataFltMode; // TODO:
    LONG                lNumChangeToBitmapFltMode;  // TODO:

    LONG                lNumChangeToBitmapFltModeOnUserRequest; //TODO:
    ULONG               ulNumSecondsInDataMode;
    ULONG               ulNumSecondsInMetaDataMode;
    ULONG               ulNumSecondsInBitmapMode;

    ULONG               ulNumSecondsInCurrentMode;
    ULONG               ulcbDataBufferSizeAllocated;
    ULONG               ulcbDataBufferSizeFree;
    ULONG               ulcbDataBufferSizeReserved;
    // This is reserved space so that later additions would not
    // change the size of the structure. padding to mulitple of 8
    ULONG               ulFlushCount;
    ULONG               ulDataFilesPending;
    // TimeInHundNanoSecondsFromJan1601
    LONGLONG            llLastFlushTime;
    ULONGLONG           ullcbDataToDiskLimit;
    ULONGLONG           ullcbDiskUsed;
    ULONGLONG           ulDataFilesReturned;
    ULONG               ulDataFilesReverted;
    etFilteringMode     eModeOfNextDirtyBlock;
    etFilteringMode     ePrevFilteringMode;
    ULONG               ulConfiguredPriority;
    ULONG               ulEffectivePriority;
    ULONG               ulWriterId;
    ULONG               ulDuInstances;
#define USER_MODE_INSTANCES_OF_DISK_USAGE_TRACKED   0x05
    ULONGLONG           ullDuAtOutOfDataMode[USER_MODE_INSTANCES_OF_DISK_USAGE_TRACKED];
    ULONG               ulIoSizeArray[USER_MODE_MAX_IO_BUCKETS];
    ULONGLONG           ullIoSizeCounters[USER_MODE_MAX_IO_BUCKETS];
    ULONGLONG           ullcbDataPendingNotify;
    ULONG               ulcbDataNotifyThreshold;
    LONG                lNumBitMapReadErrors;
    LONG                lNumBitMapClearErrors;
    ULONG               ulNumMemoryAllocFailures;
    ULONGLONG           ullReserved[0x3D];
} VOLUME_STATS, *PVOLUME_STATS;

#define VSDF_SERVICE_DISABLED_DATA_MODE                 0x0001
#define VSDF_DRIVER_DISABLED_DATA_MODE                  0x0002
#define VSDF_DRIVER_FAILED_TO_ALLOCATE_DATA_POOL        0x0004
#define VSDF_SERVICE2_DID_NOT_REGISTER_FOR_DATA_MODE    0x0008
#define VSDF_SERVICE_DISABLED_DATA_FILES                0x0010
#define VSDF_DRIVER_DISABLED_DATA_FILES                 0x0020
#define VSDF_SERVICE2_DID_NOT_REGISTER_FOR_DATA_FILES   0x0040

typedef struct _VOLUME_STATS_DATA
{
    USHORT          usMajorVersion;
    USHORT          usMinorVersion;
    ULONG           ulTotalVolumes;
    ULONG           ulVolumesReturned;
    LONG            lDirtyBlocksAllocated;
    etServiceState  eServiceState;
    ULONG           ulDriverFlags;
    ULONG           ulDataPoolSizeAllocated;
    ULONG           ulDataPoolSizeFree;
    ULONG           ulDataPoolSizeFreeAndLocked;
    ULONG           ulNumMemoryAllocFailures;
    // This is reserved space so that later additions would not
    // change the size of the structure. padding to mulitple of 8
    ULONG           ulReserved[0x0C];
    VOLUME_STATS    VolumeArray[1];
} VOLUME_STATS_DATA, *PVOLUME_STATS_DATA;


//
// From our ioctl, we return the dirty blocks in a fixed length structure
// If this does not exhaust our dirty blocks (which we accumulate in a queue), the sentinel fetches more.
//
#define MAX_DIRTY_CHANGES 8192

typedef struct _DISK_CHANGE
{
    LARGE_INTEGER   ByteOffset;
    ULONG           Length;
    USHORT          usBufferIndex;
    USHORT          usNumberOfBuffers;
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

#define TAG_VOLUME_INPUT_FLAGS_ATOMIC_TO_VOLUME_GROUP   0x0001

#ifndef INVOFLT_STREAM_FUNCTIONS
#define INVOFLT_STREAM_FUNCTIONS

typedef struct _STREAM_REC_HDR_4B
{
    USHORT      usStreamRecType;
    UCHAR       ucFlags;
    UCHAR       ucLength; // Length includes size of this header too.
} STREAM_REC_HDR_4B, *PSTREAM_REC_HDR_4B;

typedef struct _STREAM_REC_HDR_8B
{
    USHORT      usStreamRecType;
    UCHAR       ucFlags;    // STREAM_REC_FLAGS_LENGTH_BIT bit is set for this record.
    UCHAR       ucReserved;
    ULONG       ulLength; // Length includes size of this header too.
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
        ((PSTREAM_REC_HDR_4B)pHeader)->ucLength = (UCHAR)Len;   \
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

typedef struct _TIME_STAMP_TAG
{
    STREAM_REC_HDR_4B   Header;
    ULONG       ulSequenceNumber;
    ULONGLONG   TimeInHundNanoSecondsFromJan1601;
} TIME_STAMP_TAG, *PTIME_STAMP_TAG;

#define INVOLFLT_DATA_SOURCE_UNDEFINED  0x00
#define INVOLFLT_DATA_SOURCE_BITMAP     0x01
#define INVOLFLT_DATA_SOURCE_META_DATA  0x02
#define INVOLFLT_DATA_SOURCE_DATA       0x03

typedef struct _DATA_SOURCE_TAG
{
    STREAM_REC_HDR_4B   Header;
    ULONG       ulDataSource;
} DATA_SOURCE_TAG, *PDATA_SOURCE_TAG;

#define UDIRTY_BLOCK_FLAG_START_OF_SPLIT_CHANGE 0x00000001
#define UDIRTY_BLOCK_FLAG_PART_OF_SPLIT_CHANGE  0x00000002
#define UDIRTY_BLOCK_FLAG_END_OF_SPLIT_CHANGE   0x00000004
#define UDIRTY_BLOCK_FLAG_DATA_FILE             0x00000008
#define UDIRTY_BLOCK_FLAG_SVD_STREAM			0x00000010

typedef struct _UDIRTY_BLOCK
{
#define UDIRTY_BLOCK_HEADER_SIZE    0x200   // 512 Bytes
#define UDIRTY_BLOCK_TAGS_SIZE      0xE00   // 7 * 512 Bytes
// UDIRTY_BLOCK_MAX_FILE_NAME is UDIRTY_BLOCK_TAGS_SIZE / sizeof(WCHAR) - 1(for length field)
#define UDIRTY_BLOCK_MAX_FILE_NAME  0x6FF   // (0xE00 /2) - 1
    // uHeader is .5 KB and uTags is 3.5 KB, uHeader + uTags = 4KB
    union {
        struct {
            ULARGE_INTEGER  uliTransactionID;
            ULARGE_INTEGER  ulicbChanges;
            ULONG           cChanges;
            ULONG           ulTotalChangesPending;
            ULONG           ulFlags;
            ULONG           ulSequenceIDforSplitIO;
            ULONG           ulBufferSize;
            USHORT          usMaxNumberOfBuffers;
            USHORT          usNumberOfBuffers;
            PVOID           *ppBufferArray;
            ULONG           ulcbBufferArraySize;
			ULONG			ulcbChangesInStream;
        } Hdr;
        UCHAR BufferReservedForHeader[UDIRTY_BLOCK_HEADER_SIZE];
    } uHdr;

    // Start of Markers
    union {
        struct {
            STREAM_REC_HDR_4B   TagStartOfList;
            STREAM_REC_HDR_4B   TagPadding;
            TIME_STAMP_TAG      TagTimeStampOfFirstChange;
            TIME_STAMP_TAG      TagTimeStampOfLastChange;
            DATA_SOURCE_TAG     TagDataSource;
            STREAM_REC_HDR_4B   TagEndOfList;
        } TagList;
        struct {
            USHORT  usLength; // Filename length in bytes not including NULL
            WCHAR   FileName[UDIRTY_BLOCK_MAX_FILE_NAME];
        } DataFile;
        UCHAR BufferForTags[UDIRTY_BLOCK_TAGS_SIZE];
    } uTagList;

    DISK_CHANGE     Changes[ MAX_DIRTY_CHANGES ];
} UDIRTY_BLOCK, *PUDIRTY_BLOCK; //, DIRTY_BLOCKS, *PDIRTY_BLOCKS;


typedef struct _COMMIT_TRANSACTION
{
    WCHAR           VolumeGUID[GUID_SIZE_IN_CHARS];
    ULARGE_INTEGER  ulTransactionID;
} COMMIT_TRANSACTION, *PCOMMIT_TRANSACTION;

typedef struct _VOLUME_DB_EVENT_INFO
{
    WCHAR       VolumeGUID[GUID_SIZE_IN_CHARS];
    HANDLE      hEvent;
} VOLUME_DB_EVENT_INFO, *PVOLUME_DB_EVENT_INFO;

#define DEBUG_INFO_FLAGS_RESET_MODULES  0x0001
#define DEBUG_INFO_FLAGS_SET_LEVEL_ALL  0x0002
#define DEBUG_INFO_FLAGS_SET_LEVEL      0x0004

typedef struct _DEBUG_INFO
{
    ULONG   ulDebugInfoFlags;
    ULONG   ulDebugLevelForAll;
    ULONG   ulDebugLevelForMod;
    ULONG   ulDebugModules;
} DEBUG_INFO, *PDEBUG_INFO;


#define SET_DATA_FILE_THREAD_PRIORITY_FLAGS_VALID_GUID  0x0001

typedef struct _SET_DATA_FILE_THREAD_PRIORITY_INPUT
{
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS + 2];
    ULONG   ulFlags;
    ULONG   ulThreadPriority;
    ULONG   ulDefaultThreadPriority;
    ULONG   ulThreadPriorityForAllVolumes;
} SET_DATA_FILE_THREAD_PRIORITY_INPUT, *PSET_DATA_FILE_THREAD_PRIORITY_INPUT;

typedef struct _SET_DATA_FILE_THREAD_PRIORITY_OUTPUT
{
    ULONG   ulFlags;
    ULONG   ulErrorInSettingForVolume;
    ULONG   ulErrorInSettingDefault;
    ULONG   ulErrorInSettingForAllVolumes;
} SET_DATA_FILE_THREAD_PRIORITY_OUTPUT, *PSET_DATA_FILE_THREAD_PRIORITY_OUTPUT;

#define SET_DATA_TO_DISK_SIZE_LIMIT_FLAGS_VALID_GUID  0x0001

typedef struct _SET_DATA_TO_DISK_SIZE_LIMIT_INPUT
{
    // Adding +2 for alignment and null terminator
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS + 2];
    ULONG   ulFlags;
    ULONG   ulDataToDiskSizeLimit;
    ULONG   ulDefaultDataToDiskSizeLimit;
    ULONG   ulDataToDiskSizeLimitForAllVolumes;
} SET_DATA_TO_DISK_SIZE_LIMIT_INPUT, *PSET_DATA_TO_DISK_SIZE_LIMIT_INPUT;

typedef struct _SET_DATA_TO_DISK_SIZE_LIMIT_OUTPUT
{
    ULONG   ulFlags;
    ULONG   ulErrorInSettingForVolume; // STATUS_DEVICE_DOES_NOT_EXIST
    ULONG   ulErrorInSettingDefault;
    ULONG   ulErrorInSettingForAllVolumes;
} SET_DATA_TO_DISK_SIZE_LIMIT_OUTPUT, *PSET_DATA_TO_DISK_SIZE_LIMIT_OUTPUT;

#define SET_DATA_NOTIFY_SIZE_LIMIT_FLAGS_VALID_GUID  0x0001

typedef struct _SET_DATA_NOTIFY_SIZE_LIMIT_INPUT
{
    // Adding +2 for alignment and null terminator
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS + 2];
    ULONG   ulFlags;
    ULONG   ulLimit;
    ULONG   ulDefaultLimit;
    ULONG   ulLimitForAllVolumes;
} SET_DATA_NOTIFY_SIZE_LIMIT_INPUT, *PSET_DATA_NOTIFY_SIZE_LIMIT_INPUT;

typedef struct _SET_DATA_NOTIFY_SIZE_LIMIT_OUTPUT
{
    ULONG   ulFlags;
    ULONG   ulErrorInSettingForVolume; // STATUS_DEVICE_DOES_NOT_EXIST
    ULONG   ulErrorInSettingDefault;
    ULONG   ulErrorInSettingForAllVolumes;
} SET_DATA_NOTIFY_SIZE_LIMIT_OUTPUT, *PSET_DATA_NOTIFY_SIZE_LIMIT_OUTPUT;

#define SET_IO_SIZE_ARRAY_INPUT_FLAGS_VALID_GUID    0x0001
typedef struct _SET_IO_SIZE_ARRAY_INPUT
{
    // Adding +2 for alignment and null terminator
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS + 2];
    ULONG   ulFlags;
    ULONG   ulArraySize;
    ULONG   ulIoSizeArray[USER_MODE_MAX_IO_BUCKETS];
} SET_IO_SIZE_ARRAY_INPUT, *PSET_IO_SIZE_ARRAY_INPUT;

typedef struct _GET_IO_SIZE_ARRAY_INPUT
{
    // Adding +2 for alignment and null terminator
    WCHAR   VolumeGUID[GUID_SIZE_IN_CHARS + 2];
    ULONG   ulFlags;
} GET_IO_SIZE_ARRAY_INPUT, *PGET_IO_SIZE_ARRAY_INPUT;

typedef struct _IO_SIZE_ARRAY_OUTPUT
{
    ULONG   ulArraySize;
    ULONG   ulIoSizeArray[USER_MODE_MAX_IO_BUCKETS];
} IO_SIZE_ARRAY_OUTPUT;

typedef struct _GET_IO_SIZE_ARRAY_OUTPUT
{
    ULONG   ulNumVolumes;
    IO_SIZE_ARRAY_OUTPUT    IoSizeArray[0x01];
} GET_IO_SIZE_ARRAY_OUTPUT, *PGET_IO_SIZE_ARRAY_OUTPUT;

#define DRIVER_CONFIG_FLAGS_HWM_SERVICE_NOT_STARTED   0x00000001
#define DRIVER_CONFIG_FLAGS_HWM_SERVICE_RUNNING       0x00000002
#define DRIVER_CONFIG_FLAGS_HWM_SERVICE_SHUTDOWN      0x00000004
#define DRIVER_CONFIG_FLAGS_LWM_SERVICE_RUNNING       0x00000008
#define DRIVER_CONFIG_FLAGS_CHANGES_TO_PURGE_ON_HWM   0x00000010
#define DRIVER_CONFIG_FLAGS_TIME_BETWEEN_MEM_FAIL_LOG 0x00000020

#define DRIVER_CONFIG_FLAGS1_VALID                    0x0000003F

typedef struct _DRIVER_CONFIG
{
    ULONG   ulFlags1;
    ULONG   ulFlags2;
    ULONG   ulhwmServiceNotStarted;
    ULONG   ulhwmServiceRunning;
    ULONG   ulhwmServiceShutdown;
    ULONG   ullwmServiceRunning;
    ULONG   ulChangesToPergeOnhwm;
    ULONG   ulSecsBetweenMemAllocFailureEvents;
    ULONG   ulReserved[58];
} DRIVER_CONFIG, *PDRIVER_CONFIG;
#endif // __INVOLFLT__H__
