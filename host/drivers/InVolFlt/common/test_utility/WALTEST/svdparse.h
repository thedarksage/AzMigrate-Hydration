#include "inmtypes.h"

///
/// @file svdparse.h
///
/// Define interface to SV Delta files. Delta files are a list of chunks.
/// Each chunk begins with a four character tag,
/// a length, and then that many data bytes. 
///
#ifndef SVDPARSE__H
#define SVDPARSE__H

/* ***********************************************************************
 *        THIS FILE SHOULD BE REMOVES IN FUTURE. THIS FILE SHOULD BE COMMON
 *        FOR ALL FILTER DRIVERS IRRESPECTIVE OF OS.
 * ***********************************************************************
 */
 
#define BYTEMASK(ch)    ((ch) & (0xFF))
#define INMAGE_MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
                (BYTEMASK(ch0) | (BYTEMASK(ch1) << 8) |   \
                (BYTEMASK(ch2) << 16) | (BYTEMASK(ch3) << 24 ))

#define SVD_TAG_HEADER1                  INMAGE_MAKEFOURCC( 'S', 'V', 'D', '1' )
#define SVD_TAG_HEADER_V2                INMAGE_MAKEFOURCC( 'S', 'V', 'D', '2' )
#define SVD_TAG_DIRTY_BLOCKS             INMAGE_MAKEFOURCC( 'D', 'I', 'R', 'T' )
#define SVD_TAG_DIRTY_BLOCK_DATA         INMAGE_MAKEFOURCC( 'D', 'R', 'T', 'D' )
#define SVD_TAG_DIRTY_BLOCK_DATA_V2      INMAGE_MAKEFOURCC( 'D', 'D', 'V', '2' )
#define SVD_TAG_SENTINEL_HEADER          INMAGE_MAKEFOURCC( 'S', 'E', 'N', 'T' )
#define SVD_TAG_SENTINEL_DIRT            INMAGE_MAKEFOURCC( 'S', 'D', 'R', 'T' )
#define SVD_TAG_SYNC_HASH_COMPARE_DATA   INMAGE_MAKEFOURCC( 'S', 'H', 'C', 'D' )
#define SVD_TAG_SYNC_DATA                INMAGE_MAKEFOURCC( 'S', 'D', 'A', 'T' )
#define SVD_TAG_SYNC_DATA_NEEDED_INFO    INMAGE_MAKEFOURCC( 'S', 'D', 'N', 'I' )
#define SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE  INMAGE_MAKEFOURCC( 'T', 'S', 'F', 'C' )
#define SVD_TAG_TIME_STAMP_OF_LAST_CHANGE   INMAGE_MAKEFOURCC( 'T', 'S', 'L', 'C' )
#define SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE_V2  INMAGE_MAKEFOURCC( 'T', 'F', 'V', '2' )
#define SVD_TAG_TIME_STAMP_OF_LAST_CHANGE_V2   INMAGE_MAKEFOURCC( 'T', 'L', 'V', '2' )
#define SVD_TAG_LENGTH_OF_DRTD_CHANGES      INMAGE_MAKEFOURCC( 'L', 'O', 'D', 'C' )
#define SVD_TAG_USER                     INMAGE_MAKEFOURCC( 'U', 'S', 'E', 'R' )

#define SVD_MAJOR_VERSION 0x02
#define SVD_MINOR_VERSION 0x00

typedef struct tagGUID {
    unsigned int    Data1;
    unsigned short  Data2;
    unsigned short  Data3;
    unsigned char   Data4[8];
} SV_GUID;

#pragma pack(1) 

typedef struct 
{
    unsigned int tag;
    unsigned int count;
    unsigned int Flags;
}SVD_PREFIX;

typedef struct 
{
    unsigned char  MD5Checksum[16];        // MD5 checksum of all data that follows this field
    SV_GUID   SVId;                   // Unique ID assigned by amethyst
    SV_GUID   OriginHost;             // Unique origin host id
    SV_GUID   OriginVolumeGroup;      // Unique origin vol group id
    SV_GUID   OriginVolume;           // Unique origin vol id
    SV_GUID   DestHost;               // Unique dest host id
    SV_GUID   DestVolumeGroup;        // Unique dest vol group id
    SV_GUID   DestVolume;             // Unique dest vol id
} SVD_HEADER1;

/* SVD VERSION INFO */
typedef struct {
	sv_u32_t	Major;
	sv_u32_t	Minor;
} SVD_VERSION_INFO;

typedef struct 
{
    SVD_VERSION_INFO Version;
    unsigned char  MD5Checksum[16];        // MD5 checksum of all data that follows this field
    SV_GUID   SVId;                   // Unique ID assigned by amethyst
    SV_GUID   OriginHost;             // Unique origin host id
    SV_GUID   OriginVolumeGroup;      // Unique origin vol group id
    SV_GUID   OriginVolume;           // Unique origin vol id
    SV_GUID   DestHost;               // Unique dest host id
    SV_GUID   DestVolumeGroup;        // Unique dest vol group id
    SV_GUID   DestVolume;             // Unique dest vol id
} SVD_HEADER_V2;

typedef struct 
{
    long long         Length;
    long long         ByteOffset;
} SVD_DIRTY_BLOCK;

typedef struct 
{
    unsigned int      	Length;
    unsigned long long	ByteOffset;
    unsigned int      	uiSequenceNumberDelta;
    unsigned int      	uiTimeDelta;
} SVD_DIRTY_BLOCK_V2;

typedef struct 
{
    unsigned int      	Length;
    unsigned long long	ByteOffset;
    unsigned int      	uiSequenceNumberDelta;
    unsigned int      	uiTimeDelta;
} SVD_DIRTY_BLOCK_V3;

typedef struct 
{
    SVD_DIRTY_BLOCK DirtyBlock;
    unsigned long long DataFileOffset;
}SVD_DIRTY_BLOCK_INFO;

typedef struct 
{
    long long         Length;
    long long         ByteOffset;
    unsigned char     MD5Checksum[16];  // MD5 checksum of all data that follows this field
} SVD_BLOCK_CHECKSUM;

/* Doesn't exist anymore. Just an array of SVD_DIRTY_BLOCK followed by Length data bytes. */
typedef struct 
{
    long long BlockCount;
}SVD_DIRTY_BLOCK_DATA;

// FIXME:
// these are defined in invalflt.h, but should not be 
// as such it causes compile issue so for now
// redefine them here and make sure to keep in sync with driver 
// typedef struct _TIME_STAMP_TAG
//{
//    STREAM_REC_HDR_4B   Header;
//    ULONG       ulSequenceNumber;
//    ULONGLONG   TimeInHundNanoSecondsFromJan1601;
//} TIME_STAMP_TAG, *PTIME_STAMP_TAG;

struct SVD_TIME_STAMP_HEADER 
{
    unsigned short      usStreamRecType;
    unsigned char       ucFlags;
    unsigned char       ucLength;
};

typedef struct 
{
    struct SVD_TIME_STAMP_HEADER   Header;
    unsigned int             ulSequenceNumber;
    unsigned long long            TimeInHundNanoSecondsFromJan1601;
}SVD_TIME_STAMP;

typedef struct 
{
    struct SVD_TIME_STAMP_HEADER   Header;
    unsigned long long             ullSequenceNumber;
    unsigned long long             TimeInHundNanoSecondsFromJan1601;
}SVD_TIME_STAMP_V2;

//
// Raw sentinel dirt file header. Two blocks: SENT and SDRT
//
struct SENTINEL_DIRTYFILE_HEADER
{
    unsigned int tagSentinelHeader;
    unsigned int dwSize;
    unsigned int dwMajorVersion, dwMinorVersion;
    unsigned long long ullVolumeCapacity;
    unsigned int dwPageSize;
    unsigned int tagSentinelDirty;
    unsigned int dwDirtSize;
};
#pragma pack()

#endif // SVDPARSE__H

