#include "FltFunc.h"
#include "IoctlInfo.h"
#include "misc.h"
#include "HelperFunctions.h"
#include "VContext.h"

const ULONGLONG a_ullDiskMaximumChurnPerBuckets[] = {
    0,
    5 * 1024 * 1024,
    10 * 1024 * 1024,
    15 * 1024 * 1024,
    20 * 1024 * 1024,
    25 * 1024 * 1024,
    30 * 1024 * 1024,
    35 * 1024 * 1024,
    40 * 1024 * 1024,
    45 * 1024 * 1024,
    50 * 1024 * 1024
};

const ULONGLONG a_ullVMMaximumChurnPerBuckets[] = {
    0,
    10 * 1024 * 1024,
    20 * 1024 * 1024,
    30 * 1024 * 1024,
    40 * 1024 * 1024,
    50 * 1024 * 1024,
    60 * 1024 * 1024,
    70 * 1024 * 1024,
    80 * 1024 * 1024,
    90 * 1024 * 1024,
    100 * 1024 * 1024
};

NTSTATUS
DeviceIoControlCommitDBFailTrans(
    IN      PDEVICE_OBJECT  DeviceObject,
    _Inout_ PIRP            Irp
)
/*++
Routine Description:

This routine is the handler for IOCTL_INMAGE_COMMITDB_FAIL_TRANS.
This routine does following
1) Validates Input Paramameter
2) Validates if device id is valid.
3) If there is any CX session is in progess it increments number
    of nw errors seen in current CX session.
    If it is first nw error seen in current CX session
        first nw error time is recorded.
    Last nw error timestamp is set to current timestamp.

Arguments:
    DeviceObject    - Supplies the device object.
    Irp             - Supplies the IO request packet.

Return Value:
    STATUS_SUCCESS
            In all successful cases.
    STATUS_INFO_LENGTH_MISMATCH
            If input buffer length is less than the expected length.
    STATUS_NO_SUCH_DEVICE
            If device id doesn't exist in current list.
    STATUS_INVALID_PARAMETER
            If there is no pending transaction
            If input transaction id doesn't match pending transaction id.
            if COMMITDB_NETWORK_FAILURE flag is not set inside input flag.
--*/
{
    NTSTATUS                    Status = STATUS_SUCCESS;
    PCOMMIT_DB_FAILURE_STATS    pCommitDBFailureStats = NULL;
    KIRQL                       oldIrql = 0;
    PIO_STACK_LOCATION          IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PDEV_CONTEXT                pDevContext = NULL;

    ASSERT(DriverContext.ControlDevice == DeviceObject);
    ASSERT(IOCTL_INMAGE_COMMITDB_FAIL_TRANS == IrpSp->Parameters.DeviceIoControl.IoControlCode);

    Irp->IoStatus.Information = 0;

    pCommitDBFailureStats = (PCOMMIT_DB_FAILURE_STATS) Irp->AssociatedIrp.SystemBuffer;

    if (IrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof(COMMIT_DB_FAILURE_STATS)) {
        Status = STATUS_INFO_LENGTH_MISMATCH;
        goto cleanup;
    }

    // Flag should be set for nw failure
    if (!TEST_FLAG(pCommitDBFailureStats->ullFlags, COMMITDB_NETWORK_FAILURE)) {
        Status = STATUS_INVALID_PARAMETER;
        goto cleanup;
    }

    pDevContext = GetDevContextForThisIOCTL(DeviceObject, Irp);
    if (NULL == pDevContext) {
        Status = STATUS_NO_SUCH_DEVICE;
        goto cleanup;
    }

    KeAcquireSpinLock(&pDevContext->Lock, &oldIrql);

    if (NULL == pDevContext->pDBPendingTransactionConfirm) {
        if (pDevContext->ulPendingTSOTransactionID.QuadPart !=
            pCommitDBFailureStats->ulTransactionID.QuadPart) {
            Status = STATUS_INVALID_PARAMETER;
            KeReleaseSpinLock(&pDevContext->Lock, oldIrql);
            goto cleanup;
        }
    }

    // If pending transaction id doesn't match current session
    //  fail the IOCTL.
    else{
        if (pDevContext->pDBPendingTransactionConfirm->uliTransactionID.QuadPart !=
            pCommitDBFailureStats->ulTransactionID.QuadPart) {
            Status = STATUS_INVALID_PARAMETER;
            KeReleaseSpinLock(&pDevContext->Lock, oldIrql);
            goto cleanup;
        }
    }

    KeAcquireSpinLockAtDpcLevel(&DriverContext.vmCxSessionContext.CxSessionLock);
    if (!pDevContext->CxSession.hasCxSession) {
        KeReleaseSpinLockFromDpcLevel(&DriverContext.vmCxSessionContext.CxSessionLock);
        KeReleaseSpinLock(&pDevContext->Lock, oldIrql);
        goto cleanup;
    }

    if (CxSessionInProgress != DriverContext.vmCxSessionContext.vmCxSession.cxState) {
        KeReleaseSpinLockFromDpcLevel(&DriverContext.vmCxSessionContext.CxSessionLock);
        KeReleaseSpinLock(&pDevContext->Lock, oldIrql);
        goto cleanup;
    }

    if (pDevContext->CxSession.ullSessionId != DriverContext.vmCxSessionContext.ullSessionId) {
        KeReleaseSpinLockFromDpcLevel(&DriverContext.vmCxSessionContext.CxSessionLock);
        KeReleaseSpinLock(&pDevContext->Lock, oldIrql);
        goto cleanup;
    }

    if (0 == pDevContext->CxSession.ullTotalNWErrors) {
            KeQuerySystemTime(&pDevContext->CxSession.liFirstNwFailureTS);
    }
    KeQuerySystemTime(&pDevContext->CxSession.liLastNwFailureTS);
    ++pDevContext->CxSession.ullTotalNWErrors;
    pDevContext->CxSession.ullLastNWErrorCode = pCommitDBFailureStats->ullErrorCode;

    SET_FLAG(pDevContext->CxSession.ullFlags, DISK_CXSTATUS_NWFAILURE_FLAG);
    KeReleaseSpinLockFromDpcLevel(&DriverContext.vmCxSessionContext.CxSessionLock);
    KeReleaseSpinLock(&pDevContext->Lock, oldIrql);

cleanup:
    if (pDevContext) {
        DereferenceDevContext(pDevContext);
    }

    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS
DeviceIoControlGetCXStatsNotify(
    IN      PDEVICE_OBJECT  DeviceObject,
    _Inout_ PIRP            Irp
)
/*++
Routine Description:

    This routine is the handler for IOCTL_INMAGE_GET_CXSTATS_NOTIFY.
    This routine does following
        1) Validates Input Paramameter
        2) Validates input contains all filtered devices currently.
        3) size is correct

Arguments:
    DeviceObject    - Supplies the device object.
    Irp             - Supplies the IO request packet.

Return Value:
    STATUS_SUCCESS
    STATUS_DEVICE_BUSY
            If there is already another IOCTL that is getting processed.
    STATUS_INFO_LENGTH_MISMATCH
            If input buffer length is less than expected.
    STATUS_BUFFER_TOO_SMALL
            If output buffer length is less than expected.
    STATUS_PENDING
            If 
    STATUS_CANCELLED


--*/
{
    NTSTATUS                    Status = STATUS_SUCCESS;
    PGET_CXFAILURE_NOTIFY       pGetCxFailureNotify = NULL;
    KIRQL                       oldIrql = 0;
    PIO_STACK_LOCATION          IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PIRP                        prevCxNotifyIrp = NULL;
    PVM_CXFAILURE_STATS         pVmCxFailureStats = NULL;
    PVM_CX_SESSION_CONTEXT      pVmCxSessionCtxt = NULL;
    ULONG                       ulOutputBufferLength = 0;

    UNREFERENCED_PARAMETER(DeviceObject);

    ASSERT(DriverContext.ControlDevice == DeviceObject);
    ASSERT(IOCTL_INMAGE_GET_CXSTATS_NOTIFY == IrpSp->Parameters.DeviceIoControl.IoControlCode);

    pVmCxSessionCtxt = &DriverContext.vmCxSessionContext;

    ulOutputBufferLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;

    // First validation is if there is already GetCXStatsNotify IOCTL
    //  in progress.
    // We cannot process multiple CX IRP in parallel as we need to make sure
    // we have just one pending CX IRP
    // Rest of IRPs should be cancelled. With multiple IRPs in progress
    // We can run into an issue where one thread is trying to cancel
    // another IRP.
    KeAcquireSpinLock(&pVmCxSessionCtxt->CxSessionLock, &oldIrql);
    if (DriverContext.vmCxSessionContext.isCxIrpInProgress) {
        Status = STATUS_DEVICE_BUSY;
        KeReleaseSpinLock(&pVmCxSessionCtxt->CxSessionLock, oldIrql);
        goto cleanup;
    }
    DriverContext.vmCxSessionContext.isCxIrpInProgress = TRUE;
    KeReleaseSpinLock(&pVmCxSessionCtxt->CxSessionLock, oldIrql);

    if (IrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof(GET_CXFAILURE_NOTIFY)) {
        Status = STATUS_INFO_LENGTH_MISMATCH;
        goto cleanup;
    }

    if (IrpSp->Parameters.DeviceIoControl.OutputBufferLength < sizeof(VM_CXFAILURE_STATS)) {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto cleanup;
    }

    pGetCxFailureNotify = (PGET_CXFAILURE_NOTIFY) Irp->AssociatedIrp.SystemBuffer;
    pVmCxFailureStats = (PVM_CXFAILURE_STATS)Irp->AssociatedIrp.SystemBuffer;

    Status = ValidateGetCxFailureNotifyInput(pGetCxFailureNotify,
                        IrpSp->Parameters.DeviceIoControl.InputBufferLength,
                        ulOutputBufferLength);
    if (STATUS_SUCCESS != Status) {
        Irp->IoStatus.Information = 0;
        goto cleanup;
    }

    // Cleanup any previous pending cx notify IRP
    KeAcquireSpinLock(&pVmCxSessionCtxt->CxSessionLock, &oldIrql);
    if (TEST_FLAG(pGetCxFailureNotify->ullFlags, CXSTATUS_COMMIT_PREV_SESSION)){
        if ((0 != pGetCxFailureNotify->ulTransactionID.QuadPart) &&
            (pVmCxSessionCtxt->vmCxSession.ullPendingTransId ==
            pGetCxFailureNotify->ulTransactionID.QuadPart)) {

            pVmCxSessionCtxt->vmCxSession.ullNumFailedTagsReported =
                pVmCxSessionCtxt->vmCxSession.ullNumPendTagFail;
            pVmCxSessionCtxt->vmCxSession.ullPendingTransId = 0;

            // There can have been one more jump before user mode committed earlier timestmap
            if (pVmCxSessionCtxt->vmCxSession.ullPendingTimeJumpTS ==
                pVmCxSessionCtxt->vmCxSession.TimeJumpTS) {
                pVmCxSessionCtxt->vmCxSession.TimeJumpTS = 0;
                pVmCxSessionCtxt->vmCxSession.ullTimeJumpInMS = 0;
                CLEAR_FLAG(pVmCxSessionCtxt->vmCxSession.ullFlags, VM_CXSTATUS_TIMEJUMP_BCKWD_FLAG);
                CLEAR_FLAG(pVmCxSessionCtxt->vmCxSession.ullFlags, VM_CXSTATUS_TIMEJUMP_FWD_FLAG);
            }
        }
    }
    pVmCxSessionCtxt->ullLastUserTransactionId++;
    prevCxNotifyIrp = DriverContext.vmCxSessionContext.cxNotifyIrp;
    DriverContext.vmCxSessionContext.cxNotifyIrp = NULL;

    pVmCxSessionCtxt->ullDiskPeakChurnThresoldInBytes = pGetCxFailureNotify->ullMaxDiskChurnSupportedMBps * (1024 * 1024);
    pVmCxSessionCtxt->ullVmPeakChurnThresoldInBytes = pGetCxFailureNotify->ullMaxVMChurnSupportedMBps * (1024 * 1024);
    pVmCxSessionCtxt->ullMinConsTagFail = pGetCxFailureNotify->ullMinConsecutiveTagFailures;
    pVmCxSessionCtxt->ullMaxAccpetableTimeJumpFWDInMS = pGetCxFailureNotify->ullMaximumTimeJumpFwdAcceptableInMs;
    pVmCxSessionCtxt->ullMaxAccpetableTimeJumpBKWDInMS = pGetCxFailureNotify->ullMaximumTimeJumpBwdAcceptableInMs;
    KeReleaseSpinLock(&pVmCxSessionCtxt->CxSessionLock, oldIrql);


    // Cancel earlier pending IRP.
    if (prevCxNotifyIrp) {
        IoReleaseCancelSpinLock(prevCxNotifyIrp->CancelIrql);
        prevCxNotifyIrp->IoStatus.Status = STATUS_CANCELLED;
        IoCompleteRequest(prevCxNotifyIrp, IO_NO_INCREMENT);
    }

    // Check if we can complete this IRP.
    if (CopyVMCxContextDataIfCxConditionsAreSet(pVmCxFailureStats, TRUE)){
        Status = CopyDiskCxSessionData(pVmCxFailureStats, ulOutputBufferLength);
        Irp->IoStatus.Information = (STATUS_SUCCESS == Status)? ulOutputBufferLength : 0;
        goto cleanup;
    }

    // We don't have enough data to complete this IRP currently
    KeAcquireSpinLock(&pVmCxSessionCtxt->CxSessionLock, &oldIrql);
    IoSetCancelRoutine(Irp, InDskFltCancelCxNotifyIrp);

    // Check if IRP is already cancelled.
    if (Irp->Cancel && IoSetCancelRoutine(Irp, NULL))
    {
        Status = STATUS_CANCELLED;
        KeReleaseSpinLock(&pVmCxSessionCtxt->CxSessionLock, oldIrql);
        goto cleanup;
    }

    IoMarkIrpPending(Irp);
    Status = STATUS_PENDING;
    pVmCxSessionCtxt->cxNotifyIrp = Irp;
    KeReleaseSpinLock(&pVmCxSessionCtxt->CxSessionLock, oldIrql);

cleanup:
    if (STATUS_PENDING != Status) {
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    }

    // Now any new GetCxFailure Stats IOCTL can be handled.
    KeAcquireSpinLock(&pVmCxSessionCtxt->CxSessionLock, &oldIrql);
    DriverContext.vmCxSessionContext.isCxIrpInProgress = FALSE;
    KeReleaseSpinLock(&pVmCxSessionCtxt->CxSessionLock, oldIrql);

    return Status;
}

// Cancellation routine for IOCTL_INMAGE_GET_CXSTATS_NOTIFY.
_Use_decl_annotations_
VOID
InDskFltCancelCxNotifyIrp(
    _Inout_                     PDEVICE_OBJECT  DeviceObject,
    _Inout_ _IRQL_uses_cancel_  PIRP            Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    KIRQL                       irql;
    PVM_CX_SESSION_CONTEXT      pVmCxSessionCtxt = NULL;

    pVmCxSessionCtxt = &DriverContext.vmCxSessionContext;

    IoReleaseCancelSpinLock(Irp->CancelIrql);

    // There can be multiple sceanarios here
    // 1. Cancellation routine is called
    // 2. Before first monitoring thread has exit, second monitoring thread has come.
    // above2 steps can race
    // To handle this race following is the logic
    // Thread which cancels cxNotifyIrp resets cxNotifyIrp to NULL.
    // So first check that happens here 
    //      to validate if global and current IRP is same
    //      If these 2 are different, it means another thread has already cancelled Irp or,
    //                      is in process of cancelling IRP
    KeAcquireSpinLock(&pVmCxSessionCtxt->CxSessionLock, &irql);
    if (Irp != DriverContext.vmCxSessionContext.cxNotifyIrp) {
        KeReleaseSpinLock(&pVmCxSessionCtxt->CxSessionLock, irql);
        return;
    }
    DriverContext.vmCxSessionContext.cxNotifyIrp = NULL;
    KeReleaseSpinLock(&pVmCxSessionCtxt->CxSessionLock, irql);

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = STATUS_CANCELLED;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return;
}

NTSTATUS
CopyDiskCxSessionData(
    _Inout_ PVM_CXFAILURE_STATS     pVmCxFailureStats,
    IN      ULONG                   ulOutputLength
)
{
    KIRQL                   oldIrql;
    PDEV_CONTEXT            pDevContext = NULL;
    PLIST_ENTRY             pDevExtList;
    ULONG                   ulNumDevices = 0;
    NTSTATUS                Status = STATUS_SUCCESS;
    PVM_CX_SESSION_CONTEXT  pVmCxSessionCtxt = NULL;
    PVM_CX_SESSION_DATA     pVmCxSessionData = NULL;

    pVmCxSessionCtxt = &DriverContext.vmCxSessionContext;
    pVmCxSessionData = &pVmCxSessionCtxt->vmCxSession;

    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
    pDevExtList = DriverContext.HeadForDevContext.Flink;

    for (; pDevExtList != &DriverContext.HeadForDevContext; pDevExtList = pDevExtList->Flink)
    {
        pDevContext = (PDEV_CONTEXT)pDevExtList;
        KeAcquireSpinLockAtDpcLevel(&pDevContext->Lock);

        // Ignore uninitialized disks
        if (!TEST_FLAG(pDevContext->ulFlags, DCF_DEVID_OBTAINED)) {
            KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
            continue;
        }

        // Ignore following disks
        //      Disk which has conflicting disk id
        //      Disk which are in removal path
        if (TEST_FLAG(pDevContext->ulFlags, DCF_DISKID_CONFLICT |
            DCF_SURPRISE_REMOVAL |
            DCF_REMOVE_CLEANUP_PENDING |
            DCF_FILTERING_STOPPED)) {
            KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
            continue;
        }

        KeAcquireSpinLockAtDpcLevel(&pVmCxSessionCtxt->CxSessionLock);
        // For this disk there is no active CX session
        if (!pDevContext->CxSession.hasCxSession ||
            (pDevContext->CxSession.ullSessionId != pVmCxSessionCtxt->ullSessionId))
        {
            KeReleaseSpinLockFromDpcLevel(&pVmCxSessionCtxt->CxSessionLock);
            KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
            continue;
        }

        ++ulNumDevices;

        if (ulOutputLength < (sizeof(VM_CXFAILURE_STATS) + (ulNumDevices - 1) * sizeof(DEVICE_CXFAILURE_STATS))) {
            Status = STATUS_BUFFER_OVERFLOW;
            KeReleaseSpinLockFromDpcLevel(&pVmCxSessionCtxt->CxSessionLock);
            KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
            break;
        }

        pVmCxFailureStats->ullNumDisks = ulNumDevices;
        CopyDeviceCxStatus(pDevContext, &pVmCxFailureStats->DeviceCxStats[ulNumDevices - 1]);
        Status = STATUS_SUCCESS;

        KeReleaseSpinLockFromDpcLevel(&pVmCxSessionCtxt->CxSessionLock);
        KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
    }

    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);

    KeAcquireSpinLock(&pVmCxSessionCtxt->CxSessionLock, &oldIrql);
    if (STATUS_SUCCESS == Status)
    {
        pVmCxFailureStats->ulTransactionID.QuadPart = pVmCxSessionCtxt->ullLastUserTransactionId;
        pVmCxSessionData->ullPendingTransId = pVmCxSessionCtxt->ullLastUserTransactionId;
        pVmCxSessionData->ullNumPendTagFail = pVmCxSessionData->ullConsTagFail;
        pVmCxSessionData->ullPendingTimeJumpTS = pVmCxSessionData->TimeJumpTS;
    }
    KeReleaseSpinLock(&pVmCxSessionCtxt->CxSessionLock, oldIrql);

    return Status;
}

NTSTATUS
ValidateGetCxFailureNotifyInput(
    IN  PGET_CXFAILURE_NOTIFY   pGetCxFailureNotify,
    IN  ULONG                   ulInputLength,
    IN  ULONG                   ulOutputLength
)
{
    PLIST_ENTRY     pDevExtList;
    NTSTATUS        Status = STATUS_SUCCESS;
    KIRQL           oldIrql;
    PDEV_CONTEXT    pDevContext = NULL;
    WCHAR           wDeviceID[GUID_SIZE_IN_CHARS + 2] = { 0 };
    BOOLEAN         bDeviceFound = FALSE;
    ULONG           ulNumDevices = 0;
    ULONG           ulNumUnusedDevicesInInput = 0;
    ULONG           ulInputBufferSizeNeeded = 0;
    ULONG           ulOutputBufferSizeNeeded = 0;
    ULONG           ulNumDevicesNotFoundInInput = 0;

    // Validate input parameters
    if ((NULL == pGetCxFailureNotify) || 
        (0 == pGetCxFailureNotify->ullMaxDiskChurnSupportedMBps) ||
        (0 == pGetCxFailureNotify->ullMaximumTimeJumpBwdAcceptableInMs) ||
        (0 == pGetCxFailureNotify->ullMaximumTimeJumpFwdAcceptableInMs) ||
        (0 == pGetCxFailureNotify->ullMaxVMChurnSupportedMBps) ||
        (0 == pGetCxFailureNotify->ulNumberOfOutputDisks) ||
        (0 == pGetCxFailureNotify->ulNumberOfProtectedDisks)) {
        Status = STATUS_INVALID_PARAMETER;
        goto cleanup;
    }

    RtlZeroMemory(wDeviceID, sizeof(wDeviceID));

    ulInputBufferSizeNeeded = sizeof(GET_CXFAILURE_NOTIFY) + 
        (pGetCxFailureNotify->ulNumberOfOutputDisks - 1) * GUID_SIZE_IN_BYTES;

    if (ulInputLength < ulInputBufferSizeNeeded) {
        Status = STATUS_INFO_LENGTH_MISMATCH;
        goto cleanup;
    }

    ulOutputBufferSizeNeeded = sizeof(VM_CXFAILURE_STATS) +
        (pGetCxFailureNotify->ulNumberOfOutputDisks - 1) * sizeof(DEVICE_CXFAILURE_STATS);

    if (ulOutputLength < ulOutputBufferSizeNeeded) {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto cleanup;
    }

    for (ULONG ul = 0; ul < pGetCxFailureNotify->ulNumberOfOutputDisks; ul++) {
        if (sizeof(pGetCxFailureNotify->DeviceIdList[ul]) == 
            RtlCompareMemory(pGetCxFailureNotify->DeviceIdList[ul], 
                             wDeviceID,
                             sizeof(pGetCxFailureNotify->DeviceIdList[ul]))) {
            ++ulNumUnusedDevicesInInput;
        }
    }

    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
    pDevExtList = DriverContext.HeadForDevContext.Flink;

    for (; pDevExtList != &DriverContext.HeadForDevContext; pDevExtList = pDevExtList->Flink)
    {
        pDevContext = (PDEV_CONTEXT)pDevExtList;
        KeAcquireSpinLockAtDpcLevel(&pDevContext->Lock);

        // Ignore uninitialized disks
        if (!TEST_FLAG(pDevContext->ulFlags, DCF_DEVID_OBTAINED)) {
            KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
            continue;
        }

        // Ignore following disks
        //      Disk which has conflicting disk id
        //      Disk which are in removal path
        if (TEST_FLAG(pDevContext->ulFlags, DCF_DISKID_CONFLICT | 
                                            DCF_REMOVE_CLEANUP_PENDING |
                                            DCF_FILTERING_STOPPED)) {
            KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
            continue;
        }

        ++ulNumDevices;

        if (pGetCxFailureNotify->ulNumberOfOutputDisks < ulNumDevices) {
            Status = STATUS_BUFFER_OVERFLOW;
            KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
            break;
        }

        RtlZeroMemory(wDeviceID, sizeof(wDeviceID));

        RtlCopyMemory(wDeviceID, pDevContext->wDevID, sizeof(pDevContext->wDevID));
        ConvertGUIDtoLowerCase(wDeviceID);

        bDeviceFound = FALSE;

        for (ULONG ul = 0; ul < pGetCxFailureNotify->ulNumberOfOutputDisks; ul++) {
            if (sizeof(pGetCxFailureNotify->DeviceIdList[ul]) == 
                        RtlCompareMemory(wDeviceID, 
                                         pGetCxFailureNotify->DeviceIdList[ul],
                                         sizeof(pGetCxFailureNotify->DeviceIdList[ul]))){
                bDeviceFound = TRUE;
                break;
            }
        }
        KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);

        // We have got a disk which is filtered but this is not included in input disk list.
        // If we have additional ununsed disks in input list otherwise
        // We will return an error indicating missing disk.
        if (!bDeviceFound) {
            ++ulNumDevicesNotFoundInInput;
            if (ulNumDevicesNotFoundInInput > ulNumUnusedDevicesInInput) {
                Status = STATUS_BUFFER_OVERFLOW;
                break;
            }
        }
    }
    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);

cleanup:
    return Status;
}

BOOLEAN
CopyVMCxContextDataIfCxConditionsAreSet(
    _Inout_     PVM_CXFAILURE_STATS pVmCxFailureStats,
    IN          BOOLEAN             bAcquireLock
)
{
    KIRQL                   oldIrql = 0;
    PVM_CX_SESSION_CONTEXT  pVmCxSessionCtxt = &DriverContext.vmCxSessionContext;
    PVM_CX_SESSION_DATA     pVmCxSessionData = &pVmCxSessionCtxt->vmCxSession;
    BOOLEAN                 bReportTimeJump = FALSE;

    AcquireLockIf(bAcquireLock, &pVmCxSessionCtxt->CxSessionLock, &oldIrql);
    bReportTimeJump = TEST_FLAG(pVmCxSessionData->ullFlags, VM_CXSTATUS_TIMEJUMP_BCKWD_FLAG |
                                                            VM_CXSTATUS_TIMEJUMP_FWD_FLAG);

    if (!bReportTimeJump) {
        if (CxSessionOver != pVmCxSessionData->cxState) {
            ReleaseLockIf(bAcquireLock, &pVmCxSessionCtxt->CxSessionLock, oldIrql);
            return FALSE;
        }

        // Check if we saw a product issue
        if ((pVmCxSessionData->ullNumS2Quits > 0) || (pVmCxSessionData->ullNumSvagentsQuit > 0)) {
            ReleaseLockIf(bAcquireLock, &pVmCxSessionCtxt->CxSessionLock, oldIrql);
            return FALSE;
        }

        if ((pVmCxSessionData->ullConsTagFail - pVmCxSessionData->ullNumFailedTagsReported) <
            pVmCxSessionCtxt->ullMinConsTagFail) {
            ReleaseLockIf(bAcquireLock, &pVmCxSessionCtxt->CxSessionLock, oldIrql);
            return FALSE;
        }
    }

    RtlZeroMemory(pVmCxFailureStats, sizeof(*pVmCxFailureStats));

    pVmCxFailureStats->ullFlags = pVmCxSessionData->ullFlags;

    for (int i = 0; i < ARRAYSIZE(pVmCxSessionData->ChurnBucketsMBps); i++) {
        pVmCxFailureStats->ChurnBucketsMBps[i] = pVmCxSessionData->ChurnBucketsMBps[i];

        if (0 == pVmCxSessionData->ChurnBucketsMBps[i]) {
            continue;
        }

        if (a_ullVMMaximumChurnPerBuckets[i] > pVmCxSessionCtxt->ullVmPeakChurnThresoldInBytes) {
            SET_FLAG(pVmCxFailureStats->ullFlags, VM_CXSTATUS_PEAKCHURN_FLAG);
            if (0 == pVmCxFailureStats->firstPeakChurnTS) {
                pVmCxFailureStats->firstPeakChurnTS = pVmCxSessionData->FirstChurnTSBuckets[i];
            }
            else if (pVmCxSessionData->FirstChurnTSBuckets[i] < pVmCxFailureStats->firstPeakChurnTS) {
                pVmCxFailureStats->firstPeakChurnTS = pVmCxSessionData->FirstChurnTSBuckets[i];
            }

            if (0 == pVmCxFailureStats->lastPeakChurnTS) {
                pVmCxFailureStats->lastPeakChurnTS = pVmCxSessionData->LastChurnTSBuckets[i];
            }
            else if (pVmCxSessionData->LastChurnTSBuckets[i] > pVmCxFailureStats->lastPeakChurnTS) {
                pVmCxFailureStats->lastPeakChurnTS = pVmCxSessionData->LastChurnTSBuckets[i];
            }
        }
    }

    for (int i = 0; i < ARRAYSIZE(pVmCxSessionData->ExcessChurnBucketsMBps); i++) {
        pVmCxFailureStats->ExcessChurnBucketsMBps[i] = pVmCxSessionData->ExcessChurnBucketsMBps[i];
    }

    pVmCxFailureStats->CxStartTS = pVmCxSessionData->ullCxStartTS;
    pVmCxFailureStats->CxEndTS = pVmCxSessionData->ullCxEndTS;
    pVmCxFailureStats->firstPeakChurnTS = pVmCxSessionData->firstPeakChurnTS;
    pVmCxFailureStats->lastPeakChurnTS = pVmCxSessionData->lastPeakChurnTS;
    pVmCxFailureStats->ullMaxDiffChurnThroughputInBytes = pVmCxSessionData->ullMaxDiffChurnThroughputInBytes;
    pVmCxFailureStats->ullNumOfConsecutiveTagFailures = pVmCxSessionData->ullConsTagFail;
    pVmCxFailureStats->ullMaxChurnThroughputTS = pVmCxSessionData->ullMaxChurnThroughputTS;

    pVmCxFailureStats->ullMaximumPeakChurnInBytes = pVmCxSessionData->ullMaximumPeakChurnInBytes;
    pVmCxFailureStats->ullDiffChurnThroughputInBytes = pVmCxSessionData->ullDiffChurnThroughputInBytes;

    pVmCxFailureStats->TimeJumpTS = pVmCxSessionData->TimeJumpTS;
    pVmCxFailureStats->ullTimeJumpInMS = pVmCxSessionData->ullTimeJumpInMS;
    pVmCxFailureStats->ullMaxS2LatencyInMS = pVmCxSessionData->ullMaxS2LatencyInMS;

    CalculateExcessChurn((PULONGLONG) a_ullVMMaximumChurnPerBuckets,
                          pVmCxSessionData->ChurnBucketsMBps,
                          ARRAYSIZE(pVmCxSessionData->ChurnBucketsMBps),
                          pVmCxSessionCtxt->ullVmPeakChurnThresoldInBytes,
                          &pVmCxFailureStats->ullTotalExcessChurnInBytes);

    if (pVmCxFailureStats->ullTotalExcessChurnInBytes > 0) {
        SET_FLAG(pVmCxFailureStats->ullFlags, VM_CXSTATUS_EXCESS_CHURNBUCKETS_FLAG);
    }
    ReleaseLockIf(bAcquireLock, &pVmCxSessionCtxt->CxSessionLock, oldIrql);

    return TRUE;
}

// This function should be called inside
// DevContext and VmContext Spin lock
VOID
CopyDeviceCxStatus(
IN          PDEV_CONTEXT                pDevContext,
_Inout_     PDEVICE_CXFAILURE_STATS     pDeviceCxFailureStats
)
{
    PDEVICE_CX_SESSION  pDevCxSession = &pDevContext->CxSession;

    ASSERT(FALSE == KeTryToAcquireSpinLockAtDpcLevel(&pDevContext->Lock));
    ASSERT(FALSE == KeTryToAcquireSpinLockAtDpcLevel(&DriverContext.vmCxSessionContext.CxSessionLock));

    RtlZeroMemory(pDeviceCxFailureStats->DeviceId, sizeof(pDeviceCxFailureStats->DeviceId));
    RtlCopyMemory(pDeviceCxFailureStats->DeviceId, pDevContext->wDevID, wcslen(pDevContext->wDevID)*sizeof(WCHAR));

    pDeviceCxFailureStats->ullFlags = pDevCxSession->ullFlags;

    pDeviceCxFailureStats->CxStartTS = pDevCxSession->ullCxStartTS;
    pDeviceCxFailureStats->ullMaxDiffChurnThroughputTS = pDevCxSession->ullMaxChurnThroughputTS;
    pDeviceCxFailureStats->firstNwFailureTS = pDevCxSession->liFirstNwFailureTS.QuadPart;
    pDeviceCxFailureStats->lastNwFailureTS = pDevCxSession->liLastNwFailureTS.QuadPart;
    pDeviceCxFailureStats->firstPeakChurnTS = pDevCxSession->liFirstPeakChurnTS.QuadPart;
    pDeviceCxFailureStats->lastPeakChurnTS = pDevCxSession->liLastPeakChurnTS.QuadPart;
    pDeviceCxFailureStats->CxEndTS = pDevCxSession->liCxEndTS.QuadPart;

    pDeviceCxFailureStats->ullLastNWErrorCode = pDevCxSession->ullLastNWErrorCode;
    pDeviceCxFailureStats->ullMaximumPeakChurnInBytes = pDevCxSession->ullMaximumPeakChurnInBytes;
    pDeviceCxFailureStats->ullDiffChurnThroughputInBytes = pDevCxSession->ullDiffChurnThroughputInBytes;
    pDeviceCxFailureStats->ullMaxDiffChurnThroughputInBytes = pDevCxSession->ullMaxDiffChurnThroughputInBytes;
    pDeviceCxFailureStats->ullTotalNWErrors = pDevCxSession->ullTotalNWErrors;
    pDeviceCxFailureStats->ullMaxS2LatencyInMS = pDevCxSession->ullMaxS2LatencyInMS;

    for (int i = 0; i < ARRAYSIZE(pDevContext->CxSession.ChurnBucketsMBps); i++) {
        pDeviceCxFailureStats->ChurnBucketsMBps[i] = pDevCxSession->ChurnBucketsMBps[i];
        if (0 == pDevCxSession->ChurnBucketsMBps[i]) {
            continue;
        }

        if (a_ullDiskMaximumChurnPerBuckets[i] > DriverContext.vmCxSessionContext.ullDiskPeakChurnThresoldInBytes) {
            pDeviceCxFailureStats->ullFlags |= DISK_CXSTATUS_PEAKCHURN_FLAG;
            if (0 == pDeviceCxFailureStats->firstPeakChurnTS) {
                pDeviceCxFailureStats->firstPeakChurnTS = pDevCxSession->FirstChurnTSBuckets[i];
            }
            else if (pDevCxSession->FirstChurnTSBuckets[i] < pDeviceCxFailureStats->firstPeakChurnTS) {
                pDeviceCxFailureStats->firstPeakChurnTS = pDevCxSession->FirstChurnTSBuckets[i];
            }

            if (0 == pDeviceCxFailureStats->lastPeakChurnTS) {
                pDeviceCxFailureStats->lastPeakChurnTS = pDevCxSession->LastChurnTSBuckets[i];
            }
            else if (pDevCxSession->LastChurnTSBuckets[i] > pDeviceCxFailureStats->lastPeakChurnTS) {
                pDeviceCxFailureStats->lastPeakChurnTS = pDevCxSession->LastChurnTSBuckets[i];
            }
        }
    }

    for (int i = 0; i < ARRAYSIZE(pDevContext->CxSession.ExcessChurnBucketsMBps); i++) {
        pDeviceCxFailureStats->ExcessChurnBucketsMBps[i] = pDevCxSession->ExcessChurnBucketsMBps[i];
    }

    CalculateExcessChurn((PULONGLONG)a_ullDiskMaximumChurnPerBuckets,
        pDevCxSession->ChurnBucketsMBps,
        ARRAYSIZE(pDevCxSession->ChurnBucketsMBps),
        DriverContext.vmCxSessionContext.ullDiskPeakChurnThresoldInBytes,
        &pDeviceCxFailureStats->ullTotalExcessChurnInBytes);

    if (pDeviceCxFailureStats->ullTotalExcessChurnInBytes > 0) {
        pDeviceCxFailureStats->ullFlags |= DISK_CXSTATUS_EXCESS_CHURNBUCKET_FLAG;
    }
}


// This function should be called inside devcontext
VOID
UpdateCXCountersAfterIo(
    IN DEV_CONTEXT*    DevContext,
    IN ULONG            ulIoSize
)
{
    ASSERT(FALSE == KeTryToAcquireSpinLockAtDpcLevel(&DevContext->Lock));

    LARGE_INTEGER           liTimeStamp;
    PVM_CX_SESSION_CONTEXT  pVmCxSessionCtxt = NULL;
    PVM_CX_SESSION_DATA     pVmCxSessionData = NULL;

    pVmCxSessionCtxt = &DriverContext.vmCxSessionContext;
    pVmCxSessionData = &pVmCxSessionCtxt->vmCxSession;

    KeAcquireSpinLockAtDpcLevel(&pVmCxSessionCtxt->CxSessionLock);
    if ((CxSessionInvalid == pVmCxSessionData->cxState) ||
        (CxSessionOver == pVmCxSessionData->cxState)){
        KeReleaseSpinLockFromDpcLevel(&pVmCxSessionCtxt->CxSessionLock);
        return;
    }

    // Ignore updating counters when number when not all filtered disks
    // are in non-write order state.
    if (pVmCxSessionCtxt->ulNumFilteredDisks != pVmCxSessionCtxt->ulDisksInDataMode){
        KeReleaseSpinLockFromDpcLevel(&pVmCxSessionCtxt->CxSessionLock);
        return;
    }

    // If there is no VM session
    // A new session cannot start until disk hits cx session thresold
    if ((CxSessionInProgress != pVmCxSessionData->cxState) &&
        (DevContext->ChangeList.ullcbDataChangesPending < pVmCxSessionCtxt->ullDiskDiffThroughputThresoldInBytes)) {
        KeReleaseSpinLockFromDpcLevel(&pVmCxSessionCtxt->CxSessionLock);
        return;
    }

    // Query current system time
    KeQuerySystemTime(&liTimeStamp);

    if (DevContext->ChangeList.ullcbDataChangesPending >= pVmCxSessionCtxt->ullDiskDiffThroughputThresoldInBytes) {
        if (!DevContext->CxSession.hasCxSession ||
            (DevContext->CxSession.ullSessionId != pVmCxSessionCtxt->ullSessionId))
        {
            if (DevContext->CxSession.ullSessionId != pVmCxSessionCtxt->ullSessionId) {
                RtlZeroMemory(&DevContext->CxSession, sizeof(DevContext->CxSession));
                DevContext->CxSession.ullSessionId = pVmCxSessionCtxt->ullSessionId;
            }

            DevContext->CxSession.hasCxSession = TRUE;
            DevContext->CxSession.ullCxStartTS = liTimeStamp.QuadPart;
            DevContext->CxSession.uliIOTimeinSec.QuadPart = liTimeStamp.QuadPart;
            DevContext->CxSession.ullChurnInBytesPerSecond = 0;
            ++pVmCxSessionData->ulNumDiskCxSessionInProgress;


            if (CxSessionInProgress != pVmCxSessionData->cxState) {
                pVmCxSessionData->ullCxStartTS = liTimeStamp.QuadPart;
                pVmCxSessionData->cxState = CxSessionInProgress;
                // TODO: Logging Pending
                //InDskFltWriteEventWithDevCtxt(INFLTDRV_INFO_VM_CXSESSION_START, DevContext, pVmCxSessionCtxt->ullSessionId, liTimeStamp.QuadPart);
            }
            else {
                // TODO: Logging Pending
                //InDskFltWriteEventWithDevCtxt(INFLTDRV_INFO_DISK_CXSESSION_START, DevContext, pVmCxSessionCtxt->ullSessionId, liTimeStamp.QuadPart);
            }
        }
    }

    if (DevContext->CxSession.hasCxSession) {
        if (DevContext->ChangeList.ullcbDataChangesPending > DevContext->CxSession.ullMaxDiffChurnThroughputInBytes) {
            DevContext->CxSession.ullMaxDiffChurnThroughputInBytes = DevContext->ChangeList.ullcbDataChangesPending;
            DevContext->CxSession.ullMaxChurnThroughputTS = liTimeStamp.QuadPart;
        }

        if ((liTimeStamp.QuadPart - DevContext->CxSession.uliIOTimeinSec.QuadPart) > HUNDRED_NANO_SECS_IN_SEC) {
            // Put it in Disk churn bucket
            ULONG ulMaxBucketIndex = ARRAYSIZE(a_ullDiskMaximumChurnPerBuckets) - 1;
            for (int i = ulMaxBucketIndex; i >= 0; i--) {
                if (DevContext->CxSession.ullChurnInBytesPerSecond > a_ullDiskMaximumChurnPerBuckets[i]){
                    DevContext->CxSession.ChurnBucketsMBps[i]++;
                    DevContext->CxSession.ExcessChurnBucketsMBps[i] +=
                        (DevContext->CxSession.ullChurnInBytesPerSecond - a_ullDiskMaximumChurnPerBuckets[i]);
                    if (0 == DevContext->CxSession.FirstChurnTSBuckets[i]) {
                        DevContext->CxSession.FirstChurnTSBuckets[i] = liTimeStamp.QuadPart;
                    }
                    DevContext->CxSession.LastChurnTSBuckets[i] = liTimeStamp.QuadPart;
                    break;
                }
            }

            if (DevContext->CxSession.ullChurnInBytesPerSecond > DevContext->CxSession.ullMaximumPeakChurnInBytes) {
                 DevContext->CxSession.ullMaximumPeakChurnInBytes = DevContext->CxSession.ullChurnInBytesPerSecond;
            }

            DevContext->CxSession.uliIOTimeinSec.QuadPart = liTimeStamp.QuadPart;
            DevContext->CxSession.ullChurnInBytesPerSecond = 0;
        }

        DevContext->CxSession.ullChurnInBytesPerSecond += ulIoSize;
    }

    if ((liTimeStamp.QuadPart - pVmCxSessionData->uliIOTimeinSec.QuadPart) > HUNDRED_NANO_SECS_IN_SEC)
    {
        ULONG ulMaxBucketIndex = ARRAYSIZE(a_ullVMMaximumChurnPerBuckets) - 1;
        for (int i = ulMaxBucketIndex; i >= 0; i--) {
            if (pVmCxSessionData->ullChurnInBytesPerSecond > a_ullVMMaximumChurnPerBuckets[i]){
                pVmCxSessionData->ChurnBucketsMBps[i]++;
                pVmCxSessionData->ExcessChurnBucketsMBps[i] +=
                    pVmCxSessionData->ullChurnInBytesPerSecond - a_ullVMMaximumChurnPerBuckets[i];
                if (0 == pVmCxSessionData->FirstChurnTSBuckets[i]) {
                    pVmCxSessionData->FirstChurnTSBuckets[i] = liTimeStamp.QuadPart;
                }
                pVmCxSessionData->LastChurnTSBuckets[i] = liTimeStamp.QuadPart;
                break;
            }
        }

        if (pVmCxSessionData->ullChurnInBytesPerSecond > pVmCxSessionData->ullMaximumPeakChurnInBytes) {
            pVmCxSessionData->ullMaximumPeakChurnInBytes = pVmCxSessionData->ullChurnInBytesPerSecond;
        }
        pVmCxSessionData->ullChurnInBytesPerSecond = 0;
        pVmCxSessionData->uliIOTimeinSec.QuadPart = liTimeStamp.QuadPart;
    }

    pVmCxSessionData->ullChurnInBytesPerSecond += ulIoSize;
    KeReleaseSpinLockFromDpcLevel(&pVmCxSessionCtxt->CxSessionLock);
}


VOID
UpdateCXCountersAfterCommit(
    IN  struct _DEV_CONTEXT*    DevContext
)
{
    ASSERT(NULL != DevContext);
    KIRQL      oldIrql;
    PVM_CX_SESSION_CONTEXT  pVmCxSessionCtxt = NULL;
    PVM_CX_SESSION_DATA     pVmCxSessionData = NULL;
    LARGE_INTEGER           liTimeStamp;

    KeAcquireSpinLock(&DevContext->Lock, &oldIrql);
    pVmCxSessionCtxt = &DriverContext.vmCxSessionContext;
    pVmCxSessionData = &pVmCxSessionCtxt->vmCxSession;

    KeAcquireSpinLockAtDpcLevel(&pVmCxSessionCtxt->CxSessionLock);
    if ((pVmCxSessionData->cxState != CxSessionInProgress) ||
        !DevContext->CxSession.hasCxSession)
    {
        KeReleaseSpinLockFromDpcLevel(&pVmCxSessionCtxt->CxSessionLock);
        KeReleaseSpinLock(&DevContext->Lock, oldIrql);
        return;
    }

    KeQuerySystemTime(&liTimeStamp);

    if (DevContext->ChangeList.ullcbDataChangesPending < pVmCxSessionCtxt->ullDiskDiffThroughputThresoldInBytes) {
        DevContext->CxSession.hasCxSession = FALSE;
        RtlZeroMemory(&DevContext->CxSession, sizeof(DevContext->CxSession));

        --pVmCxSessionData->ulNumDiskCxSessionInProgress;
        if (0 == pVmCxSessionData->ulNumDiskCxSessionInProgress) {
            RtlZeroMemory(pVmCxSessionData, sizeof(*pVmCxSessionData));
            pVmCxSessionCtxt->ullSessionId++;
            // TODO: Add event properly
            //InDskFltWriteEventWithDevCtxt(INFLTDRV_INFO_VM_CXSESSION_CLOSE, DevContext, pVmCxSessionCtxt->ullSessionId, liTimeStamp.QuadPart);
        }
        else {
            //TODO: Add event properly
            //InDskFltWriteEventWithDevCtxt(INFLTDRV_INFO_DISK_CXSESSION_CLOSE, DevContext, pVmCxSessionCtxt->ullSessionId, liTimeStamp.QuadPart);
        }
        DevContext->CxSession.ullSessionId = pVmCxSessionCtxt->ullSessionId;
    }

    KeReleaseSpinLockFromDpcLevel(&pVmCxSessionCtxt->CxSessionLock);
    KeReleaseSpinLock(&DevContext->Lock, oldIrql);
}

VOID
CxSessionStopFilteringOrRemoveDevice(struct _DEV_CONTEXT *pDevContext)
{
    PVM_CX_SESSION_CONTEXT  pVmCxSessionCtxt = NULL;
    PVM_CX_SESSION_DATA     pVmCxSessionData = NULL;
    LARGE_INTEGER          uliTimeStamp = { 0 };

    ASSERT(FALSE == KeTryToAcquireSpinLockAtDpcLevel(&pDevContext->Lock));

    pVmCxSessionCtxt = &DriverContext.vmCxSessionContext;
    pVmCxSessionData = &pVmCxSessionCtxt->vmCxSession;

    KeQuerySystemTime(&uliTimeStamp);
    KeAcquireSpinLockAtDpcLevel(&pVmCxSessionCtxt->CxSessionLock);
    // There is a window where stop filtering or, remove device
    // can decrement filtered disk list
    if (!pDevContext->isInVmCxSessionFilteredList) {
        KeReleaseSpinLockFromDpcLevel(&pVmCxSessionCtxt->CxSessionLock);
        return;
    }

    --pVmCxSessionCtxt->ulNumFilteredDisks;
    if (ecWriteOrderStateData == pDevContext->eWOState) {
        --pVmCxSessionCtxt->ulDisksInDataMode;
    }
    pDevContext->isInVmCxSessionFilteredList = FALSE;

    // We need to handle 2 conditions
    // 1. There is no cx session on this disk
    // 2. Disk CX session doesn't belong to this sessioon
    if (!pDevContext->CxSession.hasCxSession ||
        (pDevContext->CxSession.ullSessionId != pVmCxSessionCtxt->ullSessionId)) {
        KeReleaseSpinLockFromDpcLevel(&pVmCxSessionCtxt->CxSessionLock);
        return;
    }

    --pVmCxSessionData->ulNumDiskCxSessionInProgress;

    if (0 == pVmCxSessionData->ulNumDiskCxSessionInProgress) {
        RtlZeroMemory(pVmCxSessionData, sizeof(*pVmCxSessionData));
        ++pVmCxSessionCtxt->ullSessionId;
    }

    KeReleaseSpinLockFromDpcLevel(&pVmCxSessionCtxt->CxSessionLock);
}

// This function should be called inside Devcontext spinlock
VOID
EndOrCloseCxSessionIfNeeded(struct _DEV_CONTEXT *pDevContext)
{
    PVM_CX_SESSION_CONTEXT  pVmCxSessionCtxt = NULL;
    PVM_CX_SESSION_DATA     pVmCxSessionData = NULL;
    LARGE_INTEGER           liTimeStamp = { 0 };

    ASSERT(NULL != pDevContext);
    ASSERT(FALSE == KeTryToAcquireSpinLockAtDpcLevel(&pDevContext->Lock));

    pVmCxSessionCtxt = &DriverContext.vmCxSessionContext;
    pVmCxSessionData = &pVmCxSessionCtxt->vmCxSession;

    KeQuerySystemTime(&liTimeStamp);

    KeAcquireSpinLockAtDpcLevel(&pVmCxSessionCtxt->CxSessionLock);
    // There is a window where stop filtering or, remove device
    // can decrement filtered disk list
    if (!pDevContext->isInVmCxSessionFilteredList) {
        KeReleaseSpinLockFromDpcLevel(&pVmCxSessionCtxt->CxSessionLock);
        return;
    }

    if (pDevContext->ePrevWOState == pDevContext->eWOState) {
        KeReleaseSpinLockFromDpcLevel(&pVmCxSessionCtxt->CxSessionLock);
        return;
    }

    // Current disk has moved into data mode from non-wos state
    // It can trigger 2 actions
    // 1. increase number of disks in data mode
    // 2. If VM has a ended CX session, then if all disks have moved back to data mode
    //      It will be reset
    if (ecWriteOrderStateData == pDevContext->eWOState) {
        ++pVmCxSessionCtxt->ulDisksInDataMode;

        if ((pVmCxSessionData->cxState == CxSessionOver) &&
            (pVmCxSessionCtxt->ulDisksInDataMode == pVmCxSessionCtxt->ulNumFilteredDisks)){
            InDskFltWriteEventWithDevCtxt(INFLTDRV_INFO_VM_CXSESSION_CLOSE, pDevContext, pVmCxSessionCtxt->ullSessionId, liTimeStamp.QuadPart);
            RtlZeroMemory(pVmCxSessionData, sizeof(*pVmCxSessionData));
            ++pVmCxSessionCtxt->ullSessionId;
        }
        RtlZeroMemory(&pDevContext->CxSession, sizeof(pDevContext->CxSession));
        pDevContext->CxSession.ullSessionId = pVmCxSessionCtxt->ullSessionId;
        KeReleaseSpinLockFromDpcLevel(&pVmCxSessionCtxt->CxSessionLock);
        return;
    }

    // This is a mode switch between 2 non-write order states
    // Nothing to do
    if (ecWriteOrderStateData != pDevContext->ePrevWOState) {
        KeReleaseSpinLockFromDpcLevel(&pVmCxSessionCtxt->CxSessionLock);
        return;
    }

    // This is switch from wostate to non-wostate
    --pVmCxSessionCtxt->ulDisksInDataMode;

    // This is switch from WO state to non-wo state
    if (CxSessionInProgress == pVmCxSessionData->cxState) {
        pVmCxSessionData->cxState = CxSessionOver;
        pVmCxSessionData->ullCxEndTS = liTimeStamp.QuadPart;
        pVmCxSessionData->ullDiffChurnThroughputInBytes = pDevContext->ChangeList.ullcbDataChangesPending;
        if (pVmCxSessionData->ullDiffChurnThroughputInBytes > 0) {
            SET_FLAG(pVmCxSessionData->ullFlags, VM_CXSTATUS_CHURNTHROUGHPUT_FLAG);
        }
        InDskFltWriteEventWithDevCtxt(INFLTDRV_INFO_VM_CXSESSION_END, pDevContext, pVmCxSessionCtxt->ullSessionId, liTimeStamp.QuadPart);
    }

    if (pDevContext->CxSession.hasCxSession) {
        pDevContext->CxSession.liCxEndTS.QuadPart = liTimeStamp.QuadPart;
        pDevContext->CxSession.ullDiffChurnThroughputInBytes = pDevContext->ChangeList.ullcbDataChangesPending;
        if (pDevContext->CxSession.ullDiffChurnThroughputInBytes > 0) {
            SET_FLAG(pDevContext->CxSession.ullFlags, DISK_CXSTATUS_CHURNTHROUGHPUT_FLAG);
        }
        InDskFltWriteEventWithDevCtxt(INFLTDRV_INFO_DISK_CXSESSION_END, pDevContext, pVmCxSessionCtxt->ullSessionId, liTimeStamp.QuadPart);
    }
    KeReleaseSpinLockFromDpcLevel(&pVmCxSessionCtxt->CxSessionLock);
}

VOID CXSessionTagFailureNotify()
{
    KIRQL                   oldIrql;
    PIRP                    cxNotifyIrp = NULL;
    PVM_CX_SESSION_CONTEXT  pVmCxSessionCxtx = &DriverContext.vmCxSessionContext;
    PVM_CX_SESSION_DATA     pVmCxSessionData = &pVmCxSessionCxtx->vmCxSession;
    PVM_CXFAILURE_STATS     pVmCxFailureStats = NULL;
    NTSTATUS                Status = STATUS_SUCCESS;
    ULONG                   ulOutputLength = 0;
    PIO_STACK_LOCATION      IrpSp = NULL;

    // Check if there is a cx session that has ended.
    KeAcquireSpinLock(&pVmCxSessionCxtx->CxSessionLock, &oldIrql);
    if (CxSessionOver != pVmCxSessionData->cxState) {
        KeReleaseSpinLock(&pVmCxSessionCxtx->CxSessionLock, oldIrql);
        return;
    }

    ++pVmCxSessionData->ullConsTagFail;

    if (NULL == pVmCxSessionCxtx->cxNotifyIrp)
    {
        KeReleaseSpinLock(&pVmCxSessionCxtx->CxSessionLock, oldIrql);
        return;
    }

    pVmCxFailureStats = (PVM_CXFAILURE_STATS) pVmCxSessionCxtx->cxNotifyIrp->AssociatedIrp.SystemBuffer;
    if (!CopyVMCxContextDataIfCxConditionsAreSet(pVmCxFailureStats, FALSE)){
        KeReleaseSpinLock(&pVmCxSessionCxtx->CxSessionLock, oldIrql);
        return;
    }

    cxNotifyIrp = pVmCxSessionCxtx->cxNotifyIrp;
    pVmCxSessionCxtx->cxNotifyIrp = NULL;

    // Reset Cancel Routine
    IoSetCancelRoutine(cxNotifyIrp, NULL);

    IrpSp = IoGetCurrentIrpStackLocation(cxNotifyIrp);
    ulOutputLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;;
    KeReleaseSpinLock(&pVmCxSessionCxtx->CxSessionLock, oldIrql);

    Status = CopyDiskCxSessionData(pVmCxFailureStats, ulOutputLength);
    cxNotifyIrp->IoStatus.Status = Status;
    cxNotifyIrp->IoStatus.Information = (STATUS_SUCCESS == Status) ? ulOutputLength : 0;
    IoCompleteRequest(cxNotifyIrp, IO_NO_INCREMENT);
}

VOID CxNotifyProductIssue(IN ProductIssue issue)
{
    KIRQL                   oldIrql;
    PVM_CX_SESSION_CONTEXT  pVmCxSessionCxtx = &DriverContext.vmCxSessionContext;
    PVM_CX_SESSION_DATA     pVmCxSessionData = &pVmCxSessionCxtx->vmCxSession;

    KeAcquireSpinLock(&pVmCxSessionCxtx->CxSessionLock, &oldIrql);
    if ((pVmCxSessionData->cxState == CxSessionInProgress) || 
        (pVmCxSessionData->cxState == CxSessionOver)) {
        if (issue == ProductIssueS2Quit) {
            ++pVmCxSessionData->ullNumS2Quits;
        }
        else if (issue == ProductIssueSvAgentsQuit) {
            ++pVmCxSessionData->ullNumSvagentsQuit;
        }
    }
    KeReleaseSpinLock(&pVmCxSessionCxtx->CxSessionLock, oldIrql);
}

// This function should be called inside devcontext and vmsession spin lock
VOID



CalculateExcessChurn(
    PULONGLONG  a_MaxChurnBuckets,
    PULONGLONG  a_MaxChurnCountPerBucket,
    ULONG       ulNumBuckets,
    ULONGLONG   ullDiskPeakChurnThresoldInBytes,
    ULONGLONG   *pullTotalExcessChurnInBytes)
{
    ULONGLONG       ullExcessChurnForBucket = 0;
    *pullTotalExcessChurnInBytes = 0;

    for (ULONG ul = 0; ul < ulNumBuckets; ul++) {
        if (a_MaxChurnBuckets[ul] < ullDiskPeakChurnThresoldInBytes) {
            continue;
        }

        ullExcessChurnForBucket = a_ullDiskMaximumChurnPerBuckets[ul] - ullDiskPeakChurnThresoldInBytes;
        *pullTotalExcessChurnInBytes += ullExcessChurnForBucket * a_MaxChurnCountPerBucket[ul];
    }
}

VOID
CxNotifyTimeJump(
    IN  ULONGLONG   ullJumpDetectedAtTS,
    IN  ULONGLONG   ullTotalTimeJumpDetectedInMs,
    IN  BOOLEAN     bTimeJumpForward
)
{
    PVM_CX_SESSION_CONTEXT  pVmCxSessionCxtx = &DriverContext.vmCxSessionContext;
    PVM_CX_SESSION_DATA     pVmCxSessionData = &pVmCxSessionCxtx->vmCxSession;
    KIRQL                   oldIrql;
    PVM_CXFAILURE_STATS     pVmCxFailureStats = NULL;
    PIRP                    CxNotifyIrp = NULL;
    PIO_STACK_LOCATION      IrpSp;
    ULONG                   ulOutputLength;
    NTSTATUS                Status = STATUS_SUCCESS;

    KeAcquireSpinLock(&pVmCxSessionCxtx->CxSessionLock, &oldIrql);
    if (NULL == pVmCxSessionCxtx->cxNotifyIrp) {
        KeReleaseSpinLock(&pVmCxSessionCxtx->CxSessionLock, oldIrql);
        return;
    }

    // Check if we need to handle this time jump
    if ((bTimeJumpForward && (ullTotalTimeJumpDetectedInMs < pVmCxSessionCxtx->ullMaxAccpetableTimeJumpFWDInMS)) ||
        (!bTimeJumpForward && (ullTotalTimeJumpDetectedInMs < pVmCxSessionCxtx->ullMaxAccpetableTimeJumpBKWDInMS))) {
        KeReleaseSpinLock(&pVmCxSessionCxtx->CxSessionLock, oldIrql);
        return;
    }

    pVmCxSessionData->TimeJumpTS = ullJumpDetectedAtTS;
    pVmCxSessionData->ullTimeJumpInMS = ullTotalTimeJumpDetectedInMs;

    if (bTimeJumpForward) {
        SET_FLAG(pVmCxSessionData->ullFlags, VM_CXSTATUS_TIMEJUMP_FWD_FLAG);
    }
    else {
        SET_FLAG(pVmCxSessionData->ullFlags, VM_CXSTATUS_TIMEJUMP_BCKWD_FLAG);
    }

    pVmCxFailureStats = (PVM_CXFAILURE_STATS) pVmCxSessionCxtx->cxNotifyIrp->AssociatedIrp.SystemBuffer;
    if (!CopyVMCxContextDataIfCxConditionsAreSet(pVmCxFailureStats, FALSE)) {
        KeReleaseSpinLock(&pVmCxSessionCxtx->CxSessionLock, oldIrql);
        return;
    }

    CxNotifyIrp = pVmCxSessionCxtx->cxNotifyIrp;
    pVmCxSessionCxtx->cxNotifyIrp = NULL;
    IoSetCancelRoutine(CxNotifyIrp, NULL);
    IrpSp = IoGetCurrentIrpStackLocation(CxNotifyIrp);
    ulOutputLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;
    KeReleaseSpinLock(&pVmCxSessionCxtx->CxSessionLock, oldIrql);

    Status = CopyDiskCxSessionData(pVmCxFailureStats, ulOutputLength);
    CxNotifyIrp->IoStatus.Status = Status;
    CxNotifyIrp->IoStatus.Information = (STATUS_SUCCESS == Status) ? ulOutputLength : 0;
    IoCompleteRequest(CxNotifyIrp, IO_NO_INCREMENT);
}