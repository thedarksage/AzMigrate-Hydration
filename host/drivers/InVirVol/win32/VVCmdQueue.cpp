#include <VVCommon.h>
#include "VsnapKernel.h"
#include <VVDriverContext.h>

extern DRIVER_CONTEXT   DriverContext;

PIOCOMMAND_QUEUE
AllocateIOCmdQueue(PDEVICE_EXTENSION DeviceExtension)//, PWCHAR CmdQueueDescription, PCOMMAND_ROUTINE CmdRoutine)
{
    PIOCOMMAND_QUEUE  pIOCmdQueue;
    KIRQL           OldIrql;
    HANDLE          hThread;
    NTSTATUS        Status;

    pIOCmdQueue = (PIOCOMMAND_QUEUE)ALLOC_MEMORY_WITH_TAG(sizeof(IOCOMMAND_QUEUE), NonPagedPool, VVTAG_VSNAP_IO_QUEUE);
    if (NULL == pIOCmdQueue) {
        InVolDbgPrint(DL_VV_ERROR, DM_VV_CMD_QUEUE, ("AllocateIOCmdQueue: ALLOC_MEMORY_WITH_TAG failed\n"));
        return NULL;
    }

    RtlZeroMemory(pIOCmdQueue, sizeof(IOCOMMAND_QUEUE));

    pIOCmdQueue->lRefCount = 0x01;
    pIOCmdQueue->DeviceExtension = DeviceExtension;
    

    InitializeListHead(&pIOCmdQueue->CmdList);
    ExInitializeFastMutex(&pIOCmdQueue->hMutex);

    // At the end, increment reference count and add the queue to driver context.
    ReferenceIOCmdQueue(pIOCmdQueue);
    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    InsertTailList(&DriverContext.CmdQueueList, &pIOCmdQueue->ListEntry);
    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

    return pIOCmdQueue;
}


VOID
DeallocateIOCmdQueue(PIOCOMMAND_QUEUE pIOCmdQueue)
{
    KIRQL   OldIrql;

    ASSERT(NULL != pIOCmdQueue);
    ASSERT(IsListEmpty(&pIOCmdQueue->CmdList));
	
    FREE_MEMORY(pIOCmdQueue);

    return;
}

PIOCOMMAND_QUEUE
ReferenceIOCmdQueue(PIOCOMMAND_QUEUE pIOCmdQueue)
{
    ASSERT(NULL != pIOCmdQueue);
    ASSERT(pIOCmdQueue->lRefCount >= 0x01);

    InterlockedIncrement(&pIOCmdQueue->lRefCount);

    return pIOCmdQueue;
}

BOOLEAN
DereferenceIOCmdQueue(PIOCOMMAND_QUEUE pIOCmdQueue)
{
    ASSERT(NULL != pIOCmdQueue);
    ASSERT(pIOCmdQueue->lRefCount >= 0x01);

    if (0 == InterlockedDecrement(&pIOCmdQueue->lRefCount)) {
        DeallocateIOCmdQueue(pIOCmdQueue);
        return TRUE;
    }

    return FALSE;
}


NTSTATUS
QueueIOCmdToCmdQueue(PIOCOMMAND_QUEUE pIOCmdQueue, PIO_COMMAND_STRUCT pIOCommand)
{
    KIRQL       OldIrql;
    NTSTATUS    Status = STATUS_SUCCESS;

    ASSERT(NULL != pIOCmdQueue);
    ASSERT(NULL != pIOCommand);

    if ((NULL != pIOCmdQueue) && (NULL != pIOCommand)) {
        ReferenceIOCommand(pIOCommand);
        ExAcquireFastMutex(&pIOCmdQueue->hMutex);
        InsertTailList(&pIOCmdQueue->CmdList, &pIOCommand->ListEntry);
        pIOCmdQueue->Count++;
        ExReleaseFastMutex(&pIOCmdQueue->hMutex);
    } else {
        Status = STATUS_INVALID_PARAMETER;
    }

    return Status;
}

PIO_COMMAND_STRUCT  RemoveIOCommandFromQueue(PIOCOMMAND_QUEUE pIOCmdQueue) 
{
    PIO_COMMAND_STRUCT      pIOCommand = NULL;
    KIRQL                   OldIrql;
    
    ExAcquireFastMutex(&pIOCmdQueue->hMutex);
    if(!IsListEmpty(&pIOCmdQueue->CmdList)) {
        pIOCommand = (PIO_COMMAND_STRUCT)RemoveHeadList(&pIOCmdQueue->CmdList);
        pIOCmdQueue->Count--;
        ASSERT(pIOCommand->lRefCount != 0 );
    }
    
    
        
    ExReleaseFastMutex(&pIOCmdQueue->hMutex);

    ASSERT(pIOCmdQueue->Count >= 0);

    return  pIOCommand;
}



VOID UndoRemoveIOCommandFromQueue(PIOCOMMAND_QUEUE pIOCmdQueue, PIO_COMMAND_STRUCT pIOCommand) 
{
    KIRQL                   OldIrql;

    if(pIOCommand == NULL)
        return;
    
    ExAcquireFastMutex(&pIOCmdQueue->hMutex);
    pIOCmdQueue->Count++;
    InsertHeadList(&pIOCmdQueue->CmdList, &pIOCommand->ListEntry);
    ExReleaseFastMutex(&pIOCmdQueue->hMutex);

}




PCOMMAND_QUEUE
AllocateCmdQueue(PDEVICE_EXTENSION DeviceExtension, PWCHAR CmdQueueDescription, PCOMMAND_ROUTINE CmdRoutine)
{
    PCOMMAND_QUEUE  pCmdQueue;
    KIRQL           OldIrql;
    HANDLE          hThread;
    NTSTATUS        Status;

    InVolDbgPrint(DL_VV_INFO, DM_VV_CMD_QUEUE, ("AllocateCmdQueue: Allocating %S Queue with CmdRoutine %#p\n",
            CmdQueueDescription, CmdRoutine));

    pCmdQueue = (PCOMMAND_QUEUE)ALLOC_MEMORY_WITH_TAG(sizeof(COMMAND_QUEUE), NonPagedPool, VVTAG_COMMAND_QUEUE);
    if (NULL == pCmdQueue) {
        InVolDbgPrint(DL_VV_ERROR, DM_VV_CMD_QUEUE, ("AllocateCmdQueue: ALLOC_MEMORY_WITH_TAG failed\n"));
        return NULL;
    }

    RtlZeroMemory(pCmdQueue, sizeof(COMMAND_QUEUE));

    pCmdQueue->lRefCount = 0x01;
	pCmdQueue->DeviceExtension = DeviceExtension;
    RtlStringCchCopyW((NTSTRSAFE_PWSTR)&pCmdQueue->QueueDescription, MAX_QUEUE_DESCRIPTION_LEN, CmdQueueDescription);
    pCmdQueue->CmdProcessingRoutine = CmdRoutine;

    InitializeListHead(&pCmdQueue->CmdList);
    KeInitializeSpinLock(&pCmdQueue->Lock);
    KeInitializeEvent(&pCmdQueue->WakeEvent, NotificationEvent, FALSE);

    Status = PsCreateSystemThread(&hThread, THREAD_ALL_ACCESS, NULL, NULL, NULL, CmdThreadRoutine, (PVOID)pCmdQueue);
    if (NT_SUCCESS(Status)) {
        Status = ObReferenceObjectByHandle(hThread, SYNCHRONIZE, NULL, KernelMode, (PVOID *)&(pCmdQueue->Thread), NULL);
        if (NT_SUCCESS(Status)) {
            ZwClose(hThread);
        } else {
            // TODO: Need to cleanup
            InVolDbgPrint(DL_VV_ERROR, DM_VV_CMD_QUEUE, ("AllocateCmdQueue: ObReferenceObjectByHandle failed for %S queue\n",
                CmdQueueDescription));
            return NULL;
        }
    } else {
        // TODOL Need to cleanup
        InVolDbgPrint(DL_VV_ERROR, DM_VV_CMD_QUEUE, ("AllocateCmdQueue: PsCreateSystemThread failed for %S queue\n",
            CmdQueueDescription));
        return NULL;
    }
    
    // At the end, increment reference count and add the queue to driver context.
    ReferenceCmdQueue(pCmdQueue);
    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    InsertTailList(&DriverContext.CmdQueueList, &pCmdQueue->ListEntry);
    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

    InVolDbgPrint(DL_VV_VERBOSE, DM_VV_CMD_QUEUE, ("AllocateCmdQueue: Created successfully command queue for %S\n",
        CmdQueueDescription));
    return pCmdQueue;
}


VOID
DeallocateCmdQueue(PCOMMAND_QUEUE pCmdQueue)
{
    KIRQL   OldIrql;

    ASSERT(NULL != pCmdQueue);
    ASSERT(IsListEmpty(&pCmdQueue->CmdList));
    ASSERT(NULL == pCmdQueue->Thread);

    InVolDbgPrint(DL_VV_VERBOSE, DM_VV_CMD_QUEUE, ("DeallocateCmdQueue: Command queue deleted successfully for %S\n",
        pCmdQueue->QueueDescription));

    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    RemoveEntryList(&pCmdQueue->ListEntry);
    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

    FREE_MEMORY(pCmdQueue);

    return;
}

PCOMMAND_QUEUE
ReferenceCmdQueue(PCOMMAND_QUEUE pCmdQueue)
{
    ASSERT(NULL != pCmdQueue);
    ASSERT(pCmdQueue->lRefCount >= 0x01);

    InterlockedIncrement(&pCmdQueue->lRefCount);

    return pCmdQueue;
}

BOOLEAN
DereferenceCmdQueue(PCOMMAND_QUEUE pCmdQueue)
{
    ASSERT(NULL != pCmdQueue);
    ASSERT(pCmdQueue->lRefCount >= 0x01);

    if (0 == InterlockedDecrement(&pCmdQueue->lRefCount)) {
        DeallocateCmdQueue(pCmdQueue);
        return TRUE;
    }

    return FALSE;
}

NTSTATUS
QueueCmdToCmdQueue(PCOMMAND_QUEUE pCmdQueue, PCOMMAND_STRUCT pCommand)
{
    KIRQL       OldIrql;
    NTSTATUS    Status = STATUS_SUCCESS;

    ASSERT(NULL != pCmdQueue);
    ASSERT(NULL != pCommand);

    if ((NULL != pCmdQueue) && (NULL != pCommand)) {
        ReferenceCommand(pCommand);

        KeAcquireSpinLock(&pCmdQueue->Lock, &OldIrql);
        InsertTailList(&pCmdQueue->CmdList, &pCommand->ListEntry);
        KeSetEvent(&pCmdQueue->WakeEvent, IO_NO_INCREMENT, FALSE);
        KeReleaseSpinLock(&pCmdQueue->Lock, OldIrql);
    } else {
        Status = STATUS_INVALID_PARAMETER;
    }

    return Status;
}

VOID
CmdThreadRoutine(PVOID pContext)
{
    PCOMMAND_QUEUE      pCmdQueue = (PCOMMAND_QUEUE)pContext;
    PVOID               WaitObjects[2];
    NTSTATUS            Status;
#if (NTDDI_VERSION >= NTDDI_WS03)
    NTSTATUS            StatusImpersonation;
#endif
    BOOLEAN             bShutdownThread = FALSE;
    KIRQL               OldIrql;
    LIST_ENTRY          CmdList;
    PLIST_ENTRY         pEntry;
    PCOMMAND_STRUCT     pCommand;
    PCOMMAND_ROUTINE    CmdRoutine;
#if (NTDDI_VERSION >= NTDDI_WS03)
    bool bImpersonation = 0;
#endif
    ASSERT(NULL != pCmdQueue);
    if (NULL == pCmdQueue)
        return;

    WaitObjects[0] = &DriverContext.ShutdownEvent;
    WaitObjects[1] = &pCmdQueue->WakeEvent;

    do
    {
        Status = KeWaitForMultipleObjects(2, WaitObjects, WaitAny, Executive, KernelMode, FALSE, NULL, NULL);
        if (NT_SUCCESS(Status))
        {

            if (STATUS_WAIT_0 == Status)
            {
                bShutdownThread = TRUE;
            }

            InitializeListHead(&CmdList);
            KeAcquireSpinLock(&pCmdQueue->Lock, &OldIrql);
            
            while(!IsListEmpty(&pCmdQueue->CmdList))
            {
                pEntry = RemoveHeadList(&pCmdQueue->CmdList);
                InsertTailList(&CmdList, pEntry);
            }

            KeResetEvent(&pCmdQueue->WakeEvent);
            KeReleaseSpinLock(&pCmdQueue->Lock, OldIrql);

            while (!IsListEmpty(&CmdList))
            {
#if (NTDDI_VERSION >= NTDDI_WS03)
                if (!bImpersonation) {
                    StatusImpersonation = VVImpersonate(&DriverContext.ImpersonatioContext, NULL);
                    if (NT_SUCCESS(StatusImpersonation)) {
                        bImpersonation = 1;
                    }
                }
#endif 
                pCommand = (PCOMMAND_STRUCT)RemoveHeadList(&CmdList);
                if (bShutdownThread)
                    pCommand->ulFlags |= COMMAND_FLAGS_THREAD_REQUIRES_SHUTDOWN;

                // Call the specified command routine.
                CmdRoutine = (NULL != pCommand->CmdRoutine) ? pCommand->CmdRoutine : pCmdQueue->CmdProcessingRoutine;
                ASSERT(NULL != CmdRoutine);
                if (NULL != CmdRoutine)
                    CmdRoutine(pCmdQueue->DeviceExtension, pCommand);
                
                // release the source thread, if waiting on this command completion.
                if (pCommand->ulFlags & COMMAND_FLAGS_WAITING_FOR_COMPLETION) {
                    KeSetEvent(&pCommand->WaitEvent, IO_NO_INCREMENT, FALSE);
                }

                // Free the command.
                if (pCommand->FreeRoutine) {
                    CmdRoutine = pCommand->FreeRoutine;
                    CmdRoutine(pCmdQueue->DeviceExtension, pCommand);
                } else {
                    DereferenceCommand(pCommand);
                }
            }
        }
        
        if(pCmdQueue->ulFlags & CMDQUEUE_FLAGS_THREAD_IS_SHUTDOWN)
            bShutdownThread = TRUE;

    }while(FALSE == bShutdownThread);
#if (NTDDI_VERSION >= NTDDI_WS03)
    if (bImpersonation) {
        SeStopImpersonatingClient();
    }
#endif
    return;
}

/*-----------------------------------------------------------------------------
 *
 * Routines to allocate and deallocate commands
 *
 *-----------------------------------------------------------------------------
 */
PCOMMAND_STRUCT
AllocateCommand(VOID)
{
    PCOMMAND_STRUCT pCommand;

    pCommand = (PCOMMAND_STRUCT)ExAllocateFromNPagedLookasideList(&DriverContext.CommandPool);
    if (NULL != pCommand) {
        InterlockedIncrement(&DriverContext.lCommandStructsAllocated);
        RtlZeroMemory(pCommand, sizeof(COMMAND_STRUCT));

        pCommand->lRefCount = 0x01;
        KeInitializeEvent(&pCommand->WaitEvent, NotificationEvent, FALSE);
    }
    return pCommand;
}

VOID
DeallocateCommand(PCOMMAND_STRUCT pCommand)
{
    ASSERT(NULL != pCommand);
    ASSERT(0 == pCommand->lRefCount);
    ASSERT(DriverContext.lCommandStructsAllocated >= 1);

    InterlockedDecrement(&DriverContext.lCommandStructsAllocated);
    ExFreeToNPagedLookasideList(&DriverContext.CommandPool, pCommand);

    return;
}

PCOMMAND_STRUCT
ReferenceCommand(PCOMMAND_STRUCT pCommand)
{
    ASSERT(NULL != pCommand);
    ASSERT(pCommand->lRefCount >= 1);

    InterlockedIncrement(&pCommand->lRefCount);

    return pCommand;
}

BOOLEAN
DereferenceCommand(PCOMMAND_STRUCT pCommand)
{
    ASSERT(NULL != pCommand);
    ASSERT(pCommand->lRefCount >= 1);

    if (0 == InterlockedDecrement(&pCommand->lRefCount)) {
        DeallocateCommand(pCommand);
        return TRUE;
    }

    return FALSE;
}




PIO_COMMAND_STRUCT
AllocateIOCommand(VOID)
{
    PIO_COMMAND_STRUCT pIOCommand = NULL;

    pIOCommand = (PIO_COMMAND_STRUCT)ExAllocateFromNPagedLookasideList(&DriverContext.IOCommandPool);
    if (NULL != pIOCommand) {
        InterlockedIncrement(&DriverContext.lIOCommandStructsAllocated);
        RtlZeroMemory(pIOCommand, sizeof(IO_COMMAND_STRUCT));

        pIOCommand->lRefCount = 0x01;
        KeInitializeEvent(&pIOCommand->WaitEvent, NotificationEvent, FALSE);
    }
    return pIOCommand;
}

VOID
DeallocateIOCommand(PIO_COMMAND_STRUCT pIOCommand)
{
    PIRP  Irp = NULL;
    ASSERT(NULL != pIOCommand);
    ASSERT(0 == pIOCommand->lRefCount);
    ASSERT(DriverContext.lIOCommandStructsAllocated >= 1);

    if(pIOCommand->ulCommand == VVOLUME_CMD_READ) {
        Irp = pIOCommand->Cmd.Read.Irp;
        Irp->IoStatus.Information = pIOCommand->Cmd.Read.ulLength;
        if(pIOCommand->ulFlags & FLAG_READ_INTO_TEMPORARY_BUFFER) {
            RtlCopyMemory(pIOCommand->pOrigBuffer, pIOCommand->pBuffer, pIOCommand->Cmd.Read.ulLength);
            FREE_MEMORY(pIOCommand->pBuffer);
        }
    } else if(pIOCommand->ulCommand == VVOLUME_CMD_WRITE) {
        Irp = pIOCommand->Cmd.Write.Irp;
    }

    Irp->IoStatus.Status = pIOCommand->CmdStatus;
    if(pIOCommand->CmdStatus != STATUS_SUCCESS) {
        Irp->IoStatus.Information = 0;
        //ASSERT(0);
    }

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    if(FLAG_VIRTUAL_VOLUME == (pIOCommand->ulFlags & FLAG_VIRTUAL_VOLUME))
        IoReleaseRemoveLock(&pIOCommand->VVolume->DevExtension->IoRemoveLock, Irp);
    else
        IoReleaseRemoveLock(&pIOCommand->Vsnap->DevExtension->IoRemoveLock, Irp);

    InterlockedDecrement(&DriverContext.lIOCommandStructsAllocated);
    ExFreeToNPagedLookasideList(&DriverContext.IOCommandPool, pIOCommand);

    return;
}

PIO_COMMAND_STRUCT
ReferenceIOCommand(PIO_COMMAND_STRUCT pCommand)
{
    ASSERT(NULL != pCommand);
    ASSERT(pCommand->lRefCount >= 1);

    InterlockedIncrement(&pCommand->lRefCount);

    return pCommand;
}

BOOLEAN
DereferenceIOCommand(PIO_COMMAND_STRUCT pCommand)
{
    ASSERT(NULL != pCommand);
    ASSERT(pCommand->lRefCount >= 1);

    if (0 == InterlockedDecrement(&pCommand->lRefCount)) {
        DeallocateIOCommand(pCommand);
        return TRUE;
    }

    return FALSE;
}

PVSNAP_TASK  AllocateTask()
{
    PVSNAP_TASK           task = (PVSNAP_TASK)ALLOC_MEMORY_WITH_TAG(sizeof(VSNAP_TASK), PagedPool, VVTAG_VSNAP_IO_TASK);
    if(NULL != task) {
        InterlockedIncrement(&DriverContext.lVsnapTaskAllocated);
        RtlZeroMemory(task, sizeof(VSNAP_TASK));
    }

    return task;
}

void       DeAllocateTask(VSNAP_TASK *task) 
{
    if(task != NULL) {
        FREE_MEMORY(task);
        InterlockedDecrement(&DriverContext.lVsnapTaskAllocated);

    }
}



VSNAP_UPDATE_TASK* AllocateUpdateCommand()
{
    VSNAP_UPDATE_TASK *pUpdateCommand;

    pUpdateCommand = (VSNAP_UPDATE_TASK *)ALLOC_MEMORY_WITH_TAG(sizeof(VSNAP_UPDATE_TASK), PagedPool, VVTAG_VSNAP_UPDATE_COMMAND);
    if (NULL != pUpdateCommand) {
        RtlZeroMemory(pUpdateCommand, sizeof(VSNAP_UPDATE_TASK));

        pUpdateCommand->RefCnt = 0x01;
    }
    return pUpdateCommand;
}



VSNAP_UPDATE_TASK* ReferenceUpdateCommand(VSNAP_UPDATE_TASK *UpdateTask)
{
    if(UpdateTask) {
        InterlockedIncrement(&UpdateTask->RefCnt);
    }
    return UpdateTask;
}

bool DereferenceUpdateCommand(VSNAP_UPDATE_TASK *UpdateTask)
{
    ASSERT(NULL != UpdateTask);
    ASSERT(UpdateTask->RefCnt >= 1);
    

    if (0 == InterlockedDecrement(&UpdateTask->RefCnt)) {
        
        DeallocateUpdateCommand(UpdateTask);
        return TRUE;
    }

    return FALSE;
}

void DeallocateUpdateCommand(VSNAP_UPDATE_TASK *UpdateTask)
{
    ASSERT(NULL != UpdateTask);
    ASSERT(0 == UpdateTask->RefCnt);
    PDEVICE_EXTENSION         ControlDeviceExtension = (PDEVICE_EXTENSION)DriverContext.ControlDevice->DeviceExtension;

    if(UpdateTask->InputData->Cow)
        FREE_MEMORY(UpdateTask->Buffer);
    UpdateTask->Irp->IoStatus.Status = UpdateTask->CmdStatus;
    UpdateTask->Irp->IoStatus.Information = 0;
    IoCompleteRequest(UpdateTask->Irp, IO_NO_INCREMENT);
    IoReleaseRemoveLock(&ControlDeviceExtension->IoRemoveLock, UpdateTask->Irp);
    FREE_MEMORY(UpdateTask);
    
}