
#ifndef INVOLFLT_H
#define INVOLFLT_H


/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : involflt.h
 *
 * Description: Header shared between linux filter driver and S2.
 */

#define INMAGE_FILTER_DEVICE_NAME    "/dev/involflt"
#define GUID_SIZE_IN_CHARS 128

typedef enum _etBitOperation {
    ecBitOpNotDefined = 0,
    ecBitOpSet = 1,
    ecBitOpReset = 2,
} etBitOperation;

#define PROCESS_START_NOTIFY_INPUT_FLAGS_DATA_FILES_AWARE   0x0001
#define PROCESS_START_NOTIFY_INPUT_FLAGS_64BIT_PROCESS      0x0002

#define SHUTDOWN_NOTIFY_FLAGS_ENABLE_DATA_FILTERING 0x00000001
#define SHUTDOWN_NOTIFY_FLAGS_ENABLE_DATA_FILES     0x00000002

typedef struct
{
    char volume_guid[GUID_SIZE_IN_CHARS];
} VOLUME_GUID;

typedef struct _SHUTDOWN_NOTIFY_INPUT
{
    unsigned int ulFlags;
} SHUTDOWN_NOTIFY_INPUT, *PSHUTDOWN_NOTIFY_INPUT;

typedef SHUTDOWN_NOTIFY_INPUT SYS_SHUTDOWN_NOTIFY_INPUT;

typedef struct _PROCESS_START_NOTIFY_INPUT
{
    unsigned int  ulFlags;
} PROCESS_START_NOTIFY_INPUT, *PPROCESS_START_NOTIFY_INPUT;

typedef struct _DISK_CHANGE
{
    unsigned long long   ByteOffset;
    unsigned int         Length;
    unsigned short       usBufferIndex;
    unsigned short       usNumberOfBuffers;
} DISK_CHANGE, DiskChange, *PDISK_CHANGE;

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

#define STREAM_REC_TYPE_START_OF_TAG_LIST   0x0001
#define STREAM_REC_TYPE_END_OF_TAG_LIST     0x0002
#define STREAM_REC_TYPE_TIME_STAMP_TAG      0x0003
#define STREAM_REC_TYPE_DATA_SOURCE         0x0004
#define STREAM_REC_TYPE_USER_DEFINED_TAG    0x0005
#define STREAM_REC_TYPE_PADDING             0x0006

typedef struct _STREAM_REC_HDR_4B
{
    unsigned short       usStreamRecType;
    unsigned char        ucFlags;
    unsigned char        ucLength; // Length includes size of this header too.
} STREAM_REC_HDR_4B, *PSTREAM_REC_HDR_4B;

typedef struct _STREAM_REC_HDR_8B
{
    unsigned short       usStreamRecType;
    unsigned char        ucFlags;    // STREAM_REC_FLAGS_LENGTH_BIT bit is set for this record.
    unsigned char        ucReserved;
    unsigned int         ulLength; // Length includes size of this header too.
} STREAM_REC_HDR_8B, *PSTREAM_REC_HDR_8B;

#define FILL_STREAM_HEADER_4B(pHeader, Type, Len)           \
{                                                           \
    ((PSTREAM_REC_HDR_4B)pHeader)->usStreamRecType = Type;  \
    ((PSTREAM_REC_HDR_4B)pHeader)->ucFlags = 0;             \
    ((PSTREAM_REC_HDR_4B)pHeader)->ucLength = Len;          \
}

#define FILL_STREAM_HEADER_8B(pHeader, Type, Len)           \
{                                                           \
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

#define FILL_STREAM_HEADER(pHeader, Type, Len)                  \
{                                                               \
    if((unsigned int )Len > (unsigned int )0xFF) {                              \
        ((PSTREAM_REC_HDR_8B)pHeader)->usStreamRecType = Type;  \
        ((PSTREAM_REC_HDR_8B)pHeader)->ucFlags = STREAM_REC_FLAGS_LENGTH_BIT;             \
        ((PSTREAM_REC_HDR_8B)pHeader)->ucReserved = 0;          \
        ((PSTREAM_REC_HDR_8B)pHeader)->ulLength = Len;          \
    } else {                                                    \
        ((PSTREAM_REC_HDR_4B)pHeader)->usStreamRecType = Type;  \
        ((PSTREAM_REC_HDR_4B)pHeader)->ucFlags = 0;             \
        ((PSTREAM_REC_HDR_4B)pHeader)->ucLength = (unsigned char )Len;   \
    }                                                           \
}

#define FILL_STREAM(pHeader, Type, Len, pData)                  \
{                                                               \
    if((unsigned int )Len > (unsigned int )0xFF) {                              \
        ((PSTREAM_REC_HDR_8B)pHeader)->usStreamRecType = Type;  \
        ((PSTREAM_REC_HDR_8B)pHeader)->ucFlags = STREAM_REC_FLAGS_LENGTH_BIT;             \
        ((PSTREAM_REC_HDR_8B)pHeader)->ucReserved = 0;          \
        ((PSTREAM_REC_HDR_8B)pHeader)->ulLength = Len;          \
        RtlCopyMemory(((Punsigned char )pHeader) + sizeof(PSTREAM_REC_HDR_8B), pData, Len);  \
    } else {                                                    \
        ((PSTREAM_REC_HDR_4B)pHeader)->usStreamRecType = Type;  \
        ((PSTREAM_REC_HDR_4B)pHeader)->ucFlags = 0;             \
        ((PSTREAM_REC_HDR_4B)pHeader)->ucLength = (unsigned char )Len;   \
        RtlCopyMemory(((Punsigned char )pHeader) + sizeof(PSTREAM_REC_HDR_4B), pData, Len);  \
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
#define STREAM_REC_DATA(pHeader)    ((Punsigned char )pHeader + STREAM_REC_HEADER_SIZE(pHeader))

typedef struct _TIME_STAMP_TAG
{
    STREAM_REC_HDR_4B    Header;
    unsigned int         ulSequenceNumber;
    unsigned long long   TimeInHundNanoSecondsFromJan1601;
} TIME_STAMP_TAG, *PTIME_STAMP_TAG;

#define INVOLFLT_DATA_SOURCE_UNDEFINED  0x00
#define INVOLFLT_DATA_SOURCE_BITMAP     0x01
#define INVOLFLT_DATA_SOURCE_META_DATA  0x02
#define INVOLFLT_DATA_SOURCE_DATA       0x03

typedef struct _DATA_SOURCE_TAG
{
    STREAM_REC_HDR_4B   Header;
    unsigned int        ulDataSource;
} DATA_SOURCE_TAG, *PDATA_SOURCE_TAG;

#define MAX_DIRTY_CHANGES 256

#define UDIRTY_BLOCK_FLAG_START_OF_SPLIT_CHANGE     0x00000001
#define UDIRTY_BLOCK_FLAG_PART_OF_SPLIT_CHANGE      0x00000002
#define UDIRTY_BLOCK_FLAG_END_OF_SPLIT_CHANGE       0x00000004
#define UDIRTY_BLOCK_FLAG_DATA_FILE                 0x00000008

#define UDIRTY_BLOCK_FLAG_VOLUME_RESYNC_REQUIRED    0x80000000

typedef struct _UDIRTY_BLOCK
{
#define UDIRTY_BLOCK_HEADER_SIZE    0x200   // 512 Bytes
#define UDIRTY_BLOCK_MAX_ERROR_STRING_SIZE  0x80    // 128 Bytes
#define UDIRTY_BLOCK_TAGS_SIZE      0xE00   // 7 * 512 Bytes
// UDIRTY_BLOCK_MAX_FILE_NAME is UDIRTY_BLOCK_TAGS_SIZE / sizeof(unsigned char ) - 1(for length field)
#define UDIRTY_BLOCK_MAX_FILE_NAME  0x6FF   // (0xE00 /2) - 1
    // uHeader is .5 KB and uTags is 3.5 KB, uHeader + uTags = 4KB
    union {
        struct {
        unsigned long long      uliTransactionID;
        unsigned long long      ulicbChanges;
        unsigned int            cChanges;
        unsigned int            ulTotalChangesPending;
        unsigned long long      liOutOfSyncTimeStamp;
        unsigned long long      ulicbTotalChangesPending;
        unsigned long           ulOutOfSyncErrorCode;
        unsigned int            ulFlags;
        unsigned int            ulSequenceIDforSplitIO;
        unsigned char           ErrorStringForResync[UDIRTY_BLOCK_MAX_ERROR_STRING_SIZE]; 
        unsigned int            ulBufferSize;
        unsigned short          usMaxNumberOfBuffers;
        unsigned short          usNumberOfBuffers;
        void                    **ppBufferArray; //This is actually a pointer to memory and not an array of pointers. It contains Changes in linear memorylocation.
        unsigned int            ulcbBufferArraySize;
        } Hdr;
        unsigned char  BufferReservedForHeader[UDIRTY_BLOCK_HEADER_SIZE];
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
            unsigned short   usLength; // Filename length in bytes not including NULL
            unsigned char    FileName[UDIRTY_BLOCK_MAX_FILE_NAME];
        } DataFile;
        unsigned char  BufferForTags[UDIRTY_BLOCK_TAGS_SIZE];
    } uTagList;

    DISK_CHANGE     Changes[ MAX_DIRTY_CHANGES ];
} UDIRTY_BLOCK, *PUDIRTY_BLOCK; //, DIRTY_BLOCKS, *PDIRTY_BLOCKS;

typedef struct _COMMIT_TRANSACTION
{
    unsigned char        VolumeGUID[GUID_SIZE_IN_CHARS];
    unsigned long long   ulTransactionID;
    unsigned int     ulFlags;
} COMMIT_TRANSACTION, *PCOMMIT_TRANSACTION;

typedef struct _VOLUME_FLAGS_INPUT
{
    unsigned char    VolumeGUID[GUID_SIZE_IN_CHARS];
    // if eOperation is BitOpSet the bits in ulVolumeFlags will be set
    // if eOperation is BitOpReset the bits in ulVolumeFlags will be unset
    etBitOperation  eOperation;
    unsigned int    ulVolumeFlags;
} VOLUME_FLAGS_INPUT, *PVOLUME_FLAGS_INPUT;

typedef struct _VOLUME_FLAGS_OUTPUT
{
    unsigned int    ulVolumeFlags;
} VOLUME_FLAGS_OUTPUT, *PVOLUME_FLAGS_OUTPUT;

#define DRIVER_FLAG_DISABLE_DATA_FILES                  0x00000001
#define DRIVER_FLAG_DISABLE_DATA_FILES_FOR_NEW_VOLUMES  0x00000002
#define DRIVER_FLAGS_VALID                              0x00000003

typedef struct _DRIVER_FLAGS_INPUT
{
    // if eOperation is BitOpSet the bits in ulFlags will be set
    // if eOperation is BitOpReset the bits in ulFlags will be unset
    etBitOperation  eOperation;
    unsigned int    ulFlags;
} DRIVER_FLAGS_INPUT, *PDRIVER_FLAGS_INPUT;

typedef struct _DRIVER_FLAGS_OUTPUT
{
    unsigned int    ulFlags;
} DRVER_FLAGS_OUTPUT, *PDRIVER_FLAGS_OUTPUT;

typedef struct 
{
    unsigned char    VolumeGUID[GUID_SIZE_IN_CHARS]; 
    int         Seconds; //Maximum time to wait in the kernel
} WAIT_FOR_DB_NOTIFY;

/* IOCTL codes for involflt driver in linux.
 */
#define FLT_IOCTL 0xfe

enum {
    STOP_FILTER_CMD = 0,
    START_FILTER_CMD,
    START_NOTIFY_CMD,
    SHUTDOWN_NOTIFY_CMD,
    GET_DB_CMD,
    COMMIT_DB_CMD,
    SET_VOL_FLAGS_CMD,
    GET_VOL_FLAGS_CMD,
    WAIT_FOR_DB_CMD,
    CLEAR_DIFFS_CMD,
    GET_TIME_CMD,
    UNSTACK_ALL_CMD,
    SYS_SHUTDOWN_NOTIFY_CMD,
    TAG_CMD,	
    WAKEUP_THREADS_CMD,
    GET_DB_THRESHOLD_CMD,	
    VOLUME_STACKING_CMD,
    RESYNC_START_CMD,
    RESYNC_END_CMD,
    GET_DRIVER_VER_CMD,
    GET_SHELL_LOG_CMD,
	AT_LUN_CREATE_CMD,
	AT_LUN_DELETE_CMD,
	AT_LUN_LAST_WRITE_VI_CMD,
	AT_LUN_QUERY_CMD,
	GET_GLOBAL_STATS_CMD,
	GET_VOLUME_STATS_CMD,
	GET_PROTECTED_VOLUME_LIST_CMD,
	GET_SET_ATTR_CMD,
    BOOTTIME_STACKING_CMD,
    VOLUME_UNSTACKING_CMD,
    START_FILTER_CMD_V2,
    SYNC_TAG_CMD,
    SYNC_TAG_STATUS_CMD,
    START_MIRROR_CMD,
    STOP_MIRROR_CMD,
    MIRROR_VOLUME_STACKING_CMD,
    MIRROR_EXCEPTION_NOTIFY_CMD,
    AT_LUN_LAST_HOST_IO_TIMESTAMP_CMD,
    GET_DMESG_CMD,
    BLOCK_AT_LUN_CMD,
    BLOCK_AT_LUN_ACCESS_CMD,
    MAX_XFER_SZ_CMD,
    GET_ADDITIONAL_VOLUME_STATS_CMD,
    GET_VOLUME_LATENCY_STATS_CMD,
    GET_VOLUME_BMAP_STATS_CMD,
    SET_INVOLFLT_VERBOSITY_CMD,
    MIRROR_TEST_HEARTBEAT_CMD,
    INIT_DRIVER_FULLY_CMD,
    VOLUME_FREEZE_CMD,
    TAG_CMD_V2,
    VOLUME_THAW_CMD,
    TAG_CMD_V3,
    CREATE_BARRIER,
    REMOVE_BARRIER,
    TAG_COMMIT_V2,
    SYS_PRE_SHUTDOWN_NOTIFY_CMD,
    GET_MONITORING_STATS_CMD,
    GET_BLK_MQ_STATUS_CMD,
    GET_VOLUME_STATS_V2_CMD,
};

#define IOCTL_INMAGE_PROCESS_START_NOTIFY     _IOW(FLT_IOCTL, START_NOTIFY_CMD, PROCESS_START_NOTIFY_INPUT) 
#define IOCTL_INMAGE_SERVICE_SHUTDOWN_NOTIFY     _IOW(FLT_IOCTL, SHUTDOWN_NOTIFY_CMD, SHUTDOWN_NOTIFY_INPUT)
#define IOCTL_INMAGE_STOP_FILTERING_DEVICE      _IOW(FLT_IOCTL, STOP_FILTER_CMD, VOLUME_GUID)
#define IOCTL_INMAGE_START_FILTERING_DEVICE       _IOW(FLT_IOCTL, START_FILTER_CMD, VOLUME_GUID)
/* A temp hack. Maximum structure size that could be passed through _IO i/f is 16K whereas
 * UDIRTY_BLOCK is 20K, hence we just pass VolumeGuid as structure only for compilation pruposes,
 * In the kernel, necessary checks are done prior to accessing the user space UDIRTY block.
 */ 
#define IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS    _IOWR(FLT_IOCTL, GET_DB_CMD, VOLUME_GUID)
#define IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS    _IOW(FLT_IOCTL, COMMIT_DB_CMD, COMMIT_TRANSACTION)
#define IOCTL_INMAGE_SET_VOLUME_FLAGS        _IOW(FLT_IOCTL, SET_VOL_FLAGS_CMD, VOLUME_FLAGS_INPUT)
#define IOCTL_INMAGE_GET_VOLUME_FLAGS        _IOR(FLT_IOCTL, GET_VOL_FLAGS_CMD, VOLUME_FLAGS_INPUT)
#define IOCTL_INMAGE_WAIT_FOR_DB        _IOW(FLT_IOCTL, WAIT_FOR_DB_CMD, WAIT_FOR_DB_NOTIFY)
#define IOCTL_INMAGE_CLEAR_DIFFERENTIALS    _IOW(FLT_IOCTL, CLEAR_DIFFS_CMD, VOLUME_GUID)
#define IOCTL_INMAGE_GET_NANOSECOND_TIME    _IOWR(FLT_IOCTL, GET_TIME_CMD, long long)
#define IOCTL_INMAGE_UNSTACK_ALL 	_IO(FLT_IOCTL, UNSTACK_ALL_CMD)
#define IOCTL_INMAGE_SYS_SHUTDOWN              _IOW(FLT_IOCTL, SYS_SHUTDOWN_NOTIFY_CMD, SYS_SHUTDOWN_NOTIFY_INPUT)
#define IOCTL_INMAGE_SYS_PRE_SHUTDOWN           _IOW(FLT_IOCTL, SYS_PRE_SHUTDOWN_NOTIFY_CMD, void *)

/* Error numbers logged to registry when volume is out of sync */
#define ERROR_TO_REG_BITMAP_READ_ERROR                      0x0002
#define ERROR_TO_REG_BITMAP_WRITE_ERROR                     0x0003
#define ERROR_TO_REG_BITMAP_OPEN_ERROR                      0x0004
#define ERROR_TO_REG_BITMAP_OPEN_FAIL_CHANGES_LOST          (0x0005)

#endif // ifndef INVOLFLT_H
