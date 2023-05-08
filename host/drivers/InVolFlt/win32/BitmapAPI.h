
#ifndef BitmapAPI_INCLUDED
#define BitmapAPI_INCLUDED

#include "WriteMetaData.h"
#include "Common.h"
#include "global.h"
#include "VContext.h"

// thse are flags used in the bitmap file header
#define BITMAP_FILE_VERSION (0x00010004)
#define BITMAP_FILE_VERSION_V2 (0x00010005)// This version to represent addition of new persistent values to tLogHeader
#define BITMAP_FILE_VERSION_HEADER_SIZE_INCREASE  (8 * sizeof(UINT32) + 7 * sizeof(UINT64))
#define BITMAP_FILE_ENDIAN_FLAG (0x000000FF)
#define BITMAP_HEADER_OOD_SUPPORT 0x00000001 // Indicates the newly added fields to support OOD solution

//flag to store the filtering state in bitmap header
#define BITMAP_FLAGS_FILTERING_STATE_STOPPED           0x00000001
//flag to store the resync required flag in case of cluster volume
#define BITMAP_RESYNC_REQUIRED                             0x00000002

#define MAX_WRITE_GROUPS_IN_BITMAP_HEADER (31)
#define MAX_CHANGES_IN_WRITE_GROUP (64)
#define DISK_SECTOR_SIZE (512)
#define HEADER_CHECKSUM_SIZE (16)
#define HEADER_CHECKSUM_DATA_SIZE (sizeof(tBitmapHeader) - HEADER_CHECKSUM_SIZE)
#define LOG_HEADER_SIZE ((DISK_SECTOR_SIZE * MAX_WRITE_GROUPS_IN_BITMAP_HEADER) + DISK_SECTOR_SIZE)
#define LOG_HEADER_OFFSET (LOG_HEADER_SIZE)

enum tagLogRecoveryState {
    uninitialized = 0,          // new bitmap file
    cleanShutdown = 1,          // no changes lost
    dirtyShutdown = 2,          // system shutdown before changes written (i.e. crash)
    lostSync = 3};              // lost sync previously and not yet reset


typedef struct {
    UCHAR validationChecksum[HEADER_CHECKSUM_SIZE]; // 0x00: this is read first to verify it's our sector, part 
                                                    //       of physical writing carefully checksum calculation starts 
                                                    //       HERE and is sizeof(tBitmapHeader) - HEADER_CHECKSUM_SIZE bytes long 
    UINT32 endian;                                  // 0x10: read as 0xFF is little endian and 0xFF000000 means wrong endian
    UINT32 headerSize;                              // 0x14:
    UINT32 version;                                 // 0x18: 0x10000 initial version 0x18
    UINT32 dataOffset;                              // 0x1C: where the bits start 0x1C
    UINT64 bitmapOffset;                            // 0x20: location of this structure on volume/file, integrity check
    UINT64 bitmapSize;                              // 0x28:
    UINT64 bitmapGranularity;                       // 0x30:
    LONGLONG volumeSize;                            // 0x38:
    enum tagLogRecoveryState recoveryState;         // 0x40: did the system crash?
    UINT32 lastChanceChanges;                       // 0x44: how many changes are there in the last chance area
    UINT32 bootCycles;                              // 0x48: how many times have we opened this bitmap file
    UINT32 changesLost;                             // 0x4C: how many changes didn't get saved
    UINT64 LastTimeStamp;                           // 0x50: added to persist Global Time Stamp per volume (V2)
    UINT64 LastSequenceNumber;                      // 0x58: added to persist Global sequence No per volume (V2)
    UINT32 ControlFlags;                            // 0x60: Now this flag is used to hold the current filtering state (V2)
    UINT32 FieldFlags;                              // 0x64: This is used to make out that particular field is present in the 
                                                    //       header or not. It will save changing the header versions. (V2)
    UINT64 ullOutOfSyncTimeStamp;                   // 0x68:
    UINT64 ullOutOfSyncIndicatedTimeStamp;          // 0x70:
    UINT64 ullOutOfSyncResetTimeStamp;              // 0x78:
    UINT32 ulOutOfSyncIndicatedAtCount;             // 0x80:
    UINT32 ulOutOfSyncCount;                        // 0x84:
    UINT32 ulOutOfSyncErrorCode;                    // 0x88:
    UINT32 ulOutOfSyncErrorStatus;                  // 0x8c:
    UINT64 PrevEndTimeStamp;                        // 0x90
    UINT32 PrevSequenceId;                          // 0x98
    UINT32 ulReserved;                              // 0x9C
    UINT64 PrevEndSequenceNumber;                   // 0xA0
} tLogHeader;                                       // 0xA8

typedef struct {
    union {
        UINT64 lengthOffsetPair[MAX_CHANGES_IN_WRITE_GROUP]; // low 48-bits is offset / 512, upper 16-bits is size / 512
        UCHAR sectorFill[DISK_SECTOR_SIZE];
    };
} tLastChanceChanges;

typedef struct {
    union {
        tLogHeader header; // this get's padded to a sector size be taking the same space as below
        UCHAR sectorFill[DISK_SECTOR_SIZE];
    };
    tLastChanceChanges changeGroup[MAX_WRITE_GROUPS_IN_BITMAP_HEADER];
} tBitmapHeader;

class BitRunLengthOffsetArray;

class BitRunLengthOffsetArray {
public:
    LONGLONG    runOffsetArray[MAX_BITRUN_CHANGES];
    ULONG       runLengthArray[MAX_BITRUN_CHANGES];
};

class BitRuns;

class BitRuns {
public:
    NTSTATUS finalStatus; // return Status
    ULONG nbrRunsProcessed; // on failure, how far did it get
    PVOID context1; // caller context
    PVOID context2; // caller context
    void (*completionCallback)(class BitRuns * runs); // will call on completion
    ULONG nbrRuns; // input size
    // bit runs
    LONGLONG    *runOffsetArray;
    ULONG       *runLengthArray;
    PKDIRTY_BLOCK pdirtyBlock;
    BitRunLengthOffsetArray *pBitRunLengthOffsetArray;
};

typedef enum _etBitmapFileState {
    ecBitmapStateUnInitialized = 0,
    ecBitmapStateOpened = 1,
    ecBitmapStateRawIO = 2,
    ecBitmapStateClosed = 3
} etBitmapFileState;

class BitmapAPI : public BaseClass {
public:
    BitmapAPI();
    static NTSTATUS InitializeBitmapAPI();
    static NTSTATUS TerminateBitmapAPI();
    NTSTATUS Open(  IN  PUNICODE_STRING pBitmapFileName,
                    IN  ULONG   ulBitmapGranularity,
                    IN  ULONG   ulBitmapOffset,
                    IN  LONGLONG volumeSize,
                    IN  WCHAR   DriveLetter,
                    ULONG   SegmentCacheLimit,
                    bool    bClearOnOpen,
                    BitmapPersistentValues &BitmapPersistentValue,
                    OUT NTSTATUS * detailStatus);
    NTSTATUS Close( BitmapPersistentValues &BitmapPersistentValue, bool bDelete = false, NTSTATUS *pInMageCloseStatus = NULL);
    NTSTATUS SetBitRun(BitRuns * bitRuns, LONGLONG runOffsetArray, ULONG runLengthArray);
    NTSTATUS SetBits(BitRuns * bitRuns);
    NTSTATUS ClearBits(BitRuns * bitRuns);
    NTSTATUS ClearAllBits();
    NTSTATUS GetFirstRuns(BitRuns * bitRuns);
    NTSTATUS GetNextRuns(BitRuns * bitRuns);
    ULONG64  GetDatBytesInBitmap();
    NTSTATUS FlushTimeAndSequence(ULONGLONG TimeStamp, 
                                  ULONGLONG SequenceNo, 
                                  bool IsCommitNeeded = false);
    NTSTATUS DeleteLog();
    NTSTATUS AddResynRequiredToBitmap(ULONG     ulOutOfSyncErrorCode, 
                                      ULONG     ulOutOfSyncErrorStatus ,
                                      ULONGLONG ullOutOfSyncTimeStamp, 
                                      ULONG     ulOutOfSyncCount,
                                      ULONG     ulOutOfSyncIndicatedAtCount);
    NTSTATUS ResetResynRequiredInBitmap(ULONG     ulOutOfSyncCount,
                                        ULONG     ulOutOfSyncIndicatedAtCount,
                                        ULONGLONG ullOutOfSyncIndicatedTimeStamp,
                                        ULONGLONG ullOutOfSyncResetTimeStamp,
                                        bool      ResetResyncRequired);


    NTSTATUS SaveWriteMetaDataToBitmap(PWRITE_META_DATA pWriteMetaData);
    NTSTATUS ChangeBitmapModeToRawIO();
    void     ResetPhysicalDO();
    NTSTATUS CommitBitmap(BitmapPersistentValues &BitmapPersistentValue, NTSTATUS *pInMageCloseStatus = NULL);
    NTSTATUS IsVolumeInSync(BOOLEAN *pVolumeInSync, NTSTATUS *pOutOfSyncErrorCode);
    BOOLEAN  IsBitmapClosed();
    BOOLEAN  IsBitmapFileCreatedNew() 
    { 
        return m_bNewBitmapFile;
    }

    bool IsFilteringStopped()
    {
        if (m_BitmapHeader.header.ControlFlags & BITMAP_FLAGS_FILTERING_STATE_STOPPED) {
            return true;
        } else {
            return false;
        }
    }

    void SetFilteringStarted()
    {
        m_BitmapHeader.header.ControlFlags &= ~BITMAP_FLAGS_FILTERING_STATE_STOPPED;
    }

    void SetFilteringStopped()
    {
        m_BitmapHeader.header.ControlFlags |= BITMAP_FLAGS_FILTERING_STATE_STOPPED;
    }

    VOID SaveFilteringStateToBitmap(bool FilteringStopped, bool IsCleanShutdownNeeded, bool IsCommitNeeded);
    unsigned long long GetCacheHit()
    {
    	return segmentedBitmap->GetCacheHit();
    }
    unsigned long long GetCacheMiss()
    {
    	return segmentedBitmap->GetCacheMiss();
    }

public:
	//Bug 27337
	FSINFORMATION  *pFsInfo; //Let this be public so that class object can access it.
		
protected:
    // Stores the state of the bitmap.
    etBitmapFileState   m_eBitmapState;
    KMUTEX          m_BitmapMutex;   // multiple threads would conflict in bitmap access
    
    // Status values for debugging purposes
    BOOLEAN m_bCorruptBitmapFile;
    BOOLEAN m_bEmptyBitmapFile;
    BOOLEAN m_bNewBitmapFile;
    BOOLEAN m_bVolumeInSync;
    NTSTATUS    m_ErrorCausingOutOfSync;

    class SegmentedBitmap *segmentedBitmap;
    class FileStream *m_fsBitmapStream;
    class FileStreamSegmentMapper *segmentMapper;

    // Used for logging purposes.
    WCHAR   m_wcDriveLetter;

    // Bitmap parameters that decide the granularity and size of bitmap.
    LONGLONG    m_llVolumeSize;     // Size of the volume represented by this bitmap
    ULONG       m_ulBitmapGranularity;
    ULONG       m_ulBitmapOffset;
    ULONG       m_ulNumberOfBitsInBitmap;   // number of bits in bitmap file representing volume data
    ULONG       m_ulBitMapSizeInBytes;      // Bitmap size rounded to BITMAP_FILE_SEGMENT_SIZE also includes BITMAP_HEADER
                                            // This does not included the BitmapOffset. So the file size coule be differed
                                            // If m_ulBitmapOffset is non zero.
    
    UNICODE_STRING  m_ustrBitmapFilename;
    WCHAR           m_wstrBitmapFilenameBuffer[MAX_LOG_PATHNAME + 1];
    tBitmapHeader   m_BitmapHeader; // stored at offset 0 of section for this volume
    class IOBuffer * m_iobBitmapHeader;

    NTSTATUS FastZeroBitmap();
    void CalculateHeaderIntegrityChecksums(tBitmapHeader * header);
    bool VerifyHeader(tBitmapHeader * header, bool bUpdateOnUpgrade = false);

    // Returns Scaled Size and updates Scaled Offset parameter.
    void GetScaledOffsetAndSizeFromDiskChange(LONGLONG llOffset, ULONG ulLength, ULONGLONG *pullScaledOffset, ULONGLONG *pullScaledSize);
    void GetScaledOffsetAndSizeFromWriteMetadata(PWRITE_META_DATA pWriteMetaData, ULONGLONG *pullScaledOffset, ULONGLONG *pullScaledSize);
    NTSTATUS SetWriteSizeNotToExceedVolumeSize(LONGLONG &llOffset, ULONG &ulLength);
	//Fix For Bug 27337
	NTSTATUS SetUserReadSizeNotToExceedFsSize(LONGLONG &llOffset, ULONG &ulLength);
    ULONG   m_SegmentCacheLimit;
private:
    // These routines are called with mutex already held. User can not call this routines.
    NTSTATUS CommitBitmapInternal(NTSTATUS *pInMageStatus, BitmapPersistentValues &BitmapPersistentValue);
    NTSTATUS CommitHeader(BOOLEAN bVerifyExistingHeaderInRawIO, NTSTATUS *pInMageStatus);
    NTSTATUS InitBitmapFile(NTSTATUS *pInMageStatus, BitmapPersistentValues &BitmapPersistentValue);
    NTSTATUS LoadBitmapHeaderFromFileStream(NTSTATUS *pInMageStatus, bool bClearOnOpen, BitmapPersistentValues &BitmapPersistentValue);
    NTSTATUS MoveRawIOChangesToBitmap(NTSTATUS  *pInMageOpenStatus);
    NTSTATUS ReadAndVerifyBitmapHeader(NTSTATUS *pInMageCloseStatus);

};

#endif