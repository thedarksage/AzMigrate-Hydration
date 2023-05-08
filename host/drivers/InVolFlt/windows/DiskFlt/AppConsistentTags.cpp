#include "Common.h" 
#include "FltFunc.h"
#include "VContext.h"
#include "misc.h"

NTSTATUS
InDskFltCompareExchangeAppConsistentTagState(
_In_        PUCHAR              TagContext,
_In_        etTagProtocolState  oldState,
_In_        etTagProtocolState  newState,
_Out_       PBOOLEAN            pIsValidContext,
_In_        BOOLEAN             bLockAcquired
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
    KIRQL           oldIrql = 0;
    ULONG           ulTagContextGUIDLength = DriverContext.ulTagContextGUIDLength;
    NTSTATUS        Status = STATUS_SUCCESS;
    BOOLEAN         bTagInProgress = TRUE;

    *pIsValidContext = FALSE;

    if (!bLockAcquired)
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
    if (!bLockAcquired)
        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
    return Status;
}
/*
Input Tag Stream Structure for disk devices
_____________________________________________________________________________________________________________________________________________________________
|  Context   |             |          |                |                 |    |              |              |         |          |     |         |           |
| GUID(UCHAR)|   Flags     |timeout   | No. of GUIDs(n)|  Volume GUID1   | ...| Volume GUIDn | Tag Len (k)  |Tag1 Size| Tag1 data| ... |Tagk Size| Tagk Data |
|_(36 Bytes)_|____(4 Bytes)|_(4 Bytes)|_ (4 Bytes)_____|___(36 Bytes)____|____|______________|___(4 bytes)__|(2 bytes)|______________________________________|

*/

bool
IsValidIoctlTagDiskInputBuffer(
    __in PUCHAR pBuffer,
    __in ULONG ulBufferLength
)
{
    ULONG ulNumberOfDevices = 0;
    ULONG ulBytesParsed = 0;
    ULONG ulNumberOfTags = 0;

    if (ulBufferLength < (GUID_SIZE_IN_CHARS + 3 * sizeof(ULONG)))
        return false;

    ulNumberOfDevices = *(PULONG)(pBuffer + GUID_SIZE_IN_CHARS + (2 * sizeof(ULONG)));
    ulBytesParsed += GUID_SIZE_IN_CHARS + 3 * sizeof(ULONG);

    if (0 == ulNumberOfDevices)
        return false;

    if (ulBufferLength < (ulBytesParsed + ulNumberOfDevices * GUID_SIZE_IN_BYTES))
        return false;

    ulBytesParsed += ulNumberOfDevices * GUID_SIZE_IN_BYTES;

    if (ulBufferLength < ulBytesParsed + sizeof(ULONG))
        return false;

    ulNumberOfTags = *(PULONG)(pBuffer + ulBytesParsed);
    ulBytesParsed += sizeof(ULONG);

    if (0 == ulNumberOfTags)
        return false;

    PUCHAR  pTags = pBuffer + ulBytesParsed;

    for (ULONG i = 0; i < ulNumberOfTags; i++) {
        if (ulBufferLength < ulBytesParsed + sizeof(USHORT))
            return false;

        USHORT usTagDataLength = *(USHORT UNALIGNED *) (pBuffer + ulBytesParsed);
        ulBytesParsed += sizeof(USHORT);

        if (0 == usTagDataLength)
            return false;

        if (ulBufferLength < ulBytesParsed + usTagDataLength)
            return false;

        ulBytesParsed += usTagDataLength;
    }

    // This is size of buffer needed to save tags in dirtyblock
    ULONG ulMemNeeded = GetBufferSizeNeededForTags(pTags, ulNumberOfTags);
    if (ulMemNeeded > DriverContext.ulMaxTagBufferSize)
        return false;

    return true;
}
void
FreeTagInfo(
__in    PTAG_INFO pTagInfo
)
/*
Dereference and Free the Global TagInfo structure
*/
{
    ASSERT(NULL != pTagInfo);
    for (ULONG index = 0; index < pTagInfo->ulNumberOfDevices; index++) {
        if (NULL != pTagInfo->TagStatus[index].DevContext) {
            DereferenceDevContext(pTagInfo->TagStatus[index].DevContext);
        }
    }
    ExFreePoolWithTag(pTagInfo, TAG_DB_TAG_INFO);
}

void
ResetHoldIrpCancelRoutine(
__in       PIRP *pHoldIrp,
__out      BOOLEAN*  bCancelRoutineExecuted
)
/*
Reset and return the hold IRP cancellation routine
*/
{
    if (NULL != DriverContext.pTagProtocolHoldIrp) {
        if (IoSetCancelRoutine(DriverContext.pTagProtocolHoldIrp, NULL)) {
            *bCancelRoutineExecuted = FALSE;
            *pHoldIrp = DriverContext.pTagProtocolHoldIrp;
            DriverContext.pTagProtocolHoldIrp = NULL;
            // Mark the IRP as cancelled
        }
    }
}

void
ResetCancelTimer(PKTIMER Timer, BOOLEAN* breturn) {
    if (TRUE == DriverContext.bTimer) {
        *breturn = KeCancelTimer(Timer);
        DriverContext.bTimer = FALSE;
    }
}

NTSTATUS
CleanupTagProtocol(BOOLEAN bCancelTimer, BOOLEAN bCancelIrp, etTagStateTriggerReason tagStateReason, BOOLEAN bCommitAll, PTAG_INFO* ppTagInfo)
{
    KIRQL oldIrql = 0;
    PIRP  pHoldIrp = NULL;
    BOOLEAN bCancelRoutineExecuted = TRUE;
    PTAG_INFO pTagInfo = NULL;
    BOOLEAN bPostCleanup = FALSE;
    NTSTATUS IrpStatus = STATUS_SUCCESS;

    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
    if (!bCancelIrp) {
        if (NULL != DriverContext.pTagProtocolHoldIrp) {
            bCancelRoutineExecuted = FALSE;
            pHoldIrp = DriverContext.pTagProtocolHoldIrp;
            DriverContext.pTagProtocolHoldIrp = NULL;
            //Check for irp if set not to cleanup the goto Exit;
        }
    }
    if (!bCancelTimer) {
        DriverContext.bTimer = FALSE;
    }
    switch (DriverContext.TagProtocolState) {
    case ecTagNoneInProgress:
        ASSERT(DriverContext.pTagProtocolHoldIrp == NULL);
        ASSERT(DriverContext.TagType == ecTagNone);
        break;
    case ecTagCommitStarted:
        __fallthrough;
    case ecTagInsertTagStarted:
        __fallthrough;
    case ecTagInsertTagCompleted:
        bPostCleanup = TRUE;
        if (bCancelIrp) {
            ResetHoldIrpCancelRoutine(&pHoldIrp, &bCancelRoutineExecuted);
        }

        if (bCancelIrp && bCancelRoutineExecuted) {
            bPostCleanup = FALSE;
        }

        if (bCancelTimer) {
            ResetCancelTimer(&DriverContext.AppTagProtocolTimer, &bPostCleanup);
        }
        else {
            bPostCleanup = TRUE;
        }
        
       
        break;
    case ecTagCleanup:
        break;
    default:
        // do nothing
        break;
    }

    if (!bPostCleanup) {
        // Look at commt flag, do revoke as well cleanup
        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
        goto Exit;
    }

    // Let's handle cleanup here and mark as cleanup
    
    ASSERT(NULL != DriverContext.pTagInfo);
    pTagInfo = DriverContext.pTagInfo;
    DriverContext.pTagInfo = NULL;
    DriverContext.TagProtocolState = ecTagCleanup;
    
    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
   
    if ((NULL != pTagInfo)) {
        FinalizeDeviceTagStateAll(pTagInfo, false, bCommitAll, tagStateReason);
        if (NULL != ppTagInfo) {
            *ppTagInfo = pTagInfo;
            pTagInfo = NULL;
        } else {
            FreeTagInfo(pTagInfo);
        }
    }
    
    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
    DriverContext.TagType = ecTagNone;
    //reset the GUID context
    DriverContext.TagProtocolState = ecTagNoneInProgress;
    RtlZeroMemory(DriverContext.TagContextGUID, GUID_SIZE_IN_CHARS);
    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
Exit :
    if (FALSE == bCancelRoutineExecuted) {        
        IrpStatus = STATUS_CANCELLED;
        pHoldIrp->IoStatus.Status = IrpStatus;
        pHoldIrp->IoStatus.Information = 0;
        IoCompleteRequest(pHoldIrp, IO_NO_INCREMENT);
    }
    return IrpStatus;	
}
NTSTATUS
DeviceIoControlAppTagDisk(
__in    PDEVICE_OBJECT DeviceObject,
__in    PIRP Irp
)
/*
Description : This IOCTL is issued by vacp.exe, when the protected disks are in freeze state.This function validates the input buffer, prepares the taginfo list, issues the tags in sequence.
This is a common function for single-node or multi-node case as IO barrier is handled by VSS. This is best effort inspite of any of device going to MD or failed to allocate internal strcutures

Arguments :
DeviceObject     - pointer to control device object
Irp              - Pointer to the IRP

Return   :
STATUS_PENDING                    - On successfull insertion of tag
STATUS_INFO_LENGTH_MISMATCH       - zero Input buffer length
STATUS_INVALID_PARAMETER_1        - Zero Context GUID
STATUS_INVALID_PARAMETER_2        - Error validating input buffer
STATUS_INVALID_PARAMETER_3        - Error on finding unexpected flags
STATUS_INVALID_PARAMETER_4        - Zero Timeout
STATUS_INVALID_PARAMETER_5        - Zero number of disks
STATUS_INSUFFICIENT_RESOURCES     - Memory allocation failure (Not for internal structures)
STATUS_NOT_FOUND                  - Does not found one or more devices 
STATUS_INVALID_DEVICE_STATE       - Tag insertion has failed as one of the device has filtering stopped,
                                    This can be expanded for other cases
STATUS_CANCELLED                  - IoCTL has cancelled on the way
STATUS_DEVICE_BUSY                - One protocol is already active
*/
{
    NTSTATUS              Status = STATUS_SUCCESS;
    ULONG                 ulNumberOfDisks = 0;
    ULONG                 ulTagStartOffset = 0;
    PUCHAR                pBuffer = (PUCHAR)Irp->AssociatedIrp.SystemBuffer;
    PIO_STACK_LOCATION    IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG                 IoctlTagDeviceFlags = 0;
    USHORT                usDeviceGUIDIndex = GUID_SIZE_IN_CHARS + 3 * sizeof(ULONG);
    ULONG                 TimeoutinMilliSecs = 0;
    LARGE_INTEGER         Timeout;
    KIRQL                 oldIrql = 0;
    BOOLEAN               bValidGUIDContext = FALSE;
    BOOLEAN               bCancelTimer = FALSE;
    BOOLEAN               bCancelIrp = FALSE;
    TAG_TELEMETRY_COMMON  TagCommonAttribs = { 0 };
    ULONG                 ulMessageType = ecMsgUninitialized;


    Irp->IoStatus.Information = 0;

    UNREFERENCED_PARAMETER(DeviceObject);

    ASSERT(IOCTL_INMAGE_TAG_DISK == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

    INDSKFLT_SET_TAGREQUEST_TIME(&TagCommonAttribs.liTagRequestTime);

    TagCommonAttribs.ulIoCtlCode = IoStackLocation->Parameters.DeviceIoControl.IoControlCode;
    TagCommonAttribs.usTotalNumDisk = (USHORT)DriverContext.ulNumDevs;
    TagCommonAttribs.usNumProtectedDisk = (USHORT)InDskFltGetNumFilteredDevices();

    if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < GUID_SIZE_IN_CHARS) {
        ulMessageType = ecMsgAppInputBufferMismatch;
        Status = STATUS_INFO_LENGTH_MISMATCH;
        goto _Cleanup;
    }

    // Validation of input buffer and Max. Memory limit set by driver
    if (!IsValidIoctlTagDiskInputBuffer(pBuffer, IoStackLocation->Parameters.DeviceIoControl.InputBufferLength)) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_TAGS,
            ("DeviceIoControlAppTagDisk: IOCTL rejected, invalid input buffer \n"));
        ulMessageType = ecMsgAppInvalidTagInputBuffer;
        Status = STATUS_INVALID_PARAMETER_2;
        goto _Cleanup;
    }

    // Get the tag marker GUID, note that buffer is already validated above
    InDskFltAppGetTagMarkerGUID(pBuffer, TagCommonAttribs.TagMarkerGUID, GUID_SIZE_IN_CHARS, IoStackLocation->Parameters.DeviceIoControl.InputBufferLength);

    IoctlTagDeviceFlags = *(PULONG)((PCHAR)pBuffer + GUID_SIZE_IN_CHARS);

    if (IoctlTagDeviceFlags & TAG_INFO_FLAGS_BASELINE_APP)
        TagCommonAttribs.TagType = ecTagBaselineApp;
    else if (IoctlTagDeviceFlags & TAG_INFO_FLAGS_DISTRIBUTED_APP)
        TagCommonAttribs.TagType = ecTagDistributedApp;
    else if (IoctlTagDeviceFlags & TAG_INFO_FLAGS_LOCAL_APP)
        TagCommonAttribs.TagType = ecTagLocalApp;

    // Check buffer flags; These flags are not required for disk.
    if (TEST_FLAG(IoctlTagDeviceFlags, TAG_VOLUME_INPUT_FLAGS_NO_ERROR_IF_GUID_NOT_FOUND) ||
        TEST_FLAG(IoctlTagDeviceFlags, TAG_VOLUME_INPUT_FLAGS_NO_ERROR_IF_FILTERING_STOPPED) ||
        TEST_FLAG(IoctlTagDeviceFlags, TAG_VOLUME_INPUT_FLAGS_ATOMIC_TO_VOLUME_GROUP)) {
        ulMessageType = ecMsgAppUnexpectedFlags;
        Status = STATUS_INVALID_PARAMETER_3;
        goto _Cleanup;
    }

    TimeoutinMilliSecs = *(PULONG)((PCHAR)pBuffer + GUID_SIZE_IN_CHARS + sizeof(ULONG));
    if (0 == TimeoutinMilliSecs) {
        ulMessageType = ecMsgInputZeroTimeOut;
        Status = STATUS_INVALID_PARAMETER_4;
        goto _Cleanup;
    }
    Timeout.QuadPart = MILLISECONDS_RELATIVE(TimeoutinMilliSecs);

    ulNumberOfDisks = *(PULONG)(pBuffer + GUID_SIZE_IN_CHARS + (2 * sizeof(ULONG)));
    if ((ulNumberOfDisks == 0) || (ulNumberOfDisks > DriverContext.ulNumDevs)) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_TAGS,
            ("DeviceIoControlAppTagDisk: IOCTL rejected, invalid number of disks(%d), ulNumDevs(%d)\n",
            ulNumberOfDisks, DriverContext.ulNumDevs));
        ulMessageType = ecMsgAppInvalidInputDiskNum;
        Status = STATUS_INVALID_PARAMETER_5;
        goto _Cleanup;
    }

    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < (SIZE_OF_TAG_DISK_STATUS_OUTPUT_DATA(ulNumberOfDisks))) {
        ulMessageType = ecMsgAppOutputBufferTooSmall;
        Status = STATUS_BUFFER_TOO_SMALL;
        goto _Cleanup;
    }

    // Retrieve the appropriate DeviceContext by parsing the input buffer
    ULONG   ulSizeOfTagInfo = sizeof(TAG_INFO)+(ulNumberOfDisks * sizeof(TAG_STATUS));
    PTAG_INFO  pTagInfo = (PTAG_INFO)ExAllocatePoolWithTag(NonPagedPool, ulSizeOfTagInfo, TAG_DB_TAG_INFO);
    if (NULL == pTagInfo) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_TAGS, ("DeviceIoControlTagVolume: memory allocation failed \n"));
        ulMessageType = ecMsgAppTagInfoMemAllocFailure;
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto _Cleanup;
    }
    RtlZeroMemory(pTagInfo, ulSizeOfTagInfo);

    ASSERT(ecTagNoneInProgress == DriverContext.TagProtocolState);
    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);

    Status = InDskFltCompareExchangeAppConsistentTagState((PUCHAR)pBuffer, ecTagNoneInProgress, ecTagInsertTagStarted, &bValidGUIDContext, TRUE);
    if (!NT_SUCCESS(Status)) {
        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
        bValidGUIDContext = FALSE;
        ExFreePoolWithTag(pTagInfo, TAG_DB_TAG_INFO);
        pTagInfo = NULL;
        ulMessageType = ecMsgCompareExchangeTagStateFailure;
        goto _Cleanup;
    }

    // Build the list to insert tags
    DriverContext.pTagInfo = pTagInfo;
    DriverContext.TagType = TagCommonAttribs.TagType;

    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);

    if (TEST_FLAG(IoctlTagDeviceFlags, TAG_INFO_FLAGS_IGNORE_DISKS_IN_NON_WO_STATE))
        pTagInfo->ulFlags |= TAG_INFO_FLAGS_IGNORE_DISKS_IN_NON_WO_STATE;

    pTagInfo->ulNumberOfDevices = ulNumberOfDisks;

    for (ULONG index = 0; index < ulNumberOfDisks; index++) {
        pTagInfo->TagStatus[index].DevContext = GetDevContextWithGUID((PWCHAR)(pBuffer + index * GUID_SIZE_IN_BYTES + usDeviceGUIDIndex), GUID_SIZE_IN_BYTES);
        if (NULL != pTagInfo->TagStatus[index].DevContext) {
            pTagInfo->TagStatus[index].Status = STATUS_INVALID_DEVICE_STATE;
            InVolDbgPrint(DL_FUNC_TRACE, DM_TAGS, ("\tGUID=%S Volume=%S\n", pTagInfo->TagStatus[index].DevContext->wDevID, pTagInfo->TagStatus[index].DevContext->wDevNum));
        }
        else { // Devcontext is not found. Either it could be Raw disk or Disk removed
            pTagInfo->TagStatus[index].Status = STATUS_NOT_FOUND;
            pTagInfo->ulNumberOfDevicesIgnored++;
        }
    }

    if (pTagInfo->ulNumberOfDevicesIgnored) {
        ulMessageType = ecMsgAppDeviceNotFound;
        Status = STATUS_NOT_FOUND;
        goto _Cleanup;
    } 

    ulTagStartOffset = usDeviceGUIDIndex + (ulNumberOfDisks * GUID_SIZE_IN_BYTES);
    pTagInfo->pBuffer = (PVOID)(pBuffer + ulTagStartOffset);
    pTagInfo->TagCommonAttribs = TagCommonAttribs;

    Status = TagVolumesInSequence(pTagInfo, pBuffer + ulTagStartOffset);
    if (!NT_SUCCESS(Status)) {
        PTAG_DISK_STATUS_OUTPUT pTagDiskStatus = (TAG_DISK_STATUS_OUTPUT*)Irp->AssociatedIrp.SystemBuffer;
        for (ULONG index = 0; index < ulNumberOfDisks; index++) {
            pTagDiskStatus->TagStatus[index] = pTagInfo->TagStatus[index].Status;
        }
        Irp->IoStatus.Information = SIZE_OF_TAG_DISK_STATUS_OUTPUT_DATA(ulNumberOfDisks);
        ulMessageType = ecMsgTagVolumeInSequenceFailure;
        goto _Cleanup;
    } 
    // Handle Cancellation for IRP as timeout may be little delayed compared to Application exit or crash
    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);

    DriverContext.TagProtocolState = ecTagInsertTagCompleted;
    IoSetCancelRoutine(Irp, AppTagHoldIrpCancelRoutine);

    if (Irp->Cancel && IoSetCancelRoutine(Irp, NULL)) {
        Status = STATUS_CANCELLED;
        ulMessageType = ecMsgTagIrpCancelled;
      // Cancel is already marked and should complete as STATUS_CANCELLED
      // You need to revoke the tags and do cleanup
    } else {
        IoMarkIrpPending(Irp);
        Status = STATUS_PENDING;
        DriverContext.pTagProtocolHoldIrp = Irp;
        KeSetTimer(&DriverContext.AppTagProtocolTimer, Timeout, &DriverContext.AppTagProtocolTimerDpc);
        DriverContext.bTimer = TRUE;
        bCancelTimer = TRUE;
        bCancelIrp = TRUE;
    }

    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);

_Cleanup :
    if (!NT_SUCCESS(Status)) {
        KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
        InDskFltTelemetryTagIOCTLFailure(&TagCommonAttribs, Status, ulMessageType);
        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);

        if (bValidGUIDContext) {
            // do cleanup;
            Status = CleanupTagProtocol(bCancelTimer, bCancelIrp, ecRevokeAppTagInsertIOCTL);
        } else {
            Irp->IoStatus.Status = Status;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
        }
        return Status;
    }

    ASSERT(STATUS_PENDING == Status);
    return Status;
}


void
AppTagHoldIrpCancelRoutine(
    __in PDEVICE_OBJECT DeviceObject, 
    __in PIRP Irp
)
/*
Description : This routine is called as part of application exit.
a. Is timer is not running?
   Cleanup is alreay done, return
b. Is timer is running
   CancelTimer
   Mark the tag as Revoke
   Do cleanup
    Complete the IRP
    Reset the ContextGUID
    Free the TagInfo Buffer Allocated
    */
{
    IoReleaseCancelSpinLock(Irp->CancelIrql);

    UNREFERENCED_PARAMETER(DeviceObject);

    CleanupTagProtocol(TRUE, FALSE, ecRevokeCancelIrpRoutine);

}

NTSTATUS
DeviceIoControlCommitAppTagDisk(
__in   PDEVICE_OBJECT DeviceObject,
__in   PIRP Irp
)
/*
Description : This IOCTL is issued when the App consistency protocol (single or multi-node) released the barrier(by VSS provider)

Arguments :
DeviceObject     - pointer to control device object
Irp              - Pointer to the IRP

Handles state transitions :
ecAppTagInsertCompleted->ecAppTagCommitStarted - Owns the cleanup in case of IOCTL failures or protocol failures
otherwise does nothing

Input Flag : TAG_INFO_FLAGS_COMMIT_TAG indicates the driver the tag is accurate and otherwise driver would revoke the tag

Return   :
ulNumDisks                          Count of disks for which status is returned. This is filled starting 1802 release.

STATUS_SUCCESS                    - On succesful IOCTL with detailed status as per the "etTagStatus"
STATUS_DEVICE_BUSY                - One protocol is already active
STATUS_INFO_LENGTH_MISMATCH       - Error validating input buffer
STATUS_UNSUCCESSFUL               - protocol is already completed
STATUS_INVALID_DEVICE_STATE       - TagInfo is NULL
STATUS_BUFFER_TOO_SMALL           - output buffer is small

*/
{
    NTSTATUS              Status = STATUS_SUCCESS;
    ULONG                 ulFlags;
    PTAG_INFO             pTagInfo = NULL;	
    PTAG_DISK_STATUS_OUTPUT pTagDiskStatus = NULL;    
    BOOLEAN               bValidGUIDContext = TRUE;

    PIO_STACK_LOCATION    IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    PTAG_DISK_COMMIT_INPUT pCommitTagInput = NULL;
    ULONG                   ulMessageType   = ecMsgUninitialized;
    etTagStateTriggerReason tagStateReason  = ecNotApplicable;

    Irp->IoStatus.Information = 0;

    UNREFERENCED_PARAMETER(DeviceObject);
    ASSERT(IOCTL_INMAGE_COMMIT_TAG_DISK == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

    if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < sizeof(TAG_DISK_COMMIT_INPUT)) {
        ulMessageType = ecMsgAppInputBufferMismatch;
        Status = STATUS_INFO_LENGTH_MISMATCH;
        goto _Cleanup;
    }
    pCommitTagInput = (PTAG_DISK_COMMIT_INPUT)Irp->AssociatedIrp.SystemBuffer;

    Status = InDskFltCompareExchangeAppConsistentTagState(pCommitTagInput->TagContext, ecTagInsertTagCompleted, ecTagCommitStarted, &bValidGUIDContext);
    
    if (!NT_SUCCESS(Status)) {
        ulMessageType = ecMsgCompareExchangeTagStateFailure;
        goto _Cleanup;
    }  
    
    ASSERT(STATUS_SUCCESS == Status);

    ulFlags = pCommitTagInput->ulFlags;

    if (!TEST_FLAG(ulFlags, TAG_INFO_FLAGS_COMMIT_TAG))
        tagStateReason = ecRevokeCommitIOCTL;

    Status = CleanupTagProtocol(TRUE, TRUE, tagStateReason, (ulFlags & TAG_INFO_FLAGS_COMMIT_TAG) ? TRUE : FALSE, &pTagInfo);
    if (NULL == pTagInfo) {
        //CancelIrp or Timer is expired
        Status = STATUS_UNSUCCESSFUL;
        goto _Cleanup;
    }
    if (STATUS_CANCELLED == Status) {
        Status = STATUS_SUCCESS;
    }
    if (!NT_SUCCESS(Status)) {
        goto _Cleanup;
    }
    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < (SIZE_OF_TAG_DISK_STATUS_OUTPUT_DATA(pTagInfo->ulNumberOfDevices))) {
        ulMessageType = ecMsgAppOutputBufferTooSmall;
        Status = STATUS_BUFFER_TOO_SMALL;
        goto _Cleanup;
    }
    pTagDiskStatus = (TAG_DISK_STATUS_OUTPUT*)Irp->AssociatedIrp.SystemBuffer;
    pTagDiskStatus->ulNumDisks = pTagInfo->ulNumberOfDevices;
    for (ULONG index = 0; index < pTagInfo->ulNumberOfDevices; index++) {
        pTagDiskStatus->TagStatus[index] = pTagInfo->TagStatus[index].Status;
    }
    Irp->IoStatus.Information = SIZE_OF_TAG_DISK_STATUS_OUTPUT_DATA(pTagInfo->ulNumberOfDevices);

_Cleanup :
    // All other cases are handled as part of CleanupTagProtocol
    if (!NT_SUCCESS(Status) && !bValidGUIDContext) {
        InDskFltWriteEvent(INDSKFLT_ERROR_TAG_IOCTL_FAILURE, Status, ulMessageType,
                           IoStackLocation->Parameters.DeviceIoControl.IoControlCode);
    }

    if (NULL != pTagInfo)
        FreeTagInfo(pTagInfo);
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

void
InDskFltAppTagProtocolDPC(
__in struct _KDPC  *Dpc,
__in_opt PVOID  DeferredContext,
__in_opt PVOID  SystemArgument1,
__in_opt PVOID  SystemArgument2
)
/*
This routine would be called on expiration of time and does the cleanup operation
Valid state = ecAppTagInsertCompleted
*/
{
    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(DeferredContext);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    CleanupTagProtocol(FALSE, TRUE, ecRevokeTimeOut);
    return;
}

BOOLEAN
SetCommitRevoke(
__in unsigned long index, 
__in PDEV_CONTEXT pDevContext, 
__in PTAG_INFO    pTagInfo, 
__in BOOLEAN      CommitAll,
__in etTagStateTriggerReason tagStateReason
)
/*
Description : This function marks the diry block as revoke or commit and subsequently dirtyblock would be purged later
Runs at DISPATCH_LEVEL
*/
{
    PLIST_ENTRY       pCurr;
    BOOLEAN           bFound = FALSE;
    ULONG             bTagDBCount = 0;

    pCurr = pDevContext->ChangeList.Head.Flink;
    while (pCurr != &pDevContext->ChangeList.Head) {
        PCHANGE_NODE pChangeNode = (PCHANGE_NODE)pCurr;
        PKDIRTY_BLOCK pDirtyBlock = pChangeNode->DirtyBlock;
        // if it is not success, add if it is valid
        if (TEST_FLAG(pDirtyBlock->ulFlags, KDIRTY_BLOCK_FLAG_CONTAINS_TAGS) &&
            TEST_FLAG(pDirtyBlock->ulFlags, KDIRTY_BLOCK_FLAG_TAGS_PENDING)) {
            bFound = TRUE;
            ++bTagDBCount;

            if (NULL != pDirtyBlock->pTagHistory) {
                KeQuerySystemTime(&pDirtyBlock->pTagHistory->liTagCompleteTimeStamp);
            }

            if (TRUE == CommitAll) {
                pTagInfo->TagStatus[index].Status = ecTagStatusCommited;
            } else {
                SET_FLAG(pDirtyBlock->ulFlags, KDIRTY_BLOCK_FLAG_REVOKE_TAGS);
                pTagInfo->TagStatus[index].Status = ecTagStatusRevoked;

                if (NULL != pDirtyBlock->pTagHistory) {
                    InDskFltTelemetryLogTagHistory(pDirtyBlock, pDevContext, ecTagStatusRevoked, tagStateReason, ecMsgTagRevoked);
                }
            }
            CLEAR_FLAG(pDirtyBlock->ulFlags, KDIRTY_BLOCK_FLAG_TAGS_PENDING);
        }

        pCurr = pCurr->Flink;
    }

    // This is added to know is there any leak in any protocol which would accidentally
    //  commit more than one dirtyblocks and may cause DI
    if (bTagDBCount > 1) {
        
        InVolDbgPrint(DL_ERROR, DM_DIRTY_BLOCKS,
            (" Found multiple tag dirtyblocks with pending state - Count : %u, CommitALL : %u\n",
            bTagDBCount, CommitAll));

        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_UNEXPECTED_NUMER_OF_TAGDB, pDevContext, bTagDBCount, CommitAll);
    }

    // If there is any committed tag, set event to drain it
    // Even for revoked tag there can be pending changes during tag was in pending state
    if (bFound) {
        if (pDevContext->bNotify && pDevContext->DBNotifyEvent && DirtyBlockReadyForDrain(pDevContext)) {
            pDevContext->liTickCountAtNotifyEventSet = KeQueryPerformanceCounter(NULL);
            KeSetEvent(pDevContext->DBNotifyEvent, EVENT_INCREMENT, FALSE);
            pDevContext->bNotify = false;
        }
    }

    return bFound;
}

void
FinalizeDeviceTagStateAll(
    __in PTAG_INFO pTagInfo,
    __in bool      LockAcquired,
    __in BOOLEAN   CommitAll,
    __in etTagStateTriggerReason tagStateReason
 )
 /*
Description : This function finds and finalizes the state of tag either commit or revoke for all protected devices

Arguments   : 
pTagInfo      - pointer to TagInfo structure 
LockAcquired  - For future use
CommitAll     - KDIRTY_BLOCK_FLAG_TAGS_PENDING -> KDIRTY_BLOCK_FLAG_COMMIT_TAGS 

Return        - None
Status would be set appropriately in the TagInfo structure 
*/
{
    KIRQL                OldIrql = 0;
    PDEV_CONTEXT         pDevContext = NULL;
    BOOLEAN              bFound = FALSE;

    UNREFERENCED_PARAMETER(LockAcquired);

    ASSERT(NULL != pTagInfo);
    ASSERT(0 != pTagInfo->ulNumberOfDevices);
    IndskFltValidateTagInsertion(pTagInfo);
    if ((NULL != pTagInfo) && (pTagInfo->ulNumberOfDevices) && (pTagInfo->ulNumberOfDevicesTagsApplied)) {

        for (unsigned long index = 0; index < pTagInfo->ulNumberOfDevices; index++) {
            // Reference already held for valid device context
            if (STATUS_SUCCESS != pTagInfo->TagStatus[index].Status) {
                pTagInfo->TagStatus[index].Status = ecTagStatusFailure;
                continue;
            }
            pDevContext = pTagInfo->TagStatus[index].DevContext; 
            if (NULL != pDevContext) {
                KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
                if (FALSE == TEST_FLAG(pDevContext->ulFlags, DCF_FILTERING_STOPPED) &&
                    (FALSE == TEST_FLAG(pDevContext->ulFlags, DCF_REMOVE_CLEANUP_PENDING))) {
                    // Parse through list of change nodes
                    // Verify tags are in head list only
                    bFound = SetCommitRevoke(index, pDevContext, pTagInfo, CommitAll, tagStateReason);

                    if (FALSE == bFound)
                        pTagInfo->TagStatus[index].Status = ecTagStatusDropped;
                } else { // stop filtering or surprise removal
                    pTagInfo->TagStatus[index].Status = ecTagStatusDeleted;
                }
                KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
            } else { // if (NULL != DevContext)
                pTagInfo->TagStatus[index].Status = ecTagStatusInvalidGUID;
            }
        } // loop through all devices

    } 
    else { // TagInfo == NULL or No Tags are applied
        if (NULL != pTagInfo)
            pTagInfo->Status = STATUS_INVALID_DEVICE_STATE;
    }
    return;
}

