#pragma once

#include "Common.h"
#include "WriteMetaData.h"
#include "DataFlt.h"
#include "ChangeNode.h"

struct _DEV_CONTEXT;

// Windows 2003 system uses 8 bytes for keeping tag information. So allocating 2040 bytes
// would alloate 2048 i.e. 2K.
// 168(changes) * 12(Offset and length) + 24 (header size) = 2040
// 338(changes) * 12(Offset and length) + 24 (header size) = 4080 leaves 8 bytes that is not used
// 100(changes) * 20(offset, length, time delta, seq delta) + 40(header size) = 2040
#define CB_SIZE_FOR_2K_CB       2040
#define MAX_CHANGES_IN_2K_CB    100
//#define MAX_CHANGES_IN_4K_CB    338

/*
0                                                                                                         2040
-----------------------------------------------------------------
|Header    |ChangeLength  |ChangeOffset  |TimeDelta        |SequenceDelta |
|40 bytes  |(100*4) bytes  |(100*8) bytes  | (100*4) bytes  |(100*4) bytes  |
-----------------------------------------------------------------
*/
typedef struct _CHANGE_BLOCK {
    struct _CHANGE_BLOCK    *Next;  // 0x00
    LONGLONG    *ChangeOffset;      // 0x08: This points into blob where Offset Array starts
    USHORT      usFirstIndex;       // 0x10
    USHORT      usLastIndex;        // 0x12
    USHORT      usMaxChanges;       // 0x14
    USHORT      usBlockSize;        // 0x16
    ULONG       *TimeDelta;         // 0x18 This points into blob where TimeDelta Array starts
    ULONG       *SequenceDelta;     // 0x20 This points into blob where SequenceDelta Array starts
    ULONG       ChangeLength[1];    // 0x28
} CHANGE_BLOCK, *PCHANGE_BLOCK;

#define KDIRTY_BLOCK_FLAG_START_OF_SPLIT_CHANGE 0x00000001
#define KDIRTY_BLOCK_FLAG_PART_OF_SPLIT_CHANGE  0x00000002
#define KDIRTY_BLOCK_FLAG_END_OF_SPLIT_CHANGE   0x00000004
#define KDIRTY_BLOCK_FLAG_SPLIT_CHANGE_MASK     (KDIRTY_BLOCK_FLAG_START_OF_SPLIT_CHANGE | KDIRTY_BLOCK_FLAG_PART_OF_SPLIT_CHANGE | KDIRTY_BLOCK_FLAG_END_OF_SPLIT_CHANGE)

#define KDIRTY_BLOCK_FLAG_CONTAINS_TAGS             0x00000010
#define KDIRTY_BLOCK_FLAG_TAG_WAITING_FOR_COMMIT    0x00000020
#define KDIRTY_BLOCK_FLAG_TAGS_PENDING              0x00000040
#define KDIRTY_BLOCK_FLAG_REVOKE_TAGS               0x00000080
#define KDIRTY_BLOCK_FLAG_TAG_NOTIFY                0x00000100
#define KDIRTY_BLOCK_FLAG_NO_COMMIT_TAGS_MASK    (KDIRTY_BLOCK_FLAG_TAGS_PENDING | KDIRTY_BLOCK_FLAG_REVOKE_TAGS)


#define KDIRTY_BLOCK_FLAG_PREPARED_FOR_USERMODE     0x80000000
#define KDIRTY_BLOCK_FLAG_FINALIZED_DATA_STREAM     0x40000000

#define MAX_KDIRTY_CHANGES      1024

typedef struct _TAG_DISK_REPLICATION_STATS {
        ULONGLONG  ullTotalChangesPending;
        ULONGLONG  ullTrackedBytes;
        ULONGLONG  ullDrainDBCount;
        ULONGLONG  ullDrainedDataInBytes;
        ULONGLONG  ullCommitDBCount;
        ULONGLONG  ullRevertedDBCount;
        ULONGLONG  NwLatBckt1; // <=150ms
        ULONGLONG  NwLatBckt2; // <=250ms
        ULONGLONG  NwLatBckt3; // <=500ms
        ULONGLONG  NwLatBckt4; // <=1sec
        ULONGLONG  NwLatBckt5; // > 1sec
        ULONGLONG  ullCommitDBFailCount;
        ULONGLONG  LossInGetDbGtr4K;
        ULONGLONG  ullTotalCommitLatencyInDM;
}TAG_DISK_REPLICATION_STATS, *PTAG_DISK_REPLICATION_STATS;
typedef enum _etTagType
{
    ecTagNone = 0,
    ecTagLocalCrash,
    ecTagDistributedCrash,
    ecTagLocalApp,
    ecTagDistributedApp,
    ecTagBaselineApp
} etTagType;
typedef struct _TAG_HISTORY {
    etTagType                   TagType;
    LARGE_INTEGER               liTagRequestTime;
    LARGE_INTEGER               liTagInsertSystemTimeStamp;
    LARGE_INTEGER               liLastTagInsertSystemTime;
    LARGE_INTEGER               liLastSuccessInsertSystemTime;
    CHAR                        TagMarkerGUID[GUID_SIZE_IN_CHARS + 1];
    USHORT                      usTotalNumDisk;// total number of disks known to Driver
    USHORT                      usNumProtectedDisk; // total number of protected disks by Driver
    ULONG                       ulNumberOfDevicesTagsApplied; //Number of tags inserted
    ULONG                       ulIoCtlCode;
    NTSTATUS                    TagStatus;// Tag Status either Per-Disk status or Overall IOCTL Status
    ULONGLONG                   ullBlendedState; // varius reasons about disk, tag IOCTL, remove device
    ULONGLONG                   ullTagState;// CommitDB/Dropped/Revoked/BarrierFail/Insert Failure/PreCheckInsertFailures
    LARGE_INTEGER               liTagCompleteTimeStamp; // Tag commit or revoke timestamp
    LARGE_INTEGER               FirstGetDbTimeOnDrainBlk;
    TAG_DISK_REPLICATION_STATS  TagInsertSuccessStats; // successful tag insert stats
    TAG_DISK_REPLICATION_STATS  PrevTagInsertStats; // Irrespective of success or failure to insert the tag, always carry previous
    TAG_DISK_REPLICATION_STATS  CurrentTagInsertStats;
}TAG_HISTORY, *PTAG_HISTORY;

typedef struct _KDIRTY_BLOCK
{
    PCHANGE_NODE    ChangeNode;             // 0x00:32bit   0x00:64bit
    _DEV_CONTEXT    *DevContext;            // 0x04:32bit   0x08:64bit
    ULARGE_INTEGER  uliTransactionID;       // 0x08:32bit   0x10:64bit
    ULARGE_INTEGER  ulicbChanges;           // 0x10:32bit   0x18:64bit
    ULONG           cChanges;               // 0x18:32bit   0x20:64bit
    ULONG           ulFlags;                // 0x1C:32bit   0x24:64bit
    // ulSequenceIdForSplitIO starts at one for the first dirty block in sequence and then increments further.
    ULONG           ulSequenceIdForSplitIO; // 0x20:32bit   0x28:64bit
    //ulTagBytesComplete does not contain the size of TagEndOfList tag as it is considered to be a floating tag.
    //But while calculating the space for thenew tag to be inserted, it is taken into account.
    ULONG           ulDataBlocksAllocated;  // 0x24:32bit   0x2C:64bit            
    LIST_ENTRY      DataBlockList;          // 0x28:32bit   0x30:64bit
    PDATA_BLOCK     CurrentDataBlock;       // 0x30:32bit   0x40:64bit
    ULONG           ulcbDataUsed;           // 0x34:32bit   0x48:64bit
    ULONG           ulcbDataFree;           // 0x38:32bit   0x4C:64bit
    etWriteOrderState eWOState;
    UNICODE_STRING  FileName; // This needs to have non paged memory. 0x3C:32bit 0x50:64bit
    ULONGLONG       ullFileSize;            // 0x44:32bit   0x60:64bit
    ULONGLONG       ullDataChangesSize; // This includes the SVD_PREFIX + SVD_DIRTY_BLOCK + data of each
                                        // change in the dirty block. 0x4C:32bit    0x68:64bit
    PUCHAR          SVDFirstChangeTimeStamp;    // 0x54:32bit   0x70:64bit
    PUCHAR          SVDcbChangesTag;    // Points to SVD_TAG_LENGTH_OF_DRTD_CHANGES 0x58:32bit 0x78:64bit
    PUCHAR          SVDLastChangeTimeStamp; // 0x5C:32bit   0x80:64bit
    LARGE_INTEGER   liTickCountAtFirstIO;   // 0x60:32bit   0x88:64bit
    LARGE_INTEGER   liTickCountAtLastIO;    // 0x68:32bit   0x90:64bit
    LARGE_INTEGER   liTickCountAtUserRequest;   // 0x70:32bit 0x98:64bit
    PUCHAR          TagBuffer;              // 0x78:32bit   0xA0:64bit
    ULONG           ulTagBufferSize;        // 0x7C:32bit   0xA8:64bit
    ULONG           ulDataSource;           // 0x80:32bit   0xAC:64bit
    TIME_STAMP_TAG_V2  TagTimeStampOfFirstChange;  // 0x84:32bit   0xB0:64bit
    TIME_STAMP_TAG_V2  TagTimeStampOfLastChange;   // 0x94:32bit   0xC8:64bit
    PCHANGE_BLOCK   ChangeBlockList;            // 0xA4:32bit   0xE0:64bit
    PCHANGE_BLOCK   CurrentChangeBlock;         // 0xA8:32bit   0xE8:64bit
    GUID            TagGUID;                    // 0xAC:32bit   0xF0:64bit
    ULONG           ulTagDevIndex;           // 0xBC:32bit   0x100:64bit
    ULONG           ulMaxDataSizePerDirtyBlock; // 0xC0:32bit   0x104:64bit


#define MAX_CHANGES_EMB_IN_DB       24
    ULONG           ChangeLength[MAX_CHANGES_EMB_IN_DB]; // Size is 0x60
    LONGLONG        ChangeOffset[MAX_CHANGES_EMB_IN_DB]; // Size is 0xC0
    ULONG           TimeDelta[MAX_CHANGES_EMB_IN_DB];    // Size is 0x60
    ULONG           SequenceDelta[MAX_CHANGES_EMB_IN_DB];// Size is 0x60
    PTAG_HISTORY    pTagHistory;
} KDIRTY_BLOCK, *PKDIRTY_BLOCK;

struct _DEV_BITMAP;

NTSTATUS
StopFilteringDevice(
    _DEV_CONTEXT      *pDevContext,
    bool              bLockAcquired,
    bool              bClearBitmapFile = false,
    bool              bCloseBitmapFile = false,
    _DEV_BITMAP       **ppDevBitmap = NULL
    );

NTSTATUS
StartFilteringDevice(
    _DEV_CONTEXT*   DevContext
);

NTSTATUS
StartFilteringDevice(
    _DEV_CONTEXT        *DevContext,
    bool                bLockAcquired,
    bool                bUser = false
    );

NTSTATUS
IsDiskClustered(
    __in   _DEV_CONTEXT* pDevContext,
    __out  bool* isClustered,
    __out  bool* isOnline
);

NTSTATUS
SetDiskClusterState(__in   _DEV_CONTEXT* pDevContext);


void
AddResyncRequiredFlag(
    PUDIRTY_BLOCK_V2 UserDirtyBlock,
    _DEV_CONTEXT *DevContext
    );

PKDIRTY_BLOCK
AllocateDirtyBlock(_DEV_CONTEXT *DevContext, ULONG ulMaxDataSizePerDirtyBlock);

VOID
DeallocateDirtyBlock(PKDIRTY_BLOCK pDirtyBlock, bool bVCLockAcquired);

PKDIRTY_BLOCK
GetNextDirtyBlock(PKDIRTY_BLOCK DirtyBlock, _DEV_CONTEXT *DevContext);

bool
ReserveSpaceForChangeInDirtyBlock(PKDIRTY_BLOCK DirtyBlock);

NTSTATUS
AddChangeToDirtyBlock(PKDIRTY_BLOCK DirtyBlock, 
    LONGLONG Offset, 
    ULONG Length,
    ULONG TimeDelta,
    ULONG SequenceDelta,
    ULONG uDataSource,
    BOOLEAN &Coalesced);

void //Added for per io time stamp changes
CopyChangeMetaData(PKDIRTY_BLOCK DirtyBlock, 
    PULONGLONG OffsetArray, 
    PULONG LengthArray,
    ULONG ulMaxElements,
    PULONG TimeDeltaArray,
    PULONG SequenceDeltaArray);

NTSTATUS
CommitDirtyBlockTransaction(
    _DEV_CONTEXT        *pDevContext,
    PCOMMIT_TRANSACTION pCommitTransaction
    );

VOID
OrphanMappedDataBlocksAndMoveCommitPendingDirtyBlockToMainQueue(
    _DEV_CONTEXT     *pDevContext
    );

ULONG
SetSVDLastChangeTimeStamp(PKDIRTY_BLOCK DirtyBlock);

VOID
DBFreeDataBlocks(PKDIRTY_BLOCK DirtyBlock, bool bVCLockAcquired);

ULONG AddMetaDataToDirtyBlock(_DEV_CONTEXT *DevContext, _KDIRTY_BLOCK *CurrentDirtyBlock, _WRITE_META_DATA *pWriteMetaData, ULONG uDataSource);
ULONG SplitChangeIntoDirtyBlock(_DEV_CONTEXT *DevContext, _WRITE_META_DATA *pWriteMetaData, ULONG uDataSource, ULONG MaxDataSizePerDirtyBlock);

