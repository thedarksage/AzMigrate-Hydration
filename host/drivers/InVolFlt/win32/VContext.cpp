#include "Common.h"
#include "VContext.h"
#include "TagNode.h"
#include "global.h"
#include "MntMgr.h"
#include "VBitmap.h"
#include "misc.h"
#include "HelperFunctions.h"
#include "svdparse.h"


/*-----------------------------------------------------------------------------
 *
 * File Name - VContext.cpp
 * This file contains all the routines to handle Volume Context
 * abbrevated as VC
 *
 *-----------------------------------------------------------------------------
 */

VOID
SetDBallocFailureError(PVOLUME_CONTEXT VolumeContext)
{
    LARGE_INTEGER   liTickCount, liTickIncrementPerSec;
    KIRQL           OldIrql;
    bool            bLog = false;

    KeAcquireSpinLock(&DriverContext.NoMemoryLogEventLock, &OldIrql);

    liTickCount = KeQueryPerformanceCounter(&liTickIncrementPerSec);
    DriverContext.ulNumMemoryAllocFailures++;
    VolumeContext->ulNumMemoryAllocFailures++;

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
        InMageFltLogError(VolumeContext->FilterDeviceObject,__LINE__, INVOLFLT_ERR_NO_NPAGED_POOL_FOR_DIRTYBLOCKS, STATUS_NO_MEMORY, 
            VolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES, VolumeContext->wGUID, GUID_SIZE_IN_BYTES);

    }

    KeReleaseSpinLock(&DriverContext.NoMemoryLogEventLock, OldIrql);

    return;
}

VOID
SetBitmapOpenFailDueToLossOfChanges(PVOLUME_CONTEXT VolumeContext, bool bLockAcquired)
{
    LARGE_INTEGER   liTickCount, liTickIncrementPerSec, liTickDiff;
    ULONG           ulSecsFromFirstOpenFail;
    KIRQL           OldIrql;

    liTickCount = KeQueryPerformanceCounter(&liTickIncrementPerSec);
    liTickDiff.QuadPart = VolumeContext->liLastBitmapOpenErrorAtTickCount.QuadPart - VolumeContext->liFirstBitmapOpenErrorAtTickCount.QuadPart;
    ulSecsFromFirstOpenFail = (ULONG)(liTickDiff.QuadPart/liTickIncrementPerSec.QuadPart);

    if (FALSE == bLockAcquired)
        KeAcquireSpinLock(&VolumeContext->Lock, &OldIrql);

    // As we are loosing changes, we should set resync required
    // and also disable filterin temporarily for this volume

    VolumeContext->ulFlags |= VCF_OPEN_BITMAP_FAILED;
    StopFilteringDevice(VolumeContext, true, false, false);

    InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, 
        ("SetBitmapOpenFailDueToLossOfChanges: Stopping filtering on volume %c: due to loss of changes.\n%d bitmap open errors and %d secs time diff between first and curr failure\n",
            (UCHAR)VolumeContext->DriveLetter[0], VolumeContext->lNumBitmapOpenErrors, ulSecsFromFirstOpenFail));

    if (FALSE == bLockAcquired)
        KeReleaseSpinLock(&VolumeContext->Lock, OldIrql);

    SetVolumeOutOfSync(VolumeContext, ERROR_TO_REG_BITMAP_OPEN_FAIL_CHANGES_LOST, STATUS_SUCCESS, bLockAcquired);

    return;
}

VOID
SetBitmapOpenError(PVOLUME_CONTEXT  VolumeContext, bool bLockAcquired, NTSTATUS Status)
{
    LARGE_INTEGER   liTickCount, liTickIncrementPerSec, liTickDiff;
    ULONG           ulSecsFromFirstOpenFail = 0;
    BOOLEAN         bOutOfSync = FALSE;
    BOOLEAN         bFailFurtherOpens = FALSE;
    KIRQL           OldIrql;

    InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, 
        ("SetBitmapOpenError: On volume %c: due to %d bitmap open errors till now.\n",
            (UCHAR)VolumeContext->DriveLetter[0], VolumeContext->lNumBitmapOpenErrors));

    liTickCount = KeQueryPerformanceCounter(&liTickIncrementPerSec);

    if (!bLockAcquired)
        KeAcquireSpinLock(&VolumeContext->Lock, &OldIrql);

    VolumeContext->lNumBitmapOpenErrors++;
    if (0 == VolumeContext->liFirstBitmapOpenErrorAtTickCount.QuadPart) {
        VolumeContext->liFirstBitmapOpenErrorAtTickCount = liTickCount;
        InMageFltLogError(VolumeContext->FilterDeviceObject,__LINE__, INVOLFLT_WARNING_FIRST_FAILURE_TO_OPEN_BITMAP, STATUS_SUCCESS, 
            VolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES, VolumeContext->wGUID, GUID_SIZE_IN_BYTES);
        InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, 
            ("SetBitmapOpenError: On volume %c: first failure\n", (UCHAR)VolumeContext->DriveLetter[0]));                    
    } else {
        liTickDiff.QuadPart = liTickCount.QuadPart - VolumeContext->liFirstBitmapOpenErrorAtTickCount.QuadPart;
        ulSecsFromFirstOpenFail = (ULONG)(liTickDiff.QuadPart/liTickIncrementPerSec.QuadPart);
        InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, 
            ("SetBitmapOpenError: On volume %c: %d secs time diff between first and curr failure\n",
                (UCHAR)VolumeContext->DriveLetter[0], ulSecsFromFirstOpenFail));                    
        if (( ulSecsFromFirstOpenFail > MAX_DELAY_FOR_BITMAP_FILE_OPEN_IN_SECONDS) &&
            (VolumeContext->lNumBitmapOpenErrors > MAX_BITMAP_OPEN_ERRORS_TO_STOP_FILTERING))
        {
            bFailFurtherOpens = TRUE;
        }
    }

    VolumeContext->liLastBitmapOpenErrorAtTickCount = liTickCount;

    if (bFailFurtherOpens) {
        // As we are loosing changes, we should set resync required
        // and also disable filterin temporarily for this volume

        VolumeContext->ulFlags |= VCF_OPEN_BITMAP_FAILED;
        bOutOfSync = TRUE;
        StopFilteringDevice(VolumeContext, true, false, false);
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, 
            ("SetBitmapOpenError: Stopping filtering on volume %c: due to %d bitmap open errors and %d secs time diff between first and curr failure\n",
                (UCHAR)VolumeContext->DriveLetter[0], VolumeContext->lNumBitmapOpenErrors, ulSecsFromFirstOpenFail));                    
    }

    if (!bLockAcquired)
        KeReleaseSpinLock(&VolumeContext->Lock, OldIrql);

    if (TRUE == bOutOfSync)
        SetVolumeOutOfSync(VolumeContext, ERROR_TO_REG_BITMAP_OPEN_ERROR, Status, bLockAcquired);

    return;
}

BOOLEAN
CanOpenBitmapFile(PVOLUME_CONTEXT   VolumeContext, BOOLEAN bLoseChanges)
{
    LARGE_INTEGER   liTickCount, liTickIncrementPerSec;
    ULONG           ulSecsFromLastOpenFail, ulSecsDelayForNextOpen;

    InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, 
        ("CanOpenBitmapFile: Called on volume %c: VolumeContext %#p, bLoseChanges = %d\n", 
            (UCHAR)VolumeContext->DriveLetter[0], VolumeContext, bLoseChanges));                    

    if (VolumeContext->ulFlags & (VCF_FILTERING_STOPPED | VCF_OPEN_BITMAP_FAILED)) {
        InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, 
            ("CanOpenBitmapFile: Returning FALSE for volume %c: Volume is either stopped for open failed. ulFlags = %#x\n", 
                (UCHAR)VolumeContext->DriveLetter[0], VolumeContext->ulFlags));                    
        return FALSE;
    }

    if ((TRUE == bLoseChanges) || (0 == VolumeContext->lNumBitmapOpenErrors)) {
        InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, 
            ("CanOpenBitmapFile: Returning TRUE for volume %c: bLoseChanges(%d) is TRUE or lNumBitmapOpenErrors(%d) is zero\n", 
                (UCHAR)VolumeContext->DriveLetter[0], bLoseChanges, VolumeContext->lNumBitmapOpenErrors));                    
        return TRUE;
    }

    liTickCount = KeQueryPerformanceCounter(&liTickIncrementPerSec);
    ulSecsFromLastOpenFail =  (ULONG) ((liTickCount.QuadPart - VolumeContext->liLastBitmapOpenErrorAtTickCount.QuadPart) / liTickIncrementPerSec.QuadPart);
    if (ulSecsFromLastOpenFail > 0) {
        ulSecsDelayForNextOpen = MIN_DELAY_FOR_BIMTAP_FILE_OPEN_IN_SECONDS * (1 << VolumeContext->lNumBitmapOpenErrors);
        InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, 
            ("CanOpenBitmapFile: On volume %c: ulSecsFromLastOpenFail(%d), ulSecsDelayForNextOpen(%d) lNumBitmapOpenErrors(%d)\n", 
                (UCHAR)VolumeContext->DriveLetter[0], ulSecsFromLastOpenFail, ulSecsDelayForNextOpen, VolumeContext->lNumBitmapOpenErrors));                    
        if (ulSecsFromLastOpenFail >= ulSecsDelayForNextOpen) {
            InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, ("CanOpenBitmapFile: Returning TRUE for volume %c:\n", (UCHAR)VolumeContext->DriveLetter[0]));
            return TRUE;
        }
    } else {
        InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, 
            ("CanOpenBitmapFile: On volume %c: ulSecsFromLastOpenFail(%d) is TRUE or lNumBitmapOpenErrors(%d) is zero\n", 
                (UCHAR)VolumeContext->DriveLetter[0], ulSecsFromLastOpenFail, VolumeContext->lNumBitmapOpenErrors));
    }

    InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, ("CanOpenBitmapFile: Returning FALSE for volume %c:\n", (UCHAR)VolumeContext->DriveLetter[0]));                    
    return FALSE;
}

VOID
LogBitmapOpenSuccessEvent(PVOLUME_CONTEXT VolumeContext)
{
    LARGE_INTEGER   liTickCount, liTickIncrementPerSec, liTickDiff;
    ULONG           ulSecsFromFirstOpenFail = 0;
    WCHAR           BufferForRetries[20];
    WCHAR           BufferForSeconds[20];
    NTSTATUS        Status;

    InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, 
        ("LogBitmapOpenSucessEvent: Called on volume %S(%S): BitmapOpenErrors %d\n", 
            VolumeContext->wDriveLetter, VolumeContext->wGUID, VolumeContext->lNumBitmapOpenErrors));

    if (0 == VolumeContext->lNumBitmapOpenErrors) {
        return;
    }

    ASSERT(0 != VolumeContext->liFirstBitmapOpenErrorAtTickCount.QuadPart);

    liTickCount = KeQueryPerformanceCounter(&liTickIncrementPerSec);
    liTickDiff.QuadPart = liTickCount.QuadPart - VolumeContext->liFirstBitmapOpenErrorAtTickCount.QuadPart;
    ulSecsFromFirstOpenFail = (ULONG)(liTickDiff.QuadPart/liTickIncrementPerSec.QuadPart);
    InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, 
        ("LogBitmapOpenSucessEvent: On volume %S(%S): %d secs time diff between first and curr failure\n",
            VolumeContext->wDriveLetter, VolumeContext->wGUID, ulSecsFromFirstOpenFail));
    Status = RtlStringCbPrintfExW(BufferForRetries, sizeof(BufferForRetries), NULL, NULL,
                                 STRSAFE_NULL_ON_FAILURE, L"%d", VolumeContext->lNumBitmapOpenErrors);
    ASSERT(STATUS_SUCCESS == Status);
    Status = RtlStringCbPrintfExW(BufferForSeconds, sizeof(BufferForSeconds), NULL, NULL,
                                 STRSAFE_NULL_ON_FAILURE, L"%d", ulSecsFromFirstOpenFail);
    InMageFltLogError(VolumeContext->FilterDeviceObject,__LINE__, INVOLFLT_SUCCESS_OPEN_BITMAP_AFTER_RETRY, STATUS_SUCCESS,
        VolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES, VolumeContext->wGUID, GUID_SIZE_IN_BYTES,
                      BufferForRetries, sizeof(BufferForRetries), BufferForSeconds, sizeof(BufferForSeconds));

    return;
}

/*-----------------------------------------------------------------------------
 * Functions related to DataBlocks and DataFiltering
 *----------------------------------------------------------------------------0
 */

VOID
VCAdjustCountersToUndoReserve(
    PVOLUME_CONTEXT VolumeContext,
    ULONG   ulBytes
    )
{
    if (VolumeContext->ullcbDataAlocMissed) {
        if (VolumeContext->ullcbDataAlocMissed >= ulBytes) {
            VolumeContext->ullcbDataAlocMissed -= ulBytes;
            ulBytes = 0;
        } else {
            ulBytes -= (ULONG)VolumeContext->ullcbDataAlocMissed;
            VolumeContext->ullcbDataAlocMissed = 0;
        }
    }

    if (ulBytes) {
        ASSERT(VolumeContext->ulcbDataReserved >= ulBytes);
        if (VolumeContext->ulcbDataReserved >= ulBytes) {
            VolumeContext->ulcbDataFree += ulBytes;
            VolumeContext->ulcbDataReserved -= ulBytes;
        } else {
             VolumeContext->ulcbDataFree -= VolumeContext->ulcbDataReserved;
             VolumeContext->ulcbDataReserved = 0;
        }
    }

    return;
}

VOID
VCAdjustCountersForLostDataBytes(
    PVOLUME_CONTEXT VolumeContext,
    PDATA_BLOCK *ppDataBlock,
    ULONG ulBytesLost,
    bool bAllocate,
    bool bCanLock
    )
{
    InVolDbgPrint(DL_FUNC_TRACE, DM_DATA_FILTERING, ("VCAdjustCountersForLostDataBytes: %s(%s) ulBytesLost = %#x\n",
                                                     VolumeContext->GUIDinANSI, VolumeContext->DriveLetter, ulBytesLost));
    InVolDbgPrint(DL_TRACE_L1, DM_DATA_FILTERING, 
                  ("VCAdjustCountersForLostDataBytes: (Before) Alloced = %#x Missed = %#I64x Free = %#x Reserved = %#x\n",
                   VolumeContext->ulcbDataAllocated, VolumeContext->ullcbDataAlocMissed,
                   VolumeContext->ulcbDataFree, VolumeContext->ulcbDataReserved));
    if (0 == ulBytesLost) {
        return;
    }

    if (ulBytesLost <= VolumeContext->ulcbDataFree) {
        VolumeContext->ulcbDataFree -= ulBytesLost;
        ulBytesLost = 0;
        InVolDbgPrint(DL_TRACE_L1, DM_DATA_FILTERING, 
                      ("VCAdjustCountersForLostDataBytes: (After) Alloced = %#x Missed = %#I64x Free = %#x Reserved = %#x\n",
                       VolumeContext->ulcbDataAllocated, VolumeContext->ullcbDataAlocMissed,
                       VolumeContext->ulcbDataFree, VolumeContext->ulcbDataReserved));
        return;
    }

    if (0 == VolumeContext->ullcbDataAlocMissed) {
        // Add data blocks if and only if we did not miss any data.
        if ((NULL != ppDataBlock) && (NULL != *ppDataBlock)) {
            VCAddToReservedDataBlockList(VolumeContext, *ppDataBlock);
            VolumeContext->ulcbDataFree += DriverContext.ulDataBlockSize;
            *ppDataBlock = NULL;
        } else if (bAllocate) {
            PDATA_BLOCK DataBlock = AllocateLockedDataBlock(bCanLock);
            if (NULL != DataBlock) {
                VCAddToReservedDataBlockList(VolumeContext, DataBlock);
                VolumeContext->ulcbDataFree += DriverContext.ulDataBlockSize;
            }
        }
    }

    if (ulBytesLost <= VolumeContext->ulcbDataFree) {
        VolumeContext->ulcbDataFree -= ulBytesLost;
        ulBytesLost = 0;
        InVolDbgPrint(DL_TRACE_L1, DM_DATA_FILTERING, 
                      ("VCAdjustCountersForLostDataBytes: (After) Alloced = %#x Missed = %#I64x Free = %#x Reserved = %#x\n",
                       VolumeContext->ulcbDataAllocated, VolumeContext->ullcbDataAlocMissed,
                       VolumeContext->ulcbDataFree, VolumeContext->ulcbDataReserved));
        return;
    }

    ulBytesLost -= VolumeContext->ulcbDataFree;
    VolumeContext->ulcbDataFree = 0;

    ASSERT(VolumeContext->ulcbDataReserved >= ulBytesLost);
    if (VolumeContext->ulcbDataReserved >= ulBytesLost) {
        VolumeContext->ulcbDataReserved -= ulBytesLost;
    } else {
        VolumeContext->ulcbDataReserved = 0;
    }

    VolumeContext->ullcbDataAlocMissed += ulBytesLost;
    ulBytesLost = 0;
    ASSERT((VolumeContext->eCaptureMode != ecCaptureModeData) || 
           (VolumeContext->ullNetWritesSeenInBytes == VolumeContext->ullUnaccountedBytes + VolumeContext->ulcbDataReserved + VolumeContext->ullcbDataAlocMissed));

    InVolDbgPrint(DL_TRACE_L1, DM_DATA_FILTERING, 
                  ("VCAdjustCountersForLostDataBytes: (After) Alloced = %#x Missed = %#I64x Free = %#x Reserved = %#x\n",
                   VolumeContext->ulcbDataAllocated, VolumeContext->ullcbDataAlocMissed,
                   VolumeContext->ulcbDataFree, VolumeContext->ulcbDataReserved));

    return;
}

PDATA_BLOCK
VCGetReservedDataBlock(PVOLUME_CONTEXT VolumeContext)
{
    PDATA_BLOCK DataBlock = NULL;

    ASSERT(NULL != VolumeContext);

    if (!IsListEmpty(&VolumeContext->ReservedDataBlockList)) {
        ASSERT(VolumeContext->ulDataBlocksReserved > 0);
        DataBlock = (PDATA_BLOCK)RemoveHeadList(&VolumeContext->ReservedDataBlockList);
        VolumeContext->ulDataBlocksReserved--;
    }

    return DataBlock;
}

VOID
VCAddToReservedDataBlockList(PVOLUME_CONTEXT VolumeContext, PDATA_BLOCK DataBlock)
{
    InsertTailList(&VolumeContext->ReservedDataBlockList, &DataBlock->ListEntry);
    VolumeContext->ulcbDataAllocated += DriverContext.ulDataBlockSize;
    VolumeContext->ulDataBlocksReserved++;

    return;
}

VOID
VCFreeReservedDataBlockList(PVOLUME_CONTEXT VolumeContext)
{
    InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, ("VCFreeReservedDataBlockList: Called\n"));
    while(!IsListEmpty(&VolumeContext->ReservedDataBlockList)) {
        PDATA_BLOCK DataBlock = (PDATA_BLOCK)RemoveHeadList(&VolumeContext->ReservedDataBlockList);

        ASSERT(VolumeContext->ulDataBlocksReserved > 0);
        VolumeContext->ulDataBlocksReserved--;

        DeallocateDataBlock(DataBlock, true);
        ASSERT(VolumeContext->ulcbDataAllocated >= DriverContext.ulDataBlockSize);
        VolumeContext->ulcbDataAllocated -= DriverContext.ulDataBlockSize;
    }

    InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, ("VCFreeReservedDataBlockList: ulDataBlocksReserved = %#x, ulcbDataAllocated = %#x\n",
                                               VolumeContext->ulDataBlocksReserved, VolumeContext->ulcbDataAllocated));
    return;
}

VOID
VCFreeUsedDataBlockList(PVOLUME_CONTEXT VolumeContext, bool bVCLockAcquired)
{
    PCHANGE_NODE    ChangeNode = (PCHANGE_NODE)VolumeContext->ChangeList.Head.Flink;

    while ((PLIST_ENTRY)ChangeNode != &VolumeContext->ChangeList.Head) {
        if (ecChangeNodeDirtyBlock == ChangeNode->eChangeNode) {
            PKDIRTY_BLOCK   DirtyBlock = ChangeNode->DirtyBlock;

            if (INVOLFLT_DATA_SOURCE_DATA == DirtyBlock->ulDataSource) {
                DBFreeDataBlocks(DirtyBlock, bVCLockAcquired);
            }
        }

        ChangeNode = (PCHANGE_NODE)ChangeNode->ListEntry.Flink;
    }

    return;
}

VOID
VCFreeAllDataBuffers(PVOLUME_CONTEXT    VolumeContext, bool bVCLockAcquired)
{
    ASSERT(NULL != VolumeContext);

    VCFreeReservedDataBlockList(VolumeContext);
    VCFreeUsedDataBlockList(VolumeContext, bVCLockAcquired);
    VolumeContext->ulcbDataFree = 0;
    VolumeContext->ulcbDataReserved = 0;

    return;
}

NTSTATUS
AddUserDefinedTagsInDataMode(
    PKDIRTY_BLOCK   DirtyBlock,
    PUCHAR          pSequenceOfTags,
    PGUID           pTagGUID,
    ULONG           ulTagVolumeIndex
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
        DirtyBlock->ulTagVolumeIndex = ulTagVolumeIndex;
    }

    return STATUS_SUCCESS;
}

//NTSTATUS
//AddUserDefinedTagsInNonDataMode(
//    PVOLUME_CONTEXT VolumeContext,
//    PUCHAR          pSequenceOfTags,
//    PTIME_STAMP_TAG_V2 *ppTimeStamp,
//    PLARGE_INTEGER  pliTickCount,
//    PGUID           pTagGUID,
//    ULONG           ulTagVolumeIndex
//    )
//{
//    ASSERT(NULL != pSequenceOfTags);
//
//    ULONG   ulNumberOfTags = *(PULONG)pSequenceOfTags;
//    ULONG   ulMemNeeded = 0;
//    PUCHAR  pTags = pSequenceOfTags + sizeof(ULONG);
//
//    // This is validated in IsValidIoctlTagVolumeInputBuffer
//    ASSERT(0 != ulNumberOfTags);
//
//    ulMemNeeded = GetBufferSizeNeededForTags(pTags, ulNumberOfTags);
//
//    // This is validated in IsValidIoctlTagVolumeInputBuffer
//    ASSERT (ulMemNeeded <= DriverContext.ulMaxTagBufferSize);
//
//    PUCHAR TagBuffer = (PUCHAR)ExAllocatePoolWithTag(NonPagedPool, ulMemNeeded, TAG_TAG_BUFFER);
//    if (NULL == TagBuffer) {
//        return STATUS_NO_MEMORY;
//    }
//
//    PKDIRTY_BLOCK   DirtyBlock = AllocateDirtyBlock(VolumeContext, DriverContext.ulMaxDataSizePerNonDataModeDirtyBlock);
//    if (NULL == DirtyBlock) {
//        ExFreePoolWithTag(TagBuffer, TAG_TAG_BUFFER);
//        return STATUS_NO_MEMORY;
//    }
//
//    AddDirtyBlockToVolumeContext(VolumeContext, DirtyBlock, ppTimeStamp, pliTickCount, NULL);
//    DirtyBlock->TagBuffer = TagBuffer;
//    DirtyBlock->ulTagBufferSize = ulMemNeeded;
//    DirtyBlock->ulFlags |= KDIRTY_BLOCK_FLAG_CONTAINS_TAGS;
//    pTags = pSequenceOfTags + sizeof(ULONG);
//
//    for (ULONG ulIndex = 0; ulIndex < ulNumberOfTags; ulIndex++) {
//        USHORT  usTagDataLength = *(USHORT UNALIGNED *)pTags;
//        ULONG   ulTagLengthWoHeaderWoAlign = sizeof(USHORT) + usTagDataLength;
//        ULONG   ulTagLengthWoHeader = GET_ALIGNMENT_BOUNDARY(ulTagLengthWoHeaderWoAlign, sizeof(ULONG));
//        ULONG   ulTagHeaderLength = StreamHeaderSizeFromDataSize(ulTagLengthWoHeader);
//        ASSERT(ulTagLengthWoHeader >= ulTagLengthWoHeaderWoAlign);
//        ULONG   ulPadding = ulTagLengthWoHeader - ulTagLengthWoHeaderWoAlign;
//
//        //Copy Tag header
//        FILL_STREAM_HEADER(TagBuffer, STREAM_REC_TYPE_USER_DEFINED_TAG, ulTagHeaderLength + ulTagLengthWoHeader);
//        TagBuffer += ulTagHeaderLength;
//
//        //Copy Tag length + Tag data
//        TagBuffer += ulTagLengthWoHeaderWoAlign;
//
//        // Fill Padding
//        if (ulPadding) {
//            RtlZeroMemory(TagBuffer, ulPadding);
//        }
//        TagBuffer += ulPadding;
//        pTags += ulTagLengthWoHeaderWoAlign;
//    }
//
//    if (pTagGUID) {
//        ASSERT(!(DirtyBlock->ulFlags & KDIRTY_BLOCK_FLAG_TAG_WAITING_FOR_COMMIT));
//
//        DirtyBlock->ulFlags |= KDIRTY_BLOCK_FLAG_TAG_WAITING_FOR_COMMIT;
//        DirtyBlock->ulTagVolumeIndex = ulTagVolumeIndex;
//    }
//
//    // ASSERT we did not overrun allocated memory
//    ASSERT(TagBuffer == DirtyBlock->TagBuffer + DirtyBlock->ulTagBufferSize);
//
//    return STATUS_SUCCESS;
//}

NTSTATUS
AddUserDefinedTags(
    PVOLUME_CONTEXT VolumeContext,
    PUCHAR          SequenceOfTags,
    PTIME_STAMP_TAG_V2 *ppTimeStamp,
    PLARGE_INTEGER  pliTickCount,
    PGUID           pTagGUID,
    ULONG           ulTagVolumeIndex
    )
{
    NTSTATUS    Status = STATUS_SUCCESS;
#if DBG
    InMageFltLogError(VolumeContext->FilterDeviceObject, __LINE__, INVOLFLT_IOCTL_DISK_COPY_DATA_RECEIVED_TAGINFO, STATUS_SUCCESS,
        VolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES, VolumeContext->wGUID, GUID_SIZE_IN_BYTES,
        VolumeContext->IoctlDiskCopyDataCount, VolumeContext->IoctlDiskCopyDataCountSuccess);

    VolumeContext->IoctlDiskCopyDataCount = 0;
    VolumeContext->IoctlDiskCopyDataCountSuccess = 0;
    VolumeContext->IoctlDiskCopyDataCountFailure = 0;
#endif

    // Bug#23098 : With performance changes, capture mode could be data irrespective of WO state. As tag is not considered as a change, there could be a dirtyblock(non WOS) with just a tag
    // and subsequent dirty block may be a WOS which is partially filled. Draining prior dirtyblock as per the priority leading to OOD. So allowing tags only when WOS is data.

    if (ecWriteOrderStateData != VolumeContext->eWOState) {
        Status = STATUS_NOT_SUPPORTED;
        VolumeContext->ulTagsinNonWOSDrop++;
        return Status;
    }

    ULONG  ulRecordSize = SVRecordSizeForTags(SequenceOfTags);
    ULONG  ulBytesToReserve = ulRecordSize;
    ReserveDataBytes(VolumeContext, false, ulBytesToReserve, true, true, true);

    PKDIRTY_BLOCK   DirtyBlock = GetDirtyBlockToCopyChange(VolumeContext, ulRecordSize, true, ppTimeStamp, pliTickCount);
    if (NULL != DirtyBlock) {
        // We have space to add tags in stream format
        InVolDbgPrint(DL_ERROR, DM_TAGS, 
            ("AddUserDefinedTags: Number of TAGS are : %d \n", *(PULONG)SequenceOfTags)); 
        // Make a copy of tags pointer
        PUCHAR TempSeqTags = SequenceOfTags;
        Status = AddUserDefinedTagsInDataMode(DirtyBlock, SequenceOfTags, pTagGUID, ulTagVolumeIndex);

         if (NT_SUCCESS(Status))
            VolumeContext->ulTagsinWOState++;

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
            if (max_taglen == RtlCompareMemory(VolumeContext->LastTag, TempSeqTags, max_taglen)) {
                InVolDbgPrint( DL_ERROR, DM_INMAGE_IOCTL | DM_TAGS,
                    ("Same tag received in sequence IOCTL tag count : %ld, DataModeCount : %ld, NonDataModeCount : %ld \n",
                    DriverContext.ulNumberofAsyncTagIOCTLs, VolumeContext->ulTagsinWOState, VolumeContext->ulTagsinNonWOSDrop));

                InMageFltLogError(VolumeContext->FilterDeviceObject, __LINE__, INVOLFLT_LOG_SAME_TAG_IN_SEQ, STATUS_SUCCESS,
                                 VolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES,
                                 DriverContext.ulNumberofAsyncTagIOCTLs, VolumeContext->ulTagsinWOState, 
                                 VolumeContext->ulTagsinNonWOSDrop);
               ASSERT(max_taglen != RtlCompareMemory(VolumeContext->LastTag, TempSeqTags,max_taglen));
            }
            RtlZeroMemory(VolumeContext->LastTag, INMAGE_MAX_TAG_LENGTH);
            RtlMoveMemory(VolumeContext->LastTag, TempSeqTags, max_taglen);
        }
        if (VolumeContext->ulcbDataReserved > ulRecordSize) {
             VolumeContext->ulcbDataReserved -= ulRecordSize;
        } else {
             VolumeContext->ulcbDataReserved = 0;
        }
        if (VolumeContext->ullNetWritesSeenInBytes > ulRecordSize) {
             VolumeContext->ullNetWritesSeenInBytes -= ulRecordSize;
        } else {
             VolumeContext->ullNetWritesSeenInBytes = 0;
        }
    } else {
        VolumeContext->ullNetWritesSeenInBytes -= ulRecordSize;
        VCAdjustCountersToUndoReserve(VolumeContext, ulRecordSize);
        VCSetCaptureModeToMetadata(VolumeContext, false);
        VCSetWOState(VolumeContext,false,__FUNCTION__);
        Status = STATUS_NOT_SUPPORTED;
        VolumeContext->ulTagsinNonWOSDrop++;
    }

    // ReserveDataBytes with bFirst true increments ulNumPendingIrps, this has to be decremented.
    VolumeContext->ulNumPendingIrps--;
    return Status;
}

NTSTATUS
VCAddKernelTag(
    PVOLUME_CONTEXT VolumeContext,
    PVOID           pTag,
    USHORT          usTagLen,
    PDATA_BLOCK     *ppDataBlock
    )
{
    NTSTATUS        Status = STATUS_SUCCESS;
    PKDIRTY_BLOCK   DirtyBlock;

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

    Status = AddUserDefinedTags(VolumeContext, TagBuffer, NULL, NULL, NULL, 0);
    //Clanup of TagBuffer. KernelTag is disabled by default
    if (STATUS_NO_MEMORY == Status) {
        ExFreePoolWithTag(TagBuffer, TAG_TAG_BUFFER);
    }
    
    return Status;
}

bool
IsDataCaptureEnabledForThisVolume(PVOLUME_CONTEXT VolumeContext)
{
    if (!DriverContext.bServiceSupportsDataCapture) {
        return false;
    }

    if (!DriverContext.bEnableDataCapture) {
        return false;
    }

    if (VolumeContext->ulFlags & VCF_DATA_CAPTURE_DISABLED) {
        return false;
    }

    return true;
}

bool
IsDataFilesEnabledForThisVolume(PVOLUME_CONTEXT VolumeContext)
{
    if (!DriverContext.bServiceSupportsDataFiles) {
        return false;
    }

    if (!DriverContext.bEnableDataFiles) {
        return false;
    }

    if (VolumeContext->ulFlags & VCF_DATA_FILES_DISABLED) {
        return false;
    }

    return true;
}

bool
NeedWritingToDataFile(PVOLUME_CONTEXT VolumeContext)
{
    ULONG   ulDaBInMemory = VolumeContext->lDataBlocksInUse - VolumeContext->lDataBlocksQueuedToFileWrite;

    if (ecServiceShutdown == DriverContext.eServiceState) {
        return false;
    }

    if (!IsDataFilesEnabledForThisVolume(VolumeContext))
        return false;
        
    if (VolumeContext->eWOState != ecWriteOrderStateData)
        return false;        

    ULONG ulNumDaBPerDirtyBlock = VolumeContext->ulMaxDataSizePerDataModeDirtyBlock / DriverContext.ulDataBlockSize;
    if (ulDaBInMemory <= ulNumDaBPerDirtyBlock) {
        return false;
    }

    ULONG   ulFreeDataBlocks = DriverContext.ulDataBlocksInFreeDataBlockList + DriverContext.ulDataBlocksInLockedDataBlockList;
    ULONG   ulDaBFreeThresholdForFileWrite = GetFreeThresholdForFileWrite(VolumeContext);
    if (ulFreeDataBlocks <= ulDaBFreeThresholdForFileWrite) {
        InVolDbgPrint(DL_TRACE_L1, DM_DATA_FILTERING,
                      ("%s: FreeDaB = %#x, FreeLockedDaB = %#x, Thres = %#x\n", __FUNCTION__,
                       DriverContext.ulDataBlocksInFreeDataBlockList, DriverContext.ulDataBlocksInLockedDataBlockList,
                       ulDaBFreeThresholdForFileWrite));
        return true;
    }

    ULONG ulDaBThresholdPerVolumeForFileWrite = GetThresholdPerVolumeForFileWrite(VolumeContext);
    if (ulDaBInMemory > ulDaBThresholdPerVolumeForFileWrite) {    
        InVolDbgPrint(DL_TRACE_L1, DM_DATA_FILTERING,
                      ("%s: DaBInUse = %#x, DaBQueued = %#x, Thres = %#x\n", __FUNCTION__,
                       VolumeContext->lDataBlocksInUse, VolumeContext->lDataBlocksQueuedToFileWrite,
                       ulDaBThresholdPerVolumeForFileWrite));
        return true;
    }

    return false;
}

void
VCSetWOStateToMetaData(PVOLUME_CONTEXT VolumeContext, bool bServiceInitiated, etWOSChangeReason eWOSChangeReason)
{
    LARGE_INTEGER   liOldTickCount, liTickIncrementPerSec, liTickDiff;
    ULONG           ulSecsFromLastChange;
    PDATA_BLOCK     DataBlock = NULL;

    ASSERT(0 != VolumeContext->liTickCountAtLastWOStateChange.QuadPart);
    // Close the current dirty block.
    AddLastChangeTimestampToCurrentNode(VolumeContext, NULL);

    liOldTickCount = VolumeContext->liTickCountAtLastWOStateChange;
    VolumeContext->liTickCountAtLastWOStateChange = KeQueryPerformanceCounter(&liTickIncrementPerSec);
    liTickDiff.QuadPart = VolumeContext->liTickCountAtLastWOStateChange.QuadPart - liOldTickCount.QuadPart;
    ulSecsFromLastChange = (ULONG)(liTickDiff.QuadPart / liTickIncrementPerSec.QuadPart);

    ASSERT(ecWriteOrderStateMetadata != VolumeContext->eWOState);
    if (ecWriteOrderStateData == VolumeContext->eWOState) {
        VolumeContext->ulNumSecondsInDataWOState += ulSecsFromLastChange;
        VCAddKernelTag(VolumeContext, DriverContext.DataToMetaDataModeTag, DriverContext.usDataToMetaDataModeTagLen, &DataBlock);
    } else {
        VolumeContext->ulNumSecondsInBitmapWOState += ulSecsFromLastChange;
        VCAddKernelTag(VolumeContext, DriverContext.BitmapToMetaDataModeTag, DriverContext.usBitmapToMetaDataModeTagLen, &DataBlock);
    }

    VolumeContext->ePrevWOState = VolumeContext->eWOState;
    VolumeContext->eWOState = ecWriteOrderStateMetadata;

    if (bServiceInitiated) {
        VolumeContext->lNumChangeToMetaDataWOStateOnUserRequest++;
    } else {
        VolumeContext->lNumChangeToMetaDataWOState++;
    }

    ULONG ulIndex = VolumeContext->ulWOSChangeInstance % INSTANCES_OF_WO_STATE_TRACKED;

    KeQuerySystemTime(&VolumeContext->liWOstateChangeTS[ulIndex]);
    VolumeContext->ulNumSecondsInWOS[ulIndex] = ulSecsFromLastChange;
    VolumeContext->eOldWOState[ulIndex] = VolumeContext->ePrevWOState;
    VolumeContext->eNewWOState[ulIndex] = ecWriteOrderStateMetadata;
    VolumeContext->ullcbDChangesPendingAtWOSchange[ulIndex] = VolumeContext->ChangeList.ullcbDataChangesPending;
    VolumeContext->ullcbMDChangesPendingAtWOSchange[ulIndex] = VolumeContext->ChangeList.ullcbMetaDataChangesPending;
    VolumeContext->ullcbBChangesPendingAtWOSchange[ulIndex] = VolumeContext->ChangeList.ullcbBitmapChangesPending;
    VolumeContext->eWOSChangeReason[ulIndex] = eWOSChangeReason;
    VolumeContext->ulWOSChangeInstance++;

    return;
}

void
VCSetWOStateToData(PVOLUME_CONTEXT VolumeContext, etWOSChangeReason eWOSChangeReason)
{
    LARGE_INTEGER   liOldTickCount, liTickIncrementPerSec, liTickDiff;
    ULONG           ulSecsFromLastChange;
    PDATA_BLOCK     DataBlock = NULL;

    ASSERT(0 != VolumeContext->liTickCountAtLastWOStateChange.QuadPart);

    ASSERT(VolumeContext->ChangeList.ullcbMetaDataChangesPending == 0);
    ASSERT(VolumeContext->ChangeList.ullcbBitmapChangesPending == 0);
    ASSERT(ValidateDataWOSTransition(VolumeContext) == TRUE);
    
    AddLastChangeTimestampToCurrentNode(VolumeContext, NULL);

    liOldTickCount = VolumeContext->liTickCountAtLastWOStateChange;
    VolumeContext->liTickCountAtLastWOStateChange = KeQueryPerformanceCounter(&liTickIncrementPerSec);
    liTickDiff.QuadPart = VolumeContext->liTickCountAtLastWOStateChange.QuadPart - liOldTickCount.QuadPart;
    ulSecsFromLastChange = (ULONG)(liTickDiff.QuadPart / liTickIncrementPerSec.QuadPart);

    ASSERT(ecWriteOrderStateData != VolumeContext->eWOState);
    if (ecWriteOrderStateBitmap == VolumeContext->eWOState) {
        VolumeContext->ulNumSecondsInBitmapWOState += ulSecsFromLastChange;
        VCAddKernelTag(VolumeContext, DriverContext.BitmapToDataModeTag, DriverContext.usBitmapToDataModeTagLen, &DataBlock);
    } else {
        VolumeContext->ulNumSecondsInMetaDataWOState += ulSecsFromLastChange;
        VCAddKernelTag(VolumeContext, DriverContext.MetaDataToDataModeTag, DriverContext.usMetaDataToDataModeTagLen, &DataBlock);
    }

    VolumeContext->ePrevWOState = VolumeContext->eWOState;
    VolumeContext->eWOState = ecWriteOrderStateData;
    VolumeContext->lNumChangeToDataWOState++;

    ULONG ulIndex = VolumeContext->ulWOSChangeInstance % INSTANCES_OF_WO_STATE_TRACKED;

    KeQuerySystemTime(&VolumeContext->liWOstateChangeTS[ulIndex]);
    VolumeContext->ulNumSecondsInWOS[ulIndex] = ulSecsFromLastChange;
    VolumeContext->eOldWOState[ulIndex] = VolumeContext->ePrevWOState;
    VolumeContext->eNewWOState[ulIndex] = ecWriteOrderStateData;
    VolumeContext->ullcbDChangesPendingAtWOSchange[ulIndex] = VolumeContext->ChangeList.ullcbDataChangesPending;
    VolumeContext->ullcbMDChangesPendingAtWOSchange[ulIndex] = VolumeContext->ChangeList.ullcbMetaDataChangesPending;
    VolumeContext->ullcbBChangesPendingAtWOSchange[ulIndex] = VolumeContext->ChangeList.ullcbBitmapChangesPending;
    VolumeContext->eWOSChangeReason[ulIndex] = eWOSChangeReason;
    VolumeContext->ulWOSChangeInstance++;

    return;
}

void
VCSetWOStateToBitmap(PVOLUME_CONTEXT VolumeContext, bool bServiceInitiated, etWOSChangeReason eWOSChangeReason)
{
    LARGE_INTEGER   liOldTickCount, liTickIncrementPerSec, liTickDiff;
    ULONG           ulSecsFromLastChange;

    ASSERT(0 != VolumeContext->liTickCountAtLastWOStateChange.QuadPart);

    AddLastChangeTimestampToCurrentNode(VolumeContext, NULL);

    liOldTickCount = VolumeContext->liTickCountAtLastWOStateChange;
    VolumeContext->liTickCountAtLastWOStateChange = KeQueryPerformanceCounter(&liTickIncrementPerSec);
    liTickDiff.QuadPart = VolumeContext->liTickCountAtLastWOStateChange.QuadPart - liOldTickCount.QuadPart;
    ulSecsFromLastChange = (ULONG)(liTickDiff.QuadPart / liTickIncrementPerSec.QuadPart);

    if (bServiceInitiated) {
        VolumeContext->lNumChangeToBitmapWOStateOnUserRequest++;
    } else {
        VolumeContext->lNumChangeToBitmapWOState++;
    }

    ASSERT(ecWriteOrderStateBitmap != VolumeContext->eWOState);

    if (ecWriteOrderStateData == VolumeContext->eWOState) {
        VolumeContext->ulNumSecondsInDataWOState += ulSecsFromLastChange;
    } else {
        VolumeContext->ulNumSecondsInMetaDataWOState += ulSecsFromLastChange;
    }

    VolumeContext->ePrevWOState = VolumeContext->eWOState;
    VolumeContext->eWOState = ecWriteOrderStateBitmap;

    ULONG ulIndex = VolumeContext->ulWOSChangeInstance % INSTANCES_OF_WO_STATE_TRACKED;

    KeQuerySystemTime(&VolumeContext->liWOstateChangeTS[ulIndex]);
    VolumeContext->ulNumSecondsInWOS[ulIndex] = ulSecsFromLastChange;
    VolumeContext->eOldWOState[ulIndex] = VolumeContext->ePrevWOState;
    VolumeContext->eNewWOState[ulIndex] = ecWriteOrderStateBitmap;
    VolumeContext->ullcbDChangesPendingAtWOSchange[ulIndex] = VolumeContext->ChangeList.ullcbDataChangesPending;
    VolumeContext->ullcbMDChangesPendingAtWOSchange[ulIndex] = VolumeContext->ChangeList.ullcbMetaDataChangesPending;
    VolumeContext->ullcbBChangesPendingAtWOSchange[ulIndex] = VolumeContext->ChangeList.ullcbBitmapChangesPending;
    VolumeContext->eWOSChangeReason[ulIndex] = eWOSChangeReason;
    VolumeContext->ulWOSChangeInstance++;
    return;
}

bool
VCSetWOState(PVOLUME_CONTEXT VolumeContext, bool bServiceInitiated, char *FunctionName)
{
    // If bitmap file is not open, or bitmap file is not in ecVBitmapStateClosed and not in ecvBitmapStateReadCompleted
    // or if bitmapChanges are pending, volume is in ecWriterOrderStateBitmap
    if (ecServiceShutdown == DriverContext.eServiceState) {
        if (ecWriteOrderStateBitmap == VolumeContext->eWOState) {
            return false;
        } else {
            InVolDbgPrint(DL_INFO, DM_WO_STATE, 
                          ("%s(%s): Service is shutdown - WOState changing from %s to ecWOStateBitmap on volume %s(%s)\n", __FUNCTION__, FunctionName,
                                     WOStateString(VolumeContext->eWOState), VolumeContext->GUIDinANSI, VolumeContext->DriveLetter));
            VCSetWOStateToBitmap(VolumeContext, bServiceInitiated, eCWOSChangeReasonServiceShutdown);
            return true;
        }
    }

    if (VolumeContext->ChangeList.ulBitmapChangesPending) {
        if (ecWriteOrderStateBitmap == VolumeContext->eWOState) {
            return false;
        } else {
            InVolDbgPrint(DL_INFO, DM_WO_STATE, 
                          ("%s(%s): Bitmap changes pending - WOState changing from %s to ecWOStateBitmap on volume %s(%s)\n", __FUNCTION__, FunctionName,
                                     WOStateString(VolumeContext->eWOState), VolumeContext->GUIDinANSI, VolumeContext->DriveLetter));
            VCSetWOStateToBitmap(VolumeContext, bServiceInitiated, ecWOSChangeReasonBitmapChanges);
            return true;
        }
    }

    if (NULL == VolumeContext->pVolumeBitmap) {
        if (ecWriteOrderStateBitmap == VolumeContext->eWOState) {
            return false;
        } else {
            InVolDbgPrint(DL_INFO, DM_WO_STATE, 
                          ("%s(%s): Bitmap is not opened - WOState changing from %s to ecWOStateBitmap on volume %s(%s)\n", __FUNCTION__, FunctionName,
                                     WOStateString(VolumeContext->eWOState), VolumeContext->GUIDinANSI, VolumeContext->DriveLetter));
            VCSetWOStateToBitmap(VolumeContext, bServiceInitiated, ecWOSChangeReasonBitmapNotOpen);
            return true;
        }
    }

    if ((ecVBitmapStateClosed != VolumeContext->pVolumeBitmap->eVBitmapState) && 
        (ecVBitmapStateReadCompleted != VolumeContext->pVolumeBitmap->eVBitmapState))
    {
        if (ecWriteOrderStateBitmap == VolumeContext->eWOState) {
            return false;
        } else {
            InVolDbgPrint(DL_INFO, DM_WO_STATE, 
                          ("%s(%s): BitmapState(%s) - WOState changing from %s to ecWOStateBitmap on volume %s(%s)\n", __FUNCTION__, FunctionName,
                                     VBitmapStateString(VolumeContext->pVolumeBitmap->eVBitmapState), WOStateString(VolumeContext->eWOState), 
                           VolumeContext->GUIDinANSI, VolumeContext->DriveLetter));
            VCSetWOStateToBitmap(VolumeContext, bServiceInitiated, ecWOSChangeReasonBitmapState);
            return true;
        }
    }

    // Not in ecWriteOrderStateBitmap
    // If MetaDataChanges are pending then volume is in ecWriterOrderStateMetadata
    if (ecCaptureModeMetaData == VolumeContext->eCaptureMode) {
        if (ecWriteOrderStateMetadata == VolumeContext->eWOState) {
            return false;
        } else {
            InVolDbgPrint(DL_INFO, DM_WO_STATE, 
                          ("%s(%s): Capture mode meta-data - WOState changing from %s to ecWOStateMetaData on volume %s(%s)\n", __FUNCTION__, FunctionName,
                                     WOStateString(VolumeContext->eWOState), VolumeContext->GUIDinANSI, VolumeContext->DriveLetter));
            VCSetWOStateToMetaData(VolumeContext, bServiceInitiated, ecWOSChangeReasonCaptureModeMD);
            return true;
        }
    }

    if (VolumeContext->ChangeList.ulMetaDataChangesPending) {
        if (ecWriteOrderStateMetadata == VolumeContext->eWOState) {
            return false;
        } else {
            InVolDbgPrint(DL_INFO, DM_WO_STATE, 
                          ("%s(%s): Metadata changes pending - WOState changing from %s to ecWOStateMetaData on volume %s(%s)\n", __FUNCTION__, FunctionName,
                                     WOStateString(VolumeContext->eWOState), VolumeContext->GUIDinANSI, VolumeContext->DriveLetter));
            VCSetWOStateToMetaData(VolumeContext, bServiceInitiated, ecWOSChangeReasonMDChanges);
            return true;
        }
    }

    // Not in bitmap state, not in meta-data state
    if (ecWriteOrderStateData == VolumeContext->eWOState) {
        return false;
    } else {
        InVolDbgPrint(DL_INFO, DM_WO_STATE, 
                      ("%s(%s): Only data changes pending - WOState changing from %s to ecWOStateData on volume %s(%s)\n", __FUNCTION__, FunctionName,
                                 WOStateString(VolumeContext->eWOState), VolumeContext->GUIDinANSI, VolumeContext->DriveLetter));
        VCSetWOStateToData(VolumeContext, ecWOSChangeReasonDChanges);
        return true;
    }

    return false;
}

bool
CanSwitchToDataCaptureMode(PVOLUME_CONTEXT VolumeContext, bool bClearDiff)
{
    if (ecServiceRunning != DriverContext.eServiceState) {
        return false;
    }

    if (ecCaptureModeData == VolumeContext->eCaptureMode)
        return true;

    if (!IsDataCaptureEnabledForThisVolume(VolumeContext)) {
        return false;
    }

    // Bitmap is not read completely, we should not move into data capture mode. => Old logic
    //
    // With new changes where we track data as much as possible in memory, we must move to
    // data capture mode ASAP.
    if (NULL == VolumeContext->pVolumeBitmap) {
        return false;
    }

    ULONG usMaxDataBlocksInDirtyBlock = VolumeContext->ulMaxDataSizePerDataModeDirtyBlock / DriverContext.ulDataBlockSize;
    if ((DriverContext.ulDataBlocksInFreeDataBlockList + DriverContext.ulDataBlocksInLockedDataBlockList) < usMaxDataBlocksInDirtyBlock) {
        InVolDbgPrint(DL_INFO, DM_DATA_FILTERING | DM_CAPTURE_MODE, 
                      ("%s: Returning false as FreeDataBlocks(%#x) < MaxDataBlocksInDirtyBlock(%#x)\n", __FUNCTION__,
                        DriverContext.ulDataBlocksInFreeDataBlockList, usMaxDataBlocksInDirtyBlock));
        return false;
    }

    if (!bClearDiff) {
    if ((DriverContext.ulDataBlocksInFreeDataBlockList + DriverContext.ulDataBlocksInLockedDataBlockList) < 
        (((DriverContext.ulAvailableDataBlockCountInPercentage) * (DriverContext.ulMaxDataSizePerVolume / DriverContext.ulDataBlockSize))/100)) {
        InVolDbgPrint(DL_INFO, DM_DATA_FILTERING | DM_CAPTURE_MODE, 
            ("%s: Returning false as DC-Free+Locked DataBlocks(%#d) < (%#d) [%#d Percentage of MaxDataBlocksPerVolume %#d]\n", __FUNCTION__,
                      DriverContext.ulDataBlocksInFreeDataBlockList + DriverContext.ulDataBlocksInLockedDataBlockList,
                      (((DriverContext.ulAvailableDataBlockCountInPercentage) * (DriverContext.ulMaxDataSizePerVolume / DriverContext.ulDataBlockSize))/100),
                      DriverContext.ulAvailableDataBlockCountInPercentage,
                      DriverContext.ulMaxDataSizePerVolume / DriverContext.ulDataBlockSize));
        return false;
    }

    if (VolumeContext->ulcbDataAllocated > (((DriverContext.ulMaxDataAllocLimitInPercentage) * (DriverContext.ulMaxDataSizePerVolume))/100)) {
        InVolDbgPrint(DL_INFO, DM_DATA_FILTERING | DM_CAPTURE_MODE, 
            ("%s: Returning false as VC-ulcbDataAllocated(%#d) > (%#d) [%#d Percentage of MaxDataSizePerVolume %#d] \n", __FUNCTION__,
                      VolumeContext->ulcbDataAllocated, 
                      (((DriverContext.ulMaxDataAllocLimitInPercentage) * (DriverContext.ulMaxDataSizePerVolume))/100),
                      DriverContext.ulMaxDataAllocLimitInPercentage,
                      DriverContext.ulMaxDataSizePerVolume));
        return false;
    }
    }

    if(ecCaptureModeMetaData == VolumeContext->eCaptureMode){
        
        LARGE_INTEGER   liCurrentTickCount, liTickIncrementPerSec, liTickDiff;
        ULONG           ulSecsInCurrentCaptureMode;
        liCurrentTickCount = KeQueryPerformanceCounter(&liTickIncrementPerSec);
        liTickDiff.QuadPart = liCurrentTickCount.QuadPart - VolumeContext->liTickCountAtLastCaptureModeChange.QuadPart;
        ulSecsInCurrentCaptureMode = (ULONG)(liTickDiff.QuadPart / liTickIncrementPerSec.QuadPart);
        if(VolumeContext->ullcbDataAlocMissed && (ulSecsInCurrentCaptureMode <= DriverContext.ulMaxWaitTimeForIrpCompletion ))
            return false;
    }

    InVolDbgPrint(DL_INFO, DM_DATA_FILTERING | DM_CAPTURE_MODE, 
                  ("%s: Returning true.\nFreeDataBlocks(%#x), MaxDataBlocksInDB(%#x), MaxDataBlocksPerVol(%#x)\n",
                   __FUNCTION__, DriverContext.ulDataBlocksInFreeDataBlockList,
                   usMaxDataBlocksInDirtyBlock, (DriverContext.ulMaxDataSizePerVolume / DriverContext.ulDataBlockSize)));
    return true;
}

void
VCSetCaptureModeToMetadata(PVOLUME_CONTEXT VolumeContext, bool bServiceInitiated)
{
    ASSERT(0 != VolumeContext->liTickCountAtLastCaptureModeChange.QuadPart);

    if (ecCaptureModeMetaData == VolumeContext->eCaptureMode) {
        return;
    }

    LARGE_INTEGER   liOldTickCount, liTickIncrementPerSec, liTickDiff;
    ULONG           ulSecsFromLastChange;
    PDATA_BLOCK     DataBlock = NULL;

    // Close the current dirty block.
    AddLastChangeTimestampToCurrentNode(VolumeContext, NULL);

    liOldTickCount = VolumeContext->liTickCountAtLastCaptureModeChange;
    VolumeContext->liTickCountAtLastCaptureModeChange = KeQueryPerformanceCounter(&liTickIncrementPerSec);
    liTickDiff.QuadPart = VolumeContext->liTickCountAtLastCaptureModeChange.QuadPart - liOldTickCount.QuadPart;
    ulSecsFromLastChange = (ULONG)(liTickDiff.QuadPart / liTickIncrementPerSec.QuadPart);

    VolumeContext->eCaptureMode = ecCaptureModeMetaData;

    // Free all data buffers that are reserved.
    VolumeContext->ullDiskUsageArray[VolumeContext->ulMuCurrentInstance % INSTANCES_OF_MEM_USAGE_TRACKED] =
        VolumeContext->ullcbDataWrittenToDisk;
    VolumeContext->ulMemAllocatedArray[VolumeContext->ulMuCurrentInstance % INSTANCES_OF_MEM_USAGE_TRACKED] =
        VolumeContext->ulcbDataAllocated;
    VolumeContext->ulMemReservedArray[VolumeContext->ulMuCurrentInstance % INSTANCES_OF_MEM_USAGE_TRACKED] =
        VolumeContext->ulcbDataReserved;
    VolumeContext->ulMemFreeArray[VolumeContext->ulMuCurrentInstance % INSTANCES_OF_MEM_USAGE_TRACKED] = 
        VolumeContext->ulcbDataFree;
    VolumeContext->ulTotalMemFreeArray[VolumeContext->ulMuCurrentInstance % INSTANCES_OF_MEM_USAGE_TRACKED] =
        (DriverContext.ulDataBlocksInFreeDataBlockList + DriverContext.ulDataBlocksInLockedDataBlockList) * DriverContext.ulDataBlockSize;
    KeQuerySystemTime(&VolumeContext->liCaptureModeChangeTS[VolumeContext->ulMuCurrentInstance % INSTANCES_OF_MEM_USAGE_TRACKED]);
    VolumeContext->ulMuCurrentInstance++;
    VCFreeReservedDataBlockList(VolumeContext);
    VolumeContext->ullcbDataAlocMissed += VolumeContext->ulcbDataReserved;
    VolumeContext->ulcbDataReserved = 0;
    VolumeContext->ulcbDataFree = 0;

    VolumeContext->ulNumSecondsInDataCaptureMode += ulSecsFromLastChange;

    if (bServiceInitiated) {
        VolumeContext->lNumChangeToMetaDataCaptureModeOnUserRequest++;
    } else {
        VolumeContext->lNumChangeToMetaDataCaptureMode++;
    }

    return;
}

void
VCSetCaptureModeToData(PVOLUME_CONTEXT VolumeContext)
{
    ASSERT(0 != VolumeContext->liTickCountAtLastCaptureModeChange.QuadPart);

    if (ecCaptureModeData == VolumeContext->eCaptureMode) {
        return;
    }

    LARGE_INTEGER   liOldTickCount, liTickIncrementPerSec, liTickDiff;
    ULONG           ulSecsFromLastChange;
    PDATA_BLOCK     DataBlock = NULL;

    AddLastChangeTimestampToCurrentNode(VolumeContext, NULL);

    liOldTickCount = VolumeContext->liTickCountAtLastCaptureModeChange;
    VolumeContext->liTickCountAtLastCaptureModeChange = KeQueryPerformanceCounter(&liTickIncrementPerSec);
    liTickDiff.QuadPart = VolumeContext->liTickCountAtLastCaptureModeChange.QuadPart - liOldTickCount.QuadPart;
    ulSecsFromLastChange = (ULONG)(liTickDiff.QuadPart / liTickIncrementPerSec.QuadPart);

    VolumeContext->eCaptureMode = ecCaptureModeData;
    VolumeContext->lNumChangeToDataCaptureMode++;

    VolumeContext->ulcbDataFree = 0;
    VolumeContext->ullcbDataAlocMissed = 0;
    VolumeContext->ullNetWritesSeenInBytes = 0;
    VolumeContext->ulNumSecondsInMetadataCaptureMode += ulSecsFromLastChange;
    VolumeContext->ulNumPendingIrps = 0; 
    return;
}

VOID
FlushAndCloseBitmapFile(PVOLUME_CONTEXT pVolumeContext)
{
    KIRQL               OldIrql;
    PVOLUME_BITMAP      pVolumeBitmap;
    
    if (NULL == pVolumeContext)
        return;
    
    InMageFltSaveAllChanges(pVolumeContext, TRUE, NULL);

    KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
    pVolumeBitmap = pVolumeContext->pVolumeBitmap;
    pVolumeContext->pVolumeBitmap = NULL;
    KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);

    if (pVolumeBitmap) {
        BitmapPersistentValues BitmapPersistentValue(pVolumeContext);
        CloseBitmapFile(pVolumeBitmap, false, BitmapPersistentValue, false);
    }

    return;
}

NTSTATUS
InMageFltSaveAllChanges(PVOLUME_CONTEXT   pVolumeContext, BOOLEAN bWaitRequired, PLARGE_INTEGER pliTimeOut, bool bMarkVolumeOffline)
{
    LIST_ENTRY          ListHead;
    PWORK_QUEUE_ENTRY   pWorkItem = NULL;
    KIRQL               OldIrql;

    if (NULL == pVolumeContext)
        return STATUS_SUCCESS;

    if (pVolumeContext->ulFlags & VCF_BITMAP_WRITE_DISABLED)
        return STATUS_SUCCESS;

    if (IS_VOLUME_OFFLINE(pVolumeContext)) {
        // If Volume is on cluster disk, and volume is released.
        // we should not have any changes here.
        ASSERT((0 == pVolumeContext->ChangeList.ulTotalChangesPending) && (0 == pVolumeContext->ChangeList.ullcbTotalChangesPending));

        if (pVolumeContext->ChangeList.ulTotalChangesPending || pVolumeContext->ulChangesPendingCommit) {
            InVolDbgPrint( DL_ERROR, DM_BITMAP_WRITE | DM_CLUSTER, 
                ("InMageFltSaveAllChanges: Volume %s offline has ChangesPending = 0x%x ChangesPendingCommit = 0x%x\n",
                        pVolumeContext->DriveLetter, pVolumeContext->ChangeList.ulTotalChangesPending,
                        pVolumeContext->ulChangesPendingCommit));
        } else {
            InVolDbgPrint( DL_TRACE_L1, DM_BITMAP_OPEN | DM_BITMAP_WRITE | DM_CLUSTER,
                           ("InMageFltSaveAllChanges: Volume %s offline: Pending = 0x%x, PendingCommit = 0x%x\n",
                            pVolumeContext->DriveLetter,
                            pVolumeContext->ChangeList.ulTotalChangesPending, pVolumeContext->ulChangesPendingCommit));
        }

        return STATUS_SUCCESS;
    }

    InitializeListHead(&ListHead);

    if (0 == (pVolumeContext->ulFlags & VCF_BITMAP_WRITE_DISABLED)) {
        KeWaitForSingleObject(&pVolumeContext->SyncEvent, Executive, KernelMode, FALSE, NULL);
        KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);

        // Device is being shutdown soon, write all pending dirty blocks to bit map.
        OrphanMappedDataBlocksAndMoveCommitPendingDirtyBlockToMainQueue(pVolumeContext);
        if (!IsListEmpty(&pVolumeContext->ChangeList.Head)) {
            if (!AddVCWorkItemToList(ecWorkItemBitmapWrite, pVolumeContext, 0, TRUE, &ListHead)) {
            	//Mark for resync if failed to add work item
                QueueWorkerRoutineForSetVolumeOutOfSync(pVolumeContext, INVOLFLT_ERR_NO_GENERIC_NPAGED_POOL, STATUS_NO_MEMORY, true);
            }
        }
        if (bMarkVolumeOffline)
            pVolumeContext->ulFlags &= ~VCF_CLUSTER_VOLUME_ONLINE;

        KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);
        KeSetEvent(&pVolumeContext->SyncEvent, IO_NO_INCREMENT, FALSE);
    }

    while (!IsListEmpty(&ListHead)) {
        pWorkItem = (PWORK_QUEUE_ENTRY)RemoveHeadList(&ListHead);
        ProcessVContextWorkItems(pWorkItem);
        DereferenceWorkQueueEntry(pWorkItem);
    }

    NTSTATUS Status = STATUS_SUCCESS;

    if (bWaitRequired)
        Status = WaitForAllWritesToComplete(pVolumeContext->pVolumeBitmap, pliTimeOut);

    return Status;
}

VOID
ProcessVContextWorkItems(PWORK_QUEUE_ENTRY pWorkItem)
{
    PVOLUME_CONTEXT     pVolumeContext = NULL;
    PVOLUME_BITMAP      pVolumeBitmap = NULL;
    KIRQL               OldIrql;

    PAGED_CODE();

    ASSERT(NULL != pWorkItem);
    if (NULL == pWorkItem) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN,
            ("ProcessVContextWorkItems: Called with NULL pWorkItemt\n"));
        return;
    }

    ASSERT(NULL != pWorkItem->Context);
    if (NULL == pWorkItem->Context) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN,
            ("ProcessVContextWorkItems: Type = %#x called with NULL pVolumeContext\n", pWorkItem->eWorkItem));
        return;
    }

    pVolumeContext = (PVOLUME_CONTEXT)pWorkItem->Context;
    KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
    if (pVolumeContext->pVolumeBitmap)
        pVolumeBitmap = ReferenceVolumeBitmap(pVolumeContext->pVolumeBitmap);
    KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);

    switch(pWorkItem->eWorkItem) {
    case ecWorkItemOpenBitmap:
        if (NULL == pVolumeBitmap) {
            NTSTATUS    Status = STATUS_SUCCESS;

            InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, 
                ("ProcessVContextWorkItems: Opening bitmap for Volume %S(%S) VolumeContext %p\n",
                    pVolumeContext->wGUID, pVolumeContext->wDriveLetter, pVolumeContext));
            
            pVolumeBitmap = OpenBitmapFile(pVolumeContext, Status);
            
            KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);            
            if (IS_NOT_CLUSTER_VOLUME(pVolumeContext)) {
                ASSERT(NULL == pVolumeContext->pVolumeBitmap);
                ASSERT(pVolumeContext->ulFlags & VCF_OPEN_BITMAP_REQUESTED);
            }

            pVolumeContext->ulFlags &= ~VCF_OPEN_BITMAP_REQUESTED;
            if (NULL != pVolumeBitmap) {
                pVolumeContext->pVolumeBitmap = ReferenceVolumeBitmap(pVolumeBitmap);
                if(pVolumeContext->bQueueChangesToTempQueue) {
                    MoveUnupdatedDirtyBlocks(pVolumeContext);
                }
            } else {
                // Set error for open bitmap. So that further bitmap opens are not done.
                InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("ProcessVContextWorkItems: Open bitmap for volume %c: has failed\n",
                        (UCHAR)pVolumeContext->wDriveLetter[0]));
                SetBitmapOpenError(pVolumeContext, true, Status);
            }
            KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);
        }
        break;

    case ecWorkItemBitmapWrite:
        if (NULL != pVolumeBitmap) {
            InVolDbgPrint(DL_INFO, DM_BITMAP_WRITE, 
                ("ProcessVContextWorkItems: writing to bitmap for Volume %S(%S\n",
                    pVolumeContext->wGUID, pVolumeContext->wDriveLetter));
         
            ExAcquireFastMutex(&pVolumeBitmap->Mutex);

            KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql); 
            ASSERT((ecWriteOrderStateBitmap == pVolumeContext->eWOState) ||
                   (ecVBitmapStateReadCompleted == pVolumeBitmap->eVBitmapState) ||
                   (ecVBitmapStateClosed == pVolumeBitmap->eVBitmapState) ||
                   (ecVBitmapStateReadError == pVolumeBitmap->eVBitmapState) ||
                   (ecVBitmapStateInternalError == pVolumeBitmap->eVBitmapState));
                   
            // Set state to adding changes to bitmap
            pVolumeBitmap->eVBitmapState = ecVBitmapStateAddingChanges;

            if (ecWriteOrderStateBitmap != pVolumeContext->eWOState) {
                VCSetWOState(pVolumeContext, false, __FUNCTION__);
            }

            VCSetCaptureModeToMetadata(pVolumeContext, false);

            ASSERT(NULL != pVolumeContext->pVolumeBitmap);
            QueueWorkerRoutineForBitmapWrite(pVolumeContext, pWorkItem->ulInfo1);
            KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);

            ExReleaseFastMutex(&pVolumeBitmap->Mutex);
        } else {
            SetBitmapOpenFailDueToLossOfChanges(pVolumeContext, false);
        }
        break;
    case ecWorkItemStartBitmapRead:
        if (NULL != pVolumeBitmap) {
            InVolDbgPrint(DL_INFO, DM_BITMAP_READ, 
                ("ProcessVContextWorkItems: starting read from bitmap for Volume %S(%S)\n",
                    pVolumeContext->wGUID, pVolumeContext->wDriveLetter));

		  //Fix For Bug 27337
		  //By now Bitmap file should be in opened state. 
	      #if (NTDDI_VERSION >= NTDDI_WS03)
			if( (!pVolumeContext->FsInfoWrapper.FsInfoFetchRetries) && \
	  		(0 >= pVolumeContext->FsInfoWrapper.FsInfo.MaxVolumeByteOffsetForFs) &&\
	  		(1 < pVolumeContext->UniqueVolumeName.Length)){

				pVolumeContext->FsInfoWrapper.FsInfoNtStatus = \
				InMageFltFetchFsEndClusterOffset(&pVolumeContext->UniqueVolumeName,
										 &pVolumeContext->FsInfoWrapper.FsInfo);

		      if(STATUS_SUCCESS == pVolumeContext->FsInfoWrapper.FsInfoNtStatus){
				pVolumeContext->pVolumeBitmap->pBitmapAPI->pFsInfo = &pVolumeContext->FsInfoWrapper.FsInfo;
			  }
			  InterlockedIncrement(&pVolumeContext->FsInfoWrapper.FsInfoFetchRetries);
			}
    	  #endif
	            
            ExAcquireFastMutex(&pVolumeBitmap->Mutex);
            if ((ecVBitmapStateOpened == pVolumeBitmap->eVBitmapState) ||
                (ecVBitmapStateAddingChanges == pVolumeBitmap->eVBitmapState))
            {
                pVolumeBitmap->eVBitmapState = ecVBitmapStateReadStarted;
                QueueWorkerRoutineForStartBitmapRead(pVolumeBitmap);
            }
            ExReleaseFastMutex(&pVolumeBitmap->Mutex);
        } else {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE,
                ("ProcessVContextWorkItems: write bitmap called with NULL pVolumeContext\n"));
        }
        break;
    case ecWorkItemContinueBitmapRead:
        if (NULL != pVolumeBitmap) {
            InVolDbgPrint(DL_INFO, DM_BITMAP_READ, 
                ("ProcessVContextWorkItems: starting read from bitmap for Volume %S(%S)\n",
                    pVolumeContext->wGUID, pVolumeContext->wDriveLetter));
            
            ExAcquireFastMutex(&pVolumeBitmap->Mutex);
            if ((ecVBitmapStateReadStarted == pVolumeBitmap->eVBitmapState) ||
                (ecVBitmapStateReadPaused == pVolumeBitmap->eVBitmapState))
            {
                pVolumeBitmap->eVBitmapState = ecVBitmapStateReadStarted;
                QueueWorkerRoutineForContinueBitmapRead(pVolumeBitmap);
            }
            ExReleaseFastMutex(&pVolumeBitmap->Mutex);
        } else {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE,
                ("ProcessVContextWorkItems: write bitmap called with NULL pVolumeContext\n"));
        }
        break;
    case ecWorkItemForVolumeUnload:
        if (!IsListEmpty(&pVolumeContext->ChangeList.Head)) {
            if (NULL == pVolumeBitmap) {
                NTSTATUS    Status = STATUS_SUCCESS;
                pVolumeBitmap = OpenBitmapFile(pVolumeContext, Status);
            }

            KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);            
            ASSERT(NULL != pVolumeContext->pVolumeBitmap);
            QueueWorkerRoutineForBitmapWrite(pVolumeContext, 0);
            KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);
        }

        DriverContext.DataFileWriterManager->FreeVolumeWriter(pVolumeContext);
        FlushAndCloseBitmapFile(pVolumeContext);

        break;

    default:
        InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT,
            ("ProcessVContextWorkItems: Unknown work item type %#x\n", pWorkItem->eWorkItem));
        break;
    }

    DereferenceVolumeContext(pVolumeContext);
    if (NULL != pVolumeBitmap)
        DereferenceVolumeBitmap(pVolumeBitmap);

    return;
}

/*-----------------------------------------------------------------------------
 *
 * Worker Functions for Volume Context.
 *
 *-----------------------------------------------------------------------------
 */

// This routine is called with pVolumeContext->Lock held.
BOOLEAN
AddVCWorkItemToList(
    etWorkItem          eWorkItem,
    PVOLUME_CONTEXT     pVolumeContext,
    ULONG               ulAdditionalInfo,
    BOOLEAN             bOpenBitmapIfRequired,
    PLIST_ENTRY         ListHead
    )
{
    PWORK_QUEUE_ENTRY   pWorkQueueEntry = NULL;

    if (eWorkItem != ecWorkItemOpenBitmap) {
        if ((NULL == pVolumeContext->pVolumeBitmap) &&
            (0 == (pVolumeContext->ulFlags & VCF_OPEN_BITMAP_REQUESTED)) &&
            bOpenBitmapIfRequired)
        {
            InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, ("AddVCWorkItemToList: Queing Open bitmap for Volume=%#p, DriveLetter=%c\n", 
                        pVolumeContext, (CHAR)pVolumeContext->wDriveLetter[0]));
            pWorkQueueEntry = AllocateWorkQueueEntry();
            if (NULL != pWorkQueueEntry) {
                pVolumeContext->ulFlags |= VCF_OPEN_BITMAP_REQUESTED;
                pWorkQueueEntry->eWorkItem = ecWorkItemOpenBitmap;
                pWorkQueueEntry->Context = ReferenceVolumeContext(pVolumeContext);
                pWorkQueueEntry->ulInfo1 = 0;
                InsertTailList(ListHead, &pWorkQueueEntry->ListEntry);
                pWorkQueueEntry = NULL;
            } else {
                InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT | DM_BITMAP_OPEN, 
                    ("AddVCWorkItemToList: Failed to allocate WorkQueueEntry for bitmap open\n"));
                return FALSE;
            }
        }
    }

    pWorkQueueEntry = AllocateWorkQueueEntry();
    if (NULL != pWorkQueueEntry) {
        pWorkQueueEntry->eWorkItem = eWorkItem;
        pWorkQueueEntry->Context = ReferenceVolumeContext(pVolumeContext);
        pWorkQueueEntry->ulInfo1 = ulAdditionalInfo;
        InsertTailList(ListHead, &pWorkQueueEntry->ListEntry);
    } else {
        InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, 
            ("AddVCWorkItemToList: Failed to allocate WorkQueueEntry for type %#x\n", eWorkItem));
        return FALSE;
    }

    return TRUE;
}

NTSTATUS
OpenVolumeParametersRegKey(PVOLUME_CONTEXT pVolumeContext, Registry **ppVolumeParam)
{
    Registry        *pVolumeParam;
    NTSTATUS        Status;

    PAGED_CODE();

    if (NULL == ppVolumeParam) {
        InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, ("OpenVolumeParametersRegKey: ppVolumeParam NULL\n"));
        return STATUS_INVALID_PARAMETER;
    }

    pVolumeParam = new Registry();
    if (NULL == pVolumeParam) {
        InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, 
            ("OpenVolumeParametersRegKey: Failed in creating registry object\n"));
        return STATUS_NO_MEMORY;
    }

    Status = pVolumeParam->OpenKeyReadWrite(&pVolumeContext->VolumeParameters);
    if (!NT_SUCCESS(Status)) {
        Status = pVolumeParam->CreateKeyReadWrite(&pVolumeContext->VolumeParameters);
        if (!NT_SUCCESS(Status)) {
            InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, 
                ("OpenVolumeParametersRegKey: Failed to Creat reg key %wZ with Status 0x%x", 
                 &pVolumeContext->VolumeParameters, Status));

            delete pVolumeParam;
            pVolumeParam = NULL;
        } else {
            InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                ("OpenVolumeParametersRegKey: Created reg key %wZ", &pVolumeContext->VolumeParameters));
        }
    } else {
        InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
            ("OpenVolumeParametersRegKey: Opened reg key %wZ", &pVolumeContext->VolumeParameters));
    }

    *ppVolumeParam = pVolumeParam;
    return Status;
}

PWCHAR
GetVolumeOverride(PVOLUME_CONTEXT pVolumeContext)
{
    Registry        *pVolumeParam = NULL;
    PWSTR           pVolumeOverride = NULL;
    PBASIC_VOLUME   pBasicVolume = NULL;
    NTSTATUS        Status;

    PAGED_CODE();

    // See if there is a volume override.
    Status = OpenVolumeParametersRegKey(pVolumeContext, &pVolumeParam);
    if (NULL != pVolumeParam) {
        Status = pVolumeParam->ReadWString(VOLUME_LOG_PATH_NAME, STRING_LEN_NULL_TERMINATED, &pVolumeOverride);
        if (NT_SUCCESS(Status) && (NULL != pVolumeOverride)) {
            // We have volume override. Lets check to see if the volume is a cluster volume.
            pBasicVolume = pVolumeContext->pBasicVolume;
            
            if ((NULL != pBasicVolume) && (NULL != pBasicVolume->pDisk) &&
                (pBasicVolume->pDisk->ulFlags & BD_FLAGS_IS_CLUSTER_RESOURCE))
            {
                UNICODE_STRING  usClusterPrefix, usVolumeOverride;
                const unsigned long ulNumCharsInClusterBuffer = 150;
                WCHAR           ClusterPrefixBuffer[ulNumCharsInClusterBuffer];

                RtlInitUnicodeString(&usVolumeOverride, pVolumeOverride);

                InMageInitUnicodeString(&usClusterPrefix, ClusterPrefixBuffer, ulNumCharsInClusterBuffer*sizeof(WCHAR), 0);
                RtlAppendUnicodeToString(&usClusterPrefix, L"\\??\\Volume{");
                RtlAppendUnicodeToString(&usClusterPrefix, pVolumeContext->wGUID);
                RtlAppendUnicodeToString(&usClusterPrefix, L"}\\");
                if (FALSE == RtlPrefixUnicodeString(&usClusterPrefix, &usVolumeOverride, TRUE)) {
                    InMageInitUnicodeString(&usClusterPrefix, ClusterPrefixBuffer, ulNumCharsInClusterBuffer*sizeof(WCHAR), 0);
                    RtlAppendUnicodeToString(&usClusterPrefix, L"\\DosDevices\\Volume{");
                    RtlAppendUnicodeToString(&usClusterPrefix, pVolumeContext->wGUID);
                    RtlAppendUnicodeToString(&usClusterPrefix, L"}\\");
                    if (FALSE == RtlPrefixUnicodeString(&usClusterPrefix, &usVolumeOverride, TRUE)) {
                        if (pBasicVolume->ulFlags & BV_FLAGS_HAS_DRIVE_LETTER) {
                            // ClusterVolume has drive letter
                            if ((FALSE == IS_DRIVE_LETTER(pVolumeOverride[0])) ||
                                (L':' != pVolumeOverride[1]) || (pBasicVolume->DriveLetter != pVolumeOverride[0]))
                            {
                                // Volume override path is NOT on the same volume.
                                ExFreePoolWithTag(pVolumeOverride, TAG_REGISTRY_DATA);
                                pVolumeOverride = NULL;
                            }
                        } else {
                            // Volume does not have a drive letter, and drive letter independent path did not match
                            // Considering this path is not on the same volume.
                            ExFreePoolWithTag(pVolumeOverride, TAG_REGISTRY_DATA);
                            pVolumeOverride = NULL;
                        }
                    }
                }
            }
        } else {
            // Getting override has failed.
            if (NULL != pVolumeOverride) {
                ExFreePoolWithTag(pVolumeOverride, TAG_REGISTRY_DATA);
                pVolumeOverride = NULL;
            }
        }

        delete pVolumeParam;
    }

    return pVolumeOverride;
}

NTSTATUS
FillBitmapFilenameInVolumeContext(PVOLUME_CONTEXT pVolumeContext)
{
TRC
    NTSTATUS        Status = STATUS_SUCCESS;
    Registry        reg;
    UNICODE_STRING  uszOverrideFilename;
    Registry        *pVolumeParam = NULL;
    PWSTR           pVolumeOverride = NULL;
    PBASIC_VOLUME   pBasicVolume = NULL;

    PAGED_CODE();

    if (NULL == pVolumeContext)
        return STATUS_INVALID_PARAMETER;

    InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, ("FillBitmapFilenameInVolumeContext: For volume %S\n", pVolumeContext->wDriveLetter));

    ASSERT((MAX_LOG_PATHNAME * sizeof(WCHAR)) <= pVolumeContext->BitmapFileName.MaximumLength);
    pVolumeContext->BitmapFileName.Length = 0;
    uszOverrideFilename.Buffer = NULL;

    // See if there is a volume override.
    pVolumeOverride = GetVolumeOverride(pVolumeContext);
    if (NULL != pVolumeOverride) {
        // Found valid volume override.
        if ((TRUE == IS_DRIVE_LETTER(pVolumeOverride[0])) && (L':' == pVolumeOverride[1])) {
            RtlAppendUnicodeToString(&pVolumeContext->BitmapFileName, DOS_DEVICES_PATH);
        }

        RtlAppendUnicodeToString(&pVolumeContext->BitmapFileName, pVolumeOverride);
        ExFreePoolWithTag(pVolumeOverride, TAG_REGISTRY_DATA);
        goto cleanup;
    }

    // There is no valid volume override.
    uszOverrideFilename.Length = 0;
    uszOverrideFilename.MaximumLength = MAX_LOG_PATHNAME*sizeof(WCHAR);
    uszOverrideFilename.Buffer = new WCHAR[MAX_LOG_PATHNAME];
    InVolDbgPrint(DL_INFO, DM_MEM_TRACE, ("FillBitmapFilenameInVolumeContext: Allocation %p\n", uszOverrideFilename.Buffer));
    if (NULL == uszOverrideFilename.Buffer) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("FillBitmapFilenameInVolumeContext: Allocation failed for Override filename\n"));
        Status = STATUS_NO_MEMORY;
        goto cleanup;
    }

    // Check if this volume is a cluster volume.
    pBasicVolume = pVolumeContext->pBasicVolume;
    if (NULL != pBasicVolume) {
        ASSERT(NULL != pBasicVolume->pDisk);

        if ((NULL != pBasicVolume->pDisk) && 
            (pBasicVolume->pDisk->ulFlags & BD_FLAGS_IS_CLUSTER_RESOURCE))
        {
            // Volume on clusdisk. So create LogPathName.
            pVolumeContext->BitmapFileName.Length = 0;
            RtlAppendUnicodeStringToString(&pVolumeContext->BitmapFileName, &pVolumeContext->UniqueVolumeName);
            RtlAppendUnicodeToString(&pVolumeContext->BitmapFileName, DEFAULT_LOG_DIR_FOR_CLUSDISKS);
            RtlAppendUnicodeToString(&pVolumeContext->BitmapFileName, LOG_FILE_NAME_FOR_CLUSTER_VOLUME);

            // Write this path back to registry
            SetStringInRegVolumeCfg(pVolumeContext, VOLUME_LOG_PATH_NAME, &pVolumeContext->BitmapFileName);

            // We have a valid path name, let us return
            goto cleanup;
        }
    }

    // build a default log filename
    ASSERT((NULL != DriverContext.DefaultLogDirectory.Buffer) && (0 != DriverContext.DefaultLogDirectory.Length));
    pVolumeContext->BitmapFileName.Length = 0;
    uszOverrideFilename.Length = 0;

    RtlAppendUnicodeStringToString(&uszOverrideFilename, &DriverContext.DefaultLogDirectory);
    if (L'\\' != uszOverrideFilename.Buffer[(uszOverrideFilename.Length / sizeof(WCHAR)) - 1]) {
        RtlAppendUnicodeToString(&uszOverrideFilename, L"\\");
    }
    RtlAppendUnicodeToString(&uszOverrideFilename, LOG_FILE_NAME_PREFIX); // so things sort by name nice
    RtlAppendUnicodeToString(&uszOverrideFilename, pVolumeContext->wGUID);
    RtlAppendUnicodeToString(&uszOverrideFilename, LOG_FILE_NAME_SUFFIX);

    // Write this path back to registry
    SetStringInRegVolumeCfg(pVolumeContext, VOLUME_LOG_PATH_NAME, &uszOverrideFilename);

    RtlAppendUnicodeToString(&pVolumeContext->BitmapFileName, DOS_DEVICES_PATH);
    RtlAppendUnicodeStringToString(&pVolumeContext->BitmapFileName, &uszOverrideFilename);

    Status = STATUS_SUCCESS;

cleanup:
    if (NULL != uszOverrideFilename.Buffer)
        delete uszOverrideFilename.Buffer;

    ValidatePathForFileName(&pVolumeContext->BitmapFileName);

    return Status;
}

NTSTATUS
InitializeDataLogDirectory(PVOLUME_CONTEXT VolumeContext, PWSTR pDataLogDirectory, bool bCustom)
{
    NTSTATUS        Status = STATUS_SUCCESS;
    PBASIC_VOLUME   BasicVolume;
    bool            bWriteBackToRegistry = false;

    // If Directory name is read from volume specific parameters keep that.
    // If volume specific parameters does not have data log directory name
    // compute it by checking if it is a cluster volume or not
    if ((0 == VolumeContext->DataLogDirectory.Length) || (bCustom)) {
        // Directory not read from volume parameters
        // If Volume is a clustered volume
        // Check if this volume is a cluster volume.
        BasicVolume = VolumeContext->pBasicVolume;
        ASSERT((NULL == BasicVolume) || (NULL != BasicVolume->pDisk));
        if ((NULL != BasicVolume) && (NULL != BasicVolume->pDisk) && 
            (BasicVolume->pDisk->ulFlags & BD_FLAGS_IS_CLUSTER_RESOURCE))
        {
            if (!bCustom) {
                // Volume on clusdisk. So create DataLogDirectory.
                InMageCopyUnicodeString(&VolumeContext->DataLogDirectory, &VolumeContext->UniqueVolumeName);
                InMageAppendUnicodeString(&VolumeContext->DataLogDirectory, DEFAULT_DATA_LOG_DIR_FOR_CLUSDISKS);
            } else {
            	return STATUS_INVALID_PARAMETER;
            }
        } else {
            InMageCopyUnicodeString(&VolumeContext->DataLogDirectory, pDataLogDirectory);
            InMageTerminateUnicodeStringWith(&VolumeContext->DataLogDirectory, L'\\');
            InMageAppendUnicodeString(&VolumeContext->DataLogDirectory, &VolumeContext->wGUID[0]);
            InMageTerminateUnicodeStringWith(&VolumeContext->DataLogDirectory, L'\\');
            if (bCustom) {
                KIRQL OldIrql;
                KeAcquireSpinLock(&VolumeContext->Lock, &OldIrql);
                if (VolumeContext->ulFlags & VCF_DATALOG_DIRECTORY_VALIDATED) {
                    VolumeContext->ulFlags &= ~VCF_DATALOG_DIRECTORY_VALIDATED;
                }
                KeReleaseSpinLock(&VolumeContext->Lock, OldIrql);
            }
        }

        // Write this path back to registry
        bWriteBackToRegistry = true;
    } else {
        InMageTerminateUnicodeStringWith(&VolumeContext->DataLogDirectory, L'\\');
    }

    //This code is commented for fixing system not responding issue in win2k8 r2 bug id 9338. We are validating datalogdirectory in GenerateDataFilename
    /*
    Status = ValidatePathForFileName(&VolumeContext->DataLogDirectory);
    if (NT_SUCCESS(Status)) {
        VolumeContext->ulFlags |= VCF_DATALOG_DIRECTORY_VALIDATED;
    }
    */
    if (bWriteBackToRegistry) {
        SetStringInRegVolumeCfg(VolumeContext, VOLUME_DATALOG_DIRECTORY, &VolumeContext->DataLogDirectory);
    }

    return Status;
}

/*-----------------------------------------------------------------------------
 * Function : LoadVolumeCfgFromRegistry
 *
 * Args -
 *  pVolumeContext - Points to volume whose configuration is read from registry
 *
 * Notes 
 *  This function uses VolumeParameters field in VOLUME_CONTEXT. VolumeParamets
 *  specifies the registry key under which the volume configuration is stored.
 *  
 *  As this function does registry operations, IRQL should be < DISPATCH_LEVEL
 *-----------------------------------------------------------------------------
 */
NTSTATUS
LoadVolumeCfgFromRegistry(PVOLUME_CONTEXT pVolumeContext)
{
    NTSTATUS    Status = STATUS_SUCCESS;
    Registry    *pVolumeParam = NULL;
    ULONG       ulValue;
    ULONG       ulVolumeDataCapture, ulVolumeDataFiles;

    ASSERT((NULL != pVolumeContext) && (pVolumeContext->ulFlags & VCF_GUID_OBTAINED));
    ASSERT(DISPATCH_LEVEL > KeGetCurrentIrql());

    InVolDbgPrint(DL_FUNC_TRACE, DM_VOLUME_CONTEXT, 
        ("LoadVolumeCfgFromRegistry: Called - VolumeContext(%p) for GUID(%S)\n", 
        pVolumeContext, pVolumeContext->wGUID));

    if (NULL == pVolumeContext) {
        InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, ("pVolumeContext NULL\n"));
        return STATUS_INVALID_PARAMETER_1;
    }

    if (!(pVolumeContext->ulFlags & VCF_GUID_OBTAINED)) {
        InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, 
            ("LoadVolumeCfgFromRegistry: Called before GUID is obtained\n"));
        return STATUS_INVALID_PARAMETER_1;
    }

    // Let us open/create the registry key for this volume.
    pVolumeParam = new Registry();
    if (NULL == pVolumeParam) {
        InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, 
                ("Failed in creating registry object\n"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Status = pVolumeParam->OpenKeyReadWrite(&pVolumeContext->VolumeParameters);
    if (!NT_SUCCESS(Status)) {
        Status = pVolumeParam->CreateKeyReadWrite(&pVolumeContext->VolumeParameters);
        if (!NT_SUCCESS(Status)) {
            InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, 
                ("LoadVolumeCfgFromRegistry: Failed to Creat reg key %wZ with Status 0x%x", 
                 &pVolumeContext->VolumeParameters, Status));
            return Status;
        } else {
            InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                ("LoadVolumeCfgFromRegistry: Created reg key %wZ", &pVolumeContext->VolumeParameters));
        }
    } else {
        InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
            ("LoadVolumeCfgFromRegistry: Opened reg key %wZ", &pVolumeContext->VolumeParameters));
    }

    if (!IS_CLUSTER_VOLUME(pVolumeContext)) {
        ULONG   ulVolumeFilteringDisabledDefaultValue;

        if (DriverContext.bDisableVolumeFilteringForNewVolumes) {
            ulVolumeFilteringDisabledDefaultValue = 0x01;
        } else {
            ulVolumeFilteringDisabledDefaultValue = 0x00;
        }

        Status = pVolumeParam->ReadULONG(VOLUME_FILTERING_DISABLED, (ULONG32 *)&ulValue, 
                            ulVolumeFilteringDisabledDefaultValue, TRUE);
        if (NT_SUCCESS(Status)) {
            if (STATUS_INMAGE_OBJECT_CREATED == Status) {
                InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                    ("LoadVolumeCfgFromRegistry: Created %S with Default Value 0x%x\n",
                                        VOLUME_FILTERING_DISABLED, 0));
            } else {
                InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                    ("LoadVolumeCfgFromRegistry: Read Value %S Value = 0x%x\n",
                                        VOLUME_FILTERING_DISABLED, ulValue));
            }
        } else {
                InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, 
                    ("LoadVolumeCfgFromRegistry: Failed to Read Value %S with error = 0x%x\n",
                                        VOLUME_FILTERING_DISABLED, Status));
        }

        // Read Per Volume Persistent Values from Registry
        GetVContextFieldsFromRegistry(pVolumeContext);

        if (0 != ulValue) {
            // Volume Filtering is disabled
            StopFilteringDevice(pVolumeContext, false);
        } else {
            StartFilteringDevice(pVolumeContext, false);
#ifdef _WIN64
            SetDataPool(1, DriverContext.ullDataPoolSize);
#endif
        }
    }

    Status = pVolumeParam->ReadULONG(VOLUME_BITMAP_READ_DISABLED, (ULONG32 *)&ulValue, 
                            VOLUME_BITMAP_READ_DISABLED_DEFAULT_VALUE, TRUE);
    if (NT_SUCCESS(Status)) {
        if (STATUS_INMAGE_OBJECT_CREATED == Status) {
            InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                ("LoadVolumeCfgFromRegistry: Created %S with Default Value 0x%x\n",
                                    VOLUME_BITMAP_READ_DISABLED, 0));
        } else {
            InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                ("LoadVolumeCfgFromRegistry: Read Value %S Value = 0x%x\n",
                                    VOLUME_BITMAP_READ_DISABLED, ulValue));

        }
    } else {
            InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, 
                ("LoadVolumeCfgFromRegistry: Failed to Read Value %S with error = 0x%x\n",
                                    VOLUME_BITMAP_READ_DISABLED, Status));
    }

    if (0 != ulValue) {
        pVolumeContext->ulFlags |= VCF_BITMAP_READ_DISABLED;
    }

    Status = pVolumeParam->ReadULONG(VOLUME_BITMAP_WRITE_DISABLED, (ULONG32 *)&ulValue, 
                                        VOLUME_BITMAP_WRITE_DISABLED_DEFAULT_VALUE, TRUE);
    if (NT_SUCCESS(Status)) {
        if (STATUS_INMAGE_OBJECT_CREATED == Status) {
            InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                ("LoadVolumeCfgFromRegistry: Created %S with Default Value 0x%x\n",
                                    VOLUME_BITMAP_WRITE_DISABLED, 0));
        } else {
            InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                ("LoadVolumeCfgFromRegistry: Read Value %S Value = 0x%x\n",
                                    VOLUME_BITMAP_WRITE_DISABLED, ulValue));
        }
    } else {
            InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, 
                ("LoadVolumeCfgFromRegistry: Failed to Read Value %S with error = 0x%x\n",
                                    VOLUME_BITMAP_WRITE_DISABLED, Status));
    }

    if (0 != ulValue) {
        pVolumeContext->ulFlags |= VCF_BITMAP_WRITE_DISABLED;
    }

    Status = pVolumeParam->ReadULONG(VOLUME_BITMAP_OFFSET, (ULONG32 *)&ulValue, 
                                        VOLUME_BITMAP_OFFSET_DEFAULT_VALUE, FALSE);
    if (NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
           ("LoadVolumeCfgFromRegistry: Read Value %S Value = 0x%x\n",
                                VOLUME_BITMAP_OFFSET, ulValue));
    }
    pVolumeContext->ulBitmapOffset = ulValue;

    Status = pVolumeParam->ReadULONG(VOLUME_READ_ONLY, (ULONG32 *)&ulValue, 
                                        VOLUME_READ_ONLY_DEFAULT_VALUE, TRUE);
    if (NT_SUCCESS(Status)) {
        if (STATUS_INMAGE_OBJECT_CREATED == Status) {
            InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                ("LoadVolumeCfgFromRegistry: Created %S with Default Value 0x%x\n",
                                    VOLUME_READ_ONLY, VOLUME_READ_ONLY_DEFAULT_VALUE));
        } else {
            InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                ("LoadVolumeCfgFromRegistry: Read Value %S Value = 0x%x\n",
                                    VOLUME_READ_ONLY, ulValue));
        }
    } else {
            InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, 
                ("LoadVolumeCfgFromRegistry: Failed to Read Value %S with error = 0x%x\n",
                                    VOLUME_READ_ONLY, Status));
    }

    if (0 != ulValue) {
        pVolumeContext->ulFlags |= VCF_READ_ONLY;
    }

    //For Cluster volumes we started saving the resync required information in the bitmap
    if (!IS_CLUSTER_VOLUME(pVolumeContext)) {
        // Get Resync Required values
        Status = pVolumeParam->ReadULONG(VOLUME_RESYNC_REQUIRED, (ULONG32 *)&ulValue,
                                              DEFAULT_VOLUME_RESYNC_REQUIRED_VALUE, FALSE);
        if (NT_SUCCESS(Status)) {
            if (STATUS_INMAGE_OBJECT_CREATED == Status) {
                InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                    ("LoadVolumeCfgFromRegistry: Created %S with Default Value 0x%x\n",
                                        VOLUME_RESYNC_REQUIRED, DEFAULT_VOLUME_RESYNC_REQUIRED_VALUE));
            } else {
                InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                    ("LoadVolumeCfgFromRegistry: Read Value %S Value = 0x%x\n",
                                        VOLUME_RESYNC_REQUIRED, ulValue));
            }
        } else {
                InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, 
                    ("LoadVolumeCfgFromRegistry: Failed to Read Value %S with error = 0x%x\n",
                                        VOLUME_RESYNC_REQUIRED, Status));
        }
    
        if (VOLUME_RESYNC_REQUIRED_SET == ulValue) {
            pVolumeContext->bResyncRequired = true;
    
            Status = pVolumeParam->ReadULONG(VOLUME_OUT_OF_SYNC_COUNT, (ULONG32 *)&ulValue,
                                                DEFAULT_VOLUME_OUT_OF_SYNC_COUNT, FALSE);
            if (NT_SUCCESS(Status)) {
                if (STATUS_INMAGE_OBJECT_CREATED == Status) {
                    InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                        ("LoadVolumeCfgFromRegistry: Created %S with Default Value 0x%x\n",
                                            VOLUME_OUT_OF_SYNC_COUNT, DEFAULT_VOLUME_OUT_OF_SYNC_COUNT));
                } else {
                    InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                        ("LoadVolumeCfgFromRegistry: Read Value %S Value = 0x%x\n",
                                            VOLUME_OUT_OF_SYNC_COUNT, ulValue));
                }
            } else {
                    InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, 
                        ("LoadVolumeCfgFromRegistry: Failed to Read Value %S with error = 0x%x\n",
                                            VOLUME_OUT_OF_SYNC_COUNT, Status));
            }
    
            pVolumeContext->ulOutOfSyncCount = ulValue;
    
            Status = pVolumeParam->ReadULONG(VOLUME_OUT_OF_SYNC_ERRORCODE, (ULONG32 *)&ulValue,
                                                DEFAULT_VOLUME_OUT_OF_SYNC_ERRORCODE, FALSE);
            if (NT_SUCCESS(Status)) {
                if (STATUS_INMAGE_OBJECT_CREATED == Status) {
                    InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                        ("LoadVolumeCfgFromRegistry: Created %S with Default Value 0x%x\n",
                                            VOLUME_OUT_OF_SYNC_ERRORCODE, DEFAULT_VOLUME_OUT_OF_SYNC_ERRORCODE));
                } else {
                    InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                        ("LoadVolumeCfgFromRegistry: Read Value %S Value = 0x%x\n",
                                            VOLUME_OUT_OF_SYNC_ERRORCODE, ulValue));
                }
            } else {
                    InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, 
                        ("LoadVolumeCfgFromRegistry: Failed to Read Value %S with error = 0x%x\n",
                                            VOLUME_OUT_OF_SYNC_ERRORCODE, Status));
            }
    
            pVolumeContext->ulOutOfSyncErrorCode = ulValue;
    
            Status = pVolumeParam->ReadULONG(VOLUME_OUT_OF_SYNC_ERRORSTATUS, (ULONG32 *)&ulValue,
                                                DEFAULT_VOLUME_OUT_OF_SYNC_ERRORSTATUS, FALSE);
            if (NT_SUCCESS(Status)) {
                if (STATUS_INMAGE_OBJECT_CREATED == Status) {
                    InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                        ("LoadVolumeCfgFromRegistry: Created %S with Default Value 0x%x\n",
                                            VOLUME_OUT_OF_SYNC_ERRORSTATUS, DEFAULT_VOLUME_OUT_OF_SYNC_ERRORSTATUS));
                } else {
                    InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                        ("LoadVolumeCfgFromRegistry: Read Value %S Value = 0x%x\n",
                                            VOLUME_OUT_OF_SYNC_ERRORSTATUS, ulValue));
                }
            } else {
                    InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, 
                        ("LoadVolumeCfgFromRegistry: Failed to Read Value %S with error = 0x%x\n",
                                            VOLUME_OUT_OF_SYNC_ERRORSTATUS, Status));
            }
    
            pVolumeContext->ulOutOfSyncErrorStatus = ulValue;
    
            ULONG           ulcbRead = 0;
            ULARGE_INTEGER  uliDefaultData;
            PVOID           pAddress = &pVolumeContext->liOutOfSyncTimeStamp;
    
            uliDefaultData.QuadPart = 0;
            Status = pVolumeParam->ReadBinary(VOLUME_OUT_OF_SYNC_TIMESTAMP_IN_GMT, 
                                              STRING_LEN_NULL_TERMINATED, 
                                              &pAddress,
                                              sizeof(ULARGE_INTEGER),
                                              &ulcbRead,
                                              (PVOID)&uliDefaultData,
                                              sizeof(ULARGE_INTEGER));
            if (NT_SUCCESS(Status)) {
                ASSERT(sizeof(ULARGE_INTEGER) == ulcbRead);
                if (STATUS_INMAGE_OBJECT_CREATED == Status) {
                    InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                        ("LoadVolumeCfgFromRegistry: Created %S with Default Value %#I64x\n",
                                            VOLUME_OUT_OF_SYNC_TIMESTAMP, 0));
                } else {
                    InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                        ("LoadVolumeCfgFromRegistry: Read Value %S Value = %#I64x\n",
                                            VOLUME_OUT_OF_SYNC_TIMESTAMP, pVolumeContext->liOutOfSyncTimeStamp.QuadPart));
                }
            } else {
                    InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, 
                        ("LoadVolumeCfgFromRegistry: Failed to Read Value %S with error = %#x\n",
                                            VOLUME_OUT_OF_SYNC_TIMESTAMP, Status));
            }
    
        } else {
            pVolumeContext->bResyncRequired = false;
        }
    }

    // Get data filtering values.
    if (DriverContext.bEnableDataCapture) {
        if (DriverContext.bEnableDataFilteringForNewVolumes) {
            ulVolumeDataCapture = 0x01;
        } else {
            ulVolumeDataCapture = 0x00;
        }
        Status = pVolumeParam->ReadULONG(VOLUME_DATA_CAPTURE, (ULONG32 *)&ulValue,
                                         ulVolumeDataCapture, TRUE);
        if (NT_SUCCESS(Status)) {
            if (STATUS_INMAGE_OBJECT_CREATED == Status) {
                InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                    ("LoadVolumeCfgFromRegistry: Created %S with Default Value 0x%x\n",
                                        VOLUME_DATA_CAPTURE, ulVolumeDataCapture));
            } else {
                InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                    ("LoadVolumeCfgFromRegistry: Read Value %S Value = 0x%x\n",
                                        VOLUME_DATA_CAPTURE, ulValue));
            }
        } else {
                InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, 
                    ("LoadVolumeCfgFromRegistry: Failed to Read Value %S with error = 0x%x\n",
                                        VOLUME_DATA_CAPTURE, Status));
        }

        if (0 == ulValue) {
            pVolumeContext->ulFlags |= VCF_DATA_CAPTURE_DISABLED;
            InVolDbgPrint(DL_WARNING, DM_VOLUME_CONTEXT,
                          ("LoadVolumeCfgFromRegistry: Setting data capture disabled for volume %S(%S)\n",
                            pVolumeContext->wGUID, pVolumeContext->wDriveLetter));
        }

        if (DriverContext.bEnableDataFilesForNewVolumes) {
            ulVolumeDataFiles = 0x01;
        } else {
            ulVolumeDataFiles = 0x00;
        }

        Status = pVolumeParam->ReadULONG(VOLUME_DATA_FILES, (ULONG32 *)&ulValue,
                                         ulVolumeDataFiles, TRUE);
        if (NT_SUCCESS(Status)) {
            if (STATUS_INMAGE_OBJECT_CREATED == Status) {
                InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                    ("LoadVolumeCfgFromRegistry: Created %S with Default Value 0x%x\n",
                                        VOLUME_DATA_FILES, ulVolumeDataFiles));
            } else {
                InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                    ("LoadVolumeCfgFromRegistry: Read Value %S Value = 0x%x\n",
                                        VOLUME_DATA_FILES, ulValue));
            }
        } else {
                InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, 
                    ("LoadVolumeCfgFromRegistry: Failed to Read Value %S with error = 0x%x\n",
                                        VOLUME_DATA_FILES, Status));
        }

        if (0 == ulValue) {
            pVolumeContext->ulFlags |= VCF_DATA_FILES_DISABLED;
            InVolDbgPrint(DL_WARNING, DM_VOLUME_CONTEXT,
                          ("LoadVolumeCfgFromRegistry: Setting data files disabled for volume %S(%S)\n",
                            pVolumeContext->wGUID, pVolumeContext->wDriveLetter));
        }

        Status = pVolumeParam->ReadULONG(VOLUME_DATA_TO_DISK_LIMIT_IN_MB, (ULONG32 *)&ulValue,
                                            DriverContext.ulDataToDiskLimitPerVolumeInMB, TRUE);
        if (NT_SUCCESS(Status)) {
            if (STATUS_INMAGE_OBJECT_CREATED == Status) {
                InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                    ("LoadVolumeCfgFromRegistry: Created %S with Default Value 0x%x\n",
                                        VOLUME_DATA_TO_DISK_LIMIT_IN_MB, DriverContext.ulDataToDiskLimitPerVolumeInMB));
            } else {
                InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                    ("LoadVolumeCfgFromRegistry: Read Value %S Value = %#x\n",
                                        VOLUME_DATA_TO_DISK_LIMIT_IN_MB, ulValue));
            }
        } else {
            InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, 
                ("LoadVolumeCfgFromRegistry: Failed to Read Value %S with error = %#x, initialized to default value %#x\n",
                                    VOLUME_DATA_TO_DISK_LIMIT_IN_MB, Status, ulValue));
        }

        if (0 != ulValue) {
            pVolumeContext->ullcbDataToDiskLimit = ulValue;
            pVolumeContext->ullcbDataToDiskLimit *= MEGABYTES;
            InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                ("LoadVolumeCfgFromRegistry: Read %S with Value 0x%x\n",
                                    VOLUME_DATA_TO_DISK_LIMIT_IN_MB, ulValue));
        }

        Status = pVolumeParam->ReadULONG(VOLUME_MIN_FREE_DATA_TO_DISK_LIMIT_IN_MB, (ULONG32 *)&ulValue,
                                            DriverContext.ulMinFreeDataToDiskLimitPerVolumeInMB, TRUE);
        if (NT_SUCCESS(Status)) {
            if (STATUS_INMAGE_OBJECT_CREATED == Status) {
                InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                    ("LoadVolumeCfgFromRegistry: Created %S with Default Value 0x%x\n",
                                        VOLUME_MIN_FREE_DATA_TO_DISK_LIMIT_IN_MB, DriverContext.ulMinFreeDataToDiskLimitPerVolumeInMB));
            } else {
                InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                    ("LoadVolumeCfgFromRegistry: Read Value %S Value = %#x\n",
                                        VOLUME_MIN_FREE_DATA_TO_DISK_LIMIT_IN_MB, ulValue));
            }
        } else {
            InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, 
                ("LoadVolumeCfgFromRegistry: Failed to Read Value %S with error = %#x, initialized to default value %#x\n",
                                    VOLUME_MIN_FREE_DATA_TO_DISK_LIMIT_IN_MB, Status, ulValue));
        }

        if (0 != ulValue) {
            if((ulValue * MEGABYTES) > pVolumeContext->ullcbDataToDiskLimit){
                pVolumeContext->ullcbMinFreeDataToDiskLimit = (ULONG)pVolumeContext->ullcbDataToDiskLimit;
            } else {
                pVolumeContext->ullcbMinFreeDataToDiskLimit = ulValue;
                pVolumeContext->ullcbMinFreeDataToDiskLimit *= MEGABYTES;
                InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                   ("LoadVolumeCfgFromRegistry: Read %S with Value 0x%x\n",
                                    VOLUME_MIN_FREE_DATA_TO_DISK_LIMIT_IN_MB, ulValue));
            }
        }

        Status = pVolumeParam->ReadULONG(VOLUME_DATA_NOTIFY_LIMIT_IN_KB, (ULONG32 *)&ulValue,
                                            DriverContext.ulDataNotifyThresholdInKB, TRUE);
        if (NT_SUCCESS(Status)) {
            if (STATUS_INMAGE_OBJECT_CREATED == Status) {
                InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                    ("LoadVolumeCfgFromRegistry: Created %S with Default Value 0x%x\n",
                                        VOLUME_DATA_NOTIFY_LIMIT_IN_KB, DriverContext.ulDataNotifyThresholdInKB));
            } else {
                InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                    ("LoadVolumeCfgFromRegistry: Read Value %S Value = %#x\n",
                                        VOLUME_DATA_NOTIFY_LIMIT_IN_KB, ulValue));
            }
        } else {
            InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, 
                ("LoadVolumeCfgFromRegistry: Failed to Read Value %S with error = %#x, initialized to default value %#x\n",
                                    VOLUME_DATA_NOTIFY_LIMIT_IN_KB, Status, ulValue));
        }

        if (0 != ulValue) {
            pVolumeContext->ulcbDataNotifyThreshold = ulValue;
            pVolumeContext->ulcbDataNotifyThreshold *= KILOBYTES;
            InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                ("LoadVolumeCfgFromRegistry: Read %S with Value 0x%x\n",
                                    VOLUME_DATA_NOTIFY_LIMIT_IN_KB, ulValue));
        }

        Status = pVolumeParam->ReadULONG(VOLUME_FILE_WRITER_THREAD_PRIORITY, (ULONG32 *)&pVolumeContext->FileWriterThreadPriority,
                                            DriverContext.FileWriterThreadPriority, TRUE);
        if (NT_SUCCESS(Status)) {
            if (STATUS_INMAGE_OBJECT_CREATED == Status) {
                InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                    ("LoadVolumeCfgFromRegistry: Created %S with Default Value 0x%x\n",
                                        VOLUME_FILE_WRITER_THREAD_PRIORITY, DriverContext.FileWriterThreadPriority));
            } else {
                InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                    ("LoadVolumeCfgFromRegistry: Read Value %S Value = %#x\n",
                                        VOLUME_FILE_WRITER_THREAD_PRIORITY, pVolumeContext->FileWriterThreadPriority));
            }
        } else {
            InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, 
                ("LoadVolumeCfgFromRegistry: Failed to Read Value %S with error = %#x, initialized to default value %#x\n",
                                    VOLUME_PAGEFILE_WRITES_IGNORED, Status, pVolumeContext->FileWriterThreadPriority));
        }

        Status = pVolumeParam->ReadULONG(VOLUME_FILE_WRITER_ID, (ULONG32 *)&pVolumeContext->ulWriterId, 0, FALSE);
        if (NT_SUCCESS(Status)) {
            InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                ("LoadVolumeCfgFromRegistry: Read Value %S Value = %#x\n",
                                    VOLUME_FILE_WRITER_ID, pVolumeContext->ulWriterId));
        } else {
            InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                ("LoadVolumeCfgFromRegistry: Failed to find Value %S with error = %#x, initialized to default value %#x\n",
                                    VOLUME_FILE_WRITER_ID, Status, pVolumeContext->ulWriterId));
        }

        // Read the data file directory. We store data log files at this location.
        Status = pVolumeParam->ReadUnicodeString(VOLUME_DATALOG_DIRECTORY, STRING_LEN_NULL_TERMINATED, 
                                                 &pVolumeContext->DataLogDirectory);

        InitializeDataLogDirectory(pVolumeContext, DriverContext.DataLogDirectory.Buffer);
        pVolumeContext->VolumeFileWriter = DriverContext.DataFileWriterManager->NewVolume(pVolumeContext);
    } else {
        // Globally data filtering is disabled.
        InVolDbgPrint(DL_WARNING, DM_VOLUME_CONTEXT,
                      ("LoadVolumeCfgFromRegistry: Using global override and setting data capture disabled for volume %S(%S)\n",
                        pVolumeContext->wGUID, pVolumeContext->wDriveLetter));
        pVolumeContext->ulFlags |= VCF_DATA_CAPTURE_DISABLED;
    }

    Status = pVolumeParam->ReadULONG(VOLUME_PAGEFILE_WRITES_IGNORED, (ULONG32 *)&ulValue, 
                                        VOLUME_PAGEFILE_WRITES_IGNORED_DEFAULT_VALUE, TRUE);
    if (NT_SUCCESS(Status)) {
        if (STATUS_INMAGE_OBJECT_CREATED == Status) {
            InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                ("LoadVolumeCfgFromRegistry: Created %S with Default Value 0x%x\n",
                                    VOLUME_PAGEFILE_WRITES_IGNORED, VOLUME_PAGEFILE_WRITES_IGNORED_DEFAULT_VALUE));
        } else {
            InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                ("LoadVolumeCfgFromRegistry: Read Value %S Value = 0x%x\n",
                                    VOLUME_PAGEFILE_WRITES_IGNORED, ulValue));
        }
    } else {
            InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, 
                ("LoadVolumeCfgFromRegistry: Failed to Read Value %S with error = 0x%x\n",
                                    VOLUME_PAGEFILE_WRITES_IGNORED, Status));
    }

    if (0 != ulValue) {
        pVolumeContext->ulFlags |= VCF_PAGEFILE_WRITES_IGNORED;
    } else {
        if ( 0 == (pVolumeContext->ulFlags & VCF_DATA_CAPTURE_DISABLED)) {
            // Page file writes are tracked and data filtering is enabled this will lead to recursion.
            // Settings Ignore page file writes.
            InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT,
                ("LoadVolumeCfgFromRegistry: Data capture enabled and page file writes are not ignored for Volume %S(%S).\n"
                 "This will lead to recursion. Setting to ingore page file writes\n",
                 pVolumeContext->wGUID, pVolumeContext->wDriveLetter));
            pVolumeContext->ulFlags |= VCF_PAGEFILE_WRITES_IGNORED;
            pVolumeParam->WriteULONG(VOLUME_PAGEFILE_WRITES_IGNORED, VOLUME_PAGEFILE_WRITES_IGNORED_SET);
        }
    }

    if (pVolumeContext->ulFlags & VCF_VOLUME_LETTER_OBTAINED) {
        WCHAR   DriveLetters[MAX_NUM_DRIVE_LETTERS + 1] = {0};

        Status = ConvertDriveLetterBitmapToDriveLetterString(&pVolumeContext->DriveLetterBitMap, 
                                                    DriveLetters,
                                                    (MAX_NUM_DRIVE_LETTERS + 1)*sizeof(WCHAR));
        if (NT_SUCCESS(Status)) {
            Status = pVolumeParam->WriteWString(VOLUME_LETTER, DriveLetters);
            if (NT_SUCCESS(Status)) {
                InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
                    ("LoadVolumeCfgFromRegistry: Added DriveLetter(s) %S for GUID %S\n",
                                            DriveLetters, pVolumeContext->wGUID));
            } else {
                InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, 
                    ("LoadVolumeCfgFromRegistry: Adding DriveLetter %S for GUID %S Failed with Status 0x%x\n",
                                            DriveLetters, pVolumeContext->wGUID, Status));
            }
        } else {
                InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, 
                    ("LoadVolumeCfgFromRegistry: ConvertDriveLetterBitmapToDriveLetterString for GUID %S Failed with Status 0x%x\n",
                                            pVolumeContext->wGUID, Status));
        }
    }

    delete pVolumeParam;
    
    return STATUS_SUCCESS;
}


/*-----------------------------------------------------------------------------
 * Function : AddVolumeLetterToRegVolumeCfg
 *
 * Args -
 *  pVolumeContext - Points to volume for which drive letter has to be added.
 *
 * Notes 
 *  This function uses VolumeParameters field in VOLUME_CONTEXT. VolumeParamets
 *  specifies the registry key under which the volume configuration is stored.
 *  
 *  As this function does registry operations, IRQL should be < DISPATCH_LEVEL
 *-----------------------------------------------------------------------------
 */
NTSTATUS
AddVolumeLetterToRegVolumeCfg(PVOLUME_CONTEXT pVolumeContext)
{
    NTSTATUS    Status = STATUS_SUCCESS;
    Registry    *pVolumeParam = NULL;
    ULONG       ulValue;
    WCHAR       DriveLetters[MAX_NUM_DRIVE_LETTERS + 1] = {0};

    ASSERT((NULL != pVolumeContext) && 
            (pVolumeContext->ulFlags & VCF_GUID_OBTAINED) &&
            (pVolumeContext->ulFlags & VCF_VOLUME_LETTER_OBTAINED));
    PAGED_CODE();

    InVolDbgPrint(DL_FUNC_TRACE, DM_VOLUME_CONTEXT, 
        ("AddVolumeLetterToRegVolumeCfg: Called - VolumeContext(%p)\n", pVolumeContext));

    if (NULL == pVolumeContext) {
        InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, ("AddVolumeLetterToRegVolumeCfg: pVolumeContext NULL\n"));
        return STATUS_INVALID_PARAMETER_1;
    }

    if (!(pVolumeContext->ulFlags & VCF_GUID_OBTAINED)) {
        InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, ("AddVolumeLetterToRegVolumeCfg: Called before GUID is obtained\n"));
        return STATUS_INVALID_PARAMETER_1;
    }

    if (!(pVolumeContext->ulFlags & VCF_VOLUME_LETTER_OBTAINED)) {
        InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, ("AddVolumeLetterToRegVolumeCfg: Called before volume letter is obtained\n"));
        return STATUS_INVALID_PARAMETER_1;
    }

    Status = ConvertDriveLetterBitmapToDriveLetterString(&pVolumeContext->DriveLetterBitMap, 
                                                DriveLetters,
                                                (MAX_NUM_DRIVE_LETTERS + 1)*sizeof(WCHAR));
    if (!NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, 
            ("AddVolumeLetterToRegVolumeCfg: ConvertDriveLetterBitmapToDriveLetterString Failed with Status 0x%x\n",
                                    Status));
        return STATUS_INVALID_PARAMETER_1;
    }

    // Let us open/create the registry key for this volume.
    Status = OpenVolumeParametersRegKey(pVolumeContext, &pVolumeParam);
    if (NULL == pVolumeParam) {
        InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, 
                ("AddVolumeLetterToRegVolumeCfg: Failed to open volume parameters key"));
        return Status;
    }    
    
    Status = pVolumeParam->WriteWString(VOLUME_LETTER, DriveLetters);
    if (NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
            ("AddVolumeLetterToRegVolumeCfg: Added DriveLetter(s) %S for GUID %S\n",
                                    DriveLetters, pVolumeContext->wGUID));
    } else {
        InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, 
            ("AddVolumeLetterToRegVolumeCfg: Adding DriveLetter(s) %S for GUID %S Failed with Status 0x%x\n",
                                    DriveLetters, pVolumeContext->wGUID, Status));
    }

    delete pVolumeParam;
    
    return STATUS_SUCCESS;
}

NTSTATUS
SetDWORDInRegVolumeCfg(PVOLUME_CONTEXT pVolumeContext, PWCHAR pValueName, ULONG ulValue, bool bFlush)
{
    NTSTATUS    Status = STATUS_SUCCESS;
    Registry    *pVolumeParam = NULL;

    ASSERT((NULL != pVolumeContext) && 
            (pVolumeContext->ulFlags & VCF_GUID_OBTAINED));
    PAGED_CODE();

    InVolDbgPrint(DL_FUNC_TRACE, DM_VOLUME_CONTEXT, 
        ("SetDWORDInRegVolumeCfg: Called - VolumeContext(%p)\n", pVolumeContext));

    if (NULL == pVolumeContext) {
        InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, ("SetDWORDInRegVolumeCfg: pVolumeContext NULL\n"));
        return STATUS_INVALID_PARAMETER_1;
    }

    if (!(pVolumeContext->ulFlags & VCF_GUID_OBTAINED)) {
        InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, ("SetDWORDInRegVolumeCfg: Called before GUID is obtained\n"));
        return STATUS_INVALID_PARAMETER_1;
    }

    // Let us open/create the registry key for this volume.
    Status = OpenVolumeParametersRegKey(pVolumeContext, &pVolumeParam);
    if (NULL == pVolumeParam) {
        InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, 
                ("SetDWORDInRegVolumeCfg: Failed to open volume parameters key"));
        return Status;
    }
    
    Status = pVolumeParam->WriteULONG(pValueName, ulValue);
    if (NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
            ("SetDWORDInRegVolumeCfg: Added %S with value %#x for GUID %S\n", pValueName, ulValue,
                                    pVolumeContext->wGUID));
        if (bFlush) {
            Status = pVolumeParam->FlushKey();
        }
    } else {
        InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, 
            ("SetDWORDInRegVolumeCfg: Adding %S with value %#x for GUID %S Failed with Status 0x%x\n",
                                    pValueName, ulValue, pVolumeContext->wGUID, Status));
    }

    delete pVolumeParam;
    
    return STATUS_SUCCESS;
}

NTSTATUS
SetStringInRegVolumeCfg(PVOLUME_CONTEXT pVolumeContext, PWCHAR pValueName, PUNICODE_STRING pValue)
{
    NTSTATUS    Status = STATUS_SUCCESS;
    Registry    *pVolumeParam = NULL;

    ASSERT((NULL != pVolumeContext) && 
            (pVolumeContext->ulFlags & VCF_GUID_OBTAINED));
    PAGED_CODE();

    InVolDbgPrint(DL_FUNC_TRACE, DM_VOLUME_CONTEXT, 
        ("SetStringInRegVolumeCfg: Called - VolumeContext(%p)\n", pVolumeContext));

    if (NULL == pVolumeContext) {
        InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, ("SetStringInRegVolumeCfg: pVolumeContext NULL\n"));
        return STATUS_INVALID_PARAMETER_1;
    }

    if (!(pVolumeContext->ulFlags & VCF_GUID_OBTAINED)) {
        InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, ("SetStringInRegVolumeCfg: Called before GUID is obtained\n"));
        return STATUS_INVALID_PARAMETER_1;
    }

    // Let us open/create the registry key for this volume.
    Status = OpenVolumeParametersRegKey(pVolumeContext, &pVolumeParam);
    if (NULL == pVolumeParam) {
        InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, 
                ("SetStringInRegVolumeCfg: Failed to open volume parameters key"));
        return Status;
    }
    
    Status = pVolumeParam->WriteUnicodeString(pValueName, pValue);
    if (NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_INFO, DM_VOLUME_CONTEXT, 
            ("SetStringInRegVolumeCfg: Added %S with value %wZ for GUID %S\n", pValueName, pValue,
                                    pVolumeContext->wGUID));
    } else {
        InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT, 
            ("SetStringInRegVolumeCfg: Adding %S with value %wZ for GUID %S Failed with Status 0x%x\n",
                                    pValueName, pValue, pVolumeContext->wGUID, Status));
    }

    delete pVolumeParam;
    
    return Status;
}

/*-----------------------------------------------------------------------------
 *  Functions releated to Allocate, Deallocate, Reference and Dereference 
 *  VolumeContext
 *-----------------------------------------------------------------------------
 */
VOID
DeallocateVolumeContext(PVOLUME_CONTEXT   pVolumeContext);

PVOLUME_CONTEXT
AllocateVolumeContext(VOID)
{
    PVOLUME_CONTEXT     pVolumeContext = NULL;
    ULONG               ulAllocationSize;
    USHORT              usVolumeParametersSize;

    // Allocate enough memory for VOLUME_CONTEXT and for the buffer to store 
    // Volume Parameters registry key and out of sync timestamp.
    usVolumeParametersSize = DriverContext.DriverParameters.Length +
                                VOLUME_NAME_SIZE_IN_BYTES + sizeof(WCHAR);
    ulAllocationSize = sizeof(VOLUME_CONTEXT) + usVolumeParametersSize ;

    pVolumeContext = (PVOLUME_CONTEXT) ExAllocatePoolWithTag(
                                NonPagedPool, 
                                ulAllocationSize, 
                                TAG_VOLUME_CONTEXT);
    if (pVolumeContext) {
        RtlZeroMemory(pVolumeContext, ulAllocationSize);
        pVolumeContext->pReSyncWorkQueueEntry = AllocateWorkQueueEntry();
        if (NULL == pVolumeContext->pReSyncWorkQueueEntry) {
            ExFreePoolWithTag(pVolumeContext, TAG_VOLUME_CONTEXT);
            return NULL;
        }
        pVolumeContext->lRefCount = 1;
        pVolumeContext->uliTransactionId.QuadPart = 1;
        pVolumeContext->bNotify = false;
        pVolumeContext->eContextType = ecVolumeContext;

        InitializeChangeList(&pVolumeContext->ChangeList);

        pVolumeContext->eDSDBCdeviceState = ecDSDBCdeviceOnline;
        pVolumeContext->eCaptureMode = ecCaptureModeUninitialized;
        pVolumeContext->ePrevWOState = ecWriteOrderStateUnInitialized;
        pVolumeContext->eWOState = ecWriteOrderStateUnInitialized;
        pVolumeContext->liTickCountAtLastCaptureModeChange.QuadPart = 0;
        pVolumeContext->liTickCountAtLastWOStateChange.QuadPart = 0;

        RtlInitializeBitMap(&pVolumeContext->DriveLetterBitMap, 
                            &pVolumeContext->BufferForDriveLetterBitMap,
                            MAX_NUM_DRIVE_LETTERS);

        KeInitializeSpinLock(&pVolumeContext->Lock);
        KeInitializeMutex(&pVolumeContext->Mutex, 0);
        KeInitializeMutex(&pVolumeContext->BitmapOpenCloseMutex, 0);
        KeInitializeEvent(&pVolumeContext->SyncEvent, SynchronizationEvent, TRUE);

        pVolumeContext->UniqueVolumeName.MaximumLength = 
                MAX_UNIQUE_VOLUME_NAME_SIZE_IN_WCHARS * sizeof(WCHAR);
        pVolumeContext->UniqueVolumeName.Buffer =
                        (PWCHAR)pVolumeContext->BufferForUniqueVolumeName;

        pVolumeContext->BitmapFileName.MaximumLength =
                MAX_LOG_PATHNAME * sizeof(WCHAR);
        pVolumeContext->BitmapFileName.Buffer = 
                        (PWCHAR)pVolumeContext->BufferForBitmapFileName;

        pVolumeContext->VolumeParameters.MaximumLength = usVolumeParametersSize;
        pVolumeContext->VolumeParameters.Buffer = 
                (PWCHAR)((PCHAR)pVolumeContext + sizeof(VOLUME_CONTEXT));

        pVolumeContext->wDriveLetter[1] = L':';
        pVolumeContext->DriveLetter[1] = L':';

        InitializeListHead(&pVolumeContext->ReservedDataBlockList);
        pVolumeContext->LastCommittedTimeStamp = 0;
        pVolumeContext->LastCommittedSequenceNumber = 0;
        pVolumeContext->LastCommittedSplitIoSeqId = 0;
        // Added for Debugging - Timestamp/SequenceNumber/SequenceId read on opening either Bitmap or Registry on reboot
        pVolumeContext->LastCommittedTimeStampReadFromStore = 0;
        pVolumeContext->LastCommittedSequenceNumberReadFromStore = 0;
        pVolumeContext->LastCommittedSplitIoSeqIdReadFromStore = 0;

        pVolumeContext->ulIoSizeArray[0] = VC_DEFAULT_IO_SIZE_BUCKET_0;
        pVolumeContext->ulIoSizeArray[1] = VC_DEFAULT_IO_SIZE_BUCKET_1;
        pVolumeContext->ulIoSizeArray[2] = VC_DEFAULT_IO_SIZE_BUCKET_2;
        pVolumeContext->ulIoSizeArray[3] = VC_DEFAULT_IO_SIZE_BUCKET_3;
        pVolumeContext->ulIoSizeArray[4] = VC_DEFAULT_IO_SIZE_BUCKET_4;
        pVolumeContext->ulIoSizeArray[5] = VC_DEFAULT_IO_SIZE_BUCKET_5;
        pVolumeContext->ulIoSizeArray[6] = VC_DEFAULT_IO_SIZE_BUCKET_6;
        pVolumeContext->ulIoSizeArray[7] = VC_DEFAULT_IO_SIZE_BUCKET_7;
        pVolumeContext->ulIoSizeArray[8] = VC_DEFAULT_IO_SIZE_BUCKET_8;
        pVolumeContext->ulIoSizeArray[9] = VC_DEFAULT_IO_SIZE_BUCKET_9;
        pVolumeContext->ulIoSizeArray[10] = VC_DEFAULT_IO_SIZE_BUCKET_10;
        pVolumeContext->ulIoSizeArray[11] = 0;

        pVolumeContext->VolumeProfile.ReadProfile.ReadIoSizeArray[0] = VC_DEFAULT_IO_SIZE_BUCKET_0;
        pVolumeContext->VolumeProfile.ReadProfile.ReadIoSizeArray[1] = VC_DEFAULT_IO_SIZE_BUCKET_1;
        pVolumeContext->VolumeProfile.ReadProfile.ReadIoSizeArray[2] = VC_DEFAULT_IO_SIZE_BUCKET_2;
        pVolumeContext->VolumeProfile.ReadProfile.ReadIoSizeArray[3] = VC_DEFAULT_IO_SIZE_BUCKET_3;
        pVolumeContext->VolumeProfile.ReadProfile.ReadIoSizeArray[4] = VC_DEFAULT_IO_SIZE_BUCKET_4;
        pVolumeContext->VolumeProfile.ReadProfile.ReadIoSizeArray[5] = VC_DEFAULT_IO_SIZE_BUCKET_5;
        pVolumeContext->VolumeProfile.ReadProfile.ReadIoSizeArray[6] = VC_DEFAULT_IO_SIZE_BUCKET_6;
        pVolumeContext->VolumeProfile.ReadProfile.ReadIoSizeArray[7] = VC_DEFAULT_IO_SIZE_BUCKET_7;
        pVolumeContext->VolumeProfile.ReadProfile.ReadIoSizeArray[8] = VC_DEFAULT_IO_SIZE_BUCKET_8;
        pVolumeContext->VolumeProfile.ReadProfile.ReadIoSizeArray[9] = VC_DEFAULT_IO_SIZE_BUCKET_9;
        pVolumeContext->VolumeProfile.ReadProfile.ReadIoSizeArray[10] = VC_DEFAULT_IO_SIZE_BUCKET_10;
        pVolumeContext->VolumeProfile.ReadProfile.ReadIoSizeArray[11] = 0;
        pVolumeContext->VolumeProfile.ReadProfile.MostFrequentlyUsedBucket = 1;

        pVolumeContext->LatencyDistForNotify = new ValueDistribution(MAX_DISTRIBUTION_FOR_NOTIFY_LATENCY, 
                                                                    MAX_DISTRIBUTION_FOR_NOTIFY_LATENCY,
                                                                     (PULONG)NotifyLatencyBucketsInMicroSeconds);
        pVolumeContext->LatencyLogForNotify = new ValueLog(MAX_NOTIFY_LATENCY_LOG_SIZE);
        pVolumeContext->LatencyDistForCommit = new ValueDistribution(MAX_DISTRIBUTION_FOR_COMMIT_LATENCY, 
                                                                    MAX_DISTRIBUTION_FOR_COMMIT_LATENCY,
                                                                     (PULONG)CommitLatencyBucketsInMicroSeconds);
        pVolumeContext->LatencyLogForCommit = new ValueLog(MAX_COMMIT_LATENCY_LOG_SIZE);
        pVolumeContext->LatencyDistForS2DataRetrieval = new ValueDistribution(MAX_DISTRIBUTION_FOR_S2_DATA_RETRIEVAL_LATENCY,
                                                                              MAX_DISTRIBUTION_FOR_S2_DATA_RETRIEVAL_LATENCY,
                                                                              (PULONG)S2DataRetrievalLatencyBucketsInMicroSeconds);
        pVolumeContext->LatencyLogForS2DataRetrieval = new ValueLog(MAX_S2_DATA_RETRIEVAL_LATENCY_LOG_SIZE);

        pVolumeContext->bDataLogLimitReached = false;
        // By Default, Let's Move the changes to Temp Queue
        // Let's make it False, When we determine the Volume is a Non-Cluster Volume
        pVolumeContext->bQueueChangesToTempQueue = true;
        pVolumeContext->ulPendingTSOTransactionID.QuadPart = 0;
        pVolumeContext->TSOEndSequenceNumber = 0;
        pVolumeContext->TSOEndTimeStamp = 0;
        pVolumeContext->TSOSequenceId = 0;

        pVolumeContext->ulDiskNumber = 0xffffffff;
        pVolumeContext->ulMaxDataSizePerDataModeDirtyBlock = DriverContext.ulMaxDataSizePerDataModeDirtyBlock;
        // Added for debugging same tag seen twice issue
        pVolumeContext->ulTagsinWOState = 0;
        pVolumeContext->ulTagsinNonWOSDrop = 0;
        pVolumeContext->MDFirstChangeTS = 0;
        pVolumeContext->EndTimeStampofDB = 0;
        pVolumeContext->NTFSDevObj = NULL;
   }

    return pVolumeContext;
}

// This function is called with volume context lock held
VOID
AddDirtyBlockToVolumeContext(
    PVOLUME_CONTEXT     VolumeContext,
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

    ASSERT((INVOLFLT_DATA_SOURCE_DATA == DirtyBlock->ulDataSource) || (DirtyBlock->eWOState != ecWriteOrderStateData));

    // Generate transaction ID
    DirtyBlock->uliTransactionID.QuadPart = VolumeContext->uliTransactionId.QuadPart++;
    if (0 == DirtyBlock->uliTransactionID.QuadPart)
        DirtyBlock->uliTransactionID.QuadPart = VolumeContext->uliTransactionId.QuadPart++;

    if (VolumeContext->bQueueChangesToTempQueue && IsListEmpty(&VolumeContext->ChangeList.TempQueueHead)) {
        ASSERT(NULL == VolumeContext->ChangeList.CurrentNode);
        GenerateTimeStampTag(&DirtyBlock->TagTimeStampOfFirstChange);
        DirtyBlock->liTickCountAtFirstIO = KeQueryPerformanceCounter(NULL);
        InsertTailList(&VolumeContext->ChangeList.TempQueueHead, &ChangeNode->ListEntry);
    } else if (!VolumeContext->bQueueChangesToTempQueue && IsListEmpty(&VolumeContext->ChangeList.Head)) { 
        // Let's see any changes added to temp queue before ioctl MountDevQueryUniqueId for non-cluster volume
        if (!IsListEmpty(&VolumeContext->ChangeList.TempQueueHead)) {
            ASSERT(!IS_CLUSTER_VOLUME(VolumeContext));
            MoveUnupdatedDirtyBlocks(VolumeContext);
        } else {
            ASSERT(NULL == VolumeContext->ChangeList.CurrentNode);
        }

        GenerateTimeStampTag(&DirtyBlock->TagTimeStampOfFirstChange);
        DirtyBlock->liTickCountAtFirstIO = KeQueryPerformanceCounter(NULL);
        InsertTailList(&VolumeContext->ChangeList.Head, &ChangeNode->ListEntry);
    } else {
        ASSERT(NULL != VolumeContext->ChangeList.CurrentNode);

        PCHANGE_NODE    LastChangeNode = VolumeContext->ChangeList.CurrentNode;

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
            AddLastChangeTimestampToCurrentNode(VolumeContext, NULL);
            GenerateTimeStampTag(&DirtyBlock->TagTimeStampOfFirstChange);
            DirtyBlock->liTickCountAtFirstIO = KeQueryPerformanceCounter(NULL);
        }

        if (VolumeContext->bQueueChangesToTempQueue) {
            InsertTailList(&VolumeContext->ChangeList.TempQueueHead, &ChangeNode->ListEntry);
        } else {
            InsertTailList(&VolumeContext->ChangeList.Head, &ChangeNode->ListEntry);
        }

    }

    VolumeContext->ChangeList.CurrentNode = ChangeNode;
    VolumeContext->ChangeList.lDirtyBlocksInQueue++;

    if (DirtyBlock->eWOState == ecWriteOrderStateData &&
            DirtyBlock->ulDataSource == INVOLFLT_DATA_SOURCE_DATA) {
                VolumeContext->NumChangeNodeInDataWOSCreated++;
    }

    if(!VolumeContext->bQueueChangesToTempQueue) {

        if (DirtyBlock->eWOState != ecWriteOrderStateData &&
                DirtyBlock->ulDataSource == INVOLFLT_DATA_SOURCE_DATA) {
            InsertTailList(&VolumeContext->ChangeList.DataCaptureModeHead, &ChangeNode->DataCaptureModeListEntry);
            ChangeNode->NumChangeNodeInDataWOSAhead = VolumeContext->NumChangeNodeInDataWOSCreated;
        }
    }
}

/**
 * AddDirtyBlockToVolumeContext - This is used to add a new dirty block with known time stamp
 * @param VolumeContext - Volumes context to which dirty block has to be added
 * @param ppTimeStamp - If this is NULL, function updates with pointer to generated timestamp
 * If not NULL, this is used as the first change timestamp. This points to memory
 *                      inside dirty block passed to function. DO NOT free the timestamp
 */
VOID
AddDirtyBlockToVolumeContext(
    PVOLUME_CONTEXT     VolumeContext,
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
    if (ecCaptureModeData == VolumeContext->eCaptureMode)
        DirtyBlock->ulDataSource = INVOLFLT_DATA_SOURCE_DATA;
    else
        DirtyBlock->ulDataSource = INVOLFLT_DATA_SOURCE_META_DATA;

    ASSERT((INVOLFLT_DATA_SOURCE_DATA == DirtyBlock->ulDataSource) || (DirtyBlock->eWOState != ecWriteOrderStateData));

    // Generate transaction ID
    DirtyBlock->uliTransactionID.QuadPart = VolumeContext->uliTransactionId.QuadPart++;
    if (0 == DirtyBlock->uliTransactionID.QuadPart)
        DirtyBlock->uliTransactionID.QuadPart = VolumeContext->uliTransactionId.QuadPart++;

    if (VolumeContext->bQueueChangesToTempQueue) {
        if (IsListEmpty(&VolumeContext->ChangeList.TempQueueHead)) {
            ASSERT(NULL == VolumeContext->ChangeList.CurrentNode);
            InsertTailList(&VolumeContext->ChangeList.TempQueueHead, &ChangeNode->ListEntry);
        } else {
            ASSERT(NULL != VolumeContext->ChangeList.CurrentNode);
            AddLastChangeTimestampToCurrentNode(VolumeContext,ppDataBlock);
            InsertTailList(&VolumeContext->ChangeList.TempQueueHead, &ChangeNode->ListEntry);
        }
    } else {
        if (IsListEmpty(&VolumeContext->ChangeList.Head)) {
            if (IsListEmpty(&VolumeContext->ChangeList.TempQueueHead)) {
                ASSERT(NULL == VolumeContext->ChangeList.CurrentNode);
                InsertTailList(&VolumeContext->ChangeList.Head, &ChangeNode->ListEntry);
            } else {
                ASSERT(!IS_CLUSTER_VOLUME(VolumeContext));
                MoveUnupdatedDirtyBlocks(VolumeContext);
                InsertTailList(&VolumeContext->ChangeList.Head, &ChangeNode->ListEntry);
            }
        } else {
            ASSERT(NULL != VolumeContext->ChangeList.CurrentNode);
            AddLastChangeTimestampToCurrentNode(VolumeContext,ppDataBlock);
            InsertTailList(&VolumeContext->ChangeList.Head, &ChangeNode->ListEntry);
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

    VolumeContext->ChangeList.CurrentNode = ChangeNode;
    VolumeContext->ChangeList.lDirtyBlocksInQueue++;

    if (DirtyBlock->eWOState == ecWriteOrderStateData &&
            DirtyBlock->ulDataSource == INVOLFLT_DATA_SOURCE_DATA) {
                VolumeContext->NumChangeNodeInDataWOSCreated++;
    }    


    if(!VolumeContext->bQueueChangesToTempQueue) {
        if (DirtyBlock->eWOState != ecWriteOrderStateData &&
                DirtyBlock->ulDataSource == INVOLFLT_DATA_SOURCE_DATA) {
            InsertTailList(&VolumeContext->ChangeList.DataCaptureModeHead, &ChangeNode->DataCaptureModeListEntry);
            ChangeNode->NumChangeNodeInDataWOSAhead = VolumeContext->NumChangeNodeInDataWOSCreated;
        }
    }
}

VOID
AddLastChangeTimestampToCurrentNode(PVOLUME_CONTEXT VolumeContext, PDATA_BLOCK *ppDataBlock, bool bAllocate)
{
    PCHANGE_NODE    LastChangeNode = VolumeContext->ChangeList.CurrentNode;

    if (NULL != LastChangeNode) {

        if (ecChangeNodeDirtyBlock == LastChangeNode->eChangeNode) {
            PKDIRTY_BLOCK   LastDirtyBlock = LastChangeNode->DirtyBlock;
            ASSERT(NULL != LastDirtyBlock);

            if (0 == LastDirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601) {
                if (LastDirtyBlock->eWOState != ecWriteOrderStateData &&
                                LastDirtyBlock->ulDataSource == INVOLFLT_DATA_SOURCE_DATA) {
                    ASSERT(LastChangeNode->DataCaptureModeListEntry.Flink != NULL &&
                            LastChangeNode->DataCaptureModeListEntry.Blink != NULL);
                    GenerateTimeStampTag(&LastDirtyBlock->TagTimeStampOfFirstChange);                                
                }
                
                GenerateLastTimeStampTag(LastDirtyBlock);

                if (LastDirtyBlock->eWOState != ecWriteOrderStateData &&
                                LastDirtyBlock->ulDataSource == INVOLFLT_DATA_SOURCE_DATA) {
                    ASSERT(LastChangeNode->DataCaptureModeListEntry.Flink != NULL &&
                            LastChangeNode->DataCaptureModeListEntry.Blink != NULL);                                
                    ASSERT(LastDirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601 ==
                        LastDirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601);                                
                    ASSERT(LastDirtyBlock->TagTimeStampOfFirstChange.ullSequenceNumber ==
                        LastDirtyBlock->TagTimeStampOfLastChange.ullSequenceNumber);
                }

                LastDirtyBlock->liTickCountAtLastIO = KeQueryPerformanceCounter(NULL);
                if ((INVOLFLT_DATA_SOURCE_DATA == LastDirtyBlock->ulDataSource) &&
                    (LastDirtyBlock->ulcbDataFree)) 
                {
                    ASSERT(NULL != LastDirtyBlock->CurrentDataBlock);
                    ASSERT((LastDirtyBlock->ulcbDataFree > DriverContext.ulDataBlockSize) ||
                           (LastDirtyBlock->CurrentDataBlock->ulcbDataFree == LastDirtyBlock->ulcbDataFree));

                    InVolDbgPrint(DL_TRACE_L1, DM_DATA_FILTERING | DM_TAGS,
                                  ("AddLastChangeTimestampToCurrentNode: LastDataBlock contains data\n"));
                    ULONG ulTrailerSize = SetSVDLastChangeTimeStamp(LastDirtyBlock);
                    InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, ("AddLastChangeTimestampToCurrentNode: Have to skip the left over data VCReserved = %#x VCFree = %#x DBFree = %#x\n",
                                                               VolumeContext->ulcbDataReserved, VolumeContext->ulcbDataFree, 
                                                               LastDirtyBlock->ulcbDataFree));
                    VCAdjustCountersForLostDataBytes(VolumeContext, 
                                                     ppDataBlock, 
                                                     LastDirtyBlock->ulcbDataFree + ulTrailerSize,
                                                     bAllocate);
                    LastDirtyBlock->ulcbDataFree = 0;
                }
            }
        }else {
            if (VolumeContext->bResyncStartReceived == true)  {
                ULONGLONG TimeStamp = 0;
                KeQuerySystemTime((PLARGE_INTEGER)&TimeStamp);

                // "LastChangeNode is of type DataFile. So not closing it. Volume: %s (%s), Current timestamp: %s, Last ResyncStartTS: %s."
                InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                    INVOLFLT_ERR_FIRSTFILE_VALIDATION7, STATUS_SUCCESS,
                    VolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES,
                    VolumeContext->wGUID, GUID_SIZE_IN_BYTES,
                    TimeStamp,
                    VolumeContext->ResynStartTimeStamp);
            }
        }
    }else {
        if (VolumeContext->bResyncStartReceived == true)  {
            ULONGLONG TimeStamp = 0;
            KeQuerySystemTime((PLARGE_INTEGER)&TimeStamp);

            // "LastChangeNode is NULL. So can not close it. Volume: %s (%s), Current timestamp: %s, Last ResyncStartTS: %s."
            InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                INVOLFLT_ERR_FIRSTFILE_VALIDATION8, STATUS_SUCCESS,
                VolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES,
                VolumeContext->wGUID, GUID_SIZE_IN_BYTES,
                TimeStamp,
                VolumeContext->ResynStartTimeStamp);
        }
    }

}

VOID
PrependChangeNodeToVolumeContext(PVOLUME_CONTEXT VolumeContext, PCHANGE_NODE ChangeNode)
{
    PKDIRTY_BLOCK   DirtyBlock = NULL;

    ReferenceChangeNode(ChangeNode);

    InsertHeadList(&VolumeContext->ChangeList.Head, &ChangeNode->ListEntry);
    VolumeContext->ChangeList.lDirtyBlocksInQueue++;

    if (NULL == VolumeContext->ChangeList.CurrentNode) {
        VolumeContext->ChangeList.CurrentNode = ChangeNode;
    }

    if ((ecChangeNodeDataFile == ChangeNode->eChangeNode) || (ecChangeNodeDirtyBlock == ChangeNode->eChangeNode)) {
        DirtyBlock = ChangeNode->DirtyBlock;
        ASSERT(NULL != DirtyBlock);
        
        VolumeContext->ulChangesReverted += DirtyBlock->cChanges;
        VolumeContext->ulicbChangesReverted.QuadPart += DirtyBlock->ulicbChanges.QuadPart;
        VolumeContext->ulChangesPendingCommit -= DirtyBlock->cChanges;
        VolumeContext->ullcbChangesPendingCommit -= DirtyBlock->ulicbChanges.QuadPart;

        if ( (VolumeContext->bNotify) && (NULL != VolumeContext->DBNotifyEvent)) {
            ASSERT(VolumeContext->ChangeList.ullcbTotalChangesPending >= VolumeContext->ullcbChangesPendingCommit);
            ULONGLONG   ullcbTotalChangesPending = VolumeContext->ChangeList.ullcbTotalChangesPending - VolumeContext->ullcbChangesPendingCommit;
            if (ullcbTotalChangesPending >= VolumeContext->ulcbDataNotifyThreshold) {
                // This is more than threshold
                VolumeContext->bNotify = false;
                VolumeContext->liTickCountAtNotifyEventSet = KeQueryPerformanceCounter(NULL);
                KeSetEvent(VolumeContext->DBNotifyEvent, EVENT_INCREMENT, FALSE);
            }
        }
    }
    if (ChangeNode->DirtyBlock != NULL && ChangeNode->DirtyBlock->eWOState == ecWriteOrderStateData &&
            ChangeNode->DirtyBlock->ulDataSource == INVOLFLT_DATA_SOURCE_DATA) {
                VolumeContext->NumChangeNodeInDataWOSCreated++;
    }    

    return;
}

VOID
DeallocateVolumeContext(PVOLUME_CONTEXT   pVolumeContext)
{
    PVOLUME_GUID    pVolumeGUID;

    pVolumeGUID = pVolumeContext->GUIDList;
    while (NULL != pVolumeGUID) {
        PVOLUME_GUID pTemp = pVolumeGUID;
        pVolumeGUID = pTemp->NextGUID;
        ExFreePoolWithTag(pTemp, TAG_GENERIC_NON_PAGED);
    }

    ExFreePoolWithTag(pVolumeContext, TAG_VOLUME_CONTEXT);
    return;
}

PVOLUME_CONTEXT
ReferenceVolumeContext(PVOLUME_CONTEXT   pVolumeContext)
{
    ASSERT(pVolumeContext->lRefCount >= 1);

    InterlockedIncrement(&pVolumeContext->lRefCount);

    return pVolumeContext;
}

VOID
DereferenceVolumeContext(PVOLUME_CONTEXT   pVolumeContext)
{
    ASSERT(pVolumeContext->lRefCount > 0);
    ASSERT(NULL != pVolumeContext->pReSyncWorkQueueEntry);
    if (InterlockedDecrement(&pVolumeContext->lRefCount) <= 0)
    {
    	//Free WorkQueueEntry
    	ASSERT(0 == pVolumeContext->bReSyncWorkQueueRef);
        DereferenceWorkQueueEntry(pVolumeContext->pReSyncWorkQueueEntry);
        DeallocateVolumeContext(pVolumeContext);
    }

    return;
}

PCHANGE_NODE
GetDataModeChangeNode(PVOLUME_CONTEXT VolumeContext, bool bFromHead)
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

    if (IsListEmpty(&VolumeContext->ChangeList.DataCaptureModeHead)) {
        return ChangeNode;
    }

    ASSERT(!IsListEmpty(&VolumeContext->ChangeList.Head));
    ListEntry = (PLIST_ENTRY) RemoveHeadList(&VolumeContext->ChangeList.DataCaptureModeHead);
    ChangeNode = (PCHANGE_NODE) CONTAINING_RECORD((PCHAR) ListEntry, CHANGE_NODE, DataCaptureModeListEntry);
    ASSERT(ChangeNode != NULL);

    DirtyBlock = ChangeNode->DirtyBlock;
    ASSERT(DirtyBlock != NULL);
    ASSERT(DirtyBlock->eWOState != ecWriteOrderStateData && DirtyBlock->ulDataSource == INVOLFLT_DATA_SOURCE_DATA);

    DiffCount = ChangeNode->NumChangeNodeInDataWOSAhead - VolumeContext->NumChangeNodeInDataWOSDrained;
    ASSERT(DiffCount >= 0);

    if (!IsListEmpty(&VolumeContext->ChangeList.Head)) {
        PCHANGE_NODE FirstChangeNodeInMainQ = (PCHANGE_NODE) VolumeContext->ChangeList.Head.Flink;
        ASSERT(FirstChangeNodeInMainQ != NULL);
        
        PKDIRTY_BLOCK FirstDBInMainQ = FirstChangeNodeInMainQ->DirtyBlock;
        ASSERT(FirstDBInMainQ != NULL);

        if (FirstDBInMainQ->eWOState == ecWriteOrderStateData &&
                FirstDBInMainQ->ulDataSource == INVOLFLT_DATA_SOURCE_DATA) {
                    // There are DBs in Data WOS in Main Q ahead of this DB. We must allow all Data WOS
                    // DBs to drain and only after that drain DBs from list DataCaptureModeHead.
                    // Lets add ChangeNode back to list DataCaptureModeHead and drain it later.
                    ASSERT(DiffCount > 0);
                    InsertHeadList(&VolumeContext->ChangeList.DataCaptureModeHead, &ChangeNode->DataCaptureModeListEntry);
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
        ASSERT(IsListEmpty(&VolumeContext->ChangeList.DataCaptureModeHead));
        // Lets add it back to list DataCaptureModeHead.
        InsertTailList(&VolumeContext->ChangeList.DataCaptureModeHead, &ChangeNode->DataCaptureModeListEntry);
        ChangeNode = NULL;
        return ChangeNode;
    }

    if (ChangeNode == VolumeContext->ChangeList.CurrentNode) {
        ASSERT(!IsListEmpty(&VolumeContext->ChangeList.Head));
    }

    if (!IsListEmpty(&VolumeContext->ChangeList.Head)) {
        if (ChangeNode == VolumeContext->ChangeList.CurrentNode) {
            if (ChangeNode->ListEntry.Blink != &VolumeContext->ChangeList.Head) {
                ASSERT(ChangeNode->ListEntry.Flink == &VolumeContext->ChangeList.Head);
                NewCurrentNode = (PCHANGE_NODE) ChangeNode->ListEntry.Blink;
                ASSERT(NewCurrentNode->DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601 != 0);
            }
        }
        RemoveEntryList(&ChangeNode->ListEntry);
        if (ChangeNode == VolumeContext->ChangeList.CurrentNode) {
            ASSERT(IsListEmpty(&VolumeContext->ChangeList.Head) || NewCurrentNode != NULL);
        }
        if (IsListEmpty(&VolumeContext->ChangeList.Head)) {
            ASSERT(NewCurrentNode == NULL);
            ASSERT(ChangeNode == VolumeContext->ChangeList.CurrentNode);
            VolumeContext->ChangeList.CurrentNode = NULL;
            ASSERT(IsListEmpty(&VolumeContext->ChangeList.DataCaptureModeHead));
        }
        if (NewCurrentNode != NULL) {
            ASSERT(!IsListEmpty(&VolumeContext->ChangeList.Head));
            ASSERT(ChangeNode == VolumeContext->ChangeList.CurrentNode);
            VolumeContext->ChangeList.CurrentNode = NewCurrentNode;
        }
    }

    if (IsListEmpty(&VolumeContext->ChangeList.Head)) {
        ASSERT(VolumeContext->ChangeList.CurrentNode == NULL);
        ASSERT(IsListEmpty(&VolumeContext->ChangeList.DataCaptureModeHead));
    }

    if (VolumeContext->ChangeList.CurrentNode == NULL) {
        ASSERT(IsListEmpty(&VolumeContext->ChangeList.Head));
        ASSERT(IsListEmpty(&VolumeContext->ChangeList.DataCaptureModeHead));
    }
    
    VolumeContext->ChangeList.lDirtyBlocksInQueue--;
    if (VolumeContext->ChangeList.lDirtyBlocksInQueue == 0) {
        ASSERT(IsListEmpty(&VolumeContext->ChangeList.Head));
        ASSERT(IsListEmpty(&VolumeContext->ChangeList.DataCaptureModeHead));
        ASSERT(VolumeContext->ChangeList.CurrentNode == NULL);
    }
    if (ecChangeNodeDirtyBlock == ChangeNode->eChangeNode) {
        ASSERT(NULL != DirtyBlock);
        ASSERT((NULL != VolumeContext->ChangeList.CurrentNode) || (NULL != VolumeContext->pDBPendingTransactionConfirm) ||
               (DirtyBlock->cChanges == VolumeContext->ChangeList.ulTotalChangesPending));
        ASSERT((NULL != VolumeContext->ChangeList.CurrentNode) || (NULL != VolumeContext->pDBPendingTransactionConfirm) ||
               (DirtyBlock->ulicbChanges.QuadPart == VolumeContext->ChangeList.ullcbTotalChangesPending));
    }

    ChangeNode->DataCaptureModeListEntry.Flink = NULL;
    ChangeNode->DataCaptureModeListEntry.Blink = NULL;    

    return ChangeNode;
}

VOID
GenerateNewSequenceNumberAndTimeStamp(PVOLUME_CONTEXT VolumeContext, PCHANGE_NODE ChangeNode, bool bFromHead)
{
    PKDIRTY_BLOCK DirtyBlock = NULL;
    PLIST_ENTRY ListEntry = NULL;
    PCHANGE_NODE TmpChangeNode = NULL;
    PKDIRTY_BLOCK TmpDirtyBlock = NULL;
    ULONGLONG TimeStamp = 0;
    ULONGLONG ullSequenceNum = 0;
    ULONG ulSequenceNum = 0;
    ULONGLONG TmpTimeStamp = 0;
    ULONGLONG ullTmpSequenceNum = 0;
    ULONG ulTmpSequenceNum = 0;
    KIRQL OldIrql;
    
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
    if (DirtyBlock->ulDataSource == INVOLFLT_DATA_SOURCE_DATA) {
        return;
    }

    ASSERT(DirtyBlock->eWOState != ecWriteOrderStateData);

    // No pending DW's at this point as all before this would have been drained and
    // there wont be any after this in the list as only when all MDs are drained we could
    // track data using DW. However, DMs could be there. Since in new logic we drain
    // DMs ahead of MDs, there could be just one pending DM and that too in open
    // state (being filled) when this MD is being drained.

    // Validate that there are no pending DWs.
    ASSERT(VolumeContext->NumChangeNodeInDataWOSDrained == VolumeContext->NumChangeNodeInDataWOSCreated);

    // DataCaptureModeHead List must either be empty or contain
    // just one DM type of DB which is still open (not yet closed).
    // Validate this.
    if (!IsListEmpty(&VolumeContext->ChangeList.DataCaptureModeHead)) {
        ASSERT(!IsListEmpty(&VolumeContext->ChangeList.Head));
        ListEntry = (PLIST_ENTRY) RemoveHeadList(&VolumeContext->ChangeList.DataCaptureModeHead);
        TmpChangeNode = (PCHANGE_NODE) CONTAINING_RECORD((PCHAR) ListEntry, CHANGE_NODE, DataCaptureModeListEntry);
        ASSERT(TmpChangeNode != NULL);
        TmpDirtyBlock = TmpChangeNode->DirtyBlock;
        ASSERT(TmpDirtyBlock != NULL);

        ASSERT(TmpDirtyBlock->eWOState != ecWriteOrderStateData && TmpDirtyBlock->ulDataSource == INVOLFLT_DATA_SOURCE_DATA);
        ASSERT(0 == TmpDirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601);
        ASSERT(ecChangeNodeDirtyBlock == TmpChangeNode->eChangeNode);
        ASSERT(IsListEmpty(&VolumeContext->ChangeList.DataCaptureModeHead));
        InsertTailList(&VolumeContext->ChangeList.DataCaptureModeHead, &TmpChangeNode->DataCaptureModeListEntry);
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
GetChangeNodeFromChangeList(PVOLUME_CONTEXT VolumeContext, bool bFromHead)
{
    PCHANGE_NODE    ChangeNode = NULL;

    ChangeNode = GetDataModeChangeNode(VolumeContext, bFromHead);
    if (ChangeNode != NULL) {
        return ChangeNode;
    }

    if (!IsListEmpty(&VolumeContext->ChangeList.Head)) {
        if (bFromHead) {
            ChangeNode = (PCHANGE_NODE)RemoveHeadList(&VolumeContext->ChangeList.Head);
            if (ChangeNode == VolumeContext->ChangeList.CurrentNode) {
                ASSERT(IsListEmpty(&VolumeContext->ChangeList.Head));

                VolumeContext->ChangeList.CurrentNode = NULL;
            }
        } else {
            PCHANGE_NODE    CurrentNode = VolumeContext->ChangeList.CurrentNode;
            if (CurrentNode->ListEntry.Blink != &VolumeContext->ChangeList.Head) {
                ChangeNode = (PCHANGE_NODE)CurrentNode->ListEntry.Blink;
                RemoveEntryList(&ChangeNode->ListEntry);
            } else {
                ChangeNode = (PCHANGE_NODE)RemoveTailList(&VolumeContext->ChangeList.Head);
                ASSERT(IsListEmpty(&VolumeContext->ChangeList.Head));

                VolumeContext->ChangeList.CurrentNode = NULL;
            }
        }
        VolumeContext->ChangeList.lDirtyBlocksInQueue--;
        if (ChangeNode != NULL && ChangeNode->DirtyBlock->eWOState == ecWriteOrderStateData &&
                ChangeNode->DirtyBlock->ulDataSource == INVOLFLT_DATA_SOURCE_DATA) {
                    // ChangeNode will be either drained or written to bitmap.
                    VolumeContext->NumChangeNodeInDataWOSDrained++;
        }        
        if (ecChangeNodeDirtyBlock == ChangeNode->eChangeNode) {
            PKDIRTY_BLOCK   DirtyBlock = ChangeNode->DirtyBlock;

            ASSERT(NULL != DirtyBlock);
            ASSERT((NULL != VolumeContext->ChangeList.CurrentNode) || (NULL != VolumeContext->pDBPendingTransactionConfirm) ||
                   (DirtyBlock->cChanges == VolumeContext->ChangeList.ulTotalChangesPending));
            ASSERT((NULL != VolumeContext->ChangeList.CurrentNode) || (NULL != VolumeContext->pDBPendingTransactionConfirm) ||
                   (DirtyBlock->ulicbChanges.QuadPart == VolumeContext->ChangeList.ullcbTotalChangesPending));

            if (0 == DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601) {
                if (DirtyBlock->eWOState != ecWriteOrderStateData &&
                                DirtyBlock->ulDataSource == INVOLFLT_DATA_SOURCE_DATA) {
                    ASSERT(ChangeNode->DataCaptureModeListEntry.Flink != NULL &&
                            ChangeNode->DataCaptureModeListEntry.Blink != NULL);
                    GenerateTimeStampTag(&DirtyBlock->TagTimeStampOfFirstChange);                                
                }
                
                GenerateLastTimeStampTag(DirtyBlock);
                DirtyBlock->liTickCountAtLastIO = KeQueryPerformanceCounter(NULL);

                if (DirtyBlock->eWOState != ecWriteOrderStateData &&
                                DirtyBlock->ulDataSource == INVOLFLT_DATA_SOURCE_DATA) {
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
                PKDIRTY_BLOCK   DirtyBlock = ChangeNode->DirtyBlock;
                ASSERT(DirtyBlock->eWOState != ecWriteOrderStateData && DirtyBlock->ulDataSource == INVOLFLT_DATA_SOURCE_DATA);
                ASSERT(!IsListEmpty(&VolumeContext->ChangeList.DataCaptureModeHead));
                RemoveEntryList(&ChangeNode->DataCaptureModeListEntry);
                ChangeNode->DataCaptureModeListEntry.Flink = NULL;
                ChangeNode->DataCaptureModeListEntry.Blink = NULL;
                if (IsListEmpty(&VolumeContext->ChangeList.Head)) {
                    ASSERT(IsListEmpty(&VolumeContext->ChangeList.DataCaptureModeHead));
                    ASSERT(VolumeContext->ChangeList.CurrentNode == NULL);
                }
        }
        if (VolumeContext->ChangeList.lDirtyBlocksInQueue == 0) {
            ASSERT(IsListEmpty(&VolumeContext->ChangeList.Head));
            ASSERT(IsListEmpty(&VolumeContext->ChangeList.DataCaptureModeHead));
            ASSERT(VolumeContext->ChangeList.CurrentNode == NULL);
        }        
    } else {
        ASSERT(VolumeContext->ChangeList.CurrentNode == NULL);
        ASSERT(IsListEmpty(&VolumeContext->ChangeList.DataCaptureModeHead));
    }

    GenerateNewSequenceNumberAndTimeStamp(VolumeContext, ChangeNode, bFromHead);

    // Do not dereference change node here. Caller would dereference this
    // Count incremented at the time of adding to list is compensated by
    // callers dereference.
    return ChangeNode;
}

PKDIRTY_BLOCK
GetCurrentDirtyBlockFromChangeList(PVOLUME_CONTEXT VolumeContext)
{
    PCHANGE_NODE    ChangeNode = VolumeContext->ChangeList.CurrentNode;
    PKDIRTY_BLOCK   DirtyBlock = NULL;
    if ((NULL != ChangeNode) && (ecChangeNodeDirtyBlock == ChangeNode->eChangeNode)) {
        DirtyBlock = ChangeNode->DirtyBlock;
    }

    return DirtyBlock;
}

PCHANGE_NODE
GetChangeNodeToSaveAsFile(PVOLUME_CONTEXT VolumeContext)
{
    PCHANGE_NODE    ChangeNode = (PCHANGE_NODE)VolumeContext->ChangeList.Head.Blink;

    ASSERT((NULL != ChangeNode) && ((PLIST_ENTRY)ChangeNode != &VolumeContext->ChangeList.Head));
    // Never save the last change node.
    ChangeNode = (PCHANGE_NODE)ChangeNode->ListEntry.Blink;
    while ((PLIST_ENTRY)ChangeNode != &VolumeContext->ChangeList.Head) {
        if ((ecChangeNodeDirtyBlock == ChangeNode->eChangeNode) && 
            (INVOLFLT_DATA_SOURCE_DATA == ChangeNode->DirtyBlock->ulDataSource) &&
            (0 == (ChangeNode->ulFlags & (CHANGE_NODE_FLAGS_QUEUED_FOR_DATA_WRITE | CHANGE_NODE_FLAGS_ERROR_IN_DATA_WRITE))))
        {
            ChangeNode->ulFlags |= CHANGE_NODE_FLAGS_QUEUED_FOR_DATA_WRITE;
            VolumeContext->lDataBlocksQueuedToFileWrite += ChangeNode->DirtyBlock->ulDataBlocksAllocated;
            ASSERT(VolumeContext->lDataBlocksQueuedToFileWrite <= VolumeContext->lDataBlocksInUse);
            return ReferenceChangeNode(ChangeNode);
        }

        ChangeNode = (PCHANGE_NODE)ChangeNode->ListEntry.Blink;
    }

    InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, ("%s: Failed to find change node to write to file\n", __FUNCTION__));
    return NULL;
}


VOID
QueueChangeNodesToSaveAsFile(PVOLUME_CONTEXT VolumeContext, bool bLockAcquired)
{
    PCHANGE_NODE    ChangeNode = NULL;
    NTSTATUS        Status = STATUS_SUCCESS;
    KIRQL           OldIrql;

    if (!bLockAcquired)
        KeAcquireSpinLock(&VolumeContext->Lock, &OldIrql);

    ChangeNode = (PCHANGE_NODE)VolumeContext->ChangeList.Head.Blink;;
    ASSERT((NULL != ChangeNode) && ((PLIST_ENTRY)ChangeNode != &VolumeContext->ChangeList.Head));
    // Never save the last change node.
    ChangeNode = (PCHANGE_NODE)ChangeNode->ListEntry.Blink;
    while ((PLIST_ENTRY)ChangeNode != &VolumeContext->ChangeList.Head) {
        if ((ecChangeNodeDirtyBlock == ChangeNode->eChangeNode) && 
            (INVOLFLT_DATA_SOURCE_DATA == ChangeNode->DirtyBlock->ulDataSource) &&
            (0 == (ChangeNode->ulFlags & (CHANGE_NODE_FLAGS_QUEUED_FOR_DATA_WRITE | CHANGE_NODE_FLAGS_ERROR_IN_DATA_WRITE))))
        {
            ChangeNode->ulFlags |= CHANGE_NODE_FLAGS_QUEUED_FOR_DATA_WRITE;
            VolumeContext->lDataBlocksQueuedToFileWrite += ChangeNode->DirtyBlock->ulDataBlocksAllocated;
            ASSERT(VolumeContext->lDataBlocksQueuedToFileWrite <= VolumeContext->lDataBlocksInUse);
            ReferenceChangeNode(ChangeNode);
            Status = DriverContext.DataFileWriterManager->AddWorkItem(VolumeContext->VolumeFileWriter, ChangeNode, VolumeContext);
            ASSERT(STATUS_SUCCESS == Status);
            DereferenceChangeNode(ChangeNode);

        }

        ChangeNode = (PCHANGE_NODE)ChangeNode->ListEntry.Blink;
    }

    if (!bLockAcquired)
        KeReleaseSpinLock(&VolumeContext->Lock, OldIrql);

    InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, ("%s: Failed to find change node to write to file\n", __FUNCTION__));
   
}
ULONG
CurrentDataSource(PVOLUME_CONTEXT VolumeContext)
{
    switch(VolumeContext->eCaptureMode) {
    case ecCaptureModeData:
        return INVOLFLT_DATA_SOURCE_DATA;
    case ecCaptureModeMetaData:
    default:
        return INVOLFLT_DATA_SOURCE_META_DATA;
    }

    return INVOLFLT_DATA_SOURCE_META_DATA;
}

NTSTATUS
GetNextVolumeGUID(PVOLUME_CONTEXT VolumeContext, PWCHAR VolumeGUID)
{
    NTSTATUS    Status = STATUS_SUCCESS;
    bool        bFound = false;
    KIRQL       OldIrql;
    ASSERT(NULL != VolumeContext);

    KeAcquireSpinLock(&VolumeContext->Lock, &OldIrql);
    PVOLUME_GUID    GUIDList = VolumeContext->GUIDList;
    // Lets first check if VolumeGUID matches primary GUID
    if (GUID_SIZE_IN_BYTES == RtlCompareMemory(VolumeGUID, VolumeContext->wGUID, GUID_SIZE_IN_BYTES)) {
        // Matched primary GUID
        bFound = true;
        if (NULL != GUIDList) {
            RtlCopyMemory(VolumeGUID, GUIDList->GUID, GUID_SIZE_IN_BYTES);
        } else {
            Status = STATUS_NOT_FOUND;
        }
    } else {
        while (NULL != GUIDList) {
            if (GUID_SIZE_IN_BYTES == RtlCompareMemory(VolumeGUID, GUIDList->GUID, GUID_SIZE_IN_BYTES)) {
                // Found GUID
                bFound = true;
                PVOLUME_GUID   NextGUID = GUIDList->NextGUID;
                if (NULL != NextGUID) {
                    RtlCopyMemory(VolumeGUID, NextGUID->GUID, GUID_SIZE_IN_BYTES);
                } else {
                    Status = STATUS_NOT_FOUND;
                }
                break;
            }
            GUIDList = GUIDList->NextGUID;
        }
    }

    if (!bFound) {
        // GUID may not have been in the list, or GUID may have been deleted in between
        RtlCopyMemory(VolumeGUID, VolumeContext->wGUID, GUID_SIZE_IN_BYTES);
    }

    KeReleaseSpinLock(&VolumeContext->Lock, OldIrql);

    return Status;
}

//This function is called with volume context lock held
void
MoveUnupdatedDirtyBlocks(PVOLUME_CONTEXT pVolumeContext) {
    LIST_ENTRY   *pDestinationList = &pVolumeContext->ChangeList.Head;
    LIST_ENTRY  *pSourceList = &pVolumeContext->ChangeList.TempQueueHead;
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

    pVolumeContext->bQueueChangesToTempQueue = false;
    InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("%s: Added the Dirty blocks from temp queue to main queue\n", __FUNCTION__));
}

void InitializeClusterDiskAcquireParams(PVOLUME_CONTEXT pVolumeContext)
{
    PVOLUME_BITMAP  pVolumeBitmap;

    pVolumeBitmap = pVolumeContext->pVolumeBitmap;
    pVolumeContext->pVolumeBitmap = NULL;

    if (pVolumeBitmap) {
        // This is the bitmap file in Raw IO mode from previous acquire.
        // Delete this bitmap file and create a new one.
        if (pVolumeBitmap->pVolumeContext) {
            DereferenceVolumeContext(pVolumeBitmap->pVolumeContext);
            pVolumeBitmap->pVolumeContext = NULL;
        }
        DereferenceVolumeBitmap(pVolumeBitmap);
    }
    StartFilteringDevice(pVolumeContext, true);
    pVolumeContext->ulFlags |= VCF_CLUSTER_VOLUME_ONLINE;
    pVolumeContext->ulFlags &= ~VCF_CV_FS_UNMOUTNED;
    pVolumeContext->ulFlags &= ~VCF_CLUSDSK_RETURNED_OFFLINE;
    pVolumeContext->liDisMountFailNotifyTimeStamp.QuadPart = 0;
    pVolumeContext->liDisMountNotifyTimeStamp.QuadPart = 0;
    pVolumeContext->liMountNotifyTimeStamp.QuadPart = 0;
    pVolumeContext->liFirstBitmapOpenErrorAtTickCount.QuadPart = 0;
    pVolumeContext->liLastBitmapOpenErrorAtTickCount.QuadPart = 0;
    pVolumeContext->lNumBitmapOpenErrors=0;
    pVolumeContext->lNumBitMapClearErrors=0;
    pVolumeContext->lNumBitMapReadErrors=0;
    pVolumeContext->lNumBitMapWriteErrors=0;
    //Making this flag as true so that it will update the globals again.
    pVolumeContext->bQueueChangesToTempQueue = true;
}

NTSTATUS
GetVContextFieldsFromRegistry(PVOLUME_CONTEXT pVolumeContext)
{
    NTSTATUS        Status = STATUS_SUCCESS;
    ULONG           ulcbRead = 0;
    CHAR            ShutdownMarker = uninitialized;
    Registry        *pVolumeParam = NULL;

    pVolumeParam = new Registry();
    if (NULL == pVolumeParam)
        return STATUS_INSUFFICIENT_RESOURCES;

    Status = pVolumeParam->OpenKeyReadWrite(&pVolumeContext->VolumeParameters, TRUE);
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
        Status = pVolumeParam->ReadBinary(VC_LAST_PERSISTENT_VALUES, 
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
                    ("%s: Created %S with Default PrevEndTimeStamp %#I64x, ",__FUNCTION__,VC_LAST_PERSISTENT_VALUES, 
                     pDefaultInfo->PersistentValues[0].Data));
                InVolDbgPrint(DL_INFO, DM_REGISTRY, 
                    ("PrevEndSequenceNumber %#I64x\n, PrevSeqID %#x for Volume %S\n",pDefaultInfo->PersistentValues[1].Data,
                     pDefaultInfo->PersistentValues[2].Data,
                     pVolumeContext->wGUID));
            } else {//Registry found. Values has been read.
                ShutdownMarker = pPersistentInfo->ShutdownMarker;
                InVolDbgPrint(DL_INFO, DM_REGISTRY, 
                    ("%s: Read Registry %S Shutdown Marker  = %d\n",__FUNCTION__,VC_LAST_PERSISTENT_VALUES, pPersistentInfo->ShutdownMarker));

                for (ULONG i = 0; i < pPersistentInfo->NumberOfInfo; i++) {
                    switch (pPersistentInfo->PersistentValues[i].PersistentValueType) {
                        case PREV_END_TIMESTAMP:
                            if (ShutdownMarker == DirtyShutdown)//means that the last shutdown was dirty.
                                pVolumeContext->LastCommittedTimeStamp = MAX_PREV_TIMESTAMP;
                            else//Means that the last shutdown was clean.
                                pVolumeContext->LastCommittedTimeStamp = pPersistentInfo->PersistentValues[i].Data;
                            InVolDbgPrint(DL_INFO, DM_REGISTRY, 
                                ("%s: Read Registry %S PrevEndTimeStamp = %#I64x for Volume %S\n",__FUNCTION__,
                                       VC_LAST_PERSISTENT_VALUES, pVolumeContext->LastCommittedTimeStamp, 
                                       pVolumeContext->wGUID));
                            pVolumeContext->LastCommittedTimeStampReadFromStore = pVolumeContext->LastCommittedTimeStamp;
                            break;
                        case PREV_END_SEQUENCENUMBER:
                            if (ShutdownMarker == DirtyShutdown)//Means that the last shutdown was dirty.
                                pVolumeContext->LastCommittedSequenceNumber = MAX_PREV_SEQNUMBER;
                            else//Means the last shutdown was clean.
                                pVolumeContext->LastCommittedSequenceNumber = pPersistentInfo->PersistentValues[i].Data;

                            pVolumeContext->LastCommittedSequenceNumberReadFromStore = (ULONGLONG)pVolumeContext->LastCommittedSequenceNumber;
                            InVolDbgPrint(DL_INFO, DM_REGISTRY, 
                                ("%s: Read Registry %S PrevEndSequenceNumber = %#I64x for Volume %S\n",__FUNCTION__,
                                       VC_LAST_PERSISTENT_VALUES, pVolumeContext->LastCommittedSequenceNumber, 
                                       pVolumeContext->wGUID));
                            break;
                        case  PREV_SEQUENCEID:
                            if (ShutdownMarker == DirtyShutdown)//means that the last shutdown was dirty.
                                pVolumeContext->LastCommittedSplitIoSeqId = MAX_SEQ_ID;
                            else//Means that the last shutdown was clean.
                                pVolumeContext->LastCommittedSplitIoSeqId = (ULONG)pPersistentInfo->PersistentValues[i].Data;
                            InVolDbgPrint(DL_INFO, DM_REGISTRY, 
                                ("%s: Read Registry %S PrevSeqId = %#I64x for Volume %S\n",__FUNCTION__,
                                       VC_LAST_PERSISTENT_VALUES, pVolumeContext->LastCommittedSplitIoSeqId, 
                                       pVolumeContext->wGUID));
                            pVolumeContext->LastCommittedSplitIoSeqIdReadFromStore = pVolumeContext->LastCommittedSplitIoSeqId;
                            break;
                        case  SOURCE_TARGET_LAG:
                            if (ShutdownMarker == DirtyShutdown)//means that the last shutdown was dirty.
                                pVolumeContext->EndTimeStampofDB = 0;
                            else//Means that the last shutdown was clean.
                                pVolumeContext->EndTimeStampofDB = (ULONGLONG)pPersistentInfo->PersistentValues[i].Data;                                
                            InVolDbgPrint(DL_INFO, DM_REGISTRY, 
                                ("%s: Read Registry %S EndTimeStampofDB = %#I64x for Volume %S\n",__FUNCTION__,
                                       VC_LAST_PERSISTENT_VALUES, pVolumeContext->EndTimeStampofDB, 
                                       pVolumeContext->wGUID));
                            break;

                        default :
                            InVolDbgPrint(DL_INFO, DM_REGISTRY, 
                                ("%s: Read Registry for %S is corrupted.\n", __FUNCTION__, VC_LAST_PERSISTENT_VALUES));
                            break;
                    }
                }
                // Mark DirtyShutdown to differentiate whether next shutdown is Clean or Dirty
                pPersistentInfo->ShutdownMarker = DirtyShutdown;
                Status = pVolumeParam->WriteBinary(VC_LAST_PERSISTENT_VALUES, pPersistentInfo, SizeOfBuffer);
                if (STATUS_SUCCESS != Status) {
                    InVolDbgPrint(DL_ERROR, DM_REGISTRY,
                        ("GetVContextFieldsFromRegistry: Failed to Set DirtyShutdown after Persistent Values read for Volume GUID(%S)\n",
                         pVolumeContext->wGUID));
                } else {
                    Status = pVolumeParam->FlushKey();
                    if (STATUS_SUCCESS != Status) {
                        InVolDbgPrint(DL_ERROR, DM_REGISTRY, 
                            ("%s: Flushing the Registry Info to Disk Failed for Volume : %S\n", __FUNCTION__,
                             pVolumeContext->wDriveLetter));
                    } else {
                        InVolDbgPrint(DL_ERROR, DM_REGISTRY,
                            ("GetVContextFieldsFromRegistry: Flushing the VolumeContext Registry Info to Disk Succeeded for Volume GUID(%S)\n",
                             pVolumeContext->wGUID));
                    }
                }
            }
        } else {
            InVolDbgPrint(DL_INFO, DM_REGISTRY, 
                ("%s: ReadBinary for %S has failed for Volume %S\n", __FUNCTION__, VC_LAST_PERSISTENT_VALUES, pVolumeContext->wGUID));
        }
    }
    return Status;
}

NTSTATUS PersistVContextFields(PVOLUME_CONTEXT pVolumeContext)
{
    NTSTATUS     Status;
    KIRQL        OldIrql;
    Registry    *pVolumeParam = NULL;

    pVolumeParam = new Registry();
    if (NULL == pVolumeParam)
        return STATUS_INSUFFICIENT_RESOURCES;

    Status = pVolumeParam->OpenKeyReadWrite(&pVolumeContext->VolumeParameters, TRUE);
    if (NT_SUCCESS(Status)) {
        PPERSISTENT_INFO pPersistentInfo;
        ULONGLONG PrevEndTimeStamp = 0;
        ULONGLONG PrevEndSeqNumber = 0;
        ULONGLONG EndTimeStampofDB = 0;
        ULONG PrevSequenceId = 0;

        ULONG NumberOfInfo = 4;
        const unsigned long SizeOfBuffer = sizeof(PERSISTENT_INFO) + (3 * sizeof(PERSISTENT_VALUE));
        CHAR Buffer[SizeOfBuffer];

        KeWaitForSingleObject(&pVolumeContext->Mutex, Executive, KernelMode, FALSE, NULL);
        KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);

        pVolumeContext->ulFlags |= VCF_VCONTEXT_FIELDS_PERSISTED;

        PrevEndTimeStamp = pVolumeContext->LastCommittedTimeStamp;
        PrevEndSeqNumber = pVolumeContext->LastCommittedSequenceNumber;
        PrevSequenceId = pVolumeContext->LastCommittedSplitIoSeqId;
        EndTimeStampofDB = pVolumeContext->EndTimeStampofDB;
        KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);
        
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

        Status = pVolumeParam->WriteBinary(VC_LAST_PERSISTENT_VALUES, pPersistentInfo, SizeOfBuffer);
        if (STATUS_SUCCESS != Status) {
            InVolDbgPrint(DL_ERROR, DM_REGISTRY,
                ("%s: Failed to Write Persistent Values for Volume (%S)\n", __FUNCTION__,
                 pVolumeContext->wDriveLetter));
        } else {
            Status = pVolumeParam->FlushKey();
            if (STATUS_SUCCESS != Status) {
                InVolDbgPrint(DL_ERROR, DM_REGISTRY, 
                    ("%s: Flushing the Registry Info to Disk Failed for Volume : %S\n", __FUNCTION__,
                     pVolumeContext->wDriveLetter));
            } else {
                InVolDbgPrint(DL_INFO, DM_REGISTRY, 
                   ("Added PrevEndTimeStamp : %#I64x, PrevEndSequenceNumber : %#I64x\n", PrevEndTimeStamp, PrevEndSeqNumber ));
                InVolDbgPrint(DL_INFO, DM_REGISTRY, 
                   ("PrevSequenceId : %#x to Registry for Volume %S\n", PrevSequenceId, pVolumeContext->wDriveLetter));
            }
        }
        KeReleaseMutex(&pVolumeContext->Mutex, false);
    } 
    return Status;
}

void UpdateVolumeContextWithValuesReadFromBitmap(BOOLEAN bVolumeInSync, BitmapPersistentValues BitmapPersistentValue)
{
    PVOLUME_CONTEXT pVolumeContext = BitmapPersistentValue.m_pVolumeContext;

    if (IS_CLUSTER_VOLUME(pVolumeContext) && pVolumeContext->bQueueChangesToTempQueue)  {
        if (!bVolumeInSync) {
            pVolumeContext->LastCommittedTimeStamp = MAX_PREV_TIMESTAMP;
            pVolumeContext->LastCommittedSequenceNumber = MAX_PREV_SEQNUMBER;
            pVolumeContext->LastCommittedSplitIoSeqId = MAX_SEQ_ID;
        } else {
            pVolumeContext->LastCommittedTimeStamp = BitmapPersistentValue.GetPrevEndTimeStampFromBitmap();
            pVolumeContext->LastCommittedSequenceNumber = BitmapPersistentValue.GetPrevEndSequenceNumberFromBitmap();
            pVolumeContext->LastCommittedSplitIoSeqId = BitmapPersistentValue.GetPrevSequenceIdFromBitmap();
        }
    }
    return;
}
/////////////////////////////////////////////////////////////////
//Check if this is a W2k system. if Yes let's not proceed with, since most of
//the MS FS utility calls are not supported in this OS version.- Kartha.
#if (NTDDI_VERSION >= NTDDI_WS03)
////////////////////////////////////////////////////////////////
/*
 Fix For Bug 27337
 This procedure takes a single input which is the unique volume object link
 and from there tries to get the start sector of first LCN known to FS.
 From which we also get the Max Byte Offset known to FS which is again
 one of the output parameters.
 The procedure is intended to be used only on OS >= Windows 2K3
 Only NTFS, REFS and FAT32 filesystems are supported.

 Notes:
 * Any error condition will take the highest precedence i.e. either a fool-proof
    solution or Resync.
 * We will not support windows 2000 OS since none of the major API's (must have)
    are not supported here.

 Arguments:

  Input:

  pUniqueVolumeName.
  >>should be in format L"\\??\\Volume{aaf1f466-194b-11e2-88cf-806e6f6e6963}"

  Output:

  pFsInfo.
  >>This structure is allocated and input by the caller. If Success, this structure shall
      be populated with relevant values.
      
 */

NTSTATUS 
InMageFltFetchFsEndClusterOffset(
 __in PUNICODE_STRING pUniqueVolumeName,
 __inout PFSINFORMATION pFsInfo
 )

{
         
  NTSTATUS Status = STATUS_UNSUCCESSFUL;
  OBJECT_ATTRIBUTES objectAttributes;
  FILE_FS_SIZE_INFORMATION FsSizeInformation;
  IO_STATUS_BLOCK ioStatusBlock;
  HANDLE volumeHandle = NULL;
  LONGLONG Seconds = (-10 * 1000 * 1000) * 5;
  LARGE_INTEGER ByteOffset;
  LARGE_INTEGER NumberSectors;
  LARGE_INTEGER TotalClusters;
  LARGE_INTEGER SectorOfFirstLCN;
  ULONG SectorsPerCluster;
  ULONG BytesPerSector;
  ULONG BytesPerCluster;
  FileSystemTypeList FsType;
  RTL_OSVERSIONINFOW osVersion;
  ULONG Fat32RootSectors, Fat32FatSectors, Fat32DataSectors;
  ULONG Fat32RootDirectoryFirstSector, Fat32FirstDataSectorOfFirstLCN;
  FAT32_BPB         fat32Bpb; 
  FAT32_BOOT_SECTOR bootSector;
  NTFS_VOLUME_DATA_BUFFER  NtfsData;
  REFS_VOLUME_DATA_BUFFER  RefsData;
  FILE_FS_ATTRIBUTE_INFORMATION *pfai;
  LARGE_INTEGER faibuf[CONSTRUCT(1, FILE_FS_ATTRIBUTE_INFORMATION, 32)];


  ASSERT(NULL != pFsInfo);
  ASSERT(NULL != pUniqueVolumeName->Buffer);
    
  if( (NULL == pUniqueVolumeName->Buffer) || (NULL == pFsInfo)){
  	
	Status = STATUS_INVALID_PARAMETER;
	goto ClusterOffsetEpilogue;
  }
  
  pfai = (FILE_FS_ATTRIBUTE_INFORMATION *) faibuf;
  
  RtlZeroMemory(pFsInfo, sizeof(FSINFORMATION));
  RtlZeroMemory(&osVersion, sizeof(RTL_OSVERSIONINFOW));

  osVersion.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOW);
  RtlGetVersion(&osVersion);

  //win2k. 5.0
  if(osVersion.dwMajorVersion <= 5 && osVersion.dwMinorVersion == 0){

	 Status = STATUS_NOT_IMPLEMENTED;
	 goto ClusterOffsetEpilogue;
  	}

  InitializeObjectAttributes( &objectAttributes, 
                                pUniqueVolumeName, 
                                OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, 
                                NULL, 
                                NULL );

  Status = ZwOpenFile( &volumeHandle,
                             FILE_READ_DATA | SYNCHRONIZE,
                             &objectAttributes,
                             &ioStatusBlock,
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             FILE_SYNCHRONOUS_IO_NONALERT );

  if (!NT_SUCCESS(Status)){
  	
		InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT,\
			("InMageFltFetchFsEndClusterOffset::ZwOpenFile() failed (Status %lx)\n",Status));
        goto ClusterOffsetEpilogue;
    }
	
  //Lets try to get the FileFsSizeInformation on the volume in question. - Kartha
  
  Status = ZwQueryVolumeInformationFile(volumeHandle,
                                             &ioStatusBlock,
                                             &FsSizeInformation,
                                             sizeof(FsSizeInformation),
                                             FileFsSizeInformation);
											 
  if (!NT_SUCCESS(Status)){
  	
        InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT,\
			("ZwQueryVolumeInformationFile() failed for FileFsSizeInformation (Status %lx)\n",Status));
		goto ClusterOffsetEpilogue;
    }

  //Now we have : FsSizeInformation.SectorsPerAllocationUnit (This gives number of sectors per cluster i.e. FS BLOCK)
  //FsSizeInformation.BytesPerSector
	
  /*Let's use ZwQueryVolumeInformationFile-FileFsAttributeInformation combination to get our hands on the
    File System type we have. First call to ZwQueryVolumeInformationFile is bound to fail with
    STATUS_BUFFER_OVERFLOW since we would have passed a smaller buffer as argument b'coz
    we didn't know the real file system name size. To avoid calling ZwQueryVolumeInformationFile
    twice we can cleverly implement it using a fixed size buffer on stack- Kartha*/


  RtlZeroMemory(faibuf, sizeof(faibuf));

  Status = ZwQueryVolumeInformationFile(volumeHandle,
                                        &ioStatusBlock,
                                        pfai,
                                        sizeof(faibuf),
                                        FileFsAttributeInformation);

  if (!NT_SUCCESS(Status)){
  	
	  InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT,\
			("ZwQueryVolumeInformationFile() failed for FileFsAttributeInformation (Status %lx)\n",Status));
	  goto ClusterOffsetEpilogue;
    }


  //We only support NTFS, REFS and FAT32 for this functionality.
  if( (sizeof(WCHAR) << 2) == RtlCompareMemory(pfai->FileSystemName, L"NTFS", sizeof(WCHAR) << 2)){

		FsType = FSTYPE_NTFS;
    }
  else if( (sizeof(WCHAR) << 2) == RtlCompareMemory(pfai->FileSystemName, L"ReFS", sizeof(WCHAR) << 2)){

	    FsType = FSTYPE_REFS;
  	}
  else if( (sizeof(WCHAR) * 5) == RtlCompareMemory(pfai->FileSystemName, L"FAT32", sizeof(WCHAR) * 5)){

	    FsType = FSTYPE_FAT;
  	}
  else{
  	 FsType = FSTYPE_UNKNOWN;
	 Status = STATUS_UNRECOGNIZED_MEDIA;
	 goto ClusterOffsetEpilogue;
	}

  /*Note: Unlike FATx variants NTFS doesnot have anything like sector offset to the first logical
      cluster number (LCN) of the file system relative to the start of the volume.
      i.e. In case of NTFS ::  first FS LCN offset == first Volume LCN offset == 0th Volume sector.
      While writing this code, it's nowhere elucidated by MS if this is true in case of REFS. Thus for
      REFS we shall use FSCTL_GET_RETRIEVAL_POINTER_BASE the first sector of FS LCN relative
      to volume start.- Kartha.*/
      
  switch(FsType)
  {
    case FSTYPE_NTFS:
		
		Status = ZwFsControlFile(
                   volumeHandle,
                   0, 
                   NULL,
                   NULL,
                   &ioStatusBlock,
                   FSCTL_GET_NTFS_VOLUME_DATA,
                   NULL,
                   0,
                   (PVOID)&NtfsData,
                   sizeof(NTFS_VOLUME_DATA_BUFFER)
                  );

		if (!NT_SUCCESS(Status)){

			InVolDbgPrint(DL_WARNING, DM_VOLUME_CONTEXT,\
			("ZwFsControlFile() failed for FSCTL_GET_NTFS_VOLUME_DATA on NTFS (Status %lx)\n",Status));
			goto ClusterOffsetEpilogue;
		}

		NumberSectors.QuadPart = NtfsData.NumberSectors.QuadPart;
		TotalClusters.QuadPart = NtfsData.TotalClusters.QuadPart;
		BytesPerSector = NtfsData.BytesPerSector;
		BytesPerCluster = NtfsData.BytesPerCluster;

		Status = ZwFsControlFile(
                   volumeHandle,
                   NULL, 
                   NULL,
                   NULL,
                   &ioStatusBlock,
                   FSCTL_GET_RETRIEVAL_POINTER_BASE,
                   NULL,
                   0,
                   &SectorOfFirstLCN,
                   8
                  );

		if (!NT_SUCCESS(Status)){

     	    InVolDbgPrint(DL_WARNING, DM_VOLUME_CONTEXT,\
			("ZwFsControlFile() failed for FSCTL_GET_RETRIEVAL_POINTER_BASE on NTFS (Status %lx)\n",Status));	
			//NTFS sector of first FS data LCN always starts at zero, so no issues.
			SectorOfFirstLCN.QuadPart = 0;
		}

		SectorsPerCluster = (ULONG)(NumberSectors.QuadPart / TotalClusters.QuadPart);

		pFsInfo->TotalClustersInFS.QuadPart = TotalClusters.QuadPart;
		pFsInfo->SizeOfFsBlockInBytes = BytesPerCluster;
		pFsInfo->SectorOffsetOfFirstDataLCN.QuadPart = SectorOfFirstLCN.QuadPart;
		pFsInfo->SectorsPerCluster = SectorsPerCluster;
		pFsInfo->BytesPerSector = BytesPerSector;

		pFsInfo->MaxVolumeByteOffsetForFs = (SectorOfFirstLCN.QuadPart * BytesPerSector) + \
			(TotalClusters.QuadPart * SectorsPerCluster * BytesPerSector);

		Status = STATUS_SUCCESS;
        goto ClusterOffsetEpilogue;
		
		break;
	case FSTYPE_REFS:

		Status = ZwFsControlFile(
                   volumeHandle,
                   0, 
                   NULL,
                   NULL,
                   &ioStatusBlock,
                   FSCTL_GET_REFS_VOLUME_DATA,
                   NULL,
                   0,
                   (PVOID)&RefsData,
                   sizeof(REFS_VOLUME_DATA_BUFFER)
                  );

		if (!NT_SUCCESS(Status)){

			InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT,\
			("ZwFsControlFile() failed for FSCTL_GET_REFS_VOLUME_DATA on ReFS (Status %lx)\n",Status));
			goto ClusterOffsetEpilogue;
		}

		NumberSectors.QuadPart = RefsData.NumberSectors.QuadPart;
		TotalClusters.QuadPart = RefsData.TotalClusters.QuadPart;
		BytesPerSector = RefsData.BytesPerSector;
		BytesPerCluster = RefsData.BytesPerCluster;

		Status = ZwFsControlFile(
                   volumeHandle,
                   NULL,
                   NULL,
                   NULL,
                   &ioStatusBlock,
                   FSCTL_GET_RETRIEVAL_POINTER_BASE,
                   NULL,
                   0,
                   &SectorOfFirstLCN,
                   8
                  );

		if (!NT_SUCCESS(Status)){

			InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT,\
			("ZwFsControlFile() failed for FSCTL_GET_RETRIEVAL_POINTER_BASE on ReFS (Status %lx)\n",Status));
			//NTFS sector of first FS data LCN always starts at zero, so no issues, but not sure
			//about REFS case. so rather than taking chance we set error.
			goto ClusterOffsetEpilogue;
		}

		SectorsPerCluster = (ULONG)(NumberSectors.QuadPart / TotalClusters.QuadPart);

		pFsInfo->TotalClustersInFS.QuadPart = TotalClusters.QuadPart;
		pFsInfo->SizeOfFsBlockInBytes = BytesPerCluster;
		pFsInfo->SectorOffsetOfFirstDataLCN.QuadPart = SectorOfFirstLCN.QuadPart;
		pFsInfo->SectorsPerCluster = SectorsPerCluster;
		pFsInfo->BytesPerSector = BytesPerSector;

		pFsInfo->MaxVolumeByteOffsetForFs = (SectorOfFirstLCN.QuadPart * BytesPerSector) + \
			(TotalClusters.QuadPart * SectorsPerCluster * BytesPerSector);

		Status = STATUS_SUCCESS;
        goto ClusterOffsetEpilogue;
		
	break;

	case FSTYPE_FAT:

        ByteOffset.QuadPart = 0;
		
        Status = ZwReadFile(
                   volumeHandle,
                   NULL,
                   NULL,
                   NULL,
                   &ioStatusBlock,
                   &bootSector,
                   sizeof(FAT32_BOOT_SECTOR),
                   &ByteOffset,
                   NULL
                  );

		if (!NT_SUCCESS(Status)){

			InVolDbgPrint(DL_ERROR, DM_VOLUME_CONTEXT,\
			("ZwReadFile() failed reading FAT32 BIOS PARAMETER BLOCK (Status %lx)\n",Status));
			goto ClusterOffsetEpilogue;
		}

		Fat32UnpackBios(&fat32Bpb, &bootSector.bpb.ondiskBpb);

		//Now we have FAT32 Bios Parameter Block ready for our FS max volume offset calculation -Kartha.
		//In fat all LCN counting starts from Starting data area i.e. after FAT. Formula for fat
        //should be StartingDataSector *bytes/sector+Total LCN*sectors/cluster*bytes/sector.
               
        Fat32RootDirectoryFirstSector = Fat32FirstDataSectorOfFirstLCN = (ULONG)0;

		Fat32RootDirectoryFirstSector = fat32Bpb.ReservedSectors + \
			(fat32Bpb.NumberOfFats * fat32Bpb.BigSectorsPerFat);

		Fat32FirstDataSectorOfFirstLCN = Fat32RootDirectoryFirstSector + \
			((fat32Bpb.RootEntries * 32) / 512);//Each directory entry is 32 bytes long fixed.

		if ((fat32Bpb.RootEntries * 32) % 512) {
            Fat32FirstDataSectorOfFirstLCN++;
        }

		//Let's find total clusters that FS is aware of.- Kartha.
		
        Fat32RootSectors = (fat32Bpb.RootEntries * 32) + \
        ( (fat32Bpb.BytesPerSector - 1)/fat32Bpb.BytesPerSector);

		Fat32FatSectors = fat32Bpb.NumberOfFats * fat32Bpb.SectorsPerFat;

		Fat32DataSectors = fat32Bpb.BigNumberOfSectors - \
		(fat32Bpb.ReservedSectors + Fat32FatSectors + Fat32RootSectors);

        //NOTE: This is for entire Volume which includes FAT32 agnostic areas at the end.
		TotalClusters.QuadPart = Fat32DataSectors / fat32Bpb.SectorsPerCluster;

        //FAT32 FS aware clusters FsSizeInformation.TotalAllocationUnits.QuadPart
		pFsInfo->TotalClustersInFS.QuadPart = FsSizeInformation.TotalAllocationUnits.QuadPart;
		pFsInfo->SizeOfFsBlockInBytes = fat32Bpb.SectorsPerCluster * fat32Bpb.BytesPerSector;
		pFsInfo->SectorOffsetOfFirstDataLCN.QuadPart = Fat32FirstDataSectorOfFirstLCN;
		pFsInfo->SectorsPerCluster = fat32Bpb.SectorsPerCluster;
		pFsInfo->BytesPerSector = fat32Bpb.BytesPerSector;

		pFsInfo->MaxVolumeByteOffsetForFs = (Fat32FirstDataSectorOfFirstLCN * fat32Bpb.BytesPerSector) + \
		(FsSizeInformation.TotalAllocationUnits.QuadPart* fat32Bpb.SectorsPerCluster * fat32Bpb.BytesPerSector);

		Status = STATUS_SUCCESS;
        goto ClusterOffsetEpilogue;
			

    break;
	
	default:

	break;
		
  }


  
  ClusterOffsetEpilogue:

	if(volumeHandle)ZwClose(volumeHandle);

  return Status;
}
#endif
