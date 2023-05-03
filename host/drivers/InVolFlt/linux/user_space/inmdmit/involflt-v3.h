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
#include "inmtypes.h"
#include "svdparse.h"

#define INMAGE_FILTER_DEVICE_NAME    "/dev/involflt"
#define GUID_SIZE_IN_CHARS 128
#define MAX_INITIATOR_NAME_LEN 24
#define INM_GUID_LEN_MAX 256

// Version information stated with Driver Version 2.0.0.0
// This version has data stream changes
#define DRIVER_MAJOR_VERSION    0x02
#define DRIVER_MINOR_VERSION    0x01
#define DRIVER_MINOR_VERSION2   0x00
#define DRIVER_MINOR_VERSION3   0x00

#define INM_DRIVER_VERSION(a, b, c, d)	\
		(((a) << 24) + ((b) << 16) +  ((c) << 8)  + (d))

typedef enum inm_device {
    FILTER_DEV_FABRIC_LUN = 1,
    FILTER_DEV_HOST_VOLUME = 2,
    FILTER_DEV_FABRIC_VSNAP = 3,
    FILTER_DEV_FABRIC_RESYNC = 4,
} inm_dev_t;

typedef struct inm_dev_info {
    inm_dev_t d_type;
    char d_guid[INM_GUID_LEN_MAX];
    unsigned long long d_nblks;
    unsigned long long d_bsize;
} inm_dev_info_t;

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
    unsigned int   ulSequenceNumber;
    unsigned long long   TimeInHundNanoSecondsFromJan1601;
} TIME_STAMP_TAG, *PTIME_STAMP_TAG;

typedef struct _TIME_STAMP_TAG_V2
{
    STREAM_REC_HDR_4B    Header;
    unsigned long long   ulSequenceNumber;
    unsigned long long   TimeInHundNanoSecondsFromJan1601;
} TIME_STAMP_TAG_V2, *PTIME_STAMP_TAG_V2;

typedef enum {
	INVOLFLT_DATA_SOURCE_UNDEFINED=0x00,
	INVOLFLT_DATA_SOURCE_BITMAP=0x01,
	INVOLFLT_DATA_SOURCE_META_DATA=0x02,
	INVOLFLT_DATA_SOURCE_DATA=0x03,
	INVOLFLT_DATA_SOURCE_CONTROL_DATA=0x04,
} sv_data_src_t;

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
#define UDIRTY_BLOCK_FLAG_SVD_STREAM                0x00000010
#define UDIRTY_BLOCK_FLAG_VOLUME_RESYNC_REQUIRED    0x80000000
#define UDIRTY_BLOCK_FLAG_64BIT			    0x40000000

#define COMMIT_TRANSACTION_FLAG_RESET_RESYNC_REQUIRED_FLAG  0x00000001

/* values for accessType in UdirtyMetaData structure */
#define INM_MDATA_COPIED			0x00
#define INM_MDATA_MAPPED			0x01

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

#define UDIRTY_BLOCK_HEADER_SIZE_V2 	64
#define TOTAL_UDIRTY_BLOCK_SIZE		2*4096	//2 PAGES
#define UDIRTY_BLOCK_DATA_SIZE_V2       ((TOTAL_UDIRTY_BLOCK_SIZE) -    \
						UDIRTY_BLOCK_HEADER_SIZE_V2)
#define MAX_DIRTY_CHANGES_V2            (((TOTAL_UDIRTY_BLOCK_SIZE) - \
						((UDIRTY_BLOCK_HEADER_SIZE_V2) + \
						offsetof(struct UdirtyMetaData, MetaChanges))) / \
					sizeof(struct UMetaChange)

#define UDIRTY_BLOCK_MAX_FILE_NAME_V2 0x1000 //4k

struct UdirtyDataStream {
	sv_u64_t	ullcbChanges;		/* # of changes in bytes */
	sv_u32_t	ulcChanges;		/* # of changes */
	sv_u16_t	ulBufferSize;		/* Amount of dirty changes in 
					 * buffer */
	sv_u16_t	usNumberOfBuffers;
	sv_u32_t	ulcbChangesInStream;	/* # of bytes in svd stream */
	sv_u32_t	ulcbBufferArraySize;
	sv_u8_t		ulSequenceIDforSplitIO; /* sequence id for split io */
	sv_uchar	Reserved1[7];
	union {
		void 		*pBufferArray[1];
		sv_uchar	Reserved2[8];
	};
};

struct UdirtyDataFile {
	sv_u64_t	ullcbChanges;		/* # of changes in bytes */
	sv_u32_t	ulcChanges;		/* # of changes */
	sv_u8_t		ulSequenceIDforSplitIO; /* sequence id for split io */
	sv_uchar	Reserved1[3];
	sv_u16_t	usLength;
	sv_uchar	FileName[UDIRTY_BLOCK_MAX_FILE_NAME_V2];
};

struct UMetaChange {
	sv_u32_t	ChangeOffsetHi;
	sv_u32_t	ChangeOffsetLow;
	sv_u32_t	ChangeLen;
	sv_u32_t	SeqNrDelta;
	sv_u32_t	TimeDelta;
};




struct UdirtyMetaData {
	sv_u64_t	ullcbChanges;		/* # of changes in bytes */
	sv_u32_t	ulcChanges;		/* # of changes */
	sv_u8_t		accessType; 	/* copied/mapped */
	sv_u8_t		ulSequenceIDforSplitIO; /* sequence id for split io */
	sv_uchar	Reserved1[2];
	union {
		struct {
			sv_u16_t	ulBufferSize;
			sv_u16_t	usNumberOfBuffers;
			void   	  	*pMetaArray[1];
	    	};
		struct UMetaChange changes[1];
	} MetaChanges;
};

struct UdirtyTag {
	union {
		sv_uchar *pTag;
		sv_uchar Reserved[8];
	};
};
struct UdirtyControlData {
	/* To handle resync operation */
	sv_u32_t	ulOutOfSyncCount;	/* # of tmes out of sync 
						 * marked */
	sv_u32_t	ulOutOfSyncErrorCode;	/* out of sync err code */
	sv_u64_t	ullOutOfSyncTimeStamp;	/* out of sync - time stamp */
	sv_uchar				/* err str for out of sync */ \
		ErrorStringForResync[UDIRTY_BLOCK_MAX_ERROR_STRING_SIZE];

	/* To handle volume resize operation */
	sv_u64_t	ullVolumeResize;
	sv_u32_t	uiVolumeResizeCount;
	sv_uchar	Reserved[4];
};
	

typedef struct _UDIRTY_BLOCK_V2 {
	/* Dirty Block Header */
	union {
		struct {
			sv_u64_t	ullTransactionID;
			sv_u32_t 	uiFlags;
			sv_data_src_t	uiDataSource;
			sv_u64_t	ullTimeStampOfFirstChange;
			sv_u64_t 	ullSeqNrOfFirstChange;
			sv_u64_t 	ullTimeStampOfLastChange;
			sv_u64_t 	ullSeqNrOfLastChange;
			sv_u64_t 	ullcbTotalChangesPending; 
			sv_u32_t 	ulTotalChangesPending;
			sv_uchar	Reserved[4];
		} DBHdr;
	
		sv_uchar			/* Reserved area for Hdr */ \
			BufferReservedForHeader[UDIRTY_BLOCK_HEADER_SIZE_V2];
	} uHdr;

	/* Dirty Block Content */
	union {
		struct UdirtyControlData 	ControlData;
		struct UdirtyDataStream 	DataStream;
		struct UdirtyDataFile		DataFile;
		struct UdirtyMetaData		MetaData;
		struct UdirtyTag		Tag;
		sv_uchar			\
			Buffer[UDIRTY_BLOCK_DATA_SIZE_V2]; 
	};
}UDIRTY_BLOCK_V2, *PUDIRTY_BLOCK_V2; //, DIRTY_BLOCKS,*PDIRTY_BLOCKS;
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
    sv_uchar VolumeGUID[GUID_SIZE_IN_CHARS];
    sv_u64_t TimeInHundNanoSecondsFromJan1601;
    sv_u64_t ullSequenceNumber;
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
    sv_uchar VolumeGUID[GUID_SIZE_IN_CHARS];
    sv_u64_t TimeInHundNanoSecondsFromJan1601;
    sv_u64_t ullSequenceNumber;
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
	AT_LUN_QUERY_CMD
};

#define IOCTL_INMAGE_VOLUME_STACKING     	_IOW(FLT_IOCTL, VOLUME_STACKING_CMD, PROCESS_VOLUME_STACKING_INPUT) 
#define IOCTL_INMAGE_PROCESS_START_NOTIFY     	_IOW(FLT_IOCTL, START_NOTIFY_CMD, PROCESS_START_NOTIFY_INPUT) 
#define IOCTL_INMAGE_SERVICE_SHUTDOWN_NOTIFY     _IOW(FLT_IOCTL, SHUTDOWN_NOTIFY_CMD, SHUTDOWN_NOTIFY_INPUT)
#define IOCTL_INMAGE_STOP_FILTERING_DEVICE      _IOW(FLT_IOCTL, STOP_FILTER_CMD, VOLUME_GUID)
#define IOCTL_INMAGE_START_FILTERING_DEVICE       _IOW(FLT_IOCTL, START_FILTER_CMD, VOLUME_GUID)
/* A temp hack. Maximum structure size that could be passed through _IO i/f is 16K whereas
 * UDIRTY_BLOCK is 20K, hence we just pass VolumeGuid as structure only for compilation pruposes,
 * In the kernel, necessary checks are done prior to accessing the user space UDIRTY block.
 */ 
#define IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS    	_IOWR(FLT_IOCTL, GET_DB_CMD, VOLUME_GUID)
#define IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS  _IOW(FLT_IOCTL, COMMIT_DB_CMD, COMMIT_TRANSACTION)
#define IOCTL_INMAGE_SET_VOLUME_FLAGS        	_IOW(FLT_IOCTL, SET_VOL_FLAGS_CMD, VOLUME_FLAGS_INPUT)
#define IOCTL_INMAGE_GET_VOLUME_FLAGS        	_IOR(FLT_IOCTL, GET_VOL_FLAGS_CMD, VOLUME_FLAGS_INPUT)
#define IOCTL_INMAGE_WAIT_FOR_DB        	_IOW(FLT_IOCTL, WAIT_FOR_DB_CMD, WAIT_FOR_DB_NOTIFY)
#define IOCTL_INMAGE_CLEAR_DIFFERENTIALS    	_IOW(FLT_IOCTL, CLEAR_DIFFS_CMD, VOLUME_GUID)
#define IOCTL_INMAGE_GET_NANOSECOND_TIME    	_IOWR(FLT_IOCTL, GET_TIME_CMD, long long)
#define IOCTL_INMAGE_UNSTACK_ALL 		_IO(FLT_IOCTL, UNSTACK_ALL_CMD)
#define IOCTL_INMAGE_SYS_SHUTDOWN       	_IOW(FLT_IOCTL, SYS_SHUTDOWN_NOTIFY_CMD, SYS_SHUTDOWN_NOTIFY_INPUT)
#define IOCTL_INMAGE_TAG_VOLUME			_IOWR(FLT_IOCTL, TAG_CMD, unsigned long)
#define IOCTL_INMAGE_WAKEUP_ALL_THREADS     	_IO(FLT_IOCTL, WAKEUP_THREADS_CMD)
#define IOCTL_INMAGE_GET_DB_NOTIFY_THRESHOLD	_IOWR(FLT_IOCTL, GET_DB_THRESHOLD_CMD, get_db_thres_t )
#define IOCTL_INMAGE_RESYNC_START_NOTIFICATION	_IOWR(FLT_IOCTL, RESYNC_START_CMD, RESYNC_START )
#define IOCTL_INMAGE_RESYNC_END_NOTIFICATION	_IOWR(FLT_IOCTL, RESYNC_END_CMD, RESYNC_END)
#define IOCTL_INMAGE_GET_DRIVER_VERSION  	_IOWR(FLT_IOCTL, GET_DRIVER_VER_CMD, DRIVER_VERSION)
#define IOCTL_INMAGE_SHELL_LOG    		_IOWR(FLT_IOCTL, GET_SHELL_LOG_CMD, char *)
#define IOCTL_INMAGE_AT_LUN_CREATE          _IOW(FLT_IOCTL,  AT_LUN_CREATE_CMD, LUN_CREATE_INPUT)
#define IOCTL_INMAGE_AT_LUN_DELETE          _IOW(FLT_IOCTL,  AT_LUN_DELETE_CMD, LUN_DELETE_INPUT)
#define IOCTL_INMAGE_AT_LUN_LAST_WRITE_VI   _IOWR(FLT_IOCTL, AT_LUN_LAST_WRITE_VI_CMD, \
                                                  AT_LUN_LAST_WRITE_VI)
#define IOCTL_INMAGE_AT_LUN_QUERY           _IOWR(FLT_IOCTL, AT_LUN_QUERY_CMD, LUN_QUERY_DATA)
/* Error numbers logged to registry when volume is out of sync */
#define ERROR_TO_REG_BITMAP_READ_ERROR                      0x0002
#define ERROR_TO_REG_BITMAP_WRITE_ERROR                     0x0003
#define ERROR_TO_REG_BITMAP_OPEN_ERROR                      0x0004
#define ERROR_TO_REG_BITMAP_OPEN_FAIL_CHANGES_LOST          (0x0005)
#define ERROR_TO_REG_DESCRIPTION_IN_EVENT_LOG               0x0006
#define ERROR_TO_REG_MAX_ERROR                              ERROR_TO_REG_DESCRIPTION_IN_EVENT_LOG

#endif // ifndef INVOLFLT_H
