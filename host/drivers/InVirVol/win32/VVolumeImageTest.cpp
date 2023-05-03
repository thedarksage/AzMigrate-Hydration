#include "VVDriverContext.h"
#include "VVolumeImageTest.h"
#include "VsnapKernel.h"

PCOMMAND_STRUCT pCommandArray[MAX_COMMAND_PENDING];

VOID
VVolumeImageProcessWriteRequestTest(PDEVICE_EXTENSION DevExtension, PCOMMAND_STRUCT pCommand)
{
    LARGE_INTEGER   TickCount;
    ULONG           ulSlotIndex;
    PCOMMAND_STRUCT pCommandToExecute = NULL;
    KIRQL           OldIrql;

    ULONG Increment = KeQueryTimeIncrement();

    KeQueryTickCount(&TickCount);
    ulSlotIndex = (ULONG) TickCount.QuadPart % MAX_COMMAND_PENDING;

    pCommand->ExpiryTickCount.QuadPart = TickCount.QuadPart + COMMAND_TIMEOUT;

    ReferenceCommand(pCommand);
    
    pCommandToExecute = pCommand;

    KeWaitForSingleObject(&DriverContext.hMutex, Executive, KernelMode, FALSE, NULL);

    if(DriverContext.OutOfOrderTestThread != NULL)
    {
        KeWaitForSingleObject(&DriverContext.PendingQueueMutex, Executive, KernelMode, FALSE, NULL);

        pCommandToExecute = pCommandArray[ulSlotIndex];
        pCommandArray[ulSlotIndex] = pCommand;

        KeReleaseMutex(&DriverContext.PendingQueueMutex, FALSE);
    }

    KeReleaseMutex(&DriverContext.hMutex, FALSE);

    if(pCommandToExecute)
    {
        InVolDbgPrint(DL_VV_WARNING, DM_VV_DEVICE_CONTROL, ("VVolumeImageProcessWriteRequestTest\n"));

        PIO_STACK_LOCATION IoStackLocation = IoGetCurrentIrpStackLocation(pCommandToExecute->Cmd.Write.Irp);
        VVolumeImageProcessWriteRequest((PDEVICE_EXTENSION)IoStackLocation->DeviceObject->DeviceExtension, pCommandToExecute);
        DereferenceCommand(pCommandToExecute);
    }
}

VOID
CommandExpiryMonitor(PVOID pContext)
{
    NTSTATUS        Status = STATUS_SUCCESS;
#if (NTDDI_VERSION >= NTDDI_WS03)
    NTSTATUS        StatusImpersonation;
#endif
    PVOID           WaitObjects[2];
    BOOLEAN         bShutdownThread = FALSE;
    LARGE_INTEGER   CommandTimeout;
    LARGE_INTEGER   CurrentTickCount;
    PCOMMAND_STRUCT pCommandToExecute[MAX_COMMAND_PENDING];
    ULONG           ulCommandToExecute = 0;
    KIRQL           OldIrql;
#if (NTDDI_VERSION >= NTDDI_WS03)
    bool bImpersonation = 0;
#endif
    CommandTimeout.QuadPart = -COMMAND_THREAD_PERIOD;
    WaitObjects[0] = &DriverContext.ShutdownEvent;
    WaitObjects[1] = &DriverContext.hTestThreadShutdown;

    do
    {
        ulCommandToExecute = 0;
        Status = KeWaitForMultipleObjects(2, WaitObjects, WaitAny, Executive, KernelMode, FALSE, &CommandTimeout, NULL);

        if(STATUS_WAIT_0 == Status || STATUS_WAIT_1 == Status)
        {
            //Driver is shutting down
            KeWaitForSingleObject(&DriverContext.PendingQueueMutex, Executive, KernelMode, FALSE, NULL);

            for(ULONG i = 0; i < MAX_COMMAND_PENDING; i++)
            {
                if(pCommandArray[i] == NULL)
                    continue;

                pCommandToExecute[ulCommandToExecute++] = pCommandArray[i];
                pCommandArray[i] = NULL;
            }

            KeReleaseMutex(&DriverContext.PendingQueueMutex, FALSE);

            bShutdownThread = TRUE;
        }
        else
        {
            KeQueryTickCount(&CurrentTickCount);

            KeWaitForSingleObject(&DriverContext.PendingQueueMutex, Executive, KernelMode, FALSE, NULL);

            for(ULONG i = 0; i < MAX_COMMAND_PENDING; i++)
            {
                if(pCommandArray[i] == NULL || pCommandArray[i]->ExpiryTickCount.QuadPart > CurrentTickCount.QuadPart)
                    continue;

                pCommandToExecute[ulCommandToExecute++] = pCommandArray[i];
                pCommandArray[i] = NULL;
            }

            KeReleaseMutex(&DriverContext.PendingQueueMutex, FALSE);
        }

        for(ULONG i = 0; i < ulCommandToExecute; i++)
        {
#if (NTDDI_VERSION >= NTDDI_WS03)
            if (!bImpersonation) {
                StatusImpersonation = VVImpersonate(&DriverContext.ImpersonatioContext, NULL);
                if (NT_SUCCESS(StatusImpersonation)) {
                    bImpersonation = 1;
                }
            }
#endif
            InVolDbgPrint(DL_VV_WARNING, DM_VV_DEVICE_CONTROL, ("CommandExpiryMonitor\n"));

            PIO_STACK_LOCATION IoStackLocation = IoGetCurrentIrpStackLocation(pCommandToExecute[i]->Cmd.Write.Irp);
            VVolumeImageProcessWriteRequest((PDEVICE_EXTENSION)IoStackLocation->DeviceObject->DeviceExtension, pCommandToExecute[i]);
            DereferenceCommand(pCommandToExecute[i]);
        }

    }while(FALSE == bShutdownThread);
#if (NTDDI_VERSION >= NTDDI_WS03)
    if (bImpersonation) {
        SeStopImpersonatingClient();
    }
#endif
}
