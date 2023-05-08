#include "Common.h"
#include "global.h"
#include "VContext.h"
#include "TagNode.h"
#ifdef VOLUME_FLT
#include "MntMgr.h"
#endif
#include "misc.h"
#include "VBitmap.h"
#include "DirtyBlock.h"

void
RequestServiceThreadToOpenBitmap(PDEV_CONTEXT  pDevContext)
{
    KIRQL   OldIrql;
    BOOLEAN bWakeServiceThread = FALSE;

    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
    if ((NULL == pDevContext->pDevBitmap) &&
            0 == (pDevContext->ulFlags & DCF_OPEN_BITMAP_REQUESTED)) {
        pDevContext->ulFlags |= DCF_OPEN_BITMAP_REQUESTED;
        // Wake up the Device monitor thread.
        // Requesting to open bitmap after lock is released.
        bWakeServiceThread = TRUE;
    }
    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);

    if (bWakeServiceThread)
        WakeServiceThread(FALSE /*DriverContext.Lock is not acquired */);

    return;
}

#if DBG
const char *
GetDevBitmapStateString(etVBitmapState eVBitmapState)
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
#endif
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
    PDEV_BITMAP  pDevBitmap;
    PDEV_CONTEXT pDevContext;
    KIRQL           OldIrql;

    PAGED_CODE();
    ASSERT((NULL != pBitmapWorkItem) && (NULL != pBitmapWorkItem->pDevBitmap));
    if ((NULL == pBitmapWorkItem) || (NULL == pBitmapWorkItem->pDevBitmap)) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE, 
            ("WriteBitMapCompletion: pBitmapWorkItem = %#p, pDevBitmap = %#p\n",
            pBitmapWorkItem, pBitmapWorkItem ? pBitmapWorkItem->pDevBitmap : NULL));
        return;
    }

    pDevBitmap = pBitmapWorkItem->pDevBitmap;

    ExAcquireFastMutex(&pDevBitmap->Mutex);
    
    pDevContext = pDevBitmap->pDevContext;
    if (ecBitmapWorkItemSetBits == pBitmapWorkItem->eBitmapWorkItem) {
        if (STATUS_SUCCESS != pBitmapWorkItem->BitRuns.finalStatus) {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE,
                ("WriteBitMapCompletion: SetBits work item failed with status %#x for device %S(%S)\n",
                    pBitmapWorkItem->BitRuns.finalStatus, pDevBitmap->DevID, pDevBitmap->wDevNum));

            if ((ecVBitmapStateClosed != pDevBitmap->eVBitmapState) &&
                (NULL != pDevContext))
            {
                InterlockedIncrement(&pDevContext->lNumBitMapWriteErrors);
                SetDevOutOfSync(pDevContext, ERROR_TO_REG_BITMAP_WRITE_ERROR, pBitmapWorkItem->BitRuns.finalStatus, false);
                InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE,
                    ("WriteBitMapCompletion: Setting device %S(%S) out of sync due to write error\n",
                        pDevBitmap->DevID, pDevBitmap->wDevNum));
            }
        } else {
            if ((ecVBitmapStateClosed != pDevBitmap->eVBitmapState) &&
                (NULL != pDevContext))
            {
                KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
                pDevContext->ulChangesQueuedForWriting -= pBitmapWorkItem->ulChanges;
                pDevContext->uliChangesWrittenToBitMap.QuadPart += pBitmapWorkItem->ulChanges;
                pDevContext->ulicbChangesQueuedForWriting.QuadPart -= pBitmapWorkItem->ullCbChangeData;
                pDevContext->ulicbChangesWrittenToBitMap.QuadPart += pBitmapWorkItem->ullCbChangeData;
                KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
            }
            if ((ecVBitmapStateClosed == pDevBitmap->eVBitmapState) &&
                (NULL != pDevContext)) {
                InDskFltWriteEventWithDevCtxt(INDSKFLT_WARN_FILE_CLOSE_ON_BITMAP_WRITE_COMPLETION,
                                              pDevContext);
            }
        }
    } else {
        ASSERT(ecBitmapWorkItemClearBits == pBitmapWorkItem->eBitmapWorkItem);

        if ((NULL != pDevContext) &&
#ifdef VOLUME_CLUSTER_SUPPORT
            (0 == (pDevContext->ulFlags & DCF_CV_FS_UNMOUTNED)) &&
#endif
            (STATUS_SUCCESS != pBitmapWorkItem->BitRuns.finalStatus)) 
        {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE,
                ("WriteBitMapCompletion: ClearBits work item failed with status %#x for device %S(%S)\n",
                    pBitmapWorkItem->BitRuns.finalStatus, pDevBitmap->DevID, pDevBitmap->wDevNum));
            InterlockedIncrement(&pDevContext->lNumBitMapClearErrors);
        }
    }

    // For all the cases we have to remove this entry from list.
    if (ecBitmapWorkItemSetBits == pBitmapWorkItem->eBitmapWorkItem) {
        KeAcquireSpinLock(&pDevBitmap->Lock, &OldIrql);
        ASSERT(TRUE == IsEntryInList(&pDevBitmap->SetBitsWorkItemList, &pBitmapWorkItem->ListEntry));

        RemoveEntryList(&pBitmapWorkItem->ListEntry);
        if (pDevBitmap->ulFlags & DEV_BITMAP_FLAGS_WAITING_FOR_SETBITS_WORKITEM_LIST_EMPTY_NOTIFICATION) {
            if (IsListEmpty(&pDevBitmap->SetBitsWorkItemList)) {
                pDevBitmap->ulFlags &= ~DEV_BITMAP_FLAGS_WAITING_FOR_SETBITS_WORKITEM_LIST_EMPTY_NOTIFICATION;
                KeSetEvent(&pDevBitmap->SetBitsWorkItemListEmtpyNotification, IO_NO_INCREMENT, FALSE);
            }
        }

        KeReleaseSpinLock(&pDevBitmap->Lock, OldIrql);
    } else {
        ASSERT(ecBitmapWorkItemClearBits == pBitmapWorkItem->eBitmapWorkItem);
        ASSERT(TRUE == IsEntryInList(&pDevBitmap->WorkItemList, &pBitmapWorkItem->ListEntry));
        RemoveEntryList(&pBitmapWorkItem->ListEntry);
    }

    ExReleaseFastMutex(&pDevBitmap->Mutex);

    DereferenceBitmapWorkItem(pBitmapWorkItem);
    return;
}

/*-----------------------------------------------------------------------------
 * Function Name : QueueWorkerRoutineForBitmapWrite
 * Parameters :
 *      pDevContext : This parameter is referenced and stored in the 
 *                            work item.
 *      ulNumDirtyChanges   : if this value is zero all the dirty changes are 
 *                            copied. If this value is non zero, minimum of 
 *                            ulNumDirtyChanges are copied.
 * Retruns :
 *      BOOLEAN             : TRUE if succeded in queing work item for write
 *                            FALSE if failed in queing work item
 * NOTES :
 * This function is called at DISPATCH_LEVEL. This function is called with 
 * Spinlock to DevContext held.
 *
 *      This function removes changes from FLtDev context and queues set bits
 * work item for each dirty block. These work items are later removed and
 * processed by Work queue
 *      
 *
 *--------------------------------------------------------------------------------------------
 */
BOOLEAN
QueueWorkerRoutineForBitmapWrite(
    PDEV_CONTEXT        pDevContext,
    ULONG               ulNumDirtyChanges
    )
{
    PWORK_QUEUE_ENTRY   pWorkItem = NULL;
    PBITMAP_WORK_ITEM   pBitmapWorkItem = NULL;
    PKDIRTY_BLOCK       pDirtyBlock = NULL;
    ULONG               ulChangesCopied = 0;
    KIRQL               OldIrql;

    if (NULL == pDevContext->pDevBitmap) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE,
            ("QueueWorkerRoutineForBitmapWrite: Called when pDevBitmap is NULL\n"));
        SetBitmapOpenFailDueToLossOfChanges(pDevContext, true);
        return FALSE;
    }

    // Removing (0 != pDevContext->ulTotalChangesPending) condition as
    // we want to flush last tags which do not have changes.
    while (!IsListEmpty(&pDevContext->ChangeList.Head)) {
        // There are changes to be copied, so lets create Dirty Block work items.
        if (ulNumDirtyChanges && (ulChangesCopied >= ulNumDirtyChanges))
            break; 
                // Nothing is drained from memory and writing to bitmap
#ifdef VOLUME_CLUSTER_SUPPORT
            if ((IS_NOT_CLUSTER_VOLUME(pDevContext)) && (0 == pDevContext->EndTimeStampofDB))
#else
            if (0 == pDevContext->EndTimeStampofDB)
#endif
            {
                PCHANGE_NODE pOldestChangeNode = (PCHANGE_NODE)pDevContext->ChangeList.Head.Flink;
                PKDIRTY_BLOCK pOldestDB = pOldestChangeNode->DirtyBlock;

                if (pOldestDB->cChanges)
                    pDevContext->EndTimeStampofDB = pOldestDB->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601;
            } 
            pBitmapWorkItem = AllocateBitmapWorkItem(ecBitmapWorkItemSetBits);
            if (NULL != pBitmapWorkItem) {
                PDEV_BITMAP         pDevBitmap = NULL;
                PCHANGE_NODE        ChangeNode = GetChangeNodeFromChangeList(pDevContext, false);
                pDirtyBlock = ChangeNode->DirtyBlock;

                if (pDirtyBlock->ulFlags & KDIRTY_BLOCK_FLAG_CONTAINS_TAGS) {
                    pDevContext->ulNumberOfTagPointsDropped++;

                    if (!TEST_FLAG(pDirtyBlock->ulFlags, KDIRTY_BLOCK_FLAG_REVOKE_TAGS))
                        InDskFltTelemetryLogTagHistory(pDirtyBlock, pDevContext, ecTagStatusDropped, ecBitmapWrite, ecMsgTagDropped);

                    if (pDirtyBlock->ulFlags & KDIRTY_BLOCK_FLAG_TAG_WAITING_FOR_COMMIT) {
                        PTAG_NODE TagNode = AddStatusAndReturnNodeIfComplete(pDirtyBlock->TagGUID, pDirtyBlock->ulTagDevIndex, ecTagStatusDropped);
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
                    ASSERT(pDirtyBlock->ulcbDataFree <= pDevContext->ulcbDataFree); 
                    VCAdjustCountersForLostDataBytes(pDevContext, NULL, pDirtyBlock->ulcbDataFree);
                    pDirtyBlock->ulcbDataFree = 0;
                }
                DeductChangeCountersOnDataSource(&pDevContext->ChangeList, pDirtyBlock->ulDataSource,
                                                 pDirtyBlock->cChanges, pDirtyBlock->ulicbChanges.QuadPart);

                pDevContext->ulChangesQueuedForWriting += pDirtyBlock->cChanges;
                pDevContext->ulicbChangesQueuedForWriting.QuadPart += pDirtyBlock->ulicbChanges.QuadPart;
                ulChangesCopied += pDirtyBlock->cChanges;

                pDevBitmap = pDevContext->pDevBitmap;
                pBitmapWorkItem->eBitmapWorkItem = ecBitmapWorkItemSetBits;
                pBitmapWorkItem->pDevBitmap = ReferenceDevBitmap(pDevBitmap);
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
                KeAcquireSpinLock(&pDevBitmap->Lock, &OldIrql);
                InsertHeadList(&pDevBitmap->SetBitsWorkItemList, &pBitmapWorkItem->ListEntry);
                KeReleaseSpinLock(&pDevBitmap->Lock, OldIrql);

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
    PDEV_BITMAP         pDevBitmap;
    bool                bRemoveEntry = false;
    PKDIRTY_BLOCK DirtyBlock = NULL;
    PCHANGE_NODE ChangeNode = NULL;    

    pBitmapWorkItem = (PBITMAP_WORK_ITEM) pWorkQueueEntry->Context;
    ASSERT(1 == pBitmapWorkItem->bBitMapWorkQueueRef);
    pBitmapWorkItem->bBitMapWorkQueueRef = 0;

    ASSERT(NULL != pBitmapWorkItem);
    if (NULL == pBitmapWorkItem) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE, ("BitmapWriteWorkerRoutine: pBitmapWorkItem is NULL\n"));
        goto _cleanup;
    }

    ASSERT(NULL != pBitmapWorkItem->BitRuns.pdirtyBlock);
    if (NULL != pBitmapWorkItem->BitRuns.pdirtyBlock) {
        DirtyBlock = pBitmapWorkItem->BitRuns.pdirtyBlock;
        ChangeNode = DirtyBlock->ChangeNode;
    }

    pDevBitmap = pBitmapWorkItem->pDevBitmap;
    if (NULL == pDevBitmap) {
        RemoveEntryList(&pBitmapWorkItem->ListEntry);
        DereferenceBitmapWorkItem(pBitmapWorkItem);
        InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE,
            ("BitmapWriteWorkerRoutine: pBitmapWorkItem->pDevBitmap is NULL\n"));
        goto _cleanup;
    }

    // ReferenceDevBitmap to keep bitmap available till it is unlocked.
    ReferenceDevBitmap(pDevBitmap);
    ExAcquireFastMutex(&pDevBitmap->Mutex);

    if ((NULL != pDevBitmap->pDevContext)) {
        // if the device is not removed
        if ((ecVBitmapStateClosed == pDevBitmap->eVBitmapState) ||
            (NULL == pDevBitmap->pBitmapAPI))
        {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE,
                ("BitmapWriteWorkerRoutine: Failed State = %s, pBitmapAPI = %#p for device %S(%S)\n",
                    GetDevBitmapStateString(pDevBitmap->eVBitmapState), pDevBitmap->pBitmapAPI,
                    pDevBitmap->DevID, pDevBitmap->wDevNum));
            bRemoveEntry = true;
        } else {
            pBitmapWorkItem->BitRuns.context1 = pBitmapWorkItem;
            pBitmapWorkItem->BitRuns.completionCallback = WriteBitmapCompletionCallback;
            pDevBitmap->pBitmapAPI->SetBits(&pBitmapWorkItem->BitRuns);
        }
    } else {
        bRemoveEntry = true;
    }
    if (bRemoveEntry) {
        if (ecBitmapWorkItemSetBits == pBitmapWorkItem->eBitmapWorkItem) {
            KIRQL       OldIrql;

            KeAcquireSpinLock(&pDevBitmap->Lock, &OldIrql);
            ASSERT(TRUE == IsEntryInList(&pDevBitmap->SetBitsWorkItemList, &pBitmapWorkItem->ListEntry));

            RemoveEntryList(&pBitmapWorkItem->ListEntry);
            if (pDevBitmap->ulFlags & DEV_BITMAP_FLAGS_WAITING_FOR_SETBITS_WORKITEM_LIST_EMPTY_NOTIFICATION) {
                if (IsListEmpty(&pDevBitmap->SetBitsWorkItemList)) {
                    pDevBitmap->ulFlags &= ~DEV_BITMAP_FLAGS_WAITING_FOR_SETBITS_WORKITEM_LIST_EMPTY_NOTIFICATION;
                    KeSetEvent(&pDevBitmap->SetBitsWorkItemListEmtpyNotification, IO_NO_INCREMENT, FALSE);
                }
            }

            KeReleaseSpinLock(&pDevBitmap->Lock, OldIrql);
        } else {
            ASSERT(ecBitmapWorkItemClearBits == pBitmapWorkItem->eBitmapWorkItem);
            ASSERT(TRUE == IsEntryInList(&pDevBitmap->WorkItemList, &pBitmapWorkItem->ListEntry));
            RemoveEntryList(&pBitmapWorkItem->ListEntry);
        }

        DereferenceBitmapWorkItem(pBitmapWorkItem);
    }

    ExReleaseFastMutex(&pDevBitmap->Mutex);
    DereferenceDevBitmap(pDevBitmap);

_cleanup:
    if (ChangeNode != NULL) {
        ChangeNodeCleanup(ChangeNode);
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
QueueWorkerRoutineForStartBitmapRead(PDEV_BITMAP  pDevBitmap)
{
    PWORK_QUEUE_ENTRY       pWorkQueueEntry;
    PBITMAP_WORK_ITEM       pBitmapWorkItem;

    ASSERT(NULL != pDevBitmap);

    if (NULL == pDevBitmap) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_READ | DM_BITMAP_OPEN, 
                ("QueueWorkerRoutineForStartBitmapRead: pDevBitmap is NULL\n"));
        return;
    }

        pBitmapWorkItem = AllocateBitmapWorkItem(ecBitmapWorkItemStartRead);
        if (NULL != pBitmapWorkItem) {
            pWorkQueueEntry = pBitmapWorkItem->pWorkQueueEntryPreAlloc;
            pWorkQueueEntry->Context = pBitmapWorkItem;
            pBitmapWorkItem->bBitMapWorkQueueRef = 1;
            pWorkQueueEntry->WorkerRoutine = StartBitmapReadWorkerRoutine;
            pBitmapWorkItem->pDevBitmap = ReferenceDevBitmap(pDevBitmap);
            pBitmapWorkItem->eBitmapWorkItem = ecBitmapWorkItemStartRead;
            AddItemToWorkQueue(&DriverContext.WorkItemQueue, pWorkQueueEntry);
        } else {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_READ,
                ("QueueWorkerRoutineForStartBitmapRead: Failed to allocate BitmapWorkItem\n"));
        }

    return;
}

VOID
QueueWorkerRoutineForContinueBitmapRead(PDEV_BITMAP  pDevBitmap)
{
    PWORK_QUEUE_ENTRY       pWorkQueueEntry;
    PBITMAP_WORK_ITEM       pBitmapWorkItem;

    ASSERT(NULL != pDevBitmap);

    if (NULL == pDevBitmap) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_READ | DM_BITMAP_OPEN, 
                ("QueueWorkerRoutineForContinueBitmapRead: pDevBitmap is NULL\n"));
        return;
    }

        pBitmapWorkItem = AllocateBitmapWorkItem(ecBitmapWorkItemContinueRead);
        if (NULL != pBitmapWorkItem) {
            pWorkQueueEntry = pBitmapWorkItem->pWorkQueueEntryPreAlloc;
            pWorkQueueEntry->Context = pBitmapWorkItem;
            pBitmapWorkItem->bBitMapWorkQueueRef = 1;
            pWorkQueueEntry->WorkerRoutine = ContinueBitmapReadWorkerRoutine;
            pBitmapWorkItem->pDevBitmap = ReferenceDevBitmap(pDevBitmap);
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
    PDEV_BITMAP         pDevBitmap;

    pBitmapWorkItem = (PBITMAP_WORK_ITEM) pWorkQueueEntry->Context;
    ASSERT(1 == pBitmapWorkItem->bBitMapWorkQueueRef);
    pBitmapWorkItem->bBitMapWorkQueueRef = 0;

    ASSERT((NULL != pBitmapWorkItem) && (NULL != pBitmapWorkItem->pDevBitmap));
    if ((NULL == pBitmapWorkItem) || (NULL == pBitmapWorkItem->pDevBitmap)) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_READ, ("StartBitmapReadWorkerRoutine: pBitmapWorkItem = %#p, pDevBitmap = %#p\n",
            pBitmapWorkItem, pBitmapWorkItem ? pBitmapWorkItem->pDevBitmap : NULL));
        return;
    }

    // Acquire the lock.
    pDevBitmap = ReferenceDevBitmap(pBitmapWorkItem->pDevBitmap);
    if ((NULL != pDevBitmap->pDevContext) && (0 == (pDevBitmap->pDevContext->ulFlags & DCF_FILTERING_STOPPED))
        && (!TEST_FLAG(pDevBitmap->pDevContext->ulFlags, DONT_READ_BITMAP))) {
        ExAcquireFastMutex(&pDevBitmap->Mutex);

        if ((ecVBitmapStateReadStarted == pDevBitmap->eVBitmapState) &&
            (NULL != pDevBitmap->pBitmapAPI))
        {
            pBitmapWorkItem->BitRuns.context1 = pBitmapWorkItem;
            pBitmapWorkItem->BitRuns.completionCallback = ReadBitmapCompletionCallback;
            InsertHeadList(&pDevBitmap->WorkItemList, &pBitmapWorkItem->ListEntry);
            pDevBitmap->pBitmapAPI->GetFirstRuns(&pBitmapWorkItem->BitRuns);
            pBitmapWorkItem = NULL;
        }

        ExReleaseFastMutex(&pDevBitmap->Mutex);
    }
    DereferenceDevBitmap(pDevBitmap);

    if (NULL != pBitmapWorkItem)
        DereferenceBitmapWorkItem(pBitmapWorkItem);

    return;
}

VOID
ContinueBitmapReadWorkerRoutine(PWORK_QUEUE_ENTRY   pWorkQueueEntry)
{
    PBITMAP_WORK_ITEM   pBitmapWorkItem;

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
    PDEV_BITMAP      pDevBitmap;

    pDevBitmap = pBitmapWorkItem->pDevBitmap;
    // Acquire the lock.
    if (NULL != pDevBitmap) {
        ReferenceDevBitmap(pDevBitmap);
        if ((NULL != pDevBitmap->pDevContext) && (0 == (pDevBitmap->pDevContext->ulFlags & DCF_FILTERING_STOPPED))
            && (!TEST_FLAG(pDevBitmap->pDevContext->ulFlags, DONT_READ_BITMAP))) {
            if (FALSE == bMutexAcquired)
                ExAcquireFastMutex(&pDevBitmap->Mutex);

            if ((ecVBitmapStateReadStarted != pDevBitmap->eVBitmapState) ||
                (NULL == pDevBitmap->pBitmapAPI))
            {
                DereferenceBitmapWorkItem(pBitmapWorkItem);
                // Ignore the read.
            } else {
                pBitmapWorkItem->BitRuns.context1 = pBitmapWorkItem;
                pBitmapWorkItem->BitRuns.completionCallback = ReadBitmapCompletionCallback;
                InsertHeadList(&pDevBitmap->WorkItemList, &pBitmapWorkItem->ListEntry);
                pDevBitmap->pBitmapAPI->GetNextRuns(&pBitmapWorkItem->BitRuns);
            }

            if (FALSE == bMutexAcquired)
                ExReleaseFastMutex(&pDevBitmap->Mutex);
        }
        DereferenceDevBitmap(pDevBitmap);
    } else {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_READ,
            ("StartBitmapReadWorkerRoutine: pBitmapWorkItem->pDevBitmap is NULL\n"));
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

    // Holding reference to DevContext so that BitMap File is not closed before the writes are completed.
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
    PDEV_BITMAP         pDevBitmap;
    PKDIRTY_BLOCK       pDirtyBlock = NULL;
    KIRQL               OldIrql;
    PDEV_CONTEXT        pDevContext = NULL;
    BOOLEAN             bClearBitsRead, bContinueBitMapRead;
	BOOLEAN             bLoopAlwaysFalse = FALSE;

    // pBitmapWorkItem is later used for clear bits.
    PAGED_CODE();

    ASSERT((NULL != pBitmapWorkItem) && (NULL != pBitmapWorkItem->pDevBitmap));
    if ((NULL == pBitmapWorkItem) || (NULL == pBitmapWorkItem->pDevBitmap)) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_READ, ("ReadBitmapCompletion: pBitmapWorkItem = %#p, pDevBitmap = %#p\n",
            pBitmapWorkItem, pBitmapWorkItem ? pBitmapWorkItem->pDevBitmap : NULL));
        return;
    }

    bClearBitsRead = FALSE;
    bContinueBitMapRead = FALSE;

    // Let us see if this bitmap is still operational.
    pDevBitmap = ReferenceDevBitmap(pBitmapWorkItem->pDevBitmap);
    ExAcquireFastMutex(&pDevBitmap->Mutex);
    // Check if bitmap is still in read state.
    if ((ecVBitmapStateReadStarted != pDevBitmap->eVBitmapState) ||
        (NULL == pDevBitmap->pBitmapAPI) ||
        (NULL == pDevBitmap->pDevContext) ||
        (TEST_FLAG(pDevBitmap->pDevContext->ulFlags, DONT_READ_BITMAP)) ||
        (TEST_FLAG(pDevBitmap->pDevContext->ulFlags, DCF_FILTERING_STOPPED))
#ifdef VOLUME_CLUSTER_SUPPORT
        || (pDevBitmap->pDevContext->ulFlags & DCF_CV_FS_UNMOUTNED)
#endif
        )
    {
        // Bitmap is closed, or bitmap is moved to write state. Ingore this read.
        RemoveEntryList(&pBitmapWorkItem->ListEntry);
        DereferenceBitmapWorkItem(pBitmapWorkItem);
        InVolDbgPrint(DL_INFO, DM_BITMAP_READ,
            ("ReadBitmapCompletion: Ignoring bitmap read - State = %s, pBitmapAPI = %#p, pDevContext = %#p\n",
                GetDevBitmapStateString(pDevBitmap->eVBitmapState), pDevBitmap->pBitmapAPI, pDevBitmap->pDevContext));
    } else {
        pDevContext = pDevBitmap->pDevContext;
        do {
            // error processing.    
            if ((STATUS_SUCCESS != pBitmapWorkItem->BitRuns.finalStatus) &&
                (STATUS_MORE_PROCESSING_REQUIRED != pBitmapWorkItem->BitRuns.finalStatus))
            {
                pDevBitmap->eVBitmapState = ecVBitmapStateReadError;
                InterlockedIncrement(&pDevContext->lNumBitMapReadErrors);
                SetDevOutOfSync(pDevContext, ERROR_TO_REG_BITMAP_READ_ERROR, pBitmapWorkItem->BitRuns.finalStatus, false);
                InVolDbgPrint(DL_ERROR, DM_BITMAP_READ,
                    ("ReadBitmapCompletion: Received Error %#x Setting bitmap state of Volume %S(%S) to ecVBitmapError\n",
                        pBitmapWorkItem->BitRuns.finalStatus, pDevBitmap->DevID, pDevBitmap->wDevNum));
                break;
            }

            // Read Succeded, check if any changes are returned
            if ((STATUS_SUCCESS == pBitmapWorkItem->BitRuns.finalStatus) && (0 == pBitmapWorkItem->BitRuns.nbrRuns)) {
                pDevBitmap->eVBitmapState = ecVBitmapStateReadCompleted;
                InVolDbgPrint(DL_INFO, DM_BITMAP_READ,
                    ("ReadBitmapCompletion: Bitmap read of Volume %S(%S) is completed\n",
                        pDevBitmap->DevID, pDevBitmap->wDevNum));
                break;
            }
          
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
                    pDevBitmap->eVBitmapState = ecVBitmapStateOpened;
                    break;
                }    
            }

            unsigned int i = 0;
            KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
            for (i = 0; i < pBitmapWorkItem->BitRuns.nbrRuns; i++) 
            {
                WRITE_META_DATA WriteMetaData;
                ULONG VCSplitChangesReturned = 0;

                WriteMetaData.llOffset = pBitmapWorkItem->BitRuns.runOffsetArray[i];
                WriteMetaData.ulLength = pBitmapWorkItem->BitRuns.runLengthArray[i];
                
                VCSplitChangesReturned = AddMetaDataToDirtyBlock(pDevContext, pDirtyBlock, &WriteMetaData, INFLTDRV_DATA_SOURCE_BITMAP);

                if(0 == VCSplitChangesReturned)
                    break;

                VCcbChangesReadFromBitMap += WriteMetaData.ulLength;
                VCTotalChangesPending += VCSplitChangesReturned;

                if(VCSplitChangesReturned > 1)
                    pDirtyBlock = NULL;
                else
                    pDirtyBlock = pDevContext->ChangeList.CurrentNode->DirtyBlock;
            }

            pDevContext->uliChangesReadFromBitMap.QuadPart += VCTotalChangesPending;
            pDevContext->ulicbChangesReadFromBitMap.QuadPart += VCcbChangesReadFromBitMap;

            KeReleaseSpinLock(&pDevContext->Lock, OldIrql);

            if(i < pBitmapWorkItem->BitRuns.nbrRuns)
            {
                pDevBitmap->eVBitmapState = ecVBitmapStateOpened;
                break;
            }

            bClearBitsRead = TRUE;
            if (STATUS_SUCCESS == pBitmapWorkItem->BitRuns.finalStatus) {
                // This is the last read.
                pDevBitmap->eVBitmapState = ecVBitmapStateReadCompleted;
                InVolDbgPrint(DL_INFO, DM_BITMAP_READ,
                    ("ReadBitmapCompletion: Bitmap read of Volume %S(%S) is completed\n",
                        pDevBitmap->DevID, pDevBitmap->wDevNum));
            } else if ((0 != DriverContext.DBLowWaterMarkWhileServiceRunning) && 
                (pDevContext->ChangeList.ulTotalChangesPending >=  DriverContext.DBLowWaterMarkWhileServiceRunning))
            {
                // pBitmapWorkItem->BitRuns.finalStatus is STATUS_MORE_PROCESSING_REQUIRED.
                // If this is not a final read, and reached the low water mark pause the reads.
                pDevBitmap->eVBitmapState = ecVBitmapStateReadPaused;
                InVolDbgPrint(DL_INFO, DM_BITMAP_READ,
                    ("ReadBitmapCompletion: Bitmap read of Volume %S(%S) is paused\n",
                        pDevBitmap->DevID, pDevBitmap->wDevNum));
            } else {
                bContinueBitMapRead = TRUE;
            }

        } while (bLoopAlwaysFalse); //Fixed W4 Warnings. AI P3:- Code need to be rewritten to get rid of loop

        if (TRUE == bContinueBitMapRead) {
            PBITMAP_WORK_ITEM   pBWIforContinuingRead = NULL;

            pBWIforContinuingRead = AllocateBitmapWorkItem(ecBitmapWorkItemContinueRead);
            if (NULL != pBWIforContinuingRead) {
                pBWIforContinuingRead->eBitmapWorkItem = ecBitmapWorkItemContinueRead;
                pBWIforContinuingRead->pDevBitmap = ReferenceDevBitmap(pDevBitmap);
                ContinueBitmapRead(pBWIforContinuingRead, TRUE);
            } else {
                pDevBitmap->eVBitmapState = ecVBitmapStateReadPaused;
                InVolDbgPrint(DL_ERROR, DM_MEM_TRACE | DM_BITMAP_READ,
                    ("ReadBitmapCompletion: Failed to allocate BitmapWorkItem to continue read for device %S(%S)\nSetting state to paused",
                        pDevBitmap->pDevContext->wDevID, pDevBitmap->pDevContext->wDevNum));
            }                
        }

        if (TRUE == bClearBitsRead) {
            // Do not have to set pBitmapWorkItem->pDevBitmap and pBitmapWorkItem->BitRuns.context1
            // These are set when received. 
            // pBitmapWorkItem is already inserted in pDevBitmap->WorkItemList.
            // should not re-insert would lead to corruption. have to remove from list before re-insert
            // pBitmapWorkItem->pDevBitmap == pDevBitmap;
            // pBitmapWorkItem->BitRuns.context1 = pBitmapWorkItem; 
            // RemoveEntryList(&pBitmapWorkItem->ListEntry);
            // InsertHeadList(&pDevBitmap->WorkItemList, &pBitmapWorkItem->ListEntry);
            ASSERT(pBitmapWorkItem->BitRuns.context1 == pBitmapWorkItem);
            ASSERT(pBitmapWorkItem->pDevBitmap == pDevBitmap);
            pBitmapWorkItem->eBitmapWorkItem = ecBitmapWorkItemClearBits;
            pBitmapWorkItem->BitRuns.completionCallback = WriteBitmapCompletionCallback;
            pDevBitmap->pBitmapAPI->ClearBits(&pBitmapWorkItem->BitRuns);
        } else {
            RemoveEntryList(&pBitmapWorkItem->ListEntry);
            DereferenceBitmapWorkItem(pBitmapWorkItem);
        }

        if (NULL != pDevContext) {
            KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
            if (ecVBitmapStateReadCompleted == pDevBitmap->eVBitmapState) {
                // Check if capture mode can be changed.
                if ((ecCaptureModeMetaData == pDevContext->eCaptureMode) && CanSwitchToDataCaptureMode(pDevContext)) {
                    InVolDbgPrint(DL_INFO, DM_CAPTURE_MODE | DM_DATA_FILTERING,
                                  ("%s: Changing capture mode to DATA for device %s(%s)\n", __FUNCTION__,
                                   pDevContext->chDevID, pDevContext->chDevNum));
                    VCSetCaptureModeToData(pDevContext);
                }
                VCSetWOState(pDevContext, false, __FUNCTION__, ecMDReasonNotApplicable);
            }

            if ((NULL != pDevContext->DBNotifyEvent) && (pDevContext->bNotify)) {
                ASSERT(pDevContext->ChangeList.ullcbTotalChangesPending >= pDevContext->ullcbChangesPendingCommit);
                if (DirtyBlockReadyForDrain(pDevContext)) {
                    pDevContext->bNotify = false;
                    pDevContext->liTickCountAtNotifyEventSet = KeQueryPerformanceCounter(NULL);
                    KeSetEvent(pDevContext->DBNotifyEvent, EVENT_INCREMENT, FALSE);
                }
            }

            KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
        }
    }

    ExReleaseFastMutex(&pDevBitmap->Mutex);
    DereferenceDevBitmap(pDevBitmap);
    return;
}

NTSTATUS
GetDevBitmapGranularity(__in PDEV_CONTEXT pDevContext,
                       __out PULONG pulBitmapGranularity)
/*++
Description : This function sets and retrieves the granularity based on the size of device, both in-memory and registry
Parameter   : pDevContext          - Pointer to device context
              pulBitmapGranularity - Pointer to granularity field
Return      : STATUS_INVALID_PARAMETER or STATUS_SUCCESS

History     :
12/8/15     - Added fix to set granularity based on the size of device

Assumptions : Device size should be up-to-date. This can be ensured by retrieving the size of device on every bitmap open.
--*/

{
    NTSTATUS        Status = STATUS_INVALID_PARAMETER;
    Registry        *pDevParam = NULL;
    ULONG           ulDefaultGranularity = 0;

    ASSERT(NULL != pDevContext);
    ASSERT(0 != pDevContext->llDevSize);
    ASSERT(NULL != pulBitmapGranularity);

    if (NULL == pulBitmapGranularity) {
        InVolDbgPrint( DL_INFO, DM_BITMAP_OPEN, ("GetDevBitmapGranularity: pulBitmapGranularity is NULL\n"));
        return STATUS_INVALID_PARAMETER_1;
    }

    if (NULL == pDevContext) {
        InVolDbgPrint( DL_INFO, DM_BITMAP_OPEN, ("GetDevBitmapGranularity: pDevContext is NULL\n"));
        return STATUS_INVALID_PARAMETER_2;
    }

    if (0 == pDevContext->llDevSize) {
        InVolDbgPrint( DL_INFO, DM_BITMAP_OPEN, 
                ("GetDevBitmapGranularity: VolumeSize is %#I64x\n", pDevContext->llDevSize));
        return STATUS_INVALID_PARAMETER_3;
    }

    Status = OpenDeviceParametersRegKey(pDevContext, &pDevParam);
    if (!NT_SUCCESS(Status) || (NULL == pDevParam)) {
        InVolDbgPrint( DL_INFO, DM_BITMAP_OPEN, 
            ("GetDevBitmapGranularity: Failed to open volume parameters key. Status = %#x\n", Status));        
        return Status;
    }
    // Bitmap granularity is calculated based on the size of device, this need to be updated in-memory and registry
    // to accommodate device resize
    // * If the key does not exist, Create key and set the default granularity for a given size
    // * Update the registry key if the granularity does not match with the one read from registry

    ulDefaultGranularity = DefaultGranularityFromDevSize(pDevContext->llDevSize);

    if (0 == ulDefaultGranularity)
        ulDefaultGranularity = DEVICE_BITMAP_GRANULARITY_DEFAULT_VALUE;

    Status = pDevParam->ReadULONG(DEVICE_BITMAP_GRANULARITY, (ULONG32 *)pulBitmapGranularity, ulDefaultGranularity, TRUE);
    if (!NT_SUCCESS(Status)) {
        InVolDbgPrint( DL_ERROR, DM_BITMAP_OPEN, ("GetDevBitmapGranularity: Failed in retreiving %S, Status = %#x\n", 
                        DEVICE_BITMAP_GRANULARITY, Status));
    }

    if (ulDefaultGranularity != *pulBitmapGranularity) {
        Status = pDevParam->WriteULONG(DEVICE_BITMAP_GRANULARITY, ulDefaultGranularity);
        if (!NT_SUCCESS(Status)) {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("GetDevBitmapGranularity: Failed to write %S, Status = %#x\n",
                DEVICE_BITMAP_GRANULARITY, Status));
        }
        *pulBitmapGranularity = ulDefaultGranularity;
    }

    delete pDevParam;

    return Status;
}

VOID
ClearBitmapFile(PDEV_BITMAP pDevBitmap)
{
    BOOLEAN         bWaitForNotification = FALSE;
    BOOLEAN         bSetBitsWorkItemListIsEmpty;
    NTSTATUS        Status;
    KIRQL           OldIrql;

    PAGED_CODE();

    InVolDbgPrint(DL_INFO, DM_BITMAP_WRITE,
        ("ClearBitmapFile: Called for device %S(%S)\n", pDevBitmap->DevID, pDevBitmap->wDevNum));

    if ((NULL != pDevBitmap) && (NULL != pDevBitmap->pBitmapAPI)) {
        do {
            if (bWaitForNotification) {
                InVolDbgPrint(DL_INFO, DM_BITMAP_WRITE,
                    ("ClearBitmapFile: Waiting on SetBitsWorkItemListEmtpyNotification for device %S(%S)\n", 
                    pDevBitmap->DevID, pDevBitmap->wDevNum));
                Status = KeWaitForSingleObject(&pDevBitmap->SetBitsWorkItemListEmtpyNotification, Executive, KernelMode, FALSE, NULL);
                bWaitForNotification = FALSE;
            }

            ExAcquireFastMutex(&pDevBitmap->Mutex);
            
            KeAcquireSpinLock(&pDevBitmap->Lock, &OldIrql);
            bSetBitsWorkItemListIsEmpty = IsListEmpty(&pDevBitmap->SetBitsWorkItemList);
            KeReleaseSpinLock(&pDevBitmap->Lock, OldIrql);
            
            if (bSetBitsWorkItemListIsEmpty) {
                if (NULL != pDevBitmap->pBitmapAPI) {
                    pDevBitmap->pBitmapAPI->ClearAllBits();
                }

                // Set the state of bitmap to readcompleted.
                InVolDbgPrint(DL_INFO, DM_BITMAP_WRITE,
                    ("ClearBitmapFile: Setting bitmap state to ecVBitmapStateReadCompleted for device %S(%S)\n", 
                     pDevBitmap->DevID, pDevBitmap->wDevNum));
                pDevBitmap->eVBitmapState = ecVBitmapStateReadCompleted;

            } else {
                InVolDbgPrint(DL_INFO, DM_BITMAP_WRITE,
                    ("ClearBitmapFile: SetBitsWorkItemList is not Emtpy for device %S(%S)\n", pDevBitmap->DevID, pDevBitmap->wDevNum));
                KeClearEvent(&pDevBitmap->SetBitsWorkItemListEmtpyNotification);
                pDevBitmap->ulFlags |= DEV_BITMAP_FLAGS_WAITING_FOR_SETBITS_WORKITEM_LIST_EMPTY_NOTIFICATION;
                bWaitForNotification = TRUE;
            }
            ExReleaseFastMutex(&pDevBitmap->Mutex);
        } while (bWaitForNotification);        
    }

    return;
}

NTSTATUS
WaitForAllWritesToComplete(PDEV_BITMAP pDevBitmap, PLARGE_INTEGER pliTimeOut)
{
    BOOLEAN         bWaitForNotification = FALSE;
    BOOLEAN         bSetBitsWorkItemListIsEmpty;
    NTSTATUS        Status = STATUS_SUCCESS;
    KIRQL           OldIrql;

    PAGED_CODE();

    InVolDbgPrint(DL_INFO, DM_BITMAP_WRITE,
        ("WaitForAllWritesToComplete: Called for device %S(%S)\n", pDevBitmap->DevID, pDevBitmap->wDevNum));

    if (NULL != pDevBitmap) {
        ASSERT(FALSE == bWaitForNotification);

        ExAcquireFastMutex(&pDevBitmap->Mutex);
        
        KeAcquireSpinLock(&pDevBitmap->Lock, &OldIrql);
        bSetBitsWorkItemListIsEmpty = IsListEmpty(&pDevBitmap->SetBitsWorkItemList);
        KeReleaseSpinLock(&pDevBitmap->Lock, OldIrql);
        
        if (FALSE == bSetBitsWorkItemListIsEmpty) {
            InVolDbgPrint(DL_INFO, DM_BITMAP_CLOSE,
                ("WaitForAllWritesToComplete: SetBitsWorkItemList is not Emtpy for device %S(%S)\n", 
                pDevBitmap->DevID, pDevBitmap->wDevNum));
            KeClearEvent(&pDevBitmap->SetBitsWorkItemListEmtpyNotification);
            pDevBitmap->ulFlags |= DEV_BITMAP_FLAGS_WAITING_FOR_SETBITS_WORKITEM_LIST_EMPTY_NOTIFICATION;
            bWaitForNotification = TRUE;
        }
        ExReleaseFastMutex(&pDevBitmap->Mutex);

        if (bWaitForNotification) {
            InVolDbgPrint(DL_INFO, DM_BITMAP_WRITE,
                ("WaitForAllWritesToComplete: Waiting on SetBitsWorkItemListEmtpyNotification for device %S(%S)\n", 
                pDevBitmap->DevID, pDevBitmap->wDevNum));
            Status = KeWaitForSingleObject(&pDevBitmap->SetBitsWorkItemListEmtpyNotification, Executive, KernelMode, FALSE, pliTimeOut);
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
    
    if (NULL != pBitmapWorkItem->pDevBitmap) {
        DereferenceDevBitmap(pBitmapWorkItem->pDevBitmap);
        pBitmapWorkItem->pDevBitmap = NULL;
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
        pBitmapWorkItem->pDevBitmap = NULL;
        pBitmapWorkItem->eBitmapWorkItem = ecBitmapWorkItemNotInitialized;
    }

    return pBitmapWorkItem;
}
/*
VOID
ReferenceBitmapWorkItem(PBITMAP_WORK_ITEM pBitmapWorkItem)
{
    ASSERT(pBitmapWorkItem->lRefCount >= 1);

    InterlockedIncrement(&pBitmapWorkItem->lRefCount);

    return;
}
*/
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
 * DEV_BITMAP Allocation and Deallocation Routines
 *--------------------------------------------------------------------------------------------
 */

PDEV_BITMAP
AllocateDevBitmap()
{
    PDEV_BITMAP  pDevBitmap;
    KIRQL           OldIrql;

    pDevBitmap = (PDEV_BITMAP)ExAllocatePoolWithTag(NonPagedPool, sizeof(DEV_BITMAP), TAG_DEV_BITMAP);
    if (NULL != pDevBitmap) {
        RtlZeroMemory(pDevBitmap, sizeof(DEV_BITMAP));
        pDevBitmap->lRefCount = 1;
        pDevBitmap->eVBitmapState = ecVBitmapStateUnInitialized;
        InitializeListHead(&pDevBitmap->WorkItemList);
        InitializeListHead(&pDevBitmap->SetBitsWorkItemList);
        KeInitializeEvent(&pDevBitmap->SetBitsWorkItemListEmtpyNotification, NotificationEvent, FALSE);
        ExInitializeFastMutex(&pDevBitmap->Mutex);
        KeInitializeSpinLock(&pDevBitmap->Lock);

        KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
        InsertHeadList(&DriverContext.HeadForDevBitmaps, &pDevBitmap->ListEntry);
        DriverContext.ulNumDevBitmaps += 1;
        KeReleaseSpinLock(&DriverContext.Lock, OldIrql);
    }

    return pDevBitmap;
}

VOID
DeallocateDevBitmap(PDEV_BITMAP pDevBitmap)
{
    KIRQL   OldIrql;

    ASSERT(NULL != pDevBitmap);
    ASSERT(0 == pDevBitmap->lRefCount);
    ASSERT(NULL == pDevBitmap->pDevContext);
    ASSERT(IsListEmpty(&pDevBitmap->WorkItemList));
    ASSERT(IsListEmpty(&pDevBitmap->SetBitsWorkItemList));
    ASSERT(0 < DriverContext.ulNumDevBitmaps);

    if (NULL != pDevBitmap->pBitmapAPI)
        pDevBitmap->pBitmapAPI->Release();

    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    RemoveEntryList(&pDevBitmap->ListEntry);
    DriverContext.ulNumDevBitmaps -= 1;
    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

    ExFreePoolWithTag(pDevBitmap, TAG_DEV_BITMAP);
    return;
}

PDEV_BITMAP
ReferenceDevBitmap(PDEV_BITMAP    pDevBitmap)
{
    ASSERT(0 < pDevBitmap->lRefCount);

    InterlockedIncrement(&pDevBitmap->lRefCount);

    return pDevBitmap;
}

VOID
DereferenceDevBitmap(PDEV_BITMAP  pDevBitmap)
{
    ASSERT(0 < pDevBitmap->lRefCount);

    if (InterlockedDecrement(&pDevBitmap->lRefCount) <= 0) {
        DeallocateDevBitmap(pDevBitmap);
    }

    return;
}
