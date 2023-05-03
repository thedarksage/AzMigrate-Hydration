///
/// @file svdparse.h
///
/// Define interface to SV Delta files. Delta files are a list of chunks.
/// Each chunk begins with a four character tag,
/// a length, and then that many data bytes. 
///
#ifndef SVDPARSE__H
#define SVDPARSE__H

#include "svtypes.h"

#define BYTEMASK(ch)    ((ch) & (0xFF))
#define INMAGE_MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
                (BYTEMASK(ch0) | (BYTEMASK(ch1) << 8) |   \
                (BYTEMASK(ch2) << 16) | (BYTEMASK(ch3) << 24 ))



#define SVD_TAG_HEADER1                  INMAGE_MAKEFOURCC( 'S', 'V', 'D', '1' )
#define SVD_TAG_DIRTY_BLOCKS             INMAGE_MAKEFOURCC( 'D', 'I', 'R', 'T' )
#define SVD_TAG_DIRTY_BLOCK_DATA         INMAGE_MAKEFOURCC( 'D', 'R', 'T', 'D' )
#define SVD_TAG_SENTINEL_HEADER          INMAGE_MAKEFOURCC( 'S', 'E', 'N', 'T' )
#define SVD_TAG_SENTINEL_DIRT            INMAGE_MAKEFOURCC( 'S', 'D', 'R', 'T' )
#define SVD_TAG_SYNC_HASH_COMPARE_DATA   INMAGE_MAKEFOURCC( 'S', 'H', 'C', 'D' )
#define SVD_TAG_SYNC_DATA                INMAGE_MAKEFOURCC( 'S', 'D', 'A', 'T' )
#define SVD_TAG_SYNC_DATA_NEEDED_INFO    INMAGE_MAKEFOURCC( 'S', 'D', 'N', 'I' )
#define SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE  INMAGE_MAKEFOURCC( 'T', 'S', 'F', 'C' )
#define SVD_TAG_TIME_STAMP_OF_LAST_CHANGE   INMAGE_MAKEFOURCC( 'T', 'S', 'L', 'C' )
#define SVD_TAG_LENGTH_OF_DRTD_CHANGES      INMAGE_MAKEFOURCC( 'L', 'O', 'D', 'C' )
#define SVD_TAG_USER                     INMAGE_MAKEFOURCC( 'U', 'S', 'E', 'R' )

#pragma pack( push, 1 )
struct SVD_PREFIX
{
    SV_ULONG tag;
    SV_ULONG count;
    SV_ULONG Flags;
};

struct SVD_HEADER1
{
    SV_UCHAR  MD5Checksum[16];        // MD5 checksum of all data that follows this field
    SV_GUID   SVId;                   // Unique ID assigned by amethyst
    SV_GUID   OriginHost;             // Unique origin host id
    SV_GUID   OriginVolumeGroup;      // Unique origin vol group id
    SV_GUID   OriginVolume;           // Unique origin vol id
    SV_GUID   DestHost;               // Unique dest host id
    SV_GUID   DestVolumeGroup;        // Unique dest vol group id
    SV_GUID   DestVolume;             // Unique dest vol id
};

struct SVD_DIRTY_BLOCK
{
    SV_LONGLONG         Length;
    SV_LONGLONG         ByteOffset;
};

struct SVD_DIRTY_BLOCK_INFO
{
	struct SVD_DIRTY_BLOCK DirtyBlock;
	SV_ULONGLONG DataFileOffset;
};

struct SVD_BLOCK_CHECKSUM
{
    SV_LONGLONG         Length;
    SV_LONGLONG         ByteOffset;
    SV_UCHAR            MD5Checksum[16];  // MD5 checksum of all data that follows this field
};

/* Doesn't exist anymore. Just an array of SVD_DIRTY_BLOCK followed by Length data bytes. */
struct SVD_DIRTY_BLOCK_DATA
{
    SV_LONGLONG BlockCount;
};

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
    SV_USHORT      usStreamRecType;
    SV_UCHAR       ucFlags;
    SV_UCHAR       ucLength;
};

struct SVD_TIME_STAMP
{
    SVD_TIME_STAMP_HEADER   Header;
    SV_ULONG                ulSequenceNumber;
    SV_ULONGLONG            TimeInHundNanoSecondsFromJan1601;
};


//
// Raw sentinel dirt file header. Two blocks: SENT and SDRT
//
struct SENTINEL_DIRTYFILE_HEADER
{
	SV_ULONG tagSentinelHeader;
	SV_ULONG dwSize;
	SV_ULONG dwMajorVersion, dwMinorVersion;
	SV_ULONGLONG ullVolumeCapacity;
	SV_ULONG dwPageSize;
	SV_ULONG tagSentinelDirty;
	SV_ULONG dwDirtSize;
};
#pragma pack( pop )

#endif // SVDPARSE__H
