
#define INITGUID
#include "FltFunc.h"
#include "VContext.h"
#include "ListNode.h"

VOID
InDskClusterNotificationThread(
    _In_    PVOID    pContext
);

NTSTATUS
InDskFltCreateClusterNotificationThread(
    _In_    PDEV_CONTEXT    pDevContext
)
/*
Routine Description:
    This function creates cluster notification thread.
     It checks for following before creating thread
        If disk is not clustered, skip thread creation.
        If notification thread is already running, skip thread creation.
Arguments:
    pDevContext
        Device context.
Return Value:
    NTSTATUS        Status of thread creation.
*/
{
    HANDLE              hThread;
    PVOID               threadObject;
    NTSTATUS            Status = STATUS_SUCCESS;
    KIRQL               oldIrql = 0;
    bool                isNotificationThreadRunning = false;

    if (!IS_DISK_CLUSTERED(pDevContext)) {
        InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_CLUSTER_DISK_NOT_CLUSTERED, pDevContext);
        return Status;
    }

    ReferenceDevContext(pDevContext);

    KeClearEvent(&pDevContext->ClusterNotificationEvent);

    KeAcquireSpinLock(&pDevContext->Lock, &oldIrql);
    // Notification Thread is already created.
    if (NULL != pDevContext->pClusterStateThread) {
        isNotificationThreadRunning = true;
    }
    KeReleaseSpinLock(&pDevContext->Lock, oldIrql);

    if (isNotificationThreadRunning) {
        InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_CLUSTER_NOTIFICATION_ALREADY_RUNNING, pDevContext);
        Status = STATUS_SUCCESS;
        goto Cleanup;
    }

    // Create System Thread for releasing writes
    Status = PsCreateSystemThread(&hThread,
        THREAD_ALL_ACCESS,
        NULL,
        NULL,
        NULL,
        InDskClusterNotificationThread,
        pDevContext);

    // We cannot issue tags on this disk
    if (!NT_SUCCESS(Status))
    {
        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_CLUSTER_NOTIFICATION_FAILED, pDevContext, Status);
        goto Cleanup;
    }

    Status = ObReferenceObjectByHandle(hThread, THREAD_ALL_ACCESS, NULL, KernelMode, &threadObject, NULL);
    ZwClose(hThread);

    if (!NT_SUCCESS(Status))
    {
        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_CLUSTER_NOTIFICATIONREF_FAILED, pDevContext, Status);
        goto Cleanup;
    }

    KeAcquireSpinLock(&pDevContext->Lock, &oldIrql);
    pDevContext->pClusterStateThread = threadObject;
    KeReleaseSpinLock(&pDevContext->Lock, oldIrql);

Cleanup:
    DereferenceDevContext(pDevContext);

    return Status;
}

VOID
InDskFltDeleteClusterNotificationThread(
    _In_    PDEV_CONTEXT    pDevContext
)
/*
Routine Description:
    This function deletes cluster notification thread.
        If notification thread is not running, skip thread deletion.
        Issue Cluster Termination event.
        It waits for cluster notification thread to terminate.
Arguments:
    pDevContext
        Device context.
Return Value:
    None.
*/
{
    KIRQL               irql;
    PVOID               pClusterStateThread = NULL;

    ReferenceDevContext(pDevContext);

    KeAcquireSpinLock(&pDevContext->Lock, &irql);
    if (NULL == pDevContext->pClusterStateThread) {
        KeReleaseSpinLock(&pDevContext->Lock, irql);
        DereferenceDevContext(pDevContext);
        return;
    }
    pClusterStateThread = pDevContext->pClusterStateThread;
    pDevContext->pClusterStateThread = NULL;
    KeReleaseSpinLock(&pDevContext->Lock, irql);

    KeSetEvent(&pDevContext->ClusterNotificationEvent, IO_DISK_INCREMENT, TRUE);
    KeWaitForSingleObject(pClusterStateThread, Executive, KernelMode, FALSE, NULL);

    ObDereferenceObject(pClusterStateThread);

    DereferenceDevContext(pDevContext);
}

VOID
InDskClusterNotificationThread(
    _In_    PVOID    pContext
)
/*
Routine Description:
    Cluster notification thread.
     It rescans cluster state every 30 secs
        It calls another function to take action based upon cluster state.
     It exits if exit is called.
Arguments:
    pDevContext
        Device context.
Return Value:
    None.
*/
{
    PDEV_CONTEXT    pDevContext = (PDEV_CONTEXT)pContext;

    ReferenceDevContext(pDevContext);
    InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_CLUSTER_NOTIFICATION_STARTED, pDevContext);

    LARGE_INTEGER     TimeOut;
    NTSTATUS          Status;
    TimeOut.QuadPart = (-1) * (CLUSTER_STATE_RESCAN_TIME_SEC) * (HUNDRED_NANO_SECS_IN_SEC);
    do {
        Status = KeWaitForSingleObject((PVOID)&pDevContext->ClusterNotificationEvent,
            Executive,
            KernelMode,
            FALSE,
            &TimeOut);
        if (STATUS_TIMEOUT == Status) {
            SetDiskClusterState(pDevContext);
        }
    } while (STATUS_SUCCESS != Status);
    InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_CLUSTER_NOTIFICATION_STOPPED, pDevContext);
    DereferenceDevContext(pDevContext);
}


NTSTATUS
SetDiskClusterState(__in   PDEV_CONTEXT pDevContext)
/*
Routine Description:
    Cluster State Action thread.
     If disk is not clustered, return.
     Based upon disk state transition from online or offline to offline or online.
        Old         New         Action
        Offline     Offline     Do nothing.
        Offline     Online      Start filtering, if disk filtering was stopped by cluster.
        Online      Offline     Stop filtering. Mark filtering stopped by cluster.
        Online      Online      Do nothing.
Arguments:
    pDevContext
        Device context.
Return Value:
    NTSTATUS.
*/
{
    bool    isDiskClustered = false;
    bool    isDiskOnline = false;
    KIRQL   irql = 0;
    bool    isStateChanged = false;
    NTSTATUS   Status = STATUS_SUCCESS;
    bool    isLockAcquired = false;

    if (!IS_DISK_CLUSTERED(pDevContext)) {
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    Status = IsDiskClustered(pDevContext, &isDiskClustered, &isDiskOnline);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    KeAcquireSpinLock(&pDevContext->Lock, &irql);
    isLockAcquired = true;

    isStateChanged = (IS_DISK_ONLINE(pDevContext) != isDiskOnline);

    if (isDiskOnline) {
        SET_FLAG(pDevContext->ulClusterFlags, DCF_CLUSTER_FLAGS_DISK_ONLINE);
    }
    else {
        CLEAR_FLAG(pDevContext->ulClusterFlags, DCF_CLUSTER_FLAGS_DISK_ONLINE);
    }
    KeReleaseSpinLock(&pDevContext->Lock, irql);

    isLockAcquired = false;

    if (isStateChanged) 
    {
        InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_CLUSTER_STATE, pDevContext,
            IS_DISK_CLUSTERED(pDevContext) ? L"Clustered" : L"Not Clustered",
            IS_DISK_ONLINE(pDevContext) ? L"Online" : L"Offline");
    }

    KeAcquireSpinLock(&pDevContext->Lock, &irql);
    isLockAcquired = true;

    if (!IS_DISK_CLUSTERED(pDevContext)) {
        goto cleanup;
    }

    if (!TEST_FLAG(pDevContext->ulClusterFlags, DCF_CLUSTER_FLAGS_DISK_PROTECTED)) {
        goto cleanup;
    }

    KeReleaseSpinLock(&pDevContext->Lock, irql);
    isLockAcquired = false;

    if (IS_DISK_ONLINE(pDevContext)) {
        StartFilteringDevice(pDevContext);
        goto cleanup;
    }

    // Stop filtering in following conditions
    //  For offline disk stop filtering
    if (TEST_FLAG(pDevContext->ulFlags, DCF_FILTERING_STOPPED)) {
        goto cleanup;
    }

    StopFiltering(pDevContext, true, false);

cleanup:
    if (isLockAcquired) {
        KeReleaseSpinLock(&pDevContext->Lock, irql);
    }
    return Status;
}

NTSTATUS
IsDiskClustered(
    __in   PDEV_CONTEXT pDevContext,
    __out  bool* isClustered,
    __out  bool* isOnline
)
/*
Routine Description:
    Routine that detects if disk is clustered and online.
    Gets top of device stack using IoGetAttachedDeviceReference.
    Gets Disk is clustered or not using IOCTL_DISK_IS_CLUSTERED.
    Gets Disk Online state using IOCTL_DISK_GET_DISK_ATTRIBUTES.
Arguments:
    pDevContext
        pDevContext     Device context.
        isClustered     variable that receives if disk is clustered or not.
        isOnline        variable that receives if disk is online or offline.
Return Value:
    NTSTATUS.
*/
{
    IO_STATUS_BLOCK         IoStatus = { 0 };
    NTSTATUS                Status = STATUS_INVALID_DEVICE_REQUEST;
    PDEVICE_OBJECT          pDevObject = NULL;
    GET_DISK_ATTRIBUTES     attributes = { 0 };
    BOOLEAN                  bClustered = FALSE;

    *isClustered = false;

    pDevObject = IoGetAttachedDeviceReference(pDevContext->TargetDeviceObject);
    if (NULL == pDevObject)
    {
        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_FUNCTION_FAILURE, pDevContext,
            L"Failed to get top of stack",
            Status);

        return Status;
    }

    // Get the Device ID & Size
    Status = InMageSendDeviceControl(pDevObject,
        IOCTL_DISK_IS_CLUSTERED,
        NULL,
        0,
        &bClustered,
        sizeof(bClustered),
        &IoStatus);

    if (!NT_SUCCESS(Status)) {
        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_FUNCTION_FAILURE, pDevContext,
            L"IOCTL_DISK_IS_CLUSTERED",
            Status);
        goto Cleanup;
    }

    if (IoStatus.Information < sizeof(bClustered)) {
        Status = STATUS_BUFFER_TOO_SMALL;
        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_FUNCTION_FAILURE, pDevContext,
            L"IOCTL_DISK_IS_CLUSTERED",
            Status);
        goto Cleanup;
    }

    *isClustered = (TRUE == bClustered);

    attributes.Version = sizeof(GET_DISK_ATTRIBUTES);

    Status = InMageSendDeviceControl(pDevObject,
        IOCTL_DISK_GET_DISK_ATTRIBUTES,
        NULL,
        0,
        &attributes,
        sizeof(attributes),
        &IoStatus);
    if (!NT_SUCCESS(Status)) {
        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_FUNCTION_FAILURE, pDevContext,
            L"IOCTL_DISK_GET_DISK_ATTRIBUTES",
            Status);
        goto Cleanup;
    }

    if (IoStatus.Information < sizeof(attributes)) {
        Status = STATUS_BUFFER_TOO_SMALL;
        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_FUNCTION_FAILURE, pDevContext,
            L"IOCTL_DISK_GET_DISK_ATTRIBUTES",
            Status);
        goto Cleanup;
    }

    BOOLEAN bOffline = TEST_FLAG(attributes.Attributes, DISK_ATTRIBUTE_OFFLINE);
    *isOnline = (FALSE == bOffline);

Cleanup:
    if (NULL != pDevObject) {
        ObDereferenceObject(pDevObject);
    }
    return Status;
}

NTSTATUS
DeviceIoControlSetDiskClustered(
    IN      PDEVICE_OBJECT  DeviceObject,
    _Inout_ PIRP            Irp
)
/*
Routine Description:
    Handler for IOCTL_INMAGE_START_CLUSTER_FILTERING.
    It enables or disable cluster feature for indskflt driver.
    If feature is enabled it figures all disks that are protected and clustered
        it creates notification thread.
    If feature is disabled, it deletes notification thread for protected clustered disk.
    Arguments:
    pDevContext
        DeviceObject        Indskflt control device.
        Irp                 Irp that contains user mode request.
Return Value:
    NTSTATUS.
*/
{
    NTSTATUS                Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION      IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    KIRQL                   oldIrql = 0;
    ULONG                   ulClusterFlags = 0;
    PDEV_CONTEXT            pDevContext = NULL;
    bool                    isDiskClustered = false;
    bool                    isDiskOnline = false;

    UNREFERENCED_PARAMETER(DeviceObject);
    ASSERT(DeviceObject == DriverContext.ControlDevice);
    ASSERT(IOCTL_INMAGE_SET_DISK_CLUSTERED == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

    if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < sizeof(SET_DISK_CLUSTERED_INPUT)) {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto Cleanup;
    }

    pDevContext = GetDevContextForThisIOCTL(DeviceObject, Irp);
    if (NULL == pDevContext) {
        Status = STATUS_NO_SUCH_DEVICE;
        goto Cleanup;
    }

    Status = IsDiskClustered(pDevContext, &isDiskClustered, &isDiskOnline);
    if (!NT_SUCCESS(Status)) {
        goto Cleanup;
    }

    if (!isDiskClustered) {
        Status = STATUS_INVALID_DEVICE_REQUEST;
        InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_CLUSTER_STATE, pDevContext,
            isDiskClustered ? L"Clustered" : L"Not Clustered",
            isDiskOnline ? L"Online" : L"Offline");

        goto Cleanup;
    }

    KeAcquireSpinLock(&pDevContext->Lock, &oldIrql);
    ulClusterFlags = pDevContext->ulClusterFlags;
    SET_FLAG(ulClusterFlags, DCF_CLUSTER_FLAGS_DISK_CLUSTERED);

    KeReleaseSpinLock(&pDevContext->Lock, oldIrql);

    // Set Cluster Attributes in registry
    Status = SetDWORDInRegVolumeCfg(pDevContext, CLUSTER_ATTRIBUTES, ulClusterFlags);
    if (!NT_SUCCESS(Status)) {
        // Failed to update Cluster Attributes
        InDskFltWriteEventWithDevCtxt(INDSKFLT_WARN_CLUSTER_KEY_WRITE_FAILED, pDevContext, Status);
        goto Cleanup;
    }

    // Cluster Disk protection state is derived from current filtering state.
    // This value will always be tied with current filtering state of disk.
    // This value will be reset on reboot.
    // Post reboot if disk is filtered, cluster disk protection state will be protected.
    KeAcquireSpinLock(&pDevContext->Lock, &oldIrql);
    if (!TEST_FLAG(pDevContext->ulFlags, DCF_FILTERING_STOPPED)) {
        SET_FLAG(ulClusterFlags, DCF_CLUSTER_FLAGS_DISK_PROTECTED);
    }

    pDevContext->ulClusterFlags = ulClusterFlags;
    KeReleaseSpinLock(&pDevContext->Lock, oldIrql);

    Status = SetDiskClusterState(pDevContext);
    InDskFltCreateClusterNotificationThread(pDevContext);
Cleanup:
    if (NULL != pDevContext) {
        DereferenceDevContext(pDevContext);
    }

    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}