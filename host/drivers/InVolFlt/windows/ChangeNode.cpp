#include "Common.h"
#include "ChangeNode.h"
#include "TagNode.h"
#include "VContext.h"

/**
 * InitializeChangeList
 */
void
InitializeChangeList(PCHANGE_LIST   pList)
{
    ASSERT(NULL != pList);

    RtlZeroMemory(pList, sizeof(CHANGE_LIST));
    InitializeListHead(&pList->Head);
    InitializeListHead(&pList->TempQueueHead);
    InitializeListHead(&pList->DataCaptureModeHead);

    return;
}

void
DeductChangeCountersOnDataSource(PCHANGE_LIST pList, ULONG ulDataSource, ULONG ulChanges, ULONGLONG ullcbChanges)
{
    ASSERT(NULL != pList);
    switch (ulDataSource) {
    case INFLTDRV_DATA_SOURCE_DATA:
        ASSERT((pList->ulDataChangesPending >= ulChanges) && (pList->ullcbDataChangesPending >= ullcbChanges));
        pList->ulDataChangesPending -= ulChanges;
        pList->ullcbDataChangesPending -= ullcbChanges;
        ASSERT((0 != pList->ulDataChangesPending) || (0 == pList->ullcbDataChangesPending));
        ASSERT((0 != pList->ullcbDataChangesPending) || (0 == pList->ulDataChangesPending));
        break;
    case INFLTDRV_DATA_SOURCE_META_DATA:
        ASSERT((pList->ulMetaDataChangesPending >= ulChanges) && (pList->ullcbMetaDataChangesPending >= ullcbChanges));
        pList->ulMetaDataChangesPending -= ulChanges;
        pList->ullcbMetaDataChangesPending -= ullcbChanges;
        ASSERT((0 != pList->ulMetaDataChangesPending) || (0 == pList->ullcbMetaDataChangesPending));
        ASSERT((0 != pList->ullcbMetaDataChangesPending) || (0 == pList->ulMetaDataChangesPending));
        break;
    case INFLTDRV_DATA_SOURCE_BITMAP:
        ASSERT((pList->ulBitmapChangesPending >= ulChanges) && (pList->ullcbBitmapChangesPending >= ullcbChanges));
        pList->ulBitmapChangesPending -= ulChanges;
        pList->ullcbBitmapChangesPending -= ullcbChanges;
        ASSERT((0 != pList->ulBitmapChangesPending) || (0 == pList->ullcbBitmapChangesPending));
        ASSERT((0 != pList->ullcbBitmapChangesPending) || (0 == pList->ulBitmapChangesPending));
        break;
    default:
        ASSERTMSG(0, "DeductChangeCountersOnDataSource: DataSource did not match any");
        break;
    }

    pList->ulTotalChangesPending -= ulChanges;
    pList->ullcbTotalChangesPending -= ullcbChanges;
    ASSERT((0 != pList->ulTotalChangesPending) || (0 == pList->ullcbTotalChangesPending));
    ASSERT((0 != pList->ullcbTotalChangesPending) || (0 == pList->ulTotalChangesPending));

    return;
}

void
AddChangeCountersOnDataSource(PCHANGE_LIST pList, ULONG ulDataSource, ULONG ulChanges, ULONGLONG ullcbChanges)
{
    ASSERT(NULL != pList);
    switch (ulDataSource) {
    case INFLTDRV_DATA_SOURCE_DATA:
        pList->ulDataChangesPending += ulChanges;
        pList->ullcbDataChangesPending += ullcbChanges;
        break;
    case INFLTDRV_DATA_SOURCE_META_DATA:
        pList->ulMetaDataChangesPending += ulChanges;
        pList->ullcbMetaDataChangesPending += ullcbChanges;
        break;
    case INFLTDRV_DATA_SOURCE_BITMAP:
        pList->ulBitmapChangesPending += ulChanges;
        pList->ullcbBitmapChangesPending += ullcbChanges;
        break;
    default:
        ASSERTMSG(0, "AddChangeCountersOnDataSource: DataSource did not match any");
        break;
    }

    pList->ulTotalChangesPending += ulChanges;
    pList->ullcbTotalChangesPending += ullcbChanges;

    return;
}

void
FreeChangeNodeList(PCHANGE_LIST pList, etTagStateTriggerReason tagStateReason)
{
    ASSERT(NULL != pList);

    while(!IsListEmpty(&pList->TempQueueHead)) {
        //Extract in LIFO order and insert from the head in the main list
        InsertHeadList(&pList->Head, RemoveTailList(&pList->TempQueueHead));
    }

    while (!IsListEmpty(&pList->Head)) {
        PCHANGE_NODE    ChangeNode = (PCHANGE_NODE)RemoveHeadList(&pList->Head);

        if (pList->CurrentNode == ChangeNode) {
            pList->CurrentNode = NULL;
        }

        if (ChangeNode->DataCaptureModeListEntry.Flink != NULL &&
            ChangeNode->DataCaptureModeListEntry.Blink != NULL) {
#if DBG
                PKDIRTY_BLOCK DirtyBlock = ChangeNode->DirtyBlock;
                ASSERT(DirtyBlock->eWOState != ecWriteOrderStateData && DirtyBlock->ulDataSource == INFLTDRV_DATA_SOURCE_DATA);
                ASSERT(!IsListEmpty(&ChangeNode->DirtyBlock->DevContext->ChangeList.DataCaptureModeHead));
#endif
                RemoveEntryList(&ChangeNode->DataCaptureModeListEntry);
                ChangeNode->DataCaptureModeListEntry.Flink = NULL;
                ChangeNode->DataCaptureModeListEntry.Blink = NULL;
                if (IsListEmpty(&pList->Head)) {
                    ASSERT(IsListEmpty(&ChangeNode->DirtyBlock->DevContext->ChangeList.DataCaptureModeHead));
                    ASSERT(pList->CurrentNode == NULL);
                }
        }

        switch (ChangeNode->eChangeNode) {
            case ecChangeNodeDataFile:
            case ecChangeNodeDirtyBlock:
            {
                PKDIRTY_BLOCK   DirtyBlock = ChangeNode->DirtyBlock;
    
                DeductChangeCountersOnDataSource(pList, DirtyBlock->ulDataSource,
                                                 DirtyBlock->cChanges, DirtyBlock->ulicbChanges.QuadPart);

                if (DirtyBlock->ulFlags & KDIRTY_BLOCK_FLAG_CONTAINS_TAGS) {
                    ASSERT(NULL != DirtyBlock->DevContext);
                    DirtyBlock->DevContext->ulNumberOfTagPointsDropped++;

                     //If the tag is in pending(Any consistency protocol is in progress), tag can be dropped for any reason
                     //If the tag is revoked, telemetry is logged immediately though tag DB is removed later in the drain path
                     //Let's not log those dirtyblocks which are marked revoke to avoid double logging
                    if (!TEST_FLAG(DirtyBlock ->ulFlags, KDIRTY_BLOCK_FLAG_REVOKE_TAGS))
                        InDskFltTelemetryLogTagHistory(DirtyBlock, DirtyBlock->DevContext, ecTagStatusDropped, tagStateReason, ecMsgTagDropped);

                    if (DirtyBlock->ulFlags & KDIRTY_BLOCK_FLAG_TAG_WAITING_FOR_COMMIT) {
                        PTAG_NODE TagNode = AddStatusAndReturnNodeIfComplete(DirtyBlock->TagGUID, DirtyBlock->ulTagDevIndex, ecTagStatusDeleted);
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
                }

                ASSERT(pList->lDirtyBlocksInQueue > 0);
                pList->lDirtyBlocksInQueue--;
                if (pList->lDirtyBlocksInQueue == 0) {
                    ASSERT(IsListEmpty(&ChangeNode->DirtyBlock->DevContext->ChangeList.DataCaptureModeHead));
                    ASSERT(pList->CurrentNode == NULL);
                    ASSERT(IsListEmpty(&pList->Head));
                }
                if (DirtyBlock->eWOState == ecWriteOrderStateData &&
                        DirtyBlock->ulDataSource == INFLTDRV_DATA_SOURCE_DATA) {
                            // This ChangeNode is going away, we assume this as drain condition.
                            DirtyBlock->DevContext->NumChangeNodeInDataWOSDrained++;
                }                
    
                QueueWorkerRoutineForChangeNodeCleanup(ChangeNode);
                break;
            }
            default:
            ASSERTMSG(0, "Unhandled message type");
                break;
        }

        DereferenceChangeNode(ChangeNode);
    }

    ASSERT((0 == pList->ulTotalChangesPending) && (0 == pList->ullcbTotalChangesPending));
    ASSERT((0 == pList->ulDataChangesPending) && (0 == pList->ullcbDataChangesPending));
    ASSERT((0 == pList->ulMetaDataChangesPending) && (0 == pList->ullcbMetaDataChangesPending));
    ASSERT((0 == pList->ulBitmapChangesPending) && (0 == pList->ullcbBitmapChangesPending));
    ASSERT(0 == pList->lDirtyBlocksInQueue);
}

VOID
ChangeNodeCleanup(PCHANGE_NODE ChangeNode)
{
    PAGED_CODE();

    PKDIRTY_BLOCK       DirtyBlock = ChangeNode->DirtyBlock;
    PDEV_CONTEXT     DevContext = DirtyBlock->DevContext;
    NTSTATUS            Status;

    // Keep the change node, as we are acquiring the mutex
    ReferenceChangeNode(ChangeNode);
    KeWaitForSingleObject(&ChangeNode->Mutex, Executive, KernelMode, FALSE, NULL);

    switch (ChangeNode->eChangeNode) {
    case ecChangeNodeDirtyBlock:
        if(ChangeNode->ulFlags & CHANGE_NODE_FLAGS_QUEUED_FOR_DATA_WRITE) {
            KIRQL   OldIrql;

            KeAcquireSpinLock(&DevContext->Lock, &OldIrql);
            DevContext->lDataBlocksQueuedToFileWrite -= ChangeNode->DirtyBlock->ulDataBlocksAllocated;
            KeReleaseSpinLock(&DevContext->Lock, OldIrql);
        }
        
        ChangeNode->ulFlags &= ~CHANGE_NODE_FLAGS_QUEUED_FOR_DATA_WRITE; 
        break;
    case ecChangeNodeDataFile:
        InVolDbgPrint(DL_TRACE_L1, DM_DATA_FILTERING,
                      ("%s: Deleting file %wZ\n", __FUNCTION__, &DirtyBlock->FileName));

        // File has to be deleted here.
        Status = CDataFile::DeleteFile(&DirtyBlock->FileName);
        if (!NT_SUCCESS(Status)) {
            InDskFltWriteEventWithDevCtxt(INDSKFLT_WARN_DELETE_FILE_FAILED,
                DevContext, DirtyBlock->FileName.Buffer, Status, SourceLocationChangeNodeCleanup);
        }

        KeWaitForSingleObject(&DevContext->Mutex, Executive, KernelMode, FALSE, NULL);
        ASSERT(DevContext->ullcbDataWrittenToDisk >= DirtyBlock->ullFileSize);
        DevContext->ullcbDataWrittenToDisk -= DirtyBlock->ullFileSize;
        DevContext->ChangeList.ulDataFilesPending--;
        KeReleaseMutex(&DevContext->Mutex, FALSE);
        break;
    default:
        ASSERTMSG(0, "Unhandled message type");
        break;
    }

    ASSERT(!(DirtyBlock->ulFlags & KDIRTY_BLOCK_FLAG_TAG_WAITING_FOR_COMMIT));

    DeallocateDirtyBlock(DirtyBlock, false);
    KeReleaseMutex(&ChangeNode->Mutex, FALSE);
    DereferenceChangeNode(ChangeNode);

    return;
}

PCHANGE_NODE
AllocateChangeNode()
{
    InVolDbgPrint(DL_FUNC_TRACE, DM_DATA_FILTERING, ("%s: Called\n", __FUNCTION__));

    if (MaxNonPagedPoolLimitReached()) {
        InVolDbgPrint(DL_WARNING, DM_DATA_FILTERING, 
                      ("%s: Allocation failed as NonPagedPoolAlloc(%#x) reached MaxNonPagedLimit(%#x)\n", 
                       __FUNCTION__, DriverContext.lNonPagedMemoryAllocated, DriverContext.ulMaxNonPagedPool));
        return NULL;
    }

    PCHANGE_NODE    Node = (PCHANGE_NODE)ExAllocateFromNPagedLookasideList(&DriverContext.ChangeNodePool);

    if (Node) {
        ULONG ulSize = (ULONG)sizeof(CHANGE_NODE);

        RtlZeroMemory(Node, sizeof(CHANGE_NODE));
        //PreAllocation of WorkQueueEntry for cleanup of ChangeNode
        Node->pCleanupWorkQueueEntry = AllocateWorkQueueEntry();
        //Cleanup if Allocation of WorkQueuEnty fail
        if (NULL == Node->pCleanupWorkQueueEntry) {
            ExFreeToNPagedLookasideList(&DriverContext.ChangeNodePool, Node);
            return NULL;
        }
        Node->lRefCount = 0x01;
        KeInitializeMutex(&Node->Mutex, 0);
        InterlockedIncrement(&DriverContext.lChangeNodesAllocated);
        InterlockedExchangeAdd(&DriverContext.lNonPagedMemoryAllocated, ulSize);
        Node->DataCaptureModeListEntry.Flink = NULL;
        Node->DataCaptureModeListEntry.Blink = NULL;
    } else {
        InVolDbgPrint(DL_WARNING, DM_DATA_FILTERING, ("%s: Allocation failed\n", __FUNCTION__));
    }

    return Node;
}

void
DeallocateChangeNode(PCHANGE_NODE Node)
{
    LONG lSize = (LONG)sizeof(CHANGE_NODE);
    ASSERT(0 == Node->lRefCount);

    ExFreeToNPagedLookasideList(&DriverContext.ChangeNodePool, Node);
    InterlockedDecrement(&DriverContext.lChangeNodesAllocated);
    InterlockedExchangeAdd(&DriverContext.lNonPagedMemoryAllocated, -lSize);
    ASSERT(DriverContext.lNonPagedMemoryAllocated >= 0);
}

PCHANGE_NODE
ReferenceChangeNode(PCHANGE_NODE Node)
{
    ASSERT(Node->lRefCount >= 0x01);
    InterlockedIncrement(&Node->lRefCount);

    return Node;
}

void
DereferenceChangeNode(PCHANGE_NODE Node)
{
    ASSERT(Node->lRefCount >= 0x01);
    if (0 == InterlockedDecrement(&Node->lRefCount)) {
    	ASSERT(NULL != Node->pCleanupWorkQueueEntry);
    	ASSERT(0 == Node->bCleanupWorkQueueRef);
    	//Free WorkQueueEntry
        DereferenceWorkQueueEntry(Node->pCleanupWorkQueueEntry);
        DeallocateChangeNode(Node);
    }
}
