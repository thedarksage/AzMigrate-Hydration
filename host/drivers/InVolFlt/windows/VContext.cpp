#include "Common.h"
#include "VContext.h"
#include "TagNode.h"
#ifdef VOLUME_FLT
#include "MntMgr.h"
#endif
#include "VBitmap.h"
#include "ListNode.h"
#include "misc.h"
#include "HelperFunctions.h"
#include "svdparse.h"
#include "CxSession.h"

/*-----------------------------------------------------------------------------
 *
 * File Name - VContext.cpp
 * This file contains all the routines to handle FLtDev Context
 * abbrevated as VC
 *
 *-----------------------------------------------------------------------------
 */

VOID
SetDBallocFailureError(PDEV_CONTEXT DevContext)
{
    LARGE_INTEGER   liTickCount, liTickIncrementPerSec;
    KIRQL           OldIrql;
    bool            bLog = false;

    KeAcquireSpinLock(&DriverContext.NoMemoryLogEventLock, &OldIrql);

    liTickCount = KeQueryPerformanceCounter(&liTickIncrementPerSec);
    DriverContext.ulNumMemoryAllocFailures++;
    DevContext->ulNumMemoryAllocFailures++;

    if ((0 == DriverContext.ulSecondsBetweenNoMemoryLogEvents) || 
        (0 == DriverContext.liLastDBallocFailAtTickCount.QuadPart)) 
    {
        bLog = true;
    }
    else
    {
        LARGE_INTEGER   liTickDiff;
        ULONG           ulSecsFromLastLog;

        liTickDiff.QuadPart = liTickCount.QuadPart - DriverContext.liLastDBallocFailAtTickCount.QuadPart;
        ulSecsFromLastLog = (ULONG)(liTickDiff.QuadPart/liTickIncrementPerSec.QuadPart);
        if (ulSecsFromLastLog >= DriverContext.ulSecondsBetweenNoMemoryLogEvents) {
            bLog = true;
        }
    }

    if (bLog) {
        DriverContext.liLastDBallocFailAtTickCount.QuadPart = liTickCount.QuadPart;
        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_NO_NPAGED_POOL_FOR_DIRTYBLOCKS, DevContext);
    }

    KeReleaseSpinLock(&DriverContext.NoMemoryLogEventLock, OldIrql);

    return;
}

VOID
SetBitmapOpenFailDueToLossOfChanges(PDEV_CONTEXT DevContext, bool bLockAcquired)
{
    LARGE_INTEGER   liTickCount, liTickIncrementPerSec, liTickDiff;
    ULONG           ulSecsFromFirstOpenFail;
    KIRQL           OldIrql = 0;

    liTickCount = KeQueryPerformanceCounter(&liTickIncrementPerSec);
    liTickDiff.QuadPart = DevContext->liLastBitmapOpenErrorAtTickCount.QuadPart - DevContext->liFirstBitmapOpenErrorAtTickCount.QuadPart;
    ulSecsFromFirstOpenFail = (ULONG)(liTickDiff.QuadPart/liTickIncrementPerSec.QuadPart);

    if (FALSE == bLockAcquired)
        KeAcquireSpinLock(&DevContext->Lock, &OldIrql);

    // As we are loosing changes, we should set resync required
    // and also disable filterin temporarily for this Dev

    DevContext->ulFlags |= DCF_OPEN_BITMAP_FAILED;
    StopFilteringDevice(DevContext, true, false, false);

    InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, 
        ("SetBitmapOpenFailDueToLossOfChanges: Stopping filtering on device %s due to loss of changes.\n%d bitmap open errors and %d secs time diff between first and curr failure\n",
            DevContext->chDevNum, DevContext->lNumBitmapOpenErrors, ulSecsFromFirstOpenFail));

    if (FALSE == bLockAcquired)
        KeReleaseSpinLock(&DevContext->Lock, OldIrql);

    SetDevOutOfSync(DevContext, ERROR_TO_REG_BITMAP_OPEN_FAIL_CHANGES_LOST, STATUS_SUCCESS, bLockAcquired);

    return;
}

VOID
SetBitmapOpenError(PDEV_CONTEXT  DevContext, bool bLockAcquired, NTSTATUS Status)
{
    LARGE_INTEGER   liTickCount, liTickIncrementPerSec, liTickDiff;
    ULONG           ulSecsFromFirstOpenFail = 0;
    BOOLEAN         bOutOfSync = FALSE;
    BOOLEAN         bFailFurtherOpens = FALSE;
    KIRQL           OldIrql = 0;

    InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, 
        ("SetBitmapOpenError: On device %s due to %d bitmap open errors till now.\n",
            DevContext->chDevNum, DevContext->lNumBitmapOpenErrors));

    liTickCount = KeQueryPerformanceCounter(&liTickIncrementPerSec);

    if (!bLockAcquired)
        KeAcquireSpinLock(&DevContext->Lock, &OldIrql);

    DevContext->lNumBitmapOpenErrors++;
    if (0 == DevContext->liFirstBitmapOpenErrorAtTickCount.QuadPart) {
        DevContext->liFirstBitmapOpenErrorAtTickCount = liTickCount;
        InDskFltWriteEventWithDevCtxt(INDSKFLT_WARN_FIRST_FAILURE_TO_OPEN_BITMAP, DevContext);
        InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, 
            ("SetBitmapOpenError: On device %s first failure\n", DevContext->chDevNum));                    
    } else {
        liTickDiff.QuadPart = liTickCount.QuadPart - DevContext->liFirstBitmapOpenErrorAtTickCount.QuadPart;
        ulSecsFromFirstOpenFail = (ULONG)(liTickDiff.QuadPart/liTickIncrementPerSec.QuadPart);
        InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, 
            ("SetBitmapOpenError: On device %s %d secs time diff between first and curr failure\n",
                DevContext->chDevNum, ulSecsFromFirstOpenFail));                    
        if (( ulSecsFromFirstOpenFail > MAX_DELAY_FOR_BITMAP_FILE_OPEN_IN_SECONDS) &&
            (DevContext->lNumBitmapOpenErrors > MAX_BITMAP_OPEN_ERRORS_TO_STOP_FILTERING))
        {
            bFailFurtherOpens = TRUE;
        }
    }

    DevContext->liLastBitmapOpenErrorAtTickCount = liTickCount;

    if (bFailFurtherOpens) {
        // As we are loosing changes, we should set resync required
        // and also disable filterin temporarily for this Dev

        DevContext->ulFlags |= DCF_OPEN_BITMAP_FAILED;
        bOutOfSync = TRUE;
        StopFilteringDevice(DevContext, true, false, false);
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, 
            ("SetBitmapOpenError: Stopping filtering on device %s due to %d bitmap open errors and %d secs time diff between first and curr failure\n",
                DevContext->chDevNum, DevContext->lNumBitmapOpenErrors, ulSecsFromFirstOpenFail));                    
    }

    if (!bLockAcquired)
        KeReleaseSpinLock(&DevContext->Lock, OldIrql);

    if (TRUE == bOutOfSync)
        SetDevOutOfSync(DevContext, ERROR_TO_REG_BITMAP_OPEN_ERROR, Status, bLockAcquired);

    return;
}

BOOLEAN
CanOpenBitmapFile(PDEV_CONTEXT   DevContext, BOOLEAN bLoseChanges)
{
    LARGE_INTEGER   liTickCount, liTickIncrementPerSec;
    ULONG           ulSecsFromLastOpenFail, ulSecsDelayForNextOpen;

    InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, 
        ("CanOpenBitmapFile: Called on device %s DevContext %#p, bLoseChanges = %d\n", 
            DevContext->chDevNum, DevContext, bLoseChanges));                    

    if (DevContext->ulFlags & (DCF_FILTERING_STOPPED | DCF_OPEN_BITMAP_FAILED)) {
        InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, 
            ("CanOpenBitmapFile: Returning FALSE for device %s Volume is either stopped for open failed. ulFlags = %#x\n", 
                DevContext->chDevNum, DevContext->ulFlags));                    
        return FALSE;
    }

    if ((TRUE == bLoseChanges) || (0 == DevContext->lNumBitmapOpenErrors)) {
        InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, 
            ("CanOpenBitmapFile: Returning TRUE for device %s bLoseChanges(%d) is TRUE or lNumBitmapOpenErrors(%d) is zero\n", 
                DevContext->chDevNum, bLoseChanges, DevContext->lNumBitmapOpenErrors));                    
        return TRUE;
    }

    liTickCount = KeQueryPerformanceCounter(&liTickIncrementPerSec);
    ulSecsFromLastOpenFail =  (ULONG) ((liTickCount.QuadPart - DevContext->liLastBitmapOpenErrorAtTickCount.QuadPart) / liTickIncrementPerSec.QuadPart);
    if (ulSecsFromLastOpenFail > 0) {
        ulSecsDelayForNextOpen = MIN_DELAY_FOR_BIMTAP_FILE_OPEN_IN_SECONDS * (1 << DevContext->lNumBitmapOpenErrors);
        InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, 
            ("CanOpenBitmapFile: On device %s ulSecsFromLastOpenFail(%d), ulSecsDelayForNextOpen(%d) lNumBitmapOpenErrors(%d)\n", 
                DevContext->chDevNum, ulSecsFromLastOpenFail, ulSecsDelayForNextOpen, DevContext->lNumBitmapOpenErrors));                    
        if (ulSecsFromLastOpenFail >= ulSecsDelayForNextOpen) {
            InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, ("CanOpenBitmapFile: Returning TRUE for device %s\n", DevContext->chDevNum));
            return TRUE;
        }
    } else {
        InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, 
            ("CanOpenBitmapFile: On device %s ulSecsFromLastOpenFail(%d) is TRUE or lNumBitmapOpenErrors(%d) is zero\n", 
                DevContext->chDevNum, ulSecsFromLastOpenFail, DevContext->lNumBitmapOpenErrors));
    }

    InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, ("CanOpenBitmapFile: Returning FALSE for device %s\n", DevContext->chDevNum));                    
    return FALSE;
}

VOID
LogBitmapOpenSuccessEvent(PDEV_CONTEXT DevContext)
{
    LARGE_INTEGER   liTickCount, liTickIncrementPerSec, liTickDiff;
    ULONG           ulSecsFromFirstOpenFail = 0;

    InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, 
        ("LogBitmapOpenSucessEvent: Called on device %S(%S): BitmapOpenErrors %d\n", 
            DevContext->wDevNum, DevContext->wDevID, DevContext->lNumBitmapOpenErrors));

    if (0 == DevContext->lNumBitmapOpenErrors) {
        return;
    }

    ASSERT(0 != DevContext->liFirstBitmapOpenErrorAtTickCount.QuadPart);

    liTickCount = KeQueryPerformanceCounter(&liTickIncrementPerSec);
    liTickDiff.QuadPart = liTickCount.QuadPart - DevContext->liFirstBitmapOpenErrorAtTickCount.QuadPart;
    ulSecsFromFirstOpenFail = (ULONG)(liTickDiff.QuadPart/liTickIncrementPerSec.QuadPart);
    InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, 
        ("LogBitmapOpenSucessEvent: On device %S(%S): %d secs time diff between first and curr failure\n",
            DevContext->wDevNum, DevContext->wDevID, ulSecsFromFirstOpenFail));

    InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_SUCCESS_OPEN_BITMAP_AFTER_RETRY,
        DevContext, DevContext->lNumBitmapOpenErrors, ulSecsFromFirstOpenFail);

    return;
}

/*-----------------------------------------------------------------------------
 * Functions related to DataBlocks and DataFiltering
 *----------------------------------------------------------------------------0
 */

VOID
VCAdjustCountersToUndoReserve(
    PDEV_CONTEXT DevContext,
    ULONG   ulBytes
    )
{
    if (DevContext->ullcbDataAlocMissed) {
        if (DevContext->ullcbDataAlocMissed >= ulBytes) {
            DevContext->ullcbDataAlocMissed -= ulBytes;
            ulBytes = 0;
        } else {
            ulBytes -= (ULONG)DevContext->ullcbDataAlocMissed;
            DevContext->ullcbDataAlocMissed = 0;
        }
    }

    if (ulBytes) {
        ASSERT(DevContext->ulcbDataReserved >= ulBytes);
        if (DevContext->ulcbDataReserved >= ulBytes) {
            DevContext->ulcbDataFree += ulBytes;
            DevContext->ulcbDataReserved -= ulBytes;
        } else {
             DevContext->ulcbDataFree -= DevContext->ulcbDataReserved;
             DevContext->ulcbDataReserved = 0;
        }
    }

    return;
}

VOID
VCAdjustCountersForLostDataBytes(
    PDEV_CONTEXT DevContext,
    PDATA_BLOCK *ppDataBlock,
    ULONG ulBytesLost,
    bool bAllocate,
    bool bCanLock
    )
{
    InVolDbgPrint(DL_FUNC_TRACE, DM_DATA_FILTERING, ("VCAdjustCountersForLostDataBytes: %s(%s) ulBytesLost = %#x\n",
                                                     DevContext->chDevID, DevContext->chDevNum, ulBytesLost));
    InVolDbgPrint(DL_TRACE_L1, DM_DATA_FILTERING, 
                  ("VCAdjustCountersForLostDataBytes: (Before) Alloced = %#x Missed = %#I64x Free = %#x Reserved = %#x\n",
                   DevContext->ulcbDataAllocated, DevContext->ullcbDataAlocMissed,
                   DevContext->ulcbDataFree, DevContext->ulcbDataReserved));
    if (0 == ulBytesLost) {
        return;
    }

    if (ulBytesLost <= DevContext->ulcbDataFree) {
        DevContext->ulcbDataFree -= ulBytesLost;
        ulBytesLost = 0;
        InVolDbgPrint(DL_TRACE_L1, DM_DATA_FILTERING, 
                      ("VCAdjustCountersForLostDataBytes: (After) Alloced = %#x Missed = %#I64x Free = %#x Reserved = %#x\n",
                       DevContext->ulcbDataAllocated, DevContext->ullcbDataAlocMissed,
                       DevContext->ulcbDataFree, DevContext->ulcbDataReserved));
        return;
    }

    if (0 == DevContext->ullcbDataAlocMissed) {
        // Add data blocks if and only if we did not miss any data.
        if ((NULL != ppDataBlock) && (NULL != *ppDataBlock)) {
            VCAddToReservedDataBlockList(DevContext, *ppDataBlock);
            DevContext->ulcbDataFree += DriverContext.ulDataBlockSize;
            *ppDataBlock = NULL;
        } else if (bAllocate) {
            PDATA_BLOCK DataBlock = AllocateLockedDataBlock(bCanLock);
            if (NULL != DataBlock) {
                VCAddToReservedDataBlockList(DevContext, DataBlock);
                DevContext->ulcbDataFree += DriverContext.ulDataBlockSize;
            }
        }
    }

    if (ulBytesLost <= DevContext->ulcbDataFree) {
        DevContext->ulcbDataFree -= ulBytesLost;
        ulBytesLost = 0;
        InVolDbgPrint(DL_TRACE_L1, DM_DATA_FILTERING, 
                      ("VCAdjustCountersForLostDataBytes: (After) Alloced = %#x Missed = %#I64x Free = %#x Reserved = %#x\n",
                       DevContext->ulcbDataAllocated, DevContext->ullcbDataAlocMissed,
                       DevContext->ulcbDataFree, DevContext->ulcbDataReserved));
        return;
    }

    ulBytesLost -= DevContext->ulcbDataFree;
    DevContext->ulcbDataFree = 0;

    ASSERT(DevContext->ulcbDataReserved >= ulBytesLost);
    if (DevContext->ulcbDataReserved >= ulBytesLost) {
        DevContext->ulcbDataReserved -= ulBytesLost;
    } else {
        DevContext->ulcbDataReserved = 0;
    }

    DevContext->ullcbDataAlocMissed += ulBytesLost;
    ulBytesLost = 0;
    ASSERT((DevContext->eCaptureMode != ecCaptureModeData) || 
           (DevContext->ullNetWritesSeenInBytes == DevContext->ullUnaccountedBytes + DevContext->ulcbDataReserved + DevContext->ullcbDataAlocMissed));

    InVolDbgPrint(DL_TRACE_L1, DM_DATA_FILTERING, 
                  ("VCAdjustCountersForLostDataBytes: (After) Alloced = %#x Missed = %#I64x Free = %#x Reserved = %#x\n",
                   DevContext->ulcbDataAllocated, DevContext->ullcbDataAlocMissed,
                   DevContext->ulcbDataFree, DevContext->ulcbDataReserved));

    return;
}

PDATA_BLOCK
VCGetReservedDataBlock(PDEV_CONTEXT DevContext)
{
    PDATA_BLOCK DataBlock = NULL;

    ASSERT(NULL != DevContext);

    if (!IsListEmpty(&DevContext->ReservedDataBlockList)) {
        ASSERT(DevContext->ulDataBlocksReserved > 0);
        DataBlock = (PDATA_BLOCK)RemoveHeadList(&DevContext->ReservedDataBlockList);
        DevContext->ulDataBlocksReserved--;
    }

    return DataBlock;
}

VOID
VCAddToReservedDataBlockList(PDEV_CONTEXT DevContext, PDATA_BLOCK DataBlock)
{
    InsertTailList(&DevContext->ReservedDataBlockList, &DataBlock->ListEntry);
    DevContext->ulcbDataAllocated += DriverContext.ulDataBlockSize;
    DevContext->ulDataBlocksReserved++;

    return;
}

VOID
VCFreeReservedDataBlockList(PDEV_CONTEXT DevContext)
{
    InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, ("VCFreeReservedDataBlockList: Called\n"));
    while(!IsListEmpty(&DevContext->ReservedDataBlockList)) {
        PDATA_BLOCK DataBlock = (PDATA_BLOCK)RemoveHeadList(&DevContext->ReservedDataBlockList);

        ASSERT(DevContext->ulDataBlocksReserved > 0);
        DevContext->ulDataBlocksReserved--;

        DeallocateDataBlock(DataBlock, true);
        ASSERT(DevContext->ulcbDataAllocated >= DriverContext.ulDataBlockSize);
        DevContext->ulcbDataAllocated -= DriverContext.ulDataBlockSize;
    }

    InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, ("VCFreeReservedDataBlockList: ulDataBlocksReserved = %#x, ulcbDataAllocated = %#x\n",
                                               DevContext->ulDataBlocksReserved, DevContext->ulcbDataAllocated));
    return;
}

VOID
VCFreeUsedDataBlockList(PDEV_CONTEXT DevContext, bool bVCLockAcquired)
{
    PCHANGE_NODE    ChangeNode = (PCHANGE_NODE)DevContext->ChangeList.Head.Flink;

    while ((PLIST_ENTRY)ChangeNode != &DevContext->ChangeList.Head) {
        if (ecChangeNodeDirtyBlock == ChangeNode->eChangeNode) {
            PKDIRTY_BLOCK   DirtyBlock = ChangeNode->DirtyBlock;

            if (INFLTDRV_DATA_SOURCE_DATA == DirtyBlock->ulDataSource) {
                DBFreeDataBlocks(DirtyBlock, bVCLockAcquired);
            }
        }

        ChangeNode = (PCHANGE_NODE)ChangeNode->ListEntry.Flink;
    }

    return;
}

VOID
VCFreeAllDataBuffers(PDEV_CONTEXT    DevContext, bool bVCLockAcquired)
{
    ASSERT(NULL != DevContext);

    VCFreeReservedDataBlockList(DevContext);
    VCFreeUsedDataBlockList(DevContext, bVCLockAcquired);
    DevContext->ulcbDataFree = 0;
    DevContext->ulcbDataReserved = 0;

    return;
}

NTSTATUS
AddUserDefinedTagsInDataMode(
    PKDIRTY_BLOCK   DirtyBlock,
    PUCHAR          pSequenceOfTags,
    PGUID           pTagGUID,
    ULONG           ulTagDevIndex
    )
{
    ASSERT(NULL != DirtyBlock->CurrentDataBlock);
    ASSERT(NULL == DirtyBlock->SVDcbChangesTag);

    SVD_PREFIX  SVDPrefix;
    ULONG       ulNumberOfTags = *(PULONG)pSequenceOfTags;   
    pSequenceOfTags += sizeof(ULONG);

    ULONG   ulTagIndex;

    for(ulTagIndex = 0; ulTagIndex < ulNumberOfTags; ulTagIndex++) {
        USHORT  usTagDataLength = *(USHORT UNALIGNED *)pSequenceOfTags;
        pSequenceOfTags += sizeof(USHORT);

        SVDPrefix.tag = SVD_TAG_USER;
        SVDPrefix.count = 1;
        SVDPrefix.Flags = usTagDataLength;

        ULONG ulDataCopied = CopyDataToDirtyBlock(DirtyBlock, (PUCHAR)&SVDPrefix, sizeof(SVD_PREFIX));
        ASSERT(ulDataCopied == sizeof(SVD_PREFIX));
        
        ulDataCopied = CopyDataToDirtyBlock(DirtyBlock, pSequenceOfTags, (ULONG)usTagDataLength);
        ASSERT(ulDataCopied == usTagDataLength);
        pSequenceOfTags += usTagDataLength;
    }

    DirtyBlock->SVDcbChangesTag = DirtyBlock->CurrentDataBlock->CurrentPosition;
    DirtyBlock->ulFlags |= KDIRTY_BLOCK_FLAG_CONTAINS_TAGS;

    IncrementCurrentPosition(DirtyBlock, SVFileFormatChangesHeaderSize());

    if (pTagGUID) {
        ASSERT(!(DirtyBlock->ulFlags & KDIRTY_BLOCK_FLAG_TAG_WAITING_FOR_COMMIT));

        DirtyBlock->ulFlags |= KDIRTY_BLOCK_FLAG_TAG_WAITING_FOR_COMMIT;
        RtlCopyMemory(&DirtyBlock->TagGUID, pTagGUID, sizeof(GUID));
        DirtyBlock->ulTagDevIndex = ulTagDevIndex;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
AddTagsInDBHeader(
    __in PDEV_CONTEXT    DevContext,
    __in PUCHAR          pSequenceOfTags,
    __in PTAG_TELEMETRY_COMMON pTagCommonAttribs,
    __in bool           isCommitNotifyTagRequest
    )
/*
Description : This routine fills the KDIRTY_BLOCK header with tag buffer received from user space and closes the DirtyBlock. This functiona calculates the actual length required
              including header and paddig

Arguments :
DevContext       - Pointer to Disk filter Device context
pSequenceofTags  - Pointers to the tag data

Return   :
STATUS_INSUFFICIENT_RESOURCES - Memory allocation failure
STATUS_INFO_LENGTH_MISMATCH   - Error while copying the tag buffer
STATUS_SUCCESS                - Fills and close the DB
*/
{
    ASSERT(NULL != pSequenceOfTags);

    ULONG                   ulNumberOfTags = *(PULONG)pSequenceOfTags;
    ULONG                   ulMemNeeded = 0;
    PUCHAR                  pTags = pSequenceOfTags + sizeof(ULONG);
    NTSTATUS                Status = STATUS_SUCCESS;
    // This is validated in IsValidIoctlTagVolumeInputBuffer
    ASSERT(0 != ulNumberOfTags);

    ulMemNeeded = GetBufferSizeNeededForTags(pTags, ulNumberOfTags);

    // This is validated in IsValidIoctlTagVolumeInputBuffer
    ASSERT (ulMemNeeded <= DriverContext.ulMaxTagBufferSize);

    PUCHAR TagBuffer = (PUCHAR)ExAllocatePoolWithTag(NonPagedPool, ulMemNeeded, TAG_TAG_BUFFER);
    if (NULL == TagBuffer) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Cleanup;
    }
    RtlSecureZeroMemory(TagBuffer, ulMemNeeded);
    // Set the Maxsize of the dirtyblock to zero 
    PKDIRTY_BLOCK   DirtyBlock = AllocateDirtyBlock(DevContext, 0); 
    if (NULL == DirtyBlock) {
        ExFreePoolWithTag(TagBuffer, TAG_TAG_BUFFER);
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Cleanup;
    }
    
    AddDirtyBlockToDevContext(DevContext, DirtyBlock, NULL, NULL, NULL);
    DirtyBlock->TagBuffer = TagBuffer;
    DirtyBlock->ulTagBufferSize = ulMemNeeded;
    DirtyBlock->ulFlags |= KDIRTY_BLOCK_FLAG_CONTAINS_TAGS;
    pTags = pSequenceOfTags + sizeof(ULONG);

    for (ULONG ulIndex = 0; ulIndex < ulNumberOfTags; ulIndex++) {
        USHORT  usTagDataLength = *(USHORT UNALIGNED *)pTags;
        ULONG   ulTagLengthWoHeaderWoAlign = sizeof(USHORT) + usTagDataLength;
        ULONG   ulTagLengthWoHeader = GET_ALIGNMENT_BOUNDARY(ulTagLengthWoHeaderWoAlign, sizeof(ULONG));
        ULONG   ulTagHeaderLength = StreamHeaderSizeFromDataSize(ulTagLengthWoHeader);
        ASSERT(ulTagLengthWoHeader >= ulTagLengthWoHeaderWoAlign);
        ULONG   ulPadding = ulTagLengthWoHeader - ulTagLengthWoHeaderWoAlign;
    
        //Copy Tag header
        FILL_STREAM_HEADER(TagBuffer, STREAM_REC_TYPE_USER_DEFINED_TAG, ulTagHeaderLength + ulTagLengthWoHeader);
        TagBuffer += ulTagHeaderLength;
    
        //Copy Tag length + Tag data
        RtlCopyMemory(TagBuffer, pTags, ulTagLengthWoHeaderWoAlign);
        TagBuffer += ulTagLengthWoHeaderWoAlign;
    
        // Fill Padding
        if (ulPadding) {
            RtlZeroMemory(TagBuffer, ulPadding);
        }
        TagBuffer += ulPadding;
        pTags += ulTagLengthWoHeaderWoAlign;
    }

    // ASSERT we did not overrun allocated memory
    ASSERT(TagBuffer == DirtyBlock->TagBuffer + DirtyBlock->ulTagBufferSize);
    if (TagBuffer != DirtyBlock->TagBuffer + DirtyBlock->ulTagBufferSize) {
        // TagBuffer is freed while Deallocating DirtyBlock
        DeallocateDirtyBlock(DirtyBlock, true);
        Status = STATUS_INFO_LENGTH_MISMATCH;
        goto Cleanup;
    }
    if (NULL != pTagCommonAttribs) {
        InDskFltTelemetryFillHistoryOnTagInsert(DirtyBlock, DevContext, pTagCommonAttribs);
    }
    DevContext->ulTagsinWOState++;
    //Mark the tags on successful validation
    DirtyBlock->ulFlags |= KDIRTY_BLOCK_FLAG_CONTAINS_TAGS;
    DirtyBlock->ulFlags |= KDIRTY_BLOCK_FLAG_TAGS_PENDING;
    if (isCommitNotifyTagRequest) {
        SET_FLAG(DirtyBlock->ulFlags, KDIRTY_BLOCK_FLAG_TAG_NOTIFY);
    }
    // Don't allow any changes and close the DirtyBlock
    AddLastChangeTimestampToCurrentNode(DevContext, NULL);

Cleanup :
        return Status;
}

NTSTATUS
AddUserDefinedTagsForDiskDevice(
      __in PDEV_CONTEXT    DevContext,
      __in PUCHAR          pSequenceOfTags,
      __in PTAG_TELEMETRY_COMMON pTagCommonAttribs,
      __in bool           isCommitNotifyTagRequest
      )
/*
Description : Add tags in Data write order state

Arguments   :
DevContext       - Pointer to Disk filter Device context
pSequenceofTags  - Pointer to the tag data

Return      : 
Appropriate status if the Device state is WriteOrder
STATUS_INVALID_DEVICE_STATE - Fails with this error, if WO state of the Device is Non-Data
*/
{
    NTSTATUS    Status = STATUS_SUCCESS;

    if (ecWriteOrderStateData != DevContext->eWOState) {
        Status = STATUS_INVALID_DEVICE_STATE;
        DevContext->ulTagsinNonWOSDrop++;
        goto Cleanup;
    }
    ASSERT(ecCaptureModeData == DevContext->eCaptureMode);
    ASSERT(ecWriteOrderStateData == DevContext->eWOState);

    Status = AddTagsInDBHeader(DevContext, pSequenceOfTags, pTagCommonAttribs, isCommitNotifyTagRequest);

Cleanup :
         return Status;
}
NTSTATUS
AddUserDefinedTags(
    PDEV_CONTEXT DevContext,
    PUCHAR          SequenceOfTags,
    PTIME_STAMP_TAG_V2 *ppTimeStamp,
    PLARGE_INTEGER  pliTickCount,
    PGUID           pTagGUID,
    ULONG           ulTagDevIndex
    )
{
    NTSTATUS    Status = STATUS_SUCCESS;
#ifdef VOLUME_FLT
#if DBG
    InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_IOCTL_DISK_COPY_DATA_RECEIVED,
            DevContext, DevContext->IoctlDiskCopyDataCount, DevContext->IoctlDiskCopyDataCountSuccess);

    DevContext->IoctlDiskCopyDataCount = 0;
    DevContext->IoctlDiskCopyDataCountSuccess = 0;
    DevContext->IoctlDiskCopyDataCountFailure = 0;
#endif
#endif
    // Bug#23098 : With performance changes, capture mode could be data irrespective of WO state. As tag is not considered as a change, there could be a dirtyblock(non WOS) with just a tag
    // and subsequent dirty block may be a WOS which is partially filled. Draining prior dirtyblock as per the priority leading to OOD. So allowing tags only when WOS is data.

    if (ecWriteOrderStateData != DevContext->eWOState) {
        Status = STATUS_NOT_SUPPORTED;
        DevContext->ulTagsinNonWOSDrop++;
        return Status;
    }

    ULONG  ulRecordSize = SVRecordSizeForTags(SequenceOfTags);
    ULONG  ulBytesToReserve = ulRecordSize;
    ReserveDataBytes(DevContext, false, ulBytesToReserve, true, true, true);

    PKDIRTY_BLOCK   DirtyBlock = GetDirtyBlockToCopyChange(DevContext, ulRecordSize, true, ppTimeStamp, pliTickCount);
    if (NULL != DirtyBlock) {
        // We have space to add tags in stream format
        InVolDbgPrint(DL_ERROR, DM_TAGS, 
            ("AddUserDefinedTags: Number of TAGS are : %d \n", *(PULONG)SequenceOfTags)); 
        // Make a copy of tags pointer
        PUCHAR TempSeqTags = SequenceOfTags;
        Status = AddUserDefinedTagsInDataMode(DirtyBlock, SequenceOfTags, pTagGUID, ulTagDevIndex);

         if (NT_SUCCESS(Status))
            DevContext->ulTagsinWOState++;

        if (NT_SUCCESS(Status) && (1 == *(PULONG)TempSeqTags)) {
            TempSeqTags += sizeof(ULONG);
            USHORT taglen= *(USHORT UNALIGNED *)TempSeqTags;
            TempSeqTags += sizeof(USHORT);
	    
            USHORT max_taglen = INMAGE_MAX_TAG_LENGTH;
            // TagLenth in stream can be more than INMAGE_MAX_TAG_LENGTH e.g. Revocation tag
            // To avoid memory corruption compare Last tag with minimum of (INMAGE_MAX_TAG_LENGTH, taglength)
            // Bug Fix: 19336
            if (taglen < max_taglen) {
                max_taglen = taglen;
            }
            if (max_taglen == RtlCompareMemory(DevContext->LastTag, TempSeqTags, max_taglen)) {
                InVolDbgPrint( DL_ERROR, DM_INMAGE_IOCTL | DM_TAGS,
                    ("Same tag received in sequence IOCTL tag count : %ld, DataModeCount : %ld, NonDataModeCount : %ld \n",
                    DriverContext.ulNumberofAsyncTagIOCTLs, DevContext->ulTagsinWOState, DevContext->ulTagsinNonWOSDrop));

                InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_LOG_SAME_TAG_IN_SEQ,
                    DevContext, DriverContext.ulNumberofAsyncTagIOCTLs, DevContext->ulTagsinWOState,
                    DevContext->ulTagsinNonWOSDrop);
               ASSERT(max_taglen != RtlCompareMemory(DevContext->LastTag, TempSeqTags,max_taglen));
            }
            RtlZeroMemory(DevContext->LastTag, INMAGE_MAX_TAG_LENGTH);
            RtlMoveMemory(DevContext->LastTag, TempSeqTags, max_taglen);
        }
        if (DevContext->ulcbDataReserved > ulRecordSize) {
             DevContext->ulcbDataReserved -= ulRecordSize;
        } else {
             DevContext->ulcbDataReserved = 0;
        }
        if (DevContext->ullNetWritesSeenInBytes > ulRecordSize) {
             DevContext->ullNetWritesSeenInBytes -= ulRecordSize;
        } else {
             DevContext->ullNetWritesSeenInBytes = 0;
        }
    } else {
        DevContext->ullNetWritesSeenInBytes -= ulRecordSize;
        VCAdjustCountersToUndoReserve(DevContext, ulRecordSize);
        VCSetCaptureModeToMetadata(DevContext, false);
        VCSetWOState(DevContext, false, __FUNCTION__, ecMDReasonInsertTagsLegacy);
        Status = STATUS_NOT_SUPPORTED;
        DevContext->ulTagsinNonWOSDrop++;
    }

    // ReserveDataBytes with bFirst true increments ulNumPendingIrps, this has to be decremented.
    DevContext->ulNumPendingIrps--;
    return Status;
}

NTSTATUS
VCAddKernelTag(
    PDEV_CONTEXT DevContext,
    PVOID           pTag,
    USHORT          usTagLen
    )
{
    NTSTATUS        Status = STATUS_SUCCESS;

    if (!DriverContext.bKernelTagsEnabled) {
        return Status;
    }

    ULONG ulStreamHeaderSize = StreamHeaderSizeFromDataSize(usTagLen);
    ULONG ulMemNeeded = (2 * sizeof(USHORT)) + ulStreamHeaderSize + usTagLen;
    PUCHAR  TagBuffer = (PUCHAR)ExAllocatePoolWithTag(NonPagedPool, ulMemNeeded, TAG_TAG_BUFFER);
    if (NULL == TagBuffer) {
        return STATUS_NO_MEMORY;
    }

    // Fill out the buffer with Tag format
    PUCHAR  Temp = TagBuffer;
    // USHORT with number of tags
    *(PUSHORT)Temp = 0x01;
    Temp += sizeof(USHORT);

    // USHORT with Tag length that is followed.
    *(PUSHORT)Temp = (USHORT) (StreamHeaderSizeFromDataSize(usTagLen) + usTagLen);
    Temp += sizeof(USHORT);

    // STREAM header
    FILL_STREAM_HEADER(Temp, STREAM_REC_TYPE_USERDEFINED_EVENT, (ulStreamHeaderSize + usTagLen));
    Temp += ulStreamHeaderSize;

    RtlCopyMemory(Temp, pTag, usTagLen);

    Status = AddUserDefinedTags(DevContext, TagBuffer, NULL, NULL, NULL, 0);
    //Clanup of TagBuffer. KernelTag is disabled by default
    if (STATUS_NO_MEMORY == Status) {
        ExFreePoolWithTag(TagBuffer, TAG_TAG_BUFFER);
    }
    
    return Status;
}

bool
IsDataCaptureEnabledForThisDev(PDEV_CONTEXT DevContext)
{
    if (!DriverContext.bServiceSupportsDataCapture) {
        return false;
    }

    if (!DriverContext.bEnableDataCapture) {
        return false;
    }

    if (DevContext->ulFlags & DCF_DATA_CAPTURE_DISABLED) {
        return false;
    }

    return true;
}

bool
IsDataFilesEnabledForThisDev(PDEV_CONTEXT DevContext)
{
    if (!DriverContext.bServiceSupportsDataFiles) {
        return false;
    }

    if (!DriverContext.bEnableDataFiles) {
        return false;
    }

    if (DevContext->ulFlags & DCF_DATA_FILES_DISABLED) {
        return false;
    }

    return true;
}

bool
NeedWritingToDataFile(PDEV_CONTEXT DevContext)
{
    ULONG   ulDaBInMemory = DevContext->lDataBlocksInUse - DevContext->lDataBlocksQueuedToFileWrite;

    if (ecServiceShutdown == DriverContext.eServiceState) {
        return false;
    }

    if (!IsDataFilesEnabledForThisDev(DevContext))
        return false;
        
    if (DevContext->eWOState != ecWriteOrderStateData)
        return false;        

    ULONG ulNumDaBPerDirtyBlock = DevContext->ulMaxDataSizePerDataModeDirtyBlock / DriverContext.ulDataBlockSize;
    if (ulDaBInMemory <= ulNumDaBPerDirtyBlock) {
        return false;
    }

    ULONG   ulFreeDataBlocks = DriverContext.ulDataBlocksInFreeDataBlockList + DriverContext.ulDataBlocksInLockedDataBlockList;
    ULONG   ulDaBFreeThresholdForFileWrite = GetFreeThresholdForFileWrite(DevContext);
    if (ulFreeDataBlocks <= ulDaBFreeThresholdForFileWrite) {
        InVolDbgPrint(DL_TRACE_L1, DM_DATA_FILTERING,
                      ("%s: FreeDaB = %#x, FreeLockedDaB = %#x, Thres = %#x\n", __FUNCTION__,
                       DriverContext.ulDataBlocksInFreeDataBlockList, DriverContext.ulDataBlocksInLockedDataBlockList,
                       ulDaBFreeThresholdForFileWrite));
        return true;
    }

    ULONG ulDaBThresholdPerDevForFileWrite = GetThresholdPerDevForFileWrite(DevContext);
    if (ulDaBInMemory > ulDaBThresholdPerDevForFileWrite) {    
        InVolDbgPrint(DL_TRACE_L1, DM_DATA_FILTERING,
                      ("%s: DaBInUse = %#x, DaBQueued = %#x, Thres = %#x\n", __FUNCTION__,
                       DevContext->lDataBlocksInUse, DevContext->lDataBlocksQueuedToFileWrite,
                       ulDaBThresholdPerDevForFileWrite));
        return true;
    }

    return false;
}

void
VCSetWOStateToMetaData(PDEV_CONTEXT DevContext, bool bServiceInitiated, etWOSChangeReason eWOSChangeReason, etCModeMetaDataReason eMetaDataModeReason)
{
    LARGE_INTEGER   liOldTickCount, liTickIncrementPerSec, liTickDiff;
    ULONG           ulSecsFromLastChange;

    ASSERT(0 != DevContext->liTickCountAtLastWOStateChange.QuadPart);
    // Close the current dirty block.
    AddLastChangeTimestampToCurrentNode(DevContext, NULL);

    liOldTickCount = DevContext->liTickCountAtLastWOStateChange;
    DevContext->liTickCountAtLastWOStateChange = KeQueryPerformanceCounter(&liTickIncrementPerSec);
    liTickDiff.QuadPart = DevContext->liTickCountAtLastWOStateChange.QuadPart - liOldTickCount.QuadPart;
    ulSecsFromLastChange = (ULONG)(liTickDiff.QuadPart / liTickIncrementPerSec.QuadPart);

    ASSERT(ecWriteOrderStateMetadata != DevContext->eWOState);
    if (ecWriteOrderStateData == DevContext->eWOState) {
        DevContext->ulNumSecondsInDataWOState += ulSecsFromLastChange;
        VCAddKernelTag(DevContext, DriverContext.DataToMetaDataModeTag, DriverContext.usDataToMetaDataModeTagLen);
    } else {
        DevContext->ulNumSecondsInBitmapWOState += ulSecsFromLastChange;
        VCAddKernelTag(DevContext, DriverContext.BitmapToMetaDataModeTag, DriverContext.usBitmapToMetaDataModeTagLen);
    }

    DevContext->ePrevWOState = DevContext->eWOState;
    DevContext->eWOState = ecWriteOrderStateMetadata;

    if (bServiceInitiated) {
        DevContext->lNumChangeToMetaDataWOStateOnUserRequest++;
    } else {
        DevContext->lNumChangeToMetaDataWOState++;
    }

    ULONG ulIndex = DevContext->ulWOSChangeInstance % INSTANCES_OF_WO_STATE_TRACKED;

    KeQuerySystemTime(&DevContext->liWOstateChangeTS[ulIndex]);
    DevContext->ulNumSecondsInWOS[ulIndex] = ulSecsFromLastChange;
    DevContext->eOldWOState[ulIndex] = DevContext->ePrevWOState;
    DevContext->eNewWOState[ulIndex] = ecWriteOrderStateMetadata;
    DevContext->ullcbDChangesPendingAtWOSchange[ulIndex] = DevContext->ChangeList.ullcbDataChangesPending;
    DevContext->ullcbMDChangesPendingAtWOSchange[ulIndex] = DevContext->ChangeList.ullcbMetaDataChangesPending;
    DevContext->ullcbBChangesPendingAtWOSchange[ulIndex] = DevContext->ChangeList.ullcbBitmapChangesPending;
    DevContext->eWOSChangeReason[ulIndex] = eWOSChangeReason;
    DevContext->ulWOSChangeInstance++;

    if (ecWriteOrderStateData == DevContext->ePrevWOState) {
        InDskFltCaptureLastNonWOSTransitionStats(ulIndex, DevContext, eWOSChangeReason, eMetaDataModeReason);
    }
    EndOrCloseCxSessionIfNeeded(DevContext);
    return;
}

void
VCSetWOStateToData(PDEV_CONTEXT DevContext, etWOSChangeReason eWOSChangeReason)
{
    LARGE_INTEGER   liOldTickCount, liTickIncrementPerSec, liTickDiff;
    ULONG           ulSecsFromLastChange;

    ASSERT(0 != DevContext->liTickCountAtLastWOStateChange.QuadPart);

    ASSERT(DevContext->ChangeList.ullcbMetaDataChangesPending == 0);
    ASSERT(DevContext->ChangeList.ullcbBitmapChangesPending == 0);
    ASSERT(ValidateDataWOSTransition(DevContext) == TRUE);
    
    AddLastChangeTimestampToCurrentNode(DevContext, NULL);

    liOldTickCount = DevContext->liTickCountAtLastWOStateChange;
    DevContext->liTickCountAtLastWOStateChange = KeQueryPerformanceCounter(&liTickIncrementPerSec);
    liTickDiff.QuadPart = DevContext->liTickCountAtLastWOStateChange.QuadPart - liOldTickCount.QuadPart;
    ulSecsFromLastChange = (ULONG)(liTickDiff.QuadPart / liTickIncrementPerSec.QuadPart);

    ASSERT(ecWriteOrderStateData != DevContext->eWOState);
    if (ecWriteOrderStateBitmap == DevContext->eWOState) {
        DevContext->ulNumSecondsInBitmapWOState += ulSecsFromLastChange;
        VCAddKernelTag(DevContext, DriverContext.BitmapToDataModeTag, DriverContext.usBitmapToDataModeTagLen);
    } else {
        DevContext->ulNumSecondsInMetaDataWOState += ulSecsFromLastChange;
        VCAddKernelTag(DevContext, DriverContext.MetaDataToDataModeTag, DriverContext.usMetaDataToDataModeTagLen);
    }

    DevContext->ePrevWOState = DevContext->eWOState;
    DevContext->eWOState = ecWriteOrderStateData;
    DevContext->lNumChangeToDataWOState++;

    ULONG ulIndex = DevContext->ulWOSChangeInstance % INSTANCES_OF_WO_STATE_TRACKED;

    KeQuerySystemTime(&DevContext->liWOstateChangeTS[ulIndex]);
    DevContext->ulNumSecondsInWOS[ulIndex] = ulSecsFromLastChange;
    DevContext->eOldWOState[ulIndex] = DevContext->ePrevWOState;
    DevContext->eNewWOState[ulIndex] = ecWriteOrderStateData;
    DevContext->ullcbDChangesPendingAtWOSchange[ulIndex] = DevContext->ChangeList.ullcbDataChangesPending;
    DevContext->ullcbMDChangesPendingAtWOSchange[ulIndex] = DevContext->ChangeList.ullcbMetaDataChangesPending;
    DevContext->ullcbBChangesPendingAtWOSchange[ulIndex] = DevContext->ChangeList.ullcbBitmapChangesPending;
    DevContext->eWOSChangeReason[ulIndex] = eWOSChangeReason;
    DevContext->ulWOSChangeInstance++;
    DevContext->DiskTelemetryInfo.LastWOSTime = DevContext->liWOstateChangeTS[ulIndex];

    EndOrCloseCxSessionIfNeeded(DevContext);
    return;
}

void
VCSetWOStateToBitmap(PDEV_CONTEXT DevContext, bool bServiceInitiated, etWOSChangeReason eWOSChangeReason, etCModeMetaDataReason eMetaDataModeReason)
{
    LARGE_INTEGER   liOldTickCount, liTickIncrementPerSec, liTickDiff;
    ULONG           ulSecsFromLastChange;

    ASSERT(0 != DevContext->liTickCountAtLastWOStateChange.QuadPart);

    AddLastChangeTimestampToCurrentNode(DevContext, NULL);

    liOldTickCount = DevContext->liTickCountAtLastWOStateChange;
    DevContext->liTickCountAtLastWOStateChange = KeQueryPerformanceCounter(&liTickIncrementPerSec);
    liTickDiff.QuadPart = DevContext->liTickCountAtLastWOStateChange.QuadPart - liOldTickCount.QuadPart;
    ulSecsFromLastChange = (ULONG)(liTickDiff.QuadPart / liTickIncrementPerSec.QuadPart);

    if (bServiceInitiated) {
        DevContext->lNumChangeToBitmapWOStateOnUserRequest++;
    } else {
        DevContext->lNumChangeToBitmapWOState++;
    }

    ASSERT(ecWriteOrderStateBitmap != DevContext->eWOState);

    if (ecWriteOrderStateData == DevContext->eWOState) {
        DevContext->ulNumSecondsInDataWOState += ulSecsFromLastChange;
    } else {
        DevContext->ulNumSecondsInMetaDataWOState += ulSecsFromLastChange;
    }

    DevContext->ePrevWOState = DevContext->eWOState;
    DevContext->eWOState = ecWriteOrderStateBitmap;

    ULONG ulIndex = DevContext->ulWOSChangeInstance % INSTANCES_OF_WO_STATE_TRACKED;

    KeQuerySystemTime(&DevContext->liWOstateChangeTS[ulIndex]);
    DevContext->ulNumSecondsInWOS[ulIndex] = ulSecsFromLastChange;
    DevContext->eOldWOState[ulIndex] = DevContext->ePrevWOState;
    DevContext->eNewWOState[ulIndex] = ecWriteOrderStateBitmap;
    DevContext->ullcbDChangesPendingAtWOSchange[ulIndex] = DevContext->ChangeList.ullcbDataChangesPending;
    DevContext->ullcbMDChangesPendingAtWOSchange[ulIndex] = DevContext->ChangeList.ullcbMetaDataChangesPending;
    DevContext->ullcbBChangesPendingAtWOSchange[ulIndex] = DevContext->ChangeList.ullcbBitmapChangesPending;
    DevContext->eWOSChangeReason[ulIndex] = eWOSChangeReason;
    DevContext->ulWOSChangeInstance++;
    if (ecWriteOrderStateData == DevContext->ePrevWOState) {
        InDskFltCaptureLastNonWOSTransitionStats(ulIndex, DevContext, eWOSChangeReason, eMetaDataModeReason);
    }
    EndOrCloseCxSessionIfNeeded(DevContext);
    return;
}

bool
VCSetWOState(PDEV_CONTEXT DevContext, bool bServiceInitiated, char *FunctionName, etCModeMetaDataReason eMetaDataModeReason)
{

#if !DBG
    UNREFERENCED_PARAMETER(FunctionName);
#endif
    // If bitmap file is not open, or bitmap file is not in ecVBitmapStateClosed and not in ecvBitmapStateReadCompleted
    // or if bitmapChanges are pending, device is in ecWriterOrderStateBitmap
    if (ecServiceShutdown == DriverContext.eServiceState) {
        if (ecWriteOrderStateBitmap == DevContext->eWOState) {
            return false;
        } else {
            InVolDbgPrint(DL_INFO, DM_WO_STATE, 
                          ("%s(%s): Service is shutdown - WOState changing from %s to ecWOStateBitmap on device %s(%s)\n", __FUNCTION__, FunctionName,
                                     WOStateString(DevContext->eWOState), DevContext->chDevID, DevContext->chDevNum));
            VCSetWOStateToBitmap(DevContext, bServiceInitiated, eCWOSChangeReasonServiceShutdown, eMetaDataModeReason);
            return true;
        }
    }

    if (DevContext->ChangeList.ulBitmapChangesPending) {
        if (ecWriteOrderStateBitmap == DevContext->eWOState) {
            return false;
        } else {
            InVolDbgPrint(DL_INFO, DM_WO_STATE, 
                          ("%s(%s): Bitmap changes pending - WOState changing from %s to ecWOStateBitmap on device %s(%s)\n", __FUNCTION__, FunctionName,
                                     WOStateString(DevContext->eWOState), DevContext->chDevID, DevContext->chDevNum));
            VCSetWOStateToBitmap(DevContext, bServiceInitiated, ecWOSChangeReasonBitmapChanges, eMetaDataModeReason);
            return true;
        }
    }

    if (NULL == DevContext->pDevBitmap) {
        if (ecWriteOrderStateBitmap == DevContext->eWOState) {
            return false;
        } else {
            InVolDbgPrint(DL_INFO, DM_WO_STATE, 
                          ("%s(%s): Bitmap is not opened - WOState changing from %s to ecWOStateBitmap on device %s(%s)\n", __FUNCTION__, FunctionName,
                                     WOStateString(DevContext->eWOState), DevContext->chDevID, DevContext->chDevNum));
            VCSetWOStateToBitmap(DevContext, bServiceInitiated, ecWOSChangeReasonBitmapNotOpen, eMetaDataModeReason);
            return true;
        }
    }

    if ((ecVBitmapStateClosed != DevContext->pDevBitmap->eVBitmapState) && 
        (ecVBitmapStateReadCompleted != DevContext->pDevBitmap->eVBitmapState))
    {
        if (ecWriteOrderStateBitmap == DevContext->eWOState) {
            return false;
        } else {
            InVolDbgPrint(DL_INFO, DM_WO_STATE, 
                          ("%s(%s): BitmapState(%s) - WOState changing from %s to ecWOStateBitmap on device %s(%s)\n", __FUNCTION__, FunctionName,
                                     VBitmapStateString(DevContext->pDevBitmap->eVBitmapState), WOStateString(DevContext->eWOState), 
                           DevContext->chDevID, DevContext->chDevNum));
            VCSetWOStateToBitmap(DevContext, bServiceInitiated, ecWOSChangeReasonBitmapState, eMetaDataModeReason);
            return true;
        }
    }

    if (TEST_FLAG(DevContext->ulFlags, DONT_CAPTURE_IN_DATA)) {
        if (ecWriteOrderStateMetadata == DevContext->eWOState) {
            return false;
        } else {
            etWOSChangeReason eWOChangeReason = ecWOSChangeReasonExplicitNonWO;
            InVolDbgPrint(DL_INFO, DM_WO_STATE,
                          ("%s(%s): Device Context is Marked not to capture in data"
                          "- WOState changing from %s to ecWOStateMetaData on device %s(%s)\n", 
                          __FUNCTION__, FunctionName,
                          WOStateString(DevContext->eWOState),
                          DevContext->chDevID,
                          DevContext->chDevNum));
            if (TEST_FLAG(DevContext->ulFlags, DCF_PAGE_FILE_MISSED)) {
                eWOChangeReason = ecWOSChangeReasonPageFileMissed;
            } else if (TEST_FLAG(DevContext->ulFlags, DCF_DONT_PAGE_FAULT)) {
                eWOChangeReason = ecWOSChangeReasonDontPageFault;
            }
            VCSetWOStateToMetaData(DevContext,
                                   bServiceInitiated,
                                   eWOChangeReason,
                                   eMetaDataModeReason);
            return true;
        }
    }

    // Not in ecWriteOrderStateBitmap
    // If MetaDataChanges are pending then volume is in ecWriterOrderStateMetadata
    if (ecCaptureModeMetaData == DevContext->eCaptureMode) {
        if (ecWriteOrderStateMetadata == DevContext->eWOState) {
            return false;
        } else {
            InVolDbgPrint(DL_INFO, DM_WO_STATE, 
                          ("%s(%s): Capture mode meta-data - WOState changing from %s to ecWOStateMetaData on device %s(%s)\n", __FUNCTION__, FunctionName,
                                     WOStateString(DevContext->eWOState), DevContext->chDevID, DevContext->chDevNum));
            VCSetWOStateToMetaData(DevContext, bServiceInitiated, ecWOSChangeReasonCaptureModeMD, eMetaDataModeReason);
            return true;
        }
    }

    if (DevContext->ChangeList.ulMetaDataChangesPending) {
        if (ecWriteOrderStateMetadata == DevContext->eWOState) {
            return false;
        } else {
            InVolDbgPrint(DL_INFO, DM_WO_STATE, 
                          ("%s(%s): Metadata changes pending - WOState changing from %s to ecWOStateMetaData on device %s(%s)\n", __FUNCTION__, FunctionName,
                                     WOStateString(DevContext->eWOState), DevContext->chDevID, DevContext->chDevNum));
            VCSetWOStateToMetaData(DevContext, bServiceInitiated, ecWOSChangeReasonMDChanges, eMetaDataModeReason);
            return true;
        }
    }

    // Not in bitmap state, not in meta-data state
    if (ecWriteOrderStateData == DevContext->eWOState) {
        return false;
    } 
    InVolDbgPrint(DL_INFO, DM_WO_STATE, 
                  ("%s(%s): Only data changes pending - WOState changing from %s to ecWOStateData on device %s(%s)\n", __FUNCTION__, FunctionName,
                             WOStateString(DevContext->eWOState), DevContext->chDevID, DevContext->chDevNum));
    VCSetWOStateToData(DevContext, ecWOSChangeReasonDChanges);
    return true;
}

bool
CanSwitchToDataCaptureMode(PDEV_CONTEXT DevContext, bool bClearDiff)
{
    if (ecServiceRunning != DriverContext.eServiceState) {
        return false;
    }

    if (TEST_FLAG(DevContext->ulFlags, DONT_CAPTURE_IN_DATA)) {
         return false;
    }

    if (ecCaptureModeData == DevContext->eCaptureMode)
        return true;

    if (!IsDataCaptureEnabledForThisDev(DevContext)) {
        return false;
    }

    // Bitmap is not read completely, we should not move into data capture mode. => Old logic
    //
    // With new changes where we track data as much as possible in memory, we must move to
    // data capture mode ASAP.
    if (NULL == DevContext->pDevBitmap) {
        return false;
    }

    ULONG usMaxDataBlocksInDirtyBlock = DevContext->ulMaxDataSizePerDataModeDirtyBlock / DriverContext.ulDataBlockSize;
    if ((DriverContext.ulDataBlocksInFreeDataBlockList + DriverContext.ulDataBlocksInLockedDataBlockList) < usMaxDataBlocksInDirtyBlock) {
        InVolDbgPrint(DL_INFO, DM_DATA_FILTERING | DM_CAPTURE_MODE, 
                      ("%s: Returning false as FreeDataBlocks(%#x) < MaxDataBlocksInDirtyBlock(%#x)\n", __FUNCTION__,
                        DriverContext.ulDataBlocksInFreeDataBlockList, usMaxDataBlocksInDirtyBlock));
        return false;
    }

    if (!bClearDiff) {
    if ((DriverContext.ulDataBlocksInFreeDataBlockList + DriverContext.ulDataBlocksInLockedDataBlockList) < 
        (((DriverContext.ulAvailableDataBlockCountInPercentage) * (DriverContext.ulMaxDataSizePerDev / DriverContext.ulDataBlockSize))/100)) {
        InVolDbgPrint(DL_INFO, DM_DATA_FILTERING | DM_CAPTURE_MODE, 
            ("%s: Returning false as DC-Free+Locked DataBlocks(%#d) < (%#d) [%#d Percentage of MaxDataBlocksPerVolume %#d]\n", __FUNCTION__,
                      DriverContext.ulDataBlocksInFreeDataBlockList + DriverContext.ulDataBlocksInLockedDataBlockList,
                      (((DriverContext.ulAvailableDataBlockCountInPercentage) * (DriverContext.ulMaxDataSizePerDev / DriverContext.ulDataBlockSize))/100),
                      DriverContext.ulAvailableDataBlockCountInPercentage,
                      DriverContext.ulMaxDataSizePerDev / DriverContext.ulDataBlockSize));
        return false;
    }

    if (DevContext->ulcbDataAllocated > (((DriverContext.ulMaxDataAllocLimitInPercentage) * (DriverContext.ulMaxDataSizePerDev))/100)) {
        InVolDbgPrint(DL_INFO, DM_DATA_FILTERING | DM_CAPTURE_MODE, 
            ("%s: Returning false as VC-ulcbDataAllocated(%#d) > (%#d) [%#d Percentage of MaxDataSizePerVolume %#d] \n", __FUNCTION__,
                      DevContext->ulcbDataAllocated, 
                      (((DriverContext.ulMaxDataAllocLimitInPercentage) * (DriverContext.ulMaxDataSizePerDev))/100),
                      DriverContext.ulMaxDataAllocLimitInPercentage,
                      DriverContext.ulMaxDataSizePerDev));
        return false;
    }
    }

    if(ecCaptureModeMetaData == DevContext->eCaptureMode){
        
        LARGE_INTEGER   liCurrentTickCount, liTickIncrementPerSec, liTickDiff;
        ULONG           ulSecsInCurrentCaptureMode;
        liCurrentTickCount = KeQueryPerformanceCounter(&liTickIncrementPerSec);
        liTickDiff.QuadPart = liCurrentTickCount.QuadPart - DevContext->liTickCountAtLastCaptureModeChange.QuadPart;
        ulSecsInCurrentCaptureMode = (ULONG)(liTickDiff.QuadPart / liTickIncrementPerSec.QuadPart);
        if(DevContext->ullcbDataAlocMissed && (ulSecsInCurrentCaptureMode <= DriverContext.ulMaxWaitTimeForIrpCompletion ))
            return false;
    }

    InVolDbgPrint(DL_INFO, DM_DATA_FILTERING | DM_CAPTURE_MODE, 
                  ("%s: Returning true.\nFreeDataBlocks(%#x), MaxDataBlocksInDB(%#x), MaxDataBlocksPerVol(%#x)\n",
                   __FUNCTION__, DriverContext.ulDataBlocksInFreeDataBlockList,
                   usMaxDataBlocksInDirtyBlock, (DriverContext.ulMaxDataSizePerDev / DriverContext.ulDataBlockSize)));
    return true;
}

void
VCSetCaptureModeToMetadata(PDEV_CONTEXT DevContext, bool bServiceInitiated)
{
    ASSERT(0 != DevContext->liTickCountAtLastCaptureModeChange.QuadPart);

    if (ecCaptureModeMetaData == DevContext->eCaptureMode) {
        return;
    }

    LARGE_INTEGER   liOldTickCount, liTickIncrementPerSec, liTickDiff;
    ULONG           ulSecsFromLastChange;

    // Close the current dirty block.
    AddLastChangeTimestampToCurrentNode(DevContext, NULL);

    liOldTickCount = DevContext->liTickCountAtLastCaptureModeChange;
    DevContext->liTickCountAtLastCaptureModeChange = KeQueryPerformanceCounter(&liTickIncrementPerSec);
    liTickDiff.QuadPart = DevContext->liTickCountAtLastCaptureModeChange.QuadPart - liOldTickCount.QuadPart;
    ulSecsFromLastChange = (ULONG)(liTickDiff.QuadPart / liTickIncrementPerSec.QuadPart);

    DevContext->eCaptureMode = ecCaptureModeMetaData;

    // Free all data buffers that are reserved.
    DevContext->ullDiskUsageArray[DevContext->ulMuCurrentInstance % INSTANCES_OF_MEM_USAGE_TRACKED] =
        DevContext->ullcbDataWrittenToDisk;
    DevContext->ulMemAllocatedArray[DevContext->ulMuCurrentInstance % INSTANCES_OF_MEM_USAGE_TRACKED] =
        DevContext->ulcbDataAllocated;
    DevContext->ulMemReservedArray[DevContext->ulMuCurrentInstance % INSTANCES_OF_MEM_USAGE_TRACKED] =
        DevContext->ulcbDataReserved;
    DevContext->ulMemFreeArray[DevContext->ulMuCurrentInstance % INSTANCES_OF_MEM_USAGE_TRACKED] = 
        DevContext->ulcbDataFree;
    DevContext->ulTotalMemFreeArray[DevContext->ulMuCurrentInstance % INSTANCES_OF_MEM_USAGE_TRACKED] =
        (DriverContext.ulDataBlocksInFreeDataBlockList + DriverContext.ulDataBlocksInLockedDataBlockList) * DriverContext.ulDataBlockSize;
    KeQuerySystemTime(&DevContext->liCaptureModeChangeTS[DevContext->ulMuCurrentInstance % INSTANCES_OF_MEM_USAGE_TRACKED]);
    DevContext->ulMuCurrentInstance++;
    VCFreeReservedDataBlockList(DevContext);
    DevContext->ullcbDataAlocMissed += DevContext->ulcbDataReserved;
    DevContext->ulcbDataReserved = 0;
    DevContext->ulcbDataFree = 0;

    DevContext->ulNumSecondsInDataCaptureMode += ulSecsFromLastChange;

    if (bServiceInitiated) {
        DevContext->lNumChangeToMetaDataCaptureModeOnUserRequest++;
    } else {
        DevContext->lNumChangeToMetaDataCaptureMode++;
    }

    return;
}

void
VCSetCaptureModeToData(PDEV_CONTEXT DevContext)
{
    ASSERT(0 != DevContext->liTickCountAtLastCaptureModeChange.QuadPart);

    if (ecCaptureModeData == DevContext->eCaptureMode) {
        return;
    }

    LARGE_INTEGER   liOldTickCount, liTickIncrementPerSec, liTickDiff;
    ULONG           ulSecsFromLastChange;

    AddLastChangeTimestampToCurrentNode(DevContext, NULL);

    liOldTickCount = DevContext->liTickCountAtLastCaptureModeChange;
    DevContext->liTickCountAtLastCaptureModeChange = KeQueryPerformanceCounter(&liTickIncrementPerSec);
    liTickDiff.QuadPart = DevContext->liTickCountAtLastCaptureModeChange.QuadPart - liOldTickCount.QuadPart;
    ulSecsFromLastChange = (ULONG)(liTickDiff.QuadPart / liTickIncrementPerSec.QuadPart);

    DevContext->eCaptureMode = ecCaptureModeData;
    DevContext->lNumChangeToDataCaptureMode++;

    DevContext->ulcbDataFree = 0;
    DevContext->ullcbDataAlocMissed = 0;
    DevContext->ullNetWritesSeenInBytes = 0;
    DevContext->ulNumSecondsInMetadataCaptureMode += ulSecsFromLastChange;
    DevContext->ulNumPendingIrps = 0; 
    return;
}

VOID
FlushAndCloseBitmapFile(PDEV_CONTEXT pDevContext)
{
    KIRQL               OldIrql;
    PDEV_BITMAP      pDevBitmap;
    
    if (NULL == pDevContext)
        return;
    
    InMageFltSaveAllChanges(pDevContext, TRUE, NULL);

    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
    pDevBitmap = pDevContext->pDevBitmap;
    pDevContext->pDevBitmap = NULL;
    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);

    if (pDevBitmap) {
        CloseBitmapFile(pDevBitmap, false, pDevContext, cleanShutdown, false);
    }

    return;
}

VOID
ProcessDevContextWorkItems(PWORK_QUEUE_ENTRY pWorkItem)
{
    PDEV_CONTEXT     pDevContext = NULL;
    PDEV_BITMAP      pDevBitmap = NULL;
    KIRQL               OldIrql;

    PAGED_CODE();

    ASSERT(NULL != pWorkItem);
    if (NULL == pWorkItem) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN,
            ("ProcessDevContextWorkItems: Called with NULL pWorkItemt\n"));
        return;
    }

    ASSERT(NULL != pWorkItem->Context);
    if (NULL == pWorkItem->Context) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN,
            ("ProcessDevContextWorkItems: Type = %#x called with NULL pDevContext\n", pWorkItem->eWorkItem));
        return;
    }

    pDevContext = (PDEV_CONTEXT)pWorkItem->Context;
    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
    if (pDevContext->pDevBitmap)
        pDevBitmap = ReferenceDevBitmap(pDevContext->pDevBitmap);
    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);

    switch(pWorkItem->eWorkItem) {
    case ecWorkItemOpenBitmap:
        if (NULL == pDevBitmap) {
            NTSTATUS    Status = STATUS_SUCCESS;

            InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, 
                ("ProcessDevContextWorkItems: Opening bitmap for Volume %S(%S) DevContext %p\n",
                    pDevContext->wDevID, pDevContext->wDevNum, pDevContext));
            
            pDevBitmap = OpenBitmapFile(pDevContext, Status);
            
            KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
#ifdef VOLUME_CLUSTER_SUPPORT
            if (IS_NOT_CLUSTER_VOLUME(pDevContext)) {
#endif
                ASSERT(NULL == pDevContext->pDevBitmap);
                ASSERT(pDevContext->ulFlags & DCF_OPEN_BITMAP_REQUESTED);
#ifdef VOLUME_CLUSTER_SUPPORT
            }
#endif

            pDevContext->ulFlags &= ~DCF_OPEN_BITMAP_REQUESTED;
            if (NULL != pDevBitmap) {
                pDevContext->pDevBitmap = ReferenceDevBitmap(pDevBitmap);
                if(pDevContext->bQueueChangesToTempQueue) {
                    MoveUnupdatedDirtyBlocks(pDevContext);
                }
            } else {
                // Set error for open bitmap. So that further bitmap opens are not done.
                InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("ProcessDevContextWorkItems: Open bitmap for device %s has failed\n",
                        pDevContext->chDevNum));
                SetBitmapOpenError(pDevContext, true, Status);
            }
            KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
        }
        break;

    case ecWorkItemBitmapWrite:
        if (NULL != pDevBitmap) {
            InVolDbgPrint(DL_INFO, DM_BITMAP_WRITE, 
                ("ProcessDevContextWorkItems: writing to bitmap for Volume %S(%S\n",
                    pDevContext->wDevID, pDevContext->wDevNum));
         
            ExAcquireFastMutex(&pDevBitmap->Mutex);

            KeAcquireSpinLock(&pDevContext->Lock, &OldIrql); 
            ASSERT((ecWriteOrderStateBitmap == pDevContext->eWOState) ||
                   (ecVBitmapStateReadCompleted == pDevBitmap->eVBitmapState) ||
                   (ecVBitmapStateClosed == pDevBitmap->eVBitmapState) ||
                   (ecVBitmapStateReadError == pDevBitmap->eVBitmapState) ||
                   (ecVBitmapStateInternalError == pDevBitmap->eVBitmapState));
                   
            // Set state to adding changes to bitmap
            pDevBitmap->eVBitmapState = ecVBitmapStateAddingChanges;

            if (ecWriteOrderStateBitmap != pDevContext->eWOState) {
                VCSetWOState(pDevContext, false, __FUNCTION__, ecMDReasonBitmapWrite);
            }

            VCSetCaptureModeToMetadata(pDevContext, false);

            ASSERT(NULL != pDevContext->pDevBitmap);
            QueueWorkerRoutineForBitmapWrite(pDevContext, pWorkItem->ulInfo1);
            KeReleaseSpinLock(&pDevContext->Lock, OldIrql);

            ExReleaseFastMutex(&pDevBitmap->Mutex);
        } else {
            SetBitmapOpenFailDueToLossOfChanges(pDevContext, false);
        }
        break;
    case ecWorkItemStartBitmapRead:
        if (NULL != pDevBitmap) {
            InVolDbgPrint(DL_INFO, DM_BITMAP_READ, 
                ("ProcessDevContextWorkItems: starting read from bitmap for Volume %S(%S)\n",
                    pDevContext->wDevID, pDevContext->wDevNum));

		  //Fix For Bug 27337
		  //By now Bitmap file should be in opened state. 
#ifdef VOLUME_FLT
			if( (!pDevContext->FsInfoWrapper.FsInfoFetchRetries) && \
	  		(0 >= pDevContext->FsInfoWrapper.FsInfo.MaxVolumeByteOffsetForFs) &&\
	  		(1 < pDevContext->UniqueVolumeName.Length)){

				pDevContext->FsInfoWrapper.FsInfoNtStatus = \
				InMageFltFetchFsEndClusterOffset(&pDevContext->UniqueVolumeName,
										 &pDevContext->FsInfoWrapper.FsInfo);

		      if(STATUS_SUCCESS == pDevContext->FsInfoWrapper.FsInfoNtStatus){
				pDevContext->pDevBitmap->pBitmapAPI->pFsInfo = &pDevContext->FsInfoWrapper.FsInfo;
			  }
			  InterlockedIncrement(&pDevContext->FsInfoWrapper.FsInfoFetchRetries);
			}
#endif
	            
            ExAcquireFastMutex(&pDevBitmap->Mutex);
            if ((ecVBitmapStateOpened == pDevBitmap->eVBitmapState) ||
                (ecVBitmapStateAddingChanges == pDevBitmap->eVBitmapState))
            {
                pDevBitmap->eVBitmapState = ecVBitmapStateReadStarted;
                QueueWorkerRoutineForStartBitmapRead(pDevBitmap);
            }
            ExReleaseFastMutex(&pDevBitmap->Mutex);
        } else {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE,
                ("ProcessDevContextWorkItems: write bitmap called with NULL pDevContext\n"));
        }
        break;
    case ecWorkItemContinueBitmapRead:
        if (NULL != pDevBitmap) {
            InVolDbgPrint(DL_INFO, DM_BITMAP_READ, 
                ("ProcessDevContextWorkItems: starting read from bitmap for Volume %S(%S)\n",
                    pDevContext->wDevID, pDevContext->wDevNum));
            
            ExAcquireFastMutex(&pDevBitmap->Mutex);
            if ((ecVBitmapStateReadStarted == pDevBitmap->eVBitmapState) ||
                (ecVBitmapStateReadPaused == pDevBitmap->eVBitmapState))
            {
                pDevBitmap->eVBitmapState = ecVBitmapStateReadStarted;
                QueueWorkerRoutineForContinueBitmapRead(pDevBitmap);
            }
            ExReleaseFastMutex(&pDevBitmap->Mutex);
        } else {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE,
                ("ProcessDevContextWorkItems: write bitmap called with NULL pDevContext\n"));
        }
        break;
    case ecWorkItemForDeviceUnload:
#ifdef VOLUME_FLT
        if (!IsListEmpty(&pDevContext->ChangeList.Head)) {
            if (NULL == pDevBitmap) {
                NTSTATUS    Status = STATUS_SUCCESS;
                pDevBitmap = OpenBitmapFile(pDevContext, Status);
            }

            KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);            
            ASSERT(NULL != pDevContext->pDevBitmap);
            QueueWorkerRoutineForBitmapWrite(pDevContext, 0);
            KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
        }

        DriverContext.DataFileWriterManager->FreeDevWriter(pDevContext);
        FlushAndCloseBitmapFile(pDevContext);
#else
        DriverContext.DataFileWriterManager->FreeDevWriter(pDevContext);
        PostRemoveDevice(pDevContext);
#endif
        break;

    default:
        InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT,
            ("ProcessDevContextWorkItems: Unknown work item type %#x\n", pWorkItem->eWorkItem));
        break;
    }

    DereferenceDevContext(pDevContext);
    if (NULL != pDevBitmap)
        DereferenceDevBitmap(pDevBitmap);

    return;
}

/*-----------------------------------------------------------------------------
 *
 * Worker Functions for Volume Context.
 *
 *-----------------------------------------------------------------------------
 */

// This routine is called with pDevContext->Lock held.
BOOLEAN
AddVCWorkItemToList(
    etWorkItem          eWorkItem,
    PDEV_CONTEXT     pDevContext,
    ULONG               ulAdditionalInfo,
    BOOLEAN             bOpenBitmapIfRequired,
    PLIST_ENTRY         ListHead
    )
{
    PWORK_QUEUE_ENTRY   pWorkQueueEntry = NULL;

    if (eWorkItem != ecWorkItemOpenBitmap) {
        if ((NULL == pDevContext->pDevBitmap) &&
            (0 == (pDevContext->ulFlags & DCF_OPEN_BITMAP_REQUESTED)) &&
            bOpenBitmapIfRequired)
        {
            InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, ("AddVCWorkItemToList: Queing Open bitmap for Volume=%#p, DevNum=%s\n", 
                        pDevContext, pDevContext->chDevNum));
            pWorkQueueEntry = AllocateWorkQueueEntry();
            if (NULL != pWorkQueueEntry) {
                pDevContext->ulFlags |= DCF_OPEN_BITMAP_REQUESTED;
                pWorkQueueEntry->eWorkItem = ecWorkItemOpenBitmap;
                pWorkQueueEntry->Context = ReferenceDevContext(pDevContext);
                pWorkQueueEntry->ulInfo1 = 0;
                InsertTailList(ListHead, &pWorkQueueEntry->ListEntry);
                pWorkQueueEntry = NULL;
            } else {
                InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT | DM_BITMAP_OPEN, 
                    ("AddVCWorkItemToList: Failed to allocate WorkQueueEntry for bitmap open\n"));
                return FALSE;
            }
        }
    }

    pWorkQueueEntry = AllocateWorkQueueEntry();
    if (NULL != pWorkQueueEntry) {
        pWorkQueueEntry->eWorkItem = eWorkItem;
        pWorkQueueEntry->Context = ReferenceDevContext(pDevContext);
        pWorkQueueEntry->ulInfo1 = ulAdditionalInfo;
        InsertTailList(ListHead, &pWorkQueueEntry->ListEntry);
    } else {
        InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT, 
            ("AddVCWorkItemToList: Failed to allocate WorkQueueEntry for type %#x\n", eWorkItem));
        return FALSE;
    }

    return TRUE;
}


NTSTATUS
OpenDeviceParametersRegKey(PDEV_CONTEXT pDevContext, Registry **ppVolumeParam)
{
    Registry        *pDevParam;
    NTSTATUS        Status;

    PAGED_CODE();

    if (NULL == ppVolumeParam) {
        InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT, ("OpenDeviceParametersRegKey: ppVolumeParam NULL\n"));
        return STATUS_INVALID_PARAMETER;
    }

    pDevParam = new Registry();
    if (NULL == pDevParam) {
        InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT, 
            ("OpenDeviceParametersRegKey: Failed in creating registry object\n"));
        return STATUS_NO_MEMORY;
    }

    Status = pDevParam->OpenKeyReadWrite(&pDevContext->DevParameters);
    if (!NT_SUCCESS(Status)) {
        Status = pDevParam->CreateKeyReadWrite(&pDevContext->DevParameters);
        if (!NT_SUCCESS(Status)) {
            InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT, 
                ("OpenDeviceParametersRegKey: Failed to Creat reg key %wZ with Status 0x%x", 
                 &pDevContext->DevParameters, Status));

            delete pDevParam;
            pDevParam = NULL;
        } else {
            InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT, 
                ("OpenDeviceParametersRegKey: Created reg key %wZ", &pDevContext->DevParameters));
        }
    } else {
        InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT, 
            ("OpenDeviceParametersRegKey: Opened reg key %wZ", &pDevContext->DevParameters));
    }

    *ppVolumeParam = pDevParam;
    return Status;
}

NTSTATUS
InitializeDataLogDirectory(PDEV_CONTEXT DevContext, PWSTR pDataLogDirectory, bool bCustom)
{
    NTSTATUS        Status = STATUS_SUCCESS;
#ifdef VOLUME_FLT
#ifdef VOLUME_CLUSTER_SUPPORT
    PBASIC_VOLUME   BasicVolume;
#endif
#endif
    bool            bWriteBackToRegistry = false;

    // If Directory name is read from volume specific parameters keep that.
    // If volume specific parameters does not have data log directory name
    // compute it by checking if it is a cluster volume or not
    if ((0 == DevContext->DataLogDirectory.Length) || (bCustom)) {
#ifdef VOLUME_FLT
#ifdef VOLUME_CLUSTER_SUPPORT
        // Directory not read from volume parameters
        // If Volume is a clustered volume
        // Check if this volume is a cluster volume.
        BasicVolume = DevContext->pBasicVolume;
        ASSERT((NULL == BasicVolume) || (NULL != BasicVolume->pDisk));
        if ((NULL != BasicVolume) && (NULL != BasicVolume->pDisk) && 
            (BasicVolume->pDisk->ulFlags & BD_FLAGS_IS_CLUSTER_RESOURCE))
        {
            if (!bCustom) {
                // Volume on clusdisk. So create DataLogDirectory.
                InMageCopyUnicodeString(&DevContext->DataLogDirectory, &DevContext->UniqueVolumeName);
                InMageAppendUnicodeString(&DevContext->DataLogDirectory, DEFAULT_DATA_LOG_DIR_FOR_CLUSDISKS);
            } else {
            	return STATUS_INVALID_PARAMETER;
            }
        } else {
#endif
#endif
            InMageCopyUnicodeString(&DevContext->DataLogDirectory, pDataLogDirectory);
            InMageTerminateUnicodeStringWith(&DevContext->DataLogDirectory, L'\\');
            InMageAppendUnicodeString(&DevContext->DataLogDirectory, &DevContext->wDevID[0]);
            InMageTerminateUnicodeStringWith(&DevContext->DataLogDirectory, L'\\');
            if (bCustom) {
                KIRQL OldIrql;
                KeAcquireSpinLock(&DevContext->Lock, &OldIrql);
                if (DevContext->ulFlags & DCF_DATALOG_DIRECTORY_VALIDATED) {
                    DevContext->ulFlags &= ~DCF_DATALOG_DIRECTORY_VALIDATED;
                }
                KeReleaseSpinLock(&DevContext->Lock, OldIrql);
            }
#ifdef VOLUME_CLUSTER_SUPPORT
        }
#endif

        // Write this path back to registry
        bWriteBackToRegistry = true;
    } else {
        InMageTerminateUnicodeStringWith(&DevContext->DataLogDirectory, L'\\');
    }

    //This code is commented for fixing system not responding issue in win2k8 r2 bug id 9338. We are validating datalogdirectory in GenerateDataFilename
    /*
    Status = ValidatePathForFileName(&DevContext->DataLogDirectory);
    if (NT_SUCCESS(Status)) {
        DevContext->ulFlags |= DCF_DATALOG_DIRECTORY_VALIDATED;
    }
    */
    if (bWriteBackToRegistry) {
       SetStringInRegVolumeCfg(DevContext, DEVICE_DATAFILELOG_DIRECTORY, &DevContext->DataLogDirectory);
    }

    return Status;
}

NTSTATUS
SetDWORDInRegVolumeCfg(PDEV_CONTEXT pDevContext, PWCHAR pValueName, ULONG ulValue, bool bFlush)
{

    NTSTATUS    Status = STATUS_SUCCESS;
    Registry    *pDevParam = NULL;

    ASSERT((NULL != pDevContext) && 
            (pDevContext->ulFlags & DCF_DEVID_OBTAINED));
    PAGED_CODE();

    InVolDbgPrint(DL_FUNC_TRACE, DM_DEV_CONTEXT, 
        ("SetDWORDInRegVolumeCfg: Called - DevContext(%p)\n", pDevContext));

    if (NULL == pDevContext) {
        InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT, ("SetDWORDInRegVolumeCfg: pDevContext NULL\n"));
        return STATUS_INVALID_PARAMETER_1;
    }

    if (!(pDevContext->ulFlags & DCF_DEVID_OBTAINED)) {
        InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT, ("SetDWORDInRegVolumeCfg: Called before GUID is obtained\n"));
        return STATUS_INVALID_PARAMETER_2;
    }

    // Let us open/create the registry key for this volume.
    Status = OpenDeviceParametersRegKey(pDevContext, &pDevParam);
    if (NULL == pDevParam) {
        InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT, 
                ("SetDWORDInRegVolumeCfg: Failed to open volume parameters key"));
        return Status;
    }
    
    Status = pDevParam->WriteULONG(pValueName, ulValue);
    if (NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT, 
            ("SetDWORDInRegVolumeCfg: Added %S with value %#x for GUID %S\n", pValueName, ulValue,
                                    pDevContext->wDevID));
        if (bFlush) {
            Status = pDevParam->FlushKey();
        }
    } else {
        InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT, 
            ("SetDWORDInRegVolumeCfg: Adding %S with value %#x for GUID %S Failed with Status 0x%x\n",
                                    pValueName, ulValue, pDevContext->wDevID, Status));
    }

    delete pDevParam;
    return STATUS_SUCCESS;
}

NTSTATUS
SetStringInRegVolumeCfg(PDEV_CONTEXT pDevContext, PWCHAR pValueName, PUNICODE_STRING pValue)
{
    NTSTATUS    Status = STATUS_SUCCESS;

    Registry    *pDevParam = NULL;

    ASSERT((NULL != pDevContext) && 
            (pDevContext->ulFlags & DCF_DEVID_OBTAINED));
    PAGED_CODE();

    InVolDbgPrint(DL_FUNC_TRACE, DM_DEV_CONTEXT, 
        ("SetStringInRegVolumeCfg: Called - DevContext(%p)\n", pDevContext));

    if (NULL == pDevContext) {
        InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT, ("SetStringInRegVolumeCfg: pDevContext NULL\n"));
        return STATUS_INVALID_PARAMETER_1;
    }

    if (!(pDevContext->ulFlags & DCF_DEVID_OBTAINED)) {
        InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT, ("SetStringInRegVolumeCfg: Called before GUID is obtained\n"));
        return STATUS_INVALID_PARAMETER_2;
    }

    // Let us open/create the registry key for this volume.
    Status = OpenDeviceParametersRegKey(pDevContext, &pDevParam);
    if (NULL == pDevParam) {
        InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT, 
                ("SetStringInRegVolumeCfg: Failed to open volume parameters key"));
        return Status;
    }
    
    Status = pDevParam->WriteUnicodeString(pValueName, pValue);
    if (NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT, 
            ("SetStringInRegVolumeCfg: Added %S with value %wZ for GUID %S\n", pValueName, pValue,
                                    pDevContext->wDevID));
    } else {
        InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT, 
            ("SetStringInRegVolumeCfg: Adding %S with value %wZ for GUID %S Failed with Status 0x%x\n",
                                    pValueName, pValue, pDevContext->wDevID, Status));
    }

    delete pDevParam;
    return Status;
}

/*-----------------------------------------------------------------------------
 *  Functions releated to Allocate, Deallocate, Reference and Dereference 
 *  DevContext
 *-----------------------------------------------------------------------------
 */
VOID
DeallocateDevContext(PDEV_CONTEXT   pDevContext);

PDEV_CONTEXT
AllocateDevContext(VOID)
{
    PDEV_CONTEXT     pDevContext = NULL;
    ULONG               ulAllocationSize;
    USHORT              usVolumeParametersSize;

    // Allocate enough memory for DEV_CONTEXT and for the buffer to store 
    // Volume Parameters registry key and out of sync timestamp.
    usVolumeParametersSize = DriverContext.DriverParameters.Length +
                                VOLUME_NAME_SIZE_IN_BYTES + sizeof(WCHAR);
    ulAllocationSize = sizeof(DEV_CONTEXT) + usVolumeParametersSize ;

    pDevContext = (PDEV_CONTEXT) ExAllocatePoolWithTag(
                                NonPagedPool, 
                                ulAllocationSize, 
                                TAG_DEV_CONTEXT);
    if (pDevContext) {
        RtlZeroMemory(pDevContext, ulAllocationSize);
        pDevContext->pReSyncWorkQueueEntry = AllocateWorkQueueEntry();
        if (NULL == pDevContext->pReSyncWorkQueueEntry) {
            ExFreePoolWithTag(pDevContext, TAG_DEV_CONTEXT);
            return NULL;
        }
        pDevContext->lRefCount = 1;
        InterlockedIncrement(&DriverContext.lCntDevContextAllocs);
        pDevContext->uliTransactionId.QuadPart = 1;
        pDevContext->bNotify = false;
#ifdef VOLUME_NO_REBOOT_SUPPORT
        pDevContext->eContextType = ecDevContext;
#endif
        InitializeChangeList(&pDevContext->ChangeList);

        pDevContext->eDSDBCdeviceState = ecDSDBCdeviceOnline;
        pDevContext->eCaptureMode = ecCaptureModeUninitialized;
        pDevContext->ePrevWOState = ecWriteOrderStateUnInitialized;
        pDevContext->eWOState = ecWriteOrderStateUnInitialized;
        pDevContext->liTickCountAtLastCaptureModeChange.QuadPart = 0;
        pDevContext->liTickCountAtLastWOStateChange.QuadPart = 0;
#ifdef VOLUME_FLT
        RtlInitializeBitMap(&pDevContext->DriveLetterBitMap, 
                            &pDevContext->BufferForDriveLetterBitMap,
                            MAX_NUM_DRIVE_LETTERS);
#endif
        KeInitializeSpinLock(&pDevContext->Lock);
        KeInitializeMutex(&pDevContext->Mutex, 0);
        KeInitializeMutex(&pDevContext->BitmapOpenCloseMutex, 0);
        KeInitializeEvent(&pDevContext->SyncEvent, SynchronizationEvent, TRUE);
        KeInitializeEvent(&pDevContext->hReleaseWritesEvent, SynchronizationEvent, FALSE);
        KeInitializeEvent(&pDevContext->hReleaseWritesThreadShutdown, SynchronizationEvent, FALSE);
        KeInitializeEvent(&pDevContext->ClusterNotificationEvent, SynchronizationEvent, FALSE);
#ifdef VOLUME_FLT
        pDevContext->UniqueVolumeName.MaximumLength = 
                MAX_UNIQUE_VOLUME_NAME_SIZE_IN_WCHARS * sizeof(WCHAR);
        pDevContext->UniqueVolumeName.Buffer =
                        (PWCHAR)pDevContext->BufferForUniqueVolumeName;
#endif
        pDevContext->BitmapFileName.MaximumLength =
                MAX_LOG_PATHNAME * sizeof(WCHAR);
        pDevContext->BitmapFileName.Buffer = 
                        (PWCHAR)pDevContext->BufferForBitmapFileName;
        pDevContext->DevParameters.MaximumLength = usVolumeParametersSize;
        pDevContext->DevParameters.Buffer = 
                (PWCHAR)((PCHAR)pDevContext + sizeof(DEV_CONTEXT));

#ifdef VOLUME_FLT
        pDevContext->wDevNum[1] = L':';
#if DBG
        pDevContext->chDevNum[1] = L':';
#endif
#else
		pDevContext->wDevNum[0] = L'?';
#if DBG
	    pDevContext->chDevNum[0] = L'?';
#endif
#endif
        InitializeListHead(&pDevContext->ReservedDataBlockList);
        pDevContext->LastCommittedTimeStamp = 0;
        pDevContext->LastCommittedSequenceNumber = 0;
        pDevContext->LastCommittedSplitIoSeqId = 0;
        // Added for Debugging - Timestamp/SequenceNumber/SequenceId read on opening either Bitmap or Registry on reboot
        pDevContext->LastCommittedTimeStampReadFromStore = 0;
        pDevContext->LastCommittedSequenceNumberReadFromStore = 0;
        pDevContext->LastCommittedSplitIoSeqIdReadFromStore = 0;

        pDevContext->ullTotalTrackedBytes = 0;
        pDevContext->ullTotalIoCount = 0;
        pDevContext->ullTotalCommitLatencyInDataMode = 0;
        pDevContext->ullTotalCommitLatencyInNonDataMode = 0;
        pDevContext->ulIoSizeArray[0] = DC_DEFAULT_IO_SIZE_BUCKET_0;
        pDevContext->ulIoSizeArray[1] = DC_DEFAULT_IO_SIZE_BUCKET_1;
        pDevContext->ulIoSizeArray[2] = DC_DEFAULT_IO_SIZE_BUCKET_2;
        pDevContext->ulIoSizeArray[3] = DC_DEFAULT_IO_SIZE_BUCKET_3;
        pDevContext->ulIoSizeArray[4] = DC_DEFAULT_IO_SIZE_BUCKET_4;
        pDevContext->ulIoSizeArray[5] = DC_DEFAULT_IO_SIZE_BUCKET_5;
        pDevContext->ulIoSizeArray[6] = DC_DEFAULT_IO_SIZE_BUCKET_6;
        pDevContext->ulIoSizeArray[7] = DC_DEFAULT_IO_SIZE_BUCKET_7;
        pDevContext->ulIoSizeArray[8] = DC_DEFAULT_IO_SIZE_BUCKET_8;
        pDevContext->ulIoSizeArray[9] = DC_DEFAULT_IO_SIZE_BUCKET_9;
        pDevContext->ulIoSizeArray[10] = DC_DEFAULT_IO_SIZE_BUCKET_10;
        pDevContext->ulIoSizeArray[11] = 0;

        pDevContext->DeviceProfile.ReadProfile.ReadIoSizeArray[0] = DC_DEFAULT_IO_SIZE_BUCKET_0;
        pDevContext->DeviceProfile.ReadProfile.ReadIoSizeArray[1] = DC_DEFAULT_IO_SIZE_BUCKET_1;
        pDevContext->DeviceProfile.ReadProfile.ReadIoSizeArray[2] = DC_DEFAULT_IO_SIZE_BUCKET_2;
        pDevContext->DeviceProfile.ReadProfile.ReadIoSizeArray[3] = DC_DEFAULT_IO_SIZE_BUCKET_3;
        pDevContext->DeviceProfile.ReadProfile.ReadIoSizeArray[4] = DC_DEFAULT_IO_SIZE_BUCKET_4;
        pDevContext->DeviceProfile.ReadProfile.ReadIoSizeArray[5] = DC_DEFAULT_IO_SIZE_BUCKET_5;
        pDevContext->DeviceProfile.ReadProfile.ReadIoSizeArray[6] = DC_DEFAULT_IO_SIZE_BUCKET_6;
        pDevContext->DeviceProfile.ReadProfile.ReadIoSizeArray[7] = DC_DEFAULT_IO_SIZE_BUCKET_7;
        pDevContext->DeviceProfile.ReadProfile.ReadIoSizeArray[8] = DC_DEFAULT_IO_SIZE_BUCKET_8;
        pDevContext->DeviceProfile.ReadProfile.ReadIoSizeArray[9] = DC_DEFAULT_IO_SIZE_BUCKET_9;
        pDevContext->DeviceProfile.ReadProfile.ReadIoSizeArray[10] = DC_DEFAULT_IO_SIZE_BUCKET_10;
        pDevContext->DeviceProfile.ReadProfile.ReadIoSizeArray[11] = 0;
        pDevContext->DeviceProfile.ReadProfile.MostFrequentlyUsedBucket = 1;

        pDevContext->LatencyDistForNotify = new ValueDistribution(MAX_DISTRIBUTION_FOR_NOTIFY_LATENCY, 
                                                                    MAX_DISTRIBUTION_FOR_NOTIFY_LATENCY,
                                                                     (PULONG)NotifyLatencyBucketsInMicroSeconds);
        pDevContext->LatencyLogForNotify = new ValueLog(MAX_NOTIFY_LATENCY_LOG_SIZE);
        pDevContext->LatencyDistForCommit = new ValueDistribution(MAX_DISTRIBUTION_FOR_COMMIT_LATENCY, 
                                                                    MAX_DISTRIBUTION_FOR_COMMIT_LATENCY,
                                                                     (PULONG)CommitLatencyBucketsInMicroSeconds);
        pDevContext->LatencyLogForCommit = new ValueLog(MAX_COMMIT_LATENCY_LOG_SIZE);
        pDevContext->LatencyDistForS2DataRetrieval = new ValueDistribution(MAX_DISTRIBUTION_FOR_S2_DATA_RETRIEVAL_LATENCY,
                                                                              MAX_DISTRIBUTION_FOR_S2_DATA_RETRIEVAL_LATENCY,
                                                                              (PULONG)S2DataRetrievalLatencyBucketsInMicroSeconds);
        pDevContext->LatencyLogForS2DataRetrieval = new ValueLog(MAX_S2_DATA_RETRIEVAL_LATENCY_LOG_SIZE);

        pDevContext->bDataLogLimitReached = false;
#ifdef VOLUME_FLT
        // By Default, Let's Move the changes to Temp Queue
        // Let's make it False, When we determine the Volume is a Non-Cluster Volume
        pDevContext->bQueueChangesToTempQueue = true;
#endif
        pDevContext->ulPendingTSOTransactionID.QuadPart = 0;
        pDevContext->TSOEndSequenceNumber = 0;
        pDevContext->TSOEndTimeStamp = 0;
        pDevContext->TSOSequenceId = 0;
#ifdef VOLUME_FLT
        pDevContext->ulDiskNumber = 0xffffffff;
#endif
        pDevContext->ulMaxDataSizePerDataModeDirtyBlock = DriverContext.ulMaxDataSizePerDataModeDirtyBlock;
        // Added for debugging same tag seen twice issue
        pDevContext->ulTagsinWOState = 0;
        pDevContext->ulTagsinNonWOSDrop = 0;
        pDevContext->MDFirstChangeTS = 0;
        pDevContext->EndTimeStampofDB = 0;
        pDevContext->NTFSDevObj = NULL;
#ifndef VOLUME_FLT //Initialize Disk Variables
        pDevContext->ulDevNum = 0xFFFFFFFF;
        pDevContext->DiskPartitionFormat = PARTITION_STYLE_RAW;
#endif
		//DTAP check end
		//TODO - This need to be removed when data notify limit is set as tunable
		pDevContext->ulcbDataNotifyThreshold = 8192;
		pDevContext->ulcbDataNotifyThreshold *= KILOBYTES;
        pDevContext->IoCounterWithPagingIrpSet = 0;
        pDevContext->IoCounterWithSyncPagingIrpSet = 0;
        pDevContext->ullGroupingID = 0;
        pDevContext->ulMaxBitmapHdrWriteGroups = MAX_WRITE_GROUPS_IN_BITMAP_HEADER_DEFAULT_VALUE;
   }

    return pDevContext;
}

// This function is called with volume context lock held
VOID
AddDirtyBlockToDevContext(
    PDEV_CONTEXT     DevContext,
    PKDIRTY_BLOCK       DirtyBlock,
    ULONG               ulDataSource,
    bool                bContinuation
)
{
    ASSERT(NULL != DirtyBlock);
    ASSERT(NULL != DirtyBlock->ChangeNode);

    // Reference the node as we are going to add this node to list.
    PCHANGE_NODE    ChangeNode = ReferenceChangeNode(DirtyBlock->ChangeNode);

    // Set Data Source
    DirtyBlock->ulDataSource = ulDataSource;

    ASSERT((INFLTDRV_DATA_SOURCE_DATA == DirtyBlock->ulDataSource) || (DirtyBlock->eWOState != ecWriteOrderStateData));

    // Generate transaction ID
    DirtyBlock->uliTransactionID.QuadPart = DevContext->uliTransactionId.QuadPart++;
    if (0 == DirtyBlock->uliTransactionID.QuadPart)
        DirtyBlock->uliTransactionID.QuadPart = DevContext->uliTransactionId.QuadPart++;

    if (DevContext->bQueueChangesToTempQueue && IsListEmpty(&DevContext->ChangeList.TempQueueHead)) {
        ASSERT(NULL == DevContext->ChangeList.CurrentNode);
        GenerateTimeStampTag(&DirtyBlock->TagTimeStampOfFirstChange);
        DirtyBlock->liTickCountAtFirstIO = KeQueryPerformanceCounter(NULL);
        InsertTailList(&DevContext->ChangeList.TempQueueHead, &ChangeNode->ListEntry);
    } else if (!DevContext->bQueueChangesToTempQueue && IsListEmpty(&DevContext->ChangeList.Head)) { 
        // Let's see any changes added to temp queue before ioctl MountDevQueryUniqueId for non-cluster volume
        if (!IsListEmpty(&DevContext->ChangeList.TempQueueHead)) {
#ifdef VOLUME_CLUSTER_SUPPORT
            ASSERT(!IS_CLUSTER_VOLUME(DevContext));
#endif
            MoveUnupdatedDirtyBlocks(DevContext);
        } else {
            ASSERT(NULL == DevContext->ChangeList.CurrentNode);
        }

        GenerateTimeStampTag(&DirtyBlock->TagTimeStampOfFirstChange);
        DirtyBlock->liTickCountAtFirstIO = KeQueryPerformanceCounter(NULL);
        InsertTailList(&DevContext->ChangeList.Head, &ChangeNode->ListEntry);
    } else {
        ASSERT(NULL != DevContext->ChangeList.CurrentNode);

        PCHANGE_NODE    LastChangeNode = DevContext->ChangeList.CurrentNode;

        if (bContinuation) {
            ASSERT(ecChangeNodeDirtyBlock == LastChangeNode->eChangeNode);
            PKDIRTY_BLOCK   LastDirtyBlock = LastChangeNode->DirtyBlock;

            if (0 == LastDirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601) {
                CopyStreamRecord(&LastDirtyBlock->TagTimeStampOfLastChange,
                                 &LastDirtyBlock->TagTimeStampOfFirstChange,
                                 sizeof(TIME_STAMP_TAG_V2));
                LastDirtyBlock->liTickCountAtLastIO.QuadPart = 
                    LastDirtyBlock->liTickCountAtFirstIO.QuadPart;
            }

            CopyStreamRecord(&DirtyBlock->TagTimeStampOfFirstChange,
                             &LastDirtyBlock->TagTimeStampOfFirstChange,
                             sizeof(TIME_STAMP_TAG_V2));
            DirtyBlock->liTickCountAtFirstIO.QuadPart = LastDirtyBlock->liTickCountAtFirstIO.QuadPart;

            CopyStreamRecord(&DirtyBlock->TagTimeStampOfLastChange,
                             &LastDirtyBlock->TagTimeStampOfFirstChange,
                             sizeof(TIME_STAMP_TAG_V2));
            DirtyBlock->liTickCountAtLastIO.QuadPart = LastDirtyBlock->liTickCountAtFirstIO.QuadPart;
        } else {
            AddLastChangeTimestampToCurrentNode(DevContext, NULL);
            GenerateTimeStampTag(&DirtyBlock->TagTimeStampOfFirstChange);
            DirtyBlock->liTickCountAtFirstIO = KeQueryPerformanceCounter(NULL);
        }

        if (DevContext->bQueueChangesToTempQueue) {
            InsertTailList(&DevContext->ChangeList.TempQueueHead, &ChangeNode->ListEntry);
        } else {
            InsertTailList(&DevContext->ChangeList.Head, &ChangeNode->ListEntry);
        }

    }

    DevContext->ChangeList.CurrentNode = ChangeNode;
    DevContext->ChangeList.lDirtyBlocksInQueue++;

    if (DirtyBlock->eWOState == ecWriteOrderStateData &&
            DirtyBlock->ulDataSource == INFLTDRV_DATA_SOURCE_DATA) {
                DevContext->NumChangeNodeInDataWOSCreated++;
    }

    if(!DevContext->bQueueChangesToTempQueue) {

        if (DirtyBlock->eWOState != ecWriteOrderStateData &&
                DirtyBlock->ulDataSource == INFLTDRV_DATA_SOURCE_DATA) {
            InsertTailList(&DevContext->ChangeList.DataCaptureModeHead, &ChangeNode->DataCaptureModeListEntry);
            ChangeNode->NumChangeNodeInDataWOSAhead = DevContext->NumChangeNodeInDataWOSCreated;
        }
    }
}

/**
 * AddDirtyBlockToDevContext - This is used to add a new dirty block with known time stamp
 * @param DevContext - Volumes context to which dirty block has to be added
 * @param ppTimeStamp - If this is NULL, function updates with pointer to generated timestamp
 * If not NULL, this is used as the first change timestamp. This points to memory
 *                      inside dirty block passed to function. DO NOT free the timestamp
 */
VOID
AddDirtyBlockToDevContext(
    PDEV_CONTEXT     DevContext,
    PKDIRTY_BLOCK       DirtyBlock,
    PTIME_STAMP_TAG_V2  *ppTimeStamp,
    PLARGE_INTEGER      pliTickCount,
    PDATA_BLOCK         *ppDataBlock
)
{
    ASSERT(NULL != DirtyBlock);
    ASSERT(NULL != DirtyBlock->ChangeNode);

    // Reference the node as we are going to add this node to list.
    PCHANGE_NODE    ChangeNode = ReferenceChangeNode(DirtyBlock->ChangeNode);

    // Set Data Source
    if (ecCaptureModeData == DevContext->eCaptureMode)
        DirtyBlock->ulDataSource = INFLTDRV_DATA_SOURCE_DATA;
    else
        DirtyBlock->ulDataSource = INFLTDRV_DATA_SOURCE_META_DATA;

    ASSERT((INFLTDRV_DATA_SOURCE_DATA == DirtyBlock->ulDataSource) || (DirtyBlock->eWOState != ecWriteOrderStateData));

    // Generate transaction ID
    DirtyBlock->uliTransactionID.QuadPart = DevContext->uliTransactionId.QuadPart++;
    if (0 == DirtyBlock->uliTransactionID.QuadPart)
        DirtyBlock->uliTransactionID.QuadPart = DevContext->uliTransactionId.QuadPart++;

    if (DevContext->bQueueChangesToTempQueue) {
        if (IsListEmpty(&DevContext->ChangeList.TempQueueHead)) {
            ASSERT(NULL == DevContext->ChangeList.CurrentNode);
            InsertTailList(&DevContext->ChangeList.TempQueueHead, &ChangeNode->ListEntry);
        } else {
            ASSERT(NULL != DevContext->ChangeList.CurrentNode);
            AddLastChangeTimestampToCurrentNode(DevContext,ppDataBlock);
            InsertTailList(&DevContext->ChangeList.TempQueueHead, &ChangeNode->ListEntry);
        }
    } else {
        if (IsListEmpty(&DevContext->ChangeList.Head)) {
            if (IsListEmpty(&DevContext->ChangeList.TempQueueHead)) {
                ASSERT(NULL == DevContext->ChangeList.CurrentNode);
                InsertTailList(&DevContext->ChangeList.Head, &ChangeNode->ListEntry);
            } else {
#ifdef VOLUME_CLUSTER_SUPPORT
                ASSERT(!IS_CLUSTER_VOLUME(DevContext));
#endif
                MoveUnupdatedDirtyBlocks(DevContext);
                InsertTailList(&DevContext->ChangeList.Head, &ChangeNode->ListEntry);
            }
        } else {
            ASSERT(NULL != DevContext->ChangeList.CurrentNode);
            AddLastChangeTimestampToCurrentNode(DevContext,ppDataBlock);
            InsertTailList(&DevContext->ChangeList.Head, &ChangeNode->ListEntry);
        }
    }

    if ((ppTimeStamp != NULL) && (*ppTimeStamp != NULL) && (pliTickCount != NULL) && (pliTickCount->QuadPart != 0)) {
        RtlCopyMemory(&DirtyBlock->TagTimeStampOfFirstChange, *ppTimeStamp, sizeof(TIME_STAMP_TAG_V2));
        DirtyBlock->liTickCountAtFirstIO.QuadPart = pliTickCount->QuadPart;
    } else {
        GenerateTimeStampTag(&DirtyBlock->TagTimeStampOfFirstChange);
        DirtyBlock->liTickCountAtFirstIO = KeQueryPerformanceCounter(NULL);
        if (ppTimeStamp) {
            *ppTimeStamp = &DirtyBlock->TagTimeStampOfFirstChange;
            ASSERT(NULL != pliTickCount);
        }

        if (pliTickCount) {
            pliTickCount->QuadPart = DirtyBlock->liTickCountAtFirstIO.QuadPart;
        }
    }

    DevContext->ChangeList.CurrentNode = ChangeNode;
    DevContext->ChangeList.lDirtyBlocksInQueue++;

    if (DirtyBlock->eWOState == ecWriteOrderStateData &&
            DirtyBlock->ulDataSource == INFLTDRV_DATA_SOURCE_DATA) {
                DevContext->NumChangeNodeInDataWOSCreated++;
    }    


    if(!DevContext->bQueueChangesToTempQueue) {
        if (DirtyBlock->eWOState != ecWriteOrderStateData &&
                DirtyBlock->ulDataSource == INFLTDRV_DATA_SOURCE_DATA) {
            InsertTailList(&DevContext->ChangeList.DataCaptureModeHead, &ChangeNode->DataCaptureModeListEntry);
            ChangeNode->NumChangeNodeInDataWOSAhead = DevContext->NumChangeNodeInDataWOSCreated;
        }
    }
}

VOID
AddLastChangeTimestampToCurrentNode(PDEV_CONTEXT DevContext, PDATA_BLOCK *ppDataBlock, bool bAllocate)
{
    PCHANGE_NODE    LastChangeNode = DevContext->ChangeList.CurrentNode;

    if (NULL == LastChangeNode) {
        if (DevContext->bResyncStartReceived == true) {
        ULONGLONG TimeStamp = 0;
        KeQuerySystemTime((PLARGE_INTEGER)&TimeStamp);

        // "LastChangeNode is NULL for disk %s. So can not close it. Current timestamp: %s, Last ResyncStartTS: %s."
        InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_FIRSTFILE_VALIDATION8,
            DevContext, TimeStamp, DevContext->ResynStartTimeStamp);
        }
        return;
    }

    if (ecChangeNodeDirtyBlock == LastChangeNode->eChangeNode) {
        PKDIRTY_BLOCK   LastDirtyBlock = LastChangeNode->DirtyBlock;
        ASSERT(NULL != LastDirtyBlock);

        if (0 == LastDirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601) {
            if (LastDirtyBlock->eWOState != ecWriteOrderStateData &&
                            LastDirtyBlock->ulDataSource == INFLTDRV_DATA_SOURCE_DATA) {
                ASSERT(LastChangeNode->DataCaptureModeListEntry.Flink != NULL &&
                        LastChangeNode->DataCaptureModeListEntry.Blink != NULL);
                GenerateTimeStampTag(&LastDirtyBlock->TagTimeStampOfFirstChange);                                
            }
                
            GenerateLastTimeStampTag(LastDirtyBlock);

            if (LastDirtyBlock->eWOState != ecWriteOrderStateData &&
                            LastDirtyBlock->ulDataSource == INFLTDRV_DATA_SOURCE_DATA) {
                ASSERT(LastChangeNode->DataCaptureModeListEntry.Flink != NULL &&
                        LastChangeNode->DataCaptureModeListEntry.Blink != NULL);                                
                ASSERT(LastDirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601 ==
                    LastDirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601);                                
                ASSERT(LastDirtyBlock->TagTimeStampOfFirstChange.ullSequenceNumber ==
                    LastDirtyBlock->TagTimeStampOfLastChange.ullSequenceNumber);
            }

            LastDirtyBlock->liTickCountAtLastIO = KeQueryPerformanceCounter(NULL);
            if ((INFLTDRV_DATA_SOURCE_DATA == LastDirtyBlock->ulDataSource) &&
                (LastDirtyBlock->ulcbDataFree)) 
            {
                ASSERT(NULL != LastDirtyBlock->CurrentDataBlock);
                ASSERT((LastDirtyBlock->ulcbDataFree > DriverContext.ulDataBlockSize) ||
                        (LastDirtyBlock->CurrentDataBlock->ulcbDataFree == LastDirtyBlock->ulcbDataFree));

                InVolDbgPrint(DL_TRACE_L1, DM_DATA_FILTERING | DM_TAGS,
                                ("AddLastChangeTimestampToCurrentNode: LastDataBlock contains data\n"));
                ULONG ulTrailerSize = SetSVDLastChangeTimeStamp(LastDirtyBlock);
                InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, ("AddLastChangeTimestampToCurrentNode: Have to skip the left over data VCReserved = %#x VCFree = %#x DBFree = %#x\n",
                                                            DevContext->ulcbDataReserved, DevContext->ulcbDataFree, 
                                                            LastDirtyBlock->ulcbDataFree));
                VCAdjustCountersForLostDataBytes(DevContext, 
                                                    ppDataBlock, 
                                                    LastDirtyBlock->ulcbDataFree + ulTrailerSize,
                                                    bAllocate);
                LastDirtyBlock->ulcbDataFree = 0;
            }
        }
    }else {
        if (DevContext->bResyncStartReceived == true)  {
            ULONGLONG TimeStamp = 0;
            KeQuerySystemTime((PLARGE_INTEGER)&TimeStamp);

            // "LastChangeNode for disk %1 is of type DataFile. So not closing it. Current timestamp: %s, Last ResyncStartTS: %s."
            InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_FIRSTFILE_VALIDATION7,
                    DevContext, TimeStamp, DevContext->ResynStartTimeStamp);
        }
    }   
}

VOID
PrependChangeNodeToDevContext(PDEV_CONTEXT DevContext, PCHANGE_NODE ChangeNode)
{
    PKDIRTY_BLOCK   DirtyBlock = NULL;

    ReferenceChangeNode(ChangeNode);

    InsertHeadList(&DevContext->ChangeList.Head, &ChangeNode->ListEntry);
    DevContext->ChangeList.lDirtyBlocksInQueue++;

    if (NULL == DevContext->ChangeList.CurrentNode) {
        DevContext->ChangeList.CurrentNode = ChangeNode;
    }

    if ((ecChangeNodeDataFile == ChangeNode->eChangeNode) || (ecChangeNodeDirtyBlock == ChangeNode->eChangeNode)) {
        DirtyBlock = ChangeNode->DirtyBlock;
        ASSERT(NULL != DirtyBlock);
        
        DevContext->ulChangesReverted += DirtyBlock->cChanges;
        DevContext->ulicbChangesReverted.QuadPart += DirtyBlock->ulicbChanges.QuadPart;
        DevContext->ulChangesPendingCommit -= DirtyBlock->cChanges;
        DevContext->ullcbChangesPendingCommit -= DirtyBlock->ulicbChanges.QuadPart;

        if ( (DevContext->bNotify) && (NULL != DevContext->DBNotifyEvent)) {
            ASSERT(DevContext->ChangeList.ullcbTotalChangesPending >= DevContext->ullcbChangesPendingCommit);
            if (DirtyBlockReadyForDrain(DevContext)) {
                DevContext->bNotify = false;
                DevContext->liTickCountAtNotifyEventSet = KeQueryPerformanceCounter(NULL);
                KeSetEvent(DevContext->DBNotifyEvent, EVENT_INCREMENT, FALSE);
            }
        }
    }
    if (ChangeNode->DirtyBlock != NULL && ChangeNode->DirtyBlock->eWOState == ecWriteOrderStateData &&
            ChangeNode->DirtyBlock->ulDataSource == INFLTDRV_DATA_SOURCE_DATA) {
                DevContext->NumChangeNodeInDataWOSCreated++;
    }    

    return;
}

VOID
DeallocateDevContext(PDEV_CONTEXT   pDevContext)
{
#ifdef VOLUME_FLT
    PDEVICE_ID    pDeviceID;

    pDeviceID = pDevContext->GUIDList;
    while (NULL != pDeviceID) {
        PDEVICE_ID pTemp = pDeviceID;
        pDeviceID = pTemp->NextGUID;
        ExFreePoolWithTag(pTemp, TAG_GENERIC_NON_PAGED);
    }
#endif
    if (NULL != pDevContext->CurrentHardwareIdName) {
        ExFreePoolWithTag(pDevContext->CurrentHardwareIdName, TAG_GENERIC_NON_PAGED);

    }
    if (NULL != pDevContext->PrevHardwareIdName) {
        ExFreePoolWithTag(pDevContext->PrevHardwareIdName, TAG_REGISTRY_DATA);
    }

    if (NULL != pDevContext->pDeviceDescriptor) {
        ExFreePoolWithTag(pDevContext->pDeviceDescriptor, TAG_DEVICE_INFO);
    }

    ExFreePoolWithTag(pDevContext, TAG_DEV_CONTEXT);
    InterlockedIncrement(&DriverContext.lCntDevContextFree);

    return;
}

PDEV_CONTEXT
ReferenceDevContext(PDEV_CONTEXT   pDevContext)
{
    ASSERT(pDevContext->lRefCount >= 1);

    InterlockedIncrement(&pDevContext->lRefCount);

    return pDevContext;
}

VOID
DereferenceDevContext(PDEV_CONTEXT   pDevContext)
{
    ASSERT(pDevContext->lRefCount > 0);
    ASSERT(NULL != pDevContext->pReSyncWorkQueueEntry);
    if (InterlockedDecrement(&pDevContext->lRefCount) <= 0)
    {
    	//Free WorkQueueEntry
    	ASSERT(0 == pDevContext->bReSyncWorkQueueRef);
        DereferenceWorkQueueEntry(pDevContext->pReSyncWorkQueueEntry);
        DeallocateDevContext(pDevContext);
    }

    return;
}

PCHANGE_NODE
GetDataModeChangeNode(PDEV_CONTEXT DevContext, bool bFromHead)
{
    PCHANGE_NODE ChangeNode = NULL;
    PCHANGE_NODE NewCurrentNode = NULL;
    PLIST_ENTRY ListEntry = NULL;
    PKDIRTY_BLOCK DirtyBlock = NULL;
    LONGLONG DiffCount = 0;
    
    if (!bFromHead) {
        return ChangeNode;
    }

    //
    // This is Draining code path and not Bitmap writing code path.
    //

    if (IsListEmpty(&DevContext->ChangeList.DataCaptureModeHead)) {
        return ChangeNode;
    }

    ASSERT(!IsListEmpty(&DevContext->ChangeList.Head));
    ListEntry = (PLIST_ENTRY) RemoveHeadList(&DevContext->ChangeList.DataCaptureModeHead);
    ChangeNode = (PCHANGE_NODE) CONTAINING_RECORD((PCHAR) ListEntry, CHANGE_NODE, DataCaptureModeListEntry);
    ASSERT(ChangeNode != NULL);

    DirtyBlock = ChangeNode->DirtyBlock;
    ASSERT(DirtyBlock != NULL);
    ASSERT(DirtyBlock->eWOState != ecWriteOrderStateData && DirtyBlock->ulDataSource == INFLTDRV_DATA_SOURCE_DATA);

    DiffCount = ChangeNode->NumChangeNodeInDataWOSAhead - DevContext->NumChangeNodeInDataWOSDrained;
    ASSERT(DiffCount >= 0);

    if (!IsListEmpty(&DevContext->ChangeList.Head)) {
        PCHANGE_NODE FirstChangeNodeInMainQ = (PCHANGE_NODE) DevContext->ChangeList.Head.Flink;
        ASSERT(FirstChangeNodeInMainQ != NULL);
        
        PKDIRTY_BLOCK FirstDBInMainQ = FirstChangeNodeInMainQ->DirtyBlock;
        ASSERT(FirstDBInMainQ != NULL);

        if (FirstDBInMainQ->eWOState == ecWriteOrderStateData &&
                FirstDBInMainQ->ulDataSource == INFLTDRV_DATA_SOURCE_DATA) {
                    // There are DBs in Data WOS in Main Q ahead of this DB. We must allow all Data WOS
                    // DBs to drain and only after that drain DBs from list DataCaptureModeHead.
                    // Lets add ChangeNode back to list DataCaptureModeHead and drain it later.
                    ASSERT(DiffCount > 0);
                    InsertHeadList(&DevContext->ChangeList.DataCaptureModeHead, &ChangeNode->DataCaptureModeListEntry);
                    ChangeNode = NULL;
                    return ChangeNode;
        } else {
                    ASSERT(DiffCount == 0);
        }
    }
    
    if (0 == DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601) {
        // Lets not process ChangeNode that is not yet closed.
        // This essentially means this is the last ChangeNode in list DataCaptureModeHead.
        // This changenode can not be in "data file" mode at this point as it is not closed.
        ASSERT(IsListEmpty(&DevContext->ChangeList.DataCaptureModeHead));
        // Lets add it back to list DataCaptureModeHead.
        InsertTailList(&DevContext->ChangeList.DataCaptureModeHead, &ChangeNode->DataCaptureModeListEntry);
        ChangeNode = NULL;
        return ChangeNode;
    }

    if (ChangeNode == DevContext->ChangeList.CurrentNode) {
        ASSERT(!IsListEmpty(&DevContext->ChangeList.Head));
    }

    if (!IsListEmpty(&DevContext->ChangeList.Head)) {
        if (ChangeNode == DevContext->ChangeList.CurrentNode) {
            if (ChangeNode->ListEntry.Blink != &DevContext->ChangeList.Head) {
                ASSERT(ChangeNode->ListEntry.Flink == &DevContext->ChangeList.Head);
                NewCurrentNode = (PCHANGE_NODE) ChangeNode->ListEntry.Blink;
                ASSERT(NewCurrentNode->DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601 != 0);
            }
        }
        RemoveEntryList(&ChangeNode->ListEntry);
        if (ChangeNode == DevContext->ChangeList.CurrentNode) {
            ASSERT(IsListEmpty(&DevContext->ChangeList.Head) || NewCurrentNode != NULL);
        }
        if (IsListEmpty(&DevContext->ChangeList.Head)) {
            ASSERT(NewCurrentNode == NULL);
            ASSERT(ChangeNode == DevContext->ChangeList.CurrentNode);
            DevContext->ChangeList.CurrentNode = NULL;
            ASSERT(IsListEmpty(&DevContext->ChangeList.DataCaptureModeHead));
        }
        if (NewCurrentNode != NULL) {
            ASSERT(!IsListEmpty(&DevContext->ChangeList.Head));
            ASSERT(ChangeNode == DevContext->ChangeList.CurrentNode);
            DevContext->ChangeList.CurrentNode = NewCurrentNode;
        }
    }

    if (IsListEmpty(&DevContext->ChangeList.Head)) {
        ASSERT(DevContext->ChangeList.CurrentNode == NULL);
        ASSERT(IsListEmpty(&DevContext->ChangeList.DataCaptureModeHead));
    }

    if (DevContext->ChangeList.CurrentNode == NULL) {
        ASSERT(IsListEmpty(&DevContext->ChangeList.Head));
        ASSERT(IsListEmpty(&DevContext->ChangeList.DataCaptureModeHead));
    }
    
    DevContext->ChangeList.lDirtyBlocksInQueue--;
    if (DevContext->ChangeList.lDirtyBlocksInQueue == 0) {
        ASSERT(IsListEmpty(&DevContext->ChangeList.Head));
        ASSERT(IsListEmpty(&DevContext->ChangeList.DataCaptureModeHead));
        ASSERT(DevContext->ChangeList.CurrentNode == NULL);
    }
    if (ecChangeNodeDirtyBlock == ChangeNode->eChangeNode) {
        ASSERT(NULL != DirtyBlock);
        ASSERT((NULL != DevContext->ChangeList.CurrentNode) || (NULL != DevContext->pDBPendingTransactionConfirm) ||
               (DirtyBlock->cChanges == DevContext->ChangeList.ulTotalChangesPending));
        ASSERT((NULL != DevContext->ChangeList.CurrentNode) || (NULL != DevContext->pDBPendingTransactionConfirm) ||
               (DirtyBlock->ulicbChanges.QuadPart == DevContext->ChangeList.ullcbTotalChangesPending));
    }

    ChangeNode->DataCaptureModeListEntry.Flink = NULL;
    ChangeNode->DataCaptureModeListEntry.Blink = NULL;    

    return ChangeNode;
}

VOID
GenerateNewSequenceNumberAndTimeStamp(PDEV_CONTEXT DevContext, PCHANGE_NODE ChangeNode, bool bFromHead)
{
    PKDIRTY_BLOCK DirtyBlock = NULL;
    PLIST_ENTRY ListEntry = NULL;
    PCHANGE_NODE TmpChangeNode = NULL;
    PKDIRTY_BLOCK TmpDirtyBlock = NULL;
    
    if (ChangeNode == NULL || !bFromHead) {
        return;
    }

    //
    // This is Draining code path and not Bitmap writing code path.
    //

    DirtyBlock = ChangeNode->DirtyBlock;
    ASSERT(DirtyBlock != NULL);

    // This DB is selected for draining from Main Q.
    // DB could be DW, DM, MD => need to change TS/Seq# only for MD.
    if (DirtyBlock->ulDataSource == INFLTDRV_DATA_SOURCE_DATA) {
        return;
    }

    ASSERT(DirtyBlock->eWOState != ecWriteOrderStateData);

    // No pending DW's at this point as all before this would have been drained and
    // there wont be any after this in the list as only when all MDs are drained we could
    // track data using DW. However, DMs could be there. Since in new logic we drain
    // DMs ahead of MDs, there could be just one pending DM and that too in open
    // state (being filled) when this MD is being drained.

    // Validate that there are no pending DWs.
    ASSERT(DevContext->NumChangeNodeInDataWOSDrained == DevContext->NumChangeNodeInDataWOSCreated);

    // DataCaptureModeHead List must either be empty or contain
    // just one DM type of DB which is still open (not yet closed).
    // Validate this.
    if (!IsListEmpty(&DevContext->ChangeList.DataCaptureModeHead)) {
        ASSERT(!IsListEmpty(&DevContext->ChangeList.Head));
        ListEntry = (PLIST_ENTRY) RemoveHeadList(&DevContext->ChangeList.DataCaptureModeHead);
        TmpChangeNode = (PCHANGE_NODE) CONTAINING_RECORD((PCHAR) ListEntry, CHANGE_NODE, DataCaptureModeListEntry);
        ASSERT(TmpChangeNode != NULL);
        TmpDirtyBlock = TmpChangeNode->DirtyBlock;
        ASSERT(TmpDirtyBlock != NULL);

        ASSERT(TmpDirtyBlock->eWOState != ecWriteOrderStateData && TmpDirtyBlock->ulDataSource == INFLTDRV_DATA_SOURCE_DATA);
        ASSERT(0 == TmpDirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601);
        ASSERT(ecChangeNodeDirtyBlock == TmpChangeNode->eChangeNode);
        ASSERT(IsListEmpty(&DevContext->ChangeList.DataCaptureModeHead));
        InsertTailList(&DevContext->ChangeList.DataCaptureModeHead, &TmpChangeNode->DataCaptureModeListEntry);
    }

    // Generate TS and Seq# for this MD. This MD DB is already closed.
    ASSERT(ecChangeNodeDirtyBlock == ChangeNode->eChangeNode);
    
    GenerateTimeStampTag(&DirtyBlock->TagTimeStampOfFirstChange);

    GenerateLastTimeStampTag(DirtyBlock);
    ASSERT(DirtyBlock->TagTimeStampOfFirstChange.ullSequenceNumber ==
        DirtyBlock->TagTimeStampOfLastChange.ullSequenceNumber);
    ASSERT(DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601 ==
        DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601);

    // DirtyBlock->liTickCountAtFirstIO = KeQueryPerformanceCounter(NULL);
    // DirtyBlock->liTickCountAtLastIO.Quadpart = DirtyBlock->liTickCountAtFirstIO.Quadpart;

    // Convert split-IOs into independent IOs.
    if (DirtyBlock->ulFlags & KDIRTY_BLOCK_FLAG_SPLIT_CHANGE_MASK) {
        ASSERT(DirtyBlock->ulSequenceIdForSplitIO >= 1);
        DirtyBlock->ulSequenceIdForSplitIO = 1;
        if (DirtyBlock->ulFlags & KDIRTY_BLOCK_FLAG_START_OF_SPLIT_CHANGE) {
            DirtyBlock->ulFlags &= ~KDIRTY_BLOCK_FLAG_START_OF_SPLIT_CHANGE;
        }
        if (DirtyBlock->ulFlags & KDIRTY_BLOCK_FLAG_PART_OF_SPLIT_CHANGE) {
            DirtyBlock->ulFlags &= ~KDIRTY_BLOCK_FLAG_PART_OF_SPLIT_CHANGE;
        }
        if (DirtyBlock->ulFlags & KDIRTY_BLOCK_FLAG_END_OF_SPLIT_CHANGE) {
            DirtyBlock->ulFlags &= ~KDIRTY_BLOCK_FLAG_END_OF_SPLIT_CHANGE;
        }
    } else {
        ASSERT(DirtyBlock->ulSequenceIdForSplitIO == 1);
    }

    return;
}

PCHANGE_NODE
GetChangeNodeFromChangeList(PDEV_CONTEXT DevContext, bool bFromHead)
{
    PCHANGE_NODE    ChangeNode = NULL;

    ChangeNode = GetDataModeChangeNode(DevContext, bFromHead);
    if (ChangeNode != NULL) {
        return ChangeNode;
    }

    if (!IsListEmpty(&DevContext->ChangeList.Head)) {
        if (bFromHead) {
            ChangeNode = (PCHANGE_NODE)RemoveHeadList(&DevContext->ChangeList.Head);
            if (ChangeNode == DevContext->ChangeList.CurrentNode) {
                ASSERT(IsListEmpty(&DevContext->ChangeList.Head));

                DevContext->ChangeList.CurrentNode = NULL;
            }
        } else {
            PCHANGE_NODE    CurrentNode = DevContext->ChangeList.CurrentNode;
            if (CurrentNode->ListEntry.Blink != &DevContext->ChangeList.Head) {
                ChangeNode = (PCHANGE_NODE)CurrentNode->ListEntry.Blink;
                RemoveEntryList(&ChangeNode->ListEntry);
            } else {
                ChangeNode = (PCHANGE_NODE)RemoveTailList(&DevContext->ChangeList.Head);
                ASSERT(IsListEmpty(&DevContext->ChangeList.Head));

                DevContext->ChangeList.CurrentNode = NULL;
            }
        }
        DevContext->ChangeList.lDirtyBlocksInQueue--;
        if (ChangeNode != NULL && ChangeNode->DirtyBlock->eWOState == ecWriteOrderStateData &&
                ChangeNode->DirtyBlock->ulDataSource == INFLTDRV_DATA_SOURCE_DATA) {
                    // ChangeNode will be either drained or written to bitmap.
                    DevContext->NumChangeNodeInDataWOSDrained++;
        }        
        if (ecChangeNodeDirtyBlock == ChangeNode->eChangeNode) {
            PKDIRTY_BLOCK   DirtyBlock = ChangeNode->DirtyBlock;

            ASSERT(NULL != DirtyBlock);
            ASSERT((NULL != DevContext->ChangeList.CurrentNode) || (NULL != DevContext->pDBPendingTransactionConfirm) ||
                   (DirtyBlock->cChanges == DevContext->ChangeList.ulTotalChangesPending));
            ASSERT((NULL != DevContext->ChangeList.CurrentNode) || (NULL != DevContext->pDBPendingTransactionConfirm) ||
                   (DirtyBlock->ulicbChanges.QuadPart == DevContext->ChangeList.ullcbTotalChangesPending));

            if (0 == DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601) {
                if (DirtyBlock->eWOState != ecWriteOrderStateData &&
                                DirtyBlock->ulDataSource == INFLTDRV_DATA_SOURCE_DATA) {
                    ASSERT(ChangeNode->DataCaptureModeListEntry.Flink != NULL &&
                            ChangeNode->DataCaptureModeListEntry.Blink != NULL);
                    GenerateTimeStampTag(&DirtyBlock->TagTimeStampOfFirstChange);
                }

                GenerateLastTimeStampTag(DirtyBlock);
                DirtyBlock->liTickCountAtLastIO = KeQueryPerformanceCounter(NULL);

                if (DirtyBlock->eWOState != ecWriteOrderStateData &&
                                DirtyBlock->ulDataSource == INFLTDRV_DATA_SOURCE_DATA) {
                    ASSERT(ChangeNode->DataCaptureModeListEntry.Flink != NULL &&
                            ChangeNode->DataCaptureModeListEntry.Blink != NULL);
                    ASSERT(DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601 ==
                        DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601);
                    ASSERT(DirtyBlock->TagTimeStampOfFirstChange.ullSequenceNumber ==
                        DirtyBlock->TagTimeStampOfLastChange.ullSequenceNumber);
                }
            }
        }
        
        if (ChangeNode->DataCaptureModeListEntry.Flink != NULL &&
            ChangeNode->DataCaptureModeListEntry.Blink != NULL) {
#if DBG
                PKDIRTY_BLOCK   DirtyBlock = ChangeNode->DirtyBlock;
                ASSERT(DirtyBlock->eWOState != ecWriteOrderStateData && DirtyBlock->ulDataSource == INFLTDRV_DATA_SOURCE_DATA);
                ASSERT(!IsListEmpty(&DevContext->ChangeList.DataCaptureModeHead));
#endif
                RemoveEntryList(&ChangeNode->DataCaptureModeListEntry);
                ChangeNode->DataCaptureModeListEntry.Flink = NULL;
                ChangeNode->DataCaptureModeListEntry.Blink = NULL;
                if (IsListEmpty(&DevContext->ChangeList.Head)) {
                    ASSERT(IsListEmpty(&DevContext->ChangeList.DataCaptureModeHead));
                    ASSERT(DevContext->ChangeList.CurrentNode == NULL);
                }
        }
        if (DevContext->ChangeList.lDirtyBlocksInQueue == 0) {
            ASSERT(IsListEmpty(&DevContext->ChangeList.Head));
            ASSERT(IsListEmpty(&DevContext->ChangeList.DataCaptureModeHead));
            ASSERT(DevContext->ChangeList.CurrentNode == NULL);
        }        
    } else {
        ASSERT(DevContext->ChangeList.CurrentNode == NULL);
        ASSERT(IsListEmpty(&DevContext->ChangeList.DataCaptureModeHead));
    }

    GenerateNewSequenceNumberAndTimeStamp(DevContext, ChangeNode, bFromHead);

    // Do not dereference change node here. Caller would dereference this
    // Count incremented at the time of adding to list is compensated by
    // callers dereference.
    return ChangeNode;
}

PKDIRTY_BLOCK
GetCurrentDirtyBlockFromChangeList(PDEV_CONTEXT DevContext)
{
    PCHANGE_NODE    ChangeNode = DevContext->ChangeList.CurrentNode;
    PKDIRTY_BLOCK   DirtyBlock = NULL;
    if ((NULL != ChangeNode) && (ecChangeNodeDirtyBlock == ChangeNode->eChangeNode)) {
        DirtyBlock = ChangeNode->DirtyBlock;
    }

    return DirtyBlock;
}

/*
Description : This routine converts the change nodes from Main list and write it to a data file

Parameters  : DevContext - Pointer to Device context

Return      : Pointer to a change node

History     :
19/06/15 - Support for keeping tags in stream format is deprecated and they are placed as part of KDB and close it immediately.
           Don't convert such DirtyBlock as a data file
*/
PCHANGE_NODE
GetChangeNodeToSaveAsFile(PDEV_CONTEXT DevContext)
{
    PCHANGE_NODE    ChangeNode = (PCHANGE_NODE)DevContext->ChangeList.Head.Blink;

    ASSERT((NULL != ChangeNode) && ((PLIST_ENTRY)ChangeNode != &DevContext->ChangeList.Head));
    // Never save the last change node.
    ChangeNode = (PCHANGE_NODE)ChangeNode->ListEntry.Blink;
    while ((PLIST_ENTRY)ChangeNode != &DevContext->ChangeList.Head) {
        if ((ecChangeNodeDirtyBlock == ChangeNode->eChangeNode) && 
            (INFLTDRV_DATA_SOURCE_DATA == ChangeNode->DirtyBlock->ulDataSource) &&
            (0 == (ChangeNode->ulFlags & (CHANGE_NODE_FLAGS_QUEUED_FOR_DATA_WRITE | CHANGE_NODE_FLAGS_ERROR_IN_DATA_WRITE))) &&
            ((!TEST_FLAG(ChangeNode->DirtyBlock->ulFlags, KDIRTY_BLOCK_FLAG_CONTAINS_TAGS) && (!IsListEmpty(&ChangeNode->DirtyBlock->DataBlockList)))))
        {
            ASSERT((!TEST_FLAG(ChangeNode->DirtyBlock->ulFlags, KDIRTY_BLOCK_FLAG_CONTAINS_TAGS) && (!IsListEmpty(&ChangeNode->DirtyBlock->DataBlockList))));
            ChangeNode->ulFlags |= CHANGE_NODE_FLAGS_QUEUED_FOR_DATA_WRITE;
            DevContext->lDataBlocksQueuedToFileWrite += ChangeNode->DirtyBlock->ulDataBlocksAllocated;
            ASSERT(DevContext->lDataBlocksQueuedToFileWrite <= DevContext->lDataBlocksInUse);
            return ReferenceChangeNode(ChangeNode);
        }

        ChangeNode = (PCHANGE_NODE)ChangeNode->ListEntry.Blink;
    }

    InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, ("%s: Failed to find change node to write to file\n", __FUNCTION__));
    return NULL;
}

/*History:
19/06/ 15 - Support for keeping tags in stream format is deprecated and they are placed as part of KDB and close it immediately.
            Don't convert such DirtyBlock as a data file
*/
VOID
QueueChangeNodesToSaveAsFile(PDEV_CONTEXT DevContext, bool bLockAcquired)
{
    PCHANGE_NODE    ChangeNode = NULL;
    NTSTATUS        Status = STATUS_SUCCESS;
    KIRQL           OldIrql = 0;

    if (!bLockAcquired)
        KeAcquireSpinLock(&DevContext->Lock, &OldIrql);

    ChangeNode = (PCHANGE_NODE)DevContext->ChangeList.Head.Blink;;
    ASSERT((NULL != ChangeNode) && ((PLIST_ENTRY)ChangeNode != &DevContext->ChangeList.Head));
    // Never save the last change node.
    ChangeNode = (PCHANGE_NODE)ChangeNode->ListEntry.Blink;
    while ((PLIST_ENTRY)ChangeNode != &DevContext->ChangeList.Head) {
        if ((ecChangeNodeDirtyBlock == ChangeNode->eChangeNode) && 
            (INFLTDRV_DATA_SOURCE_DATA == ChangeNode->DirtyBlock->ulDataSource) &&
            (0 == (ChangeNode->ulFlags & (CHANGE_NODE_FLAGS_QUEUED_FOR_DATA_WRITE | CHANGE_NODE_FLAGS_ERROR_IN_DATA_WRITE)))&&
            ((!TEST_FLAG(ChangeNode->DirtyBlock->ulFlags, KDIRTY_BLOCK_FLAG_CONTAINS_TAGS) && (!IsListEmpty(&ChangeNode->DirtyBlock->DataBlockList)))))
        {
            ASSERT((!TEST_FLAG(ChangeNode->DirtyBlock->ulFlags, KDIRTY_BLOCK_FLAG_CONTAINS_TAGS) && (!IsListEmpty(&ChangeNode->DirtyBlock->DataBlockList))));
            ChangeNode->ulFlags |= CHANGE_NODE_FLAGS_QUEUED_FOR_DATA_WRITE;
            DevContext->lDataBlocksQueuedToFileWrite += ChangeNode->DirtyBlock->ulDataBlocksAllocated;
            ASSERT(DevContext->lDataBlocksQueuedToFileWrite <= DevContext->lDataBlocksInUse);
            ReferenceChangeNode(ChangeNode);
            Status = DriverContext.DataFileWriterManager->AddWorkItem(DevContext->DevFileWriter, ChangeNode, DevContext);
            ASSERT(STATUS_SUCCESS == Status);
            DereferenceChangeNode(ChangeNode);

        }

        ChangeNode = (PCHANGE_NODE)ChangeNode->ListEntry.Blink;
    }

    if (!bLockAcquired)
        KeReleaseSpinLock(&DevContext->Lock, OldIrql);

    InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, ("%s: Failed to find change node to write to file\n", __FUNCTION__));
   
}
ULONG
CurrentDataSource(PDEV_CONTEXT DevContext)
{
    switch(DevContext->eCaptureMode) {
    case ecCaptureModeData:
        return INFLTDRV_DATA_SOURCE_DATA;
    case ecCaptureModeMetaData:
    default:
        return INFLTDRV_DATA_SOURCE_META_DATA;
    }
}

//This function is called with volume context lock held
void
MoveUnupdatedDirtyBlocks(PDEV_CONTEXT pDevContext) {
    LIST_ENTRY   *pDestinationList = &pDevContext->ChangeList.Head;
    LIST_ENTRY  *pSourceList = &pDevContext->ChangeList.TempQueueHead;
    LIST_ENTRY  *CurrentEntry = pSourceList->Flink;

    //Update the time stamp of the SouceList
    while(CurrentEntry != pSourceList) {
        PKDIRTY_BLOCK DirtyBlock;
        PCHANGE_NODE ChangeNode = (PCHANGE_NODE)CurrentEntry;//Extract in FIFO order
        DirtyBlock = ChangeNode->DirtyBlock;

        GenerateTimeStampForUnupdatedDB( DirtyBlock);
        CurrentEntry = CurrentEntry->Flink;
    }

    while(!IsListEmpty(pSourceList)) {
        InsertHeadList(pDestinationList, RemoveTailList(pSourceList));//Extract in LIFO order and insert from the head in the main list
    	}

    pDevContext->bQueueChangesToTempQueue = false;
    InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("%s: Added the Dirty blocks from temp queue to main queue\n", __FUNCTION__));
}

NTSTATUS
GetDeviceContextFieldsFromRegistry(PDEV_CONTEXT pDevContext)
{
    NTSTATUS        Status = STATUS_SUCCESS;
    ULONG           ulcbRead = 0;
    CHAR            ShutdownMarker = uninitialized;
    Registry        *pDevParam = NULL;

    pDevParam = new Registry();
    if (NULL == pDevParam)
        return STATUS_INSUFFICIENT_RESOURCES;

    Status = pDevParam->OpenKeyReadWrite(&pDevContext->DevParameters, TRUE);
    if (NT_SUCCESS(Status)) {
        ULONG           NumberOfInfo = 4;
        const unsigned long SizeOfBuffer = sizeof(PERSISTENT_INFO) + ( 3 * sizeof(PERSISTENT_VALUE));

        CHAR            LocalBuffer[SizeOfBuffer];
        PPERSISTENT_INFO  pPersistentInfo, pDefaultInfo;

        pPersistentInfo = (PPERSISTENT_INFO)LocalBuffer;
        pDefaultInfo = (PPERSISTENT_INFO)LocalBuffer;

        //Filling in the default values
        pDefaultInfo->NumberOfInfo = NumberOfInfo;
        pDefaultInfo->ShutdownMarker = DirtyShutdown;
        RtlZeroMemory(pDefaultInfo->Reserved, sizeof(pDefaultInfo->Reserved));

        pDefaultInfo->PersistentValues[0].PersistentValueType = PREV_END_TIMESTAMP;
        pDefaultInfo->PersistentValues[0].Reserved = 0;
        pDefaultInfo->PersistentValues[0].Data = 0; 

        pDefaultInfo->PersistentValues[1].PersistentValueType = PREV_END_SEQUENCENUMBER;
        pDefaultInfo->PersistentValues[1].Reserved = 0;
        pDefaultInfo->PersistentValues[1].Data = 0;

        pDefaultInfo->PersistentValues[2].PersistentValueType = PREV_SEQUENCEID;
        pDefaultInfo->PersistentValues[2].Reserved = 0;
        pDefaultInfo->PersistentValues[2].Data = 0;

        pDefaultInfo->PersistentValues[3].PersistentValueType = SOURCE_TARGET_LAG;
        pDefaultInfo->PersistentValues[3].Reserved = 0;
        pDefaultInfo->PersistentValues[3].Data = 0;

        //Reading the registry
        Status = pDevParam->ReadBinary(DC_LAST_PERSISTENT_VALUES, 
                                            STRING_LEN_NULL_TERMINATED, 
                                            (PVOID *)&pPersistentInfo,
                                            SizeOfBuffer,
                                            &ulcbRead,
                                            (PVOID)pDefaultInfo,
                                            SizeOfBuffer,
                                            TRUE);//sending bCreateIfFail as TRUE so as to create the key if not found
        if (NT_SUCCESS(Status)) {
            ASSERT(SizeOfBuffer == ulcbRead);
            if (STATUS_INMAGE_OBJECT_CREATED == Status) {//New Registry has been created
                InVolDbgPrint(DL_INFO, DM_REGISTRY, 
                    ("%s: Created %S with Default PrevEndTimeStamp %#I64x, ",__FUNCTION__,DC_LAST_PERSISTENT_VALUES, 
                     pDefaultInfo->PersistentValues[0].Data));
                InVolDbgPrint(DL_INFO, DM_REGISTRY, 
                    ("PrevEndSequenceNumber %#I64x\n, PrevSeqID %#x for Volume %S\n",pDefaultInfo->PersistentValues[1].Data,
                     pDefaultInfo->PersistentValues[2].Data,
                     pDevContext->wDevID));
            } else {//Registry found. Values has been read.
                ShutdownMarker = pPersistentInfo->ShutdownMarker;
                pDevContext->ucShutdownMarker = ShutdownMarker;
                InVolDbgPrint(DL_INFO, DM_REGISTRY, 
                    ("%s: Read Registry %S Shutdown Marker  = %d\n",__FUNCTION__,DC_LAST_PERSISTENT_VALUES, pPersistentInfo->ShutdownMarker));

                for (ULONG i = 0; i < pPersistentInfo->NumberOfInfo; i++) {
                    switch (pPersistentInfo->PersistentValues[i].PersistentValueType) {
                        case PREV_END_TIMESTAMP:
                            if (ShutdownMarker == DirtyShutdown)//means that the last shutdown was dirty.
                                pDevContext->LastCommittedTimeStamp = MAX_PREV_TIMESTAMP;
                            else//Means that the last shutdown was clean.
                                pDevContext->LastCommittedTimeStamp = pPersistentInfo->PersistentValues[i].Data;
                            InVolDbgPrint(DL_INFO, DM_REGISTRY, 
                                ("%s: Read Registry %S PrevEndTimeStamp = %#I64x for Volume %S\n",__FUNCTION__,
                                       DC_LAST_PERSISTENT_VALUES, pDevContext->LastCommittedTimeStamp, 
                                       pDevContext->wDevID));
                            pDevContext->LastCommittedTimeStampReadFromStore = pDevContext->LastCommittedTimeStamp;
                            break;
                        case PREV_END_SEQUENCENUMBER:
                            if (ShutdownMarker == DirtyShutdown)//Means that the last shutdown was dirty.
                                pDevContext->LastCommittedSequenceNumber = MAX_PREV_SEQNUMBER;
                            else//Means the last shutdown was clean.
                                pDevContext->LastCommittedSequenceNumber = pPersistentInfo->PersistentValues[i].Data;

                            pDevContext->LastCommittedSequenceNumberReadFromStore = (ULONGLONG)pDevContext->LastCommittedSequenceNumber;
                            InVolDbgPrint(DL_INFO, DM_REGISTRY, 
                                ("%s: Read Registry %S PrevEndSequenceNumber = %#I64x for Volume %S\n",__FUNCTION__,
                                       DC_LAST_PERSISTENT_VALUES, pDevContext->LastCommittedSequenceNumber, 
                                       pDevContext->wDevID));
                            break;
                        case  PREV_SEQUENCEID:
                            if (ShutdownMarker == DirtyShutdown)//means that the last shutdown was dirty.
                                pDevContext->LastCommittedSplitIoSeqId = MAX_SEQ_ID;
                            else//Means that the last shutdown was clean.
                                pDevContext->LastCommittedSplitIoSeqId = (ULONG)pPersistentInfo->PersistentValues[i].Data;
                            InVolDbgPrint(DL_INFO, DM_REGISTRY, 
                                ("%s: Read Registry %S PrevSeqId = %#I64x for Volume %S\n",__FUNCTION__,
                                       DC_LAST_PERSISTENT_VALUES, pDevContext->LastCommittedSplitIoSeqId, 
                                       pDevContext->wDevID));
                            pDevContext->LastCommittedSplitIoSeqIdReadFromStore = pDevContext->LastCommittedSplitIoSeqId;
                            break;
                        case  SOURCE_TARGET_LAG:
                            if (ShutdownMarker == DirtyShutdown)//means that the last shutdown was dirty.
                                pDevContext->EndTimeStampofDB = 0;
                            else//Means that the last shutdown was clean.
                                pDevContext->EndTimeStampofDB = (ULONGLONG)pPersistentInfo->PersistentValues[i].Data;                                
                            InVolDbgPrint(DL_INFO, DM_REGISTRY, 
                                ("%s: Read Registry %S EndTimeStampofDB = %#I64x for Volume %S\n",__FUNCTION__,
                                       DC_LAST_PERSISTENT_VALUES, pDevContext->EndTimeStampofDB, 
                                       pDevContext->wDevID));
                            break;

                        default :
                            InVolDbgPrint(DL_INFO, DM_REGISTRY, 
                                ("%s: Read Registry for %S is corrupted.\n", __FUNCTION__, DC_LAST_PERSISTENT_VALUES));
                            break;
                    }
                }
                // Mark DirtyShutdown to differentiate whether next shutdown is Clean or Dirty
                pPersistentInfo->ShutdownMarker = DirtyShutdown;
                Status = pDevParam->WriteBinary(DC_LAST_PERSISTENT_VALUES, pPersistentInfo, SizeOfBuffer);
                if (STATUS_SUCCESS != Status) {
                    InVolDbgPrint(DL_ERROR, DM_REGISTRY,
                        ("%s: Failed to Set DirtyShutdown after Persistent Values read for Volume GUID(%S)\n",
                        __FUNCTION__, pDevContext->wDevID));
                } else {
                    Status = pDevParam->FlushKey();
                    if (STATUS_SUCCESS != Status) {
                        InVolDbgPrint(DL_ERROR, DM_REGISTRY, 
                            ("%s: Flushing the Registry Info to Disk Failed for Volume : %S\n", __FUNCTION__,
                             pDevContext->wDevNum));
                    } else {
                        InVolDbgPrint(DL_ERROR, DM_REGISTRY,
                            ("%s: Flushing the DevContext Registry Info to Disk Succeeded for Volume GUID(%S)\n", __FUNCTION__,
                             pDevContext->wDevID));
                    }
                }
            }
        } else {
            InVolDbgPrint(DL_INFO, DM_REGISTRY, 
                ("%s: ReadBinary for %S has failed for Volume %S\n", __FUNCTION__, DC_LAST_PERSISTENT_VALUES, pDevContext->wDevID));
        }
    }
    return Status;
}

NTSTATUS PersistFDContextFields(PDEV_CONTEXT pDevContext, LONG *OutOfSyncErrorCode)
{
    NTSTATUS     Status;
    KIRQL        OldIrql;
    Registry    *pDevParam = NULL;

    pDevParam = new Registry();
    if (NULL == pDevParam)
        return STATUS_INSUFFICIENT_RESOURCES;

    Status = pDevParam->OpenKeyReadWrite(&pDevContext->DevParameters, TRUE);
    if (NT_SUCCESS(Status)) {
        PPERSISTENT_INFO pPersistentInfo;
        ULONGLONG PrevEndTimeStamp = 0;
        ULONGLONG PrevEndSeqNumber = 0;
        ULONGLONG EndTimeStampofDB = 0;
        ULONG PrevSequenceId = 0;

        ULONG NumberOfInfo = 4;
        const unsigned long SizeOfBuffer = sizeof(PERSISTENT_INFO) + (3 * sizeof(PERSISTENT_VALUE));
        CHAR Buffer[SizeOfBuffer];

        KeWaitForSingleObject(&pDevContext->Mutex, Executive, KernelMode, FALSE, NULL);
        KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);

        pDevContext->ulFlags |= DCF_FIELDS_PERSISTED;

        PrevEndTimeStamp = pDevContext->LastCommittedTimeStamp;
        PrevEndSeqNumber = pDevContext->LastCommittedSequenceNumber;
        PrevSequenceId = pDevContext->LastCommittedSplitIoSeqId;
        EndTimeStampofDB = pDevContext->EndTimeStampofDB;
        KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
        
        pPersistentInfo = (PPERSISTENT_INFO)Buffer;
        RtlZeroMemory(pPersistentInfo, SizeOfBuffer);
        pPersistentInfo->NumberOfInfo = NumberOfInfo;
        pPersistentInfo->ShutdownMarker = CleanShutdown;

        pPersistentInfo->PersistentValues[0].PersistentValueType = PREV_END_TIMESTAMP;
        pPersistentInfo->PersistentValues[0].Reserved = 0;
        pPersistentInfo->PersistentValues[0].Data = PrevEndTimeStamp;

        pPersistentInfo->PersistentValues[1].PersistentValueType = PREV_END_SEQUENCENUMBER;
        pPersistentInfo->PersistentValues[1].Reserved = 0;
        pPersistentInfo->PersistentValues[1].Data = PrevEndSeqNumber;

        pPersistentInfo->PersistentValues[2].PersistentValueType = PREV_SEQUENCEID;
        pPersistentInfo->PersistentValues[2].Reserved = 0;
        pPersistentInfo->PersistentValues[2].Data = PrevSequenceId;

        pPersistentInfo->PersistentValues[3].PersistentValueType = SOURCE_TARGET_LAG;
        pPersistentInfo->PersistentValues[3].Reserved = 0;
        pPersistentInfo->PersistentValues[3].Data = EndTimeStampofDB;

        Status = pDevParam->WriteBinary(DC_LAST_PERSISTENT_VALUES, pPersistentInfo, SizeOfBuffer);
        if (STATUS_SUCCESS != Status) {
            InVolDbgPrint(DL_ERROR, DM_REGISTRY,
                ("%s: Failed to Write Persistent Values for Volume (%S)\n", __FUNCTION__,
                 pDevContext->wDevNum));
            *OutOfSyncErrorCode = MSG_INDSKFLT_ERROR_REGISTRY_FLUSH_DEVCONTEXT_FIELDS_Message;
        } else {
            Status = pDevParam->FlushKey();
            if (STATUS_SUCCESS != Status) {
                InVolDbgPrint(DL_ERROR, DM_REGISTRY, 
                    ("%s: Flushing the Registry Info to Disk Failed for Volume : %S\n", __FUNCTION__,
                     pDevContext->wDevNum));
                *OutOfSyncErrorCode = MSG_INDSKFLT_ERROR_PRESHUTDOWN_BITMAP_FLUSH_FAILURE_Message;
            } else {
                InVolDbgPrint(DL_INFO, DM_REGISTRY, 
                   ("Added PrevEndTimeStamp : %#I64x, PrevEndSequenceNumber : %#I64x\n", PrevEndTimeStamp, PrevEndSeqNumber ));
                InVolDbgPrint(DL_INFO, DM_REGISTRY, 
                   ("PrevSequenceId : %#x to Registry for Volume %S\n", PrevSequenceId, pDevContext->wDevNum));
            }
        }
        KeReleaseMutex(&pDevContext->Mutex, false);
    } 
    return Status;


}


/*------------------------------------------------------------------------------
 * This array is documented in InmFltInterface.h changes to this array have to be made
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

/*-----------------------------------------------------------------------------
 * Function Name : SetDevOutOfSync
 * Parameters :
 *      pDevContext : This parameter points to the volume that is 
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
SetDevOutOfSync(
    PDEV_CONTEXT   pDevContext,
    ULONG       ulOutOfSyncErrorCode,
    NTSTATUS    StatusToLog,
    bool        bLockAcquired
    )
{
    NTSTATUS    Status = STATUS_SUCCESS;
    Registry    *pDevParam = NULL;
    LARGE_INTEGER   SystemTime, LocalTime;
    TIME_FIELDS     TimeFields;
    KIRQL           OldIrql = 0;
    ULONGLONG   OutOfSyncTimeStamp;
    ULONGLONG   OutOfSyncSequenceNumber;

    if (!bLockAcquired)
        KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);

    if (pDevContext->bQueueChangesToTempQueue) {
        FreeChangeNodeList(&pDevContext->ChangeList, ecChangesQueuedToTempQueue);
    }

    if (!bLockAcquired)
        KeReleaseSpinLock(&pDevContext->Lock, OldIrql);

    if (TEST_FLAG(pDevContext->ulFlags, DCF_IGNORE_BITMAP_CREATION) &&
        (MSG_INDSKFLT_INFO_BITMAP_FILE_CREATED_Message == ulOutOfSyncErrorCode)) {
        InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, ("SetDevOutOfSync: Ignoring bitmap creation as out of sync for device %s(%s)\n",
                                                pDevContext->chDevNum, pDevContext->chDevID));
        return Status;
    }

    if (bLockAcquired || (PASSIVE_LEVEL != KeGetCurrentIrql())) {
        // This is being called at dispatch level, and as registry can not be accessed
        // at dispatch level, we have to queue a work item to do so.

        Status = QueueWorkerRoutineForSetDevOutOfSync(pDevContext, ulOutOfSyncErrorCode, StatusToLog, bLockAcquired);
        if (STATUS_SUCCESS == Status)
            return STATUS_PENDING;

        return Status;
    }

#ifdef VOLUME_CLUSTER_SUPPORT
    if (IS_NOT_CLUSTER_VOLUME(pDevContext)) {
#endif

        pDevParam = new Registry();
        if (NULL == pDevParam)
            return STATUS_INSUFFICIENT_RESOURCES;
    
        Status = pDevParam->OpenKeyReadWrite(&pDevContext->DevParameters, TRUE);

#ifdef VOLUME_CLUSTER_SUPPORT
    }
#endif

    KeWaitForSingleObject(&pDevContext->Mutex, Executive, KernelMode, FALSE, NULL);

    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
    GenerateTimeStamp(OutOfSyncTimeStamp, OutOfSyncSequenceNumber);
    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);

    SystemTime.QuadPart = OutOfSyncTimeStamp;
    pDevContext->bResyncRequired = true;
    pDevContext->ulOutOfSyncCount++;
    pDevContext->liOutOfSyncTimeStamp.QuadPart = SystemTime.QuadPart;
    pDevContext->ulOutOfSyncErrorStatus = StatusToLog;
    pDevContext->ulOutOfSyncErrorCode = ulOutOfSyncErrorCode;
    pDevContext->ulOutOfSyncTotalCount++;
    pDevContext->ullOutOfSyncSequnceNumber = OutOfSyncSequenceNumber;
#ifdef VOLUME_CLUSTER_SUPPORT
    if (IS_NOT_CLUSTER_VOLUME(pDevContext) && NT_SUCCESS(Status)) {
#else
    if (NT_SUCCESS(Status)) {
#endif
        UNICODE_STRING  ErrorString;
        InMageInitUnicodeString(&ErrorString);
        Status = pDevParam->WriteULONG(DEVICE_RESYNC_REQUIRED, DEVICE_RESYNC_REQUIRED_SET);
        Status = pDevParam->WriteULONG(DEVICE_OUT_OF_SYNC_ERRORCODE, ulOutOfSyncErrorCode);

        if (ulOutOfSyncErrorCode >= ERROR_TO_REG_MAX_ERROR) {
            ulOutOfSyncErrorCode = ERROR_TO_REG_DESCRIPTION_IN_EVENT_LOG;
        }
        Status = InMageAppendUnicodeStringWithFormat(&ErrorString, PagedPool, ErrorToRegErrorDescriptionsW[ulOutOfSyncErrorCode], StatusToLog);
        if (NT_SUCCESS(Status)) {
            Status = InMageTerminateUnicodeStringWith(&ErrorString, L'\0', PagedPool);
            if (NT_SUCCESS(Status)) {

                Status = pDevParam->WriteWString(DEVICE_OUT_OF_SYNC_ERROR_DESCRIPTION, ErrorString.Buffer);

            }
        }
        InMageFreeUnicodeString(&ErrorString);

        Status = pDevParam->WriteULONG(DEVICE_OUT_OF_SYNC_ERRORSTATUS, StatusToLog);
        Status = pDevParam->WriteULONG(DEVICE_OUT_OF_SYNC_COUNT, pDevContext->ulOutOfSyncCount);

        ExSystemTimeToLocalTime(&SystemTime, &LocalTime);
        RtlTimeToTimeFields(&LocalTime, &TimeFields);
        // VOLUME_OUT_OF_SYNC_TIMESTAMP is in formation MM:DD:YYYY::HH:MM:SS:XXX(MilliSeconds) 24 + NULL
        Status = RtlStringCbPrintfW(pDevContext->BufferForOutOfSyncTimeStamp,
                                    MAX_DEV_OUT_OF_SYNC_TIMESTAMP_SIZE_IN_WCHARS * sizeof(WCHAR),
                                    L"%02hu:%02hu:%04hu::%02hu:%02hu:%02hu:%03hu",
                                    TimeFields.Month, TimeFields.Day, TimeFields.Year,
                                    TimeFields.Hour, TimeFields.Minute, TimeFields.Second, TimeFields.Milliseconds);
        if (STATUS_SUCCESS == Status) {

            Status = pDevParam->WriteWString(DEVICE_OUT_OF_SYNC_TIMESTAMP, pDevContext->BufferForOutOfSyncTimeStamp);
        }

        Status = pDevParam->WriteBinary(DEVICE_OUT_OF_SYNC_TIMESTAMP_IN_GMT, &SystemTime.QuadPart, sizeof(SystemTime.QuadPart));
        if (STATUS_SUCCESS != Status) {
            InVolDbgPrint(DL_WARNING, DM_DIRTY_BLOCKS, 
                    ("SetDevOutOfSync: Key %ws could not be set, Status: %x\n", DEVICE_OUT_OF_SYNC_TIMESTAMP_IN_GMT, Status));
        }

#ifdef VOLUME_CLUSTER_SUPPORT
   } else if (pDevContext->ulFlags & DCF_CLUSTER_VOLUME_ONLINE) {
        PDEV_BITMAP pDevBitmap = NULL;
        KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
        if (pDevContext->pDevBitmap)
            pDevBitmap = ReferenceDevBitmap(pDevContext->pDevBitmap);
        KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
        if (NULL != pDevBitmap) {
            ExAcquireFastMutex(&pDevBitmap->Mutex);
            if (pDevBitmap->pBitmapAPI) {
                pDevBitmap->pBitmapAPI->AddResynRequiredToBitmap(pDevContext->ulOutOfSyncErrorCode,
                                                                    StatusToLog,
                                                                    SystemTime.QuadPart, 
                                                                    pDevContext->ulOutOfSyncCount,
                                                                    pDevContext->ulOutOfSyncIndicatedAtCount);
            }
            ExReleaseFastMutex(&pDevBitmap->Mutex);
            DereferenceDevBitmap(pDevBitmap);
        }
#endif
    }

    if (pDevContext->ulPrevOutOfSyncErrorCodeSet != pDevContext->ulOutOfSyncErrorCode)
     {
        InDskFltWriteEventWithDevCtxt(INDSKFLT_WARN_OUT_OF_SYNC, pDevContext,
                                    pDevContext->ulOutOfSyncErrorCode,
                                    pDevContext->liOutOfSyncTimeStamp.QuadPart,
                                    pDevContext->ulOutOfSyncTotalCount,
                                    pDevContext->ulOutOfSyncCount,
                                    pDevContext->ulOutOfSyncErrorStatus);
     }

     pDevContext->ulPrevOutOfSyncErrorCodeSet = pDevContext->ulOutOfSyncErrorCode;

    KeReleaseMutex(&pDevContext->Mutex, FALSE);
    if (NULL != pDevParam) {
        pDevParam->Release();
    }
    return Status;
}

void ResetDeviceId(
    IN PDEV_CONTEXT pDevContext,
    IN bool bResetIfFiteringStopped
)
{
    KIRQL   oldIrql;
    WCHAR   wDevID[DEVID_LENGTH] = L"";
    ULONG   ulDeviceNum = MAXULONG;

    KeAcquireSpinLock(&pDevContext->Lock, &oldIrql);
    if (bResetIfFiteringStopped && 
        !TEST_FLAG(pDevContext->ulFlags, DCF_FILTERING_STOPPED)) {
        KeReleaseSpinLock(&pDevContext->Lock, oldIrql);
        return;
    }

    CLEAR_FLAG(pDevContext->ulFlags, DCF_DEVID_OBTAINED);
    RtlCopyMemory(wDevID, pDevContext->wDevID, sizeof(pDevContext->wDevID));
    RtlZeroMemory(pDevContext->wDevID, sizeof(pDevContext->wDevID));

    if (TEST_FLAG(pDevContext->ulFlags, DCF_DEVNUM_OBTAINED)) {
        ulDeviceNum = pDevContext->ulDevNum;
    }
    KeReleaseSpinLock(&pDevContext->Lock, oldIrql);

    InDskFltWriteEvent(INDSKFLT_INFO_DEVICE_ID_RESET, ulDeviceNum, wDevID);
}

/*-------------------------------------------------------------------------------
 * bool bClear - bClear is set if Resync information has to be cleared. This
 * function is called with bClear set to true when differentials are cleared. We
 * would like to clear resync information in that case.
 *-------------------------------------------------------------------------------
 */ 
void
ResetDevOutOfSync(PDEV_CONTEXT   DevContext, bool bClear)
{
    NTSTATUS        Status = STATUS_SUCCESS;
    Registry        *pDevParam = NULL;
#ifdef VOLUME_CLUSTER_SUPPORT
	PDEV_BITMAP  pDevBitmap = NULL;
    KIRQL           OldIrql;
#endif
    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());
    /* BugFix 19342: Resync Required set to YES during Failback Protectection in Step2 */
    /* ClearDiff should reset all the pending resync as Resync step1 is expected*/
    if ((!DevContext->bResyncRequired || !DevContext->bResyncIndicated) && (!bClear)){
        return;
    }
    pDevParam = new Registry();
    KeWaitForSingleObject(&DevContext->Mutex, Executive, KernelMode, FALSE, NULL);
    ASSERT(bClear || (DevContext->ulOutOfSyncCount >= DevContext->ulOutOfSyncIndicatedAtCount));
    if (bClear || (DevContext->ulOutOfSyncIndicatedAtCount >= DevContext->ulOutOfSyncCount)) {
        // ReSync clear is requested or
        // No resync was set between resync indication and acking the resync action
        DevContext->bResyncRequired = false;
        DevContext->ulOutOfSyncCount = 0;
        if (DevContext->liOutOfSyncTimeStamp.QuadPart) {
            DevContext->liLastOutOfSyncTimeStamp.QuadPart = DevContext->liOutOfSyncTimeStamp.QuadPart;
            DevContext->liLogLastOutOfSyncTimeStamp.QuadPart = DevContext->liOutOfSyncTimeStamp.QuadPart;
        }
        DevContext->liOutOfSyncTimeStamp.QuadPart = 0;
        if (DevContext->ulOutOfSyncErrorCode) {
            DevContext->ulLastOutOfSyncErrorCode = DevContext->ulOutOfSyncErrorCode;
            DevContext->ulLogLastOutOfSyncErrorCode = DevContext->ulOutOfSyncErrorCode;
        }
        DevContext->ulOutOfSyncErrorCode = 0;
        if (DevContext->ullOutOfSyncSequnceNumber) {
            DevContext->ullLastOutOfSyncSeqNumber = DevContext->ullOutOfSyncSequnceNumber;
        }
        DevContext->ullOutOfSyncSequnceNumber = 0;  
        DevContext->ulOutOfSyncIndicatedAtCount = 0;
        KeQuerySystemTime(&DevContext->liOutOfSyncResetTimeStamp);
#ifdef VOLUME_CLUSTER_SUPPORT
        if (IS_NOT_CLUSTER_VOLUME(DevContext)) {
#endif

            if (NULL != pDevParam) {
                Status = pDevParam->OpenKeyReadWrite(&DevContext->DevParameters, TRUE);
                if (NT_SUCCESS(Status)) {
                    Status = pDevParam->WriteULONG(DEVICE_RESYNC_REQUIRED, DEVICE_RESYNC_REQUIRED_UNSET);
                    Status = pDevParam->DeleteValue(DEVICE_OUT_OF_SYNC_ERRORCODE, STRING_LEN_NULL_TERMINATED);
                    Status = pDevParam->DeleteValue(DEVICE_OUT_OF_SYNC_ERRORSTATUS, STRING_LEN_NULL_TERMINATED);
                    Status = pDevParam->DeleteValue(DEVICE_OUT_OF_SYNC_ERROR_DESCRIPTION, STRING_LEN_NULL_TERMINATED);
                    Status = pDevParam->DeleteValue(DEVICE_OUT_OF_SYNC_COUNT, STRING_LEN_NULL_TERMINATED);
                    Status = pDevParam->DeleteValue(DEVICE_OUT_OF_SYNC_TIMESTAMP, STRING_LEN_NULL_TERMINATED);
                    Status = pDevParam->DeleteValue(DEVICE_OUT_OF_SYNC_TIMESTAMP_IN_GMT, STRING_LEN_NULL_TERMINATED);
                }
            }
#ifdef VOLUME_CLUSTER_SUPPORT
        } else if (DevContext->ulFlags & DCF_CLUSTER_VOLUME_ONLINE) {
            KeAcquireSpinLock(&DevContext->Lock, &OldIrql);
            if (DevContext->pDevBitmap)
                pDevBitmap = ReferenceDevBitmap(DevContext->pDevBitmap);
            KeReleaseSpinLock(&DevContext->Lock, OldIrql);
            if (NULL != pDevBitmap) {
                ExAcquireFastMutex(&pDevBitmap->Mutex);
                if (pDevBitmap->pBitmapAPI) {
                    pDevBitmap->pBitmapAPI->ResetResynRequiredInBitmap(DevContext->ulOutOfSyncCount,
                                                                          DevContext->ulOutOfSyncIndicatedAtCount,
                                                                          DevContext->liOutOfSyncIndicatedTimeStamp.QuadPart,
                                                                          DevContext->liOutOfSyncResetTimeStamp.QuadPart,
                                                                          true);
                }
                ExReleaseFastMutex(&pDevBitmap->Mutex);
                DereferenceDevBitmap(pDevBitmap);
            }
        }
#endif
    } else {
        // Resync is set after indicating resync required. Do not reset bResyncRequired
        DevContext->ulOutOfSyncCount -= DevContext->ulOutOfSyncIndicatedAtCount;
#ifdef VOLUME_CLUSTER_SUPPORT
        if (IS_NOT_CLUSTER_VOLUME(DevContext)) {
#endif
            if (NULL != pDevParam) {
                Status = pDevParam->OpenKeyReadWrite(&DevContext->DevParameters, TRUE);
                if (NT_SUCCESS(Status)) {
                    Status = pDevParam->WriteULONG(DEVICE_OUT_OF_SYNC_COUNT, DevContext->ulOutOfSyncCount);
                }
            }
#ifdef VOLUME_CLUSTER_SUPPORT
        } else if (DevContext->ulFlags & DCF_CLUSTER_VOLUME_ONLINE) {
            KeAcquireSpinLock(&DevContext->Lock, &OldIrql);
            if (DevContext->pDevBitmap)
                pDevBitmap = ReferenceDevBitmap(DevContext->pDevBitmap);
            KeReleaseSpinLock(&DevContext->Lock, OldIrql);
            if (NULL != pDevBitmap) {
                ExAcquireFastMutex(&pDevBitmap->Mutex);
                if (pDevBitmap->pBitmapAPI) {
                    pDevBitmap->pBitmapAPI->ResetResynRequiredInBitmap(DevContext->ulOutOfSyncCount,
                                                                          DevContext->ulOutOfSyncIndicatedAtCount,
                                                                          DevContext->liOutOfSyncIndicatedTimeStamp.QuadPart,
                                                                          DevContext->liOutOfSyncResetTimeStamp.QuadPart,
                                                                          false);
                }
                ExReleaseFastMutex(&pDevBitmap->Mutex);
                DereferenceDevBitmap(pDevBitmap);
            }
        }
#endif
    }

    DevContext->bResyncIndicated = false;
    KeReleaseMutex(&DevContext->Mutex, FALSE);

    if (NULL != pDevParam) {
        pDevParam->Release();
    }
    return;
}

VOID
SetDevOutOfSyncWorkerRoutine(PWORK_QUEUE_ENTRY    pWorkQueueEntry)
{
    PDEV_CONTEXT   pDevContext;
    ULONG       ulOutOfSyncErrorCode;
    KIRQL           OldIrql;
    NTSTATUS    Status = (NTSTATUS)pWorkQueueEntry->ulInfo2;

    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    pDevContext = (PDEV_CONTEXT)pWorkQueueEntry->Context;
    ulOutOfSyncErrorCode = pWorkQueueEntry->ulInfo1;

    // Holding reference to DevContext so that BitMap File is not closed 
    // before the writes are completed.
    
    // Clearing usage status of WorkQueueEntry for resync
    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);

    pDevContext->bReSyncWorkQueueRef = false;

    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);

    SetDevOutOfSync(pDevContext, ulOutOfSyncErrorCode, Status, false);

    DereferenceDevContext(pDevContext);
    return;
}

NTSTATUS
QueueWorkerRoutineForSetDevOutOfSync(PDEV_CONTEXT  pDevContext, ULONG ulOutOfSyncErrorCode, NTSTATUS Status, bool bLockAcquired)
{
    PWORK_QUEUE_ENTRY   pWorkQueueEntry = NULL;
    KIRQL           OldIrql = 0;
    if (!bLockAcquired)
        KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
    
    // Check the usage status of WorkQueueEntry
    if (!pDevContext->bReSyncWorkQueueRef) {
        pWorkQueueEntry = pDevContext->pReSyncWorkQueueEntry;
        pDevContext->bReSyncWorkQueueRef = 1;
        // Cleanup of the given WorkQueuEntry required for reusing
        InitializeWorkQueueEntry(pWorkQueueEntry);
    }

    if (!bLockAcquired)
        KeReleaseSpinLock(&pDevContext->Lock, OldIrql);

    if (NULL != pWorkQueueEntry)
    {
        pWorkQueueEntry->Context = ReferenceDevContext(pDevContext);
        pWorkQueueEntry->ulInfo1 = ulOutOfSyncErrorCode;
        pWorkQueueEntry->ulInfo2 = Status;
        pWorkQueueEntry->WorkerRoutine = SetDevOutOfSyncWorkerRoutine;

        AddItemToWorkQueue(&DriverContext.WorkItemQueue, pWorkQueueEntry);
        return STATUS_SUCCESS;
    }

    return STATUS_INSUFFICIENT_RESOURCES;
}

NTSTATUS
LoadDeviceConfigFromRegistry(PDEV_CONTEXT pDevContext)
/*++

Routine Description: This routine reads the per-device configuration data from registry. If the registry keys are not found, this creates
                     new keys and set the default values

Arguments: pDevContext - pointer to per-device context

Return Value: NTSTATUS

This is the common function for both the filters
--*/
{
    NTSTATUS    Status = STATUS_SUCCESS;
    Registry    *pDevParam = NULL;
    ULONG       ulValue;
    ULONG       ulDevDataModeCapture, ulDeviceDataFiles;

    ASSERT((NULL != pDevContext) && (pDevContext->ulFlags & DCF_DEVID_OBTAINED));
    ASSERT(DISPATCH_LEVEL > KeGetCurrentIrql());

    InVolDbgPrint(DL_FUNC_TRACE, DM_DEV_CONTEXT,
        ("LoadDeviceCfgFromRegistry: Called - DevContext(%p) for GUID(%S)\n",
        pDevContext, pDevContext->wDevID));

    if (NULL == pDevContext) {
        InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT, ("pDevContext NULL\n"));
        return STATUS_INVALID_PARAMETER_1;
    }

    if (!(pDevContext->ulFlags & DCF_DEVID_OBTAINED)) {
        InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT,
            ("LoadDeviceCfgFromRegistry: Called before GUID is obtained\n"));
        return STATUS_INVALID_PARAMETER_2;
    }

// Let us open/create the registry key for this device.
    pDevParam = new Registry();
    if (NULL == pDevParam) {
        InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT,
            ("Failed in creating registry object\n"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Status = pDevParam->OpenKeyReadWrite(&pDevContext->DevParameters);
    if (!NT_SUCCESS(Status)) {
        Status = pDevParam->CreateKeyReadWrite(&pDevContext->DevParameters);
        if (!NT_SUCCESS(Status)) {
            InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT,
                ("LoadDeviceCfgFromRegistry: Failed to Creat reg key %wZ with Status 0x%x",
                &pDevContext->DevParameters, Status));
            return Status;
        }
        else {

            InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                ("LoadDeviceCfgFromRegistry: Created reg key %wZ", &pDevContext->DevParameters));
        }
    }
    else {
        InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
            ("LoadDeviceCfgFromRegistry: Opened reg key %wZ", &pDevContext->DevParameters));
    }

    Status = pDevParam->ReadULONG(
        DEVICE_BITMAP_MAX_WRITEGROUPS_IN_HEADER,
        (ULONG32 *)&ulValue,
        MAX_WRITE_GROUPS_IN_BITMAP_HEADER_DEFAULT_VALUE,
        TRUE);

    if (NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
            ("LoadDeviceCfgFromRegistry: Read Value %S Value = 0x%x\n",
            DEVICE_BITMAP_MAX_WRITEGROUPS_IN_HEADER, ulValue));
        pDevContext->ulMaxBitmapHdrWriteGroups = ulValue;
    } else {
        InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
            ("LoadDeviceCfgFromRegistry: Read Value %S failed with error = 0x%x\n",
            DEVICE_BITMAP_MAX_WRITEGROUPS_IN_HEADER, Status));

        // If this registry read is failed for disk with 256KB(511 sectors + 1 hdr) header, 
        // defaults to 16KB (31 sectors + 1 hdr) header, verification of header or checksum eventually will fail
        pDevContext->ulMaxBitmapHdrWriteGroups = MAX_WRITE_GROUPS_IN_BITMAP_HEADER_DEFAULT_VALUE;
        Status = STATUS_SUCCESS;
    }

#ifdef VOLUME_CLUSTER_SUPPORT
    if (IS_NOT_CLUSTER_VOLUME(pDevContext)) {
#endif
        ULONG   ulDevFilteringDisabledDefaultValue;

        if (DriverContext.bDisableFilteringForNewDevice) {
            ulDevFilteringDisabledDefaultValue = 0x01;
        }
        else {
            ulDevFilteringDisabledDefaultValue = 0x00;
        }

        Status = pDevParam->ReadULONG(DEVICE_DRAIN_STATE, (ULONG32 *)&ulValue, 0, TRUE);

        if (NT_SUCCESS(Status) && (1 == ulValue)) {
            SET_FLAG(pDevContext->ulFlags2, DEVICE_FLAGS_DONT_DRAIN);
        }
        ulValue = 0;

        ULONG   ulClusterFlags = 0;

        Status = pDevParam->ReadULONG(CLUSTER_ATTRIBUTES, (ULONG32*)&ulClusterFlags);
        CLEAR_FLAG(ulClusterFlags, DCF_CLUSTER_FLAGS_DISK_ONLINE);
        CLEAR_FLAG(ulClusterFlags, DCF_CLUSTER_FLAGS_DISK_PROTECTED);
        pDevContext->ulClusterFlags = ulClusterFlags;

        Status = pDevParam->ReadULONG(DEVICE_FILTERING_DISABLED, (ULONG32 *)&ulValue,
            ulDevFilteringDisabledDefaultValue, TRUE);
        if (NT_SUCCESS(Status)) {
            if (STATUS_INMAGE_OBJECT_CREATED == Status) {
                InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                    ("LoadDeviceCfgFromRegistry: Created %S with Default Value 0x%x\n",
                    DEVICE_FILTERING_DISABLED, 0));
            }
            else {
                InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                    ("LoadDeviceCfgFromRegistry: Read Value %S Value = 0x%x\n",
                    DEVICE_FILTERING_DISABLED, ulValue));
            }
        }
        else {
            InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT,
                ("LoadDeviceCfgFromRegistry: Failed to Read Value %S with error = 0x%x\n",
                DEVICE_FILTERING_DISABLED, Status));

        }


        // Read Per device Persistent Values from Registry
        GetDeviceContextFieldsFromRegistry(pDevContext);

        if (0 != ulValue) {
            // filtering is disabled on a device
            StopFilteringDevice(pDevContext, false);
        }
        else {
            if (!IS_DISK_CLUSTERED(pDevContext)) {
                StartFilteringDevice(pDevContext, false);
            }
            else {
                SET_FLAG(ulClusterFlags, DCF_CLUSTER_FLAGS_DISK_PROTECTED);
                SetDiskClusterState(pDevContext);
                InDskFltCreateClusterNotificationThread(pDevContext);
            }

#ifdef _WIN64
            SetDataPool(1, DriverContext.ullDataPoolSize);
#endif
        }
#ifdef VOLUME_CLUSTER_SUPPORT
    }
#endif

    Status = pDevParam->ReadULONG(DEVICE_BITMAP_READ_DISABLED, (ULONG32 *)&ulValue,
        DEVICE_BITMAP_READ_DISABLED_DEFAULT_VALUE, TRUE);
    if (NT_SUCCESS(Status)) {
        if (STATUS_INMAGE_OBJECT_CREATED == Status) {
            InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                ("LoadDeviceCfgFromRegistry: Created %S with Default Value 0x%x\n",
                DEVICE_BITMAP_READ_DISABLED, 0));
        }
        else {
            InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                ("LoadDeviceCfgFromRegistry: Read Value %S Value = 0x%x\n",
                DEVICE_BITMAP_READ_DISABLED, ulValue));

        }
    }
    else {
        InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT,
            ("LoadDeviceCfgFromRegistry: Failed to Read Value %S with error = 0x%x\n",
            DEVICE_BITMAP_READ_DISABLED, Status));
    }

    if (0 != ulValue) {
        pDevContext->ulFlags |= DCF_BITMAP_READ_DISABLED;
    }

    Status = pDevParam->ReadULONG(DEVICE_BITMAP_WRITE_DISABLED, (ULONG32 *)&ulValue,
        DEVICE_BITMAP_WRITE_DISABLED_DEFAULT_VALUE, TRUE);
    if (NT_SUCCESS(Status)) {
        if (STATUS_INMAGE_OBJECT_CREATED == Status) {
            InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                ("LoadDeviceCfgFromRegistry: Created %S with Default Value 0x%x\n",
                DEVICE_BITMAP_WRITE_DISABLED, 0));
        }
        else {
            InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                ("LoadDeviceCfgFromRegistry: Read Value %S Value = 0x%x\n",
                DEVICE_BITMAP_WRITE_DISABLED, ulValue));
        }
    }
    else {
        InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT,
            ("LoadDeviceCfgFromRegistry: Failed to Read Value %S with error = 0x%x\n",
            DEVICE_BITMAP_WRITE_DISABLED, Status));
    }

    if (0 != ulValue) {
        pDevContext->ulFlags |= DCF_BITMAP_WRITE_DISABLED;
    }

    Status = pDevParam->ReadULONG(DEVICE_BITMAP_OFFSET, (ULONG32 *)&ulValue,
        DEVICE_BITMAP_OFFSET_DEFAULT_VALUE, FALSE);
    if (NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
            ("LoadDeviceCfgFromRegistry: Read Value %S Value = 0x%x\n",
            DEVICE_BITMAP_OFFSET, ulValue));
    }
    pDevContext->ulBitmapOffset = ulValue;



#ifdef VOLUME_FLT
    Status = pDevParam->ReadULONG(DEVICE_READ_ONLY, (ULONG32 *)&ulValue,
        DEVICE_READ_ONLY_DEFAULT_VALUE, TRUE);
    if (NT_SUCCESS(Status)) {
        if (STATUS_INMAGE_OBJECT_CREATED == Status) {
            InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                ("LoadDeviceCfgFromRegistry: Created %S with Default Value 0x%x\n",
                DEVICE_READ_ONLY, DEVICE_READ_ONLY_DEFAULT_VALUE));
        }
        else {
            InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                ("LoadDeviceCfgFromRegistry: Read Value %S Value = 0x%x\n",
                DEVICE_READ_ONLY, ulValue));
        }
    }
    else {
        InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT,
            ("LoadDeviceCfgFromRegistry: Failed to Read Value %S with error = 0x%x\n",
            DEVICE_READ_ONLY, Status));
    }

    if (0 != ulValue) {
        pDevContext->ulFlags |= DCF_READ_ONLY;
    }
#endif
#ifdef VOLUME_CLUSTER_SUPPORT
    //For Cluster volumes we started saving the resync required information in the bitmap
    if (IS_NOT_CLUSTER_VOLUME(pDevContext)) {
#endif
        // Get Resync Required values
        Status = pDevParam->ReadULONG(DEVICE_RESYNC_REQUIRED, (ULONG32 *)&ulValue,
            DEFAULT_DEVICE_RESYNC_REQUIRED_VALUE, FALSE);
        if (NT_SUCCESS(Status)) {
            if (STATUS_INMAGE_OBJECT_CREATED == Status) {
                InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                    ("LoadDeviceCfgFromRegistry: Created %S with Default Value 0x%x\n",
                    DEVICE_RESYNC_REQUIRED, DEFAULT_DEVICE_RESYNC_REQUIRED_VALUE));
            }
            else {
                InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                    ("LoadDeviceCfgFromRegistry: Read Value %S Value = 0x%x\n",
                    DEVICE_RESYNC_REQUIRED, ulValue));
            }
        }
        else {
            InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT,
                ("LoadDeviceCfgFromRegistry: Failed to Read Value %S with error = 0x%x\n",
                DEVICE_RESYNC_REQUIRED, Status));
        }

        if (DEVICE_RESYNC_REQUIRED_SET == ulValue) {
            pDevContext->bResyncRequired = true;

            Status = pDevParam->ReadULONG(DEVICE_OUT_OF_SYNC_COUNT, (ULONG32 *)&ulValue,
                DEFAULT_DEVICE_OUT_OF_SYNC_COUNT, FALSE);
            if (NT_SUCCESS(Status)) {
                if (STATUS_INMAGE_OBJECT_CREATED == Status) {
                    InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                        ("LoadDeviceCfgFromRegistry: Created %S with Default Value 0x%x\n",
                        DEVICE_OUT_OF_SYNC_COUNT, DEFAULT_DEVICE_OUT_OF_SYNC_COUNT));
                }
                else {
                    InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                        ("LoadDeviceCfgFromRegistry: Read Value %S Value = 0x%x\n",
                        DEVICE_OUT_OF_SYNC_COUNT, ulValue));
                }
            }
            else {
                InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT,
                    ("LoadDeviceCfgFromRegistry: Failed to Read Value %S with error = 0x%x\n",
                    DEVICE_OUT_OF_SYNC_COUNT, Status));
            }

            pDevContext->ulOutOfSyncCount = ulValue;

            Status = pDevParam->ReadULONG(DEVICE_OUT_OF_SYNC_ERRORCODE, (ULONG32 *)&ulValue,
                DEFAULT_DEVICE_OUT_OF_SYNC_ERRORCODE, FALSE);
            if (NT_SUCCESS(Status)) {
                if (STATUS_INMAGE_OBJECT_CREATED == Status) {
                    InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                        ("LoadDeviceCfgFromRegistry: Created %S with Default Value 0x%x\n",
                        DEVICE_OUT_OF_SYNC_ERRORCODE, DEFAULT_DEVICE_OUT_OF_SYNC_ERRORCODE));
                }
                else {
                    InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                        ("LoadDeviceCfgFromRegistry: Read Value %S Value = 0x%x\n",
                        DEVICE_OUT_OF_SYNC_ERRORCODE, ulValue));
                }
            }
            else {
                InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT,
                    ("LoadDeviceCfgFromRegistry: Failed to Read Value %S with error = 0x%x\n",
                    DEVICE_OUT_OF_SYNC_ERRORCODE, Status));
            }

            pDevContext->ulOutOfSyncErrorCode = ulValue;

            Status = pDevParam->ReadULONG(DEVICE_OUT_OF_SYNC_ERRORSTATUS, (ULONG32 *)&ulValue,
                DEFAULT_DEVICE_OUT_OF_SYNC_ERRORSTATUS, FALSE);
            if (NT_SUCCESS(Status)) {
                if (STATUS_INMAGE_OBJECT_CREATED == Status) {
                    InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                        ("LoadDeviceCfgFromRegistry: Created %S with Default Value 0x%x\n",
                        DEVICE_OUT_OF_SYNC_ERRORSTATUS, DEFAULT_DEVICE_OUT_OF_SYNC_ERRORSTATUS));
                }
                else {
                    InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                        ("LoadDeviceCfgFromRegistry: Read Value %S Value = 0x%x\n",
                        DEVICE_OUT_OF_SYNC_ERRORSTATUS, ulValue));
                }
            }
            else {
                InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT,
                    ("LoadDeviceCfgFromRegistry: Failed to Read Value %S with error = 0x%x\n",
                    DEVICE_OUT_OF_SYNC_ERRORSTATUS, Status));
            }

            pDevContext->ulOutOfSyncErrorStatus = ulValue;

            ULONG           ulcbRead = 0;
            ULARGE_INTEGER  uliDefaultData;
            PVOID           pAddress = &pDevContext->liOutOfSyncTimeStamp;

            uliDefaultData.QuadPart = 0;
            Status = pDevParam->ReadBinary(DEVICE_OUT_OF_SYNC_TIMESTAMP_IN_GMT,
                STRING_LEN_NULL_TERMINATED,
                &pAddress,
                sizeof(ULARGE_INTEGER),
                &ulcbRead,
                (PVOID)&uliDefaultData,
                sizeof(ULARGE_INTEGER));
            if (NT_SUCCESS(Status)) {
                ASSERT(sizeof(ULARGE_INTEGER) == ulcbRead);
                if (STATUS_INMAGE_OBJECT_CREATED == Status) {
                    InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                        ("LoadDeviceCfgFromRegistry: Created %S with Default Value %#I64x\n",
                        DEVICE_OUT_OF_SYNC_TIMESTAMP, 0));
                }
                else {
                    InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                        ("LoadDeviceCfgFromRegistry: Read Value %S Value = %#I64x\n",
                        DEVICE_OUT_OF_SYNC_TIMESTAMP, pDevContext->liOutOfSyncTimeStamp.QuadPart));
                }
            }
            else {
                InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT,
                    ("LoadDeviceCfgFromRegistry: Failed to Read Value %S with error = %#x\n",
                    DEVICE_OUT_OF_SYNC_TIMESTAMP, Status));
            }

            InDskFltWriteEventWithDevCtxt(INDSKFLT_WARN_RESYNC_FOUND_ON_REGISTRY_READ, 
                                          pDevContext, 
                                          pDevContext->ulOutOfSyncErrorCode, 
                                          pDevContext->ulOutOfSyncErrorStatus,
                                          pDevContext->liOutOfSyncTimeStamp.QuadPart,
                                          pDevContext->ulOutOfSyncCount);
        }
        else {
            pDevContext->bResyncRequired = false;
        }
#ifdef VOLUME_CLUSTER_SUPPORT
    }
#endif
    // Get data filtering values.
    if (DriverContext.bEnableDataCapture) {
        if (DriverContext.bEnableDataModeCaptureForNewDevices) {
            ulDevDataModeCapture = 0x01;
        }
        else {
            ulDevDataModeCapture = 0x00;
        }
        Status = pDevParam->ReadULONG(DEVICE_DATAMODE_CAPTURE, (ULONG32 *)&ulValue,
            ulDevDataModeCapture, TRUE);
        if (NT_SUCCESS(Status)) {
            if (STATUS_INMAGE_OBJECT_CREATED == Status) {
                InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                    ("LoadDeviceCfgFromRegistry: Created %S with Default Value 0x%x\n",
                    DEVICE_DATAMODE_CAPTURE, ulDevDataModeCapture));
            }
            else {
                InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                    ("LoadDeviceCfgFromRegistry: Read Value %S Value = 0x%x\n",
                    DEVICE_DATAMODE_CAPTURE, ulValue));
            }
        }
        else {
            InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT,
                ("LoadDeviceCfgFromRegistry: Failed to Read Value %S with error = 0x%x\n",
                DEVICE_DATAMODE_CAPTURE, Status));
        }

        if (0 == ulValue) {
            pDevContext->ulFlags |= DCF_DATA_CAPTURE_DISABLED;
            InVolDbgPrint(DL_WARNING, DM_DEV_CONTEXT,
                ("LoadDeviceCfgFromRegistry: Setting data capture disabled for device %S(%S)\n",
                pDevContext->wDevID, pDevContext->wDevNum));
        }

        if (DriverContext.bEnableDataFilesForNewDevs) {
            ulDeviceDataFiles = 0x01;
        }
        else {
            ulDeviceDataFiles = 0x00;
        }

        Status = pDevParam->ReadULONG(DEVICE_DATA_FILES, (ULONG32 *)&ulValue,
            ulDeviceDataFiles, TRUE);
        if (NT_SUCCESS(Status)) {
            if (STATUS_INMAGE_OBJECT_CREATED == Status) {
                InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                    ("LoadDeviceCfgFromRegistry: Created %S with Default Value 0x%x\n",
                    DEVICE_DATA_FILES, ulDeviceDataFiles));
            }
            else {
                InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                    ("LoadDeviceCfgFromRegistry: Read Value %S Value = 0x%x\n",
                    DEVICE_DATA_FILES, ulValue));
            }
        }
        else {
            InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT,
                ("LoadDeviceCfgFromRegistry: Failed to Read Value %S with error = 0x%x\n",
                DEVICE_DATA_FILES, Status));
        }

        if (0 == ulValue) {
            pDevContext->ulFlags |= DCF_DATA_FILES_DISABLED;
            InVolDbgPrint(DL_WARNING, DM_DEV_CONTEXT,
                ("LoadDeviceCfgFromRegistry: Setting data files disabled for device %S(%S)\n",
                pDevContext->wDevID, pDevContext->wDevNum));
        }

        Status = pDevParam->ReadULONG(DEVICE_DATA_TO_DISK_LIMIT_IN_MB, (ULONG32 *)&ulValue,
            DriverContext.ulDataToDiskLimitPerDevInMB, TRUE);
        if (NT_SUCCESS(Status)) {
            if (STATUS_INMAGE_OBJECT_CREATED == Status) {
                InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                    ("LoadDeviceCfgFromRegistry: Created %S with Default Value 0x%x\n",
                    DEVICE_DATA_TO_DISK_LIMIT_IN_MB, DriverContext.ulDataToDiskLimitPerDevInMB));
            }
            else {
                InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                    ("LoadDeviceCfgFromRegistry: Read Value %S Value = %#x\n",
                    DEVICE_DATA_TO_DISK_LIMIT_IN_MB, ulValue));
            }
        }
        else {
            InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT,
                ("LoadDeviceCfgFromRegistry: Failed to Read Value %S with error = %#x, initialized to default value %#x\n",
                DEVICE_DATA_TO_DISK_LIMIT_IN_MB, Status, ulValue));
        }

        if (0 != ulValue) {
            pDevContext->ullcbDataToDiskLimit = ulValue;
            pDevContext->ullcbDataToDiskLimit *= MEGABYTES;
            InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                ("LoadDeviceCfgFromRegistry: Read %S with Value 0x%x\n",
                DEVICE_DATA_TO_DISK_LIMIT_IN_MB, ulValue));
        }

        Status = pDevParam->ReadULONG(DEVICE_MIN_FREE_DATA_TO_DISK_LIMIT_IN_MB, (ULONG32 *)&ulValue,
            DriverContext.ulMinFreeDataToDiskLimitPerDevInMB, TRUE);
        if (NT_SUCCESS(Status)) {
            if (STATUS_INMAGE_OBJECT_CREATED == Status) {
                InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                    ("LoadDeviceCfgFromRegistry: Created %S with Default Value 0x%x\n",
                    DEVICE_MIN_FREE_DATA_TO_DISK_LIMIT_IN_MB, DriverContext.ulMinFreeDataToDiskLimitPerDevInMB));
            }
            else {
                InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                    ("LoadDeviceCfgFromRegistry: Read Value %S Value = %#x\n",
                    DEVICE_MIN_FREE_DATA_TO_DISK_LIMIT_IN_MB, ulValue));
            }
        }
        else {
            InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT,
                ("LoadDeviceCfgFromRegistry: Failed to Read Value %S with error = %#x, initialized to default value %#x\n",
                DEVICE_MIN_FREE_DATA_TO_DISK_LIMIT_IN_MB, Status, ulValue));
        }

        if (0 != ulValue) {
            if ((ulValue * MEGABYTES) > pDevContext->ullcbDataToDiskLimit){
                pDevContext->ullcbMinFreeDataToDiskLimit = (ULONG)pDevContext->ullcbDataToDiskLimit;
            }
            else {
                pDevContext->ullcbMinFreeDataToDiskLimit = ulValue;
                pDevContext->ullcbMinFreeDataToDiskLimit *= MEGABYTES;
                InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                    ("LoadDeviceCfgFromRegistry: Read %S with Value 0x%x\n",
                    DEVICE_MIN_FREE_DATA_TO_DISK_LIMIT_IN_MB, ulValue));
            }
        }

        Status = pDevParam->ReadULONG(DEVICE_DATA_OS_MAJOR_VER, (ULONG32 *)&ulValue,
            DriverContext.osMajorVersion, TRUE);

        if (!NT_SUCCESS(Status)) {

            InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_READ_DEVICE_REGISTRY,
                pDevContext,
                DEVICE_DATA_OS_MAJOR_VER,
                Status);
            pDevContext->ulOldOsMajorVersion = DriverContext.osMajorVersion;
        }
        else {
            pDevContext->ulOldOsMajorVersion = ulValue;
        }

        Status = pDevParam->ReadULONG(DEVICE_DATA_OS_MINOR_VER, (ULONG32 *)&ulValue,
            DriverContext.osMinorVersion, TRUE);
        if (!NT_SUCCESS(Status)) {
            InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_READ_DEVICE_REGISTRY,
                pDevContext,
                DEVICE_DATA_OS_MINOR_VER,
                Status);

            pDevContext->ulOldOsMinorVersion = DriverContext.osMinorVersion;
        }
        else {
            pDevContext->ulOldOsMinorVersion = ulValue;
        }

        Status = pDevParam->ReadULONG(DEVICE_DATA_NOTIFY_LIMIT_IN_KB, (ULONG32 *)&ulValue,
            DriverContext.ulDataNotifyThresholdInKB, TRUE);
        if (NT_SUCCESS(Status)) {
            if (STATUS_INMAGE_OBJECT_CREATED == Status) {
                InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                    ("LoadDeviceCfgFromRegistry: Created %S with Default Value 0x%x\n",
                    DEVICE_DATA_NOTIFY_LIMIT_IN_KB, DriverContext.ulDataNotifyThresholdInKB));
            }
            else {
                InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                    ("LoadDeviceCfgFromRegistry: Read Value %S Value = %#x\n",
                    DEVICE_DATA_NOTIFY_LIMIT_IN_KB, ulValue));
            }
        }
        else {
            InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT,
                ("LoadDeviceCfgFromRegistry: Failed to Read Value %S with error = %#x, initialized to default value %#x\n",
                DEVICE_DATA_NOTIFY_LIMIT_IN_KB, Status, ulValue));
        }

        if (0 != ulValue) {
            pDevContext->ulcbDataNotifyThreshold = ulValue;
            pDevContext->ulcbDataNotifyThreshold *= KILOBYTES;
            InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                ("LoadDeviceCfgFromRegistry: Read %S with Value 0x%x\n",
                DEVICE_DATA_NOTIFY_LIMIT_IN_KB, ulValue));
        }

        Status = pDevParam->ReadULONG(DEVICE_FILE_WRITER_THREAD_PRIORITY, (ULONG32 *)&pDevContext->FileWriterThreadPriority,
            DriverContext.FileWriterThreadPriority, TRUE);
        if (NT_SUCCESS(Status)) {
            if (STATUS_INMAGE_OBJECT_CREATED == Status) {
                InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                    ("LoadDeviceCfgFromRegistry: Created %S with Default Value 0x%x\n",
                    DEVICE_FILE_WRITER_THREAD_PRIORITY, DriverContext.FileWriterThreadPriority));
            }
            else {
                InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                    ("LoadDeviceCfgFromRegistry: Read Value %S Value = %#x\n",
                    DEVICE_FILE_WRITER_THREAD_PRIORITY, pDevContext->FileWriterThreadPriority));
            }
        }
        else {
            InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT,
                ("LoadDeviceCfgFromRegistry: Failed to Read Value %S with error = %#x, initialized to default value %#x\n",
                 DEVICE_FILE_WRITER_THREAD_PRIORITY, Status, pDevContext->FileWriterThreadPriority));
        }

        Status = pDevParam->ReadULONG(DEVICE_FILE_WRITER_ID, (ULONG32 *)&pDevContext->ulWriterId, 0, FALSE);
        if (NT_SUCCESS(Status)) {
            InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                ("LoadDeviceCfgFromRegistry: Read Value %S Value = %#x\n",
                DEVICE_FILE_WRITER_ID, pDevContext->ulWriterId));
        }
        else {
            InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                ("LoadDeviceCfgFromRegistry: Failed to find Value %S with error = %#x, initialized to default value %#x\n",
                DEVICE_FILE_WRITER_ID, Status, pDevContext->ulWriterId));
        }

        // Read the data file directory. We store data log files at this location.
        Status = pDevParam->ReadUnicodeString(DEVICE_DATAFILELOG_DIRECTORY, STRING_LEN_NULL_TERMINATED,
            &pDevContext->DataLogDirectory);

        InitializeDataLogDirectory(pDevContext, DriverContext.DataLogDirectory.Buffer);
        pDevContext->DevFileWriter = DriverContext.DataFileWriterManager->NewDev(pDevContext);
    }
    else {
        // Globally data filtering is disabled.
        InVolDbgPrint(DL_WARNING, DM_DEV_CONTEXT,
            ("LoadDeviceCfgFromRegistry: Using global override and setting data capture disabled for device %S(%S)\n",
            pDevContext->wDevID, pDevContext->wDevNum));
        pDevContext->ulFlags |= DCF_DATA_CAPTURE_DISABLED;
    }
#ifdef VOLUME_FLT
    Status = pDevParam->ReadULONG(VOLUME_PAGEFILE_WRITES_IGNORED, (ULONG32 *)&ulValue,
        VOLUME_PAGEFILE_WRITES_IGNORED_DEFAULT_VALUE, TRUE);
    if (NT_SUCCESS(Status)) {
        if (STATUS_INMAGE_OBJECT_CREATED == Status) {
            InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                ("LoadDeviceCfgFromRegistry: Created %S with Default Value 0x%x\n",
                VOLUME_PAGEFILE_WRITES_IGNORED, VOLUME_PAGEFILE_WRITES_IGNORED_DEFAULT_VALUE));
        }
        else {
            InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                ("LoadDeviceCfgFromRegistry: Read Value %S Value = 0x%x\n",
                VOLUME_PAGEFILE_WRITES_IGNORED, ulValue));
        }
    }
    else {
        InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT,
            ("LoadDeviceCfgFromRegistry: Failed to Read Value %S with error = 0x%x\n",
            VOLUME_PAGEFILE_WRITES_IGNORED, Status));
    }

    if (0 != ulValue) {
        pDevContext->ulFlags |= DCF_PAGEFILE_WRITES_IGNORED;
    }
    else {
        if (0 == (pDevContext->ulFlags & DCF_DATA_CAPTURE_DISABLED)) {
            // Page file writes are tracked and data filtering is enabled this will lead to recursion.
            // Settings Ignore page file writes.
            InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT,
                ("LoadDeviceCfgFromRegistry: Data capture enabled and page file writes are not ignored for Volume %S(%S).\n"
                "This will lead to recursion. Setting to ingore page file writes\n",
                pDevContext->wDevID, pDevContext->wDevNum));
            pDevContext->ulFlags |= DCF_PAGEFILE_WRITES_IGNORED;
            pDevParam->WriteULONG(VOLUME_PAGEFILE_WRITES_IGNORED, VOLUME_PAGEFILE_WRITES_IGNORED_SET);
        }
    }

    if (pDevContext->ulFlags & DCF_VOLUME_LETTER_OBTAINED) {
        WCHAR   DriveLetters[MAX_NUM_DRIVE_LETTERS + 1] = { 0 };

        Status = ConvertDriveLetterBitmapToDriveLetterString(&pDevContext->DriveLetterBitMap,
            DriveLetters,
            (MAX_NUM_DRIVE_LETTERS + 1)*sizeof(WCHAR));
        if (NT_SUCCESS(Status)) {
            Status = pDevParam->WriteWString(VOLUME_LETTER, DriveLetters);
            if (NT_SUCCESS(Status)) {
                InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT,
                    ("LoadDeviceCfgFromRegistry: Added DriveLetter(s) %S for GUID %S\n",
                    DriveLetters, pDevContext->wDevID));
            }
            else {
                InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT,
                    ("LoadDeviceCfgFromRegistry: Adding DriveLetter %S for GUID %S Failed with Status 0x%x\n",
                    DriveLetters, pDevContext->wDevID, Status));
            }
        }
        else {
            InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT,
                ("LoadVolumeCfgFromRegistry: ConvertDriveLetterBitmapToDriveLetterString for GUID %S Failed with Status 0x%x\n",
                pDevContext->wDevID, Status));
        }
    }
#endif
    delete pDevParam;

    return STATUS_SUCCESS;
}

VOID
SetDevCntFlag (
    __in    PDEV_CONTEXT pDevContext,
    __in    ULONG        ulFlags,
    __in    bool         bDCLockAcquired
    )
/*++

Routine Description:

    Set the Flag and Execute Post Operation

    This function must be called at IRQL <= DISPATCH_LEVEL

Arguments:

    pDevObject:- Target Object to set flag
    ulFlags:- Flags to sets
    bDCLockAcquired:- Caller Acquired the lock
--*/
{
    KIRQL                   OldIrql = 0;

    ASSERT(NULL != pDevContext);

    if (!bDCLockAcquired)
        KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);

    SET_FLAG(pDevContext->ulFlags, ulFlags);

    if (TEST_FLAG(ulFlags, DCF_DONT_PAGE_FAULT | 
                           DCF_PAGE_FILE_MISSED | 
                           DCF_EXPLICIT_NONWO_NODRAIN)) {  
        if (!TEST_FLAG(pDevContext->ulFlags, DCF_FILTERING_STOPPED)) {
            // Set to MetaDataMode Capture mode Explicitly
            VCSetCaptureModeToMetadata(pDevContext, false);
            // Set to MetaDataMode WO mode Explicitly
            VCSetWOState(pDevContext, false, __FUNCTION__, ecMDReasonExplicitByDriver);
            // DTAP Call IO Barrier
        }
    }
    if (TEST_FLAG(ulFlags, DCF_BITMAP_DEV_OFF)) {
        // Reset the BitmapDevObject.
        if (NULL != pDevContext->pBitmapDevObject) {
            pDevContext->pBitmapDevObject = NULL;
        }
    }
    if (!bDCLockAcquired)
        KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
    return;
}

bool
AddDeviceContextToList (
    __in    PDEV_CONTEXT pDevContext,
    __in    ULONG        ulExcludeAnySet,
    __in    ULONG        ulIncludeAllSet,
    __in    PLIST_ENTRY  pListHead,
    __in    bool         bDCLockAcquired,
    __in    bool         pPreShutdown
    )
/*++

Routine Description:

    Add the given Device Context to the list

    Exclude the devices with the given any flag set

    Include only devices with the given all flag set

    This function must be called at IRQL <= DISPATCH_LEVEL

Arguments:

    pDevObject:- Target Object to include in the list
    ulExcludeAnySet:- Exclude the devices with the given any flag set
    ulIncludeAllSet:- Include only devices with the given all flag set
    bDCLockAcquired:- Caller Acquired the lock

Return Value:

    Return True :- Inserted in the list

--*/
{
    KIRQL		            OldIrql	= 0;
    bool                    bRet = 0;
    PLIST_NODE              pListNode = NULL;

    ASSERT(NULL != pDevContext);
    ASSERT(NULL != pListHead);

    if (!bDCLockAcquired)
        KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);

    if (TEST_FLAG(pDevContext->ulFlags, ulExcludeAnySet)) {
        goto Cleanup;
    }

    if (!TEST_ALL_FLAGS(pDevContext->ulFlags, ulIncludeAllSet)) {
        goto Cleanup;
    }

    pListNode = AllocateListNode();
    if (NULL == pListNode) {

        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_NO_MEMORY_VERBOSE, pDevContext, sizeof(LIST_NODE));
        goto Cleanup;
    }
    pListNode->pData = (PVOID)pDevContext;
    //Dereference the DevContext on removal from the list
    ReferenceDevContext(pDevContext);
    InsertTailList(pListHead, &pListNode->ListEntry);
    if (pPreShutdown) {
        ++DriverContext.PreShutdownDiskCount;
    }
    bRet = 1;

Cleanup:
    if (!bDCLockAcquired)
       	KeReleaseSpinLock(&pDevContext->Lock, OldIrql);

    return bRet;
}

tagLogRecoveryState
CheckResyncDetected (
    __in    PDEV_CONTEXT pDevContext,
    __in    bool         bDCLockAcquired
    )
/*++

Routine Description:
    Check Peding changes and resync and set appropiate error message

Arguments:

    pDevObject:- Target Object to check for resync and pending changes

    bDCLockAcquired:- Caller Acquired the lock

Return Value:

    tagLogRecoveryState :- Code to be written to bitmap

--*/
{
    KIRQL                   OldIrql = 0;
    tagLogRecoveryState     recoveryState = dirtyShutdown;

    if (!bDCLockAcquired)
        KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);

    if (pDevContext->ChangeList.ulTotalChangesPending ||
        pDevContext->ulChangesPendingCommit) {
        recoveryState = pendingChanges;
        goto Cleanup;
    }

    if (pDevContext->bResyncRequired) {
        recoveryState = resyncMarked;
        goto Cleanup;
    }

    recoveryState = cleanShutdown;

Cleanup:
    if (!bDCLockAcquired)
        KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
    return recoveryState;
}

NTSTATUS
ChangeModeToRawBitmap (
    _In_    PDEV_CONTEXT pDevContext,
    _In_    LONG         *OutOfSyncErrorCode
    )
/*++

Routine Description:

    Change the Bitmap Mode to Raw

    This function must be called at IRQL < APC_LEVEL

Arguments:

    pDevObject:- Target Object to set the info
    
Return Value:

    NTSTATUS

--*/
{
    NTSTATUS	            Status = STATUS_UNSUCCESSFUL;

    if ((NULL == pDevContext->pDevBitmap) ||
        (NULL == pDevContext->pDevBitmap->pBitmapAPI)) {
        goto Cleanup;
    }
    Status = pDevContext->pDevBitmap->pBitmapAPI->ChangeBitmapModeToRawIO(OutOfSyncErrorCode);
    if (!NT_SUCCESS(Status)) {
        goto Cleanup;
    }
    // Store the BitmapDevObj to track PNP/Power operation
    pDevContext->pBitmapDevObject = pDevContext->pDevBitmap->pBitmapAPI->GetBitmapDevObj();
    if (NULL == pDevContext->pBitmapDevObject) {
        Status = STATUS_INVALID_DEVICE_STATE;
        *OutOfSyncErrorCode = MSG_INDSKFLT_ERROR_BITMAP_DEVOBJ_NOT_FOUND_Message;
        InDskFltWriteEventWithDevCtxt(INDSKFLT_WARN_BITMAP_DEVOBJ_NOT_FOUND, pDevContext, pDevContext->ulFlags);
    }


Cleanup:
    return Status;
}

PPAGE_FILE_NODE
GetPageFileNodeFromGlobalList (
    __in    bool bPFLockAcquired
    )
/*++

Routine Description:

    Get Page file Node from Global List to store Page file details

    This function must be called at IRQL <= DISPATCH_LEVEL

Arguments:

    bPFLockAcquired:- Caller Acquired the lock

Return Value:

    PPAGE_FILE_NODE

--*/
{
    PLIST_ENTRY             Entry = NULL;
    PPAGE_FILE_NODE         pPageFileNode = NULL;
    KIRQL                   OldIrql = 0;

    if (!bPFLockAcquired)
        KeAcquireSpinLock(&DriverContext.PageFileListLock, &OldIrql);

    if (IsListEmpty(&DriverContext.FreePageFileList)) {
        goto Cleanup;
    }

    DriverContext.ulNumOfFreePageNodes--;
    Entry = RemoveHeadList(&DriverContext.FreePageFileList);
    pPageFileNode = (PPAGE_FILE_NODE)CONTAINING_RECORD((PCHAR)Entry, PAGE_FILE_NODE, ListEntry);
    RtlZeroMemory(pPageFileNode, sizeof(PAGE_FILE_NODE));
    InterlockedIncrement(&pPageFileNode->lRefCount);

Cleanup:
    if (!bPFLockAcquired)
        KeReleaseSpinLock(&DriverContext.PageFileListLock, OldIrql);
    return pPageFileNode;
}

void 
FreePageFileNodeToGlobalList (
    __in    PPAGE_FILE_NODE pPageFileNode,
    __in    bool            bPFLockAcquired
    )
/*++

Routine Description:

    Free the Page file to Global List

    This function must be called at IRQL <= DISPATCH_LEVEL

Arguments:
    pPageFileNode:- Object to be freed
    bPFLockAcquired:- Caller Acquired the lock

--*/
{
    KIRQL OldIrql = 0;

    if (!bPFLockAcquired)
        KeAcquireSpinLock(&DriverContext.PageFileListLock, &OldIrql);

    if (NULL == pPageFileNode) {
        goto Cleanup;
    }
    InitializeListHead(&pPageFileNode->ListEntry);
    InsertTailList(&DriverContext.FreePageFileList, &pPageFileNode->ListEntry);
    DriverContext.ulNumOfFreePageNodes++;

Cleanup:
    if (!bPFLockAcquired)
        KeReleaseSpinLock(&DriverContext.PageFileListLock, OldIrql);
    return;
}

PPAGE_FILE_NODE 
FindPageFileNodeInDevCnt (
    __in    PDEV_CONTEXT pDevContext,
    __in    PFILE_OBJECT pFileObject,
    __in    bool         bDCLockAcquired
    )
/*++

Routine Description:

    Find the Page file in a DevContext

    This function must be called at IRQL <= DISPATCH_LEVEL

Arguments:
    pDevContext:- Target Object to search for Page file node
    pFileObject:- File Object to be searched
    bDCLockAcquired:- Caller Acquired the lock

Return Value:

    PPAGE_FILE_NODE

--*/
{
    PPAGE_FILE_NODE         pPageFileNode = NULL;
    PLIST_ENTRY             Entry = NULL;
    KIRQL                   OldIrql = 0;

    ASSERT(NULL != pDevContext);
    ASSERT(NULL != pFileObject);

    if (!bDCLockAcquired)
        KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);

    Entry = pDevContext->PageFileList.Flink;
    while(Entry != &pDevContext->PageFileList){
        pPageFileNode = (PPAGE_FILE_NODE)CONTAINING_RECORD((PCHAR)Entry, PAGE_FILE_NODE, ListEntry);
        if (pFileObject == pPageFileNode->pFileObject) {
            break;
        }
        Entry = Entry->Flink;
        pPageFileNode = NULL;
	}

    if (!bDCLockAcquired)
        KeReleaseSpinLock(&pDevContext->Lock, OldIrql);

    return pPageFileNode;
}

PPAGE_FILE_NODE
ReferencePageFileNode (
    __in    PPAGE_FILE_NODE pPageFileNode
    )
/*++

Routine Description:

    Increase the reference for given PagefileNode

    This function must be called at IRQL <= DISPATCH_LEVEL

--*/
{
    ASSERT(pPageFileNode->lRefCount >= 1);

    InterlockedIncrement(&pPageFileNode->lRefCount);

    return pPageFileNode;
}

bool
DereferencePageFileNode (
    __in    PPAGE_FILE_NODE   pPageFileNode
    )
/*++

Routine Description:

    Decrease the reference for given PagefileNode

    Free the Pagefile node in case there are no references and retrun true

    This function must be called at IRQL <= DISPATCH_LEVEL

--*/
{
    bool                    bRet = 0;
    
    ASSERT(pPageFileNode->lRefCount > 0);
    if (InterlockedDecrement(&pPageFileNode->lRefCount) <= 0) {
        FreePageFileNodeToGlobalList(pPageFileNode, 0);
        bRet = 1;
    }
    return bRet;
}

void
RemovePageFileObject (
    __in    PDEV_CONTEXT pDevContext,
    __in    PFILE_OBJECT pFileObject
    )
/*++

Routine Description:

    Dereference Page file from the Device Context List

    This function must be called at IRQL <= DISPATCH_LEVEL

Arguments:

    pDevObject:- Target Object to get Device Page File list
    pFileObject:- File Object to be removed

--*/
{
    PPAGE_FILE_NODE         pPageFileNode = NULL;
    KIRQL                   OldIrql = 0;
    bool                    bLock = 0;

    ASSERT(NULL != pDevContext);
    if (NULL == pFileObject) {
        goto Cleanup;
    }
    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
    bLock = 1;
    pPageFileNode = FindPageFileNodeInDevCnt(pDevContext, pFileObject, 1);
    if (NULL == pPageFileNode) {
        goto Cleanup;
    }
    if (DereferencePageFileNode(pPageFileNode)) {
        //Decrement the counter if page file node is deleted
        pDevContext->ulNumOfPageFiles--;
    }
Cleanup:
    if (bLock)
        KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
    return;
}

void
AddPageFileObject (
    __in    PDEV_CONTEXT pDevContext,
    __in    PFILE_OBJECT pFileObject
    )
/*++

Routine Description:

    Reference Page file from the Device Context List

    This function must be called at IRQL <= DISPATCH_LEVEL

Arguments:

    pDevObject:- Target Object to get Device Page File list
    pFileObject:- File Object to be reference

--*/
{
    PPAGE_FILE_NODE         pPageFileNode = NULL;
    KIRQL                   OldIrql = 0;
    bool                    bLock = 0;

    ASSERT(NULL != pDevContext);
    if (NULL == pFileObject) {
        goto Cleanup;
    }
    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
    bLock = 1;
    pPageFileNode = FindPageFileNodeInDevCnt(pDevContext, pFileObject, bLock);
    if (NULL != pPageFileNode) {
        ReferencePageFileNode(pPageFileNode);
        goto Cleanup;
    }
    pPageFileNode = GetPageFileNodeFromGlobalList(0);
    if (NULL == pPageFileNode) {
        SetDevCntFlag(pDevContext, DCF_PAGE_FILE_MISSED, bLock);
#ifndef VOLUME_FLT
        InDskFltWriteEvent(INDSKFLT_ERROR_PAGE_FILE_MISSED,
                          DriverContext.ulNumOfAllocPageNodes,
                          DriverContext.ulNumOfFreePageNodes);
#endif
        goto Cleanup;
    }
    pPageFileNode->pDevContext = pDevContext;
    pPageFileNode->pFileObject = pFileObject;
    InsertTailList(&pDevContext->PageFileList, &pPageFileNode->ListEntry);
    pDevContext->ulNumOfPageFiles++;

Cleanup:
    if (bLock)
        KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
    return;
}

void
FreeAllPageFileNodeToGlobalList (
    __in    PDEV_CONTEXT pDevContext,
    __in    bool         bDCLockAcquired
    )
/*++

Routine Description:

    Free all Page File Node in given Device Context

    This function must be called at IRQL <= DISPATCH_LEVEL

Arguments:

    pDevObject:- Target Object to get free Page File list
    pFileObject:- File Object to be reference

--*/
{
    PLIST_ENTRY             Entry = NULL;
    PPAGE_FILE_NODE         pPageFileNode = NULL;
    KIRQL                   OldIrql = 0;

    ASSERT(NULL != pDevContext);
    if (!bDCLockAcquired)
        KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);

    KeAcquireSpinLockAtDpcLevel(&DriverContext.PageFileListLock);
    while (!IsListEmpty(&pDevContext->PageFileList)) {
        Entry = RemoveHeadList(&pDevContext->PageFileList);
        pPageFileNode = (PPAGE_FILE_NODE)CONTAINING_RECORD((PCHAR)Entry,
                                                           PAGE_FILE_NODE,
                                                           ListEntry);
        FreePageFileNodeToGlobalList(pPageFileNode, 1);
        pDevContext->ulNumOfPageFiles--;
    }
	KeReleaseSpinLockFromDpcLevel(&DriverContext.PageFileListLock);
    if (!bDCLockAcquired)
        KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
    return;
}

void
RemoveDeviceContextFromGlobalList(
    __in    PDEV_CONTEXT    pDevContext
)
/*++

Routine Description:

    Remove Device Context from global list and decrement reference count

    This function must be called at IRQL <= DISPATCH_LEVEL

Arguments:

    DevContext - To be removed

--*/
{
    KIRQL OldIrql = 0;

    ASSERT(NULL != pDevContext);
    // Free of Page has to be post ReleaseLock as some writes are for Pagefile IO.
    // PageFile Device cannot be removed. It is cleanup function in scenario where pagefile is deleted
    FreeAllPageFileNodeToGlobalList(pDevContext, 0);
    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    RemoveEntryList((PLIST_ENTRY)pDevContext);
    DriverContext.ulNumDevs--;
    InitializeListHead((PLIST_ENTRY)pDevContext);
    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);
    DereferenceDevContext(pDevContext);
    return;
}