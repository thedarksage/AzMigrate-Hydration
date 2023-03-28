#include "Common.h"
#include "WorkQueue.h"

/*----------------------------------------------------------------------------------------------------------------
    WorkQueueEntry Functions
 *----------------------------------------------------------------------------------------------------------------
 */

VOID
DeallocateWorkQueueEntry(PWORK_QUEUE_ENTRY  pWorkQueueEntry)
{
    ASSERT(pWorkQueueEntry->lRefCount <= 0x00);
    ExFreeToNPagedLookasideList(&DriverContext.WorkQueueEntriesPool, pWorkQueueEntry);

    return;
}

PWORK_QUEUE_ENTRY
AllocateWorkQueueEntry()
{
    PWORK_QUEUE_ENTRY   pWorkQueueEntry;

    pWorkQueueEntry = (PWORK_QUEUE_ENTRY)ExAllocateFromNPagedLookasideList(&DriverContext.WorkQueueEntriesPool);
    if (pWorkQueueEntry) {
        InitializeWorkQueueEntry(pWorkQueueEntry);
    } else {
        InVolDbgPrint(DL_ERROR, DM_MEM_TRACE, ("AllocateWorkQueueEntry: failed in allocating\n"));
    }

    return pWorkQueueEntry;
}

VOID
InitializeWorkQueueEntry(PWORK_QUEUE_ENTRY pWorkQueueEntry) {
    RtlZeroMemory(pWorkQueueEntry, sizeof(WORK_QUEUE_ENTRY));
    pWorkQueueEntry->lRefCount = 1;
}

VOID
ReferenceWorkQueueEntry(PWORK_QUEUE_ENTRY   pWorkQueueEntry)
{
    ASSERT(pWorkQueueEntry->lRefCount >= 0x01);

    InterlockedIncrement(&pWorkQueueEntry->lRefCount);

    return;
}

VOID
DereferenceWorkQueueEntry(PWORK_QUEUE_ENTRY pWorkQueueEntry)
{
    ASSERT(pWorkQueueEntry->lRefCount >= 0x01);

    if (InterlockedDecrement(&pWorkQueueEntry->lRefCount) <= 0x00)
    {
        DeallocateWorkQueueEntry(pWorkQueueEntry);
    }

    return;
}

VOID
GenericWorkerThreadRoutine(PVOID pContext);

NTSTATUS
SetWorkQueueThreadPriority(PWORK_QUEUE pWorkQueue, ULONG ulPriority)
{
    NTSTATUS    Status;

    if ((0 == ulPriority) || (NULL == pWorkQueue->ThreadObject)) {
        return STATUS_SUCCESS;
    }

    if (ulPriority == pWorkQueue->ulThreadPriority) {
        return STATUS_SUCCESS;
    }

    Status = KeSetPriorityThread(pWorkQueue->ThreadObject, ulPriority);
    if (NT_SUCCESS(Status)) {
        pWorkQueue->ulThreadPriority = ulPriority;
    }

    return Status;
}

NTSTATUS
InitializeWorkQueue(PWORK_QUEUE pWorkQueue, PKSTART_ROUTINE WorkerThreadRoutine, ULONG ulPriority)
{
    NTSTATUS    Status = STATUS_SUCCESS;
    HANDLE      hThread;

    ASSERT(NULL != pWorkQueue);

    if (NULL == pWorkQueue)
        return Status;

    if (NULL == WorkerThreadRoutine)
        WorkerThreadRoutine = GenericWorkerThreadRoutine;

    RtlZeroMemory(pWorkQueue, sizeof(WORK_QUEUE));

    InitializeListHead(&pWorkQueue->WorkerQueueHead);
    KeInitializeSpinLock(&pWorkQueue->Lock);
    KeInitializeEvent(&pWorkQueue->WakeEvent, NotificationEvent, FALSE);
    KeInitializeEvent(&pWorkQueue->ShutdownEvent, NotificationEvent, FALSE);
    
    pWorkQueue->WorkerThreadRoutine = WorkerThreadRoutine;

    Status = PsCreateSystemThread(&hThread, THREAD_ALL_ACCESS, NULL, NULL, NULL, WorkerThreadRoutine, (PVOID)pWorkQueue);
    if (NT_SUCCESS(Status))
    {
        Status = ObReferenceObjectByHandle(hThread, SYNCHRONIZE, NULL, KernelMode, (PVOID *)&(pWorkQueue->ThreadObject), NULL);
        if (NT_SUCCESS(Status))
        {
            SetWorkQueueThreadPriority(pWorkQueue, ulPriority);
            ZwClose(hThread);
        }
    }

    return Status;
}

NTSTATUS
CleanWorkQueue(PWORK_QUEUE pWorkQueue)
{
    KIRQL       OldIrql;

    KeAcquireSpinLock(&pWorkQueue->Lock, &OldIrql);
    pWorkQueue->ulFlags |= WQ_FLAGS_THREAD_SHUTDOWN;
    KeSetEvent(&pWorkQueue->ShutdownEvent, IO_DISK_INCREMENT, FALSE);
    KeReleaseSpinLock(&pWorkQueue->Lock, OldIrql);

    KeWaitForSingleObject(pWorkQueue->ThreadObject, Executive, KernelMode, FALSE, NULL);
    ObDereferenceObject(pWorkQueue->ThreadObject);
    pWorkQueue->ThreadObject = NULL;
    
    return STATUS_SUCCESS;
}

NTSTATUS
AddItemToWorkQueue(PWORK_QUEUE pWorkQueue, PWORK_QUEUE_ENTRY pEntry)
{
    KIRQL       OldIrql;
    NTSTATUS    Status = STATUS_SUCCESS;

    KeAcquireSpinLock(&pWorkQueue->Lock, &OldIrql);
    if (0x00 == (pWorkQueue->ulFlags & WQ_FLAGS_THREAD_SHUTDOWN))
    {
        InsertTailList(&pWorkQueue->WorkerQueueHead, (PLIST_ENTRY)pEntry);
        KeSetEvent(&pWorkQueue->WakeEvent, IO_DISK_INCREMENT, FALSE);
    }
    else
        Status = STATUS_UNSUCCESSFUL;

    KeReleaseSpinLock(&pWorkQueue->Lock, OldIrql);
    
    return Status;
}

VOID
GenericWorkerThreadRoutine(PVOID pContext)
{
    PWORK_QUEUE pWorkQueue = (PWORK_QUEUE)pContext;
    PVOID       WaitObjects[2];
    NTSTATUS    Status;
    BOOLEAN     bShutdownThread = FALSE;
    KIRQL       OldIrql;
    LIST_ENTRY  WorkQueueItemHead;
    PLIST_ENTRY pEntry;
    PWORK_QUEUE_ENTRY   pWorkQueueEntry;

    ASSERT(NULL != pWorkQueue);
    if (NULL == pWorkQueue)
        return;

    WaitObjects[0] = &pWorkQueue->ShutdownEvent;
    WaitObjects[1] = &pWorkQueue->WakeEvent;

    do
    {
        Status = KeWaitForMultipleObjects(2, WaitObjects, WaitAny, Executive, KernelMode, FALSE, NULL, NULL);
        if (NT_SUCCESS(Status))
        {
            if (STATUS_WAIT_0 == Status)
            {
                bShutdownThread = TRUE;
            }

            InitializeListHead(&WorkQueueItemHead);
            KeAcquireSpinLock(&pWorkQueue->Lock, &OldIrql);
            
            while(!IsListEmpty(&pWorkQueue->WorkerQueueHead))
            {
                pEntry = RemoveTailList(&pWorkQueue->WorkerQueueHead);
                InsertHeadList(&WorkQueueItemHead, pEntry);
            }

            KeResetEvent(&pWorkQueue->WakeEvent);
            KeReleaseSpinLock(&pWorkQueue->Lock, OldIrql);

            while (!IsListEmpty(&WorkQueueItemHead))
            {
                pWorkQueueEntry = (PWORK_QUEUE_ENTRY)RemoveHeadList(&WorkQueueItemHead);
                if (bShutdownThread)
                    pWorkQueueEntry->ulFlags |= WQE_FLAGS_THREAD_SHUTDOWN;

                pWorkQueueEntry->WorkerRoutine(pWorkQueueEntry);
            }
        }
    }while(FALSE == bShutdownThread);

    return;
}