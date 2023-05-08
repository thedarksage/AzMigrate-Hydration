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

#ifndef INVOLFLT_H
#define INVOLFLT_H

#define PERSISTENT_DIR "/etc/vxagent/involflt"
#define INMAGE_FILTER_DEVICE_NAME    "/dev/involflt"
#define GUID_SIZE_IN_CHARS 128
#define MAX_INITIATOR_NAME_LEN 24
#define INM_GUID_LEN_MAX 256
#define INM_MAX_VOLUMES_IN_LIST     0xFF
#define INM_MAX_SCSI_ID_SIZE     256
#define GUID_LEN                36

// Version information stated with Driver Version 2.0.0.0
// This version has data stream changes
#define DRIVER_MAJOR_VERSION    0x02
#define DRIVER_MINOR_VERSION    0x03
#define DRIVER_MINOR_VERSION2   0x0f
#define DRIVER_MINOR_VERSION3   0x2f

#define TAG_VOLUME_MAX_LENGTH   256

#define TAG_VOLUME_INPUT_FLAGS_ATOMIC_TO_VOLUME_GROUP 0x0001
#define TAG_FS_CONSISTENCY_REQUIRED                   0x0002
#define TAG_ALL_PROTECTED_VOLUME_IOBARRIER            0x0004

typedef uint64_t inm_u64_t;
typedef int64_t inm_s64_t;

typedef enum
{
    FLT_MODE_UNINITIALIZED = 0,
    FLT_MODE_METADATA,
    FLT_MODE_DATA
} flt_mode;

typedef enum _etWriterOrderState
{
    ecWriteOrderStateUnInitialized = 0,
    ecWriteOrderStateBitmap = 1,
    ecWriteOrderStateMetadata = 2,
    ecWriteOrderStateData = 3,
    ecWriteOrderStateRawBitmap = 4
} etWriteOrderState, *petWriteOrderState;
 
#define INM_DRIVER_VERSION(a, b, c, d) (((a) << 24) + ((b) << 16) +  ((c) << 8)  + (d))
#define INM_DRIVER_VERSION_LATEST      INM_DRIVER_VERSION(2, 1, 2, 0)

typedef enum inm_device {
    FILTER_DEV_FABRIC_LUN = 1,
    FILTER_DEV_HOST_VOLUME = 2,
    FILTER_DEV_FABRIC_VSNAP = 3,
    FILTER_DEV_FABRIC_RESYNC = 4,
    FILTER_DEV_MIRROR_SETUP = 5,
} inm_dev_t;

#define INM_PATH_MAX (4096)
#define MIRROR_SETUP_PENDING_RESYNC_CLEARED_FLAG  0x0000000000000001
#define FULL_DISK_FLAG                            0x0000000000000002
#define MIRROR_VOLUME_STACKING_FLAG               0x00008000

#define FULL_DISK_FLAG                            0x0000000000000002
#define FULL_DISK_PARTITION_FLAG                  0x0000000000000004
#define FULL_DISK_LABEL_VTOC                     0x0000000000000008
#define FULL_DISK_LABEL_EFI                      0x0000000000000010
#define INM_IS_DEVICE_MULTIPATH                  0x0000000000000100

/*
 * This structure is defined for backward compatibility with older drivers
 * who accept the older structure without d_pname. This structure is only
 * used for defining the ioctl command values.
 */
typedef struct inm_dev_info_compat {
    inm_dev_t d_type;
    char d_guid[INM_GUID_LEN_MAX];
    char d_mnt_pt[INM_PATH_MAX];
    uint64_t d_nblks;
    uint32_t d_bsize;
    uint64_t d_flags;
} inm_dev_info_compat_t;

typedef struct inm_dev_info {
    inm_dev_t d_type;
    char d_guid[INM_GUID_LEN_MAX];
    char d_mnt_pt[INM_PATH_MAX];
    uint64_t d_nblks;
    uint32_t d_bsize;
    uint64_t d_flags;
    char d_pname[INM_GUID_LEN_MAX];
} inm_dev_info_t;

typedef enum vendor {
    FILTER_DEV_UNKNOWN_VENDOR = 1,
    FILTER_DEV_NATIVE = 2,
    FILTER_DEV_DEVMAPPER = 3,
    FILTER_DEV_MPXIO = 4,
    FILTER_DEV_EMC = 5,
    FILTER_DEV_HDLM = 6,
    FILTER_DEV_DEVDID = 7,
    FILTER_DEV_DEVGLOBAL = 8,
    FILTER_DEV_VXDMP = 9,
    FILTER_DEV_SVM = 10,
    FILTER_DEV_VXVM = 11,
    FILTER_DEV_LVM = 12,
    FILTER_DEV_INMVOLPACK = 13,
    FILTER_DEV_INMVSNAP = 14,
    FILTER_DEV_CUSTOMVENDOR = 15,
    FILTER_DEV_ASM = 16,
}inm_vendor_t;

typedef enum eMirrorConfErrors
{
    MIRROR_NO_ERROR = 0,
    SRC_LUN_INVALID,
    ATLUN_INVALID,
    DRV_MEM_ALLOC_ERR,
    DRV_MEM_COPYIN_ERR,
    DRV_MEM_COPYOUT_ERR,
    SRC_NAME_CHANGED_ERR,
    ATLUN_NAME_CHANGED_ERR,
    MIRROR_STACKING_ERR,
    RESYNC_CLEAR_ERROR,
    RESYNC_NOT_SET_ON_CLEAR_ERR,
    SRC_DEV_LIST_MISMATCH_ERR,
    ATLUN_DEV_LIST_MISMATCH_ERR,
    SRC_DEV_SCSI_ID_ERR,
    DST_DEV_SCSI_ID_ERR,
    MIRROR_NOT_SETUP
} eMirrorConfErrors_t;

typedef struct _mirror_conf_info
{
    uint64_t d_flags;
    uint64_t d_nblks;
    uint64_t d_bsize;
    uint64_t startoff;

#ifdef INM_AIX
#ifdef __64BIT__
    /* [volume name 1]..[volume name n] */
    char *src_guid_list;
#else
    int  padding;
    /* [volume name 1]..[volume name n] */
    char *src_guid_list;
#endif

#ifdef __64BIT__
    /* [volume name 1]..[volume name n] */
    char *dst_guid_list;
#else
    int  padding_2;
    /* [volume name 1]..[volume name n] */
    char *dst_guid_list;
#endif
#else
    /* [volume name 1]..[volume name n] */
    char *src_guid_list;

    /* [volume name 1]..[volume name n] */
    char *dst_guid_list;
#endif

    eMirrorConfErrors_t d_status;
    inm_u32_t nsources;
    inm_u32_t ndestinations;
    inm_dev_t d_type;
    inm_vendor_t d_vendor;

    char src_scsi_id[INM_MAX_SCSI_ID_SIZE];
    char dst_scsi_id[INM_MAX_SCSI_ID_SIZE];

    /* AT LUN id */
    char at_name[INM_GUID_LEN_MAX];

} mirror_conf_info_t;


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

typedef struct
{
    char scsi_id[INM_MAX_SCSI_ID_SIZE];
} SCSI_ID;

typedef struct _SHUTDOWN_NOTIFY_INPUT
{
    unsigned int ulFlags;
} SHUTDOWN_NOTIFY_INPUT, *PSHUTDOWN_NOTIFY_INPUT;

typedef SHUTDOWN_NOTIFY_INPUT SYS_SHUTDOWN_NOTIFY_INPUT;

typedef struct _PROCESS_START_NOTIFY_INPUT
{
    unsigned int  ulFlags;
} PROCESS_START_NOTIFY_INPUT, *PPROCESS_START_NOTIFY_INPUT;

typedef struct _PROCESS_VOLUME_STACKING_INPUT
{
    unsigned int  ulFlags;
} PROCESS_VOLUME_STACKING_INPUT, *PPROCESS_VOLUME_STACKING_INPUT;

typedef struct _DISK_CHANGE
{
    unsigned long long   ByteOffset;
    unsigned int         Length;
    unsigned short       usBufferIndex;
    unsigned short       usNumberOfBuffers;
} DISK_CHANGE, DiskChange, *PDISK_CHANGE;

typedef struct _LUN_CREATE_INPUT
{
    char uuid[GUID_SIZE_IN_CHARS+1];
    unsigned long long lunSize;
    unsigned long long lunStartOff;
    unsigned int blockSize;
    inm_dev_t lunType;
} LUN_CREATE_INPUT, LunCreateData, *PLUN_CREATE_DATA;

typedef struct _LUN_DELETE_DATA
{
    char uuid[GUID_SIZE_IN_CHARS+1];
    inm_dev_t lunType;
} LUN_DELETE_INPUT, LunDeleteData, *PLUN_DELETE_DATA;

typedef struct _AT_LUN_LAST_WRITE_VI
{
    char uuid[GUID_SIZE_IN_CHARS+1];
    char initiator_name[MAX_INITIATOR_NAME_LEN];
    unsigned long long timestamp; /* Return timestamp at which ioctl was issued */
} AT_LUN_LAST_WRITE_VI,ATLunLastWriteVI , *PATLUN_LAST_WRITE_VI;

typedef struct _LUN_DATA
{
    char uuid[GUID_SIZE_IN_CHARS+1];
    inm_dev_t lun_type;
} LUN_DATA, LunData, *PLUNDATA;

typedef struct _LUN_QUERY_DATA
{
    unsigned int count;
    unsigned int lun_count;
    LunData lun_data[1];
} LUN_QUERY_DATA, LunQueryData, *PLUN_QUERY_DATA;

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

#ifndef INVOFLT_STREAM_FUNCTIONS
#define INVOFLT_STREAM_FUNCTIONS
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
#endif

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

typedef struct _TIME_STAMP_TAG_V2
{
    STREAM_REC_HDR_4B    Header;
    STREAM_REC_HDR_4B	 Reserved;
    unsigned long long   ullSequenceNumber;
    unsigned long long   TimeInHundNanoSecondsFromJan1601;
} TIME_STAMP_TAG_V2, *PTIME_STAMP_TAG_V2;

#define INVOLFLT_DATA_SOURCE_UNDEFINED  0x00
#define INVOLFLT_DATA_SOURCE_BITMAP     0x01
#define INVOLFLT_DATA_SOURCE_META_DATA  0x02
#define INVOLFLT_DATA_SOURCE_DATA       0x03

typedef struct _DATA_SOURCE_TAG
{
    STREAM_REC_HDR_4B   Header;
    unsigned int        ulDataSource;
} DATA_SOURCE_TAG, *PDATA_SOURCE_TAG;

typedef struct _VOLUME_STATS{
	VOLUME_GUID	guid;
	char		*bufp;
	inm_u32_t	buf_len;
} VOLUME_STATS;


// Light weight stats about Tags
typedef struct _VOLUME_TAG_STATS
{
    inm_u64_t    TagsDropped;
} VOLUME_TAG_STATS;

// Light weight stats about write churn
typedef struct _VOLUME_CHURN_STATS
{
    inm_s64_t    NumCommitedChangesInBytes;
} VOLUME_CHURN_STATS;

// User passes below flag indicating required Light weight stats
typedef enum
{
    GET_TAG_STATS  = 1,
    GET_CHURN_STATS
} ReqStatsType;

typedef struct _MONITORING_STATS{
    VOLUME_GUID VolumeGuid;
    ReqStatsType ReqStat;

    union {
        VOLUME_TAG_STATS TagStats;
        VOLUME_CHURN_STATS ChurnStats;
    };
} MONITORING_STATS;

typedef struct _BLK_MQ_STATUS{
    VOLUME_GUID VolumeGuid;
    int blk_mq_enabled;
} BLK_MQ_STATUS;


typedef struct _GET_VOLUME_LIST {
#ifdef INM_AIX
#ifdef __64BIT__
    char        *bufp;
#else
    int		padding;
    char        *bufp;
#endif
#else
    char        *bufp;
#endif
    inm_u32_t   buf_len;
} GET_VOLUME_LIST;

typedef struct _inm_attribute{
	inm_u32_t	type;
	inm_u32_t	why;
	VOLUME_GUID 	guid;
	char		*bufp;
	inm_u32_t	buflen;
}inm_attribute_t;

#define MAX_DIRTY_CHANGES 256

#ifdef __sparcv9
#define MAX_DIRTY_CHANGES_V2 409
#else
#define MAX_DIRTY_CHANGES_V2 204
#endif

#define UDIRTY_BLOCK_FLAG_START_OF_SPLIT_CHANGE     0x00000001
#define UDIRTY_BLOCK_FLAG_PART_OF_SPLIT_CHANGE      0x00000002
#define UDIRTY_BLOCK_FLAG_END_OF_SPLIT_CHANGE       0x00000004
#define UDIRTY_BLOCK_FLAG_DATA_FILE                 0x00000008
#define UDIRTY_BLOCK_FLAG_SVD_STREAM                0x00000010			
#define UDIRTY_BLOCK_FLAG_VOLUME_RESYNC_REQUIRED    0x80000000
#define UDIRTY_BLOCK_FLAG_TSO_FILE                  0x00000020

#define COMMIT_TRANSACTION_FLAG_RESET_RESYNC_REQUIRED_FLAG  0x00000001

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
	    unsigned long long      ulicbTotalChangesPending;
	    unsigned int            ulFlags;
	    unsigned int            ulSequenceIDforSplitIO;
	    unsigned int            ulBufferSize;
	    unsigned short          usMaxNumberOfBuffers;
	    unsigned short          usNumberOfBuffers;
	    unsigned int		ulcbChangesInStream;

	    /* This is actually a pointer to memory and not an array of pointers. 
	     * It contains Changes in linear memorylocation.
	     */
	    void                    **ppBufferArray; 
	    unsigned int            ulcbBufferArraySize;

	    // resync flags
	    unsigned long 			ulOutOfSyncCount;
	    unsigned char           ErrorStringForResync[UDIRTY_BLOCK_MAX_ERROR_STRING_SIZE]; 
	    unsigned long           ulOutOfSyncErrorCode;
	    unsigned long long      liOutOfSyncTimeStamp;
	    uint64_t   ullPrevEndSequenceNumber;
            uint64_t   ullPrevEndTimeStamp;
            uint32_t   ulPrevSequenceIDforSplitIO;
	    etWriteOrderState   eWOState;

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

    //DISK_CHANGE     Changes[ MAX_DIRTY_CHANGES ];
    long long ChangeOffsetArray[MAX_DIRTY_CHANGES];	
    unsigned int ChangeLengthArray[MAX_DIRTY_CHANGES];	
} UDIRTY_BLOCK, *PUDIRTY_BLOCK; //, DIRTY_BLOCKS, *PDIRTY_BLOCKS;

typedef struct _UDIRTY_BLOCK_V2
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
	    unsigned long long      ulicbTotalChangesPending;
	    unsigned int            ulFlags;
	    unsigned int            ulSequenceIDforSplitIO;
	    unsigned int            ulBufferSize;
	    unsigned short          usMaxNumberOfBuffers;
	    unsigned short          usNumberOfBuffers;
	    unsigned int		ulcbChangesInStream;

	    /* This is actually a pointer to memory and not an array of pointers. 
	     * It contains Changes in linear memorylocation.
	     */
	    void                    **ppBufferArray; 
	    unsigned int            ulcbBufferArraySize;

	    // resync flags
	    unsigned long 			ulOutOfSyncCount;
	    unsigned char           ErrorStringForResync[UDIRTY_BLOCK_MAX_ERROR_STRING_SIZE]; 
	    unsigned long           ulOutOfSyncErrorCode;
	    unsigned long long      liOutOfSyncTimeStamp;
	    uint64_t   ullPrevEndSequenceNumber;
            uint64_t   ullPrevEndTimeStamp;
            uint32_t   ulPrevSequenceIDforSplitIO;
	    etWriteOrderState   eWOState;

        } Hdr;
        unsigned char  BufferReservedForHeader[UDIRTY_BLOCK_HEADER_SIZE];
    } uHdr;

    // Start of Markers
    union {
        struct {
            STREAM_REC_HDR_4B   TagStartOfList;
            STREAM_REC_HDR_4B   TagPadding;
            TIME_STAMP_TAG_V2      TagTimeStampOfFirstChange;
            TIME_STAMP_TAG_V2      TagTimeStampOfLastChange;
            DATA_SOURCE_TAG     TagDataSource;
            STREAM_REC_HDR_4B   TagEndOfList;
        } TagList;
        struct {
            unsigned short   usLength; // Filename length in bytes not including NULL
            unsigned char    FileName[UDIRTY_BLOCK_MAX_FILE_NAME];
        } DataFile;
        unsigned char  BufferForTags[UDIRTY_BLOCK_TAGS_SIZE];
    } uTagList;

    //DISK_CHANGE     Changes[ MAX_DIRTY_CHANGES ];
    unsigned long long ChangeOffsetArray[MAX_DIRTY_CHANGES_V2];	
    unsigned int ChangeLengthArray[MAX_DIRTY_CHANGES_V2];	
    unsigned int TimeDeltaArray[MAX_DIRTY_CHANGES_V2];
    unsigned int SequenceNumberDeltaArray[MAX_DIRTY_CHANGES_V2];
} UDIRTY_BLOCK_V2, *PUDIRTY_BLOCK_V2; //, DIRTY_BLOCKS, *PDIRTY_BLOCKS;

typedef struct _UDIRTY_BLOCK_V2 UDIRTY_BLOCK_V3;

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

typedef struct 
{
    unsigned char    VolumeGUID[GUID_SIZE_IN_CHARS]; 
    unsigned int threshold;
} get_db_thres_t;

typedef struct
{
    unsigned char VolumeGUID[GUID_SIZE_IN_CHARS];
    unsigned long long TimeInHundNanoSecondsFromJan1601;
    unsigned int ulSequenceNumber;
    unsigned int ulReserved;
} RESYNC_START;
typedef struct
{
    unsigned char VolumeGUID[GUID_SIZE_IN_CHARS];
    unsigned long long TimeInHundNanoSecondsFromJan1601;
    unsigned long long ullSequenceNumber;
} RESYNC_START_V2;

typedef struct
{
    unsigned char VolumeGUID[GUID_SIZE_IN_CHARS];
    unsigned long long TimeInHundNanoSecondsFromJan1601;
    unsigned int ulSequenceNumber;
    unsigned int ulReserved;
} RESYNC_END;
typedef struct
{
    unsigned char VolumeGUID[GUID_SIZE_IN_CHARS];
    unsigned long long TimeInHundNanoSecondsFromJan1601;
    unsigned long long ullSequenceNumber;
} RESYNC_END_V2;

typedef struct _DRIVER_VERSION
{
    unsigned short  ulDrMajorVersion;
    unsigned short  ulDrMinorVersion;
    unsigned short  ulDrMinorVersion2;
    unsigned short  ulDrMinorVersion3;
    unsigned short  ulPrMajorVersion;
    unsigned short  ulPrMinorVersion;
    unsigned short  ulPrMinorVersion2;
    unsigned short  ulPrBuildNumber;
} DRIVER_VERSION, *PDRIVER_VERSION;

#define TAG_VOLUME_INPUT_FLAGS_ATOMIC_TO_VOLUME_GROUP 0x0001
#define TAG_FS_CONSISTENCY_REQUIRED		      0x0002

//freeze, thaw, tag volume return status
#define STATUS_FREEZE_SUCCESS    0x0000
#define STATUS_FREEZE_FAILED     0x0001

#define STATUS_THAW_SUCCESS      0x0000
#define STATUS_THAW_FAILED       0x0001


/* Structure definition to freeze */
typedef struct volume_info
{
    int           flags; /* Fill it with 0s, will be used in future */
    int           status; /* Status of the volume */
    char          vol_name[TAG_VOLUME_MAX_LENGTH]; /* volume name */
} volume_info_t;

typedef struct freeze_info
{
    int                   nr_vols; /* No. of volumes */
    int                   timeout;    /* timeout in terms of seconds */
    volume_info_t         *vol_info;  /* array of volume_info_t object */
} freeze_info_t;

/* Structure definition to thaw */
typedef struct thaw_info
{
    int                   nr_vols;    /* No. of volumes */
    int                   timeout;
    volume_info_t         *vol_info;  /* array of volume_info_t object */
} thaw_info_t;

#define INM_VOL_NAME_MAP_GUID 0x1
#define INM_VOL_NAME_MAP_PNAME 0x2

typedef struct vol_pname{
    int  vnm_flags;
    char vnm_request[INM_GUID_LEN_MAX];
    char vnm_response[INM_GUID_LEN_MAX];
} vol_name_map_t;

enum LCW_OP {
    LCW_OP_NONE,
    LCW_OP_BMAP_MAP_FILE,
    LCW_OP_BMAP_SWITCH_RAWIO,
    LCW_OP_BMAP_CLOSE,
    LCW_OP_BMAP_OPEN,
    LCW_OP_MAP_FILE,
    LCW_OP_MAX
};

typedef struct lcw_op
{
    enum LCW_OP lo_op;
    VOLUME_GUID lo_name;
} lcw_op_t;

typedef struct _COMMIT_DB_FAILURE_STATS {
    VOLUME_GUID    DeviceID;
    inm_u64_t      ulTransactionID;
    inm_u64_t      ullFlags;
    inm_u64_t      ullErrorCode;
} COMMIT_DB_FAILURE_STATS;

#define COMMITDB_NETWORK_FAILURE     0x00000001

#define DEFAULT_NR_CHURN_BUCKETS 11
typedef struct _DEVICE_CXFAILURE_STATS {
    VOLUME_GUID    DeviceId;
    inm_u64_t      ullFlags;
    inm_u64_t      ChurnBucketsMBps[DEFAULT_NR_CHURN_BUCKETS]; /* Churn
                                                                  Buckets */

    inm_u64_t      ExcessChurnBucketsMBps[DEFAULT_NR_CHURN_BUCKETS]; /* Excess
                                                               Churn buckets */
    inm_u64_t      CxStartTS;
    inm_u64_t      ullMaxDiffChurnThroughputTS;
    inm_u64_t      firstNwFailureTS;
    inm_u64_t      lastNwFailureTS;
    inm_u64_t      firstPeakChurnTS;
    inm_u64_t      lastPeakChurnTS;
    inm_u64_t      CxEndTS;

    inm_u64_t      ullLastNWErrorCode;
    inm_u64_t      ullMaximumPeakChurnInBytes;
    inm_u64_t      ullDiffChurnThroughputInBytes;
    inm_u64_t      ullMaxDiffChurnThroughputInBytes;
    inm_u64_t      ullTotalNWErrors;
    inm_u64_t      ullNumOfConsecutiveTagFailures;
    inm_u64_t      ullTotalExcessChurnInBytes;
    inm_u64_t      ullMaxS2LatencyInMS;
} DEVICE_CXFAILURE_STATS, *PDEVICE_CXFAILURE_STATS;

/* Disk Level Flags */
#define DISK_CXSTATUS_NWFAILURE_FLAG           0x00000001
#define DISK_CXSTATUS_PEAKCHURN_FLAG           0x00000002
#define DISK_CXSTATUS_CHURNTHROUGHPUT_FLAG     0x00000004
#define DISK_CXSTATUS_EXCESS_CHURNBUCKET_FLAG  0x00000008
#define DISK_CXSTATUS_MAX_CHURNTHROUGHPUT_FLAG 0x00000010
#define DISK_CXSTATUS_DISK_NOT_FILTERED        0x00000020
#define DISK_CXSTATUS_DISK_REMOVED             0x00000040

typedef struct _GET_CXFAILURE_NOTIFY {
    inm_u64_t    ullFlags;
    inm_u64_t    ulTransactionID;
    inm_u64_t    ullMinConsecutiveTagFailures;
    inm_u64_t    ullMaxVMChurnSupportedMBps;
    inm_u64_t    ullMaxDiskChurnSupportedMBps;
    inm_u64_t    ullMaximumTimeJumpFwdAcceptableInMs;
    inm_u64_t    ullMaximumTimeJumpBwdAcceptableInMs;
    inm_u32_t    ulNumberOfOutputDisks;
    inm_u32_t    ulNumberOfProtectedDisks;
    VOLUME_GUID  DeviceIdList[1];
} GET_CXFAILURE_NOTIFY, *PGET_CXFAILURE_NOTIFY;

#define CXSTATUS_COMMIT_PREV_SESSION 0x00000001

typedef struct _VM_CXFAILURE_STATS {
    inm_u64_t    ullFlags;
    inm_u64_t    ulTransactionID;
    inm_u64_t    ChurnBucketsMBps[DEFAULT_NR_CHURN_BUCKETS];

    inm_u64_t    ExcessChurnBucketsMBps[DEFAULT_NR_CHURN_BUCKETS];

    inm_u64_t    CxStartTS;
    inm_u64_t    ullMaxChurnThroughputTS;
    inm_u64_t    firstPeakChurnTS;
    inm_u64_t    lastPeakChurnTS;
    inm_u64_t    CxEndTS;

    inm_u64_t    ullMaximumPeakChurnInBytes;
    inm_u64_t    ullDiffChurnThroughputInBytes;

    inm_u64_t    ullMaxDiffChurnThroughputInBytes;
    inm_u64_t    ullTotalExcessChurnInBytes;

    inm_u64_t    TimeJumpTS;
    inm_u64_t    ullTimeJumpInMS;

    inm_u64_t    ullNumOfConsecutiveTagFailures;

    inm_u64_t    ullMaxS2LatencyInMS;

    inm_u32_t    ullNumDisks;
    DEVICE_CXFAILURE_STATS DeviceCxStats[1];
} VM_CXFAILURE_STATS, *PVM_CXFAILURE_STATS;

/* VM Level Flags */
#define VM_CXSTATUS_PEAKCHURN_FLAG           0x00000001
#define VM_CXSTATUS_CHURNTHROUGHPUT_FLAG     0x00000002
#define VM_CXSTATUS_TIMEJUMP_FWD_FLAG        0x00000004
#define VM_CXSTATUS_TIMEJUMP_BCKWD_FLAG      0x00000008
#define VM_CXSTATUS_EXCESS_CHURNBUCKETS_FLAG 0x00000010
#define VM_CXSTATUS_MAX_CHURNTHROUGHPUT_FLAG 0x00000020

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
    VOLUME_GUID              DeviceId;
    DEVICE_STATUS            Status;
    TAG_DEVICE_COMMIT_STATUS TagStatus;
    inm_u64_t                TagInsertionTime;
    inm_u64_t                TagSequenceNumber;
    DEVICE_CXFAILURE_STATS   DeviceCxStats;
} TAG_COMMIT_STATUS, *PTAG_COMMIT_STATUS;

#define TAG_COMMIT_NOTIFY_BLOCK_DRAIN_FLAG      0x00000001

typedef struct _TAG_COMMIT_NOTIFY_INPUT {
    char               TagGUID[GUID_LEN];
    inm_u64_t          ulFlags;
    inm_u64_t          ulNumDisks;
    VOLUME_GUID        DeviceId[1];
} TAG_COMMIT_NOTIFY_INPUT, *PTAG_COMMIT_NOTIFY_INPUT;

typedef struct _TAG_COMMIT_NOTIFY_OUTPUT {
    char               TagGUID[GUID_LEN];
    inm_u64_t          ulFlags;
    VM_CXFAILURE_STATS vmCxStatus;
    inm_u64_t          ulNumDisks;
    TAG_COMMIT_STATUS  TagStatus[1];
} TAG_COMMIT_NOTIFY_OUTPUT, *PTAG_COMMIT_NOTIFY_OUTPUT;

typedef struct _SET_DRAIN_STATE_INPUT {
    inm_u64_t   ulFlags;
    inm_u64_t   ulNumDisks;
    VOLUME_GUID DeviceId[1];
} SET_DRAIN_STATE_INPUT, *PSET_DRAIN_STATE_INPUT;

typedef enum {
    SET_DRAIN_STATUS_SUCCESS = 0,
    SET_DRAIN_STATUS_DEVICE_NOT_FOUND,
    SET_DRAIN_STATUS_PERSISTENCE_FAILED,
    SET_DRAIN_STATUS_UNKNOWN
} ERROR_SET_DRAIN_STATUS;

typedef struct _SET_DRAIN_STATUS {
    VOLUME_GUID             DeviceId;
    inm_u64_t               ulFlags;
    ERROR_SET_DRAIN_STATUS  Status;
    inm_u64_t               ulInternalError;
} SET_DRAIN_STATUS, *PSET_DRAIN_STATUS;

typedef struct _SET_DRAIN_STATE_OUTPUT {
    inm_u64_t           ulFlags;
    inm_u64_t           ulNumDisks;
    SET_DRAIN_STATUS    diskStatus[1];
} SET_DRAIN_STATE_OUTPUT, *PSET_DRAIN_STATE_OUTPUT;

typedef struct _GET_DISK_STATE_INPUT {
    inm_u64_t   ulNumDisks;
    VOLUME_GUID DeviceId[1];
} GET_DISK_STATE_INPUT, *PGET_DISK_STATE_INPUT;

typedef struct _DISK_STATE {
    VOLUME_GUID DeviceId;
    inm_u64_t   Status;
    inm_u64_t   ulFlags;
} DISK_STATE;

#define DISK_STATE_FILTERED          0x00000001
#define DISK_STATE_DRAIN_BLOCKED     0x00000002

typedef struct _GET_DISK_STATE_OUTPUT {
    inm_u64_t   ulSupportedFlags;
    inm_u64_t   ulNumDisks;
    DISK_STATE  diskState[1];
} GET_DISK_STATE_OUTPUT, *PGET_DISK_STATE_OUTPUT;

typedef struct _MODIFY_PERSISTENT_DEVICE_NAME_INPUT {
    VOLUME_GUID DevName;
    VOLUME_GUID OldPName;
    VOLUME_GUID NewPName;
} MODIFY_PERSISTENT_DEVICE_NAME_INPUT, *PMODIFY_PERSISTENT_DEVICE_NAME_INPUT;

#define TAG_MAX_LENGTH 256

typedef struct tag_names
{
    unsigned short  tag_len;/*tag length header plus name*/
    char            tag_name[TAG_MAX_LENGTH]; /* volume name */
} tag_names_t;

/* Structure definition to tag */
typedef struct tag_info
{
    int                   flags;/* Fill it with 0s, will be used in future */
    int                   nr_vols;
    int                   nr_tags;
    int                   timeout;/*time to not drain the dirty block has tag*/
    char                  tag_guid[GUID_LEN];/* one guid fro set of volumes*/
    volume_info_t         *vol_info;  /* Array of volume_info_t object */
    tag_names_t           *tag_names; /* Arrays of tag names */
                                      /* each of length TAG_MAX_LENGTH */
} tag_info_t_v2;

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
    REPLICATION_STATE,
    NAME_MAPPING,
    COMMITDB_FAIL_TRANS,
    GET_CXSTATS_NOTIFY,
    WAKEUP_GET_CXSTATS_NOTIFY_THREAD,
    LCW,
    WAIT_FOR_DB_CMD_V2,
    TAG_DRAIN_NOTIFY,
    WAKEUP_TAG_DRAIN_NOTIFY_THREAD,
    MODIFY_PERSISTENT_DEVICE_NAME,
    GET_DRAIN_STATE_CMD,
    SET_DRAIN_STATE_CMD
};


#ifdef INM_LINUX
#define IOCTL_INMAGE_VOLUME_STACKING            _IOW(FLT_IOCTL, VOLUME_STACKING_CMD, PROCESS_VOLUME_STACKING_INPUT)
#define IOCTL_INMAGE_PROCESS_START_NOTIFY       _IOW(FLT_IOCTL, START_NOTIFY_CMD, PROCESS_START_NOTIFY_INPUT)
#define IOCTL_INMAGE_SERVICE_SHUTDOWN_NOTIFY     _IOW(FLT_IOCTL, SHUTDOWN_NOTIFY_CMD, SHUTDOWN_NOTIFY_INPUT)
#define IOCTL_INMAGE_STOP_FILTERING_DEVICE      _IOW(FLT_IOCTL, STOP_FILTER_CMD, VOLUME_GUID)
#define IOCTL_INMAGE_START_FILTERING_DEVICE       _IOW(FLT_IOCTL, START_FILTER_CMD, VOLUME_GUID)
#define IOCTL_INMAGE_START_FILTERING_DEVICE_V2       _IOW(FLT_IOCTL, START_FILTER_CMD_V2, inm_dev_info_compat_t)
#define IOCTL_INMAGE_FREEZE_VOLUME              _IOW(FLT_IOCTL, VOLUME_FREEZE_CMD, freeze_info_t)
#define IOCTL_INMAGE_THAW_VOLUME                _IOW(FLT_IOCTL, VOLUME_THAW_CMD, thaw_info_t)

/* Mirror setup related ioctls */
#define IOCTL_INMAGE_START_MIRRORING_DEVICE     _IOW(FLT_IOCTL, START_MIRROR_CMD, mirror_conf_info_t)
#define IOCTL_INMAGE_STOP_MIRRORING_DEVICE      _IOW(FLT_IOCTL, STOP_MIRROR_CMD, SCSI_ID)
#define IOCTL_INMAGE_MIRROR_VOLUME_STACKING     _IOW(FLT_IOCTL, MIRROR_VOLUME_STACKING_CMD, mirror_conf_info_t)
#define IOCTL_INMAGE_MIRROR_EXCEPTION_NOTIFY    _IOW(FLT_IOCTL, MIRROR_EXCEPTION_NOTIFY_CMD, INM_MAX_SCSI_ID_SIZE)
#define IOCTL_INMAGE_MIRROR_TEST_HEARTBEAT      _IOW(FLT_IOCTL, MIRROR_TEST_HEARTBEAT_CMD, SCSI_ID)
#define IOCTL_INMAGE_BLOCK_AT_LUN		_IOW(FLT_IOCTL, BLOCK_AT_LUN_CMD, inm_at_lun_reconfig_t)

/* A temp hack. Maximum structure size that could be passed through _IO i/f is 16K whereas
 * UDIRTY_BLOCK is 20K, hence we just pass VolumeGuid as structure only for compilation pruposes,
 * In the kernel, necessary checks are done prior to accessing the user space UDIRTY block.
 */
#define IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS     _IOWR(FLT_IOCTL, GET_DB_CMD, VOLUME_GUID)
#define IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS  _IOW(FLT_IOCTL, COMMIT_DB_CMD, COMMIT_TRANSACTION)
#define IOCTL_INMAGE_SET_VOLUME_FLAGS           _IOW(FLT_IOCTL, SET_VOL_FLAGS_CMD, VOLUME_FLAGS_INPUT)
#define IOCTL_INMAGE_GET_VOLUME_FLAGS           _IOR(FLT_IOCTL, GET_VOL_FLAGS_CMD, VOLUME_FLAGS_INPUT)
#define IOCTL_INMAGE_WAIT_FOR_DB                _IOW(FLT_IOCTL, WAIT_FOR_DB_CMD, WAIT_FOR_DB_NOTIFY)
#define IOCTL_INMAGE_CLEAR_DIFFERENTIALS        _IOW(FLT_IOCTL, CLEAR_DIFFS_CMD, VOLUME_GUID)
#define IOCTL_INMAGE_GET_NANOSECOND_TIME        _IOWR(FLT_IOCTL, GET_TIME_CMD, long long)
#define IOCTL_INMAGE_UNSTACK_ALL                _IO(FLT_IOCTL, UNSTACK_ALL_CMD)
#define IOCTL_INMAGE_SYS_SHUTDOWN               _IOW(FLT_IOCTL, SYS_SHUTDOWN_NOTIFY_CMD, SYS_SHUTDOWN_NOTIFY_INPUT)
#define IOCTL_INMAGE_TAG_VOLUME                 _IOWR(FLT_IOCTL, TAG_CMD, unsigned long)
#define IOCTL_INMAGE_SYNC_TAG_VOLUME            _IOWR(FLT_IOCTL, SYNC_TAG_CMD, unsigned long)
#define IOCTL_INMAGE_GET_TAG_VOLUME_STATUS      _IOWR(FLT_IOCTL, SYNC_TAG_STATUS_CMD, unsigned long)
#define IOCTL_INMAGE_WAKEUP_ALL_THREADS         _IO(FLT_IOCTL, WAKEUP_THREADS_CMD)
#define IOCTL_INMAGE_GET_DB_NOTIFY_THRESHOLD    _IOWR(FLT_IOCTL, GET_DB_THRESHOLD_CMD, get_db_thres_t )
#define IOCTL_INMAGE_RESYNC_START_NOTIFICATION  _IOWR(FLT_IOCTL, RESYNC_START_CMD, RESYNC_START )
#define IOCTL_INMAGE_RESYNC_END_NOTIFICATION    _IOWR(FLT_IOCTL, RESYNC_END_CMD, RESYNC_END)
#define IOCTL_INMAGE_GET_DRIVER_VERSION         _IOWR(FLT_IOCTL, GET_DRIVER_VER_CMD, DRIVER_VERSION)
#define IOCTL_INMAGE_SHELL_LOG                  _IOWR(FLT_IOCTL, GET_SHELL_LOG_CMD, char *)
#define IOCTL_INMAGE_AT_LUN_CREATE          _IOW(FLT_IOCTL,  AT_LUN_CREATE_CMD, LUN_CREATE_INPUT)
#define IOCTL_INMAGE_AT_LUN_DELETE          _IOW(FLT_IOCTL,  AT_LUN_DELETE_CMD, LUN_DELETE_INPUT)
#define IOCTL_INMAGE_AT_LUN_LAST_WRITE_VI   _IOWR(FLT_IOCTL, AT_LUN_LAST_WRITE_VI_CMD, \
                                                  AT_LUN_LAST_WRITE_VI)
#define IOCTL_INMAGE_AT_LUN_QUERY           _IOWR(FLT_IOCTL, AT_LUN_QUERY_CMD, LUN_QUERY_DATA)
#define IOCTL_INMAGE_GET_GLOBAL_STATS           _IO(FLT_IOCTL, GET_GLOBAL_STATS_CMD)
#define IOCTL_INMAGE_GET_VOLUME_STATS           _IO(FLT_IOCTL, GET_VOLUME_STATS_CMD)
#define IOCTL_INMAGE_GET_PROTECTED_VOLUME_LIST  _IO(FLT_IOCTL, GET_PROTECTED_VOLUME_LIST_CMD)
#define IOCTL_INMAGE_GET_SET_ATTR               _IOWR(FLT_IOCTL, GET_SET_ATTR_CMD, struct _inm_attribute *)
#define IOCTL_INMAGE_BOOTTIME_STACKING		    _IO(FLT_IOCTL, BOOTTIME_STACKING_CMD)
#define IOCTL_INMAGE_VOLUME_UNSTACKING          _IOW(FLT_IOCTL, VOLUME_UNSTACKING_CMD, VOLUME_GUID)
#define IOCTL_INMAGE_GET_ADDITIONAL_VOLUME_STATS _IOWR(FLT_IOCTL, GET_ADDITIONAL_VOLUME_STATS_CMD, VOLUME_STATS_ADDITIONAL_INFO)
#define IOCTL_INMAGE_GET_VOLUME_LATENCY_STATS   _IOWR(FLT_IOCTL, GET_VOLUME_LATENCY_STATS_CMD, VOLUME_LATENCY_STATS)
#define IOCTL_INMAGE_GET_VOLUME_BMAP_STATS              _IOWR(FLT_IOCTL, GET_VOLUME_BMAP_STATS_CMD, VOLUME_BMAP_STATS)
#define IOCTL_INMAGE_SET_INVOLFLT_VERBOSITY	_IOWR(FLT_IOCTL, SET_INVOLFLT_VERBOSITY_CMD, int)
#define IOCTL_INMAGE_GET_MONITORING_STATS   _IOWR(FLT_IOCTL, GET_MONITORING_STATS_CMD, MONITORING_STATS)
#define IOCTL_INMAGE_INIT_DRIVER_FULLY		_IOW(FLT_IOCTL, INIT_DRIVER_FULLY_CMD, int)
#define IOCTL_INMAGE_GET_BLK_MQ_STATUS   _IOWR(FLT_IOCTL, GET_BLK_MQ_STATUS_CMD, int)
#define IOCTL_INMAGE_GET_VOLUME_STATS_V2   _IOWR(FLT_IOCTL, GET_VOLUME_STATS_V2_CMD, TELEMETRY_VOL_STATS)
#define IOCTL_INMAGE_NAME_MAPPING   _IOWR(FLT_IOCTL, NAME_MAPPING, vol_name_map_t)
#define IOCTL_INMAGE_LCW    _IOR(FLT_IOCTL, LCW, lcw_op_t)
#define IOCTL_INMAGE_WAIT_FOR_DB_V2              _IOW(FLT_IOCTL, WAIT_FOR_DB_CMD_V2, WAIT_FOR_DB_NOTIFY)
#define IOCTL_INMAGE_COMMITDB_FAIL_TRANS              _IOW(FLT_IOCTL, COMMITDB_FAIL_TRANS, COMMIT_DB_FAILURE_STATS)
#define IOCTL_INMAGE_GET_CXSTATS_NOTIFY               _IOWR(FLT_IOCTL, GET_CXSTATS_NOTIFY, VM_CXFAILURE_STATS)
#define IOCTL_INMAGE_WAKEUP_GET_CXSTATS_NOTIFY_THREAD _IO(FLT_IOCTL, WAKEUP_GET_CXSTATS_NOTIFY_THREAD)
#define IOCTL_INMAGE_TAG_DRAIN_NOTIFY                 _IOWR(FLT_IOCTL, TAG_DRAIN_NOTIFY, TAG_COMMIT_NOTIFY_OUTPUT)
#define IOCTL_INMAGE_WAKEUP_TAG_DRAIN_NOTIFY_THREAD   _IO(FLT_IOCTL, WAKEUP_TAG_DRAIN_NOTIFY_THREAD)
#define IOCTL_INMAGE_IOBARRIER_TAG_VOLUME       _IOWR(FLT_IOCTL, TAG_CMD_V3, tag_info_t_v2)
#define IOCTL_INMAGE_CREATE_BARRIER_ALL         _IOWR(FLT_IOCTL, CREATE_BARRIER, flt_barrier_create_t)
#define IOCTL_INMAGE_REMOVE_BARRIER_ALL         _IOWR(FLT_IOCTL, REMOVE_BARRIER, flt_barrier_remove_t)
#define IOCTL_INMAGE_MODIFY_PERSISTENT_DEVICE_NAME    _IOW(FLT_IOCTL, MODIFY_PERSISTENT_DEVICE_NAME, MODIFY_PERSISTENT_DEVICE_NAME_INPUT)
#define IOCTL_INMAGE_GET_DRAIN_STATE                  _IOWR(FLT_IOCTL, GET_DRAIN_STATE_CMD, GET_DISK_STATE_OUTPUT)
#define IOCTL_INMAGE_SET_DRAIN_STATE                  _IOWR(FLT_IOCTL, SET_DRAIN_STATE_CMD, SET_DRAIN_STATE_OUTPUT)


#endif
#ifdef INM_SOLARIS
#define INM_FLT_IOCTL		(0xfe)
#define INM_IOCTL_CODE(x)	(INM_FLT_IOCTL << 8 | (x))
#define IOCTL_INMAGE_VOLUME_STACKING        	INM_IOCTL_CODE(VOLUME_STACKING_CMD)
#define IOCTL_INMAGE_PROCESS_START_NOTIFY     	INM_IOCTL_CODE(START_NOTIFY_CMD)
#define IOCTL_INMAGE_SERVICE_SHUTDOWN_NOTIFY    INM_IOCTL_CODE(SHUTDOWN_NOTIFY_CMD)
#define IOCTL_INMAGE_STOP_FILTERING_DEVICE      INM_IOCTL_CODE(STOP_FILTER_CMD)
#define IOCTL_INMAGE_START_FILTERING_DEVICE     INM_IOCTL_CODE(START_FILTER_CMD)
#define IOCTL_INMAGE_START_FILTERING_DEVICE_V2  INM_IOCTL_CODE(START_FILTER_CMD_V2)
#define IOCTL_INMAGE_FREEZE_VOLUME              INM_IOCTL(VOLUME_FREEZE_CMD)
#define IOCTL_INMAGE_THAW_VOLUME                INM_IOCTL(VOLUME_THAW_CMD)

/* Mirror setup related ioctls */
#define IOCTL_INMAGE_START_MIRRORING_DEVICE     INM_IOCTL_CODE(START_MIRROR_CMD)
#define IOCTL_INMAGE_STOP_MIRRORING_DEVICE      INM_IOCTL_CODE(STOP_MIRROR_CMD)
#define IOCTL_INMAGE_MIRROR_VOLUME_STACKING    	INM_IOCTL_CODE(MIRROR_VOLUME_STACKING_CMD)
#define IOCTL_INMAGE_MIRROR_EXCEPTION_NOTIFY    INM_IOCTL_CODE(MIRROR_EXCEPTION_NOTIFY_CMD) 
#define IOCTL_INMAGE_MIRROR_TEST_HEARTBEAT      _IOW(FLT_IOCTL, MIRROR_TEST_HEARTBEAT_CMD, SCSI_ID)
#define IOCTL_INMAGE_BLOCK_AT_LUN		INM_IOCTL_CODE(BLOCK_AT_LUN_CMD)

/* A temp hack. Maximum structure size that could be passed through _IO i/f is 16K whereas
 * UDIRTY_BLOCK is 20K, hence we just pass VolumeGuid as structure only for compilation pruposes,
 * In the kernel, necessary checks are done prior to accessing the user space UDIRTY block.
 */ 
#define IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS    	INM_IOCTL_CODE(GET_DB_CMD)
#define IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS  INM_IOCTL_CODE(COMMIT_DB_CMD)
#define IOCTL_INMAGE_SET_VOLUME_FLAGS        	INM_IOCTL_CODE(SET_VOL_FLAGS_CMD)
#define IOCTL_INMAGE_GET_VOLUME_FLAGS        	INM_IOCTL_CODE(GET_VOL_FLAGS_CMD)
#define IOCTL_INMAGE_WAIT_FOR_DB        	INM_IOCTL_CODE(WAIT_FOR_DB_CMD)
#define IOCTL_INMAGE_CLEAR_DIFFERENTIALS    	INM_IOCTL_CODE(CLEAR_DIFFS_CMD)
#define IOCTL_INMAGE_GET_NANOSECOND_TIME    	INM_IOCTL_CODE(GET_TIME_CMD)
#define IOCTL_INMAGE_UNSTACK_ALL 		INM_IOCTL_CODE(UNSTACK_ALL_CMD)
#define IOCTL_INMAGE_SYS_SHUTDOWN       	INM_IOCTL_CODE(SYS_SHUTDOWN_NOTIFY_CMD)
#define IOCTL_INMAGE_TAG_VOLUME			INM_IOCTL_CODE(TAG_CMD)
#define IOCTL_INMAGE_SYNC_TAG_VOLUME		INM_IOCTL_CODE(SYNC_TAG_CMD)
#define IOCTL_INMAGE_GET_TAG_VOLUME_STATUS      INM_IOCTL_CODE(SYNC_TAG_STATUS_CMD)
#define IOCTL_INMAGE_WAKEUP_ALL_THREADS     	INM_IOCTL_CODE(WAKEUP_THREADS_CMD)
#define IOCTL_INMAGE_GET_DB_NOTIFY_THRESHOLD	INM_IOCTL_CODE(GET_DB_THRESHOLD_CMD)
#define IOCTL_INMAGE_RESYNC_START_NOTIFICATION	INM_IOCTL_CODE(RESYNC_START_CMD)
#define IOCTL_INMAGE_RESYNC_END_NOTIFICATION	INM_IOCTL_CODE(RESYNC_END_CMD)
#define IOCTL_INMAGE_GET_DRIVER_VERSION  	INM_IOCTL_CODE(GET_DRIVER_VER_CMD)
#define IOCTL_INMAGE_SHELL_LOG    		INM_IOCTL_CODE(GET_SHELL_LOG_CMD)
#define IOCTL_INMAGE_AT_LUN_CREATE		INM_IOCTL_CODE(AT_LUN_CREATE_CMD)
#define IOCTL_INMAGE_AT_LUN_DELETE		INM_IOCTL_CODE(AT_LUN_DELETE_CMD)
#define IOCTL_INMAGE_AT_LUN_LAST_WRITE_VI	INM_IOCTL_CODE(AT_LUN_LAST_WRITE_VI_CMD)
#define IOCTL_INMAGE_AT_LUN_QUERY		INM_IOCTL_CODE(AT_LUN_QUERY_CMD)
#define IOCTL_INMAGE_GET_GLOBAL_STATS		INM_IOCTL_CODE(GET_GLOBAL_STATS_CMD)
#define IOCTL_INMAGE_GET_VOLUME_STATS		INM_IOCTL_CODE(GET_VOLUME_STATS_CMD)
#define IOCTL_INMAGE_GET_PROTECTED_VOLUME_LIST  INM_IOCTL_CODE(GET_PROTECTED_VOLUME_LIST_CMD)
#define IOCTL_INMAGE_GET_SET_ATTR		INM_IOCTL_CODE(GET_SET_ATTR_CMD)
#define IOCTL_INMAGE_BOOTTIME_STACKING		INM_IOCTL_CODE(BOOTTIME_STACKING_CMD)
#define IOCTL_INMAGE_VOLUME_UNSTACKING          INM_IOCTL_CODE(VOLUME_UNSTACKING_CMD)
#define IOCTL_INMAGE_GET_ADDITIONAL_VOLUME_STATS INM_IOCTL_CODE(GET_ADDITIONAL_VOLUME_STATS_CMD)
#define IOCTL_INMAGE_GET_VOLUME_LATENCY_STATS   INM_IOCTL_CODE(GET_VOLUME_LATENCY_STATS_CMD)
#define IOCTL_INMAGE_GET_VOLUME_BMAP_STATS             INM_IOCTL_CODE(GET_VOLUME_BMAP_STATS_CMD)
#define IOCTL_INMAGE_SET_INVOLFLT_VERBOSITY	INM_IOCTL_CODE(SET_INVOLFLT_VERBOSITY_CMD)
#define IOCTL_INMAGE_MIRROR_TEST_HEARTBEAT      INM_IOCTL_CODE(MIRROR_TEST_HEARTBEAT_CMD)

#endif

#ifdef INM_AIX
#define INM_FLT_IOCTL          (0xfe)
#define INM_IOCTL_CODE(x)      (INM_FLT_IOCTL << 8 | (x))
#define IOCTL_INMAGE_VOLUME_STACKING           INM_IOCTL_CODE(VOLUME_STACKING_CMD)
#define IOCTL_INMAGE_PROCESS_START_NOTIFY      INM_IOCTL_CODE(START_NOTIFY_CMD)
#define IOCTL_INMAGE_SERVICE_SHUTDOWN_NOTIFY    INM_IOCTL_CODE(SHUTDOWN_NOTIFY_CMD)
#define IOCTL_INMAGE_STOP_FILTERING_DEVICE      INM_IOCTL_CODE(STOP_FILTER_CMD)
#define IOCTL_INMAGE_START_FILTERING_DEVICE     INM_IOCTL_CODE(START_FILTER_CMD)
#define IOCTL_INMAGE_START_FILTERING_DEVICE_V2  INM_IOCTL_CODE(START_FILTER_CMD_V2)
#define IOCTL_INMAGE_FREEZE_VOLUME              INM_IOCTL_CODE(VOLUME_FREEZE_CMD)
#define IOCTL_INMAGE_THAW_VOLUME                INM_IOCTL_CODE(VOLUME_THAW_CMD)

/* Mirror setup related ioctls */
#define IOCTL_INMAGE_START_MIRRORING_DEVICE     INM_IOCTL_CODE(START_MIRROR_CMD)
#define IOCTL_INMAGE_STOP_MIRRORING_DEVICE      INM_IOCTL_CODE(STOP_MIRROR_CMD)
#define IOCTL_INMAGE_MIRROR_VOLUME_STACKING     INM_IOCTL_CODE(MIRROR_VOLUME_STACKING_CMD)
#define IOCTL_INMAGE_BLOCK_AT_LUN		INM_IOCTL_CODE(BLOCK_AT_LUN_CMD)
#define IOCTL_INMAGE_BLOCK_AT_LUN_ACCESS	INM_IOCTL_CODE(BLOCK_AT_LUN_ACCESS_CMD)
#define IOCTL_INMAGE_MAX_XFER_SZ		INM_IOCTL_CODE(MAX_XFER_SZ_CMD)
#define IOCTL_INMAGE_MIRROR_TEST_HEARTBEAT      INM_IOCTL_CODE(MIRROR_TEST_HEARTBEAT_CMD)

/* A temp hack. Maximum structure size that could be passed through _IO i/f is 16K whereas
 * UDIRTY_BLOCK is 20K, hence we just pass VolumeGuid as structure only for compilation pruposes,
 * In the kernel, necessary checks are done prior to accessing the user space UDIRTY block.
 */ 
#define IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS            INM_IOCTL_CODE(GET_DB_CMD)
#define IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS  INM_IOCTL_CODE(COMMIT_DB_CMD)
#define IOCTL_INMAGE_SET_VOLUME_FLAGS          INM_IOCTL_CODE(SET_VOL_FLAGS_CMD)
#define IOCTL_INMAGE_GET_VOLUME_FLAGS          INM_IOCTL_CODE(GET_VOL_FLAGS_CMD)
#define IOCTL_INMAGE_WAIT_FOR_DB               INM_IOCTL_CODE(WAIT_FOR_DB_CMD)
#define IOCTL_INMAGE_CLEAR_DIFFERENTIALS       INM_IOCTL_CODE(CLEAR_DIFFS_CMD)
#define IOCTL_INMAGE_GET_NANOSECOND_TIME       INM_IOCTL_CODE(GET_TIME_CMD)
#define IOCTL_INMAGE_UNSTACK_ALL               INM_IOCTL_CODE(UNSTACK_ALL_CMD)
#define IOCTL_INMAGE_SYS_SHUTDOWN              INM_IOCTL_CODE(SYS_SHUTDOWN_NOTIFY_CMD)
#define IOCTL_INMAGE_TAG_VOLUME                        INM_IOCTL_CODE(TAG_CMD)
#define IOCTL_INMAGE_WAKEUP_ALL_THREADS        INM_IOCTL_CODE(WAKEUP_THREADS_CMD)
#define IOCTL_INMAGE_GET_DB_NOTIFY_THRESHOLD   INM_IOCTL_CODE(GET_DB_THRESHOLD_CMD)
#define IOCTL_INMAGE_RESYNC_START_NOTIFICATION INM_IOCTL_CODE(RESYNC_START_CMD)
#define IOCTL_INMAGE_RESYNC_END_NOTIFICATION   INM_IOCTL_CODE(RESYNC_END_CMD)
#define IOCTL_INMAGE_GET_DRIVER_VERSION        INM_IOCTL_CODE(GET_DRIVER_VER_CMD)
#define IOCTL_INMAGE_SHELL_LOG                 INM_IOCTL_CODE(GET_SHELL_LOG_CMD)
#define IOCTL_INMAGE_AT_LUN_CREATE             INM_IOCTL_CODE(AT_LUN_CREATE_CMD)
#define IOCTL_INMAGE_AT_LUN_DELETE             INM_IOCTL_CODE(AT_LUN_DELETE_CMD)
#define IOCTL_INMAGE_AT_LUN_LAST_WRITE_VI      INM_IOCTL_CODE(AT_LUN_LAST_WRITE_VI_CMD)
#define IOCTL_INMAGE_AT_LUN_QUERY              INM_IOCTL_CODE(AT_LUN_QUERY_CMD)
#define IOCTL_INMAGE_GET_GLOBAL_STATS          INM_IOCTL_CODE(GET_GLOBAL_STATS_CMD)
#define IOCTL_INMAGE_GET_VOLUME_STATS          INM_IOCTL_CODE(GET_VOLUME_STATS_CMD)
#define IOCTL_INMAGE_GET_PROTECTED_VOLUME_LIST  INM_IOCTL_CODE(GET_PROTECTED_VOLUME_LIST_CMD)
#define IOCTL_INMAGE_GET_SET_ATTR              INM_IOCTL_CODE(GET_SET_ATTR_CMD)
#define IOCTL_INMAGE_BOOTTIME_STACKING         INM_IOCTL_CODE(BOOTTIME_STACKING_CMD)
#define IOCTL_INMAGE_VOLUME_UNSTACKING          INM_IOCTL_CODE(VOLUME_UNSTACKING_CMD)
#define IOCTL_INMAGE_GET_DMESG			INM_IOCTL_CODE(GET_DMESG_CMD)
#define IOCTL_INMAGE_GET_ADDITIONAL_VOLUME_STATS INM_IOCTL_CODE(GET_ADDITIONAL_VOLUME_STATS_CMD)
#define IOCTL_INMAGE_GET_VOLUME_LATENCY_STATS   INM_IOCTL_CODE(GET_VOLUME_LATENCY_STATS_CMD)
#define IOCTL_INMAGE_GET_VOLUME_BMAP_STATS             INM_IOCTL_CODE(GET_VOLUME_BMAP_STATS_CMD)
#define IOCTL_INMAGE_SET_INVOLFLT_VERBOSITY	INM_IOCTL_CODE(SET_INVOLFLT_VERBOSITY_CMD)
#endif

/* Error numbers logged to registry when volume is out of sync */
#define ERROR_TO_REG_BITMAP_READ_ERROR                      0x0002
#define ERROR_TO_REG_BITMAP_WRITE_ERROR                     0x0003
#define ERROR_TO_REG_BITMAP_OPEN_ERROR                      0x0004
#define ERROR_TO_REG_BITMAP_OPEN_FAIL_CHANGES_LOST          (0x0005)
#define ERROR_TO_REG_OUT_OF_BOUND_IO                        0x0006
#define ERROR_TO_REG_INVALID_IO                             0x0007
#define ERROR_TO_REG_DESCRIPTION_IN_EVENT_LOG               0x0008
#define ERROR_TO_REG_NO_MEM_FOR_WORK_QUEUE_ITEM             0x0009
#define ERROR_TO_REG_WRITE_TO_CNT_IOC_PATH                  0x000a
#define ERROR_TO_REG_VENDOR_CDB_ERR                         0x000b
#define RESYNC_DUE_TO_ERR_INJECTION                         0x000c
#define ERROR_TO_REG_AT_PATHS_FAILURE                       0x000d
#define ERROR_TO_REG_BAD_AT_DEVICE_LIST                     0x000e
#define ERROR_TO_REG_FAILED_TO_ALLOC_BIOINFO                0x000f
#define ERROR_TO_REG_MAX_ERROR ERROR_TO_REG_FAILED_TO_ALLOC_BIOINFO

#define INITIAL_BUFSZ_FOR_VOL_LIST                              4096 * 500

#define TAG_FS_FROZEN_IN_USERSPACE                    0x0004

typedef struct _VOLUME_STATS_ADDITIONAL_INFO
{
    VOLUME_GUID VolumeGuid;
    uint64_t ullTotalChangesPending;
    uint64_t ullOldestChangeTimeStamp;
    uint64_t ullDriverCurrentTimeStamp;
}VOLUME_STATS_ADDITIONAL_INFO, *PVOLUME_STATS_ADDITIONAL_INFO;


#define INM_LATENCY_DIST_BKT_CAPACITY   12 //takecare of this macro in target context.h 
#define INM_LATENCY_LOG_CAPACITY        12

typedef struct __VOLUME_LATENCY_STATS
{
	VOLUME_GUID	VolumeGuid;
        inm_u64_t       s2dbret_bkts[INM_LATENCY_DIST_BKT_CAPACITY];
        inm_u32_t       s2dbret_freq[INM_LATENCY_DIST_BKT_CAPACITY];
        inm_u32_t       s2dbret_nr_avail_bkts;
        inm_u64_t       s2dbret_log_buf[INM_LATENCY_LOG_CAPACITY];
        inm_u64_t       s2dbret_log_min;
        inm_u64_t       s2dbret_log_max;

        inm_u64_t       s2dbwait_notify_bkts[INM_LATENCY_DIST_BKT_CAPACITY];
        inm_u32_t       s2dbwait_notify_freq[INM_LATENCY_DIST_BKT_CAPACITY];
        inm_u32_t       s2dbwait_notify_nr_avail_bkts;
        inm_u64_t       s2dbwait_notify_log_buf[INM_LATENCY_LOG_CAPACITY];
        inm_u64_t       s2dbwait_notify_log_min;
        inm_u64_t       s2dbwait_notify_log_max;

        inm_u64_t       s2dbcommit_bkts[INM_LATENCY_DIST_BKT_CAPACITY];
        inm_u32_t       s2dbcommit_freq[INM_LATENCY_DIST_BKT_CAPACITY];
        inm_u32_t       s2dbcommit_nr_avail_bkts;
        inm_u64_t       s2dbcommit_log_buf[INM_LATENCY_LOG_CAPACITY];
        inm_u64_t       s2dbcommit_log_min;
        inm_u64_t       s2dbcommit_log_max;
} VOLUME_LATENCY_STATS;

typedef struct __VOLUME_BMAP_STATS
{
        VOLUME_GUID     VolumeGuid;
        inm_u64_t       bmap_gran;
        inm_u64_t       bmap_data_sz;
        inm_u32_t       nr_dbs;
} VOLUME_BMAP_STATS;

/* only Linux is using inm_dmit to issue the AT lun blocking ioctl.
 * AIX uses other binary and Solaris uses generic sd_open block, that 
 * why here we just populated Linux specific structure. 
 * If required for the other platform populate specific structure here.
 */

typedef struct _inm_at_lun_reconfig{
    inm_u64_t flag;
    char      atdev_name[INM_GUID_LEN_MAX];
}inm_at_lun_reconfig_t;

#define ADD_AT_LUN_GLOBAL_LIST 1
#define DEL_AT_LUN_GLOBAL_LIST 2

typedef enum _svc_state {
    SERVICE_UNITIALIZED = 0x00,
    SERVICE_NOTSTARTED  = 0x01,
    SERVICE_RUNNING     = 0x02,
    SERVICE_SHUTDOWN    = 0x03,
    MAX_SERVICE_STATES  = 0x04,
} svc_state_t;

typedef enum _etDriverMode {
    UninitializedMode = 0,
    NoRebootMode,
    RebootMode
} etDriverMode;

#define VOLUME_STATS_DATA_MAJOR_VERSION         0x0003
#define VOLUME_STATS_DATA_MINOR_VERSION         0x0000

typedef struct _VOLUME_STATS_DATA {
        unsigned short          usMajorVersion;
        unsigned short          usMinorVersion;
        unsigned long           ulVolumesReturned;
        unsigned long           ulNonPagedMemoryLimitInMB;
        unsigned long           LockedDataBlockCounter;
        unsigned long           ulTotalVolumes;
        unsigned short          ulNumProtectedDisk;
        svc_state_t             eServiceState;
        etDriverMode            eDiskFilterMode;
        char                    LastShutdownMarker;
        int                     PersistentRegistryCreated;
        unsigned long           ulDriverFlags;
        long                    ulCommonBootCounter;
        unsigned long long      ullDataPoolSizeAllocated;
        unsigned long long      ullPersistedTimeStampAfterBoot;
        unsigned long long      ullPersistedSequenceNumberAfterBoot;
} VOLUME_STATS_DATA;

typedef struct _LARGE_INTEGER {
    long long QuadPart;
} LARGE_INTEGER;

typedef struct _ULARGE_INTEGER {
    unsigned long long QuadPart;
} ULARGE_INTEGER;

typedef struct _VOLUME_STATS_V2 {
    // driver stats, but included in this per disk structure as windows
    // doing this and to make agent code compatible for both Linux and Windows
    char                VolumeGUID[GUID_SIZE_IN_CHARS];
    unsigned long long  ullDataPoolSize;
    LARGE_INTEGER       liDriverLoadTime;
    long long           llTimeJumpDetectedTS;
    long long           llTimeJumpedTS;
    LARGE_INTEGER       liLastS2StartTime;
    LARGE_INTEGER       liLastS2StopTime;
    LARGE_INTEGER       liLastAgentStartTime;
    LARGE_INTEGER       liLastAgentStopTime;
    LARGE_INTEGER       liLastTagReq;
    LARGE_INTEGER       liStopFilteringAllTimeStamp;

    // per disk stats
    unsigned long long  ullTotalTrackedBytes;
    ULARGE_INTEGER      ulVolumeSize;
    unsigned long       ulVolumeFlags;
    LARGE_INTEGER       liVolumeContextCreationTS;
    LARGE_INTEGER       liStartFilteringTimeStamp;
    LARGE_INTEGER       liStartFilteringTimeStampByUser;
    LARGE_INTEGER       liStopFilteringTimeStamp;
    LARGE_INTEGER       liStopFilteringTimestampByUser;
    LARGE_INTEGER       liClearDiffsTimeStamp;
    LARGE_INTEGER       liCommitDBTimeStamp;
    LARGE_INTEGER       liGetDBTimeStamp;
} VOLUME_STATS_V2;

typedef struct _TELEMETRY_VOL_STATS
{
    VOLUME_STATS_DATA  drv_stats;
    VOLUME_STATS_V2    vol_stats;
} TELEMETRY_VOL_STATS;

typedef struct flt_barrier_create {
    char    fbc_guid[GUID_LEN];
    int     fbc_timeout_ms;
    int     fbc_flags;
} flt_barrier_create_t;

typedef struct flt_barrier_remove {
    char    fbr_guid[GUID_LEN];
    int     fbr_flags;
} flt_barrier_remove_t;

#endif // ifndef INVOLFLT_H
