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
#include <ioevent.h>
#include "IoctlInfo.h"
#include "ListNode.h"
#include "misc.h"
#include "VBitmap.h"
#include "MetaDataMode.h"
#include "svdparse.h"
#include "HelperFunctions.h"
#include "VContext.h"

#include "ntddscsi.h"
#include <version.h>

#define GOTO_CLEANUP_ON_FAIL(op) \
    { \
        NT_ASSERT(NT_SUCCESS(Status)); \
        Status = op; \
        if (!NT_SUCCESS(Status)) { \
            goto cleanup; \
        } \
    }

//
// Dirty blocks tracked by this driver, DRIVER global so shared betwen all devices
//
//
// Function declarations
//

extern "C"  NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

NTSTATUS
InDskFltAddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PhysicalDeviceObject
    );

NTSTATUS
InDskFltDispatchPnp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
InDskFltDispatchPower(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
InDskFltDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );


NTSTATUS
InMageFltStartDevice(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN BOOLEAN bNoRebootLayer = FALSE,
    IN PDRIVER_DISPATCH pDispatchEntryFunction = NULL
    );

VOID
InSetPNPStateToStart(
    IN PDEVICE_OBJECT DeviceObject
    );

NTSTATUS
InMageFltRemoveDevice(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );


NTSTATUS
InDskFltDispatchWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
InMageFltShutdown(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
InDskFltCleanup(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS 
SetDevNumInDevContext (
    __in   PDEV_CONTEXT pDevContext,
    __in   PSTORAGE_DEVICE_NUMBER pStorageDevNum
    );

NTSTATUS
SetDevIDAndSizeInDevContext(
    __in    PDEV_CONTEXT pDevContext,
    __in    PDISK_PARTITION_INFO pPartitionInfo,
    __in    ULONG ulSetFlags
);

NTSTATUS
SetDevInfoInDevContext (
    __in    PDEV_CONTEXT pDevContext
    );

BOOLEAN
SetFiltState(
    __in    PDEV_CONTEXT pDevContext,
    __in    ULONG ulFlags
    );

void
PreShutdown (
    );

void
PostShutdown (
    );

void
BitmapDeviceOff (
    __in    PDEVICE_OBJECT pBitmapDevObject
);

NTSTATUS
IndskFltSetDeviceInformation (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp
    );


IO_COMPLETION_ROUTINE InWriteCompInDataMode;

IO_COMPLETION_ROUTINE InWriteCompInMetaDataMode;

extern WCHAR *ErrorToRegErrorDescriptionsW[];

//
// Define the sections that allow for discarding (i.e. paging) some of
// the code.
//

/*

Routine Description:

    Installable driver initialization entry point.
    This entry point is called directly by the I/O manager to set up the 
    InMage driver. The driver object is set up and then the Pnp manager
    calls InMageFltAddDevice to attach to the boot devices.

Arguments:

    DriverObject - The InMage driver object.

    RegistryPath - pointer to a unicode string representing the path,
                   to driver-specific key in the registry.

Return Value:

    STATUS_SUCCESS if successful

*/

extern "C" NTSTATUS DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
{

    ULONG                   ulIndex;
    PDRIVER_DISPATCH        *dispatch;
    HANDLE                  hThread;
    NTSTATUS                Status;
    NTSTATUS                etwProvRegisterStatus;

    // we want this printed if we are printing anything
    InVolDbgPrint(DL_ERROR, DM_DRIVER_INIT, ("InDskFlt!DriverEntry: entered\nCompiled %s  %s\n", __DATE__,  __TIME__));

    // Register the ETW event provider of the driver
    etwProvRegisterStatus = EventRegisterMicrosoft_InMage_InDiskFlt();
    if (!NT_SUCCESS(etwProvRegisterStatus))
    {
        InVolDbgPrint(DL_ERROR, DM_DRIVER_INIT,
            ("InDskFlt!DriverEntry: Failed to register ETW provider of the driver. Status = %#x\n", etwProvRegisterStatus));
    }

    if (BypassCheckBootIni() == STATUS_SUCCESS) {
        InDskFltWriteEvent(INDSKFLT_INFO_BYPASS_MODE);
        InVolDbgPrint(DL_ERROR, DM_DRIVER_INIT, ("InDevFlt:Bypassing...\n"));
        // install pass through functions into dispatch table
        // used if the drver breaks and prevents booting
        for (ulIndex = 0, dispatch = DriverObject->MajorFunction;
            ulIndex <= IRP_MJ_MAXIMUM_FUNCTION;
            ulIndex++, dispatch++) {
            *dispatch = BypassPass;
            }

        DriverObject->MajorFunction[IRP_MJ_PNP]            = BypassDispatchPnp;
        DriverObject->MajorFunction[IRP_MJ_POWER]          = BypassDispatchPower;
        DriverObject->DriverExtension->AddDevice           = BypassAddDevice;
        DriverObject->DriverUnload                         = BypassUnload;

        return STATUS_SUCCESS;
    }

    //
    // do global initilization for the persistent bitmap code
    //

    BitmapAPI::InitializeBitmapAPI();
    CDataFileWriter::InitializeLAL();

    // Initialize GlobalData;
    InitializeDriverContext(DriverObject, RegistryPath);

    // Create the system thread to read and write bitmaps.
    Status = PsCreateSystemThread(&hThread, THREAD_ALL_ACCESS, NULL, NULL, NULL, ServiceStateChangeFunction, NULL);
    if (NT_SUCCESS(Status))
    {
        Status = ObReferenceObjectByHandle(hThread, SYNCHRONIZE, NULL, KernelMode, (PVOID *)&DriverContext.pServiceThread, NULL);
        if (NT_SUCCESS(Status))
        {
            ZwClose(hThread);
        } else {
            InDskFltWriteEvent(INDSKFLT_ERROR_OBJECT_REFERENCE_FAILED, Status);
        }
    } else {
        InDskFltWriteEvent(INDSKFLT_ERROR_THREAD_CREATION_FAILED, Status);
    }

    // Create the system thread to Flush the Time Stamp and Sequence Number in the persistent space.
    Status = PsCreateSystemThread(&hThread, THREAD_ALL_ACCESS, NULL, NULL, NULL, PersistentValuesFlush, NULL);
    if (NT_SUCCESS(Status))
    {
        Status = ObReferenceObjectByHandle(hThread, SYNCHRONIZE, NULL, KernelMode, (PVOID *)&DriverContext.pTimeSeqFlushThread, NULL);
        if (NT_SUCCESS(Status))
        {
            ZwClose(hThread);
        } else {
            InDskFltWriteEvent(INDSKFLT_ERROR_OBJECT_REFERENCE_FAILED, Status);
        }
    } else {
        InDskFltWriteEvent(INDSKFLT_ERROR_THREAD_CREATION_FAILED, Status);
    }

    // Register for Low memory condition
    InDskFltRegisterLowMemoryNotification();

    //
    // Create dispatch points
    //
    for (ulIndex = 0, dispatch = DriverObject->MajorFunction;
         ulIndex <= IRP_MJ_MAXIMUM_FUNCTION;
         ulIndex++, dispatch++) {

        *dispatch = InMageFltSendToNextDriver;
    }

    //
    // Set up the device driver entry points.
    //


    DriverObject->MajorFunction[IRP_MJ_CREATE]          = InMageFltCreate;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]           = InMageFltClose;
    DriverObject->MajorFunction[IRP_MJ_CLEANUP]         = InDskFltCleanup;
#ifdef DBG
    DriverObject->MajorFunction[IRP_MJ_READ]            = InMageFltRead;
#endif
    DriverObject->MajorFunction[IRP_MJ_WRITE]           = InDskFltDispatchWrite;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]  = InDskFltDeviceControl;
    DriverObject->MajorFunction[IRP_MJ_SHUTDOWN]        = InMageFltShutdown;
    
    DriverObject->MajorFunction[IRP_MJ_PNP]             = InDskFltDispatchPnp;
    DriverObject->MajorFunction[IRP_MJ_POWER]           = InDskFltDispatchPower;

    DriverObject->DriverExtension->AddDevice            = InDskFltAddDevice;
    DriverObject->DriverUnload                          = InMageFltUnload;

    if (NoRebootMode == DriverContext.eDiskFilterMode) {
      PWCHAR pSymLinkList = NULL;
      if (NT_SUCCESS(GetDeviceSymLinkList((LPGUID) &GUID_DEVINTERFACE_DISK, &pSymLinkList))) {

          CreateFilterDeviceWithoutReboot(pSymLinkList);

          // Save Generic Disk Driver major functions
          // Replace with disk filter functions which are used in reboot driver
 
          if (DriverContext.pTargetDriverObject)
              ReplaceDiskDriverMajorFunctions();
 
          CloseDeviceHandles();

          InDskFltRescanUninitializedDisks();

          InVolDbgPrint(DL_INFO, DM_DRIVER_INIT, ("DriverEntry : Disk Filter Mode : %d \n", 
              DriverContext.eDiskFilterMode)); 
      }
  }
    else {
     //Always place this call just above return of STATUS_SUCCESS.
     IoRegisterBootDriverReinitialization(DriverObject,
                                          InMageFltOnReinitialize,
                                          NULL);
 }

    InDskFltWriteEvent(INDSKFLT_INFO_DRIVER_VERSION, DISKFLT_DRIVER_MAJOR_VERSION, DISKFLT_DRIVER_MINOR_VERSION, 
                       DISKFLT_DRIVER_MINOR_VERSION2, DISKFLT_DRIVER_MINOR_VERSION3, INMAGE_PRODUCT_VERSION_MAJOR,
                        INMAGE_PRODUCT_VERSION_MINOR, INMAGE_PRODUCT_VERSION_BUILDNUM, INMAGE_PRODUCT_VERSION_PRIVATE);

    KeQuerySystemTime(&DriverContext.globalTelemetryInfo.liDriverLoadTime);
    return STATUS_SUCCESS;

} // end DriverEntry()

NTSTATUS
InDskFltDispatchPower(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    NTSTATUS Status = STATUS_UNSUCCESSFUL;

    // Legacy code was called in each of following minor function
    // It was getting called after acquiring remove lock for wrong device
    // Moved at appropriate place, Mostly not required
    if ((DeviceObject == DriverContext.ControlDevice) ||
        (DeviceObject == DriverContext.PostShutdownDO)) {
        Status = STATUS_NOT_IMPLEMENTED;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        goto Cleanup;
    }

    Status = InMageFltDispatchPower(DeviceObject, Irp);
Cleanup:
    return Status;
}

NTSTATUS
InMageFltDispatchPower(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN BOOLEAN bNoRebootLayer,
    IN PDRIVER_DISPATCH pDispatchEntryFunction
    )
{
    PDEVICE_EXTENSION       pDeviceExtension   = NULL;
    PDEVICE_EXTENSION       pBitmapDeviceExtension   = NULL;
    PIO_STACK_LOCATION      currentIrpStack    = NULL;
    PDEV_CONTEXT            pDevContext        = NULL;
    PDEV_CONTEXT            pBitmapDevContext  = NULL;
    NTSTATUS                Status             = STATUS_SUCCESS;
    KIRQL                   OldIrql		       = 0;
    BOOLEAN                 bSkipSystemPower   = 1;
    bool                    bLock              = 0;


    pDeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    currentIrpStack = IoGetCurrentIrpStackLocation(Irp);

    //Fix for Bug 28568 and similar.
    Status = IoAcquireRemoveLock(&pDeviceExtension->RemoveLock, Irp);
    if (!NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_INFO, DM_POWER,
                ("InMageFltDispatchPower: IoAcquireRemoveLock Dishonoured with Status %lx\n",
                Status));
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    // Invalid Case(ASSERT). In case a call comes to NoReboot Device directly
    if (NULL == pDeviceExtension->TargetDeviceObject) {
        Status = STATUS_DEVICE_NOT_CONNECTED;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        IoReleaseRemoveLock(&pDeviceExtension->RemoveLock,Irp);
        return Status;
    }

    InVolDbgPrint(DL_INFO, DM_POWER, 
                 ("InMageFltDispatchPower Device = %#p MinorFunction = %#x  ShutdownType = %#x\n", 
                 DeviceObject, 
                 currentIrpStack->MinorFunction, 
                 currentIrpStack->Parameters.Power.ShutdownType));

    // Support only Device Power off on reboot/shutdown
    if ((IRP_MN_SET_POWER != currentIrpStack->MinorFunction) ||
        ((PowerActionShutdown != currentIrpStack->Parameters.Power.ShutdownType) &&
         (PowerActionShutdownReset != currentIrpStack->Parameters.Power.ShutdownType) &&
         (PowerActionShutdownOff != currentIrpStack->Parameters.Power.ShutdownType))) {

        goto Cleanup;
    }

    pDevContext = pDeviceExtension->pDevContext;
    if (NULL == pDevContext) {
        goto Cleanup;
    }

    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
    bLock = 1;

    //Get the Bitmap Device Context to get the inrush flag
    if (NULL != pDevContext->pBitmapDevObject) {
        pBitmapDeviceExtension = (PDEVICE_EXTENSION)pDevContext->pBitmapDevObject->DeviceExtension;
        pBitmapDevContext = pBitmapDeviceExtension->pDevContext;        
    } else if (TEST_FLAG(pDevContext->ulFlags, DCF_BITMAP_DEV)) {
        pBitmapDeviceExtension = pDeviceExtension;
        pBitmapDevContext = pDevContext;
    }

    if ((NULL != pBitmapDevContext) &&
        (TEST_FLAG(pBitmapDevContext->ulFlags, DCF_PHY_DEVICE_INRUSH))) {
        //Act on System Power Irp
        bSkipSystemPower = 0;
    }

    //Skip the System Power IRP
    if (bSkipSystemPower &&
            (SystemPowerState == currentIrpStack->Parameters.Power.Type)) {
        goto Cleanup;
    }

    //Process either system or device power irp
    if (TEST_FLAG(pDevContext->ulFlags, DCF_PROCESSED_POWER)) {
        goto Cleanup;
    }

    SET_FLAG(pDevContext->ulFlags, DCF_PROCESSED_POWER);

    // Last IO detected.
    SetDevCntFlag(pDevContext, DCF_LAST_IO, bLock);

    // Verify Device contains bitmap files for other/self devices
    if (!TEST_FLAG(pDevContext->ulFlags, DCF_BITMAP_DEV)) {
        goto Cleanup;
    }
    
    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
    bLock = 0;

    //Process closer of bitmap files stored on device
    BitmapDeviceOff(DeviceObject);

Cleanup:
    if (bLock) {
        KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
    }
    InSkipCurrentIrpStackLocation(Irp, bNoRebootLayer);

    Status = InCallDriver(pDeviceExtension->TargetDeviceObject, 
                          Irp, 
                          bNoRebootLayer, 
                          pDispatchEntryFunction);

    IoReleaseRemoveLock(&pDeviceExtension->RemoveLock,Irp);

    return Status;

} // end InMageFltDispatchPower


NTSTATUS InWriteCompInDataMode(
    IN PDEVICE_OBJECT FltDeviceObject,
    IN PIRP           Irp,
    IN PVOID          Context
    )
/*++

Routine Description:
    Completion Routine for Reboot Driver In Data Mode
    IRQL <=DISPATCH_LEVEL

Arguments:
    FltDeviceObject - Device Object sent by IoManager
    Irp - IRP sent by IoManager
    Context - Context set during IoSetCompletion Routine

Return Value:
    Return STATUS_CONTINUE_COMPLETION

--*/
{
    WRITE_COMPLETION_FIELDS     WriteCompFields;
    PIO_STACK_LOCATION          IoStackLocation = NULL;
    PDEVICE_EXTENSION           pDeviceExtension = (PDEVICE_EXTENSION)FltDeviceObject->DeviceExtension;
    PDEV_CONTEXT                pDevContext = pDeviceExtension->pDevContext;
    PIRP                        OriginalIrp = (PIRP)Context;
    NTSTATUS                    Status = STATUS_CONTINUE_COMPLETION;

    IoStackLocation = (NULL == OriginalIrp) ? 
                        IoGetCurrentIrpStackLocation(Irp) :
                        IoGetCurrentIrpStackLocation(OriginalIrp);

    RtlZeroMemory(&WriteCompFields, sizeof(WRITE_COMPLETION_FIELDS));
    WriteCompFields.llOffset = IoStackLocation->Parameters.Write.ByteOffset.QuadPart;
    WriteCompFields.ulLength = IoStackLocation->Parameters.Write.Length;
    WriteCompFields.pDevContext = pDevContext;

    InCaptureIOInDataMode(FltDeviceObject, Irp, &WriteCompFields);

    if (NULL == OriginalIrp) {
        if (Irp->PendingReturned) {
            IoMarkIrpPending(Irp);
        }
        goto cleanup;
    }

    Status = STATUS_MORE_PROCESSING_REQUIRED;
    OriginalIrp->IoStatus.Information = Irp->IoStatus.Information;
    OriginalIrp->IoStatus.Status = Irp->IoStatus.Status;

    IoFreeIrp(Irp);
    IoCompleteRequest(OriginalIrp, IO_NO_INCREMENT);
cleanup:
    return Status;
}


NTSTATUS InWriteCompInMetaDataMode(
    IN PDEVICE_OBJECT FltDeviceObject,
    IN PIRP           Irp,
    IN PVOID          Context
    )
/*++

Routine Description:
    Completion Routine for Reboot Driver In MetaData Mode
    IRQL <=DISPATCH_LEVEL

Arguments:
    FltDeviceObject - Device Object sent by IoManager
    Irp - IRP sent by IoManager
    Context - Context set during IoSetCompletion Routine

Return Value:
    Return STATUS_CONTINUE_COMPLETION

--*/
{
    WRITE_COMPLETION_FIELDS     WriteCompFields;
    PIO_STACK_LOCATION          IoStackLocation = NULL;

    PDEVICE_EXTENSION           pDeviceExtension = (PDEVICE_EXTENSION)FltDeviceObject->DeviceExtension;
    PDEV_CONTEXT                pDevContext = pDeviceExtension->pDevContext;
    PIRP                        OriginalIrp = (PIRP) Context;
    NTSTATUS                    Status = STATUS_CONTINUE_COMPLETION;

    IoStackLocation = (NULL == OriginalIrp) ? 
                                IoGetCurrentIrpStackLocation(Irp) :
                                IoGetCurrentIrpStackLocation(OriginalIrp);

    RtlZeroMemory(&WriteCompFields, sizeof(WRITE_COMPLETION_FIELDS));
    WriteCompFields.llOffset = IoStackLocation->Parameters.Write.ByteOffset.QuadPart;
    WriteCompFields.ulLength = IoStackLocation->Parameters.Write.Length;
    WriteCompFields.pDevContext = pDevContext;

    InCaptureIOInMetaDataMode(FltDeviceObject, Irp, &WriteCompFields);
    if (NULL == OriginalIrp) {
        if (Irp->PendingReturned) {
            IoMarkIrpPending(Irp);
        }
        goto cleanup;
    }

    Status = STATUS_MORE_PROCESSING_REQUIRED;
    OriginalIrp->IoStatus.Information = Irp->IoStatus.Information;
    OriginalIrp->IoStatus.Status = Irp->IoStatus.Status;

    IoFreeIrp(Irp);
    IoCompleteRequest(OriginalIrp, IO_NO_INCREMENT);

cleanup:
    return Status;
}


/*
Description: This function determines whether given write IRP is pageable or not.
For NoReboot Mode : Checks the IRP_PAGING_IO or IRP_SYNCHRONOUS_PAGING_IO. Paging File writes are guaranteed to have these flags set.
For Reboot Mode   : If FO is NULL, We cannot detect paging IO or not. Check IRP flags to handle in dispatch path.

Parameters : pDevContext     - Device Context of the disk device object
Irp             - Write IRP
bNoRebootLayer  - Indicates the driver mode

Return Value: None

*/
FORCEINLINE
bool
IsWriteCanbePageable(PDEV_CONTEXT pDevContext, PIRP Irp, BOOLEAN bNoRebootLayer)
{
    ASSERT(NULL != pDevContext);
    ASSERT(NULL != Irp);

    bool bCanPageFault = true;
    PIO_STACK_LOCATION topIrpStack = NULL;

    if (bNoRebootLayer) {
        if (IS_IRP_PAGING_FLAG_SET(Irp))
            bCanPageFault = false;
    }
    else {
        topIrpStack = (PIO_STACK_LOCATION)((PCHAR)Irp + sizeof(IRP)) + (Irp->StackCount - 1);
        if ((NULL != topIrpStack) && (NULL == topIrpStack->FileObject) && IS_IRP_PAGING_FLAG_SET(Irp)) {
            bCanPageFault = false;
        }
    }
    if ((NULL != topIrpStack) && (NULL == topIrpStack->FileObject))
        pDevContext->IoCounterWithNullFileObject++;
    if (FALSE == bCanPageFault) {
        if (TEST_FLAG(Irp->Flags, IRP_PAGING_IO)) {
            pDevContext->IoCounterWithPagingIrpSet++;
        }
        else if (TEST_FLAG(Irp->Flags, IRP_SYNCHRONOUS_PAGING_IO)) {
            pDevContext->IoCounterWithSyncPagingIrpSet++;
        }
    }
    return bCanPageFault;
}

FORCEINLINE
NTSTATUS
InDskFltBuildRedirectionIrp(
_In_    PDEV_CONTEXT               pDevContext,
_In_    PIRP                       OriginalIrp,
_In_    PIO_COMPLETION_ROUTINE     pIoCompletionRoutine,
_Out_   PIRP*                      ppRedirectIrp
)
/*++

Routine Description:
    Routine to build redirection IRP.

Arguments:
    DeviceObject        .   Indskflt Filter Device Object.
    OriginalIrp             Irp received in Write request.
    pIoCompletionRoutine    Completion routine for IRP.
    ppRedirectIrp           Pointer to pointer for redirect IRP
Return Value:
    NTSTATUS

--*/
{
    PIRP                Irp = NULL;
    NTSTATUS            Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION  IrpSp = NULL;
    PIO_STACK_LOCATION  OriginalIrpSp = IoGetCurrentIrpStackLocation(OriginalIrp);
    PDEVICE_OBJECT      TargetDeviceObject = NULL;

    // Skipping NULL check for ppRedirectIrp as it is getting called in write path and
    //                      it is internal function. There is no chance that this
    //                      parameter will be NULL.
    ASSERT(NULL != ppRedirectIrp);

    TargetDeviceObject = pDevContext->TargetDeviceObject;

    Irp = IoAllocateIrp(TargetDeviceObject->StackSize + 1, FALSE);

    if (NULL == Irp)
    {
        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_REDIRECT_IRP_ALLOC, pDevContext);
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Cleanup;
    }

    Irp->MdlAddress = OriginalIrp->MdlAddress;
    Irp->Flags = OriginalIrp->Flags;
    Irp->Tail.Overlay.Thread = OriginalIrp->Tail.Overlay.Thread;
    Irp->Tail.Overlay.OriginalFileObject = OriginalIrp->Tail.Overlay.OriginalFileObject;

    // Set current stack location
    IoSetNextIrpStackLocation(Irp);
    IoGetCurrentIrpStackLocation(Irp)->DeviceObject = pDevContext->FilterDeviceObject;

    // As per driver book
    // Once Original Irp completes, driver can be unloaded anytime.
    // In completion routine post original IRP completion we want to make
    // sure that completion routine remains valid till return.
    // For the same IoSetCompletionRoutineEx is recommended for subsidiary IRP.
    Status = IoSetCompletionRoutineEx(pDevContext->FilterDeviceObject,
                                        Irp,
                                        pIoCompletionRoutine,
                                        OriginalIrp,
                                        TRUE,
                                        TRUE,
                                        TRUE);

    if (!NT_SUCCESS(Status)) {
        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_REDIRECT_IRP_COMPLETION, pDevContext, Status);
        IoFreeIrp(Irp);
        Irp = NULL;
        goto Cleanup;
    }

    // Setup location for next stack
    IrpSp = IoGetNextIrpStackLocation(Irp);
    IrpSp->MajorFunction = OriginalIrpSp->MajorFunction;
    IrpSp->Flags = OriginalIrpSp->Flags;
    IrpSp->Parameters.Write.ByteOffset.QuadPart = OriginalIrpSp->Parameters.Write.ByteOffset.QuadPart;
    IrpSp->Parameters.Write.Length = OriginalIrpSp->Parameters.Write.Length;
    IrpSp->Parameters.Write.Key = OriginalIrpSp->Parameters.Write.Key;

    // IoCall Driver
    Irp->CurrentLocation--;
    IrpSp = IoGetNextIrpStackLocation(Irp);
    Irp->Tail.Overlay.CurrentStackLocation = IrpSp;
    IrpSp->DeviceObject = TargetDeviceObject;

Cleanup:
    *ppRedirectIrp = Irp;
    return Status;
}

NTSTATUS
InDskFltDispatchWrite(
    IN PDEVICE_OBJECT pFltDeviceObject,
    IN PIRP Irp
    )
{

    NTSTATUS Status = STATUS_UNSUCCESSFUL;

    Status = ValidateRequestForDeviceAndHandle(pFltDeviceObject, Irp);
    if (!NT_SUCCESS(Status)) {
        goto Cleanup;
    }

    Status = InMageFltDispatchWrite(pFltDeviceObject, Irp);
Cleanup:
    return Status;
}

/*
Description:    This is the driver entry point for  write requests to disks to which the InMageFlt driver has attached. 
                     This driver collects data/metadata and then sets a completion routine. Then it calls the next driver below it.
                     This functions checks for threshold overflowing and underflowing. If Threshoulds are crossed the ServiceThread
					 is waked to take subsequent action regarding thresholds being crossed. This function itself does not take any 
					 action for threshould crossing other than waking service thread.

Args			:   DeviceObject - Encapsulates the current volume context
					Irp                - The i/o request meant for lower level drivers, usually

Return Value:     NTSTATUS
*/

NTSTATUS
InMageFltDispatchWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN BOOLEAN bNoRebootLayer,
    IN PDRIVER_DISPATCH pDispatchEntryFunction
    )
{
    PDEVICE_EXTENSION           pDeviceExtension ; 
    PDEV_CONTEXT                pDevContext = NULL;
    PIO_STACK_LOCATION          currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION          topIrpStack = NULL;
    PIO_COMPLETION_ROUTINE      pCompletionRoutine = NULL;

    //Fix 7181339. By default set bCanPageFault to true, 
    //Later checks are done to make it false in case required
    bool bCanPageFault = true;

    NTSTATUS            Status = STATUS_SUCCESS;
    KIRQL               OldIrql;
    PIRP                redirectIrp = NULL;
    BOOLEAN             bSkipCurrentIrp = TRUE;
    ULONG               ulOutOfSyncErrorCode = 0;

    pDeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;

    InVolDbgPrint(DL_VERBOSE, DM_WRITE, ("%s: Device = %#p  Irp = %#p Length = %#x  Offset = %#I64x\n", 
            __FUNCTION__, DeviceObject, Irp, currentIrpStack->Parameters.Write.Length,
            currentIrpStack->Parameters.Write.ByteOffset.QuadPart));

    pDevContext = pDeviceExtension->pDevContext;

   //This is a fix is required starting windows 2008 R2 onwards, where it was observed that autochk.exe failed
   //at boot time while attempting to fix volume corruption. It was found to be caused due to Inmage filter
   //locking bitmap files on boot volume which incapacitated autochk.exe from doing exclusive locks
   //on files.BUG.ID: 26376 | http://bugzilla/show_bug.cgi?id=26376
   //Here the intention is to capture first write IO (by default filter starts in metadata mode) and then
   //confirm that it's to boot device (usually always yes) followed by registering devicechange notification
   //for boot volume. 
   //First use a simple cmp instruction before using lock on bus instruction for speed. We are in IO path.
    if (RebootMode == DriverContext.eDiskFilterMode) {
        if ((!DriverContext.bootVolNotificationInfo.bootVolumeDetected) && \
            (DriverContext.bootVolNotificationInfo.reinitCount)){

            if (MAX_FAILURE > DriverContext.bootVolNotificationInfo.failureCount){
                if (!InterlockedCompareExchange(&DriverContext.bootVolNotificationInfo.bootVolumeDetected, (LONG)1, (LONG)0)){
                    InterlockedExchangePointer((PVOID *)&DriverContext.bootVolNotificationInfo.pBootDevContext, pDevContext);
                    //signal the event i.e. release the threads waiting on KeWait.... function.
                    KeSetEvent(&DriverContext.bootVolNotificationInfo.bootNotifyEvent, IO_NO_INCREMENT, FALSE);
                }
            }

        }
    }

    // check to see if the request is ANY request generated from an AsyncIORequest object
    // Also check if IO generated by AsyncIORequest object got splitted and marked as masterirp
    if ((AsyncIORequest::IsOurIO(Irp)) || ((Irp->Flags & IRP_ASSOCIATED_IRP) && (Irp->AssociatedIrp.MasterIrp != NULL) && 
            (AsyncIORequest::IsOurIO(Irp->AssociatedIrp.MasterIrp)))) {

        InVolDbgPrint(DL_INFO, DM_WRITE, 
            ("%s: Device = %#p  Irp = %#p  Bypassing capture of our own writes\n", 
            __FUNCTION__, DeviceObject, Irp));

        // do NOT capture dirty blocks for our own log files, prevents write cascade
        goto Cleanup;
    }

    //
    // Device is not initialized properly, or Filtering is stoped on this volume.
    // Blindly pass the irp along
    //

    if ((NULL == pDevContext) || TEST_FLAG(pDevContext->ulFlags, DCF_FILTERING_STOPPED)){
        goto Cleanup;
    }

    if (ecCaptureModeUninitialized == pDevContext->eCaptureMode) {
        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_INVALID_CAPTURE_STATE, pDevContext);
        StartFilteringDevice(pDevContext, false);
        SetDevOutOfSync(pDevContext, MSG_INDSKFLT_ERROR_INVALID_CAPTURE_STATE_Message, STATUS_INVALID_DEVICE_STATE, false);;
    }

    // validate the write parameters, prevents other areas from having problems

    if (0 == currentIrpStack->Parameters.Write.Length) {
        InVolDbgPrint(DL_INFO, DM_WRITE, ("%s: Device = %#p Write Irp with length 0 and Offset %#I64x. Skipping...\n",
                                          __FUNCTION__, DeviceObject, currentIrpStack->Parameters.Write.ByteOffset.QuadPart));
        goto Cleanup;
    }

    if ((currentIrpStack->Parameters.Write.ByteOffset.QuadPart >= pDevContext->llDevSize) ||
        (currentIrpStack->Parameters.Write.ByteOffset.QuadPart < 0) ||
        ((currentIrpStack->Parameters.Write.ByteOffset.QuadPart + currentIrpStack->Parameters.Write.Length) > pDevContext->llDevSize)) 
    {
        InVolDbgPrint(DL_ERROR, DM_WRITE, ("%s: Device = %#p  Write Irp with ByteOffset %I64X outside volume range\n", 
            __FUNCTION__, DeviceObject, currentIrpStack->Parameters.Write.ByteOffset.QuadPart));

        ulOutOfSyncErrorCode = MSG_INDSKFLT_WARN_DEV_WRITE_PAST_EOD_Message;
        Status = STATUS_NOT_SUPPORTED;

        InDskFltWriteEventWithDevCtxt(INDSKFLT_WARN_DEV_WRITE_PAST_EOD2,
            pDevContext,
            (ULONGLONG)pDevContext->llDevSize,
            (ULONGLONG)currentIrpStack->Parameters.Write.ByteOffset.QuadPart,
            currentIrpStack->Parameters.Write.Length);

        goto Cleanup;
    }

    // Walk down the irp and find the originating file in the first irp stack slot
    topIrpStack = (PIO_STACK_LOCATION)((PCHAR)Irp + sizeof(IRP)) + 
                  (Irp->StackCount - 1);

    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
    if ((NULL != topIrpStack) && 
        (NULL != topIrpStack->FileObject)) {
         // Check given file object is Pagefile
        if  (NULL != (FindPageFileNodeInDevCnt(pDevContext, 
                                               topIrpStack->FileObject, 
                                               1))) {
            KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
            goto Cleanup;
        }
    }

    // Check if barrier is ON
    if (pDevContext->bBarrierOn)
    {
        DriverContext.llNumQueuedIos++;

        IoMarkIrpPending(Irp);
        IoCsqInsertIrp(&pDevContext->CsqIrpQueue, Irp, NULL);
        KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
        return STATUS_PENDING;
    }
    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);

    // These validations are limited to device with paging file 
    if (pDevContext->IsPageFileDevicePossible) {
        bCanPageFault = IsWriteCanbePageable(pDevContext, Irp, bNoRebootLayer);
    }

    if (ecCaptureModeData != pDevContext->eCaptureMode) {
        InWriteDispatchInMetaDataMode(DeviceObject, Irp);
        pCompletionRoutine = InWriteCompInMetaDataMode;
    } else {
        InWriteDispatchInDataMode(DeviceObject, Irp, bCanPageFault);
        pCompletionRoutine = InWriteCompInDataMode;
    }

    bSkipCurrentIrp = FALSE;

    ReferenceDevContext(pDevContext);

    if (!bNoRebootLayer) {
        IoCopyCurrentIrpStackLocationToNext(Irp);
        IoSetCompletionRoutine(Irp,
            pCompletionRoutine,
            NULL,
            TRUE,
            TRUE,
            TRUE);

        goto Cleanup;
    }

    // Check if we need a redirect IRP
    if (!TEST_ALL_FLAGS(currentIrpStack->Control, INVOKE_ON_ALL) ||
        (NULL == currentIrpStack->CompletionRoutine)) {

        Status = InDskFltBuildRedirectionIrp(pDevContext,
                                             Irp,
                                             pCompletionRoutine,
                                             &redirectIrp);
        if (!NT_SUCCESS(Status)) {
            ulOutOfSyncErrorCode = MSG_INDSKFLT_ERROR_NOREBOOT_WRITE_TRACK_FAILED_Message;
            goto Cleanup;
        }

        InterlockedIncrement64(&pDevContext->llRedirectIrpCount);

        // In current scenario we are not taking remove lock. As per driver book,
        // The PNP manager might conceivably decide to unload your driver before
        // one of your completion routine has a chance to return to IO manager.
        // Anyone who sends you an IRP is supposed to prevent this unhappy
        // occurance by making sure you cannot be unloaded until you have
        // finished handling that IRP.
        // As per this definition, as we are pending original IRP. So it is
        // guaranteed that unload will not occur until original IRP is 
        // completed.
        // https://msdn.microsoft.com/en-us/library/windows/hardware/ff549686(v=vs.85).aspx

        // As per msdn
        // https://msdn.microsoft.com/en-us/library/windows/hardware/ff549422(v=vs.85).aspx
        // Unless the driver's dispatch routine completes the IRP (by calling IoCompleteRequest) 
        // or passes the IRP on to lower drivers, it must call IoMarkIrpPending with the IRP. 
        // Otherwise, the I/O manager attempts to complete the IRP as soon as the dispatch
        // routine returns control.
        // After calling IoMarkIrpPending, the dispatch routine must return STATUS_PENDING.
        IoMarkIrpPending(Irp);
        (*pDispatchEntryFunction)(pDeviceExtension->TargetDeviceObject, redirectIrp);
        return STATUS_PENDING;
    }

    pCompletionRoutine = (ecCaptureModeData != pDevContext->eCaptureMode) ?
                                    InCaptureIOInMetaDataMode :
                                    InCaptureIOInDataMode;

    Status = InSetCompletionRoutine(Irp,
                                    pCompletionRoutine,
                                    DeviceObject);

    if (!NT_SUCCESS(Status)) {
        ulOutOfSyncErrorCode = MSG_INDSKFLT_ERROR_NOREBOOT_WRITE_TRACK_FAILED_Message;
        InterlockedIncrement64(&pDevContext->llInCorrectCompletionRoutineCount);
    }

Cleanup:
    if (0 != ulOutOfSyncErrorCode) {
        QueueWorkerRoutineForSetDevOutOfSync(pDevContext,
            ulOutOfSyncErrorCode,
            Status,
            false);
    }

    if (!NT_SUCCESS(Status)) {
        if (!bSkipCurrentIrp) {
            DereferenceDevContext(pDevContext);
        }
        bSkipCurrentIrp = TRUE;
    }

    if (bSkipCurrentIrp) {
        InSkipCurrentIrpStackLocation(Irp, bNoRebootLayer);
    }

    Status = InCallDriver(pDeviceExtension->TargetDeviceObject, 
                          Irp, 
                          bNoRebootLayer, 
                          pDispatchEntryFunction);

    return Status;
} // end InMageFltWrite()

PDEV_CONTEXT
GetDevContextForThisIOCTL(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{                                  
    PDEV_CONTEXT        pDevContext = NULL;
    PDEVICE_EXTENSION   DeviceExtension;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);            
    ULONG               ulInputBufLen = 0;
                                      
    DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    
    ASSERT(NULL != DeviceExtension);
    if (NULL == DeviceExtension->TargetDeviceObject) {
        // The IOCTL has come on control device object. So we need to find the Device ID required to issue various IOCTLs
        // Allow valid input buffer and length

        ulInputBufLen = IoStackLocation->Parameters.DeviceIoControl.InputBufferLength;
        ASSERT(0 != ulInputBufLen);
        if ((0 != ulInputBufLen) && (NULL != Irp->AssociatedIrp.SystemBuffer)) {
            pDevContext = GetDevContextWithGUID((PWCHAR)Irp->AssociatedIrp.SystemBuffer, ulInputBufLen);
            }
        else {
            InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL, ("GetDevContextForThisIOCTL: Might be wrong Input, Buffer length : %ul", ulInputBufLen));
        }
    } else {
        pDevContext = DeviceExtension->pDevContext;
		if (NULL != pDevContext)
            ReferenceDevContext(pDevContext);
    }
    return pDevContext;
}

// attempts deletion of bitmap file corresponding to the dev context directly
// by name, without using/opening any handles in the BitmapAPI class.
_IRQL_requires_max_(PASSIVE_LEVEL)
static
void
TryDeleteBitmapFile(_In_ _Notnull_ PDEV_CONTEXT pDevContext)
{
    NTSTATUS            Status;
    OBJECT_ATTRIBUTES   oa;
    UNICODE_STRING      usLogFilePath;

    PAGED_CODE();

    RtlInitUnicodeString(&usLogFilePath, NULL);

    usLogFilePath.Buffer = (WCHAR*)ExAllocatePoolWithTag(
        PagedPool,
        sizeof(WCHAR) * MAX_LOG_PATHNAME,
        TAG_GENERIC_PAGED);

    InVolDbgPrint(DL_INFO, DM_MEM_TRACE, (__FUNCTION__ ": Allocation %#p\n", usLogFilePath.Buffer));

    if (usLogFilePath.Buffer == NULL) {
        Status = STATUS_NO_MEMORY;
        goto Cleanup;
    }

    usLogFilePath.MaximumLength = sizeof(WCHAR) * MAX_LOG_PATHNAME;

    Status = GetBitmapFileName(pDevContext, usLogFilePath, false);

    if (!NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_CLOSE,
            (__FUNCTION__ ": Error in getting the bitmap file name of device %S(%S) Status Code %#x.\n",
            pDevContext->wDevID, pDevContext->wDevNum, Status));

        goto Cleanup;
    }

    InitializeObjectAttributes(
        &oa,
        &usLogFilePath,
        OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
        NULL,
        NULL);

    Status = ZwDeleteFile(&oa);

    if (!NT_SUCCESS(Status) &&
        Status != STATUS_OBJECT_NAME_NOT_FOUND &&
        Status != STATUS_OBJECT_PATH_NOT_FOUND) {

        InVolDbgPrint(DL_ERROR, DM_BITMAP_CLOSE,
            (__FUNCTION__ ": Error in deleting the bitmap file %wZ of device %S(%S) Status Code %#x.\n",
            &usLogFilePath, pDevContext->wDevID, pDevContext->wDevNum, Status));

        goto Cleanup;
    } else if (NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_INFO, DM_BITMAP_CLOSE,
            (__FUNCTION__ ": Successfully deleted the bitmap file %wZ of device %S(%S) Status Code %#x.\n",
            &usLogFilePath, pDevContext->wDevID, pDevContext->wDevNum, Status));
    } else {
        InVolDbgPrint(DL_VERBOSE, DM_BITMAP_CLOSE,
            (__FUNCTION__ ": Ignoring failure to find the bitmap file %wZ of device %S(%S) Status Code %#x.\n",
            &usLogFilePath, pDevContext->wDevID, pDevContext->wDevNum, Status));
    }

Cleanup:
    SafeFreeUnicodeStringWithTag(usLogFilePath, TAG_GENERIC_PAGED);

    return;
}

NTSTATUS
StopFiltering(PDEV_CONTEXT  pDevContext, bool bDeleteBitmapFile)
{
    return StopFiltering(pDevContext, bDeleteBitmapFile, true);
}

NTSTATUS
StopFiltering(PDEV_CONTEXT  pDevContext, bool bDeleteBitmapFile, bool bSetRegistry)
{
    NTSTATUS            Status = STATUS_SUCCESS;
    KIRQL               OldIrql;
    bool                bChanged = false;
    PDEV_BITMAP         pDevBitmap = NULL;
    bool                bFilteringAlreadyStopped = false;
    bool                bBitmapOpened = false;
    bool                isCommitNotifyPending = false;

    if (NULL == pDevContext) {
        Status = STATUS_NO_SUCH_DEVICE;
        return Status;
    }

    // This IOCTL should be treated as nop if already volume is stopped from filtering.
    // no registry writes should happen in nop condition
    KeWaitForMutexObject(&pDevContext->BitmapOpenCloseMutex, Executive, KernelMode, FALSE, NULL);
    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);

    RtlZeroMemory(&pDevContext->replicationStats, sizeof(pDevContext->replicationStats));

    if (!TEST_FLAG(pDevContext->ulFlags, DCF_FILTERING_STOPPED)) {
        // Stop Filtering with following option
        // bLockAcquired: true
        // bClearBitmapFile: false
        KeQuerySystemTime(&pDevContext->DiskTelemetryInfo.liStopFilteringTimestampByUser);
        SET_FLAG(pDevContext->DiskTelemetryInfo.ullDiskBlendedState, DBS_FILTERING_STOPPED_BY_USER);
        isCommitNotifyPending = pDevContext->TagCommitNotifyContext.isCommitNotifyTagPending;
        StopFilteringDevice(pDevContext, true, false, bDeleteBitmapFile, &pDevBitmap);
        bChanged = true;
    }
    else if (bDeleteBitmapFile) {
        pDevBitmap = pDevContext->pDevBitmap;
        pDevContext->pDevBitmap = NULL;
        ASSERT((ecCaptureModeUninitialized == pDevContext->eCaptureMode) &&
            (ecWriteOrderStateUnInitialized == pDevContext->ePrevWOState) &&
            (ecWriteOrderStateUnInitialized == pDevContext->eWOState));
        // Below assignment for eCaptureMode, ePrevWOState, eWOState are part of 
        // defensive programming. These values should already have been in this state.
        // The above ASSERT enforces this.
        pDevContext->eCaptureMode = ecCaptureModeUninitialized;
        pDevContext->ePrevWOState = ecWriteOrderStateUnInitialized;
        pDevContext->eWOState = ecWriteOrderStateUnInitialized;
        bChanged = true;
        bFilteringAlreadyStopped = true;
    }
    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);

    if (isCommitNotifyPending) {
        IndskFltCompleteTagNotifyIfDone(pDevContext);
    }

    ResetDrainState(pDevContext);

    if (bChanged) {
        if (NULL != pDevBitmap) {
            if (bDeleteBitmapFile) {
                KeQuerySystemTime(&pDevContext->liDeleteBitmapTimeStamp);
                // bClearBitmap: true
                // bDeleteBitmap: true
                CloseBitmapFile(pDevBitmap, true, pDevContext, cleanShutdown, true);
            }
            else {
                ClearBitmapFile(pDevBitmap);
            }
        }

        if (bDeleteBitmapFile) {
            // Irrespective of CloseBitmapFile called or not, attempt to delete
            // the bitmap file. Few cases where this will be useful are
            // a) if the filtering hasn't been started for this device / stopped.
            // b) if the bitmap file handle wasn't opened by BitmapAPI.

            TryDeleteBitmapFile(pDevContext);
        }
    }
    else {
        InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
            ("DeviceIoControlStopFiltering: Filtering already disabled on device %S(%S)\n",
                pDevContext->wDevID, pDevContext->wDevNum));
    }

    if (bSetRegistry)
    {
        SetDWORDInRegVolumeCfg(pDevContext, DEVICE_FILTERING_DISABLED, DEVICE_FILTERING_DISABLED_SET, 1);
    }

    KeReleaseMutex(&pDevContext->BitmapOpenCloseMutex, FALSE);

    if (bBitmapOpened) {
        DereferenceDevBitmap(pDevBitmap);
    }

    if (bSetRegistry) {
        InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_FILTERING_STOPPED, pDevContext, pDevContext->ulFlags);
    }
    else {
        InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_FILTERING_STOPPED_BY_CLUSTER, pDevContext, pDevContext->ulFlags);
    }

    return Status;
}

NTSTATUS
DeviceIoControlStopFiltering(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS                Status = STATUS_SUCCESS;
    NTSTATUS                FinalStatus = STATUS_SUCCESS;
    PDEV_CONTEXT            pDevContext = NULL;
    PIO_STACK_LOCATION      IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    bool                    bDeleteBitmapFile = false;
    bool                    bResetClusterState = true;

    PSTOP_FILTERING_INPUT   Input = (PSTOP_FILTERING_INPUT)Irp->AssociatedIrp.SystemBuffer;
    LIST_ENTRY              VolumeNodeList;

    InitializeListHead(&VolumeNodeList);

    ASSERT(IOCTL_INMAGE_STOP_FILTERING_DEVICE == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

    if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < sizeof(STOP_FILTERING_INPUT)) {
        FinalStatus = STATUS_BUFFER_TOO_SMALL;
        goto Cleanup;
    }
    
    bDeleteBitmapFile = TEST_FLAG(Input->ulFlags, STOP_FILTERING_FLAGS_DELETE_BITMAP);
    if (!bDeleteBitmapFile)
    {
        FinalStatus = STATUS_INVALID_PARAMETER;
        goto Cleanup;
    }

    bResetClusterState = !TEST_FLAG(Input->ulFlags, STOP_FILTERING_FLAGS_DONT_CLEAN_CLUSTER_STATE);

    if (TEST_FLAG(Input->ulFlags, STOP_ALL_FILTERING_FLAGS))
    {
        Status = GetListOfDevs(&VolumeNodeList);
        if (!NT_SUCCESS(Status)) {
            FinalStatus = Status;
            goto Cleanup;
        }

        if (IsListEmpty(&VolumeNodeList)) {
            FinalStatus = STATUS_NO_SUCH_DEVICE;
            goto Cleanup;
        } else {
           KeQuerySystemTime(&DriverContext.liStopFilteringAllTimestamp);
           InDskFltWriteEvent(INDSKFLT_INFO_STOPF_ALL_TIMESTAMP, DriverContext.liStopFilteringAllTimestamp.QuadPart);
        }
    }
    else
    {
        pDevContext = GetDevContextForThisIOCTL(DeviceObject, Irp);
        if (NULL == pDevContext) {
            FinalStatus = STATUS_NO_SUCH_DEVICE;
            goto Cleanup;
        }
    }

    while ((NULL != pDevContext) || !IsListEmpty(&VolumeNodeList)) {
        if (NULL == pDevContext) {
            PLIST_NODE      pNode;
            pNode = (PLIST_NODE)RemoveHeadList(&VolumeNodeList);
            if (NULL != pNode) {
                pDevContext = (PDEV_CONTEXT)pNode->pData;
                DereferenceListNode(pNode);
            }
        }

        InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_FILTERING_STOPPED_BY_USER, pDevContext, pDevContext->ulFlags);

        if (IS_DISK_CLUSTERED(pDevContext)) {
            CLEAR_FLAG(pDevContext->ulClusterFlags, DCF_CLUSTER_FLAGS_DISK_PROTECTED);

            if (bResetClusterState) {
                pDevContext->ulClusterFlags = 0;


                // Reset Cluster Attributes in registry
                NTSTATUS Status1 = SetDWORDInRegVolumeCfg(pDevContext, CLUSTER_ATTRIBUTES, 0);
                if (!NT_SUCCESS(Status1)) {
                    // Failed to update Cluster Attributes
                    // This call can fail if device id is not obtained or device id has been reset.
                    InDskFltWriteEventWithDevCtxt(INDSKFLT_WARN_CLUSTER_KEY_WRITE_FAILED, pDevContext, Status1);
                }

                InDskFltDeleteClusterNotificationThread(pDevContext);
            }
        }

        Status = StopFiltering(pDevContext, bDeleteBitmapFile);
        if (NT_SUCCESS(Status))
        {
            // At this point Bitmap is deleted
            // Clear Resync info
            ResetDevOutOfSync(pDevContext, bDeleteBitmapFile);
            ResetDeviceId(pDevContext, true);
        }
        else if (NT_SUCCESS(FinalStatus))
        {
            // Ideally we want status for each of disk for which driver is stopping filtering.
            // That may be changed later. Currently retruning first error that we see while stopping filtering.
            FinalStatus = Status;
        }
        DereferenceDevContext(pDevContext);

        pDevContext = NULL;
    }
Cleanup:
    Irp->IoStatus.Status = FinalStatus;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return FinalStatus;
}
/* TODO - Let's not mix the driver version with feature set supported by driver, introduce new IOCTL to retrieve set of features
*/
NTSTATUS
DeviceIoControlGetVersion(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    UNREFERENCED_PARAMETER(DeviceObject);

    ASSERT(IOCTL_INMAGE_GET_VERSION == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(DRIVER_VERSION)) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL, 
            ("%s: IOCTL rejected, input buffer too small\n", __FUNCTION__));
        Status = STATUS_BUFFER_TOO_SMALL;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    PDRIVER_VERSION DriverVersion = (PDRIVER_VERSION)Irp->AssociatedIrp.SystemBuffer;

    DriverVersion->ulDrMajorVersion = DISKFLT_DRIVER_MAJOR_VERSION;
    DriverVersion->ulDrMinorVersion = DISKFLT_DRIVER_MINOR_VERSION;
    DriverVersion->ulDrMinorVersion2 = DISKFLT_DRIVER_MINOR_VERSION2;
    DriverVersion->ulDrMinorVersion3 = DISKFLT_DRIVER_MINOR_VERSION3;

    DriverVersion->ulPrMajorVersion = INMAGE_PRODUCT_VERSION_MAJOR;
    DriverVersion->ulPrMinorVersion = INMAGE_PRODUCT_VERSION_MINOR;
    DriverVersion->ulPrMinorVersion2 = INMAGE_PRODUCT_VERSION_PRIVATE;
    DriverVersion->ulPrBuildNumber = INMAGE_PRODUCT_VERSION_BUILDNUM;

    Status = STATUS_SUCCESS;
    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = sizeof(DRIVER_VERSION);
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS
InDskFltRescanDisk(
_In_    PDEV_CONTEXT pDevContext
)
/*++

Routine Description:

This routine tries to get signature of the given disk.

Arguments:

pDevContext    - Device context for the given disk.

Return Value:
NTSTATUS
--*/
{
    KIRQL               irql;
    NTSTATUS            Status = STATUS_NO_SUCH_DEVICE;
    PDEVICE_OBJECT      DeviceObject = NULL;
    PDEVICE_EXTENSION   DeviceExtension = NULL;

    KeAcquireSpinLock(&pDevContext->Lock, &irql);
    if (NULL == pDevContext->FilterDeviceObject)
    {
        KeReleaseSpinLock(&pDevContext->Lock, irql);
        return Status;
    }

    DeviceObject = pDevContext->FilterDeviceObject;
    DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;

    Status = IoAcquireRemoveLock(&DeviceExtension->RemoveLock, pDevContext);
    KeReleaseSpinLock(&pDevContext->Lock, irql);

    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    Status = IndskFltSetDeviceInformation(DeviceObject, NULL);
    IoReleaseRemoveLock(&DeviceExtension->RemoveLock, pDevContext);
    return Status;
}

BOOLEAN
InDskFltRescanUninitializedDisks()
/*++

Routine Description:

This routine tries to get signature of all uninitialized disks.

Arguments:

None

Return Value:
    TRUE: If signature is received for atleast one uninitialized disk.
--*/
{
    NTSTATUS        Status = FALSE;
    BOOLEAN         bReturn = FALSE;
    LIST_ENTRY      UninitializedDevExtList;
    PDEV_CONTEXT    pDevContext = NULL;
    PLIST_NODE      pNode;
    PLIST_ENTRY     l = NULL;

    InitializeListHead(&UninitializedDevExtList);

    // If there was already one rescan
    // Wait for that rescan to get completed.
    // Ideally this code can be improved by piggying back on one rescan.
    // For time being this is simple fix for disk rescan.
    KeWaitForMutexObject(&DriverContext.DiskRescanMutex, Executive, KernelMode, FALSE, NULL);

    Status = GetUninitializedDisks(&UninitializedDevExtList);
    if (!NT_SUCCESS(Status))
    {
        goto Cleanup;
    }

    for (l = UninitializedDevExtList.Flink; l != &UninitializedDevExtList; l = l->Flink)
    {
        pNode = CONTAINING_RECORD(l, LIST_NODE, ListEntry);
        pDevContext = (PDEV_CONTEXT)pNode->pData;
        Status = InDskFltRescanDisk(pDevContext);
        if (NT_SUCCESS(Status))
        {
            bReturn = TRUE;
        }

        RemoveEntryList(l);
        l = l->Blink;

        DereferenceDevContext(pDevContext);
        DereferenceListNode(pNode);
    }

Cleanup:
    KeReleaseMutex(&DriverContext.DiskRescanMutex, FALSE);
    return bReturn;
}

/*
Description : This function starts the filtering on a given device object, if already filtering is already stopped.
              Registry would be updated with filtering state to refer on next boot

Input buffer : START_FILTERING_INPUT

Output buffer : NULL
*/

NTSTATUS
DeviceIoControlStartFiltering(
    PDEVICE_OBJECT  DeviceObject,
    PIRP            Irp
)
{
    NTSTATUS                       Status = STATUS_SUCCESS;
    PDEV_CONTEXT                   pDevContext = NULL;
    PIO_STACK_LOCATION             IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    PSTART_FILTERING_INPUT         pStartFilteringInputBuf = NULL;

    // This IOCTL should be treated as nop if already volume is actively filtered.
    // no registry writes should happen in nop condition
    ASSERT(IOCTL_INMAGE_START_FILTERING_DEVICE == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

    if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < sizeof(START_FILTERING_INPUT)) {
        Status = STATUS_INFO_LENGTH_MISMATCH;
        goto Cleanup;
    }

    pStartFilteringInputBuf = (PSTART_FILTERING_INPUT)(Irp->AssociatedIrp.SystemBuffer);

    pDevContext = GetDevContextForThisIOCTL(DeviceObject, Irp);

    if (NULL == pDevContext) {
        Status = STATUS_NO_SUCH_DEVICE;
        goto Cleanup;
    }

    // For cluster disks
    // Cluster workflow will take care of start and stop filtering.
    if (IS_DISK_CLUSTERED(pDevContext)) {
        SET_FLAG(pDevContext->ulClusterFlags, DCF_CLUSTER_FLAGS_DISK_PROTECTED);
        SetDWORDInRegVolumeCfg(pDevContext, DEVICE_FILTERING_DISABLED, DEVICE_FILTERING_DISABLED_UNSET, 1);
        Status = SetDiskClusterState(pDevContext);
        goto Cleanup;
    }

    Status = StartFilteringDevice(pDevContext);
Cleanup:
    if (NULL != pDevContext) {
        DereferenceDevContext(pDevContext);
    }

    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS
StartFilteringDevice(PDEV_CONTEXT   pDevContext)
{
    NTSTATUS                       Status = STATUS_SUCCESS;
    KIRQL                          OldIrql;
    bool                           bChanged = false;

    ASSERT(NULL != pDevContext);

    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
    if (pDevContext->ulFlags & DCF_FILTERING_STOPPED) {
        StartFilteringDevice(pDevContext, true, true);
        pDevContext->ulFlags &= ~DCF_OPEN_BITMAP_FAILED;
        pDevContext->ulFlags |= DCF_IGNORE_BITMAP_CREATION;
        pDevContext->lNumBitmapOpenErrors = 0;
        pDevContext->liFirstBitmapOpenErrorAtTickCount.QuadPart = 0;
        pDevContext->liLastBitmapOpenErrorAtTickCount.QuadPart = 0;
        pDevContext->lNumBitMapClearErrors=0;
        pDevContext->lNumBitMapReadErrors=0;
        pDevContext->lNumBitMapWriteErrors=0;
        bChanged = true;
    }

    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
      
    if (bChanged) {
#ifdef _WIN64
       /* By default Filtering is started on all cluster volume and bitmap file is opened for reading. 
        * If in bitmap filtering is not configured for volume then filtering is stoped. 
        * Data pool is only allocated in openbitmap if filtering is on for cluster volume. 
        * So if later pair is configured then Data Pool should be allocated as bitmap is not reopen */
 
        SetDataPool(1, DriverContext.ullDataPoolSize);

#endif
        Status = SetDWORDInRegVolumeCfg(
            pDevContext,
            DEVICE_BITMAP_MAX_WRITEGROUPS_IN_HEADER,
            pDevContext->ulMaxBitmapHdrWriteGroups,
            true);

        if (NT_SUCCESS(Status)) {
            InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_MAX_HDR_GROUPS_REG_PERSISTED,
                pDevContext,
                Status,
                pDevContext->ulMaxBitmapHdrWriteGroups);
        } else {
            InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_MAX_HDR_GROUPS_REG_WRITE,
                pDevContext,
                Status,
                pDevContext->ulMaxBitmapHdrWriteGroups);

            // Ignore this error.
            Status = STATUS_SUCCESS;
        }

        SetDWORDInRegVolumeCfg(pDevContext, DEVICE_FILTERING_DISABLED, DEVICE_FILTERING_DISABLED_UNSET, 1);
        // For Non-cluster volumes, lets open/read Bitmap File on start filtering, so that the Volume
        // may move to Data WOS instead of just staying in Bitmap WOS. This is needed for VCON
        // where they issue a volume-group based tag. This tag fails if any of the volume is not in
        // Data WOS. This action (opening of bitmap file) will ensure that even if a volume is idle i.e.
        // not receiving any IO, it will move to Data WOS and facilitate issuing of group-based tag.
        // Normally, we open/read bitmap file on first IO, but we have seen cases, where a filtered
        // device may not recieve any IO.
        InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                    ("DeviceIoControlStartFiltering: Filtering enabled on non-cluster device %S(%S). Opening bitmap file\n",
                        pDevContext->wDevID, pDevContext->wDevNum));
        PDEV_BITMAP  pDevBitmap = NULL;
        KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
        if (NULL != pDevContext->pDevBitmap) {
            pDevBitmap = ReferenceDevBitmap(pDevContext->pDevBitmap);
        }
        KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
        if (NULL != pDevBitmap) {
            ExAcquireFastMutex(&pDevBitmap->Mutex);
        }
        KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
        pDevContext->ulFlags |= DCF_OPEN_BITMAP_REQUESTED;
        if (NULL != pDevBitmap){
            if (ecVBitmapStateReadCompleted == pDevBitmap->eVBitmapState) {
                pDevBitmap->eVBitmapState = ecVBitmapStateOpened;
            }
        }
        KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
        if (NULL != pDevBitmap) {
            ExReleaseFastMutex(&pDevBitmap->Mutex);
            DereferenceDevBitmap(pDevBitmap);
        }
        KeSetEvent(&DriverContext.ServiceThreadWakeEvent, IO_DISK_INCREMENT, FALSE);

    } else {
        InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                        ("DeviceIoControlStartFiltering: Filtering already enabled on device %S(%S)\n",
                            pDevContext->wDevID, pDevContext->wDevNum));
    }

    Status = STATUS_SUCCESS;

    return Status;
}


// DTAP to be removed and replaced with new flags
NTSTATUS
DeviceIoControlGetVolumeFlags(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS            Status = STATUS_SUCCESS;
    PDEV_CONTEXT        pDevContext = NULL;
    ULONG               *ulFlags,ulVolFlags;
    KIRQL               OldIrql;

    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(ULONG)) {
        Status = STATUS_BUFFER_TOO_SMALL;
        Irp->IoStatus.Information = 0;
        goto CleanUp;
    }
    
    
    if (0 != IoStackLocation->Parameters.DeviceIoControl.InputBufferLength) {
        pDevContext = GetDevContextForThisIOCTL(DeviceObject, Irp);
    }

    if(NULL == pDevContext) {
        Status = STATUS_NO_SUCH_DEVICE;
        Irp->IoStatus.Information = 0;
        goto CleanUp;
    }

    ulFlags = (ULONG *)Irp->AssociatedIrp.SystemBuffer;
    *ulFlags = 0;

    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);

    ulVolFlags = pDevContext->ulFlags;

    if (pDevContext->bNotify)
        (*ulFlags) |= VSF_DATA_NOTIFY_SET;

    if (DriverContext.bTooManyLastChanceWritesHit &&
        pDevContext->ulMaxBitmapHdrWriteGroups < MAX_WRITE_GROUPS_IN_BITMAP_HEADER_CURRENT) {

        (*ulFlags) |= VSF_RECREATE_BITMAP_FILE;
    }

    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);

    if (ulVolFlags & DCF_FILTERING_STOPPED)
        (*ulFlags)  |= VSF_FILTERING_STOPPED;

    if (ulVolFlags & DCF_BITMAP_WRITE_DISABLED)
        (*ulFlags) |= VSF_BITMAP_WRITE_DISABLED;

    if (ulVolFlags & DCF_BITMAP_READ_DISABLED)
        (*ulFlags) |= VSF_BITMAP_READ_DISABLED;

    if (ulVolFlags & DCF_DATA_CAPTURE_DISABLED)
        (*ulFlags) |= VSF_DATA_CAPTURE_DISABLED;

    if (ulVolFlags & DCF_DATA_FILES_DISABLED)
        (*ulFlags) |= VSF_DATA_FILES_DISABLED;
#ifdef VOLUME_CLUSTER_SUPPORT
    if (ulVolFlags & DCF_VOLUME_ON_CLUS_DISK)
        (*ulFlags) |= VSF_CLUSTER_VOLUME;
#endif
    if (ulVolFlags & DCF_SURPRISE_REMOVAL)
        (*ulFlags) |= VSF_SURPRISE_REMOVED;

    if (ulVolFlags & DCF_FIELDS_PERSISTED)
        (*ulFlags) |= VSF_VCFIELDS_PERSISTED;

#ifdef VOLUME_FLT
    if (ulVolFlags & DCF_PAGEFILE_WRITES_IGNORED)
        (*ulFlags) |= VSF_PAGEFILE_WRITES_IGNORED;
#endif
#ifdef VOLUME_CLUSTER_SUPPORT
    if (ulVolFlags & DCF_CLUSTER_VOLUME_ONLINE)
        (*ulFlags) |= VSF_CLUSTER_VOLUME_ONLINE;
#endif
#ifdef VOLUME_FLT
    if (ulVolFlags & DCF_VOLUME_ON_BASIC_DISK)
        (*ulFlags) |= VSF_VOLUME_ON_BASIC_DISK;
#endif
#ifdef VOLUME_CLUSTER_SUPPORT
    if (ulVolFlags & DCF_VOLUME_ON_CLUS_DISK)
        (*ulFlags) |= VSF_VOLUME_ON_CLUS_DISK;
#endif

    if (ulVolFlags & DCF_EXPLICIT_NONWO_NODRAIN)
        (*ulFlags) |= VSF_EXPLICIT_NONWO_NODRAIN;

    if (ulVolFlags & DCF_DONT_PAGE_FAULT)
        (*ulFlags) |= VSF_DONT_PAGE_FAULT;

    if (ulVolFlags & DCF_LAST_IO)
        (*ulFlags) |= VSF_LAST_IO;

    if (ulVolFlags & DCF_BITMAP_DEV_OFF)
        (*ulFlags) |= VSF_BITMAP_DEV_OFF;

    if (ulVolFlags & DCF_PAGE_FILE_MISSED)
        (*ulFlags) |= VSF_PAGE_FILE_MISSED;

    if (TEST_FLAG(ulVolFlags, DCF_DEVID_OBTAINED))
    {
        if (TEST_FLAG(ulVolFlags, DCF_DISKID_CONFLICT))
            SET_FLAG((*ulFlags), VSF_DISK_ID_CONFLICT);
    }

    if (ulVolFlags & DCF_DEVNUM_OBTAINED)
        (*ulFlags) |= VSF_DEVNUM_OBTAINED;

    if (ulVolFlags & DCF_DEVSIZE_OBTAINED)
        (*ulFlags) |= VSF_DEVSIZE_OBTAINED;

    if (ulVolFlags & DCF_BITMAP_DEV)
        (*ulFlags) |= VSF_BITMAP_DEV;

    if (ulVolFlags & DCF_PHY_DEVICE_INRUSH)
        (*ulFlags) |= VSF_PHY_DEVICE_INRUSH;

    Irp->IoStatus.Information = sizeof(ULONG);

CleanUp:
    if (pDevContext) {
        DereferenceDevContext(pDevContext);
    }
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;

}

// DTAP to be removed and replaced with new flags
NTSTATUS
DeviceIoControlSetVolumeFlags(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status;
    KIRQL               OldIrql;
    PDEV_CONTEXT        pDevContext = NULL;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulInputFlags;
    etBitOperation      eInputBitOperation;
    PVOLUME_FLAGS_INPUT pVolumeFlagsInput = NULL;
    bool ValidOutBuff = 0;

    ASSERT(IOCTL_INMAGE_SET_VOLUME_FLAGS == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);
    Irp->IoStatus.Information = 0;

    if( IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < sizeof(VOLUME_FLAGS_INPUT) ) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL, 
            ("DeviceIoControlSetVolumeFlags: IOCTL rejected, input buffer too small\n"));
        Status = STATUS_BUFFER_TOO_SMALL;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    // Save input flags and operation to perform in local variables. Systembuffer
    // will be used to save the output information.
    pVolumeFlagsInput = (PVOLUME_FLAGS_INPUT)Irp->AssociatedIrp.SystemBuffer;
    eInputBitOperation = pVolumeFlagsInput->eOperation;
    ulInputFlags = pVolumeFlagsInput->ulVolumeFlags;
    // Check if Flag and bitop set in input buffer are all valid flags.
    if ((0 != (ulInputFlags & (~VOLUME_FLAGS_VALID))) ||
        ((ecBitOpReset != eInputBitOperation) && (ecBitOpSet != eInputBitOperation)) ) 
    {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL,
            ("DeviceIoControlSetVolumeFlags: IOCTL rejected, invalid parameters\n"));
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    pDevContext = GetDevContextForThisIOCTL(DeviceObject, Irp);
    if (pDevContext) {
        Status = STATUS_SUCCESS;
        KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
        if (ecBitOpSet == eInputBitOperation) {
#ifdef VOLUME_FLT
            if (ulInputFlags & VOLUME_FLAG_IGNORE_PAGEFILE_WRITES) {
                if (0 == (pDevContext->ulFlags & DCF_PAGEFILE_WRITES_IGNORED)) {
                    pDevContext->ulFlags |= DCF_PAGEFILE_WRITES_IGNORED;
                } else {
                    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                                  ("DeviceIoControlSetVolumeFlags: Already pagefile write ignored flag is set\n"));
                    ulInputFlags &= ~VOLUME_FLAG_IGNORE_PAGEFILE_WRITES;
                }				
            }
#endif
            if (ulInputFlags & VOLUME_FLAG_DISABLE_BITMAP_READ) {
                if (0 == (pDevContext->ulFlags & DCF_BITMAP_READ_DISABLED)) {
                    pDevContext->ulFlags |= DCF_BITMAP_READ_DISABLED;
                } else {
                    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                                  ("DeviceIoControlSetVolumeFlags: Already bitmap read disabled flag is set\n"));
                    ulInputFlags &= ~VOLUME_FLAG_DISABLE_BITMAP_READ;
                }
            }

            if (ulInputFlags & VOLUME_FLAG_DISABLE_BITMAP_WRITE) {
                if (0 == (pDevContext->ulFlags & DCF_BITMAP_WRITE_DISABLED)) {
                    pDevContext->ulFlags |= DCF_BITMAP_WRITE_DISABLED;
                } else {
                    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                                  ("DeviceIoControlSetVolumeFlags: Already bitmap write disabled flag is set\n"));
                    ulInputFlags &= ~VOLUME_FLAG_DISABLE_BITMAP_WRITE;
                }
            }
#ifdef VOLUME_CLUSTER_SUPPORT
            if (ulInputFlags & VOLUME_FLAG_ONLINE) {
                if(0 == (pDevContext->ulFlags & DCF_CLUSTER_VOLUME_ONLINE))  {

                    pDevContext->ulFlags |= DCF_CLUSTER_VOLUME_ONLINE;
                    pDevContext->ulFlags &= ~DCF_CV_FS_UNMOUTNED;
                    pDevContext->ulFlags &= ~DCF_CLUSDSK_RETURNED_OFFLINE;
                } else {
                    
                    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                                  ("DeviceIoControlSetVolumeFlags: Already cluster volume online flag is set\n"));
                    ulInputFlags &= ~VOLUME_FLAG_ONLINE;
                }

            }
#endif

            if (ulInputFlags & VOLUME_FLAG_DISABLE_DATA_CAPTURE) {
                if (0 == (pDevContext->ulFlags & DCF_DATA_CAPTURE_DISABLED)) {
                    pDevContext->ulFlags |= DCF_DATA_CAPTURE_DISABLED;
                    VCSetCaptureModeToMetadata(pDevContext, true);
                    VCSetWOState(pDevContext, true, __FUNCTION__, ecMDReasonUserRequest);
                } else {
                    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                                  ("DeviceIoControlSetVolumeFlags: Already data capture disabled flag is set\n"));
                    ulInputFlags &= ~VOLUME_FLAG_DISABLE_DATA_CAPTURE;
                }
            }

            if (ulInputFlags & VOLUME_FLAG_FREE_DATA_BUFFERS) {
                // Have to free all data buffers for the volume.
                VCFreeAllDataBuffers(pDevContext, true);
            }

            if (ulInputFlags & VOLUME_FLAG_DISABLE_DATA_FILES) {
                if (0 == (pDevContext->ulFlags & DCF_DATA_FILES_DISABLED)) {
                    pDevContext->ulFlags |= DCF_DATA_FILES_DISABLED;
                } else {
                    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                                  ("%s: Already data files disabled flag is set\n", __FUNCTION__));
                    ulInputFlags &= ~DCF_DATA_FILES_DISABLED;
                }
            }

            if (ulInputFlags & VOLUME_FLAG_VCFIELDS_PERSISTED) {
                if (0 == (pDevContext->ulFlags & DCF_FIELDS_PERSISTED)) {
                    pDevContext->ulFlags |= DCF_FIELDS_PERSISTED;
                } else {
                    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                                  ("%s: Already Persist DevContext flas is set \n", __FUNCTION__));
                    ulInputFlags &= ~VOLUME_FLAG_VCFIELDS_PERSISTED;
                }
            }
        } else {
#ifdef VOLUME_FLT
            if (ulInputFlags & VOLUME_FLAG_IGNORE_PAGEFILE_WRITES) {
                if (pDevContext->ulFlags & DCF_PAGEFILE_WRITES_IGNORED) {
                    pDevContext->ulFlags &= ~DCF_PAGEFILE_WRITES_IGNORED;
                } else {
                    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                                  ("DeviceIoControlSetVolumeFlags: Already pagefile write flag is unset\n"));
                    ulInputFlags &= ~VOLUME_FLAG_IGNORE_PAGEFILE_WRITES;
                }
            }
#endif
            if (ulInputFlags & VOLUME_FLAG_DISABLE_BITMAP_READ) {
                if (pDevContext->ulFlags & DCF_BITMAP_READ_DISABLED) {
                    pDevContext->ulFlags &= ~DCF_BITMAP_READ_DISABLED;                   
                } else {
                    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                                  ("DeviceIoControlSetVolumeFlags: Already disable bitmap read flag is unset\n"));
                    ulInputFlags &= ~VOLUME_FLAG_DISABLE_BITMAP_READ;
                }
            }

            if (ulInputFlags & VOLUME_FLAG_DISABLE_BITMAP_WRITE) {
                if (pDevContext->ulFlags & DCF_BITMAP_WRITE_DISABLED) {
                    pDevContext->ulFlags &= ~DCF_BITMAP_WRITE_DISABLED;
                } else {
                    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                                  ("DeviceIoControlSetVolumeFlags: Already disable bitmap read flag is unset\n"));
                    ulInputFlags &= ~VOLUME_FLAG_DISABLE_BITMAP_WRITE;
                }
            }
#ifdef VOLUME_CLUSTER_SUPPORT
            if (ulInputFlags & VOLUME_FLAG_ONLINE) {
                if(pDevContext->ulFlags & DCF_CLUSTER_VOLUME_ONLINE)  {

                    pDevContext->ulFlags &= ~DCF_CLUSTER_VOLUME_ONLINE;
                } else {
                    
                    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                                  ("DeviceIoControlSetVolumeFlags: Already cluster volume online flag is unset\n"));
                    ulInputFlags &= ~VOLUME_FLAG_ONLINE;
                }

            }
#endif

            if (ulInputFlags & VOLUME_FLAG_DISABLE_DATA_CAPTURE) {
                if (pDevContext->ulFlags & DCF_DATA_CAPTURE_DISABLED) {
                    pDevContext->ulFlags &= ~DCF_DATA_CAPTURE_DISABLED;
                } else {
                    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                                  ("DeviceIoControlSetVolumeFlags: Already data capture disabled flag is unset\n"));
                    ulInputFlags &= ~VOLUME_FLAG_DISABLE_DATA_CAPTURE;
                }
            }

            if (ulInputFlags & VOLUME_FLAG_FREE_DATA_BUFFERS) {
                InVolDbgPrint(DL_WARNING, DM_INMAGE_IOCTL,
                              ("DeviceIoControlSetVolumeFlags: FreeDatBuffers unset flag is invalid\n"));
                ulInputFlags &= ~VOLUME_FLAG_FREE_DATA_BUFFERS;
            }

            if (ulInputFlags & VOLUME_FLAG_DISABLE_DATA_FILES) {
                if (pDevContext->ulFlags & DCF_DATA_FILES_DISABLED) {
                    pDevContext->ulFlags &= ~DCF_DATA_FILES_DISABLED;
                } else {
                    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                                  ("%s: Already data files disabled flag is unset\n", __FUNCTION__));
                    ulInputFlags &= ~VOLUME_FLAG_DISABLE_DATA_FILES;
                }
            }

            if (ulInputFlags & VOLUME_FLAG_VCFIELDS_PERSISTED) {
                if (pDevContext->ulFlags & DCF_FIELDS_PERSISTED) {
                    pDevContext->ulFlags &= ~DCF_FIELDS_PERSISTED;
                } else {
                    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                                  ("%s: Already Persist DevContext flag is unset \n", __FUNCTION__));
                    ulInputFlags &= ~VOLUME_FLAG_VCFIELDS_PERSISTED;
                }
            }
        }
        PVOLUME_FLAGS_OUTPUT    pVolumeFlagsOutput = NULL;
        if( IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength >= sizeof(VOLUME_FLAGS_OUTPUT) ) {

            pVolumeFlagsOutput = (PVOLUME_FLAGS_OUTPUT)Irp->AssociatedIrp.SystemBuffer;
            pVolumeFlagsOutput->ulVolumeFlags = 0;
#ifdef VOLUME_FLT
            if (pDevContext->ulFlags & DCF_PAGEFILE_WRITES_IGNORED)
                pVolumeFlagsOutput->ulVolumeFlags |= VOLUME_FLAG_IGNORE_PAGEFILE_WRITES;
#endif
            if (pDevContext->ulFlags & DCF_BITMAP_READ_DISABLED)
                pVolumeFlagsOutput->ulVolumeFlags |= VOLUME_FLAG_DISABLE_BITMAP_READ;

            if (pDevContext->ulFlags & DCF_BITMAP_WRITE_DISABLED)
                pVolumeFlagsOutput->ulVolumeFlags |= VOLUME_FLAG_DISABLE_BITMAP_WRITE;

            if (pDevContext->ulFlags & DCF_DATA_CAPTURE_DISABLED)
                pVolumeFlagsOutput->ulVolumeFlags |= VOLUME_FLAG_DISABLE_DATA_CAPTURE;

            if (pDevContext->ulFlags & DCF_DATA_FILES_DISABLED)
                pVolumeFlagsOutput->ulVolumeFlags |= VOLUME_FLAG_DISABLE_DATA_FILES;

            if (pDevContext->ulFlags & DCF_FIELDS_PERSISTED )
                pVolumeFlagsOutput->ulVolumeFlags |= VOLUME_FLAG_VCFIELDS_PERSISTED;

            ValidOutBuff = 1;
            Irp->IoStatus.Information = sizeof(VOLUME_FLAGS_OUTPUT);
        }

        KeReleaseSpinLock(&pDevContext->Lock, OldIrql);

        if (ulInputFlags & VOLUME_FLAG_DATA_LOG_DIRECTORY) {
            if ((pVolumeFlagsInput->DataFile.usLength > 0) && (pVolumeFlagsInput->DataFile.usLength < UDIRTY_BLOCK_MAX_DATA_LOG_DIRECTORY)) {	
                if (pVolumeFlagsInput->DataFile.FileName[pVolumeFlagsInput->DataFile.usLength/sizeof(WCHAR)] == L'\0') {
                    NTSTATUS StatusVol = KeWaitForSingleObject(&pDevContext->Mutex, Executive, KernelMode, FALSE, NULL);                         
                    ASSERT(STATUS_SUCCESS == StatusVol);
                    StatusVol = InitializeDataLogDirectory(pDevContext, pVolumeFlagsInput->DataFile.FileName, 1);
                    KeReleaseMutex(&pDevContext->Mutex, FALSE);
                    if (NT_SUCCESS(StatusVol) && ValidOutBuff) {
                        pVolumeFlagsOutput->ulVolumeFlags |= VOLUME_FLAG_DATA_LOG_DIRECTORY;
                    }
                } else {
                    InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL,
                            ("DeviceIoControlSetVolumeFlags: IOCTL rejected, DataFile Path is not null terminated\n"));
                }
            } else {
                InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL,
                        ("DeviceIoControlSetVolumeFlags: IOCTL rejected, DataFile Length %d should be less than %d\n", 
                        pVolumeFlagsInput->DataFile.usLength, UDIRTY_BLOCK_MAX_DATA_LOG_DIRECTORY));
            }	    
        }

        if (ecBitOpSet == eInputBitOperation) {
#ifdef VOLUME_FLT
            if (ulInputFlags & VOLUME_FLAG_IGNORE_PAGEFILE_WRITES)
                SetDWORDInRegVolumeCfg(pDevContext, VOLUME_PAGEFILE_WRITES_IGNORED, VOLUME_PAGEFILE_WRITES_IGNORED_SET);
#endif            
            if (ulInputFlags & VOLUME_FLAG_DISABLE_BITMAP_READ)
                SetDWORDInRegVolumeCfg(pDevContext, DEVICE_BITMAP_READ_DISABLED, DEVICE_BITMAP_READ_DISABLED_SET);

            if (ulInputFlags & VOLUME_FLAG_DISABLE_BITMAP_WRITE)
                SetDWORDInRegVolumeCfg(pDevContext, DEVICE_BITMAP_WRITE_DISABLED, DEVICE_BITMAP_WRITE_DISABLED_SET);

            if (ulInputFlags & VOLUME_FLAG_READ_ONLY)
                SetDWORDInRegVolumeCfg(pDevContext, DEVICE_READ_ONLY, DEVICE_READ_ONLY_SET);

            if (ulInputFlags & VOLUME_FLAG_DISABLE_DATA_CAPTURE)
                SetDWORDInRegVolumeCfg(pDevContext, DEVICE_DATAMODE_CAPTURE, DEVICE_DATA_CAPTURE_UNSET);
            
            if (ulInputFlags & VOLUME_FLAG_DISABLE_DATA_FILES)
                SetDWORDInRegVolumeCfg(pDevContext, DEVICE_DATA_FILES, DEVICE_DATA_FILES_UNSET);
            
        } else {
#ifdef VOLUME_FLT
            if (ulInputFlags & VOLUME_FLAG_IGNORE_PAGEFILE_WRITES)
                SetDWORDInRegVolumeCfg(pDevContext, VOLUME_PAGEFILE_WRITES_IGNORED, VOLUME_PAGEFILE_WRITES_IGNORED_UNSET);
#endif            
            if (ulInputFlags & VOLUME_FLAG_DISABLE_BITMAP_READ)
                SetDWORDInRegVolumeCfg(pDevContext, DEVICE_BITMAP_READ_DISABLED, DEVICE_BITMAP_READ_DISABLED_UNSET);

            if (ulInputFlags & VOLUME_FLAG_DISABLE_BITMAP_WRITE)
                SetDWORDInRegVolumeCfg(pDevContext, DEVICE_BITMAP_WRITE_DISABLED, DEVICE_BITMAP_WRITE_DISABLED_UNSET);

            if (ulInputFlags & VOLUME_FLAG_READ_ONLY)
                SetDWORDInRegVolumeCfg(pDevContext, DEVICE_READ_ONLY, DEVICE_READ_ONLY_UNSET);

            if (ulInputFlags & VOLUME_FLAG_DISABLE_DATA_CAPTURE)
                SetDWORDInRegVolumeCfg(pDevContext, DEVICE_DATAMODE_CAPTURE, DEVICE_DATA_CAPTURE_SET);

            if (ulInputFlags & VOLUME_FLAG_DISABLE_DATA_FILES)
                SetDWORDInRegVolumeCfg(pDevContext, DEVICE_DATA_FILES, DEVICE_DATA_FILES_SET);
     
        }

        DereferenceDevContext(pDevContext);
    }
    else
    {
		Status = STATUS_NO_SUCH_DEVICE;
    }

    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DeviceIoControlNotifySystemState(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    NTSTATUS    Status;
    bool        bStateRegMutexAcquired = false;
    LONG        flagsToSet;
    PIO_STACK_LOCATION          IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    PCNOTIFY_SYSTEM_STATE_INPUT pInput = (PCNOTIFY_SYSTEM_STATE_INPUT)Irp->AssociatedIrp.SystemBuffer;
    PNOTIFY_SYSTEM_STATE_OUTPUT pOutput = (PNOTIFY_SYSTEM_STATE_OUTPUT)Irp->AssociatedIrp.SystemBuffer;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(DeviceObject);
    NT_ASSERT(IoStackLocation->Parameters.DeviceIoControl.IoControlCode == IOCTL_INMAGE_NOTIFY_SYSTEM_STATE);

    if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < sizeof(NOTIFY_SYSTEM_STATE_INPUT)) {

        NT_ASSERT(FALSE);
        Status = STATUS_INFO_LENGTH_MISMATCH;

        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL,
            (__FUNCTION__ ": IOCTL rejected, input buffer size %lu is less than expected size %lu. Status Code %#x.\n",
            IoStackLocation->Parameters.DeviceIoControl.InputBufferLength, sizeof(NOTIFY_SYSTEM_STATE_INPUT), Status));

        goto Cleanup;
    }

    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(NOTIFY_SYSTEM_STATE_OUTPUT)) {

        NT_ASSERT(FALSE);
        Status = STATUS_BUFFER_TOO_SMALL;

        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL,
            (__FUNCTION__ ": IOCTL rejected, output buffer size %lu is less than expected size %lu. Status Code %#x.\n",
            IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength, sizeof(NOTIFY_SYSTEM_STATE_OUTPUT), Status));

        goto Cleanup;
    }

    flagsToSet = pInput->Flags;

    if (0 != (flagsToSet & ~(ssFlagsAreBitmapFilesEncryptedOnDisk))) {

        NT_ASSERT(FALSE);
        Status = STATUS_INVALID_PARAMETER;

        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL,
            (__FUNCTION__ ": IOCTL rejected, unknown flags detected in %d. Status Code %#x.\n",
            flagsToSet, Status));

        goto Cleanup;
    }

    NT_VERIFY(STATUS_SUCCESS ==
        KeWaitForSingleObject(&DriverContext.SystemStateRegistryMutex, Executive, KernelMode, FALSE, NULL));

    bStateRegMutexAcquired = true;

    if (DriverContext.SystemStateFlags != flagsToSet) {

        Status = SetDWORDInParameters(SYSTEM_STATE_FLAGS, flagsToSet);

        if (!NT_SUCCESS(Status)) {

            InDskFltWriteEvent(INDSKFLT_ERROR_SYSTEM_STATE_FLAGS_REG_SET, flagsToSet, Status);

            NT_ASSERT(FALSE);
            goto Cleanup;
        }

        InDskFltWriteEvent(INDSKFLT_INFO_SYSTEM_STATE_FLAGS_REG_PERSISTED, flagsToSet, Status);

        NT_VERIFY(flagsToSet !=
            InterlockedExchange(&DriverContext.SystemStateFlags, flagsToSet));
    }

    // Fill the output buffer
    pOutput->ResultFlags = DriverContext.SystemStateFlags;
    Status = STATUS_SUCCESS;

Cleanup:
    if (bStateRegMutexAcquired) {
        NT_VERIFY(0 ==
            KeReleaseMutex(&DriverContext.SystemStateRegistryMutex, FALSE));

        bStateRegMutexAcquired = false;
    }

    Irp->IoStatus.Information = NT_SUCCESS(Status) ? sizeof(NOTIFY_SYSTEM_STATE_OUTPUT) : 0;
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return Status;
}

NTSTATUS
ProcessLocalDeviceControlRequests(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulIoControlCode;
    NTSTATUS            Status = STATUS_INVALID_DEVICE_REQUEST;
 
    ASSERT(DeviceObject == DriverContext.ControlDevice);

    ulIoControlCode = IoStackLocation->Parameters.DeviceIoControl.IoControlCode;

    InVolDbgPrint(DL_VERBOSE, DM_INMAGE_IOCTL, 
        ("ProcessLocalDeviceControlRequests: DeviceObject = %#p  Irp = %#p  CtlCode = %#x\n",
        DeviceObject, Irp, ulIoControlCode));

    switch(ulIoControlCode) {
    case IOCTL_INMAGE_PROCESS_START_NOTIFY:
        Status = DeviceIoControlProcessStartNotify(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_SERVICE_SHUTDOWN_NOTIFY:
        Status = DeviceIoControlServiceShutdownNotify(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_CLEAR_DIFFERENTIALS:
        Status = DeviceIoControlClearBitMap(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_STOP_FILTERING_DEVICE:
        Status = DeviceIoControlStopFiltering(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_START_FILTERING_DEVICE:
        Status = DeviceIoControlStartFiltering(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_SET_DISK_CLUSTERED:
        Status = DeviceIoControlSetDiskClustered(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_GET_VOLUME_WRITE_ORDER_STATE:
        Status = DeviceIoControlGetVolumeWriteOrderState(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_GET_DEVICE_TRACKING_SIZE:
        Status = DeviceIoControlGetDeviceTrackingSize(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_GET_VOLUME_FLAGS: //DTAP to be removed
        Status = DeviceIoControlGetVolumeFlags(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_GET_VOLUME_STATS:
        Status = DeviceIoControlGetVolumeStats(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_GET_ADDITIONAL_DEVICE_STATS:
        Status = DeviceIoControlGetAdditionalDeviceStats(DeviceObject, Irp);		
        break;
    case IOCTL_INMAGE_GET_DATA_MODE_INFO:
        Status = DeviceIoControlGetDMinfo(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS:
        Status = DeviceIoControlGetDirtyBlocksTransaction(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS:
        Status = DeviceIoControlCommitDirtyBlocksTransaction(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_SET_RESYNC_REQUIRED:
        Status = DeviceIoControlSetResyncRequired(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_SET_VOLUME_FLAGS: //DTAP to be removed
        Status = DeviceIoControlSetVolumeFlags(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_CLEAR_VOLUME_STATS:
        Status = DeviceIoControlClearDevStats(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_TAG_DISK:
        Status = DeviceIoControlAppTagDisk(DeviceObject, Irp);
        break;
	case IOCTL_INMAGE_COMMIT_TAG_DISK:
        Status = DeviceIoControlCommitAppTagDisk(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_IOBARRIER_TAG_DISK:
        Status = DeviceIoControlIoBarrierTagDisk(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_GET_NUM_INFLIGHT_IOS:
        Status = DeviceIoControlGetNumInflightIos(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_HOLD_WRITES:
        Status = DeviceIoControlHoldWrites(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_DISTRIBUTED_CRASH_TAG:
        Status = DeviceIoControlDistributedCrashTag(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_RELEASE_WRITES:
        Status = DeviceIoControlReleaseWrites(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_COMMIT_DISTRIBUTED_CRASH_TAG:
        Status = DeviceIoControlCommitDistributedTag(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_GET_TAG_VOLUME_STATUS:
        Status = DeviceIoControlGetTagVolumeStatus(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_SET_DIRTY_BLOCK_NOTIFY_EVENT:
        Status = DeviceIoControlSetDirtyBlockNotifyEvent(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_SET_DATA_FILE_THREAD_PRIORITY:
        Status = DeviceIoControlSetDataFileThreadPriority(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_SET_WORKER_THREAD_PRIORITY:
        Status = DeviceIoControlSetWorkerThreadPriority(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_SET_DATA_SIZE_TO_DISK_LIMIT:
        Status = DeviceIoControlSetDataSizeToDiskLimit(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_SET_MIN_FREE_DATA_SIZE_TO_DISK_LIMIT:
        Status = DeviceIoControlSetMinFreeDataSizeToDiskLimit(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_SET_DATA_NOTIFY_LIMIT:
        Status = DeviceIoControlSetDataNotifyLimit(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_SET_IO_SIZE_ARRAY:
        Status = DeviceIoControlSetIoSizeArray(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_SET_DRIVER_FLAGS:
        Status = DeviceIoControlSetDriverFlags(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_SET_DRIVER_CONFIGURATION:
        Status = DeviceIoControlSetDriverConfig(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_GET_VERSION:
        Status = DeviceIoControlGetVersion(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_RESYNC_START_NOTIFICATION:
        Status = DeviceIoControlResyncStartNotification(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_RESYNC_END_NOTIFICATION:
        Status = DeviceIoControlResyncEndNotification(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_GET_LCN:
        Status = DeviceIoControlGetLcn(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_GET_GLOBAL_TIMESTAMP:
        Status = DeviceIoControlGetGlobalTSSN(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_TAG_PRECHECK:
        Status = DeviceIoControlPreCheckTagDisk(DeviceObject, Irp);
        break;
#ifdef _CUSTOM_COMMAND_CODE_        
    case IOCTL_INMAGE_CUSTOM_COMMAND:
        Status = DeviceIoControlCustomCommand(DeviceObject, Irp);
        break;
#endif // _CUSTOM_COMMAND_CODE_
#if DBG
    case IOCTL_INMAGE_SET_DEBUG_INFO:
        Status = DeviceIoControlSetDebugInfo(DeviceObject, Irp);
        break;
#endif // DBG
    //case IOCTL_INMAGE_SYNC_TAG_VOLUME:
        //DTAP check
        //Status = DeviceIoControlSyncTagVolume(DeviceObject, Irp);
#ifndef VOLUME_FLT
    case IOCTL_INMAGE_GET_MONITORING_STATS:
        Status = DeviceIoControlGetMonitoringStats(DeviceObject, Irp);
        break;
#endif
    case IOCTL_INMAGE_SET_REPLICATION_STATE:
        Status = DeviceIoControlSetReplicationState(DeviceObject, Irp);
        break;

    case IOCTL_INMAGE_GET_REPLICATION_STATS:
        Status = DeviceIoControlGetReplicationStats(DeviceObject, Irp);
        break;

    case IOCTL_INMAGE_GET_DISK_INDEX_LIST:
        Status = DeviceIoControlGetDiskIndexList(DeviceObject, Irp);
        break;

    case IOCTL_INMAGE_COMMITDB_FAIL_TRANS:
        Status = DeviceIoControlCommitDBFailTrans(DeviceObject, Irp);
        break;

    case IOCTL_INMAGE_GET_CXSTATS_NOTIFY:
        Status = DeviceIoControlGetCXStatsNotify(DeviceObject, Irp);
        break;

    case IOCTL_INMAGE_NOTIFY_SYSTEM_STATE:
        Status = DeviceIoControlNotifySystemState(DeviceObject, Irp);
        break;

    case IOCTL_INMAGE_SET_SAN_POLICY:
        Status = DeviceIoControlSetSanPolicy(DeviceObject, Irp);
        break;

    case IOCTL_INMAGE_GET_DRIVER_FLAGS:
        Status = DeviceIoControlGetDriverFlags(DeviceObject, Irp);
        break;

    case IOCTL_INMAGE_TAG_COMMIT_NOTIFY:
        Status = DeviceIoControlTagCommitNotify(DeviceObject, Irp);
        break;

    case IOCTL_INMAGE_SET_DRAIN_STATE:
        Status = DeviceIoControlSetDrainState(DeviceObject, Irp);
        break;

    case IOCTL_INMAGE_GET_DEVICE_STATE:
        Status = DeviceIoControlGetDiskState(DeviceObject, Irp);
        break;

    default:
        Status = STATUS_INVALID_DEVICE_REQUEST;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        break;
    }

    return Status;

} // end ProcessLocalDeviceControlRequests()

NTSTATUS
InDskFltDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{

    NTSTATUS Status = STATUS_UNSUCCESSFUL;

    if (DeviceObject == DriverContext.ControlDevice) {
        Status = ProcessLocalDeviceControlRequests(DeviceObject, Irp);
        goto Cleanup;
    } else if (DeviceObject == DriverContext.PostShutdownDO) {
        Status = STATUS_INVALID_DEVICE_REQUEST;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        goto Cleanup;
    }
    Status = InMageFltDeviceControl(DeviceObject, Irp);
Cleanup:
    return Status;
}

/*

Description:    This device iocontrol handles the IOCTLs from agent to facilitate start, stop, resync and drain the differentials
                      This also handles the defrag IOCTL.

Args			:   DeviceObject - Encapsulates the current volume context
					Irp                - The i/o request meant for lower level drivers, usually

Return Value:     NTSTATUS
*/

NTSTATUS
InMageFltDeviceControl(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    IN BOOLEAN bNoRebootLayer,
    IN PDRIVER_DISPATCH pDispatchEntryFunction
    )
{
    PDEVICE_EXTENSION   DeviceExtension = NULL;//(PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulIoControlCode;
    NTSTATUS            Status = STATUS_SUCCESS;
    

    ulIoControlCode = IoStackLocation->Parameters.DeviceIoControl.IoControlCode;

    DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
#ifdef INMAGE_DBG
    //DumpIoctlInformation(DeviceObject, Irp);
#endif  
    
    InVolDbgPrint(DL_FUNC_TRACE, DM_INMAGE_IOCTL, ("InMageDeviceControl: Process = %.16s(%x.%x) DeviceObject = %#p  Irp = %#p  CtlCode = %#x\n",
                        PsGetProcessImageFileName(PsGetCurrentProcess()), PtrToUlong(PsGetCurrentProcessId()), PtrToUlong(PsGetCurrentThreadId()), DeviceObject, Irp, ulIoControlCode));

    // If request has come on device object try to acquire remove lock on these devices
    Status = IoAcquireRemoveLock(&DeviceExtension->RemoveLock, Irp);
    if (!NT_SUCCESS(Status))
    {
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    switch(ulIoControlCode) {
    case IOCTL_DISK_GET_LENGTH_INFO:
        Status = DeviceIoControlGetLengthInfo(DeviceObject, Irp, bNoRebootLayer);
        break;

#ifdef _WIN64
    case IOCTL_STORAGE_PERSISTENT_RESERVE_OUT:
        Status = DeviceIoControlStoragePersistentReserveOut(DeviceObject, Irp, bNoRebootLayer, pDispatchEntryFunction);
        break;
    case IOCTL_SCSI_PASS_THROUGH:
    case IOCTL_SCSI_PASS_THROUGH_DIRECT:
    case IOCTL_SCSI_PASS_THROUGH_EX:
    case IOCTL_SCSI_PASS_THROUGH_DIRECT_EX:
        Status = DeviceIoControlSCSIPassThrough(DeviceObject, Irp);
        __fallthrough;
#endif
    default:
        InSkipCurrentIrpStackLocation(Irp, bNoRebootLayer);
        Status = InCallDriver(DeviceExtension->TargetDeviceObject, 
                              Irp, 
                              bNoRebootLayer, 
                              pDispatchEntryFunction);
        break;
    }

    IoReleaseRemoveLock(&DeviceExtension->RemoveLock, Irp);

    return Status;

} // end InMageDeviceControl()

NTSTATUS
InMageFltShutdown(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    PDEVICE_EXTENSION       pDeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
	NTSTATUS                Status = STATUS_UNSUCCESSFUL;

    InVolDbgPrint(DL_INFO, DM_SHUTDOWN, 
                 ("InMageFltShutdown: DeviceObject %#p Irp %#p\n", DeviceObject,
                 Irp));
    // Pass down IRP for the filter device object
    if ((DeviceObject != DriverContext.PostShutdownDO) &&
        (DeviceObject != DriverContext.ControlDevice)) {

        Status = IoAcquireRemoveLock(&pDeviceExtension->RemoveLock, Irp);
        if (!NT_SUCCESS(Status)) {
            InVolDbgPrint(DL_INFO, DM_SHUTDOWN,
                         ("InMageFltDispatchPower: IoAcquireRemoveLock Dishonoured with Status %lx\n",
                          Status));
            Irp->IoStatus.Status = Status;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return Status;
        }
        // Invalid Case(ASSERT). In case a call comes to NoReboot Device directly
        if (NULL == pDeviceExtension->TargetDeviceObject) {
            Status = STATUS_DEVICE_NOT_CONNECTED;
            Irp->IoStatus.Status = Status;
            Irp->IoStatus.Information = 0;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
        } else {
            IoSkipCurrentIrpStackLocation(Irp);
            Status = IoCallDriver(pDeviceExtension->TargetDeviceObject, Irp);
        }
        IoReleaseRemoveLock(&pDeviceExtension->RemoveLock,Irp);
        return Status;
    }
    // PreShutdown Operations
    if (DeviceObject == DriverContext.ControlDevice) {
        InVolDbgPrint(DL_INFO, DM_SHUTDOWN, 
                     ("InMageFltShutdown: Shutdown IRP received on control device\n"));	

        PreShutdown();
        goto Cleanup;
    }

    // PostShutdown Operations
    PostShutdown();

Cleanup:
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
} // end InMageFltShutdown()

NTSTATUS
InDskFltAddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PhysicalDeviceObject
    )
{
    NTSTATUS Status;
    Status = InMageFltAddDevice(DriverObject, PhysicalDeviceObject);
    if (!NT_SUCCESS(Status)) {
        InDskFltWriteEvent(INDSKFLT_ERROR_REBOOT_ADD_DISK_FAILURE, Status);
    }
    return Status;
}

/*
Description: Creates and initializes a new filter device object FiDO for the corresponding PDO. Then it attaches the device object to the device
                  stack of the drivers for the device.
                  
Args        : DriverObject - filter filter driver's Driver Object
              PhysicalDeviceObject - Physical Device Object from the underlying layered driver

Return Value:  NTSTATUS
*/

NTSTATUS
InMageFltAddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT LowerDeviceObject,
    IN BOOLEAN        bNoRebootLayer,
    IN BOOLEAN        bIsPageFileDevicePossible
    )
{
    NTSTATUS            Status;
    PDEVICE_OBJECT      FilterDeviceObject;
    PDEVICE_EXTENSION   pDeviceExtension;
    PDEV_CONTEXT        pDevContext = NULL;
    KIRQL               OldIrql;
    HANDLE             FileHandle = NULL;
    //
    // Create a filter device object for this device (partition).
    //

    InVolDbgPrint(DL_FUNC_TRACE, DM_DEV_ARRIVAL,
        ("InMageFltAddDevice: Driver = %#p  PDO = %#p\n", DriverObject, LowerDeviceObject));

    if (bNoRebootLayer) {
        // Open a handle to disk device to prevent removal
        Status = OpenHandleToDevice(LowerDeviceObject, &FileHandle);
        if (!NT_SUCCESS(Status)) {
            return Status;
        }
    }

    pDevContext = AllocateDevContext();
    if (NULL == pDevContext){
        if (bNoRebootLayer) {
            ZwClose(FileHandle);
        }
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Status = InDskFltCreateReleaseWritesThread(pDevContext);
    if (!NT_SUCCESS(Status))
    {
        if (bNoRebootLayer) {
            ZwClose(FileHandle);
        }
        DereferenceDevContext(pDevContext);
        return Status;
    }

    Status = IoCreateDevice(DriverObject,
                            DEVICE_EXTENSION_SIZE,
                            NULL,
                            FILE_DEVICE_DISK,
                            FILE_DEVICE_SECURE_OPEN,
                            FALSE,
                            &FilterDeviceObject);

    if (!NT_SUCCESS(Status)) {
       InVolDbgPrint(DL_ERROR, DM_DEV_ARRIVAL,
           ("InMageFltAddDevice: IoCreateDevice failed with Status %#x - Cannot create filterDeviceObject\n",
           Status));
       if (bNoRebootLayer) {
           ZwClose(FileHandle);
       }
       DereferenceDevContext(pDevContext);
       return Status;
    }

    FilterDeviceObject->Flags |= DO_DIRECT_IO;

    pDeviceExtension = (PDEVICE_EXTENSION) FilterDeviceObject->DeviceExtension;
    RtlZeroMemory(pDeviceExtension, DEVICE_EXTENSION_SIZE);
    if (bNoRebootLayer) {
        // Don't attach the Filter device object to stack
        pDeviceExtension->TargetDeviceObject = LowerDeviceObject;
        pDevContext->IsDeviceEnumerated = TRUE;
        pDeviceExtension->PhysicalDeviceObject = 
            IoGetDeviceAttachmentBaseRef(pDeviceExtension->TargetDeviceObject);
        if (NULL != pDeviceExtension->PhysicalDeviceObject) {
            ObDereferenceObject(pDeviceExtension->PhysicalDeviceObject);
        }
    } else {
        //
        // Attaches the device object to the highest device object in the chain and
        // return the previously highest device object, which is passed to
        // IoCallDriver when pass IRPs down the device stack
        //
        pDeviceExtension->TargetDeviceObject =
                IoAttachDeviceToDeviceStack(FilterDeviceObject, LowerDeviceObject);
        pDeviceExtension->PhysicalDeviceObject = LowerDeviceObject;
    }
	pDevContext->IsPageFileDevicePossible = bIsPageFileDevicePossible;

    if (pDeviceExtension->TargetDeviceObject == NULL) {
        if (bNoRebootLayer) {
            ZwClose(FileHandle);
        }
        IoDeleteDevice(FilterDeviceObject);
        DereferenceDevContext(pDevContext);
        InVolDbgPrint(DL_ERROR, DM_DEV_ARRIVAL,
            ("InMageFltAddDevice: IoAttachDeviceToDeviceStack failed - Unable to attach %#p to target %#p\n",
            FilterDeviceObject, LowerDeviceObject));
        return STATUS_NO_SUCH_DEVICE;
    }

    ReferenceDevContext(pDevContext);
    pDeviceExtension->pDevContext = pDevContext;
    pDeviceExtension->DeviceObject = FilterDeviceObject;
    pDevContext->FilterDeviceObject = FilterDeviceObject;
    pDevContext->TargetDeviceObject = pDeviceExtension->TargetDeviceObject;
    InitializeListHead(&pDevContext->HoldIrpQueue);
    IoCsqInitialize(&pDevContext->CsqIrpQueue, InDskFltCsqInsertIrp, InDskFltCsqRemoveIrp,
                    InDskFltCsqPeekNextIrp, InDskFltCsqAcquireLock, InDskFltCsqReleaseLock, InDskFltCompleteCancelledIrp);

    KeInitializeSpinLock(&pDevContext->CsqSpinLock);
	
    InitializeListHead(&pDevContext->PageFileList);

	KeQuerySystemTime(&pDevContext->liDevContextCreationTS);
    InVolDbgPrint(DL_INFO, DM_DEV_ARRIVAL, 
        ("InMageFltAddDevice: Successfully added PhysicalDO = %#p, FilterDO = %#p, LowerDO = %#p, VC = %#p\n",
        LowerDeviceObject, FilterDeviceObject, pDeviceExtension->TargetDeviceObject, pDevContext));
    KeInitializeEvent(&pDeviceExtension->PagingPathCountEvent,
                      NotificationEvent,
                      TRUE);
    //Fix for Bug 28568
    INITIALIZE_PNP_STATE(pDeviceExtension);
    IoInitializeRemoveLock(&pDeviceExtension->RemoveLock,TAG_REMOVE_LOCK,0,0);
	
    // Remove hDevId from DataBlockList
    KeAcquireSpinLock(&DriverContext.DataBlockListLock, &OldIrql);
    PLIST_ENTRY Entry = DriverContext.OrphanedMappedDataBlockList.Flink;

    while (Entry != &DriverContext.OrphanedMappedDataBlockList) {
        PDATA_BLOCK DataBlock = (PDATA_BLOCK)Entry;

        if (DataBlock->hDevId == (HANDLE)pDevContext) {
            DataBlock->hDevId = 0;
        }
        Entry = Entry->Flink;
    }
    KeReleaseSpinLock(&DriverContext.DataBlockListLock, OldIrql);

    // Add Volume Context to DriverContext list.
    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    ReferenceDevContext(pDevContext);
//  Bug 25726. Adding New volume context at head of the list 
    InsertHeadList(&DriverContext.HeadForDevContext, &pDevContext->ListEntry);    
    DriverContext.ulNumDevs++;
    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);
    // As Filter device object is not attach dont set the flag
    if (!bNoRebootLayer) {
        // Populate the Power flags from lower object
        if (TEST_FLAG(pDeviceExtension->TargetDeviceObject->Flags, DO_POWER_PAGABLE)) {
            SET_FLAG(FilterDeviceObject->Flags, DO_POWER_PAGABLE);
        }
    }
    if (TEST_FLAG(pDeviceExtension->TargetDeviceObject->Flags, DO_POWER_INRUSH)) {
        SET_FLAG(FilterDeviceObject->Flags, DO_POWER_INRUSH);
    }

    if ((NULL != pDeviceExtension->PhysicalDeviceObject) &&
        (TEST_FLAG(pDeviceExtension->PhysicalDeviceObject->Flags, DO_POWER_INRUSH))) {
        SET_FLAG(pDevContext->ulFlags, DCF_PHY_DEVICE_INRUSH);
    }
    //
    // Clear the DO_DEVICE_INITIALIZING flag
    //

    FilterDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    if (bNoRebootLayer) {
        // Simulate start device
        InSetPNPStateToStart(FilterDeviceObject);
        KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
        SET_FLAG(pDevContext->ulFlags, DCF_FILTERING_STOPPED);
        pDevContext->DeviceHandle = FileHandle;
        KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
        InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_FILTERING_STOPPED, pDevContext, pDevContext->ulFlags);
    }

    // Free the reference of pDevContext
    DereferenceDevContext(pDevContext);

    return STATUS_SUCCESS;

} // end InMageFltAddDevice()

NTSTATUS
GenerateRegistryPathForDeviceConfig(
    IN PDEV_CONTEXT pDevContext)
/*++
Description : This routine generates registry path to get/set the per-device configuration

Arguments   : pDevContext - pointer to Device Context for a given disk device

Return      : STATUS_SUCCES            - on successful string append 
              STATUS_BUFFER_TOO_SMALL  - Destination buffer is too small
--*/
{
    NTSTATUS Status = STATUS_SUCCESS;
    RtlCopyUnicodeString(&pDevContext->DevParameters, &DriverContext.DriverParameters);
    Status = RtlAppendUnicodeToString(&pDevContext->DevParameters, L"\\");
    if (NT_SUCCESS(Status)) {
        Status = RtlAppendUnicodeToString(&pDevContext->DevParameters, &pDevContext->wDevID[0]);
        if (!NT_SUCCESS(Status)) {
            InVolDbgPrint(DL_ERROR, DM_DRIVER_PNP | DM_DEV_ARRIVAL,
                ("%s: Registry operation is Failed with Status 0x%x, DevParameters : %wS \n",
                __FUNCTION__, Status, &pDevContext->DevParameters.Buffer[0]));
        }
    }
    return Status;
}

NTSTATUS
GetRegistryPathForDeviceConfig(
    _Inout_   PUNICODE_STRING  pDevParameters, 
    IN PWCHAR   wDevID)
{
    NTSTATUS Status = STATUS_SUCCESS;
    InMageCopyUnicodeString(pDevParameters, &DriverContext.DriverParameters);

    Status = InMageAppendUnicodeString(pDevParameters, L"\\");
    if (!NT_SUCCESS(Status)) {
        goto cleanup;
    }

    Status = InMageAppendUnicodeString(pDevParameters, wDevID);

cleanup:
    if (!NT_SUCCESS(Status)) {
        RtlFreeUnicodeString(pDevParameters);
    }

    return Status;
}

VOID
InSetPNPStateToStart(
    IN PDEVICE_OBJECT DeviceObject
    )
{
    PDEVICE_EXTENSION       pDeviceExtension = NULL;
    pDeviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

    //Set the Filtering State, Don't Call Cleanup
    SET_NEW_PNP_STATE(pDeviceExtension, Started);

    InMageFltSyncFilterWithTarget(DeviceObject,
                                  pDeviceExtension->TargetDeviceObject);
}

NTSTATUS
IndskFltSetDeviceInformation (
    IN PDEVICE_OBJECT DeviceObject,
    IN  PIRP            Irp
    ) 
{
    NTSTATUS                    Status = STATUS_UNSUCCESSFUL;
    PDEVICE_EXTENSION           pDeviceExtension = NULL;
    PDEV_CONTEXT                pDevContext = NULL;
    KIRQL                       OldIrql;

    pDeviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

    pDevContext = pDeviceExtension->pDevContext;
    if (NULL == pDevContext) {
        InVolDbgPrint(DL_ERROR, DM_DRIVER_PNP | DM_DEV_ARRIVAL, 
             ("%s: pDevContext is NULL\n", 
              __FUNCTION__));
        InDskFltWriteEvent(INDSKFLT_ERROR_GENERIC_START_DEVICE_ERROR, (UINT32)STATUS_INVALID_DEVICE_STATE, SourceLocationSetDevInfo);
        goto Cleanup;
    }

    // Start device reissued
    if (TEST_FLAG(pDevContext->ulFlags, DCF_DEVID_OBTAINED)) {
        Status = STATUS_SUCCESS;
        goto Cleanup;
    }

    Status = SetDevInfoInDevContext(pDevContext);
    if (!NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_ERROR, DM_DRIVER_PNP | DM_DEV_ARRIVAL,
            ("%s: SetDevInfoInDevContext Failed with Status 0x%x\n",
            __FUNCTION__, Status));
        goto Cleanup;
    }

    // Ignore disk for whose disk Id is not obtained or,
    // it is a conflicting disk.
    if (!TEST_FLAG(pDevContext->ulFlags, DCF_DEVID_OBTAINED) || 
        TEST_FLAG(pDevContext->ulFlags, DCF_DISKID_CONFLICT)) {
        goto Cleanup;
    }

    // Get/Set the registry keys on retrieving valid device id
    Status = GenerateRegistryPathForDeviceConfig(pDevContext);
    if (!NT_SUCCESS(Status)) {
        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_PER_DEVICE_REGISTRY_ERROR, pDevContext, Status, SourceLocationSetDevInfoGenRegPathFail);
        goto Cleanup;
    }

    Status = LoadDeviceConfigFromRegistry(pDevContext);
    if (!NT_SUCCESS(Status)){
        InVolDbgPrint(DL_ERROR, DM_DRIVER_PNP | DM_DEV_ARRIVAL,
        ("%s: Loading per-device registry is failed with Status : 0x%x, Device : %x64\n",
            __FUNCTION__, Status, pDevContext));
        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_PER_DEVICE_REGISTRY_ERROR, pDevContext, Status, SourceLocationSetDevInfoLoadDevCtxtRegFail);
        goto Cleanup;
    }

    // No-Hydration WF is only valid
    //      Driver mode is reboot mode
    //      It is normal StartDevice path
    if ((etDriverMode::RebootMode == DriverContext.eDiskFilterMode) &&
        (NULL != Irp)) {

        // Check if Upgrade is detected.. Mark resync if needed
        VerifyOSUpgrade(pDevContext);

        Status = QueryNoHydration(pDevContext, Irp);
    }

    if (STATUS_SUCCESS == Status) {
        InDskFltSetSanPolicy(pDeviceExtension);
    }

    if (TEST_FLAG(pDevContext->ulFlags, DCF_FILTERING_STOPPED)) {
        goto Cleanup;
    }

Cleanup:
    // Mark filtering state as STOPPED under error conditions
    // DEVID is not found or registry call failures
    //
    if (TEST_FLAG(pDevContext->ulFlags, DCF_DISKID_CONFLICT) ||
        !TEST_FLAG(pDevContext->ulFlags, DCF_DEVID_OBTAINED) ||
        !NT_SUCCESS(Status)) {
            KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
            SET_FLAG(pDevContext->ulFlags, DCF_FILTERING_STOPPED);
            KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
            InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_FILTERING_STOPPED_STATUS, pDevContext, pDevContext->ulFlags, Status);
    }

    return Status;
}

NTSTATUS
IndskfltGetDiskIdentity(
    __in        PDEV_CONTEXT            pDevContext,
    __inout     PWSTR                   wBuffer,
    __in        USHORT                  usBufferLength,
    __inout     PARTITION_STYLE         *partitionStyle)

/*++

    Routine Description:

        Gets the disk identity. Identity will be disk signature in case of MBR disk
        and disk guid in case of gpt disk.

        This function calls IoReadDiskSignature to get disk signature. 
        If this call fails it falls back to older method of sending
        IOCTL_DISK_GET_DRIVE_GEOMETRY_EX to lower driver.

        Once it gets the disk identity it copies it into the buffer and returns.

        This function must be called at IRQL < DISPATCH_LEVEL

    Arguments:

        pDevContext -       Device context.
        Buffer -            Buffer to store the string
        BufferLength -      It is length. It is number of characters
        partitionStyle      Parition style of the disk.

    Return Value:

        NTSTATUS

    --*/

{
    ASSERT(NULL != pDevContext);
    ASSERT(NULL != wBuffer);
    ASSERT(0 != usBufferLength);

    DISK_GEOMETRY           geometry = { 0 };
    NTSTATUS                Status = STATUS_SUCCESS;
    IO_STATUS_BLOCK         IoStatus = { 0 };
    DISK_SIGNATURE          diskSignature = { 0 };
    PDISK_GEOMETRY_EX       pGeometryEx = NULL;
    PDISK_PARTITION_INFO    pDiskPartInfo = NULL;

    // Get the basic disk geometry
    // Bytes per sector is needed to get Disk Signature.
    Status = InMageSendDeviceControl(pDevContext->TargetDeviceObject,
        IOCTL_DISK_GET_DRIVE_GEOMETRY,
        NULL,
        0,
        &geometry,
        sizeof(geometry),
        &IoStatus);

    if (NT_SUCCESS(Status)) {
        Status = IoReadDiskSignature(pDevContext->TargetDeviceObject, geometry.BytesPerSector, &diskSignature);
        if (NT_SUCCESS(Status)) {
            goto Cleanup;
        }

        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_FUNCTION_FAILURE, pDevContext,
            L"IoReadDiskSignature",
            Status);
    }

    InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_FUNCTION_FAILURE, pDevContext,
        L"IOCTL_DISK_GET_DRIVE_GEOMETRY",
        Status);

    Status = IndskfltGetGeometryEx(pDevContext, &pGeometryEx);
    if (!NT_SUCCESS(Status)) {
        goto Cleanup;
    }

    pDiskPartInfo = DiskGeometryGetPartition(pGeometryEx);
    if (NULL == pDiskPartInfo) {
        Status = STATUS_INVALID_PARAMETER_1;
        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_FUNCTION_FAILURE, pDevContext,
            L"DiskGeometryGetPartition",
            Status);
        goto Cleanup;
    }

    diskSignature.PartitionStyle = pDiskPartInfo->PartitionStyle;
    diskSignature.Gpt.DiskId = pDiskPartInfo->Gpt.DiskId;
    diskSignature.Mbr.Signature = pDiskPartInfo->Mbr.Signature;

Cleanup:
    if (NT_SUCCESS(Status)) {
        Status = ExtractDevIDFromDiskSignature(&diskSignature, wBuffer, usBufferLength);
        if (NT_SUCCESS(Status)) {
            *partitionStyle = (PARTITION_STYLE) diskSignature.PartitionStyle;
        }
    }

    if (NULL != pGeometryEx) {
        ExFreePool(pGeometryEx);
    }

    return Status;
}

NTSTATUS
IndskfltGetGeometryEx(PDEV_CONTEXT            pDevContext, PDISK_GEOMETRY_EX* ppGeometryEx)
/*++

    Routine Description:

        Gets the disk geometry using IOCTL
        IOCTL_DISK_GET_DRIVE_GEOMETRY_EX to lower driver.

        It allocates buffer for geometry. Caller is supposed to free the allocated buffer.

        This function must be called at IRQL < DISPATCH_LEVEL

    Arguments:

        pDevContext -       Disk context.
        ppGeometryEx -      Disk geometry allocated by function to get geometry.

    Return Value:

        NTSTATUS

    --*/
{
    ASSERT(NULL != pDevContext);
    PDISK_GEOMETRY_EX               GeometryEx = NULL;
    ULONG                           Bytes = PAGE_SIZE;
    NTSTATUS                        Status = STATUS_SUCCESS;
    ULONG                           ulNumTries = 0;
    IO_STATUS_BLOCK                 IoStatus = { 0 };

    PDEVICE_OBJECT                  pDevObject = NULL;
    PFILE_OBJECT                    pFileObject = NULL;

    *ppGeometryEx = NULL;

    Status = InmageGetTopOfDeviceStack(pDevContext, &pFileObject, &pDevObject);
    if (!NT_SUCCESS(Status))
    {
        pDevObject = pDevContext->TargetDeviceObject;
    }

    for (ulNumTries = 0; ulNumTries < 32; ulNumTries++) {

        GeometryEx = (PDISK_GEOMETRY_EX) ExAllocatePool(PagedPool, Bytes);

        if (!GeometryEx) {

            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto Cleanup;
        }

        Status = InMageSendDeviceControl(pDevObject,
            IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
            NULL,
            0,
            GeometryEx,
            Bytes,
            &IoStatus);

        if (NT_SUCCESS(Status)) {
            break;
        }

        if (Status != STATUS_BUFFER_TOO_SMALL) {
            InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_FUNCTION_FAILURE, pDevContext,
                L"IOCTL_DISK_GET_DRIVE_GEOMETRY_EX",
                Status);

            goto Cleanup;
        }

        Bytes *= 2;
        ExFreePool(GeometryEx);
    }

    if (IoStatus.Information < sizeof(DISK_GEOMETRY_EX)) {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto Cleanup;
    }

Cleanup:
    if (!NT_SUCCESS(Status) && (NULL != GeometryEx)) {
        ExFreePool(GeometryEx);
        GeometryEx = NULL;
    }

    SafeObDereferenceObject(pFileObject);

    *ppGeometryEx = GeometryEx;
    return Status;
}


//Start Device is sent by IO manager on behalf of the underlying BUS that owns
//this device pdo. It comes from top of PNP stack. Unless we get PNP start device
//our device in stack has not useful 

/*
Routine     : This routine is called when a Pnp Start Irp is received.

Args        :  DeviceObject, which encapsulates the Device context
                       Irp, I/O request packet sent by IoManager

Return Value:  NTSTATUS
*/
NTSTATUS
InMageFltStartDevice(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN BOOLEAN bNoRebootLayer,
    IN PDRIVER_DISPATCH pDispatchEntryFunction
    )
{
    PDEVICE_EXTENSION       pDeviceExtension = NULL; 
    NTSTATUS                IrpStatus = STATUS_SUCCESS;
    NTSTATUS                Status = STATUS_SUCCESS;
    PDEV_CONTEXT            pDevContext = NULL;

    pDeviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    pDevContext = (PDEV_CONTEXT)pDeviceExtension->pDevContext;

    if (bNoRebootLayer) {        
        // DevId is learned in start filtering.
        // Rest of functionality does not require to make it syncronous
        // Making Irp Syncronous adds complexity for NoReboot
        Status = InCallDriver(pDeviceExtension->TargetDeviceObject, Irp, bNoRebootLayer, pDispatchEntryFunction);	
        if (NT_SUCCESS(Status)) {
            InSetPNPStateToStart(DeviceObject);
        }
        return Status;
    }

    IrpStatus = InMageFltForwardIrpSynchronous(DeviceObject, Irp);
    if (!NT_SUCCESS(IrpStatus)) {
        InVolDbgPrint(DL_ERROR, DM_DRIVER_PNP | DM_DEV_ARRIVAL, 
                     ("%s: InMageFltForwardIrpSynchronous Failed with Status 0x%x\n", 
                     __FUNCTION__, IrpStatus));
        InDskFltWriteEvent(INDSKFLT_ERROR_GENERIC_START_DEVICE_ERROR, IrpStatus, SourceLocationStartDevice);
        goto Cleanup;
    }

    Status = IndskFltSetDeviceInformation(DeviceObject, Irp);
    if (STATUS_PENDING == Status) {
        if (pDevContext) {
            InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_DISK_START_DEVICE_DONE, pDevContext, Status, pDevContext->ulFlags);
        }

        return Status;
    }

    Status = STATUS_SUCCESS;
    InSetPNPStateToStart(DeviceObject);

Cleanup:
    if (pDevContext) {
        InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_DISK_START_DEVICE_DONE, pDevContext, Status, pDevContext->ulFlags);
    }

    // Complete the Irp
    Irp->IoStatus.Status = IrpStatus;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return IrpStatus;
}

VOID
InMageFltSurpriseRemoveDevice(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    PDEVICE_EXTENSION DeviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    PDEV_CONTEXT pDevContext = DeviceExtension->pDevContext;
    KIRQL    OldIrql = 0;
#ifndef DBG
    UNREFERENCED_PARAMETER(Irp);
#endif
    if (NULL == pDevContext) {
        goto Cleanup;
    }

    InVolDbgPrint(DL_INFO, DM_SHUTDOWN,
                 ("InMageFltSurpriseRemoveDevice: DeviceObject %#p Irp %#p\n",
                 DeviceObject, Irp));

    InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_DISK_SURPRISE_REMOVAL_DETAILED, pDevContext, pDevContext->ulFlags);
    // Set required flags and cleanup in remove device/start device/Power
    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
    SET_FLAG(pDevContext->ulFlags, (DCF_SURPRISE_REMOVAL | DCF_REMOVE_CLEANUP_PENDING));
    SetDevCntFlag(pDevContext, DCF_EXPLICIT_NONWO_NODRAIN, 1);

    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
    InVolDbgPrint(DL_INFO, DM_CLUSTER, 
                  ("InMageFltSurpriseRemoveDevice: DevContext is %p, DevNum=%S DevBitmap is %p VolumeFlags is 0x%x\n",
                  pDevContext, pDevContext->wDevNum, pDevContext->pDevBitmap , pDevContext->ulFlags));

    // Release all writes for this disk
    InDskFltIoBarrierCleanup(pDevContext);

Cleanup:
    return;
}

/*
Description: This routine is called when the device is to be removed.It will de-register itself from WMI first, detach itself from the
                  stack before deleting itself.

Args			:    DeviceObject - a pointer to the device object
                     Irp - a pointer to the irp

Return Value:  NTSTATUS
*/
NTSTATUS
InMageFltRemoveDevice(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN BOOLEAN bNoRebootLayer,
    IN PDRIVER_DISPATCH pDispatchEntryFunction
    )
{
    NTSTATUS            IrpStatus = STATUS_SUCCESS;
    PDEV_CONTEXT        pDevContext = NULL;
    PDEVICE_OBJECT      TargetDevObj = NULL;
    PDEVICE_EXTENSION   pDeviceExtension = NULL;
    KIRQL               OldIrql = 0;
    KIRQL               OldIrqlDevCnt = 0;
    bool                bLock = 0;

    pDeviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

    InVolDbgPrint(DL_FUNC_TRACE, DM_SHUTDOWN,
                 ("InMageFltRemoveDevice: DeviceObject %#p Irp %#p\n",
                 DeviceObject, Irp));

    pDevContext = pDeviceExtension->pDevContext;

    if (NULL == pDevContext) {
        goto Cleanup;
    }

    InVolDbgPrint(DL_INFO, DM_SHUTDOWN,
                 ("InMageFltRemoveDevice: Removing Device - pDevContext = %#p Volume = %S(%S)\n",
                 pDevContext, pDevContext->wDevID, pDevContext->wDevNum));
    InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_DISK_REMOVAL_DETAILED, pDevContext, pDevContext->ulFlags);

    // this Debugprint has to run outside the spinlock as the Unicode string table is paged
    InVolDbgPrint(DL_INFO, DM_SHUTDOWN,
                 ("InMageFltRemoveDevice: Device = %S ulTotalChangesPending = %i\n", 
                 pDevContext->wDevNum, 
                 pDevContext->ChangeList.ulTotalChangesPending));
    // Set the required flags specially to handle syncronization with barrier
    KeAcquireSpinLock(&pDevContext->Lock, &OldIrqlDevCnt);
    bLock = 1;
    ReferenceDevContext(pDevContext);
    SetDevCntFlag(pDevContext, DCF_EXPLICIT_NONWO_NODRAIN, bLock);
    SET_FLAG(pDevContext->ulFlags, DCF_REMOVE_CLEANUP_PENDING);
    CxSessionStopFilteringOrRemoveDevice(pDevContext);
    KeReleaseSpinLock(&pDevContext->Lock, OldIrqlDevCnt);

    InDskFltIoBarrierCleanup(pDevContext);
    IndskFltCompleteTagNotifyIfDone(pDevContext);

    bLock = 0; 

    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    if (DriverContext.isBootDeviceStarted) {
        KeAcquireSpinLockAtDpcLevel(&pDevContext->Lock);
        if (pDevContext->isBootDisk) {
            DriverContext.isBootDeviceStarted = false;
        }
        KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
    }
    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

Cleanup:
	if (bLock) {
        KeReleaseSpinLock(&pDevContext->Lock, OldIrqlDevCnt);
        bLock = 0;
    }
    InSkipCurrentIrpStackLocation(Irp, bNoRebootLayer);
    IrpStatus = InCallDriver(pDeviceExtension->TargetDeviceObject, 
                             Irp, 
                             bNoRebootLayer, 
                             pDispatchEntryFunction);

	//Fix for Bug 28568 and similar.
    IoReleaseRemoveLockAndWait(&pDeviceExtension->RemoveLock,Irp);

    if (pDevContext) {
        if (!bLock) {
             KeAcquireSpinLock(&pDevContext->Lock, &OldIrqlDevCnt);
             bLock = 1;
        }
        // Post RemoveLock all the IO should fail ideally 
        SetDevCntFlag(pDevContext, DCF_LAST_IO, bLock);

        // As Bitmap is on another device 
        // Flush and close of bitmap is done through service thread
        pDevContext->eDSDBCdeviceState = ecDSDBCdeviceOffline;
        pDevContext->FilterDeviceObject = NULL;
        if (bLock) {
            KeReleaseSpinLock(&pDevContext->Lock, OldIrqlDevCnt);
            bLock = 0;
        }
        // Dereference as Device object and Device context are delinked
        DereferenceDevContext(pDeviceExtension->pDevContext);
        pDeviceExtension->pDevContext = NULL;

        // Syncronize with service thread
        KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
        // Trigger activity to start writing existing data to bit map file.
        KeSetEvent(&DriverContext.ServiceThreadWakeEvent, IO_DISK_INCREMENT, FALSE);
        KeReleaseSpinLock(&DriverContext.Lock, OldIrql);
        // Reference was taken at start of function
        DereferenceDevContext(pDevContext);
    }

    // Unwind the stack
    TargetDevObj = pDeviceExtension->TargetDeviceObject;
    pDeviceExtension->TargetDeviceObject = NULL;
    if (!bNoRebootLayer) {
        // Filter Device is not in stack
        IoDetachDevice(TargetDevObj);
    }
    IoDeleteDevice(DeviceObject);

    //
    // Complete the Irp
    //

    return IrpStatus;
}

NTSTATUS
InmageDevUsageNotification(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN BOOLEAN bNoRebootLayer,
    IN PDRIVER_DISPATCH pDispatchEntryFunction
    )
/*++

Routine Description:

    Clear the Power Flags in case Pagefile/Hibernate/Crash file is set
    
    Learn Pagefile Objects and maintain them in Device Context

    This function must be called at IRQL < APC_LEVEL

Arguments:

    pDevObject:- Target Object
    Irp:- Information about the request

Return Value:

    Irp can be failed in case not handled

    InDskflt does not fail except AcquireRemoveLock

--*/
{
    NTSTATUS                IrpStatus = STATUS_UNSUCCESSFUL;
    PDEV_CONTEXT            pDevContext = NULL;
    PDEVICE_EXTENSION       pDeviceExtension = NULL;
    PIO_STACK_LOCATION      currentIrpStack = NULL;
    PIO_STACK_LOCATION      topIrpStack = NULL;
    UNICODE_STRING          pagefileName;    
    BOOLEAN                 bInPath;
    DEVICE_USAGE_NOTIFICATION_TYPE type;

    InVolDbgPrint(DL_INFO, DM_DRIVER_PNP, 
                 ("InMageFltDispatchPnp: Minor Function DEVICE_USAGE_NOTIFICATION\n"));

    currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    type = currentIrpStack->Parameters.UsageNotification.Type;
    bInPath = currentIrpStack->Parameters.UsageNotification.InPath;
    pDeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    pDevContext = pDeviceExtension->pDevContext;

    InVolDbgPrint(DL_INFO, DM_DRIVER_PNP | DM_SHUTDOWN,
        ("%s : pDevContext = %#p bInPath 0x%x Type 0x%x DevObj %#p\n",
                __FUNCTION__, pDeviceExtension->pDevContext, bInPath, type, DeviceObject));

    if (bNoRebootLayer) {
        // Adding pagefile post NoReboot Driver installation is rare scenario
        // By default NoReboot driver unset the DO_POWER_PAGABLE as not in stack
        // Rest of functionality does not require to make it syncronous
        // Making Irp Syncronous adds complexity for NoReboot
        IrpStatus = InCallDriver(pDeviceExtension->TargetDeviceObject, 
                                 Irp, 
                                 bNoRebootLayer, 
                                 pDispatchEntryFunction);
        if (NT_SUCCESS(IrpStatus) &&
            (bInPath) &&
            (DeviceUsageTypePaging == type) &&
            (pDevContext)) {
            pDevContext->IsPageFileDevicePossible = TRUE;
        }
        IoReleaseRemoveLock(&pDeviceExtension->RemoveLock,Irp);
        return IrpStatus;
    }

    // Pass the Ire Synchronously as documented
    Irp->IoStatus.Status = STATUS_SUCCESS;
    IrpStatus = InMageFltForwardIrpSynchronous(DeviceObject, Irp);
    if (!NT_SUCCESS(IrpStatus)){
        goto Cleanup;
    }

    // Act only for Page/Hibernation/Crash file
    if ((DeviceUsageTypePaging != type) &&
        (DeviceUsageTypeDumpFile != type) &&
        (DeviceUsageTypeHibernation != type)) {
        goto Cleanup;
    }

    KeWaitForSingleObject(&pDeviceExtension->PagingPathCountEvent, 
                          Executive, KernelMode, 
                          FALSE, NULL);
    // Clear Power flag in case Pagefile/Hibernate/Crash file is set
    // Win2k8R2 onwards driver can set/unset Power/Inrush flag independently
    // Setting to Power Pagable makes entire stack Pagable
    // As Indskflt Code and data segment is NonPaged, It is okay to unset
    // Power flag is never set again
    // PagePool Memory is taken care by special flag wherever required
    
    if (bInPath) {
        CLEAR_FLAG(DeviceObject->Flags, DO_POWER_PAGABLE);
    }

    KeSetEvent(&pDeviceExtension->PagingPathCountEvent,
               IO_NO_INCREMENT, FALSE);

    if ((DeviceUsageTypeDumpFile == type) ||
        (DeviceUsageTypeHibernation == type)) {
        goto Cleanup;
    }

    if (NULL == pDevContext) {
        goto Cleanup;
    }

    topIrpStack = (PIO_STACK_LOCATION)((PCHAR)Irp + sizeof(IRP)) + (Irp->StackCount - 1);
    if ((NULL == topIrpStack) ||
        (NULL == topIrpStack->FileObject)) {
        goto Cleanup;
    }

    // Learn Pagefile object and set/reset according to configuration
    if (!bInPath) {
        RemovePageFileObject(pDevContext, topIrpStack->FileObject);
    } else {
        pDevContext->IsPageFileDevicePossible = TRUE;
        RtlInitUnicodeString(&pagefileName, L"\\pagefile.sys");
        if (topIrpStack->FileObject->FileName.Length  <= 0) {
            goto Cleanup;
        }
        if (0 != RtlCompareUnicodeString(&(topIrpStack->FileObject->FileName), &pagefileName, TRUE)) {
            goto Cleanup;
        }
        AddPageFileObject(pDevContext, topIrpStack->FileObject);
    }
Cleanup:
    IoReleaseRemoveLock(&pDeviceExtension->RemoveLock,Irp);
    Irp->IoStatus.Status = IrpStatus;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return IrpStatus;
}


NTSTATUS
InDskFltDispatchPnp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{

    NTSTATUS Status = STATUS_UNSUCCESSFUL;

    // Legacy code was called in each of following minor function
    // It was getting called after acquiring remove lock for wrong device
    // Moved at appropriate place, Mostly not required
    if ((DeviceObject == DriverContext.ControlDevice) ||
        (DeviceObject == DriverContext.PostShutdownDO)) {
        Status = STATUS_NOT_IMPLEMENTED;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        goto Cleanup;
    }

    Status = InMageFltDispatchPnp(DeviceObject, Irp);

Cleanup:
    return Status;
}


/*
Description	:  This dispatch routine handles various PNP notifications. As per the Minor Function, involft driver either captures few details or sets the 
                       respective flag so that driver can adopt to the device arrival/surprise removals/remove events.

Args				:  DeviceObject, which encapsulates the volume context
                       Irp, I/O request packet sent by IoManager

Return Value	:   NTSTATUS
*/

NTSTATUS
InMageFltDispatchPnp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN BOOLEAN bNoRebootLayer,
    IN PDRIVER_DISPATCH pDispatchEntryFunction
    )
{
    PIO_STACK_LOCATION  currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS            Status;
    PDEVICE_EXTENSION   pDeviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

    InVolDbgPrint(DL_FUNC_TRACE, DM_DRIVER_PNP | DM_DEV_ARRIVAL | DM_SHUTDOWN ,
        ("InMageFltDispatchPnp: Device = %#p  Irp = %#p  MinorFunction = %#x\n", 
        DeviceObject, Irp, currentIrpStack->MinorFunction));

	//Fix for Bug 28568 and similar.
	//It's a pretty good idea to have remlocks especially when we have
	//non-file-object based IO's from above us, originating outside IO manager
	//or non-state-changing PNP calls or power IRP's. This is of real importance
	//while processing REMOVE_DEVICE.
	Status = IoAcquireRemoveLock(&pDeviceExtension->RemoveLock,Irp);

	if(!NT_SUCCESS(Status))
	{
		InVolDbgPrint(DL_INFO, DM_DRIVER_PNP,
                ("InMageFltDispatchPnp: IoAcquireRemoveLock Dishonoured with Status %lx\n",Status));
		Irp->IoStatus.Status = Status;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return Status;
	}

    if (NULL == pDeviceExtension->TargetDeviceObject) {
        Status = STATUS_DEVICE_NOT_CONNECTED;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        goto Cleanup;
    }

    switch(currentIrpStack->MinorFunction) {

        case IRP_MN_START_DEVICE:
            // Get the respective disk number to faciliate the disk-volume mapping
           
            InVolDbgPrint(DL_INFO, DM_DRIVER_PNP | DM_DEV_ARRIVAL,
                ("InMageFltDispatchPnp: pDevContext = %#p, Minor Function - START_DEVICE\n",
                pDeviceExtension->pDevContext));
            Status = InMageFltStartDevice(DeviceObject,
                                          Irp, 
                                          bNoRebootLayer, 
                                          pDispatchEntryFunction);

            break;

        case IRP_MN_QUERY_STOP_DEVICE:
            InVolDbgPrint(DL_INFO, DM_DRIVER_PNP | DM_SHUTDOWN,
                ("InMageFltDispatchPnp: pDevContext = %#p - Query shutdown, flushing diry blocks\n", pDeviceExtension->pDevContext));

            // New state should be set for Query IRP before passing down
            SET_NEW_PNP_STATE(pDeviceExtension, StopPending);

            InSkipCurrentIrpStackLocation(Irp, bNoRebootLayer);
            Status = InCallDriver(pDeviceExtension->TargetDeviceObject,
                                  Irp, 
                                  bNoRebootLayer, 
                                  pDispatchEntryFunction);
            break;

        case IRP_MN_SURPRISE_REMOVAL:

            //Note: Once this call has come to us, as per MS documentation we may/may not
  	    	//get IRP_MN_REMOVE_DEVICE call. If any handle remains open to
		    //the device object then PNP does not send the IRP_MN_REMOVE_DEVICE
		    //thus onus lies on us to take care of any corner cases.
            InVolDbgPrint(DL_INFO, DM_DRIVER_PNP | DM_SHUTDOWN,
                ("InMageFltDispatchPnp: pDevContext = %#p SURPRISE_REMOVAL\n",
                pDeviceExtension->pDevContext));
			SET_NEW_PNP_STATE(pDeviceExtension, SurpriseRemovePending);
			Irp->IoStatus.Status = STATUS_SUCCESS;//harmless, only for w2k, seems like there was a bug in pcmcia.sys
            InMageFltSurpriseRemoveDevice(DeviceObject, Irp);
            InSkipCurrentIrpStackLocation(Irp, bNoRebootLayer);
            
            Status = InCallDriver(pDeviceExtension->TargetDeviceObject,
                                  Irp, 
                                  bNoRebootLayer, 
                                  pDispatchEntryFunction);
            break;
			
		case IRP_MN_CANCEL_STOP_DEVICE:
            InSkipCurrentIrpStackLocation(Irp, bNoRebootLayer);
            Status = InCallDriver(pDeviceExtension->TargetDeviceObject, Irp, bNoRebootLayer, pDispatchEntryFunction);
            // Query IRP may not have received as upper layer can fail it
            // New state should be set post completion 
            // Ideally it should be set in completion routine or make sync. 
            // Currently PNP stats is not used for any fucntionality. So it is skipped
            if (StopPending == pDeviceExtension->DevicePnPState) { 
                RESTORE_PREVIOUS_PNP_STATE(pDeviceExtension);
            }
			break;
			
		case IRP_MN_STOP_DEVICE:

            // Set in Dispatch
		    SET_NEW_PNP_STATE(pDeviceExtension, Stopped);

            InSkipCurrentIrpStackLocation(Irp, bNoRebootLayer);
            
            Status = InCallDriver(pDeviceExtension->TargetDeviceObject,
                                  Irp, 
                                  bNoRebootLayer, 
                                  pDispatchEntryFunction);
			break;
			
		case IRP_MN_QUERY_REMOVE_DEVICE:
		
            // New state should be set for Query IRP before passing down
		    SET_NEW_PNP_STATE(pDeviceExtension, RemovePending);
            InSkipCurrentIrpStackLocation(Irp, bNoRebootLayer);
            Status = InCallDriver(pDeviceExtension->TargetDeviceObject,
                                  Irp, 
                                  bNoRebootLayer, 
                                  pDispatchEntryFunction);
            break;

        case IRP_MN_REMOVE_DEVICE:
            
            // PnP Manager will not send MN_REMOVE_DEVICE to a devnode till
            // a file object exists on it.
            InVolDbgPrint(DL_INFO, DM_DRIVER_PNP | DM_SHUTDOWN,
                ("InMageFltDispatchPnp: pDevContext = %#p Minor Function - REMOVE_DEVICE\n",
                pDeviceExtension->pDevContext));

            // Set in Dispatch
            SET_NEW_PNP_STATE(pDeviceExtension,Deleted);
			Irp->IoStatus.Status = STATUS_SUCCESS;//only for w2k, seems like there was a bug in pcmcia.sys
            Status = InMageFltRemoveDevice(DeviceObject, 
                                           Irp, 
                                           bNoRebootLayer, 
                                           pDispatchEntryFunction);
            return Status;
			
		case IRP_MN_CANCEL_REMOVE_DEVICE:
            InSkipCurrentIrpStackLocation(Irp, bNoRebootLayer);
            Status = InCallDriver(pDeviceExtension->TargetDeviceObject,
                                  Irp, 
                                  bNoRebootLayer, 
                                  pDispatchEntryFunction);            
            // Query IRP may not have received as upper layer can fail it
            // New state should be set post completion
            // Ideally it should be set in completion routine or make sync. 
            // Currently PNP stats is not used for any fucntionality. So it is skipped
			if(RemovePending == pDeviceExtension->DevicePnPState)
            {
                RESTORE_PREVIOUS_PNP_STATE(pDeviceExtension);
            }
			break;
        case IRP_MN_DEVICE_USAGE_NOTIFICATION:

            Status = InmageDevUsageNotification(DeviceObject, 
                                                Irp, 
                                                bNoRebootLayer, 
                                                pDispatchEntryFunction);
            return Status;

        default:
            InVolDbgPrint(DL_INFO, DM_DRIVER_PNP, ("InMageFltDispatchPnp: Forwarding irp\n"));

            InSkipCurrentIrpStackLocation(Irp, bNoRebootLayer);
            Status = InCallDriver(pDeviceExtension->TargetDeviceObject,
                                  Irp, 
                                  bNoRebootLayer, 
                                  pDispatchEntryFunction);

    }
Cleanup:
	IoReleaseRemoveLock(&pDeviceExtension->RemoveLock,Irp);
    return Status;

}


NTSTATUS
InMageFltSaveAllChanges(PDEV_CONTEXT   pDevContext, BOOLEAN bWaitRequired, PLARGE_INTEGER pliTimeOut, bool bOpenBitmap)
{
    LIST_ENTRY          ListHead;
    PWORK_QUEUE_ENTRY   pWorkItem = NULL;
    KIRQL               OldIrql;

    if (NULL == pDevContext)
        return STATUS_SUCCESS;

    if (pDevContext->ulFlags & DCF_BITMAP_WRITE_DISABLED)
        return STATUS_SUCCESS;

    if (PASSIVE_LEVEL != KeGetCurrentIrql()) {
        return STATUS_UNSUCCESSFUL;
    }

    InitializeListHead(&ListHead);

    if (0 == (pDevContext->ulFlags & DCF_BITMAP_WRITE_DISABLED)) {
        KeWaitForSingleObject(&pDevContext->SyncEvent, Executive, KernelMode, FALSE, NULL);
        KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);

        // Device is being shutdown soon, write all pending dirty blocks to bit map.
        OrphanMappedDataBlocksAndMoveCommitPendingDirtyBlockToMainQueue(pDevContext);
        // OpenBitmap incase requested irrespective of changes pending
        if ((!IsListEmpty(&pDevContext->ChangeList.Head)) ||
            (bOpenBitmap)){
            if (!AddVCWorkItemToList(ecWorkItemBitmapWrite, pDevContext, 0, TRUE, &ListHead)) {
            	//Mark for resync if failed to add work item
                QueueWorkerRoutineForSetDevOutOfSync(pDevContext, MSG_INDSKFLT_ERROR_NO_GENERIC_NPAGED_POOL_Message, STATUS_NO_MEMORY, true);
            }
        }

        KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
        KeSetEvent(&pDevContext->SyncEvent, IO_NO_INCREMENT, FALSE);
    }

    while (!IsListEmpty(&ListHead)) {
        pWorkItem = (PWORK_QUEUE_ENTRY)RemoveHeadList(&ListHead);
        ProcessDevContextWorkItems(pWorkItem);
        DereferenceWorkQueueEntry(pWorkItem);
    }

    NTSTATUS Status = STATUS_SUCCESS;

    if (bWaitRequired)
        Status = WaitForAllWritesToComplete(pDevContext->pDevBitmap, pliTimeOut);

    return Status;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
static
void
MarkGlobalTooManyLastChanceWritesHitFlag()
{
    NTSTATUS    Status;
    bool        bFirstTooManyLastChanceWritesHit;
    KIRQL       OldIrql;

    PAGED_CODE();

    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);

    bFirstTooManyLastChanceWritesHit = !DriverContext.bTooManyLastChanceWritesHit;
    DriverContext.bTooManyLastChanceWritesHit = true;

    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

    if (!bFirstTooManyLastChanceWritesHit) {
        Status = STATUS_SUCCESS;
        goto Cleanup;
    }

    InDskFltWriteEvent(INDSKFLT_INFO_GLOBAL_TOO_MANY_LCW_FLAG_SET);

    // Given an error in setting this registry is being ignored, not
    // flushing the registry key. Since, if the machine crashes before the
    // value is flushed, it's just like another error in setting registry.
    Status = SetDWORDInParameters(TOO_MANY_LAST_CHANCE_WRITES_HIT, (ULONG)true);

    if (NT_SUCCESS(Status)) {
        InDskFltWriteEvent(INDSKFLT_INFO_GLOBAL_TOO_MANY_LCW_FLAG_REG_PERSISTED, Status);
    } else {
        InDskFltWriteEvent(INDSKFLT_ERROR_GLOBAL_TOO_MANY_LCW_FLAG_REG_SET, Status);

        // Best effort. Ignore failure to write to the registry.
        Status = STATUS_SUCCESS;
    }

Cleanup:
    return;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Ret_maybenull_
_Must_inspect_result_
PDEV_BITMAP
OpenBitmapFile(
    _In_ _Notnull_  PDEV_CONTEXT    pDevContext,
    _Outref_        NTSTATUS        &Status
    )
{
    PDEV_BITMAP             pDevBitmap = NULL;
    NTSTATUS                InMageOpenStatus = STATUS_SUCCESS;
    ULONG                   ulBitmapGranularity;
    BOOLEAN                 bDevInSync = TRUE;
    BitmapAPI               *pBitmapAPI = NULL;
    tDevLogContext          DevLogContext = {};
#ifdef _WIN64
    // Flag to indicate whether to allocate DATA Pool.
    BOOLEAN                 bAllocMemory = FALSE;
#endif

    PAGED_CODE();

    InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, ("OpenBitmapFile: Called on device %p\n", pDevContext));

    //DTAP:- Why there is a check for FilterDeviceObject
    if ((NULL == pDevContext) || (NULL == pDevContext->FilterDeviceObject)) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("OpenBitmapFile: Invalid parameter %p\n", pDevContext));
        goto Cleanup_And_Return_Error;
    }

    C_ASSERT(sizeof(DevLogContext.wDevID) == sizeof(pDevContext->wDevID));
    RtlCopyMemory(DevLogContext.wDevID, pDevContext->wDevID, sizeof(pDevContext->wDevID));
    DevLogContext.ulDevNum = pDevContext->ulDevNum;

    if (!IS_DEVINFO_OBTAINED(pDevContext->ulFlags)) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("OpenBitmapFile: DevContext %p does not have GUID obtained yet\n", pDevContext));
        InDskFltWriteEvent(INDSKFLT_INFO_OPEN_BITMAP_CALLED_PRIOR_TO_OBTAINING_DEVICEID);
        goto Cleanup_And_Return_Error;
    }

    if (TEST_FLAG(pDevContext->ulFlags, DONT_OPEN_BITMAP)) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("OpenBitmapFile: DevContext %p DONT_OPEN_BITMAP is set %lu\n", pDevContext, pDevContext->ulFlags));
        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_DONT_OPEN_BITMAP_IS_SET, pDevContext, pDevContext->ulFlags);
        goto Cleanup_And_Return_Error;
    }

    pDevBitmap = AllocateDevBitmap();
    if (NULL == pDevBitmap) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("OpenBitmapFile: AllocteDevBitmap failed for %S\n", pDevContext->wDevNum));
        goto Cleanup_And_Return_Error;
    }

    InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, ("OpenBitmapFile: Size of device %S is %#I64x\n",
                    pDevContext->wDevNum, pDevContext->llDevSize));

    pDevContext->BitmapFileName.Length = 0;
    Status = FillBitmapFilenameInDevContext(pDevContext);
    if (!NT_SUCCESS(Status)) {
        InVolDbgPrint( DL_ERROR, DM_BITMAP_OPEN, 
            ("OpenBitmapFile: Failed to Get bitmap file name. Status = %#x\n", Status));        
        goto Cleanup_And_Return_Error;
    }

    Status = GetDevBitmapGranularity(pDevContext, &ulBitmapGranularity);
    if (!NT_SUCCESS(Status) || (0 == ulBitmapGranularity)) {
        InVolDbgPrint( DL_ERROR, DM_BITMAP_OPEN, ("OpenBitmapFile: Failed in retreiving %S, Status = %#x\n", 
                        DEVICE_BITMAP_GRANULARITY, Status));
        goto Cleanup_And_Return_Error;
    }

    pDevBitmap->SegmentCacheLimit = MAX_BITMAP_SEGMENT_BUFFERS;

    pBitmapAPI = new BitmapAPI(&DevLogContext);
    if (NULL == pBitmapAPI) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("OpenBitmapFile: Failed to allocate BitmapAPI\n"));
        goto Cleanup_And_Return_Error;
    }
    InVolDbgPrint(DL_INFO, DM_MEM_TRACE, ("OpenBitmapFile: Allocation %#p\n", pBitmapAPI));
    pDevBitmap->pBitmapAPI = pBitmapAPI;

    KeWaitForMutexObject(&pDevContext->BitmapOpenCloseMutex, Executive, KernelMode, FALSE, NULL);
    Status = pBitmapAPI->Open(
        &pDevContext->BitmapFileName,
        ulBitmapGranularity,
        pDevContext->ulBitmapOffset,
        pDevContext->llDevSize,
#if DBG
        pDevContext->chDevNum,
#endif
        pDevBitmap->SegmentCacheLimit,
        ((0 == (pDevContext->ulFlags & DCF_CLEAR_BITMAP_ON_OPEN)) ? false : true),
        &InMageOpenStatus,
        (tagLogRecoveryState *)&pDevContext->BitmapRecoveryState,
        pDevContext->ulMaxBitmapHdrWriteGroups);

    if (STATUS_SUCCESS == Status) {
#ifdef _WIN64
        //Set when volume is non-cluster and filtering is started on any volume
        bAllocMemory = 1;
#endif
    }
    KeReleaseMutex(&pDevContext->BitmapOpenCloseMutex, FALSE);
#ifdef _WIN64
    // Data Pool get allocated when bitmap file is open first time and filtering is on.
    if (bAllocMemory) {
        SetDataPool(1, DriverContext.ullDataPoolSize);
    }
#endif
    if (STATUS_SUCCESS == Status) {
        if (pDevContext->lNumBitmapOpenErrors) {
            LogBitmapOpenSuccessEvent(pDevContext);
        }
    } else {
        // Opening bitmap file has failed.
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("OpenBitmapFile: bitmap file %wZ open for device %S failed, Status(%#x), InMageStatus(%#x)\n", 
                            &pDevContext->BitmapFileName, pDevContext->wDevNum, Status, InMageOpenStatus));

        goto Cleanup_And_Return_Error;
    }

    Status = pBitmapAPI->IsDevInSync(&bDevInSync, &InMageOpenStatus);
    if (Status != STATUS_SUCCESS) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("OpenBitmapFile: bitmap file %wZ open for device %S failed to get sync status, Status(%#x)\n", 
                            &pDevContext->BitmapFileName, pDevContext->wDevNum, Status));
        goto Cleanup_And_Return_Error;
    }

    if (FALSE == bDevInSync) {
        if (MSG_INDSKFLT_ERROR_TOO_MANY_LAST_CHANCE_Message == InMageOpenStatus) {
            MarkGlobalTooManyLastChanceWritesHitFlag();
        }

        SetDevOutOfSync(pDevContext, InMageOpenStatus, InMageOpenStatus, false);
        // Cleanup data files from previous boot.
        CDataFile::DeleteDataFilesInDirectory(&pDevContext->DataLogDirectory);
    }

    // Unset ignore bitmap creation .
    pDevContext->ulFlags &= ~DCF_IGNORE_BITMAP_CREATION;
    pDevContext->ulFlags &= ~DCF_CLEAR_BITMAP_ON_OPEN;

    pDevBitmap->eVBitmapState = ecVBitmapStateOpened;
    pDevBitmap->pDevContext = ReferenceDevContext(pDevContext);
  
	RtlCopyMemory(pDevBitmap->DevID, pDevContext->wDevID, GUID_SIZE_IN_BYTES + sizeof(WCHAR));
#if DBG
    RtlCopyMemory(pDevBitmap->wDevNum, pDevContext->wDevNum, DEVICE_NUM_LENGTH * sizeof(WCHAR));
#endif
    return pDevBitmap;

Cleanup_And_Return_Error:

    if (NULL != pDevBitmap) {
        DereferenceDevBitmap(pDevBitmap);
        pDevBitmap = NULL;
    }

    return pDevBitmap;
}

/* Changed signature of CloseBitmapFile to get pDevContext. 
   As BitmapPersistentValue object was constructed with DevContext 
*/
VOID
CloseBitmapFile(PDEV_BITMAP pDevBitmap, bool bClearBitmap,
        PDEV_CONTEXT pDevContext,
        tagLogRecoveryState recoveryState,
        bool bDeleteBitmap,
        ULONG ResyncErrorCode,
        LONGLONG ResyncTimeStamp)

{
    BOOLEAN         bWaitForNotification = FALSE;
    BOOLEAN         bSetBitsWorkItemListIsEmpty;
    NTSTATUS        Status;
    KIRQL           OldIrql;
    class BitmapAPI *pBitmapAPI = NULL;

    UNREFERENCED_PARAMETER(pDevContext);

    PAGED_CODE();

    InVolDbgPrint(DL_INFO, DM_BITMAP_CLOSE,
        ("CloseBitmapFile: Called for device %S(%S)\n", pDevBitmap->DevID, pDevBitmap->wDevNum));

    if (NULL != pDevBitmap) {
        ASSERT(FALSE == bWaitForNotification);
        do {
            if (bWaitForNotification) {
                InVolDbgPrint(DL_INFO, DM_BITMAP_CLOSE,
                    ("CloseBitmapFile: Waiting on SetBitsWorkItemListEmtpyNotification for device %S(%S)\n", 
                    pDevBitmap->DevID, pDevBitmap->wDevNum));
                Status = KeWaitForSingleObject(&pDevBitmap->SetBitsWorkItemListEmtpyNotification, Executive, KernelMode, FALSE, NULL);
                bWaitForNotification = FALSE;
            }

            ExAcquireFastMutex(&pDevBitmap->Mutex);
            
            KeAcquireSpinLock(&pDevBitmap->Lock, &OldIrql);
            bSetBitsWorkItemListIsEmpty = IsListEmpty(&pDevBitmap->SetBitsWorkItemList);
            KeReleaseSpinLock(&pDevBitmap->Lock, OldIrql);
            
            if (bSetBitsWorkItemListIsEmpty) {
                // Set the state of bitmap to close.
                InVolDbgPrint(DL_INFO, DM_BITMAP_CLOSE,
                    ("CloseBitmapFile: Setting bitmap state to closed for device %S(%S)\n", pDevBitmap->DevID, pDevBitmap->wDevNum));
                pDevBitmap->eVBitmapState = ecVBitmapStateClosed;

                if (NULL != pDevBitmap->pDevContext) {
                    DereferenceDevContext(pDevBitmap->pDevContext);
                    pDevBitmap->pDevContext = NULL;
                }

                if (NULL != pDevBitmap->pBitmapAPI) {
                    if (bClearBitmap) {
                        pDevBitmap->pBitmapAPI->ClearAllBits();
                    }
                    pBitmapAPI = pDevBitmap->pBitmapAPI;

                    pDevBitmap->pBitmapAPI = NULL;
                }
            } else {
                InVolDbgPrint(DL_INFO, DM_BITMAP_CLOSE,
                    ("CloseBitmapFile: SetBitsWorkItemList is not Emtpy for device %S(%S)\n", pDevBitmap->DevID, pDevBitmap->wDevNum));
                KeClearEvent(&pDevBitmap->SetBitsWorkItemListEmtpyNotification);
                pDevBitmap->ulFlags |= DEV_BITMAP_FLAGS_WAITING_FOR_SETBITS_WORKITEM_LIST_EMPTY_NOTIFICATION;
                bWaitForNotification = TRUE;
            }
            ExReleaseFastMutex(&pDevBitmap->Mutex);
            if (pBitmapAPI) {
                pBitmapAPI->Close(recoveryState, bDeleteBitmap, NULL, ResyncErrorCode, ResyncTimeStamp);
                delete pBitmapAPI;
                pBitmapAPI = NULL;
            }
        } while (bWaitForNotification);

        DereferenceDevBitmap(pDevBitmap);
    }

    return;
}

// This function should be called while holding driver context and device context lock.
VOID
CheckAndSetDiskIdConflict(IN PDEV_CONTEXT pDevContext)
{
    PDEV_CONTEXT    DevContext = NULL;
    PLIST_ENTRY     pDevExtList;
    BOOLEAN         bDiskIdConflict = FALSE;
    ULONG           ulConflictingDiskNum = 0;


    if (!TEST_FLAG(pDevContext->ulFlags, DCF_DEVID_OBTAINED))
    {
        return;
    }

    pDevExtList = DriverContext.HeadForDevContext.Flink;

    for (; pDevExtList != &DriverContext.HeadForDevContext; pDevExtList = pDevExtList->Flink)
    {
        DevContext = (PDEV_CONTEXT)pDevExtList;
        if (pDevContext == DevContext){
            continue;
        }

        KeAcquireSpinLockAtDpcLevel(&DevContext->Lock);
        // Ignore disks which already are blocked due to conflicting id
        //              which are waiting for earlier removal to get cleaned up
        if (TEST_FLAG(DevContext->ulFlags, DCF_DEVID_OBTAINED) &&
            !TEST_FLAG(DevContext->ulFlags, DCF_DISKID_CONFLICT) &&            
            (GUID_SIZE_IN_BYTES == RtlCompareMemory(pDevContext->wDevID, 
                                   DevContext->wDevID,
                                   GUID_SIZE_IN_BYTES)))
        {
            ulConflictingDiskNum = DevContext->ulDevNum;

            if (TEST_FLAG(DevContext->ulFlags, DCF_REMOVE_CLEANUP_PENDING))
            {
                KeReleaseSpinLockFromDpcLevel(&DevContext->Lock);
                InDskFltWriteEventWithDevCtxt(INDSKFLT_WARN_CONFLICT_DEVICE_ID_GETTING_REMOVED, pDevContext, ulConflictingDiskNum);

                continue;
            }

            bDiskIdConflict = TRUE;
            KeReleaseSpinLockFromDpcLevel(&DevContext->Lock);
            break;
        }

        KeReleaseSpinLockFromDpcLevel(&DevContext->Lock);
    }

    if (bDiskIdConflict){
        SET_FLAG(pDevContext->ulFlags, DCF_FILTERING_STOPPED);
        SET_FLAG(pDevContext->ulFlags, DCF_DISKID_CONFLICT);

        InDskFltWriteEventWithDevCtxt(INDSKFLT_WARN_DEVICE_ID_CONFLICT, pDevContext, ulConflictingDiskNum);
        InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_FILTERING_STOPPED, pDevContext, pDevContext->ulFlags);
    }
}

PDEV_CONTEXT
GetDevContextWithGUID(
    __in   PWCHAR  pInputBuf,
    __in   ULONG inputbuflen
    )
/*++

Description:
    This is common function for fetching the DeviceContext for a given device ID
    (MBR sig/GPT GUID).
    Existing function is rewritten to avoid many changes across user/kernel space.
    For MBR signature(usually max. 10 characters) rest of the chars are set to NULL
    at both user/kernel space.

Arguments:

    pInputBuf   - User space fills either a pre-defined structure which is array
                  of GUID_SIZE_IN_BYTES (without NULL-termination) or a pointer.
                    For GUID, either the array is filled or pointer which is
                  minimum length of GUID_SIZE_IN_BYTES.
                    For Signature, rest of the array is filled with NULL and
                  driver does the same.
                    For pointer based inputs, this function refers input length
                  and copies the exact buffer.

    inputbuflen - input buffer LENGTH IN BYTES fetched from the IOCTL.

Assumptions:
    Signature/GUID exists within the first GUID_SIZE_IN_BYTES input buffer.

Return:

    PDEV_CONTEXT    - Pointer to appropriate Device Context.

    NULL            - Input params is zero or data buffer is zero or buffer does
                      not match with the list.

--*/
{
    PDEV_CONTEXT    pDevContext = NULL;
    PLIST_ENTRY     pCurr;
    KIRQL           OldIrql;
    // some callers include NULL-terminator and alignment as well.
    WCHAR           wDeviceID[GUID_SIZE_IN_CHARS + 2];
    BOOLEAN         bRetry = FALSE;
    BOOLEAN         bRescan = FALSE;

    ASSERT(0 != inputbuflen);
    ASSERT(NULL != pInputBuf);

    if ((0 == inputbuflen) || (NULL == pInputBuf)) {
#if DBG
        ASSERTMSG("GetDevContextWithGUID : One of the Input parameter is NULL", 0);
#endif
        InVolDbgPrint(DL_ERROR,
                      DM_INMAGE_IOCTL,
                      ("GetDevContextWithGUID : One of the Input parameter is NULL \n"));
        return NULL;
    }

    RtlZeroMemory(wDeviceID, ((GUID_SIZE_IN_CHARS + 2) * sizeof(WCHAR)));

    if (inputbuflen < GUID_SIZE_IN_BYTES) {
        if (inputbuflen == RtlCompareMemory(pInputBuf,
                                            DriverContext.ZeroGUID,
                                            inputbuflen)) {
            InVolDbgPrint(DL_ERROR,
                          DM_INMAGE_IOCTL,
                          ("GetDevContextWithGUID : Input Buffer is filled with NULL chars \n"));
            return NULL;
        }
        RtlCopyMemory(wDeviceID, pInputBuf, inputbuflen);
    } else {
        // We are interested in first 0x48 bytes 
        if (GUID_SIZE_IN_BYTES == RtlCompareMemory(pInputBuf,
                                                   DriverContext.ZeroGUID,
                                                   GUID_SIZE_IN_BYTES)) {
            InVolDbgPrint(DL_ERROR,
                          DM_INMAGE_IOCTL,
                          ("GetDevContextWithGUID : Input Buffer is filled with NULL chars \n"));
            return NULL;
        }

        // Systemcalls fill DeviceContext with GUID in lowercase now. Signature
        // is in decimal numbers, so no need to convert.
        RtlCopyMemory(wDeviceID, pInputBuf, GUID_SIZE_IN_BYTES);
        ConvertGUIDtoLowerCase(wDeviceID);
    }

    InVolDbgPrint(DL_INFO,
                  DM_INMAGE_IOCTL,
                  ("Value of wDeviceID is %S and Length is %lu \n", wDeviceID, wcslen(wDeviceID)));

    do
    {
        bRescan = FALSE;

        // Find the appropriate DeviceContext
        KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);

        for (pCurr = DriverContext.HeadForDevContext.Flink;
             pCurr != &DriverContext.HeadForDevContext;
             pCurr = pCurr->Flink) {

            PDEV_CONTEXT pTemp = (PDEV_CONTEXT)pCurr;

            if (TEST_FLAG(pTemp->ulFlags, DCF_REMOVE_CLEANUP_PENDING)) {
                continue;
            }

            // Rescan only when we have not retried till now
            if (!TEST_FLAG(pTemp->ulFlags, DCF_DEVID_OBTAINED)) {
                // If there is an uninitialized disk
                // scan its id if we have not retried till now.
                bRescan = !bRetry;
                continue;
            }

            // DISKID Conflict makes sense only when Disk Id is obtained
            if (TEST_FLAG(pTemp->ulFlags, DCF_DISKID_CONFLICT)) {
                continue;
            }

            if (PARTITION_STYLE_MBR == pTemp->DiskPartitionFormat) {
                if (GUID_SIZE_IN_BYTES == RtlCompareMemory(wDeviceID,
                                                           pTemp->wDevID,
                                                           GUID_SIZE_IN_BYTES)) {
                    pDevContext = pTemp;
                    InVolDbgPrint(DL_INFO,
                                  DM_INMAGE_IOCTL,
                                  ("GetDevContextWithGUID : MBR - Found Disk Filter Device Context \n"));
                    ReferenceDevContext(pDevContext);
                    break;
                }
            } else if (PARTITION_STYLE_GPT == pTemp->DiskPartitionFormat) {
                if (GUID_SIZE_IN_BYTES == RtlCompareMemory(wDeviceID,
                                                           pTemp->wDevID,
                                                           GUID_SIZE_IN_BYTES)) {
                    pDevContext = pTemp;
                    InVolDbgPrint(DL_INFO,
                                  DM_INMAGE_IOCTL,
                                  ("GetDevContextWithGUID : GPT - Found Disk Filter Device Context \n"));
                    ReferenceDevContext(pDevContext);
                    break;
                }
            } else{
                // This is definitely PARTITION_STYLE_RAW, Uninitialized disks
                // are not supported for now.

                // Do nothing
            }
        }

        KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

        // If disk id is already figured or, there is no rescan needed exit this loop
        if ((NULL != pDevContext) || !bRescan) {
            break;
        }

        // This piece of code can be optimized by figuring disk Ids from
        // only list of uninitialized disks.
        InDskFltRescanUninitializedDisks();

        //Retry now  as rescan has happened
        bRetry = TRUE;
    } while (bRetry);

    return pDevContext;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
FillBitmapFilenameInDevContext(_In_ _Notnull_ PDEV_CONTEXT pDevContext)
{
    return GetBitmapFileName(pDevContext, pDevContext->BitmapFileName, true);
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
GetBitmapFileName(
    _In_ _Notnull_ PDEV_CONTEXT pDevContext,
    _In_ UNICODE_STRING &bitmapFileName,
    _In_ bool writeFileNameToRegistry)
{
TRC
    NTSTATUS        Status = STATUS_SUCCESS;
    Registry        reg;
    UNICODE_STRING  uszOverrideFilename;
    UNICODE_STRING  usSystemDir;

    PAGED_CODE();

    if (NULL == pDevContext)
        return STATUS_INVALID_PARAMETER;

    InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, ("GetBitmapFileName: For device %S\n", pDevContext->wDevNum));

    ASSERT((MAX_LOG_PATHNAME * sizeof(WCHAR)) <= bitmapFileName.MaximumLength);
    bitmapFileName.Length = 0;
    uszOverrideFilename.Buffer = NULL;
    // There is no valid volume override.
    uszOverrideFilename.Length = 0;
    uszOverrideFilename.MaximumLength = MAX_LOG_PATHNAME*sizeof(WCHAR);
    uszOverrideFilename.Buffer = new WCHAR[MAX_LOG_PATHNAME];

    InVolDbgPrint(DL_INFO, DM_MEM_TRACE, ("GetBitmapFileName: Allocation %p\n", uszOverrideFilename.Buffer));
    if (NULL == uszOverrideFilename.Buffer) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("GetBitmapFileName: Allocation failed for Override filename\n"));
        Status = STATUS_NO_MEMORY;
        goto cleanup;
    }

    RtlZeroMemory(uszOverrideFilename.Buffer, MAX_LOG_PATHNAME*sizeof(WCHAR));
    // build a default log filename
    ASSERT((NULL != DriverContext.DefaultLogDirectory.Buffer) && (0 != DriverContext.DefaultLogDirectory.Length));
    bitmapFileName.Length = 0;
    uszOverrideFilename.Length = 0;

    GOTO_CLEANUP_ON_FAIL(RtlAppendUnicodeStringToString(&uszOverrideFilename, &DriverContext.DefaultLogDirectory));

    if (L'\\' != uszOverrideFilename.Buffer[(uszOverrideFilename.Length / sizeof(WCHAR)) - 1]) {
        GOTO_CLEANUP_ON_FAIL(RtlAppendUnicodeToString(&uszOverrideFilename, L"\\"));
    }
    
    GOTO_CLEANUP_ON_FAIL(RtlAppendUnicodeToString(&uszOverrideFilename, LOG_FILE_NAME_PREFIX)); // so things sort by name nice
    GOTO_CLEANUP_ON_FAIL(RtlAppendUnicodeToString(&uszOverrideFilename, pDevContext->wDevID));
    GOTO_CLEANUP_ON_FAIL(RtlAppendUnicodeToString(&uszOverrideFilename, LOG_FILE_NAME_SUFFIX));

    if (writeFileNameToRegistry) {
        // Write this path back to registry
        Status = SetStringInRegVolumeCfg(pDevContext, DEVICE_BITMAPLOG_PATH_NAME, &uszOverrideFilename);

        if (!NT_SUCCESS(Status)) {
            InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN,
                ("GetBitmapFileName: For device %S failed to persist log path name %wZ into registry with Status 0x%x\n",
                pDevContext->wDevNum, &uszOverrideFilename, Status));

            // Ignore the failure
            Status = STATUS_SUCCESS;
        }
    }

    // whether \systemroot\ is prefix
    RtlInitUnicodeString(&usSystemDir, SYSTEM_ROOT_PATH);
    if (FALSE == RtlPrefixUnicodeString(&usSystemDir, &uszOverrideFilename, TRUE)) {
        GOTO_CLEANUP_ON_FAIL(RtlAppendUnicodeToString(&bitmapFileName, DOS_DEVICES_PATH));
    }

    GOTO_CLEANUP_ON_FAIL(RtlAppendUnicodeStringToString(&bitmapFileName, &uszOverrideFilename));

cleanup:
    if (NULL != uszOverrideFilename.Buffer)
        delete uszOverrideFilename.Buffer;

    ValidatePathForFileName(&bitmapFileName);

    return Status;
}

NTSTATUS
GetDeviceNumber (
    __in   PDEVICE_OBJECT pDevObject,
    __inout  PSTORAGE_DEVICE_NUMBER pStorageDevNum,
    __in   USHORT usBufferSize
    )
/*++

Routine Description:

    Get the Device Num. Call IOCTL_STORAGE_GET_DEVICE_NUMBER

    This function must be called at IRQL < DISPATCH_LEVEL

Arguments:

    pDevObject:- Target Object to retrieve info
    pStorageDevNum:- Buffer o store the info,  
    usBufferSize:- Size of Input Buffer

Return Value:

    NTSTATUS

--*/
{
    IO_STATUS_BLOCK         IoStatus = {0};
    NTSTATUS                Status = STATUS_SUCCESS;

    ASSERT(NULL != pDevObject);

    //PAGED_CODE();
    Status = InMageSendDeviceControl(pDevObject,
                                     IOCTL_STORAGE_GET_DEVICE_NUMBER,
                                     NULL,
                                     0,
                                     pStorageDevNum,
                                     usBufferSize,
                                     &IoStatus);
    if (!NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT, 
                     ("%s: IOCTL_STORAGE_GET_DEVICE_NUMBER failed with Status 0x%x\n", 
                     __FUNCTION__, Status));
        InDskFltWriteEvent(INDSKFLT_ERROR_STORAGE_GET_DEVICE_NUMBER_ERROR, Status);
    }
    return Status;
}

NTSTATUS 
SetDevNumInDevContext (
    __in   PDEV_CONTEXT pDevContext,
    __in   PSTORAGE_DEVICE_NUMBER pStorageDevNum
    ) 
/*++

Routine Description:

    Sets the Device Num in Device Context

    This function takes the DevContext Lock.

    This function must be called at IRQL < DISPATCH_LEVEL

Arguments:

    DevContext - Get the Device Extention to set the info
    pStorageDevNum - Contains the disk number

Return Value:

    NTSTATUS

--*/
{
    KIRQL                   OldIrql                    = 0;
    NTSTATUS                Status                     = STATUS_SUCCESS;
    WCHAR                   wDevNum[DEVICE_NUM_LENGTH] = L"";
#if DBG
    CHAR                    chDevNum[DEVICE_NUM_LENGTH] = "";
#endif

    Status = ConvertUlongToWchar((ULONG)pStorageDevNum->DeviceNumber, 10, wDevNum, DEVICE_NUM_LENGTH);
    if (!NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT,
                     ("%s: ConvertUlongToWchar Failed with Status 0x%x\n", 
                     __FUNCTION__, Status));
        goto Cleanup;
    }
#if DBG
    //To Maintain legacy code        
    Status = ConvertWcharToChar(wDevNum, chDevNum, DEVICE_NUM_LENGTH);
    if (!NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT, 
                     ("%s: ConvertWcharToChar failed with Status 0x%x\n", 
                     __FUNCTION__, Status));
        //Dont Fail the call as it is part of information
        Status = STATUS_SUCCESS;
    }
    InVolDbgPrint(DL_VERBOSE, DM_DEV_CONTEXT, 
                 ("%s: DevNum %u wDevNum %S chDevNum %s\n", 
                 __FUNCTION__, pStorageDevNum->DeviceNumber, wDevNum, chDevNum));
#endif

    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
    pDevContext->ulDevNum = pStorageDevNum->DeviceNumber;
    RtlCopyMemory(pDevContext->wDevNum, 
                  wDevNum, 
                  sizeof(wDevNum));
#if DBG
    RtlCopyMemory(pDevContext->chDevNum, 
                  chDevNum, 
                  sizeof(chDevNum));
#endif
    SET_FLAG(pDevContext->ulFlags, DCF_DEVNUM_OBTAINED);
    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
Cleanup:
    return Status;
}


NTSTATUS
ExtractDevIDFromDiskSignature(
    __in    PDISK_SIGNATURE pDiskSignature,
    __out   PWSTR wBuffer,
    __in    USHORT usBufferLength
)
/*++

Routine Description:

    Get the MBR/GUID into String format

    This function must be called at IRQL < DISPATCH_LEVEL

Arguments:

    pDiskSignature -    Disk signature structure as defined by ntddk.
    Buffer -            Buffer to store the string
    BufferLength -      It is length. It is number of characters

Return Value:

    NTSTATUS

--*/
{
    UNICODE_STRING  UniString;
    UNICODE_STRING  GUIDString = { 0 };
    USHORT          usGuidLength = 0;
    USHORT          usBufferSize = usBufferLength * sizeof(WCHAR);
    NTSTATUS        Status = STATUS_SUCCESS;

    //PAGED_CODE();

    if (NULL == pDiskSignature) {
        Status = STATUS_INVALID_PARAMETER_1;
        goto Cleanup;
    }
    ASSERT(NULL != wBuffer);
    ASSERT(usBufferLength == DEVID_LENGTH);

    RtlInitEmptyUnicodeString(&UniString, wBuffer, usBufferSize);

    if (PARTITION_STYLE_MBR == pDiskSignature->PartitionStyle) {
        //It seems from disk class driver value is set to 0 for RAW. Not documented
        if (0 == pDiskSignature->Mbr.Signature) {
            Status = STATUS_NO_SUCH_DEVICE;
            goto Cleanup;
        }

        //Convert MBR to String
        Status = RtlIntegerToUnicodeString((ULONG)pDiskSignature->Mbr.Signature,
            10,
            &UniString);
        if (!NT_SUCCESS(Status)) {
            InVolDbgPrint(DL_ERROR, DM_DRIVER_INIT,
                ("%s: RtlIntegerToUnicodeString Failed with Status 0x%x\n",
                    __FUNCTION__, Status));

            goto Cleanup;
        }
    }
    else if (PARTITION_STYLE_GPT == pDiskSignature->PartitionStyle) {
        //Convert Guid to String
        Status = RtlStringFromGUID(pDiskSignature->Gpt.DiskId, &GUIDString);
        if (!NT_SUCCESS(Status)) {
            InVolDbgPrint(DL_ERROR, DM_DRIVER_INIT,
                ("%s: RtlStringFromGUID Failed with Status 0x%x\n",
                    __FUNCTION__, Status));
            goto Cleanup;
        }
        //Dont copy braces, Decrease lenght by 2 WCHAR
        usGuidLength = GUIDString.Length - (sizeof(WCHAR) * 2);
        if (usGuidLength > usBufferSize) {
            InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT,
                ("%s: Length mismatched GuidLength %u BufferSize %u\n",
                    __FUNCTION__, usGuidLength, usBufferSize));
            goto Cleanup;
        }
        //Skip opening braces
        RtlCopyMemory(wBuffer, GUIDString.Buffer + 1, usGuidLength);
    }
    else {
        //Raw Device
        Status = STATUS_NO_SUCH_DEVICE;
        goto Cleanup;
    }
Cleanup:
    if (NULL != GUIDString.Buffer) {
        RtlFreeUnicodeString(&GUIDString);
    }
    return Status;
}


NTSTATUS
IndskfltGetDiskSize(PDEV_CONTEXT    pDevContext, LONGLONG   *llDevSize) 
{
    NTSTATUS    Status = STATUS_SUCCESS;
    GET_LENGTH_INFORMATION      getLengthInfo = { 0 };
    IO_STATUS_BLOCK             IoStatus;

    ASSERT(NULL != pDevContext);

    // Get the Device ID & Size
    Status = InMageSendDeviceControl(pDevContext->TargetDeviceObject,
        IOCTL_DISK_GET_LENGTH_INFO,
        NULL,
        0,
        &getLengthInfo,
        sizeof(getLengthInfo),
        &IoStatus);


    if (!NT_SUCCESS(Status)) {
        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_DEV_GET_LENGTH_INFO_FAILED, pDevContext, Status);
        return Status;
    }

    if (IoStatus.Information < sizeof(getLengthInfo)) {
        Status = STATUS_BUFFER_TOO_SMALL;
        return Status;
    }

    *llDevSize = getLengthInfo.Length.QuadPart;
    return Status;
}

NTSTATUS 
SetDevIDAndSizeInDevContext (
    __in    PDEV_CONTEXT pDevContext,
    __in   PWSTR wBuffer,
    __in    USHORT usBufferLength,
    __in   PARTITION_STYLE         DiskPartitionFormat,
    __in   ULONG ulSetFlags
    )
/*++

Routine Description:

    Sets the Device Info in Device Context

    This function takes the DevContext Lock.

    This function must be called at IRQL < DISPATCH_LEVEL

Arguments:

    DevContext - Get the Device Extention to set the info
    pPartitionInfo - Parition Information
    SetFlags - Set the required info

Return Value:

    NTSTATUS

--*/
{
    KIRQL                   OldIrql               = 0;
    BOOLEAN                 bSetDevId             = FALSE;
    BOOLEAN                 bSetDevSize           = FALSE;
    NTSTATUS                Status                = STATUS_SUCCESS;
    LONGLONG                llDevSize = 0;

#if DBG
    CHAR                    chDevID[DEVID_LENGTH] = "";
#endif

    ASSERT(NULL != pDevContext);

    if (TEST_FLAG(ulSetFlags, DCF_DEVSIZE_OBTAINED)) {
        bSetDevSize = TRUE;
    }

    bSetDevId = TRUE;

    Status = IndskfltGetDiskSize(pDevContext, &llDevSize);
    if (!NT_SUCCESS(Status)) {
        bSetDevSize = FALSE;
        goto Cleanup;
    }

Cleanup:
    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    KeAcquireSpinLockAtDpcLevel(&pDevContext->Lock);
    if (bSetDevSize) {
        if (!TEST_FLAG(pDevContext->ulFlags, DCF_DEVSIZE_OBTAINED)) {
            pDevContext->llDevSize = llDevSize;
            SET_FLAG(pDevContext->ulFlags, DCF_DEVSIZE_OBTAINED);
        }
        pDevContext->llActualDeviceSize = llDevSize;
    }

    if (bSetDevId &&
        !TEST_FLAG(pDevContext->ulFlags, DCF_DEVID_OBTAINED)) {

        RtlCopyMemory(pDevContext->wDevID, 
                      wBuffer, 
                      usBufferLength);
#if DBG
        RtlCopyMemory(pDevContext->chDevID, 
                      chDevID, 
                      sizeof(chDevID));
#endif
        SET_FLAG(pDevContext->ulFlags, DCF_DEVID_OBTAINED);
        pDevContext->DiskPartitionFormat = DiskPartitionFormat;
        CheckAndSetDiskIdConflict(pDevContext);
    }
    KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

    return Status;
}

NTSTATUS
SetDevInfoInDevContext (
    __in    PDEV_CONTEXT pDevContext
    )
/*++

Routine Description:

    Gets the Device Info from the OS and sets in Device Context

    This function takes the DevContext Lock.

    This function must be called at IRQL < DISPATCH_LEVEL

Arguments:

    DevContext - Get the Device Extention to set the info


Return Value:

    NTSTATUS, Return Error incase failed to set Device ID/Size
    Device Number is more of used as information 

--*/
{
    STORAGE_DEVICE_NUMBER           StorageDevNum   = {0};
    ULONG                           ulFlags         = 0;
    ULONG                           ulSetFlags      = 0;
    KIRQL                           OldIrql         = 0;
    NTSTATUS                        Status          = STATUS_SUCCESS;
    PSTORAGE_DEVICE_DESCRIPTOR      pStorageDesc = NULL;
    PWCHAR                           wDevID = NULL;
    PARTITION_STYLE                 partitionStyle = PARTITION_STYLE::PARTITION_STYLE_RAW;

    ASSERT(NULL != pDevContext);

    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
    //Get the current flags
    SET_FLAG(ulFlags, pDevContext->ulFlags);
    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);

    //Retrieve Device Number and set in DevContext
    if (!TEST_FLAG(ulFlags, DCF_DEVNUM_OBTAINED)) {
        // Get the Device Number
        Status = GetDeviceNumber(pDevContext->TargetDeviceObject,
                           &StorageDevNum,
                           sizeof(StorageDevNum));
        if (NT_SUCCESS(Status)) {
             SetDevNumInDevContext(pDevContext, &StorageDevNum);
        } else {
             //Device Number is more of used as information. Dont fail the call
             Status = STATUS_SUCCESS;
        }
    }

    Status = InDskFltQueryStorageProperty(pDevContext->TargetDeviceObject, &pStorageDesc);
    if (!NT_SUCCESS(Status)) {
        // Log an event
        Status = STATUS_SUCCESS;
    }

    if (NULL != pStorageDesc) {
        KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
        if (NULL != pDevContext->pDeviceDescriptor) {
            ExFreePoolWithTag(pDevContext->pDeviceDescriptor, TAG_DEVICE_INFO);
        }
        pDevContext->pDeviceDescriptor = pStorageDesc;
        KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
    }

    wDevID = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool, DEVID_LENGTH * sizeof(WCHAR), TAG_BASIC_DISK);
    if (NULL == wDevID) {
        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_FUNCTION_FAILURE, pDevContext,
            L"ExAllocatePoolWithTag",
            Status);

        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Cleanup;
    }

    RtlZeroMemory(wDevID, DEVID_LENGTH * sizeof(WCHAR));
    if (!IS_DEVINFO_OBTAINED(ulFlags)) {
        Status = IndskfltGetDiskIdentity(pDevContext, wDevID, DEVID_LENGTH, &partitionStyle);
        if (!NT_SUCCESS(Status)) {
            InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_FUNCTION_FAILURE, pDevContext,
                L"IndskfltGetDiskIdentity",
                Status);
            goto Cleanup;
        }

        //Set the flags to store the info in device context
        if (!TEST_FLAG(ulFlags, DCF_DEVID_OBTAINED)) {
            SET_FLAG(ulSetFlags, DCF_DEVID_OBTAINED);
        }

        if (!TEST_FLAG(ulFlags, DCF_DEVSIZE_OBTAINED)) {
            SET_FLAG(ulSetFlags, DCF_DEVSIZE_OBTAINED);
        }

        Status = SetDevIDAndSizeInDevContext(pDevContext, wDevID, DEVID_LENGTH*sizeof(WCHAR), partitionStyle, ulSetFlags);
        if (!NT_SUCCESS(Status)) {
            InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_FUNCTION_FAILURE, pDevContext,
                L"SetDevIDAndSizeInDevContext",
                Status);

            goto Cleanup;
        }
    }

    InVolDbgPrint(DL_VERBOSE, DM_DEV_CONTEXT,
                 ("%s: wDevID %ws chDevID %s DevSize %I64X PartSyle %lu DevNum %lu wDevNum %ws chDevNum %s ulFlags %lu Dev->ulFlags %lu\n",
                 __FUNCTION__,pDevContext->wDevID, pDevContext->chDevID, 
                 pDevContext->llDevSize, pDevContext->DiskPartitionFormat,
                 pDevContext->ulDevNum, pDevContext->wDevNum,
                 pDevContext->chDevNum, ulFlags, pDevContext->ulFlags));

Cleanup:
    if (NULL != wDevID) {
        ExFreePoolWithTag(wDevID, TAG_BASIC_DISK);
    }
    return Status;
}

void
ProcessNoPageFaultPreOpList (
    _In_ _Notnull_  PLIST_ENTRY pListHead,
    _Out_opt_       PUINT32     pSucceededDiskCnt
    )
/*++

Routine Description:

    Process the given device context list for NoPageFault Preoperation

    PASSIVE_LEVEL Operations

    	Flush Changes and Wait to get cached

    	Flush the cache changes to Bitmap

    	Convert to Raw and learn the Physical device and offset    

    	This function must be called at IRQL <= DISPATCH_LEVEL

Arguments:

    pListHead - List of Device Context to Process

    pSucceededDiskCnt - If not null, returns count of disks for which
    preshutdown succeeded.

--*/
{
    PLIST_ENTRY             Entry = NULL;
    PDEV_CONTEXT            pDevContext = NULL;
    PLIST_NODE              pListNode = NULL;
    LONG                    lRefCount;
    NTSTATUS                Status;
    LONG                    OutOfSyncErrorCode;
    bool                    bMarkForResync;

    ASSERT(NULL != pListHead);

    if (NULL != pSucceededDiskCnt) {
        *pSucceededDiskCnt = 0;
    }

    while(!IsListEmpty(pListHead)) {
        Status = STATUS_UNSUCCESSFUL;
        bMarkForResync = false;
        Entry = RemoveHeadList(pListHead);
        pListNode = (PLIST_NODE) CONTAINING_RECORD((PCHAR)Entry, LIST_NODE, ListEntry);
        pDevContext = (PDEV_CONTEXT)pListNode->pData;

        if (IS_DISK_CLUSTERED(pDevContext)) {
            InDskFltDeleteClusterNotificationThread(pDevContext);
        }

        if (PASSIVE_LEVEL == KeGetCurrentIrql()) {
            Status = InMageFltSaveAllChanges(pDevContext, TRUE, NULL, true);
            if (!NT_SUCCESS(Status)) {
                InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_PERSISTING_CHANGES_FAILED,
                                              pDevContext, Status);
                OutOfSyncErrorCode = MSG_INDSKFLT_ERROR_RESYNC_PERSISTING_CHANGES_FAILED_Message;
                bMarkForResync = true;
            }

            Status = PersistFDContextFields(pDevContext, &OutOfSyncErrorCode);
            if (!NT_SUCCESS(Status)) {
                InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_PERSISTING_REGISTRY_FAILED,
                                              pDevContext, Status);
                bMarkForResync = true;
            }

            Status = ChangeModeToRawBitmap(pDevContext, &OutOfSyncErrorCode);
            if (!NT_SUCCESS(Status)) {
                InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_RAW_BITMAP_FAILED,
                                              pDevContext, Status);
                bMarkForResync = true;
            }

            if (bMarkForResync) {
                SetDevOutOfSync(pDevContext, OutOfSyncErrorCode, Status, false);
            }
        } // Else:- Raw bitmap will not be enabled. Bitmap will be unclean

        if (NT_SUCCESS(Status)) {
            InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_PRESHUTDOWN_SUCCESS, pDevContext, pDevContext->ulFlags);
            if (NULL != pSucceededDiskCnt) {
                ++*pSucceededDiskCnt;
            }
        } else {
            InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_PRESHUTDOWN_FAILURE, pDevContext, pDevContext->ulFlags, KeGetCurrentIrql(), Status);
        }

        lRefCount = DereferenceListNode(pListNode);
        if (0 >= lRefCount) {
            DereferenceDevContext(pDevContext);
        }
    }
}

void
PreShutdown(
    )
/*++

Routine Description:

    Prepare the devices for raw bitmap

    Mark the devices for No Page fault henceforth

    This function must be can be called at IRQL <= DISPATCH_LEVEL
--*/
{
    PDEV_CONTEXT    pDevContextLink = NULL;
    PLIST_ENTRY     pEntry = NULL;
    KIRQL           OldIrql = 0;
    LIST_ENTRY      NoPageFaultPreOpList;
    KIRQL           CurrentIrql = 0;
    UINT32          SucceededDiskCount;

    InitializeListHead(&NoPageFaultPreOpList);

    InDskFltWriteEvent(INDSKFLT_INFO_PRESHUTDOWN_NOTIFICATION);
    InDskFltWriteEvent(INDSKFLT_INFO_DEV_ALLOC_STATS, DriverContext.lCntDevContextAllocs, DriverContext.lCntDevContextFree);

    CurrentIrql = KeGetCurrentIrql();
    if (PASSIVE_LEVEL != CurrentIrql) {
        InDskFltWriteEvent(INDSKFLT_INFO_PRESHUTDOWN_IRQL, CurrentIrql);

        // Signal All Volume flush threads to stop
        KeSetEvent(&DriverContext.StopFSFlushEvent, IO_NO_INCREMENT, FALSE);
        goto Cleanup;
    }

    if (DriverContext.bFlushFsPreShutdown) {
        InDskFltFlushFileBuffers();
    }
    else {
        // Signal All Volume flush threads to stop
        KeSetEvent(&DriverContext.StopFSFlushEvent, IO_NO_INCREMENT, FALSE);

        InDskFltWriteEvent(INDSKFLT_INFO_FS_FLUSH_DISABLED);
    }

    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    pEntry = DriverContext.HeadForDevContext.Flink;
    while (pEntry != &DriverContext.HeadForDevContext) {
        pDevContextLink = (PDEV_CONTEXT)pEntry;
        pEntry = pEntry->Flink;
        //Prepare the list of device eligible for NoPageFault Pre Operation
        AddDeviceContextToList(pDevContextLink, EXCLUDE_NOPAGE_FAULT_PREOP, 0, &NoPageFaultPreOpList, 0, true);
    }
    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

    ProcessNoPageFaultPreOpList(&NoPageFaultPreOpList, &SucceededDiskCount);

    InDskFltWriteEvent(INDSKFLT_INFO_PRESHUTDOWN_SUCCESSFUL_DISKS,
        DriverContext.PreShutdownDiskCount,
        SucceededDiskCount);

Cleanup:
    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    pEntry = DriverContext.HeadForDevContext.Flink;
    while (pEntry != &DriverContext.HeadForDevContext) {
        pDevContextLink = (PDEV_CONTEXT)pEntry;
        pEntry = pEntry->Flink;
        //By Default set to No Page fault for all devices
        SetDevCntFlag(pDevContextLink, DCF_DONT_PAGE_FAULT, 0);	
    }
    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);
    if (PASSIVE_LEVEL == KeGetCurrentIrql()) {
        //Added for Persistent Time Stamp
        if (NULL != DriverContext.pTimeSeqFlushThread) {
            KeSetEvent(&DriverContext.PersistentThreadShutdownEvent, IO_DISK_INCREMENT, FALSE);
            KeWaitForSingleObject(DriverContext.pTimeSeqFlushThread, Executive, KernelMode, FALSE, NULL);
            ObDereferenceObject(DriverContext.pTimeSeqFlushThread);
            DriverContext.pTimeSeqFlushThread = NULL;
        } else {
            InDskFltWriteEvent(INDSKFLT_ERROR_PERSISTENT_THREAD_FAILED);
        }
    }
    return;
}

void 
ProcessPostShutdownList (
    __in    PLIST_ENTRY    pListHead
    )
/*++

Routine Description:

    Process the Device Context list on Post Shutdown Notification
 
    It is best efforts to flush the changes in raw mode

    Indskflt does not mark for resync incase Post Shutdown is not processed

    This function can be called at IRQL <= DISPATCH_LEVEL

--*/
{
    PLIST_ENTRY             Entry = NULL;
    PLIST_NODE              pListNode = NULL;
    PDEV_CONTEXT            pDevContext = NULL;
    LONG                    lRefCount;

    ASSERT(NULL != pListHead);

    while(!IsListEmpty(pListHead)) {
        Entry = RemoveHeadList(pListHead);
        pListNode = CONTAINING_RECORD((PCHAR)Entry, LIST_NODE, ListEntry);
        pDevContext = (PDEV_CONTEXT)pListNode->pData;
        if (PASSIVE_LEVEL == KeGetCurrentIrql()) {
            InMageFltSaveAllChanges(pDevContext, TRUE, NULL, false);
        }
        lRefCount = DereferenceListNode(pListNode);
        if (0 >= lRefCount) {
            DereferenceDevContext(pDevContext);
        }
    }
}

void
PreparePostShutdownList (
    __in    PDEV_CONTEXT    pDevContext,
    __in    PLIST_ENTRY     pListHead
    )
/*++

Routine Description:

    Prepare the Device Context list on Post Shutdown Notification

    Skip devices where bitmap is not converted to raw

    Skip devices where bitmap dev OFF is receieved

    Insert in the queue only in case NO_PAGE_FAULT is set

    This function must be called at IRQL <= DISPATCH_LEVEL

--*/
{
    KIRQL                   OldIrql = 0;
    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
    //Fix for Bug 5645504
    if (NULL == pDevContext->pBitmapDevObject) {
        goto Cleanup;
    }
    AddDeviceContextToList(pDevContext,
                    EXCLUDE_POST_SHUTDOWN_OP,
                    INCLUDE_POST_SHUTDOWN_OP,
                    pListHead,
                    1);
Cleanup:
    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
    return;
}

void PostShutdown(
    )
/*++

Routine Description:

    Best Efforts to flush the changes in Raw mode Buffer

    It does not store the changes in bitmap

    This function must be called at IRQL <= DISPATCH_LEVEL

--*/
{
    PDEV_CONTEXT            pDevContextLink = NULL;
    PLIST_ENTRY             pEntry = NULL;
    KIRQL                   OldIrql = 0;
    LIST_ENTRY              PostShutdownDCList;
    InitializeListHead(&PostShutdownDCList);

    if (PASSIVE_LEVEL != KeGetCurrentIrql()) {
        goto Cleanup;
    }

    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    pEntry = DriverContext.HeadForDevContext.Flink;
    while (pEntry != &DriverContext.HeadForDevContext) {
        pDevContextLink = (PDEV_CONTEXT)pEntry;
        pEntry = pEntry->Flink;
        PreparePostShutdownList(pDevContextLink, &PostShutdownDCList);
    }
    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);
    ProcessPostShutdownList(&PostShutdownDCList);

Cleanup:
    return;
}

void
ProcessBitmapDevOffList (
    __in    PLIST_ENTRY    pListHead
    )
/*++

Routine Description:

    Process the Device Context list that stored bitmap on current device

    Passive Levele Operation

    	Flush the changes and persist in bitmap in raw mode

    	Validate for resync and pending changes

    	Close bitmap with appropriate error    

    This function must be called at IRQL <= DISPATCH_LEVEL

--*/
{
    PLIST_ENTRY             Entry = NULL;
    PLIST_NODE              pListNode = NULL;
    PDEV_CONTEXT            pDevContext = NULL;
    KIRQL                   OldIrql = 0;
    PDEV_BITMAP             pDevBitmap = NULL;
    LONG                    lRefCount;
    tagLogRecoveryState     recoveryState;
    ULONG                   ResyncErrorCode;
    LONGLONG                ResyncTimeStamp;

    ASSERT(NULL != pListHead);
    while(!IsListEmpty(pListHead)) {
        Entry = RemoveHeadList(pListHead);
        pListNode = (PLIST_NODE) CONTAINING_RECORD((PCHAR)Entry, LIST_NODE, ListEntry);
        pDevContext = (PDEV_CONTEXT)pListNode->pData;
        if ((PASSIVE_LEVEL == KeGetCurrentIrql()) &&
            (NULL != pDevContext->pDevBitmap)) {
            InMageFltSaveAllChanges(pDevContext, TRUE, NULL);
            KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
            recoveryState = CheckResyncDetected(pDevContext, 1);
            pDevBitmap = pDevContext->pDevBitmap;
            //D-Link Bitmap object with Device Context
            pDevContext->pDevBitmap = NULL;
            // Outstanding resync details are persisted as part of bitmap
            ResyncErrorCode = pDevContext->ulOutOfSyncErrorCode;
            ResyncTimeStamp = pDevContext->liOutOfSyncTimeStamp.QuadPart;

            KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
            CloseBitmapFile(pDevBitmap, false, pDevContext, recoveryState, false, ResyncErrorCode, ResyncTimeStamp);
        }
        //Set by defaul to the state irrespective of error
        SetDevCntFlag(pDevContext, DCF_BITMAP_DEV_OFF, 0);
        lRefCount = DereferenceListNode(pListNode);
        if (0 >= lRefCount) {
            DereferenceDevContext(pDevContext);
        }
    }
}

void
PrepareBitmapDevOffList (
    __in    PDEV_CONTEXT    pDevContext,
    __in    PDEVICE_OBJECT  pBitmapDevObject,
    __in    PLIST_ENTRY     pBitmapPreOffDCList
    )
/*++

Routine Description:

    Prepare the Device Context list which contain bitmap on current device

    Skip devices where bitmap devices flag is set to off

    Skip devices where filtering is off
    
    This function must be called at IRQL <= DISPATCH_LEVEL

--*/
{
    KIRQL                   OldIrql = 0;
    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
    if (pBitmapDevObject != pDevContext->pBitmapDevObject) {
        goto Cleanup;
    }
    AddDeviceContextToList(pDevContext,
                    EXCLUDE_BITMAPOFF_PREOP,
                    0,
                    pBitmapPreOffDCList,
                    1);

Cleanup:
	KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
}

void BitmapDeviceOff(
    __in    PDEVICE_OBJECT pBitmapDevObject
    )
/*++

Routine Description:

    Flush the change to raw bitmap and persist

    Close bitmap file with appropriate status

    This function must be called at IRQL <= DISPATCH_LEVEL

--*/

{
    PDEV_CONTEXT            pDevContextLink = NULL;
    PLIST_ENTRY             Entry = NULL;
    LIST_ENTRY              BitmapPreOffDCList;
    KIRQL                   OldIrql = 0;

    ASSERT(NULL != pBitmapDevObject);

    InitializeListHead(&BitmapPreOffDCList);
    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    Entry = DriverContext.HeadForDevContext.Flink;
    while (Entry != &DriverContext.HeadForDevContext) {
        pDevContextLink = (PDEV_CONTEXT)CONTAINING_RECORD((PCHAR)Entry, DEV_CONTEXT, ListEntry);
        Entry = Entry->Flink;
        PrepareBitmapDevOffList(pDevContextLink, pBitmapDevObject, &BitmapPreOffDCList);
    }
    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);
    ProcessBitmapDevOffList(&BitmapPreOffDCList);
    return;
}


void PostRemoveDevice(
    __in    PDEV_CONTEXT    pDevContext
    )
/*++

Routine Description:

    Flush the changes to bitmap cache

    Persist the registry changes

    Convert to Raw

    Flush the changes(Ideally Not required)

    Close Bitmap

    Remove Entry from global list

    This function must be called at IRQL < APC_LEVEL

Arguments:

    DevContext - Device Context to be removed

--*/
{
    LIST_ENTRY				ListHead;
    PDEV_BITMAP             pDevBitmap = NULL;
    KIRQL                   OldIrql = 0;

    ASSERT(NULL != pDevContext);

	InitializeListHead(&ListHead);

    // Cleanup for stop filtered device
    if ((TEST_FLAG(pDevContext->ulFlags, DCF_FILTERING_STOPPED)) &&
        (PASSIVE_LEVEL == KeGetCurrentIrql()) &&
        (NULL != pDevContext->pDevBitmap)) {
        KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
        pDevBitmap = pDevContext->pDevBitmap;
        pDevContext->pDevBitmap = NULL;
        KeReleaseSpinLock(&pDevContext->Lock, OldIrql);        
        CloseBitmapFile(pDevBitmap, false, pDevContext, dirtyShutdown, false);
    }
    AddDeviceContextToList(pDevContext, EXCLUDE_NOPAGE_FAULT_PREOP, 0, &ListHead, 0);
    ProcessNoPageFaultPreOpList(&ListHead, NULL);
    SetDevCntFlag(pDevContext, DCF_DONT_PAGE_FAULT, 0);

    InitializeListHead(&ListHead);
    AddDeviceContextToList(pDevContext,
                    EXCLUDE_BITMAPOFF_PREOP,
                    0,
                    &ListHead,
                    0);
    ProcessBitmapDevOffList(&ListHead);

    RemoveDeviceContextFromGlobalList(pDevContext);
    return;
}
