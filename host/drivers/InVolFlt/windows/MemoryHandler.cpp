/**
* @file MemoryHandler.c
*
* Low Memory condition handling functions
*
*/
#include "FltFunc.h"
#include "misc.h"

// This routine register a notification
// event by event name.
NTSTATUS
InDskFltRegisterMemoryNotificationEvents(
_In_    PUNICODE_STRING EventName,
_Out_   PKEVENT *Event
)
{
    OBJECT_ATTRIBUTES Obja;
    NTSTATUS Status;
    HANDLE hEvent;
    OBJECT_HANDLE_INFORMATION handleInformation;


    InitializeObjectAttributes(&Obja,
        EventName,
        OBJ_KERNEL_HANDLE,
        NULL,
        NULL);

    Status = ZwOpenEvent(&hEvent,
        SYNCHRONIZE | EVENT_QUERY_STATE,
        &Obja);

    if (!NT_SUCCESS(Status)) {

        goto _cleanup;
    }

    Status = ObReferenceObjectByHandle(hEvent,
        0,
        NULL,
        KernelMode,
        (PVOID *)Event,
        &handleInformation);

    ZwClose(hEvent);

_cleanup:
    if (!NT_SUCCESS(Status))
    {
        InDskFltWriteEvent(INDSKFLT_ERROR_FAILED_TO_CREATE_LOW_MEMORY_MONITOR_THREAD, Status);
    }

    return Status;
}

// This routine deregister memory event notifications
VOID
InDskFltDeregisterLowMemoryNotification(VOID)
{
    if (NULL != DriverContext.hLowMemoryThread)
    {
        // Signal Low Memory thread termination
        KeSetEvent(&DriverContext.hLowMemoryThreadShutdown,
            LOW_REALTIME_PRIORITY,
            FALSE);

        // Wait for low memory handling thread to exit
        KeWaitForSingleObject(DriverContext.hLowMemoryThread,
            Executive,
            KernelMode,
            FALSE,
            NULL);

        NtClose(DriverContext.hLowMemoryThread);
        DriverContext.hLowMemoryThread = NULL;
    }

    if (DriverContext.hLowNonPagedPoolCondition)
    {
        ObDereferenceObject(DriverContext.hLowNonPagedPoolCondition);
        DriverContext.hLowNonPagedPoolCondition = NULL;
    }

    if (DriverContext.hLowMemoryCondition)
    {
        ObDereferenceObject(DriverContext.hLowMemoryCondition);
        DriverContext.hLowMemoryCondition = NULL;
    }
}

// System Thread for polling and checking low memory condition
VOID
InDskFltLowMemoryThreadProc(
PVOID Context
)
{
    NTSTATUS          status;
    PLARGE_INTEGER    timeout = NULL;
    LARGE_INTEGER     liTimeout;
    PKEVENT           waitObjects[MAX_INDSKFLT_WAIT_OBJECTS];
    BOOLEAN           exit = FALSE;
    const ULONG       sleepTime = MAX_LOW_MEMORY_WAIT_TIMEOUT_MILLISECONDS;

    UNREFERENCED_PARAMETER(Context);

    liTimeout.QuadPart = MILLISECONDS_RELATIVE(sleepTime);

    waitObjects[INDSKFLT_WAIT_TERMINATE_THREAD] = &DriverContext.hLowMemoryThreadShutdown;
    waitObjects[INDSKFLT_WAIT_LOW_NON_PAGED_POOL_CONDITION] = DriverContext.hLowNonPagedPoolCondition;
    waitObjects[INDSKFLT_WAIT_LOW_MEMORY_CONDITION] = DriverContext.hLowMemoryCondition;

    while (FALSE == exit)
    {

        status = KeWaitForMultipleObjects(MAX_INDSKFLT_WAIT_OBJECTS,
            (PVOID *) &waitObjects[0],
            WaitAny,
            Executive,
            KernelMode,
            FALSE,
            timeout,
            NULL);

        switch (status)
        {
            case INDSKFLT_WAIT_LOW_NON_PAGED_POOL_CONDITION:
            case INDSKFLT_WAIT_LOW_MEMORY_CONDITION:
            {
                // Set Low memory condition
                InterlockedCompareExchange((LONG*)&DriverContext.bLowMemoryCondition, 1, 0);


                // We set timeout when low memory situation occurs
                // Once the low memory situation is resolved, the timeout
                // will happen on next wait, then we will clear the low
                // memory flag.
                timeout = &liTimeout;

                // Sleep for configured ms before we check low memory condition again.
                KeDelayExecutionThread(KernelMode,
                    FALSE,
                    timeout);
                break;
            }

            case INDSKFLT_WAIT_TERMINATE_THREAD:
            {
                // Thread is set to terminate
                exit = TRUE;
                break;
            }

            case STATUS_TIMEOUT:
            {
                // Low memory situation has been resolved, reset low memory flag
                InterlockedCompareExchange((LONG*)&DriverContext.bLowMemoryCondition, 0, 1);

                // Call any method which can be resumed now

                // wait indefinitly until one of the events
                // is signaled.
                timeout = NULL;
                break;
            }

            default:
                break;
        } /*end of switch()*/
    } /*end of while()*/

    PsTerminateSystemThread(STATUS_SUCCESS);
}

// Routine for registering low memory conditions.
NTSTATUS
InDskFltRegisterLowMemoryNotification(
VOID
)
{
    NTSTATUS                    status;

    DeclareUnicodeString(LowNonPagedPoolMemName, LOW_NONPAGEDPOOL_CONDITION_KERNEL_OBJECT);
    DeclareUnicodeString(LowMemName, LOW_MEMORY_CONDITION_KERNEL_OBJECT);

    KeInitializeEvent(&DriverContext.hLowMemoryThreadShutdown,
                        NotificationEvent,
                        FALSE);

    status = InDskFltRegisterMemoryNotificationEvents(&LowNonPagedPoolMemName, &DriverContext.hLowNonPagedPoolCondition);
    if (!NT_SUCCESS(status))
    {
        goto _cleanup;
    }

    status = InDskFltRegisterMemoryNotificationEvents(&LowMemName, &DriverContext.hLowMemoryCondition);
    if (!NT_SUCCESS(status))
    {
        goto _cleanup;
    }

    status = PsCreateSystemThread(&DriverContext.hLowMemoryThread,
                                    THREAD_ALL_ACCESS,
                                    NULL,
                                    NULL,
                                    NULL,
                                    InDskFltLowMemoryThreadProc,
                                    NULL);
    if(!NT_SUCCESS(status)) {
        InDskFltWriteEvent(INDSKFLT_ERROR_THREAD_CREATION_FAILED, status);
    }

_cleanup:
    if (!NT_SUCCESS(status))
    {
        InDskFltDeregisterLowMemoryNotification();
    }

    return status;
}
