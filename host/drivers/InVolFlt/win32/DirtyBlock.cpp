#include "Common.h"
#include "DirtyBlock.h"
#include "MntMgr.h"
#include "DriverContext.h"
#include "global.h"
#include "VBitmap.h"
#include "misc.h"
#include "VContext.h"
#include "TagNode.h"
#include "HelperFunctions.h"

/*-----------------------------------------------------------------------------
    Functions related to PCHANGE_BLOCK
 *-----------------------------------------------------------------------------
 */

#define COALESCE_ARRAY_LENGTH   10
ULONG CoalesceArray[10];

PCHANGE_BLOCK
AllocateChangeBlock(void)
{
    if (MaxNonPagedPoolLimitReached()) {
        InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, 
                      ("%s: Allocation failed as NonPagedPoolAlloc(%#x) reached MaxNonPagedLimit(%#x)\n", 
                       __FUNCTION__, DriverContext.lNonPagedMemoryAllocated, DriverContext.ulMaxNonPagedPool));
        return NULL;
    }

    PCHANGE_BLOCK   ChangeBlock = (PCHANGE_BLOCK)ExAllocateFromNPagedLookasideList(&DriverContext.ChangeBlock2KPool);
    if (NULL != ChangeBlock) {
        InterlockedIncrement(&DriverContext.lChangeBlocksAllocated);
        InterlockedExchangeAdd(&DriverContext.lNonPagedMemoryAllocated, CB_SIZE_FOR_2K_CB);
        ChangeBlock->Next = NULL;
        ChangeBlock->usMaxChanges = MAX_CHANGES_IN_2K_CB;
        ChangeBlock->ChangeOffset = (PLONGLONG)&ChangeBlock->ChangeLength[MAX_CHANGES_IN_2K_CB];
        ChangeBlock->TimeDelta = (PULONG)&ChangeBlock->ChangeOffset[MAX_CHANGES_IN_2K_CB];
        ChangeBlock->SequenceDelta = (PULONG)&ChangeBlock->TimeDelta[MAX_CHANGES_IN_2K_CB];
        ChangeBlock->usBlockSize = CB_SIZE_FOR_2K_CB;
        ASSERT((PUCHAR)&ChangeBlock->SequenceDelta[MAX_CHANGES_IN_2K_CB] <= (PUCHAR)ChangeBlock + CB_SIZE_FOR_2K_CB);
    }

    return ChangeBlock;
}

void
DeallocateChangeBlock(PCHANGE_BLOCK ChangeBlock)
{
    ASSERT(CB_SIZE_FOR_2K_CB == ChangeBlock->usBlockSize);

    ExFreeToNPagedLookasideList(&DriverContext.ChangeBlock2KPool, ChangeBlock);
    InterlockedDecrement(&DriverContext.lChangeBlocksAllocated);
    InterlockedExchangeAdd(&DriverContext.lNonPagedMemoryAllocated, -CB_SIZE_FOR_2K_CB);
    ASSERT(DriverContext.lNonPagedMemoryAllocated >= 0);
}

/*-------------------------------------------------------------------------------------------
    Functions related to PDIRTY_BLOCKS
 *-------------------------------------------------------------------------------------------
 */

PKDIRTY_BLOCK
AllocateDirtyBlock(PVOLUME_CONTEXT VolumeContext, ULONG ulMaxDataSizePerDirtyBlock)
{
    PKDIRTY_BLOCK   pDirtyBlock = NULL;

    InVolDbgPrint(DL_FUNC_TRACE, DM_DATA_FILTERING, ("%s: Called\n", __FUNCTION__));

    PCHANGE_NODE    ChangeNode = AllocateChangeNode();
    if (NULL == ChangeNode) {
        return NULL;
    }

    pDirtyBlock = (PKDIRTY_BLOCK)ExAllocateFromNPagedLookasideList(&DriverContext.DirtyBlocksPool);
    if (NULL == pDirtyBlock) {
        DereferenceChangeNode(ChangeNode);
        InVolDbgPrint(DL_WARNING, DM_DATA_FILTERING, ("%s: DiB Allocation failed\n", __FUNCTION__));
        return NULL;
    }

    ChangeNode->DirtyBlock = pDirtyBlock;
    ChangeNode->eChangeNode = ecChangeNodeDirtyBlock;

    pDirtyBlock->ChangeNode = ChangeNode;
    pDirtyBlock->VolumeContext = ReferenceVolumeContext(VolumeContext);
    pDirtyBlock->cChanges = 0;
    pDirtyBlock->ulicbChanges.QuadPart = 0;
    pDirtyBlock->ulFlags = 0;
    pDirtyBlock->uliTransactionID.QuadPart = 0;
    pDirtyBlock->ulSequenceIdForSplitIO = 1;
    InitializeListHead(&pDirtyBlock->DataBlockList);
    pDirtyBlock->CurrentDataBlock = NULL;
    pDirtyBlock->ulDataBlocksAllocated = 0;
    pDirtyBlock->ulcbDataUsed = 0;
    pDirtyBlock->ulcbDataFree = 0;
    pDirtyBlock->ullDataChangesSize = 0;
    pDirtyBlock->SVDFirstChangeTimeStamp = NULL;
    pDirtyBlock->SVDcbChangesTag = NULL;
    pDirtyBlock->SVDLastChangeTimeStamp = NULL;
    pDirtyBlock->ullFileSize = 0;
    RtlInitEmptyUnicodeString(&pDirtyBlock->FileName, NULL, 0);
    InterlockedIncrement(&DriverContext.lDirtyBlocksAllocated);
    InterlockedExchangeAdd(&DriverContext.lNonPagedMemoryAllocated, sizeof(KDIRTY_BLOCK));
    pDirtyBlock->ulDataSource = INVOLFLT_DATA_SOURCE_UNDEFINED;
    pDirtyBlock->eWOState = VolumeContext->eWOState;
    pDirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601 = 0;
    pDirtyBlock->TagTimeStampOfFirstChange.ullSequenceNumber = 0;
    pDirtyBlock->TagTimeStampOfLastChange.ullSequenceNumber = 0;
    pDirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601 = 0;
    pDirtyBlock->liTickCountAtFirstIO.QuadPart = 0;
    pDirtyBlock->liTickCountAtLastIO.QuadPart = 0;
    pDirtyBlock->liTickCountAtUserRequest.QuadPart = 0;
    pDirtyBlock->ChangeBlockList = NULL;
    pDirtyBlock->CurrentChangeBlock = NULL;
    pDirtyBlock->TagBuffer = NULL;
    pDirtyBlock->ulTagBufferSize = 0;
    pDirtyBlock->ulMaxDataSizePerDirtyBlock = ulMaxDataSizePerDirtyBlock;

    return pDirtyBlock;
}

VOID
DBFreeDataBlocks(PKDIRTY_BLOCK DirtyBlock, bool bVCLockAcquired)
{
    PVOLUME_CONTEXT VolumeContext = DirtyBlock->VolumeContext;
    ASSERT(NULL != VolumeContext);
    KIRQL   OldIrql;

    // Free Datablock list.
    while(!IsListEmpty(&DirtyBlock->DataBlockList)) {
        PDATA_BLOCK DataBlock = (PDATA_BLOCK)RemoveHeadList(&DirtyBlock->DataBlockList);
        DeallocateDataBlock(DataBlock, bVCLockAcquired);
        ASSERT(VolumeContext->lDataBlocksInUse > 0);
        InterlockedDecrement(&VolumeContext->lDataBlocksInUse);
        if (DataBlock == DirtyBlock->CurrentDataBlock) {
            DirtyBlock->CurrentDataBlock = NULL;
        }
    }

    ASSERT(NULL == DirtyBlock->CurrentDataBlock);
    if (!bVCLockAcquired)
        KeAcquireSpinLock(&VolumeContext->Lock, &OldIrql);
    ASSERT(VolumeContext->ulcbDataAllocated >= (DriverContext.ulDataBlockSize * DirtyBlock->ulDataBlocksAllocated));
    VolumeContext->ulcbDataAllocated -= DirtyBlock->ulDataBlocksAllocated * DriverContext.ulDataBlockSize;
    if (!bVCLockAcquired)
        KeReleaseSpinLock(&VolumeContext->Lock, OldIrql);

    DirtyBlock->ulcbDataFree = 0;
    DirtyBlock->ulcbDataUsed = 0;
    DirtyBlock->ulDataBlocksAllocated = 0;
    return;
}

VOID
DeallocateDirtyBlock(PKDIRTY_BLOCK pDirtyBlock, bool bVCLockAcquired)
{
    LONG lSize = (LONG)sizeof(KDIRTY_BLOCK);
    ASSERT(NULL != pDirtyBlock);
    ASSERT(NULL != pDirtyBlock->ChangeNode);

    DBFreeDataBlocks(pDirtyBlock, bVCLockAcquired);
    InMageFreeUnicodeString(&pDirtyBlock->FileName);
    if (NULL != pDirtyBlock->TagBuffer) {
        ASSERT(0 != pDirtyBlock->ulTagBufferSize);
        ExFreePoolWithTag(pDirtyBlock->TagBuffer, TAG_TAG_BUFFER);
        pDirtyBlock->TagBuffer = NULL;
    }
    while (NULL != pDirtyBlock->ChangeBlockList) {
        PCHANGE_BLOCK ChangeBlock = pDirtyBlock->ChangeBlockList;
        ASSERT((ChangeBlock == pDirtyBlock->CurrentChangeBlock) || (NULL != ChangeBlock->Next));
        pDirtyBlock->ChangeBlockList = ChangeBlock->Next;
        DeallocateChangeBlock(ChangeBlock);
    }
    ASSERT(NULL != pDirtyBlock->VolumeContext);
    DereferenceVolumeContext(pDirtyBlock->VolumeContext);
    DereferenceChangeNode(pDirtyBlock->ChangeNode);
    ExFreeToNPagedLookasideList(&DriverContext.DirtyBlocksPool, pDirtyBlock);
    InterlockedDecrement(&DriverContext.lDirtyBlocksAllocated);
    InterlockedExchangeAdd(&DriverContext.lNonPagedMemoryAllocated, -lSize);
    ASSERT(DriverContext.lNonPagedMemoryAllocated >= 0);
    return;
}

NTSTATUS
AddChangeBlockToDirtyBlock(PKDIRTY_BLOCK DirtyBlock)
{
    PCHANGE_BLOCK   ChangeBlock = AllocateChangeBlock();
    if (NULL == ChangeBlock) {
        return STATUS_NO_MEMORY;
    }

    if (NULL == DirtyBlock->ChangeBlockList) {
        ASSERT(NULL == DirtyBlock->CurrentChangeBlock);
        DirtyBlock->ChangeBlockList = ChangeBlock;
        ChangeBlock->usFirstIndex = MAX_CHANGES_EMB_IN_DB;
        ChangeBlock->usLastIndex = ChangeBlock->usMaxChanges + MAX_CHANGES_EMB_IN_DB - 1;
    } else {
        PCHANGE_BLOCK   CurrentBlock = DirtyBlock->CurrentChangeBlock;
        DirtyBlock->CurrentChangeBlock->Next = ChangeBlock;
        ChangeBlock->usFirstIndex = CurrentBlock->usLastIndex + 1;
        ChangeBlock->usLastIndex = CurrentBlock->usLastIndex + ChangeBlock->usMaxChanges;
    }
    DirtyBlock->CurrentChangeBlock = ChangeBlock;

    return STATUS_SUCCESS;
}

VOID
UpdateCoalesceArray(ULONG Length)
{
    ULONG i = 0;

    // Just for verification purpose, we keep track of top 10 Coalesced changes.
    // None of them should exceed DriverContext.MaxCoalescedMetaDataChangeSize
    for (i = 0; i < COALESCE_ARRAY_LENGTH; i++) {
        if (CoalesceArray[i] < Length) {
            CoalesceArray[i] = Length;
            break;
        }
    }
    return;
}

ULONG
MetaDataChangeCoalesce(PKDIRTY_BLOCK DirtyBlock, LONGLONG Offset, ULONG Length, ULONG ulIndex, ULONG InVolfltDataSource)
{
    PCHANGE_BLOCK ChangeBlock = DirtyBlock->CurrentChangeBlock;
    ULONG newIndex = 0;
    
    if (InVolfltDataSource != INVOLFLT_DATA_SOURCE_META_DATA || ulIndex == 0) {
        // Dont Coalesce Data mode and Bitmap mode changes.
        // A DirtyBlock either contains all DATA mode changes OR all non-DATA
        // mode changes. Never a mix of data and non-data changes.
        return 0;
    }
    // See the comments in function AddMetaDataToVolumeContextNew(). 
	// This Assert hit as part of supporting 32-bit Global Sequence Number support
    //ASSERT(ecCaptureModeData != DirtyBlock->VolumeContext->eCaptureMode);

    if (ulIndex <= MAX_CHANGES_EMB_IN_DB) {
		if (ulIndex < MAX_CHANGES_EMB_IN_DB)
            ASSERT(ChangeBlock == NULL);
        while (ulIndex > 0 && ulIndex <= MAX_CHANGES_EMB_IN_DB) {
            if ((DirtyBlock->ChangeOffset[ulIndex - 1] + DirtyBlock->ChangeLength[ulIndex - 1]  == Offset) &&
                (DirtyBlock->ChangeLength[ulIndex - 1] + Length <= DriverContext.MaxCoalescedMetaDataChangeSize)
               ) {
                DirtyBlock->ChangeLength[ulIndex - 1] += Length;
                UpdateCoalesceArray(DirtyBlock->ChangeLength[ulIndex - 1]);
                return 1;
            }
            ulIndex--;
        }

        return 0;
    }

    ASSERT(ChangeBlock != NULL);
    if (ChangeBlock != NULL) {
        ASSERT((USHORT) ulIndex <= (ChangeBlock->usLastIndex + 1));
        newIndex = ulIndex - ChangeBlock->usFirstIndex;
        ASSERT(newIndex <= ChangeBlock->usMaxChanges);

        while (newIndex > 0 && newIndex <= ChangeBlock->usMaxChanges) {
            if ((ChangeBlock->ChangeOffset[newIndex - 1] + ChangeBlock->ChangeLength[newIndex - 1]  == Offset) &&
                (ChangeBlock->ChangeLength[newIndex - 1] + Length <= DriverContext.MaxCoalescedMetaDataChangeSize))
            {
                // New change adjacent to exisiting change.
                ChangeBlock->ChangeLength[newIndex - 1] += Length;
                UpdateCoalesceArray(ChangeBlock->ChangeLength[newIndex - 1]);
                return 1;
            }
            newIndex--;
        }

        
    }


    return 0;
}

//Added for per io time stamp changes
NTSTATUS
AddChangeToDirtyBlock(PKDIRTY_BLOCK DirtyBlock, 
LONGLONG Offset, 
ULONG Length,
ULONG TimeDelta,
ULONG SequenceDelta,
ULONG InVolfltDataSource,
BOOLEAN &Coalesced)
{
    NTSTATUS Status = STATUS_SUCCESS;
    if (InVolfltDataSource != INVOLFLT_DATA_SOURCE_DATA) {
        ASSERT(DirtyBlock->cChanges < MAX_KDIRTY_CHANGES);
    }

    ULONG ulIndex = DirtyBlock->cChanges;

    Coalesced = FALSE;
    if (MetaDataChangeCoalesce(DirtyBlock, Offset, Length, ulIndex, InVolfltDataSource)) {
        Coalesced = TRUE;
        return Status;
    }
    
    if (ulIndex < MAX_CHANGES_EMB_IN_DB) {
        DirtyBlock->ChangeLength[ulIndex] = Length;
        DirtyBlock->ChangeOffset[ulIndex] = Offset;
        DirtyBlock->TimeDelta[ulIndex] = TimeDelta;
        DirtyBlock->SequenceDelta[ulIndex] = SequenceDelta;
    } else {
        PCHANGE_BLOCK ChangeBlock = DirtyBlock->CurrentChangeBlock;
        if ((NULL == ChangeBlock) || (ulIndex > ChangeBlock->usLastIndex)) {
            Status = AddChangeBlockToDirtyBlock(DirtyBlock);
            if (STATUS_SUCCESS != Status) {
                InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("AddMetaDataToDirtyBlock: Failed allocation of dirtyblock\n"));
                return Status;
            }
            ChangeBlock = DirtyBlock->CurrentChangeBlock;
        }
        ASSERT(ulIndex >= ChangeBlock->usFirstIndex);

        ulIndex -= ChangeBlock->usFirstIndex;
        ChangeBlock->ChangeLength[ulIndex] = Length;
        ChangeBlock->ChangeOffset[ulIndex] = Offset;
        ChangeBlock->TimeDelta[ulIndex] = TimeDelta;
        ChangeBlock->SequenceDelta[ulIndex] = SequenceDelta;
    }

    DirtyBlock->cChanges++;

    return Status;
}

bool
ReserveSpaceForChangeInDirtyBlock(PKDIRTY_BLOCK DirtyBlock)
{
    PCHANGE_BLOCK CurrentChangeBlock = DirtyBlock->CurrentChangeBlock;

    if (DirtyBlock->cChanges < MAX_CHANGES_EMB_IN_DB) {
        return true;
    }

    if ((NULL == CurrentChangeBlock) || (DirtyBlock->cChanges > CurrentChangeBlock->usLastIndex)) {
        NTSTATUS Status = AddChangeBlockToDirtyBlock(DirtyBlock);
        if (STATUS_SUCCESS != Status) {
            return false;
        }
    }

    CurrentChangeBlock = DirtyBlock->CurrentChangeBlock;
    if ((NULL != CurrentChangeBlock) && (DirtyBlock->cChanges <= CurrentChangeBlock->usLastIndex)) {
        return true;
    }

    return false;
}

void
CopyChangeMetaData(PKDIRTY_BLOCK DirtyBlock, 
                   PULONGLONG OffsetArray, 
                   PULONG LengthArray,
                   ULONG ulMaxElements,
                   PULONG TimeDeltaArray,
                   PULONG SequenceDeltaArray)
{
     if (INVOLFLT_DATA_SOURCE_DATA != DirtyBlock->ulDataSource) {
        ASSERT((DirtyBlock->cChanges <= ulMaxElements) && (DirtyBlock->cChanges <= MAX_KDIRTY_CHANGES));
    }

    PCHANGE_BLOCK   ChangeBlock = DirtyBlock->ChangeBlockList;
    ULONG           ulCopied = 0, ulElements = 0;
    ULONG           ulTotal = DirtyBlock->cChanges;

    if (0 == ulTotal) {
        return;
    }

    if (ulTotal > ulMaxElements) {
        ulTotal = ulMaxElements;
    }

    if (ulTotal > MAX_CHANGES_EMB_IN_DB) {
        ulElements = MAX_CHANGES_EMB_IN_DB;
    } else {
        ulElements = ulTotal;
    }

    //Here the memory used, like TimeDeltaArray.. etc.. could be coming in either from an on-fly allocation
    //using memory allocation routines or static arrays. Now there is no effective way to check if the
    //memory is static or on-fly unless and until some sort of tracking mechanism is designed which
    //obviously is an overhead. Any memory allocation wrapper routine with some header or footer is
    //also equally bad when one is not sure about the allocation method used - In a nutshell the best
    //way to ensure compliance in certain scenarios is to go through the code thoroughly. -Kartha.
	RtlCopyMemory(&TimeDeltaArray[ulCopied], DirtyBlock->TimeDelta, sizeof(ULONG)*ulElements);
	RtlCopyMemory(&SequenceDeltaArray[ulCopied], DirtyBlock->SequenceDelta, sizeof(ULONG)*ulElements);
	RtlCopyMemory(&OffsetArray[ulCopied], DirtyBlock->ChangeOffset, sizeof(LONGLONG)*ulElements);
	RtlCopyMemory(&LengthArray[ulCopied], DirtyBlock->ChangeLength, sizeof(ULONG)*ulElements);
    ulCopied += ulElements;

    while ((NULL != ChangeBlock) && (ulCopied < ulTotal)) {
        ULONG ulElements = ChangeBlock->usLastIndex - ChangeBlock->usFirstIndex + 1;
        if (ulTotal < ulCopied + ulElements) {
            ulElements = ulTotal - ulCopied;
        }

		RtlCopyMemory(&TimeDeltaArray[ulCopied], ChangeBlock->TimeDelta, sizeof(ULONG)*ulElements);
		RtlCopyMemory(&SequenceDeltaArray[ulCopied], ChangeBlock->SequenceDelta, sizeof(ULONG)*ulElements);
		RtlCopyMemory(&OffsetArray[ulCopied], ChangeBlock->ChangeOffset, sizeof(LONGLONG)*ulElements);
		RtlCopyMemory(&LengthArray[ulCopied], ChangeBlock->ChangeLength, sizeof(ULONG)*ulElements);
        ulCopied += ulElements;
        ChangeBlock = ChangeBlock->Next;
    }

    return;
}

/*-----------------------------------------------------------------------------
 * CommitDirtyBlockTransaction 
 * This function has to be called without acquiring VolumeContext lock
 *-----------------------------------------------------------------------------
 */
NTSTATUS
CommitDirtyBlockTransaction(
    PVOLUME_CONTEXT     VolumeContext,
    PCOMMIT_TRANSACTION pCommitTransaction
    )
{
    KIRQL           OldIrql;
    PKDIRTY_BLOCK   DirtyBlock = NULL;
    NTSTATUS   Status = STATUS_SUCCESS;


    if ((NULL == pCommitTransaction) || (NULL == VolumeContext))
        return STATUS_INVALID_PARAMETER;

    if (pCommitTransaction->ulFlags & COMMIT_TRANSACTION_FLAG_RESET_RESYNC_REQUIRED_FLAG) {
        ResetVolumeOutOfSync(VolumeContext, false);
    }

    FreeOrphanedMappedDataBlocksWithVolumeID((HANDLE)VolumeContext, PsGetCurrentProcessId());

	KeQuerySystemTime(&VolumeContext->liCommitDirtyBlockTimeStamp);

    if (0 != pCommitTransaction->ulTransactionID.QuadPart) {
        LIST_ENTRY      ListHead;

        InVolDbgPrint(DL_TRACE_L1, DM_DATA_FILTERING,
                      ("CommitDirtyBlockTransaction: (Before) Alloced = %#x Missed = %#I64x Free = %#x Reserved = %#x\n",
                       VolumeContext->ulcbDataAllocated, VolumeContext->ullcbDataAlocMissed,
                       VolumeContext->ulcbDataFree, VolumeContext->ulcbDataReserved));

        if (DriverContext.ulValidationLevel >= ADD_TO_EVENT_LOG && ((NULL != VolumeContext->pDBPendingTransactionConfirm) && 
                (pCommitTransaction->ulTransactionID.QuadPart == VolumeContext->pDBPendingTransactionConfirm->uliTransactionID.QuadPart))) {

            DirtyBlock = VolumeContext->pDBPendingTransactionConfirm;                
            if (ecChangeNodeDataFile == DirtyBlock->ChangeNode->eChangeNode) {
                if (VolumeContext->bResyncStartReceived == true) {
                    if (VolumeContext->LastCommittedTimeStamp <= VolumeContext->ResynStartTimeStamp &&
                        VolumeContext->ResynStartTimeStamp <= DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601 &&
                        VolumeContext->ResynStartTimeStamp <= DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601) {
                            VolumeContext->bResyncStartReceived = false;
                            // "First diff file after Resync start (in Data File mode) commited, Volume name: %s, PrevEndTS: %I64x, ResyncStartTS: %I64x, CurrentEndTS: %I64x, CurrentStartTS: %I64x."
                            InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                                INVOLFLT_INFO_FIRSTFILE_VALIDATION_C1, STATUS_SUCCESS,
                                VolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES,
                                VolumeContext->LastCommittedTimeStamp,
                                VolumeContext->ResynStartTimeStamp,
                                DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
                                DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
                    }
                    if (DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601 < VolumeContext->ResynStartTimeStamp &&
                        VolumeContext->ResynStartTimeStamp < DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601) {
                            VolumeContext->bResyncStartReceived = false;
                            // "Start-End TS Problem with First diff file after Resync start (in Data File mode) commited, Volume name: %s, PrevEndTS: %I64x, ResyncStartTS: %I64x, CurrentEndTS: %I64x, CurrentStartTS: %I64x."
                            InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                                INVOLFLT_ERR_FIRSTFILE_VALIDATION_C2, STATUS_SUCCESS,
                                VolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES,
                                VolumeContext->LastCommittedTimeStamp,
                                VolumeContext->ResynStartTimeStamp,
                                DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
                                DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
                    }        
                    if (VolumeContext->LastCommittedTimeStamp > VolumeContext->ResynStartTimeStamp) {
                            VolumeContext->bResyncStartReceived = false;
                            // "PrevEndTS Problem with First diff file after Resync start (in Data File mode) commited, Volume name: %s, PrevEndTS: %I64x, ResyncStartTS: %I64x, CurrentEndTS: %I64x, CurrentStartTS: %I64x."
                            InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                                INVOLFLT_ERR_FIRSTFILE_VALIDATION_C3, STATUS_SUCCESS,
                                VolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES,
                                VolumeContext->LastCommittedTimeStamp,
                                VolumeContext->ResynStartTimeStamp,
                                DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
                                DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
                    }
                }
            }else {
                if (VolumeContext->bResyncStartReceived == true) {
                    if (VolumeContext->LastCommittedTimeStamp <= VolumeContext->ResynStartTimeStamp &&
                        VolumeContext->ResynStartTimeStamp <= DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601 &&
                        VolumeContext->ResynStartTimeStamp <= DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601) {
                            VolumeContext->bResyncStartReceived = false;
                            // "First diff file after Resync start (Not in Data File mode) commited, Volume name: %s, PrevEndTS: %I64x, ResyncStartTS: %I64x, CurrentEndTS: %I64x, CurrentStartTS: %I64x."
                            InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                                INVOLFLT_INFO_FIRSTFILE_VALIDATION_C4, STATUS_SUCCESS,
                                VolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES,
                                VolumeContext->LastCommittedTimeStamp,
                                VolumeContext->ResynStartTimeStamp,
                                DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
                                DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
                    }
                    if (DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601 < VolumeContext->ResynStartTimeStamp &&
                        VolumeContext->ResynStartTimeStamp < DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601) {
                            VolumeContext->bResyncStartReceived = false;
                            // "Start-End TS Problem with First diff file after Resync start (Not in Data File mode) commited, Volume name: %s, PrevEndTS: %I64x, ResyncStartTS: %I64x, CurrentEndTS: %I64x, CurrentStartTS: %I64x."
                            InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                                INVOLFLT_ERR_FIRSTFILE_VALIDATION_C5, STATUS_SUCCESS,
                                VolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES,
                                VolumeContext->LastCommittedTimeStamp,
                                VolumeContext->ResynStartTimeStamp,
                                DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
                                DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
                    }        
                    if (VolumeContext->LastCommittedTimeStamp > VolumeContext->ResynStartTimeStamp) {
                            VolumeContext->bResyncStartReceived = false;
                            // "PrevEndTS Problem with First diff file after Resync start (Not in Data File mode) commited, Volume name: %s, PrevEndTS: %I64x, ResyncStartTS: %I64x, CurrentEndTS: %I64x, CurrentStartTS: %I64x."
                            InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                                INVOLFLT_ERR_FIRSTFILE_VALIDATION_C6, STATUS_SUCCESS,
                                VolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES,
                                VolumeContext->LastCommittedTimeStamp,
                                VolumeContext->ResynStartTimeStamp,
                                DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
                                DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
                    }
                }
            }
        }
        
        InitializeListHead(&ListHead);
        KeAcquireSpinLock(&VolumeContext->Lock, &OldIrql);

        if ((ecCaptureModeMetaData == VolumeContext->eCaptureMode) && 
            NeedWritingToDataFile(VolumeContext) && 
            VolumeContext->bDataLogLimitReached && 
            (VolumeContext->ullcbDataToDiskLimit  > VolumeContext->ullcbDataWrittenToDisk ) && 
            ((VolumeContext->ullcbDataToDiskLimit - VolumeContext->ullcbDataWrittenToDisk ) >= VolumeContext->ullcbMinFreeDataToDiskLimit))
        {                
            QueueChangeNodesToSaveAsFile(VolumeContext,TRUE);
            VolumeContext->bDataLogLimitReached = false;         
        }
        bool bPendingTransaction = true;
        bool bCaptureModeCheck = false;

        if ((NULL == VolumeContext->pDBPendingTransactionConfirm) && (0 == VolumeContext->ulPendingTSOTransactionID.QuadPart)) {
            bPendingTransaction = false;
        }

        if ((NULL != VolumeContext->pDBPendingTransactionConfirm) && 
            (pCommitTransaction->ulTransactionID.QuadPart == VolumeContext->pDBPendingTransactionConfirm->uliTransactionID.QuadPart)) 
        {
            DirtyBlock = VolumeContext->pDBPendingTransactionConfirm;
            VolumeContext->pDBPendingTransactionConfirm = NULL;

            // Update Commit statistics
            if ((INVOLFLT_DATA_SOURCE_DATA == DirtyBlock->ulDataSource) &&
                (VolumeContext->LatencyDistForCommit || VolumeContext->LatencyLogForCommit))
            {
                LARGE_INTEGER       PerformanceFrequency, CurrentTickCount, liMicroSeconds;
                CurrentTickCount = KeQueryPerformanceCounter(&PerformanceFrequency);

                liMicroSeconds.QuadPart = CurrentTickCount.QuadPart - DirtyBlock->liTickCountAtUserRequest.QuadPart;
                liMicroSeconds.QuadPart = liMicroSeconds.QuadPart * 1000 * 1000 / PerformanceFrequency.QuadPart;

                if (0 == liMicroSeconds.HighPart) {
                    if (VolumeContext->LatencyDistForCommit) {
                        VolumeContext->LatencyDistForCommit->AddToDistribution(liMicroSeconds.LowPart);
                    }

                    if (VolumeContext->LatencyLogForCommit) {
                        VolumeContext->LatencyLogForCommit->AddToLog(liMicroSeconds.LowPart);
                    }
                }
            }
            VolumeContext->LastCommittedTimeStamp = DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601;
            VolumeContext->LastCommittedSequenceNumber = DirtyBlock->TagTimeStampOfLastChange.ullSequenceNumber;
            VolumeContext->LastCommittedSplitIoSeqId = DirtyBlock->ulSequenceIdForSplitIO;
            VolumeContext->liTickCountAtNotifyEventSet.QuadPart = 0;
            if ((ecWriteOrderStateData == DirtyBlock->eWOState) || (ecWriteOrderStateMetadata == DirtyBlock->eWOState))
                VolumeContext->EndTimeStampofDB = DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601;
            
            ASSERT(VolumeContext->ullcbChangesPendingCommit >= DirtyBlock->ulicbChanges.QuadPart);
            ASSERT(VolumeContext->ulChangesPendingCommit >= DirtyBlock->cChanges);
            VolumeContext->ullcbChangesPendingCommit -= DirtyBlock->ulicbChanges.QuadPart;
            VolumeContext->ulChangesPendingCommit -= DirtyBlock->cChanges;
            VolumeContext->uliChangesReturnedToService.QuadPart += DirtyBlock->cChanges;
            VolumeContext->ulicbReturnedToService.QuadPart += DirtyBlock->ulicbChanges.QuadPart;
            DeductChangeCountersOnDataSource(&VolumeContext->ChangeList, DirtyBlock->ulDataSource,
                                             DirtyBlock->cChanges, DirtyBlock->ulicbChanges.QuadPart);
            VolumeContext->lDirtyBlocksInPendingCommitQueue--;
            bCaptureModeCheck = 1;
          
        } else if ((0 != VolumeContext->ulPendingTSOTransactionID.QuadPart) && 
                   (pCommitTransaction->ulTransactionID.QuadPart == VolumeContext->ulPendingTSOTransactionID.QuadPart))
        {

            if (DriverContext.ulValidationLevel >= ADD_TO_EVENT_LOG && VolumeContext->bResyncStartReceived == true) {
                if (VolumeContext->LastCommittedTimeStamp <= VolumeContext->ResynStartTimeStamp &&
                    VolumeContext->ResynStartTimeStamp <= VolumeContext->TSOEndTimeStamp) {                
                        VolumeContext->bResyncStartReceived = false;
                        // "First diff file after Resync start (TSO file) commited, Volume name: %s, PrevEndTS: %I64x, ResyncStartTS: %I64x, TSOEndTS: %I64x."
                        InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                            INVOLFLT_INFO_FIRSTFILE_VALIDATION9, STATUS_SUCCESS,
                            VolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES,
                            VolumeContext->LastCommittedTimeStamp,
                            VolumeContext->ResynStartTimeStamp,
                            VolumeContext->TSOEndTimeStamp);
                }
                if (VolumeContext->LastCommittedTimeStamp > VolumeContext->ResynStartTimeStamp) {
                        VolumeContext->bResyncStartReceived = false;
                        // "PrevEndTS Problem with First diff file after Resync start (TSO file) commited, Volume name: %s, PrevEndTS: %I64x, ResyncStartTS: %I64x, TSOEndTS: %I64x."
                        InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                            INVOLFLT_ERR_FIRSTFILE_VALIDATION10, STATUS_SUCCESS,
                            VolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES,
                            VolumeContext->LastCommittedTimeStamp,
                            VolumeContext->ResynStartTimeStamp,
                            VolumeContext->TSOEndTimeStamp);
                }
            }
        
            VolumeContext->ulPendingTSOTransactionID.QuadPart = 0;

            VolumeContext->LastCommittedTimeStamp = VolumeContext->TSOEndTimeStamp;
            VolumeContext->LastCommittedSplitIoSeqId = VolumeContext->TSOSequenceId;
            VolumeContext->LastCommittedSequenceNumber = VolumeContext->TSOEndSequenceNumber;
            if ((ecWriteOrderStateData == VolumeContext->eWOState) || (ecWriteOrderStateMetadata == VolumeContext->eWOState))
                VolumeContext->EndTimeStampofDB  = VolumeContext->TSOEndTimeStamp;

            bCaptureModeCheck = 1;
        } else {
            Status = STATUS_UNSUCCESSFUL;
        }

	  // Check if capture mode can be changed.
	  if (bCaptureModeCheck) {
            if ((ecCaptureModeMetaData == VolumeContext->eCaptureMode) && CanSwitchToDataCaptureMode(VolumeContext)) {
                InVolDbgPrint(DL_INFO, DM_CAPTURE_MODE | DM_DATA_FILTERING,
                              ("%s: Changing capture mode to DATA for volume %s(%s)\n", __FUNCTION__,
                               VolumeContext->GUIDinANSI, VolumeContext->DriveLetter));
                VCSetCaptureModeToData(VolumeContext);
            }

            // Check if Write order state can be changed up to meta-data or data
            VCSetWOState(VolumeContext, false, __FUNCTION__);

            // Check if bitmap read has to be continued or restarted.
            if ((ecWriteOrderStateBitmap == VolumeContext->eWOState) && (NULL != VolumeContext->pVolumeBitmap) &&
                 (VolumeContext->ChangeList.ulTotalChangesPending <  DriverContext.DBLowWaterMarkWhileServiceRunning))
            {
                if (ecVBitmapStateReadPaused == VolumeContext->pVolumeBitmap->eVBitmapState)
                    AddVCWorkItemToList(ecWorkItemContinueBitmapRead, VolumeContext, 0, FALSE, &ListHead);
                else if ((ecVBitmapStateAddingChanges == VolumeContext->pVolumeBitmap->eVBitmapState) ||
                         (ecVBitmapStateOpened == VolumeContext->pVolumeBitmap->eVBitmapState)) 
                {
                    AddVCWorkItemToList(ecWorkItemStartBitmapRead, VolumeContext, 0, FALSE, &ListHead);
                }
            }
        }
        KeReleaseSpinLock(&VolumeContext->Lock, OldIrql);

		// IOCTL sequence GetDB->Clear Diff->Commit DB path returns STATUS_UNSUCCESFUL and appears as false alarm. 
		// Returning status success so that user space does not worry about this.
        if (false == bPendingTransaction)
            Status = STATUS_SUCCESS;

        if (NULL != DirtyBlock) {
            if (DirtyBlock->ulFlags & KDIRTY_BLOCK_FLAG_TAG_WAITING_FOR_COMMIT) {
                PTAG_NODE TagNode = AddStatusAndReturnNodeIfComplete(DirtyBlock->TagGUID, DirtyBlock->ulTagVolumeIndex, ecTagStatusCommited);
                if (NULL != TagNode) {
                    // Complete the Irp with Status
                    PIRP Irp = TagNode->Irp;

                    CopyStatusToOutputBuffer(TagNode, (PTAG_VOLUME_STATUS_OUTPUT_DATA)Irp->AssociatedIrp.SystemBuffer);
                    Irp->IoStatus.Information = SIZE_OF_TAG_VOLUME_STATUS_OUTPUT_DATA(TagNode->ulNoOfVolumes);
                    Irp->IoStatus.Status = STATUS_SUCCESS;
                    IoCompleteRequest(Irp, IO_NO_INCREMENT);

                    DeallocateTagNode(TagNode);
                }
                DirtyBlock->ulFlags &= ~KDIRTY_BLOCK_FLAG_TAG_WAITING_FOR_COMMIT;
            }

            ChangeNodeCleanup(DirtyBlock->ChangeNode);
        }

        while (!IsListEmpty(&ListHead)) {
            // go through all Work Items and fire the events.
            PWORK_QUEUE_ENTRY   pWorkItem = (PWORK_QUEUE_ENTRY)RemoveHeadList(&ListHead);
            ProcessVContextWorkItems(pWorkItem);
            DereferenceWorkQueueEntry(pWorkItem);
        }
        
        InVolDbgPrint(DL_TRACE_L1, DM_DATA_FILTERING,
                      ("CommitDirtyBlockTransaction: (After) Alloced = %#x Missed = %#I64x Free = %#x Reserved = %#x\n",
                       VolumeContext->ulcbDataAllocated, VolumeContext->ullcbDataAlocMissed,
                       VolumeContext->ulcbDataFree, VolumeContext->ulcbDataReserved));
    }

    return Status;
}

/*-----------------------------------------------------------------------------
 * OrphanDataBlocksAndMoveCommitPendingDirtyBlockToMainQueue
 *
 * This function has to be called with VolumeContext lock acquired.
 *-----------------------------------------------------------------------------
 */
VOID
OrphanMappedDataBlocksAndMoveCommitPendingDirtyBlockToMainQueue(
    PVOLUME_CONTEXT     pVolumeContext
    )
{
    KIRQL           OldIrql;

    if (NULL == pVolumeContext)
        return;

    if (NULL != pVolumeContext->pDBPendingTransactionConfirm) {
        PKDIRTY_BLOCK   DirtyBlock = pVolumeContext->pDBPendingTransactionConfirm;
        pVolumeContext->pDBPendingTransactionConfirm = NULL;
        pVolumeContext->lDirtyBlocksInPendingCommitQueue--;

        ASSERT(DISPATCH_LEVEL == KeGetCurrentIrql());
        KeAcquireSpinLockAtDpcLevel(&DriverContext.DataBlockListLock);
        PLIST_ENTRY Entry = DirtyBlock->DataBlockList.Flink;
        while(Entry != &DirtyBlock->DataBlockList) {
            PDATA_BLOCK DataBlock = (PDATA_BLOCK) Entry;
            Entry = Entry->Flink;
            if (DataBlock->ulFlags & DATABLOCK_FLAGS_MAPPED) {
                if (DataBlock == DirtyBlock->CurrentDataBlock) {
                    DirtyBlock->CurrentDataBlock = NULL;
                }
                RemoveEntryList(&DataBlock->ListEntry);
                InsertTailList(&DriverContext.OrphanedMappedDataBlockList, &DataBlock->ListEntry);
                DriverContext.ulDataBlocksInOrphanedMappedDataBlockList++;
            }
        }
        KeReleaseSpinLockFromDpcLevel(&DriverContext.DataBlockListLock);
        PrependChangeNodeToVolumeContext(pVolumeContext, DirtyBlock->ChangeNode);
    }
}

PVOLUME_CONTEXT
GetVolumeContextWithGUID(PWCHAR    pGUID)
{
    PVOLUME_CONTEXT     pVolumeContext = NULL;
    PLIST_ENTRY         pCurr;
    KIRQL               OldIrql;

    // Lowercase the GUID;
    ConvertGUIDtoLowerCase(pGUID);

    if (GUID_SIZE_IN_BYTES == RtlCompareMemory(pGUID, DriverContext.ZeroGUID, GUID_SIZE_IN_BYTES)) {
        ASSERTMSG("GUID passed to GetVolumeContextWithGUID is zero", 0);
        return NULL;
    }

    // GUID should not be all zeros.
    // Find the VolumeContext;
    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);

    pCurr = DriverContext.HeadForVolumeContext.Flink;
    while (pCurr != &DriverContext.HeadForVolumeContext) {
        PVOLUME_CONTEXT pTemp = (PVOLUME_CONTEXT)pCurr;
        PVOLUME_GUID    pVolumeGUID = NULL;

        if (!(pTemp->ulFlags & VCF_DEVICE_SURPRISE_REMOVAL)) {
            if (GUID_SIZE_IN_BYTES == RtlCompareMemory(pGUID, pTemp->wGUID, GUID_SIZE_IN_BYTES)) {
                pVolumeContext = pTemp;
                ReferenceVolumeContext(pVolumeContext);
                break;
            }

            if (NULL != pTemp->GUIDList) {
                KeAcquireSpinLockAtDpcLevel(&pTemp->Lock);

                pVolumeGUID = pTemp->GUIDList;
                while (NULL != pVolumeGUID) {
                    if (GUID_SIZE_IN_BYTES == RtlCompareMemory(pGUID, pVolumeGUID->GUID, GUID_SIZE_IN_BYTES)) {
                        break;
                    }
                    pVolumeGUID = pVolumeGUID->NextGUID;
                }

                KeReleaseSpinLockFromDpcLevel(&pTemp->Lock);
            }

            if (NULL != pVolumeGUID) {
                pVolumeContext = pTemp;
                ReferenceVolumeContext(pVolumeContext);
                break;
            }
        }

        pCurr = pCurr->Flink;
    }

    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

    return pVolumeContext;
}

PKDIRTY_BLOCK
GetNextDirtyBlock(PKDIRTY_BLOCK DirtyBlock, PVOLUME_CONTEXT VolumeContext)
{
    ASSERT(NULL != DirtyBlock);

    PCHANGE_NODE    ChangeNode = DirtyBlock->ChangeNode;
    ASSERT(NULL != ChangeNode);
    ChangeNode = (PCHANGE_NODE)ChangeNode->ListEntry.Flink;
    ASSERT((PLIST_ENTRY)ChangeNode != &VolumeContext->ChangeList.Head);

    ASSERT(ecChangeNodeDirtyBlock == ChangeNode->eChangeNode);

    return ChangeNode->DirtyBlock;
}

ULONG
SetSVDLastChangeTimeStamp(PKDIRTY_BLOCK DirtyBlock)
{
    ULONG   ulBytesUsed = 0;

    ASSERT(DirtyBlock->ulcbDataFree >= SVFileFormatTrailerSize());
    ASSERT(NULL != DirtyBlock->CurrentDataBlock);
    ASSERT(NULL == DirtyBlock->SVDLastChangeTimeStamp);

    if (NULL == DirtyBlock->SVDLastChangeTimeStamp) {
        DirtyBlock->SVDLastChangeTimeStamp = 
            DirtyBlock->CurrentDataBlock->CurrentPosition;
        IncrementCurrentPosition(DirtyBlock, SVFileFormatTrailerSize());
        ulBytesUsed = SVFileFormatTrailerSize();
    }

    PDATA_BLOCK DataBlock = (PDATA_BLOCK)DirtyBlock->DataBlockList.Flink;    
    while((PLIST_ENTRY)DataBlock != &DirtyBlock->DataBlockList) {
#if DBG
        if (DataBlock->ulFlags & DATABLOCK_FLAGS_LOCKED) {
            if (DataBlock->ListEntry.Flink != &DirtyBlock->DataBlockList) {
                InVolDbgPrint(DL_ERROR, DM_MEM_TRACE,("Error many or midle block locked %p\n", DirtyBlock));
            }
        }
#endif
        UnLockDataBlock(DataBlock);
        DataBlock = (PDATA_BLOCK)DataBlock->ListEntry.Flink;
    }
    return ulBytesUsed;
}

/*-----------------------------------------------------------------------------
 *
 * Bitmap Releated Operations
 *
 *-----------------------------------------------------------------------------
 */
NTSTATUS
StartFilteringDevice(
    PVOLUME_CONTEXT     VolumeContext,
    bool                bLockAcquired,
	bool                bUser
    )
{
    NTSTATUS        Status = STATUS_SUCCESS;
    KIRQL           OldIrql;

    if (!bLockAcquired)
        KeAcquireSpinLock(&VolumeContext->Lock, &OldIrql);

    InVolDbgPrint(DL_INFO, DM_VOLUME_STATE, 
                  ("%s: Starting filtering on volume %s(%s)\n", __FUNCTION__,
                             VolumeContext->GUIDinANSI, VolumeContext->DriveLetter));

    VolumeContext->ulFlags &= ~VCF_FILTERING_STOPPED;
    KeQuerySystemTime(&VolumeContext->liStartFilteringTimeStamp);
    if (bUser)
        KeQuerySystemTime(&VolumeContext->liStartFilteringTimeStampByUser);
    VolumeContext->eWOState = ecWriteOrderStateBitmap;
    VolumeContext->liTickCountAtLastCaptureModeChange = VolumeContext->liTickCountAtLastWOStateChange = KeQueryPerformanceCounter(NULL);
    VolumeContext->eCaptureMode = ecCaptureModeMetaData;
    InVolDbgPrint(DL_INFO, DM_CAPTURE_MODE | DM_WO_STATE, 
                  ("%s: Volume %s(%s): Setting capture mode to ecCaptureModeMetaData.\n\t\tWOState to ecWriteOrderStateBitmap\n", __FUNCTION__,
                             VolumeContext->GUIDinANSI, VolumeContext->DriveLetter));

    if (!bLockAcquired)
        KeReleaseSpinLock(&VolumeContext->Lock, OldIrql);

    return Status;
}
/*-----------------------------------------------------------------------------
 * Function Name : StopFilteringDevice
 * Parameters :
 *      pVolumeContext : This parameter points to the device filtering has
 *                            to be stopped
 * Returns :
 *      NTSTATUS  : STATUS_SUCCESS if succeded in marking DSDBC to stop filter
 *                          
 * NOTES :
 * This function is called at PASSIVE_LEVEL. 
 *
 *      This function marks the VC (Device Specific Dirty Blocks Context)
 * for stop filtering and removes all the Dirty Blocks  and deletes them.
 *
 *--------------------------------------------------------------------------------------------
 */
NTSTATUS
StopFilteringDevice(
    PVOLUME_CONTEXT VolumeContext,
    bool            bLockAcquired,
    bool            bClearBitmapFile,
    bool            bCloseBitmapFile,
    PVOLUME_BITMAP  *ppVolumeBitmap
    )
{
    NTSTATUS        Status = STATUS_SUCCESS;
    KIRQL           OldIrql;
    PKDIRTY_BLOCK   pDirtyBlock, pNextDirtyBlocks;
    BOOLEAN         bWaitForWritesToBeCompleted = FALSE;
    PVOLUME_BITMAP  pVolumeBitmap = NULL;

    ASSERT(NULL != VolumeContext);

    if (NULL == VolumeContext)
        return Status;

    if (ppVolumeBitmap) {
        *ppVolumeBitmap = NULL;
    }

    if (!bLockAcquired)
        KeAcquireSpinLock(&VolumeContext->Lock, &OldIrql);

    InVolDbgPrint(DL_INFO, DM_VOLUME_STATE, 
                  ("%s: Stoping filtering on volume %s(%s)\n", __FUNCTION__,
                             VolumeContext->GUIDinANSI, VolumeContext->DriveLetter));

    // Let us free the commit pending dirty blocks too.
    OrphanMappedDataBlocksAndMoveCommitPendingDirtyBlockToMainQueue(VolumeContext);

    FreeChangeNodeList(&VolumeContext->ChangeList);
    VolumeContext->ulFlags |= VCF_FILTERING_STOPPED;
    KeQuerySystemTime(&VolumeContext->liStopFilteringTimeStamp);
    pVolumeBitmap = VolumeContext->pVolumeBitmap;
    if (bCloseBitmapFile) {
        VolumeContext->pVolumeBitmap = NULL;
    }

    VCFreeReservedDataBlockList(VolumeContext);
    VolumeContext->ulcbDataFree = 0;
    VolumeContext->ullcbDataAlocMissed += VolumeContext->ulcbDataReserved;
    VolumeContext->ulcbDataReserved = 0;

    // TODO: Check and see if these values have to be different for cases
    // if bitmap file is closed or not? When filtering mode is used the states are set as
    // following.
    // if (bCloseBitmapFile) {
    //    VolumeContext->pVolumeBitmap = NULL;
    //    VolumeContext->ePrevFilteringMode = ecFilteringModeUninitialized;
    //    VolumeContext->eFilteringMode = ecFilteringModeUninitialized;
    // } else {
    //    VolumeContext->eFilteringMode = ecFilteringModeMetaData;
    // }

    // Should this be Uninitialized or MetaData?
    VolumeContext->eCaptureMode = ecCaptureModeUninitialized;
    VolumeContext->ePrevWOState = ecWriteOrderStateUnInitialized;
    VolumeContext->eWOState = ecWriteOrderStateUnInitialized;
	// Added for OOD
    if ((VolumeContext->bNotify) && (NULL != VolumeContext->DBNotifyEvent)) {
        VolumeContext->liTickCountAtNotifyEventSet = KeQueryPerformanceCounter(NULL);
        KeSetEvent(VolumeContext->DBNotifyEvent, EVENT_INCREMENT, FALSE);
    }
    VolumeContext->LastCommittedTimeStamp = 0;
    VolumeContext->LastCommittedSequenceNumber = 0;
    VolumeContext->LastCommittedSplitIoSeqId = 0;
    VolumeContext->EndTimeStampofDB = 0;
    InVolDbgPrint(DL_INFO, DM_CAPTURE_MODE | DM_WO_STATE, 
                  ("%s: Volume %s(%s): Setting capture mode to ecCaptureModeMetaData.\n\t\tWOState to ecWriteOrderStateUnInitialized\n", __FUNCTION__,
                             VolumeContext->GUIDinANSI, VolumeContext->DriveLetter));
    
    if (!bLockAcquired)
        KeReleaseSpinLock(&VolumeContext->Lock, OldIrql);

    if (NULL != pVolumeBitmap) {
        if (!bLockAcquired) {
            if (bCloseBitmapFile) {
                BitmapPersistentValues BitmapPersistentValue(VolumeContext);
                CloseBitmapFile(pVolumeBitmap, bClearBitmapFile, BitmapPersistentValue, false);
            } else if (bClearBitmapFile) {
                ClearBitmapFile(pVolumeBitmap);
            }
            // TODO: Need to delete the log.
            // pBitmap->DeleteLog();
        } else {
            if (ppVolumeBitmap) {
                *ppVolumeBitmap = pVolumeBitmap;
            }
        }
    }

    return Status;
}

/*------------------------------------------------------------------------------
 * This array is documented in InVolFlt.h changes to this array have to be made
 * at both the places
 * Fix for bug 25726 - Added another text entry for missed PNP notification.
 *------------------------------------------------------------------------------
 */
WCHAR *ErrorToRegErrorDescriptionsW[ERROR_TO_REG_MAX_ERROR + 1] = {
    NULL,
    L"Out of NonPagedPool for dirty blocks",
    L"Bit map read failed with Status %#x",
    L"Bit map write failed with Status %#x",
    L"Bit map open failed with Status %#x",
    L"Bit map open failed and could not write changes",
    L"Probably missed a volume removal notification from PNP",
    L"See event viewer for error description"
    };

CHAR *ErrorToRegErrorDescriptionsA[ERROR_TO_REG_MAX_ERROR + 1] = {
    NULL,
    "Out of NonPagedPool for dirty blocks",
    "Bit map read failed with Status %#x",
    "Bit map write failed with Status %#x",
    "Bit map open failed with Status %#x",
    "Bit map open failed and could not write changes",
    "Probably missed a volume removal notification from PNP",
    "See event viewer for error description"
    };

VOID
SetVolumeOutOfSyncWorkerRoutine(PWORK_QUEUE_ENTRY    pWorkQueueEntry)
{
    PVOLUME_CONTEXT   pVolumeContext;
    ULONG       ulOutOfSyncErrorCode;
    KIRQL           OldIrql;
    NTSTATUS    Status = (NTSTATUS)pWorkQueueEntry->ulInfo2;

    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    pVolumeContext = (PVOLUME_CONTEXT)pWorkQueueEntry->Context;
    ulOutOfSyncErrorCode = pWorkQueueEntry->ulInfo1;

    // Holding reference to VolumeContext so that BitMap File is not closed 
    // before the writes are completed.
    
    // Clearing usage status of WorkQueueEntry for resync
    KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);

    pVolumeContext->bReSyncWorkQueueRef = false;

    KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);

    SetVolumeOutOfSync(pVolumeContext, ulOutOfSyncErrorCode, Status, false);

    DereferenceVolumeContext(pVolumeContext);
    return;
}

NTSTATUS
QueueWorkerRoutineForSetVolumeOutOfSync(PVOLUME_CONTEXT  pVolumeContext, ULONG ulOutOfSyncErrorCode, NTSTATUS Status, bool bLockAcquired)
{
    PWORK_QUEUE_ENTRY   pWorkQueueEntry = NULL;
    KIRQL           OldIrql;
    if (!bLockAcquired)
        KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
    
    // Check the usage status of WorkQueueEntry
    if (!pVolumeContext->bReSyncWorkQueueRef) {
        pWorkQueueEntry = pVolumeContext->pReSyncWorkQueueEntry;
        pVolumeContext->bReSyncWorkQueueRef = 1;
        // Cleanup of the given WorkQueuEntry required for reusing
        InitializeWorkQueueEntry(pWorkQueueEntry);
    }

    if (!bLockAcquired)
        KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);

    if (NULL != pWorkQueueEntry)
    {
        pWorkQueueEntry->Context = ReferenceVolumeContext(pVolumeContext);
        pWorkQueueEntry->ulInfo1 = ulOutOfSyncErrorCode;
        pWorkQueueEntry->ulInfo2 = Status;
        pWorkQueueEntry->WorkerRoutine = SetVolumeOutOfSyncWorkerRoutine;

        AddItemToWorkQueue(&DriverContext.WorkItemQueue, pWorkQueueEntry);
        return STATUS_SUCCESS;
    }

    return STATUS_INSUFFICIENT_RESOURCES;
}

void
AddResyncRequiredFlag(
    PUDIRTY_BLOCK_V2   UserDirtyBlock,
    PVOLUME_CONTEXT VolumeContext
    )
{
    NTSTATUS    Status;

    ASSERT(NULL != VolumeContext);

    Status = KeWaitForSingleObject(&VolumeContext->Mutex, Executive, KernelMode, FALSE, NULL);
    ASSERT(STATUS_SUCCESS == Status);

    if (VolumeContext->bResyncRequired) {
        VolumeContext->bResyncIndicated = true;
        KeQuerySystemTime(&VolumeContext->liOutOfSyncIndicatedTimeStamp);
        VolumeContext->ulOutOfSyncIndicatedAtCount = VolumeContext->ulOutOfSyncCount;
        // Resync is Required.
        UserDirtyBlock->uHdr.Hdr.ulFlags |= UDIRTY_BLOCK_FLAG_VOLUME_RESYNC_REQUIRED;
        UserDirtyBlock->uHdr.Hdr.ulOutOfSyncCount = VolumeContext->ulOutOfSyncCount;
        UserDirtyBlock->uHdr.Hdr.liOutOfSyncTimeStamp.QuadPart = VolumeContext->liOutOfSyncTimeStamp.QuadPart;
        UserDirtyBlock->uHdr.Hdr.ulOutOfSyncErrorCode = VolumeContext->ulOutOfSyncErrorCode;

        ULONG   ulOutOfSyncErrorCode = VolumeContext->ulOutOfSyncErrorCode;
        if (ulOutOfSyncErrorCode >= ERROR_TO_REG_MAX_ERROR) {
            ulOutOfSyncErrorCode = ERROR_TO_REG_DESCRIPTION_IN_EVENT_LOG;
        }

        Status = RtlStringCbPrintfExA((NTSTRSAFE_PSTR)&UserDirtyBlock->uHdr.Hdr.ErrorStringForResync[0],
                                      UDIRTY_BLOCK_MAX_ERROR_STRING_SIZE,
                                      NULL,
                                      NULL,
                                      STRSAFE_NULL_ON_FAILURE,
                                      ErrorToRegErrorDescriptionsA[ulOutOfSyncErrorCode],
                                      VolumeContext->ulOutOfSyncErrorStatus);
    } else {
        UserDirtyBlock->uHdr.Hdr.ErrorStringForResync[0] = '\0';
    }

    KeReleaseMutex(&VolumeContext->Mutex, FALSE);

}

/*-----------------------------------------------------------------------------
 * Function Name : SetVolumeOutOfSync
 * Parameters :
 *      pVolumeContext : This parameter points to the volume that is 
 *                            out of sync.
 *      ulOutOfSyncErrorCode :  This Error code gives the information of why
 *                            the volume has gone out of sync.
 * Returns :
 *      NTSTATUS  : STATUS_SUCCESS if succeded in marking volume out of sync
 *                          
 * NOTES :
 * This function should be called only at PASSIVE_LEVEL. 
 *
 *      This function marks the volume as out of sync in registry
 *
 *------------------------------------------------------------------------------
 */

NTSTATUS
SetVolumeOutOfSync(
    PVOLUME_CONTEXT   pVolumeContext,
    ULONG       ulOutOfSyncErrorCode,
    NTSTATUS    StatusToLog,
    bool        bLockAcquired
    )
{
    NTSTATUS    Status = STATUS_SUCCESS;
    Registry    *pVolumeParam = NULL;
    LARGE_INTEGER   SystemTime, LocalTime;
    TIME_FIELDS     TimeFields;
    KIRQL           OldIrql;
    ULONGLONG   OutOfSyncTimeStamp;
    ULONGLONG   OutOfSyncSequenceNumber;

    if (!bLockAcquired)
        KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);

    if (pVolumeContext->bQueueChangesToTempQueue) {
        FreeChangeNodeList(&pVolumeContext->ChangeList);
    }

    if (!bLockAcquired)
        KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);

    if ((pVolumeContext->ulFlags & VCF_IGNORE_BITMAP_CREATION) && (INVOLFLT_ERR_BITMAP_FILE_CREATED == ulOutOfSyncErrorCode)) {
        InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, ("SetVolumeOutOfSync: Ignoring bitmap creation as out of sync for volume %s(%s)\n",
                                                pVolumeContext->DriveLetter, pVolumeContext->GUIDinANSI));
        return Status;
    }

    if (bLockAcquired || (PASSIVE_LEVEL != KeGetCurrentIrql())) {
        // This is being called at dispatch level, and as registry can not be accessed
        // at dispatch level, we have to queue a work item to do so.

        Status = QueueWorkerRoutineForSetVolumeOutOfSync(pVolumeContext, ulOutOfSyncErrorCode, StatusToLog, bLockAcquired);
        if (STATUS_SUCCESS == Status)
            return STATUS_PENDING;

        return Status;
    }

    if (IS_NOT_CLUSTER_VOLUME(pVolumeContext)) {
        pVolumeParam = new Registry();
        if (NULL == pVolumeParam)
            return STATUS_INSUFFICIENT_RESOURCES;
    
        Status = pVolumeParam->OpenKeyReadWrite(&pVolumeContext->VolumeParameters, TRUE);
    }

    KeWaitForSingleObject(&pVolumeContext->Mutex, Executive, KernelMode, FALSE, NULL);

    KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
    GenerateTimeStamp(OutOfSyncTimeStamp, OutOfSyncSequenceNumber);
    KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);

    SystemTime.QuadPart = OutOfSyncTimeStamp;
    pVolumeContext->bResyncRequired = true;
    pVolumeContext->ulOutOfSyncCount++;
    pVolumeContext->liOutOfSyncTimeStamp.QuadPart = SystemTime.QuadPart;
    pVolumeContext->ulOutOfSyncErrorStatus = StatusToLog;
    pVolumeContext->ulOutOfSyncErrorCode = ulOutOfSyncErrorCode;

    if (IS_NOT_CLUSTER_VOLUME(pVolumeContext) && NT_SUCCESS(Status)) {
        UNICODE_STRING  ErrorString;
        InMageInitUnicodeString(&ErrorString);

        Status = pVolumeParam->WriteULONG(VOLUME_RESYNC_REQUIRED, 0x00001);
        Status = pVolumeParam->WriteULONG(VOLUME_OUT_OF_SYNC_ERRORCODE, ulOutOfSyncErrorCode);

        if (ulOutOfSyncErrorCode >= ERROR_TO_REG_MAX_ERROR) {
            ulOutOfSyncErrorCode = ERROR_TO_REG_DESCRIPTION_IN_EVENT_LOG;
        }
        Status = InMageAppendUnicodeStringWithFormat(&ErrorString, PagedPool, ErrorToRegErrorDescriptionsW[ulOutOfSyncErrorCode], StatusToLog);
        if (NT_SUCCESS(Status)) {
            Status = InMageTerminateUnicodeStringWith(&ErrorString, L'\0', PagedPool);
            if (NT_SUCCESS(Status)) {
                Status = pVolumeParam->WriteWString(VOLUME_OUT_OF_SYNC_ERROR_DESCRIPTION, ErrorString.Buffer);
            }
        }
        InMageFreeUnicodeString(&ErrorString);
        Status = pVolumeParam->WriteULONG(VOLUME_OUT_OF_SYNC_ERRORSTATUS, StatusToLog);
        Status = pVolumeParam->WriteULONG(VOLUME_OUT_OF_SYNC_COUNT, pVolumeContext->ulOutOfSyncCount);
        ExSystemTimeToLocalTime(&SystemTime, &LocalTime);
        RtlTimeToTimeFields(&LocalTime, &TimeFields);
        // VOLUME_OUT_OF_SYNC_TIMESTAMP is in formation MM:DD:YYYY::HH:MM:SS:XXX(MilliSeconds) 24 + NULL
        Status = RtlStringCbPrintfW(pVolumeContext->BufferForOutOfSyncTimeStamp,
                                    MAX_VOLUME_OUT_OF_SYNC_TIMESTAMP_SIZE_IN_WCHARS * sizeof(WCHAR),
                                    L"%02hu:%02hu:%04hu::%02hu:%02hu:%02hu:%03hu",
                                    TimeFields.Month, TimeFields.Day, TimeFields.Year,
                                    TimeFields.Hour, TimeFields.Minute, TimeFields.Second, TimeFields.Milliseconds);
        if (STATUS_SUCCESS == Status) {
            Status = pVolumeParam->WriteWString(VOLUME_OUT_OF_SYNC_TIMESTAMP, pVolumeContext->BufferForOutOfSyncTimeStamp);
        }

        Status = pVolumeParam->WriteBinary(VOLUME_OUT_OF_SYNC_TIMESTAMP_IN_GMT, &SystemTime.QuadPart, sizeof(SystemTime.QuadPart));
        if (STATUS_SUCCESS != Status) {
            InVolDbgPrint(DL_WARNING, DM_DIRTY_BLOCKS, 
                    ("SetVolumeOutOfSync: Key %ws could not be set, Status: %x\n", VOLUME_OUT_OF_SYNC_TIMESTAMP_IN_GMT, Status));
        }
   } else if (pVolumeContext->ulFlags & VCF_CLUSTER_VOLUME_ONLINE) {
        PVOLUME_BITMAP pVolumeBitmap = NULL;
        KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
        if (pVolumeContext->pVolumeBitmap)
            pVolumeBitmap = ReferenceVolumeBitmap(pVolumeContext->pVolumeBitmap);
        KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);
        if (NULL != pVolumeBitmap) {
            ExAcquireFastMutex(&pVolumeBitmap->Mutex);
            if (pVolumeBitmap->pBitmapAPI) {
                pVolumeBitmap->pBitmapAPI->AddResynRequiredToBitmap(pVolumeContext->ulOutOfSyncErrorCode,
                                                                    StatusToLog,
                                                                    SystemTime.QuadPart, 
                                                                    pVolumeContext->ulOutOfSyncCount,
                                                                    pVolumeContext->ulOutOfSyncIndicatedAtCount);
            }
            ExReleaseFastMutex(&pVolumeBitmap->Mutex);
            DereferenceVolumeBitmap(pVolumeBitmap);
        }
    }

    KeReleaseMutex(&pVolumeContext->Mutex, FALSE);

    if (NULL != pVolumeParam) {
        pVolumeParam->Release();
    }

    return Status;
}

/*-------------------------------------------------------------------------------
 * bool bClear - bClear is set if Resync information has to be cleared. This
 * function is called with bClear set to true when differentials are cleared. We
 * would like to clear resync information in that case.
 *-------------------------------------------------------------------------------
 */ 
void
ResetVolumeOutOfSync(PVOLUME_CONTEXT   VolumeContext, bool bClear)
{
    NTSTATUS        Status = STATUS_SUCCESS;
    Registry        *pVolumeParam = NULL;
    LARGE_INTEGER   SystemTime, LocalTime;
    TIME_FIELDS     TimeFields;
    PVOLUME_BITMAP  pVolumeBitmap = NULL;
    KIRQL           OldIrql;

    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());
    /* BugFix 19342: Resync Required set to YES during Failback Protectection in Step2 */
    /* ClearDiff should reset all the pending resync as Resync step1 is expected*/
    if ((!VolumeContext->bResyncRequired || !VolumeContext->bResyncIndicated) && (!bClear)){
        return;
    }

    pVolumeParam = new Registry();
    KeWaitForSingleObject(&VolumeContext->Mutex, Executive, KernelMode, FALSE, NULL);
    ASSERT(bClear || (VolumeContext->ulOutOfSyncCount >= VolumeContext->ulOutOfSyncIndicatedAtCount));
    if (bClear || (VolumeContext->ulOutOfSyncIndicatedAtCount >= VolumeContext->ulOutOfSyncCount)) {
        // ReSync clear is requested or
        // No resync was set between resync indication and acking the resync action
        VolumeContext->bResyncRequired = false;
        VolumeContext->ulOutOfSyncCount = 0;
        VolumeContext->liLastOutOfSyncTimeStamp.QuadPart = VolumeContext->liOutOfSyncTimeStamp.QuadPart;
        VolumeContext->liOutOfSyncTimeStamp.QuadPart = 0;
        VolumeContext->ulOutOfSyncErrorCode = 0;
        VolumeContext->ulOutOfSyncIndicatedAtCount = 0;
        KeQuerySystemTime(&VolumeContext->liOutOfSyncResetTimeStamp);
        if (IS_NOT_CLUSTER_VOLUME(VolumeContext)) {
            if (NULL != pVolumeParam) {
                Status = pVolumeParam->OpenKeyReadWrite(&VolumeContext->VolumeParameters, TRUE);
                if (NT_SUCCESS(Status)) {
                    Status = pVolumeParam->WriteULONG(VOLUME_RESYNC_REQUIRED, 0x0000);
                    Status = pVolumeParam->DeleteValue(VOLUME_OUT_OF_SYNC_ERRORCODE, STRING_LEN_NULL_TERMINATED);
                    Status = pVolumeParam->DeleteValue(VOLUME_OUT_OF_SYNC_ERRORSTATUS, STRING_LEN_NULL_TERMINATED);
                    Status = pVolumeParam->DeleteValue(VOLUME_OUT_OF_SYNC_ERROR_DESCRIPTION, STRING_LEN_NULL_TERMINATED);
                    Status = pVolumeParam->DeleteValue(VOLUME_OUT_OF_SYNC_COUNT, STRING_LEN_NULL_TERMINATED);
                    Status = pVolumeParam->DeleteValue(VOLUME_OUT_OF_SYNC_TIMESTAMP, STRING_LEN_NULL_TERMINATED);
                    Status = pVolumeParam->DeleteValue(VOLUME_OUT_OF_SYNC_TIMESTAMP_IN_GMT, STRING_LEN_NULL_TERMINATED);
                }
            }
        } else if (VolumeContext->ulFlags & VCF_CLUSTER_VOLUME_ONLINE) {
            KeAcquireSpinLock(&VolumeContext->Lock, &OldIrql);
            if (VolumeContext->pVolumeBitmap)
                pVolumeBitmap = ReferenceVolumeBitmap(VolumeContext->pVolumeBitmap);
            KeReleaseSpinLock(&VolumeContext->Lock, OldIrql);
            if (NULL != pVolumeBitmap) {
                ExAcquireFastMutex(&pVolumeBitmap->Mutex);
                if (pVolumeBitmap->pBitmapAPI) {
                    pVolumeBitmap->pBitmapAPI->ResetResynRequiredInBitmap(VolumeContext->ulOutOfSyncCount,
                                                                          VolumeContext->ulOutOfSyncIndicatedAtCount,
                                                                          VolumeContext->liOutOfSyncIndicatedTimeStamp.QuadPart,
                                                                          VolumeContext->liOutOfSyncResetTimeStamp.QuadPart,
                                                                          true);
                }
                ExReleaseFastMutex(&pVolumeBitmap->Mutex);
                DereferenceVolumeBitmap(pVolumeBitmap);
            }
        }
    } else {
        // Resync is set after indicating resync required. Do not reset bResyncRequired
        VolumeContext->ulOutOfSyncCount -= VolumeContext->ulOutOfSyncIndicatedAtCount;
        if (IS_NOT_CLUSTER_VOLUME(VolumeContext)) {
            if (NULL != pVolumeParam) {
                Status = pVolumeParam->OpenKeyReadWrite(&VolumeContext->VolumeParameters, TRUE);
                if (NT_SUCCESS(Status)) {
                    Status = pVolumeParam->WriteULONG(VOLUME_OUT_OF_SYNC_COUNT, VolumeContext->ulOutOfSyncCount);
                }
            }
        } else if (VolumeContext->ulFlags & VCF_CLUSTER_VOLUME_ONLINE) {
            KeAcquireSpinLock(&VolumeContext->Lock, &OldIrql);
            if (VolumeContext->pVolumeBitmap)
                pVolumeBitmap = ReferenceVolumeBitmap(VolumeContext->pVolumeBitmap);
            KeReleaseSpinLock(&VolumeContext->Lock, OldIrql);
            if (NULL != pVolumeBitmap) {
                ExAcquireFastMutex(&pVolumeBitmap->Mutex);
                if (pVolumeBitmap->pBitmapAPI) {
                    pVolumeBitmap->pBitmapAPI->ResetResynRequiredInBitmap(VolumeContext->ulOutOfSyncCount,
                                                                          VolumeContext->ulOutOfSyncIndicatedAtCount,
                                                                          VolumeContext->liOutOfSyncIndicatedTimeStamp.QuadPart,
                                                                          VolumeContext->liOutOfSyncResetTimeStamp.QuadPart,
                                                                          false);
                }
                ExReleaseFastMutex(&pVolumeBitmap->Mutex);
                DereferenceVolumeBitmap(pVolumeBitmap);
            }
        }
    }

    VolumeContext->bResyncIndicated = false;
    KeReleaseMutex(&VolumeContext->Mutex, FALSE);

    if (NULL != pVolumeParam) {
        pVolumeParam->Release();
    }

    return;
}

// This function is called with volume context lock held.
ULONG
AddMetaDataToDirtyBlock(
    PVOLUME_CONTEXT VolumeContext, 
    PKDIRTY_BLOCK CurrentDirtyBlock, 
    PWRITE_META_DATA pWriteMetaData, 
    ULONG InVolfltDataSource
    )
{
    ULONG MaxDataSizePerDirtyBlock = DriverContext.ulMaxDataSizePerNonDataModeDirtyBlock;
    LONGLONG AvailableSpaceInCurDirtyBlock = 0;
    ULONG NumberOfSplitChanges = 0;
    BOOLEAN Coalesced = FALSE;
    // As Split decision is made on local variable MaxDataSizePerDirtyBlock. 
    // If there is need to create newer blocks then hvae to created of size MaxDataSizePerDirtyBlock 
    if(MaxDataSizePerDirtyBlock < pWriteMetaData->ulLength)
        return SplitChangeIntoDirtyBlock(VolumeContext, pWriteMetaData, InVolfltDataSource, MaxDataSizePerDirtyBlock);

    if(NULL == CurrentDirtyBlock) {
        CurrentDirtyBlock = AllocateDirtyBlock(VolumeContext, MaxDataSizePerDirtyBlock);
        if (NULL == CurrentDirtyBlock) {
            InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("%s: Failed allocation of dirtyblock\n", __FUNCTION__));
            return NumberOfSplitChanges;
        }
        if (!ReserveSpaceForChangeInDirtyBlock(CurrentDirtyBlock)) {
            DeallocateDirtyBlock(CurrentDirtyBlock, true);
            InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("%s: Failed allocation of changeblock\n", __FUNCTION__));
            return NumberOfSplitChanges;
        }
        AddDirtyBlockToVolumeContext(VolumeContext, CurrentDirtyBlock, InVolfltDataSource, false);
    } else {
       /* Check and if req allocate memory for change block. As it may fail later becasue of which cleanup operation can be complicated.
           In case a new dirty block is allocated we will be wasting memory of Change block if allocated*/       
       if (!ReserveSpaceForChangeInDirtyBlock(CurrentDirtyBlock)) {
            InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("%s: Failed allocation of changeblock\n", __FUNCTION__));
            return NumberOfSplitChanges;
        }
    }
    
    if (((CurrentDirtyBlock->cChanges < MAX_KDIRTY_CHANGES) &&
        ((CurrentDirtyBlock->ulMaxDataSizePerDirtyBlock - CurrentDirtyBlock->ulicbChanges.QuadPart) >= pWriteMetaData->ulLength))
              //we will check if the changes are to be added to the temp queue OR
         && ((VolumeContext->bQueueChangesToTempQueue) ||
               //If they are to be added to main queue the delta is not overlflowing.
               //if it overflowing we will close the current dirty block and will use the new one
               (GenerateTimeStampDelta(CurrentDirtyBlock, pWriteMetaData->TimeDelta, pWriteMetaData->SequenceNumberDelta)))

        )
    {
        NTSTATUS Status;
        if((!(CurrentDirtyBlock->ulFlags & KDIRTY_BLOCK_FLAG_CONTAINS_TAGS)) &&
            (!CurrentDirtyBlock->cChanges)){
            //Explicitly making the delta as zero for the first change
            Status = AddChangeToDirtyBlock(CurrentDirtyBlock, 
                pWriteMetaData->llOffset,
                pWriteMetaData->ulLength,
                0,
                0,
                InVolfltDataSource,
                Coalesced);
        } else {
            if (VolumeContext->bQueueChangesToTempQueue) {
                GenerateTimeStampDeltaForUnupdatedDB(CurrentDirtyBlock, pWriteMetaData->TimeDelta, pWriteMetaData->SequenceNumberDelta);
            }
            Status = AddChangeToDirtyBlock(CurrentDirtyBlock, 
                pWriteMetaData->llOffset,
                pWriteMetaData->ulLength,
                pWriteMetaData->TimeDelta,
                pWriteMetaData->SequenceNumberDelta,
                InVolfltDataSource,
                Coalesced);
        }
        if (STATUS_SUCCESS != Status) {
            InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("%s(%d): Failed allocation of changeblock\n", __FUNCTION__, __LINE__));
            return NumberOfSplitChanges;
        }
        CurrentDirtyBlock->ulicbChanges.QuadPart += pWriteMetaData->ulLength;
        AddChangeCountersOnDataSource(&VolumeContext->ChangeList, InVolfltDataSource, 0x01, (ULONGLONG)pWriteMetaData->ulLength);
        if (Coalesced) {
            // Changes may be "Coalesced" only iff InVolfltDataSource = INVOLFLT_DATA_SOURCE_META_DATA
            // If changes are coalesced, we should not increment counters. We re-adjust counters here.
            ASSERT(InVolfltDataSource == INVOLFLT_DATA_SOURCE_META_DATA);
            VolumeContext->ChangeList.ulMetaDataChangesPending -= 0x01;
            VolumeContext->ChangeList.ulTotalChangesPending -= 0x01;
        }
        NumberOfSplitChanges++;
        return NumberOfSplitChanges;
    }

    ASSERT(MaxDataSizePerDirtyBlock >= pWriteMetaData->ulLength);
    CurrentDirtyBlock = AllocateDirtyBlock(VolumeContext, MaxDataSizePerDirtyBlock);
    if (NULL == CurrentDirtyBlock) {
        InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("AddMetaDataToDirtyBlock: Failed allocation of dirtyblock\n"));
        return NumberOfSplitChanges;
    }
    if (!ReserveSpaceForChangeInDirtyBlock(CurrentDirtyBlock)) {
        DeallocateDirtyBlock(CurrentDirtyBlock, true);
        InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("%s: Failed allocation of changeblock\n", __FUNCTION__));
        return NumberOfSplitChanges;
    }

    AddDirtyBlockToVolumeContext(VolumeContext, CurrentDirtyBlock, InVolfltDataSource, false);
    InVolDbgPrint( DL_INFO, DM_DATA_FILTERING | DM_DIRTY_BLOCKS,
                    ("AddMetaDataToDirtyBlock: Added new DirtyBlock in metadata mode\n"));

    NTSTATUS Status;
    //adding the first deltas as zero explicitly
    Status = AddChangeToDirtyBlock(CurrentDirtyBlock, pWriteMetaData->llOffset, pWriteMetaData->ulLength, 0, 0, InVolfltDataSource, Coalesced);

    ASSERT(STATUS_SUCCESS == Status);

    CurrentDirtyBlock->ulicbChanges.QuadPart += pWriteMetaData->ulLength;
    AddChangeCountersOnDataSource(&VolumeContext->ChangeList, InVolfltDataSource, 0x01, (ULONGLONG)pWriteMetaData->ulLength);
    if (Coalesced) {
        // Changes may be "Coalesced" only iff InVolfltDataSource = INVOLFLT_DATA_SOURCE_META_DATA
        // If changes are coalesced, we should not increment counters. We re-adjust counters here.
        ASSERT(InVolfltDataSource == INVOLFLT_DATA_SOURCE_META_DATA);
        VolumeContext->ChangeList.ulMetaDataChangesPending -= 0x01;
        VolumeContext->ChangeList.ulTotalChangesPending -= 0x01;
    }
    
    NumberOfSplitChanges++;
    return NumberOfSplitChanges;
}

// This function is called with volume context lock held.
ULONG
SplitChangeIntoDirtyBlock(
    PVOLUME_CONTEXT VolumeContext, 
    PWRITE_META_DATA pWriteMetaData, 
    ULONG InVolfltDataSource,
    ULONG MaxDataSizePerDirtyBlock
    )
{    
    ULONG RemainingLength = pWriteMetaData->ulLength;
    LONGLONG AvailableSpaceInCurDirtyBlock = 0;
    LONGLONG ByteOffset = pWriteMetaData->llOffset;
    ULONG NumberOfSplitChanges = 0;
    LIST_ENTRY SplitChangeListHead;
    ULONG SplitChangeLength = 0;
    PKDIRTY_BLOCK DirtyBlock = NULL;
    ULONG NumberOfDirtyBlocks = 0;
    BOOLEAN Coalesced = FALSE;

    ASSERT(pWriteMetaData->ulLength > MaxDataSizePerDirtyBlock);

    ByteOffset = pWriteMetaData->llOffset + pWriteMetaData->ulLength;
    NumberOfDirtyBlocks = (pWriteMetaData->ulLength + MaxDataSizePerDirtyBlock - 1) / MaxDataSizePerDirtyBlock;
    InitializeListHead(&SplitChangeListHead);

    while(RemainingLength)
    {
        DirtyBlock = AllocateDirtyBlock(VolumeContext, MaxDataSizePerDirtyBlock);

        if (NULL == DirtyBlock) {
            InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("%s: Failed allocation of dirtyblock\n", __FUNCTION__));
            NumberOfSplitChanges = 0;
            break;
        }

        InsertHeadList(&SplitChangeListHead, &DirtyBlock->ChangeNode->ListEntry);
        if (!ReserveSpaceForChangeInDirtyBlock(DirtyBlock)) {
            InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("%s: Failed allocation of changeblock\n", __FUNCTION__));
            NumberOfSplitChanges = 0;
            break;
        }

        SplitChangeLength = RemainingLength - ((NumberOfDirtyBlocks - 1) * MaxDataSizePerDirtyBlock);
        ByteOffset -= SplitChangeLength;
        RemainingLength -= SplitChangeLength;

        //Sending the deltas as 0. We cannot calculate the delta, the dirty block is not the part of the volume
        //context and we don't have the first change time stamp. As this is the split change and it is stamped with
        //the first change time stamp afterwards this approach will work.
        if (STATUS_SUCCESS != AddChangeToDirtyBlock(DirtyBlock, ByteOffset, SplitChangeLength,0,0, InVolfltDataSource, Coalesced)) {
            InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("%s: Failed allocation of dirtyblock\n", __FUNCTION__));
            NumberOfSplitChanges = 0;
            break;
        }

        DirtyBlock->ulicbChanges.QuadPart += SplitChangeLength;
        DirtyBlock->ulSequenceIdForSplitIO = NumberOfDirtyBlocks;
        DirtyBlock->ulFlags |= KDIRTY_BLOCK_FLAG_PART_OF_SPLIT_CHANGE;
        NumberOfDirtyBlocks--;
        NumberOfSplitChanges++;
    }

    if(0 == NumberOfSplitChanges)
    {
        //Some Error Ocurred, Delete all dirty blocks
        while(!IsListEmpty(&SplitChangeListHead))
        {
            PCHANGE_NODE ChangeNode = (PCHANGE_NODE)RemoveHeadList(&SplitChangeListHead);
            DirtyBlock = ChangeNode->DirtyBlock;
            DeallocateDirtyBlock(DirtyBlock, true);
        }
        return NumberOfSplitChanges;
    }

    ((PCHANGE_NODE)SplitChangeListHead.Flink)->DirtyBlock->ulFlags |= KDIRTY_BLOCK_FLAG_START_OF_SPLIT_CHANGE;
    ((PCHANGE_NODE)SplitChangeListHead.Flink)->DirtyBlock->ulFlags &= ~KDIRTY_BLOCK_FLAG_PART_OF_SPLIT_CHANGE;

    ((PCHANGE_NODE)SplitChangeListHead.Blink)->DirtyBlock->ulFlags |= KDIRTY_BLOCK_FLAG_END_OF_SPLIT_CHANGE;
    ((PCHANGE_NODE)SplitChangeListHead.Blink)->DirtyBlock->ulFlags &= ~KDIRTY_BLOCK_FLAG_PART_OF_SPLIT_CHANGE;

    bool Continuation = false;
    while(!IsListEmpty(&SplitChangeListHead))
    {
        PCHANGE_NODE ChangeNode = (PCHANGE_NODE)RemoveHeadList(&SplitChangeListHead);      //Extract in LIFO order
        AddDirtyBlockToVolumeContext(VolumeContext, ChangeNode->DirtyBlock, InVolfltDataSource, Continuation);
        Continuation = true;
    }

    AddChangeCountersOnDataSource(&VolumeContext->ChangeList, InVolfltDataSource, NumberOfSplitChanges, (ULONGLONG)pWriteMetaData->ulLength);

    return NumberOfSplitChanges;
}
