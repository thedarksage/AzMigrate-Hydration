///
/// @file abhaiflt.h
/// Defines shared control codes for use in sentinel .
///
#ifndef __INMFLTINTERFACE__H__
#define __INMFLTINTERFACE__H__
#include <DrvDefines.h>
#include <tchar.h>
#include <DriverTags.h>
#include "InmFltIOCTL.h"

// CDO and symbolic link for volume filter
#define INMAGE_FILTER_DOS_DEVICE_NAME                   _T("\\\\.\\InMageFilter")
#define INMAGE_DEVICE_NAME                              L"\\Device\\InMageFilter"

// CDO and symbolic link for disk filter
#define INMAGE_DISK_FILTER_DOS_DEVICE_NAME              _T("\\\\.\\InMageDiskFilter")
#define INMAGE_DISK_FILTER_DEVICE_NAME                  L"\\Device\\InMageDiskFilter"

// Kernel object names for different memory conditions
#define LOW_NONPAGEDPOOL_CONDITION_KERNEL_OBJECT        L"\\KernelObjects\\LowNonPagedPoolCondition"
#define LOW_MEMORY_CONDITION_KERNEL_OBJECT              L"\\KernelObjects\\LowMemoryCondition"

// Timeout for checking Low memory condition
#define MAX_LOW_MEMORY_WAIT_TIMEOUT_MILLISECONDS            50

#define MAX_DATA_SIZE_PER_DATA_MODE_DIRTY_BLOCK_IN_MB       16
#define MIN_DATA_SIZE_PER_DATA_MODE_DIRTY_BLOCK_IN_MB       1

#define MAX_DATA_SIZE_PER_NON_DATA_MODE_DIRTY_BLOCK_IN_MB   64
#define MIN_DATA_SIZE_PER_NON_DATA_MODE_DIRTY_BLOCK_IN_MB   1

#define MAX_DATA_SIZE_PER_VOL_PERC 80

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
    // ecTagStatusFilteringStopped is returned if device filtering is stopped
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
    // Tag got revoked as part of protocol failure or user decided to revoke
    ecTagStatusRevoked = 8,
    // Tag Insert Failure
    ecTagStatusInsertFailure = 9,
    // Tag Insert Success
    ecTagStatusInsertSuccess = 10,
    // Tag Insert IOCTL failure
    ecTagStatusIOCTLFailure = 11,
    // Tag committed as part of drain
    ecTagStatusTagCommitDBSuccess = 12,
    //  Tag Non-Data WOS as part of precheck IOCTL for App Tag
    ecTagStatusTagNonDataWOS = 13,
    // Tag Data WOS
    ecTagStatusTagDataWOS = 14,
    ecTagStatusMaxEnum = 15
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
    ecWOSChangeReasonDontPageFault = 8,
    ecWOSChangeReasonPageFileMissed = 9,
    ecWOSChangeReasonExplicitNonWO = 10,
}etWOSChangeReason, *petWOSChangeReason;

typedef enum _etCModeMetaDataReason
{
    ecMDReasonUnInitialized = 0,
    ecMDReasonUserRequest,
    ecMDReasonServiceShutdown,
    ecMDReasonBitmapWrite,
    ecMDReasonExplicitByDriver,                  // Don't page fault, Page file missing and Surprise removal
    ecMDReasonMDLNull,
    ecMDReasonMDLNotMapped,
    ecMDReasonDataModeCompletionPath,            // This should not ideally set as the code paths already set the Metadata Capture
    ecMDReasonDataModeCompletionPathInternal,    // Represent failures as part of copying the change data to DirtyBlock
    ecMDReasonMetaDataModeCompletionpath,
    ecMDReasonInsertTagsLegacy,
    ecMDReasonMDLSystemVAMapNULL,
    ecMDReasonNotApplicable = 100                // Not applicable as the capture mode is moved to data and should not be part of telemetry
}etCModeMetaDataReason, *petCModeMetaDataReason;


typedef enum ShudownMarker {
    Uninitialized = 0,
    CleanShutdown = 1,
    DirtyShutdown = 2,
}ShutdownMarker, *pShutdownMarker;

typedef enum _etDriverMode {
    UninitializedMode = 0,
    NoRebootMode,
    RebootMode
}etDriverMode;

enum _ettagLogRecoveryState {
    uninit   = 0,   // new bitmap file
    clean    = 1,   // no changes lost
    dirty    = 2,   // system shutdown before changes written (i.e. crash)
    pending  = 3,   // Pending changes during closing bitmap
    resync   = 4    // Pending resync marked
    };

//
// TODO: consider using IoRegisterDeviceInterface(); also see Q262305.
//
#define VOLUME_LETTER_IN_CHARS                   2
#define VOLUME_NAME_SIZE_IN_CHARS               44
#define VOLUME_NAME_SIZE_IN_BYTES              0x5A


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
#define DRIVER_FLAG_DISK_RECOVERY_NEEDED                0x00000004

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

//Common Code. To avoid change in interface for application added new Dev Macros
#define INFLTDRV_DATA_SOURCE_UNDEFINED  INVOLFLT_DATA_SOURCE_UNDEFINED
#define INFLTDRV_DATA_SOURCE_BITMAP     INVOLFLT_DATA_SOURCE_BITMAP
#define INFLTDRV_DATA_SOURCE_META_DATA  INVOLFLT_DATA_SOURCE_META_DATA
#define INFLTDRV_DATA_SOURCE_DATA       INVOLFLT_DATA_SOURCE_DATA


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
#define UDIRTY_BLOCK_FLAG_CONTAINS_TAG              0x00000040

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

#define VSF_DONT_PAGE_FAULT                     0x00000080
#define VSF_LAST_IO                             0x00000100
#define VSF_BITMAP_DEV_OFF                      0x00000200
#define VSF_PAGE_FILE_MISSED                    0x00000400
#define VSF_DEVNUM_OBTAINED                     0x00000800
#define VSF_DEVSIZE_OBTAINED                    0x00001000
#define VSF_BITMAP_DEV                          0x00002000
#define VSF_EXPLICIT_NONWO_NODRAIN              0x00004000
#define VSF_DISK_ID_CONFLICT                    0x00008000

#define VSF_RECREATE_BITMAP_FILE                0x00010000
#define VSF_PHY_DEVICE_INRUSH                   0x00020000
#define VSF_VOLUME_ON_BASIC_DISK                0x00040000
//VOLUME_CLUSTER_SUPPORT
#define VSF_VOLUME_ON_CLUS_DISK                 0x00080000
#define VSF_CLUSTER_VOLUME_ONLINE               0x00100000

#define VSF_PAGEFILE_WRITES_IGNORED             0x00200000
#define VSF_VCFIELDS_PERSISTED                  0x00400000
#define VSF_SURPRISE_REMOVED                    0x00800000

//VOLUME_CLUSTER_SUPPORT
#define VSF_CLUSTER_VOLUME                      0x01000000

#define VSF_DATA_NOTIFY_SET                     0x02000000
#define VSF_DATA_FILES_DISABLED                 0x04000000
#define VSF_READ_ONLY                           0x08000000
#define VSF_DATA_CAPTURE_DISABLED               0x10000000
#define VSF_BITMAP_READ_DISABLED                0x20000000
#define VSF_BITMAP_WRITE_DISABLED               0x40000000
#define VSF_FILTERING_STOPPED                   0x80000000

#define VOLUME_STATS_DATA_MAJOR_VERSION         0x0003
#define VOLUME_STATS_DATA_MINOR_VERSION         0x0000

#define VOLUMES_DM_MAJOR_VERSION                0x0001
#define VOLUMES_DM_MINOR_VERSION                0x0000

// Keep this a multiple of 2 for 8 byte alignment in VOLUME_STATS structure
#define USER_MODE_MAX_IO_BUCKETS                    12
#define USER_MODE_INSTANCES_OF_MEM_USAGE_TRACKED    10

// ULONGLONG reserved fields for VOLUME STATS
#define VOLSTATS_RSVD                               41
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

    ULONGLONG           liLastCommittedTimeStampReadFromStore;                // 0x768
    ULONGLONG           liLastCommittedSequenceNumberReadFromStore;           // 0x770
    ULONG               liLastCommittedSplitIoSeqIdReadFromStore;             // 0x778
    ULONG               ulReserved;                                           // 0x77C

    ULONGLONG           ullCacheHit, ullCacheMiss;                            //0x788
    ULONG               ulTagsinWOState;                                      //0x798
    ULONG               ulTagsinNonWOSDrop;                                   //0x79C
    LARGE_INTEGER       liStartFilteringTimeStampByUser;                      //0x7A0
    LARGE_INTEGER       liGetDBTimeStamp;                                     //0x7A8
    LARGE_INTEGER       liCommitDBTimeStamp;                                  //0x7B0
    LARGE_INTEGER       liVolumeContextCreationTS;                            //0x7B8
    ULONG               ulDataBlocksReserved;                                 //0x7C0
    ULONG               ulNumOfPageFiles;                                     //0x7C4
    ULONG               ulRevokeTagDBCount;                                   //0x7C8
    ULONGLONG           IoCounterWithPagingIrpSet;                            //0x7CC
    ULONGLONG           IoCounterWithSyncPagingIrpSet;                        //0x7D4
    BOOLEAN             bPagingFilePossible;                                  //0x7DC
    BOOLEAN             bDeviceEnumerated;                                    //0x7DD
    UCHAR               ucShutdownMarker;                                     //0x7DE
    UCHAR               ucReserved[5];                                        //0x7DF
    ULONGLONG           ullOutOfSyncSeqNumber;                                //0x7E4
    ULONG               ulLastOutOfSyncErrorCode;                             //0x7E8
    ULONG               ulOutOfSyncTotalCount;                                //0x7EC
    ULONGLONG           ullLastOutOfSyncSeqNumber;                            //0x7F0
    LONGLONG            llInCorrectCompletionRoutineCount;                    //0x7F8
    LONGLONG            llIoCounterNonPassiveLevel;                           //0x800
    LONGLONG            llIoCounterWithNULLMdl;                               //0x808
    LONGLONG            llIoCounterWithInValidMDLFlags;                       //0x810
    LARGE_INTEGER       licbReturnedToServiceInNonDataMode;                   //0x818
    LARGE_INTEGER       liChangesReturnedToServiceInNonDataMode;              //0x820
    ULONGLONG           ullCounterTSODrained;                                 //0x828
    ULONGLONG           ullLastCommittedTagTimeStamp;                         //0x830
    ULONGLONG           ullLastCommittedTagSequeneNumber;                     //0x838
    _ettagLogRecoveryState BitmapRecoveryState;                               //0x840
    ULONG               rsvd;                                                 //0x844
    ULONGLONG           IoCounterWithNullFileObject;                          //0x848
    ULONGLONG           ullTotalTrackedBytes;                                 //0x850
    // This is reserved space so that later additions would not
    // change the size of the structure. padding to mulitple of 8
    LONG                lCntDevContextAllocs;                                 //0x858
    LONG                lCntDevContextFree;                                   //0x85C
    ULONGLONG           ullDataPoolSize;                                      //0x860
    LARGE_INTEGER       liStopFilteringTimestampByUser;                       //0x870
    LARGE_INTEGER       liStopFilteringAllTimeStamp;                          //0x880
    LARGE_INTEGER       liDriverLoadTime;                                     //0x890
    LARGE_INTEGER       liLastAgentStartTime;                                 //0x8A0
    LARGE_INTEGER       liLastAgentStopTime;                                  //0x8B0
    LARGE_INTEGER       liLastS2StartTime;                                    //0x8C0
    LARGE_INTEGER       liLastS2StopTime;                                     //0x8D0
    LARGE_INTEGER       liLastTagReq;                                         //0x8E0
    LONGLONG            llTimeJumpDetectedTS;                                 //0x8F0
    LONGLONG            llTimeJumpedTS;                                       //0x900
    LONGLONG            llIoCounterMDLSystemVAMapFailCount;                   //0x910
    ULONGLONG           Reserved[VOLSTATS_RSVD];                              //0x920
                                                                              //0xA00
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
    LONG            ulCommonBootCounter;           // 0x5C
    ULONGLONG       ullPersistedTimeStampAfterBoot;      // 0x60
    ULONGLONG       ullPersistedSequenceNumberAfterBoot; // 0x68
    UCHAR           LastShutdownMarker;                  // 0x70
    BOOLEAN         PersistentRegistryCreated;           // 0x71
    ULONG           eDiskFilterMode;                     // 0x72
    USHORT          ulNumProtectedDisk;                  // 0x76
    ULONGLONG       ulDaBInOrphanedMappedDBList;         // 0x78
    ULONG           LockedDataBlockCounter;              // 0x7c
    ULONG           MaxLockedDataBlockCounter;           // 0x80
    ULONG           MappedDataBlockCounter;              // 0x84
    ULONG           MaxMappedDataBlockCounter;           // 0x88
    ULONG           ulNumOfFreePageNodes;                // 0x8c
    ULONG           ulNumOfAllocPageNodes;               // 0x90
                                                         // 0x94

    LONGLONG            llNumQueuedIos;
    LONGLONG            llIoReleaseTimeInMicroSec;

} VOLUME_STATS_DATA, *PVOLUME_STATS_DATA;

typedef struct _VOLUME_DM_DATA
{
    union { //Common Code. To avoid change in interface for application added new Dev variables
        WCHAR           VolumeGUID[GUID_SIZE_IN_CHARS];     // 0x00
        WCHAR           DevID[GUID_SIZE_IN_CHARS];          // 0x00
    };
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

} VOLUME_DM_DATA, *PVOLUME_DM_DATA, DEV_DM_DATA, *PDEV_DM_DATA; 
//Common Code. To avoid change in interface for application added new Dev variables in union. 
//It is applicable to following common code comments

typedef struct _VOLUMES_DM_DATA
{
    USHORT          usMajorVersion;             // 0x00
    USHORT          usMinorVersion;             // 0x02
    union { //Common Code
        ULONG           ulTotalVolumes;         // 0x04
        ULONG           ulTotalDevs;            // 0x04
    };
    union { //Common Code
        ULONG           ulVolumesReturned;      // 0x08
        ULONG           ulDevReturned;          // 0x08
    };
    // This is reserved space so that later additions would not
    // change the size of the structure. padding to mulitple of 8
    ULONG           ulReserved[0x1D];           // 0x0C
    union { //Common Code
        VOLUME_DM_DATA    VolumeArray[1];       // 0x80
        DEV_DM_DATA       DevArray[1];          // 0x80
    };    
} VOLUMES_DM_DATA, *PVOLUMES_DM_DATA, DEVS_DM_DATA, *PDEVS_DM_DATA;

#define COMMIT_TRANSACTION_FLAG_RESET_RESYNC_REQUIRED_FLAG  0x00000001

typedef struct _COMMIT_TRANSACTION
{
    union { //Common Code
        WCHAR           VolumeGUID[GUID_SIZE_IN_CHARS];  // 0x00
        WCHAR           DevID[GUID_SIZE_IN_CHARS];       // 0x00
    };
    ULARGE_INTEGER  ulTransactionID;                    // 0x48
    ULONG           ulFlags;                            // 0x50
    ULONG           ulReserved;                         // 0x54
                                                        // 0x58
} COMMIT_TRANSACTION, *PCOMMIT_TRANSACTION;

typedef struct _VOLUME_DB_EVENT_INFO
{
    union { //Common Code
        WCHAR           VolumeGUID[GUID_SIZE_IN_CHARS];  // 0x00
        WCHAR           DevID[GUID_SIZE_IN_CHARS];       // 0x00
    };

    HANDLE      hEvent;                             // 0x48
                                                    // 0x50 for 64Bit 0x4C for 32bit
} VOLUME_DB_EVENT_INFO, *PVOLUME_DB_EVENT_INFO, DEV_DB_EVENT_INFO, *PDEV_DB_EVENT_INFO; //Common code

typedef struct _VOLUME_DB_EVENT_INFO32
{
    union { //Common Code
        WCHAR           VolumeGUID[GUID_SIZE_IN_CHARS];     // 0x00
        WCHAR           DevID[GUID_SIZE_IN_CHARS];          // 0x00
    };
    ULONG       hEvent;                             // 0x48
                                                    // 0x4C
} VOLUME_DB_EVENT_INFO32, *PVOLUME_DB_EVENT_INFO32, DEV_DB_EVENT_INFO32, *PDEV_DB_EVENT_INFO32; //Common code

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
    union { //Common Code
        WCHAR           VolumeGUID[GUID_SIZE_IN_CHARS + 2];  // 0x00
        WCHAR           DevID[GUID_SIZE_IN_CHARS + 2];       // 0x00
    };
    ULONG   ulFlags;                                // 0x4C
    ULONG   ulThreadPriority;                       // 0x50
    ULONG   ulDefaultThreadPriority;                // 0x54
    union { //Common Code
        ULONG   ulThreadPriorityForAllVolumes;       // 0x58
        ULONG   ulThreadPriorityForAllDevs;          // 0x58
    };
    ULONG   ulReserved;                             // 0x5C
                                                    // 0x60
} SET_DATA_FILE_THREAD_PRIORITY_INPUT, *PSET_DATA_FILE_THREAD_PRIORITY_INPUT;

typedef struct _SET_DATA_FILE_THREAD_PRIORITY_OUTPUT
{
    ULONG   ulFlags;                        // 0x00
    union { //Common Code
        ULONG   ulErrorInSettingForVolume;  // 0x04
        ULONG   ulErrorInSettingForDev;     // 0x04
    };
    ULONG   ulErrorInSettingDefault;        // 0x08
    union { //Common Code
        ULONG   ulErrorInSettingForAllVolumes;  // 0x0C
        ULONG   ulErrorInSettingForAllDevs;     // 0x0C
    };
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
    union { //Common Code
        WCHAR           VolumeGUID[GUID_SIZE_IN_CHARS + 2];  // 0x00
        WCHAR           DevID[GUID_SIZE_IN_CHARS + 2];       // 0x00
    };
    ULONG   ulFlags;                                // 0x4C
    ULONG   ulDataToDiskSizeLimit;                  // 0x50
    ULONG   ulDefaultDataToDiskSizeLimit;           // 0x54
    union { //Common Code
        ULONG   ulDataToDiskSizeLimitForAllVolumes; // 0x58
        ULONG   ulDataToDiskSizeLimitForAllDevices; // 0x58
    };
    ULONG   ulReserved;                             // 0x5C
                                                    // 0x60
} SET_DATA_TO_DISK_SIZE_LIMIT_INPUT, *PSET_DATA_TO_DISK_SIZE_LIMIT_INPUT;

typedef struct _SET_DATA_TO_DISK_SIZE_LIMIT_OUTPUT
{
    ULONG   ulFlags;                        // 0x00
    union { //Common Code
        ULONG   ulErrorInSettingForVolume;  // 0x04 STATUS_DEVICE_DOES_NOT_EXIST
        ULONG   ulErrorInSettingForDev;     // 0x04 STATUS_DEVICE_DOES_NOT_EXIST
    };

    ULONG   ulErrorInSettingDefault;        // 0x08
    union {
        ULONG   ulErrorInSettingForAllVolumes;  // 0x0C
        ULONG   ulErrorInSettingForAllDevs;     // 0x0C
    };
} SET_DATA_TO_DISK_SIZE_LIMIT_OUTPUT, *PSET_DATA_TO_DISK_SIZE_LIMIT_OUTPUT;

#define SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_FLAGS_VALID_GUID  0x0001

typedef struct _SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_INPUT
{
    // Adding +2 for alignment and null terminator
    union { //Common Code
        WCHAR           VolumeGUID[GUID_SIZE_IN_CHARS + 2];  // 0x00
        WCHAR           DevID[GUID_SIZE_IN_CHARS + 2];       // 0x00
    };
    ULONG   ulFlags;                                // 0x4C
    ULONG   ulMinFreeDataToDiskSizeLimit;                  // 0x50
    ULONG   ulDefaultMinFreeDataToDiskSizeLimit;           // 0x54
    union { //Common Code
        ULONG   ulMinFreeDataToDiskSizeLimitForAllVolumes; // 0x58
        ULONG   ulMinFreeDataToDiskSizeLimitForAllDevices; // 0x58
    };
    ULONG   ulReserved;                             // 0x5C
                                                    // 0x60
} SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_INPUT, *PSET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_INPUT;

typedef struct _SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_OUTPUT
{
    ULONG   ulFlags;                        // 0x00
    union { //Common Code
        ULONG   ulErrorInSettingForVolume;  // 0x04 STATUS_DEVICE_DOES_NOT_EXIST
        ULONG   ulErrorInSettingForDev;     // 0x04 STATUS_DEVICE_DOES_NOT_EXIST
    };
    ULONG   ulErrorInSettingDefault;        // 0x08
    union { //Common Code
        ULONG   ulErrorInSettingForAllVolumes;  // 0x0C
        ULONG   ulErrorInSettingForAllDevs;     // 0x0C
    };
                                            // 0x10
} SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_OUTPUT, *PSET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_OUTPUT;


#define SET_DATA_NOTIFY_SIZE_LIMIT_FLAGS_VALID_GUID  0x0001

typedef struct _SET_DATA_NOTIFY_SIZE_LIMIT_INPUT
{
    // Adding +2 for alignment and null terminator
    union { //Common Code
        WCHAR           VolumeGUID[GUID_SIZE_IN_CHARS + 2];  // 0x00
        WCHAR           DevID[GUID_SIZE_IN_CHARS + 2];       // 0x00
    };
    ULONG   ulFlags;                                // 0x4C
    ULONG   ulLimit;                                // 0x50
    ULONG   ulDefaultLimit;                         // 0x54
    union { //Common Code
        ULONG   ulLimitForAllVolumes;               // 0x58
        ULONG   ulLimitForAllDevices;               // 0x58
    };
    ULONG   ulReserved;                             // 0x5C
                                                    // 0x60
} SET_DATA_NOTIFY_SIZE_LIMIT_INPUT, *PSET_DATA_NOTIFY_SIZE_LIMIT_INPUT;

typedef struct _SET_DATA_NOTIFY_SIZE_LIMIT_OUTPUT
{
    ULONG   ulFlags;                        // 0x00
    union { //Common Code
        ULONG   ulErrorInSettingForVolume;  // 0x04 STATUS_DEVICE_DOES_NOT_EXIST
        ULONG   ulErrorInSettingForDev;     // 0x04 STATUS_DEVICE_DOES_NOT_EXIST
    };
    ULONG   ulErrorInSettingDefault;        // 0x08
    union { //Common Code
        ULONG   ulErrorInSettingForAllVolumes; // 0x0C
        ULONG   ulErrorInSettingForAllDevs;    // 0x0C
    };
                                            // 0x10
} SET_DATA_NOTIFY_SIZE_LIMIT_OUTPUT, *PSET_DATA_NOTIFY_SIZE_LIMIT_OUTPUT;

typedef struct _SET_RESYNC_REQUIRED
{
    union { //Common Code
        WCHAR           VolumeGUID[GUID_SIZE_IN_CHARS + 2];  // 0x00
        WCHAR           DevID[GUID_SIZE_IN_CHARS + 2];       // 0x00
    };
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
    union { //Common Code
        WCHAR  VolumeGUID[GUID_SIZE_IN_CHARS + 2];  // 0x00
        WCHAR  DevID[GUID_SIZE_IN_CHARS + 2];       // 0x00
    };
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
#define DRIVER_CONFIG_FLAGS_DATA_LOG_DIRECTORY_ALL                    0x02000000
#define DRIVER_CONFIG_FLAGS_ENABLE_FS_FLUSH                           0x04000000
#define DRIVER_CONFIG_FLAGS1_VALID                                    0x07FFFFFF


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
    bool    bEnableFSFlushPreShutdown;                   // 0x31
    UCHAR   ucReserved[2];                               // 0x32
// These tunables (in percentages) helps only to switch from Metadata to Data capture mode and does not have any other significance   
    ULONG   ulAvailableDataBlockCountInPercentage;       // 0x34
    ULONG   ulMaxDataAllocLimitInPercentage;             // 0x38
    ULONG   ulMaxCoalescedMetaDataChangeSize;	// 0x3c
    ULONG   ulValidationLevel;	// 0x40
    ULONG   ulDataPoolSize;     // 0x44
    union { //Common Code
        ULONG   ulMaxDataSizePerVolume;                  // 0x48
        ULONG   ulMaxDataSizePerDev;                     // 0x48
    };
    ULONG   ulReqNonPagePool;                            // 0x4c
    union { //Common Code
        ULONG   ulDaBThresholdPerVolumeForFileWrite;      // 0x50
        ULONG   ulDaBThresholdPerDevForFileWrite;         // 0x50
    };
    ULONG   ulDaBFreeThresholdForFileWrite;              // 0x54
    union { //Common Code
        bool    bEnableDataFilteringForNewVolumes;       // 0x58
        bool    bEnableDataFilteringForNewDevs;          // 0x58
    };
    union { //Common Code
        bool    bDisableVolumeFilteringForNewVolumes;    // 0x59 
        bool    bDisableFilteringForNewDevs;             // 0x59 
    };
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

// VOLUME FILTER FEATURE VERSION 
// Version information stated with Driver Version 2.0.0.0
// This version has data stream changes
#define DRIVER_MAJOR_VERSION    0x02
// Version information with Driver Version 2.1.0.0 has Per IO Time Stamp changes
#define DRIVER_MINOR_VERSION    0x01
// Version information with Driver Version 2.1.1.0 has Write Order state changes
// Version information with Driver Version 2.1.3.0 has OOD changes
#define DRIVER_MINOR_VERSION2   0x03
#define DRIVER_MINOR_VERSION3   0x00

// DISK FILTER FEATURE VERSION
// New disk filter driver to start with version 1.0.0.0, which by default has all the features supported by volume filter

// 1.0.0.1 - Added support for Driver telemetry(First Version)
// 1.0.0.3 - Added new pre-check IOCTL for App tag
// 1.0.0.4   Added CX Version
// 1.0.0.8   Added tag commit notify Version
#define DISKFLT_DRIVER_MAJOR_VERSION    0x01
#define DISKFLT_DRIVER_MINOR_VERSION    0x00
#define DISKFLT_DRIVER_MINOR_VERSION2   0x2C
#define DISKFLT_DRIVER_MINOR_VERSION3   0x03

#define DISKFLT_MV3_TAG_PRECHECK_SUPPORT    0x02
#define DISKFLT_MV2_CXSESSION_SUPPORT       0x04

// DTAP end
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
    union { //Common Code
        WCHAR      VolumeGUID[GUID_SIZE_IN_CHARS];  // 0x00
        WCHAR      DevID[GUID_SIZE_IN_CHARS];       // 0x00
    };
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
    union { //Common Code
        WCHAR      VolumeGUID[GUID_SIZE_IN_CHARS];  // 0x00
        WCHAR      DevID[GUID_SIZE_IN_CHARS];       // 0x00
    };
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


#define STOP_FILTERING_FLAGS_DELETE_BITMAP                  0x0001
#define STOP_ALL_FILTERING_FLAGS                            0x0002
#define STOP_FILTERING_FLAGS_DONT_CLEAN_CLUSTER_STATE       0x0004

typedef struct _STOP_FILTERING_INPUT
{
    union { //Common Code
        WCHAR           VolumeGUID[GUID_SIZE_IN_CHARS]; // 0x00
        WCHAR           DevID[GUID_SIZE_IN_CHARS];      // 0x00
    };
    ULONG           ulFlags;                            // 0x48
    ULONG           ulReserved;                         // 0x4C
                                                        // 0x50
} STOP_FILTERING_INPUT, *PSTOP_FILTERING_INPUT;

#define SET_SAN_ALL_DEVICES_FLAGS            0x0001
typedef struct _SET_SAN_POLICY
{
    WCHAR           DevID[GUID_SIZE_IN_CHARS];
    ULONG           ulFlags;
    ULONG           ulPolicy;
} SET_SAN_POLICY, *PSET_SAN_POLICY;

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
    ULONG DiskSignatures[1]; // INPUT and OUTPUT : INPUT : Disks known to incdsfl for which it already has mapping and these need not be processed/returned by filter
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

typedef enum {
    ePartStyleMBR = 0,
    ePartStyleGPT = 1,
    ePartStyleUnknown = 0x0FA0,
} eDskPartitionStyle;

// This structure is specific to disk filter driver. This is added to update driver with latest disk size and some other 
// information which can be helpful without querying explicitly

#define IS_CLUSTERED_DISK       0x0001
#define IS_MULTIPATH_DISK       0x0002
#define IS_BASIC_DISK           0x0004
#define IS_DYNAMIC_DISK         0x0008

typedef struct _START_FILTERING_INPUT {
    WCHAR         DeviceId[GUID_SIZE_IN_CHARS]; // Either Disk signature or GUID
    LARGE_INTEGER DeviceSize;
    eDskPartitionStyle   ePartStyle;
    ULONG         ulFlags;
}START_FILTERING_INPUT, *PSTART_FILTERING_INPUT;

typedef struct _SET_DISK_CLUSTERED_INPUT {
    WCHAR           DeviceId[GUID_SIZE_IN_CHARS]; // Either Disk signature or GUID
    LARGE_INTEGER   DeviceSize;
    ULONG           ulFlags;
    WCHAR           BitmapPath[FILE_NAME_SIZE_LCN];
}SET_DISK_CLUSTERED_INPUT, * PSET_DISK_CLUSTERED_INPUT;

// Tag Info
#define TAG_INFO_FLAGS_WAIT_FOR_COMMIT              0x00000001
#define TAG_INFO_FLAGS_IGNORE_IF_FILTERING_STOPPED  0x00000002
#define TAG_INFO_FLAGS_TYPE_SYNC                    0x00000004
#define TAG_INFO_FLAGS_INCLUDE_ALL_FILTERED_DISKS   0x00000008
#define TAG_INFO_FLAGS_IGNORE_DISKS_IN_NON_WO_STATE 0x00000010
#define TAG_INFO_FLAGS_TAG_IF_ZERO_INFLIGHT_IO	    0x00000020
#define TAG_INFO_FLAGS_COMMIT_TAG	                0x00000040
#define CRASHCONSISTENT_FLAG_HOLD_WRITE_TIMEOUT     0x00000080
#define TAG_INFO_FLAGS_LOCAL_CRASH                  0x00000100
#define TAG_INFO_FLAGS_DISTRIBUTED_CRASH            0x00000200
#define TAG_INFO_FLAGS_LOCAL_APP                    0x00000400
#define TAG_INFO_FLAGS_DISTRIBUTED_APP              0x00000800
#define TAG_INFO_FLAGS_BASELINE_APP                 0x00001000

typedef struct _TAG_PRECHECK_INPUT {
    UCHAR        TagMarkerGUID[GUID_SIZE_IN_CHARS]; // Unique TagMarker GUID which is common across single or multi-node
    ULONG        ulFlags;                           // specify single app or multi-app..By default single-app
    USHORT       usNumDisks;                        // number of disks for which Disk Id is sent as blob of data
    UCHAR        DiskIdList[1];                     // DiskId1, DiskId2,...size = GUID_SIZE_IN_BYTES as per the 
                                                    // structure for IOCTL_INMAGE_TAG_DISK, Let's maintain the same
}TAG_PRECHECK_INPUT, *PTAG_PRECHECK_INPUT;

// Output structure for detailed status of Tag IOCTL
typedef struct _TAG_DISK_STATUS_OUTPUT
{
    ULONG       ulNumDisks;
    LONG        TagStatus[1];
} TAG_DISK_STATUS_OUTPUT, *PTAG_DISK_STATUS_OUTPUT;

typedef struct _CRASH_CONSISTENT_INPUT
{
    UCHAR       TagContext[GUID_SIZE_IN_CHARS];
    ULONG       ulFlags;
}CRASH_CONSISTENT_INPUT, *PCRASH_CONSISTENT_INPUT, TAG_DISK_COMMIT_INPUT, *PTAG_DISK_COMMIT_INPUT;

typedef struct _CRASH_CONSISTENT_HOLDWRITE_INPUT : CRASH_CONSISTENT_INPUT
{
    ULONG       ulTimeout;
    UCHAR       TagMarkerGUID[GUID_SIZE_IN_CHARS]; // Unique TagMarker GUID is explicitly sent as part of IOCTL_INMAGE_HOLD_WRITES
}CRASH_CONSISTENT_HOLDWRITE_INPUT, *PCRASH_CONSISTENT_HOLDWRITE_INPUT;

typedef struct _CRASHCONSISTENT_TAG_INPUT : CRASH_CONSISTENT_INPUT
{
    ULONG       ulNumOfTags;
    UCHAR       TagBuffer[1];
}CRASHCONSISTENT_TAG_INPUT, *PCRASHCONSISTENT_TAG_INPUT;


typedef struct _CRASHCONSISTENT_TAG_OUTPUT
{
    ULONG       ulNumFilteredDisks;
    ULONG       ulNumTaggedDisks;
}CRASHCONSISTENT_TAG_OUTPUT, *PCRASHCONSISTENT_TAG_OUTPUT;

#define SIZE_OF_TAG_DISK_STATUS_OUTPUT_DATA(x) ((ULONG)FIELD_OFFSET(TAG_DISK_STATUS_OUTPUT, TagStatus[x]))


#pragma region Monitoring statistics

typedef enum _MONITORING_STATISTIC
{
    Invalid = 0,
    CumulativeTrackedIOSize = 1,
    CumulativeDroppedTagsCount = 2,
    Count = 3
} MONITORING_STATISTIC, *PMONITORING_STATISTIC;

typedef struct _MON_STAT_CUMULATIVE_TRACKED_IO_SIZE_INPUT
{
    WCHAR   DeviceId[GUID_SIZE_IN_CHARS];
} MON_STAT_CUMULATIVE_TRACKED_IO_SIZE_INPUT, *PMON_STAT_CUMULATIVE_TRACKED_IO_SIZE_INPUT;

typedef struct _MON_STAT_CUMULATIVE_TRACKED_IO_SIZE_OUTPUT
{
    ULONGLONG   ullTotalTrackedBytes;
} MON_STAT_CUMULATIVE_TRACKED_IO_SIZE_OUTPUT, *PMON_STAT_CUMULATIVE_TRACKED_IO_SIZE_OUTPUT;

#define MonStatOutputCumulativeTrackedIOSizeMinSize (sizeof(MON_STAT_CUMULATIVE_TRACKED_IO_SIZE_OUTPUT))

typedef struct _MON_STAT_CUMULATIVE_DROPPED_TAGS_COUNT_INPUT
{
    WCHAR   DeviceId[GUID_SIZE_IN_CHARS];
} MON_STAT_CUMULATIVE_DROPPED_TAGS_COUNT_INPUT, *PMON_STAT_CUMULATIVE_DROPPED_TAGS_COUNT_INPUT;

typedef struct _MON_STAT_CUMULATIVE_DROPPED_TAGS_COUNT_OUTPUT
{
    ULONGLONG   ullDroppedTagsCount;
} MON_STAT_CUMULATIVE_DROPPED_TAGS_COUNT_OUTPUT, *PMON_STAT_CUMULATIVE_DROPPED_TAGS_COUNT_OUTPUT;

#define MonStatOutputCumulativeDroppedTagsCountMinSize (sizeof(MON_STAT_CUMULATIVE_DROPPED_TAGS_COUNT_OUTPUT))

typedef struct _MONITORING_STATS_INPUT
{
    MONITORING_STATISTIC    Statistic;

    union
    {
        MON_STAT_CUMULATIVE_TRACKED_IO_SIZE_INPUT CumulativeTrackedIOSize;        // CumulativeTrackedIOSize = 1
        MON_STAT_CUMULATIVE_DROPPED_TAGS_COUNT_INPUT CumulativeDroppedTagsCount;  // CumulativeDroppedTagsCount = 2
    };
} MONITORING_STATS_INPUT, *PMONITORING_STATS_INPUT;

#define MonStatInputMinSize (RTL_SIZEOF_THROUGH_FIELD(MONITORING_STATS_INPUT, Statistic))
#define MonStatInputCumulativeTrackedIOSizeMinSize (RTL_SIZEOF_THROUGH_FIELD(MONITORING_STATS_INPUT, CumulativeTrackedIOSize))
#define MonStatInputCumulativeDroppedTagsCountMinSize (RTL_SIZEOF_THROUGH_FIELD(MONITORING_STATS_INPUT, CumulativeDroppedTagsCount))

#pragma endregion Monitoring statistics

// REPLICATION_STATE_DIFF_SYNC_THROTTLED should be updated
// Whenever a new flag is added.
#define REPLICATION_STATE_DIFF_SYNC_THROTTLED           0x00000001
#define REPLICATION_STATES_SUPPORTED                    (REPLICATION_STATE_DIFF_SYNC_THROTTLED)

typedef struct _REPLICATION_STATE{
    WCHAR         DeviceId[GUID_SIZE_IN_CHARS];
    ULONGLONG     ulFlags;
    ULONGLONG     Timestamp;
    UCHAR         Data[1];
}REPLICATION_STATE, *PREPLICATION_STATE;

typedef struct _REPLICATION_STATS{
    LONGLONG       llDiffSyncThrottleStartTime;
    LONGLONG       llDiffSyncThrottleEndTime;
    LONGLONG       llPauseStartTime;
    LONGLONG       llPauseEndTime;
    LONGLONG       llResyncThrottleStartTime;
    LONGLONG       llResyncThrottleEndTime;
}REPLICATION_STATS, *PREPLICATION_STATS;

typedef struct _DISKINDEX_INFO{
    ULONG         ulNumberOfDisks;
    ULONG         aDiskIndex[1];
}DISKINDEX_INFO , *PDISKINDEX_INFO;

// Commit failure Related flags
#define COMMITDB_NETWORK_FAILURE     0x00000001

typedef struct _COMMIT_DB_FAILURE_STATS{
    WCHAR           DeviceID[GUID_SIZE_IN_CHARS];
    ULARGE_INTEGER  ulTransactionID;
    ULONGLONG       ullFlags;
    ULONGLONG       ullErrorCode;
} COMMIT_DB_FAILURE_STATS, *PCOMMIT_DB_FAILURE_STATS;

// CX Related flags
#define CXSTATUS_COMMIT_PREV_SESSION     0x00000001

typedef struct _GET_CXFAILURE_NOTIFY{
    ULONGLONG       ullFlags;
    ULARGE_INTEGER  ulTransactionID;
    ULONGLONG       ullMinConsecutiveTagFailures;
    ULONGLONG       ullMaxVMChurnSupportedMBps;
    ULONGLONG       ullMaxDiskChurnSupportedMBps;
    ULONGLONG       ullMaximumTimeJumpFwdAcceptableInMs;
    ULONGLONG       ullMaximumTimeJumpBwdAcceptableInMs;
    ULONG           ulNumberOfOutputDisks;
    ULONG           ulNumberOfProtectedDisks;
    WCHAR           DeviceIdList[1][GUID_SIZE_IN_CHARS];
} GET_CXFAILURE_NOTIFY, *PGET_CXFAILURE_NOTIFY;

#define DEFAULT_NR_CHURN_BUCKETS                11

// Disk Failure Flags
#define DISK_CXSTATUS_NWFAILURE_FLAG                0x00000001
#define DISK_CXSTATUS_PEAKCHURN_FLAG                0x00000002
#define DISK_CXSTATUS_CHURNTHROUGHPUT_FLAG          0x00000004
#define DISK_CXSTATUS_EXCESS_CHURNBUCKET_FLAG       0x00000008
#define DISK_CXSTATUS_MAX_CHURNTHROUGHPUT_FLAG      0x00000010
#define DISK_CXSTATUS_DISK_REMOVED                  0x00000020
#define DISK_CXSTATUS_DISK_NOT_FILTERED             0x00000040

typedef struct _DEVICE_CXFAILURE_STATS{
    WCHAR           DeviceId[GUID_SIZE_IN_CHARS];       // Device ID
    ULONGLONG       ullFlags;                           // Flags
    ULONGLONG       ChurnBucketsMBps[DEFAULT_NR_CHURN_BUCKETS]; // Peak churn count in each buckets

    // Unless DISK_CXSTATUS_EXCESS_CHURNBUCKET_FLAG is set this field is invalid.
    ULONGLONG       ExcessChurnBucketsMBps[DEFAULT_NR_CHURN_BUCKETS]; // Excess Churn buckets

    ULONGLONG       CxStartTS;

    // Unless DISK_CXSTATUS_MAX_CHURNTHROUGHPUT_FLAG is set this field is invalid.
    ULONGLONG       ullMaxDiffChurnThroughputTS;
    ULONGLONG       firstNwFailureTS;
    ULONGLONG       lastNwFailureTS;
    ULONGLONG       firstPeakChurnTS;
    ULONGLONG       lastPeakChurnTS;
    ULONGLONG       CxEndTS;

    ULONGLONG       ullLastNWErrorCode;
    ULONGLONG       ullMaximumPeakChurnInBytes;
    ULONGLONG       ullDiffChurnThroughputInBytes;

    // Unless DISK_CXSTATUS_MAX_CHURNTHROUGHPUT_FLAG is set this field is invalid.
    ULONGLONG       ullMaxDiffChurnThroughputInBytes;
    ULONGLONG       ullTotalNWErrors;
    ULONGLONG       ullNumOfConsecutiveTagFailures;
    ULONGLONG       ullTotalExcessChurnInBytes;
    ULONGLONG       ullMaxS2LatencyInMS;
}DEVICE_CXFAILURE_STATS, *PDEVICE_CXFAILURE_STATS;

// VM failure flags
#define VM_CXSTATUS_PEAKCHURN_FLAG                0x00000001
#define VM_CXSTATUS_CHURNTHROUGHPUT_FLAG          0x00000002
#define VM_CXSTATUS_TIMEJUMP_FWD_FLAG             0x00000004
#define VM_CXSTATUS_TIMEJUMP_BCKWD_FLAG           0x00000008
#define VM_CXSTATUS_EXCESS_CHURNBUCKETS_FLAG      0x00000010
#define VM_CXSTATUS_MAX_CHURNTHROUGHPUT_FLAG      0x00000020

typedef struct _VM_CXFAILURE_STATS{
    ULONGLONG       ullFlags;
    ULARGE_INTEGER  ulTransactionID;
    ULONGLONG       ChurnBucketsMBps[DEFAULT_NR_CHURN_BUCKETS];

    // Unless VM_CXSTATUS_EXCESS_CHURNBUCKETS_FLAG is set this field is invalid.
    ULONGLONG       ExcessChurnBucketsMBps[DEFAULT_NR_CHURN_BUCKETS];

    ULONGLONG       CxStartTS;
    // Unless VM_CXSTATUS_MAX_CHURNTHROUGHPUT_FLAG is set this field is invalid.
    ULONGLONG       ullMaxChurnThroughputTS;
    ULONGLONG       firstPeakChurnTS;
    ULONGLONG       lastPeakChurnTS;
    ULONGLONG       CxEndTS;

    ULONGLONG       ullMaximumPeakChurnInBytes;
    ULONGLONG       ullDiffChurnThroughputInBytes;

    // Unless VM_CXSTATUS_MAX_CHURNTHROUGHPUT_FLAG is set this field is invalid.
    ULONGLONG       ullMaxDiffChurnThroughputInBytes;
    ULONGLONG       ullTotalExcessChurnInBytes;

    ULONGLONG       TimeJumpTS;
    ULONGLONG       ullTimeJumpInMS;

    ULONGLONG       ullNumOfConsecutiveTagFailures;

    ULONGLONG       ullMaxS2LatencyInMS;

    // Number of disks
    ULONGLONG                   ullNumDisks;
    DEVICE_CXFAILURE_STATS      DeviceCxStats[1];
}VM_CXFAILURE_STATS, *PVM_CXFAILURE_STATS;

#define DISK_CXSESSION_START_THRESOLD                   (8*1024*1024)

// Structures and macro for IOCTL_INMAGE_NOTIFY_SYSTEM_STATE
enum NOTIFY_SYSTEM_STATE_FLAGS
{
    ssFlagsAreBitmapFilesEncryptedOnDisk   =   1 << 0
};

typedef struct _NOTIFY_SYSTEM_STATE_INPUT {
    UINT32  Flags;
}NOTIFY_SYSTEM_STATE_INPUT, *PNOTIFY_SYSTEM_STATE_INPUT;

typedef const NOTIFY_SYSTEM_STATE_INPUT *PCNOTIFY_SYSTEM_STATE_INPUT;

typedef struct _NOTIFY_SYSTEM_STATE_OUTPUT {
    UINT32  ResultFlags;
}NOTIFY_SYSTEM_STATE_OUTPUT, *PNOTIFY_SYSTEM_STATE_OUTPUT;

typedef const NOTIFY_SYSTEM_STATE_OUTPUT *PCNOTIFY_SYSTEM_STATE_OUTPUT;

typedef enum {
    TAG_STATUS_UNINITALIZED,
    TAG_STATUS_INPUT_VERIFIED,
    TAG_STATUS_TAG_REQUEST_NOT_RECEIVED,
    TAG_STATUS_INSERTED,
    TAG_STATUS_INSERTION_FAILED,
    TAG_STATUS_DROPPED,
    TAG_STATUS_COMMITTED,
    TAG_STATUS_UNKNOWN
}TAG_DEVICE_COMMIT_STATUS;

typedef enum {
    DEVICE_STATUS_SUCCESS = 0,
    DEVICE_STATUS_NON_WRITE_ORDER_STATE,
    DEVICE_STATUS_FILTERING_STOPPED,
    DEVICE_STATUS_REMOVED,
    DEVICE_STATUS_DISKID_CONFLICT,
    DEVICE_STATUS_NOT_FOUND,
    DEVICE_STATUS_DRAIN_BLOCK_FAILED,
    DEVICE_STATUS_DRAIN_ALREADY_BLOCKED,
    DEVICE_STATUS_UNKNOWN
}DEVICE_STATUS;

typedef struct _TAG_COMMIT_STATUS {
    WCHAR                       DeviceId[GUID_SIZE_IN_CHARS + 1]; // extra for null terminating
    DEVICE_STATUS               Status;
    TAG_DEVICE_COMMIT_STATUS    TagStatus;
    ULONGLONG                   TagInsertionTime;
    ULONGLONG                   TagCommitTime;
    ULONGLONG                   TagSequenceNumber;
    DEVICE_CXFAILURE_STATS      DeviceCxStats;
} TAG_COMMIT_STATUS, *PTAG_COMMIT_STATUS;

typedef struct _TAG_COMMIT_NOTIFY_OUTPUT {
    UCHAR                   TagGUID[GUID_SIZE_IN_CHARS + 1];
    ULONG                   ulFlags;
    VM_CXFAILURE_STATS      vmCxStatus;
    ULONG                   ulNumDisks;
    TAG_COMMIT_STATUS       TagStatus[1];
}TAG_COMMIT_NOTIFY_OUTPUT, *PTAG_COMMIT_NOTIFY_OUTPUT;

typedef struct _TAG_COMMIT_NOTIFY_INPUT {
    UCHAR       TagGUID[GUID_SIZE_IN_CHARS + 1]; // Extra for null character
    ULONG       ulFlags;
    ULONG       ulNumDisks;
    WCHAR       DeviceId[1][GUID_SIZE_IN_CHARS + 1];
}TAG_COMMIT_NOTIFY_INPUT, *PTAG_COMMIT_NOTIFY_INPUT;

#define DEFAULT_TAG_COMMIT_NOTIFY_TIMEOUT_SECS          (900) // By default it is 15 mins

#define TAG_COMMIT_NOTIFY_BLOCK_DRAIN_FLAG      0x00000001

#define DISK_STATE_FILTERED          0x00000001     // Disk is filtered or not
#define DISK_STATE_DRAIN_BLOCKED     0x00000002     // Draining for the disk is blocked or unblocked

typedef struct _DISK_STATE {
    WCHAR   DeviceId[GUID_SIZE_IN_CHARS + 1];
    ULONG   Status; // win32 status
    ULONG   ulFlags;    // Use this flag only if request is success
}DISK_STATE;

#define DRAIN_STATE_RESET_FLAG              0x00000000
#define DRAIN_STATE_SET_FLAG                0x00000001
#define DRAIN_STATE_LOCAL_FLAG              0x00000010

/*
*   0x00000000          Global Reset
*   0x00000001          Global Set
*   0x00000010          Local Reset
*   0x00000011          Local Set
*/

typedef struct _SET_DRAIN_STATE_INPUT {
    ULONG       ulFlags;
    ULONG       ulNumDisks;     // if number of disks = 0, global
    WCHAR       DeviceId[1][GUID_SIZE_IN_CHARS + 1];
} SET_DRAIN_STATE_INPUT, *PSET_DRAIN_STATE_INPUT;

typedef enum {
    SET_DRAIN_STATUS_SUCCESS = 0,
    SET_DRAIN_STATUS_DEVICE_NOT_FOUND,
    SET_DRAIN_STATUS_REGISTRY_CREATE_FAILED,
    SET_DRAIN_STATUS_REGISTRY_OPEN_FAILED,
    SET_DRAIN_STATUS_REGISTRY_WRITE_FAILED,
    SET_DRAIN_STATUS_UKNOWN
}ERROR_SET_DRAIN_STATUS;

typedef struct _SET_DRAIN_STATUS {
    WCHAR                   DeviceId[GUID_SIZE_IN_CHARS + 1];
    ULONG                   ulFlags;   // Flags point to current device flags. Use this flag if Status is success
    ERROR_SET_DRAIN_STATUS  Status;     // Set drain status
    ULONG                   ulInternalError; // win32 error code if request failed
} SET_DRAIN_STATUS, *PSET_DRAIN_STATUS;

typedef struct _SET_DRAIN_STATE_OUTPUT {
    ULONG               ulFlags;       // Reserved
    ULONG               ulNumDisks;     // if number of disks = 0, global
    SET_DRAIN_STATUS    diskStatus[1];
} SET_DRAIN_STATE_OUTPUT, *PSET_DRAIN_STATE_OUTPUT;

typedef struct _GET_DISK_STATE_INPUT {
    ULONG   ulNumDisks; // 0 means get drain state for all disks
    WCHAR   DeviceId[1][GUID_SIZE_IN_CHARS+1];
}GET_DISK_STATE_INPUT, *PGET_DISK_STATE_INPUT;

typedef     struct _GET_DISK_STATE_OUTPUT {
    ULONG           ulSupportedFlags;    // Flag is used to inform which flags are supported
    ULONG           ulNumDisks;
    DISK_STATE      diskState[1];
} GET_DISK_STATE_OUTPUT, *PGET_DISK_STATE_OUTPUT;

// ulSupportedFlags definition
#define DISK_SUPPORT_FLAGS_FILTERED        0x1
#define DISK_SUPPORT_FLAGS_DRAIN_BLOCKED   0x2

/*
// Use following query to figure if disk state is set
    if (TEST_FLAG(diskStateOutput->ulSupportedFlags, DISK_SUPPORT_FLAGS_FILTERED)) {
        if (TEST_FLAG(diskStateOutput->diskState[1]->ulFlags, DISK_STATE_FILTERED) {
            // Disk is filtered
        }
    }
*/

#endif // __INMFLTINTERFACE__H__
