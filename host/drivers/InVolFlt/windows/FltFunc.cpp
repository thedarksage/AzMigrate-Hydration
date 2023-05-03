/**
 * @file abhaiflt.c
 *
 * Kernel mode driver that captures all block changes to logical and physical volumes.
 * These changes are drained by a user space service, expanded with block data, and written
 * to a target volume.
 *

 */

#define INITGUID
#include "FltFunc.h"
#ifdef VOLUME_FLT
#include "MntMgr.h"
#else
#include <ioevent.h>
#endif
#include "IoctlInfo.h"
#include "ListNode.h"
#include "misc.h"
#include "VBitmap.h"
#include "HelperFunctions.h"
#include "VContext.h"
#include "VBitmap.h"

#include "ntddscsi.h"
#include "scsi.h"

#ifdef VOLUME_FLT
extern "C" ULONG
FillAdditionalDeviceIDs(
    PDEV_CONTEXT DevContext,
    ULONG           &ulGUIDsWritten,
    PUCHAR          pBuffer,
    ULONG           ulLength
    );
#endif

NTSTATUS
BootVolumeDriverNotificationCallback(
    PVOID NotificationStructure,
    PVOID pContext
    );

#define DEBUG_BUFFER_LENGTH 256

// ulDebugLevelForAll specifies the debug level for all modules
// if this is non zero all modules info upto this level is displayed.
ULONG   ulDebugLevelForAll = DL_WARNING;
// ulDebugLevel specifies the debug level for modules specifed in
// ulDebugMask
ULONG   ulDebugLevel = DL_TRACE_L1;
// ulDebugMask specifies the modules whose debug level is specified
// in ulDebugLevel
ULONG   ulDebugMask = DM_CLUSTER | DM_BITMAP_OPEN | DM_BITMAP_READ | 
                      DM_DRIVER_INIT | DM_SHUTDOWN | DM_DEV_ARRIVAL | 
                      DM_DRIVER_PNP | DM_TIME_STAMP | DM_WO_STATE | 
                      DM_CAPTURE_MODE | DM_BYPASS|DM_IOCTL_TRACE |
                      DM_FILE_OPEN | DM_FILE_RAW_WRAPPER;

extern WCHAR *ErrorToRegErrorDescriptionsW[];


/*
Description:	This completion routine is called by lowe level driver with "STATUS_MORE_PORCESSING_REQUIRED', so that our driver would 
                    wait and complete the driver later.
Args             DeviceObject is the device object of volsnap driver
                    Irp is the one just completed
                    Context has the event upon which the calling driver waits on

Return Value:    STATUS_MORE_PORCESSING_REQUIRED

*/
NTSTATUS
InMageFltIrpCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
{
    PKEVENT kEvent = (PKEVENT) Context;

    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(Irp);

    KeSetEvent(kEvent, IO_NO_INCREMENT, FALSE);

    return(STATUS_MORE_PROCESSING_REQUIRED);

} 

NTSTATUS
ValidateRequestForDeviceAndHandle (
    __in    PDEVICE_OBJECT    DeviceObject,
    __in    PIRP              Irp
    )
/*++

Routine Description:

    Validate the DeviceObject and the request
    Complete/Skip the request

Arguments:

    DeviceObject - Pointer to the target device object
    Irp - IRP sent by IoManager

Return Value:

    NTSTATUS

--*/
{
    PDEVICE_EXTENSION       pDeviceExtension = NULL;

    if ((DeviceObject == DriverContext.ControlDevice) ||
        (DeviceObject == DriverContext.PostShutdownDO)) {
        Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_NOT_IMPLEMENTED;
    }

    pDeviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

    if (NULL == pDeviceExtension->TargetDeviceObject) {		
        Irp->IoStatus.Status = STATUS_DEVICE_NOT_CONNECTED;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_DEVICE_NOT_READY;

    }
    return STATUS_SUCCESS;
}

/*
Routine Description: This routine sends the Irp to the next driver in line when the Irp is not processed by this driver.

Arguments:     DeviceObject - Pointer to the target device object
                      Irp                - IRP sent by IoManager

Return Value:    NTSTATUS
*/
NTSTATUS
InMageFltSendToNextDriver(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    if ((DeviceObject == DriverContext.ControlDevice) ||
        (DeviceObject == DriverContext.PostShutdownDO)) {
        Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_NOT_IMPLEMENTED;
    }

    PDEVICE_EXTENSION   pDeviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

    if (pDeviceExtension->TargetDeviceObject) {
        IoSkipCurrentIrpStackLocation(Irp);
        return IoCallDriver(pDeviceExtension->TargetDeviceObject, Irp);
    }
	Irp->IoStatus.Status = STATUS_DEVICE_NOT_CONNECTED;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_DEVICE_NOT_READY;

} // end InMageFltSendToNextDriver()

/*
Description: This routine sends the Irp to the next driver in line when the Irp needs to be processed by the lower drivers
                   prior to being processed by this one.

Args			:   DeviceObject - Encapsulates the current volume context
					Irp                - The i/o request meant for lower level drivers, usually

Return Value:    NTSTATUS

*/
NTSTATUS
InMageFltForwardIrpSynchronous(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    PDEVICE_EXTENSION   pDeviceExtension;
    KEVENT Event;
    NTSTATUS Status;

    KeInitializeEvent(&Event, NotificationEvent, FALSE);
    pDeviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

    // Simply copy the current stack location to the next stack

    IoCopyCurrentIrpStackLocationToNext(Irp);

    // Syncronous call, setting up the completion routine

    IoSetCompletionRoutine(Irp, InMageFltIrpCompletion,
                            &Event, TRUE, TRUE, TRUE);

    // Just call the next lower level driver

    Status = IoCallDriver(pDeviceExtension->TargetDeviceObject, Irp);

    // let us wait for the completion routine to come back here
    
    if (Status == STATUS_PENDING) {
        KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
        Status = Irp->IoStatus.Status;
    }

    return Status;

} 

NTSTATUS
InmageGetTopOfDeviceStack(
    _In_    PDEV_CONTEXT pDevContext,
    _Out_ PFILE_OBJECT* FileObject,
    _Out_ PDEVICE_OBJECT* DeviceObject
)
/*++

Routine Description:

This routine figures top of disk stack by using api IoGetDeviceObjectPointer.

Arguments:
        pDevContext:    Device Context
        FileObject:     File Object returned by IoGetDeviceObjectPointer.
        DeviceObject:   Device Object returned by IoGetDeviceObjectPointer.
None.

Return Value:
    NTSTATUS
*/
{
    WCHAR                   diskName[50];
    UNICODE_STRING          diskNameStr;
    NTSTATUS                status;

    // Top of disk stack can be figured only if Device number is obtained.
    if (!TEST_FLAG(pDevContext->ulFlags, DCF_DEVNUM_OBTAINED))
    {
        return STATUS_INVALID_PARAMETER;
    }

    // Prepare unicode string for Disk name string
    RtlStringCbPrintfW(diskName, sizeof(diskName), DISK_NAME_FORMAT, pDevContext->ulDevNum);
    RtlUnicodeStringInit(&diskNameStr, diskName);

    status = IoGetDeviceObjectPointer(&diskNameStr,
        FILE_READ_ATTRIBUTES,
        FileObject,
        DeviceObject);

    if (!NT_SUCCESS(status)) {

        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_FUNCTION_FAILURE, pDevContext,
            diskName,
            status);
    }
    return status;
}

NTSTATUS
InMageSendDeviceControl(
    __in      PDEVICE_OBJECT DeviceObject,
    __in      ULONG IoControlCode,
    __in_opt  PVOID InputBuffer,
    __in      ULONG InputBufferLength,
    __out_opt PVOID OutputBuffer,
    __in      ULONG OutputBufferLength,
    __inout   PIO_STATUS_BLOCK pIoStatus,
    __in_opt  BOOLEAN InternalDeviceIoControl
    )

/*++

Routine Description:

    Call the given IOCTL on a target device.

Arguments:

    DeviceObject            - Target Device Object

    IoControlCode           - IOCTL code

    InputBuffer             - Input buffer, It can be NULL

    InputBufferLength       - Length of input buffer; must be 0 if
                              InputBuffer is NULL

    OutputBuffer            - Output buffer, can be NULL

    OutputBufferLength      - Length of output buffer; must be 0 if
                              InputBuffer is NULL

    InternalDeviceIoControl - TRUE if this is IRP_MJ_INTERNAL_DEVICE_CONTROL

Return Value:

    NTSTATUS.

--*/

{
    PIRP            Irp      = NULL;
    KEVENT          Event    = {0};
    NTSTATUS        Status   = STATUS_SUCCESS;

    PAGED_CODE();

    KeInitializeEvent(&Event, NotificationEvent, FALSE);

    Irp = IoBuildDeviceIoControlRequest(IoControlCode,
                                        DeviceObject,
                                        InputBuffer,
                                        InputBufferLength,
                                        OutputBuffer,
                                        OutputBufferLength,
                                        InternalDeviceIoControl,
                                        &Event,
                                        pIoStatus);
    if (Irp) {
        Status = IoCallDriver(DeviceObject, Irp);

        if (Status == STATUS_PENDING) {
            KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
            Status = pIoStatus->Status;
        }
    } else {
        Status = STATUS_INSUFFICIENT_RESOURCES;
    }

    if (NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_INFO,
                DM_DEV_CONTEXT,
                ("%s%: IOCTL 0x%x Success, Status = %#x\n",
                __FUNCTION__,
                IoControlCode,
                Status));

    } else {
        InVolDbgPrint(DL_ERROR, 
                      DM_DEV_CONTEXT,
                      ("%s%: IOCTL 0x%x Failed, Status = %#x\n",
                      __FUNCTION__,
                      IoControlCode,
                      Status));
    }

    return Status;
}


/*
Description: The routine service any open requests. Filter driver just forwards them to lower level drivers

Args			:   DeviceObject - Encapsulates the current volume context
					Irp                - The i/o request meant for lower level drivers, usually

Return Value:   NT Status
*/

NTSTATUS
InMageFltCreate(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    PDEVICE_EXTENSION   pDeviceExtension;

    if ((DeviceObject == DriverContext.ControlDevice) ||
        (DeviceObject == DriverContext.PostShutdownDO)) {
        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }

    pDeviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

    // Invalid Case(ASSERT). In case a call comes to NoReboot Device directly
    if (NULL == pDeviceExtension->TargetDeviceObject) {
        Irp->IoStatus.Status = STATUS_DEVICE_NOT_CONNECTED;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_DEVICE_NOT_CONNECTED;
    }

    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(pDeviceExtension->TargetDeviceObject, Irp);

} // end InMageFltCreate()

NTSTATUS
InMageFltClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    PDEVICE_EXTENSION   pDeviceExtension;
    pDeviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

    if ((DeviceObject == DriverContext.ControlDevice) || 
        (DeviceObject == DriverContext.PostShutdownDO) ||
        (NULL == pDeviceExtension))
    {
        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }

    if (NULL == pDeviceExtension->TargetDeviceObject) {
        Irp->IoStatus.Status = STATUS_DEVICE_NOT_CONNECTED;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_DEVICE_NOT_CONNECTED;
    }

    IoSkipCurrentIrpStackLocation(Irp);

    return IoCallDriver(pDeviceExtension->TargetDeviceObject, Irp);

} // end InMageFltClose()


NTSTATUS
InDskFltCleanup(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    PDEVICE_EXTENSION   pDeviceExtension;
    PIO_STACK_LOCATION  IrpSp;
    PIO_STACK_LOCATION  holdIrpSp = NULL;
    PIRP                holdIrp;
    PFILE_OBJECT        fileObj;
    NTSTATUS            Status = STATUS_SUCCESS;
    KIRQL               irql;
    BOOLEAN             bTagCleanup = FALSE;

    pDeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    IrpSp = IoGetCurrentIrpStackLocation(Irp);

    fileObj = IrpSp->FileObject;

    // If this IRP is meant for disk Do following
    //  1) Check for all queued writes on this disk.
    //  2) Cancel all writes for this disk handle.
    if (DeviceObject != DriverContext.ControlDevice)
    {
        if ((DeviceObject == DriverContext.PostShutdownDO) || (NULL == pDeviceExtension))
        {
            goto _cleanup;
        }
        Status = InMageFltCleanup(DeviceObject, Irp);
        return Status;
    }

    KeAcquireSpinLock(&DriverContext.Lock, &irql);
    // This IRP is sent on Control Device
    holdIrp = DriverContext.pTagProtocolHoldIrp;

    // If handle on which pTagProtocolHoldIrp was sent is closed
    // Cancel holdIrp
    if (holdIrp)
    {
        holdIrpSp = IoGetCurrentIrpStackLocation(holdIrp);

        if (holdIrpSp->FileObject == fileObj)
        {
            if (!CAN_HANDLE_TAG_CLEANUP())
            {
                DriverContext.TagStatus = STATUS_CANCELLED;
                DriverContext.TagProtocolState = ecTagCleanup;
            }
            else
            {
                bTagCleanup = TRUE;
            }
        }
    }
    KeReleaseSpinLock(&DriverContext.Lock, irql);

    if (bTagCleanup)
    {
        InDskFltCrashConsistentTagCleanup(ecRevokeDistrubutedCrashCleanupTag);
    }

_cleanup:
    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
} // end InMageFltCleanup()

NTSTATUS
InMageFltCleanup(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN BOOLEAN bNoRebootLayer,
    IN PDRIVER_DISPATCH pDispatchEntryFunction
    )
{
    PFILE_OBJECT        fileObj = NULL;
    PIO_STACK_LOCATION  IrpSp = NULL;
    PDEVICE_EXTENSION   pDeviceExtension = NULL;
    pDeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    IrpSp = IoGetCurrentIrpStackLocation(Irp);
    fileObj = IrpSp->FileObject;

    InDskFltCancelQueuedWriteForFileObject(pDeviceExtension, fileObj);
    InSkipCurrentIrpStackLocation(Irp, bNoRebootLayer);
    return InCallDriver(pDeviceExtension->TargetDeviceObject, Irp, bNoRebootLayer, pDispatchEntryFunction);
}


#ifdef DBG
NTSTATUS
InMageFltRead(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    PDEVICE_EXTENSION   pDeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PDEV_CONTEXT        pDevContext = NULL;
    PIO_STACK_LOCATION  currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION  topIrpStack;

    InVolDbgPrint(DL_VERBOSE, DM_WRITE, ("InMageFltRead: Device = %#p  Irp = %#p Length = %#x  Offset = %#I64x\n", 
            DeviceObject, Irp, currentIrpStack->Parameters.Read.Length,
            currentIrpStack->Parameters.Read.ByteOffset.QuadPart));

    if ((DeviceObject == DriverContext.ControlDevice) ||
        (DeviceObject == DriverContext.PostShutdownDO)) {
        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }

    pDevContext = pDeviceExtension->pDevContext;
    if ((NULL == pDevContext) || (pDevContext->ulFlags & DCF_FILTERING_STOPPED)){
        return InMageFltSendToNextDriver(DeviceObject, Irp);
    }

    // walk down the irp and find the originating file in the first irp stack slot
    topIrpStack = (PIO_STACK_LOCATION)((PCHAR)Irp + sizeof(IRP)) + (Irp->StackCount - 1);
#ifdef VOLUME_FLT
    // Should we profile these reads too?
    if ((DCF_PAGEFILE_WRITES_IGNORED & pDevContext->ulFlags) && (NULL != pDeviceExtension->pagingFile)) {
        if (pDeviceExtension->pagingFile == topIrpStack->FileObject) {
            return InMageFltSendToNextDriver(DeviceObject, Irp);
        }
    }
#endif
    LogReadIOSize(currentIrpStack->Parameters.Read.Length, &pDevContext->DeviceProfile);
    
    return InMageFltSendToNextDriver(DeviceObject, Irp);
} // end InMageFltRead()
#endif

VOID
InMageFltCancelProcessStartIrp (
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    )
{
    KIRQL       OldIrql;
    BOOLEAN     bUnmapData = FALSE;
    PEPROCESS   UserProcess;
    LIST_ENTRY  VolumeNodeList;
    NTSTATUS    Status;

	UNREFERENCED_PARAMETER(DeviceObject);

    IoReleaseCancelSpinLock(Irp->CancelIrql);

    UserProcess = IoGetCurrentProcess();

    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    if (DriverContext.ProcessStartIrp == Irp) {
        ASSERT(UserProcess == DriverContext.UserProcess);
        InVolDbgPrint(DL_INFO, DM_DEVICE_STATE, 
            ("InMageFltCancelProcessStartIrp: Canceling process start Irp, Irp = %#p\n", Irp));
        
        DriverContext.ProcessStartIrp = NULL;
        KeQuerySystemTime(&DriverContext.globalTelemetryInfo.liLastS2StopTime);
        bUnmapData = TRUE;
    }

    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);


PAGED_CODE();
    if (bUnmapData) {
        CxNotifyProductIssue(ProductIssueS2Quit);
        FreeOrphanedMappedDataBlockList();

        // Unmap the data from user process.
        InitializeListHead(&VolumeNodeList);
        Status = GetListOfDevs(&VolumeNodeList);
        if (STATUS_SUCCESS == Status) {
            while(!IsListEmpty(&VolumeNodeList)) {
                PLIST_NODE  pNode;
                PDEV_CONTEXT DevContext;

                pNode = (PLIST_NODE)RemoveHeadList(&VolumeNodeList);
                DevContext = (PDEV_CONTEXT)pNode->pData;
                DereferenceListNode(pNode);
                KeWaitForSingleObject(&DevContext->SyncEvent, Executive, KernelMode, FALSE, NULL);
                if (DevContext->pDBPendingTransactionConfirm) {
                    PKDIRTY_BLOCK    DirtyBlock = DevContext->pDBPendingTransactionConfirm;
                    PDATA_BLOCK      DataBlock = (PDATA_BLOCK)DirtyBlock->DataBlockList.Flink;

                    while ((PLIST_ENTRY)DataBlock != &DirtyBlock->DataBlockList) {
                        UnMapDataBlock(DataBlock);
                        if (NULL != DataBlock->pUserBufferArray) {
                            SIZE_T   ulFreeSize = 0;
                            ZwFreeVirtualMemory(NtCurrentProcess(), (PVOID *)&DataBlock->pUserBufferArray, &ulFreeSize, MEM_RELEASE);
                            DataBlock->pUserBufferArray = NULL;
                        }
                        UnLockDataBlock(DataBlock);
                        DataBlock = (PDATA_BLOCK)DataBlock->ListEntry.Flink;
                    } // for each data block in pending transaction

                    // Remove the prepared for user mode flag. 
                    // This would allow data to be mapped again when service starts
                    DirtyBlock->ulFlags &= ~KDIRTY_BLOCK_FLAG_PREPARED_FOR_USERMODE;
                } // if pending transaction
                KeSetEvent(&DevContext->SyncEvent, IO_NO_INCREMENT, FALSE);
                KeAcquireSpinLock(&DevContext->Lock, &OldIrql);
                if (DevContext->DBNotifyEvent) {
                    ObDereferenceObject(DevContext->DBNotifyEvent);
                    DevContext->DBNotifyEvent = NULL;
                    DevContext->bNotify = false;
                    DevContext->liTickCountAtNotifyEventSet.QuadPart = 0;
                }
                KeReleaseSpinLock(&DevContext->Lock, OldIrql);

                DereferenceDevContext(DevContext);
            } // for each volume
        }
        DriverContext.UserProcess = NULL;
    }

    Irp->IoStatus.Status = STATUS_CANCELLED;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return;
}

VOID
InMageFltCancelServiceShutdownIrp (
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    )
{
    KIRQL       OldIrql;
	UNREFERENCED_PARAMETER(DeviceObject);

    BOOLEAN     isValidIrp = FALSE;
    IoReleaseCancelSpinLock(Irp->CancelIrql);

    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    if (DriverContext.ServiceShutdownIrp == Irp)
    {
        InVolDbgPrint(DL_INFO, DM_DEVICE_STATE, 
            ("InMageFltCancelServiceShutdownIrp: Canceling shutdown Irp, Irp = %#p\n", Irp));
        InDskFltWriteEvent(INDSKFLT_INFO_SERVICE_SHUTDOWN_NOTIFICATION);
        isValidIrp = TRUE;
        DriverContext.ServiceShutdownIrp = NULL;
        ASSERT(ecServiceRunning == DriverContext.eServiceState);
        DriverContext.eServiceState = ecServiceShutdown;
        KeQuerySystemTime(&DriverContext.globalTelemetryInfo.liLastAgentStopTime);
        DriverContext.ulFlags |= DC_FLAGS_SERVICE_STATE_CHANGED;

        // Trigger activity to start writing existing data to bit map file.
        KeSetEvent(&DriverContext.ServiceThreadWakeEvent, IO_DISK_INCREMENT, FALSE);
    }

    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);
    if (isValidIrp) {
        CxNotifyProductIssue(ProductIssueSvAgentsQuit);
    }

    Irp->IoStatus.Status = STATUS_CANCELLED;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return;
}

// This function is called holding spin lock of DEVICE_CONTEXT and DriverContext both.
BOOLEAN
ProcessDevStateChange(
    PDEV_CONTEXT     pDevContext,
    PLIST_ENTRY         ListHead,
    BOOLEAN             bServiceStateChanged
    )
{
    BOOLEAN             bReturn = TRUE;
    BOOLEAN             bWriteToBitmap = FALSE;

    if (ecDSDBCdeviceOffline == pDevContext->eDSDBCdeviceState) {
#ifdef VOLUME_FLT
        // Unlink the dirty block context from DriverContext LinkedList.
        RemoveEntryList((PLIST_ENTRY)pDevContext);
        DriverContext.ulNumDevs--;
        InitializeListHead((PLIST_ENTRY)pDevContext);
        //pDevContext->ulFlags |= DCF_REMOVED_FROM_LIST; // Helps in debugging to know if it is removed from list.

        bReturn = AddVCWorkItemToList(ecWorkItemForDeviceUnload, pDevContext, 0, FALSE, ListHead);
        DereferenceDevContext(pDevContext);

        return bReturn;
#else
        bReturn = AddVCWorkItemToList(ecWorkItemForDeviceUnload, pDevContext, 0, FALSE, ListHead);
        if (TRUE == bReturn) {
            // Required cleanup is done through the work item
            // Device context get removed towards the end of cleanup
            // Change the state to not process further  offline state
            pDevContext->eDSDBCdeviceState =  ecDSDBCdeviceDontProcess;
        }
        return bReturn;
#endif
    }
    if (ecDSDBCdeviceDontProcess == pDevContext->eDSDBCdeviceState) {
        return bReturn;
    }

    if (pDevContext->ulFlags & DCF_FILTERING_STOPPED) {
        // If volume filtering is disabled. we should not process this volume context.
        return bReturn;
    }

    // If device is surprise removed, we should not process this volume context.
    // In case of few ISCSI devices, we receive only IRP_MN_SURPRISE_REMOVE but not IRP_MN_REMOVE_DEVICE
    // We have orphaned volume contexts, as system does not remove them.
    if (pDevContext->ulFlags & DCF_SURPRISE_REMOVAL) {
        InVolDbgPrint(DL_INFO, DM_CLUSTER | DM_DEV_CONTEXT,
            ("ProcessDevStateChange: Device %#p, DevNum=%s is surprise removed skipping processing...\n",
            pDevContext, pDevContext->chDevNum));
        return bReturn;
    }

#ifdef VOLUME_CLUSTER_SUPPORT
    if (IS_VOLUME_OFFLINE(pDevContext)) {
        InVolDbgPrint(DL_INFO, DM_CLUSTER | DM_DEV_CONTEXT,
            ("ProcessDevStateChange: Device %#p, DevNum=%s is offline skipping bitmap load\n",
                    pDevContext, pDevContext->chDevNum));
        return bReturn;
    }
#endif
    // See if the bitmap file has to be opened. If so open the bitmap file.
    if (pDevContext->ulFlags & DCF_OPEN_BITMAP_REQUESTED) {
        bReturn = AddVCWorkItemToList(ecWorkItemOpenBitmap, pDevContext, 0, FALSE, ListHead);
        if (!bReturn)
            return bReturn;
    } else if (0 == (pDevContext->ulFlags & DCF_DEVID_OBTAINED)) {
        // Open bitmap is not requested, and GUID is not obtained for this volume yet.
        // As GUID is not obtained for this volume yet, do not open bitmap just return.
        return bReturn;
    }

    // If Non Page pool consumption exceeds 80% of Max. Non Page Pool Limit,
    // write to Bitmap. This is to avoid pair resync which occurs if we reach
    // max. non-page limit.
    if ((0 != DriverContext.ulMaxNonPagedPool) && (DriverContext.lNonPagedMemoryAllocated >= 0)) {
        ULONGLONG NonPagePoolLimit = DriverContext.ulMaxNonPagedPool * 8;
        NonPagePoolLimit /= 10;
        if ((ULONG) DriverContext.lNonPagedMemoryAllocated >= (ULONG) NonPagePoolLimit) {
            bWriteToBitmap = TRUE;
        }    
    }

    switch(DriverContext.eServiceState) {
        case ecServiceNotStarted:
            if (DriverContext.DBHighWaterMarks[ecServiceNotStarted] && 
                (0 == (pDevContext->ulFlags & DCF_BITMAP_WRITE_DISABLED)) &&
                (pDevContext->ChangeList.ulTotalChangesPending >= DriverContext.DBHighWaterMarks[ecServiceNotStarted])) 
            {
                bReturn = AddVCWorkItemToList(ecWorkItemBitmapWrite, pDevContext, 0, TRUE, ListHead);
            }
            break;
        case ecServiceRunning:
            // so the device is Online and service is running.
            if (bServiceStateChanged) {
                if (DriverContext.DBLowWaterMarkWhileServiceRunning &&
                    (0 == (pDevContext->ulFlags & DCF_BITMAP_READ_DISABLED)) &&
                    (pDevContext->ChangeList.ulTotalChangesPending < DriverContext.DBLowWaterMarkWhileServiceRunning))
                {
                    bReturn = AddVCWorkItemToList(ecWorkItemStartBitmapRead, pDevContext, 0, TRUE, ListHead);
                }
            } else {
                if (DriverContext.DBHighWaterMarks[ecServiceRunning] && 
                    (0 == (pDevContext->ulFlags & DCF_BITMAP_WRITE_DISABLED)) &&
                    ((pDevContext->ChangeList.ulTotalChangesPending >= DriverContext.DBHighWaterMarks[ecServiceRunning]) ||
                    (bWriteToBitmap == TRUE))) 
                {
                    ULONG ChangesToPurge = 0;
                    if (pDevContext->ChangeList.ulTotalChangesPending >= DriverContext.DBHighWaterMarks[ecServiceRunning]) {
                        ChangesToPurge = DriverContext.DBToPurgeWhenHighWaterMarkIsReached;
                    } else {
                        ChangesToPurge = KILOBYTES;
                    }
                    bReturn = AddVCWorkItemToList(ecWorkItemBitmapWrite, pDevContext, 
                                            ChangesToPurge, TRUE, ListHead);
                } else if (DriverContext.DBLowWaterMarkWhileServiceRunning &&
                    (0 == (pDevContext->ulFlags & DCF_BITMAP_READ_DISABLED)) &&
                    (pDevContext->ChangeList.ulTotalChangesPending < DriverContext.DBLowWaterMarkWhileServiceRunning)) 
                {
                    if (NULL == pDevContext->pDevBitmap) {
                        bReturn = AddVCWorkItemToList(ecWorkItemStartBitmapRead, pDevContext, 0, TRUE, ListHead);
                    } else {
                        if ((ecVBitmapStateOpened == pDevContext->pDevBitmap->eVBitmapState) ||
                            (ecVBitmapStateAddingChanges == pDevContext->pDevBitmap->eVBitmapState))
                        {
                            bReturn = AddVCWorkItemToList(ecWorkItemStartBitmapRead, pDevContext, 0, FALSE, ListHead);
                        } else if (ecVBitmapStateReadPaused == pDevContext->pDevBitmap->eVBitmapState) {
                            bReturn = AddVCWorkItemToList(ecWorkItemContinueBitmapRead, pDevContext, 0, FALSE, ListHead);
                        }
                    }
                }   
            }
            break;
        case ecServiceShutdown:
            // service is shutdown write all pending dirty blocks to bit map.
            if (bServiceStateChanged) {
                // move all commit pending dirty blocks to main queue.
                OrphanMappedDataBlocksAndMoveCommitPendingDirtyBlockToMainQueue(pDevContext);
                // TODO: This has to be removed from here and has to be taken care when dirty blocks are taken out
                // and queued to write to bitmap
                VCSetWOState(pDevContext, true, __FUNCTION__, ecMDReasonServiceShutdown);
                // Free all reserved data.
                InVolDbgPrint(DL_INFO, DM_CAPTURE_MODE | DM_DATA_FILTERING, 
                              ("%s: %s(%s)Changing capture mode to META-DATA as service is shutdown\n", __FUNCTION__,
                               pDevContext->chDevID, pDevContext->chDevNum));
                VCSetCaptureModeToMetadata(pDevContext, true);
                VCFreeUsedDataBlockList(pDevContext, true);
                if (0 == (pDevContext->ulFlags & DCF_BITMAP_WRITE_DISABLED)) {
                    bReturn = AddVCWorkItemToList(ecWorkItemBitmapWrite, pDevContext, 0, TRUE, ListHead);
                }
            } else {
                if (DriverContext.DBHighWaterMarks[ecServiceShutdown] && 
                        (0 == (pDevContext->ulFlags & DCF_BITMAP_WRITE_DISABLED)) &&
                        (pDevContext->ChangeList.ulTotalChangesPending >= DriverContext.DBHighWaterMarks[ecServiceShutdown])) 
                {
                    bReturn = AddVCWorkItemToList(ecWorkItemBitmapWrite, pDevContext, 0, TRUE, ListHead);
                }
            }
            break;
        default:
            ASSERT(0);
            break;
    }

    return bReturn;
}

VOID
ServiceStateChangeFunction(
    IN PVOID Context
    )
{
    KIRQL           OldIrql, DeviceContextIrql;
    BOOLEAN         bShutdownThread = FALSE;
    PVOID           WaitObjects[2];
    NTSTATUS        Status;
    PDEV_CONTEXT pDevContext;
    PLIST_ENTRY     pEntry;
    BOOLEAN         bServiceStateChanged;
    LIST_ENTRY      WorkQueueEntriesListHead;
    PWORK_QUEUE_ENTRY   pWorkItem;

	UNREFERENCED_PARAMETER(Context);

    WaitObjects[0] = &DriverContext.ServiceThreadWakeEvent;
    WaitObjects[1] = &DriverContext.ServiceThreadShutdownEvent;
    do {
        Status = KeWaitForMultipleObjects(2, WaitObjects, WaitAny, Executive, KernelMode, FALSE, NULL, NULL);
        if (!NT_SUCCESS(Status)) {
            break;
        }

        if (STATUS_WAIT_1 == Status) {
            bShutdownThread = TRUE;
            break;
        }

        if (STATUS_WAIT_0 == Status) {
            InitializeListHead(&WorkQueueEntriesListHead);

            if ((ecServiceShutdown == DriverContext.eServiceState) && (DC_FLAGS_SERVICE_STATE_CHANGED & DriverContext.ulFlags)) {
                DriverContext.DataFileWriterManager->FlushAllWriterQueues();
            }

            // Scan all Volume Contexts.
            KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
            if (DC_FLAGS_SERVICE_STATE_CHANGED & DriverContext.ulFlags) {
                bServiceStateChanged = TRUE;
                DriverContext.ulFlags &= ~DC_FLAGS_SERVICE_STATE_CHANGED;
            } else {
                bServiceStateChanged = FALSE;
            }

            pEntry = DriverContext.HeadForDevContext.Flink;
            while (pEntry != &DriverContext.HeadForDevContext)
            {
                pDevContext = (PDEV_CONTEXT)pEntry;
                pEntry = pEntry->Flink;

                KeAcquireSpinLock(&pDevContext->Lock, &DeviceContextIrql);
                ProcessDevStateChange(pDevContext, &WorkQueueEntriesListHead, bServiceStateChanged);
                KeReleaseSpinLock(&pDevContext->Lock, DeviceContextIrql);

            } // while                  

            KeResetEvent(&DriverContext.ServiceThreadWakeEvent);
            KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

            while (!IsListEmpty(&WorkQueueEntriesListHead))
            {
                // go through all Work Items and fire the events.
                pWorkItem = (PWORK_QUEUE_ENTRY)RemoveHeadList(&WorkQueueEntriesListHead);
                ProcessDevContextWorkItems(pWorkItem);
                DereferenceWorkQueueEntry(pWorkItem);
            }
        } // (STATUS_WAIT_0 == Status)

    // This condition would never be true. When it is true anyway we break out of the loop.
    // This is used just to avoid compilation warning in strict error checking.
    }while(FALSE == bShutdownThread);

    return;
}

NTSTATUS
DeviceIoControlSetDirtyBlockNotifyEvent(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status;
    PDEV_CONTEXT     DevContext = NULL;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    KIRQL               OldIrql;

    ASSERT(IOCTL_INMAGE_SET_DIRTY_BLOCK_NOTIFY_EVENT == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

#ifdef _WIN64
    if (DriverContext.bS2is64BitProcess) {
        if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < sizeof(VOLUME_DB_EVENT_INFO)) {
            InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL, 
                ("DeviceIoControlSetDirtyBlockNotifyEvent: IOCTL rejected, input buffer too small. Expected(%#x), received(%#x)\n",
                 sizeof(VOLUME_DB_EVENT_INFO), IoStackLocation->Parameters.DeviceIoControl.InputBufferLength));
            Status = STATUS_BUFFER_TOO_SMALL;
            Irp->IoStatus.Status = Status;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return Status;
        }
    } else {
#endif // _WIN64
        if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < sizeof(VOLUME_DB_EVENT_INFO32)) {
            InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL, 
                ("DeviceIoControlSetDirtyBlockNotifyEvent: IOCTL rejected, input buffer too small. Expected(%#x), received(%#x)\n",
                 sizeof(VOLUME_DB_EVENT_INFO), IoStackLocation->Parameters.DeviceIoControl.InputBufferLength));
            Status = STATUS_BUFFER_TOO_SMALL;
            Irp->IoStatus.Status = Status;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return Status;
        }
#ifdef _WIN64
    }
#endif // _WIN64

    DevContext = GetDevContextForThisIOCTL(DeviceObject, Irp);
    if (DevContext) {
        PRKEVENT   DBNotifyEvent = NULL;
        HANDLE     hEvent;

#ifdef _WIN64
        if (DriverContext.bS2is64BitProcess) {
            PDEV_DB_EVENT_INFO   DBEventInfo = (PDEV_DB_EVENT_INFO)Irp->AssociatedIrp.SystemBuffer;
            hEvent = DBEventInfo->hEvent;
        } else {
            PDEV_DB_EVENT_INFO32   DBEventInfo = (PDEV_DB_EVENT_INFO32)Irp->AssociatedIrp.SystemBuffer;
            ULONGLONG ullEvent = DBEventInfo->hEvent;
            hEvent = (HANDLE)ullEvent;
        }
#else
        PDEV_DB_EVENT_INFO   DBEventInfo = (PDEV_DB_EVENT_INFO)Irp->AssociatedIrp.SystemBuffer;
        hEvent = DBEventInfo->hEvent;
#endif // _WIN64

        if (hEvent) {
            Status = ObReferenceObjectByHandle(hEvent, SYNCHRONIZE, *ExEventObjectType, Irp->RequestorMode, (PVOID *)&DBNotifyEvent, NULL);
        } else {
            // User has sent a NULL handle. This is an indication that he wants to release the previous event.
            InVolDbgPrint(DL_WARNING, DM_INMAGE_IOCTL, ("DeviceIoControlSetDirtyBlockNotifyEvent: hEvent is NULL in input buffer\n"));
            Status = STATUS_SUCCESS;
        }

        if (NT_SUCCESS(Status)) {
            KeAcquireSpinLock(&DevContext->Lock, &OldIrql);
            if (DevContext->DBNotifyEvent) {
                InVolDbgPrint(DL_WARNING, DM_INMAGE_IOCTL, ("DBNotifyEvent is already set in Volume context\n"));
                ObDereferenceObject(DevContext->DBNotifyEvent);
                DevContext->DBNotifyEvent = NULL;
            }
            DevContext->DBNotifyEvent = DBNotifyEvent;
            DevContext->liTickCountAtNotifyEventSet.QuadPart = 0;
            KeReleaseSpinLock(&DevContext->Lock, OldIrql);
            Status = STATUS_SUCCESS;
        }
        DereferenceDevContext(DevContext);
    } else {
		Status = STATUS_NO_SUCH_DEVICE;
    }

    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

#ifdef _CUSTOM_COMMAND_CODE_
NTSTATUS
DeviceIoControlCustomCommand(
    PDEVICE_OBJECT  DeviceObject,
    PIRP            Irp
    )
{
    PIO_STACK_LOCATION      IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS                Status = STATUS_SUCCESS;
    PDRIVER_CUSTOM_COMMAND  Input;

	UNREFERENCED_PARAMETER(DeviceObject);

    ASSERT(IOCTL_INMAGE_CUSTOM_COMMAND == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

    if( IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < sizeof(DRIVER_CUSTOM_COMMAND) ) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL, 
            ("DeviceIoControlCustomCommand: IOCTL rejected, input buffer too small\n"));
        Status = STATUS_BUFFER_TOO_SMALL;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    Input = (PDRIVER_CUSTOM_COMMAND)Irp->AssociatedIrp.SystemBuffer;

    if (Input->ulParam2) {
        // Free the memory
        ULONG ulFree = 0;
        while ((!IsListEmpty(&DriverContext.MemoryAllocatedList)) && (ulFree < Input->ulParam1)) {
            PLIST_NODE  ListNode = (PLIST_NODE)RemoveHeadList(&DriverContext.MemoryAllocatedList);
            if (NULL != ListNode->pData) {
                ExFreePoolWithTag(ListNode->pData, TAG_NONPAGED_ALLOCATED);
                ListNode->pData = NULL;
            }

            DereferenceListNode(ListNode);
            ulFree += 4;
        }
    } else {
        // Allocate memory
        ULONG ulAllocated = 0;
        while (ulAllocated < Input->ulParam1) {
            PLIST_NODE  ListNode = AllocateListNode();
            if (NULL == ListNode) {
                Status = STATUS_NO_MEMORY;
                break;
            }

            ListNode->pData = ExAllocatePoolWithTag(NonPagedPool, FOUR_K_SIZE, TAG_NONPAGED_ALLOCATED);

            if (NULL == ListNode->pData) {
                DereferenceListNode(ListNode);
                Status = STATUS_NO_MEMORY;
                break;
            }

            InsertTailList(&DriverContext.MemoryAllocatedList, &ListNode->ListEntry);
            ulAllocated += 4;
        }
    }

    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}
#endif // _CUSTOM_COMMAND_CODE_

NTSTATUS
DeviceIoControlSetDriverFlags(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status = STATUS_SUCCESS;
    KIRQL               OldIrql;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulInputFlags;
    etBitOperation      eInputBitOperation;
    PDRIVER_FLAGS_INPUT pDriverFlagsInput = NULL;

	UNREFERENCED_PARAMETER(DeviceObject);

    ASSERT(IOCTL_INMAGE_SET_DRIVER_FLAGS == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);
    Irp->IoStatus.Information = 0;

    if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < sizeof(DRIVER_FLAGS_INPUT)) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL, ("%s: IOCTL rejected, input buffer too small\n", __FUNCTION__));
        Status = STATUS_BUFFER_TOO_SMALL;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }


    // Save input flags and operation to perform in local variables. Systembuffer
    // will be used to save the output information.
    pDriverFlagsInput = (PDRIVER_FLAGS_INPUT)Irp->AssociatedIrp.SystemBuffer;
    eInputBitOperation = pDriverFlagsInput->eOperation;
    ulInputFlags = pDriverFlagsInput->ulFlags;
    // Check if Flag and bitop set in input buffer are all valid flags.
    if ((0 != (ulInputFlags & (~DRIVER_FLAGS_VALID))) ||
        ((ecBitOpReset != eInputBitOperation) && (ecBitOpSet != eInputBitOperation)) ) 
    {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL,
            ("%s: IOCTL rejected, invalid parameters\n", __FUNCTION__));
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    if (ecBitOpSet == eInputBitOperation) {
        if (ulInputFlags & DRIVER_FLAG_DISABLE_DATA_FILES) {
            if (DriverContext.bEnableDataFiles) {
                DriverContext.bEnableDataFiles = false;
            } else {
                InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL, ("%s: Already data file support is disabled\n", __FUNCTION__));
                ulInputFlags &= ~DRIVER_FLAG_DISABLE_DATA_FILES;                
            }
        }

        if (ulInputFlags & DRIVER_FLAG_DISABLE_DATA_FILES_FOR_NEW_VOLUMES) {
            if (DriverContext.bEnableDataFilesForNewDevs) {
                DriverContext.bEnableDataFilesForNewDevs = false;
            } else {
                InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL, ("%s: Already data file support for new volumes is disabled\n", __FUNCTION__));
                ulInputFlags &= ~DRIVER_FLAG_DISABLE_DATA_FILES_FOR_NEW_VOLUMES;                
            }
        }        

    } else {
        if (ulInputFlags & DRIVER_FLAG_DISABLE_DATA_FILES) {
            if (!DriverContext.bEnableDataFiles) {
                DriverContext.bEnableDataFiles = true;
            } else {
                InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL, ("%s: Already data file support is enabled\n", __FUNCTION__));
                ulInputFlags &= ~DRIVER_FLAG_DISABLE_DATA_FILES;                
            }
        }

        if (ulInputFlags & DRIVER_FLAG_DISABLE_DATA_FILES_FOR_NEW_VOLUMES) {
            if (!DriverContext.bEnableDataFilesForNewDevs) {
                DriverContext.bEnableDataFilesForNewDevs = true;
            } else {
                InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL, ("%s: Already data file support for new volumes is enabled\n", __FUNCTION__));
                ulInputFlags &= ~DRIVER_FLAG_DISABLE_DATA_FILES_FOR_NEW_VOLUMES;                
            }
        }
    }

    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength >= sizeof(DRIVER_FLAGS_OUTPUT)) {
        PDRIVER_FLAGS_OUTPUT    pDriverFlagsOutput = (PDRIVER_FLAGS_OUTPUT)Irp->AssociatedIrp.SystemBuffer;

        pDriverFlagsOutput->ulFlags = 0;

        if (!DriverContext.bEnableDataFiles) {
            pDriverFlagsOutput->ulFlags |= DRIVER_FLAG_DISABLE_DATA_FILES;
        }

        if (!DriverContext.bEnableDataFilesForNewDevs) {
            pDriverFlagsOutput->ulFlags |= DRIVER_FLAG_DISABLE_DATA_FILES_FOR_NEW_VOLUMES;
        }

        Irp->IoStatus.Information = sizeof(DRIVER_FLAGS_OUTPUT);
    }

    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

    // Do any passive level operation that have to be performed.
    if (ecBitOpSet == eInputBitOperation) {
        if (ulInputFlags & DRIVER_FLAG_DISABLE_DATA_FILES)
            SetDWORDInParameters(DEVICE_DATA_FILES, DEVICE_DATA_FILES_UNSET);
        if (ulInputFlags & DRIVER_FLAG_DISABLE_DATA_FILES_FOR_NEW_VOLUMES)
            SetDWORDInParameters(DATA_FILES_FOR_NEW_DEVICES, DEVICE_DATA_FILES_UNSET);
    } else {
        if (ulInputFlags & DRIVER_FLAG_DISABLE_DATA_FILES)
            SetDWORDInParameters(DEVICE_DATA_FILES, DEVICE_DATA_FILES_SET);
        if (ulInputFlags & DRIVER_FLAG_DISABLE_DATA_FILES_FOR_NEW_VOLUMES)
            SetDWORDInParameters(DATA_FILES_FOR_NEW_DEVICES, DEVICE_DATA_FILES_SET);
    }

    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS
DeviceIoControlSetDriverConfig(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status = STATUS_SUCCESS;
    KIRQL               OldIrql;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    PDRIVER_CONFIG      pDriverInput = NULL;
    PDRIVER_CONFIG      pDriverConfig = (PDRIVER_CONFIG)Irp->AssociatedIrp.SystemBuffer;
    bool ValidOutBuff = 0;
    // OutPutFlag should be set for the changed tunables, which were not requested 
    ULONG OutPutFlag = 0;
    ULONG ulError = 0;

    UNREFERENCED_PARAMETER(DeviceObject);

    ASSERT(IOCTL_INMAGE_SET_DRIVER_CONFIGURATION == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);
    Irp->IoStatus.Information = 0;

    if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < sizeof(DRIVER_CONFIG)) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL, ("%s: IOCTL rejected, input buffer too small\n", __FUNCTION__));
        Status = STATUS_BUFFER_TOO_SMALL;
        goto Cleanup;
    }

    pDriverInput = (DRIVER_CONFIG*)ExAllocatePoolWithTag(NonPagedPool, sizeof(DRIVER_CONFIG), TAG_GENERIC_NON_PAGED);
    if (NULL == pDriverInput) {
        PCWCHAR PoolTypeW = PoolTypeToStringW(NonPagedPool);
        InDskFltWriteEvent(INDSKFLT_ERROR_INSUFFICIENT_RESOURCES, PoolTypeW, sizeof(DRIVER_CONFIG), SourceLocationSetDriverConfigInsufficientMemory);
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Cleanup;
    }

    pDriverInput->ulFlags1 = pDriverConfig->ulFlags1;
    pDriverInput->ulhwmServiceNotStarted = pDriverConfig->ulhwmServiceNotStarted;
    pDriverInput->ulhwmServiceRunning = pDriverConfig->ulhwmServiceRunning;
    pDriverInput->ulhwmServiceShutdown = pDriverConfig->ulhwmServiceShutdown;
    pDriverInput->ullwmServiceRunning = pDriverConfig->ullwmServiceRunning;
    pDriverInput->ulChangesToPergeOnhwm = pDriverConfig->ulChangesToPergeOnhwm;
    pDriverInput->ulSecsBetweenMemAllocFailureEvents = pDriverConfig->ulSecsBetweenMemAllocFailureEvents;
    pDriverInput->ulSecsMaxWaitTimeOnFailOver =  pDriverConfig->ulSecsMaxWaitTimeOnFailOver;
    pDriverInput->ulMaxLockedDataBlocks = pDriverConfig->ulMaxLockedDataBlocks;
    pDriverInput->ulMaxNonPagedPoolInMB = pDriverConfig->ulMaxNonPagedPoolInMB;
    pDriverInput->ulMaxWaitTimeForIrpCompletionInMinutes = pDriverConfig->ulMaxWaitTimeForIrpCompletionInMinutes;
    pDriverInput->bDisableVolumeFilteringForNewClusterVolumes = pDriverConfig->bDisableVolumeFilteringForNewClusterVolumes;
    pDriverInput->bDisableFilteringForNewDevs = pDriverConfig->bDisableFilteringForNewDevs;
    pDriverInput->bEnableDataFilteringForNewDevs = pDriverConfig->bEnableDataFilteringForNewDevs;
    pDriverInput->bEnableDataCapture = pDriverConfig->bEnableDataCapture;    
    pDriverInput->ulAvailableDataBlockCountInPercentage = pDriverConfig->ulAvailableDataBlockCountInPercentage;
    pDriverInput->ulMaxDataAllocLimitInPercentage = pDriverConfig->ulMaxDataAllocLimitInPercentage;
    pDriverInput->ulMaxCoalescedMetaDataChangeSize = pDriverConfig->ulMaxCoalescedMetaDataChangeSize;
    pDriverInput->ulValidationLevel = pDriverConfig->ulValidationLevel;
    pDriverInput->ulDataPoolSize = pDriverConfig->ulDataPoolSize;
    pDriverInput->ulMaxDataSizePerDev = pDriverConfig->ulMaxDataSizePerDev;
    pDriverInput->ulDaBThresholdPerDevForFileWrite = pDriverConfig->ulDaBThresholdPerDevForFileWrite;
    pDriverInput->ulDaBFreeThresholdForFileWrite = pDriverConfig->ulDaBFreeThresholdForFileWrite;
    pDriverInput->ulMaxDataSizePerNonDataModeDirtyBlock = pDriverConfig->ulMaxDataSizePerNonDataModeDirtyBlock;
    pDriverInput->ulMaxDataSizePerDataModeDirtyBlock = pDriverConfig->ulMaxDataSizePerDataModeDirtyBlock;
    pDriverInput->DataFile.usLength = pDriverConfig->DataFile.usLength;
    if (pDriverInput->DataFile.usLength > 0) {
        RtlMoveMemory(pDriverInput->DataFile.FileName, pDriverConfig->DataFile.FileName,
                      pDriverInput->DataFile.usLength);
        pDriverInput->DataFile.FileName[pDriverInput->DataFile.usLength/sizeof(WCHAR)] = L'\0';
    }
    pDriverInput->bEnableFSFlushPreShutdown = pDriverConfig->bEnableFSFlushPreShutdown;

    // Check if Flag and bitop set in input buffer are all valid flags.
    if (0 != (pDriverInput->ulFlags1 & (~DRIVER_CONFIG_FLAGS1_VALID))) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL,
            ("%s: IOCTL rejected, invalid parameters\n", __FUNCTION__));
        Status = STATUS_INVALID_PARAMETER;
        goto Cleanup;
    }

    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    if (pDriverInput->ulFlags1 & DRIVER_CONFIG_FLAGS_HWM_SERVICE_NOT_STARTED) {
        DriverContext.DBHighWaterMarks[ecServiceNotStarted] = pDriverInput->ulhwmServiceNotStarted;
    }

    if (pDriverInput->ulFlags1 & DRIVER_CONFIG_FLAGS_HWM_SERVICE_RUNNING) {
        DriverContext.DBHighWaterMarks[ecServiceRunning] = pDriverInput->ulhwmServiceRunning;
    }

    if (pDriverInput->ulFlags1 & DRIVER_CONFIG_FLAGS_HWM_SERVICE_SHUTDOWN) {
        DriverContext.DBHighWaterMarks[ecServiceShutdown] = pDriverInput->ulhwmServiceShutdown;
    }

    if (pDriverInput->ulFlags1 & DRIVER_CONFIG_FLAGS_LWM_SERVICE_RUNNING) {
        DriverContext.DBLowWaterMarkWhileServiceRunning = pDriverInput->ullwmServiceRunning;
    }

    if (pDriverInput->ulFlags1 & DRIVER_CONFIG_FLAGS_CHANGES_TO_PURGE_ON_HWM) {
        DriverContext.DBToPurgeWhenHighWaterMarkIsReached = pDriverInput->ulChangesToPergeOnhwm;
    }

    if (pDriverInput->ulFlags1 & DRIVER_CONFIG_FLAGS_TIME_BETWEEN_MEM_FAIL_LOG) {
        DriverContext.ulSecondsBetweenNoMemoryLogEvents = pDriverInput->ulSecsBetweenMemAllocFailureEvents;
    }

    if (pDriverInput->ulFlags1 & DRIVER_CONFIG_FLAGS_MAX_WAIT_TIME_ON_FAILOVER) {
        DriverContext.ulSecondsMaxWaitTimeOnFailOver = pDriverInput->ulSecsMaxWaitTimeOnFailOver;
    }

    if (pDriverInput->ulFlags1 & DRIVER_CONFIG_FLAGS_MAX_LOCKED_DATA_BLOCKS) {
        if (ValidateMaxSetLockedDataBlocks(pDriverInput->ulMaxLockedDataBlocks) == pDriverInput->ulMaxLockedDataBlocks) {
            DriverContext.ulMaxSetLockedDataBlocks = pDriverInput->ulMaxLockedDataBlocks;
            SetMaxLockedDataBlocks();
        } else {
            ulError = ulError | DRIVER_CONFIG_FLAGS_MAX_LOCKED_DATA_BLOCKS;
        }
    }

    if (pDriverInput->ulFlags1 & DRIVER_CONFIG_FLAGS_MAX_NON_PAGED_POOL) {
        DriverContext.ulMaxNonPagedPool = pDriverInput->ulMaxNonPagedPoolInMB * MEGABYTES;
    }

    if (pDriverInput->ulFlags1 & DRIVER_CONFIG_FLAGS_MAX_WAIT_TIME_FOR_IRP_COMPLETION) {
        DriverContext.ulMaxWaitTimeForIrpCompletion = pDriverInput->ulMaxWaitTimeForIrpCompletionInMinutes * SECONDS_IN_MINUTE;
    }
#ifdef VOLUME_CLUSTER_SUPPORT
    if (pDriverInput->ulFlags1 & DRIVER_CONFIG_FLAGS_DEFAULT_FILTERING_FOR_NEW_CLUSTER_VOLUMES) {
        DriverContext.bDisableVolumeFilteringForNewClusterVolumes = pDriverInput->bDisableVolumeFilteringForNewClusterVolumes;
    }
#endif
    if (pDriverInput->ulFlags1 & DRIVER_CONFIG_FLAGS_FITERING_DISABLED_FOR_NEW_VOLUMES) {
        DriverContext.bDisableFilteringForNewDevice =  pDriverInput->bDisableFilteringForNewDevs;
    }

    if (pDriverInput->ulFlags1 & DRIVER_CONFIG_FLAGS_DATA_FILTER_FOR_NEW_VOLUMES) {
        DriverContext.bEnableDataModeCaptureForNewDevices = pDriverInput->bEnableDataFilteringForNewDevs;
    }

    if (pDriverInput->ulFlags1 & DRIVER_CONFIG_FLAGS_AVAILABLE_DATA_BLOCK_COUNT) {
        DriverContext.ulAvailableDataBlockCountInPercentage = pDriverInput->ulAvailableDataBlockCountInPercentage;
    }

    if (pDriverInput->ulFlags1 & DRIVER_CONFIG_FLAGS_MAX_DATA_ALLOCATION_LIMIT) {
        DriverContext.ulMaxDataAllocLimitInPercentage = pDriverInput->ulMaxDataAllocLimitInPercentage;
    }

    if (pDriverInput->ulFlags1 & DRIVER_CONFIG_FLAGS_MAX_COALESCED_METADATA_CHANGE_SIZE) {
        DriverContext.MaxCoalescedMetaDataChangeSize = pDriverInput->ulMaxCoalescedMetaDataChangeSize;
    }

    if (pDriverInput->ulFlags1 & DRIVER_CONFIG_FLAGS_VALIDATION_LEVEL) {
        DriverContext.ulValidationLevel = pDriverInput->ulValidationLevel;
    }

    if (pDriverInput->ulFlags1 & DRIVER_CONFIG_FLAGS_VOL_THRESHOLD_FOR_FILE_WRITE) {
        ULONG ulDaBThresholdPerDevForFileWrite =  ValidateThresholdPerDevForFileWrite(pDriverInput->ulDaBThresholdPerDevForFileWrite);
    if (ulDaBThresholdPerDevForFileWrite != pDriverInput->ulDaBThresholdPerDevForFileWrite) {
            ulError = ulError | DRIVER_CONFIG_FLAGS_VOL_THRESHOLD_FOR_FILE_WRITE;
        } else {
            DriverContext.ulDaBThresholdPerDevForFileWrite = pDriverInput->ulDaBThresholdPerDevForFileWrite;
        }
    }

    if (pDriverInput->ulFlags1 & DRIVER_CONFIG_FLAGS_FREE_THRESHOLD_FOR_FILE_WRITE) {
        ULONG ulDaBFreeThresholdForFileWrite = ValidateFreeThresholdForFileWrite(pDriverInput->ulDaBFreeThresholdForFileWrite);
        if (ulDaBFreeThresholdForFileWrite != pDriverInput->ulDaBFreeThresholdForFileWrite) {
            ulError = ulError | DRIVER_CONFIG_FLAGS_FREE_THRESHOLD_FOR_FILE_WRITE;
        } else {
            DriverContext.ulDaBFreeThresholdForFileWrite = pDriverInput->ulDaBFreeThresholdForFileWrite;
        }
    }

    if (pDriverInput->ulFlags1 & DRIVER_CONFIG_DATA_POOL_SIZE) {
        // Due to limitation with ULONG types
        if (pDriverInput->ulDataPoolSize < DEFAULT_DATA_POOL_SIZE)
            pDriverInput->ulDataPoolSize = DEFAULT_DATA_POOL_SIZE;

        if (pDriverInput->ulDataPoolSize > MAX_ALLOWED_DATA_POOL_IN_MB)
            pDriverInput->ulDataPoolSize = MAX_ALLOWED_DATA_POOL_IN_MB;

        ULONGLONG ullDataPoolSize = ((ULONGLONG)pDriverInput->ulDataPoolSize) * MEGABYTES;
        /* DataPoolSize shd be greater than MaxSetLockedDataBlocks */
        if ((ullDataPoolSize != ValidateDataPoolSize(ullDataPoolSize)) || (pDriverInput->ulDataPoolSize < DriverContext.ulMaxSetLockedDataBlocks)) {
            ulError = ulError | DRIVER_CONFIG_DATA_POOL_SIZE;
            Status = STATUS_INVALID_PARAMETER_1;
            // TODO : Add event log in the new manifest file
        }
    }

    if (pDriverInput->ulFlags1 & DRIVER_CONFIG_MAX_DATA_POOL_SIZE_PER_VOLUME) {
        ULONG ulMaxDataSizePerDev = pDriverInput->ulMaxDataSizePerDev * KILOBYTES;
        DriverContext.ulMaxDataSizePerDev = ValidateMaxDataSizePerDev(ulMaxDataSizePerDev);
    }
    
    if (pDriverInput->ulFlags1 & DRIVER_CONFIG_FLAGS_MAX_DATA_PER_NON_DATA_MODE_DIRTY_BLOCK) {
        ULONG ulMaxDataSizePerNonDataModeDirtyBlock = pDriverInput->ulMaxDataSizePerNonDataModeDirtyBlock * MEGABYTES;
        if (ValidateMaxDataSizePerNonDataModeDirtyBlock(ulMaxDataSizePerNonDataModeDirtyBlock ) == ulMaxDataSizePerNonDataModeDirtyBlock ) {
            DriverContext.ulMaxDataSizePerNonDataModeDirtyBlock = ulMaxDataSizePerNonDataModeDirtyBlock;
        } else {
            ulError = ulError | DRIVER_CONFIG_FLAGS_MAX_DATA_PER_NON_DATA_MODE_DIRTY_BLOCK;
        }
    }

    if (pDriverInput->ulFlags1 & DRIVER_CONFIG_FLAGS_MAX_DATA_PER_DATA_MODE_DIRTY_BLOCK) {
        ULONG ulMaxDataSizePerDataModeDirtyBlock = pDriverInput->ulMaxDataSizePerDataModeDirtyBlock * MEGABYTES;
        if (ValidateMaxDataSizePerDataModeDirtyBlock(ulMaxDataSizePerDataModeDirtyBlock ) == ulMaxDataSizePerDataModeDirtyBlock ) {
            DriverContext.ulMaxDataSizePerDataModeDirtyBlock = ulMaxDataSizePerDataModeDirtyBlock;
            ULONG ulMaxDataSizePerNonDataModeDirtyBlock = ValidateMaxDataSizePerNonDataModeDirtyBlock(DriverContext.ulMaxDataSizePerNonDataModeDirtyBlock);
            if (ulMaxDataSizePerNonDataModeDirtyBlock != DriverContext.ulMaxDataSizePerNonDataModeDirtyBlock) {
                OutPutFlag |= DRIVER_CONFIG_FLAGS_MAX_DATA_PER_NON_DATA_MODE_DIRTY_BLOCK;                
                DriverContext.ulMaxDataSizePerNonDataModeDirtyBlock = ulMaxDataSizePerNonDataModeDirtyBlock;
            }
            ULONG ulMaxDataSizePerDev = ULONG((DriverContext.ullDataPoolSize / 100) * MAX_DATA_SIZE_PER_VOL_PERC);
            ulMaxDataSizePerDev = ValidateMaxDataSizePerDev(ulMaxDataSizePerDev);
            if (ulMaxDataSizePerDev != DriverContext.ulMaxDataSizePerDev) {
                OutPutFlag |= DRIVER_CONFIG_MAX_DATA_POOL_SIZE_PER_VOLUME;                
                DriverContext.ulMaxDataSizePerDev = ulMaxDataSizePerDev;                
            }
        } else {
            ulError = ulError | DRIVER_CONFIG_FLAGS_MAX_DATA_PER_DATA_MODE_DIRTY_BLOCK;
        }
    }

    if (pDriverInput->ulFlags1 & DRIVER_CONFIG_FLAGS_DATA_LOG_DIRECTORY) {
        if (pDriverInput->DataFile.usLength > 0) {
            NTSTATUS StatusUni = InMageCopyUnicodeString(&DriverContext.DataLogDirectory, pDriverInput->DataFile.FileName, NonPagedPool);
            if (!NT_SUCCESS(StatusUni)) {
                ulError = ulError | DRIVER_CONFIG_FLAGS_DATA_LOG_DIRECTORY;
            }
        } else {
            ulError = ulError | DRIVER_CONFIG_FLAGS_DATA_LOG_DIRECTORY;
        }
    }

    if (TEST_FLAG(pDriverInput->ulFlags1, DRIVER_CONFIG_FLAGS_ENABLE_FS_FLUSH)) {
        DriverContext.bFlushFsPreShutdown = pDriverInput->bEnableFSFlushPreShutdown;
    }

    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength >= sizeof(DRIVER_CONFIG)) {
        if (pDriverInput->ulFlags1 == 0) {
            pDriverConfig->ulFlags1 =   DRIVER_CONFIG_FLAGS_HWM_SERVICE_NOT_STARTED |
                                    DRIVER_CONFIG_FLAGS_HWM_SERVICE_RUNNING     |
                                    DRIVER_CONFIG_FLAGS_HWM_SERVICE_SHUTDOWN    |
                                    DRIVER_CONFIG_FLAGS_LWM_SERVICE_RUNNING     |
                                    DRIVER_CONFIG_FLAGS_CHANGES_TO_PURGE_ON_HWM |
                                    DRIVER_CONFIG_FLAGS_TIME_BETWEEN_MEM_FAIL_LOG |
                                    DRIVER_CONFIG_FLAGS_MAX_WAIT_TIME_ON_FAILOVER |
                                    DRIVER_CONFIG_FLAGS_MAX_LOCKED_DATA_BLOCKS |
                                    DRIVER_CONFIG_FLAGS_MAX_NON_PAGED_POOL    |
                                    DRIVER_CONFIG_FLAGS_MAX_WAIT_TIME_FOR_IRP_COMPLETION |
                                    DRIVER_CONFIG_FLAGS_DEFAULT_FILTERING_FOR_NEW_CLUSTER_VOLUMES |
                                    DRIVER_CONFIG_FLAGS_DATA_FILTER_FOR_NEW_VOLUMES |
                                    DRIVER_CONFIG_FLAGS_FITERING_DISABLED_FOR_NEW_VOLUMES |
                                    DRIVER_CONFIG_FLAGS_ENABLE_DATA_CAPTURE |
                                    DRIVER_CONFIG_FLAGS_MAX_DATA_ALLOCATION_LIMIT |
                                    DRIVER_CONFIG_FLAGS_AVAILABLE_DATA_BLOCK_COUNT |
                                    DRIVER_CONFIG_FLAGS_MAX_COALESCED_METADATA_CHANGE_SIZE |
                                    DRIVER_CONFIG_FLAGS_VALIDATION_LEVEL |
                                    DRIVER_CONFIG_DATA_POOL_SIZE |
                                    DRIVER_CONFIG_MAX_DATA_POOL_SIZE_PER_VOLUME |
                                    DRIVER_CONFIG_FLAGS_VOL_THRESHOLD_FOR_FILE_WRITE |
                                    DRIVER_CONFIG_FLAGS_FREE_THRESHOLD_FOR_FILE_WRITE |
                                    DRIVER_CONFIG_FLAGS_MAX_DATA_PER_NON_DATA_MODE_DIRTY_BLOCK |
                                    DRIVER_CONFIG_FLAGS_MAX_DATA_PER_DATA_MODE_DIRTY_BLOCK |
                                    DRIVER_CONFIG_FLAGS_DATA_LOG_DIRECTORY |
                                    DRIVER_CONFIG_FLAGS_ENABLE_FS_FLUSH;
        } else {
            pDriverConfig->ulFlags1 = pDriverInput->ulFlags1;
        }

        pDriverConfig->ulhwmServiceNotStarted = DriverContext.DBHighWaterMarks[ecServiceNotStarted];
        pDriverConfig->ulhwmServiceRunning = DriverContext.DBHighWaterMarks[ecServiceRunning];
        pDriverConfig->ulhwmServiceShutdown = DriverContext.DBHighWaterMarks[ecServiceShutdown];
        pDriverConfig->ullwmServiceRunning = DriverContext.DBLowWaterMarkWhileServiceRunning;
        pDriverConfig->ulChangesToPergeOnhwm = DriverContext.DBToPurgeWhenHighWaterMarkIsReached;
        pDriverConfig->ulSecsBetweenMemAllocFailureEvents = DriverContext.ulSecondsBetweenNoMemoryLogEvents;
        pDriverConfig->ulSecsMaxWaitTimeOnFailOver = DriverContext.ulSecondsMaxWaitTimeOnFailOver;
        pDriverConfig->ulMaxLockedDataBlocks = DriverContext.ulMaxLockedDataBlocks;
        pDriverConfig->ulMaxNonPagedPoolInMB = DriverContext.ulMaxNonPagedPool / MEGABYTES;
        pDriverConfig->ulMaxWaitTimeForIrpCompletionInMinutes = DriverContext.ulMaxWaitTimeForIrpCompletion / SECONDS_IN_MINUTE;
#ifdef VOLUME_CLUSTER_SUPPORT
        pDriverConfig->bDisableVolumeFilteringForNewClusterVolumes = DriverContext.bDisableVolumeFilteringForNewClusterVolumes;
#endif
        pDriverConfig->bDisableFilteringForNewDevs = DriverContext.bDisableFilteringForNewDevice;
        pDriverConfig->bEnableDataCapture = DriverContext.bEnableDataCapture;
        if (DriverContext.bEnableDataModeCaptureForNewDevices) {
            pDriverConfig->bEnableDataFilteringForNewDevs = 1;
        } else {
            pDriverConfig->bEnableDataFilteringForNewDevs = 0;
        }
        pDriverConfig->ulAvailableDataBlockCountInPercentage = DriverContext.ulAvailableDataBlockCountInPercentage;
        pDriverConfig->ulMaxDataAllocLimitInPercentage = DriverContext.ulMaxDataAllocLimitInPercentage;
        pDriverConfig->ulMaxCoalescedMetaDataChangeSize = DriverContext.MaxCoalescedMetaDataChangeSize;
        pDriverConfig->ulValidationLevel = DriverContext.ulValidationLevel;
        pDriverConfig->ulDataPoolSize = (ULONG)(DriverContext.ullDataPoolSize / MEGABYTES);
        pDriverConfig->ulMaxDataSizePerDev = (ULONG)((DriverContext.ulMaxDataSizePerDev) / KILOBYTES);
        pDriverConfig->ulDaBThresholdPerDevForFileWrite = DriverContext.ulDaBThresholdPerDevForFileWrite;
        pDriverConfig->ulDaBFreeThresholdForFileWrite = DriverContext.ulDaBFreeThresholdForFileWrite;
        pDriverConfig->ulMaxDataSizePerNonDataModeDirtyBlock = DriverContext.ulMaxDataSizePerNonDataModeDirtyBlock / MEGABYTES;
        pDriverConfig->ulMaxDataSizePerDataModeDirtyBlock = DriverContext.ulMaxDataSizePerDataModeDirtyBlock / MEGABYTES;
        if (pDriverConfig->ulFlags1 & DRIVER_CONFIG_FLAGS_DATA_LOG_DIRECTORY) {            
            pDriverConfig->DataFile.usLength = DriverContext.DataLogDirectory.Length;
            if ((pDriverConfig->DataFile.usLength > 0) && (pDriverConfig->DataFile.usLength < UDIRTY_BLOCK_MAX_DATA_LOG_DIRECTORY)) {
                RtlMoveMemory(pDriverConfig->DataFile.FileName, DriverContext.DataLogDirectory.Buffer,
                        pDriverConfig->DataFile.usLength);
                pDriverConfig->DataFile.FileName[pDriverConfig->DataFile.usLength/sizeof(WCHAR)] = L'\0';
            }            
        }            
        pDriverConfig->bEnableFSFlushPreShutdown = DriverContext.bFlushFsPreShutdown;
        Irp->IoStatus.Information = sizeof(DRIVER_CONFIG);
        ValidOutBuff = 1;
    }

    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

    OutPutFlag |=  pDriverInput->ulFlags1;

    // Do any passive level operation that have to be performed.
    if ((OutPutFlag & DRIVER_CONFIG_FLAGS_HWM_SERVICE_NOT_STARTED) && 
            (!(ulError & DRIVER_CONFIG_FLAGS_HWM_SERVICE_NOT_STARTED))) {
        SetDWORDInParameters(DB_HIGH_WATERMARK_SERVICE_NOT_STARTED, DriverContext.DBHighWaterMarks[ecServiceNotStarted]);
    }

    if ((OutPutFlag & DRIVER_CONFIG_FLAGS_HWM_SERVICE_RUNNING) && 
            (!(ulError & DRIVER_CONFIG_FLAGS_HWM_SERVICE_RUNNING))) {
        SetDWORDInParameters(DB_HIGH_WATERMARK_SERVICE_RUNNING, DriverContext.DBHighWaterMarks[ecServiceRunning]);
    }

    if ((OutPutFlag & DRIVER_CONFIG_FLAGS_HWM_SERVICE_SHUTDOWN) && 
            (!(ulError & DRIVER_CONFIG_FLAGS_HWM_SERVICE_SHUTDOWN))) {
        SetDWORDInParameters(DB_HIGH_WATERMARK_SERVICE_SHUTDOWN, DriverContext.DBHighWaterMarks[ecServiceShutdown]);
    }

    if ((OutPutFlag & DRIVER_CONFIG_FLAGS_LWM_SERVICE_RUNNING) && 
            (!(ulError & DRIVER_CONFIG_FLAGS_LWM_SERVICE_RUNNING))) {
        SetDWORDInParameters(DB_LOW_WATERMARK_SERVICE_RUNNING, DriverContext.DBLowWaterMarkWhileServiceRunning);
    }

    if ((OutPutFlag & DRIVER_CONFIG_FLAGS_CHANGES_TO_PURGE_ON_HWM) && 
            (!(ulError & DRIVER_CONFIG_FLAGS_CHANGES_TO_PURGE_ON_HWM))) {
        SetDWORDInParameters(DB_TO_PURGE_HIGH_WATERMARK_REACHED, DriverContext.DBToPurgeWhenHighWaterMarkIsReached);
    }

    if ((OutPutFlag & DRIVER_CONFIG_FLAGS_TIME_BETWEEN_MEM_FAIL_LOG) && 
            (!(ulError & DRIVER_CONFIG_FLAGS_TIME_BETWEEN_MEM_FAIL_LOG))) {
        SetDWORDInParameters(TIME_IN_SECONDS_BETWEEN_NO_MEMORY_LOG_EVENTS, DriverContext.ulSecondsBetweenNoMemoryLogEvents);
    }

    if ((OutPutFlag & DRIVER_CONFIG_FLAGS_MAX_WAIT_TIME_ON_FAILOVER) && 
            (!(ulError & DRIVER_CONFIG_FLAGS_MAX_WAIT_TIME_ON_FAILOVER))) {
        SetDWORDInParameters(MAX_WAIT_TIME_IN_SECONDS_ON_FAIL_OVER, DriverContext.ulSecondsMaxWaitTimeOnFailOver);
    }

    if ((OutPutFlag & DRIVER_CONFIG_FLAGS_MAX_LOCKED_DATA_BLOCKS) && 
            (!(ulError & DRIVER_CONFIG_FLAGS_MAX_LOCKED_DATA_BLOCKS))) {
        UpdateLockedDataBlockList(DriverContext.ulMaxLockedDataBlocks);
        SetDWORDInParameters(LOCKED_DATA_BLOCKS_FOR_USE_AT_DISPATCH_IRQL, DriverContext.ulMaxLockedDataBlocks);
    }

    if ((OutPutFlag & DRIVER_CONFIG_FLAGS_MAX_NON_PAGED_POOL) && 
            (!(ulError & DRIVER_CONFIG_FLAGS_MAX_NON_PAGED_POOL))) {
        SetDWORDInParameters(MAX_NON_PAGED_POOL_IN_MB, (DriverContext.ulMaxNonPagedPool / MEGABYTES));
    }

    if ((OutPutFlag & DRIVER_CONFIG_FLAGS_MAX_WAIT_TIME_FOR_IRP_COMPLETION) && 
            (!(ulError & DRIVER_CONFIG_FLAGS_MAX_WAIT_TIME_FOR_IRP_COMPLETION))) {
        SetDWORDInParameters(MAX_WAIT_TIME_FOR_IRP_COMPLETION_IN_MINUTES, (DriverContext.ulMaxWaitTimeForIrpCompletion / SECONDS_IN_MINUTE));
    }
#ifdef VOLUME_CLUSTER_SUPPORT
    if ((OutPutFlag & DRIVER_CONFIG_FLAGS_DEFAULT_FILTERING_FOR_NEW_CLUSTER_VOLUMES) && 
            (!(ulError & DRIVER_CONFIG_FLAGS_DEFAULT_FILTERING_FOR_NEW_CLUSTER_VOLUMES))) {
        SetDWORDInParameters(DISABLE_FILTERING_FOR_NEW_CLUSTER_DEVICES, DriverContext.bDisableVolumeFilteringForNewClusterVolumes ? 1 : 0);
    }
#endif
    if ((OutPutFlag & DRIVER_CONFIG_FLAGS_FITERING_DISABLED_FOR_NEW_VOLUMES) && 
            (!(ulError & DRIVER_CONFIG_FLAGS_FITERING_DISABLED_FOR_NEW_VOLUMES))) {
        SetDWORDInParameters(DISABLE_FILTERING_FOR_NEW_DEVICES, DriverContext.bDisableFilteringForNewDevice ? 1 : 0);
    }

    if ((OutPutFlag & DRIVER_CONFIG_FLAGS_DATA_FILTER_FOR_NEW_VOLUMES) && 
            (!(ulError & DRIVER_CONFIG_FLAGS_DATA_FILTER_FOR_NEW_VOLUMES))) {
        SetDWORDInParameters(DATAMODE_CAPTURE_FOR_NEW_DEVICES, DriverContext.bEnableDataModeCaptureForNewDevices ? 1 : 0);
    }

    if ((OutPutFlag & DRIVER_CONFIG_FLAGS_ENABLE_DATA_CAPTURE) && 
            (!(ulError & DRIVER_CONFIG_FLAGS_ENABLE_DATA_CAPTURE))) {
        /* Allocate or free Data Pool with respect to flag EnableDataCapture*/
        if (DriverContext.bEnableDataCapture !=  pDriverInput->bEnableDataCapture) {
            DriverContext.bEnableDataCapture = pDriverInput->bEnableDataCapture;
            if (DriverContext.bEnableDataCapture) {
                SetDataPool(1, DriverContext.ullDataPoolSize);
            } else {
                CleanDataPool();
            }
            SetDWORDInParameters(DEVICE_DATAMODE_CAPTURE, DriverContext.bEnableDataCapture ? 1 : 0);
        }
    }

    if ((OutPutFlag & DRIVER_CONFIG_FLAGS_AVAILABLE_DATA_BLOCK_COUNT) && 
            (!(ulError & DRIVER_CONFIG_FLAGS_AVAILABLE_DATA_BLOCK_COUNT))) {
        SetDWORDInParameters(FREE_AND_LOCKED_DATA_BLOCKS_COUNT_IN_PERCENTAGE, DriverContext.ulAvailableDataBlockCountInPercentage);
    }

    if ((OutPutFlag & DRIVER_CONFIG_FLAGS_MAX_DATA_ALLOCATION_LIMIT) && 
            (!(ulError & DRIVER_CONFIG_FLAGS_MAX_DATA_ALLOCATION_LIMIT))) {
        SetDWORDInParameters(DATA_ALLOCATION_LIMIT_IN_PERCENTAGE, DriverContext.ulMaxDataAllocLimitInPercentage);
    }

    if ((OutPutFlag & DRIVER_CONFIG_FLAGS_MAX_COALESCED_METADATA_CHANGE_SIZE) && 
            (!(ulError & DRIVER_CONFIG_FLAGS_MAX_COALESCED_METADATA_CHANGE_SIZE))) {
        SetDWORDInParameters(DB_MAX_COALESCED_METADATA_CHANGE_SIZE, DriverContext.MaxCoalescedMetaDataChangeSize);
    }

    if ((OutPutFlag & DRIVER_CONFIG_FLAGS_VALIDATION_LEVEL) && 
            (!(ulError & DRIVER_CONFIG_FLAGS_VALIDATION_LEVEL))) {
        SetDWORDInParameters(DB_VALIDATION_LEVEL, DriverContext.ulValidationLevel);
    }

    if ((OutPutFlag & DRIVER_CONFIG_FLAGS_VOL_THRESHOLD_FOR_FILE_WRITE) && 
            (!(ulError & DRIVER_CONFIG_FLAGS_VOL_THRESHOLD_FOR_FILE_WRITE))) {
        SetDWORDInParameters(DEVICE_THRESHOLD_FOR_FILE_WRITE, DriverContext.ulDaBThresholdPerDevForFileWrite);
    }

    if ((OutPutFlag & DRIVER_CONFIG_FLAGS_FREE_THRESHOLD_FOR_FILE_WRITE) && 
            (!(ulError & DRIVER_CONFIG_FLAGS_FREE_THRESHOLD_FOR_FILE_WRITE))) {
        SetDWORDInParameters(FREE_THRESHOLD_FOR_FILE_WRITE, DriverContext.ulDaBFreeThresholdForFileWrite);
    }

    if ((OutPutFlag & DRIVER_CONFIG_DATA_POOL_SIZE) && 
            (!(ulError & DRIVER_CONFIG_DATA_POOL_SIZE))) {
        if (((ULONG)(DriverContext.ullDataPoolSize / MEGABYTES)) != pDriverInput->ulDataPoolSize) {
            ULONGLONG ullDataPoolSize = ((ULONGLONG)pDriverInput->ulDataPoolSize) * MEGABYTES;
            /* Data Pool Allocation or free is done with respect to new DataPoolSize.
               If required Data Pool cannot be allocated by system then old value is retained.
               Data Pool need to be Freed, It will be done as data is drained
               ulMaxDataSizePerDev is reset to 80% of DataPoolSize*/
            if (0 != InDskFltGetNumFilteredDevices()) {
                Status = SetDataPool(0, ullDataPoolSize);
            } else {
                Status = STATUS_SUCCESS;
                KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
                DriverContext.ullDataPoolSize = ((ULONGLONG)pDriverInput->ulDataPoolSize) * MEGABYTES;
                ULONG ulMaxDataSizePerDev = ULONG((((DriverContext.ullDataPoolSize / MEGABYTES) * MAX_DATA_SIZE_PER_VOL_PERC) / 100) * MEGABYTES);
                DriverContext.ulMaxDataSizePerDev = ValidateMaxDataSizePerDev(ulMaxDataSizePerDev);
                KeReleaseSpinLock(&DriverContext.Lock, OldIrql);
            }
            if (!NT_SUCCESS(Status)) {
                ulError = ulError | DRIVER_CONFIG_DATA_POOL_SIZE;
            }
        }
    }

    if ((OutPutFlag & DRIVER_CONFIG_FLAGS_MAX_DATA_PER_NON_DATA_MODE_DIRTY_BLOCK) && 
            (!(ulError & DRIVER_CONFIG_FLAGS_MAX_DATA_PER_NON_DATA_MODE_DIRTY_BLOCK))) {
        SetDWORDInParameters(MAX_DATA_SIZE_PER_NON_DATA_MODE_DIRTY_BLOCK, DriverContext.ulMaxDataSizePerNonDataModeDirtyBlock / KILOBYTES);
    }
    
    if ((OutPutFlag & DRIVER_CONFIG_FLAGS_MAX_DATA_PER_DATA_MODE_DIRTY_BLOCK) && 
            (!(ulError & DRIVER_CONFIG_FLAGS_MAX_DATA_PER_DATA_MODE_DIRTY_BLOCK))) {
        SetDWORDInParameters(MAX_DATA_SIZE_PER_DATA_MODE_DIRTY_BLOCK, DriverContext.ulMaxDataSizePerDataModeDirtyBlock  / KILOBYTES);
    }

    if ((OutPutFlag & DRIVER_CONFIG_FLAGS_ENABLE_FS_FLUSH) &&
        (!(ulError & DRIVER_CONFIG_FLAGS_ENABLE_FS_FLUSH)))
    {
        ULONG   dwValue = (pDriverInput->bEnableFSFlushPreShutdown) ? 1 : 0;
        SetDWORDInParameters(FS_FLUSH_PRE_SHUTDOWN, dwValue);
    }

    if ((OutPutFlag & DRIVER_CONFIG_FLAGS_DATA_LOG_DIRECTORY) && 
            (!(ulError & DRIVER_CONFIG_FLAGS_DATA_LOG_DIRECTORY))) {
        SetStrInParameters(DATA_LOG_DIRECTORY, &DriverContext.DataLogDirectory);
        if ((OutPutFlag & DRIVER_CONFIG_FLAGS_DATA_LOG_DIRECTORY_ALL) && 
            (!(ulError & DRIVER_CONFIG_FLAGS_DATA_LOG_DIRECTORY_ALL))) {
            LIST_ENTRY  VolumeNodeList;
            NTSTATUS StatusVol = GetListOfDevs(&VolumeNodeList);
            if (STATUS_SUCCESS == StatusVol) {
                 while(!IsListEmpty(&VolumeNodeList)) {
                     PLIST_NODE  pNode = (PLIST_NODE)RemoveHeadList(&VolumeNodeList);;
                     PDEV_CONTEXT DevContext = (PDEV_CONTEXT)pNode->pData;
                     DereferenceListNode(pNode);
                     if (0 != DevContext->DataLogDirectory.Length) {
                         StatusVol = KeWaitForSingleObject(&DevContext->Mutex, Executive, KernelMode, FALSE, NULL);                         
                         ASSERT(STATUS_SUCCESS == StatusVol);
                         StatusVol = InitializeDataLogDirectory(DevContext, DriverContext.DataLogDirectory.Buffer, 1);
                         KeReleaseMutex(&DevContext->Mutex, FALSE);
                    }
                    DereferenceDevContext(DevContext);
                    if (!NT_SUCCESS(StatusVol)) {
                    	ulError |= DRIVER_CONFIG_FLAGS_DATA_LOG_DIRECTORY_ALL;
                    }
                }
            } else {
                ulError |= DRIVER_CONFIG_FLAGS_DATA_LOG_DIRECTORY_ALL;
            }
        }
    }    

    if (ValidOutBuff) {
        pDriverConfig->ulFlags1 |= OutPutFlag;
        // NonPagePool for given DataPage Pool is calculated.
        ULONGLONG ullReqNonPagePool = ReqNonPagePoolMem();
        pDriverConfig->ulReqNonPagePool = ULONG (ullReqNonPagePool / MEGABYTES);
        pDriverConfig->ulError = ulError;
        //Following variables were set after configuring output buffer.
        pDriverConfig->ulDataPoolSize = (ULONG)(DriverContext.ullDataPoolSize / MEGABYTES);
        pDriverConfig->ulMaxDataSizePerDev = (ULONG)((DriverContext.ulMaxDataSizePerDev) / KILOBYTES);
        pDriverConfig->bEnableDataCapture = DriverContext.bEnableDataCapture;
    }

Cleanup :
    if (pDriverInput) {
        ExFreePoolWithTag(pDriverInput, TAG_GENERIC_NON_PAGED);
    }

    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS
TagVolumesAtomically(
    PTAG_INFO       pTagInfo,
    PUCHAR          pSequenceOfTags
    )
{
    KIRQL               OldIrql;
    ULONG               i;
    PDEV_CONTEXT        DevContext;
    LIST_ENTRY          LockedDataBlockHead;
    PDATA_BLOCK         DataBlock = NULL;
    ASSERT(NULL != pTagInfo);
    ULONG               ulNumberOfDevices = pTagInfo->ulNumberOfDevices;
    bool                bFilteringNotEnabled = false;

    InVolDbgPrint(DL_FUNC_TRACE, DM_TAGS, ("TagVolumesAutomically: Called\n"));

    InitializeListHead(&LockedDataBlockHead);
    for (i = 0; i < ulNumberOfDevices; i++) {
        DataBlock = AllocateLockedDataBlock(TRUE);
        if (NULL != DataBlock) {
            InsertHeadList(&LockedDataBlockHead, &DataBlock->ListEntry);
        }
    }

    UpdateLockedDataBlockList(DriverContext.ulMaxLockedDataBlocks + ulNumberOfDevices);
    DataBlock = NULL;
    KeAcquireSpinLock(&DriverContext.MultiDeviceLock, &OldIrql);

    //Acquire all volume locks, and tag lastdirtyblock in volume of lastchange
    for (i = 0; i < ulNumberOfDevices; i++) {
        DevContext = pTagInfo->TagStatus[i].DevContext;
        if (NULL != DevContext) {
            KeAcquireSpinLockAtDpcLevel(&DevContext->Lock);
            if (0 == (DevContext->ulFlags & DCF_FILTERING_STOPPED)) {
                // Tag last time stamp for all dirty blocks in volume.
                if (NULL == DataBlock) {
                    if (!IsListEmpty(&LockedDataBlockHead)) {
                        DataBlock = (PDATA_BLOCK)RemoveHeadList(&LockedDataBlockHead);
                    }
                }
                AddLastChangeTimestampToCurrentNode(DevContext, &DataBlock);
            } else {
                pTagInfo->TagStatus[i].Status = STATUS_INVALID_DEVICE_STATE;
                bFilteringNotEnabled = true;
            }
        } else {
            ASSERT(STATUS_NOT_FOUND == pTagInfo->TagStatus[i].Status);
        }
    }

    NTSTATUS    Status = STATUS_SUCCESS;
    if (bFilteringNotEnabled && (0 == (pTagInfo->ulFlags & TAG_INFO_FLAGS_IGNORE_IF_FILTERING_STOPPED))) {
        Status = STATUS_INVALID_DEVICE_STATE;
    } else {
        PTIME_STAMP_TAG_V2  pTimeStamp = NULL;
        LARGE_INTEGER       liTickCount;
    
        liTickCount.QuadPart = 0;
    
        for (i = 0; ((STATUS_SUCCESS == Status) && (i < ulNumberOfDevices)); i++) {
            DevContext = pTagInfo->TagStatus[i].DevContext;
            
            if (STATUS_PENDING == pTagInfo->TagStatus[i].Status) {
                ASSERT(NULL != DevContext);
                if (pTagInfo->ulFlags & TAG_INFO_FLAGS_WAIT_FOR_COMMIT) {
                    Status = AddUserDefinedTags(DevContext, pSequenceOfTags, &pTimeStamp, &liTickCount, &pTagInfo->TagGUID, i);
                } else {
                    Status = AddUserDefinedTags(DevContext, pSequenceOfTags, &pTimeStamp, &liTickCount, NULL, 0);
                }
                pTagInfo->TagStatus[i].Status = Status;
            }    
        }
    }

    // Release all locks;
	for (i = ulNumberOfDevices; i != 0; i--) {
        DevContext = pTagInfo->TagStatus[i - 1].DevContext;
        if (NULL != DevContext) {
            KeReleaseSpinLockFromDpcLevel(&DevContext->Lock);        
        }
    }

    KeReleaseSpinLock(&DriverContext.MultiDeviceLock, OldIrql);


    if (NULL != DataBlock) {
        AddDataBlockToLockedDataBlockList(DataBlock);
    }

    while(!IsListEmpty(&LockedDataBlockHead)) {
        DataBlock = (PDATA_BLOCK)RemoveHeadList(&LockedDataBlockHead);
        AddDataBlockToLockedDataBlockList(DataBlock);
    }

    return Status;
}

NTSTATUS
TagVolumesInSequence(
    PTAG_INFO       pTagInfo,
    PUCHAR          pSequenceOfTags)
{
    return TagVolumesInSequence(pTagInfo, pSequenceOfTags, DriverContext.TagNotifyContext.isCommitNotifyTagInsertInProcess);
}

NTSTATUS
TagVolumesInSequence(
    PTAG_INFO       pTagInfo,
    PUCHAR          pSequenceOfTags,
    bool            isCommitNotifyTagRequest
    )
{
    KIRQL               OldIrql;
    ULONG               i;
    PDEV_CONTEXT        DevContext;
    NTSTATUS            Status = STATUS_SUCCESS;
    PDATA_BLOCK         DataBlock = NULL;
    ULONG               ulNumberOfDevices = pTagInfo->ulNumberOfDevices;

    InVolDbgPrint(DL_FUNC_TRACE, DM_TAGS, ("TagVolumesInSequence: Called\n"));

    for (i = 0; (STATUS_SUCCESS == Status) && (i < ulNumberOfDevices); i++) 
    {
        DevContext = pTagInfo->TagStatus[i].DevContext;

        if (NULL == DevContext)
        {
            ASSERT(STATUS_NOT_FOUND == pTagInfo->TagStatus[i].Status);
            continue;
        }

        if (DevContext->ulFlags & DCF_FILTERING_STOPPED)
        {
            pTagInfo->TagStatus[i].Status = STATUS_INVALID_DEVICE_STATE;
            if (0 == (pTagInfo->ulFlags & TAG_INFO_FLAGS_IGNORE_IF_FILTERING_STOPPED))
            {
                Status = STATUS_INVALID_DEVICE_STATE;
            }
            continue;
        }

        if (NULL == DataBlock) {
            DataBlock = AllocateLockedDataBlock(TRUE);
        }

        KeAcquireSpinLock(&DevContext->Lock, &OldIrql);

        // Close the current DirtyBlock
        AddLastChangeTimestampToCurrentNode(DevContext, &DataBlock);

        // Set Notification event if any dirty block is in queue
        if (DevContext->bNotify && DevContext->DBNotifyEvent) {
            if (DirtyBlockReadyForDrain(DevContext)) {
                DevContext->liTickCountAtNotifyEventSet = KeQueryPerformanceCounter(NULL);
                KeSetEvent(DevContext->DBNotifyEvent, EVENT_INCREMENT, FALSE);
                DevContext->bNotify = false;
            }
        }

        if (pTagInfo->ulFlags & TAG_INFO_FLAGS_WAIT_FOR_COMMIT) 
        {
            Status = AddUserDefinedTags(DevContext, pSequenceOfTags, NULL, NULL, &pTagInfo->TagGUID, i);
        }
        else
        {
#ifdef VOLUME_FLT
            Status = AddUserDefinedTags(DevContext, pSequenceOfTags, NULL, NULL, NULL, 0);
#else
            pTagInfo->TagCommonAttribs.ulNumberOfDevicesTagsApplied = pTagInfo->ulNumberOfDevicesTagsApplied;

            Status = AddUserDefinedTagsForDiskDevice(DevContext, pSequenceOfTags, &pTagInfo->TagCommonAttribs, isCommitNotifyTagRequest);
            pTagInfo->TagStatus[i].Status = Status;

            if (!NT_SUCCESS(Status)) {
                pTagInfo->TagCommonAttribs.TagStatus = Status;
                pTagInfo->TagCommonAttribs.GlobaIOCTLStatus = STATUS_NOT_SUPPORTED;
                pTagInfo->TagCommonAttribs.ullTagState = ecTagStatusInsertFailure;
                KeQuerySystemTime(&pTagInfo->TagCommonAttribs.liTagInsertSystemTimeStamp);
                InDskFltTelemetryInsertTagFailure(DevContext, &pTagInfo->TagCommonAttribs, ecMsgTagInsertFailure);
            }

            if (NT_SUCCESS(Status))
            {
                pTagInfo->ulNumberOfDevicesTagsApplied++;
                // There can be a case that device is not in commit notify list
                // So validate both of argument
                if (isCommitNotifyTagRequest && DevContext->TagCommitNotifyContext.isCommitNotifyTagPending) {
                    DevContext->TagCommitNotifyContext.TagState = DEVICE_TAG_NOTIFY_STATE_TAG_INSERTED;
                }
            }
            else 
            {
                if (pTagInfo->ulFlags & TAG_INFO_FLAGS_IGNORE_DISKS_IN_NON_WO_STATE) {
                    pTagInfo->ulNumberOfDevicesIgnored++;
                }
                Status = STATUS_SUCCESS;
                // There can be a case that device is not in commit notify list
                // So validate both of argument
                if (isCommitNotifyTagRequest && DevContext->TagCommitNotifyContext.isCommitNotifyTagPending) {
                    DevContext->TagCommitNotifyContext.TagState = DEVICE_TAG_NOTIFY_STATE_TAG_NOT_INSERTED;
                    RtlCopyMemory(&DevContext->TagCommitNotifyContext.CxSession, &DevContext->CxSession, sizeof(DevContext->CxSession));
                }
            }
#endif
        }
        KeReleaseSpinLock(&DevContext->Lock, OldIrql);
    }

    if (NULL != DataBlock) {
        AddDataBlockToLockedDataBlockList(DataBlock);
    }

    return Status;
}

NTSTATUS
DeviceIoControlSyncTagVolume(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS Status                     = STATUS_SUCCESS;
    PIO_STACK_LOCATION IoStackLocation  = IoGetCurrentIrpStackLocation(Irp);
    PSYNC_TAG_INPUT_DATA SyncTagInputData   = (PSYNC_TAG_INPUT_DATA)Irp->AssociatedIrp.SystemBuffer;
    PUCHAR pBuffer                          = (PUCHAR)&SyncTagInputData->ulFlags;
    ULONG ulNumberOfVolumes                 = SyncTagInputData->ulNumberOfVolumeGUIDs;

    UNREFERENCED_PARAMETER(DeviceObject);
    ASSERT(IOCTL_INMAGE_SYNC_TAG_VOLUME == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

    Irp->IoStatus.Information = 0;

    if ( (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < sizeof(SYNC_TAG_INPUT_DATA)) || 
		!IsValidIoctlTagDeviceInputBuffer(pBuffer, IoStackLocation->Parameters.DeviceIoControl.InputBufferLength - sizeof(GUID)))
    {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_TAGS, 
            ("DeviceIoControlTagVolume: IOCTL rejected, invalid input buffer \n"));
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    if ((ulNumberOfVolumes == 0) || (ulNumberOfVolumes > DriverContext.ulNumDevs)) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_TAGS, 
            ("DeviceIoControlTagVolume: IOCTL rejected, invalid number of volumes(%d), ulNumDevs(%d)\n", 
             ulNumberOfVolumes, DriverContext.ulNumDevs));
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < (SIZE_OF_TAG_VOLUME_STATUS_OUTPUT_DATA(ulNumberOfVolumes))) {
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    ULONG   ulSizeOfTagInfo = sizeof(TAG_INFO) + (ulNumberOfVolumes * sizeof(TAG_STATUS));
    PTAG_INFO   pTagInfo = (PTAG_INFO) ExAllocatePoolWithTag(NonPagedPool, ulSizeOfTagInfo, TAG_GENERIC_NON_PAGED);
    if (NULL == pTagInfo) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_TAGS, 
            ("DeviceIoControlTagVolume: memory allocation failed \n"));
        Status = STATUS_NO_MEMORY;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    RtlZeroMemory(pTagInfo, ulSizeOfTagInfo);
    InVolDbgPrint(DL_FUNC_TRACE, DM_TAGS, ("DeviceIoControlTagVolume: Called on %d Volumes\n", ulNumberOfVolumes));
    pTagInfo->ulNumberOfDevices = ulNumberOfVolumes;
    if (SyncTagInputData->ulFlags & TAG_VOLUME_INPUT_FLAGS_NO_ERROR_IF_FILTERING_STOPPED) {
        pTagInfo->ulFlags |= TAG_INFO_FLAGS_IGNORE_IF_FILTERING_STOPPED;
    }

    bool    bCompleteIrp = false;

    for (ULONG i = 0; i < ulNumberOfVolumes; i++) {
#ifdef VOLUME_FLT
        pTagInfo->TagStatus[i].DevContext = GetDevContextWithGUID((PWCHAR)(pBuffer + i * GUID_SIZE_IN_BYTES + 2 * sizeof(ULONG)));
#else
		pTagInfo->TagStatus[i].DevContext = GetDevContextWithGUID((PWCHAR)(pBuffer + i * GUID_SIZE_IN_BYTES + 2 * sizeof(ULONG)), GUID_SIZE_IN_BYTES);
#endif
        if (NULL != pTagInfo->TagStatus[i].DevContext) {
            pTagInfo->TagStatus[i].Status = STATUS_PENDING; // Initialize with STATUS_PENDING
            InVolDbgPrint(DL_FUNC_TRACE, DM_TAGS, 
                          ("\tGUID=%S Volume=%S\n", pTagInfo->TagStatus[i].DevContext->wDevID, pTagInfo->TagStatus[i].DevContext->wDevNum));
        } else {
            pTagInfo->TagStatus[i].Status = STATUS_NOT_FOUND; // GUID not found.
			pTagInfo->ulNumberOfDevicesIgnored++;
            if (0 == (SyncTagInputData->ulFlags & TAG_VOLUME_INPUT_FLAGS_NO_ERROR_IF_GUID_NOT_FOUND))
                bCompleteIrp = true;
        }
    }


    if (bCompleteIrp) {
        PTAG_VOLUME_STATUS_OUTPUT_DATA OutputData = (PTAG_VOLUME_STATUS_OUTPUT_DATA)Irp->AssociatedIrp.SystemBuffer;

        for (ULONG i = 0; i < ulNumberOfVolumes; i++) {
            if (NULL == pTagInfo->TagStatus[i].DevContext) {
                OutputData->VolumeStatus[i] = ecTagStatusInvalidGUID;
            } else {
                OutputData->VolumeStatus[i] = ecTagStatusUnattempted;
            }
        }

        OutputData->Status = ecTagStatusInvalidGUID;
        OutputData->ulNoOfVolumes = ulNumberOfVolumes;
        Irp->IoStatus.Information = SIZE_OF_TAG_VOLUME_STATUS_OUTPUT_DATA(ulNumberOfVolumes);
        Status = STATUS_SUCCESS;
    } else {
        ULONG ulTagStartOffset  = (2 * sizeof(ULONG)) + (ulNumberOfVolumes * GUID_SIZE_IN_BYTES);
        if (0 == (SyncTagInputData->ulFlags & TAG_VOLUME_INPUT_FLAGS_DEFER_TILL_COMMIT_SNAPSHOT)) {
            if (SyncTagInputData->ulFlags & TAG_VOLUME_INPUT_FLAGS_NO_WAIT_FOR_COMMIT) {
                bCompleteIrp = true;
                if (SyncTagInputData->ulFlags & TAG_VOLUME_INPUT_FLAGS_ATOMIC_TO_VOLUME_GROUP) {
                    Status = TagVolumesAtomically(pTagInfo, pBuffer + ulTagStartOffset);
                } else {
                    Status = TagVolumesInSequence(pTagInfo, pBuffer + ulTagStartOffset);
                }
            } else {
                RtlCopyMemory(&pTagInfo->TagGUID, &SyncTagInputData->TagGUID, sizeof(GUID));
                pTagInfo->ulFlags |= TAG_INFO_FLAGS_WAIT_FOR_COMMIT;

                Status = AddTagNodeToList(pTagInfo, Irp, ecTagStateApplied);

                if (STATUS_SUCCESS == Status) {
                    ASSERT(false == bCompleteIrp);
                    // TagNode allocation has succeeded and IRP isn't cancelled..
                    if (SyncTagInputData->ulFlags & TAG_VOLUME_INPUT_FLAGS_ATOMIC_TO_VOLUME_GROUP) {
                        Status = TagVolumesAtomically(pTagInfo, pBuffer + ulTagStartOffset);
                    } else {
                        Status = TagVolumesInSequence(pTagInfo, pBuffer + ulTagStartOffset);
                    }

                    PTAG_NODE TagNode = AddStatusAndReturnNodeIfComplete(pTagInfo);
                    if (NULL != TagNode) {
                        // TagNode is only returned if by this time IRP is not completed.
                        // IRP could have been completed if tag is commmited/dropped/deleted..
                        CopyStatusToOutputBuffer(TagNode, (PTAG_VOLUME_STATUS_OUTPUT_DATA) Irp->AssociatedIrp.SystemBuffer);
                        Irp->IoStatus.Information = SIZE_OF_TAG_VOLUME_STATUS_OUTPUT_DATA(ulNumberOfVolumes);
                        DeallocateTagNode(TagNode);
                        bCompleteIrp = true;
                        Status = STATUS_SUCCESS;
                    }
                } else {
                    bCompleteIrp = true;
                }
            }
        } else {

            if (0 == (SyncTagInputData->ulFlags & TAG_VOLUME_INPUT_FLAGS_NO_WAIT_FOR_COMMIT)) {
                RtlCopyMemory(&pTagInfo->TagGUID, &SyncTagInputData->TagGUID, sizeof(GUID));
                pTagInfo->ulFlags |= TAG_INFO_FLAGS_WAIT_FOR_COMMIT;
            }

            pTagInfo->ulFlags |= TAG_INFO_FLAGS_TYPE_SYNC;
            pTagInfo->pBuffer = pBuffer + ulTagStartOffset;   
            Status = AddTagNodeToList(pTagInfo, Irp, ecTagStateWaitForCommitSnapshot);
            if (STATUS_SUCCESS == Status) {
                bCompleteIrp = false;
                pTagInfo = NULL; // TagInfo is saved inside TagNode
            }

        }
        
    }

    if (bCompleteIrp) {
    // Dereference all volumeContexts
        for(ULONG i = 0; i < ulNumberOfVolumes; i++) {
        if (pTagInfo->TagStatus[i].DevContext != NULL) {
            DereferenceDevContext(pTagInfo->TagStatus[i].DevContext);
        }
    }

        ASSERT(STATUS_PENDING != Status);
        // Do we have to copy the information back to output buffer?
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    } else {
        Status = STATUS_PENDING;
    }

    if (pTagInfo) {
        ExFreePoolWithTag(pTagInfo, TAG_GENERIC_NON_PAGED);
    }

    return Status;
}

NTSTATUS
DeviceIoControlGetTagVolumeStatus(
    PDEVICE_OBJECT DeviceObject, 
    PIRP Irp
    )
{

    NTSTATUS Status = STATUS_SUCCESS;
    PTAG_VOLUME_STATUS_INPUT_DATA  Input = (PTAG_VOLUME_STATUS_INPUT_DATA)Irp->AssociatedIrp.SystemBuffer;
    PTAG_VOLUME_STATUS_OUTPUT_DATA  Output = (PTAG_VOLUME_STATUS_OUTPUT_DATA)Irp->AssociatedIrp.SystemBuffer;
    PIO_STACK_LOCATION  IoStackLocation=IoGetCurrentIrpStackLocation(Irp);
    ULONG OutputBufferLength = IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength;
    
    UNREFERENCED_PARAMETER(DeviceObject);
    ASSERT(IOCTL_INMAGE_GET_TAG_VOLUME_STATUS == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);
    
    if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < sizeof(TAG_VOLUME_STATUS_INPUT_DATA)) {
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    Status = CopyStatusToOutputBuffer(Input->TagGUID, Output, OutputBufferLength);

    if (!NT_SUCCESS(Status)) {
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    Irp->IoStatus.Information = SIZE_OF_TAG_VOLUME_STATUS_OUTPUT_DATA(Output->ulNoOfVolumes);
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

/*
Input Tag Stream Structure
 __________________________________________________________________________________________________________________________________
|           |                |                 |    |              |              |         |          |     |         |           |
|   Flags   | No. of GUIDs(n)|  Volume GUID1   | ...| Volume GUIDn | Tag Len (k)  |Tag1 Size| Tag1 data| ... |Tagk Size| Tagk Data |
|_(4 Bytes)_|____(4 Bytes)__ |____(36 Bytes)___|____|______________|__(4 Bytes)___|_2 Bytes_|__________|_____|_________|___________|

*/

bool
IsValidIoctlTagDeviceInputBuffer(
    PUCHAR pBuffer,
    ULONG ulBufferLength
    )
{
    ULONG ulNumberOfDevices = 0;
    ULONG ulBytesParsed     = 0;
    ULONG ulNumberOfTags    = 0;

    if (ulBufferLength < 2 * sizeof(ULONG))
        return false;

    ulNumberOfDevices = *(PULONG) (pBuffer + sizeof(ULONG));
    ulBytesParsed += 2 * sizeof(ULONG);

    if (0 == ulNumberOfDevices)
        return false;
    
    if (ulBufferLength < (ulBytesParsed + ulNumberOfDevices * GUID_SIZE_IN_BYTES))
        return false;

    ulBytesParsed += ulNumberOfDevices * GUID_SIZE_IN_BYTES;

    if (ulBufferLength < ulBytesParsed + sizeof(ULONG))
        return false;

    ulNumberOfTags = *(PULONG) (pBuffer + ulBytesParsed);
    ulBytesParsed += sizeof(ULONG);

    if (0 == ulNumberOfTags)
        return false;

    PUCHAR  pTags = pBuffer + ulBytesParsed;

    for (ULONG i = 0; i < ulNumberOfTags; i++) {
        if (ulBufferLength < ulBytesParsed + sizeof(USHORT))
            return false;
        
        USHORT usTagDataLength  = *(USHORT UNALIGNED *) (pBuffer + ulBytesParsed);
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

#if DBG
NTSTATUS
DeviceIoControlSetDebugInfo(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    PDEBUG_INFO         pDebugInfo = NULL;
    UNREFERENCED_PARAMETER(DeviceObject);
    ASSERT(IOCTL_INMAGE_SET_DEBUG_INFO == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);
    Irp->IoStatus.Information = 0;

    if( IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < sizeof(DEBUG_INFO) ) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL, 
            ("DeviceIoControlSetDebugInfo: IOCTL rejected, input buffer too small expected(%#x) received(%#x)\n",
                sizeof(DEBUG_INFO), IoStackLocation->Parameters.DeviceIoControl.InputBufferLength));
        Status = STATUS_BUFFER_TOO_SMALL;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    
    pDebugInfo = (PDEBUG_INFO)Irp->AssociatedIrp.SystemBuffer;

    if (pDebugInfo->ulDebugInfoFlags & DEBUG_INFO_FLAGS_RESET_MODULES)
        ulDebugMask = ulDebugMask & ~(pDebugInfo->ulDebugModules);
    else
        ulDebugMask |= pDebugInfo->ulDebugModules;

    if (pDebugInfo->ulDebugInfoFlags & DEBUG_INFO_FLAGS_SET_LEVEL_ALL)
        ulDebugLevelForAll = pDebugInfo->ulDebugLevelForAll;

    if (pDebugInfo->ulDebugInfoFlags & DEBUG_INFO_FLAGS_SET_LEVEL)
        ulDebugLevel = pDebugInfo->ulDebugLevelForMod;

    if ( IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength >= sizeof(DEBUG_INFO)) {
        // Copy the resultant values back to system buffer.
        pDebugInfo->ulDebugInfoFlags = 0;
        pDebugInfo->ulDebugLevelForAll = ulDebugLevelForAll;
        pDebugInfo->ulDebugLevelForMod = ulDebugLevel;
        pDebugInfo->ulDebugModules = ulDebugMask;

        Irp->IoStatus.Information = sizeof(DEBUG_INFO);
    } else {
        Irp->IoStatus.Information = 0;
    }

    Irp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}
#endif // DBG


NTSTATUS
DeviceIoControlResyncStartNotification(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status;
    KIRQL               OldIrql;
    PDEV_CONTEXT     DevContext = NULL;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    PRESYNC_START_OUTPUT_V2    pResyncStartOutput = (PRESYNC_START_OUTPUT_V2)Irp->AssociatedIrp.SystemBuffer;
    Irp->IoStatus.Information = 0;

    ASSERT(IOCTL_INMAGE_RESYNC_START_NOTIFICATION == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(RESYNC_START_OUTPUT_V2)) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL, 
            ("DeviceIoControlResyncStartNotification: IOCTL rejected, output buffer too small\n"));
        Status = STATUS_BUFFER_TOO_SMALL;
        goto cleanup;
    }

    DevContext = GetDevContextForThisIOCTL(DeviceObject, Irp);
    if (NULL == DevContext) {
        Status = STATUS_NO_SUCH_DEVICE;
        goto cleanup;
    }

    if (IS_DISK_CLUSTERED(DevContext)) {
        SetDiskClusterState(DevContext);
        if (TEST_FLAG(DevContext->ulFlags, DCF_FILTERING_STOPPED) &&
            TEST_FLAG(DevContext->ulClusterFlags, DCF_CLUSTER_FLAGS_DISK_PROTECTED) &&
            !TEST_FLAG(DevContext->ulClusterFlags, DCF_CLUSTER_FLAGS_DISK_ONLINE)) {
            Status = STATUS_FILE_IS_OFFLINE;
            InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_RESYNC_START_FAILED, DevContext, Status, DevContext->ulFlags);
            goto cleanup;
        }
    }

    if (TEST_FLAG(DevContext->ulFlags, DCF_FILTERING_STOPPED)) {
        Status = STATUS_INVALID_DEVICE_STATE;
        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_RESYNC_START_FAILED, DevContext, Status, DevContext->ulFlags);
        goto cleanup;
    }

    PDATA_BLOCK DataBlock = AllocateLockedDataBlock(TRUE);

    KeAcquireSpinLock(&DevContext->Lock, &OldIrql);
    DevContext->bResyncStartReceived = true;
    AddLastChangeTimestampToCurrentNode(DevContext, &DataBlock);
    GenerateTimeStamp(pResyncStartOutput->TimeInHundNanoSecondsFromJan1601, pResyncStartOutput->ullSequenceNumber);
    DevContext->liResyncStartTimeStamp.QuadPart = (LONGLONG)pResyncStartOutput->TimeInHundNanoSecondsFromJan1601;

    DevContext->ResynStartTimeStamp = pResyncStartOutput->TimeInHundNanoSecondsFromJan1601;
    DevContext->liLastCommittedOutOfSyncTimeStamp.QuadPart = DevContext->liLogLastOutOfSyncTimeStamp.QuadPart;
    DevContext->ulLastCommittedtOutOfSyncErrorCode = DevContext->ulLogLastOutOfSyncErrorCode;

    InDskFltWriteEventWithDevCtxt(INFLTDRV_INFO_RESYNC_START, DevContext,
                                    DevContext->liResyncStartTimeStamp.QuadPart,
                                    DevContext->liLastCommittedOutOfSyncTimeStamp.QuadPart,
                                    DevContext->ulLastCommittedtOutOfSyncErrorCode);

    DevContext->liLogLastOutOfSyncTimeStamp.QuadPart = 0;
    DevContext->ulLogLastOutOfSyncErrorCode = 0;

    KeReleaseSpinLock(&DevContext->Lock, OldIrql);

    if (NULL != DataBlock) {
        AddDataBlockToLockedDataBlockList(DataBlock);
    }

    Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = sizeof(RESYNC_START_OUTPUT_V2);

cleanup:
    if (NULL != DevContext) {
        DereferenceDevContext(DevContext);
    }
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS
DeviceIoControlResyncEndNotification(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status;
    KIRQL               OldIrql;
    PDEV_CONTEXT     DevContext = NULL;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    PRESYNC_END_OUTPUT_V2  pResyncEndOutput = (PRESYNC_END_OUTPUT_V2)Irp->AssociatedIrp.SystemBuffer;

    ASSERT(IOCTL_INMAGE_RESYNC_END_NOTIFICATION == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

    Irp->IoStatus.Information = 0;
    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(RESYNC_END_OUTPUT_V2)) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL, 
            ("DeviceIoControlResyncEndNotification: IOCTL rejected, output buffer too small\n"));
        Status = STATUS_BUFFER_TOO_SMALL;
        goto cleanup;
    }

    DevContext = GetDevContextForThisIOCTL(DeviceObject, Irp);
    if (NULL == DevContext) {
        Status = STATUS_NO_SUCH_DEVICE;
        goto cleanup;
    }

    if (IS_DISK_CLUSTERED(DevContext)) {
        SetDiskClusterState(DevContext);
        if (TEST_FLAG(DevContext->ulFlags, DCF_FILTERING_STOPPED) &&
            TEST_FLAG(DevContext->ulClusterFlags, DCF_CLUSTER_FLAGS_DISK_PROTECTED) &&
            !TEST_FLAG(DevContext->ulClusterFlags, DCF_CLUSTER_FLAGS_DISK_ONLINE)) {
            Status = STATUS_FILE_IS_OFFLINE;
            InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_RESYNC_END_FAILED, DevContext, Status, DevContext->ulFlags);
            goto cleanup;
        }
    }

    if (TEST_FLAG(DevContext->ulFlags, DCF_FILTERING_STOPPED)) {

        Status = STATUS_INVALID_DEVICE_STATE;
        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_RESYNC_END_FAILED, DevContext, Status, DevContext->ulFlags);
        goto cleanup;
    }


    PDATA_BLOCK DataBlock = AllocateLockedDataBlock(TRUE);            

    KeAcquireSpinLock(&DevContext->Lock, &OldIrql);
    AddLastChangeTimestampToCurrentNode(DevContext, &DataBlock);
    GenerateTimeStamp(pResyncEndOutput->TimeInHundNanoSecondsFromJan1601, pResyncEndOutput->ullSequenceNumber);
    DevContext->liResyncEndTimeStamp.QuadPart = (LONGLONG)pResyncEndOutput->TimeInHundNanoSecondsFromJan1601;

    InDskFltWriteEventWithDevCtxt(INFLTDRV_INFO_RESYNC_END, DevContext,
                                DevContext->liResyncStartTimeStamp.QuadPart,
                                DevContext->liResyncEndTimeStamp.QuadPart,
                                DevContext->liLastCommittedOutOfSyncTimeStamp.QuadPart,
                                DevContext->ulLastCommittedtOutOfSyncErrorCode);

    if (DevContext->ulPrevOutOfSyncErrorCodeSet) {
        InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_OUT_OF_SYNC_ON_RESYNC_END, DevContext,
                                    DevContext->ulOutOfSyncErrorCode,
                                    DevContext->liOutOfSyncTimeStamp.QuadPart,
                                    DevContext->ulOutOfSyncTotalCount,
                                    DevContext->ulOutOfSyncCount,
                                    DevContext->ulOutOfSyncErrorStatus);

        DevContext->ulPrevOutOfSyncErrorCodeSet = 0;
    }

    KeReleaseSpinLock(&DevContext->Lock, OldIrql);


    if (NULL != DataBlock) {
        AddDataBlockToLockedDataBlockList(DataBlock);
    }

    Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = sizeof(RESYNC_END_OUTPUT_V2);

cleanup:
    if (NULL != DevContext) {
        DereferenceDevContext(DevContext);
    }
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS
DeviceIoControlClearBitMap(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status = STATUS_SUCCESS;
    KIRQL               OldIrql;
    PDEV_CONTEXT     pDevContext = NULL;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);

    PKDIRTY_BLOCK       pDirtyBlock;
    PDEV_BITMAP      pDevBitmap = NULL;

    ASSERT(IOCTL_INMAGE_CLEAR_DIFFERENTIALS == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

    if (0 == IoStackLocation->Parameters.DeviceIoControl.InputBufferLength) {
        Status = STATUS_INFO_LENGTH_MISMATCH;
        goto Cleanup;
    }

    pDirtyBlock = NULL;
    pDevContext = GetDevContextForThisIOCTL(DeviceObject, Irp);
    if (NULL == pDevContext) {
        Status = STATUS_NO_SUCH_DEVICE;
        goto Cleanup;
    }

    if (IS_DISK_CLUSTERED(pDevContext)) {
        SetDiskClusterState(pDevContext);
        if (TEST_FLAG(pDevContext->ulFlags, DCF_FILTERING_STOPPED) &&
            TEST_FLAG(pDevContext->ulClusterFlags, DCF_CLUSTER_FLAGS_DISK_PROTECTED) &&
            !TEST_FLAG(pDevContext->ulClusterFlags, DCF_CLUSTER_FLAGS_DISK_ONLINE)) {
            Status = STATUS_FILE_IS_OFFLINE;
            goto Cleanup;
        }
    }

    if (TEST_FLAG(pDevContext->ulFlags, DCF_FILTERING_STOPPED)) {
        goto Cleanup;
    }

    KeWaitForSingleObject(&pDevContext->SyncEvent, Executive, KernelMode, FALSE, NULL);
    KeWaitForMutexObject(&pDevContext->BitmapOpenCloseMutex, Executive, KernelMode, FALSE, NULL);
    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);

    // Let us free the commit pending dirty blocks too.
    OrphanMappedDataBlocksAndMoveCommitPendingDirtyBlockToMainQueue(pDevContext);

    FreeChangeNodeList(&pDevContext->ChangeList, ecClearDiffs);
    if (NULL == pDevContext->pDevBitmap) {
        pDevContext->ulFlags |= DCF_CLEAR_BITMAP_ON_OPEN;
    } else {
        pDevBitmap = ReferenceDevBitmap(pDevContext->pDevBitmap);
                    // Check if capture mode can be changed.
        if ((ecCaptureModeMetaData == pDevContext->eCaptureMode) && CanSwitchToDataCaptureMode(pDevContext, 1)) {                
            InVolDbgPrint(DL_INFO, DM_CAPTURE_MODE | DM_DATA_FILTERING,
                            ("%s: Changing capture mode to DATA for device %s(%s)\n", __FUNCTION__,
                            pDevContext->chDevID, pDevContext->chDevNum));
            VCSetCaptureModeToData(pDevContext);
        }
        VCSetWOState(pDevContext, false, __FUNCTION__, ecMDReasonNotApplicable);
    }

    ClearDevStats(pDevContext);
    VCFreeReservedDataBlockList(pDevContext);
    pDevContext->ulcbDataFree = 0;
    pDevContext->ullcbDataAlocMissed += pDevContext->ulcbDataReserved;
    pDevContext->ulcbDataReserved = 0;
    KeQuerySystemTime(&pDevContext->liClearDiffsTimeStamp);
    pDevContext->EndTimeStampofDB = 0;

    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);

    // Clear the Bitmap
    if (NULL != pDevBitmap) {
        ClearBitmapFile(pDevBitmap);
        DereferenceDevBitmap(pDevBitmap);
    }

    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);        
    if (NULL != pDevContext->pDevBitmap) {
        pDevBitmap = ReferenceDevBitmap(pDevContext->pDevBitmap);
        if ((ecCaptureModeMetaData == pDevContext->eCaptureMode) && CanSwitchToDataCaptureMode(pDevContext, 1)) {
            InVolDbgPrint(DL_INFO, DM_CAPTURE_MODE | DM_DATA_FILTERING,
                            ("%s: Changing capture mode to DATA for device %s(%s)\n", __FUNCTION__,
                            pDevContext->chDevID, pDevContext->chDevNum));
            VCSetCaptureModeToData(pDevContext);
        }
        VCSetWOState(pDevContext, false, __FUNCTION__, ecMDReasonNotApplicable);
    }        
    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
    if (NULL != pDevBitmap) {
        DereferenceDevBitmap(pDevBitmap);
    }
    KeReleaseMutex(&pDevContext->BitmapOpenCloseMutex, FALSE);
    KeSetEvent(&pDevContext->SyncEvent, IO_NO_INCREMENT, FALSE);

    // Clear resync information
    ResetDevOutOfSync(pDevContext, true);
    Status = STATUS_SUCCESS;

Cleanup:
    if (pDevContext) {
        InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_DISK_CLEAR_DIFFS_ISSUED, pDevContext, Status, pDevContext->ulFlags);
        DereferenceDevContext(pDevContext);
    }

    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS
DeviceIoControlProcessStartNotify(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status = STATUS_PENDING;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    KIRQL               OldIrql;
    BOOLEAN             bIrpCancelled = FALSE;
    BOOLEAN             bFailIrp = FALSE;
    BOOLEAN             bCancelRoutineExecuted = FALSE;
    PEPROCESS           pServiceProcess = IoGetCurrentProcess();
    PROCESS_START_NOTIFY_INPUT  InputData;

    UNREFERENCED_PARAMETER(DeviceObject);

    ASSERT(IOCTL_INMAGE_PROCESS_START_NOTIFY == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

    if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength >= sizeof(PROCESS_START_NOTIFY_INPUT)) {
        InputData.ulFlags = ((PPROCESS_START_NOTIFY_INPUT)Irp->AssociatedIrp.SystemBuffer)->ulFlags;
    } else {        
		// Reject the IOCTL as buffer size is small
		InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_DIRTY_BLOCKS,
			("DeviceIoControlProcessStartNotify: IOCTL rejected, buffer too small\n"));
		Status = STATUS_BUFFER_TOO_SMALL;
		Irp->IoStatus.Information = 0;
		Irp->IoStatus.Status = Status;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return Status;
    }

    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);

    if (NULL != DriverContext.ProcessStartIrp) {
        bFailIrp = TRUE;
    } else {
        IoSetCancelRoutine(Irp, InMageFltCancelProcessStartIrp);

        // The IoSetCancelRoutine is only executed if Irp->Cancel is true.
        if (Irp->Cancel) {
            // Irp Already cancelled.
            bIrpCancelled = TRUE;
            if (NULL == IoSetCancelRoutine(Irp, NULL)) {
                bCancelRoutineExecuted = TRUE;
            } else {
                bCancelRoutineExecuted = FALSE;
            }
        } else {
            // Irp is not cancelled.
            ASSERT(NULL == DriverContext.UserProcess);
            IoMarkIrpPending(Irp);
            DriverContext.UserProcess = pServiceProcess;
            KeQuerySystemTime(&DriverContext.globalTelemetryInfo.liLastS2StartTime);
            DriverContext.ProcessStartIrp = Irp;
        }
    }

    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

    if (bFailIrp) {
        ASSERT(FALSE == bIrpCancelled);
        // Already have Process start notify irp. Fail this IRP.
        Status = STATUS_INVALID_DEVICE_STATE;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    if (bIrpCancelled) {
        Status = STATUS_CANCELLED;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        if (FALSE == bCancelRoutineExecuted) {
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
        }
        return Status;
    }

    ASSERT(STATUS_PENDING == Status);

    if (InputData.ulFlags & PROCESS_START_NOTIFY_INPUT_FLAGS_DATA_FILES_AWARE) {
        DriverContext.bS2SupportsDataFiles = true;
    }

#ifdef _WIN64
    if (InputData.ulFlags & PROCESS_START_NOTIFY_INPUT_FLAGS_64BIT_PROCESS) {
        DriverContext.bS2is64BitProcess = true;
    }
#else
    ASSERT(0 == (InputData.ulFlags & PROCESS_START_NOTIFY_INPUT_FLAGS_64BIT_PROCESS));
#endif // _WIN64
    return Status;
}

VOID
InDskFltHandleDiskResize(
_In_    PDEV_CONTEXT   pDevContext,
_In_    PGET_LENGTH_INFORMATION    pLengthInfo)
/*++

Routine Description:
    Routine for handling disk Resize.
    It compares current device size with actual size. 
    If size doesn't match 
        it logs an event.
        It also queue a resync notification.

Arguments:
    pDevContext     - Device Context.
    pLengthInfo     - Disk Length Info.

Return Value:

    None

--*/
{
    KIRQL       OldIrql;
    BOOLEAN     IsDiskFiltered = FALSE;
    LARGE_INTEGER   liOldSize = { 0 };
    LARGE_INTEGER   liNewSize = { 0 };

    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
    if (pLengthInfo->Length.QuadPart != pDevContext->llActualDeviceSize)
    {
        liOldSize.QuadPart = pDevContext->llActualDeviceSize;
        liNewSize.QuadPart = pLengthInfo->Length.QuadPart;

        pDevContext->llActualDeviceSize = pLengthInfo->Length.QuadPart;
        IsDiskFiltered = !TEST_FLAG(pDevContext->ulFlags, DCF_FILTERING_STOPPED);
    }
    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);

    if (IsDiskFiltered)
    {
        if ((0 != liOldSize.QuadPart) && (liOldSize.QuadPart != liNewSize.QuadPart)) {
            InDskFltWriteEventWithDevCtxt(INDSKFLT_WARN_DEV_RESIZED, pDevContext, (ULONGLONG)liNewSize.QuadPart / MEGABYTES);
        }

        QueueWorkerRoutineForSetDevOutOfSync(pDevContext, MSG_INDSKFLT_WARN_DEV_RESIZED_Message, STATUS_NOT_SUPPORTED, false);
    }
}

NTSTATUS
InDskSendDeviceControl(
_In_ PDEVICE_OBJECT DeviceObject,
_In_ ULONG IoControlCode,
_In_ PVOID InputBuffer,
_In_ ULONG InputBufferLength,
_Out_ PVOID OutputBuffer,
_In_ ULONG OutputBufferLength,
_In_ BOOLEAN IsInternal,
_In_ BOOLEAN bNoReboot
)
/*++

Routine Description:
    Utility routine for sending an IOCTL to a driver.
    It creates a new Device Ioctl IRP and sends to specific driver.
Arguments:

    DeviceObject        - Supplies the device object.
    IoControlCode       - Io Control Code
    InputBuffer         - Input Buffer
    InputBufferLength   - Input Buffer Length.
    OutputBuffer        - Output Buffer.
    OutputBufferLength  - Output Buffer Length.
    IsInternal          - TRUE for internal device control. FALSE otherwise.

Return Value:

NTSTATUS

--*/
{
    KEVENT          Event = { 0 };
    IO_STATUS_BLOCK IoStatus = { 0 };
    PIRP            Irp = NULL;
    NTSTATUS        Status = STATUS_SUCCESS;

    PAGED_CODE();

    KeInitializeEvent(&Event, NotificationEvent, FALSE);

    Irp = IoBuildDeviceIoControlRequest(IoControlCode,
        DeviceObject,
        InputBuffer,
        InputBufferLength,
        OutputBuffer,
        OutputBufferLength,
        IsInternal,
        &Event,
        &IoStatus);

    if (NULL == Irp)
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Cleanup;
    }

    if (bNoReboot)
    {
        IoSetNextIrpStackLocation(Irp);
        Status = DriverContext.TargetMajorFunction[IRP_MJ_DEVICE_CONTROL](DeviceObject, Irp);
    }
    else
    {
        Status = IoCallDriver(DeviceObject, Irp);
    }

    if (Status == STATUS_PENDING)
    {
        KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
        Status = IoStatus.Status;
    }

    if (NT_SUCCESS(Status) && (IoStatus.Information < OutputBufferLength))
    {
        Status = STATUS_INFO_LENGTH_MISMATCH;
    }

Cleanup:
    return Status;
}


NTSTATUS
DeviceIoControlGetLengthInfo(
_In_    PDEVICE_OBJECT   DeviceObject,
_In_    PIRP             Irp,
_In_    BOOLEAN          bNoRebootLayer
)
/*++

Routine Description:
    IOCTL_DISK_GET_LENGTH_INFO handler. This function sets a completion routine for this IOCTL.

Arguments:

DeviceObject    - Supplies the device object.

Irp             - Supplies the IO request packet.

Return Value:

NTSTATUS

--*/
{
    PDEVICE_EXTENSION           pDeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PDEV_CONTEXT                pDevContext = pDeviceExtension->pDevContext;
    NTSTATUS                    Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION          IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PGET_LENGTH_INFORMATION     pGetLengthInfo = NULL;
    GET_LENGTH_INFORMATION      getLengthInfo = { 0 };

    Irp->IoStatus.Information = 0;

    if (IrpSp->Parameters.DeviceIoControl.OutputBufferLength < sizeof(GET_LENGTH_INFORMATION))
    {
        Status = STATUS_INFO_LENGTH_MISMATCH;
        goto Cleanup;
    }

    if (bNoRebootLayer) 
    {
        Status = InDskSendDeviceControl(
                                        pDevContext->TargetDeviceObject,
                                        IOCTL_DISK_GET_LENGTH_INFO,
                                        NULL,
                                        0,
                                        &getLengthInfo,
                                        sizeof(getLengthInfo),
                                        FALSE,
                                        bNoRebootLayer);

        if (!NT_SUCCESS(Status))
        {
            InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_DEV_GET_LENGTH_INFO_FAILED, pDevContext, Status);

            return DriverContext.TargetMajorFunction[IRP_MJ_DEVICE_CONTROL](pDevContext->TargetDeviceObject, Irp);
        }

        RtlCopyMemory(Irp->AssociatedIrp.SystemBuffer, &getLengthInfo, sizeof(getLengthInfo));
        Irp->IoStatus.Information = sizeof(getLengthInfo);
    } 
    else 
    {
        IoForwardIrpSynchronously(pDevContext->TargetDeviceObject, Irp);
        Status = Irp->IoStatus.Status;
        if (!NT_SUCCESS(Status) || Irp->IoStatus.Information < sizeof(GET_LENGTH_INFORMATION))
        {
            goto Cleanup;
        }
    }

    pGetLengthInfo = (PGET_LENGTH_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
    InDskFltHandleDiskResize(pDevContext, pGetLengthInfo);

Cleanup:
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS
DeviceIoControlServiceShutdownNotify(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    KIRQL               OldIrql;
    BOOLEAN             bIrpCancelled = FALSE;
    PIRP                PriorIrp = NULL;
    SHUTDOWN_NOTIFY_INPUT   InputData;

    UNREFERENCED_PARAMETER(DeviceObject);

    ASSERT(IOCTL_INMAGE_SERVICE_SHUTDOWN_NOTIFY == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

    // Disabling the notification thread on service start
    InterlockedIncrement(&DriverContext.bootVolNotificationInfo.LockUnlockBootVolumeDisable);

    if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength >= sizeof(SHUTDOWN_NOTIFY_INPUT)) {
        InputData.ulFlags = ((PSHUTDOWN_NOTIFY_INPUT)Irp->AssociatedIrp.SystemBuffer)->ulFlags;
    } else {
        //reject the IOCTL as buffer is less than expected
		InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_DIRTY_BLOCKS,
			("DeviceIoControlServiceShutdownNotify: IOCTL rejected, buffer too small\n"));
		Status = STATUS_BUFFER_TOO_SMALL;
		Irp->IoStatus.Information = 0;
		Irp->IoStatus.Status = Status;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return Status;
    }

    InDskFltWriteEvent(INFLTDRV_INFO_SERVICE_STARTED_NOTIFICATION, InputData.ulFlags);

    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);

    IoSetCancelRoutine(Irp, InMageFltCancelServiceShutdownIrp);
    // The IoSetCancelRoutine is only executed if Irp->Cancel is true.
    if (Irp->Cancel && (NULL !=IoSetCancelRoutine(Irp, NULL)))
    {
        // Irp Already cancelled.
        bIrpCancelled = TRUE;
    }
    else
    {
        // Irp is not cancelled.
        IoMarkIrpPending(Irp);
        if (NULL != DriverContext.ServiceShutdownIrp) {
            PriorIrp = DriverContext.ServiceShutdownIrp;
        } else {
            // Change the Driver state to startup mode. Service could be starting for the first time
            // Or it could be starting after it crashed/shutdown before.
    
            DriverContext.eServiceState = ecServiceRunning;
            DriverContext.ulFlags |= DC_FLAGS_SERVICE_STATE_CHANGED;
            KeQuerySystemTime(&DriverContext.globalTelemetryInfo.liLastAgentStartTime);
            KeSetEvent(&DriverContext.ServiceThreadWakeEvent, IO_DISK_INCREMENT, FALSE);
        }

        DriverContext.ServiceShutdownIrp = Irp;
    }

    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

    if (PriorIrp)
    {
        ASSERT(FALSE == bIrpCancelled);
        if (IoSetCancelRoutine(PriorIrp, NULL))
        {
            // Cancel Routine not called.
            PriorIrp->IoStatus.Status = STATUS_SUCCESS;
            PriorIrp->IoStatus.Information = 0;
            IoCompleteRequest(PriorIrp, IO_NO_INCREMENT);
        }
    }

    if (bIrpCancelled)
    {
        Irp->IoStatus.Status = STATUS_CANCELLED;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_CANCELLED;
    }

    if (InputData.ulFlags & SHUTDOWN_NOTIFY_FLAGS_ENABLE_DATA_FILTERING) {
        DriverContext.bServiceSupportsDataCapture = true;
    }

    if (InputData.ulFlags & SHUTDOWN_NOTIFY_FLAGS_ENABLE_DATA_FILES) {
        DriverContext.bServiceSupportsDataFiles = true;
    }

    return STATUS_PENDING;
}

NTSTATUS
DeviceIoControlCommitDirtyBlocksTransaction(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status;
    PIO_STACK_LOCATION  IoStackLocation;
    PCOMMIT_TRANSACTION pCommitTransaction;
    PDEV_CONTEXT   pDevContext;

    InVolDbgPrint(DL_FUNC_TRACE, DM_INMAGE_IOCTL | DM_DIRTY_BLOCKS,
        ("DeviceIoControlCommitDirtyBlocksTransaction: Called.\n"));

    IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ASSERT(IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

    if( IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < sizeof(COMMIT_TRANSACTION) ) 
    {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_DIRTY_BLOCKS, 
            ("DeviceIoControlCommitDirtyBlocksTransaction: IOCTL rejected, buffer too small\n"));
        Status = STATUS_BUFFER_TOO_SMALL;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    pDevContext = GetDevContextForThisIOCTL(DeviceObject, Irp);
    if (NULL == pDevContext)
    {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_DIRTY_BLOCKS, 
            ("DeviceIoControlCommitDirtyBlocksTransaction: IOCTL rejected, unknown volume GUID %.*S\n",
                GUID_SIZE_IN_CHARS, (PWCHAR)Irp->AssociatedIrp.SystemBuffer));
		Status = STATUS_NO_SUCH_DEVICE;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    pCommitTransaction = (PCOMMIT_TRANSACTION)Irp->AssociatedIrp.SystemBuffer;

    KeWaitForSingleObject(&pDevContext->SyncEvent, Executive, KernelMode, FALSE, NULL); 
    Status = CommitDirtyBlockTransaction(pDevContext, pCommitTransaction);
    KeSetEvent(&pDevContext->SyncEvent, IO_NO_INCREMENT, FALSE);

    if (NT_SUCCESS(Status)) {
        UpdateCXCountersAfterCommit(pDevContext);
    }
    DereferenceDevContext(pDevContext);

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS
DeviceIoControlSetResyncRequired(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status;
    PIO_STACK_LOCATION  IoStackLocation;
    PSET_RESYNC_REQUIRED pSetResyncRequired = (PSET_RESYNC_REQUIRED)Irp->AssociatedIrp.SystemBuffer;
    PDEV_CONTEXT   pDevContext;

    InVolDbgPrint(DL_FUNC_TRACE, DM_INMAGE_IOCTL | DM_DIRTY_BLOCKS,
        ("DeviceIoControlSetResyncRequired: Called.\n"));

    IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ASSERT(IOCTL_INMAGE_SET_RESYNC_REQUIRED == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

    if( IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < sizeof(SET_RESYNC_REQUIRED) ) 
    {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_DIRTY_BLOCKS, 
            ("DeviceIoControlSetResyncRequired: IOCTL rejected, buffer too small\n"));
        Status = STATUS_BUFFER_TOO_SMALL;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    if (0 == pSetResyncRequired->ulOutOfSyncErrorCode) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_DIRTY_BLOCKS, 
            ("DeviceIoControlSetResyncRequired: IOCTL rejected, ulOutOfSyncErrorCode is 0\n"));
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    pDevContext = GetDevContextForThisIOCTL(DeviceObject, Irp);
    if (NULL == pDevContext)
    {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_DIRTY_BLOCKS, 
            ("DeviceIoControlCommitDirtyBlocksTransaction: IOCTL rejected, unknown volume GUID %.*S\n",
                GUID_SIZE_IN_CHARS, (PWCHAR)Irp->AssociatedIrp.SystemBuffer));
		Status = STATUS_NO_SUCH_DEVICE;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    SetDevOutOfSync(pDevContext, pSetResyncRequired->ulOutOfSyncErrorCode, pSetResyncRequired->ulOutOfSyncErrorStatus, false);
    Status = STATUS_SUCCESS;

    DereferenceDevContext(pDevContext);

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS
CopyKDirtyBlockToUDirtyBlock(
    PDEV_CONTEXT DevContext,
    PKDIRTY_BLOCK   pDirtyBlock,
    PUDIRTY_BLOCK_V2   pUserDirtyBlock
    )
{
    NTSTATUS    Status = STATUS_SUCCESS;
    ULONGLONG *TmpChangeOffsetArray = NULL;
    ULONG *TmpChangeLengthArray = NULL;
    ULONG *TmpTimeDeltaArray = NULL;
    ULONG *TmpSequenceNumberDeltaArray = NULL;
    ULONG MaxChanges = 0;

    ASSERT(NULL != pDirtyBlock);
    ASSERT(NULL != pDirtyBlock->ChangeNode);
    ASSERT(NULL != pDirtyBlock->DevContext);

    PCHANGE_NODE    ChangeNode = pDirtyBlock->ChangeNode;

    Status = KeWaitForSingleObject(&ChangeNode->Mutex, Executive, KernelMode, FALSE, NULL);
    ASSERT(STATUS_SUCCESS == Status);
    Status = KeWaitForSingleObject(&DevContext->Mutex, Executive, KernelMode, FALSE, NULL);
    ASSERT(STATUS_SUCCESS == Status);

    // Instead of copying the whole dirty blocks just copy the required data.
    pUserDirtyBlock->uHdr.Hdr.ulFlags = 0;
    pUserDirtyBlock->uHdr.Hdr.uliTransactionID.QuadPart = pDirtyBlock->uliTransactionID.QuadPart;
    pUserDirtyBlock->uHdr.Hdr.ulicbChanges.QuadPart = pDirtyBlock->ulicbChanges.QuadPart;
    pUserDirtyBlock->uHdr.Hdr.cChanges = pDirtyBlock->cChanges;
    pUserDirtyBlock->uHdr.Hdr.ppBufferArray = NULL;
    pUserDirtyBlock->uHdr.Hdr.ulSequenceIDforSplitIO = pDirtyBlock->ulSequenceIdForSplitIO;
    pUserDirtyBlock->uHdr.Hdr.ulDataSource = pDirtyBlock->ulDataSource;
    pUserDirtyBlock->uHdr.Hdr.eWOState = pDirtyBlock->eWOState;

    if (pDirtyBlock->ulFlags & KDIRTY_BLOCK_FLAG_SPLIT_CHANGE_MASK) {
        if (pDirtyBlock->ulFlags & KDIRTY_BLOCK_FLAG_START_OF_SPLIT_CHANGE) {
            pUserDirtyBlock->uHdr.Hdr.ulFlags |= UDIRTY_BLOCK_FLAG_START_OF_SPLIT_CHANGE;
        } else if (pDirtyBlock->ulFlags & KDIRTY_BLOCK_FLAG_PART_OF_SPLIT_CHANGE) {
            pUserDirtyBlock->uHdr.Hdr.ulFlags |= UDIRTY_BLOCK_FLAG_PART_OF_SPLIT_CHANGE;
        } else if (pDirtyBlock->ulFlags & KDIRTY_BLOCK_FLAG_END_OF_SPLIT_CHANGE) {
            pUserDirtyBlock->uHdr.Hdr.ulFlags |= UDIRTY_BLOCK_FLAG_END_OF_SPLIT_CHANGE;
        }
    }

    if (ecChangeNodeDataFile == ChangeNode->eChangeNode) {
        ASSERT(0 == (ChangeNode->ulFlags & CHANGE_NODE_FLAGS_QUEUED_FOR_DATA_WRITE));
        // This is a data file
        pUserDirtyBlock->uHdr.Hdr.ulFlags |= UDIRTY_BLOCK_FLAG_DATA_FILE;
        ASSERT(pDirtyBlock->FileName.Length < UDIRTY_BLOCK_MAX_FILE_NAME);
        pUserDirtyBlock->uTagList.DataFile.usLength = pDirtyBlock->FileName.Length;
        ASSERT(NULL != pDirtyBlock->FileName.Buffer);
        RtlMoveMemory(pUserDirtyBlock->uTagList.DataFile.FileName, pDirtyBlock->FileName.Buffer,
                      pDirtyBlock->FileName.Length);
        pUserDirtyBlock->uTagList.DataFile.FileName[pDirtyBlock->FileName.Length/sizeof(WCHAR)] = L'\0';
        DevContext->ullDataFilesReturned++;
    } else {

        if(ChangeNode->ulFlags & CHANGE_NODE_FLAGS_QUEUED_FOR_DATA_WRITE) {
            KIRQL   OldIrql;

            KeAcquireSpinLock(&DevContext->Lock, &OldIrql);
            DevContext->lDataBlocksQueuedToFileWrite -= ChangeNode->DirtyBlock->ulDataBlocksAllocated;
            KeReleaseSpinLock(&DevContext->Lock, OldIrql);
        }

        ChangeNode->ulFlags &= ~CHANGE_NODE_FLAGS_QUEUED_FOR_DATA_WRITE;

        FILL_STREAM_HEADER_4B(&pUserDirtyBlock->uTagList.TagList.TagStartOfList, STREAM_REC_TYPE_START_OF_TAG_LIST, sizeof(STREAM_REC_HDR_4B));
        FILL_STREAM_HEADER_4B(&pUserDirtyBlock->uTagList.TagList.TagPadding, STREAM_REC_TYPE_PADDING, sizeof(STREAM_REC_HDR_4B));
        FILL_STREAM_HEADER_4B(&pUserDirtyBlock->uTagList.TagList.TagDataSource, STREAM_REC_TYPE_DATA_SOURCE, sizeof(DATA_SOURCE_TAG));

        // Copy Tags.
        Status = CopyStreamRecord(&pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange, 
                                    &pDirtyBlock->TagTimeStampOfFirstChange, sizeof(TIME_STAMP_TAG_V2));
        ASSERT(STATUS_SUCCESS == Status);
        Status = CopyStreamRecord(&pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange, 
                                    &pDirtyBlock->TagTimeStampOfLastChange, sizeof(TIME_STAMP_TAG_V2));
        ASSERT(STATUS_SUCCESS == Status);

        if (DriverContext.ulValidationLevel >= ADD_TO_EVENT_LOG) {
            if (pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.ullSequenceNumber <
                        pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.ullSequenceNumber) {
                InDskFltWriteEvent(INDSKFLT_ERROR_FIRST_LAST_SEQUENCE_MISMATCH,
                    pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.ullSequenceNumber,
                    pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.ullSequenceNumber,
                    SourceLocationCopyK2uDBPreCopy);
            }
        }
        ASSERT_CHECK(pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.ullSequenceNumber >=
                        pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.ullSequenceNumber, 0);

        if (DriverContext.ulValidationLevel >= ADD_TO_EVENT_LOG) {
            if (pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601 <
                        pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601) {
                InDskFltWriteEvent(INDSKFLT_ERROR_FIRST_LAST_TIME_MISMATCH,
                    pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601,
                    pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
                    SourceLocationCopyK2uDBPreCopy);
            }
        }
        ASSERT_CHECK(pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601 >=
                        pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601, 0);

        pUserDirtyBlock->uTagList.TagList.TagDataSource.ulDataSource = pDirtyBlock->ulDataSource;

        if ((NULL != pDirtyBlock->TagBuffer) && (0 != pDirtyBlock->ulTagBufferSize)) {
            // Copy all tag records.
            RtlCopyMemory(&pUserDirtyBlock->uTagList.TagList.TagEndOfList, pDirtyBlock->TagBuffer, pDirtyBlock->ulTagBufferSize);
            PSTREAM_REC_HDR_4B *TagEndOfList = (PSTREAM_REC_HDR_4B *)((PUCHAR)&pUserDirtyBlock->uTagList.TagList.TagEndOfList + pDirtyBlock->ulTagBufferSize);
            FILL_STREAM_HEADER_4B(TagEndOfList, STREAM_REC_TYPE_END_OF_TAG_LIST, sizeof(STREAM_REC_HDR_4B));

            ASSERT(((PUCHAR)TagEndOfList + sizeof(STREAM_REC_HDR_4B)) <= (PUCHAR)pUserDirtyBlock->ChangeOffsetArray);
            // S2 expects tags in DirtyBlock header while data source is Metadata, driver still treats it's source as DATA
            ASSERT(TRUE == IsListEmpty(&pDirtyBlock->DataBlockList));
            ASSERT(pUserDirtyBlock->uTagList.TagList.TagDataSource.ulDataSource == INFLTDRV_DATA_SOURCE_DATA);
            if (pUserDirtyBlock->uTagList.TagList.TagDataSource.ulDataSource == INFLTDRV_DATA_SOURCE_DATA)
                pUserDirtyBlock->uTagList.TagList.TagDataSource.ulDataSource = INFLTDRV_DATA_SOURCE_META_DATA;
            // S2 does not know about this added for debugging
#if DBG
            SET_FLAG(pUserDirtyBlock->uHdr.Hdr.ulFlags, UDIRTY_BLOCK_FLAG_CONTAINS_TAG);
#endif
        } else {
            FILL_STREAM_HEADER_4B(&pUserDirtyBlock->uTagList.TagList.TagEndOfList, STREAM_REC_TYPE_END_OF_TAG_LIST, sizeof(STREAM_REC_HDR_4B));
        }

        // ProcessStartIrp is set to NULL in initial state of cancellation and after all mapped memory
        // is cleaned up UserProcess is set to NULL.
        if ((INFLTDRV_DATA_SOURCE_DATA == pDirtyBlock->ulDataSource) &&
            (NULL != DriverContext.ProcessStartIrp) && (NULL != DriverContext.UserProcess) &&
            (!IsListEmpty(&pDirtyBlock->DataBlockList)) )
        {
            ASSERT(DriverContext.UserProcess == IoGetCurrentProcess());
            ASSERT(0 != pDirtyBlock->ulcbDataUsed);
#ifdef _WIN64
            if (DriverContext.bS2is64BitProcess) {
                Status = PrepareDirtyBlockForUsermodeUse(pDirtyBlock, (HANDLE)DevContext);
            } else {
                Status = PrepareDirtyBlockFor32bitUsermodeUse(pDirtyBlock, (HANDLE)DevContext);
            }
#else
            Status = PrepareDirtyBlockForUsermodeUse(pDirtyBlock, (HANDLE)DevContext);
#endif // _WIN64
            if (STATUS_SUCCESS == Status) {
                USHORT usMaxDataBlocksInDirtyBlock = (USHORT) (pDirtyBlock->ulMaxDataSizePerDirtyBlock / DriverContext.ulDataBlockSize);
                pUserDirtyBlock->uHdr.Hdr.usNumberOfBuffers = (USHORT)pDirtyBlock->ulDataBlocksAllocated;
                pUserDirtyBlock->uHdr.Hdr.usMaxNumberOfBuffers = usMaxDataBlocksInDirtyBlock;
                pUserDirtyBlock->uHdr.Hdr.ulFlags |= UDIRTY_BLOCK_FLAG_SVD_STREAM;
                pUserDirtyBlock->uHdr.Hdr.ulBufferSize = DriverContext.ulDataBlockSize;
                pUserDirtyBlock->uHdr.Hdr.ulcbChangesInStream = 0;

                if (!IsListEmpty(&pDirtyBlock->DataBlockList)) {
                    PDATA_BLOCK DataBlock = (PDATA_BLOCK)pDirtyBlock->DataBlockList.Flink;    
                    pUserDirtyBlock->uHdr.Hdr.ppBufferArray = DataBlock->pUserBufferArray;
                    // 64BIT : As buffer array size would never be more than 4GB, casting to ULONG
                    pUserDirtyBlock->uHdr.Hdr.ulcbBufferArraySize = (ULONG)DataBlock->szcbBufferArraySize;
                    while ((PLIST_ENTRY)DataBlock != &pDirtyBlock->DataBlockList) {
                        pUserDirtyBlock->uHdr.Hdr.ulcbChangesInStream += DataBlock->ulcbMaxData - DataBlock->ulcbDataFree;
                        DataBlock = (PDATA_BLOCK)DataBlock->ListEntry.Flink;
                    }
                } else {
                    pUserDirtyBlock->uHdr.Hdr.ppBufferArray = NULL;
                    pUserDirtyBlock->uHdr.Hdr.ulcbBufferArraySize = 0;
                }

                ASSERT(pUserDirtyBlock->uHdr.Hdr.usNumberOfBuffers <= pUserDirtyBlock->uHdr.Hdr.usMaxNumberOfBuffers);
            }
        } // DataSource is Data

        ASSERT(0 != pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
        ASSERT(0 != pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601);
    }

    if (pDirtyBlock->cChanges) {
        if (INFLTDRV_DATA_SOURCE_DATA == pDirtyBlock->ulDataSource) {
            TmpChangeOffsetArray = (ULONGLONG *) ExAllocatePoolWithTag(PagedPool,
                                                                            pDirtyBlock->cChanges * sizeof (LONGLONG),
                                                                            TAG_GENERIC_PAGED);
            TmpChangeLengthArray = (ULONG *) ExAllocatePoolWithTag(PagedPool,
                                                                            pDirtyBlock->cChanges * sizeof (ULONG),
                                                                            TAG_GENERIC_PAGED);
            TmpTimeDeltaArray = (ULONG *) ExAllocatePoolWithTag(PagedPool,
                                                                            pDirtyBlock->cChanges * sizeof (ULONG),
                                                                            TAG_GENERIC_PAGED);
            TmpSequenceNumberDeltaArray = (ULONG *) ExAllocatePoolWithTag(PagedPool,
                                                                            pDirtyBlock->cChanges * sizeof (ULONG),
                                                                            TAG_GENERIC_PAGED);

            if (TmpChangeOffsetArray == NULL || TmpChangeLengthArray == NULL ||
                TmpTimeDeltaArray == NULL || TmpSequenceNumberDeltaArray == NULL) {
                goto Out1;
            }
           MaxChanges = pDirtyBlock->cChanges;
        } else {
            TmpChangeOffsetArray = pUserDirtyBlock->ChangeOffsetArray;
            TmpChangeLengthArray = pUserDirtyBlock->ChangeLengthArray;
            TmpTimeDeltaArray = pUserDirtyBlock->TimeDeltaArray;
            TmpSequenceNumberDeltaArray = pUserDirtyBlock->SequenceNumberDeltaArray;
            MaxChanges = MAX_UDIRTY_CHANGES_V2;
        }

        CopyChangeMetaData(pDirtyBlock,
                           TmpChangeOffsetArray,
                           TmpChangeLengthArray,
                           MaxChanges,
                           TmpTimeDeltaArray,
                           TmpSequenceNumberDeltaArray);

        ULONG i = 0;
        for (i = 1; i < pDirtyBlock->cChanges; i++) {
            if (DriverContext.ulValidationLevel >= ADD_TO_EVENT_LOG) {
                if (TmpTimeDeltaArray[i - 1] > TmpTimeDeltaArray[i]) {
                    InDskFltWriteEvent(INDSKFLT_ERROR_FIRST_NEXT_TIMEDELTA_MISMATCH,
                        TmpTimeDeltaArray[i - 1],
                        i -1,
                        TmpTimeDeltaArray[i],
                        i);
                }
            }            
            ASSERT_CHECK(TmpTimeDeltaArray[i - 1] <=
                                        TmpTimeDeltaArray[i], 0);

            if (DriverContext.ulValidationLevel >= ADD_TO_EVENT_LOG) {
                if (TmpSequenceNumberDeltaArray[i - 1] > TmpSequenceNumberDeltaArray[i]) {
                    InDskFltWriteEvent(INDSKFLT_ERROR_FIRST_NEXT_SEQDELTA_MISMATCH,
                        TmpSequenceNumberDeltaArray[i - 1],
                        i -1,
                        TmpSequenceNumberDeltaArray[i],
                        i);
                }   
            }            
            ASSERT_CHECK(TmpSequenceNumberDeltaArray[i - 1] <=
                                        TmpSequenceNumberDeltaArray[i], 0);
        }

        if (ecChangeNodeDataFile != ChangeNode->eChangeNode) {
            ULONGLONG LastSeqNum =
                pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.ullSequenceNumber +
                TmpSequenceNumberDeltaArray[pDirtyBlock->cChanges - 1];
            if (DriverContext.ulValidationLevel >= ADD_TO_EVENT_LOG) {
                if (LastSeqNum != pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.ullSequenceNumber) {
                    InDskFltWriteEvent(INDSKFLT_ERROR_LAST_SEQUENCE_GENERATION,
                        pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.ullSequenceNumber,
                        TmpSequenceNumberDeltaArray[pDirtyBlock->cChanges - 1],
                        pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.ullSequenceNumber,
                        SourceLocationCopyK2uDBPostCopyNonDataFile);
                }
            }
            ASSERT_CHECK(LastSeqNum ==
                pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.ullSequenceNumber, 0);
        } else {
            ULONGLONG LastSeqNum =
                pDirtyBlock->TagTimeStampOfFirstChange.ullSequenceNumber +
                TmpSequenceNumberDeltaArray[pDirtyBlock->cChanges - 1];
            if (DriverContext.ulValidationLevel >= ADD_TO_EVENT_LOG) {
                if (LastSeqNum != pDirtyBlock->TagTimeStampOfLastChange.ullSequenceNumber) {
                    InDskFltWriteEvent(INDSKFLT_ERROR_LAST_SEQUENCE_GENERATION,
                        pDirtyBlock->TagTimeStampOfLastChange.ullSequenceNumber,
                        TmpSequenceNumberDeltaArray[pDirtyBlock->cChanges - 1],
                        pDirtyBlock->TagTimeStampOfFirstChange.ullSequenceNumber,
                        SourceLocationCopyK2uDBPostCopyDataFile);
                }
            }
            ASSERT_CHECK(LastSeqNum == pDirtyBlock->TagTimeStampOfLastChange.ullSequenceNumber, 0);
        }
    }

Out1:

    if (INFLTDRV_DATA_SOURCE_DATA == pDirtyBlock->ulDataSource) {
        if (TmpChangeOffsetArray != NULL)
            ExFreePool(TmpChangeOffsetArray);
        if (TmpChangeLengthArray != NULL)
            ExFreePool(TmpChangeLengthArray);
        if (TmpTimeDeltaArray != NULL)
            ExFreePool(TmpTimeDeltaArray);
        if (TmpSequenceNumberDeltaArray != NULL)
            ExFreePool(TmpSequenceNumberDeltaArray);
    }
    
    
    KeReleaseMutex(&DevContext->Mutex, FALSE);
    KeReleaseMutex(&ChangeNode->Mutex, FALSE);

    return Status;
}

NTSTATUS
CommitAndFillBufferWithDirtyBlock(
    PCOMMIT_TRANSACTION pCommitTransaction,
    PDEV_CONTEXT DevContext,
    PVOID           pBuffer,
    ULONG           ulBufferLen
    )
{
    KIRQL               OldIrql;
    PKDIRTY_BLOCK       DirtyBlock = NULL;
    PUDIRTY_BLOCK_V2    pUserDirtyBlock = (PUDIRTY_BLOCK_V2)pBuffer;
    BOOLEAN             bCommitFailed = FALSE;
    ULONG               ulTotalChangesPending = 0;
    ULONGLONG           ullcbTotalChangesPending = 0;
    NTSTATUS            Status = STATUS_SUCCESS;
    NTSTATUS            CommitStatus = STATUS_SUCCESS;
    PDATA_BLOCK         DataBlock = NULL;
    BOOLEAN             bRevokeTagDirtyBlock = FALSE;

#ifndef VOLUME_CLUSTER_SUPPORT
    UNREFERENCED_PARAMETER(ulBufferLen);
#else
#if !DBG
    UNREFERENCED_PARAMETER(ulBufferLen);
#endif
    if (IS_VOLUME_OFFLINE(DevContext)) {
        ASSERT(ulBufferLen >= sizeof(UDIRTY_BLOCK_V2));
        RtlZeroMemory(pBuffer, sizeof(UDIRTY_BLOCK_V2));
        return STATUS_DEVICE_OFF_LINE;
    }
#endif
    if(DevContext->bQueueChangesToTempQueue) {
        InVolDbgPrint(DL_ERROR, DM_REGISTRY,("%s: Currently the dirty blocks are in Temp Queue. Will not return any Dirty block\n", __FUNCTION__));
        return STATUS_DEVICE_BUSY;
    }

    //
    // Return dirty block information
    // TODO: make external DIRTYBLOCKS structure not contain a next pointer; use a DIRTYBLOCKSNODE structure internally
    //

    if (NULL != pCommitTransaction) {
        CommitStatus = CommitDirtyBlockTransaction(DevContext, pCommitTransaction);
        if (STATUS_SUCCESS != CommitStatus)
            bCommitFailed = TRUE;
    }

    DataBlock = AllocateLockedDataBlock(TRUE);
    PCHANGE_NODE  ChangeNode = NULL;

    do {
        bRevokeTagDirtyBlock = FALSE;
        ChangeNode = NULL;
        DirtyBlock = NULL;
        KeAcquireSpinLock(&DevContext->Lock, &OldIrql);

        if (DevContext->pDBPendingTransactionConfirm) {
            DirtyBlock = DevContext->pDBPendingTransactionConfirm;
            ASSERT(DevContext->ChangeList.ulTotalChangesPending >= DirtyBlock->cChanges);
            ASSERT(DevContext->ChangeList.ullcbTotalChangesPending >= DirtyBlock->ulicbChanges.QuadPart);
            ulTotalChangesPending = DevContext->ChangeList.ulTotalChangesPending - DirtyBlock->cChanges;
            ullcbTotalChangesPending = DevContext->ChangeList.ullcbTotalChangesPending - DirtyBlock->ulicbChanges.QuadPart;
            DevContext->ulicbChangesReverted.QuadPart += DirtyBlock->ulicbChanges.QuadPart;
            DevContext->ulChangesReverted += DirtyBlock->cChanges;
            DevContext->DiskTelemetryInfo.ullRevertedDBCount++;
            ASSERT(NULL != DirtyBlock->ChangeNode);
            if (ecChangeNodeDataFile == DirtyBlock->ChangeNode->eChangeNode) {
                DevContext->ulDataFilesReverted++;
                DevContext->ullDataFilesReturned--;
            }
        } else if (DevContext->ulPendingTSOTransactionID.QuadPart) {
            RtlCopyMemory(&pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange, &DevContext->PendingTsoFirstChangeTS,
                sizeof(TIME_STAMP_TAG_V2));
            RtlCopyMemory(&pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange, &DevContext->PendingTsoLastChangeTS,
                sizeof(TIME_STAMP_TAG_V2));
            pUserDirtyBlock->uTagList.TagList.TagDataSource.ulDataSource = DevContext->ulPendingTsoDataSource;
            pUserDirtyBlock->uHdr.Hdr.ulDataSource = DevContext->ulPendingTsoDataSource;
            pUserDirtyBlock->uHdr.Hdr.eWOState = DevContext->ulPendingTsoWOState;
            pUserDirtyBlock->uHdr.Hdr.uliTransactionID.QuadPart = DevContext->ulPendingTSOTransactionID.QuadPart;
            DevContext->DiskTelemetryInfo.ullRevertedDBCount++;
        } else {
            ChangeNode = GetChangeNodeFromChangeList(DevContext);

            if (NULL != ChangeNode) {
                DirtyBlock = ChangeNode->DirtyBlock;

                // Check revoke flag and mark it for cleanup
                // check pending flag and retry
                if ((DirtyBlock->ulFlags & KDIRTY_BLOCK_FLAG_CONTAINS_TAGS) && (DirtyBlock->ulFlags & KDIRTY_BLOCK_FLAG_NO_COMMIT_TAGS_MASK)) {

                    DereferenceChangeNode(ChangeNode);
                    ASSERT((ecWriteOrderStateData == DirtyBlock->eWOState) &&
                        (INFLTDRV_DATA_SOURCE_DATA == DirtyBlock->ulDataSource));

                    if (DirtyBlock->ulFlags & KDIRTY_BLOCK_FLAG_REVOKE_TAGS) {
                        // Fully WO state dirtyblocks are always ordered
                        // We allow tags in Data WOS and this is the first one in Main Q
                        bRevokeTagDirtyBlock = TRUE;
                        DevContext->ulRevokeTagDBCount++;
                    } 
                    else if (DirtyBlock->ulFlags & KDIRTY_BLOCK_FLAG_TAGS_PENDING) {
                        // prepend tag node as it is still in pending state
                        PrependChangeNodeToDevContext(DevContext, DirtyBlock->ChangeNode);
                        ChangeNode = NULL;
                        if ((NULL != DirtyBlock->pTagHistory) && (0 == DirtyBlock->pTagHistory->FirstGetDbTimeOnDrainBlk.QuadPart)) {
                            KeQuerySystemTime(&DirtyBlock->pTagHistory->FirstGetDbTimeOnDrainBlk);
                        }
                        KeReleaseSpinLock(&DevContext->Lock, OldIrql);
                        Status = STATUS_RETRY;
                        break;
                    }
                } else {
                    // Add it to pending list.
                    ASSERT(NULL == DevContext->pDBPendingTransactionConfirm);
                    DevContext->pDBPendingTransactionConfirm = DirtyBlock;
                    DevContext->lDirtyBlocksInPendingCommitQueue++;

                    DevContext->ulChangesPendingCommit += DirtyBlock->cChanges;
                    DevContext->ullcbChangesPendingCommit += DirtyBlock->ulicbChanges.QuadPart;
                    ulTotalChangesPending = DevContext->ChangeList.ulTotalChangesPending - DirtyBlock->cChanges;
                    ullcbTotalChangesPending = DevContext->ChangeList.ullcbTotalChangesPending - DirtyBlock->ulicbChanges.QuadPart;
                    ASSERT((INFLTDRV_DATA_SOURCE_DATA == DirtyBlock->ulDataSource) || (DirtyBlock->eWOState != ecWriteOrderStateData));

                    if ((INFLTDRV_DATA_SOURCE_DATA == DirtyBlock->ulDataSource) &&
                        (DirtyBlock->ulcbDataFree))
                    {
                        ULONG ulTrailerSize = SetSVDLastChangeTimeStamp(DirtyBlock);
                        VCAdjustCountersForLostDataBytes(DevContext,
                            &DataBlock,
                            DirtyBlock->ulcbDataFree + ulTrailerSize);
                        DirtyBlock->ulcbDataFree = 0;
                    }

                // These fields needed to figure out lag between source and target in terms of time 
                if ((ecWriteOrderStateMetadata == DirtyBlock->eWOState) || (ecWriteOrderStateBitmap == DirtyBlock->eWOState)) {
                    DevContext->MDFirstChangeTS = DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601;
                }
                DereferenceChangeNode(ChangeNode);
            }
        } // Handling for valid change node
        else {
            ASSERT(0 == DevContext->ulPendingTSOTransactionID.QuadPart);
            InVolDbgPrint(DL_INFO, DM_DIRTY_BLOCKS,
                ("DevContext = %#p, No Changes - ulTotalChangesPending = %#lx\n",
                DevContext, ulTotalChangesPending));
            ASSERT((0 == DevContext->ChangeList.ulTotalChangesPending) && (0 == DevContext->ChangeList.ullcbTotalChangesPending));

                GenerateTimeStampTag(&pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange);
                GenerateTimeStampTag(&pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange);
                pUserDirtyBlock->uTagList.TagList.TagDataSource.ulDataSource = CurrentDataSource(DevContext);
                pUserDirtyBlock->uHdr.Hdr.ulDataSource = CurrentDataSource(DevContext);
                pUserDirtyBlock->uHdr.Hdr.eWOState = DevContext->eWOState;
                pUserDirtyBlock->uHdr.Hdr.uliTransactionID.QuadPart = DevContext->uliTransactionId.QuadPart++;
                // Let's keep a copy of these values to be used later in case of TSO commit failure
                RtlCopyMemory(&DevContext->PendingTsoFirstChangeTS, &pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange,
                    sizeof(TIME_STAMP_TAG_V2));
                RtlCopyMemory(&DevContext->PendingTsoLastChangeTS, &pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange,
                    sizeof(TIME_STAMP_TAG_V2));
                DevContext->ulPendingTsoDataSource = pUserDirtyBlock->uHdr.Hdr.ulDataSource;
                DevContext->ulPendingTsoWOState = pUserDirtyBlock->uHdr.Hdr.eWOState;
                DevContext->ulPendingTSOTransactionID.QuadPart = pUserDirtyBlock->uHdr.Hdr.uliTransactionID.QuadPart;

            } // Handling for TSO

        } // Handling for GetChangeNode()

        if (FALSE == bRevokeTagDirtyBlock) {
            KeQuerySystemTime(&DevContext->liGetDirtyBlockTimeStamp);

            if (NULL != DirtyBlock) {
                // code refactored for readability
                UpdateNotifyAndDataRetrievalStats(DevContext, DirtyBlock);
            }
        } 
        KeReleaseSpinLock(&DevContext->Lock, OldIrql);

        if (TRUE == bRevokeTagDirtyBlock) {
            // Undo what ever required to remove dirty block
            ASSERT((DirtyBlock->ulFlags & KDIRTY_BLOCK_FLAG_REVOKE_TAGS));
            ChangeNodeCleanup(DirtyBlock->ChangeNode);
            //ASSERT(NULL == DirtyBlock);
        }
      // Get Next DirtyBlock	
    } while (TRUE == bRevokeTagDirtyBlock);

    if (NULL != DataBlock) {
        AddDataBlockToLockedDataBlockList(DataBlock);
        DataBlock = NULL;
    }

    if (STATUS_RETRY == Status) {
        ASSERT(NULL == ChangeNode);
        return Status;
    }
    DevContext->DiskTelemetryInfo.ullDrainDBCount++;

    if (NULL == DirtyBlock) {
        int size = sizeof(DATA_SOURCE_TAG);
       // Instead of zeoring the whole buffer just set the next and cChanges.
       // These are set when DevContext->Lock is held, do not set them again.
       // GenerateTimeStampTag(&pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange);
       // GenerateTimeStampTag(&pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange);
       // pUserDirtyBlock->uTagList.TagList.TagDataSource.ulDataSource = CurrentDataSource(DevContext);
       pUserDirtyBlock->uHdr.Hdr.cChanges = 0;
       pUserDirtyBlock->uHdr.Hdr.ulFlags = 0;
       pUserDirtyBlock->uHdr.Hdr.ulFlags |= UDIRTY_BLOCK_FLAG_TSO_FILE;
       pUserDirtyBlock->uHdr.Hdr.ulicbChanges.QuadPart = 0;
       pUserDirtyBlock->uHdr.Hdr.ppBufferArray = NULL;
       pUserDirtyBlock->uHdr.Hdr.ulSequenceIDforSplitIO = 1;
       DevContext->TSOSequenceId = pUserDirtyBlock->uHdr.Hdr.ulSequenceIDforSplitIO;
       DevContext->TSOEndTimeStamp = pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601;
       DevContext->TSOEndSequenceNumber = pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.ullSequenceNumber;
       FILL_STREAM_HEADER_4B(&pUserDirtyBlock->uTagList.TagList.TagStartOfList, STREAM_REC_TYPE_START_OF_TAG_LIST, sizeof(STREAM_REC_HDR_4B));
       FILL_STREAM_HEADER_4B(&pUserDirtyBlock->uTagList.TagList.TagPadding, STREAM_REC_TYPE_PADDING, sizeof(STREAM_REC_HDR_4B));
       FILL_STREAM_HEADER_4B(&pUserDirtyBlock->uTagList.TagList.TagEndOfList, STREAM_REC_TYPE_END_OF_TAG_LIST, sizeof(STREAM_REC_HDR_4B));
       FILL_STREAM_HEADER(&pUserDirtyBlock->uTagList.TagList.TagDataSource, STREAM_REC_TYPE_DATA_SOURCE, size);
    } else {
        Status = CopyKDirtyBlockToUDirtyBlock(DevContext, DirtyBlock, pUserDirtyBlock);
    }

    if (DevContext->LastCommittedTimeStamp > 0 && DevContext->LastCommittedTimeStamp < MAX_PREV_TIMESTAMP) {
        if ((DirtyBlock == NULL) || (DirtyBlock != NULL && ecChangeNodeDataFile != DirtyBlock->ChangeNode->eChangeNode)) {
           ASSERT((DevContext->LastCommittedTimeStamp < pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601) ||
                (((DevContext->LastCommittedTimeStamp == pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601)&&
                 (DevContext->LastCommittedSequenceNumber < pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.ullSequenceNumber)) ||
                 DevContext->LastCommittedSplitIoSeqId < pUserDirtyBlock->uHdr.Hdr.ulSequenceIDforSplitIO));
        }
    }

    // Set global values like ulTotalChangesPending and commit failed.
    pUserDirtyBlock->uHdr.Hdr.ulTotalChangesPending = ulTotalChangesPending;
    pUserDirtyBlock->uHdr.Hdr.ulicbTotalChangesPending.QuadPart = ullcbTotalChangesPending;
    pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp = DevContext->LastCommittedTimeStamp;
    pUserDirtyBlock->uHdr.Hdr.ullPrevEndSequenceNumber = DevContext->LastCommittedSequenceNumber;
    pUserDirtyBlock->uHdr.Hdr.ulPrevSequenceIDforSplitIO = DevContext->LastCommittedSplitIoSeqId;

    if (DirtyBlock != NULL && ecChangeNodeDataFile == DirtyBlock->ChangeNode->eChangeNode) {
        if (DriverContext.ulValidationLevel >= ADD_TO_EVENT_LOG) {
            if (pUserDirtyBlock->uHdr.Hdr.ullPrevEndSequenceNumber > 
                        DirtyBlock->TagTimeStampOfFirstChange.ullSequenceNumber) {
                InDskFltWriteEvent(INDSKFLT_ERROR_PREV_CUR_SEQUENCE_MISMATCH,
                    pUserDirtyBlock->uHdr.Hdr.ullPrevEndSequenceNumber,
                    DirtyBlock->TagTimeStampOfFirstChange.ullSequenceNumber,
                    SourceLocationCommitFillDBDataFile);
            }   
        }
        if (pUserDirtyBlock->uHdr.Hdr.ullPrevEndSequenceNumber < MAX_PREV_SEQNUMBER) {
            ASSERT_CHECK(pUserDirtyBlock->uHdr.Hdr.ullPrevEndSequenceNumber <= 
                    DirtyBlock->TagTimeStampOfFirstChange.ullSequenceNumber, 0);
        }

        if (DriverContext.ulValidationLevel >= ADD_TO_EVENT_LOG) {
            if (pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp >
                        DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601) {
                InDskFltWriteEvent(INDSKFLT_ERROR_PREV_CUR_TIME_MISMATCH,
                    pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp,
                    DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601,
                    SourceLocationCommitFillDBDataFile);
            }   
        }
        if (pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp < MAX_PREV_TIMESTAMP) {
            ASSERT_CHECK(pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp <=
                DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601, 0);
        }
    }
    else {
        if (pUserDirtyBlock->uHdr.Hdr.ullPrevEndSequenceNumber > 
                        pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.ullSequenceNumber) {
                InDskFltWriteEvent(INDSKFLT_ERROR_PREV_CUR_SEQUENCE_MISMATCH,
                    pUserDirtyBlock->uHdr.Hdr.ullPrevEndSequenceNumber,
                    pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.ullSequenceNumber,
                    SourceLocationCommitFillDBNonDataFile);
                SetDevOutOfSync(DevContext, MSG_INDSKFLT_ERROR_PREV_CUR_SEQUENCE_MISMATCH_Message, STATUS_INVALID_DEVICE_STATE, false);
        }

        if (pUserDirtyBlock->uHdr.Hdr.ullPrevEndSequenceNumber < MAX_PREV_SEQNUMBER) {
            ASSERT_CHECK(pUserDirtyBlock->uHdr.Hdr.ullPrevEndSequenceNumber <= 
                    pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.ullSequenceNumber, 0);
        }

        if (pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp >
                    pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601) {
            InDskFltWriteEvent(INDSKFLT_ERROR_PREV_CUR_TIME_MISMATCH,
                pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp,
                pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601,
                SourceLocationCommitFillDBNonDataFile);
            SetDevOutOfSync(DevContext, MSG_INDSKFLT_ERROR_PREV_CUR_SEQUENCE_MISMATCH_Message, STATUS_INVALID_DEVICE_STATE, false);
    }

        if (pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp < MAX_PREV_TIMESTAMP) {
            ASSERT_CHECK(pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp <=
                pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601, 0);
        }
    }


    // 32 bit and 64 bit => done => checking only TimeStamps
    // Data file mode and other modes => done
    // TimeStamp and Seq# => done => checking only TimeStamps
    if (DriverContext.ulValidationLevel >= ADD_TO_EVENT_LOG) {
        if (DirtyBlock != NULL && ecChangeNodeDataFile == DirtyBlock->ChangeNode->eChangeNode) {
            if (DevContext->bResyncStartReceived == true) {
                if (pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp <= DevContext->ResynStartTimeStamp &&
                    DevContext->ResynStartTimeStamp <= DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601 &&
                    DevContext->ResynStartTimeStamp <= DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601) {
                        // "First diff file after Resync start (in Data File mode) being drained, Volume name: %s, PrevEndTS: %I64x, ResyncStartTS: %I64x, CurrentEndTS: %I64x, CurrentStartTS: %I64x."
                    InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_FIRSTFILE_VALIDATION1,
                            DevContext,
                            pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp,
                            DevContext->ResynStartTimeStamp,
                            DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
                            DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
                }
                if (DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601 < DevContext->ResynStartTimeStamp &&
                    DevContext->ResynStartTimeStamp < DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601) {
                        // "Start-End TS Problem with First diff file after Resync start (in Data File mode) being drained, Volume name: %s, PrevEndTS: %I64x, ResyncStartTS: %I64x, CurrentEndTS: %I64x, CurrentStartTS: %I64x."
                    InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_FIRSTFILE_VALIDATION2,
                            DevContext,
                            pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp,
                            DevContext->ResynStartTimeStamp,
                            DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
                            DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
                }        
                if (pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp > DevContext->ResynStartTimeStamp) {
                        // "PrevEndTS Problem with First diff file after Resync start (in Data File mode) being drained, Volume name: %s, PrevEndTS: %I64x, ResyncStartTS: %I64x, CurrentEndTS: %I64x, CurrentStartTS: %I64x."
                    InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_FIRSTFILE_VALIDATION3,
                            DevContext,
                            pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp,
                            DevContext->ResynStartTimeStamp,
                            DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
                            DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
                }
            }
        }else {
            if (DevContext->bResyncStartReceived == true) {
                if (pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp <= DevContext->ResynStartTimeStamp &&
                    DevContext->ResynStartTimeStamp <= pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601 &&
                    DevContext->ResynStartTimeStamp <= pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601) {
                        // "First diff file after Resync start (Not in Data File mode) being drained, Volume name: %s, PrevEndTS: %I64x, ResyncStartTS: %I64x, CurrentEndTS: %I64x, CurrentStartTS: %I64x."
                    InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_FIRSTFILE_VALIDATION4,
                            DevContext,
                            pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp,
                            DevContext->ResynStartTimeStamp,
                            pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
                            pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
                }
                if (pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601 < DevContext->ResynStartTimeStamp &&
                    DevContext->ResynStartTimeStamp < pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601) {
                        // "Start-End TS Problem with First diff file after Resync start (Not in Data File mode) being drained, Volume name: %s, PrevEndTS: %I64x, ResyncStartTS: %I64x, CurrentEndTS: %I64x, CurrentStartTS: %I64x."
                    InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_FIRSTFILE_VALIDATION5,
                            DevContext,
                            pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp,
                            DevContext->ResynStartTimeStamp,
                            pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
                            pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
                }        
                if (pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp > DevContext->ResynStartTimeStamp) {
                        // "PrevEndTS Problem with First diff file after Resync start (Not in Data File mode) being drained, Volume name: %s, PrevEndTS: %I64x, ResyncStartTS: %I64x, CurrentEndTS: %I64x, CurrentStartTS: %I64x."
                    InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_FIRSTFILE_VALIDATION6,
                            DevContext,
                            pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp,
                            DevContext->ResynStartTimeStamp,
                            pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
                            pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
                }
                // DevContext->bResyncStartReceived = false; // lets do this in commit.
            }
        }
    }

    AddResyncRequiredFlag(pUserDirtyBlock, DevContext);

    if (TRUE == bCommitFailed)
        pUserDirtyBlock->uHdr.Hdr.ulFlags |= DB_FLAGS_COMMIT_FAILED;

    return Status;
}

NTSTATUS
DeviceIoControlGetDirtyBlocksTransaction(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status;
    PIO_STACK_LOCATION  IoStackLocation;
    PDEV_CONTEXT        pDevContext = NULL;
    PCOMMIT_TRANSACTION pCommitTransaction = NULL;
    KIRQL               irql = 0;
    LARGE_INTEGER       liCurrentSystemTime = { 0 };
    BOOLEAN             bLogDiffSyncThrottle = FALSE;
    LARGE_INTEGER       liDiffSyncThrottleStartTime = { 0 };

    InVolDbgPrint(DL_FUNC_TRACE, DM_INMAGE_IOCTL | DM_DIRTY_BLOCKS,
        ("DeviceIoControlGetDirtyBlocks: Called\n"));
    
    Irp->IoStatus.Information = 0;
    if (ecServiceRunning != DriverContext.eServiceState) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_DIRTY_BLOCKS, 
            ("DeviceIoControlGetDirtyBlocksTransaction: Service is not running\n"));
        Status = STATUS_INVALID_DEVICE_STATE;
        goto cleanup;
    }

    if (true == DriverContext.PersistantValueFlushedToRegistry) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_DIRTY_BLOCKS, 
            ("DeviceIoControlGetDirtyBlocksTransaction: Persistent Values already flushed\n"));
        Status = STATUS_DEVICE_BUSY;
        goto cleanup;
    }

    if (DriverContext.bEnableDataFiles && DriverContext.bServiceSupportsDataFiles && (!DriverContext.bS2SupportsDataFiles)) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_DIRTY_BLOCKS, 
            ("DeviceIoControlGetDirtyBlocksTransaction: S2 did not register for data files\n"));
        Status = STATUS_INVALID_DEVICE_STATE;
        goto cleanup;
    }

    IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ASSERT(IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

    if( IoStackLocation->Parameters.DeviceIoControl.InputBufferLength >= sizeof(COMMIT_TRANSACTION) ) {
        InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL | DM_DIRTY_BLOCKS,
            ("DeviceIoControlGetDirtyBlocksTransaction: Commit received in GET_DIRTY_BLOCKS\n"));
        pCommitTransaction = (PCOMMIT_TRANSACTION)Irp->AssociatedIrp.SystemBuffer;
    }

    if( IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(UDIRTY_BLOCK_V2) ) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_DIRTY_BLOCKS,
            ("DeviceIoControlGetDirtyBlocks: IOCTL rejected, buffer too small\n"));
        Status = STATUS_BUFFER_TOO_SMALL;
        goto cleanup;
    }

    pDevContext = GetDevContextForThisIOCTL(DeviceObject, Irp);
    if (NULL == pDevContext) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_DIRTY_BLOCKS, 
            ("DeviceIoControlGetDirtyBlocksTransaction: IOCTL rejected, unknown volume GUID %.*S\n",
                GUID_SIZE_IN_CHARS, (PWCHAR)Irp->AssociatedIrp.SystemBuffer));
        Status = STATUS_NO_SUCH_DEVICE;
        goto cleanup;
    }

    KeQuerySystemTime(&liCurrentSystemTime);

    KeAcquireSpinLock(&pDevContext->Lock, &irql);
    if (MAXLONGLONG == pDevContext->replicationStats.llDiffSyncThrottleEndTime) {
        pDevContext->replicationStats.llDiffSyncThrottleEndTime = liCurrentSystemTime.QuadPart;
        liDiffSyncThrottleStartTime.QuadPart = pDevContext->replicationStats.llDiffSyncThrottleStartTime;
        bLogDiffSyncThrottle = TRUE;
    }
    KeReleaseSpinLock(&pDevContext->Lock, irql);

    if (bLogDiffSyncThrottle) {
        InDskFltWriteEventWithDevCtxt(INDSKFLT_WARN_DIFF_SYNC_THROTTLE_ENDED, pDevContext, liDiffSyncThrottleStartTime.QuadPart, liCurrentSystemTime.QuadPart);
    }

    KeAcquireSpinLock(&pDevContext->Lock, &irql);
    if (TEST_FLAG(pDevContext->ulFlags, DCF_FIELDS_PERSISTED)) {
        KeReleaseSpinLock(&pDevContext->Lock, irql);
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_DIRTY_BLOCKS, 
            ("DeviceIoControlGetDirtyBlocksTransaction: Volume Context fields already Persisted\n"));
        Status = STATUS_NOT_SUPPORTED;
        goto cleanup;
    }

    // Added for OOD
    if (TEST_FLAG(pDevContext->ulFlags, DCF_FILTERING_STOPPED)) {
        KeReleaseSpinLock(&pDevContext->Lock, irql);
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_DIRTY_BLOCKS, 
            ("DeviceIoControlGetDirtyBlocksTransaction: Filtering is Stopped\n"));
        Status = STATUS_NOT_SUPPORTED;
        goto cleanup;
    }

    if (TEST_FLAG(pDevContext->ulFlags, DONT_DRAIN)) {
        KeReleaseSpinLock(&pDevContext->Lock, irql);
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_DIRTY_BLOCKS,
                     ("%s: Device Context flags are set to DONT_DRAIN\n",
                     __FUNCTION__));
        Status = STATUS_NOT_SUPPORTED;
        goto cleanup;
    }

    if (TEST_FLAG(pDevContext->ulFlags2, DEVICE_FLAGS_DONT_DRAIN)) {
        KeReleaseSpinLock(&pDevContext->Lock, irql);
        Status = STATUS_INVALID_DEVICE_STATE;
        goto cleanup;
    }
    KeReleaseSpinLock(&pDevContext->Lock, irql);

    ASSERT(DriverContext.CurrentShutdownMarker == DirtyShutdown);//shutdown is on way we should not return the Dirty block 
    KeWaitForSingleObject(&pDevContext->SyncEvent, Executive, KernelMode, FALSE, NULL);
    Status = CommitAndFillBufferWithDirtyBlock( 
                    pCommitTransaction, 
                    pDevContext, 
                    Irp->AssociatedIrp.SystemBuffer,
                    IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength);
    KeSetEvent(&pDevContext->SyncEvent, IO_NO_INCREMENT, FALSE);

    if (STATUS_SUCCESS != Status) {
        RtlZeroMemory(Irp->AssociatedIrp.SystemBuffer, sizeof(UDIRTY_BLOCK_V2));
    }

    Irp->IoStatus.Information = sizeof(UDIRTY_BLOCK_V2);
cleanup:
    if (NULL != pDevContext) {
        DereferenceDevContext(pDevContext);
    }

    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

VOID
ClearDevStats(PDEV_CONTEXT   pDevContext)
{

    pDevContext->liChangesReturnedToServiceInNonDataMode.QuadPart = 0;
    pDevContext->uliChangesReadFromBitMap.QuadPart = 0;
    pDevContext->uliChangesWrittenToBitMap.QuadPart = 0;
    pDevContext->ullCounterTSODrained = 0;

    // The following should not be cleared.
    // pDevContext->ulChangesQueuedForWriting
    // pDevContext->ulicbChangesQueuedForWriting.QuadPart
    // If these are cleared when bitmap write is completed these would become negative.
    //
    // pDevContext->ulChangesPendingCommit
    // pDevContext->ulicbChangesPendingCommit.QuadPart
    // If these are cleared when commit is received these would become negative.
    //
    // ulicbChangesInBitmap are dynamically computed.
    // ulicbPendingChanges are dynamically computed.
    // 
    // pDevContext->ulTotalChangesPending
    // pDevContext->lDirtyBlocksInQueue

    pDevContext->ulicbChangesWrittenToBitMap.QuadPart = 0;
    pDevContext->ulicbChangesReadFromBitMap.QuadPart = 0;
    pDevContext->licbReturnedToServiceInNonDataMode.QuadPart = 0;
    pDevContext->ulicbChangesReverted.QuadPart = 0;
    pDevContext->ulChangesReverted = 0;
    pDevContext->lNumBitMapWriteErrors = 0;
    pDevContext->lNumBitMapReadErrors = 0;
    pDevContext->lNumBitMapClearErrors = 0;
    pDevContext->lNumBitmapOpenErrors = 0;
    pDevContext->ulNumMemoryAllocFailures = 0;
    // The following should not be reset, since these counters are used to return
    // monitoring stats to the agent, which are cumulatively increasing until the shutdown.
    // pDevContext->ulNumberOfTagPointsDropped
    // pDevContext->ullTotalTrackedBytes

    pDevContext->lNumChangeToBitmapWOState = 0;
    pDevContext->lNumChangeToBitmapWOStateOnUserRequest = 0;
    pDevContext->lNumChangeToMetaDataWOState = 0;
    pDevContext->lNumChangeToMetaDataWOStateOnUserRequest = 0;
    pDevContext->lNumChangeToDataWOState = 0;
    pDevContext->ulNumSecondsInBitmapWOState = 0;
    pDevContext->ulNumSecondsInMetaDataWOState = 0;
    pDevContext->ulNumSecondsInDataWOState = 0;

    pDevContext->lNumChangeToMetaDataCaptureMode = 0;
    pDevContext->lNumChangeToMetaDataCaptureModeOnUserRequest = 0;
    pDevContext->lNumChangeToDataCaptureMode = 0;
    pDevContext->ulNumSecondsInMetadataCaptureMode = 0;
    pDevContext->ulNumSecondsInDataCaptureMode = 0;

    // ulNumSecondsInCurrentMode is dynamically computed
    // ulDataBufferSizeAllocated should not be reset
    // ulcbDataBufferSizeFree should not be reset
    // ulcbDataBufferSizeReserved should not be reset
    // ulDataFilesPending should not be reset

    pDevContext->ulFlushCount = 0;
    pDevContext->ullDataFilesReturned = 0;
    pDevContext->ulDataFilesReverted = 0;
    pDevContext->liTickCountAtLastWOStateChange = KeQueryPerformanceCounter(NULL);
    pDevContext->ulMuCurrentInstance = 0;
    for (ULONG ul = 0; ul < MAX_DC_IO_SIZE_BUCKETS; ul++) {
        pDevContext->ullIoSizeCounters[ul] = 0;
    }

    if (pDevContext->LatencyDistForS2DataRetrieval)
        pDevContext->LatencyDistForS2DataRetrieval->Clear();

    if (pDevContext->LatencyDistForNotify)
        pDevContext->LatencyDistForNotify->Clear();

    if (pDevContext->LatencyLogForS2DataRetrieval)
        pDevContext->LatencyLogForS2DataRetrieval->Clear();

    if (pDevContext->LatencyLogForCommit)
        pDevContext->LatencyLogForCommit->Clear();

    if (pDevContext->LatencyLogForNotify)
        pDevContext->LatencyLogForNotify->Clear();

    KeQuerySystemTime(&pDevContext->liClearStatsTimeStamp);
    pDevContext->ulTagsinWOState = 0;
    pDevContext->ulTagsinNonWOSDrop = 0;
    DriverContext.ulNumberofAsyncTagIOCTLs = 0;
    pDevContext->ulRevokeTagDBCount = 0;
    pDevContext->IoCounterWithPagingIrpSet = 0;
    pDevContext->IoCounterWithSyncPagingIrpSet = 0;
    pDevContext->IoCounterWithNullFileObject = 0;

    return;
}

VOID
GetResyncStats(
    IN PVOLUME_STATS     pVolumeStats,
    IN PDEV_CONTEXT      pDevContext
)
{
    pVolumeStats->ulOutOfSyncErrorCode = pDevContext->ulOutOfSyncErrorCode;
    pVolumeStats->ulOutOfSyncErrorStatus = pDevContext->ulOutOfSyncErrorStatus;
    pVolumeStats->ulOutOfSyncCount = pDevContext->ulOutOfSyncCount;
    pVolumeStats->liOutOfSyncTimeStamp.QuadPart = pDevContext->liOutOfSyncTimeStamp.QuadPart;
    pVolumeStats->liOutOfSyncIndicatedTimeStamp.QuadPart = pDevContext->liOutOfSyncIndicatedTimeStamp.QuadPart;
    pVolumeStats->liOutOfSyncResetTimeStamp.QuadPart = pDevContext->liOutOfSyncResetTimeStamp.QuadPart;
    ULONG   ulOutOfSyncErrorCode = pDevContext->ulOutOfSyncErrorCode;
    if (ulOutOfSyncErrorCode >= ERROR_TO_REG_MAX_ERROR) {
        ulOutOfSyncErrorCode = ERROR_TO_REG_DESCRIPTION_IN_EVENT_LOG;
    }

    if (ulOutOfSyncErrorCode > 0) {
        RtlStringCbPrintfExW((NTSTRSAFE_PWSTR)&pVolumeStats->ErrorStringForResync[0],
            UDIRTY_BLOCK_MAX_ERROR_STRING_SIZE * sizeof(WCHAR),
            NULL,
            NULL,
            STRSAFE_NULL_ON_FAILURE,
            ErrorToRegErrorDescriptionsW[ulOutOfSyncErrorCode],
            pDevContext->ulOutOfSyncErrorStatus);
    }
    else {
        pVolumeStats->ErrorStringForResync[0] = 0x00;
    }
}

VOID
GetDataBytesInfoFromBitMap(
    _Inout_ PVOLUME_STATS     pVolumeStats,
    _In_    PDEV_CONTEXT      pDevContext
)
{
    KIRQL           OldIrql;
    PAGED_CODE();
    PDEV_BITMAP pDevBitmap = NULL;
    bool bIsWOStateBitmap = false;

    KeWaitForSingleObject(&pDevContext->Mutex, Executive, KernelMode, FALSE, NULL);
    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
    if (pDevContext->pDevBitmap) {
        pDevBitmap = ReferenceDevBitmap(pDevContext->pDevBitmap);
        if (ecWriteOrderStateBitmap == pDevContext->eWOState)
            bIsWOStateBitmap = true;
    }
    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);

    if (NULL != pDevBitmap) {
        ExAcquireFastMutex(&pDevBitmap->Mutex);
        pVolumeStats->eVBitmapState = pDevBitmap->eVBitmapState;
        if (bIsWOStateBitmap && (pDevBitmap->pBitmapAPI))
            pVolumeStats->ulicbChangesInBitmap.QuadPart = pDevBitmap->pBitmapAPI->GetDatBytesInBitmap();
        else
            pVolumeStats->ulicbChangesInBitmap.QuadPart = 0;
        ExReleaseFastMutex(&pDevBitmap->Mutex);
        DereferenceDevBitmap(pDevBitmap);
    }
    else {
        pVolumeStats->eVBitmapState = ecVBitmapStateUnInitialized;
        pVolumeStats->ulVolumeFlags |= VSF_BITMAP_NOT_OPENED;
        pVolumeStats->ulicbChangesInBitmap.QuadPart = 0;
    }

    GetResyncStats(pVolumeStats, pDevContext);
    KeReleaseMutex(&pDevContext->Mutex, FALSE);
}

VOID
FillVolumeStatsStructure(
    _Inout_ PVOLUME_STATS     pVolumeStats,
    _In_    PDEV_CONTEXT      pDevContext
)
{
    return FillVolumeStatsStructure(pVolumeStats, pDevContext, FALSE);
}

VOID
FillVolumeStatsStructure(
    _Inout_ PVOLUME_STATS     pVolumeStats,
    _In_    PDEV_CONTEXT      pDevContext,
    _In_    BOOLEAN           bReadBitmap
)

{
    LARGE_INTEGER   liCurrentTickCount, liTickIncrementPerSec, liTickDiff;
    ULONG           ulSecsInCurrentWOState, ulSecsInCurrentCaptureMode;
    KIRQL           OldIrql;

    PAGED_CODE();

    liCurrentTickCount = KeQueryPerformanceCounter(&liTickIncrementPerSec);

    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);

    liTickDiff.QuadPart = liCurrentTickCount.QuadPart - pDevContext->liTickCountAtLastWOStateChange.QuadPart;
    ulSecsInCurrentWOState = (ULONG)(liTickDiff.QuadPart / liTickIncrementPerSec.QuadPart);
    liTickDiff.QuadPart = liCurrentTickCount.QuadPart - pDevContext->liTickCountAtLastCaptureModeChange.QuadPart;
    ulSecsInCurrentCaptureMode = (ULONG)(liTickDiff.QuadPart / liTickIncrementPerSec.QuadPart);

    RtlCopyMemory(pVolumeStats->VolumeGUID, pDevContext->wDevID, sizeof(WCHAR) * GUID_SIZE_IN_CHARS );
    if(pDevContext->pDevBitmap && pDevContext->pDevBitmap->pBitmapAPI) {
        pVolumeStats->ullCacheHit = pDevContext->pDevBitmap->pBitmapAPI->GetCacheHit();
        pVolumeStats->ullCacheMiss = pDevContext->pDevBitmap->pBitmapAPI->GetCacheMiss();
    }
#ifdef VOLUME_FLT
    pVolumeStats->DriveLetter = pDevContext->wDevNum[0];
#endif
    pVolumeStats->uliChangesReturnedToService.QuadPart = pDevContext->uliChangesReturnedToService.QuadPart;
    pVolumeStats->uliChangesReadFromBitMap.QuadPart = pDevContext->uliChangesReadFromBitMap.QuadPart;
    pVolumeStats->uliChangesWrittenToBitMap.QuadPart = pDevContext->uliChangesWrittenToBitMap.QuadPart;
    pVolumeStats->ulChangesQueuedForWriting = pDevContext->ulChangesQueuedForWriting;

    pVolumeStats->ulicbChangesQueuedForWriting.QuadPart = pDevContext->ulicbChangesQueuedForWriting.QuadPart;
    pVolumeStats->ulicbChangesWrittenToBitMap.QuadPart = pDevContext->ulicbChangesWrittenToBitMap.QuadPart;
    pVolumeStats->ulicbChangesReadFromBitMap.QuadPart = pDevContext->ulicbChangesReadFromBitMap.QuadPart;
    pVolumeStats->ulicbChangesReturnedToService.QuadPart = pDevContext->ulicbReturnedToService.QuadPart;
    pVolumeStats->ulicbChangesPendingCommit.QuadPart = pDevContext->ullcbChangesPendingCommit;
    pVolumeStats->ulicbChangesReverted.QuadPart = pDevContext->ulicbChangesReverted.QuadPart;

    pVolumeStats->ullcbTotalChangesPending = pDevContext->ChangeList.ullcbTotalChangesPending;
    pVolumeStats->ullcbBitmapChangesPending = pDevContext->ChangeList.ullcbBitmapChangesPending;
    pVolumeStats->ullcbMetaDataChangesPending = pDevContext->ChangeList.ullcbMetaDataChangesPending;
    pVolumeStats->ullcbDataChangesPending = pDevContext->ChangeList.ullcbDataChangesPending;
    pVolumeStats->ulTotalChangesPending = pDevContext->ChangeList.ulTotalChangesPending;
    pVolumeStats->ulBitmapChangesPending = pDevContext->ChangeList.ulBitmapChangesPending;
    pVolumeStats->ulMetaDataChangesPending = pDevContext->ChangeList.ulMetaDataChangesPending;
    pVolumeStats->ulDataChangesPending = pDevContext->ChangeList.ulDataChangesPending;
    pVolumeStats->liMountNotifyTimeStamp.QuadPart = pDevContext->liMountNotifyTimeStamp.QuadPart;
    pVolumeStats->liDisMountNotifyTimeStamp.QuadPart = pDevContext->liDisMountNotifyTimeStamp.QuadPart;
    pVolumeStats->liDisMountFailNotifyTimeStamp.QuadPart = pDevContext->liDisMountFailNotifyTimeStamp.QuadPart;
    pVolumeStats->ulChangesPendingCommit = pDevContext->ulChangesPendingCommit;
    pVolumeStats->ulChangesReverted = pDevContext->ulChangesReverted;
    pVolumeStats->lDirtyBlocksInQueue = pDevContext->ChangeList.lDirtyBlocksInQueue;
    pVolumeStats->lNumBitMapWriteErrors = pDevContext->lNumBitMapWriteErrors;
    pVolumeStats->lNumBitMapClearErrors = pDevContext->lNumBitMapClearErrors;
    pVolumeStats->ulNumMemoryAllocFailures = pDevContext->ulNumMemoryAllocFailures;
    pVolumeStats->lNumBitMapReadErrors = pDevContext->lNumBitMapReadErrors;
    pVolumeStats->lNumBitmapOpenErrors = pDevContext->lNumBitmapOpenErrors;
    pVolumeStats->ulNumberOfTagPointsDropped = pDevContext->ulNumberOfTagPointsDropped;
    pVolumeStats->ulTagsinWOState = pDevContext->ulTagsinWOState;
    pVolumeStats->ulTagsinNonWOSDrop = pDevContext->ulTagsinNonWOSDrop;
    pVolumeStats->ulRevokeTagDBCount = pDevContext->ulRevokeTagDBCount;
    pVolumeStats->ulcbDataBufferSizeAllocated = pDevContext->ulcbDataAllocated;
    pVolumeStats->ulcbDataBufferSizeReserved = pDevContext->ulcbDataReserved;
    pVolumeStats->ulcbDataBufferSizeFree = pDevContext->ulcbDataFree;
    pVolumeStats->ulFlushCount = pDevContext->ulFlushCount;
    pVolumeStats->llLastFlushTime = pDevContext->liLastFlushTime.QuadPart;
    pVolumeStats->liLastCommittedTimeStampReadFromStore = pDevContext->LastCommittedTimeStampReadFromStore;
    pVolumeStats->liLastCommittedSequenceNumberReadFromStore = pDevContext->LastCommittedSequenceNumberReadFromStore;
    pVolumeStats->liLastCommittedSplitIoSeqIdReadFromStore = pDevContext->LastCommittedSplitIoSeqIdReadFromStore;
    pVolumeStats->ulDataFilesPending = pDevContext->ChangeList.ulDataFilesPending;
    pVolumeStats->ullcbDataToDiskLimit = pDevContext->ullcbDataToDiskLimit;
    pVolumeStats->ullcbMinFreeDataToDiskLimit = pDevContext->ullcbMinFreeDataToDiskLimit;
    pVolumeStats->ullcbDiskUsed = pDevContext->ullcbDataWrittenToDisk;
    pVolumeStats->ullDataFilesReturned = pDevContext->ullDataFilesReturned;
    pVolumeStats->ulDataFilesReverted = pDevContext->ulDataFilesReverted;
    pVolumeStats->ulConfiguredPriority = pDevContext->FileWriterThreadPriority;
    pVolumeStats->ulDataBlocksReserved = pDevContext->ulDataBlocksReserved;
    if ((NULL != pDevContext->DevFileWriter) && (NULL != pDevContext->DevFileWriter->WriterNode)) {
        pVolumeStats->ulEffectivePriority = pDevContext->DevFileWriter->WriterNode->writer->GetPriority();
    } else {
        pVolumeStats->ulEffectivePriority = 0;
    }
    pVolumeStats->ulWriterId = pDevContext->ulWriterId;
    pVolumeStats->ulVolumeSize.QuadPart = pDevContext->llDevSize;
    pVolumeStats->ulcbDataNotifyThreshold = pDevContext->ulcbDataNotifyThreshold;
    pVolumeStats->ulVolumeFlags = 0;
    pVolumeStats->ulNumOfPageFiles = pDevContext->ulNumOfPageFiles;
    
    if (0 != pDevContext->lNumBitMapWriteErrors)
        pVolumeStats->ulVolumeFlags |= VSF_BITMAP_WRITE_ERROR;
#ifdef VOLUME_FLT
    if (pDevContext->ulFlags & DCF_READ_ONLY)
        pVolumeStats->ulVolumeFlags |= VSF_READ_ONLY;
#endif
    if (pDevContext->ulFlags & DCF_FILTERING_STOPPED)
        pVolumeStats->ulVolumeFlags |= VSF_FILTERING_STOPPED;

    if (pDevContext->ulFlags & DCF_BITMAP_WRITE_DISABLED)
        pVolumeStats->ulVolumeFlags |= VSF_BITMAP_WRITE_DISABLED;

    if (pDevContext->ulFlags & DCF_BITMAP_READ_DISABLED)
        pVolumeStats->ulVolumeFlags |= VSF_BITMAP_READ_DISABLED;

    if (pDevContext->ulFlags & DCF_DATA_CAPTURE_DISABLED)
        pVolumeStats->ulVolumeFlags |= VSF_DATA_CAPTURE_DISABLED;

    if (pDevContext->ulFlags & DCF_DATA_FILES_DISABLED)
        pVolumeStats->ulVolumeFlags |= VSF_DATA_FILES_DISABLED;

    if (pDevContext->bNotify)
        pVolumeStats->ulVolumeFlags |= VSF_DATA_NOTIFY_SET;
#ifdef VOLUME_CLUSTER_SUPPORT
    if (pDevContext->ulFlags & DCF_VOLUME_ON_CLUS_DISK)
        pVolumeStats->ulVolumeFlags |= VSF_CLUSTER_VOLUME;
#endif
    if (pDevContext->ulFlags & DCF_SURPRISE_REMOVAL)
        pVolumeStats->ulVolumeFlags |= VSF_SURPRISE_REMOVED;

    if (pDevContext->ulFlags & DCF_FIELDS_PERSISTED)
        pVolumeStats->ulVolumeFlags |= VSF_VCFIELDS_PERSISTED;

    if (pDevContext->ulFlags & DCF_EXPLICIT_NONWO_NODRAIN)
        pVolumeStats->ulVolumeFlags |= VSF_EXPLICIT_NONWO_NODRAIN;

    if (pDevContext->ulFlags & DCF_DONT_PAGE_FAULT)
        pVolumeStats->ulVolumeFlags |= VSF_DONT_PAGE_FAULT;

    if (pDevContext->ulFlags & DCF_LAST_IO)
        pVolumeStats->ulVolumeFlags |= VSF_LAST_IO;

    if (pDevContext->ulFlags & DCF_BITMAP_DEV_OFF)
        pVolumeStats->ulVolumeFlags |= VSF_BITMAP_DEV_OFF;

    if (pDevContext->ulFlags & DCF_PAGE_FILE_MISSED)
        pVolumeStats->ulVolumeFlags |= VSF_PAGE_FILE_MISSED;

    if (pDevContext->ulFlags & DCF_DEVNUM_OBTAINED)
        pVolumeStats->ulVolumeFlags |= VSF_DEVNUM_OBTAINED;

    if (pDevContext->ulFlags & DCF_DEVSIZE_OBTAINED)
        pVolumeStats->ulVolumeFlags |= VSF_DEVSIZE_OBTAINED;

    if (TEST_FLAG(pDevContext->ulFlags, DCF_DEVID_OBTAINED))
    {
        if (TEST_FLAG(pDevContext->ulFlags, DCF_DISKID_CONFLICT))
        {
            SET_FLAG(pVolumeStats->ulVolumeFlags, VSF_DISK_ID_CONFLICT);
        }
    }

    if (pDevContext->ulFlags & DCF_PHY_DEVICE_INRUSH)
        pVolumeStats->ulVolumeFlags |= VSF_PHY_DEVICE_INRUSH;

#ifndef VOLUME_FLT
    if (pDevContext->ulFlags & DCF_BITMAP_DEV)
        pVolumeStats->ulVolumeFlags |= VSF_BITMAP_DEV;
#endif
    pVolumeStats->eWOState = pDevContext->eWOState;
    pVolumeStats->ePrevWOState = pDevContext->ePrevWOState;
    pVolumeStats->eCaptureMode = pDevContext->eCaptureMode;

    if (pDevContext->ulFlags & DCF_FILTERING_STOPPED) {
        pVolumeStats->ulNumSecondsInCurrentWOState = 0;
        pVolumeStats->ulNumSecondsInCurrentCaptureMode = 0;
    } else {
        pVolumeStats->ulNumSecondsInCurrentWOState = ulSecsInCurrentWOState;
        pVolumeStats->ulNumSecondsInCurrentCaptureMode = ulSecsInCurrentCaptureMode;
    }

    pVolumeStats->lNumChangeToMetaDataWOState = pDevContext->lNumChangeToMetaDataWOState;
    pVolumeStats->lNumChangeToMetaDataWOStateOnUserRequest = pDevContext->lNumChangeToMetaDataWOStateOnUserRequest;
    pVolumeStats->lNumChangeToDataWOState = pDevContext->lNumChangeToDataWOState;

    pVolumeStats->lNumChangeToBitmapWOState = pDevContext->lNumChangeToBitmapWOState;
    pVolumeStats->lNumChangeToBitmapWOStateOnUserRequest = pDevContext->lNumChangeToBitmapWOStateOnUserRequest;
    pVolumeStats->lNumChangeToDataCaptureMode = pDevContext->lNumChangeToDataCaptureMode;
    pVolumeStats->lNumChangeToMetaDataCaptureMode = pDevContext->lNumChangeToMetaDataCaptureMode;
    pVolumeStats->lNumChangeToMetaDataCaptureModeOnUserRequest = pDevContext->lNumChangeToMetaDataCaptureModeOnUserRequest;

    pVolumeStats->ulNumSecondsInDataCaptureMode = pDevContext->ulNumSecondsInDataCaptureMode;
    pVolumeStats->ulNumSecondsInMetadataCaptureMode = pDevContext->ulNumSecondsInMetadataCaptureMode;
    pVolumeStats->ulNumSecondsInDataWOState = pDevContext->ulNumSecondsInDataWOState;
    pVolumeStats->ulNumSecondsInMetaDataWOState = pDevContext->ulNumSecondsInMetaDataWOState;
    pVolumeStats->ulNumSecondsInBitmapWOState = pDevContext->ulNumSecondsInBitmapWOState;

    // Fill disk usage at out of data mode.
    if (pDevContext->ulMuCurrentInstance) {
        ULONG   ulMaxInstances = INSTANCES_OF_MEM_USAGE_TRACKED;
        if (pDevContext->ulMuCurrentInstance < ulMaxInstances) {
            ulMaxInstances = pDevContext->ulMuCurrentInstance;
        }

        if (USER_MODE_INSTANCES_OF_MEM_USAGE_TRACKED < ulMaxInstances) {
            ulMaxInstances = USER_MODE_INSTANCES_OF_MEM_USAGE_TRACKED;
        }

        for (ULONG ul = 0; ul < ulMaxInstances; ul++) {
            ULONG   ulInstance = pDevContext->ulMuCurrentInstance - ul - 1;
            ulInstance = ulInstance % INSTANCES_OF_MEM_USAGE_TRACKED;
            pVolumeStats->ullDuAtOutOfDataMode[ul] = pDevContext->ullDiskUsageArray[ulInstance];
            pVolumeStats->ulMAllocatedAtOutOfDataMode[ul] = pDevContext->ulMemAllocatedArray[ulInstance];
            pVolumeStats->ulMReservedAtOutOfDataMode[ul] = pDevContext->ulMemReservedArray[ulInstance];
            pVolumeStats->ulMFreeInVCAtOutOfDataMode[ul] = pDevContext->ulMemFreeArray[ulInstance];
            pVolumeStats->ulMFreeAtOutOfDataMode[ul] = pDevContext->ulTotalMemFreeArray[ulInstance];
            pVolumeStats->liCaptureModeChangeTS[ul] = pDevContext->liCaptureModeChangeTS[ulInstance];
        }
        pVolumeStats->ulMuInstances = ulMaxInstances;
    } else {
        pVolumeStats->ulMuInstances = 0;
    }

    if (pDevContext->ulWOSChangeInstance) {
        ULONG   ulMaxInstances = INSTANCES_OF_WO_STATE_TRACKED;
        if (pDevContext->ulWOSChangeInstance < ulMaxInstances) {
            ulMaxInstances = pDevContext->ulWOSChangeInstance;
        }

        if (USER_MODE_INSTANCES_OF_WO_STATE_TRACKED < ulMaxInstances) {
            ulMaxInstances = USER_MODE_INSTANCES_OF_WO_STATE_TRACKED;
        }

        for (ULONG ul = 0; ul < ulMaxInstances; ul++) {
            ULONG   ulInstance = pDevContext->ulWOSChangeInstance - ul - 1;
            ulInstance = ulInstance % INSTANCES_OF_WO_STATE_TRACKED;

            pVolumeStats->eOldWOState[ul] = pDevContext->eOldWOState[ulInstance];
            pVolumeStats->eNewWOState[ul] = pDevContext->eNewWOState[ulInstance];
            pVolumeStats->eWOSChangeReason[ul] = pDevContext->eWOSChangeReason[ulInstance];
            pVolumeStats->ulNumSecondsInWOS[ul] = pDevContext->ulNumSecondsInWOS[ulInstance];
            pVolumeStats->liWOstateChangeTS[ul] = pDevContext->liWOstateChangeTS[ulInstance];
            pVolumeStats->ullcbBChangesPendingAtWOSchange[ul] = pDevContext->ullcbBChangesPendingAtWOSchange[ulInstance];
            pVolumeStats->ullcbMDChangesPendingAtWOSchange[ul] = pDevContext->ullcbMDChangesPendingAtWOSchange[ulInstance];
            pVolumeStats->ullcbDChangesPendingAtWOSchange[ul] = pDevContext->ullcbDChangesPendingAtWOSchange[ulInstance];
        }
        pVolumeStats->ulWOSChInstances = ulMaxInstances;
    } else {
        pVolumeStats->ulWOSChInstances = 0;
    }

    // Fill Counters.
    ULONG ulMaxCounters = USER_MODE_MAX_IO_BUCKETS;
    if (ulMaxCounters > MAX_DC_IO_SIZE_BUCKETS) {
        ulMaxCounters = MAX_DC_IO_SIZE_BUCKETS;
    }

    pVolumeStats->ullTotalTrackedBytes = pDevContext->ullTotalTrackedBytes;
    for (ULONG ul = 0; ul < ulMaxCounters; ul++) {
        pVolumeStats->ulIoSizeArray[ul] = pDevContext->ulIoSizeArray[ul];
        pVolumeStats->ullIoSizeCounters[ul] = pDevContext->ullIoSizeCounters[ul];
    }

    for (ULONG ul = 0; ul < ulMaxCounters; ul++) {
        pVolumeStats->ulIoSizeReadArray[ul] = pDevContext->DeviceProfile.ReadProfile.ReadIoSizeArray[ul];
        pVolumeStats->ullIoSizeReadCounters[ul] = pDevContext->DeviceProfile.ReadProfile.ReadIoSizeCounters[ul];
    }

	pVolumeStats->liVolumeContextCreationTS.QuadPart = pDevContext->liDevContextCreationTS.QuadPart;
    pVolumeStats->liStartFilteringTimeStamp.QuadPart = pDevContext->liStartFilteringTimeStamp.QuadPart;
	pVolumeStats->liStartFilteringTimeStampByUser.QuadPart =  pDevContext->liStartFilteringTimeStampByUser.QuadPart;
	pVolumeStats->liGetDBTimeStamp = pDevContext->liGetDirtyBlockTimeStamp;
	pVolumeStats->liCommitDBTimeStamp = pDevContext->liCommitDirtyBlockTimeStamp;
    pVolumeStats->liStopFilteringTimeStamp.QuadPart = pDevContext->liStopFilteringTimeStamp.QuadPart;
    pVolumeStats->liClearDiffsTimeStamp.QuadPart = pDevContext->liClearDiffsTimeStamp.QuadPart;
    pVolumeStats->liClearStatsTimeStamp.QuadPart = pDevContext->liClearStatsTimeStamp.QuadPart;
    pVolumeStats->liResyncStartTimeStamp.QuadPart = pDevContext->liResyncStartTimeStamp.QuadPart;
    pVolumeStats->liResyncEndTimeStamp.QuadPart = pDevContext->liResyncEndTimeStamp.QuadPart;
    pVolumeStats->liLastOutOfSyncTimeStamp.QuadPart = pDevContext->liLastOutOfSyncTimeStamp.QuadPart;
    pVolumeStats->ulLastOutOfSyncErrorCode = pDevContext->ulLastOutOfSyncErrorCode;
    pVolumeStats->ullLastOutOfSyncSeqNumber = pDevContext->ullLastOutOfSyncSeqNumber;
    pVolumeStats->ullOutOfSyncSeqNumber = pDevContext->ullOutOfSyncSequnceNumber;
    pVolumeStats->ulOutOfSyncTotalCount = pDevContext->ulOutOfSyncTotalCount;
    pVolumeStats->llInCorrectCompletionRoutineCount = pDevContext->llInCorrectCompletionRoutineCount;
    pVolumeStats->llIoCounterNonPassiveLevel = pDevContext->llIoCounterNonPassiveLevel;
    pVolumeStats->llIoCounterWithNULLMdl = pDevContext->llIoCounterWithNULLMdl;
    pVolumeStats->llIoCounterWithInValidMDLFlags = pDevContext->llIoCounterWithInValidMDLFlags;
    pVolumeStats->llIoCounterMDLSystemVAMapFailCount = pDevContext->llIoCounterMDLSystemVAMapFailCount;
    pVolumeStats->licbReturnedToServiceInNonDataMode.QuadPart = pDevContext->licbReturnedToServiceInNonDataMode.QuadPart;
    pVolumeStats->liChangesReturnedToServiceInNonDataMode = pDevContext->liChangesReturnedToServiceInNonDataMode;
    pVolumeStats->ullCounterTSODrained = pDevContext->ullCounterTSODrained;
    pVolumeStats->ullLastCommittedTagTimeStamp = pDevContext->ullLastCommittedTagTimeStamp;
    pVolumeStats->ullLastCommittedTagSequeneNumber = pDevContext->ullLastCommittedTagSequeneNumber;
    pVolumeStats->ucShutdownMarker = pDevContext->ucShutdownMarker;
    pVolumeStats->BitmapRecoveryState = (_ettagLogRecoveryState)pDevContext->BitmapRecoveryState;
    // TODO : Due to lack of space in Global structure VOLUME_STATS_DATA, shipping below global counters 
    // as part of per-device stats
    pVolumeStats->lCntDevContextAllocs = DriverContext.lCntDevContextAllocs;
    pVolumeStats->lCntDevContextFree = DriverContext.lCntDevContextFree;
    pVolumeStats->ullDataPoolSize = DriverContext.ullDataPoolSize;
    pVolumeStats->liDriverLoadTime.QuadPart = DriverContext.globalTelemetryInfo.liDriverLoadTime.QuadPart;
    pVolumeStats->liLastAgentStartTime.QuadPart = DriverContext.globalTelemetryInfo.liLastAgentStartTime.QuadPart;
    pVolumeStats->liLastAgentStopTime.QuadPart = DriverContext.globalTelemetryInfo.liLastAgentStopTime.QuadPart;
    pVolumeStats->liLastS2StartTime.QuadPart = DriverContext.globalTelemetryInfo.liLastS2StartTime.QuadPart;
    pVolumeStats->liLastS2StopTime.QuadPart = DriverContext.globalTelemetryInfo.liLastS2StopTime.QuadPart;
    pVolumeStats->liLastTagReq.QuadPart = DriverContext.globalTelemetryInfo.liLastTagRequestTime.QuadPart;
    pVolumeStats->liStopFilteringTimestampByUser.QuadPart = pDevContext->DiskTelemetryInfo.liStopFilteringTimestampByUser.QuadPart;
    pVolumeStats->liStopFilteringAllTimeStamp.QuadPart = DriverContext.liStopFilteringAllTimestamp.QuadPart;
    pVolumeStats->llTimeJumpDetectedTS = DriverContext.timeJumpInfo.llExpectedTime;
    pVolumeStats->llTimeJumpedTS = DriverContext.timeJumpInfo.llCurrentTime;
    pVolumeStats->liDeleteBitmapTimeStamp.QuadPart = pDevContext->liDeleteBitmapTimeStamp.QuadPart;
#ifdef VOLUME_FLT
    pVolumeStats->lAdditionalGUIDs = pDevContext->lAdditionalGUIDs;
#endif
    pVolumeStats->ulAdditionalGUIDsReturned = 0;
    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);

    if (bReadBitmap) {
        GetDataBytesInfoFromBitMap(pVolumeStats, pDevContext);
    }
    else {
        // Bitmap data may not be correct as we have not acquired mutex.
        // But we have very little option as we dont want to acquire bitmap mutex
        // and delay subsequent bitmap operation.
        GetResyncStats(pVolumeStats, pDevContext);
    }

    switch (pVolumeStats->eVBitmapState) {
        case ecVBitmapStateUnInitialized:
        case ecVBitmapStateOpened:
            pVolumeStats->ulVolumeFlags |= VSF_BITMAP_READ_NOT_STARTED;
            break;
        case ecVBitmapStateReadStarted:
        case ecVBitmapStateAddingChanges:
            pVolumeStats->ulVolumeFlags |= VSF_BITMAP_READ_IN_PROGRESS;
            break;
        case ecVBitmapStateReadPaused:
            pVolumeStats->ulVolumeFlags |= VSF_BITMAP_READ_PAUSED;
            break;
        case ecVBitmapStateReadCompleted:
            pVolumeStats->ulVolumeFlags |= VSF_BITMAP_READ_COMPLETE;
            break;
        case ecVBitmapStateClosed:
        case ecVBitmapStateReadError:
        case ecVBitmapStateInternalError:
            pVolumeStats->ulVolumeFlags |= VSF_BITMAP_READ_ERROR;
            break;
        default:
            ASSERT(0);
    }
    pVolumeStats->IoCounterWithPagingIrpSet = pDevContext->IoCounterWithPagingIrpSet;
    pVolumeStats->IoCounterWithSyncPagingIrpSet = pDevContext->IoCounterWithSyncPagingIrpSet;
	pVolumeStats->IoCounterWithNullFileObject = pDevContext->IoCounterWithNullFileObject;
    pVolumeStats->bPagingFilePossible = pDevContext->IsPageFileDevicePossible;
    pVolumeStats->bDeviceEnumerated = pDevContext->IsDeviceEnumerated;
    return;
}

NTSTATUS
DeviceIoControlClearDevStats(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS            Status = STATUS_INVALID_PARAMETER;
    PDEV_CONTEXT        pDevContext;
    KIRQL               OldIrql;
    PLIST_ENTRY         pEntry;

    if (0 == IoStackLocation->Parameters.DeviceIoControl.InputBufferLength) {
        KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
        pEntry = DriverContext.HeadForDevContext.Flink;
        while (pEntry != &DriverContext.HeadForDevContext) {
            pDevContext = (PDEV_CONTEXT)pEntry;
            ClearDevStats(pDevContext);
            pEntry = pEntry->Flink;
        }
        KeReleaseSpinLock(&DriverContext.Lock, OldIrql);
        Status = STATUS_SUCCESS;
    } else {
        pDevContext = GetDevContextForThisIOCTL(DeviceObject, Irp);
        if (pDevContext) {
            ClearDevStats(pDevContext);
            DereferenceDevContext(pDevContext);
            Status = STATUS_SUCCESS;
		}
		else {
			Status = STATUS_NO_SUCH_DEVICE;
		}
    }
    
    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

VOID
FillDevDM(
    PDEV_DM_DATA   pDevDM,
    PDEV_CONTEXT   DevContext
    )
{
    LARGE_INTEGER   liCurrentTickCount, liTickIncrementPerSec, liTickDiff;
    ULONG           ulSecsInCurrentWOState, ulSecsInCurrentCaptureMode;

PAGED_CODE();

    liCurrentTickCount = KeQueryPerformanceCounter(&liTickIncrementPerSec);
    liTickDiff.QuadPart = liCurrentTickCount.QuadPart - DevContext->liTickCountAtLastWOStateChange.QuadPart;
    ulSecsInCurrentWOState = (ULONG)(liTickDiff.QuadPart / liTickIncrementPerSec.QuadPart);
    liTickDiff.QuadPart = liCurrentTickCount.QuadPart - DevContext->liTickCountAtLastCaptureModeChange.QuadPart;
    ulSecsInCurrentCaptureMode = (ULONG)(liTickDiff.QuadPart / liTickIncrementPerSec.QuadPart);

    RtlCopyMemory(pDevDM->DevID, DevContext->wDevID, sizeof(WCHAR) * GUID_SIZE_IN_CHARS );
#ifdef VOLUME_FLT
    pDevDM->DriveLetter = DevContext->wDevNum[0];
#endif
    pDevDM->eWOState = DevContext->eWOState;
    pDevDM->ePrevWOState = DevContext->ePrevWOState;
    pDevDM->eCaptureMode = DevContext->eCaptureMode;

    pDevDM->lNumChangeToMetaDataWOState = DevContext->lNumChangeToMetaDataWOState;
    pDevDM->lNumChangeToMetaDataWOStateOnUserRequest = DevContext->lNumChangeToMetaDataWOStateOnUserRequest;
    pDevDM->lNumChangeToDataWOState = DevContext->lNumChangeToDataWOState;

    pDevDM->lNumChangeToBitmapWOState = DevContext->lNumChangeToBitmapWOState;
    pDevDM->lNumChangeToBitmapWOStateOnUserRequest = DevContext->lNumChangeToBitmapWOStateOnUserRequest;
    pDevDM->lNumChangeToDataCaptureMode = DevContext->lNumChangeToDataCaptureMode;
    pDevDM->lNumChangeToMetaDataCaptureMode = DevContext->lNumChangeToMetaDataCaptureMode;
    pDevDM->lNumChangeToMetaDataCaptureModeOnUserRequest = DevContext->lNumChangeToMetaDataCaptureModeOnUserRequest;

    pDevDM->ulNumSecondsInDataCaptureMode = DevContext->ulNumSecondsInDataCaptureMode;
    pDevDM->ulNumSecondsInMetadataCaptureMode = DevContext->ulNumSecondsInMetadataCaptureMode;

    pDevDM->ulNumSecondsInDataWOState = DevContext->ulNumSecondsInDataWOState;
    pDevDM->ulNumSecondsInMetaDataWOState = DevContext->ulNumSecondsInMetaDataWOState;

    pDevDM->ulNumSecondsInBitmapWOState = DevContext->ulNumSecondsInBitmapWOState;

    if (DevContext->ulFlags & DCF_FILTERING_STOPPED) {
        pDevDM->ulNumSecondsInCurrentWOState = 0;
        pDevDM->ulNumSecondsInCurrentCaptureMode = 0;
    } else {
        pDevDM->ulNumSecondsInCurrentWOState = ulSecsInCurrentWOState;
        pDevDM->ulNumSecondsInCurrentCaptureMode = ulSecsInCurrentCaptureMode;
    }

    pDevDM->lDirtyBlocksInQueue = DevContext->ChangeList.lDirtyBlocksInQueue;
    pDevDM->ulDataFilesPending = DevContext->ChangeList.ulDataFilesPending;
    pDevDM->ulDataChangesPending = DevContext->ChangeList.ulDataChangesPending;
    pDevDM->ulMetaDataChangesPending = DevContext->ChangeList.ulMetaDataChangesPending;
    pDevDM->ulBitmapChangesPending = DevContext->ChangeList.ulBitmapChangesPending;
    pDevDM->ulTotalChangesPending = DevContext->ChangeList.ulTotalChangesPending;

    pDevDM->ullcbDataChangesPending = DevContext->ChangeList.ullcbDataChangesPending;
    pDevDM->ullcbMetaDataChangesPending = DevContext->ChangeList.ullcbMetaDataChangesPending;
    pDevDM->ullcbBitmapChangesPending = DevContext->ChangeList.ullcbBitmapChangesPending;
    pDevDM->ullcbTotalChangesPending = DevContext->ChangeList.ullcbTotalChangesPending;

    if (DevContext->LatencyDistForNotify) {
        DevContext->LatencyDistForNotify->GetDistribution((PULONGLONG)pDevDM->ullNotifyLatencyDistribution, USER_MODE_NOTIFY_LATENCY_DISTRIBUTION_BUCKETS);
        DevContext->LatencyDistForNotify->GetDistributionSize((PULONG)pDevDM->ulNotifyLatencyDistSizeArray, USER_MODE_NOTIFY_LATENCY_DISTRIBUTION_BUCKETS);
    }

    if (DevContext->LatencyLogForNotify) {
        pDevDM->ulNotifyLatencyLogSize = DevContext->LatencyLogForNotify->GetLog((PULONG)pDevDM->ulNotifyLatencyLogArray, 
                                                                                        USER_MODE_INSTANCES_OF_NOTIFY_LATENCY_TRACKED);
        DevContext->LatencyLogForNotify->GetMinAndMaxLoggedValues(pDevDM->ulNotifyLatencyMinLoggedValue, pDevDM->ulNotifyLatencyMaxLoggedValue);
    }

    if (DevContext->LatencyDistForCommit) {
        DevContext->LatencyDistForCommit->GetDistribution((PULONGLONG)pDevDM->ullCommitLatencyDistribution, USER_MODE_COMMIT_LATENCY_DISTRIBUTION_BUCKETS);
        DevContext->LatencyDistForCommit->GetDistributionSize((PULONG)pDevDM->ulCommitLatencyDistSizeArray, USER_MODE_COMMIT_LATENCY_DISTRIBUTION_BUCKETS);
    }

    if (DevContext->LatencyLogForCommit) {
        pDevDM->ulCommitLatencyLogSize = DevContext->LatencyLogForCommit->GetLog((PULONG)pDevDM->ulCommitLatencyLogArray, 
                                                                                        USER_MODE_INSTANCES_OF_COMMIT_LATENCY_TRACKED);
        DevContext->LatencyLogForCommit->GetMinAndMaxLoggedValues(pDevDM->ulCommitLatencyMinLoggedValue, pDevDM->ulCommitLatencyMaxLoggedValue);
    }

    if (DevContext->LatencyDistForS2DataRetrieval) {
        DevContext->LatencyDistForS2DataRetrieval->GetDistribution(
                (PULONGLONG)pDevDM->ullS2DataRetievalLatencyDistribution, USER_MODE_S2_DATA_RETRIEVAL_LATENCY_DISTRIBUTION_BUCKERS);
        DevContext->LatencyDistForS2DataRetrieval->GetDistributionSize(
                (PULONG)pDevDM->ulS2DataRetievalLatencyDistSizeArray, USER_MODE_S2_DATA_RETRIEVAL_LATENCY_DISTRIBUTION_BUCKERS);
    }

    if (DevContext->LatencyLogForS2DataRetrieval) {
        pDevDM->ulS2DataRetievalLatencyLogSize = DevContext->LatencyLogForS2DataRetrieval->GetLog(
                                                            (PULONG)pDevDM->ulS2DataRetievalLatencyLogArray, 
                                                            USER_MODE_INSTANCES_OF_S2_DATA_RETRIEVAL_LATENCY_TRACKED);
        DevContext->LatencyLogForS2DataRetrieval->GetMinAndMaxLoggedValues(
                                        pDevDM->ulS2DataRetievalLatencyMinLoggedValue, pDevDM->ulS2DataRetievalLatencyMaxLoggedValue);
    }

    // Fill disk usage at out of data mode.
    if (DevContext->ulMuCurrentInstance) {
        ULONG   ulMaxInstances = INSTANCES_OF_MEM_USAGE_TRACKED;
        if (DevContext->ulMuCurrentInstance < ulMaxInstances) {
            ulMaxInstances = DevContext->ulMuCurrentInstance;
        }

        if (USER_MODE_INSTANCES_OF_MEM_USAGE_TRACKED < ulMaxInstances) {
            ulMaxInstances = USER_MODE_INSTANCES_OF_MEM_USAGE_TRACKED;
        }

        for (ULONG ul = 0; ul < ulMaxInstances; ul++) {
            ULONG   ulInstance = DevContext->ulMuCurrentInstance - ul - 1;
            ulInstance = ulInstance % INSTANCES_OF_MEM_USAGE_TRACKED;
            pDevDM->ullDuAtOutOfDataMode[ul] = DevContext->ullDiskUsageArray[ulInstance];
            pDevDM->ulMAllocatedAtOutOfDataMode[ul] = DevContext->ulMemAllocatedArray[ulInstance];
            pDevDM->ulMReservedAtOutOfDataMode[ul] = DevContext->ulMemReservedArray[ulInstance];
            pDevDM->ulMFreeInVCAtOutOfDataMode[ul] = DevContext->ulMemFreeArray[ulInstance];
            pDevDM->ulMFreeAtOutOfDataMode[ul] = DevContext->ulTotalMemFreeArray[ulInstance];
            pDevDM->liCaptureModeChangeTS[ul] = DevContext->liCaptureModeChangeTS[ulInstance];
        }
        pDevDM->ulMuInstances = ulMaxInstances;
    } else {
        pDevDM->ulMuInstances = 0;
    }

    return;
}

NTSTATUS
DeviceIoControlGetDMinfo(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS            Status = STATUS_SUCCESS;
    PDEVS_DM_DATA    pDevDMdata;
    PDEV_CONTEXT     pDevContext = NULL;
    ULONG               ulBytesWritten = 0;
    ULONG               ulIndex, ulRemainingOutBytes;

    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(DEV_DM_DATA)) {
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    if (0 != IoStackLocation->Parameters.DeviceIoControl.InputBufferLength) {
        pDevContext = GetDevContextForThisIOCTL(DeviceObject, Irp);
    }

    pDevDMdata = (PDEVS_DM_DATA)Irp->AssociatedIrp.SystemBuffer;
    pDevDMdata->usMajorVersion = VOLUMES_DM_MAJOR_VERSION;
    pDevDMdata->usMinorVersion = VOLUMES_DM_MINOR_VERSION;
    pDevDMdata->ulDevReturned = 0;
    pDevDMdata->ulTotalDevs = DriverContext.ulNumDevs;
	RtlZeroMemory(pDevDMdata->ulReserved, sizeof(pDevDMdata->ulReserved));

    if (NULL == pDevContext) {
        LIST_ENTRY  VolumeNodeList;

        // Fill stats of all devices into output buffer
PAGED_CODE();

        ulBytesWritten = sizeof(DEV_DM_DATA) - sizeof(DEV_DM_DATA);
        ulRemainingOutBytes = IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength;
        ulRemainingOutBytes -= (sizeof(DEV_DM_DATA) - sizeof(DEV_DM_DATA));

        InitializeListHead(&VolumeNodeList);
        Status = GetListOfDevs(&VolumeNodeList);
        if (STATUS_SUCCESS == Status) {
            while(!IsListEmpty(&VolumeNodeList)) {
                PLIST_NODE      pNode;

                pNode = (PLIST_NODE)RemoveHeadList(&VolumeNodeList);
                pDevContext = (PDEV_CONTEXT)pNode->pData;
                DereferenceListNode(pNode);

                if (ulRemainingOutBytes >= sizeof(DEV_DM_DATA)) {
                    ulIndex = pDevDMdata->ulDevReturned;
                    FillDevDM(&pDevDMdata->DevArray[ulIndex], pDevContext);

                    pDevDMdata->ulDevReturned++;
                    ulRemainingOutBytes -= sizeof(DEV_DM_DATA);
                    ulBytesWritten += sizeof(DEV_DM_DATA);
                }

                DereferenceDevContext(pDevContext);
            } // for each volume
        }

        if (0 == pDevDMdata->ulDevReturned) {
            // To be on safe side atleast return the size of bytes written as sizeof(VOLUME_STATS_DATA);
            ulBytesWritten = sizeof(DEV_DM_DATA);
        }
    } else {
        // Requested volume stats for one volume
        pDevDMdata->ulDevReturned = 1;
        ulBytesWritten = sizeof(DEV_DM_DATA);
        FillDevDM(&pDevDMdata->DevArray[0], pDevContext);

        DereferenceDevContext(pDevContext);
    }

    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = ulBytesWritten;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS
DeviceIoControlGetDeviceTrackingSize(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS            Status = STATUS_SUCCESS;
    PDEV_CONTEXT        pDevContext = NULL;
    ULARGE_INTEGER     *ulDeviceSize;
    KIRQL               OldIrql;

    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(ULARGE_INTEGER)) {
		Status = STATUS_BUFFER_TOO_SMALL;
        Irp->IoStatus.Information = 0;
        goto CleanUp;
    }
    
    
    if (0 != IoStackLocation->Parameters.DeviceIoControl.InputBufferLength) {
        pDevContext = GetDevContextForThisIOCTL(DeviceObject, Irp);
    }

    if (NULL == pDevContext) {
        Status = STATUS_NO_SUCH_DEVICE;
        Irp->IoStatus.Information = 0;
        goto CleanUp;
    }

	ulDeviceSize = (ULARGE_INTEGER *)Irp->AssociatedIrp.SystemBuffer;

    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
	ulDeviceSize->QuadPart = pDevContext->llDevSize;
    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);

    DereferenceDevContext(pDevContext);

    Irp->IoStatus.Information = sizeof(ULARGE_INTEGER);

CleanUp:
    
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS DeviceIoControlGetLcn (PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS            Status = STATUS_SUCCESS;
    PGET_LCN              pGetLcnInputParameter = NULL;
    PRETRIEVAL_POINTERS_BUFFER pRetPointerBuffer = NULL;    
    HANDLE FileHandle;
    OBJECT_ATTRIBUTES   ObjectAttributes;
    IO_STATUS_BLOCK StatusBlock;
    LARGE_INTEGER StartingVcn;
    UNICODE_STRING FileName;
    ULONG OutBufferLength =  IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength;
    WCHAR               *pwcFileName = NULL;

    UNREFERENCED_PARAMETER(DeviceObject);
      
    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(RETRIEVAL_POINTERS_BUFFER) || 
        IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength > (sizeof(RETRIEVAL_POINTERS_BUFFER) + (1024 * 2 * sizeof(LARGE_INTEGER))) ) {
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Information = 0;
        goto CleanUp;
    }    
    
    if (sizeof(GET_LCN) != IoStackLocation->Parameters.DeviceIoControl.InputBufferLength) {
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Information = 0;
        goto CleanUp;
    }

    if (NULL == Irp->AssociatedIrp.SystemBuffer) {
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Information = 0;
        goto CleanUp;
    }

    pGetLcnInputParameter = (PGET_LCN)Irp->AssociatedIrp.SystemBuffer;
    pRetPointerBuffer = (PRETRIEVAL_POINTERS_BUFFER) Irp->AssociatedIrp.SystemBuffer;

    if (pGetLcnInputParameter->usFileNameLength >= (FILE_NAME_SIZE_LCN  * sizeof(WCHAR))) {
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Information = 0;
        goto CleanUp;
    }

    StartingVcn.QuadPart = pGetLcnInputParameter->StartingVcn.QuadPart;

    pwcFileName = (WCHAR *)ExAllocatePoolWithTag(PagedPool, (FILE_NAME_SIZE_LCN  * sizeof(WCHAR)), TAG_GENERIC_PAGED);
    if (NULL == pwcFileName) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        Irp->IoStatus.Information = 0;
        goto CleanUp;
    }

    RtlMoveMemory(pwcFileName, pGetLcnInputParameter->FileName,
                         pGetLcnInputParameter->usFileNameLength);

    pwcFileName[pGetLcnInputParameter->usFileNameLength/sizeof(WCHAR)] = L'\0';

    RtlInitUnicodeString(&FileName, pwcFileName);
    
    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL, ("FileName  %wZ\n", &FileName));

    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL, ("Starting  VCN %#I64x\n", StartingVcn.QuadPart));
     
    InitializeObjectAttributes ( &ObjectAttributes,
                                &FileName,
                                OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
                                NULL,
                                NULL );

    Status = IoCreateFile (&FileHandle,
                           SYNCHRONIZE,
                           &ObjectAttributes, &StatusBlock,
                           NULL,
                           0,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           FILE_OPEN,
                           FILE_SYNCHRONOUS_IO_NONALERT,
                           NULL,
                           0,
                           CreateFileTypeNone,
                           NULL,
                           IO_OPEN_PAGING_FILE | IO_NO_PARAMETER_CHECKING );
    // ERROR_SHARING_VIOATION is returned if IO_OPEN_PAGING_FILE not set 
    // ERROR_NOACCESS gets set if IO_NO_PARAMETER_CHECKING not set

    if (!NT_SUCCESS(Status)) {
        Irp->IoStatus.Information = 0;
        goto CleanUp;
    }
    
    Status = ZwFsControlFile(
                   FileHandle,
                   NULL, 
                   NULL,
                   NULL,
                   &StatusBlock,
                   FSCTL_GET_RETRIEVAL_POINTERS ,
                   &StartingVcn,
                   sizeof(LARGE_INTEGER),
                   pRetPointerBuffer,
                   OutBufferLength
                  );

    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL, ("Information %#I64x Status %x OutBufferLength %#I64x\n", 
            StatusBlock.Information, Status, OutBufferLength));

    Irp->IoStatus.Information = OutBufferLength; 
    ZwClose(FileHandle);
CleanUp:

    if (pwcFileName) {
        ExFreePoolWithTag(pwcFileName, TAG_GENERIC_PAGED);
    }
    
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

#if DBG
BOOLEAN
ValidateDataWOSTransition(
    PDEV_CONTEXT pDevContext
    )
{
    PCHANGE_NODE ChangeNode = NULL;
    PKDIRTY_BLOCK CurrDirtyBlock = NULL;
    PLIST_ENTRY ListEntry = pDevContext->ChangeList.Head.Flink;

    if (!IsListEmpty(&pDevContext->ChangeList.Head)) {
        while (&pDevContext->ChangeList.Head != ListEntry) {
            ChangeNode = (PCHANGE_NODE) ListEntry;
            CurrDirtyBlock = ChangeNode->DirtyBlock;
            if (CurrDirtyBlock->eWOState == ecWriteOrderStateData) {
                return FALSE; // Data WOS transition not valid as there are already some pending DBs in Data WOS
            }
            if (CurrDirtyBlock->ulDataSource != INFLTDRV_DATA_SOURCE_DATA) {
                return FALSE; // Data WOS transition not valid as there are some pending DBs in non data capture mode
            }
            ListEntry = ListEntry->Flink;
        }
    }

    ListEntry = pDevContext->ChangeList.TempQueueHead.Flink;
    if (!IsListEmpty(&pDevContext->ChangeList.TempQueueHead)) {
        while (&pDevContext->ChangeList.TempQueueHead != ListEntry) {
            ChangeNode = (PCHANGE_NODE) ListEntry;
            CurrDirtyBlock = ChangeNode->DirtyBlock;
            if (CurrDirtyBlock->eWOState == ecWriteOrderStateData) {
                return FALSE; // Data WOS transition not valid as there are already some pending DBs in Data WOS
            }
            if (CurrDirtyBlock->ulDataSource != INFLTDRV_DATA_SOURCE_DATA) {
                return FALSE; // Data WOS transition not valid as there are some pending DBs in non data capture mode
            }
            ListEntry = ListEntry->Flink;
        }
    }

    return TRUE;
}
#endif

NTSTATUS
DeviceIoControlGetVolumeWriteOrderState(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS            Status = STATUS_SUCCESS;
    PDEV_CONTEXT        pDevContext = NULL;
    petWriteOrderState  peWriteOrderState;
    KIRQL           OldIrql;

    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(etWriteOrderState)) {
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Information = 0;
        goto CleanUp;
    }
    
    
    if (0 != IoStackLocation->Parameters.DeviceIoControl.InputBufferLength) {
        pDevContext = GetDevContextForThisIOCTL(DeviceObject, Irp);
    }

    if (NULL == pDevContext) {
        Status = STATUS_NO_SUCH_DEVICE;
        Irp->IoStatus.Information = 0;
        goto CleanUp;
    }

	if (pDevContext && (pDevContext->ulFlags & DCF_FILTERING_STOPPED)) {
		Status = STATUS_NOT_SUPPORTED;
		Irp->IoStatus.Information = 0;
		goto CleanUp;
	}

    peWriteOrderState = (petWriteOrderState)Irp->AssociatedIrp.SystemBuffer;

    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
    (*peWriteOrderState) = pDevContext->eWOState;
    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);

    Irp->IoStatus.Information = sizeof(etWriteOrderState);
  
CleanUp:
    if (NULL != pDevContext) {
        DereferenceDevContext(pDevContext);
    }
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS
DeviceIoControlGetVolumeStats(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS            Status = STATUS_SUCCESS;
    PVOLUME_STATS_DATA  pVolumeStatsData;
    PDEV_CONTEXT        pDevContext = NULL;
    ULONG               ulBytesWritten = 0;
    ULONG               ulRemainingOutBytes;
    bool                bDeviceExists = true;

    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(VOLUME_STATS_DATA)) {
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    if (0 != IoStackLocation->Parameters.DeviceIoControl.InputBufferLength) {
        pDevContext = GetDevContextForThisIOCTL(DeviceObject, Irp);
        if (NULL == pDevContext)
            bDeviceExists = false;
    }

    pVolumeStatsData = (PVOLUME_STATS_DATA)Irp->AssociatedIrp.SystemBuffer;
    RtlZeroMemory(pVolumeStatsData, sizeof(VOLUME_STATS_DATA));
    pVolumeStatsData->usMajorVersion = VOLUME_STATS_DATA_MAJOR_VERSION;
    pVolumeStatsData->usMinorVersion = VOLUME_STATS_DATA_MINOR_VERSION;
    pVolumeStatsData->ulVolumesReturned = 0;
    pVolumeStatsData->ullPersistedTimeStampAfterBoot = DriverContext.ullPersistedTimeStampAfterBoot;
    pVolumeStatsData->PersistentRegistryCreated = DriverContext.PersistentRegistryCreatedFlag;
    pVolumeStatsData->ullPersistedSequenceNumberAfterBoot = DriverContext.ullPersistedSequenceNumberAfterBoot;
    pVolumeStatsData->LastShutdownMarker = DriverContext.LastShutdownMarker;
    pVolumeStatsData->ulTotalVolumes = DriverContext.ulNumDevs;
    pVolumeStatsData->ulCommonBootCounter = DriverContext.ulBootCounter;
    pVolumeStatsData->eServiceState = DriverContext.eServiceState;
    pVolumeStatsData->lDirtyBlocksAllocated = DriverContext.lDirtyBlocksAllocated;
    pVolumeStatsData->lChangeNodesAllocated = DriverContext.lChangeNodesAllocated;
    pVolumeStatsData->lChangeBlocksAllocated = DriverContext.lChangeBlocksAllocated;
    pVolumeStatsData->lBitmapWorkItemsAllocated = DriverContext.lBitmapWorkItemsAllocated;
    pVolumeStatsData->ulNonPagedMemoryLimitInMB = DriverContext.ulMaxNonPagedPool / MEGABYTES;
    pVolumeStatsData->lNonPagedMemoryAllocated = DriverContext.lNonPagedMemoryAllocated;
    pVolumeStatsData->ulNumMemoryAllocFailures = DriverContext.ulNumMemoryAllocFailures;
    pVolumeStatsData->liNonPagedLimitReachedTSforFirstTime.QuadPart = DriverContext.liNonPagedLimitReachedTSforFirstTime.QuadPart;
    pVolumeStatsData->liNonPagedLimitReachedTSforLastTime.QuadPart = DriverContext.liNonPagedLimitReachedTSforLastTime.QuadPart;
    pVolumeStatsData->ullDataPoolSizeAllocated = DriverContext.ulDataBlockAllocated * (ULONGLONG)DriverContext.ulDataBlockSize;
    pVolumeStatsData->ullDataPoolSizeFree = ( DriverContext.ulDataBlocksInFreeDataBlockList + DriverContext.ulDataBlocksInLockedDataBlockList ) * (ULONGLONG)DriverContext.ulDataBlockSize;
    pVolumeStatsData->ullDataPoolSizeFreeAndLocked = DriverContext.ulDataBlocksInLockedDataBlockList * (ULONGLONG)DriverContext.ulDataBlockSize;
	pVolumeStatsData->ulDaBInOrphanedMappedDBList = DriverContext.ulDataBlocksInOrphanedMappedDataBlockList;
    pVolumeStatsData->ulDriverFlags = 0;
	pVolumeStatsData->ulNumOfFreePageNodes = DriverContext.ulNumOfFreePageNodes;
	pVolumeStatsData->ulNumOfAllocPageNodes = DriverContext.ulNumOfAllocPageNodes;
    KIRQL       Irql;
    KeAcquireSpinLock(&DriverContext.DataBlockCounterLock, &Irql);
    pVolumeStatsData->LockedDataBlockCounter = DriverContext.LockedDataBlockCounter;
    pVolumeStatsData->MaxLockedDataBlockCounter = DriverContext.MaxLockedDataBlockCounter;
    pVolumeStatsData->MappedDataBlockCounter = DriverContext.MappedDataBlockCounter;
    pVolumeStatsData->MaxMappedDataBlockCounter = DriverContext.MaxMappedDataBlockCounter;
    KeReleaseSpinLock(&DriverContext.DataBlockCounterLock, Irql);

	pVolumeStatsData->AsyncTagIOCTLCount = DriverContext.ulNumberofAsyncTagIOCTLs;
    if (!DriverContext.bEnableDataCapture) {
        pVolumeStatsData->ulDriverFlags |= VSDF_DRIVER_DISABLED_DATA_CAPTURE;
    }

    if (!DriverContext.bEnableDataFiles) {
        pVolumeStatsData->ulDriverFlags |= VSDF_DRIVER_DISABLED_DATA_FILES;
    }

    if (DriverContext.ulFlags & DC_FLAGS_DATAPOOL_ALLOCATION_FAILED) {
        pVolumeStatsData->ulDriverFlags |= VSDF_DRIVER_FAILED_TO_ALLOCATE_DATA_POOL;
    }

    if (ecServiceRunning == DriverContext.eServiceState) {
        if (!DriverContext.bServiceSupportsDataCapture) {
            pVolumeStatsData->ulDriverFlags |= VSDF_SERVICE_DISABLED_DATA_MODE;
        }

        if (NULL == DriverContext.ProcessStartIrp) {
            pVolumeStatsData->ulDriverFlags |= VSDF_SERVICE2_DID_NOT_REGISTER_FOR_DATA_MODE;
        }

        if (!DriverContext.bServiceSupportsDataFiles) {
            pVolumeStatsData->ulDriverFlags |= VSDF_SERVICE_DISABLED_DATA_FILES;
        }

        if (!DriverContext.bS2SupportsDataFiles) {
            pVolumeStatsData->ulDriverFlags |= VSDF_SERVICE2_DID_NOT_REGISTER_FOR_DATA_FILES;
        }
    }

    // Disk filter mode
    pVolumeStatsData->eDiskFilterMode = DriverContext.eDiskFilterMode;
    pVolumeStatsData->ulNumProtectedDisk = (USHORT)InDskFltGetNumFilteredDevices();

    ulBytesWritten = sizeof(VOLUME_STATS_DATA);
    if (!bDeviceExists)
        goto CleanUp;

    ulRemainingOutBytes = IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength - sizeof(VOLUME_STATS_DATA);

    if (NULL == pDevContext) {
        LIST_ENTRY  VolumeNodeList;

        // Fill stats of all devices into output buffer
PAGED_CODE();

        InitializeListHead(&VolumeNodeList);
        Status = GetListOfDevs(&VolumeNodeList);
        if (STATUS_SUCCESS == Status) {
            while(!IsListEmpty(&VolumeNodeList)) {
                PLIST_NODE      pNode;

                pNode = (PLIST_NODE)RemoveHeadList(&VolumeNodeList);
                pDevContext = (PDEV_CONTEXT)pNode->pData;
                DereferenceListNode(pNode);

                if (ulRemainingOutBytes >= sizeof(VOLUME_STATS)) {
                    PVOLUME_STATS   pVolumeStats = (PVOLUME_STATS)((PUCHAR)Irp->AssociatedIrp.SystemBuffer + ulBytesWritten);
                    RtlZeroMemory(pVolumeStats, sizeof(VOLUME_STATS));
                    FillVolumeStatsStructure(pVolumeStats, pDevContext);
                    ulRemainingOutBytes -= sizeof(VOLUME_STATS);
                    ulBytesWritten += sizeof(VOLUME_STATS);
#ifdef VOLUME_FLT
                    if (pDevContext->GUIDList) {
                        PUCHAR          pGUIDlist = ((PUCHAR)Irp->AssociatedIrp.SystemBuffer + ulBytesWritten);
                        ULONG           ulGUIDbytesWritten = FillAdditionalDeviceIDs(pDevContext, pVolumeStats->ulAdditionalGUIDsReturned,
                                                                                       pGUIDlist, ulRemainingOutBytes);
                        if (ulGUIDbytesWritten) {
                            ulBytesWritten += ulGUIDbytesWritten;
                            ulRemainingOutBytes -= ulGUIDbytesWritten;
                        }
                    }
#endif
                    pVolumeStatsData->ulVolumesReturned++;
                }

                DereferenceDevContext(pDevContext);
            } // for each volume
        }
    } else {
        // Requested volume stats for one volume
        if (ulRemainingOutBytes >= sizeof(VOLUME_STATS)) {
            PVOLUME_STATS   pVolumeStats = (PVOLUME_STATS)((PUCHAR)Irp->AssociatedIrp.SystemBuffer + ulBytesWritten);

            FillVolumeStatsStructure(pVolumeStats, pDevContext);
            pVolumeStatsData->ulVolumesReturned++;
            ulRemainingOutBytes -= sizeof(VOLUME_STATS);
            ulBytesWritten += sizeof(VOLUME_STATS);
        }

        DereferenceDevContext(pDevContext);
    }
CleanUp:
    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = ulBytesWritten;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}
/*
- When WOState is Data or Metadata, return oldest change Timestamp
- WOState is Bitmap, return the last commited Timestamp drained in data or metadata
*/
NTSTATUS
DeviceIoControlGetAdditionalDeviceStats(
    PDEVICE_OBJECT  DeviceObject,
    PIRP            Irp
    )
{
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS Status = STATUS_SUCCESS;
    PDEV_CONTEXT pDevContext = NULL;
    PVOLUME_STATS_ADDITIONAL_INFO pVolumeAdditionalInfo;
    KIRQL OldIrql;

    if (ecServiceRunning != DriverContext.eServiceState) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_DIRTY_BLOCKS, 
            ("DeviceIoControlGetAdditionalDeviceStats: Service is not running\n"));
        Status = STATUS_INVALID_DEVICE_STATE;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(VOLUME_STATS_ADDITIONAL_INFO)) {
        Status = STATUS_BUFFER_TOO_SMALL;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }
    if (0 != IoStackLocation->Parameters.DeviceIoControl.InputBufferLength) {
        pDevContext = GetDevContextForThisIOCTL(DeviceObject, Irp);
	} else {
		Status = STATUS_INFO_LENGTH_MISMATCH;
		Irp->IoStatus.Information = 0;
		goto CleanUp;
	}

    if (NULL == pDevContext) {
        Status = STATUS_NO_SUCH_DEVICE;
        Irp->IoStatus.Information = 0;
        goto CleanUp;
    }
    if (pDevContext->ulFlags & DCF_FILTERING_STOPPED) {
        DereferenceDevContext(pDevContext);
        Status = STATUS_DEVICE_BUSY;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }
    ULONGLONG TimeInHundNanoSecondsFromJan1601;
    //When system time has moved back, oldest timestamp and current timestamp has gone back may lead to RPO '0'
    // Resync is marked and yet to drain, in this case oldest timestamp would be '0' at least till resync is completed

    KeQuerySystemTime((PLARGE_INTEGER)&TimeInHundNanoSecondsFromJan1601);
    if ((TimeInHundNanoSecondsFromJan1601 < DriverContext.ullLastTimeStamp) || (true == pDevContext->bResyncRequired)) {
        Status = STATUS_NOT_SUPPORTED;
        DereferenceDevContext(pDevContext);
        Irp->IoStatus.Information = 0;
        goto CleanUp;
    }

    pVolumeAdditionalInfo = (PVOLUME_STATS_ADDITIONAL_INFO)Irp->AssociatedIrp.SystemBuffer;
    PKDIRTY_BLOCK pPendingDB = NULL;
    ULONGLONG   CurrentTimeStamp;
    ULONGLONG   CurrentSequenceNumber;
    ULONGLONG   OldestChangeTS = 0;

    PDEV_BITMAP pDevBitmap = NULL;
    LARGE_INTEGER ulicbChangesInBitmap;
    ulicbChangesInBitmap.QuadPart = 0;	
    bool bIsWOStateBitmap = false;

    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
    if (pDevContext->pDevBitmap) {
        pDevBitmap = ReferenceDevBitmap(pDevContext->pDevBitmap);
        if (ecWriteOrderStateBitmap == pDevContext->eWOState)
            bIsWOStateBitmap = true;
    }
    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);

   // Read bitmap and find any changes lying on it.
   // Total changes includes changes lying in memory and bitmap

    KeWaitForSingleObject(&pDevContext->SyncEvent, Executive, KernelMode, FALSE, NULL);

    if (NULL != pDevBitmap) {
        if (bIsWOStateBitmap) {
            ExAcquireFastMutex(&pDevBitmap->Mutex);
            if (pDevBitmap->pBitmapAPI)
                ulicbChangesInBitmap.QuadPart = pDevBitmap->pBitmapAPI->GetDatBytesInBitmap();
            ExReleaseFastMutex(&pDevBitmap->Mutex);
        }
        DereferenceDevBitmap(pDevBitmap);
    } 

    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);

    GenerateTimeStamp(CurrentTimeStamp, CurrentSequenceNumber);
    pVolumeAdditionalInfo->ullTotalChangesPending = pDevContext->ChangeList.ullcbTotalChangesPending + ulicbChangesInBitmap.QuadPart;

    if (pVolumeAdditionalInfo->ullTotalChangesPending) {
        PCHANGE_NODE pFirstChangeNode = NULL;
        PKDIRTY_BLOCK pFirstDB = NULL;

        if (!IsListEmpty(&pDevContext->ChangeList.Head)) {
            pFirstChangeNode = (PCHANGE_NODE)pDevContext->ChangeList.Head.Flink;;
            pFirstDB = pFirstChangeNode->DirtyBlock;
        }

        if (pDevContext->pDBPendingTransactionConfirm) {
            pPendingDB = pDevContext->pDBPendingTransactionConfirm;

            if ((ecWriteOrderStateData == pDevContext->eWOState) || (ecWriteOrderStateData == pPendingDB->eWOState)) {
                OldestChangeTS = pPendingDB->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601;
            } else if (ecWriteOrderStateMetadata == pDevContext->eWOState) {
                if (ecWriteOrderStateMetadata == pPendingDB->eWOState) {
                    OldestChangeTS = pDevContext->MDFirstChangeTS;
                    if (NULL != pFirstDB)
                        OldestChangeTS = ((OldestChangeTS < pFirstDB->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601) ? 
                                               OldestChangeTS : pFirstDB->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
                } else if (ecWriteOrderStateBitmap == pPendingDB->eWOState){
                    // When bitmap changes are completely drained, we may find the DirtyBlock with WO state Bitmap
                    OldestChangeTS = pDevContext->MDFirstChangeTS;
                }
            } else { 
                if (ecWriteOrderStateMetadata == pPendingDB->eWOState) { 
                    OldestChangeTS = pDevContext->MDFirstChangeTS;
                    if (NULL != pFirstDB)
                        OldestChangeTS = ((OldestChangeTS < pFirstDB->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601) ? 
                                               OldestChangeTS : pFirstDB->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
                } else if (ecWriteOrderStateBitmap == pPendingDB->eWOState){
                    OldestChangeTS = pDevContext->EndTimeStampofDB;
                    if ((0 != OldestChangeTS) && (NULL != pFirstDB)) {
                        if (ecWriteOrderStateBitmap != pFirstDB->eWOState) { // This is MD WOS DirtyBlock
                            OldestChangeTS = ((OldestChangeTS < pFirstDB->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601) ? 
                                                  OldestChangeTS : pFirstDB->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
                        }
                    }
                }
            }
            if (0 == OldestChangeTS) {
                ULONGLONG FirstDBTS = 0;
                if (NULL != pFirstDB)
                     FirstDBTS = pFirstDB->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601;

                InDskFltWriteEvent(INDSKFLT_WARN_GET_ADDITIONAL_STATS_FAIL1,
                    pDevContext->EndTimeStampofDB, FirstDBTS, pDevContext->eWOState, pPendingDB->eWOState);
            }
        } else {
            ASSERT(NULL != pFirstDB);
            if (NULL != pFirstDB) {
                // if WriteOrder State is Data/metadata, We should consider First change timestamp of First DB in the memory 
                if ((ecWriteOrderStateData == pDevContext->eWOState) || (ecWriteOrderStateMetadata == pDevContext->eWOState)) {
                    OldestChangeTS = pFirstDB->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601;
                } else {
                    if ((ecWriteOrderStateData == pFirstDB->eWOState) || (ecWriteOrderStateMetadata == pFirstDB->eWOState))
                        OldestChangeTS = pFirstDB->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601;
                    else 
                        OldestChangeTS = pDevContext->EndTimeStampofDB;
                }
            } else { // Bitmap has changes
                OldestChangeTS = pDevContext->EndTimeStampofDB;
            }
            if (0 == OldestChangeTS) {
                ULONGLONG FirstDBTS = 0;
                ULONG FirstDBWOState = 4;
                if (NULL != pFirstDB) {
                     FirstDBTS = pFirstDB->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601;
                     FirstDBWOState = pFirstDB->eWOState;
                }
        
                InDskFltWriteEvent(INDSKFLT_WARN_GET_ADDITIONAL_STATS_FAIL2,
                    pDevContext->EndTimeStampofDB, FirstDBTS, pDevContext->eWOState, FirstDBWOState);
            }
         }
        pVolumeAdditionalInfo->ullOldestChangeTimeStamp =  OldestChangeTS;
    } else {
        pVolumeAdditionalInfo->ullOldestChangeTimeStamp = CurrentTimeStamp;
    }

    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);

    pVolumeAdditionalInfo->ullDriverCurrentTimeStamp = CurrentTimeStamp;

    KeSetEvent(&pDevContext->SyncEvent, IO_NO_INCREMENT, FALSE);

    Irp->IoStatus.Information = sizeof(VOLUME_STATS_ADDITIONAL_INFO);
    DereferenceDevContext(pDevContext);
CleanUp:
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

// This IOCTL gets ther Driver global Time Stamp and Sequence number
NTSTATUS
DeviceIoControlGetGlobalTSSN(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS                    Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION          IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    PDRIVER_GLOBAL_TIMESTAMP    pGlobalTSSN = (PDRIVER_GLOBAL_TIMESTAMP)Irp->AssociatedIrp.SystemBuffer;

    UNREFERENCED_PARAMETER(DeviceObject);

    ASSERT(IOCTL_INMAGE_GET_GLOBAL_TIMESTAMP == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(DRIVER_GLOBAL_TIMESTAMP)) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL, 
            ("DeviceIoControlGetGlobalTSSN: IOCTL rejected, output buffer too small\n"));
        Status = STATUS_BUFFER_TOO_SMALL;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }
	RtlZeroMemory(pGlobalTSSN, sizeof(DRIVER_GLOBAL_TIMESTAMP));
    GenerateTimeStamp(pGlobalTSSN->TimeInHundNanoSecondsFromJan1601, pGlobalTSSN->ullSequenceNumber);

    Irp->IoStatus.Information = sizeof(DRIVER_GLOBAL_TIMESTAMP);
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS
DeviceIoControlSetIoSizeArray(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    PSET_IO_SIZE_ARRAY_INPUT    pInputBuffer = (PSET_IO_SIZE_ARRAY_INPUT)Irp->AssociatedIrp.SystemBuffer;
    ULONG               ulMaxBuckets = MAX_DC_IO_SIZE_BUCKETS - 1;

    UNREFERENCED_PARAMETER(DeviceObject);

    ASSERT(IOCTL_INMAGE_SET_IO_SIZE_ARRAY == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);
    if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < sizeof(SET_IO_SIZE_ARRAY_INPUT)) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL, 
            ("%s: IOCTL rejected, input buffer(%#x) too small. Expecting(%#x)\n", __FUNCTION__, 
             IoStackLocation->Parameters.DeviceIoControl.InputBufferLength, sizeof(SET_IO_SIZE_ARRAY_INPUT)));
        Status = STATUS_BUFFER_TOO_SMALL;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    if (ulMaxBuckets > USER_MODE_MAX_IO_BUCKETS) {
        ulMaxBuckets = USER_MODE_MAX_IO_BUCKETS;
    }
    if (ulMaxBuckets > pInputBuffer->ulArraySize) {
        ulMaxBuckets = pInputBuffer->ulArraySize;
    }

    if(pInputBuffer->ulFlags & SET_IO_SIZE_ARRAY_WRITE_INPUT)
    {
        if (pInputBuffer->ulFlags & SET_IO_SIZE_ARRAY_INPUT_FLAGS_VALID_GUID) {
            PDEV_CONTEXT        DevContext = NULL;
            KIRQL               OldIrql;

            // We have to set the data size limit for this volume
#ifdef VOLUME_FLT
			DevContext = GetDevContextWithGUID(&pInputBuffer->DevID[0]);
#else
            DevContext = GetDevContextWithGUID(&pInputBuffer->DevID[0], (GUID_SIZE_IN_BYTES + 4));
#endif
            if (DevContext) {
                KeAcquireSpinLock(&DevContext->Lock, &OldIrql);

                for (ULONG ul = 0; ul < ulMaxBuckets; ul++) {
                    DevContext->ulIoSizeArray[ul] = pInputBuffer->ulIoSizeArray[ul];
                    DevContext->ullIoSizeCounters[ul] = 0;
                }

                for (ULONG ul = ulMaxBuckets; ul < MAX_DC_IO_SIZE_BUCKETS; ul++) {
                    DevContext->ulIoSizeArray[ul] = 0;
                    DevContext->ullIoSizeCounters[ul] = 0;
                }

                KeReleaseSpinLock(&DevContext->Lock, OldIrql);
                DereferenceDevContext(DevContext);
            } else {
                Status = STATUS_DEVICE_DOES_NOT_EXIST;
            }
        } else {
            LIST_ENTRY  VolumeNodeList;
            Status = GetListOfDevs(&VolumeNodeList);
            if (STATUS_SUCCESS == Status) {
                while(!IsListEmpty(&VolumeNodeList)) {
                    PLIST_NODE  pNode = (PLIST_NODE)RemoveHeadList(&VolumeNodeList);;
                    PDEV_CONTEXT DevContext = (PDEV_CONTEXT)pNode->pData;
                    KIRQL           OldIrql;

                    DereferenceListNode(pNode);

                    KeAcquireSpinLock(&DevContext->Lock, &OldIrql);

                    for (ULONG ul = 0; ul < ulMaxBuckets; ul++) {
                        DevContext->ulIoSizeArray[ul] = pInputBuffer->ulIoSizeArray[ul];
                        DevContext->ullIoSizeCounters[ul] = 0;
                    }

                    for (ULONG ul = ulMaxBuckets; ul < MAX_DC_IO_SIZE_BUCKETS; ul++) {
                        DevContext->ulIoSizeArray[ul] = 0;
                        DevContext->ullIoSizeCounters[ul] = 0;
                    }

                    KeReleaseSpinLock(&DevContext->Lock, OldIrql);
                    DereferenceDevContext(DevContext);
                } // for each volume
            }
        }
    }
    
    if(pInputBuffer->ulFlags & SET_IO_SIZE_ARRAY_READ_INPUT)
    {
        DEVICE_PROFILE_INITIALIZER DeviceProfileInitializer;
        if (pInputBuffer->ulFlags & SET_IO_SIZE_ARRAY_INPUT_FLAGS_VALID_GUID) {
            PDEV_CONTEXT     DevContext = NULL;
            KIRQL               OldIrql;
            // We have to set the data size limit for this volume
#ifdef VOLUME_FLT
			DevContext = GetDevContextWithGUID(&pInputBuffer->DevID[0]);
#else
            DevContext = GetDevContextWithGUID(&pInputBuffer->DevID[0], (GUID_SIZE_IN_BYTES + 4));
#endif
            if (DevContext) {
                KeAcquireSpinLock(&DevContext->Lock, &OldIrql);

                DeviceProfileInitializer.Type = READ_BUCKET_INITIALIZER;
                DeviceProfileInitializer.Profile.IoReadArraySizeInput = *pInputBuffer;
                InitializeProfiler(&DevContext->DeviceProfile, &DeviceProfileInitializer);
                
                KeReleaseSpinLock(&DevContext->Lock, OldIrql);
                DereferenceDevContext(DevContext);
            } else {
                Status = STATUS_DEVICE_DOES_NOT_EXIST;
            }
        } else {
            LIST_ENTRY  VolumeNodeList;
            Status = GetListOfDevs(&VolumeNodeList);
            if (STATUS_SUCCESS == Status) {
                while(!IsListEmpty(&VolumeNodeList)) {
                    PLIST_NODE  pNode = (PLIST_NODE)RemoveHeadList(&VolumeNodeList);;
                    PDEV_CONTEXT DevContext = (PDEV_CONTEXT)pNode->pData;
                    KIRQL           OldIrql;

                    DereferenceListNode(pNode);

                    KeAcquireSpinLock(&DevContext->Lock, &OldIrql);

                    DeviceProfileInitializer.Type = READ_BUCKET_INITIALIZER;
                    DeviceProfileInitializer.Profile.IoReadArraySizeInput = *pInputBuffer;
                    InitializeProfiler(&DevContext->DeviceProfile, &DeviceProfileInitializer);

                    KeReleaseSpinLock(&DevContext->Lock, OldIrql);
                    DereferenceDevContext(DevContext);
                } // for each volume
            }
        }
    }

    Status = STATUS_SUCCESS;
    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS
DeviceIoControlSetDataSizeToDiskLimit(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    SET_DATA_TO_DISK_SIZE_LIMIT_INPUT   Input = {0};
    PSET_DATA_TO_DISK_SIZE_LIMIT_INPUT  pInputBuffer = (PSET_DATA_TO_DISK_SIZE_LIMIT_INPUT)Irp->AssociatedIrp.SystemBuffer;
    PSET_DATA_TO_DISK_SIZE_LIMIT_OUTPUT pOutputBuffer = (PSET_DATA_TO_DISK_SIZE_LIMIT_OUTPUT)Irp->AssociatedIrp.SystemBuffer;

    UNREFERENCED_PARAMETER(DeviceObject);

    ASSERT(IOCTL_INMAGE_SET_DATA_SIZE_TO_DISK_LIMIT == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);
    if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < sizeof(SET_DATA_TO_DISK_SIZE_LIMIT_INPUT)) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL, 
            ("%s: IOCTL rejected, input buffer(%#x) too small. Expecting(%#x)\n", __FUNCTION__, 
             IoStackLocation->Parameters.DeviceIoControl.InputBufferLength, sizeof(SET_DATA_TO_DISK_SIZE_LIMIT_INPUT)));
        Status = STATUS_BUFFER_TOO_SMALL;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    Input.ulFlags = pInputBuffer->ulFlags;
	RtlCopyMemory(&Input.DevID, &pInputBuffer->DevID, GUID_SIZE_IN_BYTES);
    Input.ulDataToDiskSizeLimit = pInputBuffer->ulDataToDiskSizeLimit;
    Input.ulDataToDiskSizeLimitForAllDevices = pInputBuffer->ulDataToDiskSizeLimitForAllDevices;
    Input.ulDefaultDataToDiskSizeLimit = pInputBuffer->ulDefaultDataToDiskSizeLimit;

    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(SET_DATA_TO_DISK_SIZE_LIMIT_OUTPUT)) {
        pOutputBuffer = NULL;
        Irp->IoStatus.Information = 0;
    } else {
 		RtlZeroMemory(pOutputBuffer, sizeof(SET_DATA_TO_DISK_SIZE_LIMIT_OUTPUT));
        Irp->IoStatus.Information = sizeof(SET_DATA_TO_DISK_SIZE_LIMIT_OUTPUT);
    }

    // First set the default value if asked.
    if (Input.ulDefaultDataToDiskSizeLimit) {
        Registry    *pDriverParam = NULL;

        Status = OpenDriverParametersRegKey(&pDriverParam);
        if (NT_SUCCESS(Status)) {
            Status = pDriverParam->WriteULONG(DEVICE_DATA_TO_DISK_LIMIT_IN_MB, Input.ulDefaultDataToDiskSizeLimit);
            DriverContext.ulDataToDiskLimitPerDevInMB = Input.ulDefaultDataToDiskSizeLimit;
            if (!NT_SUCCESS(Status) && (NULL != pOutputBuffer)) {
                pOutputBuffer->ulErrorInSettingDefault = Status;
            }
        } else {
            if (pOutputBuffer) {
                pOutputBuffer->ulErrorInSettingDefault = Status;
            }
        }

        if (NULL != pDriverParam) {
            delete pDriverParam;
        }
    }
    // Next set the data to disk size limit for all volumes
    if (Input.ulDataToDiskSizeLimitForAllDevices) {
        LIST_ENTRY  VolumeNodeList;
        Status = GetListOfDevs(&VolumeNodeList);
        if (STATUS_SUCCESS == Status) {
            while(!IsListEmpty(&VolumeNodeList)) {
                PLIST_NODE  pNode = (PLIST_NODE)RemoveHeadList(&VolumeNodeList);;
                PDEV_CONTEXT DevContext = (PDEV_CONTEXT)pNode->pData;

                DereferenceListNode(pNode);

                if (DevContext->ulFlags & DCF_DEVID_OBTAINED) {
                    Status = KeWaitForSingleObject(&DevContext->Mutex, Executive, KernelMode, FALSE, NULL);
                    ASSERT(STATUS_SUCCESS == Status);

                    DevContext->ullcbDataToDiskLimit = ((ULONGLONG)Input.ulDataToDiskSizeLimitForAllDevices) * MEGABYTES;
                    SetDWORDInRegVolumeCfg(DevContext, DEVICE_DATA_TO_DISK_LIMIT_IN_MB, Input.ulDataToDiskSizeLimitForAllDevices);
                    KeReleaseMutex(&DevContext->Mutex, FALSE);
                }
                DereferenceDevContext(DevContext);
            } // for each volume
        }
    }
    // Last set the data to disk size limit for specific volume
    if (Input.ulFlags & SET_DATA_TO_DISK_SIZE_LIMIT_FLAGS_VALID_GUID) {
        PDEV_CONTEXT     DevContext = NULL;
        // We have to set the data size limit for this volume
#ifdef VOLUME_FLT
		DevContext = GetDevContextWithGUID(&Input.DevID[0]);
#else
        DevContext = GetDevContextWithGUID(&Input.DevID[0], (GUID_SIZE_IN_BYTES + 4));
#endif
        if (DevContext) {
            Status = KeWaitForSingleObject(&DevContext->Mutex, Executive, KernelMode, FALSE, NULL);
            ASSERT(STATUS_SUCCESS == Status);

            DevContext->ullcbDataToDiskLimit = ((ULONGLONG)Input.ulDataToDiskSizeLimit) * MEGABYTES;
            SetDWORDInRegVolumeCfg(DevContext, DEVICE_DATA_TO_DISK_LIMIT_IN_MB, Input.ulDataToDiskSizeLimit);
            KeReleaseMutex(&DevContext->Mutex, FALSE);
            DereferenceDevContext(DevContext);
        } else {
            if (pOutputBuffer) {
                pOutputBuffer->ulErrorInSettingForDev = (ULONG)STATUS_DEVICE_DOES_NOT_EXIST;
            }
        }
    }

    Status = STATUS_SUCCESS;
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}


NTSTATUS
DeviceIoControlSetMinFreeDataSizeToDiskLimit(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_INPUT   Input = {0};
    PSET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_INPUT  pInputBuffer = (PSET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_INPUT)Irp->AssociatedIrp.SystemBuffer;
    PSET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_OUTPUT pOutputBuffer = (PSET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_OUTPUT)Irp->AssociatedIrp.SystemBuffer;

    UNREFERENCED_PARAMETER(DeviceObject);

    ASSERT(IOCTL_INMAGE_SET_MIN_FREE_DATA_SIZE_TO_DISK_LIMIT == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);
    if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < sizeof(SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_INPUT)) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL, 
            ("%s: IOCTL rejected, input buffer(%#x) too small. Expecting(%#x)\n", __FUNCTION__, 
             IoStackLocation->Parameters.DeviceIoControl.InputBufferLength, sizeof(SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_INPUT)));
        Status = STATUS_BUFFER_TOO_SMALL;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    Input.ulFlags = pInputBuffer->ulFlags;

    RtlCopyMemory(&Input.DevID, &pInputBuffer->DevID, GUID_SIZE_IN_BYTES);
    Input.ulMinFreeDataToDiskSizeLimit = pInputBuffer->ulMinFreeDataToDiskSizeLimit;
    Input.ulMinFreeDataToDiskSizeLimitForAllDevices = pInputBuffer->ulMinFreeDataToDiskSizeLimitForAllDevices;
    Input.ulDefaultMinFreeDataToDiskSizeLimit = pInputBuffer->ulDefaultMinFreeDataToDiskSizeLimit;

    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_OUTPUT)) {
        pOutputBuffer = NULL;
        Irp->IoStatus.Information = 0;
    } else {
		RtlZeroMemory(pOutputBuffer, sizeof(SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_OUTPUT));
        Irp->IoStatus.Information = sizeof(SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_OUTPUT);
    }

    Status = ValidateParameters(Input,pOutputBuffer);
    if (!NT_SUCCESS(Status))
        goto cleanup;
    // First set the default value if asked.
    if (Input.ulDefaultMinFreeDataToDiskSizeLimit) {
        Registry    *pDriverParam = NULL;
        Status = OpenDriverParametersRegKey(&pDriverParam);
        if (NT_SUCCESS(Status)) {
            Status = pDriverParam->WriteULONG(DEVICE_MIN_FREE_DATA_TO_DISK_LIMIT_IN_MB, Input.ulDefaultMinFreeDataToDiskSizeLimit);
            DriverContext.ulMinFreeDataToDiskLimitPerDevInMB = Input.ulDefaultMinFreeDataToDiskSizeLimit;
            if (!NT_SUCCESS(Status) && (NULL != pOutputBuffer)) {
                pOutputBuffer->ulErrorInSettingDefault = Status;
            }
        } else {
            if (pOutputBuffer) {
                pOutputBuffer->ulErrorInSettingDefault = Status;
            }
        }

        if (NULL != pDriverParam) {
            delete pDriverParam;
        }
    }
    // Next set the data to disk size limit for all volumes
    if (Input.ulMinFreeDataToDiskSizeLimitForAllDevices) {
        LIST_ENTRY  VolumeNodeList;
        Status = GetListOfDevs(&VolumeNodeList);
        if (STATUS_SUCCESS == Status) {
            while(!IsListEmpty(&VolumeNodeList)) {
                PLIST_NODE  pNode = (PLIST_NODE)RemoveHeadList(&VolumeNodeList);;
                PDEV_CONTEXT DevContext = (PDEV_CONTEXT)pNode->pData;
                DereferenceListNode(pNode);
                if (DevContext->ulFlags & DCF_DEVID_OBTAINED) {
                    Status = KeWaitForSingleObject(&DevContext->Mutex, Executive, KernelMode, FALSE, NULL);
                    ASSERT(STATUS_SUCCESS == Status);
                    DevContext->ullcbMinFreeDataToDiskLimit = ((ULONGLONG)Input.ulMinFreeDataToDiskSizeLimitForAllDevices) * MEGABYTES;
                    SetDWORDInRegVolumeCfg(DevContext, DEVICE_MIN_FREE_DATA_TO_DISK_LIMIT_IN_MB, Input.ulMinFreeDataToDiskSizeLimitForAllDevices);
                    KeReleaseMutex(&DevContext->Mutex, FALSE);
                }
                DereferenceDevContext(DevContext);
            } // for each volume
        }
    }
    // Last set the data to disk size limit for specific volume
    if (Input.ulFlags & SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_FLAGS_VALID_GUID) {
        PDEV_CONTEXT     DevContext = NULL;
        // We have to set the data size limit for this volume
#ifdef VOLUME_FLT
		DevContext = GetDevContextWithGUID(&Input.DevID[0]);
#else
		DevContext = GetDevContextWithGUID(&Input.DevID[0], (GUID_SIZE_IN_BYTES + 4));
#endif
        if (DevContext) {
            Status = KeWaitForSingleObject(&DevContext->Mutex, Executive, KernelMode, FALSE, NULL);
            ASSERT(STATUS_SUCCESS == Status);

            DevContext->ullcbMinFreeDataToDiskLimit= ((ULONGLONG)Input.ulMinFreeDataToDiskSizeLimit) * MEGABYTES;
            SetDWORDInRegVolumeCfg(DevContext, DEVICE_MIN_FREE_DATA_TO_DISK_LIMIT_IN_MB, Input.ulMinFreeDataToDiskSizeLimit);
            KeReleaseMutex(&DevContext->Mutex, FALSE);
            DereferenceDevContext(DevContext);
        } else {
            if (pOutputBuffer) {
                pOutputBuffer->ulErrorInSettingForDev = (ULONG)STATUS_DEVICE_DOES_NOT_EXIST;
            }
        }
    }
   
cleanup:
    Status = STATUS_SUCCESS;
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}


NTSTATUS
DeviceIoControlSetDataNotifyLimit(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    SET_DATA_NOTIFY_SIZE_LIMIT_INPUT   Input = {0};
    PSET_DATA_NOTIFY_SIZE_LIMIT_INPUT  pInputBuffer = (PSET_DATA_NOTIFY_SIZE_LIMIT_INPUT)Irp->AssociatedIrp.SystemBuffer;
    PSET_DATA_NOTIFY_SIZE_LIMIT_OUTPUT pOutputBuffer = (PSET_DATA_NOTIFY_SIZE_LIMIT_OUTPUT)Irp->AssociatedIrp.SystemBuffer;

    UNREFERENCED_PARAMETER(DeviceObject);

    ASSERT(IOCTL_INMAGE_SET_DATA_NOTIFY_LIMIT == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);
    if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < sizeof(SET_DATA_NOTIFY_SIZE_LIMIT_INPUT)) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL, 
            ("%s: IOCTL rejected, input buffer(%#x) too small. Expecting(%#x)\n", __FUNCTION__, 
             IoStackLocation->Parameters.DeviceIoControl.InputBufferLength, sizeof(SET_DATA_NOTIFY_SIZE_LIMIT_INPUT)));
        Status = STATUS_BUFFER_TOO_SMALL;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    Input.ulFlags = pInputBuffer->ulFlags;
	RtlCopyMemory(&Input.DevID, &pInputBuffer->DevID, GUID_SIZE_IN_BYTES);
    Input.ulLimit = pInputBuffer->ulLimit;
    Input.ulLimitForAllDevices = pInputBuffer->ulLimitForAllDevices;
    Input.ulDefaultLimit = pInputBuffer->ulDefaultLimit;

    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(SET_DATA_NOTIFY_SIZE_LIMIT_OUTPUT)) {
        pOutputBuffer = NULL;
        Irp->IoStatus.Information = 0;
    } else {
		RtlZeroMemory(pOutputBuffer, sizeof(SET_DATA_NOTIFY_SIZE_LIMIT_OUTPUT));
        Irp->IoStatus.Information = sizeof(SET_DATA_NOTIFY_SIZE_LIMIT_OUTPUT);
    }

    // First set the default value if asked.
    if (Input.ulDefaultLimit) {
        Registry    *pDriverParam = NULL;

        Status = OpenDriverParametersRegKey(&pDriverParam);
        if (NT_SUCCESS(Status)) {
            Status = pDriverParam->WriteULONG(DEVICE_DATA_NOTIFY_LIMIT_IN_KB, Input.ulDefaultLimit);
            DriverContext.ulDataNotifyThresholdInKB = Input.ulDefaultLimit;
            if (!NT_SUCCESS(Status) && (NULL != pOutputBuffer)) {
                pOutputBuffer->ulErrorInSettingDefault = Status;
            }
        } else {
            if (pOutputBuffer) {
                pOutputBuffer->ulErrorInSettingDefault = Status;
            }
        }

        if (NULL != pDriverParam) {
            delete pDriverParam;
        }
    }
    // Next set the data to disk size limit for all volumes
    if (Input.ulLimitForAllDevices) {
        LIST_ENTRY  VolumeNodeList;
        Status = GetListOfDevs(&VolumeNodeList);
        if (STATUS_SUCCESS == Status) {
            while(!IsListEmpty(&VolumeNodeList)) {
                PLIST_NODE  pNode = (PLIST_NODE)RemoveHeadList(&VolumeNodeList);;
                PDEV_CONTEXT DevContext = (PDEV_CONTEXT)pNode->pData;

                DereferenceListNode(pNode);

                if (DevContext->ulFlags & DCF_DEVID_OBTAINED) {
                    Status = KeWaitForSingleObject(&DevContext->Mutex, Executive, KernelMode, FALSE, NULL);
                    ASSERT(STATUS_SUCCESS == Status);

                    DevContext->ulcbDataNotifyThreshold = Input.ulLimitForAllDevices * KILOBYTES;
                    SetDWORDInRegVolumeCfg(DevContext, DEVICE_DATA_NOTIFY_LIMIT_IN_KB, Input.ulLimitForAllDevices);
                    KeReleaseMutex(&DevContext->Mutex, FALSE);
                }
                DereferenceDevContext(DevContext);
            } // for each volume
        }
    }
    // Last set the data to disk size limit for specific volume
    if ((Input.ulFlags & SET_DATA_NOTIFY_SIZE_LIMIT_FLAGS_VALID_GUID) && (0 != Input.ulLimit)) {
        PDEV_CONTEXT     DevContext = NULL;
        // We have to set the data size limit for this volume
#ifdef VOLUME_FLT
		DevContext = GetDevContextWithGUID(&Input.DevID[0]);
#else
		DevContext = GetDevContextWithGUID(&Input.DevID[0], (GUID_SIZE_IN_BYTES + 4));
#endif
        if (DevContext) {
            Status = KeWaitForSingleObject(&DevContext->Mutex, Executive, KernelMode, FALSE, NULL);
            ASSERT(STATUS_SUCCESS == Status);

            DevContext->ulcbDataNotifyThreshold = Input.ulLimit * KILOBYTES;
            SetDWORDInRegVolumeCfg(DevContext, DEVICE_DATA_NOTIFY_LIMIT_IN_KB, Input.ulLimit);
            KeReleaseMutex(&DevContext->Mutex, FALSE);
            DereferenceDevContext(DevContext);
        } else {
            if (pOutputBuffer) {
                pOutputBuffer->ulErrorInSettingForDev = (ULONG)STATUS_DEVICE_DOES_NOT_EXIST;
            }
        }
    }

    Status = STATUS_SUCCESS;
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS
DeviceIoControlSetDataFileThreadPriority(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    SET_DATA_FILE_THREAD_PRIORITY_INPUT Input = {0};
    // Save input flags and values in local variables. Systembuffer
    // will be used to save the output information.
    PSET_DATA_FILE_THREAD_PRIORITY_INPUT    pInputBuffer = (PSET_DATA_FILE_THREAD_PRIORITY_INPUT)Irp->AssociatedIrp.SystemBuffer;
    PSET_DATA_FILE_THREAD_PRIORITY_OUTPUT   pOutputBuffer = (PSET_DATA_FILE_THREAD_PRIORITY_OUTPUT)Irp->AssociatedIrp.SystemBuffer;

    UNREFERENCED_PARAMETER(DeviceObject);

    ASSERT(IOCTL_INMAGE_SET_DATA_FILE_THREAD_PRIORITY == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

    if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < sizeof(SET_DATA_FILE_THREAD_PRIORITY_INPUT)) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL, 
            ("%s: IOCTL rejected, input buffer(%#x) too small. Expecting(%#x)\n", __FUNCTION__, 
             IoStackLocation->Parameters.DeviceIoControl.InputBufferLength, sizeof(SET_DATA_FILE_THREAD_PRIORITY_INPUT)));
        Status = STATUS_BUFFER_TOO_SMALL;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    Input.ulFlags = pInputBuffer->ulFlags;
	RtlCopyMemory(&Input.DevID, &pInputBuffer->DevID, GUID_SIZE_IN_BYTES);
    Input.ulThreadPriority = pInputBuffer->ulThreadPriority;
    Input.ulThreadPriorityForAllDevs = pInputBuffer->ulThreadPriorityForAllDevs;
    Input.ulDefaultThreadPriority = pInputBuffer->ulDefaultThreadPriority;

    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(SET_DATA_FILE_THREAD_PRIORITY_OUTPUT)) {
        pOutputBuffer = NULL;
        Irp->IoStatus.Information = 0;
    } else {
		RtlZeroMemory(pOutputBuffer, sizeof(SET_DATA_FILE_THREAD_PRIORITY_OUTPUT));
        Irp->IoStatus.Information = sizeof(SET_DATA_FILE_THREAD_PRIORITY_OUTPUT);
    }

    // First set the default value if asked.
    if (Input.ulDefaultThreadPriority) {
        Registry    *pDriverParam = NULL;

        Status = OpenDriverParametersRegKey(&pDriverParam);
        if (NT_SUCCESS(Status)) {
            Status = pDriverParam->WriteULONG(DEVICE_FILE_WRITER_THREAD_PRIORITY, Input.ulDefaultThreadPriority);
            DriverContext.FileWriterThreadPriority = Input.ulDefaultThreadPriority;
            if (!NT_SUCCESS(Status) && (NULL != pOutputBuffer)) {
                pOutputBuffer->ulErrorInSettingDefault = Status;
            }
        } else {
            if (pOutputBuffer) {
                pOutputBuffer->ulErrorInSettingDefault = Status;
            }
        }

        if (NULL != pDriverParam) {
            delete pDriverParam;
        }
    }

    // Next set the data to disk size limit for all volumes
    if (Input.ulThreadPriorityForAllDevs) {
        LIST_ENTRY  VolumeNodeList;
        Status = GetListOfDevs(&VolumeNodeList);
        if (STATUS_SUCCESS == Status) {
            while(!IsListEmpty(&VolumeNodeList)) {
                PLIST_NODE  pNode = (PLIST_NODE)RemoveHeadList(&VolumeNodeList);;
                PDEV_CONTEXT DevContext = (PDEV_CONTEXT)pNode->pData;

                DereferenceListNode(pNode);

                if (DevContext->ulFlags & DCF_DEVID_OBTAINED) {
                    Status = KeWaitForSingleObject(&DevContext->Mutex, Executive, KernelMode, FALSE, NULL);
                    ASSERT(STATUS_SUCCESS == Status);

                    DevContext->FileWriterThreadPriority = Input.ulThreadPriorityForAllDevs;
                    SetDWORDInRegVolumeCfg(DevContext, DEVICE_FILE_WRITER_THREAD_PRIORITY, Input.ulThreadPriorityForAllDevs);

                    if (DevContext->DevFileWriter) {
                        DriverContext.DataFileWriterManager->SetFileWriterThreadPriority(DevContext->DevFileWriter);
                    }
                    KeReleaseMutex(&DevContext->Mutex, FALSE);
                }
                DereferenceDevContext(DevContext);
            } // for each volume
        }
    }

    // Last set the data to disk size limit for specific volume
    if (Input.ulFlags & SET_DATA_FILE_THREAD_PRIORITY_FLAGS_VALID_GUID) {
        PDEV_CONTEXT     DevContext = NULL;
        // We have to set the data size limit for this volume
#ifdef VOLUME_FLT
		DevContext = GetDevContextWithGUID(&Input.DevID[0]);
#else
		DevContext = GetDevContextWithGUID(&Input.DevID[0], (GUID_SIZE_IN_BYTES + 4));
#endif
        if (DevContext) {
            Status = KeWaitForSingleObject(&DevContext->Mutex, Executive, KernelMode, FALSE, NULL);
            ASSERT(STATUS_SUCCESS == Status);

            DevContext->FileWriterThreadPriority = Input.ulThreadPriority;
            SetDWORDInRegVolumeCfg(DevContext, DEVICE_FILE_WRITER_THREAD_PRIORITY, Input.ulThreadPriority);

            if (DevContext->DevFileWriter) {
                DriverContext.DataFileWriterManager->SetFileWriterThreadPriority(DevContext->DevFileWriter);
            }

            KeReleaseMutex(&DevContext->Mutex, FALSE);
            DereferenceDevContext(DevContext);
        } else {
            if (pOutputBuffer) {
                pOutputBuffer->ulErrorInSettingForDev = (ULONG)STATUS_DEVICE_DOES_NOT_EXIST;
            }
        }
    }

    Status = STATUS_SUCCESS;
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS
DeviceIoControlSetWorkerThreadPriority(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    SET_WORKER_THREAD_PRIORITY Input = {0};
    // Save input flags and values in local variables. Systembuffer
    // will be used to save the output information.
    PSET_WORKER_THREAD_PRIORITY pInputBuffer = (PSET_WORKER_THREAD_PRIORITY)Irp->AssociatedIrp.SystemBuffer;
    PSET_WORKER_THREAD_PRIORITY pOutputBuffer = (PSET_WORKER_THREAD_PRIORITY)Irp->AssociatedIrp.SystemBuffer;

    UNREFERENCED_PARAMETER(DeviceObject);

    ASSERT(IOCTL_INMAGE_SET_WORKER_THREAD_PRIORITY == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

    if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < sizeof(SET_WORKER_THREAD_PRIORITY)) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL, 
            ("%s: IOCTL rejected, input buffer(%#x) too small. Expecting(%#x)\n", __FUNCTION__, 
             IoStackLocation->Parameters.DeviceIoControl.InputBufferLength, sizeof(SET_WORKER_THREAD_PRIORITY)));
        Status = STATUS_BUFFER_TOO_SMALL;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    Input.ulFlags = pInputBuffer->ulFlags;
    Input.ulThreadPriority = pInputBuffer->ulThreadPriority;

    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(SET_WORKER_THREAD_PRIORITY)) {
        pOutputBuffer = NULL;
        Irp->IoStatus.Information = 0;
    } else {
        RtlZeroMemory(pOutputBuffer, sizeof(SET_WORKER_THREAD_PRIORITY));
        Irp->IoStatus.Information = sizeof(SET_WORKER_THREAD_PRIORITY);
    }

    // First set the default value if asked.
    if (Input.ulFlags & SET_WORKER_THREAD_PRIORITY_VALID_PRIORITY) {
        Registry    *pDriverParam = NULL;

        Status = OpenDriverParametersRegKey(&pDriverParam);
        if (NT_SUCCESS(Status)) {
            Status = pDriverParam->WriteULONG(WORKER_THREAD_PRIORITY, Input.ulThreadPriority);
            SetWorkQueueThreadPriority(&DriverContext.WorkItemQueue, Input.ulThreadPriority);
        }

        if (NULL != pDriverParam) {
            delete pDriverParam;
        }
    }

    if (pOutputBuffer) {
        RtlZeroMemory(pOutputBuffer, sizeof(SET_WORKER_THREAD_PRIORITY));
        pOutputBuffer->ulFlags |= SET_WORKER_THREAD_PRIORITY_VALID_PRIORITY;
        pOutputBuffer->ulThreadPriority = DriverContext.WorkItemQueue.ulThreadPriority;
    }

    Status = STATUS_SUCCESS;
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS
DeviceIoControlGetMonitoringStats(
    PDEVICE_OBJECT  DeviceObject,
    PIRP            Irp
    )
{
    PIO_STACK_LOCATION                              IoStackLocation;
    NTSTATUS                                        Status;
    PDEV_CONTEXT                                    DevContext;
    MONITORING_STATISTIC                            RequestedStat;
    PMONITORING_STATS_INPUT                         pMonStatInBuffer;
    PVOID                                           pOutBuffer;
    ULONG                                           ulInBufferLen;
    ULONG                                           ulOutBufferLen;
    ULONG                                           ulExpectedInBufferLen;
    ULONG                                           ulExpectedOutBufferLen;
    ULONG                                           ulFilledOutBufferLen;
    PMON_STAT_CUMULATIVE_TRACKED_IO_SIZE_INPUT      pCumTrackedIOSizeInBuf;
    PMON_STAT_CUMULATIVE_TRACKED_IO_SIZE_OUTPUT     pCumTrackedIOSizeOutBuf;
    PMON_STAT_CUMULATIVE_DROPPED_TAGS_COUNT_INPUT   pCumDroppedTagsCntInBuf;
    PMON_STAT_CUMULATIVE_DROPPED_TAGS_COUNT_OUTPUT  pCumDroppedTagsCntOutBuf;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(DeviceObject);

    IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    pMonStatInBuffer = (PMONITORING_STATS_INPUT)Irp->AssociatedIrp.SystemBuffer;
    pOutBuffer = Irp->AssociatedIrp.SystemBuffer;
    ulInBufferLen = IoStackLocation->Parameters.DeviceIoControl.InputBufferLength;
    ulOutBufferLen = IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength;

    ulFilledOutBufferLen = 0; // Modified only on successful operation.

    if (ulInBufferLen < MonStatInputMinSize) {
        InVolDbgPrint(DL_ERROR,
                      DM_INMAGE_IOCTL,
                      ("%s: IOCTL rejected, input buffer (%#x) too small. Expecting (%#x).\n",
                      __FUNCTION__, ulInBufferLen, MonStatInputMinSize));

        Status = STATUS_BUFFER_TOO_SMALL;
        goto Cleanup;
    }

    // Get statistic from the buffer, since minimum size is guaranteed.
    RequestedStat = pMonStatInBuffer->Statistic;

    // Parse the input further based on the requested statistic.
    switch (RequestedStat)
    {
    case MONITORING_STATISTIC::CumulativeTrackedIOSize:
        {
            ulExpectedInBufferLen = MonStatInputCumulativeTrackedIOSizeMinSize;
            ulExpectedOutBufferLen = MonStatOutputCumulativeTrackedIOSizeMinSize;

            if (ulInBufferLen < ulExpectedInBufferLen) {
                InVolDbgPrint(DL_ERROR,
                              DM_INMAGE_IOCTL,
                              ("%s: IOCTL rejected, input buffer (%#x) too small. "
                              "Requested statistic (%#x). Expected (%#x).\n",
                              __FUNCTION__, ulInBufferLen, RequestedStat, ulExpectedInBufferLen));

                Status = STATUS_INFO_LENGTH_MISMATCH;
                goto Cleanup;
            }

            if (ulOutBufferLen < ulExpectedOutBufferLen) {
                InVolDbgPrint(DL_ERROR,
                              DM_INMAGE_IOCTL,
                              ("%s: IOCTL rejected, output buffer (%#x) too small. "
                              "Requested statistic (%#x). Expected (%#x).\n",
                              __FUNCTION__, ulOutBufferLen, RequestedStat, ulExpectedOutBufferLen));

                Status = STATUS_BUFFER_TOO_SMALL;
                goto Cleanup;
            }

            pCumTrackedIOSizeInBuf = &pMonStatInBuffer->CumulativeTrackedIOSize;
            DevContext = GetDevContextWithGUID(pCumTrackedIOSizeInBuf->DeviceId,
                                               sizeof(pCumTrackedIOSizeInBuf->DeviceId));
            if (NULL == DevContext) {
                // Create null terminated string for logging.
                WCHAR wDevIDNullTerm[GUID_SIZE_IN_CHARS + 1];
                RtlCopyMemory(wDevIDNullTerm, pCumTrackedIOSizeInBuf->DeviceId, GUID_SIZE_IN_BYTES);
                wDevIDNullTerm[GUID_SIZE_IN_CHARS] = UNICODE_NULL;

                InVolDbgPrint(DL_ERROR,
                              DM_INMAGE_IOCTL,
                              ("%s: IOCTL rejected, device (%s) couldn't be found. Requested statistic (%#x).\n",
                              __FUNCTION__, wDevIDNullTerm, RequestedStat));

                Status = STATUS_NOT_FOUND;
                goto Cleanup;
            }

            pCumTrackedIOSizeOutBuf = (PMON_STAT_CUMULATIVE_TRACKED_IO_SIZE_OUTPUT)pOutBuffer;
            pCumTrackedIOSizeOutBuf->ullTotalTrackedBytes = DevContext->ullTotalTrackedBytes;

            DereferenceDevContext(DevContext);
            break;
        }

    case MONITORING_STATISTIC::CumulativeDroppedTagsCount:
        {
            ulExpectedInBufferLen = MonStatInputCumulativeDroppedTagsCountMinSize;
            ulExpectedOutBufferLen = MonStatOutputCumulativeDroppedTagsCountMinSize;

            if (ulInBufferLen < ulExpectedInBufferLen) {
                InVolDbgPrint(DL_ERROR,
                              DM_INMAGE_IOCTL,
                              ("%s: IOCTL rejected, input buffer (%#x) too small. "
                              "Requested statistic (%#x). Expected (%#x).\n",
                              __FUNCTION__, ulInBufferLen, RequestedStat, ulExpectedInBufferLen));

                Status = STATUS_INFO_LENGTH_MISMATCH;
                goto Cleanup;
            }

            if (ulOutBufferLen < ulExpectedOutBufferLen) {
                InVolDbgPrint(DL_ERROR,
                              DM_INMAGE_IOCTL,
                              ("%s: IOCTL rejected, output buffer (%#x) too small. "
                              "Requested statistic (%#x). Expected (%#x).\n",
                              __FUNCTION__, ulOutBufferLen, RequestedStat, ulExpectedOutBufferLen));

                Status = STATUS_BUFFER_TOO_SMALL;
                goto Cleanup;
            }

            pCumDroppedTagsCntInBuf = &pMonStatInBuffer->CumulativeDroppedTagsCount;
            DevContext = GetDevContextWithGUID(pCumDroppedTagsCntInBuf->DeviceId,
                                               sizeof(pCumDroppedTagsCntInBuf->DeviceId));
            if (NULL == DevContext) {
                // Create null terminated string for logging.
                WCHAR wDevIDNullTerm[GUID_SIZE_IN_CHARS + 1];
                RtlCopyMemory(wDevIDNullTerm, pCumDroppedTagsCntInBuf->DeviceId, GUID_SIZE_IN_BYTES);
                wDevIDNullTerm[GUID_SIZE_IN_CHARS] = UNICODE_NULL;

                InVolDbgPrint(DL_ERROR,
                              DM_INMAGE_IOCTL,
                              ("%s: IOCTL rejected, device (%s) couldn't be found. Requested statistic (%#x).\n",
                              __FUNCTION__, wDevIDNullTerm, RequestedStat));

                Status = STATUS_NOT_FOUND;
                goto Cleanup;
            }

            pCumDroppedTagsCntOutBuf = (PMON_STAT_CUMULATIVE_DROPPED_TAGS_COUNT_OUTPUT)pOutBuffer;
            pCumDroppedTagsCntOutBuf->ullDroppedTagsCount = DevContext->ulNumberOfTagPointsDropped;

            DereferenceDevContext(DevContext);
            break;
        }
        
    case MONITORING_STATISTIC::Invalid:
        {
            InVolDbgPrint(DL_ERROR,
                          DM_INMAGE_IOCTL,
                          ("%s: IOCTL rejected, requested statistic is Invalid (= 0).\n", __FUNCTION__));

            Status = STATUS_INVALID_PARAMETER;
            goto Cleanup;
        }

    default:
        {
            InVolDbgPrint(DL_ERROR,
                          DM_INMAGE_IOCTL,
                          ("%s: IOCTL rejected, requested statistic (%#x) is not implemented in the driver.\n",
                          __FUNCTION__, RequestedStat));

            Status = STATUS_NOT_IMPLEMENTED;
            goto Cleanup;
        }
    }

    // Get statistic operation has succeeded.
    ulFilledOutBufferLen = ulExpectedOutBufferLen;
    Status = STATUS_SUCCESS;
    goto Cleanup;

Cleanup:
    Irp->IoStatus.Information = ulFilledOutBufferLen;
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS
DeviceIoControlSetReplicationState(
    PDEVICE_OBJECT  DeviceObject,
    PIRP            Irp
)
{
    NTSTATUS            Status = STATUS_SUCCESS;
    PREPLICATION_STATE  pReplicationState = NULL;
    PDEV_CONTEXT        pDevContext = NULL;
    PIO_STACK_LOCATION  IrpSp = IoGetCurrentIrpStackLocation(Irp);
    LARGE_INTEGER       liCurrentSystemTime = { 0 };
    KIRQL               irql = 0;
    BOOLEAN             bLogDiffSyncThrottle = FALSE;
    Irp->IoStatus.Information = 0;
    if (IrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof(REPLICATION_STATE))
    {
        Status = STATUS_INFO_LENGTH_MISMATCH;
        goto cleanup;
    }
    pReplicationState = (PREPLICATION_STATE)Irp->AssociatedIrp.SystemBuffer;
    if (!TEST_ALL_FLAGS(pReplicationState->ulFlags, REPLICATION_STATES_SUPPORTED)) {
        Status = STATUS_INVALID_DEVICE_REQUEST;
        goto cleanup;
    }
    pDevContext = GetDevContextForThisIOCTL(DeviceObject, Irp);
    if (NULL == pDevContext) {
        Status = STATUS_NO_SUCH_DEVICE;
        goto cleanup;
    }
    if (TEST_FLAG(pDevContext->ulFlags, DCF_FILTERING_STOPPED)) {
        Status = STATUS_NOT_SUPPORTED;
        goto cleanup;
    }
    KeQuerySystemTime(&liCurrentSystemTime);
    if (TEST_FLAG(pReplicationState->ulFlags, REPLICATION_STATE_DIFF_SYNC_THROTTLED)) {
        KeAcquireSpinLock(&pDevContext->Lock, &irql);
        if ((MAXLONGLONG != pDevContext->replicationStats.llDiffSyncThrottleEndTime) ||
            (liCurrentSystemTime.QuadPart < pDevContext->replicationStats.llDiffSyncThrottleStartTime)) {
            pDevContext->replicationStats.llDiffSyncThrottleStartTime = liCurrentSystemTime.QuadPart;
            pDevContext->replicationStats.llDiffSyncThrottleEndTime = MAXLONGLONG;
            bLogDiffSyncThrottle = TRUE;
        }
        KeReleaseSpinLock(&pDevContext->Lock, irql);
    }
    if (bLogDiffSyncThrottle) {
        InDskFltWriteEventWithDevCtxt(INDSKFLT_WARN_DIFF_SYNC_THROTTLE_STARTED, pDevContext, liCurrentSystemTime.QuadPart);
    }
cleanup:
    if (NULL != pDevContext) {
        DereferenceDevContext(pDevContext);
    }
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}
NTSTATUS
DeviceIoControlGetReplicationStats(
    PDEVICE_OBJECT  DeviceObject,
    PIRP            Irp
)
{
    NTSTATUS                Status = STATUS_SUCCESS;
    PREPLICATION_STATS      pReplicationStats = NULL;
    PDEV_CONTEXT            pDevContext = NULL;
    PIO_STACK_LOCATION      IrpSp = IoGetCurrentIrpStackLocation(Irp);
    Irp->IoStatus.Information = 0;
    if (0 == IrpSp->Parameters.DeviceIoControl.InputBufferLength) {
        Status = STATUS_INFO_LENGTH_MISMATCH;
        goto cleanup;
    }
    if (IrpSp->Parameters.DeviceIoControl.OutputBufferLength < sizeof(REPLICATION_STATS)) {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto cleanup;
    }
    pReplicationStats = (PREPLICATION_STATS)Irp->AssociatedIrp.SystemBuffer;
    pDevContext = GetDevContextForThisIOCTL(DeviceObject, Irp);
    if (NULL == pDevContext) {
        Status = STATUS_NO_SUCH_DEVICE;
        goto cleanup;
    }
    if (TEST_FLAG(pDevContext->ulFlags, DCF_FILTERING_STOPPED)) {
        Status = STATUS_NOT_SUPPORTED;
        goto cleanup;
    }
    RtlCopyMemory(pReplicationStats, &pDevContext->replicationStats, sizeof(REPLICATION_STATS));
    Irp->IoStatus.Information = sizeof(REPLICATION_STATS);
cleanup:
    if (NULL != pDevContext) {
        DereferenceDevContext(pDevContext);
    }
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

/*
Description:    This routine checks if the event log limit has reached

Return Value:   True, if the limit has reached - logging of the event should be avoided
                False, event log limit hasn't reached
*/

bool
InMageFltHasEventLimitReached(
    )
{
    bool hasLimitReached;
    LARGE_INTEGER currentSystemTime;

    // TODO-SanKumar-1607: Should we need clear-cut thread safety handling? Not required, since
    // the implementation is decent enough without thread safety.

    // If too many event mesages are happening, write a message saying we are discarding them for a while
    KeQuerySystemTime(&currentSystemTime);

    if ((DriverContext.EventLogTimePeriod.QuadPart + EVENTLOG_SQUELCH_TIME_INTERVAL) > currentSystemTime.QuadPart) {
        // we are within the the same time interval

        if (DriverContext.NbrRecentEventLogMessages > EVENTLOG_SQUELCH_MAXIMUM_EVENTS) {
            // throw away this event message
            hasLimitReached = true;
        } else if (DriverContext.NbrRecentEventLogMessages == EVENTLOG_SQUELCH_MAXIMUM_EVENTS) {
            // write a message saying we are throwing away messages (logging this
            // message directly instead of using the helper to avoid recursive loop).
            EventWriteINDSKFLT_ERROR_TOO_MANY_EVENT_LOG_EVENTS(NULL);

            DriverContext.NbrRecentEventLogMessages++;
            hasLimitReached = true;
        } else {
            // process the log message normally
            DriverContext.NbrRecentEventLogMessages++;
            hasLimitReached = false;
        }
    } else {
        DriverContext.NbrRecentEventLogMessages = 1;
        DriverContext.EventLogTimePeriod.QuadPart = currentSystemTime.QuadPart;
        hasLimitReached = false;
    }

    return hasLimitReached;
}

VOID
InMageFltSyncFilterWithTarget(
    IN PDEVICE_OBJECT FilterDevice,
    IN PDEVICE_OBJECT TargetDevice
    )
{
    ULONG                   propFlags;

    //
    // Propogate all useful flags from target to InMageFlt. MountMgr will look
    // at the InMageFlt object capabilities to figure out if the disk is
    // a removable and perhaps other things.
    //
    propFlags = TargetDevice->Characteristics & FILTER_DEVICE_PROPOGATE_CHARACTERISTICS;
    FilterDevice->Characteristics |= propFlags;
}

/*
Description: Free all the volume contexts, CDO, Bitmap related resources and Data file related resources

Arguments: DriverObject - pointer to the filter driver object.

Return Value: None
*/
VOID
InMageFltUnload(
    IN PDRIVER_OBJECT DriverObject
    )
{
    NTSTATUS etwProvUnregisterStatus;

#if !DBG
	UNREFERENCED_PARAMETER(DriverObject);
#endif
    InVolDbgPrint(DL_INFO, DM_DRIVER_UNLOAD,
        ("InDskFlt!InMageFltUnload: DriverObject %#p\n", DriverObject));

    CleanDriverContext();

    if (NULL != DriverContext.pServiceThread) {
        KeSetEvent(&DriverContext.ServiceThreadShutdownEvent, IO_DISK_INCREMENT, FALSE);
        KeWaitForSingleObject(DriverContext.pServiceThread, Executive, KernelMode, FALSE, NULL);
        ObDereferenceObject(DriverContext.pServiceThread);
        DriverContext.pServiceThread = NULL;
    }

    InDskFltDeregisterLowMemoryNotification();

    BitmapAPI::TerminateBitmapAPI();
    CDataFileWriter::CleanLAL();

    etwProvUnregisterStatus = EventUnregisterMicrosoft_InMage_InDiskFlt();
    if (!NT_SUCCESS(etwProvUnregisterStatus))
    {
        InVolDbgPrint(DL_ERROR, DM_DRIVER_UNLOAD,
            ("InDskFlt!InMageFltUnload: Failed to unregister ETW provider of the driver. Status = %#x\n", etwProvUnregisterStatus));
    }

    return;
}

NTSTATUS
ValidateParameters(SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_INPUT Input,PSET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_OUTPUT pOutputBuffer)
{
     NTSTATUS Status = STATUS_SUCCESS;
     if(Input.ulDefaultMinFreeDataToDiskSizeLimit) {
         if(DriverContext.ulDataToDiskLimitPerDevInMB < Input.ulDefaultMinFreeDataToDiskSizeLimit) {
             pOutputBuffer->ulErrorInSettingDefault = (ULONG)STATUS_INVALID_PARAMETER;
             Status = STATUS_INVALID_PARAMETER;
         }
     }
      
     if (Input.ulMinFreeDataToDiskSizeLimitForAllDevices) {
         LIST_ENTRY  VolumeNodeList;
         Status = GetListOfDevs(&VolumeNodeList);
         if (STATUS_SUCCESS == Status) {
             while(!IsListEmpty(&VolumeNodeList)) {
                 PLIST_NODE  pNode = (PLIST_NODE)RemoveHeadList(&VolumeNodeList);;
                 PDEV_CONTEXT DevContext = (PDEV_CONTEXT)pNode->pData;
                 DereferenceListNode(pNode);
                 if (DevContext->ulFlags & DCF_DEVID_OBTAINED) {
                     Status = KeWaitForSingleObject(&DevContext->Mutex, Executive, KernelMode, FALSE, NULL);
                     ASSERT(STATUS_SUCCESS == Status);
                     if(DevContext->ullcbDataToDiskLimit < (((ULONGLONG)Input.ulMinFreeDataToDiskSizeLimitForAllDevices) * MEGABYTES)){
                         pOutputBuffer->ulErrorInSettingForAllDevs = (ULONG)STATUS_INVALID_PARAMETER;
                         Status = STATUS_INVALID_PARAMETER;
                     }
                     KeReleaseMutex(&DevContext->Mutex, FALSE);
                 }
                 DereferenceDevContext(DevContext);
             } // for each volume
         }
     }

     if (Input.ulFlags & SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_FLAGS_VALID_GUID) {
         PDEV_CONTEXT     DevContext = NULL;
#ifdef VOLUME_FLT
		DevContext = GetDevContextWithGUID(&Input.DevID[0]);
#else
        DevContext = GetDevContextWithGUID(&Input.DevID[0], (GUID_SIZE_IN_BYTES + 4));
#endif
         if (DevContext){
             Status = KeWaitForSingleObject(&DevContext->Mutex, Executive, KernelMode, FALSE, NULL);
             ASSERT(STATUS_SUCCESS == Status);
             if(DevContext->ullcbDataToDiskLimit < (Input.ulMinFreeDataToDiskSizeLimit * MEGABYTES)) {
                 pOutputBuffer->ulErrorInSettingForDev = (ULONG)STATUS_INVALID_PARAMETER;
                 Status = STATUS_INVALID_PARAMETER;
             }
             KeReleaseMutex(&DevContext->Mutex, FALSE);
             DereferenceDevContext(DevContext);
         }
     }
return Status;
}

/*This Function is added as per Persistent Time Stamp
 *changes. This code is invoked every PERSISTENT_FLUSH_TIME_OUT seconds to update the Time stamp and 
 *Sequence number.
*/
VOID
PersistentValuesFlush(
    IN PVOID Context
    )
{
    LARGE_INTEGER     TimeOut, RegTimeOut;
    NTSTATUS          Status;
    Registry          ParametersKey;
    ULONG             FailedCount = 1;
    LARGE_INTEGER     liLastTimeStamp;
    LARGE_INTEGER     liCurrentTimeStamp;
    LARGE_INTEGER     liActualExpectedCurrentTimeStamp;
    LARGE_INTEGER     liMinExpectedCurrentTimeStamp;
    LARGE_INTEGER     liMaxExpectedCurrentTimeStamp;
    LARGE_INTEGER     liTolerableDelayBetweenCalls;
    LARGE_INTEGER     liExpectedTimeBetweenFlushCalls;
    LARGE_INTEGER     liTimeJumpForLogging;
    KIRQL             OldIrql = 0;

    UNREFERENCED_PARAMETER(Context);

    TimeOut.QuadPart = (-1) * (PERSISTENT_FLUSH_TIME_OUT) * (HUNDRED_NANO_SECS_IN_SEC);
    RegTimeOut.QuadPart = (-1) * (6) * (HUNDRED_NANO_SECS_IN_SEC);
    liTolerableDelayBetweenCalls.QuadPart = (TOLERABLE_DELAY_TIMER_SCHEDULING)* (HUNDRED_NANO_SECS_IN_SEC);
    liExpectedTimeBetweenFlushCalls.QuadPart = (PERSISTENT_FLUSH_TIME_OUT)* (HUNDRED_NANO_SECS_IN_SEC);
    liTimeJumpForLogging.QuadPart = (CLOCKJUMP_FOR_LOGGING)* (HUNDRED_NANO_SECS_IN_SEC);

    do {
        Status = ParametersKey.OpenKeyReadWrite(&DriverContext.DriverParameters);
        if (!NT_SUCCESS(Status)) {
            // Failed to open the key, most probably there is no parameters key in registry.
            // Let me add the key.
            Status = ParametersKey.CreateKeyReadWrite(&DriverContext.DriverParameters);
        }
        if (!NT_SUCCESS(Status)) {
            InVolDbgPrint(DL_ERROR, DM_REGISTRY,("%s: Opening of the Registry Handle failed\n", __FUNCTION__));
            ++FailedCount;
            if (FailedCount < 10) {
                Status = KeWaitForSingleObject((PVOID)&DriverContext.PersistentThreadShutdownEvent,
                                                Executive,
                                                KernelMode,
                                                FALSE, 
                                                &RegTimeOut);
                if (!NT_SUCCESS(Status)) {
                    ++FailedCount;
                }
                if (STATUS_SUCCESS == Status) {//means the machine is going down.
                    InDskFltWriteEvent(INDSKFLT_ERROR_PERSISTENT_THREAD_OPEN_REGISTRY_FAILED, Status, SourceLocationPersistValFlushMachineDowning);
                    break;
                }
            } else {
                InDskFltWriteEvent(INDSKFLT_ERROR_PERSISTENT_THREAD_OPEN_REGISTRY_FAILED, Status, SourceLocationPersistValFlushRetriesOver);
                break;
            }
        }
    } while (STATUS_SUCCESS != Status);

    if (NT_SUCCESS(Status)) {
        do {
            KeQuerySystemTime(&liLastTimeStamp);
            Status = KeWaitForSingleObject((PVOID)&DriverContext.PersistentThreadShutdownEvent, 
                                           Executive,
                                           KernelMode,
                                           FALSE, 
                                           &TimeOut);
            KeQuerySystemTime(&liCurrentTimeStamp);
            if (!NT_SUCCESS(Status)) {
                ++FailedCount;
                if (FailedCount >= 10 ) {
                    InDskFltWriteEvent(INDSKFLT_ERROR_PERSISTENT_THREAD_WAIT_FAILED, Status);
                    break;
                }
            } else {
                FailedCount = 1;
            }

           if (STATUS_TIMEOUT == Status) {
                liActualExpectedCurrentTimeStamp.QuadPart = liLastTimeStamp.QuadPart + liExpectedTimeBetweenFlushCalls.QuadPart;
                liMinExpectedCurrentTimeStamp.QuadPart = liActualExpectedCurrentTimeStamp.QuadPart - liTolerableDelayBetweenCalls.QuadPart;
                liMaxExpectedCurrentTimeStamp.QuadPart = liActualExpectedCurrentTimeStamp.QuadPart + liTolerableDelayBetweenCalls.QuadPart;
                if ((liCurrentTimeStamp.QuadPart - liActualExpectedCurrentTimeStamp.QuadPart) > liTimeJumpForLogging.QuadPart) {
                    InDskFltWriteEvent(INDSKFLT_WARN_CLOCK_JUMPED_FWD, liActualExpectedCurrentTimeStamp.QuadPart, liCurrentTimeStamp.QuadPart);
                    CxNotifyTimeJump(liCurrentTimeStamp.QuadPart,
                        ((liCurrentTimeStamp.QuadPart - liActualExpectedCurrentTimeStamp.QuadPart) / (HUNDRED_NANO_SECS_IN_MILLISEC)),
                        TRUE);
                }
                else if ((liActualExpectedCurrentTimeStamp.QuadPart - liCurrentTimeStamp.QuadPart) > liTimeJumpForLogging.QuadPart)
                {
                    InDskFltWriteEvent(INDSKFLT_WARN_CLOCK_JUMPED_BWD, liActualExpectedCurrentTimeStamp.QuadPart, liCurrentTimeStamp.QuadPart);
                    CxNotifyTimeJump(liCurrentTimeStamp.QuadPart,
                        ((liActualExpectedCurrentTimeStamp.QuadPart - liCurrentTimeStamp.QuadPart) / (HUNDRED_NANO_SECS_IN_MILLISEC)),
                        FALSE);
                }
                if ((liCurrentTimeStamp.QuadPart < liMinExpectedCurrentTimeStamp.QuadPart) ||
                    (liCurrentTimeStamp.QuadPart > liMaxExpectedCurrentTimeStamp.QuadPart))
                {
                    KeAcquireSpinLock(&DriverContext.TimeStampLock, &OldIrql);
                    DriverContext.timeJumpInfo.llCurrentTime = liCurrentTimeStamp.QuadPart;
                    DriverContext.timeJumpInfo.llExpectedTime = liActualExpectedCurrentTimeStamp.QuadPart;
                    KeReleaseSpinLock(&DriverContext.TimeStampLock, OldIrql);
                }
            }
            //Here STATUS_TIMEOUT means that there is a timeout of 1 sec
            //And STATUS_SUCCESS means that we recieved the PersistentThreadShutdownEvent
            if ((STATUS_TIMEOUT == Status) || (STATUS_SUCCESS == Status)) {
                bool     FlushNeeded = false;
                NTSTATUS          LocalStatus;
                ULONGLONG         TimeStamp;
                LIST_ENTRY        VolumeNodeList;
                ULONGLONG         SequenceNo;
                CHAR              ShutdownMarker = Uninitialized;

                if (STATUS_SUCCESS == Status) {//means that the Driver is unloading
                    //stop draining the changes to s2 as we are about to write the persistant values in the registry.
                    DriverContext.PersistantValueFlushedToRegistry = true;
                }
                //Get the Global values 
                KeAcquireSpinLock(&DriverContext.TimeStampLock, &OldIrql);
                TimeStamp = DriverContext.ullLastTimeStamp;
                SequenceNo = DriverContext.ullTimeStampSequenceNumber;
                KeReleaseSpinLock(&DriverContext.TimeStampLock, OldIrql);

                //Check whether the values has to be updated or not
                if ((DriverContext.ullLastPersistTimeStamp != DriverContext.ullLastTimeStamp) ||
                    (STATUS_SUCCESS == Status)
                    ||(DriverContext.ullLastPersistSequenceNumber != DriverContext.ullTimeStampSequenceNumber)) {

                    ULONG               NumberOfInfo = 2;
                    const unsigned long SizeOfBuffer = sizeof(PERSISTENT_INFO) + sizeof(PERSISTENT_VALUE);
                    CHAR                LocalBuffer[SizeOfBuffer];
                    PPERSISTENT_INFO    pPersistentInfo;

                    DriverContext.ullLastPersistTimeStamp = TimeStamp;
                    DriverContext.ullLastPersistSequenceNumber = SequenceNo;

                    //Fill these values in the local buffer
                    pPersistentInfo = (PPERSISTENT_INFO)LocalBuffer;

                    pPersistentInfo->NumberOfInfo = NumberOfInfo;
                    if (STATUS_SUCCESS == Status) {//means that the Driver is unloading
                        //write the Shutdown Marker in the Registry
                        pPersistentInfo->ShutdownMarker = CleanShutdown;
                        DriverContext.CurrentShutdownMarker = CleanShutdown;
                    } else {
                        pPersistentInfo->ShutdownMarker = DirtyShutdown;
                    }
                    ShutdownMarker = pPersistentInfo->ShutdownMarker;
                    RtlZeroMemory(pPersistentInfo->Reserved, sizeof(pPersistentInfo->Reserved));

                    pPersistentInfo->PersistentValues[0].PersistentValueType = LAST_GLOBAL_TIME_STAMP;
                    pPersistentInfo->PersistentValues[0].Reserved = 0;
                    pPersistentInfo->PersistentValues[0].Data = TimeStamp;

                    pPersistentInfo->PersistentValues[1].PersistentValueType = LAST_GLOBAL_SEQUENCE_NUMBER;
                    pPersistentInfo->PersistentValues[1].Reserved = 0;
                    pPersistentInfo->PersistentValues[1].Data = SequenceNo;

                    //Write the Local Buffer into the Registry
                    LocalStatus = ParametersKey.WriteBinary(LAST_PERSISTENT_VALUES, pPersistentInfo, SizeOfBuffer);
                    if (!NT_SUCCESS(LocalStatus)) {
                        if (CleanShutdown != pPersistentInfo->ShutdownMarker)  {
                            InDskFltWriteEvent(INDSKFLT_WARN_PERSISTENT_THREAD_REGISTRY_WRITE_FAILURE, LocalStatus);
                            InVolDbgPrint(DL_ERROR, DM_REGISTRY, 
                                ("%s: %ws could not be set, Status: %x\n", __FUNCTION__, LAST_PERSISTENT_VALUES, LocalStatus));
                        }
                    } else {
                        // As per msdn
                         // https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-zwflushkey
                         //  This routine can flush the entire registry. Accordingly, it can generate a great deal of I/O. 
                         // Since the system flushes key changes automatically every few seconds, you seldom need to call ZwFlushKey.
                         // As system is already flushing every few seconds, there is no point flushing every 2 mins
                         //FlushNeeded = true;
                        FlushNeeded = false;
                        InVolDbgPrint(DL_TRACE_L3, DM_REGISTRY, 
                            ("%s: Added %ws with Time = %#I64x\n", __FUNCTION__, LAST_PERSISTENT_VALUES,TimeStamp));

                        InVolDbgPrint(DL_TRACE_L3, DM_REGISTRY, 
                            ("  And Seq = %#I64x\n",SequenceNo));
                    }

                    //The Below code Update the volume bitmap with the persisting values.
                    //This is done only in case of Time Out of the thread.
                    //In case of shutdown or the failover the individual volume Bitmap are updated in the different code.
                    if (STATUS_TIMEOUT == Status) {
                        LocalStatus = GetListOfDevs(&VolumeNodeList);
                        if (STATUS_SUCCESS == LocalStatus) {
                            while(!IsListEmpty(&VolumeNodeList)) {
                                PLIST_NODE  pNode = (PLIST_NODE)RemoveHeadList(&VolumeNodeList);;
                                PDEV_CONTEXT pDevContext = (PDEV_CONTEXT)pNode->pData;
                                DereferenceListNode(pNode);
#ifdef VOLUME_CLUSTER_SUPPORT
                                if (IS_CLUSTER_VOLUME(pDevContext) && (pDevContext->ulFlags & DCF_CLUSTER_VOLUME_ONLINE)) {
                                    PDEV_BITMAP pDevBitmap = NULL;
                                    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
                                    if (pDevContext->pDevBitmap)
                                        pDevBitmap = ReferenceDevBitmap(pDevContext->pDevBitmap);
                                    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
                                    if (NULL != pDevBitmap) {
                                        ExAcquireFastMutex(&pDevBitmap->Mutex);
                                        if (pDevBitmap->pBitmapAPI)
                                            pDevBitmap->pBitmapAPI->FlushTimeAndSequence(TimeStamp,
                                                                                            SequenceNo,
                                                                                            true);
                                        ExReleaseFastMutex(&pDevBitmap->Mutex);
                                        DereferenceDevBitmap(pDevBitmap);
                                    }
                                }
#endif
                                DereferenceDevContext(pDevContext);
                            } // for each volume
                        }
                    }

                }

                //Flush the Registry Info to the disk
                if (FlushNeeded) {
                    LocalStatus = ParametersKey.FlushKey();
                    if (!NT_SUCCESS(LocalStatus)) {
                        InDskFltWriteEvent(INDSKFLT_ERROR_PERSISTENT_THREAD_REGISTRY_FLUSH_FAILURE, LocalStatus);
                        InVolDbgPrint(DL_ERROR, DM_REGISTRY, 
                            ("%s: Flushing the Registry Info to Disk Failed\n", __FUNCTION__));
                    } else if (STATUS_SUCCESS == Status) {
                        InDskFltWriteEvent(INDSKFLT_INFO_GLOBAL_VALUE_FLUSH_INFO, ShutdownMarker);
                    }
                }
            } //((STATUS_TIMEOUT == Status) || (STATUS_SUCCESS == Status))

            //This condition will only fail when the shutdown event is triggered from the InMageFltUnload
        } while (STATUS_SUCCESS != Status);

        //close the Registry
        ParametersKey.CloseKey();
    } 

    return;
}

//*****************************************************************************
//* NAME:          InMageFetchSymbolicLink
//*
//* DESCRIPTION:    Given a symbolic link name, InMageFetchSymbolicLink returns a string with the 
//*links destination and a handle to the open link name.
//*
//* PARAMETERS:     SymbolicLinkName        IN
//*                 SymbolicLink            OUT
//*                 LinkHandle              OUT
//*
//* IRQL:           IRQL_PASSIVE_LEVEL.
//*
//* RETURNS:        STATUS_SUCCESS
//* STATUS_UNSUCCESSFUL
//*
//* NOTE           Onus lies on the caller to free SymbolicLink->Buffer and close
//*****************************************************************************
static
NTSTATUS InMageFetchSymbolicLink(
        IN      PUNICODE_STRING     SymbolicLinkName,
        OUT     PUNICODE_STRING     SymbolicLink,
        OUT     PHANDLE             LinkHandle
    )
{
    NTSTATUS            status;
    NTSTATUS            returnStatus;
    OBJECT_ATTRIBUTES   oa;
    UNICODE_STRING      tmpSymbolicLink;
    HANDLE              tmpLinkHandle;
    ULONG               symbolicLinkLength;

    returnStatus = STATUS_UNSUCCESSFUL; //Sometimes be pessimistic for a happy ending.

    if (NULL == SymbolicLinkName)
        goto Cleanup;
    // Open and query the symbolic link. So first initialize to get a
    // handle to the symbolic-link object that we want to query.
    InitializeObjectAttributes(
            &oa,
            SymbolicLinkName,
            OBJ_KERNEL_HANDLE,
            NULL,
            NULL);

    status = ZwOpenSymbolicLinkObject(
            &tmpLinkHandle,
            GENERIC_READ,
            &oa);

    if ( STATUS_SUCCESS == status )
    {
        
        // Get the size of the symbolic link string
        symbolicLinkLength              = 0;
        tmpSymbolicLink.Length          = 0;
        tmpSymbolicLink.MaximumLength   = 0;

        status = ZwQuerySymbolicLinkObject(
                tmpLinkHandle,
                &tmpSymbolicLink,
                &symbolicLinkLength);

        if ( (STATUS_BUFFER_TOO_SMALL == status) && (0 <  symbolicLinkLength) )
        {
            // Allocate the memory and get the ZwQuerySymbolicLinkObject
            PVOID tempBuf = (PWSTR)ExAllocatePoolWithTag(NonPagedPool, symbolicLinkLength, TAG_GENERIC_NON_PAGED);
            if (NULL == tempBuf)
                goto Cleanup;

            tmpSymbolicLink.Buffer =(PWCHAR) tempBuf;
            tmpSymbolicLink.Length = 0;
            tmpSymbolicLink.MaximumLength = (USHORT) symbolicLinkLength;

            status = ZwQuerySymbolicLinkObject(
                    tmpLinkHandle,
                    &tmpSymbolicLink,
                    &symbolicLinkLength);

            if ( STATUS_SUCCESS == status )
            {
                SymbolicLink->Buffer        = tmpSymbolicLink.Buffer;
                SymbolicLink->Length        = tmpSymbolicLink.Length;
                *LinkHandle                 = tmpLinkHandle;
                SymbolicLink->MaximumLength = tmpSymbolicLink.MaximumLength;
                returnStatus                = STATUS_SUCCESS;
            }
            else
            {
                if (tmpSymbolicLink.Buffer)
                    ExFreePool(tmpSymbolicLink.Buffer);
            }
        }
    }
Cleanup :
    return returnStatus;
}
//*****************************************************************************
//* NAME:           InMageExtractDriveString
//*
//* DESCRIPTION:    Extracts the drive from a string.  Adds a NULL termination and 
//*                 adjusts the length of the source string.
//*
//* PARAMETERS:     Source           IN OUT
//*
//* IRQL:           IRQL_PASSIVE_LEVEL.
//*
//*	RETURNS:        STATUS_SUCCESS
//*                 STATUS_UNSUCCESSFUL
//* Extract L"\\ArcName\\multi(0)disk(0)rdisk(0)partition(3) from 
//* L"\\ArcName\\multi(0)disk(0)rdisk(0)partition(3)\\windows string
//*****************************************************************************
static
NTSTATUS InMageExtractDriveString(
        IN OUT	PUNICODE_STRING		Source
    )
{
    
    ULONG       i;
    ULONG       numSlashes;
    NTSTATUS    status = STATUS_UNSUCCESSFUL; 

    i = 0;
    numSlashes = 0;

    while (((i * 2) < Source->Length) && (ARCNAME_SLASH_COUNT != numSlashes))
    {
        if ( L'\\' == Source->Buffer[i] )
        {
            numSlashes++;
        }
        i++;
    }

    if ((ARCNAME_SLASH_COUNT == numSlashes) && (1 < i))
    {
        i--;
        Source->Buffer[i]   = L'\0';
        Source->Length      = (USHORT) i * 2;
        status              = STATUS_SUCCESS;
    }

    return status;
}

//*****************************************************************************
//* NAME:           InMageSetBootVolumeChangeNotificationFlag
//*
//* DESCRIPTION:    This is a thread routine which basically tries to identify the boot
//*                 volume (volume where windows system files reside). Once that's found
//*                 the corresponding device object of the volume owner (volume manager)
//                  is grabbed along with its File object.
//*                 The whole exercise is a one time affair during the lifetime of filter
//*                 till shutdown.
//*
//*
//*	PARAMETERS:     Pointer to InMage Volume Context.
//*
//* IRQL:           IRQL_PASSIVE_LEVEL.
//*
//* RETURNS:        VOID
//*                 
//*                 SUCCESS:
//*                 When this is the case then we reaffirm that 
//*                 DriverContext->bootVolumeDetected is true
//*                 and we have already set the pnp notification for boot volume.
//*
//*                 FAILURE:
//*                 This means something went wrong, we reset the flag
//*                 DriverContext->bootVolumeDetected to zero. We haven't yet retistered
//*                 boot volume event notification through IoRegisterPlugPlayNotification.
//
//*****************************************************************************
VOID InMageSetBootVolumeChangeNotificationFlag(PVOID pContext)
{
  UNICODE_STRING        systemRootName;
  UNICODE_STRING        systemRootSymbolicLink1;
  UNICODE_STRING        systemRootSymbolicLink2;
  PFILE_OBJECT          fileObject;
  HANDLE                linkHandle;
  PDEV_CONTEXT          pDevContext;
  PDEVICE_OBJECT        deviceObject;
  BOOLEAN               IsWindows8AndLater;
  NTSTATUS              status, returnStatus;
  UNICODE_STRING        systemRootSymbolicLink3;
  UNREFERENCED_PARAMETER(pContext);
  //Initialize the systemroot string.

  IsWindows8AndLater = DriverContext.IsWindows8AndLater;

  if(IsWindows8AndLater){
    RtlInitUnicodeString(
            &systemRootName,
            BOOT_DEVICE);
  } else {
    RtlInitUnicodeString(
            &systemRootName,
            SYSTEM_ROOT);
  }

  waitevnt:

  KeWaitForSingleObject(&DriverContext.bootVolNotificationInfo.bootNotifyEvent,\
    Executive, KernelMode, FALSE, NULL);

  linkHandle = NULL;
  fileObject =  NULL;
  deviceObject = NULL;
  pDevContext = NULL;
  returnStatus = STATUS_UNSUCCESSFUL;

  pDevContext = DriverContext.bootVolNotificationInfo.pBootDevContext;

  if(!pDevContext){goto rundown;}

  //Lets get the system root directory absolute path.
  status = InMageFetchSymbolicLink(
            &systemRootName,
            &systemRootSymbolicLink1,
            &linkHandle);

  // Windows 2008 R2 - Find the symbolic link for boot device
  // \\Arcname\\multi(0)disk(0)rdisk(0)Partition(#)\\windows -->
  // -> \Device\Harddisk0\Partition#. ->\Device\HarddiskVolume#

  //Windows 2012 & above - this value can be directly obtained from
  //bootdevice link and shall look like \Device\HarddiskVolume#,
  //so that we can directly invoke IoGetDeviceObjectPointer without any intermediate steps.
  if (STATUS_SUCCESS == status)
  {

      if (!IsWindows8AndLater){
          CloseLinkHandle(&linkHandle);
          status = InMageExtractDriveString(&systemRootSymbolicLink1);
          if (STATUS_SUCCESS == status){
              status = InMageFetchSymbolicLink(
                  &systemRootSymbolicLink1,
                  &systemRootSymbolicLink3, // \Device\Harddisk0\Partition1
                  &linkHandle);

              if (STATUS_SUCCESS == status) {
                  CloseLinkHandle(&linkHandle);
                  status = InMageFetchSymbolicLink(
                      &systemRootSymbolicLink3,
                      &systemRootSymbolicLink2,
                      &linkHandle); // \device\harddiskvolume#
                  if (systemRootSymbolicLink3.Buffer) 
                      ExFreePool(systemRootSymbolicLink3.Buffer);
              }
          }
      }
      else
      {
          CloseLinkHandle(&linkHandle);

          status = InMageFetchSymbolicLink(
              &systemRootSymbolicLink1,
              &systemRootSymbolicLink2,
              &linkHandle);

      }
        if (STATUS_SUCCESS != status) {
            if (MAX_FAILURE == DriverContext.bootVolNotificationInfo.failureCount) {
                InVolDbgPrint(DL_INFO, DM_DEVICE_STATE, ("InMageSetBootVolumeChangeNotificationFlag : Fetching boot device sym link is failed with status %d \n", status));
                InDskFltWriteEvent(INDSKFLT_WARN_BOOTDEV_REGISTRATION_FAILED, status, SourceLocationSetBootVolChangeNotFetchSymLinkFailed);
            }
        }
        else
        {
            
            //Now we should have the value of systemRootSymbolicLink2 to be
            //something like this: \Device\HarddiskVolume1

            //The IoGetDeviceObjectPointer routine returns a pointer to the top object
            //in the named device object's stack and a pointer to the corresponding file object
            status = IoGetDeviceObjectPointer(
                    &systemRootSymbolicLink2,
                    SYNCHRONIZE | FILE_ANY_ACCESS,
                    &fileObject,
                    &deviceObject);
            
            if ( (STATUS_SUCCESS == status) && (NULL != fileObject) )
            {
                //NOTE: The fileObject which we got here must have the device object
                //of the boot volume device object owned by Windows Volume Manager.
                
                //Register plug and play Notification for this in our driver.
                pDevContext->BootVolumeNotifyFileObject = fileObject;
                pDevContext->BootVolumeNotifyDeviceObject = deviceObject;

                status = IoRegisterPlugPlayNotification(
                                  EventCategoryTargetDeviceChange, 0, fileObject,
                                  DriverContext.DriverObject, BootVolumeDriverNotificationCallback, pDevContext,
                                  &pDevContext->PnPNotificationEntry);
                

                if(STATUS_SUCCESS == status){returnStatus = STATUS_SUCCESS;}

             }
         CloseLinkHandle(&linkHandle);
         if(systemRootSymbolicLink2.Buffer)ExFreePool( systemRootSymbolicLink2.Buffer );		 
        }

        if(systemRootSymbolicLink1.Buffer)ExFreePool( systemRootSymbolicLink1.Buffer );
    }
  
rundown:
    
    
    if(STATUS_SUCCESS == returnStatus){
        //Increment the DriverContext->bootVolumeDetected flag nice and slow.
        //This flag should be 1 by now since the code calling this procedure
        //would have set it initially in the first place. This is to make it
        //double sure. We assume that bootVolumeDetected variable across threads
        //use Interlocked bus locking intrinsics.
        if(0 == DriverContext.bootVolNotificationInfo.bootVolumeDetected){
            InterlockedIncrement(&DriverContext.bootVolNotificationInfo.bootVolumeDetected);}
    }else{
        InterlockedExchange(&DriverContext.bootVolNotificationInfo.bootVolumeDetected, (LONG)0);//reset.
        InterlockedIncrement(&DriverContext.bootVolNotificationInfo.failureCount);
        if(fileObject) ObDereferenceObject(fileObject);
        pDevContext->BootVolumeNotifyFileObject = NULL;
        pDevContext->BootVolumeNotifyDeviceObject = NULL;
        //try this only 10 times....
        if(MAX_FAILURE > DriverContext.bootVolNotificationInfo.failureCount){
        KeClearEvent(&DriverContext.bootVolNotificationInfo.bootNotifyEvent);//make if not-signalled or blocking.
        goto waitevnt;
        } else {
            if (MAX_FAILURE == DriverContext.bootVolNotificationInfo.failureCount) {
                InVolDbgPrint(DL_INFO, DM_DEVICE_STATE, ("InMageSetBootVolumeChangeNotificationFlag : Registering PnP Notification is failed with status %d \n", returnStatus));
                InDskFltWriteEvent(INDSKFLT_WARN_BOOTDEV_REGISTRATION_FAILED, returnStatus, SourceLocationSetBootVolChangeNotFinalFailed);
            }

        }
    }
    
  //To the OS its always STATUS_SUCCESS.
  PsTerminateSystemThread(STATUS_SUCCESS);
}
//*****************************************************************************
//* NAME:           BootVolumeDriverNotificationCallback
//*
//* DESCRIPTION:    Sevices DeviceChange notifications, namely LOCK/UNLOCK on volume.
//*
//* PARAMETERS:     NotificationStructure  pContext
//*
//* IRQL:           IRQL_PASSIVE_LEVEL.
//*
//* RETURNS:        STATUS_SUCCESS
//*                 STATUS_UNSUCCESSFUL
//*
//*Comments :
//  This callback would be returning failures on processstartnotify() IOCTL from user space
//* Succesfull chkdsk operation aborts current session & does reboot. Making bitmap file and capturing changes across such an intermittent step and then handle power IRP is little tricky as this case
//* was not accounted in the design.For now, marking filtering state to stop and delete the bitmap file.
//* All protected disks + bitmap open would be marked as resync on next reboot

//*****************************************************************************
NTSTATUS
BootVolumeDriverNotificationCallback(
    PVOID NotificationStructure,
    PVOID pContext
    )
{
    KIRQL                               OldIrql;
    PDEV_CONTEXT                     pCurrentDevContext;
    PDEV_CONTEXT                     pDevContext;
    PTARGET_DEVICE_CUSTOM_NOTIFICATION  pNotify;
    PLIST_ENTRY                       pCurrent = NULL;
  
    pCurrentDevContext = (PDEV_CONTEXT)pContext; 

    pNotify = (PTARGET_DEVICE_CUSTOM_NOTIFICATION)NotificationStructure;
    
    pDevContext = (PDEV_CONTEXT)DriverContext.HeadForDevContext.Flink;

    //Note: Inmage Filter stores all the bitmap files on System boot volume but individual
    //device context stores the bitmap context pointer only for that corresponding disk.
    //what I mean to say is that, if we get a LOCK/UNLOCK/LOCK-FAIL notification on boot
    //volume then effectively we have to LOCK all volumes by closing handles to all bitmap
    //files in each context.
    //For instance: Say we have boot disk D1 and data disk D2 but protection is only on data disk, Now when
    //we get LOCK notification on boot volume then to release all handles on it we are
    //forced to close the bitmap files on data disk too.

    if ((NULL == pDevContext) || (NULL == pNotify) || (NULL == pCurrentDevContext) )
    {return STATUS_SUCCESS;}

    //This is a restriction we consciously implement inorder to prevent LOCKING/UNLOCKING
    //of boot volume once the system has completed booting and already started running
    //services.exe. We simply return STATUS_SUCCESS, meaning it's Okay for us to
    //LOCK/UNLOCK but without releasing/closing any bitmap file which indrectly stops
    //the volume LOCK/UNLOCK process. Also remember that any boot volume dismount/
    //LOCK never succeeds in practice.

    if(0 < DriverContext.bootVolNotificationInfo.LockUnlockBootVolumeDisable){
        return STATUS_SUCCESS;
    }
    // IsEqualGUID takes a reference to GUID (GUID &).
    //GUID_IO_VOLUME_LOCK shall always be called by FS before autochk.exe
    //is run at the startup. autochk.exe is a native app.
    if (IsEqualGUID(pNotify->Event, GUID_IO_VOLUME_LOCK))
    {
        InDskFltWriteEvent(INDSKFLT_WARN_PNP_BOOTDEVLOCK_IS_IN_PROGRESS);
        for (pCurrent = DriverContext.HeadForDevContext.Flink; pCurrent != &DriverContext.HeadForDevContext; pCurrent = pCurrent->Flink)
        {
            pDevContext = (PDEV_CONTEXT)(pCurrent);
            //
            // Avoid handling repeated LOCK notifications
            //
            if (TEST_FLAG(pDevContext->ulFlags, DCF_CV_FS_LOCKED)) {
                InVolDbgPrint(DL_INFO, DM_DEVICE_STATE, ("GUID_IO_VOLUME_LOCK : Received GUID_IO_VOLUME_LOCK notification on an already locked disk %S\n", pDevContext->wDevID));
                continue;
            } else {
                PDEV_BITMAP pDevBitmap = NULL;

                InVolDbgPrint(DL_INFO, DM_DEVICE_STATE, ("BootVolumeDriverNotificationCallback::Received GUID_IO_VOLUME_LOCK notification on disk %S\n", pDevContext->wDevID));

                if (NULL == pDevContext->pDevBitmap) {
                    InVolDbgPrint(DL_INFO, DM_DEVICE_STATE, ("Filtering is ON but the bitmap file is not yet opened on %S\n", pDevContext->wDevID));
                    InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_PNPLOCK_FOUND_NO_BITMAP, pDevContext);
                    continue;
                } else {

                    KeQuerySystemTime(&pDevContext->liBootVolumeLockTS);

                    StopFilteringDevice(pDevContext, false);
                    ASSERT(TEST_FLAG(pDevContext->ulFlags, DCF_FILTERING_STOPPED));
                    // This is best attempt to mark and registry write is not guaranteed. Check this?
                    SetDWORDInRegVolumeCfg(pDevContext, DEVICE_FILTERING_DISABLED, DEVICE_FILTERING_DISABLED_SET);

                    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
                    pDevBitmap = pDevContext->pDevBitmap;
                    pDevContext->pDevBitmap = NULL;
                    SET_FLAG(pDevContext->ulFlags, DCF_CV_FS_LOCKED);
                    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);

                    if (NULL != pDevBitmap) {
                        InVolDbgPrint(DL_INFO, DM_DEVICE_STATE | DM_BITMAP_CLOSE,
                            ("Closing bitmap file on GUID_IO_VOLUME_LOCK notification for disk %S(%S)\n",
                            pDevBitmap->DevID, pDevBitmap->wDevNum));
                        // ClearBitmap false
                        // Delete Bitmap true
                        CloseBitmapFile(pDevBitmap, false, pDevContext, dirtyShutdown, true);
                        InDskFltWriteEventWithDevCtxt(INDSKFLT_WARN_PNPLOCK_DELETE_BITMAP, pDevContext);
                    }
                }
            }
        }
        if (pCurrentDevContext->BootVolumeNotifyFileObject) {
            ObDereferenceObject(pCurrentDevContext->BootVolumeNotifyFileObject);
            pCurrentDevContext->BootVolumeNotifyFileObject = NULL;
            if (pCurrentDevContext->BootVolumeNotifyDeviceObject)pCurrentDevContext->BootVolumeNotifyDeviceObject = NULL;
        }
    }
    // Handling Unlock/
    else if ((IsEqualGUID(pNotify->Event, GUID_IO_VOLUME_UNLOCK) || IsEqualGUID(pNotify->Event, GUID_IO_VOLUME_LOCK_FAILED))) {
        InVolDbgPrint( DL_INFO, DM_DEVICE_STATE, ("Received GUID_IO_VOLUME_UNLOCK/GUID_IO_VOLUME_UNLOCK notification on disk %S\n", pDevContext->wDevID));

      for (pCurrent = DriverContext.HeadForDevContext.Flink; pCurrent != &DriverContext.HeadForDevContext; pCurrent = pCurrent->Flink)
      {
        pDevContext = (PDEV_CONTEXT)(pCurrent);
        KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
        KeQuerySystemTime(&pDevContext->liBootVolumeUnLockTS);
        if (TEST_FLAG(pDevContext->ulFlags, DCF_CV_FS_LOCKED))
            CLEAR_FLAG(pDevContext->ulFlags, DCF_CV_FS_LOCKED);

        KeReleaseSpinLock(&pDevContext->Lock, OldIrql);        
       }

    }
    else {
        InVolDbgPrint(DL_INFO, DM_DEVICE_STATE, ("Received unhandled notification on volume %p %x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x\n",
            pDevContext, pNotify->Event.Data1, pNotify->Event.Data2, pNotify->Event.Data3,
            pNotify->Event.Data4[0], pNotify->Event.Data4[1], pNotify->Event.Data4[2], pNotify->Event.Data4[3],
            pNotify->Event.Data4[4], pNotify->Event.Data4[5], pNotify->Event.Data4[6], pNotify->Event.Data4[7]));
    }
    return STATUS_SUCCESS;
}

//*****************************************************************************
//* NAME:           InMageFltOnReinitialize
//*
//* DESCRIPTION:    Invoke this after initialization of all boot drivers and creation of all boot objects.
//*                 We could have used work item but NO since a work item cannot guarantee that the routine 
//*                 is not being executed before DriverEntry finishes and doesnot ensure that it runs only 
//*                 after all boot drivers are loaded.
//*
//*PARAMETERS:     IN DriverObject, IN Context, IN Count (how many times this was called)
//*
//*IRQL:           IRQL_PASSIVE_LEVEL.
//*
//*RETURNS:        NOTHING
//*
//*****************************************************************************

VOID InMageFltOnReinitialize(IN PDRIVER_OBJECT pDriverObj,
                             IN PVOID          pContext,
                             IN ULONG          ulCount)
{
  //we donot invoke a re-initialize call here so we will be executed when ulCount == 1 only.

    HANDLE               notifyThread;
    KIRQL               OldIrql = 0;

    UNREFERENCED_PARAMETER(pDriverObj);
    UNREFERENCED_PARAMETER(pContext);
    //Create a transient system thread that would be woken up by the first IO touching the boot volume.
    //This thread is responsible for registring DeviceChange notificaton event on boot volume.
    //We presume that the driver is not going to get unloaded untill and unless an OS shutdown happens. 
    //So no need to do a cleanup.


    if (
        (0 >= DriverContext.bootVolNotificationInfo.bootVolumeDetected) && (1 >= ulCount)){
        if (STATUS_SUCCESS == PsCreateSystemThread(&notifyThread, THREAD_ALL_ACCESS, 0, 0, 0, InMageSetBootVolumeChangeNotificationFlag, NULL))
        {
          InterlockedIncrement(&DriverContext.bootVolNotificationInfo.reinitCount); 
          ZwClose(notifyThread);
        }
        else{}
    }

    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    DriverContext.isNoHydWFActive = FALSE;
    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);
    UpdateBootInformation();
}
void
UpdateNotifyAndDataRetrievalStats(PDEV_CONTEXT DevContext, PKDIRTY_BLOCK DirtyBlock)
{

    LARGE_INTEGER       PerformanceFrequency;
    KIRQL               oldIrql;

    DirtyBlock->liTickCountAtUserRequest = KeQueryPerformanceCounter(&PerformanceFrequency);

    if (DevContext->liTickCountAtNotifyEventSet.QuadPart) {
        LARGE_INTEGER       liMicroSeconds;
        ULARGE_INTEGER      uliMilliSeconds;

        liMicroSeconds.QuadPart = DirtyBlock->liTickCountAtUserRequest.QuadPart - DevContext->liTickCountAtNotifyEventSet.QuadPart;
        liMicroSeconds.QuadPart = liMicroSeconds.QuadPart * 1000 * 1000 / PerformanceFrequency.QuadPart;

        if (0 == liMicroSeconds.HighPart) {
            if (DevContext->LatencyDistForNotify) {
                DevContext->LatencyDistForNotify->AddToDistribution(liMicroSeconds.LowPart);
            }

            if (DevContext->LatencyLogForNotify) {
                DevContext->LatencyLogForNotify->AddToLog(liMicroSeconds.LowPart);
            }
        }
        uliMilliSeconds.QuadPart = liMicroSeconds.QuadPart / 1000;
        KeAcquireSpinLock(&DriverContext.vmCxSessionContext.CxSessionLock, &oldIrql);
        if (DevContext->CxSession.hasCxSession &&
            (DevContext->CxSession.ullMaxS2LatencyInMS < uliMilliSeconds.QuadPart)) {
            DevContext->CxSession.ullMaxS2LatencyInMS = uliMilliSeconds.QuadPart;
        }
        if ((CxSessionInProgress == DriverContext.vmCxSessionContext.vmCxSession.cxState) &&
            (DriverContext.vmCxSessionContext.vmCxSession.ullMaxS2LatencyInMS < uliMilliSeconds.QuadPart)) {
            DriverContext.vmCxSessionContext.vmCxSession.ullMaxS2LatencyInMS = uliMilliSeconds.QuadPart;
        }
        KeReleaseSpinLock(&DriverContext.vmCxSessionContext.CxSessionLock, oldIrql);
        DevContext->liTickCountAtNotifyEventSet.QuadPart = 0;
    }

    // If Dirty block with data
    if (INFLTDRV_DATA_SOURCE_DATA == DirtyBlock->ulDataSource) {
        LARGE_INTEGER       liMicroSeconds;

        liMicroSeconds.QuadPart = DirtyBlock->liTickCountAtUserRequest.QuadPart -
            DirtyBlock->liTickCountAtLastIO.QuadPart;
        liMicroSeconds.QuadPart = liMicroSeconds.QuadPart * 1000 * 1000 / PerformanceFrequency.QuadPart;

        if (0 == liMicroSeconds.HighPart) {
            if (DevContext->LatencyDistForS2DataRetrieval) {
                DevContext->LatencyDistForS2DataRetrieval->AddToDistribution(liMicroSeconds.LowPart);
            }

            if (DevContext->LatencyLogForS2DataRetrieval) {
                DevContext->LatencyLogForS2DataRetrieval->AddToLog(liMicroSeconds.LowPart);
            }
        }
    }
}

void 
CloseLinkHandle(
_In_ HANDLE *handle)
{
    HANDLE Handle = *handle;
    if (NULL != Handle)
        ZwClose(Handle);
    *handle = NULL;
}

NTSTATUS ValidatePreCheckInputBuffer(
    _In_ PTAG_PRECHECK_INPUT pTagPrecheckInput,
    _In_ ULONG InputBufLen)
    /*
    Description : This function validates the input buffer for expected size based on the number of disks

    Arguments :
    pPrecheckInput       - Pointer to input buffer
    InputBufLen          - Length of input buffer

    Return Value :
    STATUS_INFO_LENGTH_MISMATCH -  If the input fields contains zero
    STATUS_INVALID_PARAMETER    -  If there is mismatch in the expected size of the buffer vs size of the input buffer
    */
{

    ULONG ExpectedInputSize;

    if (0 == pTagPrecheckInput->usNumDisks)
        return STATUS_INVALID_PARAMETER;

    LONG DiskIdListOffSet = FIELD_OFFSET(TAG_PRECHECK_INPUT, DiskIdList);

    ExpectedInputSize = DiskIdListOffSet + (pTagPrecheckInput->usNumDisks * GUID_SIZE_IN_BYTES);

    if (InputBufLen < ExpectedInputSize)
        return STATUS_INFO_LENGTH_MISMATCH;

    return STATUS_SUCCESS;
}

NTSTATUS DeviceIoControlPreCheckTagDisk(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
    /*
    Description : This function returns the filter disk device attributes/fields for given list of disk ID

    Arguments :
    pDevObject - Pointer to CDO
    Irp        - IO Request

    Return Value :
    Output buffer - enum code for all disk Ids part of Input list
    ecTagStatusInvalidGUID      - Disk device not found
    ecTagStatusFilteringStopped - Driver does not filter the disk device
    ecTagStatusTagDataWOS       - Disk is in Data WOS
    ecTagStatusTagNonDataWOS    - Disk is in Non-Data WOS
    */
{
    NTSTATUS                 Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION       IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    PTAG_PRECHECK_INPUT      pTagPrecheckInput = (PTAG_PRECHECK_INPUT)Irp->AssociatedIrp.SystemBuffer;
    ULONG                    IoctlInputBufferLength = IoStackLocation->Parameters.DeviceIoControl.InputBufferLength;
    ULONG                    IoctlOutputBufferLength = IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength;
    PDEV_CONTEXT             pDevContext = NULL;
    PUCHAR                   DiskIdListBuffer;
    PLONG                    pDiskStatus = NULL;
    KIRQL                    OldIrql;
    etWriteOrderState        eWriteOrderState;
    TAG_TELEMETRY_COMMON     TagCommonAttribs = { 0 };
    ULONG                    ulMessageType = ecMsgUninitialized;
    ULONG                    ulNumDisks;

    Irp->IoStatus.Information = 0;

    ASSERT(IOCTL_INMAGE_TAG_PRECHECK == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);
    UNREFERENCED_PARAMETER(DeviceObject);

    TagCommonAttribs.ulIoCtlCode = IoStackLocation->Parameters.DeviceIoControl.IoControlCode;
    TagCommonAttribs.usTotalNumDisk = (USHORT)DriverContext.ulNumDevs;

    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    KeQuerySystemTime(&DriverContext.globalTelemetryInfo.liTagRequestTime);
    TagCommonAttribs.liTagRequestTime = DriverContext.globalTelemetryInfo.liTagRequestTime;
    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

    TagCommonAttribs.usNumProtectedDisk = (USHORT)InDskFltGetNumFilteredDevices();

    if (IoctlInputBufferLength < sizeof(TAG_PRECHECK_INPUT)) {
        ulMessageType = ecMsgAppInputBufferMismatch;
        Status = STATUS_INFO_LENGTH_MISMATCH;
        goto _cleanup;
    }

    Status = ValidatePreCheckInputBuffer(pTagPrecheckInput, IoctlInputBufferLength);
    if (!NT_SUCCESS(Status)) {
        ulMessageType = ecMsgAppInvalidTagInputBuffer;
        goto _cleanup;
    }

    ulNumDisks = (ULONG)pTagPrecheckInput->usNumDisks;

    RtlCopyMemory(TagCommonAttribs.TagMarkerGUID, pTagPrecheckInput->TagMarkerGUID, GUID_SIZE_IN_CHARS);
    TagCommonAttribs.TagMarkerGUID[GUID_SIZE_IN_CHARS] = '\0';

    if (TEST_FLAG(pTagPrecheckInput->ulFlags, TAG_INFO_FLAGS_BASELINE_APP)) {
        TagCommonAttribs.TagType = ecTagBaselineApp;
    } else if (TEST_FLAG(pTagPrecheckInput->ulFlags, TAG_INFO_FLAGS_LOCAL_CRASH)) {
        TagCommonAttribs.TagType = ecTagLocalCrash;
    } else if (TEST_FLAG(pTagPrecheckInput->ulFlags, TAG_INFO_FLAGS_DISTRIBUTED_CRASH)) {
        TagCommonAttribs.TagType = ecTagDistributedCrash;
    } else if (TEST_FLAG(pTagPrecheckInput->ulFlags, TAG_INFO_FLAGS_LOCAL_APP)) {
        TagCommonAttribs.TagType = ecTagLocalApp;
    } else if (TEST_FLAG(pTagPrecheckInput->ulFlags, TAG_INFO_FLAGS_DISTRIBUTED_APP)) {
        TagCommonAttribs.TagType = ecTagDistributedApp;
    } else {
        Status = STATUS_NOT_SUPPORTED;
        ulMessageType = ecMsgStatusUnexpectedPrecheckFlags;
        goto _cleanup;
    }

    if (IoctlOutputBufferLength < SIZE_OF_TAG_DISK_STATUS_OUTPUT_DATA(ulNumDisks)) {
        ulMessageType = ecMsgAppOutputBufferTooSmall;
        Status = STATUS_BUFFER_TOO_SMALL;
        goto _cleanup;
    }

    ULONG ulSizeOfTagStatus = (ulNumDisks * sizeof(LONG));
    pDiskStatus = (LONG*)ExAllocatePoolWithTag(NonPagedPool, ulSizeOfTagStatus, TAG_GENERIC_NON_PAGED);
    if (NULL == pDiskStatus) {
        ulMessageType = ecMsgStatusNoMemory;
        Status = STATUS_NO_MEMORY;
        goto _cleanup;
    }

    DiskIdListBuffer = (PUCHAR)pTagPrecheckInput->DiskIdList;

    for (ULONG index = 0; index < ulNumDisks; index++) {
        pDevContext = GetDevContextWithGUID((PWCHAR)(DiskIdListBuffer + index * GUID_SIZE_IN_BYTES), GUID_SIZE_IN_BYTES);
        if (NULL != pDevContext) {
            KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
            if (TEST_FLAG(pDevContext->ulFlags, DCF_FILTERING_STOPPED)){
                pDiskStatus[index] = ecTagStatusFilteringStopped;
                KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
                continue;
            }
            eWriteOrderState = pDevContext->eWOState;

            if (ecWriteOrderStateData != eWriteOrderState) {
                pDiskStatus[index] = ecTagStatusTagNonDataWOS;

                TagCommonAttribs.TagStatus = STATUS_INVALID_DEVICE_STATE;
                TagCommonAttribs.GlobaIOCTLStatus = STATUS_NOT_SUPPORTED;
                TagCommonAttribs.ullTagState = ecTagStatusTagNonDataWOS;
                // This is common function used to log in case the driver is in non-data WOS mode
                // If any of the disk in NonData WOS, vacp does not proceed with protocol in case of App consistency
                if (IS_APP_TAG(pTagPrecheckInput->ulFlags))
                    InDskFltTelemetryInsertTagFailure(pDevContext, &TagCommonAttribs, ecMsgPreCheckFailure);
            }
            else {
                pDiskStatus[index] = ecTagStatusTagDataWOS;
            }
            KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
        }
        else {
            pDiskStatus[index] = ecTagStatusInvalidGUID;
        }
    }

    PTAG_DISK_STATUS_OUTPUT pDiskStatusOutput = (PTAG_DISK_STATUS_OUTPUT)Irp->AssociatedIrp.SystemBuffer;

    pDiskStatusOutput->ulNumDisks = ulNumDisks;
    for (ULONG index = 0; index < ulNumDisks; index++) {
        pDiskStatusOutput->TagStatus[index] = pDiskStatus[index];
    }
    Irp->IoStatus.Information = SIZE_OF_TAG_DISK_STATUS_OUTPUT_DATA(ulNumDisks);

_cleanup:

    if (NULL != pDiskStatus)
        ExFreePoolWithTag(pDiskStatus, TAG_GENERIC_NON_PAGED);

    if (!NT_SUCCESS(Status)) {
        KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
        InDskFltTelemetryTagIOCTLFailure(&TagCommonAttribs, Status, ulMessageType);
        KeReleaseSpinLock(&DriverContext.Lock, OldIrql);
    }

    INDSKFLT_UPDATE_LAST_TAGREQUEST_TIME(&TagCommonAttribs.liTagRequestTime);

    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS
InDskFltQueryStorageProperty(
    _In_    PDEVICE_OBJECT                  DeviceObject,
    _Inout_ PSTORAGE_DEVICE_DESCRIPTOR*     ppStorageDesc
)
{
    STORAGE_PROPERTY_QUERY      storagePropertyQuery;
    STORAGE_DESCRIPTOR_HEADER   storageDescriptorHeader = { 0 };
    IO_STATUS_BLOCK             IoStatus = { 0 };
    NTSTATUS                    Status;
    PUCHAR                      pBuffer = NULL;
    ANSI_STRING                 VendorId = { 0 };
    ULONG                       ulVendorIdLength = 0;
    ULONG                       ulVendorIdOffset = 0;

    RtlZeroMemory(&storagePropertyQuery, sizeof(storagePropertyQuery));

    *ppStorageDesc = NULL;

    Status = InMageSendDeviceControl(DeviceObject,
        IOCTL_STORAGE_QUERY_PROPERTY,
        &storagePropertyQuery,
        sizeof(STORAGE_PROPERTY_QUERY),
        &storageDescriptorHeader,
        sizeof(STORAGE_DESCRIPTOR_HEADER),
        &IoStatus,
        FALSE
        );

    if (!NT_SUCCESS(Status)) {
        // Log an event 
        // fail request
        goto Cleanup;
    }

    pBuffer = (PUCHAR)ExAllocatePoolWithTag(NonPagedPool, storageDescriptorHeader.Size, TAG_DEVICE_INFO);
    if (NULL == pBuffer) {
        // Log an event
        // Return INSUFFICIENT Resources
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Cleanup;
    }

    RtlZeroMemory(pBuffer, storageDescriptorHeader.Size);

    Status = InMageSendDeviceControl(DeviceObject,
                    IOCTL_STORAGE_QUERY_PROPERTY,
                    &storagePropertyQuery,
                    sizeof(STORAGE_PROPERTY_QUERY),
                    pBuffer,
                    storageDescriptorHeader.Size,
                    &IoStatus,
                    FALSE);

    if (!NT_SUCCESS(Status) || (storageDescriptorHeader.Size < sizeof(STORAGE_DEVICE_DESCRIPTOR))) {
        goto Cleanup;
    }

    *ppStorageDesc = (PSTORAGE_DEVICE_DESCRIPTOR)pBuffer;

    // Remove trailing spaces for 
    ulVendorIdOffset = (*ppStorageDesc)->VendorIdOffset;
    if ((ulVendorIdOffset >= sizeof(STORAGE_DESCRIPTOR_HEADER)) && (ulVendorIdOffset < storageDescriptorHeader.Size)) {
        RtlInitAnsiString(&VendorId, (CHAR*)pBuffer + ulVendorIdOffset);
        ulVendorIdLength = VendorId.Length - 1;

        // First check is needed to handle underflow. From 0 it will go to maximum unsinged.
        while ((ulVendorIdLength < VendorId.Length) && (VendorId.Buffer[ulVendorIdLength] == ' ')) {
            ulVendorIdLength--;
        }

        // If buffer underflows.. we need to set 0th offset to null terminator
        if ((MAXULONG == ulVendorIdLength) || ((USHORT) ulVendorIdLength < (VendorId.Length - 1))){
            VendorId.Buffer[ulVendorIdLength + 1] = '\0';
        }
    }

    // Remove trailing spaces from vendorId

    pBuffer = NULL;

Cleanup:
    if (NULL != pBuffer) {
        ExFreePoolWithTag(pBuffer, TAG_DEVICE_INFO);
    }
    return Status;
}





NTSTATUS
InDskFltUpdateOSVersion(
    _In_ PDEV_CONTEXT DevContext
)
{
    Registry    *pDeviceParam = NULL;
    NTSTATUS    Status = STATUS_SUCCESS;

    ASSERT(NULL != DevContext);

    Status = OpenDeviceParametersRegKey(DevContext, &pDeviceParam);
    if (!NT_SUCCESS(Status)) {
        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_OPEN_DEVICE_REGISTRY, DevContext, Status);
        goto Cleanup;
    }

    Status = pDeviceParam->WriteULONG(DEVICE_DATA_OS_MAJOR_VER, DriverContext.osMajorVersion);
    if (!NT_SUCCESS(Status)) {
        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_WRITE_DEVICE_REGISTRY, DevContext, DEVICE_DATA_OS_MAJOR_VER, Status);
        goto Cleanup;
    }

    Status = pDeviceParam->WriteULONG(DEVICE_DATA_OS_MINOR_VER, DriverContext.osMinorVersion);
    if (!NT_SUCCESS(Status)) {
        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_WRITE_DEVICE_REGISTRY, DevContext, DEVICE_DATA_OS_MINOR_VER, Status);
    }

Cleanup:
    if (NULL != pDeviceParam) {
        delete pDeviceParam;
    }

    return Status;
}

NTSTATUS
 DeviceIoControlGetDiskIndexList(
    IN PDEVICE_OBJECT  DeviceObject,
    IN PIRP            Irp
 )
 {
    NTSTATUS                Status = STATUS_SUCCESS;
    PDISKINDEX_INFO         pDiskIndexInfo = NULL;
    KIRQL                   oldIrql = 0;
    PIO_STACK_LOCATION      IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PLIST_ENTRY             pDevExtList;
    PDEV_CONTEXT            pDevContext = NULL;
    ULONG                   ulIndex = 0;
    ULONG                   ulOutputBufferNeeded = sizeof(DISKINDEX_INFO);
    ULONG                   ulDevNumber;

    UNREFERENCED_PARAMETER(DeviceObject);

    Irp->IoStatus.Information = 0;

    pDiskIndexInfo = (PDISKINDEX_INFO)Irp->AssociatedIrp.SystemBuffer;

    if (IrpSp->Parameters.DeviceIoControl.OutputBufferLength < ulOutputBufferNeeded) {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto cleanup;
    }

    RtlZeroMemory(pDiskIndexInfo, IrpSp->Parameters.DeviceIoControl.OutputBufferLength);

    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
    pDevExtList = DriverContext.HeadForDevContext.Flink;
    for (; pDevExtList != &DriverContext.HeadForDevContext; pDevExtList = pDevExtList->Flink){
        pDevContext = (PDEV_CONTEXT)pDevExtList;
        KeAcquireSpinLockAtDpcLevel(&pDevContext->Lock);

        // Skip disks
        // 1. Device number is not obtained.
        // 2. Disk is in removal path.
        // 3. Disks belong to physical paths of MPIO
        if (!TEST_FLAG(pDevContext->ulFlags, DCF_DEVNUM_OBTAINED) ||
            TEST_FLAG(pDevContext->ulFlags, DCF_REMOVE_CLEANUP_PENDING | DCF_SURPRISE_REMOVAL) ||
            (MAXULONG == pDevContext->ulDevNum)) {
            KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
            continue;
        }
        ulDevNumber = pDevContext->ulDevNum;
        KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);

        ulOutputBufferNeeded = sizeof(DISKINDEX_INFO) + ulIndex * sizeof(ULONG);

        if (IrpSp->Parameters.DeviceIoControl.OutputBufferLength < ulOutputBufferNeeded) {
            Status = STATUS_BUFFER_TOO_SMALL;
            KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
            goto cleanup;
        }

        pDiskIndexInfo->aDiskIndex[ulIndex] = ulDevNumber;
        ++ulIndex;
    }
    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);

    pDiskIndexInfo->ulNumberOfDisks = ulIndex;
    Irp->IoStatus.Information = ulOutputBufferNeeded;

cleanup:
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS
VerifyOSUpgrade(PDEV_CONTEXT    pDevContext)
{
    BOOLEAN                 bOSMajorUpgrade;
    BOOLEAN                 bOSMinorUpgrade;
    NTSTATUS                Status = STATUS_SUCCESS;

    bOSMajorUpgrade = (pDevContext->ulOldOsMajorVersion != DriverContext.osMajorVersion);
    bOSMinorUpgrade = (pDevContext->ulOldOsMinorVersion != DriverContext.osMinorVersion);

    if (bOSMajorUpgrade || bOSMinorUpgrade) {
        ULONG       ulOSUpgradeResyncError;

        if (bOSMajorUpgrade) {
            ulOSUpgradeResyncError = MSG_INDSKFLT_WARN_OS_UPGRADED_MAJOR_Message;
            InDskFltWriteEventWithDevCtxt(INDSKFLT_WARN_OS_UPGRADED_MAJOR,
                pDevContext,
                DriverContext.osMajorVersion,
                pDevContext->ulOldOsMajorVersion,
                DriverContext.osMinorVersion,
                pDevContext->ulOldOsMinorVersion);

        }
        else {
            ulOSUpgradeResyncError = MSG_INDSKFLT_WARN_OS_UPGRADED_MINOR_Message;
            InDskFltWriteEventWithDevCtxt(INDSKFLT_WARN_OS_UPGRADED_MINOR,
                pDevContext,
                DriverContext.osMajorVersion,
                pDevContext->ulOldOsMajorVersion,
                DriverContext.osMinorVersion,
                pDevContext->ulOldOsMinorVersion);
        }

        Status = SetDevOutOfSync(pDevContext, ulOSUpgradeResyncError, STATUS_NOT_SUPPORTED, false);

        // If resync WF is not scheduled stop filtering
        if (!NT_SUCCESS(Status)) {
            StopFilteringDevice(pDevContext, false);
        }
        Status = STATUS_SUCCESS;
    }

    Status = InDskFltUpdateOSVersion(pDevContext);

    return Status;
}


NTSTATUS
DeviceIoControlGetDriverFlags(
_In_    PDEVICE_OBJECT  DeviceObject,
_Inout_ PIRP            Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    NTSTATUS                Status = STATUS_SUCCESS;
    PDRIVER_FLAGS_OUTPUT    pDriverFlagsOutput = NULL;
    KIRQL                   oldIrql;
    PIO_STACK_LOCATION      IrpSp = IoGetCurrentIrpStackLocation(Irp);

    Irp->IoStatus.Information = 0;
    ASSERT(IOCTL_INMAGE_GET_DRIVER_FLAGS == IrpSp->Parameters.DeviceIoControl.IoControlCode);

    pDriverFlagsOutput = (PDRIVER_FLAGS_OUTPUT)Irp->AssociatedIrp.SystemBuffer;

    if (IrpSp->Parameters.DeviceIoControl.OutputBufferLength < sizeof(DRIVER_FLAGS_OUTPUT)) {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto cleanup;
    }

    RtlZeroMemory(pDriverFlagsOutput, IrpSp->Parameters.DeviceIoControl.OutputBufferLength);

    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
    pDriverFlagsOutput->ulFlags = DriverContext.ulFlags;
    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
    Irp->IoStatus.Information = sizeof(DRIVER_FLAGS_OUTPUT);

cleanup:
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;

}

