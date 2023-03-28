#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif
#include <linux/version.h>

#include "svdparse.h"

#define GUID_SIZE_IN_CHARS 128
#define INM_GUID_LEN_MAX 256
#define INM_PATH_MAX (4096)
#define MAX_DIRTY_CHANGES_V2 204
#define GUID_LEN             36
#define TAG_MAX_LENGTH       256
#define TAG_VOLUME_MAX_LENGTH   256

#define TAG_ALL_PROTECTED_VOLUME_IOBARRIER            0x0004

#define INM_CTRL_DEV    "/dev/involflt"

#define STREAM_REC_TYPE_START_OF_TAG_LIST   0x0001
#define STREAM_REC_TYPE_END_OF_TAG_LIST     0x0002
#define STREAM_REC_TYPE_TIME_STAMP_TAG      0x0003
#define STREAM_REC_TYPE_DATA_SOURCE         0x0004
#define STREAM_REC_TYPE_USER_DEFINED_TAG    0x0005
#define STREAM_REC_TYPE_PADDING             0x0006

#define UDIRTY_BLOCK_FLAG_START_OF_SPLIT_CHANGE     0x00000001
#define UDIRTY_BLOCK_FLAG_PART_OF_SPLIT_CHANGE      0x00000002
#define UDIRTY_BLOCK_FLAG_END_OF_SPLIT_CHANGE       0x00000004
#define UDIRTY_BLOCK_FLAG_DATA_FILE                 0x00000008
#define UDIRTY_BLOCK_FLAG_SVD_STREAM                0x00000010
#define UDIRTY_BLOCK_FLAG_VOLUME_RESYNC_REQUIRED    0x80000000
#define UDIRTY_BLOCK_FLAG_TSO_FILE                  0x00000020

#define INVOLFLT_DATA_SOURCE_UNDEFINED  0x00
#define INVOLFLT_DATA_SOURCE_BITMAP     0x01
#define INVOLFLT_DATA_SOURCE_META_DATA  0x02
#define INVOLFLT_DATA_SOURCE_DATA       0x03

#define STREAM_REC_FLAGS_LENGTH_BIT         0x01

typedef struct _STREAM_REC_HDR_8B
{
    unsigned short       usStreamRecType;
    unsigned char        ucFlags;    // STREAM_REC_FLAGS_LENGTH_BIT bit is set for this record.
    unsigned char        ucReserved;
    unsigned int         ulLength; // Length includes size of this header too.
} STREAM_REC_HDR_8B, *PSTREAM_REC_HDR_8B;

typedef struct _STREAM_REC_HDR_4B
{
    unsigned short       usStreamRecType;
    unsigned char        ucFlags;
    unsigned char        ucLength;
} STREAM_REC_HDR_4B, *PSTREAM_REC_HDR_4B;

#define GET_STREAM_LENGTH(pHeader)                              \
    ( (((PSTREAM_REC_HDR_4B)pHeader)->ucFlags & STREAM_REC_FLAGS_LENGTH_BIT) ?      \
                (((PSTREAM_REC_HDR_8B)pHeader)->ulLength) :                         \
                (((PSTREAM_REC_HDR_4B)pHeader)->ucLength))

#define COMMIT_TRANSACTION_FLAG_RESET_RESYNC_REQUIRED_FLAG  0x00000001

#define IOCTL_INMAGE_START_FILTERING_DEVICE_V2       _IOW(FLT_IOCTL, START_FILTER_CMD_V2, inm_dev_info_compat_t)

/* A temp hack. Maximum structure size that could be passed through _IO i/f is 16K whereas
 * UDIRTY_BLOCK is 20K, hence we just pass VolumeGuid as structure only for compilation pruposes,
 * In the kernel, necessary checks are done prior to accessing the user space UDIRTY block.
 */

typedef struct
{
    char volume_guid[GUID_SIZE_IN_CHARS];
} VOLUME_GUID;

#define IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS     _IOWR(FLT_IOCTL, GET_DB_CMD, VOLUME_GUID)
#define IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS  _IOW(FLT_IOCTL, COMMIT_DB_CMD, COMMIT_TRANSACTION)
#define IOCTL_INMAGE_WAIT_FOR_DB                _IOW(FLT_IOCTL, WAIT_FOR_DB_CMD, WAIT_FOR_DB_NOTIFY)

#define IOCTL_INMAGE_IOBARRIER_TAG_VOLUME       _IOWR(FLT_IOCTL, TAG_CMD_V3, tag_info_t_v2)

#define IOCTL_INMAGE_SERVICE_SHUTDOWN_NOTIFY     _IOW(FLT_IOCTL, SHUTDOWN_NOTIFY_CMD, SHUTDOWN_NOTIFY_INPUT)

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
    TAG_CMD_V3
};


typedef enum inm_device {
    FILTER_DEV_FABRIC_LUN = 1,
    FILTER_DEV_HOST_VOLUME = 2,
    FILTER_DEV_FABRIC_VSNAP = 3,
    FILTER_DEV_FABRIC_RESYNC = 4,
    FILTER_DEV_MIRROR_SETUP = 5,
} inm_dev_t;

typedef enum _etWriterOrderState
{
    ecWriteOrderStateUnInitialized = 0,
    ecWriteOrderStateBitmap = 1,
    ecWriteOrderStateMetadata = 2,
    ecWriteOrderStateData = 3,
    ecWriteOrderStateRawBitmap = 4
} etWriteOrderState, *petWriteOrderState;


typedef struct _TIME_STAMP_TAG_V2
{
    STREAM_REC_HDR_4B    Header;
    STREAM_REC_HDR_4B    Reserved;
    unsigned long long   ullSequenceNumber;
    unsigned long long   TimeInHundNanoSecondsFromJan1601;
} TIME_STAMP_TAG_V2, *PTIME_STAMP_TAG_V2;

typedef struct _DATA_SOURCE_TAG
{
    STREAM_REC_HDR_4B   Header;
    unsigned int        ulDataSource;
} DATA_SOURCE_TAG, *PDATA_SOURCE_TAG;

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
            unsigned int                ulcbChangesInStream;
            /* This is actually a pointer to memory and not an array of pointers.
             * It contains Changes in linear memorylocation.
             */

            void                    **ppBufferArray;
            unsigned int            ulcbBufferArraySize;

            // resync flags
            unsigned long                       ulOutOfSyncCount;
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
} UDIRTY_BLOCK_V2, *PUDIRTY_BLOCK_V2;//, DIRTY_BLOCKS, *PDIRTY_BLOCKS;

typedef struct _COMMIT_TRANSACTION
{
    unsigned char        VolumeGUID[GUID_SIZE_IN_CHARS];
    unsigned long long   ulTransactionID;
    unsigned int     ulFlags;
} COMMIT_TRANSACTION, *PCOMMIT_TRANSACTION;

typedef struct
{
    unsigned char    VolumeGUID[GUID_SIZE_IN_CHARS];
    int         Seconds; //Maximum time to wait in the kernel
} WAIT_FOR_DB_NOTIFY;

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

/* Structure definition to tag */
typedef struct volume_info
{
    int           flags; /* Fill it with 0s, will be used in future */
    int           status; /* Status of the volume */
    char          vol_name[TAG_VOLUME_MAX_LENGTH]; /* volume name */
} volume_info_t;

typedef struct tag_names
{
    unsigned short  tag_len;/*tag length header plus name*/
    char            tag_name[TAG_MAX_LENGTH]; /* volume name */
} tag_names_t;

typedef struct tag_info
{
    int                   flags;/* Fill it with 0s, will be used in future */
    int                   nr_vols;
    int                   nr_tags;
    int                   timeout;/*time to not drain the dirty block has tag*/
    char                  tag_guid[GUID_LEN];/* one guid for set of volumes*/
    volume_info_t         *vol_info;  /* Array of volume_info_t object */
    tag_names_t           *tag_names; /* Arrays of tag names */
                                      /* each of length TAG_MAX_LENGTH */
} tag_info_t_v2;

typedef struct _SHUTDOWN_NOTIFY_INPUT
{
    unsigned int ulFlags;
} SHUTDOWN_NOTIFY_INPUT, *PSHUTDOWN_NOTIFY_INPUT;

