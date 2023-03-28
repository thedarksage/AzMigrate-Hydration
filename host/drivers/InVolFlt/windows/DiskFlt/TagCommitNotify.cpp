#include "FltFunc.h"
#include "IoctlInfo.h"
#include "misc.h"
#include "HelperFunctions.h"
#include "VContext.h"
#include "global.h"

NTSTATUS
DeviceIoControlTagCommitNotify(
    IN      PDEVICE_OBJECT  DeviceObject,
    _Inout_ PIRP            Irp
)
/*++

Routine Description:

    This routine is the handler for IOCTL_INMAGE_TAG_COMMIT_NOTIFY.
    This routine does following
    1) Validates Input Paramameter
    2) Validate number of filtered devices.

    There are 2 parts of notification
    1. If tag is not validated or, inserted for all devices, top level routine needs to take care
    2. There are device events that can lead to tag drop. It can be due to device getting removed or,
        stop filtering getting issued or, device moving into bitmap mode.
        Moving to meta data mode is not an issue always as tag doesnt get dropped for meta data mode

    For first case work is very limited
    1. Validate input. During validation this routine owns succeess or, failure
    2. For tag insertion part, once tag is inserted, tag insertion routine will validate
        If tag is inserted for all devices.
    3. During device events like removal or, stop filtering device will take care
        of completing this IRP with proper status
    4. Device events can happen in parallel while input is getting validated or,
        tag is getting inserted. To handle this case ownership is defined
       Routine which starts validation will own this IOCTCL.
       As routine validates one device at a time, other device may change state during 
       this period.
       To handle this condition device looks at ownership and if it is not owner simply sets tag state to 
       failed.
       Once owning routine completes it looks at final status. If it has failed
       It completes IRP with failure.
       More details at the point where this logic is run.


Arguments:

DeviceObject    - Supplies the device object.

Irp             - Supplies the IO request packet.

Return Value:

NTSTATUS

--*/
{
    NTSTATUS                        Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION              IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PTAG_COMMIT_NOTIFY_INPUT        pTagCommitInput = NULL;
    KIRQL                           oldIrql = 0;
    PTAG_COMMIT_NOTIFY_OUTPUT       pTagNotifyOutput = NULL;
    PTAG_COMMIT_NOTIFY_INPUT        pTagCommitInputCopy = NULL;
    Registry                        ParametersKey;
    LARGE_INTEGER                   timeout = { 0 };

    UNREFERENCED_PARAMETER(DeviceObject);

    ASSERT(DriverContext.ControlDevice == DeviceObject);
    ASSERT(IOCTL_INMAGE_TAG_COMMIT_NOTIFY == IrpSp->Parameters.DeviceIoControl.IoControlCode);

    // Validate input buffer length
    if (IrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof(TAG_COMMIT_NOTIFY_INPUT)) 
    {
        Status = STATUS_INFO_LENGTH_MISMATCH;
        goto cleanup;
    }

    if (IrpSp->Parameters.DeviceIoControl.OutputBufferLength < sizeof(TAG_COMMIT_NOTIFY_OUTPUT))
    {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto cleanup;
    }

    pTagCommitInput = (PTAG_COMMIT_NOTIFY_INPUT) Irp->AssociatedIrp.SystemBuffer;
    pTagNotifyOutput = (PTAG_COMMIT_NOTIFY_OUTPUT)Irp->AssociatedIrp.SystemBuffer;

    // Validate if buffer has sufficient size 
    // Every device will have a size of GUID_SIZE_IN_CHARS WCHAR

    // Validate number of disks in input
    if (0 == pTagCommitInput->ulNumDisks)
    {
        Status = STATUS_INVALID_PARAMETER;
        goto cleanup;
    }

    // Validate if buffer has sufficient size 
    if (IrpSp->Parameters.DeviceIoControl.InputBufferLength <
        (ULONG)(FIELD_OFFSET(TAG_COMMIT_NOTIFY_INPUT, DeviceId[pTagCommitInput->ulNumDisks])))
    {
        Status = STATUS_INFO_LENGTH_MISMATCH;
        goto cleanup;
    }

    if (IrpSp->Parameters.DeviceIoControl.OutputBufferLength <
        (ULONG)(FIELD_OFFSET(TAG_COMMIT_NOTIFY_OUTPUT, TagStatus[pTagCommitInput->ulNumDisks])))
    {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto cleanup;
    }

    if (RtlCompareMemory(pTagCommitInput->TagGUID, DriverContext.ZeroGUID, GUID_SIZE_IN_CHARS)) {
        Status = STATUS_INVALID_PARAMETER;
        goto cleanup;
    }
    
    // Driver needs to copy all device information in output buffer
    // Input and output buffer is same as sent by IO Manager
    // To make sure that while copying output buffer, input buffer remains valid
    // Create a copy of input buffer
    pTagCommitInputCopy = (PTAG_COMMIT_NOTIFY_INPUT) ExAllocatePoolWithTag(NonPagedPool, 
                                                    IrpSp->Parameters.DeviceIoControl.InputBufferLength,
                                                    POOL_TAG_COMMIT_NOTIFY);

    if (NULL == pTagCommitInputCopy)
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto cleanup;
    }

    RtlCopyMemory(pTagCommitInputCopy, pTagCommitInput, IrpSp->Parameters.DeviceIoControl.InputBufferLength);
    RtlZeroMemory(pTagNotifyOutput, IrpSp->Parameters.DeviceIoControl.OutputBufferLength);

    RtlCopyMemory(pTagNotifyOutput->TagGUID,
        pTagCommitInputCopy->TagGUID, 
        sizeof(pTagCommitInputCopy->TagGUID));

    pTagNotifyOutput->ulNumDisks = pTagCommitInputCopy->ulNumDisks;

    Status = ParametersKey.OpenKeyReadWrite(&DriverContext.DriverParameters);
    if (NT_SUCCESS(Status)) {
        ParametersKey.ReadULONG(TAG_NOTIFY_TIMEOUT_SEC,
            (ULONG32 *)&DriverContext.ulTagNotifyTimeoutInSecs,
            DEFAULT_TAG_COMMIT_NOTIFY_TIMEOUT_SECS, TRUE);
    }

    Status = STATUS_SUCCESS;

    // Validate if all devices in the input list is present and are in write order state
    // Problem with this is as follows
    //  1. Once a device is scanned, its details will be copied in context data.
    //     As driver starts scanning next device, first device may have gone through
    //          removal or, non-writer order or, filtering stopped.
    //      It can lead to though this IOCTL is successful, devices are not in a state
    //      to serve this request.
    // 2. To handle this condition
    //      Both device routines and other tag notify routine needs to co-ordinate.
    //      Cordination happnes through isOwned field.
    //    Any routine that is supposed to handle this IOCL, sets this value to true
    //      All other routines make sure simply sets flags and quit.
    //      Onwer routine before completing the request, checks for flags
    //      If flag is set to failure, it fails ths IOCTL.
    //  In current condition
    //      This routine sets isOwned flag to true. 
    //      Once all device is enumerated, it checks the flags
    //      If flag is set to failure, it fails the IOCTL
    //      Otherwise just before sending pending status for IOCL
    //      It sets isOwned to false.
    //      As per msdn, all pending requests can be completed while the original IRP is not completed.

    // Another problem is as follows
    //  As this request is getting processed, we can have a situation that tag has arrived during validating this
    //  To avoid this condition Tag guid is only updated once validation is completed.
    PLIST_ENTRY  pCurr = NULL;

    // Check if another tag request is already pending
    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);

    if (NULL != DriverContext.TagNotifyContext.pTagCommitNotifyIrp) {
        Status = STATUS_DEVICE_BUSY;
        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
        goto cleanup;
    }

    // This is needed due to following reason
    // Tag Request has come for earlier commit notify tag
    // Tag insert process cannot be completely inside spin lock
    // This leaves a window where 
    //      1. Reportect tag arrives
    //      2. Driver starts inserting tag for each device at a time
    //      3. Calling process which pended commit notify IRP exits
    //      4. Cancellation routine for pending IRP kicks in and completes IRP.
    //      5. New Request arrives for commit notify
    // To handle race between step and step(3/4/5) there has to be
    //      Either before tag insertion for each device validate if commit tag notify
    //         Tag is same as what is getting inserted. For larger set of disks this will
    //          not be optimal performace as tag insert should be light
    //          Apart there is a IO barrier which will impact workload
    //      To avoid above condition the approach is as follows
    //      If tag request matches notify tag guid, mark state as no new request
    //      accepted. So even if 3/4 race 5 will not be accepted as devie is busy
    if (DriverContext.TagNotifyContext.isCommitNotifyTagInsertInProcess) {
        Status = STATUS_DEVICE_BUSY;
        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
        goto cleanup;
    }
    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);

    // Validate if any of disks already have drain blocked
    for (ULONG ulIndex = 0; ulIndex < pTagCommitInputCopy->ulNumDisks; ulIndex++)
    {
        PTAG_COMMIT_STATUS pTagNotifyOutputStatus = &pTagNotifyOutput->TagStatus[ulIndex];
        RtlCopyMemory(pTagNotifyOutputStatus->DeviceId, pTagCommitInputCopy->DeviceId[ulIndex], GUID_SIZE_IN_BYTES);
        ConvertGUIDtoLowerCase(pTagNotifyOutput->TagStatus[ulIndex].DeviceId);

        PDEV_CONTEXT    DevContext = GetDevContextWithGUID(pTagCommitInputCopy->DeviceId[ulIndex], GUID_SIZE_IN_BYTES);
        if (NULL == DevContext)
        {
            pTagNotifyOutputStatus->Status = DEVICE_STATUS_NOT_FOUND;
            Status = STATUS_NOT_FOUND;
            InDskFltWriteEvent(INDSKFLT_INFO_DRV_TAG_COMMIT_STATUS,
                (CHAR*)pTagCommitInputCopy->TagGUID,
                Status
            );
            goto cleanup;
        }

        KeAcquireSpinLock(&DevContext->Lock, &oldIrql);

        if (TEST_FLAG(DevContext->ulFlags2, DEVICE_FLAGS_DONT_DRAIN)) {
            pTagNotifyOutputStatus->Status = DEVICE_STATUS_DRAIN_ALREADY_BLOCKED;
            Status = STATUS_DEVICE_BUSY;
            KeReleaseSpinLock(&DevContext->Lock, oldIrql);

            InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_DISK_TAG_DROPPED,
                DevContext,
                (CHAR*)pTagCommitInputCopy->TagGUID,
                Status);

            DereferenceDevContext(DevContext);
            goto cleanup;
        }

        KeReleaseSpinLock(&DevContext->Lock, oldIrql);
        DereferenceDevContext(DevContext);
    }

    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);

    // Reset Tag Commit Notify context
    RtlZeroMemory(&DriverContext.TagNotifyContext, sizeof(DriverContext.TagNotifyContext));

    // For all devices reset Tag notify context
    for (pCurr = DriverContext.HeadForDevContext.Flink;
        pCurr != &DriverContext.HeadForDevContext;
        pCurr = pCurr->Flink) {

        PDEV_CONTEXT pTemp = (PDEV_CONTEXT)pCurr;
        KeAcquireSpinLockAtDpcLevel(&pTemp->Lock);
        RtlZeroMemory(&pTemp->TagCommitNotifyContext, sizeof(pTemp->TagCommitNotifyContext));
        KeReleaseSpinLockFromDpcLevel(&pTemp->Lock);
    }

    DriverContext.TagNotifyContext.pTagCommitNotifyIrp = Irp;

    // Until validation for all devices are over, this routine owns completion of this IRP
    DriverContext.TagNotifyContext.isOwned = true;
    DriverContext.TagNotifyContext.ulFlags = pTagCommitInputCopy->ulFlags;
    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);

    // Validate if all of disks are in a state to serve this IOCTL
    for (ULONG ulIndex = 0; ulIndex < pTagCommitInputCopy->ulNumDisks; ulIndex++) 
    {
        PTAG_COMMIT_STATUS pTagNotifyOutputStatus = &pTagNotifyOutput->TagStatus[ulIndex];
        RtlCopyMemory(pTagNotifyOutputStatus->DeviceId, pTagCommitInputCopy->DeviceId[ulIndex], GUID_SIZE_IN_BYTES);
        ConvertGUIDtoLowerCase(pTagNotifyOutput->TagStatus[ulIndex].DeviceId);

        PDEV_CONTEXT    DevContext = GetDevContextWithGUID(pTagCommitInputCopy->DeviceId[ulIndex], GUID_SIZE_IN_BYTES);
        if (NULL == DevContext) 
        {
            pTagNotifyOutputStatus->Status = DEVICE_STATUS_NOT_FOUND;
            Status = STATUS_NOT_FOUND;
            InDskFltWriteEvent(INDSKFLT_INFO_DRV_TAG_COMMIT_STATUS,
                (CHAR*)pTagCommitInputCopy->TagGUID,
                Status
            );

            continue;
        }

        KeAcquireSpinLock(&DevContext->Lock, &oldIrql);
        PTAG_COMMIT_NOTIFY_DEV_CONTEXT   pTagCommitNotifyContext = &DevContext->TagCommitNotifyContext;

        if (pTagCommitNotifyContext->isCommitNotifyTagPending) {
            // This device is already scanned. It means there are multiple devices in input which have same id
            // Fail the request
            Status = STATUS_INVALID_PARAMETER;
            InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_DISK_TAG_DROPPED,
                DevContext,
                (CHAR*)pTagCommitInputCopy->TagGUID,
                Status);

            KeReleaseSpinLock(&DevContext->Lock, oldIrql);
            DereferenceDevContext(DevContext);
            goto cleanup;
        }

        // Fail when multiple disks have same ID
        // Ideally above function will ignore the disks which have conflict
        // but to make it more tighter adding this check
        if (TEST_FLAG(DevContext->ulFlags, DCF_DISKID_CONFLICT)) {
            pTagNotifyOutputStatus->Status = DEVICE_STATUS_DISKID_CONFLICT;
            Status = STATUS_DEVICE_OFF_LINE;
            InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_DISK_TAG_DROPPED,
                DevContext,
                (CHAR*)pTagCommitInputCopy->TagGUID,
                Status);

            KeReleaseSpinLock(&DevContext->Lock, oldIrql);
            DereferenceDevContext(DevContext);
            continue;
        }

        // Validate if device is in removal path
        if (TEST_FLAG(DevContext->ulFlags, DCF_SURPRISE_REMOVAL | DCF_REMOVE_CLEANUP_PENDING)) {
            pTagNotifyOutputStatus->Status = DEVICE_STATUS_REMOVED;
            Status = STATUS_DEVICE_DOES_NOT_EXIST;
            InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_DISK_TAG_DROPPED,
                DevContext,
                (CHAR*)pTagCommitInputCopy->TagGUID,
                Status);

            KeReleaseSpinLock(&DevContext->Lock, oldIrql);
            DereferenceDevContext(DevContext);
            goto cleanup;
        }

        // Validate if this disk is not filtered
        if (TEST_FLAG(DevContext->ulFlags, DCF_FILTERING_STOPPED)) {
            pTagNotifyOutputStatus->Status = DEVICE_STATUS_FILTERING_STOPPED;
            Status = STATUS_INVALID_DEVICE_REQUEST;
            InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_DISK_TAG_DROPPED,
                DevContext,
                (CHAR*)pTagCommitInputCopy->TagGUID,
                Status);

            KeReleaseSpinLock(&DevContext->Lock, oldIrql);
            DereferenceDevContext(DevContext);
            goto cleanup;
        }

        // Validate if device is in write order state
        if (ecWriteOrderStateData != DevContext->eWOState) {
            pTagNotifyOutputStatus->Status = DEVICE_STATUS_NON_WRITE_ORDER_STATE;
            Status = STATUS_INVALID_DEVICE_STATE;
            InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_DISK_TAG_DROPPED,
                DevContext,
                (CHAR*)pTagCommitInputCopy->TagGUID,
                Status);

            KeReleaseSpinLock(&DevContext->Lock, oldIrql);
            DereferenceDevContext(DevContext);
            goto cleanup;
        }

        pTagCommitNotifyContext->TagState = DEVICE_TAG_NOTIFY_STATE_WAITING_FOR_TAG;
        pTagCommitNotifyContext->isCommitNotifyTagPending = true;
        KeReleaseSpinLock(&DevContext->Lock, oldIrql);
        DereferenceDevContext(DevContext);
    }

    if (STATUS_SUCCESS != Status) {
        goto cleanup;
    }

    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
    if (DriverContext.TagNotifyContext.TagCommitStatus != STATUS_SUCCESS) {
        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
        goto cleanup;
    }

    IoSetCancelRoutine(Irp, InDskFltCancelTagCommitNotifyIrp);

    // Check if IRP is already cancelled.
    if (Irp->Cancel && IoSetCancelRoutine(Irp, NULL))
    {
        Status = STATUS_CANCELLED;
        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
        goto cleanup;
    }

    IoMarkIrpPending(Irp);
    Status = STATUS_PENDING;

    if (0 != DriverContext.ulTagNotifyTimeoutInSecs) {
        timeout.QuadPart = SECONDS_RELATIVE(DriverContext.ulTagNotifyTimeoutInSecs);

        KeSetTimer(&DriverContext.TagNotifyProtocolTimer,
            timeout,
            &DriverContext.TagNotifyProtocolTimerDpc);
        DriverContext.TagNotifyContext.bTimerSet = TRUE;
    }
    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);

    // Check if device state changed when validation was in progress
    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
    if (STATUS_SUCCESS != DriverContext.TagNotifyContext.TagCommitStatus) {
        Status = DriverContext.TagNotifyContext.TagCommitStatus;
        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
        goto cleanup;
    }

    // Now devices can complete this IRP if they move in a state where tag cannot be inserted
    DriverContext.TagNotifyContext.isOwned = false;

    // Update the Tag Guid so that tag can be inserted now.
    RtlCopyMemory(&DriverContext.TagNotifyContext.TagGUID, pTagCommitInputCopy->TagGUID, sizeof(pTagCommitInputCopy->TagGUID));

    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);

cleanup:
    if (STATUS_PENDING != Status) {
        KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
        RtlZeroMemory(&DriverContext.TagNotifyContext, sizeof(DriverContext.TagNotifyContext));

        // For all devices reset Tag notify context
        IndskFltResetDeviceTagNotifyState(false);

        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    }

    if (NULL != pTagCommitInputCopy) {
        ExFreePoolWithTag(pTagCommitInputCopy, POOL_TAG_COMMIT_NOTIFY);
    }

    return Status;
}


NTSTATUS
DeviceIoControlSetDrainState(
    IN      PDEVICE_OBJECT  DeviceObject,
    _Inout_ PIRP            Irp
)
{
    NTSTATUS                        Status = STATUS_SUCCESS;
    NTSTATUS                        CurrStatus = STATUS_SUCCESS;
    PIO_STACK_LOCATION              IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PSET_DRAIN_STATE_INPUT          pDrainStateInput = NULL;
    PSET_DRAIN_STATE_OUTPUT         pDrainStateOutput = NULL;
    PSET_DRAIN_STATE_INPUT          pDrainStateInputCopy = NULL;
    KIRQL                           oldIrql;
    Registry*                       pDevParam = NULL;
    ULONG                           ulSize = 0;

    UNREFERENCED_PARAMETER(DeviceObject);

    ASSERT(DriverContext.ControlDevice == DeviceObject);
    ASSERT(IOCTL_INMAGE_SET_DRAIN_STATE == IrpSp->Parameters.DeviceIoControl.IoControlCode);

    // Validate input buffer length
    if (IrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof(SET_DRAIN_STATE_INPUT))
    {
        Status = STATUS_INFO_LENGTH_MISMATCH;
        goto cleanup;
    }

    if (IrpSp->Parameters.DeviceIoControl.OutputBufferLength < sizeof(SET_DRAIN_STATE_OUTPUT))
    {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto cleanup;
    }

    pDrainStateInput = (PSET_DRAIN_STATE_INPUT)Irp->AssociatedIrp.SystemBuffer;
    pDrainStateOutput = (PSET_DRAIN_STATE_OUTPUT)Irp->AssociatedIrp.SystemBuffer;

    // Validate if buffer has sufficient size 
    if (IrpSp->Parameters.DeviceIoControl.InputBufferLength <
        (ULONG)(FIELD_OFFSET(SET_DRAIN_STATE_INPUT, DeviceId[pDrainStateInput->ulNumDisks])))
    {
        Status = STATUS_INFO_LENGTH_MISMATCH;
        goto cleanup;
    }

    if (IrpSp->Parameters.DeviceIoControl.OutputBufferLength <
        (ULONG)(FIELD_OFFSET(SET_DRAIN_STATE_OUTPUT, diskStatus[pDrainStateInput->ulNumDisks])))
    {
        Status = STATUS_INFO_LENGTH_MISMATCH;
        goto cleanup;
    }

    pDrainStateInputCopy = (PSET_DRAIN_STATE_INPUT)ExAllocatePoolWithTag(PagedPool, IrpSp->Parameters.DeviceIoControl.InputBufferLength, TAG_DISKSTATE_POOL);
    if (NULL == pDrainStateInputCopy) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto cleanup;
    }

    pDevParam = new Registry();
    if (NULL == pDevParam) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto cleanup;
    }

    RtlCopyMemory(pDrainStateInputCopy, pDrainStateInput, IrpSp->Parameters.DeviceIoControl.InputBufferLength);

    bool set = TEST_FLAG(pDrainStateInput->ulFlags, DRAIN_STATE_SET_FLAG);

    if (0 == pDrainStateInputCopy->ulNumDisks) {
        Status = SetDrainStateForAllDevices(set, pDrainStateOutput, IrpSp->Parameters.DeviceIoControl.OutputBufferLength);
        goto cleanup;
    }

    for (ULONG ulIndex = 0; ulIndex < pDrainStateInputCopy->ulNumDisks; ulIndex++) {
        PDEV_CONTEXT    pDevContext = GetDevContextWithGUID(pDrainStateInputCopy->DeviceId[ulIndex], GUID_SIZE_IN_BYTES);
        RtlCopyMemory(pDrainStateOutput->diskStatus[ulIndex].DeviceId, pDrainStateInputCopy->DeviceId[ulIndex], GUID_SIZE_IN_BYTES);
        if (NULL == pDevContext) {
            pDrainStateOutput->diskStatus[ulIndex].Status = SET_DRAIN_STATUS_DEVICE_NOT_FOUND;
            continue;
        }

        KeAcquireSpinLock(&pDevContext->Lock, &oldIrql);
        if (set) {
            SET_FLAG(pDevContext->ulFlags2, DEVICE_FLAGS_DONT_DRAIN);
        }
        else {
            CLEAR_FLAG(pDevContext->ulFlags2, DEVICE_FLAGS_DONT_DRAIN);
        }
        KeReleaseSpinLock(&pDevContext->Lock, oldIrql);
        DereferenceDevContext(pDevContext);

        CurrStatus = SetDeviceState(pDevParam, pDrainStateInputCopy->DeviceId[ulIndex], DEVICE_DRAIN_STATE, set ? 1 : 0);
        if (!NT_SUCCESS(CurrStatus)) {
            pDrainStateOutput->diskStatus[ulIndex].Status = SET_DRAIN_STATUS_REGISTRY_WRITE_FAILED;
            pDrainStateOutput->diskStatus[ulIndex].ulInternalError = CurrStatus;
            Status = CurrStatus;
        }
    }

    ulSize = FIELD_OFFSET(SET_DRAIN_STATE_OUTPUT, diskStatus[pDrainStateInputCopy->ulNumDisks]);
cleanup:
    if (NULL != pDevParam) {
        delete pDevParam;
    }

    if (NULL == pDrainStateInputCopy) {
        ExFreePoolWithTag(pDrainStateInputCopy, TAG_DISKSTATE_POOL);
    }

    Irp->IoStatus.Information = ulSize;
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return Status;
}

NTSTATUS
DeviceIoControlGetDiskState(
    IN      PDEVICE_OBJECT  DeviceObject,
    _Inout_ PIRP            Irp
)
{
    NTSTATUS                        Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION              IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PGET_DISK_STATE_INPUT           pDiskStateInput = NULL;
    PGET_DISK_STATE_OUTPUT          pDiskStateOutput = NULL;
    KIRQL                           oldIrql;
    PGET_DISK_STATE_INPUT           pDiskStateInputCopy = NULL;
    ULONG                           ulOutputSize = 0;

    UNREFERENCED_PARAMETER(DeviceObject);

    ASSERT(DriverContext.ControlDevice == DeviceObject);
    ASSERT(IOCTL_INMAGE_GET_DISK_STATE == IrpSp->Parameters.DeviceIoControl.IoControlCode);

    // Validate input buffer length
    if (IrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof(GET_DISK_STATE_INPUT))
    {
        Status = STATUS_INFO_LENGTH_MISMATCH;
        goto cleanup;
    }

    if (IrpSp->Parameters.DeviceIoControl.OutputBufferLength < sizeof(GET_DISK_STATE_OUTPUT))
    {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto cleanup;
    }

    pDiskStateInput = (PGET_DISK_STATE_INPUT) Irp->AssociatedIrp.SystemBuffer;
    pDiskStateOutput = (PGET_DISK_STATE_OUTPUT) Irp->AssociatedIrp.SystemBuffer;

    if (0 == pDiskStateInput->ulNumDisks) {
        Status = STATUS_INVALID_PARAMETER;
        goto cleanup;
    }

    // Validate if buffer has sufficient size 
    if (IrpSp->Parameters.DeviceIoControl.InputBufferLength <
        (ULONG)(FIELD_OFFSET(GET_DISK_STATE_INPUT, DeviceId[pDiskStateInput->ulNumDisks])))
    {
        Status = STATUS_INFO_LENGTH_MISMATCH;
        goto cleanup;
    }

    if (IrpSp->Parameters.DeviceIoControl.OutputBufferLength < 
        (ULONG)(FIELD_OFFSET(GET_DISK_STATE_OUTPUT, diskState[pDiskStateInput->ulNumDisks])))
    {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto cleanup;
    }

    pDiskStateInputCopy = (PGET_DISK_STATE_INPUT)ExAllocatePoolWithTag(PagedPool, IrpSp->Parameters.DeviceIoControl.InputBufferLength, TAG_DISKSTATE_POOL);
    if (NULL == pDiskStateInputCopy) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto cleanup;
    }

    RtlCopyMemory(pDiskStateInputCopy, pDiskStateInput, IrpSp->Parameters.DeviceIoControl.InputBufferLength);
    RtlZeroMemory(pDiskStateOutput, IrpSp->Parameters.DeviceIoControl.OutputBufferLength);

    pDiskStateOutput->ulSupportedFlags = (DISK_SUPPORT_FLAGS_FILTERED | DISK_SUPPORT_FLAGS_DRAIN_BLOCKED);
    ulOutputSize = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;

    for (ULONG ulIndex = 0; ulIndex < pDiskStateInputCopy->ulNumDisks; ulIndex++) {
        ULONG   ulFlags = 0;

        pDiskStateOutput->ulNumDisks++;

        RtlCopyMemory(pDiskStateOutput->diskState[ulIndex].DeviceId, pDiskStateInputCopy->DeviceId[ulIndex], GUID_SIZE_IN_BYTES);
        PDEV_CONTEXT    pDevContext = GetDevContextWithGUID(pDiskStateInputCopy->DeviceId[ulIndex], GUID_SIZE_IN_BYTES);
        if (NULL == pDevContext) {
            Status = STATUS_NOT_FOUND;
            pDiskStateOutput->diskState[ulIndex].Status = RtlNtStatusToDosError(STATUS_NOT_FOUND);
            continue;
        }

        KeAcquireSpinLock(&pDevContext->Lock, &oldIrql);
        if (TEST_FLAG(pDevContext->ulFlags2, DEVICE_FLAGS_DONT_DRAIN)) {
            SET_FLAG(ulFlags, DISK_STATE_DRAIN_BLOCKED);
        }

        if (!TEST_FLAG(pDevContext->ulFlags, DCF_FILTERING_STOPPED)) {
            SET_FLAG(ulFlags, DISK_STATE_FILTERED);
        }
        KeReleaseSpinLock(&pDevContext->Lock, oldIrql);
        DereferenceDevContext(pDevContext);
        pDiskStateOutput->diskState[ulIndex].ulFlags = ulFlags;
    }

cleanup:
    if (NULL != pDiskStateInputCopy) {
        ExFreePoolWithTag(pDiskStateInputCopy, TAG_DISKSTATE_POOL);
    }
    Irp->IoStatus.Information = ulOutputSize;
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return Status;
}


// Cancellation routine for IOCTL_INMAGE_TAG_COMMIT_NOTIFY.
_Use_decl_annotations_
VOID
InDskFltCancelTagCommitNotifyIrp(
    _Inout_                     PDEVICE_OBJECT  DeviceObject,
    _Inout_ _IRQL_uses_cancel_  PIRP            Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    KIRQL                       irql;
    NTSTATUS                    Status = STATUS_CANCELLED;

    IoReleaseCancelSpinLock(Irp->CancelIrql);

    KeAcquireSpinLock(&DriverContext.Lock, &irql);
    // There can be a race in following condition
    //      Commit Notify is getting completed by driver
    //      User process exists so cancellation gets called
    //      New process sends this IOCTL
    //  In this case driver can end up cancelling the wrong IRP
    //  So it is necessary to validate context IRP with cancellation IRP
    if (Irp != DriverContext.TagNotifyContext.pTagCommitNotifyIrp) {
        KeReleaseSpinLock(&DriverContext.Lock, irql);
        return;
    }

    InDskFltWriteEvent(INDSKFLT_INFO_DRV_TAG_COMMIT_STATUS,
        (CHAR*)DriverContext.TagNotifyContext.TagGUID,
        Status);

    RtlZeroMemory(&DriverContext.TagNotifyContext, sizeof(DriverContext.TagNotifyContext));

    // For all devices reset Tag notify context
    IndskFltResetDeviceTagNotifyState(true);

    KeReleaseSpinLock(&DriverContext.Lock, irql);

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return;
}

VOID
InDskFltTagNotifyTimerDpc(
    _In_        PKDPC   TimerDpc,
    _In_opt_    PVOID   Context,
    _In_opt_    PVOID   SystemArgument1,
    _In_opt_    PVOID   SystemArgument2
)
{
    KIRQL       irql = 0;
    PIRP        pTagCommitNotifyIrp = NULL;
    NTSTATUS    Status = STATUS_TIMEOUT;

    UNREFERENCED_PARAMETER(TimerDpc);
    UNREFERENCED_PARAMETER(Context);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    KeAcquireSpinLock(&DriverContext.Lock, &irql);
    if (NULL != DriverContext.TagNotifyContext.pTagCommitNotifyIrp) {
        if (DriverContext.TagNotifyContext.isOwned) {
            DriverContext.TagNotifyContext.TagCommitStatus = STATUS_TIMEOUT;
        }
        else {
            pTagCommitNotifyIrp = DriverContext.TagNotifyContext.pTagCommitNotifyIrp;
            DriverContext.TagNotifyContext.pTagCommitNotifyIrp = NULL;

            InDskFltWriteEvent(INDSKFLT_INFO_DRV_TAG_COMMIT_STATUS,
                (CHAR*)DriverContext.TagNotifyContext.TagGUID,
                Status);

            RtlZeroMemory(&DriverContext.TagNotifyContext, sizeof(DriverContext.TagNotifyContext));
            // For all devices reset Tag notify context
            IndskFltResetDeviceTagNotifyState(true);
        }
    }
    KeReleaseSpinLock(&DriverContext.Lock, irql);

    if (NULL != pTagCommitNotifyIrp) {
        IndskFltCompleteIrp(pTagCommitNotifyIrp, Status, 0);
    }
}

NTSTATUS
IndskFltValidateTagInsertion()
{
    return IndskFltValidateTagInsertion(DriverContext.pTagInfo);
}

// Reset tag notify state of all devices.
// It should be called inside DriverContext lock
VOID
IndskFltResetDeviceTagNotifyState(bool bReleaseDrainBarrier)
{
    PLIST_ENTRY pCurr;

    // For all devices reset Tag notify context
    for (pCurr = DriverContext.HeadForDevContext.Flink;
        pCurr != &DriverContext.HeadForDevContext;
        pCurr = pCurr->Flink) {

        PDEV_CONTEXT pTemp = (PDEV_CONTEXT)pCurr;
        KeAcquireSpinLockAtDpcLevel(&pTemp->Lock);
        if (bReleaseDrainBarrier) {
            CLEAR_FLAG(pTemp->ulFlags2, DEVICE_FLAGS_DONT_DRAIN);
        }
        RtlZeroMemory(&pTemp->TagCommitNotifyContext, sizeof(pTemp->TagCommitNotifyContext));
        KeReleaseSpinLockFromDpcLevel(&pTemp->Lock);
    }
}

NTSTATUS
IndskFltValidateTagInsertion(PTAG_INFO  pTagInfo)
{
    NTSTATUS                        Status = STATUS_SUCCESS;
    KIRQL                           oldIrql = 0;
    PTAG_COMMIT_NOTIFY_OUTPUT       pTagNotifyOutput = NULL;
    PIRP                            pTagCommitNotifyIrp = NULL;
    ULONG                           ulOutputBufferLength = 0;
    PIO_STACK_LOCATION              IrpSp = NULL;

    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);

    // There is no tag in progress
    if (NULL == DriverContext.TagNotifyContext.pTagCommitNotifyIrp) {
        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
        goto cleanup;
    }

    // Validate if this tag is failback tag
    if (!DriverContext.TagNotifyContext.isCommitNotifyTagInsertInProcess) {
        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
        goto cleanup;
    }

    // Check if someone else is already completing it
    if (DriverContext.TagNotifyContext.isOwned) {
        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
        goto cleanup;
    }

    DriverContext.TagNotifyContext.isOwned = true;
    pTagNotifyOutput = (PTAG_COMMIT_NOTIFY_OUTPUT)DriverContext.TagNotifyContext.pTagCommitNotifyIrp->AssociatedIrp.SystemBuffer;

    // This is failback tag but Tag Info allocation failed for this.
    if (NULL == pTagInfo) {
        Status = STATUS_INVALID_DEVICE_STATE;
        IndskFltResetDeviceTagNotifyState(true);

        if (DriverContext.TagNotifyContext.bTimerSet) {
            // TIMER has already started. Let it complete this IRP
            if (FALSE == KeCancelTimer(&DriverContext.TagNotifyProtocolTimer)) {
                InDskFltWriteEvent(INDSKFLT_INFO_DRV_TAG_TIMER_COMMIT_STATUS,
                    (CHAR*)DriverContext.TagNotifyContext.TagGUID,
                    Status);

                KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
                goto cleanup;
            }
            DriverContext.TagNotifyContext.bTimerSet = FALSE;
        }

        pTagCommitNotifyIrp = DriverContext.TagNotifyContext.pTagCommitNotifyIrp;
        DriverContext.TagNotifyContext.pTagCommitNotifyIrp = NULL;

        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
        goto cleanup;
    }

    IrpSp = IoGetCurrentIrpStackLocation(DriverContext.TagNotifyContext.pTagCommitNotifyIrp);
    ulOutputBufferLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;

    for (ULONG ulIndex = 0; ulIndex < pTagInfo->ulNumberOfDevices; ulIndex++) {
        PDEV_CONTEXT    pDevContext = pTagInfo->TagStatus[ulIndex].DevContext;

        if (NULL == pDevContext) {
            continue;
        }

        for (ULONG ulOutputIndex = 0; ulOutputIndex < pTagNotifyOutput->ulNumDisks; ulOutputIndex++) {
            KeAcquireSpinLockAtDpcLevel(&pDevContext->Lock);

            // There can be a case where device is removed/filtering stopped during Tag Process
            // That will be handled by the device routine as insertion
            // only owns pending notify IRP only in this routine
            // If we are coming here it means tag has been inserted or dropped
            if (GUID_SIZE_IN_BYTES == RtlCompareMemory(pTagNotifyOutput->TagStatus[ulOutputIndex].DeviceId,
                pDevContext->wDevID,
                GUID_SIZE_IN_BYTES))
            {
                if (STATUS_SUCCESS != pTagInfo->TagStatus[ulIndex].Status) {
                    if (DriverContext.TagNotifyContext.bTimerSet) {
                        // TIMER has already started. Let it complete this IRP
                        if (FALSE == KeCancelTimer(&DriverContext.TagNotifyProtocolTimer)) {
                            InDskFltWriteEvent(
                                INDSKFLT_INFO_DRV_TAG_TIMER_COMMIT_STATUS,
                                (CHAR*)DriverContext.TagNotifyContext.TagGUID,
                                Status);

                            KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
                            KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
                            goto cleanup;
                        }
                        DriverContext.TagNotifyContext.bTimerSet = FALSE;
                    }

                    Status = pTagInfo->TagStatus[ulIndex].Status;
                    InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_DISK_TAG_DROPPED,
                        pDevContext,
                        (CHAR*)DriverContext.TagNotifyContext.TagGUID,
                        Status);

                    pTagNotifyOutput->TagStatus[ulOutputIndex].Status = DEVICE_STATUS_NON_WRITE_ORDER_STATE;
                    pTagNotifyOutput->TagStatus[ulOutputIndex].TagStatus = TAG_STATUS_INSERTION_FAILED;
                    if (pDevContext->CxSession.hasCxSession) {
                        KeAcquireSpinLockAtDpcLevel(&DriverContext.vmCxSessionContext.CxSessionLock);
                        CopyDeviceCxStatus(pDevContext, &pTagNotifyOutput->TagStatus[ulOutputIndex].DeviceCxStats);
                        KeReleaseSpinLockFromDpcLevel(&DriverContext.vmCxSessionContext.CxSessionLock);
                    }

                    pTagCommitNotifyIrp = DriverContext.TagNotifyContext.pTagCommitNotifyIrp;
                    DriverContext.TagNotifyContext.pTagCommitNotifyIrp = NULL;
                }
                else {
                    pTagNotifyOutput->TagStatus[ulOutputIndex].Status = DEVICE_STATUS_SUCCESS;
                    pTagNotifyOutput->TagStatus[ulOutputIndex].TagStatus = TAG_STATUS_INSERTED;
                }
            }
            KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
        }
    }

    // Validate if all output devices have tags
    if (STATUS_SUCCESS == Status) {
        for (ULONG ulOutputIndex = 0; ulOutputIndex < pTagNotifyOutput->ulNumDisks; ulOutputIndex++) {
            if ((TAG_STATUS_INSERTED != pTagNotifyOutput->TagStatus[ulOutputIndex].TagStatus) &&
                (TAG_STATUS_COMMITTED != pTagNotifyOutput->TagStatus[ulOutputIndex].TagStatus)) {

                Status = STATUS_UNSUCCESSFUL;
                InDskFltWriteEvent(INDSKFLT_INFO_DRV_TAG_COMMIT_STATUS,
                    (CHAR*)DriverContext.TagNotifyContext.TagGUID,
                    Status);

                if (DriverContext.TagNotifyContext.bTimerSet) {
                    // TIMER has already started. Let it complete this IRP
                    if (FALSE == KeCancelTimer(&DriverContext.TagNotifyProtocolTimer)) {
                        InDskFltWriteEvent(
                            INDSKFLT_INFO_DRV_TAG_TIMER_COMMIT_STATUS,
                            (CHAR*)DriverContext.TagNotifyContext.TagGUID,
                            Status);

                        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
                        goto cleanup;
                    }
                    DriverContext.TagNotifyContext.bTimerSet = FALSE;
                }


                pTagCommitNotifyIrp = DriverContext.TagNotifyContext.pTagCommitNotifyIrp;
                DriverContext.TagNotifyContext.pTagCommitNotifyIrp = NULL;
                break;
            }
        }
    }

    DriverContext.TagNotifyContext.isOwned = false;
    DriverContext.TagNotifyContext.isCommitNotifyTagInsertInProcess = false;
    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
cleanup:
    if (STATUS_SUCCESS != Status && NULL != pTagCommitNotifyIrp) {
        InDskFltWriteEvent(
            INDSKFLT_INFO_DRV_TAG_COMMIT_STATUS,
            (CHAR*)DriverContext.TagNotifyContext.TagGUID,
            Status);

        IndskFltCompleteIrp(pTagCommitNotifyIrp, Status, ulOutputBufferLength);
    }

    return Status;
}

VOID
IndskFltCompleteIrp(PIRP        Irp,
    NTSTATUS    Status,
    ULONG       ulOutputLength)
{
    IoSetCancelRoutine(Irp, NULL);
    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = ulOutputLength;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
}

NTSTATUS
SetDeviceState(Registry* pDevParam, PWCHAR wDeviceId, PWCHAR StateName, ULONG ulValue)
{
    UNICODE_STRING      DeviceParameters = { 0 };
    NTSTATUS            Status = STATUS_SUCCESS;

    Status = GetRegistryPathForDeviceConfig(&DeviceParameters, wDeviceId);
    if (!NT_SUCCESS(Status)) {
        goto cleanup;
    }

    Status = pDevParam->OpenKeyReadWrite(&DeviceParameters, TRUE);
    if (!NT_SUCCESS(Status)) {
        goto cleanup;
    }

    Status = pDevParam->WriteULONG(StateName, ulValue);
    if (!NT_SUCCESS(Status)) {
        goto cleanup;
    }

cleanup:
    Status = pDevParam->CloseKey();
    RtlFreeUnicodeString(&DeviceParameters);
    return Status;
}

NTSTATUS
SetDrainStateForAllDevices(bool set, PSET_DRAIN_STATE_OUTPUT    pDrainStateOutput, ULONG ulSize)
{
    PDEV_CONTEXT    pDevContext = NULL;
    PLIST_ENTRY     pDevExtList;
    KIRQL           oldIrql;
    NTSTATUS        Status = STATUS_SUCCESS;
    Registry*       pDevParam = NULL;
    NTSTATUS        CurrStatus = STATUS_SUCCESS;
    ULONG           ulIndex = MAXULONG;

    if (ulSize < sizeof(SET_DRAIN_STATE_OUTPUT)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    // Enumerate all devices in registry
    pDevParam = new Registry();
    if (NULL == pDevParam) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
    pDevExtList = DriverContext.HeadForDevContext.Flink;

    for (; pDevExtList != &DriverContext.HeadForDevContext; pDevExtList = pDevExtList->Flink)
    {
        pDevContext = (PDEV_CONTEXT)pDevExtList;

        KeAcquireSpinLockAtDpcLevel(&pDevContext->Lock);
        if (TEST_FLAG(pDevContext->ulFlags, VSF_DISK_ID_CONFLICT)) {
            KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
            continue;
        }

        ++ulIndex;
        if (ulSize <
            (ULONG)(FIELD_OFFSET(SET_DRAIN_STATE_OUTPUT, diskStatus[ulIndex+1])))
        {
            Status = STATUS_BUFFER_TOO_SMALL;
            KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
            KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
            goto cleanup;
        }

        ++pDrainStateOutput->ulNumDisks;
        if (set) {
            SET_FLAG(pDevContext->ulFlags2, DEVICE_FLAGS_DONT_DRAIN);
        }
        else {
            CLEAR_FLAG(pDevContext->ulFlags2, DEVICE_FLAGS_DONT_DRAIN);
        }
        RtlCopyMemory(pDrainStateOutput->diskStatus[ulIndex].DeviceId, pDevContext->wDevID, GUID_SIZE_IN_BYTES);
        KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
    }
    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);

    ulIndex = 0;
    for (; ulIndex < pDrainStateOutput->ulNumDisks; ++ulIndex) {
        CurrStatus = SetDeviceState(pDevParam, pDrainStateOutput->diskStatus[ulIndex].DeviceId, DEVICE_DRAIN_STATE, set ? 1 : 0);
        if (!NT_SUCCESS(CurrStatus)) {
            Status = CurrStatus;
            pDrainStateOutput->diskStatus[ulIndex].Status = SET_DRAIN_STATUS_REGISTRY_WRITE_FAILED;
            pDrainStateOutput->diskStatus[ulIndex].ulInternalError = CurrStatus;
        }
    }

cleanup:
    if (NULL != pDevParam) {
        delete pDevParam;
    }

    return Status;
}

NTSTATUS
ResetDrainState(_DEV_CONTEXT    *pDevContext)
{
    KIRQL   oldIrql = 0;
    NTSTATUS        Status = STATUS_SUCCESS;
    Registry        *pDevParam = NULL;

    if (NULL == pDevContext) {
        goto cleanup;
    }

    KeAcquireSpinLock(&pDevContext->Lock, &oldIrql);
    CLEAR_FLAG(pDevContext->ulFlags2, DEVICE_FLAGS_DONT_DRAIN);
    KeReleaseSpinLock(&pDevContext->Lock, oldIrql);

    // Let us open/create the registry key for this device.
    pDevParam = new Registry();
    if (NULL == pDevParam) {
        goto cleanup;
    }

    Status = pDevParam->OpenKeyReadWrite(&pDevContext->DevParameters);
    if (!NT_SUCCESS(Status)) {
        goto cleanup;
    }

    Status = pDevParam->WriteULONG(DEVICE_DRAIN_STATE, 0);

cleanup:
    if (NULL != pDevParam) {
        delete pDevParam;
    }
    return Status;

}

VOID
ResetDrainState(PTAG_COMMIT_STATUS pCommitStatus, ULONG    ulNumDevices, Registry* pDevParam)
{
    KIRQL       oldIrql = 0;

    for (ULONG ulIndex = 0; ulIndex < ulNumDevices; ulIndex++) {
        PTAG_COMMIT_STATUS      pTagCommitStatus = &pCommitStatus[ulIndex];
        PDEV_CONTEXT    pDevContext = GetDevContextWithGUID(pTagCommitStatus->DeviceId, GUID_SIZE_IN_BYTES);

        if (NULL != pDevContext) {
            KeAcquireSpinLock(&pDevContext->Lock, &oldIrql);
            CLEAR_FLAG(pDevContext->ulFlags2, DEVICE_FLAGS_DONT_DRAIN);
            KeReleaseSpinLock(&pDevContext->Lock, oldIrql);
            DereferenceDevContext(pDevContext);
        }

        // Even if device is removed make sure to persist this info
        SetDeviceState(pDevParam, pTagCommitStatus->DeviceId, DEVICE_DRAIN_STATE, 0);
    }
}

NTSTATUS
PersistDrainStatus(PTAG_COMMIT_STATUS   pCommitStatus, ULONG    ulNumDevices)
{
    NTSTATUS Status = STATUS_SUCCESS;
    Registry* pDevParam = NULL;

    ASSERT(NULL != pCommitStatus);
    if (NULL == pCommitStatus) {
        Status = STATUS_INVALID_PARAMETER;
    }

    pDevParam = new Registry();
    if (NULL == pDevParam) {
        goto cleanup;
    }

    for (ULONG ulIndex = 0; ulIndex < ulNumDevices; ulIndex++) {
        TAG_COMMIT_STATUS      TagCommitStatus = pCommitStatus[ulIndex];
        Status = SetDeviceState(pDevParam, TagCommitStatus.DeviceId, DEVICE_DRAIN_STATE, 0x1);
        if (!NT_SUCCESS(Status)) {
            TagCommitStatus.Status = DEVICE_STATUS_DRAIN_BLOCK_FAILED;
            goto cleanup;
        }
    }

cleanup:
    if (!NT_SUCCESS(Status)) {
        // Reset Drain State
        ResetDrainState(pCommitStatus, ulNumDevices, pDevParam);
    }

    if (NULL != pDevParam) {
        delete pDevParam;
    }

    return Status;
}

VOID
IndskFltCompleteTagNotifyIfDone(struct _DEV_CONTEXT *DevContext)
{
    NTSTATUS                        Status = STATUS_SUCCESS;
    KIRQL                           oldIrql = 0;
    PTAG_COMMIT_NOTIFY_OUTPUT       pTagNotifyOutput = NULL;
    PIRP                            pTagCommitNotifyIrp = NULL;
    ULONG                           ulOutputBufferLength = 0;
    PIO_STACK_LOCATION              IrpSp = NULL;
    bool                            bCompleteNotifyIrp = false;

    if (NULL == DevContext) {
        goto cleanup;
    }

    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
    // There is no tag in progress
    if (NULL == DriverContext.TagNotifyContext.pTagCommitNotifyIrp) {
        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
        goto cleanup;
    }

    pTagNotifyOutput = (PTAG_COMMIT_NOTIFY_OUTPUT)DriverContext.TagNotifyContext.pTagCommitNotifyIrp->AssociatedIrp.SystemBuffer;

    KeAcquireSpinLockAtDpcLevel(&DevContext->Lock);
    // Make sure output contains this device id
    ULONG ulOutputIndex = 0;

    for (; ulOutputIndex < pTagNotifyOutput->ulNumDisks; ulOutputIndex++) {
        if (GUID_SIZE_IN_BYTES == RtlCompareMemory(pTagNotifyOutput->TagStatus[ulOutputIndex].DeviceId,
            DevContext->wDevID,
            GUID_SIZE_IN_BYTES)) {
            break;
        }
    }

    // Ignore if disk is not found
    if (ulOutputIndex == pTagNotifyOutput->ulNumDisks) {
        KeReleaseSpinLockFromDpcLevel(&DevContext->Lock);
        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
        goto cleanup;
    }

    PTAG_COMMIT_STATUS      pTagCommitStatus = &pTagNotifyOutput->TagStatus[ulOutputIndex];

    // Validate if device is in removal path
    if (TEST_FLAG(DevContext->ulFlags, DCF_SURPRISE_REMOVAL | DCF_REMOVE_CLEANUP_PENDING)) {
        pTagCommitStatus->Status = DEVICE_STATUS_REMOVED;
        pTagCommitStatus->TagStatus = TAG_STATUS_DROPPED;
        Status = STATUS_DEVICE_DOES_NOT_EXIST;

        InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_DISK_TAG_DROPPED,
            DevContext,
            (CHAR*)pTagNotifyOutput->TagGUID,
            Status);

        if (DriverContext.TagNotifyContext.isOwned) {
            DriverContext.TagNotifyContext.TagCommitStatus = Status;
            KeReleaseSpinLockFromDpcLevel(&DevContext->Lock);
            KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
            goto cleanup;
        }

        if (DriverContext.TagNotifyContext.bTimerSet) {
            // TIMER has already started. Let it complete this IRP
            if (FALSE == KeCancelTimer(&DriverContext.TagNotifyProtocolTimer)) {
                KeReleaseSpinLockFromDpcLevel(&DevContext->Lock);
                KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
                goto cleanup;
            }
            DriverContext.TagNotifyContext.bTimerSet = FALSE;
        }

        bCompleteNotifyIrp = true;
        pTagCommitNotifyIrp = DriverContext.TagNotifyContext.pTagCommitNotifyIrp;

        IrpSp = IoGetCurrentIrpStackLocation(DriverContext.TagNotifyContext.pTagCommitNotifyIrp);
        ulOutputBufferLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;

        DriverContext.TagNotifyContext.pTagCommitNotifyIrp = NULL;

        RtlZeroMemory(&DriverContext.TagNotifyContext, sizeof(DriverContext.TagNotifyContext));

        KeReleaseSpinLockFromDpcLevel(&DevContext->Lock);
        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
        goto cleanup;
    }

    // Validate if this disk is not filtered
    if (TEST_FLAG(DevContext->ulFlags, DCF_FILTERING_STOPPED)) {
        pTagCommitStatus->Status = DEVICE_STATUS_FILTERING_STOPPED;
        pTagCommitStatus->TagStatus = TAG_STATUS_DROPPED;
        Status = STATUS_INVALID_DEVICE_REQUEST;

        InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_DISK_TAG_DROPPED,
            DevContext,
            (CHAR*)pTagNotifyOutput->TagGUID,
            Status);

        // Tag is currently owned by other thread. 
        // Set status to failure for owning thread to fail this IOCTL.
        if (DriverContext.TagNotifyContext.isOwned) {
            DriverContext.TagNotifyContext.TagCommitStatus = Status;
            KeReleaseSpinLockFromDpcLevel(&DevContext->Lock);
            KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
            goto cleanup;
        }

        if (DriverContext.TagNotifyContext.bTimerSet) {
            // TIMER has already started. Let it complete this IRP
            if (FALSE == KeCancelTimer(&DriverContext.TagNotifyProtocolTimer)) {
                KeReleaseSpinLockFromDpcLevel(&DevContext->Lock);
                KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
                goto cleanup;
            }
            DriverContext.TagNotifyContext.bTimerSet = FALSE;
        }


        bCompleteNotifyIrp = true;
        pTagCommitNotifyIrp = DriverContext.TagNotifyContext.pTagCommitNotifyIrp;

        IrpSp = IoGetCurrentIrpStackLocation(DriverContext.TagNotifyContext.pTagCommitNotifyIrp);
        ulOutputBufferLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;

        DriverContext.TagNotifyContext.pTagCommitNotifyIrp = NULL;

        RtlZeroMemory(&DriverContext.TagNotifyContext, sizeof(DriverContext.TagNotifyContext));

        KeReleaseSpinLockFromDpcLevel(&DevContext->Lock);
        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
        goto cleanup;
    }

    if ((DEVICE_TAG_NOTIFY_STATE_TAG_COMMITTED == DevContext->TagCommitNotifyContext.TagState)
        && (pTagCommitStatus->TagStatus != TAG_STATUS_COMMITTED)) {
        pTagCommitStatus->TagStatus = TAG_STATUS_COMMITTED;
        pTagCommitStatus->Status = DEVICE_STATUS_SUCCESS;
        pTagCommitStatus->TagInsertionTime = DevContext->TagCommitNotifyContext.ullInsertTimeStamp;
        pTagCommitStatus->TagSequenceNumber = DevContext->TagCommitNotifyContext.ullSequenceNumber;
        ++DriverContext.TagNotifyContext.ulNumDevicesTagCommitted;

        if (TEST_FLAG(DriverContext.TagNotifyContext.ulFlags, TAG_COMMIT_NOTIFY_BLOCK_DRAIN_FLAG)) {
            SET_FLAG(DevContext->ulFlags2, DEVICE_FLAGS_DONT_DRAIN);
        }

        InDskFltWriteEvent(INDSKFLT_INFO_TAG_NOTIFY_STATUS,
            (CHAR*)pTagNotifyOutput->TagGUID,
            pTagNotifyOutput->ulNumDisks,
            DriverContext.TagNotifyContext.ulNumDevicesTagCommitted);

        if (DriverContext.TagNotifyContext.ulNumDevicesTagCommitted == pTagNotifyOutput->ulNumDisks) 
        {
            if (DriverContext.TagNotifyContext.bTimerSet) {
                // TIMER has already started. Let it complete this IRP
                if (FALSE == KeCancelTimer(&DriverContext.TagNotifyProtocolTimer)) {
                    KeReleaseSpinLockFromDpcLevel(&DevContext->Lock);
                    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
                    goto cleanup;
                }
                DriverContext.TagNotifyContext.bTimerSet = FALSE;
            }

            bCompleteNotifyIrp = true;
            pTagCommitNotifyIrp = DriverContext.TagNotifyContext.pTagCommitNotifyIrp;

            IrpSp = IoGetCurrentIrpStackLocation(DriverContext.TagNotifyContext.pTagCommitNotifyIrp);
            ulOutputBufferLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;

            InDskFltWriteEvent(INDSKFLT_INFO_DRV_TAG_COMMIT_STATUS,
                (CHAR*)pTagNotifyOutput->TagGUID,
                Status);

            if (TEST_FLAG(DriverContext.TagNotifyContext.ulFlags, TAG_COMMIT_NOTIFY_BLOCK_DRAIN_FLAG)) {
                KeReleaseSpinLockFromDpcLevel(&DevContext->Lock);
                KeReleaseSpinLock(&DriverContext.Lock, oldIrql);

                Status = PersistDrainStatus(pTagNotifyOutput->TagStatus, pTagNotifyOutput->ulNumDisks);
                KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
                KeAcquireSpinLockAtDpcLevel(&DevContext->Lock);

            }

            DriverContext.TagNotifyContext.pTagCommitNotifyIrp = NULL;

            RtlZeroMemory(&DriverContext.TagNotifyContext, sizeof(DriverContext.TagNotifyContext));

            KeReleaseSpinLockFromDpcLevel(&DevContext->Lock);
            KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
            goto cleanup;
        }
    }

    // Ignore rest of events if tag is already committed
    if (TAG_STATUS_COMMITTED == pTagCommitStatus->TagStatus) {
        KeReleaseSpinLockFromDpcLevel(&DevContext->Lock);
        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
        goto cleanup;
    }

    KeReleaseSpinLockFromDpcLevel(&DevContext->Lock);
    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
cleanup:
    if (bCompleteNotifyIrp) {
        IndskFltCompleteIrp(pTagCommitNotifyIrp, Status, ulOutputBufferLength);
    }
    return;
}