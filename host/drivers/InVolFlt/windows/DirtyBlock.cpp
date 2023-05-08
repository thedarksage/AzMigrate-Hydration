#include "Common.h"
#include "DirtyBlock.h"
#include "DriverContext.h"
#include "global.h"
#include "VBitmap.h"
#include "misc.h"
#include "TagNode.h"
#include "HelperFunctions.h"
#include "VContext.h"

/*-----------------------------------------------------------------------------
    Functions related to PCHANGE_BLOCK
 *-----------------------------------------------------------------------------
 */

#define COALESCE_ARRAY_LENGTH   10
ULONG CoalesceArray[10];
//DTAP start

extern CHAR *ErrorToRegErrorDescriptionsA[];

//DTAP end

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
AllocateDirtyBlock(PDEV_CONTEXT DevContext, ULONG ulMaxDataSizePerDirtyBlock)
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
    pDirtyBlock->DevContext = ReferenceDevContext(DevContext);
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
    pDirtyBlock->ulDataSource = INFLTDRV_DATA_SOURCE_UNDEFINED;
    pDirtyBlock->eWOState = DevContext->eWOState;
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
    pDirtyBlock->pTagHistory = NULL;

    return pDirtyBlock;
}

VOID
DBFreeDataBlocks(PKDIRTY_BLOCK DirtyBlock, bool bVCLockAcquired)
{
    PDEV_CONTEXT DevContext = DirtyBlock->DevContext;
    ASSERT(NULL != DevContext);
    KIRQL   OldIrql = 0;

    // Free Datablock list.
    while(!IsListEmpty(&DirtyBlock->DataBlockList)) {
        PDATA_BLOCK DataBlock = (PDATA_BLOCK)RemoveHeadList(&DirtyBlock->DataBlockList);
        DeallocateDataBlock(DataBlock, bVCLockAcquired);
        ASSERT(DevContext->lDataBlocksInUse > 0);
        InterlockedDecrement(&DevContext->lDataBlocksInUse);
        if (DataBlock == DirtyBlock->CurrentDataBlock) {
            DirtyBlock->CurrentDataBlock = NULL;
        }
    }

    ASSERT(NULL == DirtyBlock->CurrentDataBlock);
    if (!bVCLockAcquired)
        KeAcquireSpinLock(&DevContext->Lock, &OldIrql);
    ASSERT(DevContext->ulcbDataAllocated >= (DriverContext.ulDataBlockSize * DirtyBlock->ulDataBlocksAllocated));
    DevContext->ulcbDataAllocated -= DirtyBlock->ulDataBlocksAllocated * DriverContext.ulDataBlockSize;
    if (!bVCLockAcquired)
        KeReleaseSpinLock(&DevContext->Lock, OldIrql);

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
    if (NULL != pDirtyBlock->pTagHistory) {
        ExFreePoolWithTag(pDirtyBlock->pTagHistory, TAG_TELEMETRY_INFO);
        pDirtyBlock->pTagHistory = NULL;
    }
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
    ASSERT(NULL != pDirtyBlock->DevContext);
    DereferenceDevContext(pDirtyBlock->DevContext);
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
MetaDataChangeCoalesce(PKDIRTY_BLOCK DirtyBlock, LONGLONG Offset, ULONG Length, ULONG ulIndex, ULONG uDataSource)
{
    PCHANGE_BLOCK ChangeBlock = DirtyBlock->CurrentChangeBlock;
    ULONG newIndex = 0;
    
    if (uDataSource != INFLTDRV_DATA_SOURCE_META_DATA || ulIndex == 0) {
        // Dont Coalesce Data mode and Bitmap mode changes.
        // A DirtyBlock either contains all DATA mode changes OR all non-DATA
        // mode changes. Never a mix of data and non-data changes.
        return 0;
    }
    // See the comments in function AddMetaDataToDevContextNew(). 
	// This Assert hit as part of supporting 32-bit Global Sequence Number support
    //ASSERT(ecCaptureModeData != DirtyBlock->DevContext->eCaptureMode);

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
ULONG uDataSource,
BOOLEAN &Coalesced)
{
    NTSTATUS Status = STATUS_SUCCESS;
    if (uDataSource != INFLTDRV_DATA_SOURCE_DATA) {
        ASSERT(DirtyBlock->cChanges < MAX_KDIRTY_CHANGES);
    }

    ULONG ulIndex = DirtyBlock->cChanges;

    Coalesced = FALSE;
    if (MetaDataChangeCoalesce(DirtyBlock, Offset, Length, ulIndex, uDataSource)) {
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
     if (INFLTDRV_DATA_SOURCE_DATA != DirtyBlock->ulDataSource) {
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
        ulElements = ChangeBlock->usLastIndex - ChangeBlock->usFirstIndex + 1;
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
 * This function has to be called without acquiring DevContext lock
 *-----------------------------------------------------------------------------
 */
NTSTATUS
CommitDirtyBlockTransaction(
    PDEV_CONTEXT        DevContext,
    PCOMMIT_TRANSACTION pCommitTransaction
    )
{
    KIRQL           OldIrql;
    PKDIRTY_BLOCK   DirtyBlock = NULL;
    NTSTATUS        Status = STATUS_SUCCESS;
    bool            isCommitNotifyPending = false;

    if ((NULL == pCommitTransaction) || (NULL == DevContext))
        return STATUS_INVALID_PARAMETER;

    if (pCommitTransaction->ulFlags & COMMIT_TRANSACTION_FLAG_RESET_RESYNC_REQUIRED_FLAG) {
        ResetDevOutOfSync(DevContext, false);
    }

    FreeOrphanedMappedDataBlocksWithDevID((HANDLE)DevContext, PsGetCurrentProcessId());

	KeQuerySystemTime(&DevContext->liCommitDirtyBlockTimeStamp);

    if (0 == pCommitTransaction->ulTransactionID.QuadPart)
    {
        goto cleanup;
    }

    LIST_ENTRY      ListHead;

    InVolDbgPrint(DL_TRACE_L1, DM_DATA_FILTERING,
                    ("CommitDirtyBlockTransaction: (Before) Alloced = %#x Missed = %#I64x Free = %#x Reserved = %#x\n",
                    DevContext->ulcbDataAllocated, DevContext->ullcbDataAlocMissed,
                    DevContext->ulcbDataFree, DevContext->ulcbDataReserved));

    if (DriverContext.ulValidationLevel >= ADD_TO_EVENT_LOG && ((NULL != DevContext->pDBPendingTransactionConfirm) && 
            (pCommitTransaction->ulTransactionID.QuadPart == DevContext->pDBPendingTransactionConfirm->uliTransactionID.QuadPart))) {

        DirtyBlock = DevContext->pDBPendingTransactionConfirm;                
        if (ecChangeNodeDataFile == DirtyBlock->ChangeNode->eChangeNode) {
            if (DevContext->bResyncStartReceived == true) {
                if (DevContext->LastCommittedTimeStamp <= DevContext->ResynStartTimeStamp &&
                    DevContext->ResynStartTimeStamp <= DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601 &&
                    DevContext->ResynStartTimeStamp <= DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601) {
                        DevContext->bResyncStartReceived = false;
                        // "First diff file after Resync start (in Data File mode) commited, Volume name: %s, PrevEndTS: %I64x, ResyncStartTS: %I64x, CurrentEndTS: %I64x, CurrentStartTS: %I64x."
                        InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_FIRSTFILE_VALIDATION_C1,
                                DevContext,
                                DevContext->LastCommittedTimeStamp,
                                DevContext->ResynStartTimeStamp,
                                DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
                                DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
                }
                if (DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601 < DevContext->ResynStartTimeStamp &&
                    DevContext->ResynStartTimeStamp < DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601) {
                        DevContext->bResyncStartReceived = false;
                        // "Start-End TS Problem with First diff file after Resync start (in Data File mode) commited, Volume name: %s, PrevEndTS: %I64x, ResyncStartTS: %I64x, CurrentEndTS: %I64x, CurrentStartTS: %I64x."
                        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_FIRSTFILE_VALIDATION_C2,
                                DevContext,
                                DevContext->LastCommittedTimeStamp,
                                DevContext->ResynStartTimeStamp,
                                DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
                                DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
                }        
                if (DevContext->LastCommittedTimeStamp > DevContext->ResynStartTimeStamp) {
                        DevContext->bResyncStartReceived = false;
                        // "PrevEndTS Problem with First diff file after Resync start (in Data File mode) commited, Volume name: %s, PrevEndTS: %I64x, ResyncStartTS: %I64x, CurrentEndTS: %I64x, CurrentStartTS: %I64x."
                        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_FIRSTFILE_VALIDATION_C3,
                                DevContext,
                                DevContext->LastCommittedTimeStamp,
                                DevContext->ResynStartTimeStamp,
                                DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
                                DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
                }
            }
        }else {
            if (DevContext->bResyncStartReceived == true) {
                if (DevContext->LastCommittedTimeStamp <= DevContext->ResynStartTimeStamp &&
                    DevContext->ResynStartTimeStamp <= DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601 &&
                    DevContext->ResynStartTimeStamp <= DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601) {
                        DevContext->bResyncStartReceived = false;
                        // "First diff file after Resync start (Not in Data File mode) commited, Volume name: %s, PrevEndTS: %I64x, ResyncStartTS: %I64x, CurrentEndTS: %I64x, CurrentStartTS: %I64x."
                        InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_FIRSTFILE_VALIDATION_C4,
                                DevContext,
                                DevContext->LastCommittedTimeStamp,
                                DevContext->ResynStartTimeStamp,
                                DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
                                DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
                }
                if (DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601 < DevContext->ResynStartTimeStamp &&
                    DevContext->ResynStartTimeStamp < DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601) {
                        DevContext->bResyncStartReceived = false;
                        // "Start-End TS Problem with First diff file after Resync start (Not in Data File mode) commited, Volume name: %s, PrevEndTS: %I64x, ResyncStartTS: %I64x, CurrentEndTS: %I64x, CurrentStartTS: %I64x."
                        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_FIRSTFILE_VALIDATION_C5,
                                DevContext,
                                DevContext->LastCommittedTimeStamp,
                                DevContext->ResynStartTimeStamp,
                                DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
                                DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
                }        
                if (DevContext->LastCommittedTimeStamp > DevContext->ResynStartTimeStamp) {
                        DevContext->bResyncStartReceived = false;
                        // "PrevEndTS Problem with First diff file after Resync start (Not in Data File mode) commited, Volume name: %s, PrevEndTS: %I64x, ResyncStartTS: %I64x, CurrentEndTS: %I64x, CurrentStartTS: %I64x."
                        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_FIRSTFILE_VALIDATION_C6,
                                DevContext,
                                DevContext->LastCommittedTimeStamp,
                                DevContext->ResynStartTimeStamp,
                                DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
                                DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
                }
            }
        }
    }
        
    InitializeListHead(&ListHead);
    KeAcquireSpinLock(&DevContext->Lock, &OldIrql);

    if ((ecCaptureModeMetaData == DevContext->eCaptureMode) && 
        NeedWritingToDataFile(DevContext) && 
        DevContext->bDataLogLimitReached && 
        (DevContext->ullcbDataToDiskLimit  > DevContext->ullcbDataWrittenToDisk ) && 
        ((DevContext->ullcbDataToDiskLimit - DevContext->ullcbDataWrittenToDisk ) >= DevContext->ullcbMinFreeDataToDiskLimit))
    {                
        QueueChangeNodesToSaveAsFile(DevContext,TRUE);
        DevContext->bDataLogLimitReached = false;         
    }
    bool bPendingTransaction = true;
    bool bCaptureModeCheck = false;

    if ((NULL == DevContext->pDBPendingTransactionConfirm) && (0 == DevContext->ulPendingTSOTransactionID.QuadPart)) {
        bPendingTransaction = false;
    }

    if ((NULL != DevContext->pDBPendingTransactionConfirm) && 
        (pCommitTransaction->ulTransactionID.QuadPart == DevContext->pDBPendingTransactionConfirm->uliTransactionID.QuadPart)) 
    {
        DirtyBlock = DevContext->pDBPendingTransactionConfirm;
        DevContext->pDBPendingTransactionConfirm = NULL;

        LARGE_INTEGER       PerformanceFrequency, CurrentTickCount, liMicroSeconds;
        CurrentTickCount = KeQueryPerformanceCounter(&PerformanceFrequency);

        liMicroSeconds.QuadPart = CurrentTickCount.QuadPart - DirtyBlock->liTickCountAtUserRequest.QuadPart;
        liMicroSeconds.QuadPart = liMicroSeconds.QuadPart * 1000 * 1000 / PerformanceFrequency.QuadPart;

        // Update Commit statistics
        if ((INFLTDRV_DATA_SOURCE_DATA == DirtyBlock->ulDataSource) &&
            (DevContext->LatencyDistForCommit || DevContext->LatencyLogForCommit))
        {
            if (0 == liMicroSeconds.HighPart) {
                if (DevContext->LatencyDistForCommit) {
                    DevContext->LatencyDistForCommit->AddToDistribution(liMicroSeconds.LowPart);
                }

                if (DevContext->LatencyLogForCommit) {
                    DevContext->LatencyLogForCommit->AddToLog(liMicroSeconds.LowPart);
                }
                DevContext->ullTotalCommitLatencyInDataMode += liMicroSeconds.LowPart;
            }
        } 

        if ((INFLTDRV_DATA_SOURCE_META_DATA == DirtyBlock->ulDataSource) || (INFLTDRV_DATA_SOURCE_BITMAP == DirtyBlock->ulDataSource)) {
            if (0 == liMicroSeconds.HighPart)
                DevContext->ullTotalCommitLatencyInNonDataMode += liMicroSeconds.LowPart;
        }
        if (DirtyBlock->ulFlags & KDIRTY_BLOCK_FLAG_CONTAINS_TAGS) {
            if (DirtyBlock->pTagHistory) {
                InDskFltTelemetryLogTagHistory(DirtyBlock, DevContext, ecTagStatusTagCommitDBSuccess, ecNotApplicable, ecMsgTagCommitDBSuccess);
            }
            DevContext->ullLastCommittedTagTimeStamp = DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601;
            DevContext->ullLastCommittedTagSequeneNumber = DirtyBlock->TagTimeStampOfLastChange.ullSequenceNumber;

            if (TEST_FLAG(DirtyBlock->ulFlags, KDIRTY_BLOCK_FLAG_TAG_NOTIFY) &&
                (DevContext->TagCommitNotifyContext.isCommitNotifyTagPending) &&
                (DEVICE_TAG_NOTIFY_STATE_TAG_INSERTED == DevContext->TagCommitNotifyContext.TagState)) {
                DevContext->TagCommitNotifyContext.ullCommitTimeStamp = DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601;
                DevContext->TagCommitNotifyContext.ullInsertTimeStamp = DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601;
                DevContext->TagCommitNotifyContext.ullSequenceNumber = DirtyBlock->TagTimeStampOfLastChange.ullSequenceNumber;
                DevContext->TagCommitNotifyContext.TagState = DEVICE_TAG_NOTIFY_STATE_TAG_COMMITTED;
                isCommitNotifyPending = true;
                if (DirtyBlock->pTagHistory) {
                    InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_DISK_TAG_COMMITTED, 
                                                  DevContext,
                                                  DirtyBlock->pTagHistory->TagMarkerGUID,
                        DevContext->TagCommitNotifyContext.ullInsertTimeStamp,
                        DevContext->TagCommitNotifyContext.ullSequenceNumber
                        );
                }
            }
        }
        DevContext->LastCommittedTimeStamp = DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601;
        DevContext->LastCommittedSequenceNumber = DirtyBlock->TagTimeStampOfLastChange.ullSequenceNumber;
        DevContext->LastCommittedSplitIoSeqId = DirtyBlock->ulSequenceIdForSplitIO;
        DevContext->liTickCountAtNotifyEventSet.QuadPart = 0;
        if ((ecWriteOrderStateData == DirtyBlock->eWOState) || (ecWriteOrderStateMetadata == DirtyBlock->eWOState))
            DevContext->EndTimeStampofDB = DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601;
            
        ASSERT(DevContext->ullcbChangesPendingCommit >= DirtyBlock->ulicbChanges.QuadPart);
        ASSERT(DevContext->ulChangesPendingCommit >= DirtyBlock->cChanges);
        DevContext->ullcbChangesPendingCommit -= DirtyBlock->ulicbChanges.QuadPart;
        DevContext->ulChangesPendingCommit -= DirtyBlock->cChanges;
        DevContext->uliChangesReturnedToService.QuadPart += DirtyBlock->cChanges;
        DevContext->ulicbReturnedToService.QuadPart += DirtyBlock->ulicbChanges.QuadPart;
        DeductChangeCountersOnDataSource(&DevContext->ChangeList, DirtyBlock->ulDataSource,
                                            DirtyBlock->cChanges, DirtyBlock->ulicbChanges.QuadPart);
        DevContext->lDirtyBlocksInPendingCommitQueue--;
        bCaptureModeCheck = true;
          
            if (DirtyBlock->ulDataSource != INVOLFLT_DATA_SOURCE_DATA) {
            DevContext->liChangesReturnedToServiceInNonDataMode.QuadPart += DirtyBlock->cChanges;
            DevContext->licbReturnedToServiceInNonDataMode.QuadPart += DirtyBlock->ulicbChanges.QuadPart;
            } else {
                DevContext->licbReturnedToServiceInDataMode.QuadPart += DirtyBlock->ulicbChanges.QuadPart;
            }

    } else if ((0 != DevContext->ulPendingTSOTransactionID.QuadPart) && 
                (pCommitTransaction->ulTransactionID.QuadPart == DevContext->ulPendingTSOTransactionID.QuadPart))
    {

        if (DriverContext.ulValidationLevel >= ADD_TO_EVENT_LOG && DevContext->bResyncStartReceived == true) {
            if (DevContext->LastCommittedTimeStamp <= DevContext->ResynStartTimeStamp &&
                DevContext->ResynStartTimeStamp <= DevContext->TSOEndTimeStamp) {                
                    DevContext->bResyncStartReceived = false;
                    // "First diff file after Resync start (TSO file) commited for disk %s. PrevEndTS: %I64x, ResyncStartTS: %I64x, TSOEndTS: %I64x."
                    InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_FIRSTFILE_VALIDATION9,
                        DevContext,
                        DevContext->LastCommittedTimeStamp,
                        DevContext->ResynStartTimeStamp,
                        DevContext->TSOEndTimeStamp);
            }
            if (DevContext->LastCommittedTimeStamp > DevContext->ResynStartTimeStamp) {
                    DevContext->bResyncStartReceived = false;
                    // "PrevEndTS Problem with First diff file after Resync start (TSO file) commited, Volume name: %s, PrevEndTS: %I64x, ResyncStartTS: %I64x, TSOEndTS: %I64x."
                    InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_FIRSTFILE_VALIDATION10,
                            DevContext,
                            DevContext->LastCommittedTimeStamp,
                            DevContext->ResynStartTimeStamp,
                            DevContext->TSOEndTimeStamp);
            }
        }
        
        DevContext->ulPendingTSOTransactionID.QuadPart = 0;

        DevContext->LastCommittedTimeStamp = DevContext->TSOEndTimeStamp;
        DevContext->LastCommittedSplitIoSeqId = DevContext->TSOSequenceId;
        DevContext->LastCommittedSequenceNumber = DevContext->TSOEndSequenceNumber;
        if ((ecWriteOrderStateData == DevContext->eWOState) || (ecWriteOrderStateMetadata == DevContext->eWOState))
            DevContext->EndTimeStampofDB  = DevContext->TSOEndTimeStamp;

        DevContext->ullCounterTSODrained++;
        bCaptureModeCheck = true;
    } else {
        DevContext->DiskTelemetryInfo.ullCommitDBFailCount++;
        Status = STATUS_UNSUCCESSFUL;
    }

    if (DevContext->DBNotifyEvent) {
        if (DirtyBlockReadyForDrain(DevContext)) {
            DevContext->liTickCountAtNotifyEventSet = KeQueryPerformanceCounter(NULL);
            KeSetEvent(DevContext->DBNotifyEvent, EVENT_INCREMENT, FALSE);
            // Avoid generating new events until next dirty block is committed
            DevContext->bNotify = false;
        }
        else {
            // Clear any earlier set event to avoid premature closure of current dirty block.
            KeClearEvent(DevContext->DBNotifyEvent);
            // Set an event if new dirty block is ready
            DevContext->bNotify = true;
        }
    }

    DevContext->DiskTelemetryInfo.ullCommitDBCount++;

    // Check if capture mode can be changed.
    if (bCaptureModeCheck) {
        if ((ecCaptureModeMetaData == DevContext->eCaptureMode) && CanSwitchToDataCaptureMode(DevContext)) {
            InVolDbgPrint(DL_INFO, DM_CAPTURE_MODE | DM_DATA_FILTERING,
                            ("%s: Changing capture mode to DATA for device %s(%s)\n", __FUNCTION__,
                            DevContext->chDevID, DevContext->chDevNum));
            VCSetCaptureModeToData(DevContext);
        }

        // Check if Write order state can be changed up to meta-data or data
        VCSetWOState(DevContext, false, __FUNCTION__, ecMDReasonNotApplicable);

        // Check if bitmap read has to be continued or restarted.
        if ((ecWriteOrderStateBitmap == DevContext->eWOState) && (NULL != DevContext->pDevBitmap) &&
                (DevContext->ChangeList.ulTotalChangesPending <  DriverContext.DBLowWaterMarkWhileServiceRunning))
        {
            if (ecVBitmapStateReadPaused == DevContext->pDevBitmap->eVBitmapState)
                AddVCWorkItemToList(ecWorkItemContinueBitmapRead, DevContext, 0, FALSE, &ListHead);
            else if ((ecVBitmapStateAddingChanges == DevContext->pDevBitmap->eVBitmapState) ||
                        (ecVBitmapStateOpened == DevContext->pDevBitmap->eVBitmapState)) 
            {
                AddVCWorkItemToList(ecWorkItemStartBitmapRead, DevContext, 0, FALSE, &ListHead);
            }
        }
    }
    KeReleaseSpinLock(&DevContext->Lock, OldIrql);

    // IOCTL sequence GetDB->Clear Diff->Commit DB path returns STATUS_UNSUCCESFUL and appears as false alarm. 
    // Returning status success so that user space does not worry about this.
    if (false == bPendingTransaction)
        Status = STATUS_SUCCESS;

    if (NULL != DirtyBlock) {
        if (DirtyBlock->ulFlags & KDIRTY_BLOCK_FLAG_TAG_WAITING_FOR_COMMIT) {
            PTAG_NODE TagNode = AddStatusAndReturnNodeIfComplete(DirtyBlock->TagGUID, DirtyBlock->ulTagDevIndex, ecTagStatusCommited);
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
        ProcessDevContextWorkItems(pWorkItem);
        DereferenceWorkQueueEntry(pWorkItem);
    }
        
    InVolDbgPrint(DL_TRACE_L1, DM_DATA_FILTERING,
                    ("CommitDirtyBlockTransaction: (After) Alloced = %#x Missed = %#I64x Free = %#x Reserved = %#x\n",
                    DevContext->ulcbDataAllocated, DevContext->ullcbDataAlocMissed,
                    DevContext->ulcbDataFree, DevContext->ulcbDataReserved));

cleanup:
    if (isCommitNotifyPending)
    {
        IndskFltCompleteTagNotifyIfDone(DevContext);
    }
    return Status;
}

/*-----------------------------------------------------------------------------
 * OrphanDataBlocksAndMoveCommitPendingDirtyBlockToMainQueue
 *
 * This function has to be called with DevContext lock acquired.
 *-----------------------------------------------------------------------------
 */
VOID
OrphanMappedDataBlocksAndMoveCommitPendingDirtyBlockToMainQueue(
    PDEV_CONTEXT     pDevContext
    )
{
    if (NULL == pDevContext)
        return;

    if (NULL != pDevContext->pDBPendingTransactionConfirm) {
        PKDIRTY_BLOCK   DirtyBlock = pDevContext->pDBPendingTransactionConfirm;
        pDevContext->pDBPendingTransactionConfirm = NULL;
        pDevContext->lDirtyBlocksInPendingCommitQueue--;

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
        PrependChangeNodeToDevContext(pDevContext, DirtyBlock->ChangeNode);
    }
}


PKDIRTY_BLOCK
GetNextDirtyBlock(PKDIRTY_BLOCK DirtyBlock, PDEV_CONTEXT DevContext)
{
    ASSERT(NULL != DirtyBlock);
#if  !DBG 
    UNREFERENCED_PARAMETER(DevContext);
#endif

    PCHANGE_NODE    ChangeNode = DirtyBlock->ChangeNode;
    ASSERT(NULL != ChangeNode);
    ChangeNode = (PCHANGE_NODE)ChangeNode->ListEntry.Flink;
    ASSERT((PLIST_ENTRY)ChangeNode != &DevContext->ChangeList.Head);

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
    PDEV_CONTEXT        DevContext,
    bool                bLockAcquired,
    bool                bUser
    )
{
    NTSTATUS        Status = STATUS_SUCCESS;
    KIRQL           OldIrql = 0;
    ULONG           PrevMaxBitmapHdrGroups;

    const ULONG     LatestHeaderSize = MAX_WRITE_GROUPS_IN_BITMAP_HEADER_CURRENT;

    ASSERT(NULL != DevContext);

    if (!bLockAcquired)
        KeAcquireSpinLock(&DevContext->Lock, &OldIrql);

    InVolDbgPrint(DL_INFO, DM_DEVICE_STATE, 
                  ("%s: Starting filtering on Device %s(%s)\n", __FUNCTION__,
                             DevContext->chDevID, DevContext->chDevNum));

    KeAcquireSpinLockAtDpcLevel(&DriverContext.vmCxSessionContext.CxSessionLock);
    if (!DevContext->isInVmCxSessionFilteredList) {
        ++DriverContext.vmCxSessionContext.ulNumFilteredDisks;
        DevContext->isInVmCxSessionFilteredList = TRUE;
    }
    KeReleaseSpinLockFromDpcLevel(&DriverContext.vmCxSessionContext.CxSessionLock);

    if (TEST_FLAG(DevContext->ulFlags, DCF_FILTERING_STOPPED)) {
        NT_ASSERT(bUser);

        if (DriverContext.bTooManyLastChanceWritesHit) {
            PrevMaxBitmapHdrGroups = DevContext->ulMaxBitmapHdrWriteGroups;
            if (DevContext->ulMaxBitmapHdrWriteGroups < LatestHeaderSize) {
                DevContext->ulMaxBitmapHdrWriteGroups = LatestHeaderSize;
            }

            if (PrevMaxBitmapHdrGroups < LatestHeaderSize) {
                InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_MAX_HDR_GROUPS_UPDATED,
                    DevContext,
                    PrevMaxBitmapHdrGroups,
                    LatestHeaderSize);
            }
        }
    }

    CLEAR_FLAG(DevContext->ulFlags, DCF_FILTERING_STOPPED);

    DevContext->llDevSize = DevContext->llActualDeviceSize;
    KeQuerySystemTime(&DevContext->liStartFilteringTimeStamp);
    if (bUser)
        KeQuerySystemTime(&DevContext->liStartFilteringTimeStampByUser);

    CLEAR_FLAG(DevContext->DiskTelemetryInfo.ullDiskBlendedState, DBS_FILTERING_STOPPED_BY_KERNEL);
    CLEAR_FLAG(DevContext->DiskTelemetryInfo.ullDiskBlendedState, DBS_FILTERING_STOPPED_BY_USER);
    
    DevContext->eWOState = ecWriteOrderStateBitmap;
    DevContext->liTickCountAtLastCaptureModeChange = DevContext->liTickCountAtLastWOStateChange = KeQueryPerformanceCounter(NULL);
    DevContext->eCaptureMode = ecCaptureModeMetaData;
    InVolDbgPrint(DL_INFO, DM_CAPTURE_MODE | DM_WO_STATE, 
                  ("%s: Device %s(%s): Setting capture mode to ecCaptureModeMetaData.\n\t\tWOState to ecWriteOrderStateBitmap\n", __FUNCTION__,
                             DevContext->chDevID, DevContext->chDevNum));

    if (!bLockAcquired)
        KeReleaseSpinLock(&DevContext->Lock, OldIrql);

    if (bUser) {
        InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_FILTERING_STARTED_BY_USER, DevContext, DevContext->ulFlags);
    }
    else {
        InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_FILTERING_STARTED, DevContext, DevContext->ulFlags);
    }

    return Status;
}
/*-----------------------------------------------------------------------------
 * Function Name : StopFilteringDevice
 * Parameters :
 *      pDevContext : This parameter points to the device filtering has
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
    PDEV_CONTEXT DevContext,
    bool            bLockAcquired,
    bool            bClearBitmapFile,
    bool            bCloseBitmapFile,
    PDEV_BITMAP     *ppDevBitmap
    )
{
    NTSTATUS        Status = STATUS_SUCCESS;
    KIRQL           OldIrql = 0;
    PDEV_BITMAP     pDevBitmap = NULL;

    ASSERT(NULL != DevContext);

    if (NULL == DevContext)
        return Status;

    if (ppDevBitmap) {
        *ppDevBitmap = NULL;
    }

    if (!bLockAcquired)
        KeAcquireSpinLock(&DevContext->Lock, &OldIrql);

    InVolDbgPrint(DL_INFO, DM_DEVICE_STATE, 
                  ("%s: Stoping filtering on device %s(%s)\n", __FUNCTION__,
                             DevContext->chDevID, DevContext->chDevNum));

    // Let us free the commit pending dirty blocks too.
    OrphanMappedDataBlocksAndMoveCommitPendingDirtyBlockToMainQueue(DevContext);

    SET_FLAG(DevContext->DiskTelemetryInfo.ullDiskBlendedState, DBS_FILTERING_STOPPED_BY_KERNEL);
    FreeChangeNodeList(&DevContext->ChangeList, ecFilteringStopped);
    CxSessionStopFilteringOrRemoveDevice(DevContext);
    SET_FLAG(DevContext->ulFlags, DCF_FILTERING_STOPPED);
    KeQuerySystemTime(&DevContext->liStopFilteringTimeStamp);
    pDevBitmap = DevContext->pDevBitmap;
    if (bCloseBitmapFile) {
        DevContext->pDevBitmap = NULL;
    }

    VCFreeReservedDataBlockList(DevContext);
    DevContext->ulcbDataFree = 0;
    DevContext->ullcbDataAlocMissed += DevContext->ulcbDataReserved;
    DevContext->ulcbDataReserved = 0;

    // TODO: Check and see if these values have to be different for cases
    // if bitmap file is closed or not? When filtering mode is used the states are set as
    // following.
    // if (bCloseBitmapFile) {
    //    DevContext->pDevBitmap = NULL;
    //    DevContext->ePrevFilteringMode = ecFilteringModeUninitialized;
    //    DevContext->eFilteringMode = ecFilteringModeUninitialized;
    // } else {
    //    DevContext->eFilteringMode = ecFilteringModeMetaData;
    // }

    // Should this be Uninitialized or MetaData?
    DevContext->eCaptureMode = ecCaptureModeUninitialized;
    DevContext->ePrevWOState = ecWriteOrderStateUnInitialized;
    DevContext->eWOState = ecWriteOrderStateUnInitialized;
	// Added for OOD
    if ((DevContext->bNotify) && (NULL != DevContext->DBNotifyEvent)) {
        DevContext->liTickCountAtNotifyEventSet = KeQueryPerformanceCounter(NULL);
        KeSetEvent(DevContext->DBNotifyEvent, EVENT_INCREMENT, FALSE);
    }
    DevContext->LastCommittedTimeStamp = 0;
    DevContext->LastCommittedSequenceNumber = 0;
    DevContext->LastCommittedSplitIoSeqId = 0;
    DevContext->EndTimeStampofDB = 0;
    InVolDbgPrint(DL_INFO, DM_CAPTURE_MODE | DM_WO_STATE, 
                  ("%s: Device %s(%s): Setting capture mode to ecCaptureModeMetaData.\n\t\tWOState to ecWriteOrderStateUnInitialized\n", __FUNCTION__,
                             DevContext->chDevID, DevContext->chDevNum));
    
    if (!bLockAcquired)
        KeReleaseSpinLock(&DevContext->Lock, OldIrql);

    if (NULL != pDevBitmap) {
        if (!bLockAcquired) {
            if (bCloseBitmapFile) {
                CloseBitmapFile(pDevBitmap, bClearBitmapFile, DevContext, cleanShutdown, false);
            } else if (bClearBitmapFile) {
                ClearBitmapFile(pDevBitmap);
            }
            // TODO: Need to delete the log.
            // pBitmap->DeleteLog();
        } else {
            if (ppDevBitmap) {
                *ppDevBitmap = pDevBitmap;
            }
        }
    }

    InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_FILTERING_STOPPED, DevContext, DevContext->ulFlags);
    return Status;
}


void
AddResyncRequiredFlag(
    PUDIRTY_BLOCK_V2   UserDirtyBlock,
    PDEV_CONTEXT DevContext
    )
{
    NTSTATUS    Status;

    ASSERT(NULL != DevContext);

    Status = KeWaitForSingleObject(&DevContext->Mutex, Executive, KernelMode, FALSE, NULL);
    ASSERT(STATUS_SUCCESS == Status);

    if (DevContext->bResyncRequired) {
        DevContext->bResyncIndicated = true;
        KeQuerySystemTime(&DevContext->liOutOfSyncIndicatedTimeStamp);
        DevContext->ulOutOfSyncIndicatedAtCount = DevContext->ulOutOfSyncCount;
        // Resync is Required.
        UserDirtyBlock->uHdr.Hdr.ulFlags |= UDIRTY_BLOCK_FLAG_VOLUME_RESYNC_REQUIRED;
        UserDirtyBlock->uHdr.Hdr.ulOutOfSyncCount = DevContext->ulOutOfSyncCount;
        UserDirtyBlock->uHdr.Hdr.liOutOfSyncTimeStamp.QuadPart = DevContext->liOutOfSyncTimeStamp.QuadPart;
        UserDirtyBlock->uHdr.Hdr.ulOutOfSyncErrorCode = DevContext->ulOutOfSyncErrorCode;

        ULONG   ulOutOfSyncErrorCode = DevContext->ulOutOfSyncErrorCode;
        if (ulOutOfSyncErrorCode >= ERROR_TO_REG_MAX_ERROR) {
            ulOutOfSyncErrorCode = ERROR_TO_REG_DESCRIPTION_IN_EVENT_LOG;
        }

        Status = RtlStringCbPrintfExA((NTSTRSAFE_PSTR)&UserDirtyBlock->uHdr.Hdr.ErrorStringForResync[0],
                                      UDIRTY_BLOCK_MAX_ERROR_STRING_SIZE,
                                      NULL,
                                      NULL,
                                      STRSAFE_NULL_ON_FAILURE,
                                      ErrorToRegErrorDescriptionsA[ulOutOfSyncErrorCode],
                                      DevContext->ulOutOfSyncErrorStatus);
    } else {
        UserDirtyBlock->uHdr.Hdr.ErrorStringForResync[0] = '\0';
    }

    KeReleaseMutex(&DevContext->Mutex, FALSE);

}

// This function is called with volume context lock held.
ULONG
AddMetaDataToDirtyBlock(
    PDEV_CONTEXT DevContext, 
    PKDIRTY_BLOCK CurrentDirtyBlock, 
    PWRITE_META_DATA pWriteMetaData, 
    ULONG uDataSource
    )
{
    ULONG MaxDataSizePerDirtyBlock = DriverContext.ulMaxDataSizePerNonDataModeDirtyBlock;
    ULONG NumberOfSplitChanges = 0;
    BOOLEAN Coalesced = FALSE;
    // As Split decision is made on local variable MaxDataSizePerDirtyBlock. 
    // If there is need to create newer blocks then hvae to created of size MaxDataSizePerDirtyBlock 
    if(MaxDataSizePerDirtyBlock < pWriteMetaData->ulLength)
        return SplitChangeIntoDirtyBlock(DevContext, pWriteMetaData, uDataSource, MaxDataSizePerDirtyBlock);

    if(NULL == CurrentDirtyBlock) {
        CurrentDirtyBlock = AllocateDirtyBlock(DevContext, MaxDataSizePerDirtyBlock);
        if (NULL == CurrentDirtyBlock) {
            InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("%s: Failed allocation of dirtyblock\n", __FUNCTION__));
            return NumberOfSplitChanges;
        }
        if (!ReserveSpaceForChangeInDirtyBlock(CurrentDirtyBlock)) {
            DeallocateDirtyBlock(CurrentDirtyBlock, true);
            InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("%s: Failed allocation of changeblock\n", __FUNCTION__));
            return NumberOfSplitChanges;
        }
        AddDirtyBlockToDevContext(DevContext, CurrentDirtyBlock, uDataSource, false);
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
         && ((DevContext->bQueueChangesToTempQueue) ||
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
                uDataSource,
                Coalesced);
        } else {
            if (DevContext->bQueueChangesToTempQueue) {
                GenerateTimeStampDeltaForUnupdatedDB(CurrentDirtyBlock, pWriteMetaData->TimeDelta, pWriteMetaData->SequenceNumberDelta);
            }
            Status = AddChangeToDirtyBlock(CurrentDirtyBlock, 
                pWriteMetaData->llOffset,
                pWriteMetaData->ulLength,
                pWriteMetaData->TimeDelta,
                pWriteMetaData->SequenceNumberDelta,
                uDataSource,
                Coalesced);
        }
        if (STATUS_SUCCESS != Status) {
            InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("%s(%d): Failed allocation of changeblock\n", __FUNCTION__, __LINE__));
            return NumberOfSplitChanges;
        }
        CurrentDirtyBlock->ulicbChanges.QuadPart += pWriteMetaData->ulLength;
        AddChangeCountersOnDataSource(&DevContext->ChangeList, uDataSource, 0x01, (ULONGLONG)pWriteMetaData->ulLength);
        if (Coalesced) {
            // Changes may be "Coalesced" only iff uDataSource = INFLTDRV_DATA_SOURCE_META_DATA
            // If changes are coalesced, we should not increment counters. We re-adjust counters here.
            ASSERT(uDataSource == INFLTDRV_DATA_SOURCE_META_DATA);
            DevContext->ChangeList.ulMetaDataChangesPending -= 0x01;
            DevContext->ChangeList.ulTotalChangesPending -= 0x01;
        }
        NumberOfSplitChanges++;
        return NumberOfSplitChanges;
    }

    ASSERT(MaxDataSizePerDirtyBlock >= pWriteMetaData->ulLength);
    CurrentDirtyBlock = AllocateDirtyBlock(DevContext, MaxDataSizePerDirtyBlock);
    if (NULL == CurrentDirtyBlock) {
        InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("AddMetaDataToDirtyBlock: Failed allocation of dirtyblock\n"));
        return NumberOfSplitChanges;
    }
    if (!ReserveSpaceForChangeInDirtyBlock(CurrentDirtyBlock)) {
        DeallocateDirtyBlock(CurrentDirtyBlock, true);
        InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("%s: Failed allocation of changeblock\n", __FUNCTION__));
        return NumberOfSplitChanges;
    }

    AddDirtyBlockToDevContext(DevContext, CurrentDirtyBlock, uDataSource, false);
    InVolDbgPrint( DL_INFO, DM_DATA_FILTERING | DM_DIRTY_BLOCKS,
                    ("AddMetaDataToDirtyBlock: Added new DirtyBlock in metadata mode\n"));

    NTSTATUS Status;
    //adding the first deltas as zero explicitly
    Status = AddChangeToDirtyBlock(CurrentDirtyBlock, pWriteMetaData->llOffset, pWriteMetaData->ulLength, 0, 0, uDataSource, Coalesced);

    ASSERT(STATUS_SUCCESS == Status);

    CurrentDirtyBlock->ulicbChanges.QuadPart += pWriteMetaData->ulLength;
    AddChangeCountersOnDataSource(&DevContext->ChangeList, uDataSource, 0x01, (ULONGLONG)pWriteMetaData->ulLength);
    if (Coalesced) {
        // Changes may be "Coalesced" only iff uDataSource = INFLTDRV_DATA_SOURCE_META_DATA
        // If changes are coalesced, we should not increment counters. We re-adjust counters here.
        ASSERT(uDataSource == INFLTDRV_DATA_SOURCE_META_DATA);
        DevContext->ChangeList.ulMetaDataChangesPending -= 0x01;
        DevContext->ChangeList.ulTotalChangesPending -= 0x01;
    }
    
    NumberOfSplitChanges++;
    return NumberOfSplitChanges;
}

// This function is called with volume context lock held.
ULONG
SplitChangeIntoDirtyBlock(
    PDEV_CONTEXT DevContext, 
    PWRITE_META_DATA pWriteMetaData, 
    ULONG uDataSource,
    ULONG MaxDataSizePerDirtyBlock
    )
{    
    ULONG RemainingLength = pWriteMetaData->ulLength;
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
        DirtyBlock = AllocateDirtyBlock(DevContext, MaxDataSizePerDirtyBlock);

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
        if (STATUS_SUCCESS != AddChangeToDirtyBlock(DirtyBlock, ByteOffset, SplitChangeLength,0,0, uDataSource, Coalesced)) {
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
        AddDirtyBlockToDevContext(DevContext, ChangeNode->DirtyBlock, uDataSource, Continuation);
        Continuation = true;
    }

    AddChangeCountersOnDataSource(&DevContext->ChangeList, uDataSource, NumberOfSplitChanges, (ULONGLONG)pWriteMetaData->ulLength);

    return NumberOfSplitChanges;
}
