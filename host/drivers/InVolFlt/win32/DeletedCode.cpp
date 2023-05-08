// Deleted Code.

VOID
OpenBitmapFileWorkerRoutine(
    PWORK_QUEUE_ENTRY   pWorkQueueEntry
    )
{
    PVOLUME_CONTEXT   pVolumeContext;
    Registry    *pVolumeParam = NULL;
    NTSTATUS    Status;
    ULONG       ulValue;

    pVolumeContext = (PVOLUME_CONTEXT)pWorkQueueEntry->Context;
    DereferenceWorkQueueEntry(pWorkQueueEntry);

    ExAcquireFastMutex(&pVolumeContext->Mutex);
    if (NULL == pVolumeContext->pBitMapFile) {
        pVolumeContext->pBitMapFile = OpenBitmapFile(pVolumeContext);
        pVolumeContext->ulFlags &= ~VCF_OPEN_BITMAP_STARTED;
    }
    ExReleaseFastMutex(&pVolumeContext->Mutex);

    if (ecServiceRunning == DriverContext.eServiceState) {
        InVolDbgPrint(DL_INFO, DM_BITMAP_READ, ("OpenBitmapFileWorkerRoutine: Starting Bitmap Read for Volume=%p, DriveLetter=%S\n", 
                    pVolumeContext, pVolumeContext->DriveLetter));
        StartBitmapRead(pVolumeContext);
    }

    DereferenceVolumeContext(pVolumeContext);
    
    return;
}

VOID
QueueWorkerRoutineForOpenBitmapFile(
    PVOLUME_CONTEXT   pVolumeContext,
    BOOLEAN           bLockAcquired
    )
{
    PWORK_QUEUE_ENTRY       pWorkQueueEntry;
    KIRQL                   OldIrql;

    if (FALSE == bLockAcquired)
        KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);

    if (0 == (pVolumeContext->ulFlags & VCF_OPEN_BITMAP_STARTED))
    {
        // Queue worker routine to retreive Volume GUIDs
        pWorkQueueEntry = AllocateWorkQueueEntry();
        if (NULL != pWorkQueueEntry)
        {
            pWorkQueueEntry->Context = pVolumeContext;
            ReferenceVolumeContext(pVolumeContext);
            pWorkQueueEntry->WorkerRoutine = OpenBitmapFileWorkerRoutine;
            pVolumeContext->ulFlags |= VCF_OPEN_BITMAP_STARTED;

            AddItemToWorkQueue(&DriverContext.WorkItemQueue, pWorkQueueEntry);
        }
    }

    if (FALSE == bLockAcquired)
        KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);

    return;
}

VOID WaitForVolumesToBeWriteable()
{
NTSTATUS status;
OBJECT_ATTRIBUTES ObjectAttributes;
HANDLE eventHandle;
UNICODE_STRING eventName;
PKEVENT event;
__int64 delayInterval;
static BOOLEAN alreadyReady = FALSE;

    if (alreadyReady) {
        return;
    }

    eventHandle = NULL;
    RtlInitUnicodeString(&eventName, VOLUMES_READY_EVENT);

    // loop around with a delay in each loop until the event get's created by the sesion manager
    do {
        InitializeObjectAttributes ( &ObjectAttributes,
                                        &eventName,
                                        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
                                        NULL,
                                        NULL );

        status = ObOpenObjectByName(&ObjectAttributes,
                                    *ExEventObjectType,
                                    NULL,
                                    KernelMode,
                                    GENERIC_READ,
                                    NULL, //   IN PACCESS_STATE PassedAccessState,
                                    &eventHandle);
        if (!NT_SUCCESS(status)) {

            delayInterval = RELATIVE_MILLISECOND_TIME * 200;
            KeDelayExecutionThread(KernelMode,
                                   FALSE,
                                   (PLARGE_INTEGER)&delayInterval);
        }
    } while (!NT_SUCCESS(status));

    status = ObReferenceObjectByHandle(eventHandle,
                                       GENERIC_READ,
                                       *ExEventObjectType,
                                       KernelMode,
                                       (PVOID *)&event,
                                       NULL);

    if (NT_SUCCESS(status)) {
        status = KeWaitForSingleObject(event,
                                       Executive,
                                       KernelMode,
                                       FALSE,
                                       NULL);
        if (NT_SUCCESS(status)) {
            alreadyReady = TRUE;
        }
        ObDereferenceObject(event);
    }

    ZwClose(eventHandle);
}

NTSTATUS
StartBitmapRead(
    PVOLUME_CONTEXT pVolumeContext
    )
{
    LIST_ENTRY       ListHead;
    BOOLEAN         bReturn = FALSE;
    PDB_WORK_ITEM   pDBworkItem;
    NTSTATUS        Status = STATUS_SUCCESS;

    ASSERT(ecServiceRunning == DriverContext.eServiceState);

    if (ecBitMapReadInProgress != pVolumeContext->eBitMapReadState) {
        InitializeListHead(&ListHead);
        bReturn = AllocateDBworkItemForRead(pVolumeContext, TRUE, &ListHead);

        if (bReturn) {
            while (!IsListEmpty(&ListHead))
            {
                // go through all Dirty Block Work Items and fire the events.
                pDBworkItem = (PDB_WORK_ITEM)RemoveHeadList(&ListHead);
                ProcessDBworkItem(pDBworkItem);
            }
        } else {
            Status = STATUS_UNSUCCESSFUL;
        }
    }

    return Status;
}

/*--------------------------------------------------------------------------------------------
 * Function Name : AllocateDBworkItemForRead
 * Parameters :
 *      pVolumeContext : This parameter is referenced and stored in the work item.
 *      bStart              : 
 *      ListHead            : pointer to list head. 
 * Returns :
 *      BOOLEAN             : TRUE if succeded in creating work item for read
 *                            FALSE if failed in creating work item
 * NOTES :
 * This function is called at DISPATCH_LEVEL. This function is called with Spinlock to
 * DirtyBlockContext held.
 *
 *      This function removes all the Dirty Blocks of the device specific dirty block context
 * and adds them to work item. these dirty blocks are later processed when work item is
 * processed by ProcessDBworkItem function
 *      This function returns NULL if there are not changes to be written for the device
 *
 *--------------------------------------------------------------------------------------------
 */
BOOLEAN
AllocateDBworkItemForRead(
    PVOLUME_CONTEXT     pVolumeContext,
    BOOLEAN             bStart,
    PLIST_ENTRY         ListHead
    )
{
    PDB_WORK_ITEM   pDBworkItem = NULL;

    if (ecBitMapReadInProgress != pVolumeContext->eBitMapReadState)
    {
        // BitMap read for this device is not started so let start it.
        pDBworkItem = AllocateDirtyBlockWorkItem();
        if (NULL != pDBworkItem) {
            if (bStart)
                pDBworkItem->eDBworkItemType = ecDBworkItemBitMapStartRead;
            else
                pDBworkItem->eDBworkItemType = ecDBworkItemBitMapContinueRead;
            ReferenceVolumeContext(pVolumeContext);
            pDBworkItem->pVolumeContext = pVolumeContext;
            pVolumeContext->eBitMapReadState = ecBitMapReadInProgress;
            InsertTailList(ListHead, &pDBworkItem->ListEntry);
        } else {
            // As we are not seting the eBitMapState to ecBitMapReadInProgress it will
            // be started later.
            return FALSE;
        }
    }

    return TRUE;
}

NTSTATUS
InMageFltSaveAllChanges(PVOLUME_CONTEXT   pVolumeContext)
{
    LIST_ENTRY          ListHead;
    PDB_WORK_ITEM       pDBworkItem = NULL;
    BOOLEAN             bWaitForWritesToBeCompleted = FALSE;
    KIRQL               OldIrql;

    if (NULL == pVolumeContext)
        return STATUS_SUCCESS;

    if (pVolumeContext->ulFlags & VCF_BITMAP_WRITE_DISABLED)
        return STATUS_SUCCESS;

    if (IS_VOLUME_OFFLINE(pVolumeContext))
    {
        // If Volume is on cluster disk, and volume is released.
        // we should not have any changes here.
        ASSERT((0 == pVolumeContext->ulTotalChangesPending) &&
                (0 == pVolumeContext->ulChangesPendingCommit));

        if (pVolumeContext->ulTotalChangesPending || pVolumeContext->ulChangesPendingCommit)
        {
            InVolDbgPrint( DL_INFO, DM_CLUSTER, 
                ("Volume DiskSig = 0x%x has ChangesPending = 0x%x ChangesPendingCommit = 0x%x\n",
                        pVolumeContext->ulDiskSignature, pVolumeContext->ulTotalChangesPending,
                        pVolumeContext->ulChangesPendingCommit));
        }

        return STATUS_SUCCESS;
    }

    InitializeListHead(&ListHead);
    KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);

    // Device is being shutdown soon, write all pending dirty blocks to bit map.
    MoveAllCommitPendingDirtyBlocksToMainQueue(pVolumeContext, TRUE);
    AllocateDBworkItemForWrite(pVolumeContext, 0, &ListHead);
    if (!IsListEmpty(&pVolumeContext->HeadForWriteDBworkItems)) {
        pVolumeContext->eDSDBCdeviceState = ecDSDBCdeviceShutdown;
        bWaitForWritesToBeCompleted = TRUE;
        KeResetEvent(&pVolumeContext->WriteListEmptyNotification);
    }
    KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);

    while (!IsListEmpty(&ListHead)) {
        pDBworkItem = (PDB_WORK_ITEM)RemoveHeadList(&ListHead);
        ProcessDBworkItem(pDBworkItem);
    }

    if (bWaitForWritesToBeCompleted) {
        // Wait for all writes to be completed.
        KeWaitForSingleObject(&pVolumeContext->WriteListEmptyNotification, Executive, KernelMode, FALSE, NULL);
    }

    return STATUS_SUCCESS;
}

VOID
ProcessDBworkItem(
    PDB_WORK_ITEM pDBworkItem
    )
{
    PVOLUME_CONTEXT pVolumeContext;
    PVOLUME_BITMAP  pVolumeBitmap;
    BitRuns         *pBitRun;
    PDIRTY_BLOCKS   pDirtyBlocks;
    KIRQL           OldIrql;

    ASSERT(NULL != pDBworkItem);
    ASSERT(NULL != pDBworkItem->pVolumeContext);

    if ((NULL == pDBworkItem) || (NULL == pDBworkItem->pVolumeContext))
        return;

    pVolumeContext = pDBworkItem->pVolumeContext;

    // For ecDBworkItemDerefDBcontext Bitmap file is not required.
    if (ecDBworkItemDerefDBcontext == pDBworkItem->eDBworkItemType) {
        DereferenceVolumeContext(pVolumeContext);
        DereferenceDirtyBlockWorkItem(pDBworkItem);
        return;
    }

    if (NULL == pVolumeContext->pBitMapFile)
    {
        // For some reason we could not open the bitmap file. we should inform service about this only if this is write
        // This failed. Need to remove write dirty blocks from list.
        if (ecDBworkItemBitMapWrite == pDBworkItem->eDBworkItemType) {
            ASSERT((NULL != pDBworkItem->DSDBCWriteEntry.Flink) || (NULL != pDBworkItem->DSDBCWriteEntry.Blink));
            KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
            RemoveEntryList(&pDBworkItem->DSDBCWriteEntry);
            KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);
            SetVolumeOutOfSync(pVolumeContext, ERROR_TO_REG_BITMAP_OPEN_ERROR);
        }
        DereferenceDirtyBlockWorkItem(pDBworkItem);
        return;
    }

    // Do the operation of read/write to bitmap.
    switch(pDBworkItem->eDBworkItemType) {
        case ecDBworkItemBitMapStartRead:
            pBitRun = &pDBworkItem->BitRuns;
            pBitRun->context1 = (PVOID)pDBworkItem;
            pBitRun->completionCallback = ReadBitMapRunsCompletionCallback;
            pVolumeContext->pBitMapFile->GetFirstRuns(pBitRun);
            // Reference of DBworkItem is released in completion.
            break;
        case ecDBworkItemBitMapContinueRead:
            pBitRun = &pDBworkItem->BitRuns;
            pBitRun->context1 = (PVOID)pDBworkItem;
            pBitRun->completionCallback = ReadBitMapRunsCompletionCallback;
            pVolumeContext->pBitMapFile->GetNextRuns(pBitRun);
            // Reference of DBworkItem is released in completion.
            break;
        case ecDBworkItemBitMapWrite:
            ASSERT(NULL == pDBworkItem->pDirtyBlocks->next);
            pDirtyBlocks = pDBworkItem->pDirtyBlocks;
            pDBworkItem->pDirtyBlocks = NULL;

            pBitRun = &pDBworkItem->BitRuns;
            memcpy(pBitRun->runs, pDirtyBlocks->Changes, pDirtyBlocks->cChanges * sizeof(DISK_CHANGE));
            pBitRun->context1 = (PVOID)pDBworkItem;
            pBitRun->completionCallback = WriteBitMapRunsCompletionCallback;
            pBitRun->nbrRuns = pDirtyBlocks->cChanges;
            DeallocateDirtyBlocks(pDirtyBlocks);
            pVolumeContext->pBitMapFile->SetBits(pBitRun);
            // Reference of DBworkItem is released in completion
            break;
        default:
            ASSERT(0);
            break;
    }

    return;
}

/*-----------------------------------------------------------------------------
 * Function : ReadBitMapRunsCompletion
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
ReadBitMapRunsCompletion(BitRuns *pBitRuns)
{
    PDIRTY_BLOCKS       pDirtyBlocks;
    PDB_WORK_ITEM       pDBworkItemForClearingBits, pDBworkItemForContinuingRead;
    KIRQL               OldIrql;
    PVOLUME_CONTEXT     pVolumeContext;
    BOOLEAN             bClearBitsRead, bContinueBitMapRead;

    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    // pDBworkItem is later used for clearing the bits.
    pDBworkItemForClearingBits = (PDB_WORK_ITEM)pBitRuns->context1;
    pDBworkItemForContinuingRead = NULL;
    bClearBitsRead = FALSE;
    bContinueBitMapRead = FALSE;


    pVolumeContext = pDBworkItemForClearingBits->pVolumeContext;

    do {
        // error processing.    
        if ((STATUS_SUCCESS != pBitRuns->finalStatus) &&
            (STATUS_MORE_PROCESSING_REQUIRED != pBitRuns->finalStatus))
        {
            pVolumeContext->eBitMapReadState = ecBitMapReadReadError;
            SetVolumeOutOfSync(pVolumeContext, ERROR_TO_REG_BITMAP_READ_ERROR);
            break;
        }

        // Read Succeded, check if any changes are returned
        if ((STATUS_SUCCESS == pBitRuns->finalStatus) && (0 == pBitRuns->nbrRuns)) {
            pVolumeContext->eBitMapReadState = ecBitMapReadCompleted;
            break;
        }

        // Read Succeded, so queue the currently read bitruns to dirty block list.
        pDirtyBlocks = AllocateDirtyBlocks();
        if (NULL == pDirtyBlocks)
        {
            pVolumeContext->eBitMapReadState = ecBitMapReadProcessingError;
            break;
        }
        
        GenerateTimeStampTag(&pDirtyBlocks->TagTimeStampOfFirstChange);
        GenerateTimeStampTag(&pDirtyBlocks->TagTimeStampOfLastChange);

        pDirtyBlocks->cChanges = pBitRuns->nbrRuns;
        RtlCopyMemory(pDirtyBlocks->Changes, pBitRuns->runs, pBitRuns->nbrRuns * sizeof(DISK_CHANGE));

        // Sum up the changes.
        for (unsigned int i = 0; i < pBitRuns->nbrRuns; i++) {
            pDirtyBlocks->ulicbChanges.QuadPart += pBitRuns->runs[i].Length;
        }

        KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
        if (ecBitMapReadInProgress != pVolumeContext->eBitMapReadState)
        {
            // Either the Read is stopped or it has been reset.
            DeallocateDirtyBlocks(pDirtyBlocks);
        }
        else
        {
            // Add the dirty blocks read to already queued changes
            if (NULL == pVolumeContext->DirtyBlocksList)
            {
                pVolumeContext->LastDirtyBlocks = pDirtyBlocks;
            }
            
            pDirtyBlocks->uliTransactionID.QuadPart = pVolumeContext->uliTransactionId.QuadPart++;
            pDirtyBlocks->next = pVolumeContext->DirtyBlocksList;
            pVolumeContext->DirtyBlocksList = pDirtyBlocks;    
            pVolumeContext->lDirtyBlocksInQueue++;
            pVolumeContext->ulTotalChangesPending += pDirtyBlocks->cChanges;
            pVolumeContext->uliChangesReadFromBitMap.QuadPart += pDirtyBlocks->cChanges;
            pVolumeContext->ulicbChangesReadFromBitMap.QuadPart += pDirtyBlocks->ulicbChanges.QuadPart;

            bClearBitsRead = TRUE;

            // reset the work item type fromr ead to write.
            pDBworkItemForClearingBits->eDBworkItemType = ecDBworkItemBitMapWrite;
            InsertTailList(&pVolumeContext->HeadForWriteDBworkItems, &pDBworkItemForClearingBits->DSDBCWriteEntry);

            if (STATUS_SUCCESS == pBitRuns->finalStatus) {
                // This is the last read.
                pVolumeContext->eBitMapReadState = ecBitMapReadCompleted;
            } else if ((0 != DriverContext.DBLowWaterMarkWhileServiceRunning) && 
                (pVolumeContext->ulTotalChangesPending >=  DriverContext.DBLowWaterMarkWhileServiceRunning))
            {
                // pBitRuns->finalStatus is STATUS_MORE_PROCESSING_REQUIRED.
                // If this is not a final read, and reached the low water mark pause the reads.
                pVolumeContext->eBitMapReadState = ecBitMapReadPaused;
            } else {
                bContinueBitMapRead = TRUE;
            }
        }

        KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);

    } while (0);

    if (TRUE == bContinueBitMapRead) {
        pDBworkItemForContinuingRead = AllocateDirtyBlockWorkItem();
        if (NULL != pDBworkItemForContinuingRead) {
            ReferenceVolumeContext(pVolumeContext);
            pDBworkItemForContinuingRead->pVolumeContext = pVolumeContext;
            pDBworkItemForContinuingRead->eDBworkItemType = ecDBworkItemBitMapContinueRead;
        } else {
            pVolumeContext->eBitMapReadState = ecBitMapReadProcessingError;
        }
    }

    if (TRUE == bClearBitsRead) {
        // Do not have to set pBitRuns and pBitRuns->context1 as they are already set when it is received
        // pBitRuns = &pDBworkItemForClearingBits->BitRuns;
        // pBitRuns->context1 = pDBworkItemForClearingBits; 
        ASSERT(pBitRuns = &pDBworkItemForClearingBits->BitRuns);
        ASSERT(pBitRuns->context1 = pDBworkItemForClearingBits); 
        pBitRuns->completionCallback = WriteBitMapRunsCompletionCallback;
        pVolumeContext->pBitMapFile->ClearBits(pBitRuns);
    } else {
        DereferenceDirtyBlockWorkItem(pDBworkItemForClearingBits);
    }


    if (NULL != pDBworkItemForContinuingRead) {
        pBitRuns = &pDBworkItemForContinuingRead->BitRuns;
        pBitRuns->context1 = pDBworkItemForContinuingRead;
        pBitRuns->completionCallback = ReadBitMapRunsCompletionCallback;
        pVolumeContext->pBitMapFile->GetNextRuns(pBitRuns);
    }

    return;
}

/*-----------------------------------------------------------------------------
 * Function : ReadBitMapRunsCompletionCallback
 * Parameters : BitRuns - This indicates the changes that have been read
 *              to bit map
 * NOTES :
 * This function is called after read completion. This function can be 
 * called at DISPACH_LEVEL. If the function is called at DISPACH_LEVEL a
 * worker routine is queued to process the completion at PASSIVE_LEVE
 * If this function is called at PASSIVE_LEVEL, the process function is 
 * called directly with out queuing worker routine.
 *
 *-----------------------------------------------------------------------------------
 */
VOID
ReadBitMapRunsCompletionCallback(BitRuns *pBitRuns)
{
    PWORK_QUEUE_ENTRY   pWorkQueueEntry;

    if (PASSIVE_LEVEL == KeGetCurrentIrql()) {
        ReadBitMapRunsCompletion(pBitRuns);
    } else {
        pWorkQueueEntry = AllocateWorkQueueEntry();
        
        if (NULL != pWorkQueueEntry) {
            pWorkQueueEntry->Context = pBitRuns;
            pWorkQueueEntry->WorkerRoutine = ReadBitMapRunsCompletionWorkerRoutine;

            AddItemToWorkQueue(&DriverContext.WorkItemQueue, pWorkQueueEntry);
            return;
        }
    }
    return;
}

/*-----------------------------------------------------------------------------
 * Function : ReadBitMapRunsCompletionWorkerRoutine
 * Parameters : BitRuns - This indicates the changes that have been read
 *              to bit map
 * NOTES :
 * This function is worker routine and queued if completion routine is called
 * at DISPATCH_LEVEL. This function calls the ReadBitMapRunsCompletion funciton
 *
 *-----------------------------------------------------------------------------------
 */
VOID
ReadBitMapRunsCompletionWorkerRoutine(PWORK_QUEUE_ENTRY    pWorkQueueEntry)
{
    BitRuns         *pBitRuns;
    PDB_WORK_ITEM   pDBworkItem;
    PVOLUME_CONTEXT pVolumeContext;
    KIRQL           OldIrql;
#if DBG
    PLIST_ENTRY     pEntry;
    BOOLEAN         bFound;
#endif // DBG

    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    // Holding reference to VolumeContext so that BitMap File is not closed before the writes are completed.
    pBitRuns = (BitRuns *)pWorkQueueEntry->Context;
    DereferenceWorkQueueEntry(pWorkQueueEntry);

    ReadBitMapRunsCompletion(pBitRuns);

    return;
}


typedef enum _etBitMapReadState
{
    ecBitMapReadNotInitialized = 0,
    ecBitMapReadNotStarted = 1,
    ecBitMapReadInProgress = 2,
    ecBitMapReadPaused = 3,
    ecBitMapReadCompleted = 4,
    ecBitMapReadStopped = 5,
    ecBitMapReadReadError = 6,
    ecBitMapReadProcessingError = 7
} etBitMapReadState, *petBitMapReadState;

/*-----------------------------------------------------------------------------
 * Function Name : AllocateDBworkItemForWrite
 * Parameters :
 *      pVolumeContext : This parameter is referenced and stored in the 
 *                            work item.
 *      ulNumDirtyChanges   : if this value is zero all the dirty changes are 
 *                            copied. If this value is non zero, minimum of 
 *                            ulNumDirtyChanges are copied.
 *      ListHead            : pointer to list head. 
 * Retruns :
 *      BOOLEAN             : TRUE if succeded in creating work item for write
 *                            FALSE if failed in creating work item
 * NOTES :
 * This function is called at DISPATCH_LEVEL. This function is called with 
 * Spinlock to DirtyBlockContext held.
 *
 *      This function removes changes from device specific dirty block context
 * and creates dirty block work item for each dirty block. All the dirty block
 * work items are added to list head. These work items are later removed and
 * processed by ProcessVCWorkItem function
 *      
 *
 *--------------------------------------------------------------------------------------------
 */
BOOLEAN
AllocateDBworkItemForWrite(
    PVOLUME_CONTEXT     pVolumeContext,
    ULONG               ulNumDirtyChanges,
    PLIST_ENTRY         ListHead
    )
{
    PDB_WORK_ITEM   pDBworkItem = NULL;
    PDIRTY_BLOCKS   pDirtyBlocks = NULL;
    ULONG           ulChangesCopied = 0;

    while ((0 != pVolumeContext->ulTotalChangesPending) &&
        (NULL != pVolumeContext->DirtyBlocksList))
    {
        // There are changes to be copied, so lets create Dirty Block work items.
        if (ulNumDirtyChanges && (ulChangesCopied >= ulNumDirtyChanges))
            break;

        pDBworkItem = AllocateDirtyBlockWorkItem();
        if (NULL != pDBworkItem) {
            pDirtyBlocks = pVolumeContext->DirtyBlocksList;
            pVolumeContext->DirtyBlocksList = pDirtyBlocks->next;
            if (pDirtyBlocks == pVolumeContext->LastDirtyBlocks)
                pVolumeContext->LastDirtyBlocks = NULL;
            pDirtyBlocks->next = NULL;
            pVolumeContext->lDirtyBlocksInQueue--;
            pVolumeContext->ulTotalChangesPending -= pDirtyBlocks->cChanges;

            pDBworkItem->ulChanges = pDirtyBlocks->cChanges;
            pDBworkItem->ulicbChangeData.QuadPart = pDirtyBlocks->ulicbChanges.QuadPart;

            pVolumeContext->ulicbChangesQueuedForWriting.QuadPart += pDBworkItem->ulicbChangeData.QuadPart;
            pVolumeContext->ulChangesQueuedForWriting += pDBworkItem->ulChanges;
            ulChangesCopied += pDirtyBlocks->cChanges;

            ReferenceVolumeContext(pVolumeContext);
            pDBworkItem->pVolumeContext = pVolumeContext;
            pDBworkItem->eDBworkItemType = ecDBworkItemBitMapWrite;
            pDBworkItem->pDirtyBlocks = pDirtyBlocks;
            InsertTailList(ListHead, &pDBworkItem->ListEntry);
            InsertTailList(&pVolumeContext->HeadForWriteDBworkItems, &pDBworkItem->DSDBCWriteEntry);
        }
        else
        {
            // There is nothing much we can do, 
            // May be free memory as we can not write these structures any way?
            return FALSE;
        }
    }

    return TRUE;
}

VOID
WriteBitMapRunsCompletion(BitRuns *pBitRuns)
{
    PDB_WORK_ITEM   pDBworkItem;
    PVOLUME_CONTEXT pVolumeContext;
    KIRQL           OldIrql;
#if DBG
    PLIST_ENTRY     pEntry;
    BOOLEAN         bFound;
#endif // DBG

    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    // Holding reference to VolumeContext so that BitMap File is not closed before the writes are completed.
    pDBworkItem = (PDB_WORK_ITEM)pBitRuns->context1;
    pVolumeContext = pDBworkItem->pVolumeContext;

    if (STATUS_SUCCESS != pBitRuns->finalStatus) {
        SetVolumeOutOfSync(pVolumeContext, ERROR_TO_REG_BITMAP_WRITE_ERROR);
        InterlockedIncrement(&pVolumeContext->lNumBitMapWriteErrors);
    }

    KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
#if DBG
    pEntry = pVolumeContext->HeadForWriteDBworkItems.Flink;
    bFound = FALSE;
    while (pEntry != &pVolumeContext->HeadForWriteDBworkItems)
    {
        if (pEntry == &pDBworkItem->DSDBCWriteEntry) {
            bFound = TRUE;
            break;
        }
        pEntry = pEntry->Flink;
    }
    ASSERT(TRUE == bFound);
#endif // DBG
    pVolumeContext->ulChangesQueuedForWriting -= pDBworkItem->ulChanges;
    pVolumeContext->uliChangesWrittenToBitMap.QuadPart += pDBworkItem->ulChanges;
    pVolumeContext->ulicbChangesQueuedForWriting.QuadPart -= pDBworkItem->ulicbChangeData.QuadPart;
    pVolumeContext->ulicbChangesWrittenToBitMap.QuadPart += pDBworkItem->ulicbChangeData.QuadPart;

    RemoveEntryList(&pDBworkItem->DSDBCWriteEntry);
    if ((ecDSDBCdeviceShutdown == pVolumeContext->eDSDBCdeviceState) &&
        (IsListEmpty(&pVolumeContext->HeadForWriteDBworkItems)))
    {
        KeSetEvent(&pVolumeContext->WriteListEmptyNotification, IO_NO_INCREMENT, FALSE);
    }

    KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);
    DereferenceDirtyBlockWorkItem(pDBworkItem);
    return;
}

/*---------------------------------------------------------------------------
 * Function Name : WriteBitMapRunsCompletionWorkerRoutine
 * Parameters :
 *      pWorkQueueEntry : pointer to work queue entry, Context member has 
 *              pointer to BitRuns. BitRuns has the write meta data/changes 
 *              we requested to write in bit map.
 * NOTES :
 * This function is called at PASSIVE_LEVEL. This function is worker routine
 * called by worker thread at PASSIVE_LEVEL. This worker routine is scheduled
 * for execution by write completion routine.
 *
 *      This function removes queued write work items in device specific dirty
 * block context and derefences the dirty block context.
 *
 *-----------------------------------------------------------------------------
 */
// This function is called at PASSIVE_LEVEL
VOID
WriteBitMapRunsCompletionWorkerRoutine(PWORK_QUEUE_ENTRY    pWorkQueueEntry)
{
    BitRuns         *pBitRuns;

    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    // Holding reference to VolumeContext so that BitMap File is not closed 
    // before the writes are completed.
    pBitRuns = (BitRuns *)pWorkQueueEntry->Context;
    DereferenceWorkQueueEntry(pWorkQueueEntry);

    WriteBitMapRunsCompletion(pBitRuns);

    return;
}

/*-----------------------------------------------------------------------------------
 * Function : WriteBitMapRunsCompletionCallback
 * Parameters : BitRuns - This indicates the changes that have been writen
 *              to bit map
 * NOTES :
 * This function is called after write completion. This function can be 
 * called at DISPACH_LEVEL. This function queues a worker routine to continue
 * the post write operation at PASSIVE_LEVEL. 
 *
 *-----------------------------------------------------------------------------------
 */
VOID
WriteBitMapRunsCompletionCallback(BitRuns *pBitRuns)
{
    PWORK_QUEUE_ENTRY   pWorkQueueEntry;

    if (PASSIVE_LEVEL == KeGetCurrentIrql()) {
        WriteBitMapRunsCompletion(pBitRuns);
    } else {
        // As this function can be called at dispatch level we can not dereference Dirtyblocks context
        // which would end up calling close.
        pWorkQueueEntry = AllocateWorkQueueEntry();
        if (NULL != pWorkQueueEntry)
        {
            pWorkQueueEntry->Context = pBitRuns;
            pWorkQueueEntry->WorkerRoutine = WriteBitMapRunsCompletionWorkerRoutine;

            AddItemToWorkQueue(&DriverContext.WorkItemQueue, pWorkQueueEntry);
        }
    }

    return;
}

/*-------------------------------------------------------------------------------------------
    Second level functions to allocate DB_WORK_ITEM for read and write methods.
 *-------------------------------------------------------------------------------------------
 */
BOOLEAN
AllocateDBworkItemForDerefDBcontext(
    PVOLUME_CONTEXT   pVolumeContext,
    PLIST_ENTRY                             ListHead
    )
{
    PDB_WORK_ITEM   pDBworkItem = NULL;

    pDBworkItem = AllocateDirtyBlockWorkItem();
    if (pDBworkItem) {
        ReferenceVolumeContext(pVolumeContext);
        pDBworkItem->pVolumeContext = pVolumeContext;
        pDBworkItem->eDBworkItemType = ecDBworkItemDerefDBcontext;
        InsertTailList(ListHead, &pDBworkItem->ListEntry);
    } else {
        return FALSE;
    }

    return TRUE;
}

/*---------------------------------------------------------------------------------------------
    Functions related to Allocate, Deallocate, Reference and Dereference Dirty Block Work Items
 *---------------------------------------------------------------------------------------------
 */
VOID
DeallocateDirtyBlockWorkItem(
    PDB_WORK_ITEM   pDBworkItem
    )
{
    ASSERT(0 == pDBworkItem->lRefCount);
    
    if (NULL != pDBworkItem->pVolumeContext) {
        DereferenceVolumeContext(pDBworkItem->pVolumeContext);
        pDBworkItem->pVolumeContext = NULL;
    }

    ExFreeToNPagedLookasideList(&DriverContext.DBworkItemPool, pDBworkItem);
    return;
}

PDB_WORK_ITEM
AllocateDirtyBlockWorkItem()
{
    PDB_WORK_ITEM   pDBworkItem;

    pDBworkItem = (PDB_WORK_ITEM)ExAllocateFromNPagedLookasideList(&DriverContext.DBworkItemPool);

    if(pDBworkItem)
    {
        pDBworkItem->ListEntry.Flink = NULL;
        pDBworkItem->ListEntry.Blink = NULL;
        pDBworkItem->DSDBCWriteEntry.Flink = NULL;
        pDBworkItem->DSDBCWriteEntry.Blink = NULL;
        pDBworkItem->lRefCount = 1;
        pDBworkItem->eDBworkItemType = ecDBworkItemNotInitialized;
        pDBworkItem->pVolumeContext = NULL;
        pDBworkItem->pDirtyBlocks = NULL;
        pDBworkItem->ulChanges = 0;
        pDBworkItem->ulicbChangeData.QuadPart = 0;
        pDBworkItem->ulReserved = 0;
    }

    return pDBworkItem;
}

VOID
ReferenceDirtyBlockWorkItem(PDB_WORK_ITEM pDBworkItem)
{
    ASSERT(pDBworkItem->lRefCount >= 1);

    InterlockedIncrement(&pDBworkItem->lRefCount);

    return;
}

VOID
DereferenceDirtyBlockWorkItem(PDB_WORK_ITEM pDBworkItem)
{
    ASSERT(pDBworkItem->lRefCount >= 1);

    if (0 == InterlockedDecrement(&pDBworkItem->lRefCount))
    {
        DeallocateDirtyBlockWorkItem(pDBworkItem);
    }

    return;
}

typedef enum _etDBworkItemType
{
    ecDBworkItemNotInitialized = 0,
    ecDBworkItemBitMapStartRead = 2,
    ecDBworkItemBitMapContinueRead = 3,
    ecDBworkItemBitMapWrite = 4,
    ecDBworkItemDerefDBcontext = 5
} etDBworkItemType, *petDBworkItemType;

typedef struct _DB_WORK_ITEM
{
    LIST_ENTRY              ListEntry;
    LIST_ENTRY              DSDBCWriteEntry;
    LONG                    lRefCount;
    etDBworkItemType        eDBworkItemType;
    ULONG                   ulChanges;
    ULONG                   ulReserved;
    ULARGE_INTEGER          ulicbChangeData;
    PVOLUME_CONTEXT         pVolumeContext;
    DIRTYBLOCKS*            pDirtyBlocks;
    BitRuns                 BitRuns;
} DB_WORK_ITEM, *PDB_WORK_ITEM;



