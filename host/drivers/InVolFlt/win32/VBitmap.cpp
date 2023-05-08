#include "Common.h"
#include "global.h"
#include "VContext.h"
#include "TagNode.h"
#include "MntMgr.h"
#include "misc.h"
#include "VBitmap.h"
#include "DirtyBlock.h"

void
RequestServiceThreadToOpenBitmap(PVOLUME_CONTEXT  pVolumeContext)
{
    KIRQL   OldIrql;
    BOOLEAN bWakeServiceThread = FALSE;

    KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
    if ((NULL == pVolumeContext->pVolumeBitmap) &&
            0 == (pVolumeContext->ulFlags & VCF_OPEN_BITMAP_REQUESTED)) {
        pVolumeContext->ulFlags |= VCF_OPEN_BITMAP_REQUESTED;
        // Wake up the Volume monitor thread.
        // Requesting to open bitmap after lock is released.
        bWakeServiceThread = TRUE;
    }
    KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);

    if (bWakeServiceThread)
        WakeServiceThread(FALSE /*DriverContext.Lock is not acquired */);

    return;
}

const char *
GetVolumeBitmapStateString(etVBitmapState eVBitmapState)
{
    switch (eVBitmapState) {
        case ecVBitmapStateUnInitialized:
            return "ecVBitmapStateUnInitialized";
            break;
        case ecVBitmapStateOpened:
            return "ecVBitmapStateOpened";
            break;
        case ecVBitmapStateReadStarted:
            return "ecVBitmapStateReadStarted";
            break;
        case ecVBitmapStateReadPaused:
            return "ecVBitmapStateReadPaused";
            break;
        case ecVBitmapStateReadCompleted:
            return "ecVBitmapStateReadCompleted";
            break;
        case ecVBitmapStateAddingChanges:
            return "ecVBitmapStateAddingChanges";
            break;
        case ecVBitmapStateClosed:
            return "ecVBitmapStateClosed";
            break;
        case ecVBitmapStateReadError:
            return "ecVBitmapStateReadError";
            break;
        case ecVBitmapStateInternalError:
            return "ecVBitmapStateInternalError";
            break;
        default:
            return "ecVBitmapStateUnknown";
            break;
    }
}

/*-----------------------------------------------------------------------------
 * Functions Related to Bitmap Write
 *-----------------------------------------------------------------------------
 */
VOID
WriteBitMapCompletion(PBITMAP_WORK_ITEM pBitmapWorkItem);

VOID
WriteBitMapCompletionWorkerRoutine(PWORK_QUEUE_ENTRY    pWorkQueueEntry);

VOID
BitmapWriteWorkerRoutine(PWORK_QUEUE_ENTRY   pWorkQueueEntry);

/*-----------------------------------------------------------------------------------
 * Function : WriteBitmapCompletionCallback
 * Parameters : BitRuns - This indicates the changes that have been writen
 *              to bit map
 * NOTES :
 * This function is called after write completion. This function can be 
 * called at DISPACH_LEVEL. If called at DISPATCH_LEVEL this function queues worker 
 * routine to continue post write operation at PASSIVE_LEVEL. 
 *
 *-----------------------------------------------------------------------------------
 */
VOID
WriteBitmapCompletionCallback(BitRuns *pBitRuns)
{
    PWORK_QUEUE_ENTRY   pWorkQueueEntry;
    PBITMAP_WORK_ITEM   pBitmapWorkItem;

    pBitmapWorkItem = (PBITMAP_WORK_ITEM)pBitRuns->context1;
    ASSERT((ecBitmapWorkItemClearBits == pBitmapWorkItem->eBitmapWorkItem) ||
           (ecBitmapWorkItemSetBits == pBitmapWorkItem->eBitmapWorkItem));
    ASSERT(NULL != pBitmapWorkItem->pWorkQueueEntryPreAlloc);    
    if (PASSIVE_LEVEL == KeGetCurrentIrql()) {
        // We can call completion directly as this would not initiate new IO causing recurstion.
        WriteBitMapCompletion(pBitmapWorkItem);
    } else {
        // As this function can be called at dispatch level we can not write to registry in case of error
        // So queue a work item for this purpose.        
        ASSERT(0 == pBitmapWorkItem->bBitMapWorkQueueRef);
        pWorkQueueEntry = pBitmapWorkItem->pWorkQueueEntryPreAlloc;
        pBitmapWorkItem->bBitMapWorkQueueRef = 1;
        //Cleanup of the given WorkQueuEntry required for reusing        
        InitializeWorkQueueEntry(pWorkQueueEntry);
        pWorkQueueEntry->Context = pBitmapWorkItem;
        pWorkQueueEntry->WorkerRoutine = WriteBitMapCompletionWorkerRoutine;
        AddItemToWorkQueue(&DriverContext.WorkItemQueue, pWorkQueueEntry);
    }

    return;
}

/*---------------------------------------------------------------------------
 * Function Name : WriteBitMapCompletionWorkerRoutine
 * Parameters :
 *      pWorkQueueEntry : pointer to work queue entry, Context member has 
 *              pointer to BitmapWorkItem. BitmapWorkItem.BitRuns has write 
 *              meta data/changes we requested to write in bit map.
 * NOTES :
 * This function is called at PASSIVE_LEVEL. This function is worker routine
 * called by worker thread at PASSIVE_LEVEL. This worker routine is scheduled
 * for execution by write completion routine.
 *
 *-----------------------------------------------------------------------------
 */
VOID
WriteBitMapCompletionWorkerRoutine(PWORK_QUEUE_ENTRY    pWorkQueueEntry)
{
    PBITMAP_WORK_ITEM   pBitmapWorkItem;

    PAGED_CODE();

    pBitmapWorkItem = (PBITMAP_WORK_ITEM)pWorkQueueEntry->Context;
    ASSERT(1 == pBitmapWorkItem->bBitMapWorkQueueRef);
    pBitmapWorkItem->bBitMapWorkQueueRef = 0;    
    WriteBitMapCompletion(pBitmapWorkItem);

    return;
}

/*---------------------------------------------------------------------------
 * Function Name : WriteBitMapCompletion
 * Parameters :
 *      pWorkQueueEntry : pointer to work queue entry, Context member has 
 *              pointer to BitmapWorkItem. BitmapWorkItem.BitRuns has write 
 *              meta data/changes we requested to write in bit map.
 * NOTES :
 * This function is called at PASSIVE_LEVEL. This function is worker routine
 * called by worker thread at PASSIVE_LEVEL. This worker routine is scheduled
 * for execution by write completion routine.
 *
 *-----------------------------------------------------------------------------
 */
VOID
WriteBitMapCompletion(PBITMAP_WORK_ITEM pBitmapWorkItem)
{
    PVOLUME_BITMAP  pVolumeBitmap;
    PVOLUME_CONTEXT pVolumeContext;
    KIRQL           OldIrql;
#if DBG
    PLIST_ENTRY     pEntry;
    BOOLEAN         bFound;
#endif // DBG

    PAGED_CODE();
    ASSERT((NULL != pBitmapWorkItem) && (NULL != pBitmapWorkItem->pVolumeBitmap));
    if ((NULL == pBitmapWorkItem) || (NULL == pBitmapWorkItem->pVolumeBitmap)) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE, 
            ("WriteBitMapCompletion: pBitmapWorkItem = %#p, pVolumeBitmap = %#p\n",
            pBitmapWorkItem, pBitmapWorkItem ? pBitmapWorkItem->pVolumeBitmap : NULL));
        return;
    }

    pVolumeBitmap = pBitmapWorkItem->pVolumeBitmap;

    ExAcquireFastMutex(&pVolumeBitmap->Mutex);
    
    pVolumeContext = pVolumeBitmap->pVolumeContext;
    if (ecBitmapWorkItemSetBits == pBitmapWorkItem->eBitmapWorkItem) {
        if (STATUS_SUCCESS != pBitmapWorkItem->BitRuns.finalStatus) {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE,
                ("WriteBitMapCompletion: SetBits work item failed with status %#x for volume %S(%S)\n",
                    pBitmapWorkItem->BitRuns.finalStatus, pVolumeBitmap->VolumeGUID, pVolumeBitmap->VolumeLetter));

            if ((ecVBitmapStateClosed != pVolumeBitmap->eVBitmapState) &&
                (NULL != pVolumeContext))
            {
                InterlockedIncrement(&pVolumeContext->lNumBitMapWriteErrors);
                SetVolumeOutOfSync(pVolumeContext, ERROR_TO_REG_BITMAP_WRITE_ERROR, pBitmapWorkItem->BitRuns.finalStatus, false);
                InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE,
                    ("WriteBitMapCompletion: Setting volume %S(%S) out of sync due to write error\n",
                        pVolumeBitmap->VolumeGUID, pVolumeBitmap->VolumeLetter));
            }
        } else {
            if ((ecVBitmapStateClosed != pVolumeBitmap->eVBitmapState) &&
                (NULL != pVolumeContext))
            {
                KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
                pVolumeContext->ulChangesQueuedForWriting -= pBitmapWorkItem->ulChanges;
                pVolumeContext->uliChangesWrittenToBitMap.QuadPart += pBitmapWorkItem->ulChanges;
                pVolumeContext->ulicbChangesQueuedForWriting.QuadPart -= pBitmapWorkItem->ullCbChangeData;
                pVolumeContext->ulicbChangesWrittenToBitMap.QuadPart += pBitmapWorkItem->ullCbChangeData;
                KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);
            }
            if ((ecVBitmapStateClosed == pVolumeBitmap->eVBitmapState) &&
                (NULL != pVolumeContext)) {
                InMageFltLogError(pVolumeContext->FilterDeviceObject, __LINE__,
                    INVOLFLT_FILE_CLOSE_ON_BITMAP_WRITE_COMPLETION, STATUS_SUCCESS,
                    pVolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES,
                    pVolumeContext->wGUID, GUID_SIZE_IN_BYTES);				
            }
        }
    } else {
        ASSERT(ecBitmapWorkItemClearBits == pBitmapWorkItem->eBitmapWorkItem);

        if ((NULL != pVolumeContext) &&
            (0 == (pVolumeContext->ulFlags & VCF_CV_FS_UNMOUTNED)) &&
            (STATUS_SUCCESS != pBitmapWorkItem->BitRuns.finalStatus)) 
        {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE,
                ("WriteBitMapCompletion: ClearBits work item failed with status %#x for volume %S(%S)\n",
                    pBitmapWorkItem->BitRuns.finalStatus, pVolumeBitmap->VolumeGUID, pVolumeBitmap->VolumeLetter));
            InterlockedIncrement(&pVolumeContext->lNumBitMapClearErrors);
        }
    }

    // For all the cases we have to remove this entry from list.
    if (ecBitmapWorkItemSetBits == pBitmapWorkItem->eBitmapWorkItem) {
        KeAcquireSpinLock(&pVolumeBitmap->Lock, &OldIrql);
        ASSERT(TRUE == IsEntryInList(&pVolumeBitmap->SetBitsWorkItemList, &pBitmapWorkItem->ListEntry));

        RemoveEntryList(&pBitmapWorkItem->ListEntry);
        if (pVolumeBitmap->ulFlags & VOLUME_BITMAP_FLAGS_WAITING_FOR_SETBITS_WORKITEM_LIST_EMPTY_NOTIFICATION) {
            if (IsListEmpty(&pVolumeBitmap->SetBitsWorkItemList)) {
                pVolumeBitmap->ulFlags &= ~VOLUME_BITMAP_FLAGS_WAITING_FOR_SETBITS_WORKITEM_LIST_EMPTY_NOTIFICATION;
                KeSetEvent(&pVolumeBitmap->SetBitsWorkItemListEmtpyNotification, IO_NO_INCREMENT, FALSE);
            }
        }

        KeReleaseSpinLock(&pVolumeBitmap->Lock, OldIrql);
    } else {
        ASSERT(ecBitmapWorkItemClearBits == pBitmapWorkItem->eBitmapWorkItem);
        ASSERT(TRUE == IsEntryInList(&pVolumeBitmap->WorkItemList, &pBitmapWorkItem->ListEntry));
        RemoveEntryList(&pBitmapWorkItem->ListEntry);
    }

    ExReleaseFastMutex(&pVolumeBitmap->Mutex);

    DereferenceBitmapWorkItem(pBitmapWorkItem);
    return;
}

/*-----------------------------------------------------------------------------
 * Function Name : QueueWorkerRoutineForBitmapWrite
 * Parameters :
 *      pVolumeContext : This parameter is referenced and stored in the 
 *                            work item.
 *      ulNumDirtyChanges   : if this value is zero all the dirty changes are 
 *                            copied. If this value is non zero, minimum of 
 *                            ulNumDirtyChanges are copied.
 * Retruns :
 *      BOOLEAN             : TRUE if succeded in queing work item for write
 *                            FALSE if failed in queing work item
 * NOTES :
 * This function is called at DISPATCH_LEVEL. This function is called with 
 * Spinlock to VolumeContext held.
 *
 *      This function removes changes from volume context and queues set bits
 * work item for each dirty block. These work items are later removed and
 * processed by Work queue
 *      
 *
 *--------------------------------------------------------------------------------------------
 */
BOOLEAN
QueueWorkerRoutineForBitmapWrite(
    PVOLUME_CONTEXT     pVolumeContext,
    ULONG               ulNumDirtyChanges
    )
{
    PWORK_QUEUE_ENTRY   pWorkItem = NULL;
    PBITMAP_WORK_ITEM   pBitmapWorkItem = NULL;
    PKDIRTY_BLOCK       pDirtyBlock = NULL;
    ULONG               ulChangesCopied = 0;
    KIRQL               OldIrql;

    if (NULL == pVolumeContext->pVolumeBitmap) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE,
            ("QueueWorkerRoutineForBitmapWrite: Called when pVolumeBitmap is NULL\n"));
        SetBitmapOpenFailDueToLossOfChanges(pVolumeContext, true);
        return FALSE;
    }

    // Removing (0 != pVolumeContext->ulTotalChangesPending) condition as
    // we want to flush last tags which do not have changes.
    while (!IsListEmpty(&pVolumeContext->ChangeList.Head)) {
        // There are changes to be copied, so lets create Dirty Block work items.
        if (ulNumDirtyChanges && (ulChangesCopied >= ulNumDirtyChanges))
            break; 
                // Nothing is drained from memory and writing to bitmap
            if ((!IS_CLUSTER_VOLUME(pVolumeContext)) && (0 == pVolumeContext->EndTimeStampofDB))
            {
                PCHANGE_NODE pOldestChangeNode = (PCHANGE_NODE)pVolumeContext->ChangeList.Head.Flink;
                PKDIRTY_BLOCK pOldestDB = pOldestChangeNode->DirtyBlock;

                if (pOldestDB->cChanges)
                    pVolumeContext->EndTimeStampofDB = pOldestDB->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601;
            } 
            pBitmapWorkItem = AllocateBitmapWorkItem(ecBitmapWorkItemSetBits);
            if (NULL != pBitmapWorkItem) {
                PVOLUME_BITMAP      pVolumeBitmap = NULL;
                PCHANGE_NODE        ChangeNode = GetChangeNodeFromChangeList(pVolumeContext, false);
                pDirtyBlock = ChangeNode->DirtyBlock;

                if (pDirtyBlock->ulFlags & KDIRTY_BLOCK_FLAG_CONTAINS_TAGS) {
                    pVolumeContext->ulNumberOfTagPointsDropped++;

                    if (pDirtyBlock->ulFlags & KDIRTY_BLOCK_FLAG_TAG_WAITING_FOR_COMMIT) {
                        PTAG_NODE TagNode = AddStatusAndReturnNodeIfComplete(pDirtyBlock->TagGUID, pDirtyBlock->ulTagVolumeIndex, ecTagStatusDropped);
                        if (NULL != TagNode) {
                            // Complete the Irp with Status
                            PIRP Irp = TagNode->Irp;

                            CopyStatusToOutputBuffer(TagNode, (PTAG_VOLUME_STATUS_OUTPUT_DATA)Irp->AssociatedIrp.SystemBuffer);
                            Irp->IoStatus.Information = SIZE_OF_TAG_VOLUME_STATUS_OUTPUT_DATA(TagNode->ulNoOfVolumes);;
                            Irp->IoStatus.Status = STATUS_SUCCESS;
                            IoCompleteRequest(Irp, IO_NO_INCREMENT);

                            DeallocateTagNode(TagNode);
                        }
                        pDirtyBlock->ulFlags &= ~KDIRTY_BLOCK_FLAG_TAG_WAITING_FOR_COMMIT;
                    }
                }
                if (pDirtyBlock->ulcbDataFree) {
                    ASSERT(pDirtyBlock->ulcbDataFree <= pVolumeContext->ulcbDataFree); 
                    VCAdjustCountersForLostDataBytes(pVolumeContext, NULL, pDirtyBlock->ulcbDataFree);
                    pDirtyBlock->ulcbDataFree = 0;
                }
                DeductChangeCountersOnDataSource(&pVolumeContext->ChangeList, pDirtyBlock->ulDataSource,
                                                 pDirtyBlock->cChanges, pDirtyBlock->ulicbChanges.QuadPart);

                pVolumeContext->ulChangesQueuedForWriting += pDirtyBlock->cChanges;
                pVolumeContext->ulicbChangesQueuedForWriting.QuadPart += pDirtyBlock->ulicbChanges.QuadPart;
                ulChangesCopied += pDirtyBlock->cChanges;

                pVolumeBitmap = pVolumeContext->pVolumeBitmap;
                pBitmapWorkItem->eBitmapWorkItem = ecBitmapWorkItemSetBits;
                pBitmapWorkItem->pVolumeBitmap = ReferenceVolumeBitmap(pVolumeBitmap);
                pBitmapWorkItem->ulChanges = pDirtyBlock->cChanges;
                pBitmapWorkItem->ullCbChangeData = pDirtyBlock->ulicbChanges.QuadPart;
                pBitmapWorkItem->BitRuns.nbrRuns = pDirtyBlock->cChanges;
                pBitmapWorkItem->BitRuns.pdirtyBlock = pDirtyBlock;
 
                DereferenceChangeNode(ChangeNode);
                // if bitmap file is already opened. We can directly queue work item to worker queue.
                // if it is not opened lets insert to list and will be processed later.
                pWorkItem = pBitmapWorkItem->pWorkQueueEntryPreAlloc;
                pBitmapWorkItem->bBitMapWorkQueueRef = 1;
                pWorkItem->Context = pBitmapWorkItem;
                pWorkItem->WorkerRoutine = BitmapWriteWorkerRoutine;
                KeAcquireSpinLock(&pVolumeBitmap->Lock, &OldIrql);
                InsertHeadList(&pVolumeBitmap->SetBitsWorkItemList, &pBitmapWorkItem->ListEntry);
                KeReleaseSpinLock(&pVolumeBitmap->Lock, OldIrql);

                AddItemToWorkQueue(&DriverContext.WorkItemQueue, pWorkItem);
                
            } else {
                InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE,
                    ("QueueWorkerRoutineForBitmapWrite: Failed to allocate BitmapWorkItem\n"));
                return FALSE;
            }            
    }

    return TRUE;
}

VOID
BitmapWriteWorkerRoutine(PWORK_QUEUE_ENTRY   pWorkQueueEntry)
{
    PBITMAP_WORK_ITEM   pBitmapWorkItem;
    BitRuns             *pBitRun;
    PVOLUME_BITMAP      pVolumeBitmap;
    bool                bRemoveEntry = false;
    PKDIRTY_BLOCK DirtyBlock = NULL;
    PCHANGE_NODE ChangeNode = NULL;    

    pBitmapWorkItem = (PBITMAP_WORK_ITEM) pWorkQueueEntry->Context;
    ASSERT(1 == pBitmapWorkItem->bBitMapWorkQueueRef);
    pBitmapWorkItem->bBitMapWorkQueueRef = 0;

    ASSERT(NULL != pBitmapWorkItem);
    if (NULL == pBitmapWorkItem) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE, ("BitmapWriteWorkerRoutine: pBitmapWorkItem is NULL\n"));
        return;
    }

    pVolumeBitmap = pBitmapWorkItem->pVolumeBitmap;
    // Acquire the lock.
    if (NULL != pVolumeBitmap) {
        // ReferenceVolumeBitmap to keep bitmap available till it is unlocked.
        ReferenceVolumeBitmap(pVolumeBitmap);
        ExAcquireFastMutex(&pVolumeBitmap->Mutex);

        if ((NULL != pVolumeBitmap->pVolumeContext) && (0 == (pVolumeBitmap->pVolumeContext->ulFlags & VCF_FILTERING_STOPPED))) {
            if ((ecVBitmapStateClosed == pVolumeBitmap->eVBitmapState) ||
                (NULL == pVolumeBitmap->pBitmapAPI))
            {
                InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE,
                    ("BitmapWriteWorkerRoutine: Failed State = %s, pBitmapAPI = %#p for volume %S(%S)\n",
                        GetVolumeBitmapStateString(pVolumeBitmap->eVBitmapState), pVolumeBitmap->pBitmapAPI,
                        pVolumeBitmap->VolumeGUID, pVolumeBitmap->VolumeLetter));
                 bRemoveEntry = true;
            } else {
                pBitmapWorkItem->BitRuns.context1 = pBitmapWorkItem;
                pBitmapWorkItem->BitRuns.completionCallback = WriteBitmapCompletionCallback;
                DirtyBlock = pBitmapWorkItem->BitRuns.pdirtyBlock;
                ChangeNode = DirtyBlock->ChangeNode;                
                pVolumeBitmap->pBitmapAPI->SetBits(&pBitmapWorkItem->BitRuns);
            }
        } else {
            bRemoveEntry = true;
        }
        if (bRemoveEntry) {
           if (ecBitmapWorkItemSetBits == pBitmapWorkItem->eBitmapWorkItem) {
                KIRQL       OldIrql;

                KeAcquireSpinLock(&pVolumeBitmap->Lock, &OldIrql);
                ASSERT(TRUE == IsEntryInList(&pVolumeBitmap->SetBitsWorkItemList, &pBitmapWorkItem->ListEntry));

                RemoveEntryList(&pBitmapWorkItem->ListEntry);
                if (pVolumeBitmap->ulFlags & VOLUME_BITMAP_FLAGS_WAITING_FOR_SETBITS_WORKITEM_LIST_EMPTY_NOTIFICATION) {
                    if (IsListEmpty(&pVolumeBitmap->SetBitsWorkItemList)) {
                        pVolumeBitmap->ulFlags &= ~VOLUME_BITMAP_FLAGS_WAITING_FOR_SETBITS_WORKITEM_LIST_EMPTY_NOTIFICATION;
                        KeSetEvent(&pVolumeBitmap->SetBitsWorkItemListEmtpyNotification, IO_NO_INCREMENT, FALSE);
                    }
                }

                KeReleaseSpinLock(&pVolumeBitmap->Lock, OldIrql);
            } else {
                ASSERT(ecBitmapWorkItemClearBits == pBitmapWorkItem->eBitmapWorkItem);
                ASSERT(TRUE == IsEntryInList(&pVolumeBitmap->WorkItemList, &pBitmapWorkItem->ListEntry));
                RemoveEntryList(&pBitmapWorkItem->ListEntry);
            }

            DereferenceBitmapWorkItem(pBitmapWorkItem);
        }

        ExReleaseFastMutex(&pVolumeBitmap->Mutex);
        DereferenceVolumeBitmap(pVolumeBitmap);

        if (ChangeNode != NULL) {
            // QueueWorkerRoutineForChangeNodeCleanup(ChangeNode);
            ChangeNodeCleanup(ChangeNode);
        }
    } else {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE,
            ("BitmapWriteWorkerRoutine: pBitmapWorkItem->pVolumeBitmap is NULL\n"));
    }
    return;
}


/*-----------------------------------------------------------------------------
 * Functions Related to Bitmap Read
 *-----------------------------------------------------------------------------
 */

VOID
StartBitmapReadWorkerRoutine(PWORK_QUEUE_ENTRY   pWorkQueueEntry);

VOID
ContinueBitmapReadWorkerRoutine(PWORK_QUEUE_ENTRY   pWorkQueueEntry);

VOID
ContinueBitmapRead(PBITMAP_WORK_ITEM   pBitmapWorkItem, BOOLEAN bMutexAcquired);

VOID
ReadBitmapCompletionCallback(BitRuns *pBitRuns);

VOID
ReadBitMapCompletionWorkerRoutine(PWORK_QUEUE_ENTRY pWorkQueueEntry);

VOID
ReadBitmapCompletion(PBITMAP_WORK_ITEM pBitmapWorkItem);

VOID
QueueWorkerRoutineForStartBitmapRead(PVOLUME_BITMAP  pVolumeBitmap)
{
    PWORK_QUEUE_ENTRY       pWorkQueueEntry;
    PBITMAP_WORK_ITEM       pBitmapWorkItem;

    ASSERT(NULL != pVolumeBitmap);

    if (NULL == pVolumeBitmap) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_READ | DM_BITMAP_OPEN, 
                ("QueueWorkerRoutineForStartBitmapRead: pVolumeBitmap is NULL\n"));
        return;
    }

        pBitmapWorkItem = AllocateBitmapWorkItem(ecBitmapWorkItemStartRead);
        if (NULL != pBitmapWorkItem) {
            pWorkQueueEntry = pBitmapWorkItem->pWorkQueueEntryPreAlloc;
            pWorkQueueEntry->Context = pBitmapWorkItem;
            pBitmapWorkItem->bBitMapWorkQueueRef = 1;
            pWorkQueueEntry->WorkerRoutine = StartBitmapReadWorkerRoutine;
            pBitmapWorkItem->pVolumeBitmap = ReferenceVolumeBitmap(pVolumeBitmap);
            pBitmapWorkItem->eBitmapWorkItem = ecBitmapWorkItemStartRead;
            AddItemToWorkQueue(&DriverContext.WorkItemQueue, pWorkQueueEntry);
        } else {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_READ,
                ("QueueWorkerRoutineForStartBitmapRead: Failed to allocate BitmapWorkItem\n"));
        }

    return;
}

VOID
QueueWorkerRoutineForContinueBitmapRead(PVOLUME_BITMAP  pVolumeBitmap)
{
    PWORK_QUEUE_ENTRY       pWorkQueueEntry;
    PBITMAP_WORK_ITEM       pBitmapWorkItem;

    ASSERT(NULL != pVolumeBitmap);

    if (NULL == pVolumeBitmap) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_READ | DM_BITMAP_OPEN, 
                ("QueueWorkerRoutineForContinueBitmapRead: pVolumeBitmap is NULL\n"));
        return;
    }

        pBitmapWorkItem = AllocateBitmapWorkItem(ecBitmapWorkItemContinueRead);
        if (NULL != pBitmapWorkItem) {
            pWorkQueueEntry = pBitmapWorkItem->pWorkQueueEntryPreAlloc;
            pWorkQueueEntry->Context = pBitmapWorkItem;
            pBitmapWorkItem->bBitMapWorkQueueRef = 1;
            pWorkQueueEntry->WorkerRoutine = ContinueBitmapReadWorkerRoutine;
            pBitmapWorkItem->pVolumeBitmap = ReferenceVolumeBitmap(pVolumeBitmap);
            pBitmapWorkItem->eBitmapWorkItem = ecBitmapWorkItemContinueRead;
            AddItemToWorkQueue(&DriverContext.WorkItemQueue, pWorkQueueEntry);
        } else {
            InVolDbgPrint(DL_ERROR, DM_MEM_TRACE | DM_BITMAP_READ,
                ("QueueWorkerRoutineForContinueBitmapRead: Failed to allocate BitmapWorkItem\n"));
        }                

    return;
}

VOID
StartBitmapReadWorkerRoutine(PWORK_QUEUE_ENTRY   pWorkQueueEntry)
{
    PBITMAP_WORK_ITEM   pBitmapWorkItem;
    BitRuns             *pBitRun;
    PVOLUME_BITMAP      pVolumeBitmap;

    pBitmapWorkItem = (PBITMAP_WORK_ITEM) pWorkQueueEntry->Context;
    ASSERT(1 == pBitmapWorkItem->bBitMapWorkQueueRef);
    pBitmapWorkItem->bBitMapWorkQueueRef = 0;

    ASSERT((NULL != pBitmapWorkItem) && (NULL != pBitmapWorkItem->pVolumeBitmap));
    if ((NULL == pBitmapWorkItem) || (NULL == pBitmapWorkItem->pVolumeBitmap)) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_READ, ("StartBitmapReadWorkerRoutine: pBitmapWorkItem = %#p, pVolumeBitmap = %#p\n",
            pBitmapWorkItem, pBitmapWorkItem ? pBitmapWorkItem->pVolumeBitmap : NULL));
        return;
    }

    // Acquire the lock.
    pVolumeBitmap = ReferenceVolumeBitmap(pBitmapWorkItem->pVolumeBitmap);
    if ((NULL != pVolumeBitmap->pVolumeContext) && (0 == (pVolumeBitmap->pVolumeContext->ulFlags & VCF_FILTERING_STOPPED))) {
        ExAcquireFastMutex(&pVolumeBitmap->Mutex);

        if ((ecVBitmapStateReadStarted == pVolumeBitmap->eVBitmapState) &&
            (NULL != pVolumeBitmap->pBitmapAPI))
        {
            pBitmapWorkItem->BitRuns.context1 = pBitmapWorkItem;
            pBitmapWorkItem->BitRuns.completionCallback = ReadBitmapCompletionCallback;
            InsertHeadList(&pVolumeBitmap->WorkItemList, &pBitmapWorkItem->ListEntry);
            pVolumeBitmap->pBitmapAPI->GetFirstRuns(&pBitmapWorkItem->BitRuns);
            pBitmapWorkItem = NULL;
        }

        ExReleaseFastMutex(&pVolumeBitmap->Mutex);
    }
    DereferenceVolumeBitmap(pVolumeBitmap);

    if (NULL != pBitmapWorkItem)
        DereferenceBitmapWorkItem(pBitmapWorkItem);

    return;
}

VOID
ContinueBitmapReadWorkerRoutine(PWORK_QUEUE_ENTRY   pWorkQueueEntry)
{
    PBITMAP_WORK_ITEM   pBitmapWorkItem;
    BitRuns             *pBitRun;

    pBitmapWorkItem = (PBITMAP_WORK_ITEM) pWorkQueueEntry->Context;
    ASSERT(1 == pBitmapWorkItem->bBitMapWorkQueueRef);
    pBitmapWorkItem->bBitMapWorkQueueRef = 0;

    ASSERT(NULL != pBitmapWorkItem);
    if (NULL == pBitmapWorkItem) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_READ, ("ContinueBitmapReadWorkerRoutine: pBitmapWorkItem is NULL\n"));
        return;
    }

    ContinueBitmapRead(pBitmapWorkItem, FALSE);
    return;
}

VOID
ContinueBitmapRead(PBITMAP_WORK_ITEM   pBitmapWorkItem, BOOLEAN bMutexAcquired)
{
    PVOLUME_BITMAP      pVolumeBitmap;

    pVolumeBitmap = pBitmapWorkItem->pVolumeBitmap;
    // Acquire the lock.
    if (NULL != pVolumeBitmap) {
        ReferenceVolumeBitmap(pVolumeBitmap);
        if ((NULL != pVolumeBitmap->pVolumeContext) && (0 == (pVolumeBitmap->pVolumeContext->ulFlags & VCF_FILTERING_STOPPED))) {
            if (FALSE == bMutexAcquired)
                ExAcquireFastMutex(&pVolumeBitmap->Mutex);

            if ((ecVBitmapStateReadStarted != pVolumeBitmap->eVBitmapState) ||
                (NULL == pVolumeBitmap->pBitmapAPI))
            {
                DereferenceBitmapWorkItem(pBitmapWorkItem);
                // Ignore the read.
            } else {
                pBitmapWorkItem->BitRuns.context1 = pBitmapWorkItem;
                pBitmapWorkItem->BitRuns.completionCallback = ReadBitmapCompletionCallback;
                InsertHeadList(&pVolumeBitmap->WorkItemList, &pBitmapWorkItem->ListEntry);
                pVolumeBitmap->pBitmapAPI->GetNextRuns(&pBitmapWorkItem->BitRuns);
            }

            if (FALSE == bMutexAcquired)
                ExReleaseFastMutex(&pVolumeBitmap->Mutex);
        }
        DereferenceVolumeBitmap(pVolumeBitmap);
    } else {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_READ,
            ("StartBitmapReadWorkerRoutine: pBitmapWorkItem->pVolumeBitmap is NULL\n"));
    }

    return;
}
    
/*-----------------------------------------------------------------------------
 * Function : ReadBitmapCompletionCallback
 * Parameters : BitRuns - This indicates the changes that have been read
 *              to bit map
 * NOTES :
 * This function is called after read completion. This function can be 
 * called at DISPACH_LEVEL. If the function is called at DISPACH_LEVEL a
 * worker routine is queued to process the completion at PASSIVE_LEVEL
 * If this function is called at PASSIVE_LEVEL, the process function is 
 * called directly with out queuing worker routine.
 *
 *-----------------------------------------------------------------------------------
 */
VOID
ReadBitmapCompletionCallback(BitRuns *pBitRuns)
{
    PWORK_QUEUE_ENTRY   pWorkQueueEntry;

    // Do not call completion directly here, This could lead to recursive call and stack overflow.
    // Currently completion is called synchronously.
    PBITMAP_WORK_ITEM   pBitmapWorkItem = (PBITMAP_WORK_ITEM)pBitRuns->context1;
    
    ASSERT(NULL != pBitmapWorkItem->pWorkQueueEntryPreAlloc);
    ASSERT(0 == pBitmapWorkItem->bBitMapWorkQueueRef);
    
    pWorkQueueEntry = pBitmapWorkItem->pWorkQueueEntryPreAlloc;
    pBitmapWorkItem->bBitMapWorkQueueRef = 1;
    InitializeWorkQueueEntry(pWorkQueueEntry);
    pWorkQueueEntry->Context = pBitmapWorkItem;
    pWorkQueueEntry->WorkerRoutine = ReadBitMapCompletionWorkerRoutine;

    AddItemToWorkQueue(&DriverContext.WorkItemQueue, pWorkQueueEntry);
    
    
    return;
}


/*-----------------------------------------------------------------------------
 * Function : ReadBitMapCompletionWorkerRoutine
 * Parameters : BitRuns - This indicates the changes that have been read
 *              to bit map
 * NOTES :
 * This function is worker routine and queued if completion routine is called
 * at DISPATCH_LEVEL. This function calls the ReadBitmapCompletion funciton
 *
 *-----------------------------------------------------------------------------------
 */
VOID
ReadBitMapCompletionWorkerRoutine(PWORK_QUEUE_ENTRY    pWorkQueueEntry)
{
    PBITMAP_WORK_ITEM   pBitmapWorkItem;

    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    // Holding reference to VolumeContext so that BitMap File is not closed before the writes are completed.
    pBitmapWorkItem = (PBITMAP_WORK_ITEM)pWorkQueueEntry->Context;
    ASSERT(1 == pBitmapWorkItem->bBitMapWorkQueueRef);
    pBitmapWorkItem->bBitMapWorkQueueRef = 0;

    ReadBitmapCompletion(pBitmapWorkItem);

    return;
}


/*-----------------------------------------------------------------------------
 * Function : ReadBitmapCompletion
 * Parameters : BitRuns - This indicates the changes that have been read
 *              to bit map
 * NOTES :
 * This function is called after read completion. This function is 
 * called at PASSIVE_LEVEL. This function queues the read changes to device
 * specific dirty block context. After queuing changes it checks if changes
 * cross low water mark the read is paused. If the changes read are the last
 * set of changes the read state is set to completed. In this function we
 * have to unset the bits successfully read and send a next read.
 * 
 * To avoid copying of the bitruns, we would use the previous read DB_WORK_ITEM for
 * clearing bits and allocate a new one for read if required.
 *
 *-----------------------------------------------------------------------------------
 */

VOID
ReadBitmapCompletion(PBITMAP_WORK_ITEM pBitmapWorkItem)
{
    PVOLUME_BITMAP      pVolumeBitmap;
    PKDIRTY_BLOCK       pDirtyBlock = NULL;
    KIRQL               OldIrql;
    PVOLUME_CONTEXT     pVolumeContext = NULL;
    BOOLEAN             bClearBitsRead, bContinueBitMapRead;

    // pBitmapWorkItem is later used for clear bits.
    PAGED_CODE();

    ASSERT((NULL != pBitmapWorkItem) && (NULL != pBitmapWorkItem->pVolumeBitmap));
    if ((NULL == pBitmapWorkItem) || (NULL == pBitmapWorkItem->pVolumeBitmap)) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_READ, ("ReadBitmapCompletion: pBitmapWorkItem = %#p, pVolumeBitmap = %#p\n",
            pBitmapWorkItem, pBitmapWorkItem ? pBitmapWorkItem->pVolumeBitmap : NULL));
        return;
    }

    bClearBitsRead = FALSE;
    bContinueBitMapRead = FALSE;

    // Let us see if this bitmap is still operational.
    pVolumeBitmap = ReferenceVolumeBitmap(pBitmapWorkItem->pVolumeBitmap);
    ExAcquireFastMutex(&pVolumeBitmap->Mutex);
    // Check if bitmap is still in read state.
    if ((ecVBitmapStateReadStarted != pVolumeBitmap->eVBitmapState) ||
        (NULL == pVolumeBitmap->pBitmapAPI) ||
        (NULL == pVolumeBitmap->pVolumeContext) ||
        (pVolumeBitmap->pVolumeContext->ulFlags & VCF_CV_FS_UNMOUTNED))
    {
        // Bitmap is closed, or bitmap is moved to write state. Ingore this read.
        RemoveEntryList(&pBitmapWorkItem->ListEntry);
        DereferenceBitmapWorkItem(pBitmapWorkItem);
        InVolDbgPrint(DL_INFO, DM_BITMAP_READ,
            ("ReadBitmapCompletion: Ignoring bitmap read - State = %s, pBitmapAPI = %#p, pVolumeContext = %#p\n",
                GetVolumeBitmapStateString(pVolumeBitmap->eVBitmapState), pVolumeBitmap->pBitmapAPI, pVolumeBitmap->pVolumeContext));
    } else {
        pVolumeContext = pVolumeBitmap->pVolumeContext;
        do {
            // error processing.    
            if ((STATUS_SUCCESS != pBitmapWorkItem->BitRuns.finalStatus) &&
                (STATUS_MORE_PROCESSING_REQUIRED != pBitmapWorkItem->BitRuns.finalStatus))
            {
                pVolumeBitmap->eVBitmapState = ecVBitmapStateReadError;
                InterlockedIncrement(&pVolumeContext->lNumBitMapReadErrors);
                SetVolumeOutOfSync(pVolumeContext, ERROR_TO_REG_BITMAP_READ_ERROR, pBitmapWorkItem->BitRuns.finalStatus, false);
                InVolDbgPrint(DL_ERROR, DM_BITMAP_READ,
                    ("ReadBitmapCompletion: Received Error %#x Setting bitmap state of Volume %S(%S) to ecVBitmapError\n",
                        pBitmapWorkItem->BitRuns.finalStatus, pVolumeBitmap->VolumeGUID, pVolumeBitmap->VolumeLetter));
                break;
            }

            // Read Succeded, check if any changes are returned
            if ((STATUS_SUCCESS == pBitmapWorkItem->BitRuns.finalStatus) && (0 == pBitmapWorkItem->BitRuns.nbrRuns)) {
                pVolumeBitmap->eVBitmapState = ecVBitmapStateReadCompleted;
                InVolDbgPrint(DL_INFO, DM_BITMAP_READ,
                    ("ReadBitmapCompletion: Bitmap read of Volume %S(%S) is completed\n",
                        pVolumeBitmap->VolumeGUID, pVolumeBitmap->VolumeLetter));
                break;
            }
          
            ULONG VCTotalChangesRead = 0;
            ULONGLONG VCcbChangesReadFromBitMap = 0;
            ULONG VCTotalChangesPending = 0;

            // If Non Page pool consumption exceeds 80% of Max. Non Page Pool Limit,
            // dont do bitmap read and dont convert bitmap changes into metadata mode
            // dirtyblock changes. This is to avoid pair resync which occurs if we reach
            // max. non-page limit.
            if ((0 != DriverContext.ulMaxNonPagedPool) && (DriverContext.lNonPagedMemoryAllocated >= 0)) {
                ULONGLONG NonPagePoolLimit = DriverContext.ulMaxNonPagedPool * 8;
                NonPagePoolLimit /= 10;
                if ((ULONG) DriverContext.lNonPagedMemoryAllocated >= (ULONG) NonPagePoolLimit) {
                    pVolumeBitmap->eVBitmapState = ecVBitmapStateOpened;
                    break;
                }    
            }

            unsigned int i = 0;
            KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
            for (i = 0; i < pBitmapWorkItem->BitRuns.nbrRuns; i++) 
            {
                WRITE_META_DATA WriteMetaData;
                ULONG VCSplitChangesReturned = 0;

                WriteMetaData.llOffset = pBitmapWorkItem->BitRuns.runOffsetArray[i];
                WriteMetaData.ulLength = pBitmapWorkItem->BitRuns.runLengthArray[i];
                
                VCSplitChangesReturned = AddMetaDataToDirtyBlock(pVolumeContext, pDirtyBlock, &WriteMetaData, INVOLFLT_DATA_SOURCE_BITMAP);

                if(0 == VCSplitChangesReturned)
                    break;

                VCcbChangesReadFromBitMap += WriteMetaData.ulLength;
                VCTotalChangesPending += VCSplitChangesReturned;

                if(VCSplitChangesReturned > 1)
                    pDirtyBlock = NULL;
                else
                    pDirtyBlock = pVolumeContext->ChangeList.CurrentNode->DirtyBlock;
            }

            pVolumeContext->uliChangesReadFromBitMap.QuadPart += VCTotalChangesPending;
            pVolumeContext->ulicbChangesReadFromBitMap.QuadPart += VCcbChangesReadFromBitMap;

            KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);

            if(i < pBitmapWorkItem->BitRuns.nbrRuns)
            {
                pVolumeBitmap->eVBitmapState = ecVBitmapStateOpened;
                break;
            }

            bClearBitsRead = TRUE;
            if (STATUS_SUCCESS == pBitmapWorkItem->BitRuns.finalStatus) {
                // This is the last read.
                pVolumeBitmap->eVBitmapState = ecVBitmapStateReadCompleted;
                InVolDbgPrint(DL_INFO, DM_BITMAP_READ,
                    ("ReadBitmapCompletion: Bitmap read of Volume %S(%S) is completed\n",
                        pVolumeBitmap->VolumeGUID, pVolumeBitmap->VolumeLetter));
            } else if ((0 != DriverContext.DBLowWaterMarkWhileServiceRunning) && 
                (pVolumeContext->ChangeList.ulTotalChangesPending >=  DriverContext.DBLowWaterMarkWhileServiceRunning))
            {
                // pBitmapWorkItem->BitRuns.finalStatus is STATUS_MORE_PROCESSING_REQUIRED.
                // If this is not a final read, and reached the low water mark pause the reads.
                pVolumeBitmap->eVBitmapState = ecVBitmapStateReadPaused;
                InVolDbgPrint(DL_INFO, DM_BITMAP_READ,
                    ("ReadBitmapCompletion: Bitmap read of Volume %S(%S) is paused\n",
                        pVolumeBitmap->VolumeGUID, pVolumeBitmap->VolumeLetter));
            } else {
                bContinueBitMapRead = TRUE;
            }

        } while (0);

        if (TRUE == bContinueBitMapRead) {
            PBITMAP_WORK_ITEM   pBWIforContinuingRead = NULL;

            pBWIforContinuingRead = AllocateBitmapWorkItem(ecBitmapWorkItemContinueRead);
            if (NULL != pBWIforContinuingRead) {
                pBWIforContinuingRead->eBitmapWorkItem = ecBitmapWorkItemContinueRead;
                pBWIforContinuingRead->pVolumeBitmap = ReferenceVolumeBitmap(pVolumeBitmap);
                ContinueBitmapRead(pBWIforContinuingRead, TRUE);
            } else {
                pVolumeBitmap->eVBitmapState = ecVBitmapStateReadPaused;
                InVolDbgPrint(DL_ERROR, DM_MEM_TRACE | DM_BITMAP_READ,
                    ("ReadBitmapCompletion: Failed to allocate BitmapWorkItem to continue read for volume %S(%S)\nSetting state to paused",
                        pVolumeBitmap->pVolumeContext->wGUID, pVolumeBitmap->pVolumeContext->wDriveLetter));
            }                
        }

        if (TRUE == bClearBitsRead) {
            // Do not have to set pBitmapWorkItem->pVolumeBitmap and pBitmapWorkItem->BitRuns.context1
            // These are set when received. 
            // pBitmapWorkItem is already inserted in pVolumeBitmap->WorkItemList.
            // should not re-insert would lead to corruption. have to remove from list before re-insert
            // pBitmapWorkItem->pVolumeBitmap == pVolumeBitmap;
            // pBitmapWorkItem->BitRuns.context1 = pBitmapWorkItem; 
            // RemoveEntryList(&pBitmapWorkItem->ListEntry);
            // InsertHeadList(&pVolumeBitmap->WorkItemList, &pBitmapWorkItem->ListEntry);
            ASSERT(pBitmapWorkItem->BitRuns.context1 == pBitmapWorkItem);
            ASSERT(pBitmapWorkItem->pVolumeBitmap == pVolumeBitmap);
            pBitmapWorkItem->eBitmapWorkItem = ecBitmapWorkItemClearBits;
            pBitmapWorkItem->BitRuns.completionCallback = WriteBitmapCompletionCallback;
            pVolumeBitmap->pBitmapAPI->ClearBits(&pBitmapWorkItem->BitRuns);
        } else {
            RemoveEntryList(&pBitmapWorkItem->ListEntry);
            DereferenceBitmapWorkItem(pBitmapWorkItem);
        }

        if (NULL != pVolumeContext) {
            KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
            if (ecVBitmapStateReadCompleted == pVolumeBitmap->eVBitmapState) {
                // Check if capture mode can be changed.
                if ((ecCaptureModeMetaData == pVolumeContext->eCaptureMode) && CanSwitchToDataCaptureMode(pVolumeContext)) {
                    InVolDbgPrint(DL_INFO, DM_CAPTURE_MODE | DM_DATA_FILTERING,
                                  ("%s: Changing capture mode to DATA for volume %s(%s)\n", __FUNCTION__,
                                   pVolumeContext->GUIDinANSI, pVolumeContext->DriveLetter));
                    VCSetCaptureModeToData(pVolumeContext);
                }
                VCSetWOState(pVolumeContext,false,__FUNCTION__);
            }

            if ((NULL != pVolumeContext->DBNotifyEvent) && (pVolumeContext->bNotify)) {
                ASSERT(pVolumeContext->ChangeList.ullcbTotalChangesPending >= pVolumeContext->ullcbChangesPendingCommit);
                ULONGLONG   ullcbTotalChangesPending = pVolumeContext->ChangeList.ullcbTotalChangesPending - pVolumeContext->ullcbChangesPendingCommit;
                if (ullcbTotalChangesPending >= pVolumeContext->ulcbDataNotifyThreshold) {
                    pVolumeContext->bNotify = false;
                    pVolumeContext->liTickCountAtNotifyEventSet = KeQueryPerformanceCounter(NULL);
                    KeSetEvent(pVolumeContext->DBNotifyEvent, EVENT_INCREMENT, FALSE);
                }
            }

            KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);
        }
    }

    ExReleaseFastMutex(&pVolumeBitmap->Mutex);
    DereferenceVolumeBitmap(pVolumeBitmap);
    return;
}


NTSTATUS
GetVolumeBitmapGranularity(PVOLUME_CONTEXT pVolumeContext, PULONG pulBitmapGranularity)
{
    NTSTATUS        Status;
    Registry        *pVolumeParam = NULL;

    ASSERT(NULL != pVolumeContext);
    ASSERT(0 != pVolumeContext->llVolumeSize);
    ASSERT(NULL != pulBitmapGranularity);

    if (NULL == pulBitmapGranularity) {
        InVolDbgPrint( DL_INFO, DM_BITMAP_OPEN, ("GetVolumeBitmapGranularity: pulBitmapGranularity is NULL\n"));
        return STATUS_INVALID_PARAMETER;
    }

    if (NULL == pVolumeContext) {
        InVolDbgPrint( DL_INFO, DM_BITMAP_OPEN, ("GetVolumeBitmapGranularity: pVolumeContext is NULL\n"));
        return STATUS_INVALID_PARAMETER;
    }

    if (0 == pVolumeContext->llVolumeSize) {
        InVolDbgPrint( DL_INFO, DM_BITMAP_OPEN, 
                ("GetVolumeBitmapGranularity: VolumeSize is %#I64x\n", pVolumeContext->llVolumeSize));
        return STATUS_INVALID_PARAMETER;
    }

    Status = OpenVolumeParametersRegKey(pVolumeContext, &pVolumeParam);
    if (!NT_SUCCESS(Status) || (NULL == pVolumeParam)) {
        InVolDbgPrint( DL_INFO, DM_BITMAP_OPEN, 
            ("GetVolumeBitmapGranularity: Failed to open volume parameters key. Status = %#x\n", Status));        
        return Status;
    }

    *pulBitmapGranularity = DefaultGranularityFromVolumeSize(pVolumeContext->llVolumeSize);
    Status = pVolumeParam->ReadULONG(VOLUME_BITMAP_GRANULARITY, (ULONG32 *)pulBitmapGranularity, *pulBitmapGranularity, TRUE);
    if (!NT_SUCCESS(Status)) {
        InVolDbgPrint( DL_ERROR, DM_BITMAP_OPEN, ("OpenBitmapFile: Failed in retreiving %S, Status = %#x\n", 
                        VOLUME_BITMAP_GRANULARITY, Status));
    }

    if (0 == *pulBitmapGranularity)
        *pulBitmapGranularity = VOLUME_BITMAP_GRANULARITY_DEFAULT_VALUE;

    delete pVolumeParam;
    return Status;
}

PVOLUME_BITMAP
OpenBitmapFile(
    PVOLUME_CONTEXT pVolumeContext,
    NTSTATUS        &Status  
    )
{
    PVOLUME_BITMAP      pVolumeBitmap = NULL;
    PDEVICE_EXTENSION   pDeviceExtension;
    NTSTATUS            InMageOpenStatus = STATUS_SUCCESS;
    ULONG               ulBitmapGranularity;
    BOOLEAN             bVolumeInSync = TRUE,bLocFound=FALSE;
    BitmapAPI           *pBitmapAPI = NULL;
    KIRQL               OldIrql;
    NTSTATUS            CommitStatus = STATUS_SUCCESS;
    BOOLEAN             bSetStartFilteringState = FALSE;
    BitmapPersistentValues BitmapPersistentValue(pVolumeContext);
#ifdef _WIN64
    // Flag to indicate whether to allocate DATA Pool.
    BOOLEAN             bAllocMemory = FALSE;
#endif
    PAGED_CODE();

    InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, ("OpenBitmapFile: Called on volume %p\n", pVolumeContext));

    if ((NULL == pVolumeContext) || (NULL == pVolumeContext->FilterDeviceObject)) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("OpenBitmapFile: Invalid parameter %p\n", pVolumeContext));
        goto Cleanup_And_Return_Error;
    }

    if (0 == (pVolumeContext->ulFlags & VCF_GUID_OBTAINED)) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("OpenBitmapFile: VolumeContext %p does not have GUID obtained yet\n", pVolumeContext));
        InMageFltLogError(pVolumeContext->FilterDeviceObject, __LINE__, INVOLFLT_INFO_OPEN_BITMAP_CALLED_PRIOR_TO_OBTAINING_GUID, STATUS_SUCCESS);
        goto Cleanup_And_Return_Error;
    }

    pVolumeBitmap = AllocateVolumeBitmap();
    if (NULL == pVolumeBitmap) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("OpenBitmapFile: AllocteVolumeBitmap failed for %S\n", pVolumeContext->wDriveLetter));
        goto Cleanup_And_Return_Error;
    }

    pDeviceExtension = (PDEVICE_EXTENSION)pVolumeContext->FilterDeviceObject->DeviceExtension; 

    Status = GetVolumeSize( pDeviceExtension->TargetDeviceObject, 
                            &pVolumeContext->llVolumeSize,
                            &InMageOpenStatus);
    if (!NT_SUCCESS(Status)) {
        if (STATUS_SUCCESS != InMageOpenStatus) {
            InMageFltLogError(pDeviceExtension->DeviceObject,__LINE__, InMageOpenStatus, Status, 
                pVolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES, pVolumeContext->wGUID, GUID_SIZE_IN_BYTES);
        }
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN,
            ("OpenBitmapFile: Failed in GetVolumeSize Status = %#x, InMageStatus = %#x\n", Status, InMageOpenStatus));
        goto Cleanup_And_Return_Error;
    }

    InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, ("OpenBitmapFile: Size of volume %S is %#I64x\n",
                    pVolumeContext->wDriveLetter, pVolumeContext->llVolumeSize));

    pVolumeContext->BitmapFileName.Length = 0;
    Status = FillBitmapFilenameInVolumeContext(pVolumeContext);
    if (!NT_SUCCESS(Status)) {
        InVolDbgPrint( DL_ERROR, DM_BITMAP_OPEN, 
            ("OpenBitmapFile: Failed to Get bitmap file name. Status = %#x\n", Status));        
        goto Cleanup_And_Return_Error;
    }

    Status = GetVolumeBitmapGranularity(pVolumeContext, &ulBitmapGranularity);
    if (!NT_SUCCESS(Status) || (0 == ulBitmapGranularity)) {
        InVolDbgPrint( DL_ERROR, DM_BITMAP_OPEN, ("OpenBitmapFile: Failed in retreiving %S, Status = %#x\n", 
                        VOLUME_BITMAP_GRANULARITY, Status));
        goto Cleanup_And_Return_Error;
    }

    pVolumeBitmap->SegmentCacheLimit = MAX_BITMAP_SEGMENT_BUFFERS;

    pBitmapAPI = new BitmapAPI();
    InVolDbgPrint(DL_INFO, DM_MEM_TRACE, ("OpenBitmapFile: Allocation %#p\n", pBitmapAPI));
    if (pBitmapAPI) {
        PWCHAR  Loc = pVolumeContext->BitmapFileName.Buffer;
        ULONG   LenTocheck = 0;

        InMageOpenStatus = STATUS_SUCCESS;
        pVolumeBitmap->pBitmapAPI = pBitmapAPI;

        KeWaitForMutexObject(&pVolumeContext->BitmapOpenCloseMutex, Executive, KernelMode, FALSE, NULL);
        do
        {
            Status = pBitmapAPI->Open(&pVolumeContext->BitmapFileName, ulBitmapGranularity, 
                        pVolumeContext->ulBitmapOffset, 
                        pVolumeContext->llVolumeSize, pVolumeContext->wDriveLetter[0],
                        pVolumeBitmap->SegmentCacheLimit,
                        ((0 == (pVolumeContext->ulFlags & VCF_CLEAR_BITMAP_ON_OPEN)) ? false : true),
                        BitmapPersistentValue,
                        &InMageOpenStatus);
            if (STATUS_SUCCESS != Status) {
                while (!bLocFound && (LenTocheck + sizeof(WCHAR)*GUID_SIZE_IN_CHARS) <=  pVolumeContext->BitmapFileName.Length){
                    if (sizeof(WCHAR)*GUID_SIZE_IN_CHARS == RtlCompareMemory(Loc,pVolumeContext->wGUID,sizeof(WCHAR)*GUID_SIZE_IN_CHARS)){
                        bLocFound = TRUE;
                        break;
                    }
                    Loc++; 
                    LenTocheck++;
                    LenTocheck++;
                }

                if (bLocFound) {
                    if (STATUS_SUCCESS != GetNextVolumeGUID(pVolumeContext, Loc)) {
                        break;
                    }
                } else {
                    break;
                }
            } else {
                if (IS_CLUSTER_VOLUME(pVolumeContext)) {
                    bool bFilteringStopped = false;
                    KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
                    if (pVolumeContext->ulFlags & VCF_SAVE_FILTERING_STATE_TO_BITMAP) {
                        pVolumeContext->ulFlags &= ~VCF_SAVE_FILTERING_STATE_TO_BITMAP;
                    } else {
                        if (BitmapPersistentValue.GetVolumeFilteringStateFromBitmap()) {
                            pVolumeContext->ulFlags |= VCF_FILTERING_STOPPED;
                        } else {
                            pVolumeContext->ulFlags &= ~VCF_FILTERING_STOPPED;
                        }
                    }
                    if (pVolumeContext->ulFlags & VCF_FILTERING_STOPPED) {
                        StopFilteringDevice(pVolumeContext, true, false, false, NULL);
                        bFilteringStopped = true;
#ifdef _WIN64			
                    } else {
                        //Set when volume is cluster and filtering is started on any volume
                        bAllocMemory = 1;
#endif
                    }
                    KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);
                    if (bFilteringStopped) {
                        // Driver marks clean, as there are no changes on stop filtering
                        pBitmapAPI->SaveFilteringStateToBitmap(true, true, true);
                    }
					pVolumeContext->pBasicVolume->pDisk->IsAccessible = 1;
#ifdef _WIN64		    
                } else {
                    //Set when volume is non-cluster and filtering is started on any volume
                    bAllocMemory = 1;
#endif
                }
                break;
            }
        } while(TRUE);

        KeReleaseMutex(&pVolumeContext->BitmapOpenCloseMutex, FALSE);
#ifdef _WIN64
        // Data Pool get allocated when bitmap file is open first time and filtering is on.
        if (bAllocMemory) {
            SetDataPool(1, DriverContext.ullDataPoolSize);
        }
#endif
        if (STATUS_SUCCESS == Status) {
            if (pVolumeContext->lNumBitmapOpenErrors) {
                LogBitmapOpenSuccessEvent(pVolumeContext);
            }
        } else {
            // Opening bitmap file has failed.
            InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("OpenBitmapFile: bitmap file %wZ open for volume %S failed, Status(%#x), InMageStatus(%#x)\n", 
                                &pVolumeContext->BitmapFileName, pVolumeContext->wDriveLetter, Status, InMageOpenStatus));
            if (STATUS_SUCCESS != InMageOpenStatus) {
                InMageFltLogError(pDeviceExtension->DeviceObject,__LINE__, InMageOpenStatus, Status,
                    pVolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES, pVolumeContext->wGUID, GUID_SIZE_IN_BYTES);
            }
            goto Cleanup_And_Return_Error;
        }

        Status = pBitmapAPI->IsVolumeInSync(&bVolumeInSync, &InMageOpenStatus);
        if (Status != STATUS_SUCCESS) {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("OpenBitmapFile: bitmap file %wZ open for volume %S failed to get sync status, Status(%#x)\n", 
                                &pVolumeContext->BitmapFileName, pVolumeContext->wDriveLetter, Status));
            goto Cleanup_And_Return_Error;
        }

        if (FALSE == bVolumeInSync) {
            SetVolumeOutOfSync(pVolumeContext, InMageOpenStatus, InMageOpenStatus, false);
            // Cleanup data files from previous boot.
            CDataFile::DeleteDataFilesInDirectory(&pVolumeContext->DataLogDirectory);
        } else if (IS_CLUSTER_VOLUME(pVolumeContext) && (pVolumeContext->ulFlags & VCF_CLUSTER_VOLUME_ONLINE)) {
            ULONG     ulOutOfSyncErrorCode =0;
            ULONG     ulOutOfSyncErrorStatus =0;
            ULONGLONG ullOutOfSyncTimeStamp =0;
            ULONG     ulOutOfSyncCount =0;
            ULONG     ulOutOfSyncIndicatedAtCount =0;
            ULONGLONG ullOutOfSyncIndicatedTimeStamp =0;
            ULONGLONG ullOutOfSyncResetTimeStamp =0;

            KeWaitForSingleObject(&pVolumeContext->Mutex, Executive, KernelMode, FALSE, NULL);
            if (BitmapPersistentValue.GetResynRequiredValues(ulOutOfSyncErrorCode, 
                ulOutOfSyncErrorStatus, 
                ullOutOfSyncTimeStamp, 
                ulOutOfSyncCount,
                ulOutOfSyncIndicatedAtCount,
                ullOutOfSyncIndicatedTimeStamp,
                ullOutOfSyncResetTimeStamp)) {
                pVolumeContext->bResyncRequired = true;
                InVolDbgPrint(DL_WARNING, DM_BITMAP_OPEN, ("OpenBitmapFile: Bitmap file %wZ volume %S Resync is TRUE)\n", 
                                &pVolumeContext->BitmapFileName, pVolumeContext->wDriveLetter));
            } else {
                pVolumeContext->bResyncRequired = false;
                pVolumeContext->liLastOutOfSyncTimeStamp.QuadPart = ullOutOfSyncTimeStamp;
                InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, ("OpenBitmapFile: Bitmap file %wZ volume %S Resync is FALSE)\n", 
                                &pVolumeContext->BitmapFileName, pVolumeContext->wDriveLetter));
            }

            pVolumeContext->ulOutOfSyncCount = ulOutOfSyncCount;
            pVolumeContext->liOutOfSyncTimeStamp.QuadPart = ullOutOfSyncTimeStamp;
            pVolumeContext->ulOutOfSyncErrorStatus = ulOutOfSyncErrorStatus;
            pVolumeContext->ulOutOfSyncErrorCode = ulOutOfSyncErrorCode;
            pVolumeContext->ulOutOfSyncIndicatedAtCount = ulOutOfSyncIndicatedAtCount;
            pVolumeContext->liOutOfSyncIndicatedTimeStamp.QuadPart = ullOutOfSyncIndicatedTimeStamp;
            pVolumeContext->liOutOfSyncResetTimeStamp.QuadPart = ullOutOfSyncResetTimeStamp;

            KeReleaseMutex(&pVolumeContext->Mutex, FALSE);
            InVolDbgPrint( DL_INFO, DM_CLUSTER, ("Updated the volume context with resync information read from Bitmap\n"));
        }

        //update the Globals with the values read.
        UpdateGlobalWithPersistentValuesReadFromBitmap(bVolumeInSync, BitmapPersistentValue);

        //update the VolumeContext with the values read from Bitmap.
        UpdateVolumeContextWithValuesReadFromBitmap(bVolumeInSync, BitmapPersistentValue);

        // Unset ignore bitmap creation .
        pVolumeContext->ulFlags &= ~VCF_IGNORE_BITMAP_CREATION;
        pVolumeContext->ulFlags &= ~VCF_CLEAR_BITMAP_ON_OPEN;

        if (pVolumeContext->ulFlags & VCF_VOLUME_ON_CLUS_DISK) {
        // Try to open volume object.
            InVolDbgPrint( DL_INFO, DM_CLUSTER, 
                    ("Open Bitmap file called on cluster volume %p\n", pVolumeContext));

            if (NULL == pVolumeContext->PnPNotificationEntry) {
                if (0 != pVolumeContext->UniqueVolumeName.Length) {
                    Status = IoGetDeviceObjectPointer(&pVolumeContext->UniqueVolumeName, FILE_READ_DATA, 
                                        &pVolumeContext->LogVolumeFileObject, &pVolumeContext->LogVolumeDeviceObject);
                    if (!NT_SUCCESS(Status)) {
                        Status = IoGetDeviceObjectPointer(&pVolumeContext->UniqueVolumeName, FILE_READ_ATTRIBUTES, 
                                        &pVolumeContext->LogVolumeFileObject, &pVolumeContext->LogVolumeDeviceObject);
                    }

                    if (NT_SUCCESS(Status)) {
                        Status = IoRegisterPlugPlayNotification(
                                        EventCategoryTargetDeviceChange, 0, pVolumeContext->LogVolumeFileObject,
                                        DriverContext.DriverObject, DriverNotificationCallback, pVolumeContext,
                                        &pVolumeContext->PnPNotificationEntry);
                        ASSERT(STATUS_SUCCESS == Status);
                        if (STATUS_SUCCESS == Status) {
                            InVolDbgPrint( DL_INFO, DM_CLUSTER, 
                                    ("PlugPlayNotification Registered for FileSystem on volume %p\n", pVolumeContext));
                        } else {
                            InVolDbgPrint( DL_ERROR, DM_CLUSTER, ("Failed to RegisterPlugPlayNotification for FileSystem Unmount\n"));
                        }
                    } else {
                        InVolDbgPrint( DL_ERROR, DM_CLUSTER, ("Failed to open log volume for PnP Notification\n"));
                    }
                } else {
                    InVolDbgPrint( DL_ERROR, DM_CLUSTER, ("UniqueVolumeName is Empty. Failed to set PnP Notification on Volume\n"));
                }
                Status = STATUS_SUCCESS;
            } else {
                InVolDbgPrint(DL_INFO, DM_CLUSTER, ("PnP Notification exists on this volume %p\n", pVolumeContext));
            }
        }
    } else {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("OpenBitmapFile: Failed to allocate BitmapAPI\n"));
    }

    pVolumeBitmap->eVBitmapState = ecVBitmapStateOpened;
    pVolumeBitmap->pVolumeContext = ReferenceVolumeContext(pVolumeContext);
  
	RtlCopyMemory(pVolumeBitmap->VolumeGUID, pVolumeContext->wGUID, GUID_SIZE_IN_BYTES + sizeof(WCHAR));
    if (pVolumeContext->ulFlags & VCF_VOLUME_LETTER_OBTAINED) {
  		RtlCopyMemory(pVolumeBitmap->VolumeLetter, pVolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES + sizeof(WCHAR));
        pVolumeBitmap->ulFlags |= VOLUME_BITMAP_FLAGS_HAS_VOLUME_LETTER;
    }

    return pVolumeBitmap;

Cleanup_And_Return_Error:

    if (NULL != pVolumeBitmap) {
        DereferenceVolumeBitmap(pVolumeBitmap);
        pVolumeBitmap = NULL;
    }

    return pVolumeBitmap;
}

VOID
CloseBitmapFile(PVOLUME_BITMAP pVolumeBitmap, bool bClearBitmap, BitmapPersistentValues &BitmapPersistentValue, bool bDeleteBitmap)
{
    BOOLEAN         bWaitForNotification = FALSE;
    BOOLEAN         bSetBitsWorkItemListIsEmpty;
    NTSTATUS        Status;
    KIRQL           OldIrql;
    class BitmapAPI *pBitmapAPI = NULL;
    
    PAGED_CODE();

    InVolDbgPrint(DL_INFO, DM_BITMAP_CLOSE,
        ("CloseBitmapFile: Called for volume %S(%S)\n", pVolumeBitmap->VolumeGUID, pVolumeBitmap->VolumeLetter));

    if (NULL != pVolumeBitmap) {
        ASSERT(FALSE == bWaitForNotification);
        do {
            if (bWaitForNotification) {
                InVolDbgPrint(DL_INFO, DM_BITMAP_CLOSE,
                    ("CloseBitmapFile: Waiting on SetBitsWorkItemListEmtpyNotification for volume %S(%S)\n", 
                    pVolumeBitmap->VolumeGUID, pVolumeBitmap->VolumeLetter));
                Status = KeWaitForSingleObject(&pVolumeBitmap->SetBitsWorkItemListEmtpyNotification, Executive, KernelMode, FALSE, NULL);
                bWaitForNotification = FALSE;
            }

            ExAcquireFastMutex(&pVolumeBitmap->Mutex);
            
            KeAcquireSpinLock(&pVolumeBitmap->Lock, &OldIrql);
            bSetBitsWorkItemListIsEmpty = IsListEmpty(&pVolumeBitmap->SetBitsWorkItemList);
            KeReleaseSpinLock(&pVolumeBitmap->Lock, OldIrql);
            
            if (bSetBitsWorkItemListIsEmpty) {
                // Set the state of bitmap to close.
                InVolDbgPrint(DL_INFO, DM_BITMAP_CLOSE,
                    ("CloseBitmapFile: Setting bitmap state to closed for volume %S(%S)\n", pVolumeBitmap->VolumeGUID, pVolumeBitmap->VolumeLetter));
                pVolumeBitmap->eVBitmapState = ecVBitmapStateClosed;

                if (NULL != pVolumeBitmap->pVolumeContext) {
                    DereferenceVolumeContext(pVolumeBitmap->pVolumeContext);
                    pVolumeBitmap->pVolumeContext = NULL;
                }

                if (NULL != pVolumeBitmap->pBitmapAPI) {
                    if (bClearBitmap) {
                        pVolumeBitmap->pBitmapAPI->ClearAllBits();
                    }
                    pBitmapAPI = pVolumeBitmap->pBitmapAPI;

                    pVolumeBitmap->pBitmapAPI = NULL;
                }
            } else {
                InVolDbgPrint(DL_INFO, DM_BITMAP_CLOSE,
                    ("CloseBitmapFile: SetBitsWorkItemList is not Emtpy for volume %S(%S)\n", pVolumeBitmap->VolumeGUID, pVolumeBitmap->VolumeLetter));
                KeClearEvent(&pVolumeBitmap->SetBitsWorkItemListEmtpyNotification);
                pVolumeBitmap->ulFlags |= VOLUME_BITMAP_FLAGS_WAITING_FOR_SETBITS_WORKITEM_LIST_EMPTY_NOTIFICATION;
                bWaitForNotification = TRUE;
            }
            ExReleaseFastMutex(&pVolumeBitmap->Mutex);
            if (pBitmapAPI) {
                pBitmapAPI->Close(BitmapPersistentValue, bDeleteBitmap, NULL);
                delete pBitmapAPI;
                pBitmapAPI = NULL;
            }
        } while (bWaitForNotification);

        
        DereferenceVolumeBitmap(pVolumeBitmap);
    }

    return;
}

VOID
ClearBitmapFile(PVOLUME_BITMAP pVolumeBitmap)
{
    BOOLEAN         bWaitForNotification = FALSE;
    BOOLEAN         bSetBitsWorkItemListIsEmpty;
    NTSTATUS        Status;
    KIRQL           OldIrql;

    PAGED_CODE();

    InVolDbgPrint(DL_INFO, DM_BITMAP_WRITE,
        ("ClearBitmapFile: Called for volume %S(%S)\n", pVolumeBitmap->VolumeGUID, pVolumeBitmap->VolumeLetter));

    if ((NULL != pVolumeBitmap) && (NULL != pVolumeBitmap->pBitmapAPI)) {
        do {
            if (bWaitForNotification) {
                InVolDbgPrint(DL_INFO, DM_BITMAP_WRITE,
                    ("ClearBitmapFile: Waiting on SetBitsWorkItemListEmtpyNotification for volume %S(%S)\n", 
                    pVolumeBitmap->VolumeGUID, pVolumeBitmap->VolumeLetter));
                Status = KeWaitForSingleObject(&pVolumeBitmap->SetBitsWorkItemListEmtpyNotification, Executive, KernelMode, FALSE, NULL);
                bWaitForNotification = FALSE;
            }

            ExAcquireFastMutex(&pVolumeBitmap->Mutex);
            
            KeAcquireSpinLock(&pVolumeBitmap->Lock, &OldIrql);
            bSetBitsWorkItemListIsEmpty = IsListEmpty(&pVolumeBitmap->SetBitsWorkItemList);
            KeReleaseSpinLock(&pVolumeBitmap->Lock, OldIrql);
            
            if (bSetBitsWorkItemListIsEmpty) {
                if (NULL != pVolumeBitmap->pBitmapAPI) {
                    pVolumeBitmap->pBitmapAPI->ClearAllBits();
                }

                // Set the state of bitmap to readcompleted.
                InVolDbgPrint(DL_INFO, DM_BITMAP_WRITE,
                    ("ClearBitmapFile: Setting bitmap state to ecVBitmapStateReadCompleted for volume %S(%S)\n", 
                     pVolumeBitmap->VolumeGUID, pVolumeBitmap->VolumeLetter));
                pVolumeBitmap->eVBitmapState = ecVBitmapStateReadCompleted;

            } else {
                InVolDbgPrint(DL_INFO, DM_BITMAP_WRITE,
                    ("ClearBitmapFile: SetBitsWorkItemList is not Emtpy for volume %S(%S)\n", pVolumeBitmap->VolumeGUID, pVolumeBitmap->VolumeLetter));
                KeClearEvent(&pVolumeBitmap->SetBitsWorkItemListEmtpyNotification);
                pVolumeBitmap->ulFlags |= VOLUME_BITMAP_FLAGS_WAITING_FOR_SETBITS_WORKITEM_LIST_EMPTY_NOTIFICATION;
                bWaitForNotification = TRUE;
            }
            ExReleaseFastMutex(&pVolumeBitmap->Mutex);
        } while (bWaitForNotification);        
    }

    return;
}

NTSTATUS
WaitForAllWritesToComplete(PVOLUME_BITMAP pVolumeBitmap, PLARGE_INTEGER pliTimeOut)
{
    BOOLEAN         bWaitForNotification = FALSE;
    BOOLEAN         bSetBitsWorkItemListIsEmpty;
    NTSTATUS        Status = STATUS_SUCCESS;
    KIRQL           OldIrql;

    PAGED_CODE();

    InVolDbgPrint(DL_INFO, DM_BITMAP_WRITE,
        ("WaitForAllWritesToComplete: Called for volume %S(%S)\n", pVolumeBitmap->VolumeGUID, pVolumeBitmap->VolumeLetter));

    if (NULL != pVolumeBitmap) {
        ASSERT(FALSE == bWaitForNotification);

        ExAcquireFastMutex(&pVolumeBitmap->Mutex);
        
        KeAcquireSpinLock(&pVolumeBitmap->Lock, &OldIrql);
        bSetBitsWorkItemListIsEmpty = IsListEmpty(&pVolumeBitmap->SetBitsWorkItemList);
        KeReleaseSpinLock(&pVolumeBitmap->Lock, OldIrql);
        
        if (FALSE == bSetBitsWorkItemListIsEmpty) {
            InVolDbgPrint(DL_INFO, DM_BITMAP_CLOSE,
                ("WaitForAllWritesToComplete: SetBitsWorkItemList is not Emtpy for volume %S(%S)\n", 
                pVolumeBitmap->VolumeGUID, pVolumeBitmap->VolumeLetter));
            KeClearEvent(&pVolumeBitmap->SetBitsWorkItemListEmtpyNotification);
            pVolumeBitmap->ulFlags |= VOLUME_BITMAP_FLAGS_WAITING_FOR_SETBITS_WORKITEM_LIST_EMPTY_NOTIFICATION;
            bWaitForNotification = TRUE;
        }
        ExReleaseFastMutex(&pVolumeBitmap->Mutex);

        if (bWaitForNotification) {
            InVolDbgPrint(DL_INFO, DM_BITMAP_WRITE,
                ("WaitForAllWritesToComplete: Waiting on SetBitsWorkItemListEmtpyNotification for volume %S(%S)\n", 
                pVolumeBitmap->VolumeGUID, pVolumeBitmap->VolumeLetter));
            Status = KeWaitForSingleObject(&pVolumeBitmap->SetBitsWorkItemListEmtpyNotification, Executive, KernelMode, FALSE, pliTimeOut);
        }
    }

    return Status;
}

/*---------------------------------------------------------------------------------------------
 *  Functions related to Allocate, Deallocate, Reference and Dereference Bitmap Work Items
 *---------------------------------------------------------------------------------------------
 */
VOID
DeallocateBitmapWorkItem(
    PBITMAP_WORK_ITEM   pBitmapWorkItem
    )
{
    LONG lSize = (LONG)sizeof(BITMAP_WORK_ITEM);
    LONG lBitRunSize = (LONG) sizeof(BitRunLengthOffsetArray);
    ASSERT(0 == pBitmapWorkItem->lRefCount);
    
    if (NULL != pBitmapWorkItem->pVolumeBitmap) {
        DereferenceVolumeBitmap(pBitmapWorkItem->pVolumeBitmap);
        pBitmapWorkItem->pVolumeBitmap = NULL;
    }

    if (NULL != pBitmapWorkItem->BitRuns.pBitRunLengthOffsetArray) {
        ExFreeToNPagedLookasideList(&DriverContext.BitRunLengthOffsetPool, pBitmapWorkItem->BitRuns.pBitRunLengthOffsetArray);
        InterlockedDecrement(&DriverContext.lBitRunLengthOffsetItemsAllocated);
        InterlockedExchangeAdd(&DriverContext.lNonPagedMemoryAllocated, -lBitRunSize);
        pBitmapWorkItem->BitRuns.pBitRunLengthOffsetArray = NULL;
        pBitmapWorkItem->BitRuns.runOffsetArray = NULL;
        pBitmapWorkItem->BitRuns.runLengthArray = NULL;        
    }    

    ExFreeToNPagedLookasideList(&DriverContext.BitmapWorkItemPool, pBitmapWorkItem);
    InterlockedDecrement(&DriverContext.lBitmapWorkItemsAllocated);
    InterlockedExchangeAdd(&DriverContext.lNonPagedMemoryAllocated, -lSize);
    ASSERT(DriverContext.lNonPagedMemoryAllocated >= 0);

    return;
}

PBITMAP_WORK_ITEM
AllocateBitmapWorkItem(etBitmapWorkItem wItemType)
{
    PBITMAP_WORK_ITEM   pBitmapWorkItem;

    if (MaxNonPagedPoolLimitReached()) {
        InVolDbgPrint(DL_WARNING, DM_DATA_FILTERING, 
                      ("%s: Allocation failed as NonPagedPoolAlloc(%#x) reached MaxNonPagedLimit(%#x)\n", 
                       __FUNCTION__, DriverContext.lNonPagedMemoryAllocated, DriverContext.ulMaxNonPagedPool));
        return NULL;
    }

    pBitmapWorkItem = (PBITMAP_WORK_ITEM)ExAllocateFromNPagedLookasideList(&DriverContext.BitmapWorkItemPool);

    if(pBitmapWorkItem) {
    	RtlZeroMemory(pBitmapWorkItem, sizeof(BITMAP_WORK_ITEM));
        pBitmapWorkItem->pWorkQueueEntryPreAlloc = AllocateWorkQueueEntry();
        if (NULL == pBitmapWorkItem->pWorkQueueEntryPreAlloc) {
            ExFreeToNPagedLookasideList(&DriverContext.BitmapWorkItemPool, pBitmapWorkItem);
            pBitmapWorkItem = NULL;
            return pBitmapWorkItem;
        }
        pBitmapWorkItem->BitRuns.pBitRunLengthOffsetArray = NULL;
        pBitmapWorkItem->BitRuns.runOffsetArray = NULL;
        pBitmapWorkItem->BitRuns.runLengthArray = NULL;
        if (ecBitmapWorkItemSetBits != wItemType) {
            // Allocate Non Paged Pool memory for BitRuns->runOffsetArray and BitRuns->runLengthArray, from lookaside list
            pBitmapWorkItem->BitRuns.pBitRunLengthOffsetArray = (BitRunLengthOffsetArray *) ExAllocateFromNPagedLookasideList(&DriverContext.BitRunLengthOffsetPool);
            if (pBitmapWorkItem->BitRuns.pBitRunLengthOffsetArray != NULL) {
                InterlockedIncrement(&DriverContext.lBitRunLengthOffsetItemsAllocated);
                InterlockedExchangeAdd(&DriverContext.lNonPagedMemoryAllocated, sizeof(BitRunLengthOffsetArray));
                pBitmapWorkItem->BitRuns.runOffsetArray = pBitmapWorkItem->BitRuns.pBitRunLengthOffsetArray->runOffsetArray;
                pBitmapWorkItem->BitRuns.runLengthArray = pBitmapWorkItem->BitRuns.pBitRunLengthOffsetArray->runLengthArray;
            } else {
                DereferenceWorkQueueEntry(pBitmapWorkItem->pWorkQueueEntryPreAlloc);
                ExFreeToNPagedLookasideList(&DriverContext.BitmapWorkItemPool, pBitmapWorkItem);
                pBitmapWorkItem = NULL;
                return pBitmapWorkItem;
            }
        }
        InterlockedIncrement(&DriverContext.lBitmapWorkItemsAllocated);
        InterlockedExchangeAdd(&DriverContext.lNonPagedMemoryAllocated, sizeof(BITMAP_WORK_ITEM));
        pBitmapWorkItem->lRefCount = 1;
        pBitmapWorkItem->ListEntry.Flink = NULL;
        pBitmapWorkItem->ListEntry.Blink = NULL;
        pBitmapWorkItem->pVolumeBitmap = NULL;
        pBitmapWorkItem->eBitmapWorkItem = ecBitmapWorkItemNotInitialized;
    }

    return pBitmapWorkItem;
}

VOID
ReferenceBitmapWorkItem(PBITMAP_WORK_ITEM pBitmapWorkItem)
{
    ASSERT(pBitmapWorkItem->lRefCount >= 1);

    InterlockedIncrement(&pBitmapWorkItem->lRefCount);

    return;
}

VOID
DereferenceBitmapWorkItem(PBITMAP_WORK_ITEM pBitmapWorkItem)
{
    ASSERT(pBitmapWorkItem->lRefCount >= 1);

    if (0 == InterlockedDecrement(&pBitmapWorkItem->lRefCount))
    {
    	//Free WorkQueueEntry
    	ASSERT(0 == pBitmapWorkItem->bBitMapWorkQueueRef);
        DereferenceWorkQueueEntry(pBitmapWorkItem->pWorkQueueEntryPreAlloc);    
        DeallocateBitmapWorkItem(pBitmapWorkItem);
    }

    return;
}


/*--------------------------------------------------------------------------------------------
 * VOLUME_BITMAP Allocation and Deallocation Routines
 *--------------------------------------------------------------------------------------------
 */

PVOLUME_BITMAP
AllocateVolumeBitmap()
{
    PVOLUME_BITMAP  pVolumeBitmap;
    KIRQL           OldIrql;

    pVolumeBitmap = (PVOLUME_BITMAP)ExAllocatePoolWithTag(NonPagedPool, sizeof(VOLUME_BITMAP), TAG_VOLUME_BITMAP);
    if (NULL != pVolumeBitmap) {
        RtlZeroMemory(pVolumeBitmap, sizeof(VOLUME_BITMAP));
        pVolumeBitmap->lRefCount = 1;
        pVolumeBitmap->eVBitmapState = ecVBitmapStateUnInitialized;
        InitializeListHead(&pVolumeBitmap->WorkItemList);
        InitializeListHead(&pVolumeBitmap->SetBitsWorkItemList);
        KeInitializeEvent(&pVolumeBitmap->SetBitsWorkItemListEmtpyNotification, NotificationEvent, FALSE);
        ExInitializeFastMutex(&pVolumeBitmap->Mutex);
        KeInitializeSpinLock(&pVolumeBitmap->Lock);

        KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
        InsertHeadList(&DriverContext.HeadForVolumeBitmaps, &pVolumeBitmap->ListEntry);
        DriverContext.ulNumVolumeBitmaps += 1;
        KeReleaseSpinLock(&DriverContext.Lock, OldIrql);
    }

    return pVolumeBitmap;
}

VOID
DeallocateVolumeBitmap(PVOLUME_BITMAP pVolumeBitmap)
{
    KIRQL   OldIrql;

    ASSERT(NULL != pVolumeBitmap);
    ASSERT(0 == pVolumeBitmap->lRefCount);
    ASSERT(NULL == pVolumeBitmap->pVolumeContext);
    ASSERT(IsListEmpty(&pVolumeBitmap->WorkItemList));
    ASSERT(IsListEmpty(&pVolumeBitmap->SetBitsWorkItemList));
    ASSERT(0 < DriverContext.ulNumVolumeBitmaps);

    if (NULL != pVolumeBitmap->pBitmapAPI)
        pVolumeBitmap->pBitmapAPI->Release();

    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    RemoveEntryList(&pVolumeBitmap->ListEntry);
    DriverContext.ulNumVolumeBitmaps -= 1;
    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

    ExFreePoolWithTag(pVolumeBitmap, TAG_VOLUME_BITMAP);
    return;
}

PVOLUME_BITMAP
ReferenceVolumeBitmap(PVOLUME_BITMAP    pVolumeBitmap)
{
    ASSERT(0 < pVolumeBitmap->lRefCount);

    InterlockedIncrement(&pVolumeBitmap->lRefCount);

    return pVolumeBitmap;
}

VOID
DereferenceVolumeBitmap(PVOLUME_BITMAP  pVolumeBitmap)
{
    ASSERT(0 < pVolumeBitmap->lRefCount);

    if (InterlockedDecrement(&pVolumeBitmap->lRefCount) <= 0) {
        DeallocateVolumeBitmap(pVolumeBitmap);
    }

    return;
}

NTSTATUS
UpdateGlobalWithPersistentValuesReadFromBitmap(BOOLEAN bVolumeInSync, BitmapPersistentValues &BitmapPersistentValue) 
{
    PVOLUME_CONTEXT pVolumeContext = BitmapPersistentValue.m_pVolumeContext;

    if (IS_CLUSTER_VOLUME(pVolumeContext) && pVolumeContext->bQueueChangesToTempQueue) {
        KIRQL   OldIrql;
        ULONGLONG TimeStamp = BitmapPersistentValue.GetGlobalTimeStampFromBitmap();
        ULONGLONG SequenceNumber = BitmapPersistentValue.GetGlobalSequenceNumberFromBitmap();

        InVolDbgPrint(DL_ERROR, DM_TIME_STAMP, ("%s: Time Stamp Read from Bitmap=%#I64x \n", __FUNCTION__,TimeStamp));
        InVolDbgPrint(DL_ERROR, DM_TIME_STAMP, ("%s: Seq Number Read from Bitmap=%#I64x \n", __FUNCTION__,SequenceNumber));

        if (!bVolumeInSync) { //system crashed or improper shutdown
            TimeStamp += TIME_STAMP_BUMP;
            SequenceNumber += SEQ_NO_BUMP;
        }

        InVolDbgPrint(DL_ERROR, DM_TIME_STAMP, ("%s: Time Stamp After Bump=%#I64x \n", __FUNCTION__,TimeStamp));
        InVolDbgPrint(DL_ERROR, DM_TIME_STAMP, ("%s: Seq Number After Bump=%#I64x \n", __FUNCTION__,SequenceNumber));

        KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
        KeAcquireSpinLockAtDpcLevel(&DriverContext.TimeStampLock);

        InVolDbgPrint(DL_ERROR, DM_TIME_STAMP, ("%s: Global Time Stamp Before Update=%#I64x \n", __FUNCTION__,DriverContext.ullLastTimeStamp));

        //Update the Global Time Stamp
        if (TimeStamp > DriverContext.ullLastTimeStamp) {
            DriverContext.ullLastTimeStamp = TimeStamp;
        } else if(TimeStamp == DriverContext.ullLastTimeStamp) {
            //As the time stamp is same increment the global time stamp by 1
            //It is done to resolve in the case of failover in the 32 bit cluster system 
            //where the time stamp is same and current sequence number running on this node
            //might be less than the sequence number running on the failing node.
            ++DriverContext.ullLastTimeStamp;
        }
        InVolDbgPrint(DL_ERROR, DM_TIME_STAMP, ("%s: Global Time Stamp After Update=%#I64x \n", __FUNCTION__,DriverContext.ullLastTimeStamp));

        InVolDbgPrint(DL_ERROR, DM_TIME_STAMP, ("%s: Global Seq Number before Update=%#I64x \n", __FUNCTION__,DriverContext.ullTimeStampSequenceNumber));
        //Update the Global Sequence Number
        if (SequenceNumber > DriverContext.ullTimeStampSequenceNumber) {
           DriverContext.ullTimeStampSequenceNumber = SequenceNumber;
        }
        InVolDbgPrint(DL_ERROR, DM_TIME_STAMP, ("%s: Global Seq Number After Update=%#I64x \n", __FUNCTION__,DriverContext.ullTimeStampSequenceNumber));

        KeReleaseSpinLockFromDpcLevel(&DriverContext.TimeStampLock);
        KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);

    }

    return STATUS_SUCCESS;
}

