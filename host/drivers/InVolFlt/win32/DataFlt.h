#ifndef _INVOLFLT_DRIVER_DATA_FILTERING_H__
#define _INVOLFLT_DRIVER_DATA_FILTERING_H__

#define DATABLOCK_FLAGS_LOCKED                  0x00000001
#define DATABLOCK_FLAGS_MAPPED                  0x00000002

typedef struct _DATA_BLOCK
{
    LIST_ENTRY  ListEntry;
    PUCHAR      KernelAddress;
    PUCHAR      MappedUserAddress;
    PMDL        Mdl;
    HANDLE      ProcessId;
    HANDLE      VolumeId;
    // This is used to store the virtual address buffer allocated to store
    // mapped user addresses. This is stored here so that buffers that are
    // mapped and have to be freed in arbitrary process context will be
    // stored in orphaned data block list.
    PVOID       *pUserBufferArray;
    SIZE_T      szcbBufferArraySize;
    ULONG       ulFlags;
    ULONG       ulcbDataFree;   // This is cb including trailer.
    ULONG       ulcbMaxData;
    PUCHAR      CurrentPosition;
} DATA_BLOCK, *PDATA_BLOCK;

struct _CHANGE_NODE;

bool
QueueWorkerRoutineForChangeNodeCleanup(
    struct _CHANGE_NODE        *ChangeNode
    );

NTSTATUS
WriteDispatchInDataFilteringMode(
    PDEVICE_OBJECT  DeviceObject,
    PIRP            Irp
    );

VOID
FreeOrphanedMappedDataBlockList();

VOID
FreeOrphanedMappedDataBlocksWithVolumeID(HANDLE VolumeId, HANDLE ProcessId);

NTSTATUS
PrepareDirtyBlockForUsermodeUse(struct _KDIRTY_BLOCK *DirtyBlock, HANDLE VolumeId);

NTSTATUS
PrepareDirtyBlockFor32bitUsermodeUse(struct _KDIRTY_BLOCK *DirtyBlock, HANDLE VolumeId);

void
FinalizeDataFileStreamInDirtyBlock(struct _KDIRTY_BLOCK *DirtyBlock);

ULONG
CopyDataToDirtyBlock(
    struct _KDIRTY_BLOCK    *DirtyBlock,
    PUCHAR                  pSource,
    ULONG                   ulLength
    );

void
ReserveDataBytes(
    struct _VOLUME_CONTEXT *VolumeContext,
    bool            bCanRetry,
    ULONG           &ulBytes,
    bool            bFirstTime,
    bool            bLockAcquired = false,
    bool            btag = false
    );

ULONG
IncrementCurrentPosition(
    struct _KDIRTY_BLOCK    *DirtyBlock,
    ULONG           ulIncrement
    );

struct _KDIRTY_BLOCK *
GetDirtyBlockToCopyChange(
    struct _VOLUME_CONTEXT   *VolumeContext,
    ULONG               ulLength,
    bool                bTags = false,
    PTIME_STAMP_TAG_V2  *ppTimeStamp = NULL,
    PLARGE_INTEGER      pliTickCount = NULL,
    PWRITE_META_DATA    pWriteMetaData = NULL
    );

/*-------------------------------------------------------------------------------------------------
 * DATA_BLOCK related functions
 *-------------------------------------------------------------------------------------------------
 */
PDATA_BLOCK
AllocateDataBlock();

VOID
DeallocateDataBlock(PDATA_BLOCK DataBlock, bool locked = false);

VOID
UnMapDataBlock(PDATA_BLOCK DataBlock);

NTSTATUS
LockDataBlock(PDATA_BLOCK DataBlock);

VOID
UnLockDataBlock(PDATA_BLOCK DataBlock);

PDATA_BLOCK
AllocateLockedDataBlock(BOOLEAN CanLockDataBlock);

VOID
AddDataBlockToLockedDataBlockList(PDATA_BLOCK DataBlock);

/*-------------------------------------------------------------------------------------------------
 * DATA_POOL related functions
 *-------------------------------------------------------------------------------------------------
 */
NTSTATUS
AllocateAndInitializeDataPool();

NTSTATUS SetDataPool(bool bInitialAlloc, ULONGLONG ullDataPoolSize);
	
NTSTATUS
UpdateLockedDataBlockList(ULONG ulDataBlocks);

NTSTATUS
AddNewDataBlockToLockedDataBlockList();

VOID
FreeLockedDataBlockList();

VOID
FreeFreeDataBlockList(bool);

VOID
CleanDataPool();

#endif // _INVOLFLT_DRIVER_DATA_FILTERING_H__
