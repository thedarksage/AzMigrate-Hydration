/*++

Module Name:

    InCDskFl.c

Abstract:

    This driver monitors ClusDisk0 device

Environment:

    kernel mode only

Notes:

--*/


#include "CDskCommon.h"
#include "CDskNode.h"
#include "InVolFlt.h"
#include "incdskfllog.h"

// ulDebugLevelForAll specifies the debug level for all modules
// if this is non zero all modules info upto this level is displayed.
ULONG   ulDebugLevelForAll = DL_WARNING;
// ulDebugLevel specifies the debug level for modules specifed in
// ulDebugMask
ULONG   ulDebugLevel = DL_TRACE_L2;
// ulDebugMask specifies the modules whose debug level is specified
// in ulDebugLevel
ULONG   ulDebugMask = DM_VOLUME_ACQUIRE | DM_VOLUME_RELEASE | DM_IOCTL_INFO | DM_DISK_DETACH | DM_DISK_ATTACH;

//
// Function declarations
//

extern "C"  NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

#if (NTDDI_VERSION >= NTDDI_VISTA)
VOID Reinitialize( 
    IN PDRIVER_OBJECT  DriverObject, 
    IN PVOID  Context, 
    IN ULONG  ReinitializeCount 
    ); 
#endif
NTSTATUS
InCDskFlForwardIrpSynchronous(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
InCDskFlSendToNextDriver(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );


NTSTATUS
InCDskFlDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
InCDskFlUnload(
    IN PDRIVER_OBJECT DriverObject
    );

NTSTATUS
InCDskFlDispatchInBypassMode(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
InCDskFlLogError(
    IN PDEVICE_OBJECT DeviceObject,
    IN ULONG UniqueId,
    IN NTSTATUS ErrorCode,
    IN NTSTATUS Status
    );

NTSTATUS
InCDskFlIrpCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
ReadUnicodeString(PCWSTR nameString, 
    PUNICODE_STRING value,
    HANDLE hRegistry);

NTSTATUS
BypassCheckBootIni();

ULONG
GetNumClusterDisks(VOID);

VOID 
InMageCDskFltLogError(
   IN PDRIVER_OBJECT DriverObject,
   IN ULONG uniqueId,
   NTSTATUS Status
   );

VOID
GetAllFsContext(
    IN PDEVICE_OBJECT TargetDeviceObject
    );

//
// Define the sections that allow for discarding (i.e. paging) some of
// the code.
//

DRIVER_CONTEXT  DriverContext;

/*++

Routine Description:

     This routine dereferences the cluster disk nodes created as part of disk acquire control path,
     also free up the resources allocated.

Return Value:

    STATUS_SUCCESS

--*/

NTSTATUS
CleanDriverContext()
{
    
	while(FALSE == IsListEmpty(&DriverContext.ClusDiskListHead)) {
        PCLUSDISK_NODE  pClusDiskNode;
        LONG            lRefCount;

        pClusDiskNode = (PCLUSDISK_NODE)RemoveHeadList(&DriverContext.ClusDiskListHead);
        lRefCount = DereferenceClusDiskNode(pClusDiskNode);
        ASSERT(0 == lRefCount);
    }

    if (DriverContext.ulFlags & DRVCXT_FLAGS_CD_POOL_INITIALZIED) {
        ExDeletePagedLookasideList(&DriverContext.ClusDiskNodePool);
        DriverContext.ulFlags &= ~DRVCXT_FLAGS_CD_POOL_INITIALZIED;
    }

    if (DriverContext.ulFlags & DRVCXT_FLAGS_DEVICE_ATTACHED) {
        IoDetachDevice(DriverContext.ClusDisk0PDO);
        DriverContext.ulFlags &= ~DRVCXT_FLAGS_DEVICE_ATTACHED;
    }

    if (NULL != DriverContext.InMageFilterFO) {
        ObDereferenceObject(DriverContext.InMageFilterFO);
        DriverContext.InMageFilterFO = NULL;
        DriverContext.InMageFilterDO = NULL;
    }

	if (NULL != DriverContext.ClusDisk0PDO) {
        ObDereferenceObject(DriverContext.ClusDisk0PDO);
		DriverContext.ClusDisk0PDO = NULL;
	}

    if (NULL != DriverContext.ClusDisk0FiDO) {
        IoDeleteDevice(DriverContext.ClusDisk0FiDO);
        DriverContext.ClusDisk0FiDO = NULL;
    }

    if (DriverContext.DriverRegPath.Buffer) {
        ExFreePool(DriverContext.DriverRegPath.Buffer);
        DriverContext.DriverRegPath.Buffer = NULL;
    }

    DriverContext.DriverRegPath.MaximumLength = 0;
    DriverContext.DriverRegPath.Length = 0;

    return STATUS_SUCCESS;
}

NTSTATUS
InitializeDriverContext(
    PDRIVER_OBJECT  DriverObject,
    PDEVICE_OBJECT ClusDisk0PDO,
    PFILE_OBJECT InMageFilterFO,
    PDEVICE_OBJECT InMageFilterDO
    )
{
    NTSTATUS    Status;
    PDEVICE_EXTENSION   pDevExt;

    RtlZeroMemory(&DriverContext, sizeof(DRIVER_CONTEXT));

    InitializeListHead(&DriverContext.ClusDiskListHead);
    ExInitializeFastMutex(&DriverContext.ClusDiskListLock);

    ExInitializePagedLookasideList(&DriverContext.ClusDiskNodePool,
                                    NULL, NULL, 0, // Allocate, Free, Flags
                                    sizeof(CLUSDISK_NODE), MEM_TAG_CLUSDISK_NODE, 0);
    DriverContext.ulFlags |= DRVCXT_FLAGS_CD_POOL_INITIALZIED;
    
    Status = IoCreateDevice(DriverObject, sizeof(DEVICE_EXTENSION), NULL, 
                            FILE_DEVICE_UNKNOWN, 0, FALSE, &DriverContext.ClusDisk0FiDO);
    if (!NT_SUCCESS(Status))
    {
        ObDereferenceObject(ClusDisk0PDO);
        return Status;
    }


    //
    //Copy respective device objects and file objects into driver context
    //
    DriverContext.ClusDisk0PDO        = ClusDisk0PDO;
    DriverContext.InMageFilterFO      = InMageFilterFO;
    DriverContext.InMageFilterDO      = InMageFilterDO;

    DriverContext.DriverObject = DriverObject;


    pDevExt = (PDEVICE_EXTENSION)DriverContext.ClusDisk0FiDO->DeviceExtension;
    RtlZeroMemory(pDevExt, sizeof(DEVICE_EXTENSION));
    pDevExt->DeviceObject = DriverContext.ClusDisk0FiDO;
    pDevExt->TargetDeviceObject = IoAttachDeviceToDeviceStack(
                                    DriverContext.ClusDisk0FiDO,
                                    DriverContext.ClusDisk0PDO);

    if(NULL == pDevExt->TargetDeviceObject) {
        Status = STATUS_INVALID_DEVICE_STATE;
        return Status;
    } else {
        DriverContext.ulFlags |= DRVCXT_FLAGS_DEVICE_ATTACHED;        
    }


    pDevExt->DeviceObject->StackSize = pDevExt->TargetDeviceObject->StackSize + 1;

    return STATUS_SUCCESS;
}


NTSTATUS
NotifyInMageFilterOfDeviceRelease(
#if (NTDDI_VERSION >= NTDDI_VISTA)
                                  PCDSKFL_INFO DiskInfo
#else
                                  ULONG ulDiskSignature
#endif
                                  )
{
    KEVENT          Event;
    PIRP            Irp;
    IO_STATUS_BLOCK IoStatusBlock;
    NTSTATUS        Status;

    if (NULL == DriverContext.InMageFilterDO)
        return STATUS_INVALID_PARAMETER;

    KeInitializeEvent(&Event, NotificationEvent, FALSE);

    Irp = IoBuildDeviceIoControlRequest(IOCTL_INMAGE_MSCS_DISK_RELEASE,
                    DriverContext.InMageFilterDO,
#if (NTDDI_VERSION >= NTDDI_VISTA)
                    DiskInfo,
                    sizeof(CDSKFL_INFO),
#else
                    &ulDiskSignature,
                    sizeof(ULONG),
#endif
                    NULL,
                    0,
                    FALSE,
                    &Event,
                    &IoStatusBlock);
    if (NULL == Irp) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Status = IoCallDriver(DriverContext.InMageFilterDO, Irp);
    if (STATUS_PENDING == Status) {
        KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
        Status = IoStatusBlock.Status;
    }

    ASSERT(STATUS_SUCCESS == Status);
    return Status;
}

NTSTATUS
NotifyInMageFilterOfDeviceDetach(
#if (NTDDI_VERSION >= NTDDI_VISTA)
                                  PCDSKFL_INFO DiskInfo
#else
                                  ULONG ulDiskSignature
#endif
                                  )
{
    KEVENT          Event;
    PIRP            Irp;
    IO_STATUS_BLOCK IoStatusBlock;
    NTSTATUS        Status;

    if (NULL == DriverContext.InMageFilterDO)
        return STATUS_INVALID_PARAMETER;

    KeInitializeEvent(&Event, NotificationEvent, FALSE);

    Irp = IoBuildDeviceIoControlRequest(IOCTL_INMAGE_MSCS_DISK_REMOVED_FROM_CLUSTER,
                    DriverContext.InMageFilterDO,
#if (NTDDI_VERSION >= NTDDI_VISTA)
                    DiskInfo,
                    sizeof(CDSKFL_INFO),
#else
                    &ulDiskSignature,
                    sizeof(ULONG),
#endif
                    NULL,
                    0,
                    FALSE,
                    &Event,
                    &IoStatusBlock);
    if (NULL == Irp) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Status = IoCallDriver(DriverContext.InMageFilterDO, Irp);
    if (STATUS_PENDING == Status) {
        KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
        Status = IoStatusBlock.Status;
    }

    ASSERT(STATUS_SUCCESS == Status);
    return Status;
}

NTSTATUS
NotifyInMageFilterOfDeviceAttach(ULONG ulDiskSignature)
{
    KEVENT          Event;
    PIRP            Irp;
    IO_STATUS_BLOCK IoStatusBlock;
    NTSTATUS        Status;

    if (NULL == DriverContext.InMageFilterDO)
        return STATUS_INVALID_PARAMETER;

    KeInitializeEvent(&Event, NotificationEvent, FALSE);

    Irp = IoBuildDeviceIoControlRequest(IOCTL_INMAGE_MSCS_DISK_ADDED_TO_CLUSTER,
                    DriverContext.InMageFilterDO,
                    &ulDiskSignature,
                    sizeof(ULONG),
                    NULL,
                    0,
                    FALSE,
                    &Event,
                    &IoStatusBlock);
    if (NULL == Irp) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Status = IoCallDriver(DriverContext.InMageFilterDO, Irp);
    if (STATUS_PENDING == Status) {
        KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
        Status = IoStatusBlock.Status;
    }

    ASSERT(STATUS_SUCCESS == Status);
    return Status;
}

NTSTATUS
NotifyInMageFilterOfDeviceAcquire(
#if (NTDDI_VERSION >= NTDDI_VISTA)
                                  PCDSKFL_INFO DiskInfo
#else
                                  ULONG ulDiskSignature
#endif
                                  )
{
    KEVENT          Event;
    PIRP            Irp;
    IO_STATUS_BLOCK IoStatusBlock;
    NTSTATUS        Status;

    if (NULL == DriverContext.InMageFilterDO)
        return STATUS_INVALID_PARAMETER;

    KeInitializeEvent(&Event, NotificationEvent, FALSE);

    Irp = IoBuildDeviceIoControlRequest(IOCTL_INMAGE_MSCS_DISK_ACQUIRE,
                    DriverContext.InMageFilterDO,
#if (NTDDI_VERSION >= NTDDI_VISTA)
                    DiskInfo,
                    sizeof(CDSKFL_INFO),
#else
                    &ulDiskSignature,
                    sizeof(ULONG),
#endif
                    NULL,
                    0,
                    FALSE,
                    &Event,
                    &IoStatusBlock);
    if (NULL == Irp) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Status = IoCallDriver(DriverContext.InMageFilterDO, Irp);
    if (STATUS_PENDING == Status) {
        KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
        Status = IoStatusBlock.Status;
    }

    ASSERT(STATUS_SUCCESS == Status);
    return Status;
}

NTSTATUS
GetInMageClusterOnlineDisks(S_GET_DISKS *sGetDisks, ULONG Size)
{
    KEVENT          Event;
    PIRP            Irp;
    IO_STATUS_BLOCK IoStatusBlock;
    NTSTATUS        Status;

    if (NULL == DriverContext.InMageFilterDO)
        return STATUS_INVALID_PARAMETER;

    KeInitializeEvent(&Event, NotificationEvent, FALSE);

    Irp = IoBuildDeviceIoControlRequest(IOCTL_INMAGE_MSCS_GET_ONLINE_DISKS,
                    DriverContext.InMageFilterDO,
                    (PVOID) sGetDisks,
                    Size,
                    (PVOID) sGetDisks,
                    Size,
                    FALSE,
                    &Event,
                    &IoStatusBlock);
    if (NULL == Irp) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Status = IoCallDriver(DriverContext.InMageFilterDO, Irp);
    if (STATUS_PENDING == Status) {
        KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
        Status = IoStatusBlock.Status;
    }

    ASSERT(STATUS_SUCCESS == Status);
    return Status;
}

/*++

Routine Description:

    Installable driver initialization entry point.
    This entry point is called directly by the I/O manager to set up the driver

Arguments:

    DriverObject - Inmage volume filter driver object.

    RegistryPath - pointer to a unicode string representing the path,
                   to driver-specific key in the registry.

Return Value:

    STATUS_SUCCESS if successful

--*/

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
{

    ULONG               ulIndex;
    PDRIVER_DISPATCH  * dispatch;
    NTSTATUS            Status;

    PFILE_OBJECT        ClusDisk0FileObject = NULL;
    PDEVICE_OBJECT      ClusDisk0PDO = NULL;
    UNICODE_STRING      uClusDisk0;

    PFILE_OBJECT        InMageFilterFO = NULL;
    PDEVICE_OBJECT      InMageFilterDO = NULL;
    UNICODE_STRING      uInMageFilter;

    DriverObject->DriverUnload                          = InCDskFlUnload;

    //
    //Initialize with bypass mode entry points
    //
    DriverObject->MajorFunction[IRP_MJ_CREATE]          = InCDskFlDispatchInBypassMode;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]           = InCDskFlDispatchInBypassMode;

	Status = BypassCheckBootIni();
 
    if (STATUS_SUCCESS == Status)
    {
        InCDskDbgPrint(DL_WARNING, DM_DRIVER_INIT, ("InCDskFl: Bypassing InCDskFl cluster filter driver as involflt is loaded in bypass mode\n"));
        InMageCDskFltLogError(DriverObject,INCDSKFL_DRIVER_IN_BYPASS_MODE_DUE_TO_BOOT_INI, STATUS_SUCCESS);
        return Status;
	} else if (STATUS_INSUFFICIENT_RESOURCES == Status) {
		return Status;
	}

    DriverContext.DriverRegPath.MaximumLength = RegistryPath->Length
                                            + sizeof(UNICODE_NULL);
    DriverContext.DriverRegPath.Buffer = (PWSTR)ExAllocatePoolWithTag(
                                    PagedPool,
                                    DriverContext.DriverRegPath.MaximumLength,
                                    MEM_TAG_GENERIC_PAGED);
    if (DriverContext.DriverRegPath.Buffer != NULL)
    {
        RtlCopyUnicodeString(&DriverContext.DriverRegPath, RegistryPath);
    } else {
        DriverContext.DriverRegPath.Length = 0;
        DriverContext.DriverRegPath.MaximumLength = 0;
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    //
    //Open ClusDisk0 object
    //
    RtlInitUnicodeString(&uClusDisk0, CLUSTER_DEVICE_NAME);
    Status = IoGetDeviceObjectPointer(&uClusDisk0, FILE_READ_ATTRIBUTES, 
                    &ClusDisk0FileObject, &ClusDisk0PDO);
    
    if (NT_SUCCESS(Status)) 
    {
        if (NULL != ClusDisk0PDO) {
            InCDskDbgPrint(DL_WARNING, DM_DRIVER_INIT, (" Referencing the ClusDisk PDO \n"));
            ObReferenceObject(ClusDisk0PDO);
        }
        if (NULL != ClusDisk0FileObject) {
            InCDskDbgPrint(DL_WARNING, DM_DRIVER_INIT, (" Dereferencing the ClusDisk FileObject \n"));
            ObDereferenceObject(ClusDisk0FileObject);
            ClusDisk0FileObject = NULL;
        }
    }
    if (!NT_SUCCESS(Status)) 
    {
        InCDskDbgPrint(DL_WARNING, DM_DRIVER_INIT, ("InCDskFl: Bypassing InCDskFl cluster filter driver as no cluster driver found %x \n", Status));        
#if (NTDDI_VERSION >= NTDDI_VISTA)
        IoRegisterDriverReinitialization(DriverObject,Reinitialize,NULL);
#else		
        ExFreePool(DriverContext.DriverRegPath.Buffer);
        DriverContext.DriverRegPath.Buffer = NULL;
		InMageCDskFltLogError(DriverObject,INCDSKFL_DRIVER_IN_BYPASS_MODE_NO_CLUSDISK, Status);
#endif
        return STATUS_SUCCESS;
    }

    //
    //Open InMageFilter Control Device
    //
    RtlInitUnicodeString(&uInMageFilter, INMAGE_DEVICE_NAME);
    Status = IoGetDeviceObjectPointer(&uInMageFilter, GENERIC_ALL, 
                    &InMageFilterFO, &InMageFilterDO);
    
    if (!NT_SUCCESS(Status)) 
    {
        InCDskDbgPrint(DL_WARNING, DM_DRIVER_INIT, ("InCDskFl: Bypassing InCDskFl cluster filter driver as no InmageFilter driver found\n"));
        InMageCDskFltLogError(DriverObject,INCDSKFL_DRIVER_IN_BYPASS_MODE_NO_INVOLFLT, Status);
        ExFreePool(DriverContext.DriverRegPath.Buffer);
        DriverContext.DriverRegPath.Buffer = NULL;

        if (NULL != ClusDisk0PDO)
            ObDereferenceObject(ClusDisk0PDO);

         return STATUS_SUCCESS;
    }

    //
    // Create dispatch points
    //
    for (ulIndex = 0, dispatch = DriverObject->MajorFunction;
         ulIndex <= IRP_MJ_MAXIMUM_FUNCTION;
         ulIndex++, dispatch++) {

        *dispatch = InCDskFlSendToNextDriver;
    }

    //
    // Set up the device driver entry points.
    //
    
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]  = InCDskFlDeviceControl;

    Status = InitializeDriverContext(
               DriverObject,
               ClusDisk0PDO,
               InMageFilterFO, 
               InMageFilterDO
               );
    if (STATUS_SUCCESS != Status)
    {
        InCDskDbgPrint(DL_ERROR, DM_DRIVER_INIT, ("InCDskFl: Error during initialization of driver context %x\n", Status));
        InMageCDskFltLogError(DriverObject,INCDSKFL_DRIVER_LOAD_FAILURE, Status);
        CleanDriverContext();
        return Status;
    }

    return(STATUS_SUCCESS);

} // end DriverEntry()

#if (NTDDI_VERSION >= NTDDI_VISTA)
VOID Reinitialize( 
    IN PDRIVER_OBJECT  DriverObject, 
    IN PVOID  Context, 
    IN ULONG  ReinitializeCount 
    )
{
    ULONG               ulIndex;
    PDRIVER_DISPATCH  * dispatch;
    NTSTATUS            Status;

    PFILE_OBJECT        ClusDisk0FileObject = NULL;
    PDEVICE_OBJECT      ClusDisk0PDO = NULL;
    UNICODE_STRING      uClusDisk0;

    PFILE_OBJECT        InMageFilterFO = NULL;
    PDEVICE_OBJECT      InMageFilterDO = NULL;
    UNICODE_STRING      uInMageFilter;
    UNREFERENCED_PARAMETER(Context);
    
    //
    //Open ClusDisk0 object
    //
    RtlInitUnicodeString(&uClusDisk0, CLUSTER_DEVICE_NAME);
    Status = IoGetDeviceObjectPointer(&uClusDisk0, FILE_READ_ATTRIBUTES, 
                    &ClusDisk0FileObject, &ClusDisk0PDO);
    if (NT_SUCCESS(Status)) 
    {
        if (NULL != ClusDisk0PDO) {
            InCDskDbgPrint(DL_WARNING, DM_DRIVER_INIT, (" Referencing the ClusDisk PDO \n"));
            ObReferenceObject(ClusDisk0PDO);
        }
        if (NULL != ClusDisk0FileObject) {
            InCDskDbgPrint(DL_WARNING, DM_DRIVER_INIT, (" Dereferencing the ClusDisk FileObject \n"));
            ObDereferenceObject(ClusDisk0FileObject);
            ClusDisk0FileObject = NULL;
        }
    }

    if (!NT_SUCCESS(Status)) 
    {
        InCDskDbgPrint(DL_WARNING, DM_DRIVER_INIT, ("InCDskFl: Bypassing InCDskFl cluster filter driver as no cluster driver found\n"));
        
        if(ReinitializeCount <= MAX_REINITIALIZE_COUNT)
            IoRegisterDriverReinitialization(DriverObject,Reinitialize,NULL);
        else
        {
            ExFreePool(DriverContext.DriverRegPath.Buffer);
            DriverContext.DriverRegPath.Buffer = NULL;
            InMageCDskFltLogError(DriverObject, INCDSKFL_DRIVER_IN_BYPASS_MODE_NO_CLUSDISK, Status);
        }

        return ;
    }

    //
    //Open InMageFilter Control Device
    //
    RtlInitUnicodeString(&uInMageFilter, INMAGE_DEVICE_NAME);
    Status = IoGetDeviceObjectPointer(&uInMageFilter, GENERIC_ALL, 
                    &InMageFilterFO, &InMageFilterDO);
    
    if (!NT_SUCCESS(Status)) 
    {
        InCDskDbgPrint(DL_WARNING, DM_DRIVER_INIT, ("InCDskFl: Bypassing InCDskFl cluster filter driver as no InmageFilter driver found\n"));
        InMageCDskFltLogError(DriverObject,INCDSKFL_DRIVER_IN_BYPASS_MODE_NO_INVOLFLT, Status);
        ExFreePool(DriverContext.DriverRegPath.Buffer);
        DriverContext.DriverRegPath.Buffer = NULL;
        if (NULL != ClusDisk0PDO)
            ObDereferenceObject(ClusDisk0PDO);
        return ;
    }

    //
    // Create dispatch points
    //
    for (ulIndex = 0, dispatch = DriverObject->MajorFunction;
         ulIndex <= IRP_MJ_MAXIMUM_FUNCTION;
         ulIndex++, dispatch++) {

        *dispatch = InCDskFlSendToNextDriver;
    }

    //
    // Set up the device driver entry points.
    //
    
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]  = InCDskFlDeviceControl;

    Status = InitializeDriverContext(
               DriverObject,
               ClusDisk0PDO,
               InMageFilterFO, 
               InMageFilterDO
               );
    if (STATUS_SUCCESS != Status)
    {
    	InCDskDbgPrint(DL_ERROR, DM_DRIVER_INIT, ("InCDskFl: Error during initialization of driver context %x\n", Status));
        InMageCDskFltLogError(DriverObject,INCDSKFL_DRIVER_LOAD_FAILURE, Status);
        CleanDriverContext();
        return ;
    }

    return;
}
#endif

NTSTATUS
InCDskFlIrpCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    Forwarded IRP completion routine. Set an event and return
    STATUS_MORE_PROCESSING_REQUIRED. Irp forwarder will wait on this
    event and then re-complete the irp after cleaning up.

Arguments:

    DeviceObject is the device object of the WMI driver
    Irp is the WMI irp that was just completed
    Context is a PKEVENT that forwarder will wait on

Return Value:

    STATUS_MORE_PORCESSING_REQUIRED

--*/

{
    PKEVENT Event = (PKEVENT) Context;

    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(Irp);

    KeSetEvent(Event, IO_NO_INCREMENT, FALSE);

    return(STATUS_MORE_PROCESSING_REQUIRED);

} // end InCDskFlIrpCompletion()


NTSTATUS
InCDskFlSendToNextDriver(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine sends the Irp to the next driver in line
    when the Irp is not processed by this driver.

Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/

{
    PDEVICE_EXTENSION   DeviceExtension;

    DeviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    IoSkipCurrentIrpStackLocation(Irp);

    return IoCallDriver(DeviceExtension->TargetDeviceObject, Irp);

} // end InCDskFlSendToNextDriver()



/*++

Routine Description:

    This routine sends the Irp to the next driver in line
    when the Irp needs to be processed by the lower drivers
    prior to being processed by this one.

Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/

NTSTATUS
InCDskFlForwardIrpSynchronous(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    PDEVICE_EXTENSION   deviceExtension;
    KEVENT event;
    NTSTATUS status;

    KeInitializeEvent(&event, NotificationEvent, FALSE);
    deviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

    //
    // copy the irpstack for the next device
    //

    IoCopyCurrentIrpStackLocationToNext(Irp);

    //
    // set a completion routine
    //

    IoSetCompletionRoutine(Irp, InCDskFlIrpCompletion,
                            &event, TRUE, TRUE, TRUE);

    //
    // call the next lower device
    //

    status = IoCallDriver(deviceExtension->TargetDeviceObject, Irp);

    //
    // wait for the actual completion
    //

    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
        status = Irp->IoStatus.Status;
    }

    return status;

} // end InCDskFlForwardIrpSynchronous()

/*++

Routine Description:

    This routine services commands in bypass mode. 
    In bypass mode this is not a filter driver so we
    just complete the Irps.

Arguments:

    DeviceObject - Context for the activity.
    Irp          - The device control argument block.

Return Value:

    NT Status

--*/

NTSTATUS
InCDskFlDispatchInBypassMode(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(DeviceObject);

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = STATUS_SUCCESS;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;

} // InCDskFlDispatchInBypassMode()

VOID
GetFsContext(ULONG Signature, PDEVICE_OBJECT TargetDeviceObject, PVOID *FsContext)
{
    ULONG Length = 4;
    IO_STATUS_BLOCK IoStatBlock;
    KEVENT Event;
    FILE_OBJECT NewFileObject = {0};
    PIRP NewIrp = NULL;
    PIO_STACK_LOCATION  IoNextStackLocation = NULL;
    NTSTATUS NewStatus = STATUS_SUCCESS;

    *FsContext = NULL;

    KeInitializeEvent(&Event, NotificationEvent, FALSE);
    NewIrp = IoBuildDeviceIoControlRequest(
                            IOCTL_CLUSTER_ACQUIRE_SCSI_DEVICE,
                            TargetDeviceObject,
                            (void *) &Signature,
                            Length,
                            NULL,
                            0,
                            FALSE,
                            &Event,
                            &IoStatBlock);
    if (NULL == NewIrp) {
        return;
    }

    IoNextStackLocation = IoGetNextIrpStackLocation(NewIrp);
    IoNextStackLocation->FileObject = &NewFileObject;
    IoNextStackLocation->FileObject->DeviceObject = TargetDeviceObject;
    IoNextStackLocation->FileObject->FsContext = NULL;
         
    NewStatus = IoCallDriver(TargetDeviceObject, NewIrp);
    if (STATUS_PENDING == NewStatus) {
        KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
        NewStatus = IoStatBlock.Status;
    }
    ASSERT(STATUS_SUCCESS == NewStatus);

    if (NewFileObject.FsContext != NULL) {
        *FsContext = NewFileObject.FsContext;
    }

    return;
}        

VOID
GetAllFsContext(PDEVICE_OBJECT TargetDeviceObject)
{
    PVOID NewFsContext = NULL;
    NTSTATUS Status = STATUS_SUCCESS;
    S_GET_DISKS *sGetDisks = NULL;
    ULONG i = 0;
    ULONG SignatureArrSize = 0;
    ULONG *SignatureArr = NULL;
    ULONG DiskSignaturesInputArrSize = 0;
    ULONG BytesNeedToAllocate = 0;

    DiskSignaturesInputArrSize = GetNumClusterDisks();
    if (DiskSignaturesInputArrSize == 0) {
        return;
    }

    BytesNeedToAllocate = DiskSignaturesInputArrSize * sizeof(ULONG) + sizeof (S_GET_DISKS);
    sGetDisks = (S_GET_DISKS *) ExAllocatePoolWithTag(
                                    PagedPool,
                                    BytesNeedToAllocate,
                                    MEM_TAG_GENERIC_PAGED);
    if (sGetDisks == NULL) {
        return;
    }
    RtlZeroMemory(sGetDisks, BytesNeedToAllocate);

    sGetDisks->DiskSignaturesInputArrSize = DiskSignaturesInputArrSize;

    // Issue an IOCTL to involflt and get signature of all disks that are accessible/READable
    // to this host. These are the devices that are ONLINE/reserved on/by this host.
    Status = GetInMageClusterOnlineDisks(sGetDisks, BytesNeedToAllocate);

    if (STATUS_SUCCESS != Status)
        goto Out;
    
    if (sGetDisks->IsVolumeAddedByEnumeration == false)
        goto Out; // Nothing to do as this is not a "NO-REBOOT" case
        
    ASSERT(sGetDisks->DiskSignaturesInputArrSize >= sGetDisks->DiskSignaturesOutputArrSize);

    //
    // Get FsContext for given disk signatures
    //
    for (i = 0; i < sGetDisks->DiskSignaturesOutputArrSize; i++) {
        NewFsContext = NULL;
        // check if mapping for "sGetDisks.DiskSignatures[i]" is already present
        PCLUSDISK_NODE pClusDiskNode = NULL;
        pClusDiskNode = GetClusDiskNodeFromSignature(sGetDisks->DiskSignatures[i]);
        if (pClusDiskNode != NULL) {
            DereferenceClusDiskNode(pClusDiskNode);
            continue;
        }

        // Issue an ACQUIRE ioctl to the lower OS cluster driver by passing disk signature.
        // This disk is ONLINE/reserved on this host.
        // Lower OS cluster driver returns, FsContext of the disk.
        GetFsContext(sGetDisks->DiskSignatures[i], TargetDeviceObject, &NewFsContext);
        
        ASSERT(NewFsContext != NULL);
        if (NewFsContext != NULL) {
                Status = AddSignatureToClusDiskList(sGetDisks->DiskSignatures[i], NewFsContext);
        }
    }

Out:    
    if (sGetDisks != NULL)
        ExFreePoolWithTag(sGetDisks, MEM_TAG_GENERIC_PAGED);

    return;
}

#if (NTDDI_VERSION >= NTDDI_VISTA)
NTSTATUS
HandleAcqurieSCSIDeviceIOCTL(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    PDEVICE_EXTENSION   DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION  pIoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulIoControlCode;
    NTSTATUS            Status, Status2;
    CDSKFL_INFO         cDiskInfo;
    UNICODE_STRING      GuidStr;

    ulIoControlCode = pIoStackLocation->Parameters.DeviceIoControl.IoControlCode;
    ASSERT(IOCTL_CLUSTER_ACQUIRE_SCSI_DEVICE == ulIoControlCode);
    ULONG Len = pIoStackLocation->Parameters.DeviceIoControl.InputBufferLength;

    if ( (NULL == Irp->AssociatedIrp.SystemBuffer) ||
         (sizeof(START_RESERVE) > pIoStackLocation->Parameters.DeviceIoControl.InputBufferLength) )
    {
        Status = InCDskFlSendToNextDriver(DeviceObject, Irp);
        return Status;
    }

    PSTART_RESERVE      StartReserve = (PSTART_RESERVE)Irp->AssociatedIrp.SystemBuffer;

    // Win2K8 uses file object to identify the disk reserve structure.
    // Before Win2K8 - FileObject->FsContext was used.
    if (((sizeof(START_RESERVE) == StartReserve->StructSize)|| ((sizeof(START_RESERVE) + SupportBytes) == StartReserve->StructSize))&&
        (NULL != pIoStackLocation->FileObject))
    {
        if (ecPartitionStyleMBR == StartReserve->DiskInfo.ePartitionStyle) {
            cDiskInfo.ePartitionStyle = ecPartStyleMBR;
            cDiskInfo.ulDiskSignature = StartReserve->DiskInfo.ulSignature;
        } else if (ecPartitionStyleGPT == StartReserve->DiskInfo.ePartitionStyle) {
            cDiskInfo.ePartitionStyle = ecPartStyleGPT;
            RtlCopyMemory((void *)&cDiskInfo.DiskId, &StartReserve->DiskInfo.Guid, sizeof(GUID));
        }
        cDiskInfo.DeviceNumber = StartReserve->DeviceNumber;
        Status = InCDskFlForwardIrpSynchronous(DeviceObject, Irp);
        if (NT_SUCCESS(Status)) {
            // Add this signature to Clusdisk list
            if (ecPartitionStyleMBR == StartReserve->DiskInfo.ePartitionStyle)
                Status2 = AddSignatureToClusDiskList(cDiskInfo.ulDiskSignature, pIoStackLocation->FileObject);
            else if (ecPartitionStyleGPT == StartReserve->DiskInfo.ePartitionStyle)
                Status2 = AddDiskGUIDToClusDiskList(cDiskInfo.DiskId, pIoStackLocation->FileObject);
            ASSERT((STATUS_SUCCESS == Status2) || (STATUS_DUPLICATE_OBJECTID == Status2));
            if (STATUS_SUCCESS == Status2) {
                NotifyInMageFilterOfDeviceAcquire(&cDiskInfo);
            }
            if (cDiskInfo.ePartitionStyle == ecPartStyleMBR) {            
                InCDskDbgPrint(DL_INFO, DM_VOLUME_ACQUIRE, ("Acquire ClusDiskNode Sig = 0x%x, FileObject = %p, Status = 0x%x\n", 
                            cDiskInfo.ulDiskSignature, pIoStackLocation->FileObject, Status));
            } else if (cDiskInfo.ePartitionStyle == ecPartitionStyleGPT) {
                InCDskDbgPrint(DL_INFO, DM_VOLUME_ACQUIRE, ("Acquire ClusDiskNode for "));
                if (STATUS_SUCCESS == RtlStringFromGUID(cDiskInfo.DiskId, &GuidStr)) {
                    InCDskDbgPrint(DL_INFO, DM_VOLUME_ACQUIRE, ("Disk GUID %wZ \n", GuidStr));
                    RtlFreeUnicodeString(&GuidStr);
                }
                InCDskDbgPrint(DL_INFO, DM_VOLUME_ACQUIRE, ("\nFileObject = %p, Status = 0x%x\n", pIoStackLocation->FileObject, Status));
            }
        }
        
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    } else {
        Status = InCDskFlSendToNextDriver(DeviceObject, Irp);
    }

    return Status;
}

NTSTATUS
HandleReleaseSCSIDeviceIOCTL(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    PDEVICE_EXTENSION   DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION  pIoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulIoControlCode = pIoStackLocation->Parameters.DeviceIoControl.IoControlCode;
    NTSTATUS            Status;
    CDSKFL_INFO         cDiskInfo;

    ASSERT(IOCTL_CLUSTER_RELEASE_SCSI_DEVICE == ulIoControlCode);

    if (NULL != pIoStackLocation->FileObject) {
        PCLUSDISK_NODE      pClusDiskNode = GetClusDiskNodeFromContext(pIoStackLocation->FileObject);
        ASSERT(NULL != pClusDiskNode);

        if (NULL != pClusDiskNode) {
            InCDskDbgPrint(DL_INFO, DM_VOLUME_RELEASE, 
                           ("Release ClusDiskNode Sig = 0x%x, FileObject = %p\n", pClusDiskNode->ulDiskSignature, pClusDiskNode->Context));
            if (ecPartitionStyleMBR == pClusDiskNode->ePartitionStyle) {
                cDiskInfo.ePartitionStyle = ecPartStyleMBR;
                cDiskInfo.ulDiskSignature = pClusDiskNode->ulDiskSignature;
            } else if ((ecPartitionStyleGPT == pClusDiskNode->ePartitionStyle)) {
                cDiskInfo.ePartitionStyle = ecPartStyleGPT;
                RtlCopyMemory(&cDiskInfo.DiskId, &pClusDiskNode->DiskGuid, sizeof(GUID));
            }
            Status = NotifyInMageFilterOfDeviceRelease(&cDiskInfo);
            ASSERT(STATUS_SUCCESS == Status);
            if (ecPartitionStyleMBR == pClusDiskNode->ePartitionStyle)
                Status = RemoveClusDiskFromClusDiskList(pClusDiskNode->ulDiskSignature);
            else if (ecPartitionStyleGPT == pClusDiskNode->ePartitionStyle)
                Status = RemoveClusGPTDiskFromClusDiskList(pClusDiskNode->DiskGuid);
            ASSERT(STATUS_SUCCESS == Status);
            DereferenceClusDiskNode(pClusDiskNode);
        }        
    }

    Status = InCDskFlSendToNextDriver(DeviceObject, Irp);

    return Status;
}

#else

NTSTATUS
HandleAcqurieSCSIDeviceIOCTL(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    PDEVICE_EXTENSION   DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION  pIoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulIoControlCode, ulDiskSignature;
    NTSTATUS            Status, Status2;
    PCLUSDISK_NODE  pClusDiskNode;

    ulIoControlCode = pIoStackLocation->Parameters.DeviceIoControl.IoControlCode;
    ASSERT(IOCTL_CLUSTER_ACQUIRE_SCSI_DEVICE == ulIoControlCode);

    if ( (NULL == Irp->AssociatedIrp.SystemBuffer) ||
         (sizeof(ULONG) > pIoStackLocation->Parameters.DeviceIoControl.InputBufferLength) )
    {
        Status = InCDskFlSendToNextDriver(DeviceObject, Irp);
        return Status;
    }

    if ( (NULL != pIoStackLocation->FileObject) &&
         (NULL == pIoStackLocation->FileObject->FsContext) )
    {
        // FsContext is NULL. So let us pass the Irp down.
        ulDiskSignature = *(ULONG *)Irp->AssociatedIrp.SystemBuffer;

        Status = InCDskFlForwardIrpSynchronous(DeviceObject, Irp);
        if (NT_SUCCESS(Status)) {
            // Add this signature to Clusdisk list
            ASSERT(NULL != pIoStackLocation->FileObject->FsContext);

            pClusDiskNode = GetClusDiskNodeFromSignature(ulDiskSignature);
            if (NULL != pClusDiskNode) {
                // If context did not match, may be we have to update the context??
                DereferenceClusDiskNode(pClusDiskNode);
                if (pClusDiskNode->State == DISK_RELEASED) {
                    pClusDiskNode->State = DISK_ACQUIRED;
                    ASSERT(pIoStackLocation->FileObject->FsContext == pClusDiskNode->Context);
                    if (pIoStackLocation->FileObject->FsContext != pClusDiskNode->Context)
                        pClusDiskNode->Context = pIoStackLocation->FileObject->FsContext;
                    Status2 = STATUS_SUCCESS;
                } else {
                    Status2 = STATUS_DUPLICATE_OBJECTID;
                }
            } else {
                Status2 = AddSignatureToClusDiskList(ulDiskSignature,
                                pIoStackLocation->FileObject->FsContext);
                ASSERT((STATUS_SUCCESS == Status2) || (STATUS_DUPLICATE_OBJECTID == Status2));
            }
            if (STATUS_SUCCESS == Status2) {
                NotifyInMageFilterOfDeviceAcquire(ulDiskSignature);
            }

            InCDskDbgPrint(DL_INFO, DM_VOLUME_ACQUIRE, ("Acquire ClusDiskNode Sig = 0x%x, Context = %p, Status = 0x%x\n", ulDiskSignature, 
                                    pIoStackLocation->FileObject->FsContext, Status));
        }
        
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    } else {
        Status = InCDskFlSendToNextDriver(DeviceObject, Irp);
    }

    return Status;
}

NTSTATUS
HandleReleaseSCSIDeviceIOCTL(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    PDEVICE_EXTENSION   DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION  pIoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulIoControlCode, ulDiskSignature;
    NTSTATUS            Status;
    PVOID               FsContext;
    PCLUSDISK_NODE      pClusDiskNode;

    ulIoControlCode = pIoStackLocation->Parameters.DeviceIoControl.IoControlCode;
    ASSERT(IOCTL_CLUSTER_RELEASE_SCSI_DEVICE == ulIoControlCode);

    if ( (NULL != pIoStackLocation->FileObject) &&
         (NULL != pIoStackLocation->FileObject->FsContext) )
    {
        FsContext = pIoStackLocation->FileObject->FsContext;

        pClusDiskNode = GetClusDiskNodeFromContext(FsContext);
        if (pClusDiskNode == NULL) {
            GetAllFsContext(DeviceExtension->TargetDeviceObject);
            pClusDiskNode = GetClusDiskNodeFromContext(FsContext);
        }
        // ASSERT(NULL != pClusDiskNode);

        if (NULL != pClusDiskNode && pClusDiskNode->State != DISK_RELEASED)
        {
            pClusDiskNode->State = DISK_RELEASED;
            InCDskDbgPrint(DL_INFO, DM_VOLUME_RELEASE, ("Release ClusDiskNode Sig = 0x%x, Context = %p\n", pClusDiskNode->ulDiskSignature, pClusDiskNode->Context));

            Status = NotifyInMageFilterOfDeviceRelease(pClusDiskNode->ulDiskSignature);
            ASSERT(STATUS_SUCCESS == Status);
        }
        if (NULL != pClusDiskNode) {
            DereferenceClusDiskNode(pClusDiskNode);
        }

    }

    Status = InCDskFlSendToNextDriver(DeviceObject, Irp);

    return Status;
}

#endif

NTSTATUS
HandleDetachClusterDisk(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    PDEVICE_EXTENSION   DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION  pIoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulIoControlCode;
    NTSTATUS            Status;
#if (NTDDI_VERSION >= NTDDI_VISTA)
    PDISK_INFO          DiskInfo;
    CDSKFL_INFO         cDiskInfo;
#else
    ULONG               ulDiskSignature;
#endif
    UNICODE_STRING      GuidStr;

    ulIoControlCode = pIoStackLocation->Parameters.DeviceIoControl.IoControlCode;
    ASSERT(IOCTL_CLUSTER_DETACH == ulIoControlCode);

    if ( (NULL == Irp->AssociatedIrp.SystemBuffer) ||
         (sizeof(ULONG) > pIoStackLocation->Parameters.DeviceIoControl.InputBufferLength) )
    {
        InCDskDbgPrint(DL_WARNING, DM_DISK_DETACH, ("Detach ClusDiskNode Sig with SystemBuffer = %#p, InputSize = %#x\n", 
                Irp->AssociatedIrp.SystemBuffer, pIoStackLocation->Parameters.DeviceIoControl.InputBufferLength));
        Status = InCDskFlSendToNextDriver(DeviceObject, Irp);
        return Status;
    }
#if (NTDDI_VERSION >= NTDDI_VISTA)
    DiskInfo = (PDISK_INFO)Irp->AssociatedIrp.SystemBuffer;
    if (ecPartitionStyleMBR == DiskInfo->ePartitionStyle) {
        cDiskInfo.ePartitionStyle = ecPartStyleMBR;
        cDiskInfo.ulDiskSignature = DiskInfo->ulSignature;
    } else if (ecPartitionStyleGPT == DiskInfo->ePartitionStyle) {
        cDiskInfo.ePartitionStyle = ecPartStyleGPT;
        RtlCopyMemory((void *)&cDiskInfo.DiskId, &DiskInfo->Guid, sizeof(GUID));
    }
    Status = NotifyInMageFilterOfDeviceDetach(&cDiskInfo);
    if (STATUS_SUCCESS != Status) {
        UCHAR *tempBuf;
        if (ecPartitionStyleMBR == DiskInfo->ePartitionStyle) {
            tempBuf =(UCHAR*)DiskInfo + sizeof(ULONG);
            InCDskDbgPrint(DL_WARNING, DM_DISK_DETACH, ("NotifyInMageFilterOfDeviceDetach return Error Status = %#x, for DiskSig = %#x\n",
                                        Status, *tempBuf));
        } else if (ecPartitionStyleGPT == DiskInfo->ePartitionStyle) {
            InCDskDbgPrint(DL_WARNING, DM_DISK_DETACH, ("NotifyInMageFilterOfDeviceDetach return Error Status = %#x, for ",
                                        Status));
            if (STATUS_SUCCESS == RtlStringFromGUID(cDiskInfo.DiskId, &GuidStr)) {
                InCDskDbgPrint(DL_WARNING, DM_VOLUME_ACQUIRE, ("Disk GUID %wZ \n", GuidStr));
                RtlFreeUnicodeString(&GuidStr);
            }
            InCDskDbgPrint(DL_WARNING, DM_DISK_DETACH, ("\n"));
        }
    }
#else
    ulDiskSignature = *(ULONG *)Irp->AssociatedIrp.SystemBuffer;
    InCDskDbgPrint(DL_INFO, DM_DISK_DETACH, ("Detach ClusDiskNode Sig = %#x\n", ulDiskSignature));
    Status = NotifyInMageFilterOfDeviceDetach(ulDiskSignature);
    if (STATUS_SUCCESS != Status) {
        InCDskDbgPrint(DL_WARNING, DM_DISK_DETACH, ("NotifyInMageFilterOfDeviceDetach return Error Status = %#x, for DiskSig = %#x\n",
                                Status, ulDiskSignature));
    }
#endif

    Status = InCDskFlSendToNextDriver(DeviceObject, Irp);

    return Status;
}

NTSTATUS
HandleAttachDisk(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    PDEVICE_EXTENSION   DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION  pIoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulIoControlCode, ulDiskSignature;
    NTSTATUS            Status, Status2;

    ulIoControlCode = pIoStackLocation->Parameters.DeviceIoControl.IoControlCode;
    ASSERT((IOCTL_CLUSTER_ATTACH_EX == ulIoControlCode) ||
           (IOCTL_CLUSTER_ATTACH == ulIoControlCode));

    if ( (NULL == Irp->AssociatedIrp.SystemBuffer) ||
         (sizeof(ULONG) > pIoStackLocation->Parameters.DeviceIoControl.InputBufferLength) )
    {
        InCDskDbgPrint(DL_WARNING, DM_DISK_ATTACH, ("Attach ClusDiskNode Sig with SystemBuffer = %#p, InputSize = %#x\n", 
                Irp->AssociatedIrp.SystemBuffer, pIoStackLocation->Parameters.DeviceIoControl.InputBufferLength));
        Status = InCDskFlSendToNextDriver(DeviceObject, Irp);
        return Status;
    }

    ulDiskSignature = *(ULONG *)Irp->AssociatedIrp.SystemBuffer;
    InCDskDbgPrint(DL_INFO, DM_DISK_ATTACH, ("HandleAttachDisk: Disk Sig = %#x\n", ulDiskSignature));
    Status = InCDskFlSendToNextDriver(DeviceObject, Irp);

    if (NT_SUCCESS(Status)) {
        Status2 = NotifyInMageFilterOfDeviceAttach(ulDiskSignature);
        ASSERT(STATUS_SUCCESS == Status2);
        if (STATUS_SUCCESS != Status2) {
            InCDskDbgPrint(DL_WARNING, DM_DISK_ATTACH, ("NotifyInMageFilterOfDeviceAttach return Error Status = %#x, for DiskSig = %#x\n",
                                Status2, ulDiskSignature));
        }
    }

    return Status;
}

/*++

Routine Description:

    This device control dispatcher handles only the disk performance
    device control. All others are passed down to the disk drivers.
    The disk performane device control returns a current snapshot of
    the performance data.

Arguments:

    DeviceObject - Context for the activity.
    Irp          - The device control argument block.

Return Value:

    Status is returned.

--*/

NTSTATUS
InCDskFlDeviceControl(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    PDEVICE_EXTENSION   DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION  pIoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulIoControlCode;
    NTSTATUS            Status;

    DumpIoctlInformation(DeviceObject, Irp);

    ulIoControlCode = pIoStackLocation->Parameters.DeviceIoControl.IoControlCode;
    switch (ulIoControlCode) {
        case IOCTL_CLUSTER_ACQUIRE_SCSI_DEVICE:
            Status = HandleAcqurieSCSIDeviceIOCTL(DeviceObject, Irp);
            break;
        case IOCTL_CLUSTER_RELEASE_SCSI_DEVICE:
            Status = HandleReleaseSCSIDeviceIOCTL(DeviceObject, Irp);
            break;
        case IOCTL_CLUSTER_DETACH:
            Status = HandleDetachClusterDisk(DeviceObject, Irp);
            break;
//        case IOCTL_CLUSTER_ATTACH:
//        case IOCTL_CLUSTER_ATTACH_EX:
//            Status = HandleAttachDisk(DeviceObject, Irp);
//            break;
        default:
            //
            // Set current stack back one.
            //
            Status = InCDskFlSendToNextDriver(DeviceObject, Irp);
            break;
    }

    return Status;

} // end InCDskFlDeviceControl()

VOID
InCDskFlUnload(
    IN PDRIVER_OBJECT DriverObject
    )

/*++

Routine Description:

    Free all the allocated resources, etc.

Arguments:

    DriverObject - pointer to a driver object.

Return Value:

    VOID.

--*/
{
    PAGED_CODE();

    return;
}


VOID
InCDskFlLogError(
    IN PDEVICE_OBJECT DeviceObject,
    IN ULONG UniqueId,
    IN NTSTATUS ErrorCode,
    IN NTSTATUS Status
    )

/*++

Routine Description:

    Routine to log an error with the Error Logger

Arguments:

    DeviceObject - the device object responsible for the error
    UniqueId     - an id for the error
    Status       - the status of the error

Return Value:

    None

--*/

{
    PIO_ERROR_LOG_PACKET errorLogEntry;

    errorLogEntry = (PIO_ERROR_LOG_PACKET)
                    IoAllocateErrorLogEntry(
                        DeviceObject,
                        (UCHAR)(sizeof(IO_ERROR_LOG_PACKET) + sizeof(DEVICE_OBJECT))
                        );

    if (errorLogEntry != NULL) {
        errorLogEntry->ErrorCode = ErrorCode;
        errorLogEntry->UniqueErrorValue = UniqueId;
        errorLogEntry->FinalStatus = Status;
        //
        // The following is necessary because DumpData is of type ULONG
        // and DeviceObject can be more than that
        //
        RtlCopyMemory(
            &errorLogEntry->DumpData[0],
            &DeviceObject,
            sizeof(DEVICE_OBJECT));
        errorLogEntry->DumpDataSize = sizeof(DEVICE_OBJECT);
        IoWriteErrorLogEntry(errorLogEntry);
    }
}

ULONG
GetNumClusterDisks(VOID)
{
    NTSTATUS status;
    HANDLE hRegistry;
    UNICODE_STRING keyName;
    OBJECT_ATTRIBUTES attr;
    KEY_FULL_INFORMATION KeyInformation;
    ULONG ResultLength = 0;

    RtlInitUnicodeString(&keyName, REG_CLUSDISK_SIGNATURES_KEY);
    InitializeObjectAttributes(&attr,
        &keyName,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL, NULL);

    status = ZwOpenKey(&hRegistry, GENERIC_READ, &attr);
    if (NT_SUCCESS(status)) {
        status = ZwQueryKey(
                        hRegistry,
                        KeyFullInformation,
                        (PVOID) &KeyInformation,
                        sizeof (KEY_FULL_INFORMATION),
                        &ResultLength
                        );

        if (status == STATUS_SUCCESS) {
            return KeyInformation.SubKeys;
        }
    }

    return 0;
}

NTSTATUS
BypassCheckBootIni()
{
    NTSTATUS status;
    UNICODE_STRING string;
    UNICODE_STRING ucString;
    PWSTR stringBuffer = NULL;
    PWSTR ucStringBuffer = NULL;
    OBJECT_ATTRIBUTES attr;
    HANDLE hRegistry;
    UNICODE_STRING keyName;
    int i;

    RtlInitUnicodeString(&keyName,L"\\Registry\\Machine\\System\\CurrentControlSet\\Control");
    InitializeObjectAttributes(&attr,
        &keyName,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL, NULL);

    status = ZwOpenKey(&hRegistry, GENERIC_READ, &attr);
    
    if (NT_SUCCESS(status)) {
        stringBuffer =(WCHAR *) ExAllocatePoolWithTag(NonPagedPool, 256 * sizeof(WCHAR), MEM_TAG_GENERIC_NONPAGED);
        ucStringBuffer =(WCHAR *) ExAllocatePoolWithTag (NonPagedPool, 256 * sizeof(WCHAR), MEM_TAG_GENERIC_NONPAGED);

        if ((NULL == stringBuffer) || (NULL == ucStringBuffer)) {            
			status = STATUS_INSUFFICIENT_RESOURCES ;
			ZwClose(hRegistry);
            goto Out;
        } else {
            string.Buffer = stringBuffer;
            ucString.Buffer = ucStringBuffer;

            string.Length = ucString.Length = 0;
            string.MaximumLength = ucString.MaximumLength = 256*sizeof(WCHAR);
        }
    } 	else {
        status = STATUS_UNSUCCESSFUL;
        goto Out;
    }
    status = ReadUnicodeString(L"SystemStartOptions", &string, hRegistry);
    ZwClose(hRegistry);

    if (NT_SUCCESS(status)) {
        RtlUpcaseUnicodeString(&ucString, &string, FALSE);
        status = STATUS_UNSUCCESSFUL;

        for(i = 0; i <= ((int)(ucString.Length/sizeof(WCHAR)) - 15);i++) {
            if ((ucString.Buffer[i +  0] == L'I') &&
                (ucString.Buffer[i +  1] == L'N') &&
                (ucString.Buffer[i +  2] == L'V') &&
                (ucString.Buffer[i +  3] == L'O') &&
                (ucString.Buffer[i +  4] == L'L') &&
                (ucString.Buffer[i +  5] == L'F') &&
                (ucString.Buffer[i +  6] == L'L') &&
                (ucString.Buffer[i +  7] == L'T') &&
                (ucString.Buffer[i +  8] == L'=') &&
                (ucString.Buffer[i +  9] == L'B') &&
                (ucString.Buffer[i + 10] == L'Y') &&
                (ucString.Buffer[i + 11] == L'P') &&
                (ucString.Buffer[i + 12] == L'A') &&
                (ucString.Buffer[i + 13] == L'S') &&
                (ucString.Buffer[i + 14] == L'S')) {
                    status = STATUS_SUCCESS;
                    break;
                } else {
                    status = STATUS_UNSUCCESSFUL;
                }
        }
    } else {
        status = STATUS_UNSUCCESSFUL;
    }

Out: 
	    if (NULL != stringBuffer) {
            ExFreePoolWithTag(stringBuffer, MEM_TAG_GENERIC_NONPAGED);
        }
        if(NULL != ucStringBuffer) {
            ExFreePoolWithTag(ucStringBuffer, MEM_TAG_GENERIC_NONPAGED);
        }
    return status;
}


NTSTATUS
ReadUnicodeString(PCWSTR nameString, PUNICODE_STRING value,HANDLE hRegistry)
{
    NTSTATUS status;
    UNICODE_STRING name;
    unsigned long actualSize;
    UCHAR buffer[512];
    PKEY_VALUE_PARTIAL_INFORMATION regValue;

    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    regValue = (PKEY_VALUE_PARTIAL_INFORMATION)buffer;

    RtlInitUnicodeString(&name, nameString);

    status = ZwQueryValueKey(hRegistry,
                              &name, 
                              KeyValuePartialInformation,
                              (PVOID)regValue,
                              sizeof(buffer),
                              &actualSize);
    if ((status == STATUS_SUCCESS) && 
         (regValue->Type == REG_SZ) && 
         (regValue->DataLength <= value->MaximumLength)) 
    {
        RtlMoveMemory((PVOID)value->Buffer,regValue->Data,regValue->DataLength);
        value->Length = (USHORT)regValue->DataLength;
        if ((value->Buffer[((value->Length) / sizeof(WCHAR))-1] == 0) && (value->Length >= 2)) {
            value->Length -= 2; // strip off trailing zero bytes as I/O functions don't like them
        }
    } else if ((status == STATUS_SUCCESS) && (regValue->DataLength <= value->MaximumLength)) {
        status = STATUS_OBJECT_TYPE_MISMATCH;
    } else if ((status == STATUS_SUCCESS) && (regValue->Type == REG_SZ)) { 
        status = STATUS_BUFFER_TOO_SMALL;
    }
        
    return status;
}

VOID InMageCDskFltLogError(IN PDRIVER_OBJECT DriverObject, IN ULONG messageId, NTSTATUS Status)
{
    ULONG packetLength = sizeof(IO_ERROR_LOG_PACKET);
    PIO_ERROR_LOG_PACKET errorLogEntry = (PIO_ERROR_LOG_PACKET) IoAllocateErrorLogEntry(DriverObject, (UCHAR) packetLength);
    errorLogEntry->ErrorCode = messageId;
	errorLogEntry->FinalStatus = Status;
    if (!errorLogEntry)
        return;
    IoWriteErrorLogEntry(errorLogEntry);
}

