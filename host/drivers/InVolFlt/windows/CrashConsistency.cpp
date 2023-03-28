/* ------------------------------------------------------------------------- *
*                                                                           *
*  Copyright (c) Microsoft Corporation                                      *
*                                                                           *
*  Module Name:  CrashConsistentcy.cpp                                      *
*                                                                           *
*  Abstract   :  Crash consistency routines.                                *
*                                                                           *
* ------------------------------------------------------------------------- */

#include "VContext.h"
#include "FltFunc.h"
#include "misc.h"

NTSTATUS
DeviceIoControlHoldWrites(
_In_        PDEVICE_OBJECT  DeviceObject,
_In_        PIRP            Irp
)
/*++

Routine Description:

This routine is the handler for IOCTL_INMAGE_HOLD_WRITES.
This routine does following
1) Validates Input Paramameter
2) Validate number of filtered devices.
3) Validate if already one crash tag is in process. Only one tag can be processed at anytime.
4) Creates IO barrier to hold all incoming writes till release has occured.
5) Sets Timer to make sure if protocol is not completed within time barrier is released.
6) Sets cancellation for this Irp. If caller crashes Io Barrier is released.

Arguments:

DeviceObject    - Supplies the device object.

Irp             - Supplies the IO request packet.

Return Value:

NTSTATUS

--*/
{
    NTSTATUS        Status;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    KIRQL           oldIrql = 0;
    ULONG           ulFlags;
    BOOLEAN         bValidContext = FALSE;
    LARGE_INTEGER   timeout = { 0 };
    PCRASH_CONSISTENT_HOLDWRITE_INPUT   pCrashTagInput = NULL;
    TAG_TELEMETRY_COMMON         TagCommonAttribs = { 0 };
    ULONG                        ulMessageType = ecMsgUninitialized;

    ASSERT(IOCTL_INMAGE_HOLD_WRITES == IrpSp->Parameters.DeviceIoControl.IoControlCode);
    ASSERT(DriverContext.ControlDevice == DeviceObject);

    UNREFERENCED_PARAMETER(DeviceObject);

    TagCommonAttribs.TagType = ecTagDistributedCrash;
    TagCommonAttribs.ulIoCtlCode = IrpSp->Parameters.DeviceIoControl.IoControlCode;
    TagCommonAttribs.usTotalNumDisk = (USHORT)DriverContext.ulNumDevs;
    INDSKFLT_SET_TAGREQUEST_TIME(&TagCommonAttribs.liTagRequestTime);

    // Validations are as follows
    // Input should atleast contain TagContext and Flags
    // Flags:
    //      1. Currently only all disks mode is supported. So this flag should be set
    //      2. If timeout is provided, it has to be non-Zero.
    pCrashTagInput = (PCRASH_CONSISTENT_HOLDWRITE_INPUT)Irp->AssociatedIrp.SystemBuffer;
    if (IrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof(CRASH_CONSISTENT_HOLDWRITE_INPUT))
    {
        ulMessageType = ecMsgCCInputBufferMismatch;
        Status = STATUS_INFO_LENGTH_MISMATCH;
        goto _cleanup;
    }

    RtlCopyMemory(TagCommonAttribs.TagMarkerGUID, pCrashTagInput->TagMarkerGUID, GUID_SIZE_IN_CHARS);
    TagCommonAttribs.TagMarkerGUID[GUID_SIZE_IN_CHARS] = '\0';

    if (0 == pCrashTagInput->ulTimeout)
    {
        ulMessageType = ecMsgInputZeroTimeOut;
        Status = STATUS_INVALID_PARAMETER;
        goto _cleanup;
    }

    timeout.QuadPart = MILLISECONDS_RELATIVE(pCrashTagInput->ulTimeout);

    ulFlags = pCrashTagInput->ulFlags;

    // Currently Crash consistency handles only all disks mode
    if (TAG_INFO_FLAGS_INCLUDE_ALL_FILTERED_DISKS != (ulFlags & TAG_INFO_FLAGS_INCLUDE_ALL_FILTERED_DISKS))
    {
        ulMessageType = ecMsgAllFilteredDiskFlagNotSet;
        Status = STATUS_NOT_SUPPORTED;
        goto _cleanup;
    }

    // Check if there is already one request in progress
    Status = InDskFltCompareExchangeTagState(pCrashTagInput->TagContext, ecTagNoneInProgress, ecTagIoPaused, &bValidContext);
    if (!NT_SUCCESS(Status))
    {
        ulMessageType = ecMsgCompareExchangeTagStateFailure;
        goto _cleanup;
    }

    // Create Io Barrier
    Status = ValidateAndCreateDeviceBarrier(&DriverContext.pTagInfo, false, ulFlags, &TagCommonAttribs);
    if (!NT_SUCCESS(Status))
    {
        ulMessageType = ecMsgValidateAndBarrierFailure;
        goto _cleanup;
    }

    if ((TAG_INFO_FLAGS_TAG_IF_ZERO_INFLIGHT_IO == (TAG_INFO_FLAGS_TAG_IF_ZERO_INFLIGHT_IO & ulFlags)) &&
        (0 != InmageGetNumInFlightIos()))
    {
        ulMessageType = ecMsgInFlightIO;
        Status = STATUS_DEVICE_BUSY;
        goto _cleanup;
    }

    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
    DriverContext.pTagProtocolHoldIrp = Irp;
    DriverContext.llNumQueuedIos = 0;
    DriverContext.llIoReleaseTimeInMicroSec = 0;
    DriverContext.TagType = ecTagDistributedCrash;

    IoSetCancelRoutine(Irp, InDskFltCancelCrashConsistentTag);

    // Check if IRP is already cancelled.
    if (Irp->Cancel && IoSetCancelRoutine(Irp, NULL))
    {
        ulMessageType = ecMsgTagIrpCancelled;
        Status = STATUS_CANCELLED;
    }
    else
    {
        IoMarkIrpPending(Irp);
        Status = STATUS_PENDING;

        // Set a timer with specified timeout
        KeSetTimer(&DriverContext.TagProtocolTimer,
            timeout,
            &DriverContext.TagProtocolTimerDpc);

    }

    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);

_cleanup:
    // Release all write held IRP
    if (!NT_SUCCESS(Status))
    {
        CXSessionTagFailureNotify();
		IndskFltValidateTagInsertion();
        KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
        InDskFltTelemetryTagIOCTLFailure(&TagCommonAttribs, Status, ulMessageType);
        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);

        if (bValidContext)
        {
            InDskFltReleaseWrites();
            InDskFltResetCrashContext();
        }

        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    }

    return Status;
}

NTSTATUS
DeviceIoControlDistributedCrashTag(
_In_        PDEVICE_OBJECT  DeviceObject,
_In_        PIRP            Irp
)
/*++

Routine Description:
    Handler for IOCTL_INMAGE_DISTRIBUTED_CRASH_TAG
    This function insert tags in the devices which are stored in pTagInfo in Hold Phase.
    Arguments:

    DeviceObject    - Supplies the device object.

    Irp             - Supplies the IO request packet.

    Return Value:

    NTSTATUS

--*/
{
    NTSTATUS        Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    BOOLEAN         bValidContext = FALSE;
    KIRQL           oldIrql;
    PCRASHCONSISTENT_TAG_INPUT pCrashTagInput;
    PCRASHCONSISTENT_TAG_OUTPUT pCrashTagOutput;
    ULONG           ulOutputBufferLength = 0;
    ULONG           ulTagBufferLength;
    ULONG           ulNumberOfDevices;
    ULONG           ulNumberOfDevicesTagsApplied;
    ULONG           ulMessageType = ecMsgUninitialized;
    PTAG_INFO       pTagInfo = NULL;

    ASSERT(IOCTL_INMAGE_DISTRIBUTED_CRASH_TAG == IrpSp->Parameters.DeviceIoControl.IoControlCode);
    ASSERT(DriverContext.ControlDevice == DeviceObject);
    UNREFERENCED_PARAMETER(DeviceObject);

    pCrashTagInput = (PCRASHCONSISTENT_TAG_INPUT)Irp->AssociatedIrp.SystemBuffer;
    ulTagBufferLength = IrpSp->Parameters.DeviceIoControl.InputBufferLength - FIELD_OFFSET(CRASHCONSISTENT_TAG_INPUT, TagBuffer);

    if ((IrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof(CRASHCONSISTENT_TAG_INPUT)) ||
        (IrpSp->Parameters.DeviceIoControl.OutputBufferLength < sizeof(CRASHCONSISTENT_TAG_OUTPUT)))
    {
        ulMessageType = ecMsgCCInputBufferMismatch;
        Status = STATUS_INFO_LENGTH_MISMATCH;
        goto _cleanup;
    }

    // Validate current tag context
    // Check if we are in right state to handle this IOCTL
    // Move from ecTagIoPaused to ecTagInsertTagStarted state
    Status = InDskFltCompareExchangeTagState(pCrashTagInput->TagContext, ecTagIoPaused, ecTagInsertTagStarted, &bValidContext);
    if (!NT_SUCCESS(Status))
    {
        ulMessageType = ecMsgCompareExchangeTagStateFailure;
        goto _cleanup;
    }

    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
    pTagInfo = DriverContext.pTagInfo;
    if (NULL != pTagInfo)
        pTagInfo->TagCommonAttribs.ulIoCtlCode = IrpSp->Parameters.DeviceIoControl.IoControlCode;
    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);

    DriverContext.pTagInfo->ulFlags = pCrashTagInput->ulFlags;

    Status = TagVolumesInSequence(DriverContext.pTagInfo, ((PUCHAR)pCrashTagInput + FIELD_OFFSET(CRASHCONSISTENT_TAG_INPUT, ulNumOfTags)));
    if (!NT_SUCCESS(Status))
    {
        ulMessageType = ecMsgTagVolumeInSequenceFailure;
        goto _cleanup;
    }

    // Check during tag if timer has expired or, cancellation routine is called
    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
    if (ecTagInsertTagStarted != DriverContext.TagProtocolState)
    {
        ulMessageType = ecMsgInValidTagProtocolState;
        Status = DriverContext.TagStatus;
        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
        goto _cleanup;
    }

    ulNumberOfDevices = DriverContext.pTagInfo->ulNumberOfDevices;
    ulNumberOfDevicesTagsApplied = DriverContext.pTagInfo->ulNumberOfDevicesTagsApplied;

    DriverContext.TagProtocolState = ecTagInsertTagCompleted;
    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);

    pCrashTagOutput = (CRASHCONSISTENT_TAG_OUTPUT*)Irp->AssociatedIrp.SystemBuffer;
    pCrashTagOutput->ulNumFilteredDisks = ulNumberOfDevices;
    pCrashTagOutput->ulNumTaggedDisks = ulNumberOfDevicesTagsApplied;
    ulOutputBufferLength = sizeof(CRASHCONSISTENT_TAG_OUTPUT);

_cleanup:

    // Release all write held IRP
    if (!NT_SUCCESS(Status))
    {
        if (bValidContext) {
            CXSessionTagFailureNotify();
            IndskFltValidateTagInsertion();
            InDskFltCrashConsistentTagCleanup(ecRevokeDistrubutedCrashInsertTag);

            KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
            pTagInfo = DriverContext.pTagInfo;
            if (NULL != pTagInfo)
                InDskFltTelemetryTagIOCTLFailure(&pTagInfo->TagCommonAttribs, Status, ulMessageType);
            KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
        } else {
            InDskFltWriteEvent(INDSKFLT_ERROR_TAG_IOCTL_FAILURE, Status, ulMessageType,
                IrpSp->Parameters.DeviceIoControl.IoControlCode);
        }
    }
    
    Irp->IoStatus.Information = ulOutputBufferLength;
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS
DeviceIoControlReleaseWrites(
_In_        PDEVICE_OBJECT  DeviceObject,
_In_        PIRP            Irp
)
/*++

Routine Description:
Handler for IOCTL_INMAGE_RELEASE_WRITES
This function releases writes hold in Hold Phase.
Arguments:

DeviceObject    - Supplies the device object.

Irp             - Supplies the IO request packet.

Return Value:

NTSTATUS

--*/
{
    NTSTATUS        Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    BOOLEAN         bValidContext = FALSE;
    LARGE_INTEGER   releaseStartTime = { 0 };
    LARGE_INTEGER   releaseEndTime = { 0 };
    LONGLONG        llTotalReleaseTime = 0;
    PCRASH_CONSISTENT_INPUT pCrashInput = NULL;
    KIRQL           oldIrql;
    ULONG           ulMessageType = ecMsgUninitialized;

    ASSERT(IOCTL_INMAGE_RELEASE_WRITES == IrpSp->Parameters.DeviceIoControl.IoControlCode);
    ASSERT(DriverContext.ControlDevice == DeviceObject);

    UNREFERENCED_PARAMETER(DeviceObject);

    pCrashInput = (PCRASH_CONSISTENT_INPUT)Irp->AssociatedIrp.SystemBuffer;
    if (IrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof(CRASH_CONSISTENT_INPUT))
    {
        ulMessageType = ecMsgCCInputBufferMismatch;
        Status = STATUS_INFO_LENGTH_MISMATCH;
        goto _cleanup;
    }

    // Validate current tag context
    Status = InDskFltCompareExchangeTagState(pCrashInput->TagContext, ecTagInsertTagCompleted, ecTagIoResumed, &bValidContext);
    if (!NT_SUCCESS(Status))
    {
        ulMessageType = ecMsgCompareExchangeTagStateFailure;
        goto _cleanup;
    }

    KeQuerySystemTime(&releaseStartTime);

    InDskFltReleaseWrites();

    KeQuerySystemTime(&releaseEndTime);

    llTotalReleaseTime = releaseEndTime.QuadPart - releaseStartTime.QuadPart;
    InterlockedExchange64(&DriverContext.llIoReleaseTimeInMicroSec, llTotalReleaseTime);

_cleanup:

    // Release all write held IRP
    if (!NT_SUCCESS(Status))
    {
        if (bValidContext) {
            CXSessionTagFailureNotify();
            IndskFltValidateTagInsertion();
            InDskFltCrashConsistentTagCleanup(ecRevokeDistrubutedCrashReleaseTag);

            KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
            PTAG_INFO pTagInfo = DriverContext.pTagInfo;
            if (NULL != pTagInfo)
                InDskFltTelemetryTagIOCTLFailure(&pTagInfo->TagCommonAttribs, Status, ulMessageType);
            KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
        } else {
            InDskFltWriteEvent(INDSKFLT_ERROR_TAG_IOCTL_FAILURE, Status, ulMessageType,
                IrpSp->Parameters.DeviceIoControl.IoControlCode);
        }
    }

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS
DeviceIoControlCommitDistributedTag(
_In_        PDEVICE_OBJECT  DeviceObject,
_In_        PIRP            Irp
)
/*++

Routine Description:
Handler for IOCTL_INMAGE_COMMIT_DISTRIBUTED_CRASH_TAG
This function commits or, revokes tag based upon flag.
Arguments:

DeviceObject    - Supplies the device object.

Irp             - Supplies the IO request packet.

Return Value:

NTSTATUS

--*/
{
    NTSTATUS        Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    BOOLEAN         bValidContext = FALSE;
    PCRASH_CONSISTENT_INPUT pCrashInput = NULL;
    BOOLEAN         bCommit = FALSE;
    ULONG           ulMessageType = ecMsgUninitialized;
    KIRQL           oldIrql;

    ASSERT(IOCTL_INMAGE_COMMIT_DISTRIBUTED_CRASH_TAG == IrpSp->Parameters.DeviceIoControl.IoControlCode);
    ASSERT(DriverContext.ControlDevice == DeviceObject);

    UNREFERENCED_PARAMETER(DeviceObject);

    pCrashInput = (PCRASH_CONSISTENT_INPUT) Irp->AssociatedIrp.SystemBuffer;
   
    if (IrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof(CRASH_CONSISTENT_INPUT))
    {
        ulMessageType = ecMsgCCInputBufferMismatch;
        Status = STATUS_INFO_LENGTH_MISMATCH;
        goto _cleanup;
    }

    // Validate current tag context
    Status = InDskFltCompareExchangeTagState(pCrashInput->TagContext, ecTagIoResumed, ecTagCommitStarted, &bValidContext);
    if (!NT_SUCCESS(Status))
    {
        ulMessageType = ecMsgCompareExchangeTagStateFailure;
        goto _cleanup;
    }

    bCommit = (TAG_INFO_FLAGS_COMMIT_TAG == (pCrashInput->ulFlags & TAG_INFO_FLAGS_COMMIT_TAG));

    // Check if timer has expired or, Cancel routine is called
    // There is a window where Timer has expired
    //  and Cleanup is executed.
    // Before starting Cleanup pTagProtocolHoldIrp is
    // reset to indicate that rundown has started.
    if (!InDskFltCancelProtocolHoldIrp(TRUE, TRUE))
    {
        Status = DriverContext.TagStatus;
        goto _cleanup;
    }

_cleanup:

    if (!NT_SUCCESS(Status)) {
        if (bValidContext) {
            CXSessionTagFailureNotify();
            KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
            PTAG_INFO pTagInfo = DriverContext.pTagInfo;
            if (NULL != pTagInfo) {
                pTagInfo->TagCommonAttribs.ulIoCtlCode = IrpSp->Parameters.DeviceIoControl.IoControlCode;
                InDskFltTelemetryTagIOCTLFailure(&pTagInfo->TagCommonAttribs, Status, ulMessageType);
            }
            KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
        } else {
            InDskFltWriteEvent(INDSKFLT_ERROR_TAG_IOCTL_FAILURE, Status, ulMessageType,
                IrpSp->Parameters.DeviceIoControl.IoControlCode);
        }
    }

    if (bValidContext)
    {
        etTagStateTriggerReason tagStateReason = ecRevokeCommitIOCTL;
        if (NT_SUCCESS(Status) && bCommit) {
            tagStateReason = ecNotApplicable;
        }
        FinalizeDeviceTagStateAll(DriverContext.pTagInfo, false, NT_SUCCESS(Status) && bCommit, tagStateReason);
        if (NT_SUCCESS(Status))
        {
            InDskFltResetCrashContext();
        }
        
    }

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return Status;
}

NTSTATUS
DeviceIoControlIoBarrierTagDisk(
_In_        PDEVICE_OBJECT  DeviceObject,
_In_        PIRP            Irp
)
/*++

Routine Description:
Handler for IOCTL_INMAGE_IOBARRIER_TAG_DISK
This function handles single node crash consistent tag.
Arguments:

DeviceObject    - Supplies the device object.

Irp             - Supplies the IO request packet.

Return Value:

NTSTATUS

--*/
{
    NTSTATUS                    Status;
    PIO_STACK_LOCATION          IrpSp = IoGetCurrentIrpStackLocation(Irp);
    ULONG                       ulTagInputBufferLength;
    ULONG                       ulCrashTagOutputLength = 0;
    BOOLEAN                     bValidContext = FALSE;
    PCRASHCONSISTENT_TAG_INPUT   pCrashTagInput = NULL;
    PCRASHCONSISTENT_TAG_OUTPUT  pCrashTagOutput = NULL;
    TAG_TELEMETRY_COMMON         TagCommonAttribs = { 0 };
    ULONG                        ulMessageType = ecMsgUninitialized;
    ULONG                        ulNumDevicesIgnored = 0;

    ASSERT(IOCTL_INMAGE_IOBARRIER_TAG_DISK == IrpSp->Parameters.DeviceIoControl.IoControlCode);
    ASSERT(DriverContext.ControlDevice == DeviceObject);

    UNREFERENCED_PARAMETER(DeviceObject);

    ulTagInputBufferLength = IrpSp->Parameters.DeviceIoControl.InputBufferLength - FIELD_OFFSET(CRASHCONSISTENT_TAG_INPUT, TagBuffer);

    pCrashTagInput = (PCRASHCONSISTENT_TAG_INPUT)Irp->AssociatedIrp.SystemBuffer;
    pCrashTagOutput = (PCRASHCONSISTENT_TAG_OUTPUT)Irp->AssociatedIrp.SystemBuffer;

    TagCommonAttribs.TagType = ecTagLocalCrash;
    TagCommonAttribs.ulIoCtlCode = IrpSp->Parameters.DeviceIoControl.IoControlCode;
    TagCommonAttribs.usTotalNumDisk = (USHORT)DriverContext.ulNumDevs;
    INDSKFLT_SET_TAGREQUEST_TIME(&TagCommonAttribs.liTagRequestTime);

    if ((IrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof(CRASHCONSISTENT_TAG_INPUT)) ||
        (IrpSp->Parameters.DeviceIoControl.OutputBufferLength < sizeof(CRASHCONSISTENT_TAG_OUTPUT)))
    {
        Status = STATUS_INFO_LENGTH_MISMATCH;
        ulMessageType = ecMsgCCInputBufferMismatch;
        goto _cleanup;
    }

    Status = IsValidBarrierIoctlTagDiskInputBuffer(pCrashTagInput->TagBuffer, ulTagInputBufferLength, pCrashTagInput->ulNumOfTags);
    if (!NT_SUCCESS(Status))
    {
        ulMessageType = ecMsgCCInvalidTagInputBuffer;
        goto _cleanup;
    }

    if (NULL != pCrashTagInput->TagBuffer)
        InDskFltGetTagMarkerGUID(pCrashTagInput->TagBuffer, TagCommonAttribs.TagMarkerGUID, GUID_SIZE_IN_CHARS, ulTagInputBufferLength);

    Status = InDskFltCompareExchangeTagState(pCrashTagInput->TagContext, ecTagNoneInProgress, ecTagInsertTagStarted, &bValidContext);
    if (!NT_SUCCESS(Status))
    {
        ulMessageType = ecMsgCompareExchangeTagStateFailure;
        goto _cleanup;
    }

    Status = ValidateAndCreateDeviceBarrier(&DriverContext.pTagInfo, FALSE, pCrashTagInput->ulFlags, &TagCommonAttribs);
    if (!NT_SUCCESS(Status))
    {
        ulMessageType = ecMsgValidateAndBarrierFailure;
        goto _cleanup;
    }

    // Check if tags can be inserted for all devices

    Status = TagVolumesInSequence(DriverContext.pTagInfo, (PUCHAR)pCrashTagInput + FIELD_OFFSET(CRASHCONSISTENT_TAG_INPUT, ulNumOfTags));
    if (!NT_SUCCESS(Status))
    {
        ulMessageType = ecMsgTagVolumeInSequenceFailure;
        goto _cleanup;
    }


    pCrashTagOutput->ulNumFilteredDisks = DriverContext.pTagInfo->ulNumberOfDevices;
    pCrashTagOutput->ulNumTaggedDisks = DriverContext.pTagInfo->ulNumberOfDevicesTagsApplied;
    ulCrashTagOutputLength = sizeof(CRASHCONSISTENT_TAG_OUTPUT);
_cleanup:

    // Release all write held IRP
    // Reset context for other guys to work
    if (bValidContext)
    {
        // If the filtering is stopped on any disk as part of the inserting the tag, below function will revoke the tag
        // By design, tag should be inserted in best effort manner for local crash tags, but to reason out this case
        // new tag state reason has added
        if (DriverContext.pTagInfo != NULL) {
            ulNumDevicesIgnored = DriverContext.pTagInfo->ulNumberOfDevicesIgnored;
            FinalizeDeviceTagStateAll(DriverContext.pTagInfo, FALSE, NT_SUCCESS(Status), ecRevokeLocalCrash);
        }
        InDskFltReleaseWrites();
        InDskFltResetCrashContext();
    }

    if (!NT_SUCCESS(Status)) {
        KIRQL oldIrql;
        KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
        InDskFltTelemetryTagIOCTLFailure(&TagCommonAttribs, Status, ulMessageType);
        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
        CXSessionTagFailureNotify();
    }
    else {
        if (ulNumDevicesIgnored > 0) {
            CXSessionTagFailureNotify();
        }
    }
    Irp->IoStatus.Information = ulCrashTagOutputLength;
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS
DeviceIoControlGetNumInflightIos(
_In_        PDEVICE_OBJECT  DeviceObject,
_In_        PIRP            Irp
)
{
    NTSTATUS            Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION  IrpSp = IoGetCurrentIrpStackLocation(Irp);
    ULONG               outputBufferLength;
    PULONG              outputBuffer = NULL;

    ASSERT(IOCTL_INMAGE_GET_NUM_INFLIGHT_IOS == IrpSp->Parameters.DeviceIoControl.IoControlCode);
    ASSERT(DriverContext.ControlDevice == DeviceObject);

    UNREFERENCED_PARAMETER(DeviceObject);

    outputBufferLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;

    if (outputBufferLength < sizeof(ULONG))
    {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto _cleanup;
    }

    outputBuffer = (PULONG)Irp->AssociatedIrp.SystemBuffer;
    *outputBuffer = InmageGetNumInFlightIos();
    Irp->IoStatus.Information = sizeof(ULONG);

_cleanup:
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

VOID
InDskFltResetCrashContext()
{
    KIRQL           oldIrql;
    PTAG_INFO       pTagInfo;

    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
    pTagInfo = DriverContext.pTagInfo;
    DriverContext.pTagInfo = NULL;
    RtlZeroMemory(DriverContext.TagContextGUID, DriverContext.ulTagContextGUIDLength);
    DriverContext.pTagProtocolHoldIrp = NULL;
    DriverContext.TagProtocolState = ecTagNoneInProgress;
    DriverContext.TagStatus = STATUS_SUCCESS;
    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);

    if (NULL != pTagInfo)
    {
        for (ULONG l = 0; l < pTagInfo->ulNumberOfDevices; l++)
        {
            if (NULL != pTagInfo->TagStatus[l].DevContext)
            {
                DereferenceDevContext(pTagInfo->TagStatus[l].DevContext);
            }
        }
        ExFreePoolWithTag(pTagInfo, TAG_GENERIC_NON_PAGED);
    }
}

NTSTATUS
InDskFltCompareExchangeTagState(
_In_        PUCHAR              TagContext,
_In_        etTagProtocolState  oldState,
_In_        etTagProtocolState  newState,
_Inout_     PBOOLEAN            pIsValidContext
)
/*
Routine Description:
    Incase of exchange DriverContext.TagContextGUID should be Zero to make sure there is no Tag in Progress
    Otherwise DriverContext.TagContextGUID should match TagContext Passed to this functionArguments:

Arguments:
    TagContext: TagContext for current request
    oldState: Old State to match for before exchanging states
    newState: New State to exchange to
    pIsValidContext: Is True if tag context matches with current tag context

Return Value:

NTSTATUS


*/
{
    KIRQL           oldIrql;
    ULONG           ulTagContextGUIDLength = DriverContext.ulTagContextGUIDLength;
    NTSTATUS        Status = STATUS_SUCCESS;
    BOOLEAN         bTagInProgress = TRUE;

    *pIsValidContext = FALSE;

    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);

    // Check if already a tag is in progress
    if (ulTagContextGUIDLength == RtlCompareMemory(DriverContext.ZeroGUID, DriverContext.TagContextGUID, ulTagContextGUIDLength))
    {
        bTagInProgress = FALSE;
    }

    // If tag is in process make sure context guid matches
    if (bTagInProgress)
    {
        // Match current tag context guid with input request
        if (ulTagContextGUIDLength != RtlCompareMemory(TagContext, DriverContext.TagContextGUID, ulTagContextGUIDLength))
        {
            Status = STATUS_DEVICE_BUSY;
            goto _cleanup;
        }
        *pIsValidContext = TRUE;
    }
    else
    {
        // Validate Tag Context Passed
        // It should not be Zero
        if (ulTagContextGUIDLength == RtlCompareMemory(DriverContext.ZeroGUID, TagContext, ulTagContextGUIDLength))
        {
            Status = STATUS_INVALID_PARAMETER;
            goto _cleanup;
        }
    }

    // Validate Tag State
    if (oldState != DriverContext.TagProtocolState)
    {
        Status = STATUS_INVALID_DEVICE_STATE;
        goto _cleanup;
    }

    if (!bTagInProgress)
    {
        // Set current tag state to input
        RtlCopyMemory(&DriverContext.TagContextGUID, TagContext, ulTagContextGUIDLength);
        *pIsValidContext = TRUE;
    }

    DriverContext.TagProtocolState = newState;

_cleanup:
    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
    return Status;
}

NTSTATUS
InDskFltCompareExchangeCrashContext(
_In_        PUCHAR  crashTagContext,
_In_        PUCHAR  oldContext,
_In_        BOOLEAN exchange
)
{
    KIRQL           oldIrql;
    ULONG           ulTagContextGUIDLength = DriverContext.ulTagContextGUIDLength;

    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);

    // Check if there is already one request in progress
    if (ulTagContextGUIDLength != RtlCompareMemory(DriverContext.TagContextGUID, oldContext, ulTagContextGUIDLength))
    {
        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
        return STATUS_DEVICE_BUSY;
    }

    if (exchange)
    {
        if (ulTagContextGUIDLength == RtlCompareMemory(DriverContext.ZeroGUID, crashTagContext, ulTagContextGUIDLength))
        {
            KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
            return STATUS_INVALID_PARAMETER;
        }
        
        RtlCopyMemory(&DriverContext.TagContextGUID, crashTagContext, ulTagContextGUIDLength);
    }

    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
    return STATUS_SUCCESS;
}

ULONG
InmageGetNumInFlightIos()
{
    PDEV_CONTEXT    pDevContext = NULL;
    PLIST_ENTRY     pDevExtList;
    ULONG           lNumInFlightIos = 0;

    KIRQL           oldIrql;

    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
    pDevExtList = DriverContext.HeadForDevContext.Flink;

    while (pDevExtList != &DriverContext.HeadForDevContext)
    {
        pDevContext = (PDEV_CONTEXT)pDevExtList;
        KeAcquireSpinLockAtDpcLevel(&pDevContext->Lock);

        if (DCF_FILTERING_STOPPED != (pDevContext->ulFlags & DCF_FILTERING_STOPPED))
        {
            lNumInFlightIos += pDevContext->ulNumPendingIrps;
        }

        KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
        pDevExtList = pDevExtList->Flink;
    }
    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);

    return lNumInFlightIos;
}

NTSTATUS
CreateCrashConsistentBarrier()
{
    PDEV_CONTEXT    pDevContext = NULL;
    PLIST_ENTRY     pDevExtList;
    KIRQL           oldIrql = 0;
    LONG            lNumFilteredDevices;
    NTSTATUS        Status = STATUS_SUCCESS;
    LONG            lCurrDeviceIdx = 0;
    BOOLEAN         bReleaseGlobalSpinLock = FALSE;

    lNumFilteredDevices = InDskFltGetNumFilteredDevices();

    if (0 == lNumFilteredDevices)
    {
        Status = STATUS_NO_SUCH_DEVICE;
        goto _cleanup;
    }

    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
    bReleaseGlobalSpinLock = TRUE;

    pDevExtList = DriverContext.HeadForDevContext.Flink;

    while (pDevExtList != &DriverContext.HeadForDevContext)
    {
        pDevContext = (PDEV_CONTEXT)pDevExtList;
        KeAcquireSpinLockAtDpcLevel(&pDevContext->Lock);

        // Ignore devices for which filtering has been stopped
        if (DCF_FILTERING_STOPPED == (pDevContext->ulFlags & DCF_FILTERING_STOPPED))
        {
            KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
            pDevExtList = pDevExtList->Flink;
            continue;
        }

        if ((lCurrDeviceIdx >= lNumFilteredDevices) ||
            (ecWriteOrderStateData != pDevContext->eWOState))
        {
            KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
            Status = STATUS_INVALID_DEVICE_STATE;
            goto _cleanup;
        }

        ++lCurrDeviceIdx;
        pDevContext->bBarrierOn = TRUE;

        KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
        pDevExtList = pDevExtList->Flink;
    }

    if (lCurrDeviceIdx != lNumFilteredDevices)
    {
        Status = STATUS_INVALID_DEVICE_STATE;
    }

_cleanup:
    if (bReleaseGlobalSpinLock)
    {
        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
    }

    return Status;
}

LONG InDskFltGetNumFilteredDevices()
{
    PDEV_CONTEXT    pDevContext = NULL;
    PLIST_ENTRY     pDevExtList;
    LONG            lNumTrackedDevices = 0;

    KIRQL           oldIrql;

    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
    pDevExtList = DriverContext.HeadForDevContext.Flink;

    while (pDevExtList != &DriverContext.HeadForDevContext)
    {
        pDevContext = (PDEV_CONTEXT)pDevExtList;
        KeAcquireSpinLockAtDpcLevel(&pDevContext->Lock);

        if (DCF_FILTERING_STOPPED != (pDevContext->ulFlags & DCF_FILTERING_STOPPED))
        {
            lNumTrackedDevices++;
        }

        KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
        pDevExtList = pDevExtList->Flink;
    }
    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);

    return lNumTrackedDevices;
}

NTSTATUS
ValidateAndCreateDeviceBarrier(
_In_        PTAG_INFO *ppTagInfo,
_In_        BOOLEAN bBarrierAlreadyCreated,
_In_        ULONG ulFlags,
_Inout_     PTAG_TELEMETRY_COMMON pTagCommonAttribs
)
{
    PDEV_CONTEXT    pDevContext = NULL;
    PLIST_ENTRY     pDevExtList;
    KIRQL           oldIrql;
    LONG            lNumTrackedDevices;
    NTSTATUS        Status = STATUS_SUCCESS;
    LONG            lCurrDeviceIdx = 0;
    BOOLEAN         bReleaseGlobalSpinLock = FALSE;
    PTAG_INFO       pTagInfo = NULL;

    *ppTagInfo = NULL;

    // Just to make compiler happy with W4
    oldIrql = KeGetCurrentIrql();
    lNumTrackedDevices = InDskFltGetNumFilteredDevices();

    if (0 == lNumTrackedDevices)
    {
        Status = STATUS_NO_SUCH_DEVICE;
        goto _cleanup;
    }

    ULONG   ulSizeOfTagInfo = sizeof(TAG_INFO)+(lNumTrackedDevices * sizeof(TAG_STATUS));
    pTagInfo = (PTAG_INFO)ExAllocatePoolWithTag(NonPagedPool, ulSizeOfTagInfo, TAG_GENERIC_NON_PAGED);
    if (NULL == pTagInfo)
    {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_TAGS,
            ("ValidateAndCreateDeviceBarrier: memory allocation failed \n"));
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto _cleanup;
    }

    RtlZeroMemory(pTagInfo, ulSizeOfTagInfo);
    pTagInfo->ulFlags = ulFlags;
    if (NULL != pTagCommonAttribs) {
        pTagCommonAttribs->usNumProtectedDisk = (USHORT)lNumTrackedDevices;
        pTagInfo->TagCommonAttribs = *pTagCommonAttribs;
    }

    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
    bReleaseGlobalSpinLock = TRUE;
    *ppTagInfo = pTagInfo;

    pDevExtList = DriverContext.HeadForDevContext.Flink;

    for (; pDevExtList != &DriverContext.HeadForDevContext; pDevExtList = pDevExtList->Flink)
    {
        pDevContext = (PDEV_CONTEXT)pDevExtList;        

        KeAcquireSpinLockAtDpcLevel(&pDevContext->Lock);

        // Ignore devices for which filtering has been stopped
        if (DCF_FILTERING_STOPPED == (pDevContext->ulFlags & DCF_FILTERING_STOPPED))
        {
            KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
            continue;
        }

        // If barrier is already created check if barrier is still true
        // This is true if a new start filtering is called between Freeze and Insert Tag
        if (bBarrierAlreadyCreated)
        {
            if (!pDevContext->bBarrierOn)
            {
                KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
                Status = STATUS_INVALID_DEVICE_STATE;
                goto _cleanup;
            }
        }

        if (pDevContext->bWritesHeldForEarlierBarrier)
        {
            KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
            Status = STATUS_DEVICE_BUSY;
            goto _cleanup;
        }

        if (lCurrDeviceIdx >= lNumTrackedDevices)
        {
            Status = STATUS_INVALID_DEVICE_STATE;
            KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
            goto _cleanup;
        }

        Status = STATUS_PENDING;

        if (ecWriteOrderStateData != pDevContext->eWOState)
        { 
            Status = STATUS_INVALID_DEVICE_STATE;
            if (NULL != pTagCommonAttribs) {
                // set of the fields need to be filled instantly...so far we filled only global fields only
                pTagCommonAttribs->ulNumberOfDevicesTagsApplied = 0; // This phase is before insert
                pTagCommonAttribs->TagStatus = Status;
                pTagCommonAttribs->GlobaIOCTLStatus = STATUS_NOT_SUPPORTED;
                pTagCommonAttribs->ullTagState = ecTagStatusInsertFailure;
                KeQuerySystemTime(&pTagCommonAttribs->liTagInsertSystemTimeStamp);
                InDskFltTelemetryInsertTagFailure(pDevContext, pTagCommonAttribs, ecMsgTagInsertFailure);
            }
            KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);

            if (0 == (ulFlags & TAG_INFO_FLAGS_IGNORE_DISKS_IN_NON_WO_STATE))
            {
                goto _cleanup;
            }
            pTagInfo->ulNumberOfDevicesIgnored++;
            continue;
        }

        ReferenceDevContext(pDevContext);
        pDevContext->bBarrierOn = TRUE;
        pDevContext->bWritesHeldForEarlierBarrier = TRUE;
        pTagInfo->TagStatus[lCurrDeviceIdx].DevContext = pDevContext;
        pTagInfo->TagStatus[lCurrDeviceIdx].Status = Status;
        pTagInfo->ulNumberOfDevices++;

        ++lCurrDeviceIdx;

        KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
    }

    if (0 == lCurrDeviceIdx)
    {
        Status = STATUS_NO_SUCH_DEVICE;
        goto _cleanup;
    }

    Status = STATUS_SUCCESS;

_cleanup:
    if (bReleaseGlobalSpinLock)
    {
        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
    }

    return Status;
}

NTSTATUS
IsValidBarrierIoctlTagDiskInputBuffer(
_In_        PUCHAR	pBuffer,
_In_        ULONG	ulBufferLength,
_In_        ULONG   ulNumberOfTags
)
{
    ULONG	ulBytesParsed = 0;

    PUCHAR  pTags = pBuffer;

    if (0 == ulNumberOfTags)
    {
        return STATUS_INVALID_PARAMETER;
    }

    for (ULONG i = 0; i < ulNumberOfTags; i++)
    {
        if (ulBufferLength < ulBytesParsed + sizeof(USHORT))
        {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        USHORT usTagDataLength = *(USHORT UNALIGNED *) (pBuffer + ulBytesParsed);
        ulBytesParsed += sizeof(USHORT);

        if (0 == usTagDataLength)
        {
            return STATUS_INVALID_PARAMETER;
        }

        if (ulBufferLength < ulBytesParsed + usTagDataLength)
        {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        ulBytesParsed += usTagDataLength;
    }

    // This is size of buffer needed to save tags in dirtyblock(non-datamode)
    ULONG ulMemNeeded = GetBufferSizeNeededForTags(pTags, ulNumberOfTags);
    if (ulMemNeeded > DriverContext.ulMaxTagBufferSize)
    {
        return STATUS_INVALID_PARAMETER;
    }

    return STATUS_SUCCESS;
}


VOID
InDskFltReleaseDeviceWrites(
_In_        PDEV_CONTEXT pDevContext
)
/*++

Routine Description:

This routine releases all writes hold for a given device.

Arguments:

pDevContext    - Device Context of the given device.

Return Value:

--*/
{
    LIST_ENTRY          q;
    KIRQL               oldIrql;
    PIRP                irp = NULL;
    PIO_STACK_LOCATION  irpSp = NULL;
    PLIST_ENTRY         l = NULL;
    PIRP                qIrp = NULL;

    InitializeListHead(&q);

    KeAcquireSpinLock(&pDevContext->Lock, &oldIrql);
    while (NULL != (qIrp = IoCsqRemoveNextIrp(&pDevContext->CsqIrpQueue, NULL)))
    {
        if (IS_IRP_PAGING_FLAG_SET(qIrp))
        {
            InsertHeadList(&q, &qIrp->Tail.Overlay.ListEntry);
        }
        else
        {
            InsertTailList(&q, &qIrp->Tail.Overlay.ListEntry);
        }
    }

    // Release Barrier
    pDevContext->bBarrierOn = FALSE;
    KeReleaseSpinLock(&pDevContext->Lock, oldIrql);

    for (l = q.Flink; l != &q; l = l->Flink)
    {
        irp = CONTAINING_RECORD(l, IRP, Tail.Overlay.ListEntry);
        irpSp = IoGetCurrentIrpStackLocation(irp);
        RemoveEntryList(l);
        l = l->Blink;
        DriverContext.DriverObject->MajorFunction[irpSp->MajorFunction](irpSp->DeviceObject, irp);
    }

    KeAcquireSpinLock(&pDevContext->Lock, &oldIrql);
    pDevContext->bWritesHeldForEarlierBarrier = FALSE;
    KeReleaseSpinLock(&pDevContext->Lock, oldIrql);
}

NTSTATUS
InDskFltCreateReleaseWritesThread(
_In_    PDEV_CONTEXT    pDevContext
)
{
    HANDLE              hThread;
    PVOID               threadObject;
    NTSTATUS            Status;

    ReferenceDevContext(pDevContext);

    // Create System Thread for releasing writes
    Status = PsCreateSystemThread(&hThread,
        THREAD_ALL_ACCESS,
        NULL,
        NULL,
        NULL,
        InDskReleaseWritesThread,
        pDevContext);

    // We cannot issue tags on this disk
    if (!NT_SUCCESS(Status))
    {
        DereferenceDevContext(pDevContext);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Status = ObReferenceObjectByHandle(hThread, THREAD_ALL_ACCESS, NULL, KernelMode, &threadObject, NULL);
    ZwClose(hThread);

    if (!NT_SUCCESS(Status))
    {
        KeSetEvent(&pDevContext->hReleaseWritesThreadShutdown, IO_NO_INCREMENT, FALSE);
        return Status;
    }

    pDevContext->pReleaseWritesThread = threadObject;

    return Status;
}

VOID
InDskReleaseWritesThread(
_In_    PVOID Context
)
/*++

Routine Description:

This thread routine releases all writes hold for a given device.
It wait for 2 events.
    When it receives release event, it releases all writes for the device.
    when it receives terminate event it exits.

Arguments:

pDevContext    - Device Context of the given device.

Return Value:

--*/
{
    PDEV_CONTEXT            pDevContext = (PDEV_CONTEXT)Context;
    NTSTATUS                Status;
    PKEVENT                 waitObjects[MAX_INDSKFLT_WAIT_TAG_OBJECTS];
    BOOLEAN                 exit = FALSE;

    if (NULL == pDevContext)
    {
        PsTerminateSystemThread(STATUS_INVALID_PARAMETER);
        return;
    }
    
    waitObjects[INDSKFLT_WAIT_TERMINATE_THREAD] = &pDevContext->hReleaseWritesThreadShutdown;
    waitObjects[INDSKFLT_WAIT_RELEASE_WRITES] = &pDevContext->hReleaseWritesEvent;

    while (FALSE == exit)
    {

        Status = KeWaitForMultipleObjects(MAX_INDSKFLT_WAIT_TAG_OBJECTS,
                                            (PVOID *)&waitObjects[0],
                                            WaitAny,
                                            Executive,
                                            KernelMode,
                                            FALSE,
                                            NULL,
                                            NULL);

        switch (Status)
        {
            case INDSKFLT_WAIT_TERMINATE_THREAD:
            {
                exit = TRUE;
                break;
            }

            case INDSKFLT_WAIT_RELEASE_WRITES:
            {
                InDskFltReleaseDeviceWrites(pDevContext);
                break;
            }
        }
    }

    DereferenceDevContext(pDevContext);
    PsTerminateSystemThread(STATUS_SUCCESS);
}

VOID
InDskFltIoBarrierCleanup(
_In_    PDEV_CONTEXT    pDevContext
)
{
    KIRQL       oldIrql;
    PVOID       threadObject;

    KeAcquireSpinLock(&pDevContext->Lock, &oldIrql);
    threadObject = pDevContext->pReleaseWritesThread;
    pDevContext->pReleaseWritesThread = NULL;
    KeReleaseSpinLock(&pDevContext->Lock, oldIrql);

    // Close Release Writes Thread
    if (NULL != threadObject)
    {
        // Release all writes for this disk
        InDskFltReleaseDeviceWrites(pDevContext);

        KeSetEvent(&pDevContext->hReleaseWritesThreadShutdown, IO_NO_INCREMENT, FALSE);

        // Wait for Release Writes thread to complete
        KeWaitForSingleObject(threadObject, Executive, KernelMode, FALSE, NULL);

        ObDereferenceObject(threadObject);
    }
}

VOID
InDskFltReleaseWrites()
/*++

Routine Description:

This routine releases all writes hold for all devices. It is normally called when crash consistent barrier is released.

Arguments:

None.

Return Value:

--*/
{
    KIRQL               oldIrql;
    PLIST_ENTRY         pDevExtList = NULL;
    PDEV_CONTEXT        pDevContext = NULL;

    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
    pDevExtList = DriverContext.HeadForDevContext.Flink;

    while (pDevExtList != &DriverContext.HeadForDevContext)
    {
        pDevContext = (PDEV_CONTEXT)pDevExtList;
        KeAcquireSpinLockAtDpcLevel(&pDevContext->Lock);

        // Release Barrier
        pDevContext->bBarrierOn = FALSE;

        KeSetEvent(&pDevContext->hReleaseWritesEvent, IO_NO_INCREMENT, FALSE);

        pDevExtList = pDevExtList->Flink;
        KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
    }

    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
}

VOID
InDskFltCrashConsistentTagCleanup(
_In_   etTagStateTriggerReason tagStateReason
)
{
    if (!InDskFltCancelProtocolHoldIrp(TRUE, TRUE))
    {
        return;
    }
    FinalizeDeviceTagStateAll(DriverContext.pTagInfo, false, FALSE, tagStateReason);
    InDskFltReleaseWrites();
    InDskFltResetCrashContext();
}

VOID
InDskFltCancelQueuedWriteForFileObject(
    _In_    PDEVICE_EXTENSION pDeviceExtension, 
    _In_    PFILE_OBJECT fileObject)
{
    PIRP            qIrp;
    PDEV_CONTEXT    pDevContext = pDeviceExtension->pDevContext;

    if (NULL != pDevContext)
    {
        while (NULL != (qIrp = IoCsqRemoveNextIrp(&pDevContext->CsqIrpQueue, fileObject)))
        {
            InterlockedDecrement64(&DriverContext.llNumQueuedIos);

            qIrp->IoStatus.Status = STATUS_CANCELLED;
            qIrp->IoStatus.Information = 0;
            IoCompleteRequest(qIrp, IO_NO_INCREMENT);
        }
    }
}

BOOLEAN
InDskFltCancelProtocolHoldIrp(
    _In_    BOOLEAN bResetCancelRoutine, 
    _In_    BOOLEAN bCancelTimer
)
{
    KIRQL       irql = 0;
    PIRP        holdIrp;

    KeAcquireSpinLock(&DriverContext.Lock, &irql);

    if (NULL == DriverContext.pTagProtocolHoldIrp)
    {
        KeReleaseSpinLock(&DriverContext.Lock, irql);
        return FALSE;
    }

    holdIrp = DriverContext.pTagProtocolHoldIrp;
    DriverContext.pTagProtocolHoldIrp = NULL;

    if (bResetCancelRoutine)
    {
        IoSetCancelRoutine(holdIrp, NULL);
    }

    if (bCancelTimer)
    {
        KeCancelTimer(&DriverContext.TagProtocolTimer);
    }
    KeReleaseSpinLock(&DriverContext.Lock, irql);
    
    holdIrp->IoStatus.Status = (bCancelTimer)? STATUS_SUCCESS : STATUS_CANCELLED;
    holdIrp->IoStatus.Information = 0;
    IoCompleteRequest(holdIrp, IO_NO_INCREMENT);

    return TRUE;
}

VOID
InDskFltCrashConsistentTimerDpc(
_In_        PKDPC   TimerDpc,
_In_opt_    PVOID   Context,
_In_opt_    PVOID   SystemArgument1,
_In_opt_    PVOID   SystemArgument2
)
{
    PAGED_CODE_LOCKED();

    UNREFERENCED_PARAMETER(TimerDpc);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);
    UNREFERENCED_PARAMETER(Context);

    // Check if we can do cleanup now
    KeAcquireSpinLockAtDpcLevel(&DriverContext.Lock);
    if (!CAN_HANDLE_TAG_CLEANUP())
    {
        DriverContext.TagStatus = STATUS_IO_TIMEOUT;
        DriverContext.TagProtocolState = ecTagCleanup;
        KeReleaseSpinLockFromDpcLevel(&DriverContext.Lock);
        return;
    }
    KeReleaseSpinLockFromDpcLevel(&DriverContext.Lock);
       
    if (!InDskFltCancelProtocolHoldIrp(TRUE, FALSE))
    {
        return;
    }

    FinalizeDeviceTagStateAll(DriverContext.pTagInfo, false, FALSE, ecRevokeTimeOut);
    InDskFltReleaseWrites();
    InDskFltResetCrashContext();
}

VOID
InDskFltCancelCrashConsistentTag(
_Inout_ PDEVICE_OBJECT  DeviceObject,
_Inout_ PIRP            Irp
)
{
    PAGED_CODE_LOCKED();

    UNREFERENCED_PARAMETER(DeviceObject);

    KIRQL   irql;

    IoReleaseCancelSpinLock(Irp->CancelIrql);

    // Check if we can do cleanup now
    KeAcquireSpinLock(&DriverContext.Lock, &irql);
    if (!CAN_HANDLE_TAG_CLEANUP())
    {
        DriverContext.TagStatus = STATUS_CANCELLED;
        DriverContext.TagProtocolState = ecTagCleanup;
        KeReleaseSpinLock(&DriverContext.Lock,irql);
        return;
    }
    KeReleaseSpinLock(&DriverContext.Lock, irql);

    if (!InDskFltCancelProtocolHoldIrp(FALSE, TRUE))
    {
        return;
    }

    FinalizeDeviceTagStateAll(DriverContext.pTagInfo, false, FALSE, ecRevokeCancelIrpRoutine);
    InDskFltReleaseWrites();
    InDskFltResetCrashContext();
}

// Cancel Safe Routines
// Acquire Cancel Lock
VOID InDskFltCsqAcquireLock(
_In_        PIO_CSQ csq, 
_In_        PKIRQL irql
)
{
    PDEV_CONTEXT pDevContext = GET_DEVICE_EXTENSION(csq);
    KeAcquireSpinLock(&pDevContext->CsqSpinLock, irql);
}

VOID InDskFltCsqReleaseLock(
_In_    PIO_CSQ csq,
_In_    KIRQL irql
)
{
    PDEV_CONTEXT pDevContext = GET_DEVICE_EXTENSION(csq);
    KeReleaseSpinLock(&pDevContext->CsqSpinLock, irql);
}

VOID InDskFltCsqInsertIrp(
_In_        PIO_CSQ csq,
_In_        PIRP Irp
)
{
    PDEV_CONTEXT pDevContext = GET_DEVICE_EXTENSION(csq);
    InsertTailList(&pDevContext->HoldIrpQueue, &Irp->Tail.Overlay.ListEntry);
}

VOID InDskFltCsqRemoveIrp(
_In_    PIO_CSQ csq,
_In_    PIRP Irp
)
{
    UNREFERENCED_PARAMETER(csq);
    RemoveEntryList(&Irp->Tail.Overlay.ListEntry);
}

VOID InDskFltCompleteCancelledIrp(
_In_    PIO_CSQ csq,
_In_    PIRP Irp
)
{
    InterlockedDecrement64(&DriverContext.llNumQueuedIos);

    UNREFERENCED_PARAMETER(csq);
    Irp->IoStatus.Status = STATUS_CANCELLED;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
}

PIRP InDskFltCsqPeekNextIrp(
_In_    PIO_CSQ csq,
_In_    PIRP Irp,
_In_    PVOID PeekContext
)
{
    PDEV_CONTEXT pDevContext = GET_DEVICE_EXTENSION(csq);
    PLIST_ENTRY next = Irp ? Irp->Tail.Overlay.ListEntry.Flink : pDevContext->HoldIrpQueue.Flink;
    PIRP        nextIrp = NULL;
    PIO_STACK_LOCATION  IrpSp;

    while (next != &pDevContext->HoldIrpQueue)
    {
        nextIrp = CONTAINING_RECORD(next, IRP, Tail.Overlay.ListEntry);
        IrpSp = IoGetCurrentIrpStackLocation(nextIrp);
        if (PeekContext  && IrpSp->FileObject == PeekContext)
        {
            return nextIrp;
        }

        if (NULL == PeekContext)
        {
            return nextIrp;
        }
        next = next->Flink;
    }

    return NULL;
}


