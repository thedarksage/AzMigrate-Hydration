/**
 * @file abhaiflt.c
 *
 * Kernel mode driver that captures all block changes to logical and physical volumes.
 * These changes are drained by a user space service, expanded with block data, and written
 * to a target volume.
 *

 */

#define INITGUID

#include "Global.h"
#include "MntMgr.h"
#include "IoctlInfo.h"
#include "mscs.h"
#include "ListNode.h"
#include "misc.h"
#include "DirtyBlock.h"
#include "VBitmap.h"
#include "VContext.h"
#include "TagNode.h"
#include "MetaDataMode.h"
#include "DataFileWriter.h"
#include "Profiler.h"
#include "svdparse.h"
#include "HelperFunctions.h"
#include "..\..\..\common\version.h"

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
InMageFltForwardIrpSynchronous(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
InMageFltAddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PhysicalDeviceObject
    );


NTSTATUS
InMageFltDispatchPnp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
InMageFltDispatchPower(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
InMageFltStartDevice(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
InMageFltRemoveDevice(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
InMageFltSendToNextDriver(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
InMageFltClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
InMageFltCleanup(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
InMageFltCreate(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
InMageFltWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
InMageFltRead(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
InMageFltDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
InMageFltShutdown(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
InMageFltFlush(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
InMageFltUnload(
    IN PDRIVER_OBJECT DriverObject
    );

NTSTATUS
InMageFltIrpCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

VOID
InMageFltSyncFilterWithTarget(
    IN PDEVICE_OBJECT FilterDevice,
    IN PDEVICE_OBJECT TargetDevice
    );

VOID
ServiceStateChangeFunction(
    IN PVOID Context
    );

//Added for Persistent Time Stamp
VOID
PersistentValuesFlush(
    IN PVOID Context
    );




NTSTATUS
InMageFltWriteOnOfflineVolumeCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp,
    IN PVOID          Context
    );

VOID
ClearVolumeStats(PVOLUME_CONTEXT   pVolumeContext);

NTSTATUS
ValidateParameters(SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_INPUT Input,PSET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_OUTPUT pOutputBuffer);

VOID AttachToVolumeWithoutReboot(IN PVOID Context);

VOID GetDeviceNumberAndSignature(PVOLUME_CONTEXT pVolumeContext);

VOID GetClusterInfoForThisVolume(PVOLUME_CONTEXT pVolumeContext);

VOID GetVolumeGuidAndDriveLetter(PVOLUME_CONTEXT pVolumeContext);


PVOLUME_CONTEXT
GetVolumeContextForThisIOCTL(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    );

bool
IsValidIoctlTagVolumeInputBuffer(
    PUCHAR pBuffer,
    ULONG  ulBufferLength
    );

ULONG
GetOnlineDisksSignature(
    S_GET_DISKS  *sDiskSignature
    );

VOID
InMageSetBootVolumeChangeNotificationFlag(
	PVOID pContext
	);

static NTSTATUS
InMageFetchSymbolicLink(
	IN		PUNICODE_STRING		SymbolicLinkName,
	OUT		PUNICODE_STRING		SymbolicLink,
	OUT		PHANDLE				LinkHandle
	);

static NTSTATUS
InMageExtractDriveString(
	IN OUT	PUNICODE_STRING		Source
	);

extern "C" NTSTATUS
BootVolumeDriverNotificationCallback(
    PVOID NotificationStructure,
    PVOID pContext
    );

VOID InMageFltOnReinitialize(IN PDRIVER_OBJECT pDriverObj,
							 IN PVOID          pContext,
							 IN ULONG          ulCount
							 );

NTSTATUS InMageCrossCheckNewVolume(IN PDEVICE_OBJECT pDeviceObject,
							 IN PVOLUME_CONTEXT pVolumeContext,
							 IN PMOUNTDEV_NAME pMountDevName
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
                      DM_DRIVER_INIT | DM_SHUTDOWN | DM_VOLUME_ARRIVAL | 
                      DM_DRIVER_PNP | DM_TIME_STAMP | DM_WO_STATE | 
                      DM_CAPTURE_MODE | DM_BYPASS|DM_IOCTL_TRACE | DM_FILE_OPEN;

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

    ULONG               ulIndex;
    PDRIVER_DISPATCH  * dispatch;
    HANDLE              hThread;
    NTSTATUS            Status;
    PWCHAR              SymbolicLinkList = NULL, Buf = NULL;
    UNICODE_STRING      ustrVolumeName;


    // we want this printed if we are printing anything
    InVolDbgPrint(DL_ERROR, DM_DRIVER_INIT, ("InVolFlt!DriverEntry: entered\nCompiled %s  %s\n", __DATE__,  __TIME__));

    if (BypassCheckBootIni() == STATUS_SUCCESS) {
        InVolDbgPrint(DL_ERROR, DM_DRIVER_INIT, ("InVolFlt:Bypassing...\n"));
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
    LoadClusterConfigFromClustDiskDriverParameters();

    // Create the system thread to read and write bitmaps.
    Status = PsCreateSystemThread(&hThread, THREAD_ALL_ACCESS, NULL, NULL, NULL, ServiceStateChangeFunction, NULL);
    if (NT_SUCCESS(Status))
    {
        Status = ObReferenceObjectByHandle(hThread, SYNCHRONIZE, NULL, KernelMode, (PVOID *)&DriverContext.pServiceThread, NULL);
        if (NT_SUCCESS(Status))
        {
            ZwClose(hThread);
        }
    }

    // Create the system thread to Flush the Time Stamp and Sequence Number in the persistent space.
    Status = PsCreateSystemThread(&hThread, THREAD_ALL_ACCESS, NULL, NULL, NULL, PersistentValuesFlush, NULL);
    if (NT_SUCCESS(Status))
    {
        Status = ObReferenceObjectByHandle(hThread, SYNCHRONIZE, NULL, KernelMode, (PVOID *)&DriverContext.pTimeSeqFlushThread, NULL);
        if (NT_SUCCESS(Status))
        {
            ZwClose(hThread);
        }
    }

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
    DriverObject->MajorFunction[IRP_MJ_CLEANUP]         = InMageFltCleanup;
#ifdef DBG
    DriverObject->MajorFunction[IRP_MJ_READ]            = InMageFltRead;
#endif
    DriverObject->MajorFunction[IRP_MJ_WRITE]           = InMageFltWrite;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]  = InMageFltDeviceControl;
    DriverObject->MajorFunction[IRP_MJ_SHUTDOWN]        = InMageFltShutdown;
    
    DriverObject->MajorFunction[IRP_MJ_FLUSH_BUFFERS]   = InMageFltFlush;
    DriverObject->MajorFunction[IRP_MJ_PNP]             = InMageFltDispatchPnp;
    DriverObject->MajorFunction[IRP_MJ_POWER]           = InMageFltDispatchPower;

    DriverObject->DriverExtension->AddDevice            = InMageFltAddDevice;
    DriverObject->DriverUnload                          = InMageFltUnload;

#if(NTDDI_VERSION >= NTDDI_WS03)//find the volume devices using device interfaces
    Status = IoGetDeviceInterfaces(&VolumeClassGuid, NULL, 0, &SymbolicLinkList);
    RtlInitUnicodeString(&ustrVolumeName, SymbolicLinkList);
    if (NT_SUCCESS(Status) && (ustrVolumeName.Length != 0)){
                       
            AttachToVolumeWithoutReboot(SymbolicLinkList);
      // Attempting one more time in case of No Devices found for a given criteria or if this call fails
    } else {
        Status = IoGetDeviceInterfaces(&VolumeClassGuid, NULL, 0, &SymbolicLinkList);
        RtlInitUnicodeString(&ustrVolumeName, SymbolicLinkList);

        if (NT_SUCCESS(Status) && (ustrVolumeName.Length != 0)){
            AttachToVolumeWithoutReboot(SymbolicLinkList);
        } else if (!NT_SUCCESS(Status)){
            InMageFltLogError(DriverContext.ControlDevice, __LINE__, INVOLFLT_ERR_ENUMERATE_DEVICE, STATUS_SUCCESS, (ULONG)Status);
        }
    }
#endif

 //Always place this call just above return of STATUS_SUCCESS.
 //Introduced for autochk failure issue. Bugzilla id: 26376
 if(DriverContext.IsWindows8AndLater){
  IoRegisterBootDriverReinitialization(DriverObject,
  									   InMageFltOnReinitialize,
  									   NULL);
 }
    return STATUS_SUCCESS;

} // end DriverEntry()

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
    if (DeviceObject == DriverContext.ControlDevice)
    {
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

    Irp->IoStatus.Status = STATUS_DEVICE_NOT_READY;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_DEVICE_NOT_READY;

} // end InMageFltSendToNextDriver()

NTSTATUS
InMageFltDispatchPower(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    PDEVICE_EXTENSION pDeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION  currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    PVOLUME_CONTEXT   pVolumeContext = NULL;
    PVOLUME_BITMAP    pVolumeBitmap = NULL;
    NTSTATUS          Status = STATUS_SUCCESS;
    KIRQL             OldIrql = NULL;
    
    InVolDbgPrint(DL_INFO, DM_POWER, ("InMageFltDispatchPower Device = %#p MinorFunction = %#x  ShutdownType = %#x\n", 
        DeviceObject, currentIrpStack->MinorFunction, currentIrpStack->Parameters.Power.ShutdownType ));

    if ((IRP_MN_SET_POWER == currentIrpStack->MinorFunction) &&
        ((PowerActionHibernate == currentIrpStack->Parameters.Power.ShutdownType) ||
         (PowerActionShutdown == currentIrpStack->Parameters.Power.ShutdownType) ||
         (PowerActionShutdownReset == currentIrpStack->Parameters.Power.ShutdownType) ||
         (PowerActionShutdownOff == currentIrpStack->Parameters.Power.ShutdownType))) 
    {
        // if the volume was not shutdown by a IRP_MJ_SHUTDOWN, we will do it here
        if ((!pDeviceExtension->shutdownSeen) && (KeGetCurrentIrql() == PASSIVE_LEVEL)) {
            pVolumeContext = pDeviceExtension->pVolumeContext;
            if ((pVolumeContext) && (!(pVolumeContext->ulFlags & VCF_FILTERING_STOPPED))) {
                if (IS_CLUSTER_VOLUME(pVolumeContext)) {
                    // For Cluster Volume, Power IRP may come before FS Unmount and Disk Release(Bug # 3453) 
                    // Let's store the changes to either Bitmap or Disk.
                    if (pVolumeContext->ChangeList.ulTotalChangesPending || pVolumeContext->ulChangesPendingCommit) {
                        if (NULL == pVolumeContext->pVolumeBitmap) {
                            pVolumeBitmap = OpenBitmapFile(pVolumeContext, Status);
                            if (pVolumeBitmap) {
                                KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
                                pVolumeContext->pVolumeBitmap = ReferenceVolumeBitmap(pVolumeBitmap);
                                if(pVolumeContext->bQueueChangesToTempQueue) {
                                    MoveUnupdatedDirtyBlocks(pVolumeContext);
                                }
                                KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);
                            }
                        }
                        if (NULL != pVolumeContext->pVolumeBitmap) {
                            InMageFltSaveAllChanges(pVolumeContext, TRUE, NULL);
                            if (pVolumeContext->pVolumeBitmap->pBitmapAPI) {
                                BitmapPersistentValues BitmapPersistentValue(pVolumeContext);
                                pVolumeContext->pVolumeBitmap->pBitmapAPI->CommitBitmap(BitmapPersistentValue, NULL);
                            }
                        } else {
                            InVolDbgPrint( DL_ERROR, DM_CLUSTER, ("InMageFltDispatchPower Opening Bitmap File for Volume %c: has failed\n", 
                                    (UCHAR)pVolumeContext->wDriveLetter[0]));
                            SetBitmapOpenFailDueToLossOfChanges(pVolumeContext, false);
                        }
                    }
                    ASSERT((0 == pVolumeContext->ChangeList.ulTotalChangesPending) && (0 == pVolumeContext->ChangeList.ullcbTotalChangesPending));
                    if (pVolumeContext->ChangeList.ulTotalChangesPending) {
                        InVolDbgPrint( DL_ERROR, DM_CLUSTER, ("Volume DiskSig = 0x%x has ChangesPending = 0x%x ChangesPendingCommit = 0x%x\n",
                                pVolumeContext->ulDiskSignature, pVolumeContext->ChangeList.ulTotalChangesPending,
                                pVolumeContext->ulChangesPendingCommit));
                    }
                } else {
                    // This volume is not a cluster volume.
                    if (pVolumeContext->pVolumeBitmap) {
                        InVolDbgPrint(DL_INFO, DM_POWER, ("InMageFltDispatchPower Flushing final volume changes Device = %#p \n",
                                DeviceObject));
                        FlushAndCloseBitmapFile(pVolumeContext);
                    }
                }
            }
        }
    }

    PoStartNextPowerIrp(Irp);

	//Fix for Bug 28568 and similar.
	Status = IoAcquireRemoveLock(&pDeviceExtension->RemoveLock,Irp);

	if(!NT_SUCCESS(Status))
	{
		InVolDbgPrint(DL_INFO, DM_POWER,
                ("InMageFltDispatchPower: IoAcquireRemoveLock Dishonoured with Status %lx\n",Status));
		Irp->IoStatus.Status = Status;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return Status;
	}
	
    IoSkipCurrentIrpStackLocation(Irp);

    Status =  PoCallDriver(pDeviceExtension->TargetDeviceObject, Irp);

	IoReleaseRemoveLock(&pDeviceExtension->RemoveLock,Irp);

	return Status;

} // end InMageFltDispatchPower
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

    if (DeviceObject == DriverContext.ControlDevice)
    {
        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }

    IoSkipCurrentIrpStackLocation(Irp);
    pDeviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
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

    if ((DeviceObject == DriverContext.ControlDevice) || (NULL == pDeviceExtension)) 
    {
        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }

    IoSkipCurrentIrpStackLocation(Irp);

    return IoCallDriver(pDeviceExtension->TargetDeviceObject, Irp);

} // end InMageFltClose()

NTSTATUS
InMageFltCleanup(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    PDEVICE_EXTENSION   pDeviceExtension;
    pDeviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

    if ((DeviceObject == DriverContext.ControlDevice) || (NULL == pDeviceExtension)) 
    {
        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }

    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(pDeviceExtension->TargetDeviceObject, Irp);

} // end InMageFltCleanup()


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
InMageFltWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    PDEVICE_EXTENSION   pDeviceExtension ; 
    PVOLUME_CONTEXT     pVolumeContext = NULL;
    PDRIVER_OBJECT      DrvObj = NULL;
    PDEVICE_OBJECT      DevObj = DeviceObject;
    PWCHAR              ServiceName = NULL;
    PIO_STACK_LOCATION  currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION  topIrpStack;
    int                 cChanges;
    PKDIRTY_BLOCK       pDirtyBlock;
    NTSTATUS            Status = STATUS_SUCCESS;
    PDRIVER_DISPATCH     DispatchEntryForWrite = NULL;
  
    
    if(DriverContext.IsDispatchEntryPatched) {
        pVolumeContext = GetVolumeContextForThisIOCTL(DeviceObject, NULL);
        if(pVolumeContext) {
            //
            // Call came to a Lower-level DeviceObject. Since its dispacth routines are taken-over by
            // involflt, involflt driver got the call. Process the IRP.
            //
            DeviceObject = pVolumeContext->FilterDeviceObject;
            pDeviceExtension = (PDEVICE_EXTENSION)pVolumeContext->FilterDeviceObject->DeviceExtension;
            if(pVolumeContext->IsVolumeCluster) {
                DispatchEntryForWrite = DriverContext.MajorFunctionForClusDrives[IRP_MJ_WRITE];
            } else {
                DispatchEntryForWrite = DriverContext.MajorFunction[IRP_MJ_WRITE];
            }

            //
            // This is a case of new volumes that are created after involflt driver
            // is loaded. Such volumes come through "AddDevice" (not enumerated).
            // For such volumes, involflt driver will be in the stack and IRPs will be
            // processed at involflt level. But because, dispatch routines of lower-level
            // DeviceObject are taken-over by involflt, the IRP again comes to involflt
            // through this route/path. Need to ignore the IRP (do not process it again)
            // and pass it down to lower-level DeviceObject.
            //
            if(pVolumeContext->IsVolumeUsingAddDevice)
                return (*DispatchEntryForWrite)(pDeviceExtension->TargetDeviceObject, Irp);

            //
            // Volume did not come through "AddDevice" but came through "enumeration".
            // Currently, we are at the level (IO_STACK_LOCATION) of lower-level
            // DeviceObject. Process the IRP and then pass-it down to the lower-level
            // DeviceObject by calling its dispatch routine directly (not through IoCallDriver)
            //
        } else {
            if (DeviceObject->DriverObject == DriverContext.DriverObject) {
                //
                // Call came to involflt, when it was not expected, as involflt is loaded
                // at run-time and its not in the stack.
                //
                // But this is possible for new volumes for which invoflt driver will
                // be in the stack and also for existing volumes which have been
                // unmounted/re-mounted.
                //
                // In such cases Involflt driver will receive same IRP multiple
                // times. At one point, IRP will directly come to involflt, as it is in
                // the stack. Second time, IRP will again come to involflt, as it
                // has taken-over dispatch routines of lower-level DeviceObject.
                // In case of cluster volumes, IRP may again come to involflt
                // as it might have taken-over dispatch routines of "clusdisk" driver too.
                //
                // In such cases IRP is handled at invofllt level and ignored at other
                // levels.
                //
                
                pDeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
                pVolumeContext = pDeviceExtension->pVolumeContext;

                //
                // Bug Fix: 14969: System stop responding with NoReboot Driver
                // Please see the bug for details
                // This is a case of existing volume (volume existed before
                // involflt driver was loaded) getiing unmounted/remounted.
                // This time for this volume involflt is in the stack but this volume
                // came through enumeration (not through "AddDevice").
                // Because involflt is in the stack, this IRP came to involflt.
                // Since, volume came by enumeration and since for this
                // volume dispacth routines of lower-level DeviceObjects are
                // taken-over by involflt, we know that this IRP will again come
                // to involflt at next/lower DeviceObjects level. We will process
                // the IRP at that point and not here.
                //
                if (pVolumeContext->IsVolumeUsingAddDevice != true) {
                    IoSkipCurrentIrpStackLocation(Irp);
                    return IoCallDriver(pDeviceExtension->TargetDeviceObject, Irp);
                }

                if (pVolumeContext){
                    if(pVolumeContext->IsVolumeCluster) {
                        DispatchEntryForWrite = DriverContext.MajorFunctionForClusDrives[IRP_MJ_WRITE];
                    } else {
                        DispatchEntryForWrite = DriverContext.MajorFunction[IRP_MJ_WRITE];
                    }
                }
            } else if (DriverContext.pDriverObject == DeviceObject->DriverObject ) {
                //
                // IRP has been already proceesed at involflt level or lower-level DeviceObjects level.
                // Should not process it again. Ignore it. Call came here because this guys/drivers
                // dispatch-routines are taken-over by involflt.
                //
                return (*DriverContext.MajorFunction[IRP_MJ_WRITE])(DeviceObject, Irp);                    
            } else
                //
                // IRP has been already proceesed at involflt level or lower-level DeviceObjects level.
                // Should not process it again. Ignore it. Call came here because this guys/drivers (clusdisk)
                // dispatch-routines are taken-over by involflt.
                //
                return (*DriverContext.MajorFunctionForClusDrives[IRP_MJ_WRITE])(DeviceObject, Irp);   
        }
    } else {
        pDeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    }

    if (DriverContext.IsDispatchEntryPatched && (pVolumeContext->IsVolumeUsingAddDevice != true)) {
        // Bug Fix: 15364: involflt handles only DIRECT IO
        if (NULL == Irp->MdlAddress) {
            return (*DispatchEntryForWrite)(pDeviceExtension->TargetDeviceObject, Irp);
        }
    }

    InVolDbgPrint(DL_VERBOSE, DM_WRITE, ("InMageFltWrite: Device = %#p  Irp = %#p Length = %#x  Offset = %#I64x\n", 
            DeviceObject, Irp, currentIrpStack->Parameters.Write.Length,
            currentIrpStack->Parameters.Write.ByteOffset.QuadPart));

    if (DeviceObject == DriverContext.ControlDevice) {
        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }

    pVolumeContext = pDeviceExtension->pVolumeContext;

   //This is a fix specific to windows 2012 OS where it was observed that autochk.exe failed
   //at boot time while attempting to fix volume corruption. It was found to be caused due to involflt.sys
   //locking bitmap files on boot volume which incapacitated autochk.exe from doing exclusive locks
   //on files.BUG.ID: 26376 | http://bugzilla/show_bug.cgi?id=26376 - Kartha
   //Here the intention is to capture first write IO (by default involflt starts in metadata mode) and then
   //confirm that it's to boot volume (usually always yes) followed by registering devicechange notification
   //for boot volume. 
   //First use a simple cmp instruction before using lock on bus instruction for speed. We are in IO path.
   if((!DriverContext.bootVolNotificationInfo.bootVolumeDetected) && \
   	(DriverContext.bootVolNotificationInfo.reinitCount) && (DriverContext.IsWindows8AndLater) ){

    if( (pVolumeContext->IsVolumeUsingAddDevice) && (!DriverContext.IsDispatchEntryPatched) &&\
		(MAX_FAILURE > DriverContext.bootVolNotificationInfo.failureCount)){
     if(!InterlockedCompareExchange(&DriverContext.bootVolNotificationInfo.bootVolumeDetected, (LONG)1, (LONG)0)){
		 InterlockedExchangePointer((PVOID *)&DriverContext.bootVolNotificationInfo.pBootVolumeContext, pVolumeContext);
		 //signal the event i.e. release the threads waiting on KeWait.... function.
		 KeSetEvent(&DriverContext.bootVolNotificationInfo.bootNotifyEvent, IO_NO_INCREMENT, FALSE);
     }
    }

   }
   
	
    // check to see if the request is ANY request generated from an AsyncIORequest object
    // Also check if IO generated by AsyncIORequest object got splitted and marked as masterirp
    if ((AsyncIORequest::IsOurIO(Irp)) || ((Irp->Flags & IRP_ASSOCIATED_IRP) && (Irp->AssociatedIrp.MasterIrp != NULL) && 
            (AsyncIORequest::IsOurIO(Irp->AssociatedIrp.MasterIrp)))) {
        // if needed pass to low-level code for snooping, used to do physical I/O at shutdown
        if (DriverContext.WriteFilteringObject) {
            DriverContext.WriteFilteringObject->LearnPhysicalIOFilter(DeviceObject, Irp);
        }
        
        InVolDbgPrint(DL_INFO, DM_WRITE, 
            ("InMageFltWrite: Device = %#p  Irp = %#p  Bypassing capture of our own writes\n", 
                DeviceObject, Irp));

        // do NOT capture dirty blocks for our own log files, prevents write cascade
//        if(IsDispatchEntryPatched) 
        if(!pVolumeContext->IsVolumeUsingAddDevice && DriverContext.IsDispatchEntryPatched)
            return (*DispatchEntryForWrite)(pDeviceExtension->TargetDeviceObject, Irp);
        else
            return InMageFltSendToNextDriver(DeviceObject, Irp);
    }




    //
    //Check if the volume is to be treated as read only.
    //If yes complete the irp without sending it down or processing it
    //
    if((NULL != pVolumeContext) && (pVolumeContext->ulFlags & VCF_READ_ONLY))
    {
        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;    
    }

    //
    // Device is not initialized properly, or Filtering is stoped on this volume.
    // Blindly pass the irp along
    //

    if ((NULL == pVolumeContext) || (pVolumeContext->ulFlags & VCF_FILTERING_STOPPED)){
        //if(IsDispatchEntryPatched) 
        if(!pVolumeContext->IsVolumeUsingAddDevice && DriverContext.IsDispatchEntryPatched)
            return (*DispatchEntryForWrite)(pDeviceExtension->TargetDeviceObject, Irp);
        else
            return InMageFltSendToNextDriver(DeviceObject, Irp);
    }

    bool PagingIO = false;
    //walk down the irp and find the originating file in the first irp stack slot
    topIrpStack = (PIO_STACK_LOCATION)((PCHAR)Irp + sizeof(IRP)) + (Irp->StackCount - 1);

    // Bug#23913 : On Windows 2012, Some Hyper-v writes have invalid fileobject leading to crash. Tests showed that all 
    // paging writes have NTFS as the top of the stack. Let's verify valid deviceobject and match with NTFS deviceobject
    // This IRP would be verified further to see paging file I/O or not.
    // check to see if it's the system paging file
    if ((NULL != pDeviceExtension->pagingFile) && (VCF_PAGEFILE_WRITES_IGNORED & pVolumeContext->ulFlags) && 
        (pDeviceExtension->pagingFile == topIrpStack->FileObject)) {
        PagingIO = true;
    } 
#if (_WIN32_WINNT >= 0x501)	
    else if (!pVolumeContext->IsVolumeUsingAddDevice && DriverContext.IsDispatchEntryPatched){ //No-reboot handling case
        bool VerifyPagingWrite = false;
        if (DriverContext.IsWindows8AndLater) {

            // Let's find the NTFS device object for this filter device object on first filtered I/O, Assuming this deviceobject 
            // always valid and unmount unlikely to happen. In the worst case driver captures the changes.
            if (NULL == pVolumeContext->NTFSDevObj) { // Let's get NTFS Device object on first write
                PDEVICE_OBJECT pDevObj = NULL;
                if (topIrpStack->DeviceObject) {
                    PDEVICE_OBJECT pDevObj = topIrpStack->DeviceObject;
                    if ((NULL != pDevObj) && (NULL != pDevObj->DriverObject) && (NULL != pDevObj->DriverObject->DriverExtension)) {
                        if (pDevObj->DriverObject->DriverExtension->ServiceKeyName.Length ==
                            RtlCompareMemory(pDevObj->DriverObject->DriverExtension->ServiceKeyName.Buffer, L"Ntfs", 
                                             pDevObj->DriverObject->DriverExtension->ServiceKeyName.Length))
                            pVolumeContext->NTFSDevObj = pDevObj; 
                    }
               } 
            }
            if ((NULL != pVolumeContext->NTFSDevObj) && (pVolumeContext->NTFSDevObj == topIrpStack->DeviceObject))
                VerifyPagingWrite = true;
        }

        // No-reboot driver does not detect the page file writes, another condition is added to skip the page file writes by 
        // detecting whether given IRP's file object is paging object
        if ((!DriverContext.IsWindows8AndLater && topIrpStack->DeviceObject) || (DriverContext.IsWindows8AndLater && VerifyPagingWrite)) {
            if (KeGetCurrentIrql() <= APC_LEVEL) {
                if ((NULL != topIrpStack->FileObject) && FsRtlIsPagingFile(topIrpStack->FileObject))
                    PagingIO = true;
           }
        }
	}
#endif

    if (PagingIO) {

        InVolDbgPrint(DL_INFO, DM_WRITE, 
            ("InMageFltWrite: Device = %#p  Irp = %#p  Bypassing capture of pagefile write\n", 
            Irp, DeviceObject));

        // do NOT capture dirty blocks for the paging file, prevents many extra dirty blocks at shutdown
        if(!pVolumeContext->IsVolumeUsingAddDevice && DriverContext.IsDispatchEntryPatched) {
            return (*DispatchEntryForWrite)(pDeviceExtension->TargetDeviceObject, Irp);
        } else
            return InMageFltSendToNextDriver(DeviceObject, Irp);
    }

    if (IS_VOLUME_OFFLINE(pVolumeContext)) {
	
        InVolDbgPrint( DL_ERROR, DM_CLUSTER, 
            ("InMageFltWrite: Device = %p Write Irp with ByteOffset %I64X and Length 0x%x on OFFLINE cluster volume\n",
            DeviceObject, currentIrpStack->Parameters.Write.ByteOffset.QuadPart, currentIrpStack->Parameters.Write.Length));

#if (_WIN32_WINNT > 0x0500)


// Above Win2k (Win2k is 0x0500, WinXP is 0x0501, Server 2003 is 0x0502)
        if (APC_LEVEL >= KeGetCurrentIrql()) {
            // Save the change to bitmap
            if ((pVolumeContext->pVolumeBitmap) && (pVolumeContext->pVolumeBitmap->pBitmapAPI)) {
                if (0 == (pVolumeContext->ulFlags & VCF_CLUSDSK_RETURNED_OFFLINE)) {
                    WRITE_META_DATA WriteMetaData;

                    WriteMetaData.llOffset = currentIrpStack->Parameters.Write.ByteOffset.QuadPart;
                    WriteMetaData.ulLength = currentIrpStack->Parameters.Write.Length;
                    Status = pVolumeContext->pVolumeBitmap->pBitmapAPI->SaveWriteMetaDataToBitmap(&WriteMetaData);
                    if (NT_SUCCESS(Status)) {
                        InVolDbgPrint(DL_INFO, DM_CLUSTER | DM_BITMAP_WRITE,
                            ("InMageFltWrite: SaveWriteMetaData succeeded\n"));
                    } else {
                        if (STATUS_DEVICE_OFF_LINE == Status)
                            pVolumeContext->ulFlags |= VCF_CLUSDSK_RETURNED_OFFLINE;

                        InVolDbgPrint(DL_WARNING, DM_CLUSTER | DM_BITMAP_WRITE,
                            ("InMageFltWrite: SaveWriteMetaData failed with status %#lx\n", Status));
                    }
                }
            } else {
                InVolDbgPrint(DL_WARNING, DM_CLUSTER | DM_BITMAP_WRITE,
                    ("InMageFltWrite: Skipping SaveWriteMetaData pBitmapFile is NULL\n"));
            }
        } else {
            InVolDbgPrint(DL_ERROR, DM_CLUSTER | DM_BITMAP_WRITE,
                ("InMageFltWrite: Skipping SaveWriteMetaData as IRQL is > APC_LEVEL\n"));
        }
        //if(IsDispatchEntryPatched) {
        if(!pVolumeContext->IsVolumeUsingAddDevice && DriverContext.IsDispatchEntryPatched) {

            COMPLETION_CONTEXT  *pCompRoutine = NULL;
            pCompRoutine = (PCOMPLETION_CONTEXT)ExAllocatePoolWithTag(NonPagedPool,
                                                                      sizeof(COMPLETION_CONTEXT),
                                                                      TAG_GENERIC_NON_PAGED);
            if (pCompRoutine) {
                pCompRoutine->eContextType = ecCompletionContext;
                pCompRoutine->VolumeContext = pVolumeContext;
                
                pCompRoutine->CompletionRoutine = currentIrpStack->CompletionRoutine;
                pCompRoutine->Context = currentIrpStack->Context;

                currentIrpStack->CompletionRoutine = InMageFltWriteOnOfflineVolumeCompletion;
                currentIrpStack->Context = pCompRoutine;

                return (*DispatchEntryForWrite)(pDeviceExtension->TargetDeviceObject, Irp);           
                
            } else {
                return (*DispatchEntryForWrite)(pDeviceExtension->TargetDeviceObject, Irp);
            }

        }
        else {
            IoCopyCurrentIrpStackLocationToNext(Irp);
            IoSetCompletionRoutine(Irp,
                            InMageFltWriteOnOfflineVolumeCompletion,
                            pVolumeContext,
                            TRUE,
                            TRUE,
                            TRUE);
            return IoCallDriver(pDeviceExtension->TargetDeviceObject, Irp);
        }
#else
// Win2K and below
        InVolDbgPrint( DL_ERROR, DM_BITMAP_WRITE | DM_CLUSTER, 
            ("InMageFltWrite: Device = %p Failing Write Irp with ByteOffset %I64X and Length 0x%x on OFFLINE cluster volume\n",
            DeviceObject, currentIrpStack->Parameters.Write.ByteOffset.QuadPart, currentIrpStack->Parameters.Write.Length));
        

        Irp->IoStatus.Status = STATUS_DEVICE_OFF_LINE;
        Irp->IoStatus.Status = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_DEVICE_OFF_LINE;
#endif 
    }

    // validate the write parameters, prevents other areas from having problems
    if (pVolumeContext->llVolumeSize > 0) {
        if ((currentIrpStack->Parameters.Write.ByteOffset.QuadPart >= pVolumeContext->llVolumeSize) ||
            (currentIrpStack->Parameters.Write.ByteOffset.QuadPart < 0) ||
            ((currentIrpStack->Parameters.Write.ByteOffset.QuadPart + currentIrpStack->Parameters.Write.Length) > pVolumeContext->llVolumeSize)) 
        {
            InVolDbgPrint(DL_ERROR, DM_WRITE, ("InMageFltWrite: Device = %#p  Write Irp with ByteOffset %I64X outside volume range\n", 
                DeviceObject, currentIrpStack->Parameters.Write.ByteOffset.QuadPart));
			QueueWorkerRoutineForSetVolumeOutOfSync(pVolumeContext,INVOLFLT_ERR_VOLUME_WRITE_PAST_EOV, STATUS_NOT_SUPPORTED, false);
            InMageFltLogError(DeviceObject,__LINE__, INVOLFLT_ERR_VOLUME_WRITE_PAST_EOV1, STATUS_SUCCESS, 
				pVolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES, pVolumeContext->wGUID, GUID_SIZE_IN_BYTES, 
				(ULONGLONG)pVolumeContext->llVolumeSize);
            //if(IsDispatchEntryPatched) {
            if(!pVolumeContext->IsVolumeUsingAddDevice && DriverContext.IsDispatchEntryPatched) {
                return (*DispatchEntryForWrite)(pDeviceExtension->TargetDeviceObject, Irp);                    
            } else
                return InMageFltSendToNextDriver(DeviceObject, Irp);
            
        }
    }

    if (0 == currentIrpStack->Parameters.Write.Length) {
        InVolDbgPrint(DL_INFO, DM_WRITE, ("InMageFltWrite: Device = %#p Write Irp with length 0 and Offset %#I64x. Skipping...\n",
                                          DeviceObject, currentIrpStack->Parameters.Write.ByteOffset.QuadPart));
        //if(IsDispatchEntryPatched) 
        if(!pVolumeContext->IsVolumeUsingAddDevice && DriverContext.IsDispatchEntryPatched)
            return (*DispatchEntryForWrite)(pDeviceExtension->TargetDeviceObject, Irp);
        else
            return InMageFltSendToNextDriver(DeviceObject, Irp);
            
    }

    bool bIrpProcessed = false;

    if (ecCaptureModeData != pVolumeContext->eCaptureMode) {
        WriteDispatchInMetaDataMode(DeviceObject, Irp); 
        bIrpProcessed = true;
    }
    
    if (bIrpProcessed) { 
        //if ( IsDispatchEntryPatched ) {
        if(!pVolumeContext->IsVolumeUsingAddDevice && DriverContext.IsDispatchEntryPatched) {
            return (*DispatchEntryForWrite)(pDeviceExtension->TargetDeviceObject, Irp);
        } else                         
            Status = IoCallDriver(pDeviceExtension->TargetDeviceObject, Irp);
        
    } else {
        Status = WriteDispatchInDataFilteringMode(DeviceObject, Irp);
    }

    return Status;
} // end InMageFltWrite()


NTSTATUS
InMageFltRead(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    PDEVICE_EXTENSION   pDeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PVOLUME_CONTEXT     pVolumeContext = NULL;
    PIO_STACK_LOCATION  currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION  topIrpStack;
    NTSTATUS            Status = STATUS_SUCCESS;

    InVolDbgPrint(DL_VERBOSE, DM_WRITE, ("InMageFltRead: Device = %#p  Irp = %#p Length = %#x  Offset = %#I64x\n", 
            DeviceObject, Irp, currentIrpStack->Parameters.Read.Length,
            currentIrpStack->Parameters.Read.ByteOffset.QuadPart));

    if (DeviceObject == DriverContext.ControlDevice) {
        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }

    pVolumeContext = pDeviceExtension->pVolumeContext;
    if ((NULL == pVolumeContext) || (pVolumeContext->ulFlags & VCF_FILTERING_STOPPED)){
        return InMageFltSendToNextDriver(DeviceObject, Irp);
    }

    // walk down the irp and find the originating file in the first irp stack slot
    topIrpStack = (PIO_STACK_LOCATION)((PCHAR)Irp + sizeof(IRP)) + (Irp->StackCount - 1);

    // Should we profile these reads too?
    if ((VCF_PAGEFILE_WRITES_IGNORED & pVolumeContext->ulFlags) && (NULL != pDeviceExtension->pagingFile)) {
        if (pDeviceExtension->pagingFile == topIrpStack->FileObject) {
            return InMageFltSendToNextDriver(DeviceObject, Irp);
        }
    }

    LogReadIOSize(currentIrpStack->Parameters.Read.Length, &pVolumeContext->VolumeProfile);
    
    return InMageFltSendToNextDriver(DeviceObject, Irp);
} // end InMageFltRead()



NTSTATUS
InMageFltWriteOnOfflineVolumeCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp,
    IN PVOID          Context
    )
{
    NTSTATUS    Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION irpStack, IoNextStackLocation = IoGetNextIrpStackLocation(Irp);
    etContextType       eContextType = (etContextType)*((PCHAR )Context + sizeof(LIST_ENTRY));

    if (eContextType == ecCompletionContext) {
        PIRP OrigIrp; 
         PCOMPLETION_CONTEXT pCompletionContext = (PCOMPLETION_CONTEXT)Context;
        //irpStack = IoGetCurrentIrpStackLocation(Irp); // Cloned Irp
        
        //ASSERT(irpStack->MajorFunction == IRP_MJ_WRITE);

        InVolDbgPrint(DL_INFO, DM_CLUSTER, ("InMageFltWriteOnOfflineVolumeCompletion:  Info = %#x, Status = %#x\n",
                Irp->IoStatus.Information, Irp->IoStatus.Status ));
        if(pCompletionContext->CompletionRoutine) {
            IoNextStackLocation->CompletionRoutine = pCompletionContext->CompletionRoutine;
            IoNextStackLocation->Context = pCompletionContext->Context;
            Status = (*pCompletionContext->CompletionRoutine)(DeviceObject, Irp, pCompletionContext->Context);

        }
        ExFreePoolWithTag(pCompletionContext, TAG_GENERIC_NON_PAGED);

       
    } else {
        irpStack = IoGetCurrentIrpStackLocation(Irp);
        PVOLUME_CONTEXT     pVolumeContext = (PVOLUME_CONTEXT)Context;
    //
    // Update counters
    //

        ASSERT(irpStack->MajorFunction == IRP_MJ_WRITE);

        InVolDbgPrint(DL_INFO, DM_CLUSTER, ("InMageFltWriteOnOfflineVolumeCompletion: length=%#x, offset=%#I64x, Info = %#x, Status = %#x\n",
            irpStack->Parameters.Write.Length,
            irpStack->Parameters.Write.ByteOffset.QuadPart, Irp->IoStatus.Information, Irp->IoStatus.Status ));

        if (Irp->PendingReturned) {
            IoMarkIrpPending(Irp);
        }
    }
   return Status;
} // InMageFltWriteOnOfflineVolumeCompletion

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

    IoReleaseCancelSpinLock(Irp->CancelIrql);

    UserProcess = IoGetCurrentProcess();

    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    if (DriverContext.ProcessStartIrp == Irp) {
        ASSERT(UserProcess == DriverContext.UserProcess);
        InVolDbgPrint(DL_INFO, DM_VOLUME_STATE, 
            ("InMageFltCancelProcessStartIrp: Canceling process start Irp, Irp = %#p\n", Irp));
        
        DriverContext.ProcessStartIrp = NULL;
        bUnmapData = TRUE;
    }

    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);


PAGED_CODE();
    if (bUnmapData) {
        FreeOrphanedMappedDataBlockList();

        // Unmap the data from user process.
        InitializeListHead(&VolumeNodeList);
        Status = GetListOfVolumes(&VolumeNodeList);
        if (STATUS_SUCCESS == Status) {
            while(!IsListEmpty(&VolumeNodeList)) {
                PLIST_NODE  pNode;
                PVOLUME_CONTEXT VolumeContext;

                pNode = (PLIST_NODE)RemoveHeadList(&VolumeNodeList);
                VolumeContext = (PVOLUME_CONTEXT)pNode->pData;
                DereferenceListNode(pNode);
                KeWaitForSingleObject(&VolumeContext->SyncEvent, Executive, KernelMode, FALSE, NULL);
                if (VolumeContext->pDBPendingTransactionConfirm) {
                    PKDIRTY_BLOCK    DirtyBlock = VolumeContext->pDBPendingTransactionConfirm;
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
                KeSetEvent(&VolumeContext->SyncEvent, IO_NO_INCREMENT, FALSE);
                KeAcquireSpinLock(&VolumeContext->Lock, &OldIrql);
                if (VolumeContext->DBNotifyEvent) {
                    ObDereferenceObject(VolumeContext->DBNotifyEvent);
                    VolumeContext->DBNotifyEvent = NULL;
                    VolumeContext->bNotify = false;
                    VolumeContext->liTickCountAtNotifyEventSet.QuadPart = 0;
                }
                KeReleaseSpinLock(&VolumeContext->Lock, OldIrql);

                DereferenceVolumeContext(VolumeContext);
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

    IoReleaseCancelSpinLock(Irp->CancelIrql);

    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    if (DriverContext.ServiceShutdownIrp == Irp)
    {
        InVolDbgPrint(DL_INFO, DM_VOLUME_STATE, 
            ("InMageFltCancelServiceShutdownIrp: Canceling shutdown Irp, Irp = %#p\n", Irp));
        
        DriverContext.ServiceShutdownIrp = NULL;
        ASSERT(ecServiceRunning == DriverContext.eServiceState);
        DriverContext.eServiceState = ecServiceShutdown;
        DriverContext.ulFlags |= DC_FLAGS_SERVICE_STATE_CHANGED;

        // Trigger activity to start writing existing data to bit map file.
        KeSetEvent(&DriverContext.ServiceThreadWakeEvent, IO_DISK_INCREMENT, FALSE);
    }

    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

    Irp->IoStatus.Status = STATUS_CANCELLED;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return;
}

BOOLEAN
ProcessVolumeStateChange(
    PVOLUME_CONTEXT     pVolumeContext,
    PLIST_ENTRY         ListHead,
    BOOLEAN             bServiceStateChanged
    )
{
    BOOLEAN          bReturn = TRUE;
    BOOLEAN          bWriteToBitmap = FALSE;

    if (ecDSDBCdeviceOffline == pVolumeContext->eDSDBCdeviceState) {
        // Unlink the dirty block context from DriverContext LinkedList.
        RemoveEntryList((PLIST_ENTRY)pVolumeContext);
        DriverContext.ulNumVolumes--;
        InitializeListHead((PLIST_ENTRY)pVolumeContext);
        pVolumeContext->ulFlags |= VCF_REMOVED_FROM_LIST; // Helps in debugging to know if it is removed from list.

        bReturn = AddVCWorkItemToList(ecWorkItemForVolumeUnload, pVolumeContext, 0, FALSE, ListHead);
        DereferenceVolumeContext(pVolumeContext);

        return bReturn;
    }

    // This function is called holding spin lock of DEVICE_CONTEXT and DriverContext both.
    if (pVolumeContext->ulFlags & VCF_FILTERING_STOPPED) {
        // If volume filtering is disabled. we should not process this volume context.
        return bReturn;
    }

    // If device is surprise removed, we should not process this volume context.
    // In case of few ISCSI devices, we receive only IRP_MN_SURPRISE_REMOVE but not IRP_MN_REMOVE_DEVICE
    // We have orphaned volume contexts, as system does not remove them.
    if (pVolumeContext->ulFlags & VCF_DEVICE_SURPRISE_REMOVAL) {
        InVolDbgPrint(DL_INFO, DM_CLUSTER | DM_VOLUME_CONTEXT,
            ("ProcessVolumeStateChange: Volume %#p, DriveLetter=%s is surprise removed skipping processing...\n",
                    pVolumeContext, pVolumeContext->DriveLetter));
        return bReturn;
    }

    if (IS_VOLUME_OFFLINE(pVolumeContext)) {
        InVolDbgPrint(DL_INFO, DM_CLUSTER | DM_VOLUME_CONTEXT,
            ("ProcessVolumeStateChange: Volume %#p, DriveLetter=%s is offline skipping bitmap load\n",
                    pVolumeContext, pVolumeContext->DriveLetter));
        return bReturn;
    }

    // See if the bitmap file has to be opened. If so open the bitmap file.
    if (pVolumeContext->ulFlags & VCF_OPEN_BITMAP_REQUESTED) {
        bReturn = AddVCWorkItemToList(ecWorkItemOpenBitmap, pVolumeContext, 0, FALSE, ListHead);
        if (!bReturn)
            return bReturn;
    } else if (0 == (pVolumeContext->ulFlags & VCF_GUID_OBTAINED)) {
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
                (0 == (pVolumeContext->ulFlags & VCF_BITMAP_WRITE_DISABLED)) &&
                (pVolumeContext->ChangeList.ulTotalChangesPending >= DriverContext.DBHighWaterMarks[ecServiceNotStarted])) 
            {
                bReturn = AddVCWorkItemToList(ecWorkItemBitmapWrite, pVolumeContext, 0, TRUE, ListHead);
            }
            break;
        case ecServiceRunning:
            // so the device is Online and service is running.
            if (bServiceStateChanged) {
                if (DriverContext.DBLowWaterMarkWhileServiceRunning &&
                    (0 == (pVolumeContext->ulFlags & VCF_BITMAP_READ_DISABLED)) &&
                    (pVolumeContext->ChangeList.ulTotalChangesPending < DriverContext.DBLowWaterMarkWhileServiceRunning))
                {
                    bReturn = AddVCWorkItemToList(ecWorkItemStartBitmapRead, pVolumeContext, 0, TRUE, ListHead);
                }
            } else {
                if (DriverContext.DBHighWaterMarks[ecServiceRunning] && 
                    (0 == (pVolumeContext->ulFlags & VCF_BITMAP_WRITE_DISABLED)) &&
                    ((pVolumeContext->ChangeList.ulTotalChangesPending >= DriverContext.DBHighWaterMarks[ecServiceRunning]) ||
                    (bWriteToBitmap == TRUE))) 
                {
                    ULONG ChangesToPurge = 0;
                    if (pVolumeContext->ChangeList.ulTotalChangesPending >= DriverContext.DBHighWaterMarks[ecServiceRunning]) {
                        ChangesToPurge = DriverContext.DBToPurgeWhenHighWaterMarkIsReached;
                    } else {
                        ChangesToPurge = KILOBYTES;
                    }
                    bReturn = AddVCWorkItemToList(ecWorkItemBitmapWrite, pVolumeContext, 
                                            ChangesToPurge, TRUE, ListHead);
                } else if (DriverContext.DBLowWaterMarkWhileServiceRunning &&
                    (0 == (pVolumeContext->ulFlags & VCF_BITMAP_READ_DISABLED)) &&
                    (pVolumeContext->ChangeList.ulTotalChangesPending < DriverContext.DBLowWaterMarkWhileServiceRunning)) 
                {
                    if (NULL == pVolumeContext->pVolumeBitmap) {
                        bReturn = AddVCWorkItemToList(ecWorkItemStartBitmapRead, pVolumeContext, 0, TRUE, ListHead);
                    } else {
                        if ((ecVBitmapStateOpened == pVolumeContext->pVolumeBitmap->eVBitmapState) ||
                            (ecVBitmapStateAddingChanges == pVolumeContext->pVolumeBitmap->eVBitmapState))
                        {
                            bReturn = AddVCWorkItemToList(ecWorkItemStartBitmapRead, pVolumeContext, 0, FALSE, ListHead);
                        } else if (ecVBitmapStateReadPaused == pVolumeContext->pVolumeBitmap->eVBitmapState) {
                            bReturn = AddVCWorkItemToList(ecWorkItemContinueBitmapRead, pVolumeContext, 0, FALSE, ListHead);
                        }
                    }
                }   
            }
            break;
        case ecServiceShutdown:
            // service is shutdown write all pending dirty blocks to bit map.
            if (bServiceStateChanged) {
                // move all commit pending dirty blocks to main queue.
                OrphanMappedDataBlocksAndMoveCommitPendingDirtyBlockToMainQueue(pVolumeContext);
                // TODO: This has to be removed from here and has to be taken care when dirty blocks are taken out
                // and queued to write to bitmap
                VCSetWOState(pVolumeContext, true, __FUNCTION__);
                // Free all reserved data.
                InVolDbgPrint(DL_INFO, DM_CAPTURE_MODE | DM_DATA_FILTERING, 
                              ("%s: %s(%s)Changing capture mode to META-DATA as service is shutdown\n", __FUNCTION__,
                               pVolumeContext->GUIDinANSI, pVolumeContext->DriveLetter));
                VCSetCaptureModeToMetadata(pVolumeContext, true);
                VCFreeUsedDataBlockList(pVolumeContext, true);
                if (0 == (pVolumeContext->ulFlags & VCF_BITMAP_WRITE_DISABLED)) {
                    bReturn = AddVCWorkItemToList(ecWorkItemBitmapWrite, pVolumeContext, 0, TRUE, ListHead);
                }
            } else {
                if (DriverContext.DBHighWaterMarks[ecServiceShutdown] && 
                        (0 == (pVolumeContext->ulFlags & VCF_BITMAP_WRITE_DISABLED)) &&
                        (pVolumeContext->ChangeList.ulTotalChangesPending >= DriverContext.DBHighWaterMarks[ecServiceShutdown])) 
                {
                    bReturn = AddVCWorkItemToList(ecWorkItemBitmapWrite, pVolumeContext, 0, TRUE, ListHead);
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
    PVOLUME_CONTEXT pVolumeContext;
    PLIST_ENTRY     pEntry;
    BOOLEAN         bServiceStateChanged;
    LIST_ENTRY      WorkQueueEntriesListHead;
    PWORK_QUEUE_ENTRY   pWorkItem;

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

            pEntry = DriverContext.HeadForVolumeContext.Flink;
            while (pEntry != &DriverContext.HeadForVolumeContext)
            {
                pVolumeContext = (PVOLUME_CONTEXT)pEntry;
                pEntry = pEntry->Flink;

                KeAcquireSpinLock(&pVolumeContext->Lock, &DeviceContextIrql);
                ProcessVolumeStateChange(pVolumeContext, &WorkQueueEntriesListHead, bServiceStateChanged);
                KeReleaseSpinLock(&pVolumeContext->Lock, DeviceContextIrql);

            } // while                  

            KeResetEvent(&DriverContext.ServiceThreadWakeEvent);
            KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

            while (!IsListEmpty(&WorkQueueEntriesListHead))
            {
                // go through all Work Items and fire the events.
                pWorkItem = (PWORK_QUEUE_ENTRY)RemoveHeadList(&WorkQueueEntriesListHead);
                ProcessVContextWorkItems(pWorkItem);
                DereferenceWorkQueueEntry(pWorkItem);
            }
        } // (STATUS_WAIT_0 == Status)

    // This condition would never be true. When it is true anyway we break out of the loop.
    // This is used just to avoid compilation warning in strict error checking.
    }while(FALSE == bShutdownThread);

    return;
}

PVOLUME_CONTEXT
GetVolumeContextForThisIOCTL(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{                                  
    PVOLUME_CONTEXT     pVolumeContext = NULL;
    if(Irp != NULL) {
        PDEVICE_EXTENSION   DeviceExtension;
        PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
            
        ULONG               ulGUIDLength;
                                      
        DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
        ASSERT(NULL != DeviceExtension);
        if (NULL == DeviceExtension->TargetDeviceObject) {
            // The IOCTL has come on control device object. So we need to find the volume GUID required to clear the bitmap.
            ulGUIDLength = sizeof(WCHAR) * GUID_SIZE_IN_CHARS;
            if (ulGUIDLength <= IoStackLocation->Parameters.DeviceIoControl.InputBufferLength) {
                pVolumeContext = GetVolumeContextWithGUID((PWCHAR)Irp->AssociatedIrp.SystemBuffer);
            }
        } else {
            pVolumeContext = DeviceExtension->pVolumeContext;
            ASSERT(NULL != pVolumeContext);
            ReferenceVolumeContext(pVolumeContext);
        }        
    } else {
        
        PLIST_ENTRY         pCurr = DriverContext.HeadForVolumeContext.Flink;
        KIRQL               OldIrql; 

        KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
        while (pCurr != &DriverContext.HeadForVolumeContext) {
            pVolumeContext = (PVOLUME_CONTEXT)pCurr;
            if(pVolumeContext->TargetDeviceObject == DeviceObject) {
                break;
            }
            pCurr = pCurr->Flink;
            pVolumeContext = NULL;
        }

        KeReleaseSpinLock(&DriverContext.Lock, OldIrql);
    }
    return pVolumeContext;
}

NTSTATUS
DeviceIoControlSetDirtyBlockNotifyEvent(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status;
    PVOLUME_CONTEXT     VolumeContext = NULL;
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

    VolumeContext = GetVolumeContextForThisIOCTL(DeviceObject, Irp);
    if (VolumeContext) {
        PRKEVENT   DBNotifyEvent = NULL;
        HANDLE     hEvent;

#ifdef _WIN64
        if (DriverContext.bS2is64BitProcess) {
            PVOLUME_DB_EVENT_INFO   DBEventInfo = (PVOLUME_DB_EVENT_INFO)Irp->AssociatedIrp.SystemBuffer;
            hEvent = DBEventInfo->hEvent;
        } else {
            PVOLUME_DB_EVENT_INFO32   DBEventInfo = (PVOLUME_DB_EVENT_INFO32)Irp->AssociatedIrp.SystemBuffer;
            ULONGLONG ullEvent = DBEventInfo->hEvent;
            hEvent = (HANDLE)ullEvent;
        }
#else
        PVOLUME_DB_EVENT_INFO   DBEventInfo = (PVOLUME_DB_EVENT_INFO)Irp->AssociatedIrp.SystemBuffer;
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
            KeAcquireSpinLock(&VolumeContext->Lock, &OldIrql);
            if (VolumeContext->DBNotifyEvent) {
                InVolDbgPrint(DL_WARNING, DM_INMAGE_IOCTL, ("DBNotifyEvent is already set in Volume context\n"));
                ObDereferenceObject(VolumeContext->DBNotifyEvent);
                VolumeContext->DBNotifyEvent = NULL;
            }
            VolumeContext->DBNotifyEvent = DBNotifyEvent;
            VolumeContext->liTickCountAtNotifyEventSet.QuadPart = 0;
            KeReleaseSpinLock(&VolumeContext->Lock, OldIrql);
            Status = STATUS_SUCCESS;
        }
        DereferenceVolumeContext(VolumeContext);
    } else {
        Status = STATUS_UNSUCCESSFUL;
    }

    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS StopFiltering(PVOLUME_CONTEXT  pVolumeContext, bool bDeleteBitmapFile) {
    NTSTATUS            Status = STATUS_SUCCESS;
    KIRQL               OldIrql;
    bool                bChanged = false;
    PVOLUME_BITMAP      pVolumeBitmap = NULL;
    bool                bFilteringAlreadyStopped = false;
	bool                bBitmapOpened = false;
    
    
    if (NULL == pVolumeContext) {
        Status = STATUS_UNSUCCESSFUL;
        return Status;
    }
    // This IOCTL should be treated as nop if already volume is stopped from filtering.
    // no registry writes should happen in nop condition
    KeWaitForMutexObject(&pVolumeContext->BitmapOpenCloseMutex, Executive, KernelMode, FALSE, NULL);
    KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
    if (0 == (pVolumeContext->ulFlags & VCF_FILTERING_STOPPED)) {
        if (bDeleteBitmapFile) {
            StopFilteringDevice(pVolumeContext, true, false, true, &pVolumeBitmap);
        } else {
            StopFilteringDevice(pVolumeContext, true, false, false, &pVolumeBitmap);
        }
        bChanged = true;
    } else if (bDeleteBitmapFile) {
        pVolumeBitmap = pVolumeContext->pVolumeBitmap;
        pVolumeContext->pVolumeBitmap = NULL;
        ASSERT((ecCaptureModeUninitialized == pVolumeContext->eCaptureMode) &&
               (ecWriteOrderStateUnInitialized == pVolumeContext->ePrevWOState) &&
               (ecWriteOrderStateUnInitialized == pVolumeContext->eWOState));
        // Below assignment for eCaptureMode, ePrevWOState, eWOState are part of 
        // defensive programming. These values should already have been in this state.
        // The above ASSERT enforces this.
        pVolumeContext->eCaptureMode = ecCaptureModeUninitialized;
        pVolumeContext->ePrevWOState = ecWriteOrderStateUnInitialized;
        pVolumeContext->eWOState = ecWriteOrderStateUnInitialized;
        bChanged = true;
        bFilteringAlreadyStopped = true;
    }
    if ((IS_CLUSTER_VOLUME(pVolumeContext)) && (!bDeleteBitmapFile) && (NULL == pVolumeContext->pVolumeBitmap)) {
        pVolumeContext->ulFlags |= VCF_SAVE_FILTERING_STATE_TO_BITMAP;
        if (false == bChanged)
            bChanged = true;
    }
    KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);

    if (bChanged) {
        if (NULL != pVolumeBitmap) {
            if (bDeleteBitmapFile) {
                BitmapPersistentValues BitmapPersistentValue(pVolumeContext);
                KeQuerySystemTime(&pVolumeContext->liDeleteBitmapTimeStamp);
                CloseBitmapFile(pVolumeBitmap, true, BitmapPersistentValue, true);
            } else {
                if (IS_CLUSTER_VOLUME(pVolumeContext)) {
                    pVolumeContext->pVolumeBitmap->pBitmapAPI->SaveFilteringStateToBitmap(true, true, true);
                }
                ClearBitmapFile(pVolumeBitmap);
            }
        } else {
            if (IS_CLUSTER_VOLUME(pVolumeContext) && (!bDeleteBitmapFile)) {
                pVolumeBitmap = OpenBitmapFile(pVolumeContext, Status);
                if (pVolumeBitmap) {
                    KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
                    pVolumeContext->pVolumeBitmap = ReferenceVolumeBitmap(pVolumeBitmap);
                    if(pVolumeContext->bQueueChangesToTempQueue) {
                        MoveUnupdatedDirtyBlocks(pVolumeContext);
                    }
                    KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);
                    pVolumeContext->pVolumeBitmap->pBitmapAPI->SaveFilteringStateToBitmap(true, true, true);
                    bBitmapOpened = true;
                } else {
                    KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
                    SetBitmapOpenError(pVolumeContext, true, Status);
                    pVolumeContext->ulFlags |= VCF_OPEN_BITMAP_REQUESTED;
                    KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);
                    KeSetEvent(&DriverContext.ServiceThreadWakeEvent, IO_DISK_INCREMENT, FALSE);
                }
                Status = STATUS_SUCCESS;
            }
        }

        if (!bFilteringAlreadyStopped && IS_NOT_CLUSTER_VOLUME(pVolumeContext))
            SetDWORDInRegVolumeCfg(pVolumeContext, VOLUME_FILTERING_DISABLED, VOLUME_FILTERING_DISABLED_SET, 1);
    } else {
        InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                        ("DeviceIoControlStopFiltering: Filtering already disabled on volume %S(%S)\n",
                            pVolumeContext->wGUID, pVolumeContext->wDriveLetter));
    }
    KeReleaseMutex(&pVolumeContext->BitmapOpenCloseMutex, FALSE);
    if (bBitmapOpened) {
        DereferenceVolumeBitmap(pVolumeBitmap);
    }
    return Status;
}

NTSTATUS
DeviceIoControlStopFiltering(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status = STATUS_SUCCESS;
    NTSTATUS            FinalStatus = STATUS_UNSUCCESSFUL;
    PVOLUME_CONTEXT     pVolumeContext = NULL;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    bool                bDeleteBitmapFile = false;
    ULONG ulFlags = 0;
    LIST_ENTRY  VolumeNodeList;
    InitializeListHead(&VolumeNodeList);

    ASSERT(IOCTL_INMAGE_STOP_FILTERING_DEVICE == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

    if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength >= sizeof(STOP_FILTERING_INPUT)) {
        PSTOP_FILTERING_INPUT Input = (PSTOP_FILTERING_INPUT)Irp->AssociatedIrp.SystemBuffer;
        ulFlags = Input->ulFlags;
        if (ulFlags & STOP_FILTERING_FLAGS_DELETE_BITMAP)
            bDeleteBitmapFile = true;
    } else {
        FinalStatus = STATUS_BUFFER_TOO_SMALL;
        goto Cleanup;
    }

    if (!(ulFlags & STOP_ALL_FILTERING_FLAGS)) {
        pVolumeContext = GetVolumeContextForThisIOCTL(DeviceObject, Irp);
        if (NULL == pVolumeContext) {
            FinalStatus = STATUS_NO_SUCH_DEVICE;
            goto Cleanup;
        }
    } else {
        Status = GetListOfVolumes(&VolumeNodeList);
        if (!NT_SUCCESS(Status)) {
            FinalStatus = Status;
            goto Cleanup;
        }
        if (IsListEmpty(&VolumeNodeList)) {
            FinalStatus = STATUS_INVALID_PARAMETER;
            goto Cleanup;
        }
    }
    FinalStatus = STATUS_SUCCESS;
    while ((NULL != pVolumeContext) || (!IsListEmpty(&VolumeNodeList))) {
        if (NULL == pVolumeContext) {
            PLIST_NODE      pNode;
            pNode = (PLIST_NODE)RemoveHeadList(&VolumeNodeList);
	        if (NULL != pNode) {
                pVolumeContext = (PVOLUME_CONTEXT)pNode->pData;
                DereferenceListNode(pNode);
            }
        }
        Status = StopFiltering(pVolumeContext,  bDeleteBitmapFile);
	if (NULL != pVolumeContext) {
            DereferenceVolumeContext(pVolumeContext);
        }
        if ((!NT_SUCCESS(Status)) && (NT_SUCCESS(FinalStatus))) {
            FinalStatus = Status;
        }
        pVolumeContext = NULL;
    }
Cleanup:
    Irp->IoStatus.Status = FinalStatus;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return FinalStatus;
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
DeviceIoControlGetVersion(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status;
    PVOLUME_CONTEXT     pVolumeContext = NULL;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    KIRQL               OldIrql;
    BOOLEAN             bChanged = FALSE;
    PVOLUME_BITMAP      pVolumeBitmap = NULL;

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

    DriverVersion->ulDrMajorVersion = DRIVER_MAJOR_VERSION;
    DriverVersion->ulDrMinorVersion = DRIVER_MINOR_VERSION;
    DriverVersion->ulDrMinorVersion2 = DRIVER_MINOR_VERSION2;
    DriverVersion->ulDrMinorVersion3 = DRIVER_MINOR_VERSION3;

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
DeviceIoControlSetVolumeFlags(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status;
    KIRQL               OldIrql;
    PVOLUME_CONTEXT     pVolumeContext = NULL;
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

    pVolumeContext = GetVolumeContextForThisIOCTL(DeviceObject, Irp);
    if (pVolumeContext) {
        Status = STATUS_SUCCESS;
        KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
        if (ecBitOpSet == eInputBitOperation) {
            if (ulInputFlags & VOLUME_FLAG_IGNORE_PAGEFILE_WRITES) {
                if (0 == (pVolumeContext->ulFlags & VCF_PAGEFILE_WRITES_IGNORED)) {
                    pVolumeContext->ulFlags |= VCF_PAGEFILE_WRITES_IGNORED;
                } else {
                    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                                  ("DeviceIoControlSetVolumeFlags: Already pagefile write ignored flag is set\n"));
                    ulInputFlags &= ~VOLUME_FLAG_IGNORE_PAGEFILE_WRITES;
                }
            }
            
            if (ulInputFlags & VOLUME_FLAG_DISABLE_BITMAP_READ) {
                if (0 == (pVolumeContext->ulFlags & VCF_BITMAP_READ_DISABLED)) {
                    pVolumeContext->ulFlags |= VCF_BITMAP_READ_DISABLED;
                } else {
                    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                                  ("DeviceIoControlSetVolumeFlags: Already bitmap read disabled flag is set\n"));
                    ulInputFlags &= ~VOLUME_FLAG_DISABLE_BITMAP_READ;
                }
            }

            if (ulInputFlags & VOLUME_FLAG_DISABLE_BITMAP_WRITE) {
                if (0 == (pVolumeContext->ulFlags & VCF_BITMAP_WRITE_DISABLED)) {
                    pVolumeContext->ulFlags |= VCF_BITMAP_WRITE_DISABLED;
                } else {
                    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                                  ("DeviceIoControlSetVolumeFlags: Already bitmap write disabled flag is set\n"));
                    ulInputFlags &= ~VOLUME_FLAG_DISABLE_BITMAP_WRITE;
                }
            }

            if (ulInputFlags & VOLUME_FLAG_ONLINE) {
                if(0 == (pVolumeContext->ulFlags & VCF_CLUSTER_VOLUME_ONLINE))  {

                    pVolumeContext->ulFlags |= VCF_CLUSTER_VOLUME_ONLINE;
                    pVolumeContext->ulFlags &= ~VCF_CV_FS_UNMOUTNED;
                    pVolumeContext->ulFlags &= ~VCF_CLUSDSK_RETURNED_OFFLINE;
                } else {
                    
                    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                                  ("DeviceIoControlSetVolumeFlags: Already cluster volume online flag is set\n"));
                    ulInputFlags &= ~VOLUME_FLAG_ONLINE;
                }

            }

            if (ulInputFlags & VOLUME_FLAG_READ_ONLY)
            {
                if (0 == (pVolumeContext->ulFlags & VCF_READ_ONLY)) {
                    if((pVolumeContext->ulFlags & VCF_FILTERING_STOPPED) || (ulInputFlags & VOLUME_FLAG_ENABLE_SOURCE_ROLLBACK)) {
                        pVolumeContext->ulFlags |= VCF_READ_ONLY;
                    } else {
                        //
                        //Clear the input volume flag so that the value in registry remains unmodified in the subsequent code
                        //Set the status as invalid parameter.
                        //
                        ulInputFlags &= ~VOLUME_FLAG_READ_ONLY;
                        Status = STATUS_INVALID_PARAMETER;
                        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL,
                            ("DeviceIoControlSetVolumeFlags: IOCTL rejected, conflicting settings for volume being filtered\n"));
                    }
                } else {
                    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                                  ("DeviceIoControlSetVolumeFlags: Already volume is set readonly\n"));
                    ulInputFlags &= ~VOLUME_FLAG_READ_ONLY;
                }
            }

            if (ulInputFlags & VOLUME_FLAG_DISABLE_DATA_CAPTURE) {
                if (0 == (pVolumeContext->ulFlags & VCF_DATA_CAPTURE_DISABLED)) {
                    pVolumeContext->ulFlags |= VCF_DATA_CAPTURE_DISABLED;
                    VCSetCaptureModeToMetadata(pVolumeContext, true);
                    VCSetWOState(pVolumeContext,true,__FUNCTION__);
                } else {
                    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                                  ("DeviceIoControlSetVolumeFlags: Already data capture disabled flag is set\n"));
                    ulInputFlags &= ~VOLUME_FLAG_DISABLE_DATA_CAPTURE;
                }
            }

            if (ulInputFlags & VOLUME_FLAG_FREE_DATA_BUFFERS) {
                // Have to free all data buffers for the volume.
                VCFreeAllDataBuffers(pVolumeContext, true);
            }

            if (ulInputFlags & VOLUME_FLAG_DISABLE_DATA_FILES) {
                if (0 == (pVolumeContext->ulFlags & VCF_DATA_FILES_DISABLED)) {
                    pVolumeContext->ulFlags |= VCF_DATA_FILES_DISABLED;
                } else {
                    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                                  ("%s: Already data files disabled flag is set\n", __FUNCTION__));
                    ulInputFlags &= ~VCF_DATA_FILES_DISABLED;
                }
            }

            if (ulInputFlags & VOLUME_FLAG_VCFIELDS_PERSISTED) {
                if (0 == (pVolumeContext->ulFlags & VCF_VCONTEXT_FIELDS_PERSISTED)) {
                    pVolumeContext->ulFlags |= VCF_VCONTEXT_FIELDS_PERSISTED;
                } else {
                    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                                  ("%s: Already Persist VolumeContext flas is set \n", __FUNCTION__));
                    ulInputFlags &= ~VOLUME_FLAG_VCFIELDS_PERSISTED;
                }
            }
        } else {
            if (ulInputFlags & VOLUME_FLAG_IGNORE_PAGEFILE_WRITES) {
                if (pVolumeContext->ulFlags & VCF_PAGEFILE_WRITES_IGNORED) {
                    pVolumeContext->ulFlags &= ~VCF_PAGEFILE_WRITES_IGNORED;
                } else {
                    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                                  ("DeviceIoControlSetVolumeFlags: Already pagefile write flag is unset\n"));
                    ulInputFlags &= ~VOLUME_FLAG_IGNORE_PAGEFILE_WRITES;
                }
            }
            
            if (ulInputFlags & VOLUME_FLAG_DISABLE_BITMAP_READ) {
                if (pVolumeContext->ulFlags & VCF_BITMAP_READ_DISABLED) {
                    pVolumeContext->ulFlags &= ~VCF_BITMAP_READ_DISABLED;                   
                } else {
                    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                                  ("DeviceIoControlSetVolumeFlags: Already disable bitmap read flag is unset\n"));
                    ulInputFlags &= ~VOLUME_FLAG_DISABLE_BITMAP_READ;
                }
            }

            if (ulInputFlags & VOLUME_FLAG_DISABLE_BITMAP_WRITE) {
                if (pVolumeContext->ulFlags & VCF_BITMAP_WRITE_DISABLED) {
                    pVolumeContext->ulFlags &= ~VCF_BITMAP_WRITE_DISABLED;
                } else {
                    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                                  ("DeviceIoControlSetVolumeFlags: Already disable bitmap read flag is unset\n"));
                    ulInputFlags &= ~VOLUME_FLAG_DISABLE_BITMAP_WRITE;
                }
            }

            if (ulInputFlags & VOLUME_FLAG_ONLINE) {
                if(pVolumeContext->ulFlags & VCF_CLUSTER_VOLUME_ONLINE)  {

                    pVolumeContext->ulFlags &= ~VCF_CLUSTER_VOLUME_ONLINE;
                } else {
                    
                    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                                  ("DeviceIoControlSetVolumeFlags: Already cluster volume online flag is unset\n"));
                    ulInputFlags &= ~VOLUME_FLAG_ONLINE;
                }

            }

            if (ulInputFlags & VOLUME_FLAG_READ_ONLY) {
                if (pVolumeContext->ulFlags & VCF_READ_ONLY) {
                    pVolumeContext->ulFlags &= ~VCF_READ_ONLY;
                } else {
                    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                                  ("DeviceIoControlSetVolumeFlags: Already volume read only flag is unset\n"));
                    ulInputFlags &= ~VOLUME_FLAG_READ_ONLY;
                }
            }

            if (ulInputFlags & VOLUME_FLAG_DISABLE_DATA_CAPTURE) {
                if (pVolumeContext->ulFlags & VCF_DATA_CAPTURE_DISABLED) {
                    pVolumeContext->ulFlags &= ~VCF_DATA_CAPTURE_DISABLED;
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
                if (pVolumeContext->ulFlags & VCF_DATA_FILES_DISABLED) {
                    pVolumeContext->ulFlags &= ~VCF_DATA_FILES_DISABLED;
                } else {
                    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                                  ("%s: Already data files disabled flag is unset\n", __FUNCTION__));
                    ulInputFlags &= ~VOLUME_FLAG_DISABLE_DATA_FILES;
                }
            }

            if (ulInputFlags & VOLUME_FLAG_VCFIELDS_PERSISTED) {
                if (pVolumeContext->ulFlags & VCF_VCONTEXT_FIELDS_PERSISTED) {
                    pVolumeContext->ulFlags &= ~VCF_VCONTEXT_FIELDS_PERSISTED;
                } else {
                    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                                  ("%s: Already Persist VolumeContext flag is unset \n", __FUNCTION__));
                    ulInputFlags &= ~VOLUME_FLAG_VCFIELDS_PERSISTED;
                }
            }
        }
        PVOLUME_FLAGS_OUTPUT    pVolumeFlagsOutput = NULL;
        if( IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength >= sizeof(VOLUME_FLAGS_OUTPUT) ) {

            pVolumeFlagsOutput = (PVOLUME_FLAGS_OUTPUT)Irp->AssociatedIrp.SystemBuffer;
            pVolumeFlagsOutput->ulVolumeFlags = 0;

            if (pVolumeContext->ulFlags & VCF_PAGEFILE_WRITES_IGNORED)
                pVolumeFlagsOutput->ulVolumeFlags |= VOLUME_FLAG_IGNORE_PAGEFILE_WRITES;

            if (pVolumeContext->ulFlags & VCF_BITMAP_READ_DISABLED)
                pVolumeFlagsOutput->ulVolumeFlags |= VOLUME_FLAG_DISABLE_BITMAP_READ;

            if (pVolumeContext->ulFlags & VCF_BITMAP_WRITE_DISABLED)
                pVolumeFlagsOutput->ulVolumeFlags |= VOLUME_FLAG_DISABLE_BITMAP_WRITE;

            if (pVolumeContext->ulFlags & VCF_READ_ONLY)
                pVolumeFlagsOutput->ulVolumeFlags |= VOLUME_FLAG_READ_ONLY;

            if (pVolumeContext->ulFlags & VCF_DATA_CAPTURE_DISABLED)
                pVolumeFlagsOutput->ulVolumeFlags |= VOLUME_FLAG_DISABLE_DATA_CAPTURE;

            if (pVolumeContext->ulFlags & VCF_DATA_FILES_DISABLED)
                pVolumeFlagsOutput->ulVolumeFlags |= VOLUME_FLAG_DISABLE_DATA_FILES;

            if (pVolumeContext->ulFlags & VCF_VCONTEXT_FIELDS_PERSISTED )
                pVolumeFlagsOutput->ulVolumeFlags |= VOLUME_FLAG_VCFIELDS_PERSISTED;

            ValidOutBuff = 1;
            Irp->IoStatus.Information = sizeof(VOLUME_FLAGS_OUTPUT);
        }

        KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);

        if (ulInputFlags & VOLUME_FLAG_DATA_LOG_DIRECTORY) {
            if ((pVolumeFlagsInput->DataFile.usLength > 0) && (pVolumeFlagsInput->DataFile.usLength < UDIRTY_BLOCK_MAX_DATA_LOG_DIRECTORY)) {	
                if (pVolumeFlagsInput->DataFile.FileName[pVolumeFlagsInput->DataFile.usLength/sizeof(WCHAR)] == L'\0') {
                    NTSTATUS StatusVol = KeWaitForSingleObject(&pVolumeContext->Mutex, Executive, KernelMode, FALSE, NULL);                         
                    ASSERT(STATUS_SUCCESS == StatusVol);
                    StatusVol = InitializeDataLogDirectory(pVolumeContext, pVolumeFlagsInput->DataFile.FileName, 1);
                    KeReleaseMutex(&pVolumeContext->Mutex, FALSE);
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
            if (ulInputFlags & VOLUME_FLAG_IGNORE_PAGEFILE_WRITES)
                SetDWORDInRegVolumeCfg(pVolumeContext, VOLUME_PAGEFILE_WRITES_IGNORED, VOLUME_PAGEFILE_WRITES_IGNORED_SET);
            
            if (ulInputFlags & VOLUME_FLAG_DISABLE_BITMAP_READ)
                SetDWORDInRegVolumeCfg(pVolumeContext, VOLUME_BITMAP_READ_DISABLED, VOLUME_BITMAP_READ_DISABLED_SET);

            if (ulInputFlags & VOLUME_FLAG_DISABLE_BITMAP_WRITE)
                SetDWORDInRegVolumeCfg(pVolumeContext, VOLUME_BITMAP_WRITE_DISABLED, VOLUME_BITMAP_WRITE_DISABLED_SET);

            if (ulInputFlags & VOLUME_FLAG_READ_ONLY)
                SetDWORDInRegVolumeCfg(pVolumeContext, VOLUME_READ_ONLY, VOLUME_READ_ONLY_SET);

            if (ulInputFlags & VOLUME_FLAG_DISABLE_DATA_CAPTURE)
                SetDWORDInRegVolumeCfg(pVolumeContext, VOLUME_DATA_CAPTURE, VOLUME_DATA_CAPTURE_UNSET);
            
            if (ulInputFlags & VOLUME_FLAG_DISABLE_DATA_FILES)
                SetDWORDInRegVolumeCfg(pVolumeContext, VOLUME_DATA_FILES, VOLUME_DATA_FILES_UNSET);
            
        } else {
            if (ulInputFlags & VOLUME_FLAG_IGNORE_PAGEFILE_WRITES)
                SetDWORDInRegVolumeCfg(pVolumeContext, VOLUME_PAGEFILE_WRITES_IGNORED, VOLUME_PAGEFILE_WRITES_IGNORED_UNSET);
            
            if (ulInputFlags & VOLUME_FLAG_DISABLE_BITMAP_READ)
                SetDWORDInRegVolumeCfg(pVolumeContext, VOLUME_BITMAP_READ_DISABLED, VOLUME_BITMAP_READ_DISABLED_UNSET);

            if (ulInputFlags & VOLUME_FLAG_DISABLE_BITMAP_WRITE)
                SetDWORDInRegVolumeCfg(pVolumeContext, VOLUME_BITMAP_WRITE_DISABLED, VOLUME_BITMAP_WRITE_DISABLED_UNSET);

            if (ulInputFlags & VOLUME_FLAG_READ_ONLY)
                SetDWORDInRegVolumeCfg(pVolumeContext, VOLUME_READ_ONLY, VOLUME_READ_ONLY_UNSET);

            if (ulInputFlags & VOLUME_FLAG_DISABLE_DATA_CAPTURE)
                SetDWORDInRegVolumeCfg(pVolumeContext, VOLUME_DATA_CAPTURE, VOLUME_DATA_CAPTURE_SET);

            if (ulInputFlags & VOLUME_FLAG_DISABLE_DATA_FILES)
                SetDWORDInRegVolumeCfg(pVolumeContext, VOLUME_DATA_FILES, VOLUME_DATA_FILES_SET);
     
        }

        DereferenceVolumeContext(pVolumeContext);
    }
    else
    {
        Status = STATUS_UNSUCCESSFUL;
    }

    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

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
            if (DriverContext.bEnableDataFilesForNewVolumes) {
                DriverContext.bEnableDataFilesForNewVolumes = false;
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
            if (!DriverContext.bEnableDataFilesForNewVolumes) {
                DriverContext.bEnableDataFilesForNewVolumes = true;
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

        if (!DriverContext.bEnableDataFilesForNewVolumes) {
            pDriverFlagsOutput->ulFlags |= DRIVER_FLAG_DISABLE_DATA_FILES_FOR_NEW_VOLUMES;
        }

        Irp->IoStatus.Information = sizeof(DRIVER_FLAGS_OUTPUT);
    }

    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

    // Do any passive level operation that have to be performed.
    if (ecBitOpSet == eInputBitOperation) {
        if (ulInputFlags & DRIVER_FLAG_DISABLE_DATA_FILES)
            SetDWORDInParameters(VOLUME_DATA_FILES, VOLUME_DATA_FILES_UNSET);
        if (ulInputFlags & DRIVER_FLAG_DISABLE_DATA_FILES_FOR_NEW_VOLUMES)
            SetDWORDInParameters(VOLUME_DATA_FILES_FOR_NEW_VOLUMES, VOLUME_DATA_FILES_UNSET);
    } else {
        if (ulInputFlags & DRIVER_FLAG_DISABLE_DATA_FILES)
            SetDWORDInParameters(VOLUME_DATA_FILES, VOLUME_DATA_FILES_SET);
        if (ulInputFlags & DRIVER_FLAG_DISABLE_DATA_FILES_FOR_NEW_VOLUMES)
            SetDWORDInParameters(VOLUME_DATA_FILES_FOR_NEW_VOLUMES, VOLUME_DATA_FILES_SET);
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
    ULONG               ulInputFlags;
    etBitOperation      eInputBitOperation;
    DRIVER_CONFIG       DriverInput = {0};
    PDRIVER_CONFIG      pDriverConfig = (PDRIVER_CONFIG)Irp->AssociatedIrp.SystemBuffer;
    bool ValidOutBuff = 0;
    // OutPutFlag should be set for the changed tunables, which were not requested 
    ULONG OutPutFlag = 0;
    ULONG ulError = 0;

    ASSERT(IOCTL_INMAGE_SET_DRIVER_CONFIGURATION == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);
    Irp->IoStatus.Information = 0;

    if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < sizeof(DRIVER_CONFIG)) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL, ("%s: IOCTL rejected, input buffer too small\n", __FUNCTION__));
        Status = STATUS_BUFFER_TOO_SMALL;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

   
    DriverInput.ulFlags1 = pDriverConfig->ulFlags1;
    DriverInput.ulhwmServiceNotStarted = pDriverConfig->ulhwmServiceNotStarted;
    DriverInput.ulhwmServiceRunning = pDriverConfig->ulhwmServiceRunning;
    DriverInput.ulhwmServiceShutdown = pDriverConfig->ulhwmServiceShutdown;
    DriverInput.ullwmServiceRunning = pDriverConfig->ullwmServiceRunning;
    DriverInput.ulChangesToPergeOnhwm = pDriverConfig->ulChangesToPergeOnhwm;
    DriverInput.ulSecsBetweenMemAllocFailureEvents = pDriverConfig->ulSecsBetweenMemAllocFailureEvents;
    DriverInput.ulSecsMaxWaitTimeOnFailOver =  pDriverConfig->ulSecsMaxWaitTimeOnFailOver;
    DriverInput.ulMaxLockedDataBlocks = pDriverConfig->ulMaxLockedDataBlocks;
    DriverInput.ulMaxNonPagedPoolInMB = pDriverConfig->ulMaxNonPagedPoolInMB;
    DriverInput.ulMaxWaitTimeForIrpCompletionInMinutes = pDriverConfig->ulMaxWaitTimeForIrpCompletionInMinutes;
    DriverInput.bDisableVolumeFilteringForNewClusterVolumes = pDriverConfig->bDisableVolumeFilteringForNewClusterVolumes;
    DriverInput.bDisableVolumeFilteringForNewVolumes = pDriverConfig->bDisableVolumeFilteringForNewVolumes;
    DriverInput.bEnableDataFilteringForNewVolumes = pDriverConfig->bEnableDataFilteringForNewVolumes;
    DriverInput.bEnableDataCapture = pDriverConfig->bEnableDataCapture;    
    DriverInput.ulAvailableDataBlockCountInPercentage = pDriverConfig->ulAvailableDataBlockCountInPercentage;
    DriverInput.ulMaxDataAllocLimitInPercentage = pDriverConfig->ulMaxDataAllocLimitInPercentage;
    DriverInput.ulMaxCoalescedMetaDataChangeSize = pDriverConfig->ulMaxCoalescedMetaDataChangeSize;
    DriverInput.ulValidationLevel = pDriverConfig->ulValidationLevel;
    DriverInput.ulDataPoolSize = pDriverConfig->ulDataPoolSize;
    DriverInput.ulMaxDataSizePerVolume = pDriverConfig->ulMaxDataSizePerVolume;
    DriverInput.ulDaBThresholdPerVolumeForFileWrite = pDriverConfig->ulDaBThresholdPerVolumeForFileWrite;
    DriverInput.ulDaBFreeThresholdForFileWrite = pDriverConfig->ulDaBFreeThresholdForFileWrite;
    DriverInput.ulMaxDataSizePerNonDataModeDirtyBlock = pDriverConfig->ulMaxDataSizePerNonDataModeDirtyBlock;
    DriverInput.ulMaxDataSizePerDataModeDirtyBlock = pDriverConfig->ulMaxDataSizePerDataModeDirtyBlock;
    DriverInput.DataFile.usLength = pDriverConfig->DataFile.usLength;
    if (DriverInput.DataFile.usLength > 0) {
        RtlMoveMemory(DriverInput.DataFile.FileName, pDriverConfig->DataFile.FileName,
                      DriverInput.DataFile.usLength);
        DriverInput.DataFile.FileName[DriverInput.DataFile.usLength/sizeof(WCHAR)] = L'\0';
    }
        
    // Check if Flag and bitop set in input buffer are all valid flags.
    if (0 != (DriverInput.ulFlags1 & (~DRIVER_CONFIG_FLAGS1_VALID))) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL,
            ("%s: IOCTL rejected, invalid parameters\n", __FUNCTION__));
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_HWM_SERVICE_NOT_STARTED) {
        DriverContext.DBHighWaterMarks[ecServiceNotStarted] = DriverInput.ulhwmServiceNotStarted;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_HWM_SERVICE_RUNNING) {
        DriverContext.DBHighWaterMarks[ecServiceRunning] = DriverInput.ulhwmServiceRunning;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_HWM_SERVICE_SHUTDOWN) {
        DriverContext.DBHighWaterMarks[ecServiceShutdown] = DriverInput.ulhwmServiceShutdown;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_LWM_SERVICE_RUNNING) {
        DriverContext.DBLowWaterMarkWhileServiceRunning = DriverInput.ullwmServiceRunning;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_CHANGES_TO_PURGE_ON_HWM) {
        DriverContext.DBToPurgeWhenHighWaterMarkIsReached = DriverInput.ulChangesToPergeOnhwm;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_TIME_BETWEEN_MEM_FAIL_LOG) {
        DriverContext.ulSecondsBetweenNoMemoryLogEvents = DriverInput.ulSecsBetweenMemAllocFailureEvents;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_MAX_WAIT_TIME_ON_FAILOVER) {
        DriverContext.ulSecondsMaxWaitTimeOnFailOver = DriverInput.ulSecsMaxWaitTimeOnFailOver;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_MAX_LOCKED_DATA_BLOCKS) {
        if (ValidateMaxSetLockedDataBlocks(DriverInput.ulMaxLockedDataBlocks) == DriverInput.ulMaxLockedDataBlocks) {
            DriverContext.ulMaxSetLockedDataBlocks = DriverInput.ulMaxLockedDataBlocks;
            SetMaxLockedDataBlocks();
        } else {
            ulError = ulError | DRIVER_CONFIG_FLAGS_MAX_LOCKED_DATA_BLOCKS;
        }
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_MAX_NON_PAGED_POOL) {
        DriverContext.ulMaxNonPagedPool = DriverInput.ulMaxNonPagedPoolInMB * MEGABYTES;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_MAX_WAIT_TIME_FOR_IRP_COMPLETION) {
        DriverContext.ulMaxWaitTimeForIrpCompletion = DriverInput.ulMaxWaitTimeForIrpCompletionInMinutes * SECONDS_IN_MINUTE;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_DEFAULT_FILTERING_FOR_NEW_CLUSTER_VOLUMES) {
        DriverContext.bDisableVolumeFilteringForNewClusterVolumes = DriverInput.bDisableVolumeFilteringForNewClusterVolumes;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_FITERING_DISABLED_FOR_NEW_VOLUMES) {
        DriverContext.bDisableVolumeFilteringForNewVolumes =  DriverInput.bDisableVolumeFilteringForNewVolumes;
    }    
    
    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_DATA_FILTER_FOR_NEW_VOLUMES) {
        DriverContext.bEnableDataFilteringForNewVolumes = DriverInput.bEnableDataFilteringForNewVolumes;
    }
    
    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_AVAILABLE_DATA_BLOCK_COUNT) {
        DriverContext.ulAvailableDataBlockCountInPercentage = DriverInput.ulAvailableDataBlockCountInPercentage;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_MAX_DATA_ALLOCATION_LIMIT) {
        DriverContext.ulMaxDataAllocLimitInPercentage = DriverInput.ulMaxDataAllocLimitInPercentage;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_MAX_COALESCED_METADATA_CHANGE_SIZE) {
        DriverContext.MaxCoalescedMetaDataChangeSize = DriverInput.ulMaxCoalescedMetaDataChangeSize;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_VALIDATION_LEVEL) {
        DriverContext.ulValidationLevel = DriverInput.ulValidationLevel;
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_VOL_THRESHOLD_FOR_FILE_WRITE) {
        ULONG ulDaBThresholdPerVolumeForFileWrite =  ValidateThresholdPerVolumeForFileWrite(DriverInput.ulDaBThresholdPerVolumeForFileWrite);
	if (ulDaBThresholdPerVolumeForFileWrite != DriverInput.ulDaBThresholdPerVolumeForFileWrite) {
            ulError = ulError | DRIVER_CONFIG_FLAGS_VOL_THRESHOLD_FOR_FILE_WRITE;
        } else {
            DriverContext.ulDaBThresholdPerVolumeForFileWrite = DriverInput.ulDaBThresholdPerVolumeForFileWrite;
        }
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_FREE_THRESHOLD_FOR_FILE_WRITE) {
        ULONG ulDaBFreeThresholdForFileWrite = ValidateFreeThresholdForFileWrite(DriverInput.ulDaBFreeThresholdForFileWrite);
	    if (ulDaBFreeThresholdForFileWrite != DriverInput.ulDaBFreeThresholdForFileWrite) {
            ulError = ulError | DRIVER_CONFIG_FLAGS_FREE_THRESHOLD_FOR_FILE_WRITE;
        } else {
            DriverContext.ulDaBFreeThresholdForFileWrite = DriverInput.ulDaBFreeThresholdForFileWrite;
        }
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_DATA_POOL_SIZE) {
        ULONGLONG ullDataPoolSize = ((ULONGLONG)DriverInput.ulDataPoolSize) * MEGABYTES;
        /* DataPoolSize shd be greater than MaxSetLockedDataBlocks */
        if ((ullDataPoolSize != ValidateDataPoolSize(ullDataPoolSize)) || (DriverInput.ulDataPoolSize < DriverContext.ulMaxSetLockedDataBlocks)) {
            ulError = ulError | DRIVER_CONFIG_DATA_POOL_SIZE;
        }
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_MAX_DATA_POOL_SIZE_PER_VOLUME) {
        ULONG ulMaxDataSizePerVolume = DriverInput.ulMaxDataSizePerVolume * KILOBYTES;
        DriverContext.ulMaxDataSizePerVolume = ValidateMaxDataSizePerVolume(ulMaxDataSizePerVolume);
    }
    
    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_MAX_DATA_PER_NON_DATA_MODE_DIRTY_BLOCK) {
        ULONG ulMaxDataSizePerNonDataModeDirtyBlock = DriverInput.ulMaxDataSizePerNonDataModeDirtyBlock * MEGABYTES;
        if (ValidateMaxDataSizePerNonDataModeDirtyBlock(ulMaxDataSizePerNonDataModeDirtyBlock ) == ulMaxDataSizePerNonDataModeDirtyBlock ) {
            DriverContext.ulMaxDataSizePerNonDataModeDirtyBlock = ulMaxDataSizePerNonDataModeDirtyBlock;
        } else {
            ulError = ulError | DRIVER_CONFIG_FLAGS_MAX_DATA_PER_NON_DATA_MODE_DIRTY_BLOCK;
        }
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_MAX_DATA_PER_DATA_MODE_DIRTY_BLOCK) {
        ULONG ulMaxDataSizePerDataModeDirtyBlock = DriverInput.ulMaxDataSizePerDataModeDirtyBlock * MEGABYTES;
        if (ValidateMaxDataSizePerDataModeDirtyBlock(ulMaxDataSizePerDataModeDirtyBlock ) == ulMaxDataSizePerDataModeDirtyBlock ) {
            DriverContext.ulMaxDataSizePerDataModeDirtyBlock = ulMaxDataSizePerDataModeDirtyBlock;
            ULONG ulMaxDataSizePerNonDataModeDirtyBlock = ValidateMaxDataSizePerNonDataModeDirtyBlock(DriverContext.ulMaxDataSizePerNonDataModeDirtyBlock);
            if (ulMaxDataSizePerNonDataModeDirtyBlock != DriverContext.ulMaxDataSizePerNonDataModeDirtyBlock) {
                OutPutFlag |= DRIVER_CONFIG_FLAGS_MAX_DATA_PER_NON_DATA_MODE_DIRTY_BLOCK;                
                DriverContext.ulMaxDataSizePerNonDataModeDirtyBlock = ulMaxDataSizePerNonDataModeDirtyBlock;
            }
            ULONG ulMaxDataSizePerVolume = ULONG((DriverContext.ullDataPoolSize / 100) * MAX_DATA_SIZE_PER_VOL_PERC);
            ulMaxDataSizePerVolume = ValidateMaxDataSizePerVolume(ulMaxDataSizePerVolume);
            if (ulMaxDataSizePerVolume != DriverContext.ulMaxDataSizePerVolume) {
                OutPutFlag |= DRIVER_CONFIG_MAX_DATA_POOL_SIZE_PER_VOLUME;                
                DriverContext.ulMaxDataSizePerVolume = ulMaxDataSizePerVolume;                
            }
        } else {
            ulError = ulError | DRIVER_CONFIG_FLAGS_MAX_DATA_PER_DATA_MODE_DIRTY_BLOCK;
        }
    }

    if (DriverInput.ulFlags1 & DRIVER_CONFIG_FLAGS_DATA_LOG_DIRECTORY) {
        if (DriverInput.DataFile.usLength > 0) {
            NTSTATUS StatusUni = InMageCopyUnicodeString(&DriverContext.DataLogDirectory, DriverInput.DataFile.FileName, NonPagedPool);
            if (!NT_SUCCESS(StatusUni)) {
                ulError = ulError | DRIVER_CONFIG_FLAGS_DATA_LOG_DIRECTORY;
            }
        } else {
            ulError = ulError | DRIVER_CONFIG_FLAGS_DATA_LOG_DIRECTORY;
        }
    }

    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength >= sizeof(DRIVER_CONFIG)) {
        if (DriverInput.ulFlags1 == 0) {
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
                                    DRIVER_CONFIG_FLAGS_DATA_LOG_DIRECTORY;
        } else {
            pDriverConfig->ulFlags1 = DriverInput.ulFlags1;
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
        pDriverConfig->bDisableVolumeFilteringForNewClusterVolumes = DriverContext.bDisableVolumeFilteringForNewClusterVolumes;
        pDriverConfig->bDisableVolumeFilteringForNewVolumes = DriverContext.bDisableVolumeFilteringForNewVolumes;
        pDriverConfig->bEnableDataCapture = DriverContext.bEnableDataCapture;
        if (DriverContext.bEnableDataFilteringForNewVolumes) {
            pDriverConfig->bEnableDataFilteringForNewVolumes = 1;
        } else {
            pDriverConfig->bEnableDataFilteringForNewVolumes = 0;
        }
        pDriverConfig->ulAvailableDataBlockCountInPercentage = DriverContext.ulAvailableDataBlockCountInPercentage;
        pDriverConfig->ulMaxDataAllocLimitInPercentage = DriverContext.ulMaxDataAllocLimitInPercentage;
        pDriverConfig->ulMaxCoalescedMetaDataChangeSize = DriverContext.MaxCoalescedMetaDataChangeSize;
        pDriverConfig->ulValidationLevel = DriverContext.ulValidationLevel;
        pDriverConfig->ulDataPoolSize = (ULONG)(DriverContext.ullDataPoolSize / MEGABYTES);
        pDriverConfig->ulMaxDataSizePerVolume = (ULONG)((DriverContext.ulMaxDataSizePerVolume) / KILOBYTES);
        pDriverConfig->ulDaBThresholdPerVolumeForFileWrite = DriverContext.ulDaBThresholdPerVolumeForFileWrite;
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

        Irp->IoStatus.Information = sizeof(DRIVER_CONFIG);
        ValidOutBuff = 1;
    }

    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

    OutPutFlag |=  DriverInput.ulFlags1;

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

    if ((OutPutFlag & DRIVER_CONFIG_FLAGS_DEFAULT_FILTERING_FOR_NEW_CLUSTER_VOLUMES) && 
            (!(ulError & DRIVER_CONFIG_FLAGS_DEFAULT_FILTERING_FOR_NEW_CLUSTER_VOLUMES))) {
        SetDWORDInParameters(VOLUME_FILTERING_DISABLED_FOR_NEW_CLUSTER_VOLUMES, DriverContext.bDisableVolumeFilteringForNewClusterVolumes ? 1 : 0);
    }

    if ((OutPutFlag & DRIVER_CONFIG_FLAGS_FITERING_DISABLED_FOR_NEW_VOLUMES) && 
            (!(ulError & DRIVER_CONFIG_FLAGS_FITERING_DISABLED_FOR_NEW_VOLUMES))) {
        SetDWORDInParameters(VOLUME_FILTERING_DISABLED_FOR_NEW_VOLUMES, DriverContext.bDisableVolumeFilteringForNewVolumes ? 1 : 0);
    }

    if ((OutPutFlag & DRIVER_CONFIG_FLAGS_DATA_FILTER_FOR_NEW_VOLUMES) && 
            (!(ulError & DRIVER_CONFIG_FLAGS_DATA_FILTER_FOR_NEW_VOLUMES))) {
        SetDWORDInParameters(VOLUME_DATA_FILTERING_FOR_NEW_VOLUMES, DriverContext.bEnableDataFilteringForNewVolumes ? 1 : 0);
    }

    if ((OutPutFlag & DRIVER_CONFIG_FLAGS_ENABLE_DATA_CAPTURE) && 
            (!(ulError & DRIVER_CONFIG_FLAGS_ENABLE_DATA_CAPTURE))) {
        /* Allocate or free Data Pool with respect to flag EnableDataCapture*/
        if (DriverContext.bEnableDataCapture !=  DriverInput.bEnableDataCapture) {
            DriverContext.bEnableDataCapture = DriverInput.bEnableDataCapture;
            if (DriverContext.bEnableDataCapture) {
                SetDataPool(1, DriverContext.ullDataPoolSize);
            } else {
                CleanDataPool();
            }
            SetDWORDInParameters(VOLUME_DATA_CAPTURE, DriverContext.bEnableDataCapture ? 1 : 0);
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
        SetDWORDInParameters(VOLUME_THRESHOLD_FOR_FILE_WRITE, DriverContext.ulDaBThresholdPerVolumeForFileWrite);
    }

    if ((OutPutFlag & DRIVER_CONFIG_FLAGS_FREE_THRESHOLD_FOR_FILE_WRITE) && 
            (!(ulError & DRIVER_CONFIG_FLAGS_FREE_THRESHOLD_FOR_FILE_WRITE))) {
        SetDWORDInParameters(FREE_THRESHOLD_FOR_FILE_WRITE, DriverContext.ulDaBFreeThresholdForFileWrite);
    }

    if ((OutPutFlag & DRIVER_CONFIG_DATA_POOL_SIZE) && 
            (!(ulError & DRIVER_CONFIG_DATA_POOL_SIZE))) {
        if (((ULONG)(DriverContext.ullDataPoolSize / MEGABYTES)) != DriverInput.ulDataPoolSize) {
            ULONGLONG ullDataPoolSize = ((ULONGLONG)DriverInput.ulDataPoolSize) * MEGABYTES;
            /* Data Pool Allocation or free is done with respect to new DataPoolSize.
               If required Data Pool cannot be allocated by system then old value is retained.
               Data Pool need to be Freed, It will be done as data is drained
               ulMaxDataSizePerVolume is reset to 80% of DataPoolSize*/
            NTSTATUS Status = SetDataPool(0, ullDataPoolSize);
            if (NT_SUCCESS(Status)) {
                ULONG ulDataPoolSize = (ULONG)(DriverContext.ullDataPoolSize / MEGABYTES);
                SetDWORDInParameters(DATA_POOL_SIZE, ulDataPoolSize);
                OutPutFlag |= DRIVER_CONFIG_MAX_DATA_POOL_SIZE_PER_VOLUME;
            } else {
                ulError = ulError | DRIVER_CONFIG_DATA_POOL_SIZE;
            }
        }
    }

    if ((OutPutFlag & DRIVER_CONFIG_MAX_DATA_POOL_SIZE_PER_VOLUME)  &&  
            (!(ulError & DRIVER_CONFIG_MAX_DATA_POOL_SIZE_PER_VOLUME))) {
        ULONG ulMaxDataSizePerVolume = (ULONG)(DriverContext.ulMaxDataSizePerVolume / KILOBYTES);
        SetDWORDInParameters(VOLUME_DATA_SIZE_LIMIT, ulMaxDataSizePerVolume);
    }
    
    if ((OutPutFlag & DRIVER_CONFIG_FLAGS_MAX_DATA_PER_NON_DATA_MODE_DIRTY_BLOCK) && 
            (!(ulError & DRIVER_CONFIG_FLAGS_MAX_DATA_PER_NON_DATA_MODE_DIRTY_BLOCK))) {
        SetDWORDInParameters(MAX_DATA_SIZE_PER_NON_DATA_MODE_DIRTY_BLOCK, DriverContext.ulMaxDataSizePerNonDataModeDirtyBlock / KILOBYTES);
    }
    
    if ((OutPutFlag & DRIVER_CONFIG_FLAGS_MAX_DATA_PER_DATA_MODE_DIRTY_BLOCK) && 
            (!(ulError & DRIVER_CONFIG_FLAGS_MAX_DATA_PER_DATA_MODE_DIRTY_BLOCK))) {
        SetDWORDInParameters(MAX_DATA_SIZE_PER_DATA_MODE_DIRTY_BLOCK, DriverContext.ulMaxDataSizePerDataModeDirtyBlock  / KILOBYTES);
    }

    if ((OutPutFlag & DRIVER_CONFIG_FLAGS_DATA_LOG_DIRECTORY) && 
            (!(ulError & DRIVER_CONFIG_FLAGS_DATA_LOG_DIRECTORY))) {
        SetStrInParameters(DATA_LOG_DIRECTORY, &DriverContext.DataLogDirectory);
        if ((OutPutFlag & DRIVER_CONFIG_FLAGS_DATA_LOG_DIRECTORY_ALL) && 
            (!(ulError & DRIVER_CONFIG_FLAGS_DATA_LOG_DIRECTORY_ALL))) {
            LIST_ENTRY  VolumeNodeList;
            NTSTATUS StatusVol = GetListOfVolumes(&VolumeNodeList);
            if (STATUS_SUCCESS == StatusVol) {
                 while(!IsListEmpty(&VolumeNodeList)) {
                     PLIST_NODE  pNode = (PLIST_NODE)RemoveHeadList(&VolumeNodeList);;
                     PVOLUME_CONTEXT VolumeContext = (PVOLUME_CONTEXT)pNode->pData;
                     DereferenceListNode(pNode);
                     if (0 != VolumeContext->DataLogDirectory.Length) {
                         StatusVol = KeWaitForSingleObject(&VolumeContext->Mutex, Executive, KernelMode, FALSE, NULL);                         
                         ASSERT(STATUS_SUCCESS == StatusVol);
                         StatusVol = InitializeDataLogDirectory(VolumeContext, DriverContext.DataLogDirectory.Buffer, 1);
                         KeReleaseMutex(&VolumeContext->Mutex, FALSE);
                    }
                    DereferenceVolumeContext(VolumeContext);
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
        pDriverConfig->ulMaxDataSizePerVolume = (ULONG)((DriverContext.ulMaxDataSizePerVolume) / KILOBYTES);
        pDriverConfig->bEnableDataCapture = DriverContext.bEnableDataCapture;
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
    PVOLUME_CONTEXT     VolumeContext;
    PKDIRTY_BLOCK       pDirtyBlock;
    LIST_ENTRY          LockedDataBlockHead;
    PDATA_BLOCK         DataBlock = NULL;
    ULONG               ulRecordSizeForTags = SVRecordSizeForTags(pSequenceOfTags);
    ASSERT(NULL != pTagInfo);
    ULONG               ulNumberOfVolumes = pTagInfo->ulNumberOfVolumes;
    bool                bFilteringNotEnabled = false;

    InVolDbgPrint(DL_FUNC_TRACE, DM_TAGS, ("TagVolumesAutomically: Called\n"));

    InitializeListHead(&LockedDataBlockHead);
    for (i = 0; i < ulNumberOfVolumes; i++) {
        DataBlock = AllocateLockedDataBlock(TRUE);
        if (NULL != DataBlock) {
            InsertHeadList(&LockedDataBlockHead, &DataBlock->ListEntry);
        }
    }

    UpdateLockedDataBlockList(DriverContext.ulMaxLockedDataBlocks + ulNumberOfVolumes);
    DataBlock = NULL;
    KeAcquireSpinLock(&DriverContext.MultiVolumeLock, &OldIrql);

    //Acquire all volume locks, and tag lastdirtyblock in volume of lastchange
    for (i = 0; i < ulNumberOfVolumes; i++) {
        VolumeContext = pTagInfo->TagStatus[i].VolumeContext;
        if (NULL != VolumeContext) {
            KeAcquireSpinLockAtDpcLevel(&VolumeContext->Lock);
            if (0 == (VolumeContext->ulFlags & VCF_FILTERING_STOPPED)) {
                // Tag last time stamp for all dirty blocks in volume.
                if (NULL == DataBlock) {
                    if (!IsListEmpty(&LockedDataBlockHead)) {
                        DataBlock = (PDATA_BLOCK)RemoveHeadList(&LockedDataBlockHead);
                    }
                }
                AddLastChangeTimestampToCurrentNode(VolumeContext, &DataBlock);
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
    
        for (i = 0; ((STATUS_SUCCESS == Status) && (i < ulNumberOfVolumes)); i++) {
            VolumeContext = pTagInfo->TagStatus[i].VolumeContext;
            
            if (STATUS_PENDING == pTagInfo->TagStatus[i].Status) {
                ASSERT(NULL != VolumeContext);
                if (pTagInfo->ulFlags & TAG_INFO_FLAGS_WAIT_FOR_COMMIT) {
                    Status = AddUserDefinedTags(VolumeContext, pSequenceOfTags, &pTimeStamp, &liTickCount, &pTagInfo->TagGUID, i);
                } else {
                    Status = AddUserDefinedTags(VolumeContext, pSequenceOfTags, &pTimeStamp, &liTickCount, NULL, 0);
                }
                pTagInfo->TagStatus[i].Status = Status;
            }    
        }
    }

    // Release all locks;
    for (i = ulNumberOfVolumes; i != 0; i--) {
        VolumeContext = pTagInfo->TagStatus[i - 1].VolumeContext;
        if (NULL != VolumeContext) {
            KeReleaseSpinLockFromDpcLevel(&VolumeContext->Lock);        
        }
    }

    KeReleaseSpinLock(&DriverContext.MultiVolumeLock, OldIrql);


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
    PUCHAR          pSequenceOfTags
    )
{
    KIRQL               OldIrql;
    ULONG               i;
    PVOLUME_CONTEXT     VolumeContext;
    PKDIRTY_BLOCK       pDirtyBlock;
    NTSTATUS            Status = STATUS_SUCCESS;
    PDATA_BLOCK         DataBlock = NULL;
    ULONG               ulNumberOfVolumes = pTagInfo->ulNumberOfVolumes;

    InVolDbgPrint(DL_FUNC_TRACE, DM_TAGS, ("TagVolumesInSequence: Called\n"));

    for (i = 0; (STATUS_SUCCESS == Status) && (i < ulNumberOfVolumes); i++) {
        VolumeContext = pTagInfo->TagStatus[i].VolumeContext;
        if (NULL == DataBlock) {
            DataBlock = AllocateLockedDataBlock(TRUE);            
        }

        if (NULL != VolumeContext) {
            if (0 == (VolumeContext->ulFlags & VCF_FILTERING_STOPPED)) {
                KeAcquireSpinLock(&VolumeContext->Lock, &OldIrql);
                AddLastChangeTimestampToCurrentNode(VolumeContext, &DataBlock);
                if (pTagInfo->ulFlags & TAG_INFO_FLAGS_WAIT_FOR_COMMIT) {
                    Status = AddUserDefinedTags(VolumeContext, pSequenceOfTags, NULL, NULL, &pTagInfo->TagGUID, i);
                } else {
                    Status = AddUserDefinedTags(VolumeContext, pSequenceOfTags, NULL, NULL, NULL, 0);
                }
                KeReleaseSpinLock(&VolumeContext->Lock, OldIrql);
                pTagInfo->TagStatus[i].Status = Status;
            } else {
                if (0 == (pTagInfo->ulFlags & TAG_INFO_FLAGS_IGNORE_IF_FILTERING_STOPPED)) {
                    Status = STATUS_INVALID_DEVICE_STATE;
                    pTagInfo->TagStatus[i].Status = Status;
                } else {
                    pTagInfo->TagStatus[i].Status = STATUS_INVALID_DEVICE_STATE;
                }
            }
        } else {
            ASSERT(STATUS_NOT_FOUND == pTagInfo->TagStatus[i].Status);
        }
    }

    if (NULL != DataBlock) {
        AddDataBlockToLockedDataBlockList(DataBlock);
    }

    return Status;
}

NTSTATUS
DeviceIoControlTagVolume(
    PDEVICE_OBJECT DeviceObject, 
    PIRP Irp
    )
{
    NTSTATUS Status                     = STATUS_SUCCESS;
    ULONG ulNumberOfVolumes             = 0;
    ULONG ulTagStartOffset              = 0;
    PUCHAR pBuffer                      = (PUCHAR) Irp->AssociatedIrp.SystemBuffer;
    PIO_STACK_LOCATION IoStackLocation  = IoGetCurrentIrpStackLocation(Irp);
    ULONG IoctlTagVolumeFlags           = 0;
    bool    bCompleteIrp = true;

    ASSERT(IOCTL_INMAGE_TAG_VOLUME == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);
    
    Irp->IoStatus.Information = 0;

    if(!IsValidIoctlTagVolumeInputBuffer(pBuffer, IoStackLocation->Parameters.DeviceIoControl.InputBufferLength)) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_TAGS, 
            ("DeviceIoControlTagVolume: IOCTL rejected, invalid input buffer \n"));
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    IoctlTagVolumeFlags = *(PULONG)pBuffer;
    ulNumberOfVolumes = *(PULONG)((PCHAR)pBuffer + sizeof(ULONG));

#if (NTDDI_VERSION >= NTDDI_WINXP)
    RTL_OSVERSIONINFOEXW  VersionInfo ;
    ULONGLONG   ullConditionMask        = 0;

    RtlZeroMemory(&VersionInfo, sizeof(RTL_OSVERSIONINFOEXW));
    VersionInfo.dwOSVersionInfoSize  = sizeof(RTL_OSVERSIONINFOEXW);
    VersionInfo.dwMajorVersion = 5;//winxp major version
    VersionInfo.dwMinorVersion = 1;//winxp minor version
    VersionInfo.wServicePackMajor = 2; //winxp sp2
    VersionInfo.wServicePackMinor = 0;

    VER_SET_CONDITION(ullConditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(ullConditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(ullConditionMask, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);
    VER_SET_CONDITION(ullConditionMask, VER_SERVICEPACKMINOR, VER_GREATER_EQUAL);
    
    if(STATUS_SUCCESS != RtlVerifyVersionInfo(&VersionInfo, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR, ullConditionMask))
    {
        if(IoctlTagVolumeFlags & TAG_VOLUME_INPUT_FLAGS_DEFER_TILL_COMMIT_SNAPSHOT) {
            Status = STATUS_INVALID_PARAMETER;
            Irp->IoStatus.Status = Status;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return Status;
        }
    }
#else if(NTDDI_VERSION == NTDDI_WIN2K)
    if(IoctlTagVolumeFlags & TAG_VOLUME_INPUT_FLAGS_DEFER_TILL_COMMIT_SNAPSHOT) {
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }
#endif

    if ((ulNumberOfVolumes == 0) || (ulNumberOfVolumes > DriverContext.ulNumVolumes)) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_TAGS, 
            ("DeviceIoControlTagVolume: IOCTL rejected, invalid number of volumes(%d), ulNumVolumes(%d)\n", 
             ulNumberOfVolumes, DriverContext.ulNumVolumes));
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Status = Status;
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
    pTagInfo->ulNumberOfVolumes = ulNumberOfVolumes;
    pTagInfo->ulFlags |= TAG_INFO_FLAGS_IGNORE_IF_FILTERING_STOPPED;
    InVolDbgPrint(DL_FUNC_TRACE, DM_TAGS, ("DeviceIoControlTagVolume: Called on %d Volumes\n", ulNumberOfVolumes));
    for (ULONG i = 0; i < ulNumberOfVolumes; i++) {
        pTagInfo->TagStatus[i].VolumeContext = GetVolumeContextWithGUID((PWCHAR)(pBuffer + i * GUID_SIZE_IN_BYTES + 2 * sizeof(ULONG)));
        if (NULL != pTagInfo->TagStatus[i].VolumeContext) {
            pTagInfo->TagStatus[i].Status = STATUS_PENDING;
            InVolDbgPrint(DL_FUNC_TRACE, DM_TAGS, ("\tGUID=%S Volume=%S\n", pTagInfo->TagStatus[i].VolumeContext->wGUID, pTagInfo->TagStatus[i].VolumeContext->wDriveLetter));
        } else {
            pTagInfo->TagStatus[i].Status = STATUS_NOT_FOUND;
            pTagInfo->ulNumberOfVolumesIgnored++;
        }
    }

    if (pTagInfo->ulNumberOfVolumesIgnored && (0 == (IoctlTagVolumeFlags & TAG_VOLUME_INPUT_FLAGS_NO_ERROR_IF_GUID_NOT_FOUND))) {
        Status = STATUS_INVALID_PARAMETER;
    } else {    
        ulTagStartOffset = (2 * sizeof(ULONG)) + (ulNumberOfVolumes * GUID_SIZE_IN_BYTES);
        pTagInfo->pBuffer = (PVOID)(pBuffer + ulTagStartOffset);
#if (NTDDI_VERSION >= NTDDI_WINXP)
        if (0 == (IoctlTagVolumeFlags & TAG_VOLUME_INPUT_FLAGS_DEFER_TILL_COMMIT_SNAPSHOT)) {
#endif
            if (IoctlTagVolumeFlags & TAG_VOLUME_INPUT_FLAGS_ATOMIC_TO_VOLUME_GROUP) {
                Status = TagVolumesAtomically(pTagInfo, pBuffer + ulTagStartOffset);
            } else {
                Status = TagVolumesInSequence(pTagInfo, pBuffer + ulTagStartOffset);
            }
#if (NTDDI_VERSION >= NTDDI_WINXP)
        } else {            

            Status = AddTagNodeToList(pTagInfo, Irp, ecTagStateWaitForCommitSnapshot);
            if (STATUS_SUCCESS == Status)
                bCompleteIrp = false;

        }
#endif
    }

    if(bCompleteIrp){
        for (ULONG i = 0; i < ulNumberOfVolumes; i++) {
        if (NULL != pTagInfo->TagStatus[i].VolumeContext) {
            DereferenceVolumeContext(pTagInfo->TagStatus[i].VolumeContext);
        }
    }
        ExFreePoolWithTag(pTagInfo, TAG_GENERIC_NON_PAGED);

        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    } else {
        Status = STATUS_PENDING;
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
    ULONG IoctlTagVolumeFlags           = 0;
    PSYNC_TAG_INPUT_DATA SyncTagInputData   = (PSYNC_TAG_INPUT_DATA)Irp->AssociatedIrp.SystemBuffer;
    PUCHAR pBuffer                          = (PUCHAR)&SyncTagInputData->ulFlags;
    ULONG ulNumberOfVolumes                 = SyncTagInputData->ulNumberOfVolumeGUIDs;

    ASSERT(IOCTL_INMAGE_SYNC_TAG_VOLUME == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

    Irp->IoStatus.Information = 0;

    if ( (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < sizeof(SYNC_TAG_INPUT_DATA)) || 
         !IsValidIoctlTagVolumeInputBuffer(pBuffer, IoStackLocation->Parameters.DeviceIoControl.InputBufferLength - sizeof(GUID)) )
    {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_TAGS, 
            ("DeviceIoControlTagVolume: IOCTL rejected, invalid input buffer \n"));
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    if ((ulNumberOfVolumes == 0) || (ulNumberOfVolumes > DriverContext.ulNumVolumes)) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_TAGS, 
            ("DeviceIoControlTagVolume: IOCTL rejected, invalid number of volumes(%d), ulNumVolumes(%d)\n", 
             ulNumberOfVolumes, DriverContext.ulNumVolumes));
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

#if (NTDDI_VERSION >= NTDDI_WINXP)
    RTL_OSVERSIONINFOEXW  VersionInfo ;
    ULONGLONG  ullConditionMask = 0;
    
    RtlZeroMemory(&VersionInfo, sizeof(RTL_OSVERSIONINFOEXW));
    VersionInfo.dwOSVersionInfoSize  = sizeof(RTL_OSVERSIONINFOEXW);
    VersionInfo.dwMajorVersion = 5;
    VersionInfo.dwMinorVersion = 1;
    VersionInfo.wServicePackMajor = 2;
    VersionInfo.wServicePackMinor = 0;

    VER_SET_CONDITION(ullConditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(ullConditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(ullConditionMask, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);
    VER_SET_CONDITION(ullConditionMask, VER_SERVICEPACKMINOR, VER_GREATER_EQUAL);
    
    if(STATUS_SUCCESS != RtlVerifyVersionInfo(&VersionInfo, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR, ullConditionMask))
    {
        if(SyncTagInputData->ulFlags & TAG_VOLUME_INPUT_FLAGS_DEFER_TILL_COMMIT_SNAPSHOT) {
            Status = STATUS_INVALID_PARAMETER;
            Irp->IoStatus.Status = Status;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return Status;
        }
    }
#else if(NTDDI_VERSION == NTDDI_WIN2K)
    if(SyncTagInputData->ulFlags & TAG_VOLUME_INPUT_FLAGS_DEFER_TILL_COMMIT_SNAPSHOT) {
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

#endif

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
    pTagInfo->ulNumberOfVolumes = ulNumberOfVolumes;
    if (SyncTagInputData->ulFlags & TAG_VOLUME_INPUT_FLAGS_NO_ERROR_IF_FILTERING_STOPPED) {
        pTagInfo->ulFlags |= TAG_INFO_FLAGS_IGNORE_IF_FILTERING_STOPPED;
    }

    bool    bCompleteIrp = false;

    for (ULONG i = 0; i < ulNumberOfVolumes; i++) {
        pTagInfo->TagStatus[i].VolumeContext = GetVolumeContextWithGUID((PWCHAR)(pBuffer + i * GUID_SIZE_IN_BYTES + 2 * sizeof(ULONG)));
        if (NULL != pTagInfo->TagStatus[i].VolumeContext) {
            pTagInfo->TagStatus[i].Status = STATUS_PENDING; // Initialize with STATUS_PENDING
            InVolDbgPrint(DL_FUNC_TRACE, DM_TAGS, 
                          ("\tGUID=%S Volume=%S\n", pTagInfo->TagStatus[i].VolumeContext->wGUID, pTagInfo->TagStatus[i].VolumeContext->wDriveLetter));
        } else {
            pTagInfo->TagStatus[i].Status = STATUS_NOT_FOUND; // GUID not found.
            pTagInfo->ulNumberOfVolumesIgnored++;
            if (0 == (SyncTagInputData->ulFlags & TAG_VOLUME_INPUT_FLAGS_NO_ERROR_IF_GUID_NOT_FOUND))
                bCompleteIrp = true;
        }
    }


    if (bCompleteIrp) {
        PTAG_VOLUME_STATUS_OUTPUT_DATA OutputData = (PTAG_VOLUME_STATUS_OUTPUT_DATA)Irp->AssociatedIrp.SystemBuffer;

        for (ULONG i = 0; i < ulNumberOfVolumes; i++) {
            if (NULL == pTagInfo->TagStatus[i].VolumeContext) {
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
#if (NTDDI_VERSION >= NTDDI_WINXP)
        if (0 == (SyncTagInputData->ulFlags & TAG_VOLUME_INPUT_FLAGS_DEFER_TILL_COMMIT_SNAPSHOT)) {
#endif
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
#if (NTDDI_VERSION >= NTDDI_WINXP)
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
#endif
        
    }

    if (bCompleteIrp) {
    // Dereference all volumeContexts
        for(ULONG i = 0; i < ulNumberOfVolumes; i++) {
        if (pTagInfo->TagStatus[i].VolumeContext != NULL) {
            DereferenceVolumeContext(pTagInfo->TagStatus[i].VolumeContext);
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
    PLIST_ENTRY         pCurr = NULL;
    ULONG OutputBufferLength = IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength;
    KIRQL  OldIrql;
    
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
IsValidIoctlTagVolumeInputBuffer(
    PUCHAR pBuffer,
    ULONG ulBufferLength
    )
{
    ULONG ulNumberOfVolumes = 0;
    ULONG ulBytesParsed     = 0;
    ULONG ulNumberOfTags    = 0;

    if (ulBufferLength < 2 * sizeof(ULONG))
        return false;

    ulNumberOfVolumes = *(PULONG) (pBuffer + sizeof(ULONG));
    ulBytesParsed += 2 * sizeof(ULONG);

    if (0 == ulNumberOfVolumes)
        return false;
    
    if (ulBufferLength < (ulBytesParsed + ulNumberOfVolumes * GUID_SIZE_IN_BYTES))
        return false;

    ulBytesParsed += ulNumberOfVolumes * GUID_SIZE_IN_BYTES;

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

    // This is size of buffer needed to save tags in dirtyblock(non-datamode)
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
    PVOLUME_CONTEXT     VolumeContext = NULL;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    PRESYNC_START_OUTPUT_V2    pResyncStartOutput = (PRESYNC_START_OUTPUT_V2)Irp->AssociatedIrp.SystemBuffer;

    ASSERT(IOCTL_INMAGE_RESYNC_START_NOTIFICATION == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(RESYNC_START_OUTPUT_V2)) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL, 
            ("DeviceIoControlResyncStartNotification: IOCTL rejected, output buffer too small\n"));
        Status = STATUS_BUFFER_TOO_SMALL;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    VolumeContext = GetVolumeContextForThisIOCTL(DeviceObject, Irp);
    if (VolumeContext) {
        PDATA_BLOCK DataBlock = AllocateLockedDataBlock(TRUE);            

        KeAcquireSpinLock(&VolumeContext->Lock, &OldIrql);
        VolumeContext->bResyncStartReceived = true;
        AddLastChangeTimestampToCurrentNode(VolumeContext, &DataBlock);
        GenerateTimeStamp(pResyncStartOutput->TimeInHundNanoSecondsFromJan1601, pResyncStartOutput->ullSequenceNumber);
        VolumeContext->liResyncStartTimeStamp.QuadPart = (LONGLONG)pResyncStartOutput->TimeInHundNanoSecondsFromJan1601;

        VolumeContext->ResynStartTimeStamp = pResyncStartOutput->TimeInHundNanoSecondsFromJan1601;
        KeReleaseSpinLock(&VolumeContext->Lock, OldIrql);


        if (NULL != DataBlock) {
            AddDataBlockToLockedDataBlockList(DataBlock);
        }

        DereferenceVolumeContext(VolumeContext);
        Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = sizeof(RESYNC_START_OUTPUT_V2);
    } else {
        Status = STATUS_UNSUCCESSFUL;
        Irp->IoStatus.Information = 0;
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
    PVOLUME_CONTEXT     VolumeContext = NULL;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    PRESYNC_END_OUTPUT_V2  pResyncEndOutput = (PRESYNC_END_OUTPUT_V2)Irp->AssociatedIrp.SystemBuffer;

    ASSERT(IOCTL_INMAGE_RESYNC_END_NOTIFICATION == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(RESYNC_END_OUTPUT_V2)) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL, 
            ("DeviceIoControlResyncEndNotification: IOCTL rejected, output buffer too small\n"));
        Status = STATUS_BUFFER_TOO_SMALL;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    VolumeContext = GetVolumeContextForThisIOCTL(DeviceObject, Irp);
    if (VolumeContext) {
        PDATA_BLOCK DataBlock = AllocateLockedDataBlock(TRUE);            

        KeAcquireSpinLock(&VolumeContext->Lock, &OldIrql);
        AddLastChangeTimestampToCurrentNode(VolumeContext, &DataBlock);
        GenerateTimeStamp(pResyncEndOutput->TimeInHundNanoSecondsFromJan1601, pResyncEndOutput->ullSequenceNumber);
        VolumeContext->liResyncEndTimeStamp.QuadPart = (LONGLONG)pResyncEndOutput->TimeInHundNanoSecondsFromJan1601;
        KeReleaseSpinLock(&VolumeContext->Lock, OldIrql);


        if (NULL != DataBlock) {
            AddDataBlockToLockedDataBlockList(DataBlock);
        }

        DereferenceVolumeContext(VolumeContext);
        Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = sizeof(RESYNC_END_OUTPUT_V2);
    } else {
        Status = STATUS_UNSUCCESSFUL;
        Irp->IoStatus.Information = 0;
    }

    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

#if (NTDDI_VERSION >= NTDDI_WINXP)
DeviceIoControlCommitVolumeSnapshot(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION   DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    PVOLUME_CONTEXT     VolumeContext = DeviceExtension->pVolumeContext;
    PTAG_NODE           pHead = NULL;
    PDATA_BLOCK         DataBlock = NULL;
    KIRQL               OldIrql,PrevIrql;
    LIST_ENTRY          ListHeadForCompletion,*pCurr = NULL;

    ASSERT(IOCTL_VOLSNAP_COMMIT_SNAPSHOT == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

  
    InitializeListHead(&ListHeadForCompletion);
    
    KeAcquireSpinLock(&DriverContext.TagListLock, &OldIrql);
    pCurr = DriverContext.HeadForTagNodes.Flink;
    while (pCurr != &DriverContext.HeadForTagNodes) {
        pHead = (PTAG_NODE)pCurr;
        if (pHead->pTagInfo == NULL) {
            // This tag is already applied. Continue looking for tags that are not applied yet.
            pCurr = pCurr->Flink;
            continue;
        }

        if (NULL == DataBlock) {
            DataBlock = AllocateLockedDataBlock(FALSE);            
        }

        for (ULONG ulCounter = 0; ulCounter < pHead->pTagInfo->ulNumberOfVolumes; ulCounter++) {
            if (VolumeContext != pHead->pTagInfo->TagStatus[ulCounter].VolumeContext) {
                continue;
            }
                
            if (VolumeContext->ulFlags & VCF_FILTERING_STOPPED) {
                if (pHead->pTagInfo->ulFlags & TAG_INFO_FLAGS_IGNORE_IF_FILTERING_STOPPED) {
                    pHead->pTagInfo->ulNumberOfVolumesIgnored++;
                    pHead->pTagInfo->TagStatus[ulCounter].Status = STATUS_INVALID_DEVICE_STATE;
                    Status = STATUS_SUCCESS;
                } else {
                    Status = STATUS_INVALID_DEVICE_STATE;
                    pHead->pTagInfo->Status = Status;
                    pHead->pTagInfo->TagStatus[ulCounter].Status = Status;
                }             
            } else {
                KeAcquireSpinLockAtDpcLevel(&VolumeContext->Lock);
                AddLastChangeTimestampToCurrentNode(VolumeContext, &DataBlock);
                
                    if (pHead->pTagInfo->ulFlags & TAG_INFO_FLAGS_WAIT_FOR_COMMIT) {
                    Status = AddUserDefinedTags(VolumeContext, (PUCHAR)pHead->pTagInfo->pBuffer, NULL, NULL , &pHead->pTagInfo->TagGUID, ulCounter);
                    } else {
                    Status = AddUserDefinedTags(VolumeContext, (PUCHAR)pHead->pTagInfo->pBuffer, NULL, NULL, NULL, 0);
                    }
                KeReleaseSpinLockFromDpcLevel(&VolumeContext->Lock);
                    pHead->pTagInfo->TagStatus[ulCounter].Status = Status;
                if (STATUS_SUCCESS == Status)
                        pHead->pTagInfo->ulNumberOfVolumesTagsApplied++;
                    }
                  
            if (STATUS_SUCCESS == Status) {
                if (pHead->pTagInfo->ulNumberOfVolumes == (pHead->pTagInfo->ulNumberOfVolumesTagsApplied + pHead->pTagInfo->ulNumberOfVolumesIgnored)) {
                    if (pHead->pTagInfo->ulFlags & TAG_INFO_FLAGS_WAIT_FOR_COMMIT) {
                        for (ULONG i = 0; i < pHead->ulNoOfVolumes; i++) {
                            if (pHead->pTagInfo->TagStatus[i].VolumeContext != NULL) {
                                DereferenceVolumeContext(pHead->pTagInfo->TagStatus[i].VolumeContext);
                            }
                        }
                        
                        if (pHead->pTagInfo) {
                            ExFreePoolWithTag(pHead->pTagInfo, TAG_GENERIC_NON_PAGED);  
                            pHead->pTagInfo = NULL;
                        }
                    } else {
                        pCurr = pHead->ListEntry.Blink;
                        RemoveEntryList((PLIST_ENTRY)pHead);
                        pHead->pTagInfo->Status = STATUS_SUCCESS;//here only thing that can change is ulNumberOfVolumesTagsApplied.
                        InsertHeadList(&ListHeadForCompletion,&pHead->ListEntry);     
                    }
                }
            } else {
                    pCurr = pHead->ListEntry.Blink;
                    RemoveEntryList((PLIST_ENTRY)pHead);
                    pHead->pTagInfo->Status = STATUS_INVALID_DEVICE_STATE;
                    InsertHeadList(&ListHeadForCompletion,&pHead->ListEntry);   
            }

            break;
        }
       
        pCurr = pCurr->Flink;
    }
   
    if (NULL != DataBlock) {
        AddDataBlockToLockedDataBlockList(DataBlock);
    }

    KeReleaseSpinLock(&DriverContext.TagListLock, OldIrql);
    
    while (!IsListEmpty(&ListHeadForCompletion)) {
        pHead = (PTAG_NODE)RemoveHeadList(&ListHeadForCompletion);
         
        pHead->Irp->IoStatus.Information = 0;
        pHead->Irp->IoStatus.Status = pHead->pTagInfo->Status;
        IoSetCancelRoutine(pHead->Irp, NULL);
        IoCompleteRequest(pHead->Irp, IO_NO_INCREMENT);
        for(ULONG i = 0; (i < pHead->ulNoOfVolumes); i++) {
            if (pHead->pTagInfo->TagStatus[i].VolumeContext != NULL) {
                DereferenceVolumeContext(pHead->pTagInfo->TagStatus[i].VolumeContext);
            }
        }

        if (pHead->pTagInfo) {
            ExFreePoolWithTag(pHead->pTagInfo, TAG_GENERIC_NON_PAGED);
            pHead->pTagInfo = NULL;
        }
        DeallocateTagNode(pHead);
    }


           
    if(!VolumeContext->IsVolumeUsingAddDevice && DriverContext.IsDispatchEntryPatched) {
        if(VolumeContext->IsVolumeCluster)
            Status = (*DriverContext.MajorFunctionForClusDrives[IRP_MJ_DEVICE_CONTROL])(DeviceExtension->TargetDeviceObject, Irp);
        else
            Status = (*DriverContext.MajorFunction[IRP_MJ_DEVICE_CONTROL])(DeviceExtension->TargetDeviceObject, Irp);
    } else {    
        IoSkipCurrentIrpStackLocation(Irp);
        Status = IoCallDriver(DeviceExtension->TargetDeviceObject, Irp);
    
    }
    
    return Status;
}
#endif

#if (NTDDI_VERSION >= NTDDI_WS03)
DeviceIoControlVolumeOffline(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION   DeviceExtension=(PDEVICE_EXTENSION)DeviceObject->DeviceExtension;;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    PVOLUME_CONTEXT     pVolumeContext=DeviceExtension->pVolumeContext;

    ASSERT(IOCTL_VOLUME_OFFLINE == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);
   
    if ((NULL != pVolumeContext) && 
        (!(pVolumeContext->ulFlags & VCF_FILTERING_STOPPED)) &&
        (pVolumeContext->ulFlags & VCF_CLUSTER_VOLUME_ONLINE)) 
    {
        // By this time already we are in last chance mode as file system is unmounted.
        // we write changes directly to volume and close the file.
        InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DeviceIoControlVolumeOffline: VolumeContext is %p, DriveLetter=%S\n",
                                pVolumeContext, pVolumeContext->wDriveLetter));

        if (pVolumeContext->ulFlags & VCF_CV_FS_UNMOUTNED) {
            InVolDbgPrint( DL_INFO, DM_CLUSTER, ("File System is found unmounted at VolumeOffline!!!\n"));
        } else {
            InVolDbgPrint( DL_WARNING, DM_CLUSTER, ("File System is found mounted at VolumeOffline!!!\n"));
        }

        if (pVolumeContext->pVolumeBitmap) {
            InMageFltSaveAllChanges(pVolumeContext, TRUE, NULL, true);
            if (pVolumeContext->pVolumeBitmap->pBitmapAPI) {
                BitmapPersistentValues BitmapPersistentValue(pVolumeContext);
                pVolumeContext->pVolumeBitmap->pBitmapAPI->CommitBitmap(BitmapPersistentValue, NULL);
                InVolDbgPrint( DL_TRACE_L1, DM_CLUSTER, ("DeviceIoControlVolumeOffline: CommitBitmap completed.\n"));
            }
        }

    }

    InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DeviceIoControlVolumeOffline: VolumeContext is %p, DriveLetter=%S VolumeBitmap is %p VolumeFlags is 0x%x\n",
        pVolumeContext, pVolumeContext->wDriveLetter, pVolumeContext->pVolumeBitmap , pVolumeContext->ulFlags));

    if(!pVolumeContext->IsVolumeUsingAddDevice && DriverContext.IsDispatchEntryPatched) {    
        if(pVolumeContext->IsVolumeCluster)
            Status = (*DriverContext.MajorFunctionForClusDrives[IRP_MJ_DEVICE_CONTROL])(DeviceExtension->TargetDeviceObject, Irp);
        else
            Status = (*DriverContext.MajorFunction[IRP_MJ_DEVICE_CONTROL])(DeviceExtension->TargetDeviceObject, Irp);
    } else {
        IoSkipCurrentIrpStackLocation(Irp);
        Status = IoCallDriver(DeviceExtension->TargetDeviceObject, Irp);
    
    }
    return Status;
}


DeviceIoControlVolumeOnline(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION   DeviceExtension=(PDEVICE_EXTENSION)DeviceObject->DeviceExtension;;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    PVOLUME_CONTEXT     pVolumeContext=DeviceExtension->pVolumeContext;
    PBASIC_DISK         pBasicDisk = NULL;
    PBASIC_VOLUME       pBasicVolume = NULL;
    PVOLUME_BITMAP      pVolumeBitmap = NULL;
    KIRQL               OldIrql;
    bool                bAddVolume = false;
    ASSERT(IOCTL_VOLUME_ONLINE == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

#if (NTDDI_VERSION >= NTDDI_WS03)

    //Fix for Bug 27670
    //pVolumeContext->ulDiskSignature cannot be zero but for GPT it's zero since wGUID is always valid.
    //Again disk number cannot be 0xffffffff after a successful call to AllocateVolumeContext.
    if((pVolumeContext->ulDiskNumber != 0xffffffff) ) {

		//In a clustered with no-reboot/no-failover case pVolumeContext->ulDiskNumber might not get
		//populated as per the current design so we can outsmart it by passing ulDiskSignature as the
		//first parameter.
        pBasicDisk = GetBasicDiskFromDriverContext(pVolumeContext->ulDiskSignature,
                               pVolumeContext->ulDiskNumber);
		
        if(NULL != pBasicDisk) {

            KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);

#if (NTDDI_VERSION >= NTDDI_VISTA)
            pVolumeContext->PartitionStyle = pBasicDisk->PartitionStyle;
            if(pVolumeContext->PartitionStyle == ecPartStyleMBR) 
                pVolumeContext->ulDiskSignature = pBasicDisk->ulDiskSignature;
            else
                RtlCopyMemory(&pVolumeContext->DiskId, &pBasicDisk->DiskId, sizeof(GUID));
#else
            pVolumeContext->ulDiskSignature = pBasicDisk->ulDiskSignature;
#endif
            if (pBasicDisk->ulFlags & BD_FLAGS_IS_CLUSTER_RESOURCE) {
                if (!(pVolumeContext->ulFlags & VCF_VOLUME_ON_CLUS_DISK)) {
                    pVolumeContext->ulFlags |= VCF_VOLUME_ON_BASIC_DISK;
                    pVolumeContext->ulFlags |= VCF_VOLUME_ON_CLUS_DISK;
                    if (pBasicDisk->ulFlags & BD_FLAGS_CLUSTER_ONLINE) {
                        InitializeClusterDiskAcquireParams(pVolumeContext);
                    }

                    bAddVolume = true;
                }
				//Fix for Bug 27670
				//A case where volume went offline and came back without underlying device removal.
				//Runs only when pVolumeContext->ulFlags & VCF_VOLUME_ON_CLUS_DISK is TRUE.
				//This flag is cleared only in DeviceIoControlMSCSDiskRemovedFromCluster() which
				//is usually called after a disk release, on purpose, as a part of failover.-Kartha.
				//In case of volume online as an upper filter driver we are not supposed to do much but reverse
				//what had been done in offline procedure of same volume, if it happened. In this case
				//it's DeviceIoControlVolumeOffline. Note:I presume here that the aforementioned volume offline
				//function i.e. DeviceIoControlVolumeOffline was originally designed and written keeping in mind
				//only a drive offline i.e. disk release/acquire case and not volume going offline and coming back.
				//So while reversing whatever done in the latter function one has to bear this in mind and exercise
				//caution. Again in offline case we would have flushed the bitmap but never closed it. And almost
				//the whole volume context state remains intact. Here we need to explicitly re-enable certain flags
				//to bring the volume up. Note that we don't Close bitmap in DeviceIoControlVolumeOffline but then
				//it's re-opened as apart of device coming up if there was a device removal followed by voloume
				//offline. So here we have to explicity re-activate the PnPNotificationEntry.
				else if ((pBasicDisk->ulFlags & BD_FLAGS_CLUSTER_ONLINE)) {
                      if ((pVolumeContext->ulFlags & VCF_VOLUME_ON_CLUS_DISK) &&
                         !(pVolumeContext->ulFlags & VCF_CLUSTER_VOLUME_ONLINE)) {

						  //After this call we should be getting an PNP notification for GUID_IO_VOLUME_MOUNT.					  

                          pVolumeContext->ulFlags |= VCF_CLUSTER_VOLUME_ONLINE;
                    	  pVolumeContext->ulFlags &= ~VCF_CLUSDSK_RETURNED_OFFLINE;


                       }
                
                   }
                
            }
			else {

                pVolumeContext->bQueueChangesToTempQueue = false;//this case will not occur. 
            }

            KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);

            if(bAddVolume) {
#if (NTDDI_VERSION >= NTDDI_VISTA)
            if(pVolumeContext->PartitionStyle == ecPartStyleMBR) 
                pBasicVolume = AddBasicVolumeToDriverContext(pVolumeContext->ulDiskSignature, pVolumeContext);
            else
                pBasicVolume = AddBasicVolumeToDriverContext(pVolumeContext->DiskId, pVolumeContext);
#else
                pBasicVolume = AddBasicVolumeToDriverContext(pVolumeContext->ulDiskSignature, pVolumeContext);
#endif

               
            }

            DereferenceBasicDisk(pBasicDisk);
        } else {
            pVolumeContext->bQueueChangesToTempQueue = false;//for mbr & gpt non cluster disk
        }
    
    }

    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                        ("nDeviceIoControlVolumeOnline: VolumeContext is %p Disk Signature is %x Disk Number is %d\n",
                            pVolumeContext, pVolumeContext->ulDiskSignature, pVolumeContext->ulDiskNumber));

#endif   

    
    if(!pVolumeContext->IsVolumeUsingAddDevice && DriverContext.IsDispatchEntryPatched) {    
        if(pVolumeContext->IsVolumeCluster)
            Status = (*DriverContext.MajorFunctionForClusDrives[IRP_MJ_DEVICE_CONTROL])(DeviceExtension->TargetDeviceObject, Irp);
        else
            Status = (*DriverContext.MajorFunction[IRP_MJ_DEVICE_CONTROL])(DeviceExtension->TargetDeviceObject, Irp);
    } else {
        IoSkipCurrentIrpStackLocation(Irp);
        Status = IoCallDriver(DeviceExtension->TargetDeviceObject, Irp);

    }
    return Status;
}

#endif

NTSTATUS
DeviceIoControlStartFiltering(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status = STATUS_SUCCESS;
    KIRQL               OldIrql;
    PVOLUME_CONTEXT     pVolumeContext = NULL;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    bool                bChanged = false;

    // This IOCTL should be treated as nop if already volume is actively filtered.
    // no registry writes should happen in nop condition
    ASSERT(IOCTL_INMAGE_START_FILTERING_DEVICE == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

    pVolumeContext = GetVolumeContextForThisIOCTL(DeviceObject, Irp);
    if (NULL == pVolumeContext) {
        Status = STATUS_UNSUCCESSFUL;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
    if (pVolumeContext->ulFlags & VCF_FILTERING_STOPPED) {
        StartFilteringDevice(pVolumeContext, true, true);
        pVolumeContext->ulFlags &= ~VCF_OPEN_BITMAP_FAILED;
        pVolumeContext->ulFlags |= VCF_IGNORE_BITMAP_CREATION;
        pVolumeContext->lNumBitmapOpenErrors = 0;
        pVolumeContext->liFirstBitmapOpenErrorAtTickCount.QuadPart = 0;
        pVolumeContext->liLastBitmapOpenErrorAtTickCount.QuadPart = 0;
        pVolumeContext->lNumBitMapClearErrors=0;
        pVolumeContext->lNumBitMapReadErrors=0;
        pVolumeContext->lNumBitMapWriteErrors=0;
        bChanged = true;
    }
    if (IS_CLUSTER_VOLUME(pVolumeContext) && (NULL == pVolumeContext->pVolumeBitmap)) {
        pVolumeContext->ulFlags |= VCF_SAVE_FILTERING_STATE_TO_BITMAP;

        InMageFltLogError(pVolumeContext->FilterDeviceObject, __LINE__,
            INVOLFLT_START_FILTERING_BEFORE_BITMAP_OPEN, STATUS_SUCCESS,
            pVolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES,
            pVolumeContext->wGUID, GUID_SIZE_IN_BYTES, (ULONGLONG)pVolumeContext->liStartFilteringTimeStampByUser.QuadPart);

        if (false == bChanged)
            bChanged = true;
    }
    KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);
      
    if (bChanged) {
#ifdef _WIN64	    
       /* By default Filtering is started on all cluster volume and bitmap file is opened for reading. 
        * If in bitmap filtering is not configured for volume then filtering is stoped. 
        * Data pool is only allocated in openbitmap if filtering is on for cluster volume. 
        * So if later pair is configured then Data Pool should be allocated as bitmap is not reopen */
 
        SetDataPool(1, DriverContext.ullDataPoolSize);

#endif	    
        if (IS_CLUSTER_VOLUME(pVolumeContext)) {
            PVOLUME_BITMAP   pVolumeBitmap = NULL;

            if (NULL == pVolumeContext->pVolumeBitmap) {
                pVolumeBitmap = OpenBitmapFile(pVolumeContext, Status);
                if (pVolumeBitmap) {
                    KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);  
                    pVolumeContext->pVolumeBitmap = ReferenceVolumeBitmap(pVolumeBitmap);
                    if (pVolumeContext->bQueueChangesToTempQueue) {
                        MoveUnupdatedDirtyBlocks(pVolumeContext);
                    }
                    KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);
                } else {
                    KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);  
                    SetBitmapOpenError(pVolumeContext, true, Status );
                    pVolumeContext->ulFlags |= VCF_OPEN_BITMAP_REQUESTED;
                    KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);
                    KeSetEvent(&DriverContext.ServiceThreadWakeEvent, IO_DISK_INCREMENT, FALSE);
                }
            } else {
                KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
                if (NULL != pVolumeContext->pVolumeBitmap) {
            	    pVolumeBitmap = ReferenceVolumeBitmap(pVolumeContext->pVolumeBitmap);
                }
            	KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);
            	if (NULL != pVolumeBitmap) {
                    ExAcquireFastMutex(&pVolumeBitmap->Mutex);                
                    KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
                    if (ecVBitmapStateReadCompleted == pVolumeBitmap->eVBitmapState) {
                        pVolumeBitmap->eVBitmapState = ecVBitmapStateOpened;
                    }
                    KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);
                    ExReleaseFastMutex(&pVolumeBitmap->Mutex);
                }
            }

            if (pVolumeContext->pVolumeBitmap) {
                pVolumeContext->pVolumeBitmap->pBitmapAPI->SaveFilteringStateToBitmap(false, false, true);
            }

            if (NULL != pVolumeBitmap) {
                DereferenceVolumeBitmap(pVolumeBitmap);
            }
        } else {
            SetDWORDInRegVolumeCfg(pVolumeContext, VOLUME_FILTERING_DISABLED, VOLUME_FILTERING_DISABLED_UNSET, 1);

            // For Non-cluster volumes, lets open/read Bitmap File on start filtering, so that the Volume
            // may move to Data WOS instead of just staying in Bitmap WOS. This is needed for VCON
            // where they issue a volume-group based tag. This tag fails if any of the volume is not in
            // Data WOS. This action (opening of bitmap file) will ensure that even if a volume is idle i.e.
            // not receiving any IO, it will move to Data WOS and facilitate issuing of group-based tag.
            // Normally, we open/read bitmap file on first IO, but we have seen cases, where a filtered
            // volume may not recieve any IO.
            InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                        ("DeviceIoControlStartFiltering: Filtering enabled on non-cluster volume %S(%S). Opening bitmap file\n",
                            pVolumeContext->wGUID, pVolumeContext->wDriveLetter));            
            PVOLUME_BITMAP  pVolumeBitmap = NULL;
            KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
            if (NULL != pVolumeContext->pVolumeBitmap) {                
            	pVolumeBitmap = ReferenceVolumeBitmap(pVolumeContext->pVolumeBitmap);                
            }
            KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);
            if (NULL != pVolumeBitmap){
                ExAcquireFastMutex(&pVolumeBitmap->Mutex);
            }
            KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
            pVolumeContext->ulFlags |= VCF_OPEN_BITMAP_REQUESTED;
            if (NULL != pVolumeBitmap){
                if (ecVBitmapStateReadCompleted == pVolumeBitmap->eVBitmapState) {
                    pVolumeBitmap->eVBitmapState = ecVBitmapStateOpened;
                }
            }
            KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);
            if (NULL != pVolumeBitmap){
                ExReleaseFastMutex(&pVolumeBitmap->Mutex);
                DereferenceVolumeBitmap(pVolumeBitmap);
            }
            KeSetEvent(&DriverContext.ServiceThreadWakeEvent, IO_DISK_INCREMENT, FALSE);            
        }
    } else {
        InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                        ("DeviceIoControlStartFiltering: Filtering already enabled on volume %S(%S)\n",
                            pVolumeContext->wGUID, pVolumeContext->wDriveLetter));
    }

    DereferenceVolumeContext(pVolumeContext);

    Status = STATUS_SUCCESS;
    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS
DeviceIoControlClearBitMap(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status;
    KIRQL               OldIrql;
    PVOLUME_CONTEXT     pVolumeContext = NULL;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    PKDIRTY_BLOCK       pDirtyBlock, pNextDirtyBlock;
    PVOLUME_BITMAP      pVolumeBitmap = NULL;

    ASSERT(IOCTL_INMAGE_CLEAR_DIFFERENTIALS == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

    pDirtyBlock = NULL;
    pVolumeContext = GetVolumeContextForThisIOCTL(DeviceObject, Irp);
    if (pVolumeContext)
    {
        KeWaitForSingleObject(&pVolumeContext->SyncEvent, Executive, KernelMode, FALSE, NULL);
        KeWaitForMutexObject(&pVolumeContext->BitmapOpenCloseMutex, Executive, KernelMode, FALSE, NULL);
        KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);

        // Let us free the commit pending dirty blocks too.
        OrphanMappedDataBlocksAndMoveCommitPendingDirtyBlockToMainQueue(pVolumeContext);

        FreeChangeNodeList(&pVolumeContext->ChangeList);
        if (NULL == pVolumeContext->pVolumeBitmap) {
            pVolumeContext->ulFlags |= VCF_CLEAR_BITMAP_ON_OPEN;
        } else {
            pVolumeBitmap = ReferenceVolumeBitmap(pVolumeContext->pVolumeBitmap);
                        // Check if capture mode can be changed.
            if ((ecCaptureModeMetaData == pVolumeContext->eCaptureMode) && CanSwitchToDataCaptureMode(pVolumeContext, 1)) {                
                InVolDbgPrint(DL_INFO, DM_CAPTURE_MODE | DM_DATA_FILTERING,
                              ("%s: Changing capture mode to DATA for volume %s(%s)\n", __FUNCTION__,
                               pVolumeContext->GUIDinANSI, pVolumeContext->DriveLetter));
                VCSetCaptureModeToData(pVolumeContext);
            }
            VCSetWOState(pVolumeContext, false, __FUNCTION__);
        }

        ClearVolumeStats(pVolumeContext);
        VCFreeReservedDataBlockList(pVolumeContext);
        pVolumeContext->ulcbDataFree = 0;
        pVolumeContext->ullcbDataAlocMissed += pVolumeContext->ulcbDataReserved;
        pVolumeContext->ulcbDataReserved = 0;
        KeQuerySystemTime(&pVolumeContext->liClearDiffsTimeStamp);
        pVolumeContext->EndTimeStampofDB = 0;

        KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);

        // Clear the Bitmap
        if (NULL != pVolumeBitmap) {
            ClearBitmapFile(pVolumeBitmap);
            DereferenceVolumeBitmap(pVolumeBitmap);
        }

        KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);        
        if (NULL != pVolumeContext->pVolumeBitmap) {
            pVolumeBitmap = ReferenceVolumeBitmap(pVolumeContext->pVolumeBitmap);
            if ((ecCaptureModeMetaData == pVolumeContext->eCaptureMode) && CanSwitchToDataCaptureMode(pVolumeContext, 1)) {
                InVolDbgPrint(DL_INFO, DM_CAPTURE_MODE | DM_DATA_FILTERING,
                              ("%s: Changing capture mode to DATA for volume %s(%s)\n", __FUNCTION__,
                               pVolumeContext->GUIDinANSI, pVolumeContext->DriveLetter));
                VCSetCaptureModeToData(pVolumeContext);
            }
            VCSetWOState(pVolumeContext, false, __FUNCTION__);
        }        
        KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);
        if (NULL != pVolumeBitmap) {
            DereferenceVolumeBitmap(pVolumeBitmap);
        }
        KeReleaseMutex(&pVolumeContext->BitmapOpenCloseMutex, FALSE);
        KeSetEvent(&pVolumeContext->SyncEvent, IO_NO_INCREMENT, FALSE);
        // Clear resync information
        ResetVolumeOutOfSync(pVolumeContext, true);
        DereferenceVolumeContext(pVolumeContext);
        Status = STATUS_SUCCESS;
    }
    else
    {
        Status = STATUS_UNSUCCESSFUL;
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

	//At this point the system executable services.exe is already up and running. So it's
	//right place to disable any LOCK/UNLOCK to boot volume feature.
	//Bugzilla id: 26376
	InterlockedIncrement(&DriverContext.bootVolNotificationInfo.LockUnlockBootVolumeDisable);

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

    ASSERT(IOCTL_INMAGE_SERVICE_SHUTDOWN_NOTIFY == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

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
    PDEVICE_EXTENSION   DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PVOLUME_CONTEXT   pVolumeContext;

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

    pVolumeContext = GetVolumeContextForThisIOCTL(DeviceObject, Irp);
    if (NULL == pVolumeContext)
    {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_DIRTY_BLOCKS, 
            ("DeviceIoControlCommitDirtyBlocksTransaction: IOCTL rejected, unknown volume GUID %.*S\n",
                GUID_SIZE_IN_CHARS, (PWCHAR)Irp->AssociatedIrp.SystemBuffer));
        Status = STATUS_INVALID_DEVICE_REQUEST;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    pCommitTransaction = (PCOMMIT_TRANSACTION)Irp->AssociatedIrp.SystemBuffer;

    KeWaitForSingleObject(&pVolumeContext->SyncEvent, Executive, KernelMode, FALSE, NULL); 
    Status = CommitDirtyBlockTransaction(pVolumeContext, pCommitTransaction);
    KeSetEvent(&pVolumeContext->SyncEvent, IO_NO_INCREMENT, FALSE);

    DereferenceVolumeContext(pVolumeContext);

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
    PDEVICE_EXTENSION   DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PVOLUME_CONTEXT   pVolumeContext;

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

    pVolumeContext = GetVolumeContextForThisIOCTL(DeviceObject, Irp);
    if (NULL == pVolumeContext)
    {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_DIRTY_BLOCKS, 
            ("DeviceIoControlCommitDirtyBlocksTransaction: IOCTL rejected, unknown volume GUID %.*S\n",
                GUID_SIZE_IN_CHARS, (PWCHAR)Irp->AssociatedIrp.SystemBuffer));
        Status = STATUS_INVALID_DEVICE_REQUEST;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    SetVolumeOutOfSync(pVolumeContext, pSetResyncRequired->ulOutOfSyncErrorCode, pSetResyncRequired->ulOutOfSyncErrorStatus, false);
    Status = STATUS_SUCCESS;

    DereferenceVolumeContext(pVolumeContext);

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

#if DBG
/*
NTSTATUS
DeviceIoControlAddDirtyBlocksTest(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status = STATUS_SUCCESS;
    PKDIRTY_BLOCKS      pDirtyBlockUser, pDirtyBlock;
    PVOLUME_CONTEXT     pVolumeContext;
    PIO_STACK_LOCATION  IoStackLocation;
    KIRQL               OldIrql;
    PDEVICE_EXTENSION   DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;

    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL | DM_DIRTY_BLOCKS,
        ("DeviceIoControlAddDirtyBlocksTest: ADD DIRTY BLOCKS ADD DIRTY BLOCKS ADD DIRTY BLOCKS\n"));

    IoStackLocation = IoGetCurrentIrpStackLocation(Irp);

    if( IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < sizeof( DIRTY_BLOCKS ) ) 
    {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_DIRTY_BLOCKS, 
            ("DeviceIoControlCommitDirtyBlocksTransaction: IOCTL rejected, buffer too small\n"));
        Status = STATUS_BUFFER_TOO_SMALL;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    pVolumeContext = DeviceExtension->pVolumeContext;
    if (NULL == pVolumeContext) {
        Status = STATUS_UNSUCCESSFUL;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    pDirtyBlock = AllocateDirtyBlocks();
    if (NULL == pDirtyBlock) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    pDirtyBlockUser = (PDIRTY_BLOCKS)Irp->AssociatedIrp.SystemBuffer;
    RtlCopyMemory(pDirtyBlock->Changes, pDirtyBlockUser->Changes, (pDirtyBlockUser->cChanges * sizeof(DISK_CHANGE)));
    pDirtyBlock->cChanges = pDirtyBlockUser->cChanges;
    pDirtyBlock->TagDataSource.ulDataSource = INVOLFLT_DATA_SOURCE_META_DATA;
        // Sum up the changes.
    for (unsigned int i = 0; i < pDirtyBlock->cChanges; i++) {
        pDirtyBlock->ulicbChanges.QuadPart += pDirtyBlock->Changes[i].Length;
    }

    KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);

    // Add to main list
    pDirtyBlock->next = pVolumeContext->DirtyBlocksList;
    pVolumeContext->DirtyBlocksList = pDirtyBlock;
    pDirtyBlock->uliTransactionID.QuadPart = pVolumeContext->uliTransactionId.QuadPart++;

    if (pVolumeContext->pLastDirtyBlock == NULL) {
        ASSERT(NULL == pDirtyBlock->next);
        pVolumeContext->pLastDirtyBlock = pDirtyBlock;
    }
    pVolumeContext->lDirtyBlocksInQueue++;
        
    // change the statistics data
    pVolumeContext->ulTotalChangesPending += pDirtyBlock->cChanges;

    KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);

    //
    // Complete request.
    //
    Status = STATUS_SUCCESS;
    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return Status;
}*/
#endif // DBG

NTSTATUS
CopyKDirtyBlockToUDirtyBlock(
    PVOLUME_CONTEXT VolumeContext,
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
    ASSERT(NULL != pDirtyBlock->VolumeContext);

    PCHANGE_NODE    ChangeNode = pDirtyBlock->ChangeNode;

    Status = KeWaitForSingleObject(&ChangeNode->Mutex, Executive, KernelMode, FALSE, NULL);
    ASSERT(STATUS_SUCCESS == Status);
    Status = KeWaitForSingleObject(&VolumeContext->Mutex, Executive, KernelMode, FALSE, NULL);
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
        VolumeContext->ullDataFilesReturned++;
    } else {

        if(ChangeNode->ulFlags & CHANGE_NODE_FLAGS_QUEUED_FOR_DATA_WRITE) {
            KIRQL   OldIrql;

            KeAcquireSpinLock(&VolumeContext->Lock, &OldIrql);
            VolumeContext->lDataBlocksQueuedToFileWrite -= ChangeNode->DirtyBlock->ulDataBlocksAllocated;
            KeReleaseSpinLock(&VolumeContext->Lock, OldIrql);
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
                ULONG Two = 1;
                InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                    INVOLFLT_ERR_FIRST_LAST_SEQUENCE_MISMATCH, STATUS_SUCCESS,
                    pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.ullSequenceNumber,
                    pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.ullSequenceNumber,
                    Two,
                    Two);
            }   
        }
        ASSERT_CHECK(pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.ullSequenceNumber >=
                        pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.ullSequenceNumber, 0);

        if (DriverContext.ulValidationLevel >= ADD_TO_EVENT_LOG) {
            if (pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601 <
                        pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601) {
                ULONG Two = 1;
                InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                    INVOLFLT_ERR_FIRST_LAST_TIME_MISMATCH, STATUS_SUCCESS,
                    pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601,
                    pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
                    Two,
                    Two);
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
        } else {
            FILL_STREAM_HEADER_4B(&pUserDirtyBlock->uTagList.TagList.TagEndOfList, STREAM_REC_TYPE_END_OF_TAG_LIST, sizeof(STREAM_REC_HDR_4B));
        }

        // ProcessStartIrp is set to NULL in initial state of cancellation and after all mapped memory
        // is cleaned up UserProcess is set to NULL.
        if ((INVOLFLT_DATA_SOURCE_DATA == pDirtyBlock->ulDataSource) &&
            (NULL != DriverContext.ProcessStartIrp) && (NULL != DriverContext.UserProcess) &&
            (!IsListEmpty(&pDirtyBlock->DataBlockList)) )
        {
            ASSERT(DriverContext.UserProcess == IoGetCurrentProcess());
            ASSERT(0 != pDirtyBlock->ulcbDataUsed);
#ifdef _WIN64
            if (DriverContext.bS2is64BitProcess) {
                Status = PrepareDirtyBlockForUsermodeUse(pDirtyBlock, (HANDLE)VolumeContext);
            } else {
                Status = PrepareDirtyBlockFor32bitUsermodeUse(pDirtyBlock, (HANDLE)VolumeContext);
            }
#else
            Status = PrepareDirtyBlockForUsermodeUse(pDirtyBlock, (HANDLE)VolumeContext);
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
        if (INVOLFLT_DATA_SOURCE_DATA == pDirtyBlock->ulDataSource) {
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
                    InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                        INVOLFLT_ERR_FIRST_NEXT_TIMEDELTA_MISMATCH, STATUS_SUCCESS,
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
                    InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                        INVOLFLT_ERR_FIRST_NEXT_SEQDELTA_MISMATCH, STATUS_SUCCESS,
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
                    ULONG One = 1;
                    InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                        INVOLFLT_ERR_LAST_SEQUENCE_GENERATION, STATUS_SUCCESS,
                        pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.ullSequenceNumber,
                        TmpSequenceNumberDeltaArray[pDirtyBlock->cChanges - 1],
                        pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.ullSequenceNumber,
                        One);
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
                    ULONG One = 1;
                    InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                        INVOLFLT_ERR_LAST_SEQUENCE_GENERATION, STATUS_SUCCESS,
                        pDirtyBlock->TagTimeStampOfLastChange.ullSequenceNumber,
                        TmpSequenceNumberDeltaArray[pDirtyBlock->cChanges - 1],
                        pDirtyBlock->TagTimeStampOfFirstChange.ullSequenceNumber,
                        One);
                }   
            }        
            ASSERT_CHECK(LastSeqNum == pDirtyBlock->TagTimeStampOfLastChange.ullSequenceNumber, 0);
        }
    }

Out1:

    if (INVOLFLT_DATA_SOURCE_DATA == pDirtyBlock->ulDataSource) {
        if (TmpChangeOffsetArray != NULL)
            ExFreePool(TmpChangeOffsetArray);
        if (TmpChangeLengthArray != NULL)
            ExFreePool(TmpChangeLengthArray);
        if (TmpTimeDeltaArray != NULL)
            ExFreePool(TmpTimeDeltaArray);
        if (TmpSequenceNumberDeltaArray != NULL)
            ExFreePool(TmpSequenceNumberDeltaArray);
    }
    
    
    KeReleaseMutex(&VolumeContext->Mutex, FALSE);
    KeReleaseMutex(&ChangeNode->Mutex, FALSE);

    return Status;
}

NTSTATUS
CommitAndFillBufferWithDirtyBlock(
    PCOMMIT_TRANSACTION pCommitTransaction,
    PVOLUME_CONTEXT VolumeContext,
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

    if (IS_VOLUME_OFFLINE(VolumeContext)) {
        ASSERT(ulBufferLen >= sizeof(UDIRTY_BLOCK_V2));
        RtlZeroMemory(pBuffer, sizeof(UDIRTY_BLOCK_V2));
        return STATUS_DEVICE_OFF_LINE;
    }

    if(VolumeContext->bQueueChangesToTempQueue) {
        InVolDbgPrint(DL_ERROR, DM_REGISTRY,("%s: Currently the dirty blocks are in Temp Queue. Will not return any Dirty block\n", __FUNCTION__));
        return STATUS_DEVICE_BUSY;
    }

    //
    // Return dirty block information
    // TODO: make external DIRTYBLOCKS structure not contain a next pointer; use a DIRTYBLOCKSNODE structure internally
    //

    if (NULL != pCommitTransaction) {
        CommitStatus = CommitDirtyBlockTransaction(VolumeContext, pCommitTransaction);
        if (STATUS_SUCCESS != CommitStatus)
            bCommitFailed = TRUE;
    }

    DataBlock = AllocateLockedDataBlock(TRUE);

    KeAcquireSpinLock(&VolumeContext->Lock, &OldIrql);

    if (VolumeContext->pDBPendingTransactionConfirm) {
        DirtyBlock = VolumeContext->pDBPendingTransactionConfirm;
        ASSERT(VolumeContext->ChangeList.ulTotalChangesPending >= DirtyBlock->cChanges);
        ASSERT(VolumeContext->ChangeList.ullcbTotalChangesPending >= DirtyBlock->ulicbChanges.QuadPart);
        ulTotalChangesPending = VolumeContext->ChangeList.ulTotalChangesPending - DirtyBlock->cChanges;
        ullcbTotalChangesPending = VolumeContext->ChangeList.ullcbTotalChangesPending - DirtyBlock->ulicbChanges.QuadPart;
        VolumeContext->ulicbChangesReverted.QuadPart += DirtyBlock->ulicbChanges.QuadPart;
        VolumeContext->ulChangesReverted += DirtyBlock->cChanges;
        ASSERT(NULL != DirtyBlock->ChangeNode);
        if (ecChangeNodeDataFile == DirtyBlock->ChangeNode->eChangeNode) {
            VolumeContext->ulDataFilesReverted++;
            VolumeContext->ullDataFilesReturned--;
        }
    } else if (VolumeContext->ulPendingTSOTransactionID.QuadPart) {
        RtlCopyMemory(&pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange, &VolumeContext->PendingTsoFirstChangeTS, 
                    sizeof(TIME_STAMP_TAG_V2));
        RtlCopyMemory(&pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange, &VolumeContext->PendingTsoLastChangeTS, 
                    sizeof(TIME_STAMP_TAG_V2));
        pUserDirtyBlock->uTagList.TagList.TagDataSource.ulDataSource = VolumeContext->ulPendingTsoDataSource;
        pUserDirtyBlock->uHdr.Hdr.ulDataSource = VolumeContext->ulPendingTsoDataSource;
        pUserDirtyBlock->uHdr.Hdr.eWOState = VolumeContext->ulPendingTsoWOState;
        pUserDirtyBlock->uHdr.Hdr.uliTransactionID.QuadPart = VolumeContext->ulPendingTSOTransactionID.QuadPart;
    } else {
        PCHANGE_NODE  ChangeNode;
        ChangeNode = GetChangeNodeFromChangeList(VolumeContext);

        if (NULL != ChangeNode) {
            DirtyBlock = ChangeNode->DirtyBlock;
            // Add it to pending list.
            ASSERT(NULL == VolumeContext->pDBPendingTransactionConfirm);
            VolumeContext->pDBPendingTransactionConfirm = DirtyBlock;
            VolumeContext->lDirtyBlocksInPendingCommitQueue++;

            VolumeContext->ulChangesPendingCommit += DirtyBlock->cChanges;
            VolumeContext->ullcbChangesPendingCommit += DirtyBlock->ulicbChanges.QuadPart;
            ulTotalChangesPending = VolumeContext->ChangeList.ulTotalChangesPending - DirtyBlock->cChanges;
            ullcbTotalChangesPending = VolumeContext->ChangeList.ullcbTotalChangesPending - DirtyBlock->ulicbChanges.QuadPart;
            ASSERT((INVOLFLT_DATA_SOURCE_DATA == DirtyBlock->ulDataSource) || (DirtyBlock->eWOState != ecWriteOrderStateData));

            if ((INVOLFLT_DATA_SOURCE_DATA == DirtyBlock->ulDataSource) &&
                (DirtyBlock->ulcbDataFree))
            {
                ULONG ulTrailerSize = SetSVDLastChangeTimeStamp(DirtyBlock);
                VCAdjustCountersForLostDataBytes(VolumeContext, 
                                                 &DataBlock, 
                                                 DirtyBlock->ulcbDataFree + ulTrailerSize);
                DirtyBlock->ulcbDataFree = 0;
            }

            if ((NULL != VolumeContext->DBNotifyEvent) && (ullcbTotalChangesPending < VolumeContext->ulcbDataNotifyThreshold)) {
                VolumeContext->bNotify = true;
            }
            // These fields needed to figure out lag between source and target in terms of time 
            if ((ecWriteOrderStateMetadata == DirtyBlock->eWOState) || (ecWriteOrderStateBitmap == DirtyBlock->eWOState)) {
                VolumeContext->MDFirstChangeTS = DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601;
            }
            DereferenceChangeNode(ChangeNode);
        } else {
            ASSERT(0 == VolumeContext->ulPendingTSOTransactionID.QuadPart);
            InVolDbgPrint(DL_INFO, DM_DIRTY_BLOCKS, 
                ("VolumeContext = %#p, No Changes - ulTotalChangesPending = %#lx\n", 
                VolumeContext, ulTotalChangesPending));
            ASSERT((0 == VolumeContext->ChangeList.ulTotalChangesPending) && (0 == VolumeContext->ChangeList.ullcbTotalChangesPending));
            
            GenerateTimeStampTag(&pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange);
            GenerateTimeStampTag(&pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange);
            pUserDirtyBlock->uTagList.TagList.TagDataSource.ulDataSource = CurrentDataSource(VolumeContext);
            pUserDirtyBlock->uHdr.Hdr.ulDataSource = CurrentDataSource(VolumeContext);
            pUserDirtyBlock->uHdr.Hdr.eWOState = VolumeContext->eWOState;
            pUserDirtyBlock->uHdr.Hdr.uliTransactionID.QuadPart = VolumeContext->uliTransactionId.QuadPart++;
            // Let's keep a copy of these values to be used later in case of TSO commit failure
            RtlCopyMemory(&VolumeContext->PendingTsoFirstChangeTS, &pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange, 
                          sizeof(TIME_STAMP_TAG_V2));
            RtlCopyMemory(&VolumeContext->PendingTsoLastChangeTS, &pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange,
                          sizeof(TIME_STAMP_TAG_V2));
            VolumeContext->ulPendingTsoDataSource = pUserDirtyBlock->uHdr.Hdr.ulDataSource;
            VolumeContext->ulPendingTsoWOState = pUserDirtyBlock->uHdr.Hdr.eWOState;
            VolumeContext->ulPendingTSOTransactionID.QuadPart = pUserDirtyBlock->uHdr.Hdr.uliTransactionID.QuadPart;
            if (NULL != VolumeContext->DBNotifyEvent) {
                VolumeContext->bNotify = true;
            }
        }

    }
    KeQuerySystemTime(&VolumeContext->liGetDirtyBlockTimeStamp);

    if (NULL != DirtyBlock) {
        LARGE_INTEGER       PerformanceFrequency;

        DirtyBlock->liTickCountAtUserRequest = KeQueryPerformanceCounter(&PerformanceFrequency);

        if (VolumeContext->liTickCountAtNotifyEventSet.QuadPart) {
            LARGE_INTEGER       liMicroSeconds;

            liMicroSeconds.QuadPart = DirtyBlock->liTickCountAtUserRequest.QuadPart - VolumeContext->liTickCountAtNotifyEventSet.QuadPart;
            liMicroSeconds.QuadPart = liMicroSeconds.QuadPart * 1000 * 1000 / PerformanceFrequency.QuadPart;

            if (0 == liMicroSeconds.HighPart) {
                if (VolumeContext->LatencyDistForNotify) {
                    VolumeContext->LatencyDistForNotify->AddToDistribution(liMicroSeconds.LowPart);
                }

                if (VolumeContext->LatencyLogForNotify) {
                    VolumeContext->LatencyLogForNotify->AddToLog(liMicroSeconds.LowPart);
                }
            }
            VolumeContext->liTickCountAtNotifyEventSet.QuadPart = 0; 
        }

        // If Dirty block with data
        if (INVOLFLT_DATA_SOURCE_DATA == DirtyBlock->ulDataSource) {
            LARGE_INTEGER       liMicroSeconds;

            liMicroSeconds.QuadPart = DirtyBlock->liTickCountAtUserRequest.QuadPart - 
                                        DirtyBlock->liTickCountAtLastIO.QuadPart;
            liMicroSeconds.QuadPart = liMicroSeconds.QuadPart * 1000 * 1000 / PerformanceFrequency.QuadPart;

            if (0 == liMicroSeconds.HighPart) {
                if (VolumeContext->LatencyDistForS2DataRetrieval) {
                    VolumeContext->LatencyDistForS2DataRetrieval->AddToDistribution(liMicroSeconds.LowPart);
                }

                if (VolumeContext->LatencyLogForS2DataRetrieval) {
                    VolumeContext->LatencyLogForS2DataRetrieval->AddToLog(liMicroSeconds.LowPart);
                }
            }
        }
    }

    KeReleaseSpinLock(&VolumeContext->Lock, OldIrql);

    if (NULL != DataBlock) {
        AddDataBlockToLockedDataBlockList(DataBlock);
        DataBlock = NULL;
    }

    if (NULL == DirtyBlock) {
       // Instead of zeoring the whole buffer just set the next and cChanges.
       // These are set when VolumeContext->Lock is held, do not set them again.
       // GenerateTimeStampTag(&pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange);
       // GenerateTimeStampTag(&pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange);
       // pUserDirtyBlock->uTagList.TagList.TagDataSource.ulDataSource = CurrentDataSource(VolumeContext);
       pUserDirtyBlock->uHdr.Hdr.cChanges = 0;
       pUserDirtyBlock->uHdr.Hdr.ulFlags = 0;
       pUserDirtyBlock->uHdr.Hdr.ulFlags |= UDIRTY_BLOCK_FLAG_TSO_FILE;
       pUserDirtyBlock->uHdr.Hdr.ulicbChanges.QuadPart = 0;
       pUserDirtyBlock->uHdr.Hdr.ppBufferArray = NULL;
       pUserDirtyBlock->uHdr.Hdr.ulSequenceIDforSplitIO = 1;
       VolumeContext->TSOSequenceId = pUserDirtyBlock->uHdr.Hdr.ulSequenceIDforSplitIO;
       VolumeContext->TSOEndTimeStamp = pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601;
       VolumeContext->TSOEndSequenceNumber = pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.ullSequenceNumber;
       FILL_STREAM_HEADER_4B(&pUserDirtyBlock->uTagList.TagList.TagStartOfList, STREAM_REC_TYPE_START_OF_TAG_LIST, sizeof(STREAM_REC_HDR_4B));
       FILL_STREAM_HEADER_4B(&pUserDirtyBlock->uTagList.TagList.TagPadding, STREAM_REC_TYPE_PADDING, sizeof(STREAM_REC_HDR_4B));
       FILL_STREAM_HEADER_4B(&pUserDirtyBlock->uTagList.TagList.TagEndOfList, STREAM_REC_TYPE_END_OF_TAG_LIST, sizeof(STREAM_REC_HDR_4B));
       FILL_STREAM_HEADER(&pUserDirtyBlock->uTagList.TagList.TagDataSource, STREAM_REC_TYPE_DATA_SOURCE, sizeof(DATA_SOURCE_TAG));
    } else {
        Status = CopyKDirtyBlockToUDirtyBlock(VolumeContext, DirtyBlock, pUserDirtyBlock);
    }

    if (VolumeContext->LastCommittedTimeStamp > 0 && VolumeContext->LastCommittedTimeStamp < MAX_PREV_TIMESTAMP) {
        if ((DirtyBlock == NULL) || (DirtyBlock != NULL && ecChangeNodeDataFile != DirtyBlock->ChangeNode->eChangeNode)) {
           ASSERT((VolumeContext->LastCommittedTimeStamp < pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601) ||
                (((VolumeContext->LastCommittedTimeStamp == pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601)&&
                 (VolumeContext->LastCommittedSequenceNumber < pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.ullSequenceNumber)) ||
                 VolumeContext->LastCommittedSplitIoSeqId < pUserDirtyBlock->uHdr.Hdr.ulSequenceIDforSplitIO));
        }
    }

    // Set global values like ulTotalChangesPending and commit failed.
    pUserDirtyBlock->uHdr.Hdr.ulTotalChangesPending = ulTotalChangesPending;
    pUserDirtyBlock->uHdr.Hdr.ulicbTotalChangesPending.QuadPart = ullcbTotalChangesPending;
    pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp = VolumeContext->LastCommittedTimeStamp;
    pUserDirtyBlock->uHdr.Hdr.ullPrevEndSequenceNumber = VolumeContext->LastCommittedSequenceNumber;
    pUserDirtyBlock->uHdr.Hdr.ulPrevSequenceIDforSplitIO = VolumeContext->LastCommittedSplitIoSeqId;

    if (DirtyBlock != NULL && ecChangeNodeDataFile == DirtyBlock->ChangeNode->eChangeNode) {
        if (DriverContext.ulValidationLevel >= ADD_TO_EVENT_LOG) {
            if (pUserDirtyBlock->uHdr.Hdr.ullPrevEndSequenceNumber > 
                        DirtyBlock->TagTimeStampOfFirstChange.ullSequenceNumber) {
                ULONG One = 1;
                InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                    INVOLFLT_PREV_CUR_SEQUENCE_MISMATCH, STATUS_SUCCESS,
                    pUserDirtyBlock->uHdr.Hdr.ullPrevEndSequenceNumber,
                    DirtyBlock->TagTimeStampOfFirstChange.ullSequenceNumber,
                    One,
                    One);
            }   
        }
        if (pUserDirtyBlock->uHdr.Hdr.ullPrevEndSequenceNumber < MAX_PREV_SEQNUMBER) {
            ASSERT_CHECK(pUserDirtyBlock->uHdr.Hdr.ullPrevEndSequenceNumber <= 
                    DirtyBlock->TagTimeStampOfFirstChange.ullSequenceNumber, 0);
        }

        if (DriverContext.ulValidationLevel >= ADD_TO_EVENT_LOG) {
            if (pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp >
                        DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601) {
                ULONG One = 1;
                InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                    INVOLFLT_PREV_CUR_TIME_MISMATCH, STATUS_SUCCESS,
                    pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp,
                    DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601,
                    One,
                    One);
            }   
        }
        if (pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp < MAX_PREV_TIMESTAMP) {
            ASSERT_CHECK(pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp <=
                DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601, 0);
        }
    } else {
        if (DriverContext.ulValidationLevel >= ADD_TO_EVENT_LOG) {
            if (pUserDirtyBlock->uHdr.Hdr.ullPrevEndSequenceNumber > 
                        pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.ullSequenceNumber) {
                ULONG One = 1;
                InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                    INVOLFLT_PREV_CUR_SEQUENCE_MISMATCH, STATUS_SUCCESS,
                    pUserDirtyBlock->uHdr.Hdr.ullPrevEndSequenceNumber,
                    pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.ullSequenceNumber,
                    One,
                    One);
            }   
        }
        if (pUserDirtyBlock->uHdr.Hdr.ullPrevEndSequenceNumber < MAX_PREV_SEQNUMBER) {
            ASSERT_CHECK(pUserDirtyBlock->uHdr.Hdr.ullPrevEndSequenceNumber <= 
                    pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.ullSequenceNumber, 0);
        }

        if (DriverContext.ulValidationLevel >= ADD_TO_EVENT_LOG) {
            if (pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp >
                        pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601) {
                ULONG One = 1;
                InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                    INVOLFLT_PREV_CUR_TIME_MISMATCH, STATUS_SUCCESS,
                    pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp,
                    pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601,
                    One,
                    One);
            }   
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
            if (VolumeContext->bResyncStartReceived == true) {
                if (pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp <= VolumeContext->ResynStartTimeStamp &&
                    VolumeContext->ResynStartTimeStamp <= DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601 &&
                    VolumeContext->ResynStartTimeStamp <= DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601) {
                        // "First diff file after Resync start (in Data File mode) being drained, Volume name: %s, PrevEndTS: %I64x, ResyncStartTS: %I64x, CurrentEndTS: %I64x, CurrentStartTS: %I64x."
                        InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                            INVOLFLT_INFO_FIRSTFILE_VALIDATION1, STATUS_SUCCESS,
                            VolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES,
                            pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp,
                            VolumeContext->ResynStartTimeStamp,
                            DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
                            DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
                }
                if (DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601 < VolumeContext->ResynStartTimeStamp &&
                    VolumeContext->ResynStartTimeStamp < DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601) {
                        // "Start-End TS Problem with First diff file after Resync start (in Data File mode) being drained, Volume name: %s, PrevEndTS: %I64x, ResyncStartTS: %I64x, CurrentEndTS: %I64x, CurrentStartTS: %I64x."
                        InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                            INVOLFLT_ERR_FIRSTFILE_VALIDATION2, STATUS_SUCCESS,
                            VolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES,
                            pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp,
                            VolumeContext->ResynStartTimeStamp,
                            DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
                            DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
                }        
                if (pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp > VolumeContext->ResynStartTimeStamp) {
                        // "PrevEndTS Problem with First diff file after Resync start (in Data File mode) being drained, Volume name: %s, PrevEndTS: %I64x, ResyncStartTS: %I64x, CurrentEndTS: %I64x, CurrentStartTS: %I64x."
                        InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                            INVOLFLT_ERR_FIRSTFILE_VALIDATION3, STATUS_SUCCESS,
                            VolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES,
                            pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp,
                            VolumeContext->ResynStartTimeStamp,
                            DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
                            DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
                }
            }
        }else {
            if (VolumeContext->bResyncStartReceived == true) {
                if (pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp <= VolumeContext->ResynStartTimeStamp &&
                    VolumeContext->ResynStartTimeStamp <= pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601 &&
                    VolumeContext->ResynStartTimeStamp <= pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601) {
                        // "First diff file after Resync start (Not in Data File mode) being drained, Volume name: %s, PrevEndTS: %I64x, ResyncStartTS: %I64x, CurrentEndTS: %I64x, CurrentStartTS: %I64x."
                        InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                            INVOLFLT_INFO_FIRSTFILE_VALIDATION4, STATUS_SUCCESS,
                            VolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES,
                            pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp,
                            VolumeContext->ResynStartTimeStamp,
                            pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
                            pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
                }
                if (pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601 < VolumeContext->ResynStartTimeStamp &&
                    VolumeContext->ResynStartTimeStamp < pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601) {
                        // "Start-End TS Problem with First diff file after Resync start (Not in Data File mode) being drained, Volume name: %s, PrevEndTS: %I64x, ResyncStartTS: %I64x, CurrentEndTS: %I64x, CurrentStartTS: %I64x."
                        InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                            INVOLFLT_ERR_FIRSTFILE_VALIDATION5, STATUS_SUCCESS,
                            VolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES,
                            pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp,
                            VolumeContext->ResynStartTimeStamp,
                            pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
                            pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
                }        
                if (pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp > VolumeContext->ResynStartTimeStamp) {
                        // "PrevEndTS Problem with First diff file after Resync start (Not in Data File mode) being drained, Volume name: %s, PrevEndTS: %I64x, ResyncStartTS: %I64x, CurrentEndTS: %I64x, CurrentStartTS: %I64x."
                        InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                            INVOLFLT_ERR_FIRSTFILE_VALIDATION6, STATUS_SUCCESS,
                            VolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES,
                            pUserDirtyBlock->uHdr.Hdr.ullPrevEndTimeStamp,
                            VolumeContext->ResynStartTimeStamp,
                            pUserDirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
                            pUserDirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
                }
                // VolumeContext->bResyncStartReceived = false; // lets do this in commit.
            }
        }
    }

    AddResyncRequiredFlag(pUserDirtyBlock, VolumeContext);

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
    PDEVICE_EXTENSION   DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PVOLUME_CONTEXT     pVolumeContext;
    PCOMMIT_TRANSACTION pCommitTransaction = NULL;


    InVolDbgPrint(DL_FUNC_TRACE, DM_INMAGE_IOCTL | DM_DIRTY_BLOCKS,
        ("DeviceIoControlGetDirtyBlocks: Called\n"));
    
    if (ecServiceRunning != DriverContext.eServiceState) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_DIRTY_BLOCKS, 
            ("DeviceIoControlGetDirtyBlocksTransaction: Service is not running\n"));
        Status = STATUS_INVALID_DEVICE_STATE;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    if (true == DriverContext.PersistantValueFlushedToRegistry) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_DIRTY_BLOCKS, 
            ("DeviceIoControlGetDirtyBlocksTransaction: Persistent Values already flushed\n"));
        Status = STATUS_DEVICE_BUSY;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    if (DriverContext.bEnableDataFiles && DriverContext.bServiceSupportsDataFiles && (!DriverContext.bS2SupportsDataFiles)) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_DIRTY_BLOCKS, 
            ("DeviceIoControlGetDirtyBlocksTransaction: S2 did not register for data files\n"));
        Status = STATUS_INVALID_DEVICE_STATE;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
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
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    pVolumeContext = GetVolumeContextForThisIOCTL(DeviceObject, Irp);
    if (NULL == pVolumeContext) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_DIRTY_BLOCKS, 
            ("DeviceIoControlGetDirtyBlocksTransaction: IOCTL rejected, unknown volume GUID %.*S\n",
                GUID_SIZE_IN_CHARS, (PWCHAR)Irp->AssociatedIrp.SystemBuffer));
        Status = STATUS_NOT_FOUND;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    if (pVolumeContext->ulFlags & VCF_VCONTEXT_FIELDS_PERSISTED) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_DIRTY_BLOCKS, 
            ("DeviceIoControlGetDirtyBlocksTransaction: Volume Context fields already Persisted\n"));
        Status = STATUS_DEVICE_BUSY;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    // Added for OOD
    if (pVolumeContext->ulFlags & VCF_FILTERING_STOPPED) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_DIRTY_BLOCKS, 
            ("DeviceIoControlGetDirtyBlocksTransaction: Filtering is Stopped\n"));
        Status = STATUS_DEVICE_BUSY;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    ASSERT(DriverContext.CurrentShutdownMarker == DirtyShutdown);//shutdown is on way we should not return the Dirty block 
    KeWaitForSingleObject(&pVolumeContext->SyncEvent, Executive, KernelMode, FALSE, NULL);
    Status = CommitAndFillBufferWithDirtyBlock( 
                    pCommitTransaction, 
                    pVolumeContext, 
                    Irp->AssociatedIrp.SystemBuffer,
                    IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength);
    KeSetEvent(&pVolumeContext->SyncEvent, IO_NO_INCREMENT, FALSE);

    if (STATUS_SUCCESS != Status) {
        RtlZeroMemory(Irp->AssociatedIrp.SystemBuffer, sizeof(UDIRTY_BLOCK_V2));
    }

    DereferenceVolumeContext(pVolumeContext);

    Irp->IoStatus.Information = sizeof(UDIRTY_BLOCK_V2);
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

VOID
ClearVolumeStats(PVOLUME_CONTEXT   pVolumeContext)
{

    pVolumeContext->uliChangesReturnedToService.QuadPart = 0;
    pVolumeContext->uliChangesReadFromBitMap.QuadPart = 0;
    pVolumeContext->uliChangesWrittenToBitMap.QuadPart = 0;

    // The following should not be cleared.
    // pVolumeContext->ulChangesQueuedForWriting
    // pVolumeContext->ulicbChangesQueuedForWriting.QuadPart
    // If these are cleared when bitmap write is completed these would become negative.
    //
    // pVolumeContext->ulChangesPendingCommit
    // pVolumeContext->ulicbChangesPendingCommit.QuadPart
    // If these are cleared when commit is received these would become negative.
    //
    // ulicbChangesInBitmap are dynamically computed.
    // ulicbPendingChanges are dynamically computed.
    // 
    // pVolumeContext->ulTotalChangesPending
    // pVolumeContext->lDirtyBlocksInQueue

    pVolumeContext->ulicbChangesWrittenToBitMap.QuadPart = 0;
    pVolumeContext->ulicbChangesReadFromBitMap.QuadPart = 0;
    pVolumeContext->ulicbReturnedToService.QuadPart = 0;
    pVolumeContext->ulicbChangesReverted.QuadPart = 0;
    pVolumeContext->ulChangesReverted = 0;
    pVolumeContext->lNumBitMapWriteErrors = 0;
    pVolumeContext->lNumBitMapReadErrors = 0;
    pVolumeContext->lNumBitMapClearErrors = 0;
    pVolumeContext->lNumBitmapOpenErrors = 0;
    pVolumeContext->ulNumMemoryAllocFailures = 0;
    pVolumeContext->ulNumberOfTagPointsDropped = 0;

    pVolumeContext->lNumChangeToBitmapWOState = 0;
    pVolumeContext->lNumChangeToBitmapWOStateOnUserRequest = 0;
    pVolumeContext->lNumChangeToMetaDataWOState = 0;
    pVolumeContext->lNumChangeToMetaDataWOStateOnUserRequest = 0;
    pVolumeContext->lNumChangeToDataWOState = 0;
    pVolumeContext->ulNumSecondsInBitmapWOState = 0;
    pVolumeContext->ulNumSecondsInMetaDataWOState = 0;
    pVolumeContext->ulNumSecondsInDataWOState = 0;

    pVolumeContext->lNumChangeToMetaDataCaptureMode = 0;
    pVolumeContext->lNumChangeToMetaDataCaptureModeOnUserRequest = 0;
    pVolumeContext->lNumChangeToDataCaptureMode = 0;
    pVolumeContext->ulNumSecondsInMetadataCaptureMode = 0;
    pVolumeContext->ulNumSecondsInDataCaptureMode = 0;

    // ulNumSecondsInCurrentMode is dynamically computed
    // ulDataBufferSizeAllocated should not be reset
    // ulcbDataBufferSizeFree should not be reset
    // ulcbDataBufferSizeReserved should not be reset
    // ulDataFilesPending should not be reset

    pVolumeContext->ulFlushCount = 0;
    pVolumeContext->ullDataFilesReturned = 0;
    pVolumeContext->ulDataFilesReverted = 0;
    pVolumeContext->liTickCountAtLastWOStateChange = KeQueryPerformanceCounter(NULL);
    pVolumeContext->ulMuCurrentInstance = 0;
    for (ULONG ul = 0; ul < MAX_VC_IO_SIZE_BUCKETS; ul++) {
        pVolumeContext->ullIoSizeCounters[ul] = 0;
    }

    if (pVolumeContext->LatencyDistForS2DataRetrieval)
        pVolumeContext->LatencyDistForS2DataRetrieval->Clear();

    if (pVolumeContext->LatencyDistForCommit)
        pVolumeContext->LatencyDistForCommit->Clear();

    if (pVolumeContext->LatencyDistForNotify)
        pVolumeContext->LatencyDistForNotify->Clear();

    if (pVolumeContext->LatencyLogForS2DataRetrieval)
        pVolumeContext->LatencyLogForS2DataRetrieval->Clear();

    if (pVolumeContext->LatencyLogForCommit)
        pVolumeContext->LatencyLogForCommit->Clear();

    if (pVolumeContext->LatencyLogForNotify)
        pVolumeContext->LatencyLogForNotify->Clear();

    KeQuerySystemTime(&pVolumeContext->liClearStatsTimeStamp);
    pVolumeContext->ulTagsinWOState = 0;
    pVolumeContext->ulTagsinNonWOSDrop = 0;
	DriverContext.ulNumberofAsyncTagIOCTLs = 0;

    return;
}

ULONG
FillAdditionalVolumeGUIDs(
    PVOLUME_CONTEXT VolumeContext,
    ULONG           &ulGUIDsWritten,
    PUCHAR          pBuffer,
    ULONG           ulLength
    )
{
    ULONG           ulBytesWritten = 0;
    KIRQL           OldIrql;

    KeAcquireSpinLock(&VolumeContext->Lock, &OldIrql);
    PVOLUME_GUID    Node = VolumeContext->GUIDList;

    while ((NULL != Node) && (ulLength >= GUID_SIZE_IN_BYTES)) {
		RtlCopyMemory(pBuffer, Node->GUID, GUID_SIZE_IN_BYTES);
        ulGUIDsWritten++;
        pBuffer += GUID_SIZE_IN_BYTES;
        ulBytesWritten += GUID_SIZE_IN_BYTES;
        ulLength -= GUID_SIZE_IN_BYTES;
        Node = Node->NextGUID;
    }
    KeReleaseSpinLock(&VolumeContext->Lock, OldIrql);

    return ulBytesWritten;
}

VOID
FillVolumeStatsStructure(
    PVOLUME_STATS     pVolumeStats,
    PVOLUME_CONTEXT   pVolumeContext
    )
{
    LARGE_INTEGER   liCurrentTickCount, liTickIncrementPerSec, liTickDiff;
    ULONG           ulSecsInCurrentWOState, ulSecsInCurrentCaptureMode;
    KIRQL           OldIrql;
    PKDIRTY_BLOCK   DirtyBlock;

PAGED_CODE();

    liCurrentTickCount = KeQueryPerformanceCounter(&liTickIncrementPerSec);

PAGED_CODE();

    KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);

    liTickDiff.QuadPart = liCurrentTickCount.QuadPart - pVolumeContext->liTickCountAtLastWOStateChange.QuadPart;
    ulSecsInCurrentWOState = (ULONG)(liTickDiff.QuadPart / liTickIncrementPerSec.QuadPart);
    liTickDiff.QuadPart = liCurrentTickCount.QuadPart - pVolumeContext->liTickCountAtLastCaptureModeChange.QuadPart;
    ulSecsInCurrentCaptureMode = (ULONG)(liTickDiff.QuadPart / liTickIncrementPerSec.QuadPart);

    RtlCopyMemory(pVolumeStats->VolumeGUID, pVolumeContext->wGUID, sizeof(WCHAR) * GUID_SIZE_IN_CHARS );
    if(pVolumeContext->pVolumeBitmap && pVolumeContext->pVolumeBitmap->pBitmapAPI) {
        pVolumeStats->ullCacheHit = pVolumeContext->pVolumeBitmap->pBitmapAPI->GetCacheHit();
        pVolumeStats->ullCacheMiss = pVolumeContext->pVolumeBitmap->pBitmapAPI->GetCacheMiss();
    }
    pVolumeStats->DriveLetter = pVolumeContext->wDriveLetter[0];
    pVolumeStats->uliChangesReturnedToService.QuadPart = pVolumeContext->uliChangesReturnedToService.QuadPart;
    pVolumeStats->uliChangesReadFromBitMap.QuadPart = pVolumeContext->uliChangesReadFromBitMap.QuadPart;
    pVolumeStats->uliChangesWrittenToBitMap.QuadPart = pVolumeContext->uliChangesWrittenToBitMap.QuadPart;
    pVolumeStats->ulChangesQueuedForWriting = pVolumeContext->ulChangesQueuedForWriting;

    pVolumeStats->ulicbChangesQueuedForWriting.QuadPart = pVolumeContext->ulicbChangesQueuedForWriting.QuadPart;
    pVolumeStats->ulicbChangesWrittenToBitMap.QuadPart = pVolumeContext->ulicbChangesWrittenToBitMap.QuadPart;
    pVolumeStats->ulicbChangesReadFromBitMap.QuadPart = pVolumeContext->ulicbChangesReadFromBitMap.QuadPart;
    pVolumeStats->ulicbChangesReturnedToService.QuadPart = pVolumeContext->ulicbReturnedToService.QuadPart;
    pVolumeStats->ulicbChangesPendingCommit.QuadPart = pVolumeContext->ullcbChangesPendingCommit;
    pVolumeStats->ulicbChangesReverted.QuadPart = pVolumeContext->ulicbChangesReverted.QuadPart;
    
    pVolumeStats->ullcbTotalChangesPending = pVolumeContext->ChangeList.ullcbTotalChangesPending;
    pVolumeStats->ullcbBitmapChangesPending = pVolumeContext->ChangeList.ullcbBitmapChangesPending;
    pVolumeStats->ullcbMetaDataChangesPending = pVolumeContext->ChangeList.ullcbMetaDataChangesPending;
    pVolumeStats->ullcbDataChangesPending = pVolumeContext->ChangeList.ullcbDataChangesPending;
    pVolumeStats->ulTotalChangesPending = pVolumeContext->ChangeList.ulTotalChangesPending;
    pVolumeStats->ulBitmapChangesPending = pVolumeContext->ChangeList.ulBitmapChangesPending;
    pVolumeStats->ulMetaDataChangesPending = pVolumeContext->ChangeList.ulMetaDataChangesPending;
    pVolumeStats->ulDataChangesPending = pVolumeContext->ChangeList.ulDataChangesPending;
    pVolumeStats->liMountNotifyTimeStamp.QuadPart = pVolumeContext->liMountNotifyTimeStamp.QuadPart;
    pVolumeStats->liDisMountNotifyTimeStamp.QuadPart = pVolumeContext->liDisMountNotifyTimeStamp.QuadPart;
    pVolumeStats->liDisMountFailNotifyTimeStamp.QuadPart = pVolumeContext->liDisMountFailNotifyTimeStamp.QuadPart;
    pVolumeStats->ulChangesPendingCommit = pVolumeContext->ulChangesPendingCommit;
    pVolumeStats->ulChangesReverted = pVolumeContext->ulChangesReverted;
    pVolumeStats->lDirtyBlocksInQueue = pVolumeContext->ChangeList.lDirtyBlocksInQueue;
    pVolumeStats->lNumBitMapWriteErrors = pVolumeContext->lNumBitMapWriteErrors;
    pVolumeStats->lNumBitMapClearErrors = pVolumeContext->lNumBitMapClearErrors;
    pVolumeStats->ulNumMemoryAllocFailures = pVolumeContext->ulNumMemoryAllocFailures;
    pVolumeStats->lNumBitMapReadErrors = pVolumeContext->lNumBitMapReadErrors;
    pVolumeStats->lNumBitmapOpenErrors = pVolumeContext->lNumBitmapOpenErrors;
    pVolumeStats->ulNumberOfTagPointsDropped = pVolumeContext->ulNumberOfTagPointsDropped;
    pVolumeStats->ulTagsinWOState = pVolumeContext->ulTagsinWOState;
    pVolumeStats->ulTagsinNonWOSDrop = pVolumeContext->ulTagsinNonWOSDrop;
    pVolumeStats->ulcbDataBufferSizeAllocated = pVolumeContext->ulcbDataAllocated;
    pVolumeStats->ulcbDataBufferSizeReserved = pVolumeContext->ulcbDataReserved;
    pVolumeStats->ulcbDataBufferSizeFree = pVolumeContext->ulcbDataFree;
    pVolumeStats->ulFlushCount = pVolumeContext->ulFlushCount;
    pVolumeStats->llLastFlushTime = pVolumeContext->liLastFlushTime.QuadPart;
    pVolumeStats->liLastCommittedTimeStampReadFromStore = pVolumeContext->LastCommittedTimeStampReadFromStore;
    pVolumeStats->liLastCommittedSequenceNumberReadFromStore = pVolumeContext->LastCommittedSequenceNumberReadFromStore;
    pVolumeStats->liLastCommittedSplitIoSeqIdReadFromStore = pVolumeContext->LastCommittedSplitIoSeqIdReadFromStore;
    pVolumeStats->ulDataFilesPending = pVolumeContext->ChangeList.ulDataFilesPending;
    pVolumeStats->ullcbDataToDiskLimit = pVolumeContext->ullcbDataToDiskLimit;
    pVolumeStats->ullcbMinFreeDataToDiskLimit = pVolumeContext->ullcbMinFreeDataToDiskLimit;
    pVolumeStats->ullcbDiskUsed = pVolumeContext->ullcbDataWrittenToDisk;
    pVolumeStats->ullDataFilesReturned = pVolumeContext->ullDataFilesReturned;
    pVolumeStats->ulDataFilesReverted = pVolumeContext->ulDataFilesReverted;
    pVolumeStats->ulConfiguredPriority = pVolumeContext->FileWriterThreadPriority;
    pVolumeStats->ulDataBlocksReserved = pVolumeContext->ulDataBlocksReserved;
    if ((NULL != pVolumeContext->VolumeFileWriter) && (NULL != pVolumeContext->VolumeFileWriter->WriterNode)) {
        pVolumeStats->ulEffectivePriority = pVolumeContext->VolumeFileWriter->WriterNode->writer->GetPriority();
    } else {
        pVolumeStats->ulEffectivePriority = 0;
    }
    pVolumeStats->ulWriterId = pVolumeContext->ulWriterId;
    pVolumeStats->ulVolumeSize.QuadPart = pVolumeContext->llVolumeSize;
    pVolumeStats->ulcbDataNotifyThreshold = pVolumeContext->ulcbDataNotifyThreshold;
    pVolumeStats->ulVolumeFlags = 0;
    
    if (0 != pVolumeContext->lNumBitMapWriteErrors)
        pVolumeStats->ulVolumeFlags |= VSF_BITMAP_WRITE_ERROR;

    if (pVolumeContext->ulFlags & VCF_READ_ONLY)
        pVolumeStats->ulVolumeFlags |= VSF_READ_ONLY;

    if (pVolumeContext->ulFlags & VCF_FILTERING_STOPPED)
        pVolumeStats->ulVolumeFlags |= VSF_FILTERING_STOPPED;

    if (pVolumeContext->ulFlags & VCF_BITMAP_WRITE_DISABLED)
        pVolumeStats->ulVolumeFlags |= VSF_BITMAP_WRITE_DISABLED;

    if (pVolumeContext->ulFlags & VCF_BITMAP_READ_DISABLED)
        pVolumeStats->ulVolumeFlags |= VSF_BITMAP_READ_DISABLED;

    if (pVolumeContext->ulFlags & VCF_DATA_CAPTURE_DISABLED)
        pVolumeStats->ulVolumeFlags |= VSF_DATA_CAPTURE_DISABLED;

    if (pVolumeContext->ulFlags & VCF_DATA_FILES_DISABLED)
        pVolumeStats->ulVolumeFlags |= VSF_DATA_FILES_DISABLED;

    if (pVolumeContext->bNotify)
        pVolumeStats->ulVolumeFlags |= VSF_DATA_NOTIFY_SET;

    if (pVolumeContext->ulFlags & VCF_VOLUME_ON_CLUS_DISK)
        pVolumeStats->ulVolumeFlags |= VSF_CLUSTER_VOLUME;

    if (pVolumeContext->ulFlags & VCF_DEVICE_SURPRISE_REMOVAL)
        pVolumeStats->ulVolumeFlags |= VSF_SURPRISE_REMOVED;

    if (pVolumeContext->ulFlags & VCF_VCONTEXT_FIELDS_PERSISTED)
        pVolumeStats->ulVolumeFlags |= VSF_VCFIELDS_PERSISTED;

    pVolumeStats->eWOState = pVolumeContext->eWOState;
    pVolumeStats->ePrevWOState = pVolumeContext->ePrevWOState;
    pVolumeStats->eCaptureMode = pVolumeContext->eCaptureMode;

    if (pVolumeContext->ulFlags & VCF_FILTERING_STOPPED) {
        pVolumeStats->ulNumSecondsInCurrentWOState = 0;
        pVolumeStats->ulNumSecondsInCurrentCaptureMode = 0;
    } else {
        pVolumeStats->ulNumSecondsInCurrentWOState = ulSecsInCurrentWOState;
        pVolumeStats->ulNumSecondsInCurrentCaptureMode = ulSecsInCurrentCaptureMode;
    }

    pVolumeStats->lNumChangeToMetaDataWOState = pVolumeContext->lNumChangeToMetaDataWOState;
    pVolumeStats->lNumChangeToMetaDataWOStateOnUserRequest = pVolumeContext->lNumChangeToMetaDataWOStateOnUserRequest;
    pVolumeStats->lNumChangeToDataWOState = pVolumeContext->lNumChangeToDataWOState;

    pVolumeStats->lNumChangeToBitmapWOState = pVolumeContext->lNumChangeToBitmapWOState;
    pVolumeStats->lNumChangeToBitmapWOStateOnUserRequest = pVolumeContext->lNumChangeToBitmapWOStateOnUserRequest;
    pVolumeStats->lNumChangeToDataCaptureMode = pVolumeContext->lNumChangeToDataCaptureMode;
    pVolumeStats->lNumChangeToMetaDataCaptureMode = pVolumeContext->lNumChangeToMetaDataCaptureMode;
    pVolumeStats->lNumChangeToMetaDataCaptureModeOnUserRequest = pVolumeContext->lNumChangeToMetaDataCaptureModeOnUserRequest;

    pVolumeStats->ulNumSecondsInDataCaptureMode = pVolumeContext->ulNumSecondsInDataCaptureMode;
    pVolumeStats->ulNumSecondsInMetadataCaptureMode = pVolumeContext->ulNumSecondsInMetadataCaptureMode;
    pVolumeStats->ulNumSecondsInDataWOState = pVolumeContext->ulNumSecondsInDataWOState;
    pVolumeStats->ulNumSecondsInMetaDataWOState = pVolumeContext->ulNumSecondsInMetaDataWOState;
    pVolumeStats->ulNumSecondsInBitmapWOState = pVolumeContext->ulNumSecondsInBitmapWOState;

    // Fill disk usage at out of data mode.
    if (pVolumeContext->ulMuCurrentInstance) {
        ULONG   ulMaxInstances = INSTANCES_OF_MEM_USAGE_TRACKED;
        if (pVolumeContext->ulMuCurrentInstance < ulMaxInstances) {
            ulMaxInstances = pVolumeContext->ulMuCurrentInstance;
        }

        if (USER_MODE_INSTANCES_OF_MEM_USAGE_TRACKED < ulMaxInstances) {
            ulMaxInstances = USER_MODE_INSTANCES_OF_MEM_USAGE_TRACKED;
        }

        for (ULONG ul = 0; ul < ulMaxInstances; ul++) {
            ULONG   ulInstance = pVolumeContext->ulMuCurrentInstance - ul - 1;
            ulInstance = ulInstance % INSTANCES_OF_MEM_USAGE_TRACKED;
            pVolumeStats->ullDuAtOutOfDataMode[ul] = pVolumeContext->ullDiskUsageArray[ulInstance];
            pVolumeStats->ulMAllocatedAtOutOfDataMode[ul] = pVolumeContext->ulMemAllocatedArray[ulInstance];
            pVolumeStats->ulMReservedAtOutOfDataMode[ul] = pVolumeContext->ulMemReservedArray[ulInstance];
            pVolumeStats->ulMFreeInVCAtOutOfDataMode[ul] = pVolumeContext->ulMemFreeArray[ulInstance];
            pVolumeStats->ulMFreeAtOutOfDataMode[ul] = pVolumeContext->ulTotalMemFreeArray[ulInstance];
            pVolumeStats->liCaptureModeChangeTS[ul] = pVolumeContext->liCaptureModeChangeTS[ulInstance];
        }
        pVolumeStats->ulMuInstances = ulMaxInstances;
    } else {
        pVolumeStats->ulMuInstances = 0;
    }

    if (pVolumeContext->ulWOSChangeInstance) {
        ULONG   ulMaxInstances = INSTANCES_OF_WO_STATE_TRACKED;
        if (pVolumeContext->ulWOSChangeInstance < ulMaxInstances) {
            ulMaxInstances = pVolumeContext->ulWOSChangeInstance;
        }

        if (USER_MODE_INSTANCES_OF_WO_STATE_TRACKED < ulMaxInstances) {
            ulMaxInstances = USER_MODE_INSTANCES_OF_WO_STATE_TRACKED;
        }

        for (ULONG ul = 0; ul < ulMaxInstances; ul++) {
            ULONG   ulInstance = pVolumeContext->ulWOSChangeInstance - ul - 1;
            ulInstance = ulInstance % INSTANCES_OF_WO_STATE_TRACKED;

            pVolumeStats->eOldWOState[ul] = pVolumeContext->eOldWOState[ulInstance];
            pVolumeStats->eNewWOState[ul] = pVolumeContext->eNewWOState[ulInstance];
            pVolumeStats->eWOSChangeReason[ul] = pVolumeContext->eWOSChangeReason[ulInstance];
            pVolumeStats->ulNumSecondsInWOS[ul] = pVolumeContext->ulNumSecondsInWOS[ulInstance];
            pVolumeStats->liWOstateChangeTS[ul] = pVolumeContext->liWOstateChangeTS[ulInstance];
            pVolumeStats->ullcbBChangesPendingAtWOSchange[ul] = pVolumeContext->ullcbBChangesPendingAtWOSchange[ulInstance];
            pVolumeStats->ullcbMDChangesPendingAtWOSchange[ul] = pVolumeContext->ullcbMDChangesPendingAtWOSchange[ulInstance];
            pVolumeStats->ullcbDChangesPendingAtWOSchange[ul] = pVolumeContext->ullcbDChangesPendingAtWOSchange[ulInstance];
        }
        pVolumeStats->ulWOSChInstances = ulMaxInstances;
    } else {
        pVolumeStats->ulWOSChInstances = 0;
    }

    // Fill Counters.
    ULONG ulMaxCounters = USER_MODE_MAX_IO_BUCKETS;
    if (ulMaxCounters > MAX_VC_IO_SIZE_BUCKETS) {
        ulMaxCounters = MAX_VC_IO_SIZE_BUCKETS;
    }

    for (ULONG ul = 0; ul < ulMaxCounters; ul++) {
        pVolumeStats->ulIoSizeArray[ul] = pVolumeContext->ulIoSizeArray[ul];
        pVolumeStats->ullIoSizeCounters[ul] = pVolumeContext->ullIoSizeCounters[ul];
    }

    for (ULONG ul = 0; ul < ulMaxCounters; ul++) {
        pVolumeStats->ulIoSizeReadArray[ul] = pVolumeContext->VolumeProfile.ReadProfile.ReadIoSizeArray[ul];
        pVolumeStats->ullIoSizeReadCounters[ul] = pVolumeContext->VolumeProfile.ReadProfile.ReadIoSizeCounters[ul];
    }

	pVolumeStats->liVolumeContextCreationTS.QuadPart = pVolumeContext->liVolumeContextCreationTS.QuadPart;
    pVolumeStats->liStartFilteringTimeStamp.QuadPart = pVolumeContext->liStartFilteringTimeStamp.QuadPart;
	pVolumeStats->liStartFilteringTimeStampByUser.QuadPart =  pVolumeContext->liStartFilteringTimeStampByUser.QuadPart;
	pVolumeStats->liGetDBTimeStamp = pVolumeContext->liGetDirtyBlockTimeStamp;
	pVolumeStats->liCommitDBTimeStamp = pVolumeContext->liCommitDirtyBlockTimeStamp;
    pVolumeStats->liStopFilteringTimeStamp.QuadPart = pVolumeContext->liStopFilteringTimeStamp.QuadPart;
    pVolumeStats->liClearDiffsTimeStamp.QuadPart = pVolumeContext->liClearDiffsTimeStamp.QuadPart;
    pVolumeStats->liClearStatsTimeStamp.QuadPart = pVolumeContext->liClearStatsTimeStamp.QuadPart;
    pVolumeStats->liResyncStartTimeStamp.QuadPart = pVolumeContext->liResyncStartTimeStamp.QuadPart;
    pVolumeStats->liResyncEndTimeStamp.QuadPart = pVolumeContext->liResyncEndTimeStamp.QuadPart;
    pVolumeStats->liLastOutOfSyncTimeStamp.QuadPart = pVolumeContext->liLastOutOfSyncTimeStamp.QuadPart;
    pVolumeStats->liDeleteBitmapTimeStamp.QuadPart = pVolumeContext->liDeleteBitmapTimeStamp.QuadPart;
    pVolumeStats->lAdditionalGUIDs = pVolumeContext->lAdditionalGUIDs;
    pVolumeStats->ulAdditionalGUIDsReturned = 0;
    KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);

    PVOLUME_BITMAP pVolumeBitmap = NULL;
    KeWaitForSingleObject(&pVolumeContext->Mutex, Executive, KernelMode, FALSE, NULL);
    KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
    if (pVolumeContext->pVolumeBitmap)
        pVolumeBitmap = ReferenceVolumeBitmap(pVolumeContext->pVolumeBitmap);
    KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);

    if (NULL != pVolumeBitmap) {
        ExAcquireFastMutex(&pVolumeBitmap->Mutex);
        pVolumeStats->eVBitmapState = pVolumeBitmap->eVBitmapState;
        if (pVolumeBitmap->pBitmapAPI)
            pVolumeStats->ulicbChangesInBitmap.QuadPart = pVolumeBitmap->pBitmapAPI->GetDatBytesInBitmap();
        ExReleaseFastMutex(&pVolumeBitmap->Mutex);
        DereferenceVolumeBitmap(pVolumeBitmap);
    } else {
        pVolumeStats->eVBitmapState = ecVBitmapStateUnInitialized;
        pVolumeStats->ulVolumeFlags |= VSF_BITMAP_NOT_OPENED;
        pVolumeStats->ulicbChangesInBitmap.QuadPart = 0;
    }

    pVolumeStats->ulOutOfSyncErrorCode = pVolumeContext->ulOutOfSyncErrorCode;
    pVolumeStats->ulOutOfSyncErrorStatus = pVolumeContext->ulOutOfSyncErrorStatus;
    pVolumeStats->ulOutOfSyncCount = pVolumeContext->ulOutOfSyncCount;
    pVolumeStats->liOutOfSyncTimeStamp.QuadPart = pVolumeContext->liOutOfSyncTimeStamp.QuadPart;
    pVolumeStats->liOutOfSyncIndicatedTimeStamp.QuadPart = pVolumeContext->liOutOfSyncIndicatedTimeStamp.QuadPart;
    pVolumeStats->liOutOfSyncResetTimeStamp.QuadPart = pVolumeContext->liOutOfSyncResetTimeStamp.QuadPart;
    ULONG   ulOutOfSyncErrorCode = pVolumeContext->ulOutOfSyncErrorCode;
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
                                      pVolumeContext->ulOutOfSyncErrorStatus);
    } else {
        pVolumeStats->ErrorStringForResync[0] = 0x00;
    }

    KeReleaseMutex(&pVolumeContext->Mutex, FALSE);

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

    return;
}

NTSTATUS
DeviceIoControlClearVolumeStats(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS            Status = STATUS_INVALID_PARAMETER;
    PVOLUME_CONTEXT     pVolumeContext;
    KIRQL               OldIrql;
    PLIST_ENTRY         pEntry;

    if (0 == IoStackLocation->Parameters.DeviceIoControl.InputBufferLength) {
        KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
        pEntry = DriverContext.HeadForVolumeContext.Flink;
        while (pEntry != &DriverContext.HeadForVolumeContext) {
            pVolumeContext = (PVOLUME_CONTEXT)pEntry;
            ClearVolumeStats(pVolumeContext);
            pEntry = pEntry->Flink;
        }
        KeReleaseSpinLock(&DriverContext.Lock, OldIrql);
        Status = STATUS_SUCCESS;
    } else {
        pVolumeContext = GetVolumeContextForThisIOCTL(DeviceObject, Irp);
        if (pVolumeContext) {
            ClearVolumeStats(pVolumeContext);
            DereferenceVolumeContext(pVolumeContext);
            Status = STATUS_SUCCESS;
        }
    }
    
    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

VOID
FillVolumeDM(
    PVOLUME_DM_DATA   pVolumeDM,
    PVOLUME_CONTEXT   VolumeContext
    )
{
    LARGE_INTEGER   liCurrentTickCount, liTickIncrementPerSec, liTickDiff;
    ULONG           ulSecsInCurrentWOState, ulSecsInCurrentCaptureMode;
    KIRQL           OldIrql;
    PKDIRTY_BLOCK   DirtyBlock;

PAGED_CODE();

    liCurrentTickCount = KeQueryPerformanceCounter(&liTickIncrementPerSec);
    liTickDiff.QuadPart = liCurrentTickCount.QuadPart - VolumeContext->liTickCountAtLastWOStateChange.QuadPart;
    ulSecsInCurrentWOState = (ULONG)(liTickDiff.QuadPart / liTickIncrementPerSec.QuadPart);
    liTickDiff.QuadPart = liCurrentTickCount.QuadPart - VolumeContext->liTickCountAtLastCaptureModeChange.QuadPart;
    ulSecsInCurrentCaptureMode = (ULONG)(liTickDiff.QuadPart / liTickIncrementPerSec.QuadPart);

    RtlCopyMemory(pVolumeDM->VolumeGUID, VolumeContext->wGUID, sizeof(WCHAR) * GUID_SIZE_IN_CHARS );
    pVolumeDM->DriveLetter = VolumeContext->wDriveLetter[0];

    pVolumeDM->eWOState = VolumeContext->eWOState;
    pVolumeDM->ePrevWOState = VolumeContext->ePrevWOState;
    pVolumeDM->eCaptureMode = VolumeContext->eCaptureMode;

    pVolumeDM->lNumChangeToMetaDataWOState = VolumeContext->lNumChangeToMetaDataWOState;
    pVolumeDM->lNumChangeToMetaDataWOStateOnUserRequest = VolumeContext->lNumChangeToMetaDataWOStateOnUserRequest;
    pVolumeDM->lNumChangeToDataWOState = VolumeContext->lNumChangeToDataWOState;

    pVolumeDM->lNumChangeToBitmapWOState = VolumeContext->lNumChangeToBitmapWOState;
    pVolumeDM->lNumChangeToBitmapWOStateOnUserRequest = VolumeContext->lNumChangeToBitmapWOStateOnUserRequest;
    pVolumeDM->lNumChangeToDataCaptureMode = VolumeContext->lNumChangeToDataCaptureMode;
    pVolumeDM->lNumChangeToMetaDataCaptureMode = VolumeContext->lNumChangeToMetaDataCaptureMode;
    pVolumeDM->lNumChangeToMetaDataCaptureModeOnUserRequest = VolumeContext->lNumChangeToMetaDataCaptureModeOnUserRequest;

    pVolumeDM->ulNumSecondsInDataCaptureMode = VolumeContext->ulNumSecondsInDataCaptureMode;
    pVolumeDM->ulNumSecondsInMetadataCaptureMode = VolumeContext->ulNumSecondsInMetadataCaptureMode;

    pVolumeDM->ulNumSecondsInDataWOState = VolumeContext->ulNumSecondsInDataWOState;
    pVolumeDM->ulNumSecondsInMetaDataWOState = VolumeContext->ulNumSecondsInMetaDataWOState;

    pVolumeDM->ulNumSecondsInBitmapWOState = VolumeContext->ulNumSecondsInBitmapWOState;

    if (VolumeContext->ulFlags & VCF_FILTERING_STOPPED) {
        pVolumeDM->ulNumSecondsInCurrentWOState = 0;
        pVolumeDM->ulNumSecondsInCurrentCaptureMode = 0;
    } else {
        pVolumeDM->ulNumSecondsInCurrentWOState = ulSecsInCurrentWOState;
        pVolumeDM->ulNumSecondsInCurrentCaptureMode = ulSecsInCurrentCaptureMode;
    }

    pVolumeDM->lDirtyBlocksInQueue = VolumeContext->ChangeList.lDirtyBlocksInQueue;
    pVolumeDM->ulDataFilesPending = VolumeContext->ChangeList.ulDataFilesPending;
    pVolumeDM->ulDataChangesPending = VolumeContext->ChangeList.ulDataChangesPending;
    pVolumeDM->ulMetaDataChangesPending = VolumeContext->ChangeList.ulMetaDataChangesPending;
    pVolumeDM->ulBitmapChangesPending = VolumeContext->ChangeList.ulBitmapChangesPending;
    pVolumeDM->ulTotalChangesPending = VolumeContext->ChangeList.ulTotalChangesPending;

    pVolumeDM->ullcbDataChangesPending = VolumeContext->ChangeList.ullcbDataChangesPending;
    pVolumeDM->ullcbMetaDataChangesPending = VolumeContext->ChangeList.ullcbMetaDataChangesPending;
    pVolumeDM->ullcbBitmapChangesPending = VolumeContext->ChangeList.ullcbBitmapChangesPending;
    pVolumeDM->ullcbTotalChangesPending = VolumeContext->ChangeList.ullcbTotalChangesPending;

    if (VolumeContext->LatencyDistForNotify) {
        VolumeContext->LatencyDistForNotify->GetDistribution((PULONGLONG)pVolumeDM->ullNotifyLatencyDistribution, USER_MODE_NOTIFY_LATENCY_DISTRIBUTION_BUCKETS);
        VolumeContext->LatencyDistForNotify->GetDistributionSize((PULONG)pVolumeDM->ulNotifyLatencyDistSizeArray, USER_MODE_NOTIFY_LATENCY_DISTRIBUTION_BUCKETS);
    }

    if (VolumeContext->LatencyLogForNotify) {
        pVolumeDM->ulNotifyLatencyLogSize = VolumeContext->LatencyLogForNotify->GetLog((PULONG)pVolumeDM->ulNotifyLatencyLogArray, 
                                                                                        USER_MODE_INSTANCES_OF_NOTIFY_LATENCY_TRACKED);
        VolumeContext->LatencyLogForNotify->GetMinAndMaxLoggedValues(pVolumeDM->ulNotifyLatencyMinLoggedValue, pVolumeDM->ulNotifyLatencyMaxLoggedValue);
    }

    if (VolumeContext->LatencyDistForCommit) {
        VolumeContext->LatencyDistForCommit->GetDistribution((PULONGLONG)pVolumeDM->ullCommitLatencyDistribution, USER_MODE_COMMIT_LATENCY_DISTRIBUTION_BUCKETS);
        VolumeContext->LatencyDistForCommit->GetDistributionSize((PULONG)pVolumeDM->ulCommitLatencyDistSizeArray, USER_MODE_COMMIT_LATENCY_DISTRIBUTION_BUCKETS);
    }

    if (VolumeContext->LatencyLogForCommit) {
        pVolumeDM->ulCommitLatencyLogSize = VolumeContext->LatencyLogForCommit->GetLog((PULONG)pVolumeDM->ulCommitLatencyLogArray, 
                                                                                        USER_MODE_INSTANCES_OF_COMMIT_LATENCY_TRACKED);
        VolumeContext->LatencyLogForCommit->GetMinAndMaxLoggedValues(pVolumeDM->ulCommitLatencyMinLoggedValue, pVolumeDM->ulCommitLatencyMaxLoggedValue);
    }

    if (VolumeContext->LatencyDistForS2DataRetrieval) {
        VolumeContext->LatencyDistForS2DataRetrieval->GetDistribution(
                (PULONGLONG)pVolumeDM->ullS2DataRetievalLatencyDistribution, USER_MODE_S2_DATA_RETRIEVAL_LATENCY_DISTRIBUTION_BUCKERS);
        VolumeContext->LatencyDistForS2DataRetrieval->GetDistributionSize(
                (PULONG)pVolumeDM->ulS2DataRetievalLatencyDistSizeArray, USER_MODE_S2_DATA_RETRIEVAL_LATENCY_DISTRIBUTION_BUCKERS);
    }

    if (VolumeContext->LatencyLogForS2DataRetrieval) {
        pVolumeDM->ulS2DataRetievalLatencyLogSize = VolumeContext->LatencyLogForS2DataRetrieval->GetLog(
                                                            (PULONG)pVolumeDM->ulS2DataRetievalLatencyLogArray, 
                                                            USER_MODE_INSTANCES_OF_S2_DATA_RETRIEVAL_LATENCY_TRACKED);
        VolumeContext->LatencyLogForS2DataRetrieval->GetMinAndMaxLoggedValues(
                                        pVolumeDM->ulS2DataRetievalLatencyMinLoggedValue, pVolumeDM->ulS2DataRetievalLatencyMaxLoggedValue);
    }

    // Fill disk usage at out of data mode.
    if (VolumeContext->ulMuCurrentInstance) {
        ULONG   ulMaxInstances = INSTANCES_OF_MEM_USAGE_TRACKED;
        if (VolumeContext->ulMuCurrentInstance < ulMaxInstances) {
            ulMaxInstances = VolumeContext->ulMuCurrentInstance;
        }

        if (USER_MODE_INSTANCES_OF_MEM_USAGE_TRACKED < ulMaxInstances) {
            ulMaxInstances = USER_MODE_INSTANCES_OF_MEM_USAGE_TRACKED;
        }

        for (ULONG ul = 0; ul < ulMaxInstances; ul++) {
            ULONG   ulInstance = VolumeContext->ulMuCurrentInstance - ul - 1;
            ulInstance = ulInstance % INSTANCES_OF_MEM_USAGE_TRACKED;
            pVolumeDM->ullDuAtOutOfDataMode[ul] = VolumeContext->ullDiskUsageArray[ulInstance];
            pVolumeDM->ulMAllocatedAtOutOfDataMode[ul] = VolumeContext->ulMemAllocatedArray[ulInstance];
            pVolumeDM->ulMReservedAtOutOfDataMode[ul] = VolumeContext->ulMemReservedArray[ulInstance];
            pVolumeDM->ulMFreeInVCAtOutOfDataMode[ul] = VolumeContext->ulMemFreeArray[ulInstance];
            pVolumeDM->ulMFreeAtOutOfDataMode[ul] = VolumeContext->ulTotalMemFreeArray[ulInstance];
            pVolumeDM->liCaptureModeChangeTS[ul] = VolumeContext->liCaptureModeChangeTS[ulInstance];
        }
        pVolumeDM->ulMuInstances = ulMaxInstances;
    } else {
        pVolumeDM->ulMuInstances = 0;
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
    PDEVICE_EXTENSION   DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    NTSTATUS            Status = STATUS_SUCCESS;
    PVOLUMES_DM_DATA    pVolumesDMdata;
    PVOLUME_CONTEXT     pVolumeContext = NULL;
    ULONG               ulBytesWritten = 0;
    ULONG               ulIndex, ulRemainingOutBytes;

    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(VOLUMES_DM_DATA)) {
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    if (0 != IoStackLocation->Parameters.DeviceIoControl.InputBufferLength) {
        pVolumeContext = GetVolumeContextForThisIOCTL(DeviceObject, Irp);
    }

    pVolumesDMdata = (PVOLUMES_DM_DATA)Irp->AssociatedIrp.SystemBuffer;
    pVolumesDMdata->usMajorVersion = VOLUMES_DM_MAJOR_VERSION;
    pVolumesDMdata->usMinorVersion = VOLUMES_DM_MINOR_VERSION;
    pVolumesDMdata->ulVolumesReturned = 0;
    pVolumesDMdata->ulTotalVolumes = DriverContext.ulNumVolumes;
	RtlZeroMemory(pVolumesDMdata->ulReserved, sizeof(pVolumesDMdata->ulReserved));

    if (NULL == pVolumeContext) {
        LIST_ENTRY  VolumeNodeList;

        // Fill stats of all devices into output buffer
PAGED_CODE();

        ulBytesWritten = sizeof(VOLUMES_DM_DATA) - sizeof(VOLUME_DM_DATA);
        ulRemainingOutBytes = IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength;
        ulRemainingOutBytes -= (sizeof(VOLUMES_DM_DATA) - sizeof(VOLUME_DM_DATA));

        InitializeListHead(&VolumeNodeList);
        Status = GetListOfVolumes(&VolumeNodeList);
        if (STATUS_SUCCESS == Status) {
            while(!IsListEmpty(&VolumeNodeList)) {
                PLIST_NODE      pNode;

                pNode = (PLIST_NODE)RemoveHeadList(&VolumeNodeList);
                pVolumeContext = (PVOLUME_CONTEXT)pNode->pData;
                DereferenceListNode(pNode);

                if (ulRemainingOutBytes >= sizeof(VOLUME_DM_DATA)) {
                    ulIndex = pVolumesDMdata->ulVolumesReturned;
                    FillVolumeDM(&pVolumesDMdata->VolumeArray[ulIndex], pVolumeContext);

                    pVolumesDMdata->ulVolumesReturned++;
                    ulRemainingOutBytes -= sizeof(VOLUME_DM_DATA);
                    ulBytesWritten += sizeof(VOLUME_DM_DATA);
                }

                DereferenceVolumeContext(pVolumeContext);
            } // for each volume
        }

        if (0 == pVolumesDMdata->ulVolumesReturned) {
            // To be on safe side atleast return the size of bytes written as sizeof(VOLUME_STATS_DATA);
            ulBytesWritten = sizeof(VOLUMES_DM_DATA);
        }
    } else {
        // Requested volume stats for one volume
        pVolumesDMdata->ulVolumesReturned = 1;
        ulBytesWritten = sizeof(VOLUMES_DM_DATA);
        FillVolumeDM(&pVolumesDMdata->VolumeArray[0], pVolumeContext);

        DereferenceVolumeContext(pVolumeContext);
    }

    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = ulBytesWritten;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS
DeviceIoControlGetVolumeSize(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS            Status = STATUS_SUCCESS;
    PVOLUME_CONTEXT     pVolumeContext = NULL;
    ULARGE_INTEGER     *ulVolumeSize;
    KIRQL               OldIrql;

    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(ULARGE_INTEGER)) {
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Information = 0;
        goto CleanUp;
    }
    
    
    if (0 != IoStackLocation->Parameters.DeviceIoControl.InputBufferLength) {
        pVolumeContext = GetVolumeContextForThisIOCTL(DeviceObject, Irp);
    }

    if (NULL == pVolumeContext) {
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Information = 0;
        goto CleanUp;
    }

    ulVolumeSize = (ULARGE_INTEGER *)Irp->AssociatedIrp.SystemBuffer;

    KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
    ulVolumeSize->QuadPart = pVolumeContext->llVolumeSize;
    KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);

    Irp->IoStatus.Information = sizeof(ULARGE_INTEGER);

CleanUp:
    
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}



NTSTATUS
DeviceIoControlGetVolumeFlags(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS            Status = STATUS_SUCCESS;
    PVOLUME_CONTEXT     pVolumeContext = NULL;
    ULONG               *ulFlags,ulVolFlags;
    KIRQL           OldIrql;

    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(ULONG)) {
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Information = 0;
        goto CleanUp;
    }
    
    
    if (0 != IoStackLocation->Parameters.DeviceIoControl.InputBufferLength) {
        pVolumeContext = GetVolumeContextForThisIOCTL(DeviceObject, Irp);
    }

    if(NULL == pVolumeContext) {
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Information = 0;
        goto CleanUp;
    }

    ulFlags = (ULONG *)Irp->AssociatedIrp.SystemBuffer;
    *ulFlags = 0;

    KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
    ulVolFlags = pVolumeContext->ulFlags;
    if (pVolumeContext->bNotify)
        (*ulFlags) |= VSF_DATA_NOTIFY_SET;
    KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);

    if (ulVolFlags & VCF_READ_ONLY)
        (*ulFlags) |= VSF_READ_ONLY;

    if (ulVolFlags & VCF_FILTERING_STOPPED)
        (*ulFlags)  |= VSF_FILTERING_STOPPED;

    if (ulVolFlags & VCF_BITMAP_WRITE_DISABLED)
        (*ulFlags) |= VSF_BITMAP_WRITE_DISABLED;

    if (ulVolFlags & VCF_BITMAP_READ_DISABLED)
        (*ulFlags) |= VSF_BITMAP_READ_DISABLED;

    if (ulVolFlags & VCF_DATA_CAPTURE_DISABLED)
        (*ulFlags) |= VSF_DATA_CAPTURE_DISABLED;

    if (ulVolFlags & VCF_DATA_FILES_DISABLED)
        (*ulFlags) |= VSF_DATA_FILES_DISABLED;

    if (ulVolFlags & VCF_VOLUME_ON_CLUS_DISK)
        (*ulFlags) |= VSF_CLUSTER_VOLUME;

    if (ulVolFlags & VCF_DEVICE_SURPRISE_REMOVAL)
        (*ulFlags) |= VSF_SURPRISE_REMOVED;
    
    if (ulVolFlags & VCF_VCONTEXT_FIELDS_PERSISTED)
        (*ulFlags) |= VSF_VCFIELDS_PERSISTED;

    
    if (ulVolFlags & VCF_PAGEFILE_WRITES_IGNORED)
        (*ulFlags) |= VSF_PAGEFILE_WRITES_IGNORED;

    if (ulVolFlags & VCF_CLUSTER_VOLUME_ONLINE)
        (*ulFlags) |= VSF_CLUSTER_VOLUME_ONLINE;

    if (ulVolFlags & VCF_VOLUME_ON_BASIC_DISK)
        (*ulFlags) |= VSF_VOLUME_ON_BASIC_DISK;

    if (ulVolFlags & VCF_VOLUME_ON_CLUS_DISK)
        (*ulFlags) |= VSF_VOLUME_ON_CLUS_DISK;

    Irp->IoStatus.Information = sizeof(ULONG);    

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
    NTSTATUS Internal_Status = STATUS_UNSUCCESSFUL;
    OBJECT_ATTRIBUTES   ObjectAttributes;
    IO_STATUS_BLOCK StatusBlock;
    LARGE_INTEGER StartingVcn;
    UNICODE_STRING FileName;
    ULONG OutBufferLength =  IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength;
    WCHAR wcFileName[FILE_NAME_SIZE_LCN];

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

    RtlMoveMemory(wcFileName, pGetLcnInputParameter->FileName,
                         pGetLcnInputParameter->usFileNameLength);

    wcFileName[pGetLcnInputParameter->usFileNameLength/sizeof(WCHAR)] = L'\0';

    RtlInitUnicodeString(&FileName, wcFileName);
    
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
    
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

BOOLEAN
ValidateDataWOSTransition(
    PVOLUME_CONTEXT pVolumeContext
    )
{
    PCHANGE_NODE ChangeNode = NULL;
    PKDIRTY_BLOCK CurrDirtyBlock = NULL;
    PLIST_ENTRY ListEntry = pVolumeContext->ChangeList.Head.Flink;

    if (!IsListEmpty(&pVolumeContext->ChangeList.Head)) {
        while (&pVolumeContext->ChangeList.Head != ListEntry) {
            ChangeNode = (PCHANGE_NODE) ListEntry;
            CurrDirtyBlock = ChangeNode->DirtyBlock;
            if (CurrDirtyBlock->eWOState == ecWriteOrderStateData) {
                return FALSE; // Data WOS transition not valid as there are already some pending DBs in Data WOS
            }
            if (CurrDirtyBlock->ulDataSource != INVOLFLT_DATA_SOURCE_DATA) {
                return FALSE; // Data WOS transition not valid as there are some pending DBs in non data capture mode
            }
            ListEntry = ListEntry->Flink;
        }
    }

    ListEntry = pVolumeContext->ChangeList.TempQueueHead.Flink;
    if (!IsListEmpty(&pVolumeContext->ChangeList.TempQueueHead)) {
        while (&pVolumeContext->ChangeList.TempQueueHead != ListEntry) {
            ChangeNode = (PCHANGE_NODE) ListEntry;
            CurrDirtyBlock = ChangeNode->DirtyBlock;
            if (CurrDirtyBlock->eWOState == ecWriteOrderStateData) {
                return FALSE; // Data WOS transition not valid as there are already some pending DBs in Data WOS
            }
            if (CurrDirtyBlock->ulDataSource != INVOLFLT_DATA_SOURCE_DATA) {
                return FALSE; // Data WOS transition not valid as there are some pending DBs in non data capture mode
            }
            ListEntry = ListEntry->Flink;
        }
    }

    return TRUE;
}


NTSTATUS
DeviceIoControlGetVolumeWriteOrderState(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS            Status = STATUS_SUCCESS;
    PVOLUME_CONTEXT     pVolumeContext = NULL;
    petWriteOrderState  peWriteOrderState;
    KIRQL           OldIrql;

    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(etWriteOrderState)) {
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Information = 0;
        goto CleanUp;
    }
    
    
    if (0 != IoStackLocation->Parameters.DeviceIoControl.InputBufferLength) {
        pVolumeContext = GetVolumeContextForThisIOCTL(DeviceObject, Irp);
    }

    if ((NULL == pVolumeContext) || 
        (pVolumeContext && (pVolumeContext->ulFlags & VCF_FILTERING_STOPPED))) {
        Status = STATUS_NOT_SUPPORTED;
        Irp->IoStatus.Information = 0;
        goto CleanUp;
    }

    peWriteOrderState = (petWriteOrderState)Irp->AssociatedIrp.SystemBuffer;

    KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
    (*peWriteOrderState) = pVolumeContext->eWOState;
    KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);

    Irp->IoStatus.Information = sizeof(etWriteOrderState);
  
CleanUp:            
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
    PDEVICE_EXTENSION   DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    NTSTATUS            Status = STATUS_SUCCESS;
    PVOLUME_STATS_DATA  pVolumeStatsData;
    PVOLUME_CONTEXT     pVolumeContext = NULL;
    ULONG               ulBytesWritten = 0;
    ULONG               ulIndex, ulRemainingOutBytes;
    bool                bDeviceExists = true;

    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(VOLUME_STATS_DATA)) {
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    if (0 != IoStackLocation->Parameters.DeviceIoControl.InputBufferLength) {
        pVolumeContext = GetVolumeContextForThisIOCTL(DeviceObject, Irp);
        if (NULL == pVolumeContext)
            bDeviceExists = false;
    }

    pVolumeStatsData = (PVOLUME_STATS_DATA)Irp->AssociatedIrp.SystemBuffer;
    pVolumeStatsData->usMajorVersion = VOLUME_STATS_DATA_MAJOR_VERSION;
    pVolumeStatsData->usMinorVersion = VOLUME_STATS_DATA_MINOR_VERSION;
    pVolumeStatsData->ulVolumesReturned = 0;
    pVolumeStatsData->ullPersistedTimeStampAfterBoot = DriverContext.ullPersistedTimeStampAfterBoot;
    pVolumeStatsData->PersistentRegistryCreated = DriverContext.PersistentRegistryCreatedFlag;
    pVolumeStatsData->ullPersistedSequenceNumberAfterBoot = DriverContext.ullPersistedSequenceNumberAfterBoot;
    pVolumeStatsData->LastShutdownMarker = DriverContext.LastShutdownMarker;
    pVolumeStatsData->ulTotalVolumes = DriverContext.ulNumVolumes;
    pVolumeStatsData->lTotalAdditionalGUIDs = DriverContext.lAdditionalGUIDs;
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

    ulBytesWritten = sizeof(VOLUME_STATS_DATA);
    if (!bDeviceExists)
        goto CleanUp;

    ulRemainingOutBytes = IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength - sizeof(VOLUME_STATS_DATA);

    if (NULL == pVolumeContext) {
        LIST_ENTRY  VolumeNodeList;

        // Fill stats of all devices into output buffer
PAGED_CODE();

        InitializeListHead(&VolumeNodeList);
        Status = GetListOfVolumes(&VolumeNodeList);
        if (STATUS_SUCCESS == Status) {
            while(!IsListEmpty(&VolumeNodeList)) {
                PLIST_NODE      pNode;

                pNode = (PLIST_NODE)RemoveHeadList(&VolumeNodeList);
                pVolumeContext = (PVOLUME_CONTEXT)pNode->pData;
                DereferenceListNode(pNode);

                if (ulRemainingOutBytes >= sizeof(VOLUME_STATS)) {
                    PVOLUME_STATS   pVolumeStats = (PVOLUME_STATS)((PUCHAR)Irp->AssociatedIrp.SystemBuffer + ulBytesWritten);

                    FillVolumeStatsStructure(pVolumeStats, pVolumeContext);
                    ulRemainingOutBytes -= sizeof(VOLUME_STATS);
                    ulBytesWritten += sizeof(VOLUME_STATS);

                    if (pVolumeContext->GUIDList) {
                        PUCHAR          pGUIDlist = ((PUCHAR)Irp->AssociatedIrp.SystemBuffer + ulBytesWritten);
                        ULONG           ulGUIDbytesWritten = FillAdditionalVolumeGUIDs(pVolumeContext, pVolumeStats->ulAdditionalGUIDsReturned,
                                                                                       pGUIDlist, ulRemainingOutBytes);
                        if (ulGUIDbytesWritten) {
                            ulBytesWritten += ulGUIDbytesWritten;
                            ulRemainingOutBytes -= ulGUIDbytesWritten;
                        }
                    }
                    pVolumeStatsData->ulVolumesReturned++;
                }

                DereferenceVolumeContext(pVolumeContext);
            } // for each volume
        }
    } else {
        // Requested volume stats for one volume
        if (ulRemainingOutBytes >= sizeof(VOLUME_STATS)) {
            PVOLUME_STATS   pVolumeStats = (PVOLUME_STATS)((PUCHAR)Irp->AssociatedIrp.SystemBuffer + ulBytesWritten);

            FillVolumeStatsStructure(pVolumeStats, pVolumeContext);
            pVolumeStatsData->ulVolumesReturned++;
            ulRemainingOutBytes -= sizeof(VOLUME_STATS);
            ulBytesWritten += sizeof(VOLUME_STATS);
        }

        DereferenceVolumeContext(pVolumeContext);
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
DeviceIoControlGetAdditionalVolumeStats(
    PDEVICE_OBJECT  DeviceObject,
    PIRP            Irp
    )
{
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS Status = STATUS_SUCCESS;
    PVOLUME_CONTEXT pVolumeContext = NULL;
    PDEVICE_EXTENSION   DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PVOLUME_STATS_ADDITIONAL_INFO pVolumeAdditionalInfo;
    KIRQL OldIrql;

    if (ecServiceRunning != DriverContext.eServiceState) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_DIRTY_BLOCKS, 
            ("DeviceIoControlGetAdditionalVolumeStats: Service is not running\n"));
        Status = STATUS_INVALID_DEVICE_STATE;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(VOLUME_STATS_ADDITIONAL_INFO)) {
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }
    if (0 != IoStackLocation->Parameters.DeviceIoControl.InputBufferLength) {
        pVolumeContext = GetVolumeContextForThisIOCTL(DeviceObject, Irp);
    }
    if (NULL == pVolumeContext) {
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Information = 0;
        goto CleanUp;
    }
    if (pVolumeContext->ulFlags & VCF_FILTERING_STOPPED) {
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
    if ((TimeInHundNanoSecondsFromJan1601 < DriverContext.ullLastTimeStamp) || (true == pVolumeContext->bResyncRequired)) {
        Status = STATUS_NOT_SUPPORTED;
        Irp->IoStatus.Information = 0;
        goto CleanUp;
    }

    pVolumeAdditionalInfo = (PVOLUME_STATS_ADDITIONAL_INFO)Irp->AssociatedIrp.SystemBuffer;
    PCHANGE_NODE ChangeNode = NULL; 
    PKDIRTY_BLOCK pPendingDB = NULL;
    ULONGLONG   CurrentTimeStamp;
    ULONGLONG   CurrentSequenceNumber;
    ULONGLONG   OldestChangeTS = 0;

    PVOLUME_BITMAP pVolumeBitmap = NULL;
    LARGE_INTEGER ulicbChangesInBitmap;
    ulicbChangesInBitmap.QuadPart = 0;	

    KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
    if (pVolumeContext->pVolumeBitmap)
        pVolumeBitmap = ReferenceVolumeBitmap(pVolumeContext->pVolumeBitmap);
    KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);

   // Read bitmap and find any changes lying on it.
   // Total changes includes changes lying in memory and bitmap

    KeWaitForSingleObject(&pVolumeContext->SyncEvent, Executive, KernelMode, FALSE, NULL);

    if (NULL != pVolumeBitmap) {
        ExAcquireFastMutex(&pVolumeBitmap->Mutex);
        if (pVolumeBitmap->pBitmapAPI)
            ulicbChangesInBitmap.QuadPart = pVolumeBitmap->pBitmapAPI->GetDatBytesInBitmap();
        ExReleaseFastMutex(&pVolumeBitmap->Mutex);
        DereferenceVolumeBitmap(pVolumeBitmap);
    } 

    KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);

    GenerateTimeStamp(CurrentTimeStamp, CurrentSequenceNumber);
    pVolumeAdditionalInfo->ullTotalChangesPending = pVolumeContext->ChangeList.ullcbTotalChangesPending + ulicbChangesInBitmap.QuadPart;

    if (pVolumeAdditionalInfo->ullTotalChangesPending) {
        PCHANGE_NODE pFirstChangeNode = NULL;
        PKDIRTY_BLOCK pFirstDB = NULL;

        if (!IsListEmpty(&pVolumeContext->ChangeList.Head)) {
            pFirstChangeNode = (PCHANGE_NODE)pVolumeContext->ChangeList.Head.Flink;;
            pFirstDB = pFirstChangeNode->DirtyBlock;
        }

        if (pVolumeContext->pDBPendingTransactionConfirm) {
            pPendingDB = pVolumeContext->pDBPendingTransactionConfirm;

            if ((ecWriteOrderStateData == pVolumeContext->eWOState) || (ecWriteOrderStateData == pPendingDB->eWOState)) {
                OldestChangeTS = pPendingDB->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601;
            } else if (ecWriteOrderStateMetadata == pVolumeContext->eWOState) {
                if (ecWriteOrderStateMetadata == pPendingDB->eWOState) {
                    OldestChangeTS = pVolumeContext->MDFirstChangeTS;
                    if (NULL != pFirstDB)
                        OldestChangeTS = ((OldestChangeTS < pFirstDB->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601) ? 
                                               OldestChangeTS : pFirstDB->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
                } else if (ecWriteOrderStateBitmap == pPendingDB->eWOState){
                    // When bitmap changes are completely drained, we may find the DirtyBlock with WO state Bitmap
                    OldestChangeTS = pVolumeContext->MDFirstChangeTS;
                }
            } else { 
                if (ecWriteOrderStateMetadata == pPendingDB->eWOState) { 
                    OldestChangeTS = pVolumeContext->MDFirstChangeTS;
                    if (NULL != pFirstDB)
                        OldestChangeTS = ((OldestChangeTS < pFirstDB->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601) ? 
                                               OldestChangeTS : pFirstDB->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
                } else if (ecWriteOrderStateBitmap == pPendingDB->eWOState){
                    OldestChangeTS = pVolumeContext->EndTimeStampofDB;
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

                InMageFltLogError(DriverContext.ControlDevice, __LINE__, INVOLFLT_GET_ADDITIONAL_STATS_FAIL1, STATUS_SUCCESS, pVolumeContext->EndTimeStampofDB, FirstDBTS, (ULONG)pVolumeContext->eWOState,
                                                              (ULONG)pPendingDB->eWOState);
            }
        } else {
            ASSERT(NULL != pFirstDB);
            if (NULL != pFirstDB) {
                // if WriteOrder State is Data/metadata, We should consider First change timestamp of First DB in the memory 
                if ((ecWriteOrderStateData == pVolumeContext->eWOState) || (ecWriteOrderStateMetadata == pVolumeContext->eWOState)) {
                    OldestChangeTS = pFirstDB->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601;
                } else {
                    if ((ecWriteOrderStateData == pFirstDB->eWOState) || (ecWriteOrderStateMetadata == pFirstDB->eWOState))
                        OldestChangeTS = pFirstDB->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601;
                    else 
                        OldestChangeTS = pVolumeContext->EndTimeStampofDB;
                }
            } else { // Bitmap has changes
                OldestChangeTS = pVolumeContext->EndTimeStampofDB;
            }
            if (0 == OldestChangeTS) {
                ULONGLONG FirstDBTS = 0;
                ULONG FirstDBWOState = 4;
                if (NULL != pFirstDB) {
                     FirstDBTS = pFirstDB->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601;
                     FirstDBWOState = pFirstDB->eWOState;
                }
        
                InMageFltLogError(DriverContext.ControlDevice, __LINE__, INVOLFLT_GET_ADDITIONAL_STATS_FAIL2, STATUS_SUCCESS, pVolumeContext->EndTimeStampofDB, FirstDBTS, (ULONG)pVolumeContext->eWOState, 
                                   (ULONG)FirstDBWOState);
            }
         }
        pVolumeAdditionalInfo->ullOldestChangeTimeStamp =  OldestChangeTS;
    } else {
        pVolumeAdditionalInfo->ullOldestChangeTimeStamp = CurrentTimeStamp;
    }

    KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);

    pVolumeAdditionalInfo->ullDriverCurrentTimeStamp = CurrentTimeStamp;

    KeSetEvent(&pVolumeContext->SyncEvent, IO_NO_INCREMENT, FALSE);

    Irp->IoStatus.Information = sizeof(VOLUME_STATS_ADDITIONAL_INFO);
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
    ULONG               ulMaxBuckets = MAX_VC_IO_SIZE_BUCKETS - 1;

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
            PVOLUME_CONTEXT     VolumeContext = NULL;
            KIRQL               OldIrql;

            // We have to set the data size limit for this volume
            VolumeContext = GetVolumeContextWithGUID(&pInputBuffer->VolumeGUID[0]);
            if (VolumeContext) {
                KeAcquireSpinLock(&VolumeContext->Lock, &OldIrql);

                for (ULONG ul = 0; ul < ulMaxBuckets; ul++) {
                    VolumeContext->ulIoSizeArray[ul] = pInputBuffer->ulIoSizeArray[ul];
                    VolumeContext->ullIoSizeCounters[ul] = 0;
                }

                for (ULONG ul = ulMaxBuckets; ul < MAX_VC_IO_SIZE_BUCKETS; ul++) {
                    VolumeContext->ulIoSizeArray[ul] = 0;
                    VolumeContext->ullIoSizeCounters[ul] = 0;
                }

                KeReleaseSpinLock(&VolumeContext->Lock, OldIrql);
                DereferenceVolumeContext(VolumeContext);
            } else {
                Status = STATUS_DEVICE_DOES_NOT_EXIST;
            }
        } else {
            LIST_ENTRY  VolumeNodeList;
            Status = GetListOfVolumes(&VolumeNodeList);
            if (STATUS_SUCCESS == Status) {
                while(!IsListEmpty(&VolumeNodeList)) {
                    PLIST_NODE  pNode = (PLIST_NODE)RemoveHeadList(&VolumeNodeList);;
                    PVOLUME_CONTEXT VolumeContext = (PVOLUME_CONTEXT)pNode->pData;
                    KIRQL           OldIrql;

                    DereferenceListNode(pNode);

                    KeAcquireSpinLock(&VolumeContext->Lock, &OldIrql);

                    for (ULONG ul = 0; ul < ulMaxBuckets; ul++) {
                        VolumeContext->ulIoSizeArray[ul] = pInputBuffer->ulIoSizeArray[ul];
                        VolumeContext->ullIoSizeCounters[ul] = 0;
                    }

                    for (ULONG ul = ulMaxBuckets; ul < MAX_VC_IO_SIZE_BUCKETS; ul++) {
                        VolumeContext->ulIoSizeArray[ul] = 0;
                        VolumeContext->ullIoSizeCounters[ul] = 0;
                    }

                    KeReleaseSpinLock(&VolumeContext->Lock, OldIrql);
                    DereferenceVolumeContext(VolumeContext);
                } // for each volume
            }
        }
    }
    
    if(pInputBuffer->ulFlags & SET_IO_SIZE_ARRAY_READ_INPUT)
    {
        VOLUME_PROFILE_INITIALIZER VolumeProfileInitializer;
        if (pInputBuffer->ulFlags & SET_IO_SIZE_ARRAY_INPUT_FLAGS_VALID_GUID) {
            PVOLUME_CONTEXT     VolumeContext = NULL;
            KIRQL               OldIrql;
            // We have to set the data size limit for this volume
            VolumeContext = GetVolumeContextWithGUID(&pInputBuffer->VolumeGUID[0]);
            if (VolumeContext) {
                KeAcquireSpinLock(&VolumeContext->Lock, &OldIrql);

                VolumeProfileInitializer.Type = READ_BUCKET_INITIALIZER;
                VolumeProfileInitializer.Profile.IoReadArraySizeInput = *pInputBuffer;
                InitializeProfiler(&VolumeContext->VolumeProfile, &VolumeProfileInitializer);
                
                KeReleaseSpinLock(&VolumeContext->Lock, OldIrql);
                DereferenceVolumeContext(VolumeContext);
            } else {
                Status = STATUS_DEVICE_DOES_NOT_EXIST;
            }
        } else {
            LIST_ENTRY  VolumeNodeList;
            Status = GetListOfVolumes(&VolumeNodeList);
            if (STATUS_SUCCESS == Status) {
                while(!IsListEmpty(&VolumeNodeList)) {
                    PLIST_NODE  pNode = (PLIST_NODE)RemoveHeadList(&VolumeNodeList);;
                    PVOLUME_CONTEXT VolumeContext = (PVOLUME_CONTEXT)pNode->pData;
                    KIRQL           OldIrql;

                    DereferenceListNode(pNode);

                    KeAcquireSpinLock(&VolumeContext->Lock, &OldIrql);

                    VolumeProfileInitializer.Type = READ_BUCKET_INITIALIZER;
                    VolumeProfileInitializer.Profile.IoReadArraySizeInput = *pInputBuffer;
                    InitializeProfiler(&VolumeContext->VolumeProfile, &VolumeProfileInitializer);

                    KeReleaseSpinLock(&VolumeContext->Lock, OldIrql);
                    DereferenceVolumeContext(VolumeContext);
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
	RtlCopyMemory(&Input.VolumeGUID, &pInputBuffer->VolumeGUID, GUID_SIZE_IN_BYTES);
    Input.ulDataToDiskSizeLimit = pInputBuffer->ulDataToDiskSizeLimit;
    Input.ulDataToDiskSizeLimitForAllVolumes = pInputBuffer->ulDataToDiskSizeLimitForAllVolumes;
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
            Status = pDriverParam->WriteULONG(VOLUME_DATA_TO_DISK_LIMIT_IN_MB, Input.ulDefaultDataToDiskSizeLimit);
            DriverContext.ulDataToDiskLimitPerVolumeInMB = Input.ulDefaultDataToDiskSizeLimit;
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
    if (Input.ulDataToDiskSizeLimitForAllVolumes) {
        LIST_ENTRY  VolumeNodeList;
        Status = GetListOfVolumes(&VolumeNodeList);
        if (STATUS_SUCCESS == Status) {
            while(!IsListEmpty(&VolumeNodeList)) {
                PLIST_NODE  pNode = (PLIST_NODE)RemoveHeadList(&VolumeNodeList);;
                PVOLUME_CONTEXT VolumeContext = (PVOLUME_CONTEXT)pNode->pData;

                DereferenceListNode(pNode);

                if (VolumeContext->ulFlags & VCF_GUID_OBTAINED) {
                    Status = KeWaitForSingleObject(&VolumeContext->Mutex, Executive, KernelMode, FALSE, NULL);
                    ASSERT(STATUS_SUCCESS == Status);

                    VolumeContext->ullcbDataToDiskLimit = ((ULONGLONG)Input.ulDataToDiskSizeLimitForAllVolumes) * MEGABYTES;
                    SetDWORDInRegVolumeCfg(VolumeContext, VOLUME_DATA_TO_DISK_LIMIT_IN_MB, Input.ulDataToDiskSizeLimitForAllVolumes);
                    KeReleaseMutex(&VolumeContext->Mutex, FALSE);
                }
                DereferenceVolumeContext(VolumeContext);
            } // for each volume
        }
    }
    // Last set the data to disk size limit for specific volume
    if (Input.ulFlags & SET_DATA_TO_DISK_SIZE_LIMIT_FLAGS_VALID_GUID) {
        PVOLUME_CONTEXT     VolumeContext = NULL;
        // We have to set the data size limit for this volume
        VolumeContext = GetVolumeContextWithGUID(&Input.VolumeGUID[0]);
        if (VolumeContext) {
            Status = KeWaitForSingleObject(&VolumeContext->Mutex, Executive, KernelMode, FALSE, NULL);
            ASSERT(STATUS_SUCCESS == Status);

            VolumeContext->ullcbDataToDiskLimit = ((ULONGLONG)Input.ulDataToDiskSizeLimit) * MEGABYTES;
            SetDWORDInRegVolumeCfg(VolumeContext, VOLUME_DATA_TO_DISK_LIMIT_IN_MB, Input.ulDataToDiskSizeLimit);
            KeReleaseMutex(&VolumeContext->Mutex, FALSE);
            DereferenceVolumeContext(VolumeContext);
        } else {
            if (pOutputBuffer) {
                pOutputBuffer->ulErrorInSettingForVolume = STATUS_DEVICE_DOES_NOT_EXIST;
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

    RtlCopyMemory(&Input.VolumeGUID, &pInputBuffer->VolumeGUID, GUID_SIZE_IN_BYTES);
    Input.ulMinFreeDataToDiskSizeLimit = pInputBuffer->ulMinFreeDataToDiskSizeLimit;
    Input.ulMinFreeDataToDiskSizeLimitForAllVolumes = pInputBuffer->ulMinFreeDataToDiskSizeLimitForAllVolumes;
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
            Status = pDriverParam->WriteULONG(VOLUME_MIN_FREE_DATA_TO_DISK_LIMIT_IN_MB, Input.ulDefaultMinFreeDataToDiskSizeLimit);
            DriverContext.ulMinFreeDataToDiskLimitPerVolumeInMB = Input.ulDefaultMinFreeDataToDiskSizeLimit;
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
    if (Input.ulMinFreeDataToDiskSizeLimitForAllVolumes) {
        LIST_ENTRY  VolumeNodeList;
        Status = GetListOfVolumes(&VolumeNodeList);
        if (STATUS_SUCCESS == Status) {
            while(!IsListEmpty(&VolumeNodeList)) {
                PLIST_NODE  pNode = (PLIST_NODE)RemoveHeadList(&VolumeNodeList);;
                PVOLUME_CONTEXT VolumeContext = (PVOLUME_CONTEXT)pNode->pData;
                DereferenceListNode(pNode);
                if (VolumeContext->ulFlags & VCF_GUID_OBTAINED) {
                    Status = KeWaitForSingleObject(&VolumeContext->Mutex, Executive, KernelMode, FALSE, NULL);
                    ASSERT(STATUS_SUCCESS == Status);
                    VolumeContext->ullcbMinFreeDataToDiskLimit = ((ULONGLONG)Input.ulMinFreeDataToDiskSizeLimitForAllVolumes) * MEGABYTES;
                    SetDWORDInRegVolumeCfg(VolumeContext, VOLUME_MIN_FREE_DATA_TO_DISK_LIMIT_IN_MB, Input.ulMinFreeDataToDiskSizeLimitForAllVolumes);
                    KeReleaseMutex(&VolumeContext->Mutex, FALSE);
                }
                DereferenceVolumeContext(VolumeContext);
            } // for each volume
        }
    }
    // Last set the data to disk size limit for specific volume
    if (Input.ulFlags & SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_FLAGS_VALID_GUID) {
        PVOLUME_CONTEXT     VolumeContext = NULL;
        // We have to set the data size limit for this volume
        VolumeContext = GetVolumeContextWithGUID(&Input.VolumeGUID[0]);
        if (VolumeContext) {
            Status = KeWaitForSingleObject(&VolumeContext->Mutex, Executive, KernelMode, FALSE, NULL);
            ASSERT(STATUS_SUCCESS == Status);

            VolumeContext->ullcbMinFreeDataToDiskLimit= ((ULONGLONG)Input.ulMinFreeDataToDiskSizeLimit) * MEGABYTES;
            SetDWORDInRegVolumeCfg(VolumeContext, VOLUME_MIN_FREE_DATA_TO_DISK_LIMIT_IN_MB, Input.ulMinFreeDataToDiskSizeLimit);
            KeReleaseMutex(&VolumeContext->Mutex, FALSE);
            DereferenceVolumeContext(VolumeContext);
        } else {
            if (pOutputBuffer) {
                pOutputBuffer->ulErrorInSettingForVolume = STATUS_DEVICE_DOES_NOT_EXIST;
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
	RtlCopyMemory(&Input.VolumeGUID, &pInputBuffer->VolumeGUID, GUID_SIZE_IN_BYTES);
    Input.ulLimit = pInputBuffer->ulLimit;
    Input.ulLimitForAllVolumes = pInputBuffer->ulLimitForAllVolumes;
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
            Status = pDriverParam->WriteULONG(VOLUME_DATA_NOTIFY_LIMIT_IN_KB, Input.ulDefaultLimit);
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
    if (Input.ulLimitForAllVolumes) {
        LIST_ENTRY  VolumeNodeList;
        Status = GetListOfVolumes(&VolumeNodeList);
        if (STATUS_SUCCESS == Status) {
            while(!IsListEmpty(&VolumeNodeList)) {
                PLIST_NODE  pNode = (PLIST_NODE)RemoveHeadList(&VolumeNodeList);;
                PVOLUME_CONTEXT VolumeContext = (PVOLUME_CONTEXT)pNode->pData;

                DereferenceListNode(pNode);

                if (VolumeContext->ulFlags & VCF_GUID_OBTAINED) {
                    Status = KeWaitForSingleObject(&VolumeContext->Mutex, Executive, KernelMode, FALSE, NULL);
                    ASSERT(STATUS_SUCCESS == Status);

                    VolumeContext->ulcbDataNotifyThreshold = Input.ulLimitForAllVolumes * KILOBYTES;
                    SetDWORDInRegVolumeCfg(VolumeContext, VOLUME_DATA_NOTIFY_LIMIT_IN_KB, Input.ulLimitForAllVolumes);
                    KeReleaseMutex(&VolumeContext->Mutex, FALSE);
                }
                DereferenceVolumeContext(VolumeContext);
            } // for each volume
        }
    }
    // Last set the data to disk size limit for specific volume
    if ((Input.ulFlags & SET_DATA_NOTIFY_SIZE_LIMIT_FLAGS_VALID_GUID) && (0 != Input.ulLimit)) {
        PVOLUME_CONTEXT     VolumeContext = NULL;
        // We have to set the data size limit for this volume
        VolumeContext = GetVolumeContextWithGUID(&Input.VolumeGUID[0]);
        if (VolumeContext) {
            Status = KeWaitForSingleObject(&VolumeContext->Mutex, Executive, KernelMode, FALSE, NULL);
            ASSERT(STATUS_SUCCESS == Status);

            VolumeContext->ulcbDataNotifyThreshold = Input.ulLimit * KILOBYTES;
            SetDWORDInRegVolumeCfg(VolumeContext, VOLUME_DATA_NOTIFY_LIMIT_IN_KB, Input.ulLimit);
            KeReleaseMutex(&VolumeContext->Mutex, FALSE);
            DereferenceVolumeContext(VolumeContext);
        } else {
            if (pOutputBuffer) {
                pOutputBuffer->ulErrorInSettingForVolume = STATUS_DEVICE_DOES_NOT_EXIST;
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
	RtlCopyMemory(&Input.VolumeGUID, &pInputBuffer->VolumeGUID, GUID_SIZE_IN_BYTES);
    Input.ulThreadPriority = pInputBuffer->ulThreadPriority;
    Input.ulThreadPriorityForAllVolumes = pInputBuffer->ulThreadPriorityForAllVolumes;
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
            Status = pDriverParam->WriteULONG(VOLUME_FILE_WRITER_THREAD_PRIORITY, Input.ulDefaultThreadPriority);
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
    if (Input.ulThreadPriorityForAllVolumes) {
        LIST_ENTRY  VolumeNodeList;
        Status = GetListOfVolumes(&VolumeNodeList);
        if (STATUS_SUCCESS == Status) {
            while(!IsListEmpty(&VolumeNodeList)) {
                PLIST_NODE  pNode = (PLIST_NODE)RemoveHeadList(&VolumeNodeList);;
                PVOLUME_CONTEXT VolumeContext = (PVOLUME_CONTEXT)pNode->pData;

                DereferenceListNode(pNode);

                if (VolumeContext->ulFlags & VCF_GUID_OBTAINED) {
                    Status = KeWaitForSingleObject(&VolumeContext->Mutex, Executive, KernelMode, FALSE, NULL);
                    ASSERT(STATUS_SUCCESS == Status);

                    VolumeContext->FileWriterThreadPriority = Input.ulThreadPriorityForAllVolumes;
                    SetDWORDInRegVolumeCfg(VolumeContext, VOLUME_FILE_WRITER_THREAD_PRIORITY, Input.ulThreadPriorityForAllVolumes);

                    if (VolumeContext->VolumeFileWriter) {
                        DriverContext.DataFileWriterManager->SetFileWriterThreadPriority(VolumeContext->VolumeFileWriter);
                    }
                    KeReleaseMutex(&VolumeContext->Mutex, FALSE);
                }
                DereferenceVolumeContext(VolumeContext);
            } // for each volume
        }
    }

    // Last set the data to disk size limit for specific volume
    if (Input.ulFlags & SET_DATA_FILE_THREAD_PRIORITY_FLAGS_VALID_GUID) {
        PVOLUME_CONTEXT     VolumeContext = NULL;
        // We have to set the data size limit for this volume
        VolumeContext = GetVolumeContextWithGUID(&Input.VolumeGUID[0]);
        if (VolumeContext) {
            Status = KeWaitForSingleObject(&VolumeContext->Mutex, Executive, KernelMode, FALSE, NULL);
            ASSERT(STATUS_SUCCESS == Status);

            VolumeContext->FileWriterThreadPriority = Input.ulThreadPriority;
            SetDWORDInRegVolumeCfg(VolumeContext, VOLUME_FILE_WRITER_THREAD_PRIORITY, Input.ulThreadPriority);

            if (VolumeContext->VolumeFileWriter) {
                DriverContext.DataFileWriterManager->SetFileWriterThreadPriority(VolumeContext->VolumeFileWriter);
            }

            KeReleaseMutex(&VolumeContext->Mutex, FALSE);
            DereferenceVolumeContext(VolumeContext);
        } else {
            if (pOutputBuffer) {
                pOutputBuffer->ulErrorInSettingForVolume = STATUS_DEVICE_DOES_NOT_EXIST;
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
DeviceIoControlMSCSDiskAcquire(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    NTSTATUS            Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulDiskSignature = 0,pKeyNameLength = 0;
    PBASIC_DISK         pBasicDisk = NULL;
    PBASIC_VOLUME       pBasicVolume = NULL;
    KIRQL               OldIrql;
    Registry            *pRegDiskSignature = NULL;
    PWCHAR              pKeyName, pValue;
    WCHAR               BufferDiskSig[9];
    UNICODE_STRING      ustrDiskSig ;


        
    ASSERT(IOCTL_INMAGE_MSCS_DISK_ACQUIRE == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

    if ((NULL == Irp->AssociatedIrp.SystemBuffer) ||
        (sizeof(ULONG) > IoStackLocation->Parameters.DeviceIoControl.InputBufferLength)) 
    {
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

#if (NTDDI_VERSION >= NTDDI_VISTA)

    CDSKFL_INFO  *cDiskInfo = (CDSKFL_INFO *)Irp->AssociatedIrp.SystemBuffer;
    
    if(cDiskInfo->ePartitionStyle == ecPartStyleMBR) {
        pBasicDisk = GetBasicDiskFromDriverContext(cDiskInfo->ulDiskSignature);
    } else if(cDiskInfo->ePartitionStyle == ecPartStyleGPT) {
        pBasicDisk = GetBasicDiskFromDriverContext(cDiskInfo->DiskId);
    } else
        pBasicDisk = NULL;

    if(pBasicDisk)
        pBasicDisk->ulDiskNumber = cDiskInfo->DeviceNumber;
#else

    ulDiskSignature = *(PULONG)Irp->AssociatedIrp.SystemBuffer;

    // We have valid ulDiskSignature, now let us get all the device.
    pBasicDisk = GetBasicDiskFromDriverContext(ulDiskSignature);
    
#endif

#if ((NTDDI_VERSION >= NTDDI_WS03) && (NTDDI_VERSION < NTDDI_VISTA))

    if(pBasicDisk && IsListEmpty(&pBasicDisk->VolumeList)) {

        ustrDiskSig.Buffer = BufferDiskSig;
        ustrDiskSig.MaximumLength = 18;
        ustrDiskSig.Length = 0;
        
        RtlIntegerToUnicodeString(ulDiskSignature, 0x10, &ustrDiskSig);

        ustrDiskSig.Buffer[8] = 0;

        pRegDiskSignature = new Registry();
        if (pRegDiskSignature) {
            pKeyNameLength = (wcslen(REG_CLUSDISK_SIGNATURES_KEY) + wcslen(L"\\XXXXXXXX")) * sizeof(WCHAR) + sizeof(WCHAR);
            pKeyName = (PWCHAR)ExAllocatePoolWithTag(PagedPool, pKeyNameLength, TAG_GENERIC_PAGED);

            if(pKeyName) {
                RtlZeroMemory(pKeyName, pKeyNameLength);
                RtlStringCbCopyW(pKeyName, pKeyNameLength, REG_CLUSDISK_SIGNATURES_KEY);
                RtlStringCbCatW(pKeyName, pKeyNameLength, L"\\");
                RtlStringCbCatNW(pKeyName, pKeyNameLength, ustrDiskSig.Buffer, ustrDiskSig.MaximumLength);
                Status = pRegDiskSignature->OpenKeyReadOnly(pKeyName);

                if (STATUS_SUCCESS == Status) {
                    Status = pRegDiskSignature->ReadWString(REG_CLUSDISK_DISKNAME_VALUE, STRING_LEN_NULL_TERMINATED, &pValue);
                    if ((STATUS_SUCCESS == Status) && (NULL != pValue)) {
                        RtlStringCchCopyW(pBasicDisk->DiskName, MAX_DISKNAME_SIZE_IN_CHAR, pValue);
                    }
                    if (NULL != pValue)
                        ExFreePoolWithTag(pValue, TAG_REGISTRY_DATA);
                }        
                if (NULL != pKeyName)
                    ExFreePoolWithTag(pKeyName, TAG_GENERIC_PAGED);                    
            }

        
            if(wcslen(DISK_NAME_PREFIX)*sizeof(WCHAR) == RtlCompareMemory(pBasicDisk->DiskName, DISK_NAME_PREFIX, wcslen(DISK_NAME_PREFIX)*sizeof(WCHAR)))
            {
                UNICODE_STRING   ustrDiskName;
                ustrDiskName.Buffer = pBasicDisk->DiskName + wcslen(DISK_NAME_PREFIX);
                ustrDiskName.MaximumLength = (MAX_DISKNAME_SIZE_IN_CHAR-wcslen(DISK_NAME_PREFIX))*sizeof(WCHAR);
                ustrDiskName.Length = (MAX_DISKNAME_SIZE_IN_CHAR-wcslen(DISK_NAME_PREFIX))*sizeof(WCHAR);
                RtlUnicodeStringToInteger(&ustrDiskName, 10, &pBasicDisk->ulDiskNumber );
            }

            
            InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DeviceIoControlMSCSDiskAcquire: Diskname is %.*S Disk Number is %d\n\n",
                                        MAX_DISKNAME_SIZE_IN_CHAR, pBasicDisk->DiskName, pBasicDisk->ulDiskNumber));
        } else {
            InMageFltLogError(DriverContext.ControlDevice, __LINE__, INVOLFLT_ERR_NO_GENERIC_PAGED_POOL, STATUS_SUCCESS);
            InVolDbgPrint(DL_ERROR, DM_CLUSTER, ("DeviceIoControlMSCSDiskAcquire: Failed to allocate memory from paged pool for Basic Disk %#x\n\n",
                          "DiskSignature %#x\n\n",pBasicDisk, ulDiskSignature));
        }
        
    }

#endif
    ASSERT(NULL != pBasicDisk);
    if (NULL != pBasicDisk) {
        KeAcquireSpinLock(&pBasicDisk->Lock, &OldIrql);

        pBasicDisk->ulFlags |= BD_FLAGS_CLUSTER_ONLINE;
        pBasicVolume = (PBASIC_VOLUME)pBasicDisk->VolumeList.Flink;
        while ((PLIST_ENTRY)pBasicVolume != &pBasicDisk->VolumeList) {
            PVOLUME_CONTEXT     pVolumeContext = pBasicVolume->pVolumeContext;

            if (pVolumeContext) {
                InitializeClusterDiskAcquireParams(pVolumeContext);

                InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DeviceIoControlMSCSDiskAcquire: VolumeContext is %p, DriveLetter=%c\n",
                                    pVolumeContext, (CHAR)pVolumeContext->wDriveLetter[0]));
            } else {
#if (NTDDI_VERSION >= NTDDI_VISTA)
                if(cDiskInfo->ePartitionStyle == ecPartStyleMBR) {
                    InVolDbgPrint(DL_WARNING, DM_CLUSTER, ("DeviceIoControlMSCSDiskAcquire: VolumeContext is NULL, DiskSignature=%#x\n",
                                    cDiskInfo->ulDiskSignature));
                } else {
                //Can not print unicode_string at dispatch level
                }
#else
                InVolDbgPrint(DL_WARNING, DM_CLUSTER, ("DeviceIoControlMSCSDiskAcquire: VolumeContext is NULL, DiskSignature=%#x\n",
                                    ulDiskSignature));
#endif

            }

            pBasicVolume = (PBASIC_VOLUME)pBasicVolume->ListEntry.Flink;
        }

        KeReleaseSpinLock(&pBasicDisk->Lock, OldIrql);
        DereferenceBasicDisk(pBasicDisk);
    } else {
#if (NTDDI_VERSION >= NTDDI_VISTA)

        if(cDiskInfo->ePartitionStyle == ecPartStyleGPT) {
            UNICODE_STRING      GUIDString;
            if(STATUS_SUCCESS == RtlStringFromGUID(cDiskInfo->DiskId, &GUIDString)) {
            
                InVolDbgPrint(DL_WARNING, DM_CLUSTER, ("DeviceIoControlMSCSDiskAcquire: BasicDisk not found for DiskGUID %wZ\n",
                    GUIDString));        

                RtlFreeUnicodeString(&GUIDString);
            }
        } else {
            InVolDbgPrint(DL_WARNING, DM_CLUSTER, ("DeviceIoControlMSCSDiskAcquire: BasicDisk not found for DiskSignature = %#x\n",
                            cDiskInfo->ulDiskSignature));        

        }
        
#else
                
        InVolDbgPrint(DL_WARNING, DM_CLUSTER, ("DeviceIoControlMSCSDiskAcquire: BasicDisk not found for DiskSignature = %#x\n",
                            ulDiskSignature));        
#endif
    }

    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS
DeviceIoControlMSCSDiskRelease(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    NTSTATUS            Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulDiskSignature;
    LIST_ENTRY          ListHead;
    PLIST_NODE          pListNode;
    PBASIC_DISK         pBasicDisk;
    PBASIC_VOLUME       pBasicVolume;
    PVOLUME_CONTEXT     pVolumeContext;

    ASSERT(IOCTL_INMAGE_MSCS_DISK_RELEASE == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);
    if ((NULL == Irp->AssociatedIrp.SystemBuffer) ||
        (sizeof(ULONG) > IoStackLocation->Parameters.DeviceIoControl.InputBufferLength)) 
    {
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

#if (NTDDI_VERSION >= NTDDI_VISTA)

    CDSKFL_INFO  *cDiskInfo = (CDSKFL_INFO *)Irp->AssociatedIrp.SystemBuffer;
    
    if(cDiskInfo->ePartitionStyle == ecPartStyleMBR) {
        pBasicDisk = GetBasicDiskFromDriverContext(cDiskInfo->ulDiskSignature);
    } else if(cDiskInfo->ePartitionStyle == ecPartStyleGPT) {
        pBasicDisk = GetBasicDiskFromDriverContext(cDiskInfo->DiskId);
    } else
        pBasicDisk = NULL;
    
    
#else
    
    ulDiskSignature = *(PULONG)Irp->AssociatedIrp.SystemBuffer;

    // We have valid ulDiskSignature, now let us get all the device.
    pBasicDisk = GetBasicDiskFromDriverContext(ulDiskSignature);
#endif

    if (NULL != pBasicDisk) {
        pBasicDisk->ulFlags &= ~BD_FLAGS_CLUSTER_ONLINE;
        InitializeListHead(&ListHead);
        GetVolumeListForBasicDisk(pBasicDisk, &ListHead); 

        while (!IsListEmpty(&ListHead)) {
            pListNode = (PLIST_NODE)RemoveHeadList(&ListHead);
            ASSERT(NULL != pListNode);

            pBasicVolume = (PBASIC_VOLUME)pListNode->pData;
            ASSERT(NULL != pBasicVolume);

            pVolumeContext = pBasicVolume->pVolumeContext;
            if ((NULL != pVolumeContext) &&(!(pVolumeContext->ulFlags & VCF_FILTERING_STOPPED))) {
                InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DeviceIoControlMSCSDiskRelease: VolumeContext is %p, DriveLetter=%S VolumeBitmap is %p VolumeFlags is 0x%x\n",
                    pVolumeContext, pVolumeContext->wDriveLetter, pVolumeContext->pVolumeBitmap , pVolumeContext->ulFlags));

                if(pVolumeContext->ulFlags & VCF_CLUSTER_VOLUME_ONLINE){
                   
                    // By this time already we are in last chance mode as file system is unmounted.
                    // we write changes directly to volume and close the file.

                    if (pVolumeContext->ulFlags & VCF_CV_FS_UNMOUTNED) {
                        InVolDbgPrint( DL_INFO, DM_CLUSTER, ("File System is found unmounted at Release Disk!!!\n"));
                    } else {
                        InVolDbgPrint( DL_WARNING, DM_CLUSTER, ("File System is found mounted at Release Disk!!!\n"));
                    }

                    if (pVolumeContext->pVolumeBitmap) {
                        InMageFltSaveAllChanges(pVolumeContext, TRUE, NULL, true);
                        if (pVolumeContext->pVolumeBitmap->pBitmapAPI) {
                            BitmapPersistentValues BitmapPersistentValue(pVolumeContext);
                            pVolumeContext->pVolumeBitmap->pBitmapAPI->CommitBitmap(BitmapPersistentValue, NULL);
                        }
                    }

                    if (pVolumeContext->PnPNotificationEntry) {
                        IoUnregisterPlugPlayNotification(pVolumeContext->PnPNotificationEntry);
                        pVolumeContext->PnPNotificationEntry = NULL;
                    }
                    ASSERT(pVolumeContext->ChangeList.ulTotalChangesPending == 0);
                }
                
            }
            // Dereference volume for the refernce in list node.
            DereferenceBasicVolume(pBasicVolume);
            DereferenceListNode(pListNode);
        }

        DereferenceBasicDisk(pBasicDisk);
    }

    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS
DeviceIoControlMSCSGetOnlineDisks(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    NTSTATUS            Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG i = 0;

    ASSERT(IOCTL_INMAGE_MSCS_GET_ONLINE_DISKS == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);
    if ((NULL == Irp->AssociatedIrp.SystemBuffer) ||
        (sizeof (S_GET_DISKS) > IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength)) 
    {
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    S_GET_DISKS  *sDiskSignature = (S_GET_DISKS *) Irp->AssociatedIrp.SystemBuffer;

    if (DriverContext.IsDispatchEntryPatched != true) {
        sDiskSignature->IsVolumeAddedByEnumeration = false;
        Status = STATUS_SUCCESS;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = sizeof (S_GET_DISKS) + sDiskSignature->DiskSignaturesInputArrSize * sizeof(ULONG);
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    } else {
        sDiskSignature->IsVolumeAddedByEnumeration = true;
    }

    sDiskSignature->DiskSignaturesOutputArrSize = GetOnlineDisksSignature(sDiskSignature);

    ASSERT(sDiskSignature->DiskSignaturesOutputArrSize <= sDiskSignature->DiskSignaturesInputArrSize);

    if (sDiskSignature->DiskSignaturesOutputArrSize > sDiskSignature->DiskSignaturesInputArrSize) 
    {
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }
    // Can there be duplicate entries? i.e. multiple BasicDisk structures having same Signature?
    // Not possible

    Status = STATUS_SUCCESS;
    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = sizeof (S_GET_DISKS) + sDiskSignature->DiskSignaturesInputArrSize * sizeof(ULONG);
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return Status;
}

ULONG
GetOnlineDisksSignature(S_GET_DISKS  *sDiskSignature)
{
    NTSTATUS            Status = STATUS_SUCCESS;
    LIST_ENTRY          DiskListHead;
    PLIST_NODE          pDiskListNode;
    PBASIC_DISK         pBasicDisk;
    ULONG index = 0;

    // Get all Basic Disks
    InitializeListHead(&DiskListHead);
    GetBasicDiskList(&DiskListHead);
    while (!IsListEmpty(&DiskListHead)) {
        pDiskListNode = (PLIST_NODE)RemoveHeadList(&DiskListHead);
        ASSERT(NULL != pDiskListNode);

        pBasicDisk = (PBASIC_DISK) pDiskListNode->pData;
        ASSERT(NULL != pBasicDisk);

        // Check if this BasicDisk is a cluster disk
        if (!(pBasicDisk->ulFlags & BD_FLAGS_IS_CLUSTER_RESOURCE) ||
                pBasicDisk->IsAccessible != 1) {
            DereferenceBasicDisk(pBasicDisk);
            DereferenceListNode(pDiskListNode);
            continue;
        }

        if (index < sDiskSignature->DiskSignaturesInputArrSize) {
            sDiskSignature->DiskSignatures[index] = pBasicDisk->ulDiskSignature;
        }

        index++;

        DereferenceBasicDisk(pBasicDisk);
        DereferenceListNode(pDiskListNode);
    }            

    return index;
}

NTSTATUS
ProcessLocalDeviceControlRequests(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    PDEVICE_EXTENSION   DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulIoControlCode;
    NTSTATUS            Status;
 
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
    case IOCTL_INMAGE_GET_VOLUME_WRITE_ORDER_STATE:
        Status = DeviceIoControlGetVolumeWriteOrderState(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_GET_VOLUME_SIZE:
        Status = DeviceIoControlGetVolumeSize(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_GET_VOLUME_FLAGS:
        Status = DeviceIoControlGetVolumeFlags(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_GET_VOLUME_STATS:
        Status = DeviceIoControlGetVolumeStats(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_GET_ADDITIONAL_VOLUME_STATS:
        Status = DeviceIoControlGetAdditionalVolumeStats(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_GET_DATA_MODE_INFO:
        Status = DeviceIoControlGetDMinfo(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_MSCS_DISK_REMOVED_FROM_CLUSTER:
        Status = DeviceIoControlMSCSDiskRemovedFromCluster(DeviceObject, Irp);
        break;
//        case IOCTL_INMAGE_MSCS_DISK_ADDED_TO_CLUSTER:
//            Status = DeviceIoControlMSCSDiskAddedToCluster(DeviceObject, Irp);
//            break;
    case IOCTL_INMAGE_MSCS_DISK_ACQUIRE:
        Status = DeviceIoControlMSCSDiskAcquire(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_MSCS_DISK_RELEASE:
        Status = DeviceIoControlMSCSDiskRelease(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_MSCS_GET_ONLINE_DISKS:
        Status = DeviceIoControlMSCSGetOnlineDisks(DeviceObject, Irp);
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
    case IOCTL_INMAGE_SET_VOLUME_FLAGS:
        Status = DeviceIoControlSetVolumeFlags(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_CLEAR_VOLUME_STATS:
        Status = DeviceIoControlClearVolumeStats(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_TAG_VOLUME:
        Status = DeviceIoControlTagVolume(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_SYNC_TAG_VOLUME:
        Status = DeviceIoControlSyncTagVolume(DeviceObject, Irp);
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
DeviceIoControlMountDevLinkCreated(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    PDEVICE_EXTENSION   DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulIoControlCode;
    NTSTATUS            Status;
    PMOUNTDEV_NAME      pMountDevName;
    PVOLUME_CONTEXT     pVolumeContext;
    UNICODE_STRING      VolumeNameWithGUID;
    WCHAR               VolumeGUID[GUID_SIZE_IN_CHARS + 1] = {0};
    pVolumeContext = DeviceExtension->pVolumeContext;

    ASSERT(DISPATCH_LEVEL > KeGetCurrentIrql());

    if (NULL == pVolumeContext) {
        IoSkipCurrentIrpStackLocation(Irp);
        Status = IoCallDriver(DeviceExtension->TargetDeviceObject, Irp);
        return Status;
    }

    // See if the mount manager is informing the GUID of the volume if so
    // save the GUID.
    pMountDevName = (PMOUNTDEV_NAME)Irp->AssociatedIrp.SystemBuffer;
    InVolDbgPrint(DL_VERBOSE, DM_CLUSTER, 
                  ("DeviceIoControlMountDevLinkCreated: %.*S\n", pMountDevName->NameLength / sizeof(WCHAR), pMountDevName->Name))
    // Minimum 0x60 for name and 0x02 for length.
    if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength >= 0x62)
    {
         if (INMAGE_IS_MOUNTDEV_NAME_VOLUME_GUID(pMountDevName)) {
            RtlCopyMemory(VolumeGUID, &pMountDevName->Name[GUID_START_INDEX_IN_MOUNTDEV_NAME], 
                        GUID_SIZE_IN_BYTES);
            ConvertGUIDtoLowerCase(VolumeGUID);
            if (pVolumeContext->ulFlags & VCF_GUID_OBTAINED) {
                // GUID is already obtained.
                if (GUID_SIZE_IN_BYTES != RtlCompareMemory(VolumeGUID, pVolumeContext->wGUID, GUID_SIZE_IN_BYTES)) {
                    PVOLUME_GUID    pVolumeGUID = NULL;
                    KIRQL           OldIrql;

                    InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DeviceIoControlMountDevLinkCreated: Found more than one GUID for volume\n%S\n%S\n",
                                                        pVolumeContext->wGUID, VolumeGUID));
                    KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
                    pVolumeGUID = pVolumeContext->GUIDList;
                    while(NULL != pVolumeGUID) {
                        if (GUID_SIZE_IN_BYTES == RtlCompareMemory(VolumeGUID, pVolumeGUID->GUID, GUID_SIZE_IN_BYTES)) {
                            InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DeviceIoControlMountDevLinkCreated: Found same GUID again\n"));
                            break;
                        }
                        pVolumeGUID = pVolumeGUID->NextGUID;
                    }

                    if (NULL == pVolumeGUID) {
                        pVolumeGUID = (PVOLUME_GUID)ExAllocatePoolWithTag(NonPagedPool, sizeof(VOLUME_GUID), TAG_GENERIC_NON_PAGED);
                        if (NULL != pVolumeGUID) {
                            InterlockedIncrement(&DriverContext.lAdditionalGUIDs);
                            pVolumeContext->lAdditionalGUIDs++;
                            RtlZeroMemory(pVolumeGUID, sizeof(VOLUME_GUID));
                            RtlCopyMemory(pVolumeGUID->GUID, VolumeGUID, GUID_SIZE_IN_BYTES);
                            pVolumeGUID->NextGUID = pVolumeContext->GUIDList;
                            pVolumeContext->GUIDList = pVolumeGUID;
                        } else {
                            InMageFltLogError(pVolumeContext->FilterDeviceObject, __LINE__, INVOLFLT_ERR_NO_GENERIC_NPAGED_POOL, STATUS_SUCCESS,
                                               pVolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES, pVolumeContext->wGUID, GUID_SIZE_IN_BYTES);
                            InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DeviceIoControlMountDevLinkCreated: Failed to allocate VOLUMEGUID Struct\n"));
                        }
                    }
                    KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);
                } else {
                    InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DeviceIoControlMountDevLinkCreated: Found same GUID again %S\n", VolumeGUID));
                }
            } else {
                // Copy VolumeDeviceName to VolumeContext
                RtlCopyMemory(pVolumeContext->BufferForUniqueVolumeName, 
                                pMountDevName->Name, 
                                MIN_UNIQUE_VOLUME_NAME_SIZE_IN_WCHARS * sizeof(WCHAR));
                pVolumeContext->BufferForUniqueVolumeName[MIN_UNIQUE_VOLUME_NAME_SIZE_IN_WCHARS] = '\0';
                pVolumeContext->UniqueVolumeName.Length = MIN_UNIQUE_VOLUME_NAME_SIZE_IN_WCHARS * sizeof(WCHAR);

				RtlCopyMemory(pVolumeContext->wGUID, VolumeGUID, GUID_SIZE_IN_BYTES);
                ASSERT (0 != pVolumeContext->VolumeParameters.MaximumLength);

                InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DeviceIoControlMountDevLinkCreated: VolumeContext is %p, GUID=%S\n",
                                    pVolumeContext, pVolumeContext->wGUID));

                RtlCopyUnicodeString(&pVolumeContext->VolumeParameters, &DriverContext.DriverParameters);
                VolumeNameWithGUID.MaximumLength = VolumeNameWithGUID.Length = VOLUME_NAME_SIZE_IN_BYTES;
                VolumeNameWithGUID.Buffer = &pMountDevName->Name[VOLUME_NAME_START_INDEX_IN_MOUNTDEV_NAME];
                RtlAppendUnicodeStringToString(&pVolumeContext->VolumeParameters, &VolumeNameWithGUID);
                pVolumeContext->ulFlags |= VCF_GUID_OBTAINED;

				//Fix for bug 25726
				Status = InMageCrossCheckNewVolume(DeviceObject, pVolumeContext, pMountDevName);

                Status = LoadVolumeCfgFromRegistry(pVolumeContext);

                if (pVolumeContext->ulFlags & VCF_VOLUME_ON_BASIC_DISK) {
                    PBASIC_VOLUME    pBasicVolume;

                    InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DeviceIoControlMountDevLinkCreated: Volume(%S) is on BasicDisk\n",
                                        pVolumeContext->wGUID));

                    // Volume is on basic disk.
                    pBasicVolume = AddBasicVolumeToDriverContext(pVolumeContext->ulDiskSignature, pVolumeContext);

                    if (NULL != pBasicVolume) {
                        ASSERT(NULL != pBasicVolume->pDisk);

                        if ((NULL != pBasicVolume->pDisk) && (pBasicVolume->pDisk->ulFlags & BD_FLAGS_CLUSTER_ONLINE)) {
                            InitializeClusterDiskAcquireParams(pVolumeContext);
                        }

                        if (pVolumeContext->ulFlags & VCF_VOLUME_LETTER_OBTAINED) {
                            pBasicVolume->BuferForDriveLetterBitMap = pVolumeContext->BufferForDriveLetterBitMap;
                            pBasicVolume->DriveLetter = pVolumeContext->wDriveLetter[0];
                            pBasicVolume->ulFlags |= BV_FLAGS_HAS_DRIVE_LETTER;
                        }

                    }
                }
            }  // if (pVolumeContext->ulFlags & VCF_GUID_OBTAINED) else ...
        }
    } else {
        // 0x1C for Drive letter and 0x02 for length
        // \DosDevices\X:
        if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength >= 0x1E) {
            if (INMAGE_IS_MOUNTDEV_NAME_VOLUME_LETTER(pMountDevName)) {
                SetBitRepresentingDriveLetter(pMountDevName->Name[VOLUME_LETTER_START_INDEX_IN_MOUNTDEV_NAME], 
                                              &pVolumeContext->DriveLetterBitMap);
                pVolumeContext->ulFlags |= VCF_VOLUME_LETTER_OBTAINED;
                pVolumeContext->wDriveLetter[0] = pMountDevName->Name[VOLUME_LETTER_START_INDEX_IN_MOUNTDEV_NAME];
                pVolumeContext->DriveLetter[0] = (CHAR)pMountDevName->Name[VOLUME_LETTER_START_INDEX_IN_MOUNTDEV_NAME];
                InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DeviceIoControlMountDevLinkCreated: VolumeContext is %p, DriveLetter=%S\n",
                                    pVolumeContext, pVolumeContext->wDriveLetter));
              
                if (pVolumeContext->ulFlags & VCF_GUID_OBTAINED) {
                    AddVolumeLetterToRegVolumeCfg(pVolumeContext);

                    if (pVolumeContext->pBasicVolume) {
                        pVolumeContext->pBasicVolume->BuferForDriveLetterBitMap = pVolumeContext->BufferForDriveLetterBitMap;
                        pVolumeContext->pBasicVolume->DriveLetter = pVolumeContext->wDriveLetter[0];
                        pVolumeContext->pBasicVolume->ulFlags |= BV_FLAGS_HAS_DRIVE_LETTER;
                    }
                }
            }
        }
    }

    

    if(!pVolumeContext->IsVolumeUsingAddDevice && DriverContext.IsDispatchEntryPatched) {
        if(pVolumeContext->IsVolumeCluster)
            Status = (*DriverContext.MajorFunctionForClusDrives[IRP_MJ_DEVICE_CONTROL])(DeviceExtension->TargetDeviceObject, Irp);
        else
            Status = (*DriverContext.MajorFunction[IRP_MJ_DEVICE_CONTROL])(DeviceExtension->TargetDeviceObject, Irp);
    } else {
        IoSkipCurrentIrpStackLocation(Irp);
        Status = IoCallDriver(DeviceExtension->TargetDeviceObject, Irp);

    }

    return Status;
}

NTSTATUS
DeviceIoControlMountDevLinkDeleted(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    PDEVICE_EXTENSION   DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulIoControlCode;
    NTSTATUS            Status;
    PMOUNTDEV_NAME      pMountDevName;
    PVOLUME_CONTEXT     pVolumeContext;
    UNICODE_STRING      VolumeNameWithGUID;
 
    pVolumeContext = DeviceExtension->pVolumeContext;
    ulIoControlCode = IoStackLocation->Parameters.DeviceIoControl.IoControlCode;

    ASSERT(DISPATCH_LEVEL > KeGetCurrentIrql());
    ASSERT((IOCTL_MOUNTDEV_LINK_DELETED_WNET_AND_ABOVE == ulIoControlCode) || (IOCTL_MOUNTDEV_LINK_DELETED_WXP_AND_BELOW == ulIoControlCode));

    if (NULL == pVolumeContext) {
        IoSkipCurrentIrpStackLocation(Irp);
        Status = IoCallDriver(DeviceExtension->TargetDeviceObject, Irp);
        return Status;
    }

    // See if the mount manager is informing the GUID of the volume if so
    // save the GUID.
    pMountDevName = (PMOUNTDEV_NAME)Irp->AssociatedIrp.SystemBuffer;
    InVolDbgPrint(DL_VERBOSE, DM_CLUSTER, 
                  ("DeviceIoControlMountDevLinkDeleted: %.*S\n", pMountDevName->NameLength / sizeof(WCHAR), pMountDevName->Name))

    if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength >= 0x1E) {
        if (INMAGE_IS_MOUNTDEV_NAME_VOLUME_LETTER(pMountDevName)) {
            ClearBitRepresentingDriveLetter(pMountDevName->Name[VOLUME_LETTER_START_INDEX_IN_MOUNTDEV_NAME], 
                                            &pVolumeContext->DriveLetterBitMap);
            if (0 == pVolumeContext->BufferForDriveLetterBitMap)
            {
                pVolumeContext->ulFlags &= ~VCF_VOLUME_LETTER_OBTAINED;
            }
        }
    }

    if(!pVolumeContext->IsVolumeUsingAddDevice && DriverContext.IsDispatchEntryPatched) {
        if(pVolumeContext->IsVolumeCluster)
            Status = (*DriverContext.MajorFunctionForClusDrives[IRP_MJ_DEVICE_CONTROL])(DeviceExtension->TargetDeviceObject, Irp);
        else
            Status = (*DriverContext.MajorFunction[IRP_MJ_DEVICE_CONTROL])(DeviceExtension->TargetDeviceObject, Irp);
    } else {    
        IoSkipCurrentIrpStackLocation(Irp);
        Status = IoCallDriver(DeviceExtension->TargetDeviceObject, Irp);
    
    }

    return Status;
}

NTSTATUS
DeviceIoControlMountDevQueryUniqueID(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    PDEVICE_EXTENSION   DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    NTSTATUS            Status;
    PVOLUME_CONTEXT     pVolumeContext;
    UNICODE_STRING      VolumeNameWithGUID;
    PMOUNTDEV_UNIQUE_ID pMountDevUniqueID = NULL;
    
    
    ASSERT(DISPATCH_LEVEL > KeGetCurrentIrql());
 
    pVolumeContext = DeviceExtension->pVolumeContext;
    if (NULL == pVolumeContext) {
        InVolDbgPrint(DL_ERROR, DM_CLUSTER, ("DeviceIoControlMountDevQueryUniqueID: VolumeContext is NULL\n"));
        IoSkipCurrentIrpStackLocation(Irp);
        Status = IoCallDriver(DeviceExtension->TargetDeviceObject, Irp);
        return Status;
    }

    Status = InMageFltForwardIrpSynchronous(DeviceObject, Irp);

    // IOCTL is method buffered. So dump the info being returned.
    if (Irp->IoStatus.Information >= 2) {
        pMountDevUniqueID  = (PMOUNTDEV_UNIQUE_ID)Irp->AssociatedIrp.SystemBuffer;
        if ((NULL != pMountDevUniqueID) && (pMountDevUniqueID->UniqueIdLength == 12) &&
            (Irp->IoStatus.Information >= (ULONG)(FIELD_OFFSET(MOUNTDEV_UNIQUE_ID, UniqueId) + pMountDevUniqueID->UniqueIdLength)))
        {
            ULONG           ulDiskSignature;
            PBASIC_DISK     pBasicDisk;

            if (pVolumeContext->ulFlags & VCF_GUID_OBTAINED) {
                InVolDbgPrint(DL_INFO, DM_CLUSTER, ("%.*S DeviceObject = %p, ", GUID_SIZE_IN_CHARS, pVolumeContext->wGUID, DeviceObject));
            } else {
                InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DeviceObject = %p, ", DeviceObject));
            }

            ulDiskSignature = *(ULONG UNALIGNED *)pMountDevUniqueID->UniqueId;
            for (int i = 0; i < pMountDevUniqueID->UniqueIdLength; i++) {
                InVolDbgPrint(DL_INFO, DM_CLUSTER, ("0x%x ", pMountDevUniqueID->UniqueId[i]));
            }
            InVolDbgPrint(DL_INFO, DM_CLUSTER, ("\n"));
            InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DiskSignature = 0x%x\n", ulDiskSignature));

            pVolumeContext->ulDiskSignature = ulDiskSignature;
            pVolumeContext->ulFlags |= VCF_VOLUME_ON_BASIC_DISK;

            pBasicDisk = AddBasicDiskToDriverContext(ulDiskSignature);
            if (NULL != pBasicDisk) {
                if (pBasicDisk->ulFlags & BD_FLAGS_IS_CLUSTER_RESOURCE) {
                    pVolumeContext->ulFlags |= VCF_VOLUME_ON_CLUS_DISK;
                } else {
                    pVolumeContext->bQueueChangesToTempQueue = false;
                }
                DereferenceBasicDisk(pBasicDisk);
            }
        }
        if ((NULL != pMountDevUniqueID) && (pMountDevUniqueID->UniqueIdLength == 24) &&
            (Irp->IoStatus.Information >= (ULONG)(FIELD_OFFSET(MOUNTDEV_UNIQUE_ID, UniqueId) + pMountDevUniqueID->UniqueIdLength)))
        {
            
            PBASIC_DISK     pBasicDisk = NULL;

            pBasicDisk = GetBasicDiskFromDriverContext(0, pVolumeContext->ulDiskNumber);

            if(pBasicDisk == NULL) {//for GPT non cluster volumes and dynamic disk volumes
                pVolumeContext->bQueueChangesToTempQueue = false;
            } else {
                DereferenceBasicDisk(pBasicDisk);
            }

            InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DeviceIoControlMountDevQueryUniqueID: Unique Id length is %d\n", pMountDevUniqueID->UniqueIdLength));
            
            
        }
    }

    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return Status;
}

NTSTATUS
DeviceControlCompletionRoutine(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
{
    NTSTATUS Status = Irp->IoStatus.Status;
    NTSTATUS NewStatus = STATUS_SUCCESS;
    ULONG ulDataWritten = (ULONG)Irp->IoStatus.Information;
    KIRQL OldIrql;
    PCOMPLETION_CONTEXT pCompletionContext = NULL;
    PVOLUME_CONTEXT VolumeContext;
    etContextType eContextType = (etContextType)*((PCHAR )Context + sizeof(LIST_ENTRY));
    DISK_COPY_DATA_PARAMETERS *DiskCopyDataParameters = NULL;
    ULONG Bufferlength = 0;
    WRITE_META_DATA WriteMetaData;    
    BOOLEAN bTrackData = TRUE;
    LONGLONG DataOffset = 0;
    LONGLONG SourceDataOffset = 0;
    LONGLONG DataLength = 0;
    ULONG MaxUlongVal = 0xFFFFFFFF;
#if DBG
    BOOLEAN bPrintStat = FALSE;
#endif
    DiskCopyDataParameters = (DISK_COPY_DATA_PARAMETERS *) Irp->AssociatedIrp.SystemBuffer;
    ASSERT(DiskCopyDataParameters != NULL);
    
    if (eContextType == ecCompletionContext) {
        pCompletionContext = (PCOMPLETION_CONTEXT)Context;
        VolumeContext = pCompletionContext->VolumeContext;
        ASSERT(pCompletionContext->llOffset == DiskCopyDataParameters->DestinationOffset.QuadPart);
        ASSERT(pCompletionContext->llLength == DiskCopyDataParameters->CopyLength.QuadPart);
        DataOffset = pCompletionContext->llOffset;
        DataLength = pCompletionContext->llLength;
        SourceDataOffset = DiskCopyDataParameters->SourceOffset.QuadPart;
    } else {
        PIO_STACK_LOCATION IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
        VolumeContext = (PVOLUME_CONTEXT)Context;
        if (Irp->PendingReturned) {
            IoMarkIrpPending(Irp);
        }
        Bufferlength = IoStackLocation->Parameters.DeviceIoControl.InputBufferLength;
        ASSERT(Bufferlength == sizeof (DISK_COPY_DATA_PARAMETERS));
        DataOffset = DiskCopyDataParameters->DestinationOffset.QuadPart;
        DataLength = DiskCopyDataParameters->CopyLength.QuadPart;
        SourceDataOffset = DiskCopyDataParameters->SourceOffset.QuadPart;
    }

    if (VolumeContext->llVolumeSize > 0) {
        if ((DataOffset >= VolumeContext->llVolumeSize) ||
            (SourceDataOffset >= VolumeContext->llVolumeSize) ||
            (DataOffset < 0) ||
            (SourceDataOffset < 0) ||
            (DataLength <= 0) ||
            ((DataOffset + DataLength) > VolumeContext->llVolumeSize) ||
            ((SourceDataOffset + DataLength) > VolumeContext->llVolumeSize)) {
                bTrackData = FALSE;
                // Log an error message in event log.
                InMageFltLogError(VolumeContext->FilterDeviceObject, __LINE__, INVOLFLT_IOCTL_DISK_COPY_DATA_RECEIVED_VALIDATION_FAILED, STATUS_SUCCESS,
                    VolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES, VolumeContext->wGUID, GUID_SIZE_IN_BYTES,
                    VolumeContext->IoctlDiskCopyDataCount, VolumeContext->IoctlDiskCopyDataCountSuccess);
        }
    }

    KeAcquireSpinLock(&VolumeContext->Lock, &OldIrql);

    if (NT_SUCCESS(Status)) {
#if DBG
        if (VolumeContext->IoctlDiskCopyDataCountSuccess == 0) {
            bPrintStat = TRUE;
        }
#endif
        VolumeContext->IoctlDiskCopyDataCountSuccess += 1;
    } else {
#if DBG
        if (VolumeContext->IoctlDiskCopyDataCountFailure == 0) {
            bPrintStat = TRUE;
        }
#endif
        VolumeContext->IoctlDiskCopyDataCountFailure += 1;
    }
    VolumeContext->IoctlDiskCopyDataCount += 1;

    ULONGLONG RetStatus = 0;
    if (NT_SUCCESS(Status)) {
        RetStatus = 1;
    }

    if ((DataOffset % 512 != 0) || (DataLength % 512 != 0) || (SourceDataOffset % 512 != 0)) {
        // Print a message in log.
        InMageFltLogError(VolumeContext->FilterDeviceObject, __LINE__, INVOLFLT_IOCTL_DISK_COPY_DATA_RECEIVED_VALIDATION_FAILED2, STATUS_SUCCESS,
            VolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES, VolumeContext->wGUID, GUID_SIZE_IN_BYTES,
            VolumeContext->IoctlDiskCopyDataCount, RetStatus);        
    }

    if (DataOffset == SourceDataOffset) {
        InMageFltLogError(VolumeContext->FilterDeviceObject, __LINE__, INVOLFLT_IOCTL_DISK_COPY_DATA_RECEIVED_VALIDATION_FAILED3, STATUS_SUCCESS,
            VolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES, VolumeContext->wGUID, GUID_SIZE_IN_BYTES,
            VolumeContext->IoctlDiskCopyDataCount, RetStatus);                
    }
    if (DataOffset > SourceDataOffset) {
        if ((SourceDataOffset + DataLength) > DataOffset) {
            InMageFltLogError(VolumeContext->FilterDeviceObject, __LINE__, INVOLFLT_IOCTL_DISK_COPY_DATA_RECEIVED_VALIDATION_FAILED3, STATUS_SUCCESS,
                VolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES, VolumeContext->wGUID, GUID_SIZE_IN_BYTES,
                VolumeContext->IoctlDiskCopyDataCount, RetStatus);                    
        }
    } else {
        if ((DataOffset + DataLength) > SourceDataOffset) {
            InMageFltLogError(VolumeContext->FilterDeviceObject, __LINE__, INVOLFLT_IOCTL_DISK_COPY_DATA_RECEIVED_VALIDATION_FAILED3, STATUS_SUCCESS,
                VolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES, VolumeContext->wGUID, GUID_SIZE_IN_BYTES,
                VolumeContext->IoctlDiskCopyDataCount, RetStatus);                    
        }
    }

    InVolDbgPrint(DL_VERBOSE, DM_IOCTL_TRACE, ("IOCTL = (IOCTL_DISK_COPY_DATA Completion)"));
#if DBG
    if (bPrintStat == TRUE) {
        InMageFltLogError(VolumeContext->FilterDeviceObject, __LINE__, INVOLFLT_IOCTL_DISK_COPY_DATA_RECEIVED, STATUS_SUCCESS,
            VolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES, VolumeContext->wGUID, GUID_SIZE_IN_BYTES,
            VolumeContext->IoctlDiskCopyDataCount, VolumeContext->IoctlDiskCopyDataCountSuccess);
    }
#endif
    if (NT_SUCCESS(Status) && bTrackData == TRUE && (0 == (VolumeContext->ulFlags & VCF_FILTERING_STOPPED))) {
        // etCaptureMode VolumeCaptureMode = VolumeContext->eCaptureMode;

        while (DataLength != 0 && NT_SUCCESS(NewStatus)) {
            if (DataLength > MaxUlongVal) {
                WriteMetaData.llOffset = DataOffset;
                WriteMetaData.ulLength = MaxUlongVal;
                DataOffset += MaxUlongVal;
                DataLength -= MaxUlongVal;
#if DBG
                InMageFltLogError(VolumeContext->FilterDeviceObject, __LINE__, INVOLFLT_IOCTL_DISK_COPY_DATA_RECEIVED_INFO, STATUS_SUCCESS,
                    VolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES, VolumeContext->wGUID, GUID_SIZE_IN_BYTES,
                    VolumeContext->IoctlDiskCopyDataCount, VolumeContext->IoctlDiskCopyDataCountSuccess);
#endif
            } else {
                WriteMetaData.llOffset = DataOffset;
                WriteMetaData.ulLength = (ULONG) DataLength;
                ASSERT(DataLength == WriteMetaData.ulLength);
                if (DataLength != WriteMetaData.ulLength) {
                    InMageFltLogError(VolumeContext->FilterDeviceObject, __LINE__, INVOLFLT_IOCTL_DISK_COPY_DATA_RECEIVED_ERROR2, STATUS_SUCCESS,
                        VolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES, VolumeContext->wGUID, GUID_SIZE_IN_BYTES,
                        VolumeContext->IoctlDiskCopyDataCount, VolumeContext->IoctlDiskCopyDataCountSuccess);
                }
                DataLength = 0;
            }
            
            NewStatus = AddMetaDataToVolumeContextNew(VolumeContext, &WriteMetaData);
            if (!NT_SUCCESS(NewStatus)) {
                break;
            }
        }
        if (!NT_SUCCESS(NewStatus)) {
            InMageFltLogError(VolumeContext->FilterDeviceObject, __LINE__, INVOLFLT_IOCTL_DISK_COPY_DATA_RECEIVED_ERROR, STATUS_SUCCESS,
                VolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES, VolumeContext->wGUID, GUID_SIZE_IN_BYTES,
                VolumeContext->IoctlDiskCopyDataCount, VolumeContext->IoctlDiskCopyDataCountSuccess);            
            QueueWorkerRoutineForSetVolumeOutOfSync(VolumeContext, ERROR_TO_REG_OUT_OF_MEMORY_FOR_DIRTY_BLOCKS, STATUS_NO_MEMORY, true);
        }

        ASSERT(ecCaptureModeMetaData == VolumeContext->eCaptureMode);
        ASSERT(ecWriteOrderStateData != VolumeContext->eWOState);

        // if (VolumeCaptureMode == ecCaptureModeData) {
        //    VCSetCaptureModeToData(VolumeContext);
        // }
        // ASSERT(ecWriteOrderStateData != VolumeContext->eWOState);
    }

    KeReleaseSpinLock(&VolumeContext->Lock, OldIrql);
    DereferenceVolumeContext(VolumeContext);

    if (eContextType == ecCompletionContext) {
        if (pCompletionContext->CompletionRoutine) {
            PIO_STACK_LOCATION IoNextStackLocation = IoGetNextIrpStackLocation(Irp);
            IoNextStackLocation->CompletionRoutine = pCompletionContext->CompletionRoutine;
            IoNextStackLocation->Context = pCompletionContext->Context;
            IoNextStackLocation->Control = pCompletionContext->Control;
            Status = (*pCompletionContext->CompletionRoutine)(DeviceObject, Irp, pCompletionContext->Context);
        }
        ExFreePool(pCompletionContext);  
    }
    
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
    PIRP Irp
    )
{
    PDEVICE_EXTENSION   DeviceExtension = NULL;//(PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulIoControlCode,ulDeviceType;
    PVOLUME_CONTEXT     pVolumeContext;
    NTSTATUS            Status;
    PDRIVER_DISPATCH     DispatchEntryForDevIOControl = NULL;
    PDEVICE_OBJECT      DevObj = DeviceObject;

    if (DeviceObject == DriverContext.ControlDevice)
    {
        return ProcessLocalDeviceControlRequests(DeviceObject, Irp);
    }

    ulIoControlCode = IoStackLocation->Parameters.DeviceIoControl.IoControlCode;

    
     if(DriverContext.IsDispatchEntryPatched) {
        pVolumeContext = GetVolumeContextForThisIOCTL(DeviceObject, NULL);
        if(pVolumeContext) {       
            DeviceObject = pVolumeContext->FilterDeviceObject;
            DeviceExtension = (PDEVICE_EXTENSION)pVolumeContext->FilterDeviceObject->DeviceExtension;
            if(pVolumeContext->IsVolumeCluster) {
                DispatchEntryForDevIOControl = DriverContext.MajorFunctionForClusDrives[IRP_MJ_DEVICE_CONTROL];
            } else {
                DispatchEntryForDevIOControl = DriverContext.MajorFunction[IRP_MJ_DEVICE_CONTROL];
            }
            
            if(pVolumeContext->IsVolumeUsingAddDevice)
                return (*DispatchEntryForDevIOControl)(DeviceExtension->TargetDeviceObject, Irp);

        } else {
            if(DeviceObject->DriverObject == DriverContext.DriverObject) {
                
                DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
                pVolumeContext = DeviceExtension->pVolumeContext;

                // Bug Fix: 14969: System stop responding with NoReboot Driver
                if (pVolumeContext->IsVolumeUsingAddDevice != true) {
                    IoSkipCurrentIrpStackLocation(Irp);
                    return IoCallDriver(DeviceExtension->TargetDeviceObject, Irp);
                }
  
                if(pVolumeContext){
                    if(pVolumeContext->IsVolumeCluster) {
                        DispatchEntryForDevIOControl = DriverContext.MajorFunctionForClusDrives[IRP_MJ_DEVICE_CONTROL];
                    } else {
                        DispatchEntryForDevIOControl = DriverContext.MajorFunction[IRP_MJ_DEVICE_CONTROL];
                    }
                }

                
            } else if (DriverContext.pDriverObject == DeviceObject->DriverObject ) {
                
                return (*DriverContext.MajorFunction[IRP_MJ_DEVICE_CONTROL])(DeviceObject, Irp);      
                
            } else
                return (*DriverContext.MajorFunctionForClusDrives[IRP_MJ_DEVICE_CONTROL])(DeviceObject, Irp);   

        }
    } else {
        DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    }

    //DumpIoctlInformation(DeviceObject, Irp);

    ulDeviceType = IOCTL_DEVICE_TYPE(ulIoControlCode);
    
    
    pVolumeContext = DeviceExtension->pVolumeContext;

#if (NTDDI_VERSION >= NTDDI_WINXP)
    InVolDbgPrint(DL_FUNC_TRACE, DM_INMAGE_IOCTL, ("InMageFltDeviceControl: Process = %.16s(%x.%x) DeviceObject = %#p  Irp = %#p  CtlCode = %#x\n",
                        PsGetProcessImageFileName(PsGetCurrentProcess()), PtrToUlong(PsGetCurrentProcessId()), PtrToUlong(PsGetCurrentThreadId()), DeviceObject, Irp, ulIoControlCode));

#else
    InVolDbgPrint(DL_FUNC_TRACE, DM_INMAGE_IOCTL, ("InMageFltDeviceControl: DeviceObject = %#p  Irp = %#p  CtlCode = %#x\n",
                        DeviceObject, Irp, ulIoControlCode));
#endif


    

    switch(ulIoControlCode) {
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
    case IOCTL_MOUNTDEV_LINK_CREATED_WXP_AND_BELOW:
    case IOCTL_MOUNTDEV_LINK_CREATED_WNET_AND_ABOVE:
        Status = DeviceIoControlMountDevLinkCreated(DeviceObject, Irp);
        break;
    case IOCTL_MOUNTDEV_LINK_DELETED_WNET_AND_ABOVE:
    case IOCTL_MOUNTDEV_LINK_DELETED_WXP_AND_BELOW:
        Status = DeviceIoControlMountDevLinkDeleted(DeviceObject, Irp);
        break;
    case IOCTL_MOUNTDEV_QUERY_UNIQUE_ID:
                
        if( !pVolumeContext->IsVolumeUsingAddDevice && DriverContext.IsDispatchEntryPatched) {    
            Status = (*DispatchEntryForDevIOControl)(DeviceExtension->TargetDeviceObject, Irp);
        } else {
            Status = DeviceIoControlMountDevQueryUniqueID(DeviceObject, Irp);
        }
        
        break;
    case IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS:
        Status = DeviceIoControlGetDirtyBlocksTransaction(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS:
        Status = DeviceIoControlCommitDirtyBlocksTransaction(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_SET_DIRTY_BLOCK_NOTIFY_EVENT:
        Status = DeviceIoControlSetDirtyBlockNotifyEvent(DeviceObject, Irp);
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
#if (NTDDI_VERSION >= NTDDI_WS03)
    case IOCTL_VOLUME_OFFLINE:
        Status = DeviceIoControlVolumeOffline(DeviceObject, Irp);
        break;
    case IOCTL_VOLUME_ONLINE:
        Status = DeviceIoControlVolumeOnline(DeviceObject, Irp);
        break;

#endif
#if (NTDDI_VERSION >= NTDDI_WINXP)
    case IOCTL_VOLSNAP_COMMIT_SNAPSHOT:
        Status = DeviceIoControlCommitVolumeSnapshot(DeviceObject, Irp);
        break;
    
#endif
#if DBG
//    case IOCTL_INMAGE_ADD_DIRTY_BLOCKS_TEST:
//        Status = DeviceIoControlAddDirtyBlocksTest(DeviceObject, Irp);
//        break;
#endif // DBG
    default:
    
        if (!pVolumeContext->IsVolumeUsingAddDevice && DriverContext.IsDispatchEntryPatched) {
            if ((!(pVolumeContext->ulFlags & VCF_FILTERING_STOPPED))  && ulDeviceType == IOCTL_DISK_BASE
                    && ulIoControlCode == IOCTL_DISK_COPY_DATA) {
                DISK_COPY_DATA_PARAMETERS *DiskCopyDataParameters = NULL;
                ULONG Bufferlength = 0;
                DiskCopyDataParameters = (DISK_COPY_DATA_PARAMETERS *) Irp->AssociatedIrp.SystemBuffer;
                ASSERT(DiskCopyDataParameters != NULL);
                Bufferlength = IoStackLocation->Parameters.DeviceIoControl.InputBufferLength;
                ASSERT(Bufferlength == sizeof (DISK_COPY_DATA_PARAMETERS));

                COMPLETION_CONTEXT *pCompRoutine = (PCOMPLETION_CONTEXT)ExAllocatePoolWithTag(NonPagedPool, sizeof(COMPLETION_CONTEXT), TAG_GENERIC_NON_PAGED);
                if (pCompRoutine) {
                    RtlZeroMemory(pCompRoutine, sizeof(COMPLETION_CONTEXT));
                    ReferenceVolumeContext(pVolumeContext);
                    pCompRoutine->eContextType = ecCompletionContext;
                    pCompRoutine->VolumeContext = pVolumeContext;
                    pCompRoutine->llOffset = DiskCopyDataParameters->DestinationOffset.QuadPart;
                    pCompRoutine->llLength = DiskCopyDataParameters->CopyLength.QuadPart;
                    pCompRoutine->CompletionRoutine = IoStackLocation->CompletionRoutine;
                    pCompRoutine->Context = IoStackLocation->Context;
                    pCompRoutine->Control = IoStackLocation->Control;

                    IoStackLocation->CompletionRoutine = DeviceControlCompletionRoutine;
                    IoStackLocation->Context = pCompRoutine;
                    IoStackLocation->Control = SL_INVOKE_ON_SUCCESS;
                    IoStackLocation->Control |= SL_INVOKE_ON_ERROR;
                    IoStackLocation->Control |= SL_INVOKE_ON_CANCEL;
                } else {
                    // Volumes detected by enumeration needs to patch the completion routine to Filter in the completion path
                    // If driver fail to patch due to non availablity of non-paged pool, We miss IO. Marking for resync
                    QueueWorkerRoutineForSetVolumeOutOfSync(pVolumeContext, INVOLFLT_ERR_NO_GENERIC_NPAGED_POOL, STATUS_NO_MEMORY, false);            
                }
                Status = (*DispatchEntryForDevIOControl)(DeviceExtension->TargetDeviceObject, Irp) ;
            } else {
                Status = (*DispatchEntryForDevIOControl)(DeviceExtension->TargetDeviceObject, Irp);
            }
        } else {
            if ((!(pVolumeContext->ulFlags & VCF_FILTERING_STOPPED)) && ulDeviceType == IOCTL_DISK_BASE
                    && ulIoControlCode == IOCTL_DISK_COPY_DATA) {
                ReferenceVolumeContext(pVolumeContext);
                IoCopyCurrentIrpStackLocationToNext(Irp);
                IoSetCompletionRoutine(Irp, DeviceControlCompletionRoutine, pVolumeContext, TRUE, TRUE, TRUE);
                Status = IoCallDriver(DeviceExtension->TargetDeviceObject, Irp);
            } else {
                IoSkipCurrentIrpStackLocation(Irp);
                Status = IoCallDriver(DeviceExtension->TargetDeviceObject, Irp);
            }
        }
        
        break;
    }

    return Status;

} // end InMageFltDeviceControl()

WCHAR HexDigitArray[16] = {L'0', L'1', L'2', L'3', L'4', L'5', L'6', L'7', L'8', L'9', L'a', L'b', L'c', L'd', L'e', L'f'};

NTSTATUS
ConvertUlongToWchar(
    ULONG UNum,
    WCHAR *WStr,
    ULONG WStrLen
    )
{
    LONG Size = 0;
    ULONG Val = UNum;
    ULONG HexDigit = 0;

    while (true) {
        Val = Val / 16;
        Size++;
        if (Val == 0)
            break;
    }
    
    ASSERT(Val == 0);
    ASSERT(Size >= 1);
    ASSERT(WStrLen > (ULONG)Size);

    if (((ULONG)Size >= WStrLen) || (Size < 1)) {
        if (WStrLen >= 1) {
            WStr[0] = L'\0';
        }
        return STATUS_UNSUCCESSFUL;
    }

    WStr[Size] = L'\0';
    Size--;
    
    while (Size >= 0) {
        HexDigit = UNum % 16;
        UNum = UNum / 16;
        WStr[Size] = HexDigitArray[HexDigit];
        Size--;
    }
    
    ASSERT(UNum == 0);

    return STATUS_SUCCESS;
}

NTSTATUS
ConvertUlonglongToWchar(
    ULONGLONG UNum,
    WCHAR *WStr,
    ULONG WStrLen
    )
{
    LONG Size = 0;
    ULONGLONG Val = UNum;
    ULONG HexDigit = 0;

    while (true) {
        Val = Val / 16;
        Size++;
        if (Val == 0)
            break;        
    }
    
    ASSERT(Val == 0);
    ASSERT(Size >= 1);
    ASSERT(WStrLen > (ULONG)Size);

    if (((ULONG)Size >= WStrLen) || (Size < 1)) {
        if (WStrLen >= 1) {
            WStr[0] = L'\0';
        }
        return STATUS_UNSUCCESSFUL;
    }    

    WStr[Size] = L'\0';
    Size--;
    
    while (Size >= 0) {
        HexDigit = (ULONG) (UNum % 16);
        UNum = UNum / 16;
        WStr[Size] = HexDigitArray[HexDigit];
        Size--;
    }
    
    ASSERT(UNum == 0);

    return STATUS_SUCCESS;
}

/*
Description:   This routine logs errors with Event logs

Args			:    DeviceObject - The Device object which encapsulates the volume context or inmage filter CDO
                     UniqueId       - __LINE__ macro used as unique id
                     Status          -  The Status of the error

Return Value:    None
*/

VOID
InMageFltLogError(
    IN PDEVICE_OBJECT deviceObject,
    IN ULONG uniqueId,
    IN NTSTATUS messageId,
    IN NTSTATUS finalStatus,
    IN PCWSTR string1, /* = NULL */
    IN ULONG maxLength1, /* = 0 */ // in bytes
    IN PCWSTR string2, /* = NULL */
    IN ULONG maxLength2, /* = 0 */ // in bytes
    IN PCWSTR string3, /* = NULL */
    IN ULONG maxLength3, /* = 0 */
    IN PCWSTR string4, /* = NULL */
    IN ULONG maxLength4,  /* = 0 */
    IN PCWSTR string5, /* = NULL */
    IN ULONG maxLength5,  /* = 0 */
    IN PCWSTR string6, /* = NULL */
    IN ULONG maxLength6  /* = 0 */
    )
{
    PIO_ERROR_LOG_PACKET errorLogEntry = NULL;
    LONG totalLength = sizeof(IO_ERROR_LOG_PACKET);
    WCHAR *p = NULL;
    ULONG nbrStrings = 0;
    LARGE_INTEGER currentSystemTime;
    // Technically ERROR_LOG_MAXIMUM_SIZE should be the maximum usable
    // but on win2k3 if maximum is used the last four bytes are not displayed
    // in insertion strings. So reducing maximum size by 8 bytes.
    const unsigned long ulMaxLogEntrySize = ERROR_LOG_MAXIMUM_SIZE - 0x8;
    const unsigned long ulArraySize = 6;
    ULONG   ulMaxLengthArr[ulArraySize] = {maxLength1, maxLength2, maxLength3, maxLength4, maxLength5, maxLength6};
    ULONG   ulLengthArr[ulArraySize] = {0};
    PCWSTR  pStringArr[ulArraySize] = {string1, string2, string3, string4, string5, string6};

    for (int i = ulArraySize - 1; i >= 0; --i) {
        if (NULL != pStringArr[i]) {
            if ((totalLength + (2*sizeof(WCHAR))) <= ulMaxLogEntrySize) {
                nbrStrings++;
                unsigned long ulSize = 0;
                while ((pStringArr[i][ulSize / 2] != 0) && (ulSize < ulMaxLengthArr[i])) {
                    ulSize += sizeof(WCHAR);
                }

                if ((totalLength + ulSize + sizeof(WCHAR)) > ulMaxLogEntrySize) {
                    ulSize = ulMaxLogEntrySize - totalLength - sizeof(WCHAR);
                    ulSize = (ulSize & ~0x01);
                }
                ulLengthArr[i] = ulSize;
                totalLength += ulSize + sizeof(WCHAR);
            } else {
                pStringArr[i] = NULL;
            }
        }
    }

    // if too many event mesages are happening, write a message saying we are discarding them for a while
    KeQuerySystemTime(&currentSystemTime);
    if ((DriverContext.EventLogTimePeriod.QuadPart + EVENTLOG_SQUELCH_TIME_INTERVAL) > currentSystemTime.QuadPart) {
        // we are withing the the same time interval

        if (DriverContext.NbrRecentEventLogMessages > EVENTLOG_SQUELCH_MAXIMUM_EVENTS) {
            // throw away this event message
            return;
        } else if (DriverContext.NbrRecentEventLogMessages == EVENTLOG_SQUELCH_MAXIMUM_EVENTS) {
            // write a message saying we are throwing away messages
            totalLength = sizeof(IO_ERROR_LOG_PACKET);
            pStringArr[0] = pStringArr[1] = pStringArr[2] = pStringArr[3] = pStringArr[4] = pStringArr[5] = NULL;
            nbrStrings = 0;
            messageId = INVOLFLT_ERR_TOO_MANY_EVENT_LOG_EVENTS;
            uniqueId = 0;
            finalStatus = 0;
            DriverContext.NbrRecentEventLogMessages++;
        } else {
            // process the log message normally
            DriverContext.NbrRecentEventLogMessages++;
        }
    } else {
        DriverContext.NbrRecentEventLogMessages = 1;
        DriverContext.EventLogTimePeriod.QuadPart = currentSystemTime.QuadPart;
    }

    errorLogEntry = (PIO_ERROR_LOG_PACKET) IoAllocateErrorLogEntry(
                                            deviceObject, // or driver object
                                            (UCHAR)totalLength);

    if (errorLogEntry) {
        errorLogEntry->NumberOfStrings = (USHORT)nbrStrings;
        errorLogEntry->StringOffset = (USHORT)FIELD_OFFSET(IO_ERROR_LOG_PACKET, DumpData[0]);
        errorLogEntry->ErrorCode = messageId;
        errorLogEntry->DumpDataSize = 0;
        errorLogEntry->SequenceNumber = 0;
        errorLogEntry->MajorFunctionCode = 0;
        errorLogEntry->IoControlCode = 0;
        errorLogEntry->RetryCount = 0;
        errorLogEntry->UniqueErrorValue = uniqueId;
        errorLogEntry->FinalStatus =  finalStatus;

        // now copy the string to the end of the log entry
        p = (WCHAR *)(&(errorLogEntry->DumpData[0])); // WCHAR ptr on end of event entry

        for (unsigned long i = 0; i < ulArraySize; i++) {
            unsigned long ulIndex = 0;
            if (NULL != pStringArr[i]) {
                while (ulIndex < ulLengthArr[i]) {
                    *p++ = pStringArr[i][ulIndex / 2];
                    ulIndex += sizeof(WCHAR);
                }
                *p++ = 0; // terminator;
            }
        }

        // verify we didn't write past our allocated memory
        ASSERT((((char *)p) - ((char *)errorLogEntry)) <= totalLength);

        IoWriteErrorLogEntry(errorLogEntry);
    }
}

VOID
InMageFltLogError(
    IN PDEVICE_OBJECT deviceObject,
    IN ULONG uniqueId,
    IN NTSTATUS messageId,
    IN NTSTATUS finalStatus,
    IN PCWSTR string1,
    IN ULONG maxLength1,
    IN PCWSTR string2,
    IN ULONG maxLength2,
    IN PCWSTR string3,
    IN ULONG maxLength3,
    IN ULONG InsertionValue4
    )
{
    NTSTATUS        Status;
    WCHAR           BufferForInsertionValue4[20];

    if (KeGetCurrentIrql() <= PASSIVE_LEVEL) {
        Status = RtlStringCbPrintfExW(BufferForInsertionValue4, sizeof(BufferForInsertionValue4), NULL, NULL,
                                     STRSAFE_NULL_ON_FAILURE, L"%#x", InsertionValue4);
        ASSERT(STATUS_SUCCESS == Status);
    } else {
        Status = ConvertUlongToWchar(InsertionValue4, BufferForInsertionValue4, 20);
        ASSERT(STATUS_SUCCESS == Status);
    }

    InMageFltLogError(deviceObject, uniqueId, messageId, finalStatus, string1, maxLength1, string2, maxLength2, 
                      string3, maxLength3, BufferForInsertionValue4, sizeof(BufferForInsertionValue4));

    return;
}

VOID
InMageFltLogError(
    IN PDEVICE_OBJECT deviceObject,
    IN ULONG uniqueId,
    IN NTSTATUS messageId,
    IN NTSTATUS finalStatus,
    IN PCWSTR string1,
    IN ULONG maxLength1,
    IN PCWSTR string2,
    IN ULONG maxLength2,
    IN ULONG InsertionValue3
    )
{
    WCHAR           BufferForInsertionValue3[20];
    NTSTATUS        Status;

    if (KeGetCurrentIrql() <= PASSIVE_LEVEL) {
        Status = RtlStringCbPrintfExW(BufferForInsertionValue3, sizeof(BufferForInsertionValue3), NULL, NULL,
                                     STRSAFE_NULL_ON_FAILURE, L"%#x", InsertionValue3);
        ASSERT(STATUS_SUCCESS == Status);
    } else {
        Status = ConvertUlongToWchar(InsertionValue3, BufferForInsertionValue3, 20);
        ASSERT(STATUS_SUCCESS == Status);
    }

    InMageFltLogError(deviceObject, uniqueId, messageId, finalStatus,
        string1, maxLength1, string2, maxLength2, BufferForInsertionValue3, sizeof(BufferForInsertionValue3));

    return;
}

VOID
InMageFltLogError(
    IN PDEVICE_OBJECT deviceObject,
    IN ULONG uniqueId,
    IN NTSTATUS messageId,
    IN NTSTATUS finalStatus,
    IN PCWSTR string1,
    IN ULONG maxLength1,
    IN PCWSTR string2,
    IN ULONG maxLength2,
    IN ULONG InsertionValue3,
    IN ULONG InsertionValue4
    )
{
    NTSTATUS        Status;
    WCHAR           BufferForInsertionValue3[20];
    WCHAR           BufferForInsertionValue4[20];

    if (KeGetCurrentIrql() <= PASSIVE_LEVEL) {
        Status = RtlStringCbPrintfExW(BufferForInsertionValue3, sizeof(BufferForInsertionValue3), NULL, NULL,
                                     STRSAFE_NULL_ON_FAILURE, L"%#x", InsertionValue3);
        ASSERT(STATUS_SUCCESS == Status);
        Status = RtlStringCbPrintfExW(BufferForInsertionValue4, sizeof(BufferForInsertionValue4), NULL, NULL,
                                     STRSAFE_NULL_ON_FAILURE, L"%#x", InsertionValue4);
        ASSERT(STATUS_SUCCESS == Status);
    } else {
        Status = ConvertUlongToWchar(InsertionValue3, BufferForInsertionValue3, 20);
        ASSERT(STATUS_SUCCESS == Status);

        Status = ConvertUlongToWchar(InsertionValue4, BufferForInsertionValue4, 20);
        ASSERT(STATUS_SUCCESS == Status);
    }

    InMageFltLogError(deviceObject, uniqueId, messageId, finalStatus,
        string1, maxLength1, string2, maxLength2,
                      BufferForInsertionValue3, sizeof(BufferForInsertionValue3), BufferForInsertionValue4, sizeof(BufferForInsertionValue4));

    return;
}


VOID
InMageFltLogError(
    IN PDEVICE_OBJECT deviceObject,
    IN ULONG uniqueId,
    IN NTSTATUS messageId,
    IN NTSTATUS finalStatus,
    IN PCWSTR string1,
    IN ULONG maxLength1,
    IN PCWSTR string2,
    IN ULONG maxLength2,
    IN ULONGLONG InsertionValue3,
    IN ULONGLONG InsertionValue4
    )
{
    NTSTATUS        Status;
    WCHAR           BufferForInsertionValue3[20];
    WCHAR           BufferForInsertionValue4[20];

    if (KeGetCurrentIrql() <= PASSIVE_LEVEL) {
        Status = RtlStringCbPrintfExW(BufferForInsertionValue3, sizeof(BufferForInsertionValue3), NULL, NULL,
                                     STRSAFE_NULL_ON_FAILURE, L"%I64x", InsertionValue3);
        ASSERT(STATUS_SUCCESS == Status);
        Status = RtlStringCbPrintfExW(BufferForInsertionValue4, sizeof(BufferForInsertionValue4), NULL, NULL,
                                     STRSAFE_NULL_ON_FAILURE, L"%I64x", InsertionValue4);
        ASSERT(STATUS_SUCCESS == Status);
    } else {
        Status = ConvertUlonglongToWchar(InsertionValue3, BufferForInsertionValue3, 20);
        ASSERT(STATUS_SUCCESS == Status); 

        Status = ConvertUlonglongToWchar(InsertionValue4, BufferForInsertionValue4, 20);
        ASSERT(STATUS_SUCCESS == Status); 
    }

    InMageFltLogError(deviceObject, uniqueId, messageId, finalStatus,
        string1, maxLength1, string2, maxLength2,
                      BufferForInsertionValue3, sizeof(BufferForInsertionValue3), BufferForInsertionValue4, sizeof(BufferForInsertionValue4));

    return;
}

VOID
InMageFltLogError(
    IN PDEVICE_OBJECT deviceObject,
    IN ULONG uniqueId,
    IN NTSTATUS messageId,
    IN NTSTATUS finalStatus,
    IN PCWSTR string1,
    IN ULONG maxLength1,
    IN ULONGLONG InsertionValue3,
    IN ULONGLONG InsertionValue4,
    IN ULONGLONG InsertionValue5,
    IN ULONGLONG InsertionValue6
    )
{
    NTSTATUS        Status;
    WCHAR           BufferForInsertionValue3[20];
    WCHAR           BufferForInsertionValue4[20];
    WCHAR           BufferForInsertionValue5[20];
    WCHAR           BufferForInsertionValue6[20];

    Status = ConvertUlonglongToWchar(InsertionValue3, BufferForInsertionValue3, 20);
    ASSERT(STATUS_SUCCESS == Status);    

    Status = ConvertUlonglongToWchar(InsertionValue4, BufferForInsertionValue4, 20);
    ASSERT(STATUS_SUCCESS == Status); 

    Status = ConvertUlonglongToWchar(InsertionValue5, BufferForInsertionValue5, 20);
    ASSERT(STATUS_SUCCESS == Status); 

    Status = ConvertUlonglongToWchar(InsertionValue6, BufferForInsertionValue6, 20);
    ASSERT(STATUS_SUCCESS == Status); 
    
    InMageFltLogError(deviceObject, uniqueId, messageId, finalStatus,
        string1, maxLength1,
                      BufferForInsertionValue3, sizeof(BufferForInsertionValue3),
                      BufferForInsertionValue4, sizeof(BufferForInsertionValue4),
                      BufferForInsertionValue5, sizeof(BufferForInsertionValue5),
                      BufferForInsertionValue6, sizeof(BufferForInsertionValue6));

    return;
}

VOID
InMageFltLogError(
    IN PDEVICE_OBJECT deviceObject,
    IN ULONG uniqueId,
    IN NTSTATUS messageId,
    IN NTSTATUS finalStatus,
    IN PCWSTR string1,
    IN ULONG maxLength1,
    IN ULONGLONG InsertionValue3,
    IN ULONGLONG InsertionValue4,
    IN ULONGLONG InsertionValue5
    )
{
    NTSTATUS        Status;
    WCHAR           BufferForInsertionValue3[20];
    WCHAR           BufferForInsertionValue4[20];
    WCHAR           BufferForInsertionValue5[20];

    Status = ConvertUlonglongToWchar(InsertionValue3, BufferForInsertionValue3, 20);
    ASSERT(STATUS_SUCCESS == Status);    

    Status = ConvertUlonglongToWchar(InsertionValue4, BufferForInsertionValue4, 20);
    ASSERT(STATUS_SUCCESS == Status); 

    Status = ConvertUlonglongToWchar(InsertionValue5, BufferForInsertionValue5, 20);
    ASSERT(STATUS_SUCCESS == Status); 
        
    InMageFltLogError(deviceObject, uniqueId, messageId, finalStatus,
        string1, maxLength1,
                      BufferForInsertionValue3, sizeof(BufferForInsertionValue3),
                      BufferForInsertionValue4, sizeof(BufferForInsertionValue4),
                      BufferForInsertionValue5, sizeof(BufferForInsertionValue5));

    return;
}


VOID
InMageFltLogError(
    IN PDEVICE_OBJECT deviceObject,
    IN ULONG uniqueId,
    IN NTSTATUS messageId,
    IN NTSTATUS finalStatus,
    IN ULONGLONG InsertionValue1,
    IN ULONGLONG InsertionValue2,
    IN ULONG InsertionValue3,
    IN ULONG InsertionValue4
    )
{
    NTSTATUS        Status;
    WCHAR           BufferForInsertionValue1[20];
    WCHAR           BufferForInsertionValue2[20];
    WCHAR           BufferForInsertionValue3[20];
    WCHAR           BufferForInsertionValue4[20];

    Status = ConvertUlonglongToWchar(InsertionValue1, BufferForInsertionValue1, 20);
    ASSERT(STATUS_SUCCESS == Status);
    
    Status = ConvertUlonglongToWchar(InsertionValue2, BufferForInsertionValue2, 20);
    ASSERT(STATUS_SUCCESS == Status);

    Status = ConvertUlongToWchar(InsertionValue3, BufferForInsertionValue3, 20);
    ASSERT(STATUS_SUCCESS == Status);    

    Status = ConvertUlongToWchar(InsertionValue4, BufferForInsertionValue4, 20);
    ASSERT(STATUS_SUCCESS == Status); 

    InMageFltLogError(deviceObject, uniqueId, messageId, finalStatus,
                        BufferForInsertionValue1, sizeof(BufferForInsertionValue1),
                        BufferForInsertionValue2, sizeof(BufferForInsertionValue2),
                        BufferForInsertionValue3, sizeof(BufferForInsertionValue3),
                        BufferForInsertionValue4, sizeof(BufferForInsertionValue4));

    return;
}

VOID
InMageFltLogError(
    IN PDEVICE_OBJECT deviceObject,
    IN ULONG uniqueId,
    IN NTSTATUS messageId,
    IN NTSTATUS finalStatus,
    IN ULONG InsertionValue1,
    IN ULONG InsertionValue2,
    IN ULONG InsertionValue3,
    IN ULONG InsertionValue4
    )
{
    NTSTATUS        Status;
    WCHAR           BufferForInsertionValue1[20];
    WCHAR           BufferForInsertionValue2[20];
    WCHAR           BufferForInsertionValue3[20];
    WCHAR           BufferForInsertionValue4[20];

    Status = ConvertUlongToWchar(InsertionValue1, BufferForInsertionValue1, 20);
    ASSERT(STATUS_SUCCESS == Status);
    
    Status = ConvertUlongToWchar(InsertionValue2, BufferForInsertionValue2, 20);
    ASSERT(STATUS_SUCCESS == Status);

    Status = ConvertUlongToWchar(InsertionValue3, BufferForInsertionValue3, 20);
    ASSERT(STATUS_SUCCESS == Status);    

    Status = ConvertUlongToWchar(InsertionValue4, BufferForInsertionValue4, 20);
    ASSERT(STATUS_SUCCESS == Status);    

    InMageFltLogError(deviceObject, uniqueId, messageId, finalStatus,
                        BufferForInsertionValue1, sizeof(BufferForInsertionValue1),
                        BufferForInsertionValue2, sizeof(BufferForInsertionValue2),
                        BufferForInsertionValue3, sizeof(BufferForInsertionValue3),
                        BufferForInsertionValue4, sizeof(BufferForInsertionValue4));

    return;
}

VOID
InMageFltLogError(
    IN PDEVICE_OBJECT deviceObject,
    IN ULONG uniqueId,
    IN NTSTATUS messageId,
    IN NTSTATUS finalStatus,
    IN ULONGLONG InsertionValue1,
    IN ULONG InsertionValue2,
    IN ULONGLONG InsertionValue3,
    IN ULONG InsertionValue4
    )
{
    NTSTATUS        Status;
    WCHAR           BufferForInsertionValue1[20];
    WCHAR           BufferForInsertionValue2[20];
    WCHAR           BufferForInsertionValue3[20];
    WCHAR           BufferForInsertionValue4[20];

    Status = ConvertUlonglongToWchar(InsertionValue1, BufferForInsertionValue1, 20);
    ASSERT(STATUS_SUCCESS == Status);
    
    Status = ConvertUlongToWchar(InsertionValue2, BufferForInsertionValue2, 20);
    ASSERT(STATUS_SUCCESS == Status);

    Status = ConvertUlonglongToWchar(InsertionValue3, BufferForInsertionValue3, 20);
    ASSERT(STATUS_SUCCESS == Status);
    
    Status = ConvertUlongToWchar(InsertionValue4, BufferForInsertionValue4, 20);
    ASSERT(STATUS_SUCCESS == Status);

    InMageFltLogError(deviceObject, uniqueId, messageId, finalStatus,
                        BufferForInsertionValue1, sizeof(BufferForInsertionValue1),
                        BufferForInsertionValue2, sizeof(BufferForInsertionValue2),
                        BufferForInsertionValue3, sizeof(BufferForInsertionValue3),
                        BufferForInsertionValue4, sizeof(BufferForInsertionValue4));

    return;
}


NTSTATUS
InMageFltShutdown(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    PDEVICE_EXTENSION   pDeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PVOLUME_CONTEXT     pVolumeContext = NULL;
    LIST_ENTRY          ListHead;
    BOOLEAN             bWaitForWritesToBeCompleted = FALSE;
    KIRQL               OldIrql;

    InVolDbgPrint(DL_INFO, DM_SHUTDOWN, 
        ("InMageFltShutdown: DeviceObject %#p Irp %#p\n", DeviceObject, Irp));
    
    if ((DeviceObject == DriverContext.ControlDevice) || (pDeviceExtension->shutdownSeen)) {
        if (DeviceObject == DriverContext.ControlDevice) {
            InVolDbgPrint(DL_INFO, DM_SHUTDOWN, 
                ("InMageFltShutdown: Shutdown IRP received on control device\n"));
            // as we may be about to shutdown, prepare low level code for physical I/O
            pVolumeContext = (PVOLUME_CONTEXT)DriverContext.HeadForVolumeContext.Flink;
            while (pVolumeContext != (PVOLUME_CONTEXT)(&DriverContext.HeadForVolumeContext)) {
                // try to save all data now, while we are in early shutdown
                if (!(pVolumeContext->ulFlags & VCF_FILTERING_STOPPED)) {
                    if (IS_CLUSTER_VOLUME(pVolumeContext)) {
                        // For cluster volumes, bitmap is saved when the disk is released.
                        // bitmap file is not closed or delted as Win2K sees some writes after release
                        // and we do all the attempt to save that changes to bitmap file.
                        // by the time shutdown has reached, file system is unmounted 
                        // and volume is taken offline.
#if (NTDDI_VERSION >= NTDDI_VISTA)

                        
                        if((0 == (pVolumeContext->ulFlags & VCF_CV_FS_UNMOUTNED))||
                            ((pVolumeContext->ulFlags & VCF_CV_FS_UNMOUTNED) && (pVolumeContext->ulFlags & VCF_CLUSTER_VOLUME_ONLINE))){
                              InMageFltSaveAllChanges(pVolumeContext, TRUE, NULL);
                              if(pVolumeContext->pVolumeBitmap && pVolumeContext->pVolumeBitmap->pBitmapAPI) {
                                  BitmapPersistentValues BitmapPersistentValue(pVolumeContext);
                                  pVolumeContext->pVolumeBitmap->pBitmapAPI->ChangeBitmapModeToRawIO();
                                  pVolumeContext->pVolumeBitmap->pBitmapAPI->CommitBitmap(BitmapPersistentValue, NULL);
                                
                            }
                        }
#endif
                        InVolDbgPrint(DL_INFO, DM_SHUTDOWN, 
                            ("InMageFltShutdown: Cluster volume, pVolumeContext = %#p, Drive = %S\n", pVolumeContext, pVolumeContext->wDriveLetter));

                        //ASSERT((0 == pVolumeContext->ChangeList.ulTotalChangesPending) &&
                        //    (0 == pVolumeContext->ChangeList.ullcbTotalChangesPending) &&
                        //        (0 == pVolumeContext->ulChangesPendingCommit));

                        if (pVolumeContext->ChangeList.ulTotalChangesPending) {
                            InVolDbgPrint( DL_ERROR, DM_SHUTDOWN | DM_CLUSTER, 
                                ("Volume %S DiskSig = 0x%x has ChangesPending = 0x%x ChangesPendingCommit = 0x%x\n",
                                        pVolumeContext->wDriveLetter, pVolumeContext->ulDiskSignature, 
                                        pVolumeContext->ChangeList.ulTotalChangesPending, pVolumeContext->ulChangesPendingCommit));
                        }
                    } else {
                        // This volume is not a cluster volume.
                        InVolDbgPrint(DL_INFO, DM_SHUTDOWN, 
                            ("InMageFltShutdown: pVolumeContext = %#p, Flushing final volume changes for Drive %S\n",
                            pVolumeContext, pVolumeContext->wDriveLetter));
                        InMageFltSaveAllChanges(pVolumeContext, TRUE, NULL);
                        // Persist the Last TimeStamp/Sequence Number/Continuation Id for non cluster Volume
                        PersistVContextFields(pVolumeContext);
                        if (pVolumeContext->pVolumeBitmap && pVolumeContext->pVolumeBitmap->pBitmapAPI) {
                            pVolumeContext->pVolumeBitmap->pBitmapAPI->ChangeBitmapModeToRawIO();
                        }
                    }
                }
                pVolumeContext = (PVOLUME_CONTEXT)(pVolumeContext->ListEntry.Flink);
            }

            //Added for Persistent Time Stamp
            if (NULL != DriverContext.pTimeSeqFlushThread) {
                KeSetEvent(&DriverContext.PersistentThreadShutdownEvent, IO_DISK_INCREMENT, FALSE);
                KeWaitForSingleObject(DriverContext.pTimeSeqFlushThread, Executive, KernelMode, FALSE, NULL);
                ObDereferenceObject(DriverContext.pTimeSeqFlushThread);
                DriverContext.pTimeSeqFlushThread = NULL;
            }

            Irp->IoStatus.Status = STATUS_SUCCESS;
            Irp->IoStatus.Information = 0;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return STATUS_SUCCESS;
        } else {
            // it must be a non-first call to shutdown for a normal device
            //
            // Set current stack back one.
            //
                IoSkipCurrentIrpStackLocation(Irp);

                return IoCallDriver(pDeviceExtension->TargetDeviceObject, Irp);
        }
    }
    pDeviceExtension->shutdownSeen = TRUE; // prevent shutdown processing twice

    // it looks like we can't write ANYTHING to a file after we get the shutdown IRP

    pVolumeContext = pDeviceExtension->pVolumeContext;


    if ((NULL != pVolumeContext) && (!(pVolumeContext->ulFlags & VCF_FILTERING_STOPPED))) {
        if (IS_CLUSTER_VOLUME(pVolumeContext)) {
            // For cluster volumes, bitmap is saved when the disk is released.
            // bitmap file is not closed or delted as Win2K sees some writes after release
            // and we do all the attempt to save that changes to bitmap file.
            // by the time shutdown has reached, file system is unmounted 
            // and volume is taken offline.

            InVolDbgPrint(DL_INFO, DM_SHUTDOWN, 
                ("InMageFltShutdown: Cluster volume, pVolumeContext = %#p, Drive = %S\n", pVolumeContext, pVolumeContext->wDriveLetter));

            ASSERT((0 == pVolumeContext->ChangeList.ulTotalChangesPending) && (0 == pVolumeContext->ChangeList.ullcbTotalChangesPending));

            if (pVolumeContext->ChangeList.ulTotalChangesPending) {
                InVolDbgPrint( DL_ERROR, DM_SHUTDOWN | DM_CLUSTER, 
                    ("Volume %S DiskSig = 0x%x has ChangesPending = 0x%x ChangesPendingCommit = 0x%x\n",
                            pVolumeContext->wDriveLetter, pVolumeContext->ulDiskSignature, 
                            pVolumeContext->ChangeList.ulTotalChangesPending, pVolumeContext->ulChangesPendingCommit));
            }
        } else {
            // This volume is not a cluster volume.
            InVolDbgPrint(DL_INFO, DM_SHUTDOWN,
                ("InMageFltShutdown: Volume = %S ulTotalChangesPending = %#x eServiceState = %i\n",   
                pVolumeContext->wDriveLetter, pVolumeContext->ChangeList.ulTotalChangesPending,
                DriverContext.eServiceState));

            if (pVolumeContext->pVolumeBitmap) {
                InVolDbgPrint(DL_INFO, DM_SHUTDOWN, 
                    ("InMageFltShutdown: pVolumeContext = %#p, Flushing final volume changes for Drive %S\n",
                    pVolumeContext, pVolumeContext->wDriveLetter));
                FlushAndCloseBitmapFile(pVolumeContext);
            }
        }
    }

    //
    // Set current stack back one.
    //
                IoSkipCurrentIrpStackLocation(Irp);

                return IoCallDriver(pDeviceExtension->TargetDeviceObject, Irp);
                
} // end InMageFltShutdown()



/*
Description:  This routine is called as part of file system flush

Args			:   DeviceObject - Encapsulates the filter driver's respective volume context
					Irp                - The i/o request meant for lower level drivers, usually.

Return Value:   NT Status
*/

NTSTATUS
InMageFltFlush(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    PDEVICE_EXTENSION  pDeviceExtension = NULL;//(PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PVOLUME_CONTEXT   pVolumeContext = NULL;        PDRIVER_OBJECT      DrvObj = NULL;
    PDRIVER_EXTENSION   DrvExt = NULL;
    PWCHAR              ServiceName = NULL;
    PDRIVER_DISPATCH     DispatchEntryForFlush = NULL;


    InVolDbgPrint(DL_FUNC_TRACE, DM_FLUSH,
        ("InMageFltFlush: DeviceObject %#p Irp %#p\n", DeviceObject, Irp));

    if(DriverContext.IsDispatchEntryPatched) {
        pVolumeContext = GetVolumeContextForThisIOCTL(DeviceObject, NULL);
        if(pVolumeContext) {       
            DeviceObject = pVolumeContext->FilterDeviceObject;
            pDeviceExtension = (PDEVICE_EXTENSION)pVolumeContext->FilterDeviceObject->DeviceExtension;
            if(pVolumeContext->IsVolumeCluster) {
                DispatchEntryForFlush = DriverContext.MajorFunctionForClusDrives[IRP_MJ_FLUSH_BUFFERS];
            } else {
                DispatchEntryForFlush = DriverContext.MajorFunction[IRP_MJ_FLUSH_BUFFERS];
            }
            
            if(pVolumeContext->IsVolumeUsingAddDevice)
                return (*DispatchEntryForFlush)(pDeviceExtension->TargetDeviceObject, Irp);

        } else {
            if(DeviceObject->DriverObject == DriverContext.DriverObject) {
                
                pDeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
                pVolumeContext = pDeviceExtension->pVolumeContext;

                // Bug Fix: 14969: system stops responding with NoReboot Driver
                if (pVolumeContext->IsVolumeUsingAddDevice != true) {
                    IoSkipCurrentIrpStackLocation(Irp);
                    return IoCallDriver(pDeviceExtension->TargetDeviceObject, Irp);
                }

                if(pVolumeContext){
                    if(pVolumeContext->IsVolumeCluster) {
                        DispatchEntryForFlush = DriverContext.MajorFunctionForClusDrives[IRP_MJ_FLUSH_BUFFERS];
                    } else {
                        DispatchEntryForFlush = DriverContext.MajorFunction[IRP_MJ_FLUSH_BUFFERS];
                    }
                }
            } else if (DriverContext.pDriverObject == DeviceObject->DriverObject ) {
                return (*DriverContext.MajorFunction[IRP_MJ_FLUSH_BUFFERS])(DeviceObject, Irp);                    
            } else
                return (*DriverContext.MajorFunctionForClusDrives[IRP_MJ_FLUSH_BUFFERS])(DeviceObject, Irp);   
        }
    } else {
        pDeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    }

    if (DeviceObject == DriverContext.ControlDevice) {
        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }


    pVolumeContext = pDeviceExtension->pVolumeContext;

    if (NULL != pVolumeContext) {
        KIRQL       OldIrql;
        KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
        pVolumeContext->ulFlushCount++;
        KeQuerySystemTime(&pVolumeContext->liLastFlushTime);
        KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);

        InVolDbgPrint(DL_INFO, DM_FLUSH,
            ("InMageFltFlush: Volume = %S(%S) FlushCount = %#x, ulChangesPending %#x ulChangesPendingCommit %#x\n",
            pVolumeContext->wGUID, pVolumeContext->wDriveLetter, pVolumeContext->ulFlushCount, 
             pVolumeContext->ChangeList.ulTotalChangesPending, pVolumeContext->ulChangesPendingCommit));

        if ((!(pVolumeContext->ulFlags & VCF_FILTERING_STOPPED)) &&
            (ecServiceShutdown == DriverContext.eServiceState))
        {
            InMageFltSaveAllChanges(pVolumeContext, FALSE, NULL);
        }
    }

    //
    // Set current stack back one.
    //
    if(!pVolumeContext->IsVolumeUsingAddDevice && DriverContext.IsDispatchEntryPatched) {
        return (*DispatchEntryForFlush)(pDeviceExtension->TargetDeviceObject, Irp);
    } else {
        IoSkipCurrentIrpStackLocation(Irp);
    
        return IoCallDriver(pDeviceExtension->TargetDeviceObject, Irp);
    }


} // end InMageFltFlush()


#define FILTER_DEVICE_PROPOGATE_FLAGS            0
#define FILTER_DEVICE_PROPOGATE_CHARACTERISTICS (FILE_REMOVABLE_MEDIA |  \
                                                 FILE_READ_ONLY_DEVICE | \
                                                 FILE_FLOPPY_DISKETTE    \
                                                 )

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
    propFlags = TargetDevice->Flags & FILTER_DEVICE_PROPOGATE_FLAGS;
    FilterDevice->Flags |= propFlags;

    propFlags = TargetDevice->Characteristics & FILTER_DEVICE_PROPOGATE_CHARACTERISTICS;
    FilterDevice->Characteristics |= propFlags;


}

/*
Description: Creates and initializes a new filter device object FiDO for the corresponding PDO. Then it attaches the device object to the device
                  stack of the drivers for the device.
				  
Args			: DriverObject - involflt filter driver's Driver Object
                  PhysicalDeviceObject - Physical Device Object from the underlying layered driver

Return Value:  NTSTATUS
*/

NTSTATUS
InMageFltAddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PhysicalDeviceObject
    )
{
    NTSTATUS            Status;
    PDEVICE_OBJECT      FilterDeviceObject;
    PDEVICE_EXTENSION   pDeviceExtension;
    PVOLUME_CONTEXT     pVolumeContext = NULL;
    KIRQL               OldIrql;

    //
    // Create a filter device object for this device (partition).
    //

    InVolDbgPrint(DL_FUNC_TRACE, DM_VOLUME_ARRIVAL,
        ("InMageFltAddDevice: Driver = %#p  PDO = %#p\n", DriverObject, PhysicalDeviceObject));


    pVolumeContext = AllocateVolumeContext();
    if (NULL == pVolumeContext)
        return STATUS_INSUFFICIENT_RESOURCES;

    Status = IoCreateDevice(DriverObject,
                            DEVICE_EXTENSION_SIZE,
                            NULL,
                            FILE_DEVICE_DISK,
                            0,
                            FALSE,
                            &FilterDeviceObject);

    if (!NT_SUCCESS(Status)) {
       InVolDbgPrint(DL_ERROR, DM_VOLUME_ARRIVAL,
           ("InMageFltAddDevice: IoCreateDevice failed with Status %#x - Cannot create filterDeviceObject\n",
           Status));
       DereferenceVolumeContext(pVolumeContext);
       return Status;
    }

    FilterDeviceObject->Flags |= DO_DIRECT_IO;

    pDeviceExtension = (PDEVICE_EXTENSION) FilterDeviceObject->DeviceExtension;
    RtlZeroMemory(pDeviceExtension, DEVICE_EXTENSION_SIZE);
    //
    // Attaches the device object to the highest device object in the chain and
    // return the previously highest device object, which is passed to
    // IoCallDriver when pass IRPs down the device stack
    //
    pDeviceExtension->TargetDeviceObject =
        IoAttachDeviceToDeviceStack(FilterDeviceObject, PhysicalDeviceObject);

    if (pDeviceExtension->TargetDeviceObject == NULL) {
        IoDeleteDevice(FilterDeviceObject);
        DereferenceVolumeContext(pVolumeContext);
        InVolDbgPrint(DL_ERROR, DM_VOLUME_ARRIVAL,
            ("InMageFltAddDevice: IoAttachDeviceToDeviceStack failed - Unable to attach %#p to target %#p\n",
            FilterDeviceObject, PhysicalDeviceObject));
        return STATUS_NO_SUCH_DEVICE;
    }

    ReferenceVolumeContext(pVolumeContext);
    pDeviceExtension->pVolumeContext = pVolumeContext;
    pDeviceExtension->PhysicalDeviceObject = PhysicalDeviceObject;
    pDeviceExtension->DeviceObject = FilterDeviceObject;
    pVolumeContext->FilterDeviceObject = FilterDeviceObject;
    pVolumeContext->TargetDeviceObject = pDeviceExtension->TargetDeviceObject;
	KeQuerySystemTime(&pVolumeContext->liVolumeContextCreationTS);
    InVolDbgPrint(DL_INFO, DM_VOLUME_ARRIVAL, 
        ("InMageFltAddDevice: Successfully added PhysicalDO = %#p, FilterDO = %#p, LowerDO = %#p, VC = %#p\n",
        PhysicalDeviceObject, FilterDeviceObject, pDeviceExtension->TargetDeviceObject, pVolumeContext));
    
    if(DriverContext.IsVolumeAddedByEnumeration) {
        if(DriverContext.IsClusterVolume)
           pVolumeContext->IsVolumeCluster = true;
        GetDeviceNumberAndSignature(pVolumeContext);
        GetClusterInfoForThisVolume(pVolumeContext);
        if (!IS_CLUSTER_VOLUME(pVolumeContext))
            pVolumeContext->bQueueChangesToTempQueue = false;
        GetVolumeGuidAndDriveLetter(pVolumeContext);
        LoadVolumeCfgFromRegistry(pVolumeContext);
    } else 
        pVolumeContext->IsVolumeUsingAddDevice = true;

    KeInitializeEvent(&pDeviceExtension->PagingPathCountEvent,
                      SynchronizationEvent, TRUE);

    //Fix for Bug 28568
    INITIALIZE_PNP_STATE(pDeviceExtension);
    IoInitializeRemoveLock(&pDeviceExtension->RemoveLock,TAG_REMOVE_LOCK,0,0);
	

	//Fix For Bug 27337
	RtlZeroMemory(&pVolumeContext->FsInfoWrapper.FsInfo, sizeof(FSINFORMATION));
	pVolumeContext->FsInfoWrapper.FsInfoFetchRetries = 0;
	pVolumeContext->FsInfoWrapper.FsInfoNtStatus = STATUS_UNSUCCESSFUL;

    // Remove VolumeID from DataBlockList
    KeAcquireSpinLock(&DriverContext.DataBlockListLock, &OldIrql);
    PLIST_ENTRY Entry = DriverContext.OrphanedMappedDataBlockList.Flink;

    while (Entry != &DriverContext.OrphanedMappedDataBlockList) {
        PDATA_BLOCK DataBlock = (PDATA_BLOCK)Entry;

        if (DataBlock->VolumeId == (HANDLE)pVolumeContext) {
            DataBlock->VolumeId = 0;
        }
        Entry = Entry->Flink;
    }
    KeReleaseSpinLock(&DriverContext.DataBlockListLock, OldIrql);

    // Add Volume Context to DriverContext list.
    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    ReferenceVolumeContext(pVolumeContext);
//  Bug 25726. Adding New volume context at head of the list 
    InsertHeadList(&DriverContext.HeadForVolumeContext, &pVolumeContext->ListEntry);    
    DriverContext.ulNumVolumes++;
    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

    //
    // default to DO_POWER_PAGABLE
    //

    FilterDeviceObject->Flags |=  DO_POWER_PAGABLE;

    //
    // Clear the DO_DEVICE_INITIALIZING flag
    //

    FilterDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    // Free the reference of pVolumeContext
    DereferenceVolumeContext(pVolumeContext);

    return STATUS_SUCCESS;

} // end InMageFltAddDevice()


/*
Routine			: This routine is called when a Pnp Start Irp is received.

Args				:  DeviceObject, which encapsulates the volume context
                       Irp, I/O request packet sent by IoManager

Return Value	:  NTSTATUS
*/
NTSTATUS
InMageFltStartDevice(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    PDEVICE_EXTENSION   pDeviceExtension;
    KEVENT              Event;
    NTSTATUS            Status;
    PVOLUME_CONTEXT     pVolumeContext;
    
    PIRP                            tempIrp;
    STORAGE_DEVICE_NUMBER           StDevNum;
    IO_STATUS_BLOCK                 IoStatus;
    


    pDeviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    pVolumeContext = pDeviceExtension->pVolumeContext;

    Status = InMageFltForwardIrpSynchronous(DeviceObject, Irp);

    InMageFltSyncFilterWithTarget(DeviceObject,
                                 pDeviceExtension->TargetDeviceObject);

#if (NTDDI_VERSION >= NTDDI_WS03) 
    do
    {
        //Fix for Bug 28568 and similar.- For better readability.
        NTSTATUS            ntStatus;

        KeInitializeEvent(&Event, NotificationEvent, FALSE);
        
        tempIrp = IoBuildDeviceIoControlRequest(IOCTL_STORAGE_GET_DEVICE_NUMBER, pDeviceExtension->TargetDeviceObject, NULL, 0, 
                &StDevNum, sizeof(STORAGE_DEVICE_NUMBER), FALSE, &Event, &IoStatus);
        
        if(!tempIrp)
            break;

        ntStatus = IoCallDriver(pDeviceExtension->TargetDeviceObject, tempIrp);
        if (ntStatus == STATUS_PENDING) {
            KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
            ntStatus = IoStatus.Status;
        }


       if(NT_SUCCESS(ntStatus)) {
           pVolumeContext->ulDiskNumber = StDevNum.DeviceNumber;
  
       }
    }while(false);

    
    InVolDbgPrint(DL_INFO, DM_DRIVER_PNP | DM_VOLUME_ARRIVAL,
        ("InMageFltStartDevice: Disk Number is %x\n",pVolumeContext->ulDiskNumber));

#endif

    //
    // Complete the Irp
    //
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return Status;
}

NTSTATUS
InMageFltSurpriseRemoveDevice(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    PDEVICE_EXTENSION DeviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    PVOLUME_CONTEXT VolumeContext = DeviceExtension->pVolumeContext;
    
    KIRQL           OldIrql;
    PVOLUME_BITMAP  VolumeBitmap = NULL;

    ASSERT(NULL != VolumeContext);

    InVolDbgPrint(DL_INFO, DM_SHUTDOWN,
        ("InMageFltSurpriseRemoveDevice: DeviceObject %#p Irp %#p\n",
        DeviceObject, Irp));

    VolumeContext->ulFlags |= VCF_DEVICE_SURPRISE_REMOVAL;

	//Fix for Bug 27670
	if ( (IS_CLUSTER_VOLUME(VolumeContext)) && (VolumeContext->PnPNotificationEntry)) {
        IoUnregisterPlugPlayNotification(VolumeContext->PnPNotificationEntry);
        VolumeContext->PnPNotificationEntry = NULL;
    }
	
	if (VolumeContext->LogVolumeFileObject) {
        ObDereferenceObject(VolumeContext->LogVolumeFileObject);
        VolumeContext->LogVolumeFileObject = NULL;
        VolumeContext->LogVolumeDeviceObject = NULL;
     }

    InVolDbgPrint(DL_INFO, DM_CLUSTER, ("InMageFltSurpriseRemoveDevice: VolumeContext is %p, DriveLetter=%S VolumeBitmap is %p VolumeFlags is 0x%x\n",
        VolumeContext, VolumeContext->wDriveLetter, VolumeContext->pVolumeBitmap , VolumeContext->ulFlags));

    InMageFltSaveAllChanges(VolumeContext, TRUE, NULL);
    if (VolumeContext->pVolumeBitmap && VolumeContext->pVolumeBitmap->pBitmapAPI) {
        BitmapPersistentValues BitmapPersistentValue(VolumeContext);
        VolumeContext->pVolumeBitmap->pBitmapAPI->CommitBitmap(BitmapPersistentValue, NULL);
        VolumeContext->pVolumeBitmap->pBitmapAPI->ChangeBitmapModeToRawIO();
    }

    return InMageFltSendToNextDriver(DeviceObject, Irp);
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
    IN PIRP Irp
    )
{
    NTSTATUS            Status;
    PDEVICE_EXTENSION   pDeviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    KIRQL               OldIrql;
    PVOLUME_CONTEXT     pVolumeContext;
    PTAG_NODE           pHead = NULL;
    LIST_ENTRY          *pCurr = NULL;
    

    InVolDbgPrint(DL_FUNC_TRACE, DM_SHUTDOWN,
        ("InMageFltRemoveDevice: DeviceObject %#p Irp %#p\n",
        DeviceObject, Irp));

    pVolumeContext = pDeviceExtension->pVolumeContext;

	//Fix for Bug 27670
	if ( (IS_CLUSTER_VOLUME(pVolumeContext)) && (pVolumeContext->PnPNotificationEntry)) {
        IoUnregisterPlugPlayNotification(pVolumeContext->PnPNotificationEntry);
        pVolumeContext->PnPNotificationEntry = NULL;
    }
	
	if (pVolumeContext->LogVolumeFileObject) {
        ObDereferenceObject(pVolumeContext->LogVolumeFileObject);
        pVolumeContext->LogVolumeFileObject = NULL;
        pVolumeContext->LogVolumeDeviceObject = NULL;
     }
	
    KeAcquireSpinLock(&DriverContext.TagListLock, &OldIrql);
    pCurr = DriverContext.HeadForTagNodes.Flink;
    while (pCurr != &DriverContext.HeadForTagNodes) {
        bool    bRemoveFromList = false;

        pHead = (PTAG_NODE)pCurr;
        pCurr = pCurr->Flink;

        if (NULL == pHead->pTagInfo) {
            // Tag is already applied.
            continue;
        }

        for (ULONG i = 0; i < pHead->ulNoOfVolumes; i++) {
            if (pHead->pTagInfo->TagStatus[i].VolumeContext && (pHead->pTagInfo->TagStatus[i].VolumeContext == pVolumeContext)) {
                bRemoveFromList = true;
                break;
            }
        }

        if (bRemoveFromList) {
            for (ULONG i = 0; i < pHead->ulNoOfVolumes; i++) {
                if (pHead->pTagInfo->TagStatus[i].VolumeContext ) {
                DereferenceVolumeContext(pHead->pTagInfo->TagStatus[i].VolumeContext);
            }
        }

            RemoveEntryList((PLIST_ENTRY)pHead);
            pHead->Irp->IoStatus.Information = 0;
            pHead->Irp->IoStatus.Status = STATUS_DEVICE_REMOVED; 
            IoCompleteRequest(pHead->Irp, IO_NO_INCREMENT);

            if(pHead->pTagInfo){
                ExFreePoolWithTag(pHead->pTagInfo, TAG_GENERIC_NON_PAGED);
                pHead->pTagInfo = NULL;    
            }
            DeallocateTagNode(pHead);
        }
    }
      
    KeReleaseSpinLock(&DriverContext.TagListLock, OldIrql);


    if (pVolumeContext)
    {
        PBASIC_VOLUME BasicVolume = NULL;

        InVolDbgPrint(DL_INFO, DM_SHUTDOWN,
            ("InMageFltRemoveDevice: Removing Device - pVolumeContext = %#p Volume = %S(%S)\n", 
            pVolumeContext, pVolumeContext->wGUID, pVolumeContext->wDriveLetter));

        KeWaitForSingleObject(&pVolumeContext->Mutex, Executive, KernelMode, FALSE, NULL);
        if (NULL != pVolumeContext->pBasicVolume && pVolumeContext->pBasicVolume->pVolumeContext) {
            ASSERT(pVolumeContext == pVolumeContext->pBasicVolume->pVolumeContext);
            BasicVolume = pVolumeContext->pBasicVolume;
            pVolumeContext->pBasicVolume = NULL;
        }

        if ( IS_CLUSTER_VOLUME(pVolumeContext) &&
             pVolumeContext->pVolumeBitmap && 
             pVolumeContext->pVolumeBitmap->pBitmapAPI ) 
        {
            // Reset physical disk as the stack is being removed.
            pVolumeContext->pVolumeBitmap->pBitmapAPI->ResetPhysicalDO();
        }
        KeReleaseMutex(&pVolumeContext->Mutex, FALSE);

        if (NULL != BasicVolume) {
            DereferenceBasicVolume(BasicVolume);    //Volume Context Reference
            DereferenceBasicVolume(BasicVolume);    //Delete Basic Volume.
        }

        // this Debugprint has to run outside the spinlock as the Unicode string table is paged
        InVolDbgPrint(DL_INFO, DM_SHUTDOWN,
            ("InMageFltRemoveDevice: Volume = %S ulTotalChangesPending = %i\n", 
            pVolumeContext->wDriveLetter, pVolumeContext->ChangeList.ulTotalChangesPending));

        ReferenceVolumeContext(pVolumeContext);
        // Acquire SyncEvent so that user mode process are not processing getdbtrans
        KeWaitForSingleObject(&pVolumeContext->SyncEvent, Executive, KernelMode, FALSE, NULL);
        KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
        KeAcquireSpinLockAtDpcLevel(&pVolumeContext->Lock);

        OrphanMappedDataBlocksAndMoveCommitPendingDirtyBlockToMainQueue(pVolumeContext);

        pVolumeContext->eDSDBCdeviceState = ecDSDBCdeviceOffline;
        pVolumeContext->FilterDeviceObject = NULL;

        DereferenceVolumeContext(pDeviceExtension->pVolumeContext);
        pDeviceExtension->pVolumeContext = NULL;

        if (pVolumeContext->ulFlags & VCF_GUID_OBTAINED) {
            // Trigger activity to start writing existing data to bit map file.
            KeSetEvent(&DriverContext.ServiceThreadWakeEvent, IO_DISK_INCREMENT, FALSE);
        } else {
            // GUID for this volume is not obtained. So the volume is unmounted even before mountdev links are created.
            ASSERT(NULL == pVolumeContext->pVolumeBitmap);
            RemoveEntryList((PLIST_ENTRY)pVolumeContext);
            DriverContext.ulNumVolumes--;
            InitializeListHead((PLIST_ENTRY)pVolumeContext);
            pVolumeContext->ulFlags |= VCF_REMOVED_FROM_LIST; // Helps in debugging to know if it is removed from list.
            DereferenceVolumeContext(pVolumeContext);
        }

        KeReleaseSpinLockFromDpcLevel(&pVolumeContext->Lock);
        KeReleaseSpinLock(&DriverContext.Lock, OldIrql);
        KeSetEvent(&pVolumeContext->SyncEvent, IO_NO_INCREMENT, FALSE);

        DereferenceVolumeContext(pVolumeContext);
        pVolumeContext = NULL;
    }

    // TODO: Should we wait for writes to complete, before removing the device??

    Status = InMageFltForwardIrpSynchronous(DeviceObject, Irp);

	//Fix for Bug 28568 and similar.
	IoReleaseRemoveLockAndWait(&pDeviceExtension->RemoveLock,Irp);

    PDEVICE_OBJECT TargetDevObj = pDeviceExtension->TargetDeviceObject;
    pDeviceExtension->TargetDeviceObject = NULL;
    IoDetachDevice(TargetDevObj);
    IoDeleteDevice(DeviceObject);

    //
    // Complete the Irp
    //
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

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
    IN PIRP Irp
    )
{
    PIO_STACK_LOCATION  currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS            Status;
    PDEVICE_EXTENSION   pDeviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION  topIrpStack;


    InVolDbgPrint(DL_FUNC_TRACE, DM_DRIVER_PNP | DM_VOLUME_ARRIVAL | DM_SHUTDOWN ,
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

    switch(currentIrpStack->MinorFunction) {

        case IRP_MN_START_DEVICE:
            // Get the respective disk number to faciliate the disk-volume mapping
           
            InVolDbgPrint(DL_INFO, DM_DRIVER_PNP | DM_VOLUME_ARRIVAL,
                ("InMageFltDispatchPnp: pVolumeContext = %#p, Minor Function - START_DEVICE\n",
                pDeviceExtension->pVolumeContext));
            Status = InMageFltStartDevice(DeviceObject, Irp);

            if (NT_SUCCESS(Status)){
              SET_NEW_PNP_STATE(pDeviceExtension,Started);
            }
            
            break;

        case IRP_MN_QUERY_STOP_DEVICE:
            InVolDbgPrint(DL_INFO, DM_DRIVER_PNP | DM_SHUTDOWN,
                ("InMageFltDispatchPnp: pVolumeContext = %#p - Query shutdown, flushing diry blocks\n", pDeviceExtension->pVolumeContext));

            if ((pDeviceExtension->pVolumeContext) && (!(pDeviceExtension->pVolumeContext->ulFlags & VCF_FILTERING_STOPPED))) {
                InVolDbgPrint(DL_INFO, DM_DRIVER_PNP | DM_SHUTDOWN,
                    ("InMageFltDispatchPnp: Saving changes for volume %S ulTotalChangesPending = %#x ulChangesPendingCommit = %#x\n",
                    pDeviceExtension->pVolumeContext->wDriveLetter, pDeviceExtension->pVolumeContext->ChangeList.ulTotalChangesPending,
                    pDeviceExtension->pVolumeContext->ulChangesPendingCommit));

                InMageFltSaveAllChanges(pDeviceExtension->pVolumeContext, TRUE, NULL);
            }

            Status = InMageFltSendToNextDriver(DeviceObject, Irp);
			SET_NEW_PNP_STATE(pDeviceExtension,StopPending);
            break;

        case IRP_MN_SURPRISE_REMOVAL:

            //Note: Once this call has come to us, as per MS documentation we may/may not
  	    	//get IRP_MN_REMOVE_DEVICE call. If any handle remains open to
		    //the device object then PNP does not send the IRP_MN_REMOVE_DEVICE
		    //thus onus lies on us to take care of any corner cases.
            InVolDbgPrint(DL_INFO, DM_DRIVER_PNP | DM_SHUTDOWN,
                ("InMageFltDispatchPnp: pVolumeContext = %#p SURPRISE_REMOVAL\n",
                pDeviceExtension->pVolumeContext));
			SET_NEW_PNP_STATE(pDeviceExtension, SurpriseRemovePending);
			Irp->IoStatus.Status = STATUS_SUCCESS;//harmless, only for w2k, seems like there was a bug in pcmcia.sys
            Status = InMageFltSurpriseRemoveDevice(DeviceObject, Irp);
			
            break;
			
		case IRP_MN_CANCEL_STOP_DEVICE:
		
		    if (StopPending == pDeviceExtension->DevicePnPState)
		    { 
			   RESTORE_PREVIOUS_PNP_STATE(pDeviceExtension);
		    }
			
            Status = InMageFltSendToNextDriver(DeviceObject, Irp); 
			break;
			
		case IRP_MN_STOP_DEVICE:
		
		    SET_NEW_PNP_STATE(pDeviceExtension, Stopped);
            Status = InMageFltSendToNextDriver(DeviceObject, Irp); 
			break;
			
		case IRP_MN_QUERY_REMOVE_DEVICE:
		
		    SET_NEW_PNP_STATE(pDeviceExtension, RemovePending);
            Status = InMageFltSendToNextDriver(DeviceObject, Irp); 
			break;

        case IRP_MN_REMOVE_DEVICE:
            
            // PnP Manager will not send MN_REMOVE_DEVICE to a devnode till
            // a file object exists on it.
            InVolDbgPrint(DL_INFO, DM_DRIVER_PNP | DM_SHUTDOWN,
                ("InMageFltDispatchPnp: pVolumeContext = %#p Minor Function - REMOVE_DEVICE\n",
                pDeviceExtension->pVolumeContext));
			SET_NEW_PNP_STATE(pDeviceExtension,Deleted);
			Irp->IoStatus.Status = STATUS_SUCCESS;//only for w2k, seems like there was a bug in pcmcia.sys
            Status = InMageFltRemoveDevice(DeviceObject, Irp);
            return Status;
			
		case IRP_MN_CANCEL_REMOVE_DEVICE:
		
		    if(RemovePending == pDeviceExtension->DevicePnPState)
            {
              RESTORE_PREVIOUS_PNP_STATE(pDeviceExtension);
            } 
			
            Status = InMageFltSendToNextDriver(DeviceObject, Irp); 
			break;
			
        case IRP_MN_DEVICE_USAGE_NOTIFICATION:
        {
            PIO_STACK_LOCATION currentIrpStack;
            ULONG count;
            BOOLEAN pagable;
            UNICODE_STRING pagefileName;

            InVolDbgPrint(DL_INFO, DM_DRIVER_PNP, 
                ("InMageFltDispatchPnp: Minor Function DEVICE_USAGE_NOTIFICATION\n"));

            currentIrpStack = IoGetCurrentIrpStackLocation(Irp);

            if (currentIrpStack->Parameters.UsageNotification.Type != DeviceUsageTypePaging) {
                Status = InMageFltSendToNextDriver(DeviceObject, Irp);
                break; // out of case statement
            }

            InVolDbgPrint(DL_INFO, DM_DRIVER_PNP | DM_PAGEFILE, 
                ("InMageFltDispatchPnp: Processing DeviceUsageTypePaging\n"));
            RtlInitUnicodeString(&pagefileName, L"\\pagefile.sys");
            pDeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;

            // Wait for paging path count event here
            Status = KeWaitForSingleObject(&pDeviceExtension->PagingPathCountEvent,
                                           Executive, KernelMode,
                                           FALSE, NULL);

            // if removing last paging device, need to set DO_POWER_PAGABLE
            // bit here, and possible re-set it below on failure.

            // are we removing the paging file that we ignore
            if (!currentIrpStack->Parameters.UsageNotification.InPath) {
                // walk down the irp and find the originating file, which is the paging file
                topIrpStack = (PIO_STACK_LOCATION)((PCHAR)Irp + sizeof(IRP)) + (Irp->StackCount - 1);

                if (pDeviceExtension->pagingFile == topIrpStack->FileObject) {
                    InVolDbgPrint(DL_INFO, DM_DRIVER_PNP | DM_PAGEFILE,
                        ("InMageFltDispatchPnp: Stopped ignoring paging file object %#p\n",
                        pDeviceExtension->pagingFile));
                    pDeviceExtension->pagingFile = NULL; // remove the monitoring
                }
            }

            pagable = FALSE;
            if (!currentIrpStack->Parameters.UsageNotification.InPath &&
                pDeviceExtension->PagingPathCount == 1 ) {

                // removing the last paging file, must have DO_POWER_PAGABLE bits set

                if (DeviceObject->Flags & DO_POWER_INRUSH) {
                    InVolDbgPrint(DL_INFO, DM_PAGEFILE | DM_DRIVER_PNP,
                        ("InMageFltDispatchPnp: last paging file removed but DO_POWER_INRUSH set, so not setting PAGABLE bit for DO %#p\n",
                        DeviceObject));
                } else {
                    InVolDbgPrint(DL_INFO, DM_PAGEFILE | DM_DRIVER_PNP,
                        ("InMageFltDispatchPnp: Setting  PAGABLE bit for DO = %p\n", 
                        DeviceObject));
                    DeviceObject->Flags |= DO_POWER_PAGABLE;
                    pagable = TRUE;
                }
            }

            // Forward the IRP synchronously and set the pagingfile appropriately

            Status = InMageFltForwardIrpSynchronous(DeviceObject, Irp);

            if (NT_SUCCESS(Status)) {

                IoAdjustPagingPathCount(
                    &pDeviceExtension->PagingPathCount,
                    currentIrpStack->Parameters.UsageNotification.InPath);

                if (currentIrpStack->Parameters.UsageNotification.InPath) {
                    if (pDeviceExtension->PagingPathCount == 1) { // This is the first paging file for this volume

                        InVolDbgPrint(DL_INFO, DM_DRIVER_PNP | DM_PAGEFILE,
                            ("InMageFltDispatchPnp: Clear Pagable bit for DeviceObject = %p\n", 
                            DeviceObject));
                        DeviceObject->Flags &= ~DO_POWER_PAGABLE;

                        // Walk up the top of the IRP stack, see if this is paging file
                        topIrpStack = (PIO_STACK_LOCATION)((PCHAR)Irp + sizeof(IRP)) + (Irp->StackCount - 1);

                        if (topIrpStack->FileObject->FileName.Length  > 0) {
                            if (0 == RtlCompareUnicodeString(&(topIrpStack->FileObject->FileName), &pagefileName, TRUE)) {
                                pDeviceExtension->pagingFile = topIrpStack->FileObject;
                                InVolDbgPrint(DL_INFO, DM_DRIVER_PNP | DM_PAGEFILE,
                                    ("InMageFltDispatchPnp: Pagingfile is set %#p\n",
                                    pDeviceExtension->pagingFile));
                            }
                        }
                    }
                }

            } else {

                if (pagable == TRUE) {
                    DeviceObject->Flags &= ~DO_POWER_PAGABLE;
                    pagable = FALSE;
                }
            }

           // set the event so the next one can occur.

            KeSetEvent(&pDeviceExtension->PagingPathCountEvent,
                       IO_NO_INCREMENT, FALSE);

           // About to complete the IRP

            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            break;

        }

        default:
            InVolDbgPrint(DL_INFO, DM_DRIVER_PNP, ("InMageFltDispatchPnp: Forwarding irp\n"));

           Status = InMageFltSendToNextDriver(DeviceObject, Irp);

    }

	IoReleaseRemoveLock(&pDeviceExtension->RemoveLock,Irp);
    return Status;

}

/*
Description: Free all the volume contexts, CDO, Bitmap related resources and Data file related resources

Arguments: DriverObject - pointer to the involflt driver object.

Return Value: None
*/
VOID
InMageFltUnload(
    IN PDRIVER_OBJECT DriverObject
    )
{
    PDEVICE_EXTENSION       pDeviceExtension;

    InVolDbgPrint(DL_INFO, DM_DRIVER_UNLOAD,
        ("InMageFltUnload: DriverObject %#p\n", DriverObject));

    CleanDriverContext();

    if (NULL != DriverContext.pServiceThread) {
        KeSetEvent(&DriverContext.ServiceThreadShutdownEvent, IO_DISK_INCREMENT, FALSE);
        KeWaitForSingleObject(DriverContext.pServiceThread, Executive, KernelMode, FALSE, NULL);
        ObDereferenceObject(DriverContext.pServiceThread);
        DriverContext.pServiceThread = NULL;
    }

    BitmapAPI::TerminateBitmapAPI();
    CDataFileWriter::CleanLAL();
    return;
}

NTSTATUS
ValidateParameters(SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_INPUT Input,PSET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_OUTPUT pOutputBuffer)
{
     NTSTATUS Status = STATUS_SUCCESS;
     if(Input.ulDefaultMinFreeDataToDiskSizeLimit) {
         if(DriverContext.ulDataToDiskLimitPerVolumeInMB < Input.ulDefaultMinFreeDataToDiskSizeLimit) {
             pOutputBuffer->ulErrorInSettingDefault = STATUS_INVALID_PARAMETER;
             Status = STATUS_INVALID_PARAMETER;
         }
     }
      
     if (Input.ulMinFreeDataToDiskSizeLimitForAllVolumes) {
         LIST_ENTRY  VolumeNodeList;
         Status = GetListOfVolumes(&VolumeNodeList);
         if (STATUS_SUCCESS == Status) {
             while(!IsListEmpty(&VolumeNodeList)) {
                 PLIST_NODE  pNode = (PLIST_NODE)RemoveHeadList(&VolumeNodeList);;
                 PVOLUME_CONTEXT VolumeContext = (PVOLUME_CONTEXT)pNode->pData;
                 DereferenceListNode(pNode);
                 if (VolumeContext->ulFlags & VCF_GUID_OBTAINED) {
                     Status = KeWaitForSingleObject(&VolumeContext->Mutex, Executive, KernelMode, FALSE, NULL);
                     ASSERT(STATUS_SUCCESS == Status);
                     if(VolumeContext->ullcbDataToDiskLimit < (((ULONGLONG)Input.ulMinFreeDataToDiskSizeLimitForAllVolumes) * MEGABYTES)){
                         pOutputBuffer->ulErrorInSettingForAllVolumes = STATUS_INVALID_PARAMETER;
                         Status = STATUS_INVALID_PARAMETER;
                     }
                     KeReleaseMutex(&VolumeContext->Mutex, FALSE);
                 }
                 DereferenceVolumeContext(VolumeContext);
             } // for each volume
         }
     }

     if (Input.ulFlags & SET_MIN_FREE_DATA_TO_DISK_SIZE_LIMIT_FLAGS_VALID_GUID) {
         PVOLUME_CONTEXT     VolumeContext = NULL;
         VolumeContext = GetVolumeContextWithGUID(&Input.VolumeGUID[0]);
         if (VolumeContext){
             Status = KeWaitForSingleObject(&VolumeContext->Mutex, Executive, KernelMode, FALSE, NULL);
             ASSERT(STATUS_SUCCESS == Status);
             if(VolumeContext->ullcbDataToDiskLimit < (Input.ulMinFreeDataToDiskSizeLimit * MEGABYTES)) {
                 pOutputBuffer->ulErrorInSettingForVolume = STATUS_INVALID_PARAMETER;
                 Status = STATUS_INVALID_PARAMETER;
             }
             KeReleaseMutex(&VolumeContext->Mutex, FALSE);
         }
     }
return Status;
}

/*This Function is added as per Persistent Time Stamp
 *changes. This code is invoked every 1 second to update the Time stamp and 
 *Sequence number.
*/
VOID
PersistentValuesFlush(
    IN PVOID Context
    )
{
    LARGE_INTEGER     TimeOut, RegTimeOut;;
    NTSTATUS          Status;
    Registry          ParametersKey;
    ULONG             FailedCount = 1;

    TimeOut.QuadPart = (-1) * (PERSISTENT_FLUSH_TIME_OUT) * (HUNDRED_NANO_SECS_IN_SEC);
    RegTimeOut.QuadPart = (-1) * (6) * (HUNDRED_NANO_SECS_IN_SEC);

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
                    InMageFltLogError(DriverContext.ControlDevice, __LINE__, INVOLFLT_ERR_PERSISTENT_THREAD_OPEN_REGISTRY_FAILED, Status);
                    break;
                }
            } else {
                InMageFltLogError(DriverContext.ControlDevice, __LINE__, INVOLFLT_ERR_PERSISTENT_THREAD_OPEN_REGISTRY_FAILED, Status);
                break;
            }
        }
    } while (STATUS_SUCCESS != Status);

    if (NT_SUCCESS(Status)) {
        do {
            Status = KeWaitForSingleObject((PVOID)&DriverContext.PersistentThreadShutdownEvent, 
                                           Executive,
                                           KernelMode,
                                           FALSE, 
                                           &TimeOut);
            if (!NT_SUCCESS(Status)) {
                ++FailedCount;
                if (FailedCount >= 10 ) {
                    InMageFltLogError(DriverContext.ControlDevice, __LINE__, INVOLFLT_ERR_PERSISTENT_THREAD_WAIT_FAILED, Status);
                    break;
                }
            } else {
                FailedCount = 1;
            }

            //Here STATUS_TIMEOUT means that there is a timeout of 1 sec
            //And STATUS_SUCCESS means that we recieved the PersistentThreadShutdownEvent
            if ((STATUS_TIMEOUT == Status) || (STATUS_SUCCESS == Status)) {
                bool     FlushNeeded = false;
                NTSTATUS          LocalStatus;
                KIRQL             OldIrql;
                ULONGLONG         TimeStamp;
                LIST_ENTRY        VolumeNodeList;
                ULONGLONG         SequenceNo;

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
                            InMageFltLogError(DriverContext.ControlDevice, __LINE__, INVOLFLT_PERSISTENT_THREAD_REGISTRY_WRITE_FAILURE,
                                              STATUS_SUCCESS, LocalStatus);
							InVolDbgPrint(DL_ERROR, DM_REGISTRY, 
								("%s: %ws could not be set, Status: %x\n", __FUNCTION__, LAST_PERSISTENT_VALUES, LocalStatus));
						}
                    } else {
                        FlushNeeded = true;
                        InVolDbgPrint(DL_TRACE_L3, DM_REGISTRY, 
                            ("%s: Added %ws with Time = %#I64x\n", __FUNCTION__, LAST_PERSISTENT_VALUES,TimeStamp));

                        InVolDbgPrint(DL_TRACE_L3, DM_REGISTRY, 
                            ("  And Seq = %#I64x\n",SequenceNo));
                    }

                    //The Below code Update the volume bitmap with the persisting values.
                    //This is done only in case of Time Out of the thread.
                    //In case of shutdown or the failover the individual volume Bitmap are updated in the different code.
                    if (STATUS_TIMEOUT == Status) {
                        LocalStatus = GetListOfVolumes(&VolumeNodeList);
                        if (STATUS_SUCCESS == LocalStatus) {
                            while(!IsListEmpty(&VolumeNodeList)) {
                                PLIST_NODE  pNode = (PLIST_NODE)RemoveHeadList(&VolumeNodeList);;
                                PVOLUME_CONTEXT pVolumeContext = (PVOLUME_CONTEXT)pNode->pData;
                                DereferenceListNode(pNode);
    
                                if (IS_CLUSTER_VOLUME(pVolumeContext) && (pVolumeContext->ulFlags & VCF_CLUSTER_VOLUME_ONLINE)) {
                                    PVOLUME_BITMAP pVolumeBitmap = NULL;
                                    KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
                                    if (pVolumeContext->pVolumeBitmap)
                                        pVolumeBitmap = ReferenceVolumeBitmap(pVolumeContext->pVolumeBitmap);
                                    KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);
                                    if (NULL != pVolumeBitmap) {
                                        ExAcquireFastMutex(&pVolumeBitmap->Mutex);
                                        if (pVolumeBitmap->pBitmapAPI)
                                            pVolumeBitmap->pBitmapAPI->FlushTimeAndSequence(TimeStamp,
                                                                                            SequenceNo,
                                                                                            true);
                                        ExReleaseFastMutex(&pVolumeBitmap->Mutex);
                                        DereferenceVolumeBitmap(pVolumeBitmap);
                                    }
                                }
    
                                DereferenceVolumeContext(pVolumeContext);
                            } // for each volume
                        }
                    }

                }

                //Flush the Registry Info to the disk
                if (FlushNeeded) {
                    LocalStatus = ParametersKey.FlushKey();
                    if (!NT_SUCCESS(LocalStatus)) {
                        InMageFltLogError(DriverContext.ControlDevice, __LINE__, INVOLFLT_PERSISTENT_THREAD_REGISTRY_FLUSH_FAILURE,
                                          STATUS_SUCCESS, LocalStatus);
                        InVolDbgPrint(DL_ERROR, DM_REGISTRY, 
                            ("%s: Flushing the Registry Info to Disk Failed\n", __FUNCTION__));
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


VOID GetDeviceNumberAndSignature(PVOLUME_CONTEXT pVolumeContext)
{
    
   
    KEVENT                          Event;
    NTSTATUS                        Status;
    PIRP                            tempIrp;
    IO_STATUS_BLOCK                 IoStatus;

    KeInitializeEvent(&Event, NotificationEvent, FALSE);

#if (NTDDI_VERSION >= NTDDI_VISTA)

    PDISK_GEOMETRY_EX               DiskGeometry = NULL;
    do {
        DiskGeometry = (PDISK_GEOMETRY_EX)ExAllocatePoolWithTag(PagedPool, 
                                                             sizeof(DISK_GEOMETRY_EX) + sizeof(DISK_PARTITION_INFO ) + sizeof(DISK_DETECTION_INFO),                                                                 TAG_GENERIC_NON_PAGED);
        if(DiskGeometry ) {

            PDISK_PARTITION_INFO DiskParInfo = NULL;

            tempIrp = IoBuildDeviceIoControlRequest(IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, pVolumeContext->TargetDeviceObject, NULL, 0, 
                     DiskGeometry, sizeof(DISK_GEOMETRY_EX) + sizeof(DISK_PARTITION_INFO ) + sizeof(DISK_DETECTION_INFO), FALSE, &Event, &IoStatus);

            if(!tempIrp)
              break;

            Status = IoCallDriver(pVolumeContext->TargetDeviceObject, tempIrp);
            if (Status == STATUS_PENDING) {
              KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
              Status = IoStatus.Status;
            }

            if(NT_SUCCESS(Status)) {
                DiskParInfo = (PDISK_PARTITION_INFO)DiskGeometry->Data;

                if(PARTITION_STYLE_MBR == DiskParInfo->PartitionStyle) {
                 pVolumeContext->ulDiskSignature = DiskParInfo->Mbr.Signature;
                 pVolumeContext->PartitionStyle = ecPartStyleMBR;
                } else if (PARTITION_STYLE_GPT == DiskParInfo->PartitionStyle) {
                  RtlCopyMemory(&pVolumeContext->DiskId, &DiskParInfo->Gpt.DiskId, sizeof(GUID));       //gpt disk
                  pVolumeContext->PartitionStyle = ecPartStyleGPT;
                }
            }
        } else {
            InVolDbgPrint(DL_ERROR, DM_CLUSTER, ("GetDeviceNumberAndSignature: Failed to allocate paged memory\n\n"));
        }
     } while(false);
     if(DiskGeometry)
         ExFreePoolWithTag(DiskGeometry, TAG_GENERIC_NON_PAGED);
    //get the signature or guid
#else
    STORAGE_DEVICE_NUMBER           StDevNum;
    do{
        tempIrp = IoBuildDeviceIoControlRequest(IOCTL_STORAGE_GET_DEVICE_NUMBER, pVolumeContext->TargetDeviceObject, NULL, 0, 
                &StDevNum, sizeof(STORAGE_DEVICE_NUMBER), FALSE, &Event, &IoStatus);

        if(!tempIrp)
            break;

        Status = IoCallDriver(pVolumeContext->TargetDeviceObject, tempIrp);
        if (Status == STATUS_PENDING) {
            KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
            Status = IoStatus.Status;
        }

        if(NT_SUCCESS(Status)) {
            pVolumeContext->ulDiskNumber = StDevNum.DeviceNumber;
        }
    }while(false);
    //get the disk number
#endif
}


VOID GetVolumeGuidAndDriveLetter(PVOLUME_CONTEXT pVolumeContext)
{
    
    KEVENT                          Event;
    NTSTATUS                        Status;
    PIRP                            tempIrp;
    IO_STATUS_BLOCK                 IoStatus;
    PMOUNTDEV_NAME                  MountDevName = NULL;
    PMOUNTMGR_MOUNT_POINT           MountMgrMountPoint = NULL;
    PMOUNTMGR_MOUNT_POINTS          MountMgrMountPoints = NULL;
    
    UNICODE_STRING                  name;
    PFILE_OBJECT                    fileObject = NULL ;
    PDEVICE_OBJECT                  deviceObject = NULL;
    UNICODE_STRING                  VolumeNameWithGUID;
    WCHAR                           VolumeGUID[GUID_SIZE_IN_CHARS + 1] = {0};
	const SIZE_T                    MountDevNameLength = 0x80;
	const SIZE_T                    MountPoints = 0x400;



    KeInitializeEvent(&Event, NotificationEvent, FALSE);

    do{
        MountDevName = (PMOUNTDEV_NAME)ExAllocatePoolWithTag(PagedPool, sizeof(MOUNTDEV_NAME) + MountDevNameLength, TAG_GENERIC_PAGED);

        if(NULL == MountDevName)
            break;

        RtlZeroMemory(MountDevName, sizeof(MOUNTDEV_NAME) + MountDevNameLength);

        MountMgrMountPoint = (PMOUNTMGR_MOUNT_POINT)ExAllocatePoolWithTag(PagedPool, sizeof(MOUNTMGR_MOUNT_POINT) + MountDevNameLength, TAG_GENERIC_PAGED);
        if(!MountMgrMountPoint)
            break;
        
        
        RtlZeroMemory(MountMgrMountPoint, sizeof(MOUNTMGR_MOUNT_POINT) + MountDevNameLength);


        tempIrp = IoBuildDeviceIoControlRequest(IOCTL_MOUNTDEV_QUERY_DEVICE_NAME, pVolumeContext->TargetDeviceObject, NULL, 0, 
                    MountDevName, sizeof(MOUNTDEV_NAME)+ MountDevNameLength, FALSE, &Event, &IoStatus);
        
        if(!tempIrp)
            break;
        
        Status = IoCallDriver(pVolumeContext->TargetDeviceObject, tempIrp);
        if (Status == STATUS_PENDING) {
            KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
            Status = IoStatus.Status;
        }

        if(NT_SUCCESS(Status)) {
            MountMgrMountPoint->DeviceNameOffset = sizeof(MOUNTMGR_MOUNT_POINT);
            MountMgrMountPoint->DeviceNameLength = MountDevName->NameLength > MountDevNameLength ? MountDevNameLength : MountDevName->NameLength;

            RtlCopyMemory((char *)MountMgrMountPoint + sizeof(MOUNTMGR_MOUNT_POINT) , MountDevName->Name,\
				MountDevName->NameLength > MountDevNameLength ? MountDevNameLength : MountDevName->NameLength);

			//Fix for bug 28915
			if (INMAGE_IS_MOUNTDEV_NAME_VOLUME_GUID(MountDevName)) {
               RtlCopyMemory(VolumeGUID, &MountDevName->Name[GUID_START_INDEX_IN_MOUNTDEV_NAME], 
                        GUID_SIZE_IN_BYTES);
               ConvertGUIDtoLowerCase(VolumeGUID);
			}
        }
        else
            break;

        RtlInitUnicodeString(&name, MOUNTMGR_DEVICE_NAME);
        Status = IoGetDeviceObjectPointer(&name,
                FILE_READ_ATTRIBUTES, 
                &fileObject, &deviceObject);

        if(!NT_SUCCESS(Status)) {
            break;
        }


        
        
      //  RtlCopyMemory((char *)MountMgrMountPoint + sizeof(MOUNTMGR_MOUNT_POINT) , MountDevName->Name, MountDevName->NameLength > 0x80 ? 0x80 : MountDevName->NameLength);
            

        MountMgrMountPoints = (PMOUNTMGR_MOUNT_POINTS)ExAllocatePoolWithTag(PagedPool, MountPoints, TAG_GENERIC_PAGED);
        if(!MountMgrMountPoints)
            break;

        tempIrp = IoBuildDeviceIoControlRequest(IOCTL_MOUNTMGR_QUERY_POINTS, deviceObject, MountMgrMountPoint, sizeof(MOUNTMGR_MOUNT_POINT) + MountDevNameLength, 
                    MountMgrMountPoints, MountPoints, FALSE, &Event, &IoStatus);
        
        if(!tempIrp)
            break;
        
        Status = IoCallDriver(deviceObject, tempIrp);
        if (Status == STATUS_PENDING) {
            KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
            Status = IoStatus.Status;
        }

        if(NT_SUCCESS(Status)) {
            
            ULONG  Offset = 0, Length = 0;
            
            for(ULONG i = 0; i < MountMgrMountPoints->NumberOfMountPoints ; i++) {
                Length = MountMgrMountPoints->MountPoints[i]. SymbolicLinkNameLength;
                Offset = MountMgrMountPoints->MountPoints[i]. SymbolicLinkNameOffset;
                if(Length == 0x60){
                    if(!(pVolumeContext->ulFlags & VCF_GUID_OBTAINED)) {
                        pVolumeContext->UniqueVolumeName.Buffer = pVolumeContext->BufferForUniqueVolumeName;
                        pVolumeContext->UniqueVolumeName.MaximumLength = 100;
                        pVolumeContext->UniqueVolumeName.Length = 0x60;
                        RtlCopyMemory(pVolumeContext->UniqueVolumeName.Buffer, (char *)MountMgrMountPoints + Offset,pVolumeContext->UniqueVolumeName.Length);
                        RtlCopyMemory(pVolumeContext->wGUID, pVolumeContext->UniqueVolumeName.Buffer + 0xb, GUID_SIZE_IN_BYTES);
                        pVolumeContext->ulFlags |= VCF_GUID_OBTAINED;
                    } else {
                        if (GUID_SIZE_IN_BYTES != RtlCompareMemory(VolumeGUID, pVolumeContext->wGUID, GUID_SIZE_IN_BYTES)) {
                            PVOLUME_GUID    pVolumeGUID = NULL;
                            KIRQL           OldIrql;
                            
                            InVolDbgPrint(DL_INFO, DM_CLUSTER, ("GetVolumeGuidAndDriveLetter: Found more than one GUID for volume\n%S\n%S\n",
                                                        pVolumeContext->wGUID, VolumeGUID));
                    
                            pVolumeGUID = pVolumeContext->GUIDList;
                            while(NULL != pVolumeGUID) {
                                if (GUID_SIZE_IN_BYTES == RtlCompareMemory(VolumeGUID, pVolumeGUID->GUID, GUID_SIZE_IN_BYTES)) {
                                    InVolDbgPrint(DL_INFO, DM_CLUSTER, ("GetVolumeGuidAndDriveLetter: Found same GUID again\n"));
                                break;
                            }
                            pVolumeGUID = pVolumeGUID->NextGUID;
                        }

                        if (NULL == pVolumeGUID) {
                            pVolumeGUID = (PVOLUME_GUID)ExAllocatePoolWithTag(NonPagedPool, sizeof(VOLUME_GUID), TAG_GENERIC_NON_PAGED);
                            if (NULL != pVolumeGUID) {
                                InterlockedIncrement(&DriverContext.lAdditionalGUIDs);
                                pVolumeContext->lAdditionalGUIDs++;
                                RtlZeroMemory(pVolumeGUID, sizeof(VOLUME_GUID));
                                RtlCopyMemory(pVolumeGUID->GUID, VolumeGUID, GUID_SIZE_IN_BYTES);
                                pVolumeGUID->NextGUID = pVolumeContext->GUIDList;
                                pVolumeContext->GUIDList = pVolumeGUID;
                            } else {
                                InVolDbgPrint(DL_INFO, DM_CLUSTER, ("GetVolumeGuidAndDriveLetter: Failed to allocate VOLUMEGUID Struct\n"));
                            }
                        }

                    } else {
                        InVolDbgPrint(DL_INFO, DM_CLUSTER, ("GetVolumeGuidAndDriveLetter: Found same GUID again %S\n", VolumeGUID));
                    }

                    
                    }

                } else if(Length == 0x1c){
                    if(!(pVolumeContext->ulFlags & VCF_VOLUME_LETTER_OBTAINED)) {

                        SetBitRepresentingDriveLetter(*((char *)MountMgrMountPoints + Offset + 0x18), 
                                              &pVolumeContext->DriveLetterBitMap);
                        pVolumeContext->wDriveLetter[0] = *((char *)MountMgrMountPoints + Offset + 0x18);
                        pVolumeContext->DriveLetter[0] = (char)pVolumeContext->wDriveLetter[0];
                        pVolumeContext->ulFlags |= VCF_VOLUME_LETTER_OBTAINED;

                    }
                }
            }
        
            RtlCopyUnicodeString(&pVolumeContext->VolumeParameters, &DriverContext.DriverParameters);
            VolumeNameWithGUID.MaximumLength = VolumeNameWithGUID.Length = VOLUME_NAME_SIZE_IN_BYTES;
            VolumeNameWithGUID.Buffer = &pVolumeContext->UniqueVolumeName.Buffer[VOLUME_NAME_START_INDEX_IN_MOUNTDEV_NAME];
            RtlAppendUnicodeStringToString(&pVolumeContext->VolumeParameters, &VolumeNameWithGUID);


       }


    }while(false);

    
    if(MountDevName)
        ExFreePoolWithTag(MountDevName, TAG_GENERIC_PAGED);

    
    if(MountMgrMountPoint)
        ExFreePoolWithTag(MountMgrMountPoint, TAG_GENERIC_PAGED);

    
    if(MountMgrMountPoints)
        ExFreePoolWithTag(MountMgrMountPoints, TAG_GENERIC_PAGED);






}

VOID GetClusterInfoForThisVolume(PVOLUME_CONTEXT pVolumeContext)
{
    
    PBASIC_DISK         pBasicDisk = NULL;
    PBASIC_VOLUME       pBasicVolume = NULL;

    
#if (NTDDI_VERSION >= NTDDI_VISTA)
        
        
        if(pVolumeContext->PartitionStyle == ecPartStyleMBR) 
            pBasicDisk = GetBasicDiskFromDriverContext(pVolumeContext->ulDiskSignature);
        else
            pBasicDisk = GetBasicDiskFromDriverContext(pVolumeContext->DiskId);
if(pBasicDisk != NULL) {

        if(pVolumeContext->PartitionStyle == ecPartStyleMBR) 
            pBasicVolume = AddBasicVolumeToDriverContext(pVolumeContext->ulDiskSignature, pVolumeContext);
        else
            pBasicVolume = AddBasicVolumeToDriverContext(pVolumeContext->DiskId, pVolumeContext);
        
    
#else    
        pBasicDisk = GetBasicDiskFromDriverContext(0, pVolumeContext->ulDiskNumber);
    
    if(pBasicDisk != NULL) {

        pVolumeContext->ulDiskSignature = pBasicDisk->ulDiskSignature;
        pBasicVolume = AddBasicVolumeToDriverContext(pVolumeContext->ulDiskSignature, pVolumeContext);
        
#endif
        
        pBasicDisk->ulFlags |= BD_FLAGS_CLUSTER_ONLINE;
        pVolumeContext->ulFlags |= VCF_VOLUME_ON_BASIC_DISK;
        pVolumeContext->ulFlags |= VCF_VOLUME_ON_CLUS_DISK;
        pVolumeContext->ulFlags |= VCF_CLUSTER_VOLUME_ONLINE;
        StartFilteringDevice(pVolumeContext, false);
        DereferenceBasicDisk(pBasicDisk);
    }
}


VOID AttachToVolumeWithoutReboot(IN PVOID Context)
{
    PWCHAR              SymbolicLinkList = (PWCHAR)Context, Buf = NULL;
    UNICODE_STRING      ustrVolumeName;
    PFILE_OBJECT        FileObj;
    PDEVICE_OBJECT      DevObj;
    
    NTSTATUS            Status;
    
    Buf = SymbolicLinkList;
    while(1) {
        RtlInitUnicodeString(&ustrVolumeName, SymbolicLinkList);
        if(ustrVolumeName.Length == 0)
            break;

        if((7 * sizeof(WCHAR) == RtlCompareMemory(ustrVolumeName.Buffer, L"\\??\\IDE", 7 * sizeof(WCHAR))) || (7 * sizeof(WCHAR) == RtlCompareMemory(ustrVolumeName.Buffer, L"\\??\\FDC", 7 * sizeof(WCHAR)))) {
            SymbolicLinkList = SymbolicLinkList + ustrVolumeName.Length / sizeof(WCHAR) + 1;
            continue;
        }

        DriverContext.IsVolumeAddedByEnumeration = true;

        Status = IoGetDeviceObjectPointer(&ustrVolumeName, STANDARD_RIGHTS_WRITE | STANDARD_RIGHTS_READ, &FileObj, &DevObj);
        
        if(NT_SUCCESS(Status)) {
            if (DevObj->DeviceType != FILE_DEVICE_DISK) {
                ObDereferenceObject(FileObj);
            } else {
                if (DevObj->DriverObject->DriverExtension->ServiceKeyName.Length == 
                        RtlCompareMemory(DevObj->DriverObject->DriverExtension->ServiceKeyName.Buffer, L"ClusDisk", 
                        DevObj->DriverObject->DriverExtension->ServiceKeyName.Length)) {
                    if(!DriverContext.pDriverObjectForCluster) {
                        DriverContext.pDriverObjectForCluster = DevObj->DriverObject;
                        ASSERT(DriverContext.pDriverObjectForCluster);                    
                    }
                    DriverContext.IsClusterVolume = true;
                } else {
                    if (!DriverContext.pDriverObject) {
                        DriverContext.pDriverObject = DevObj->DriverObject;
                        ASSERT(DriverContext.pDriverObject);                    
                    }
                }            

                InMageFltAddDevice(DriverContext.DriverObject, DevObj);
            }            

            SymbolicLinkList = SymbolicLinkList + ustrVolumeName.Length / sizeof(WCHAR) + 1;
            ustrVolumeName.Length = ustrVolumeName.MaximumLength = 0;
            ustrVolumeName.Buffer = NULL;
            DriverContext.IsClusterVolume = false;
        }

        
    }
    DriverContext.IsVolumeAddedByEnumeration = false;

    if(DriverContext.pDriverObject) {

        
        DriverContext.IsDispatchEntryPatched = true;
        InterlockedExchangePointer((PVOID *)&DriverContext.MajorFunction[IRP_MJ_WRITE], DriverContext.pDriverObject->MajorFunction[IRP_MJ_WRITE]);
        
        InterlockedExchangePointer((PVOID *)&DriverContext.pDriverObject->MajorFunction[IRP_MJ_WRITE], InMageFltWrite);


        InterlockedExchangePointer((PVOID *)&DriverContext.MajorFunction[IRP_MJ_FLUSH_BUFFERS], DriverContext.pDriverObject->MajorFunction[IRP_MJ_FLUSH_BUFFERS]);
        
        InterlockedExchangePointer((PVOID *)&DriverContext.pDriverObject->MajorFunction[IRP_MJ_FLUSH_BUFFERS], InMageFltFlush);


        InterlockedExchangePointer((PVOID *)&DriverContext.MajorFunction[IRP_MJ_DEVICE_CONTROL], DriverContext.pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]);
        
        InterlockedExchangePointer((PVOID *)&DriverContext.pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL], InMageFltDeviceControl);

    }

    if(DriverContext.pDriverObjectForCluster) {

        DriverContext.IsDispatchEntryPatched = true;
        InterlockedExchangePointer((PVOID *)&DriverContext.MajorFunctionForClusDrives[IRP_MJ_WRITE], DriverContext.pDriverObjectForCluster->MajorFunction[IRP_MJ_WRITE]);
        
        InterlockedExchangePointer((PVOID *)&DriverContext.pDriverObjectForCluster->MajorFunction[IRP_MJ_WRITE], InMageFltWrite);


        InterlockedExchangePointer((PVOID *)&DriverContext.MajorFunctionForClusDrives[IRP_MJ_FLUSH_BUFFERS], DriverContext.pDriverObjectForCluster->MajorFunction[IRP_MJ_FLUSH_BUFFERS]);
        
        InterlockedExchangePointer((PVOID *)&DriverContext.pDriverObjectForCluster->MajorFunction[IRP_MJ_FLUSH_BUFFERS], InMageFltFlush);


        InterlockedExchangePointer((PVOID *)&DriverContext.MajorFunctionForClusDrives[IRP_MJ_DEVICE_CONTROL], DriverContext.pDriverObjectForCluster->MajorFunction[IRP_MJ_DEVICE_CONTROL]);
        
        InterlockedExchangePointer((PVOID *)&DriverContext.pDriverObjectForCluster->MajorFunction[IRP_MJ_DEVICE_CONTROL], InMageFltDeviceControl);

    }
    ExFreePool(Buf);
}

//*****************************************************************************
//* NAME:			InMageFetchSymbolicLink
//*
//* DESCRIPTION:	Given a symbolic link name, InMageFetchSymbolicLink returns a string with the 
//*					links destination and a handle to the open link name.
//*					
//*	PARAMETERS:		SymbolicLinkName		IN
//*					SymbolicLink			OUT
//*					LinkHandle				OUT
//*
//*	IRQL:			IRQL_PASSIVE_LEVEL.
//*
//*	RETURNS:		STATUS_SUCCESS
//*					STATUS_UNSUCCESSFUL
//*
//* NOTE:			Onus lies on the caller to free SymbolicLink->Buffer and close
//*					the LinkHandle after a successful call to this routine. -Kartha.
//*                              Bugzilla id: 26376
//*****************************************************************************
static
NTSTATUS InMageFetchSymbolicLink(
		IN		PUNICODE_STRING		SymbolicLinkName,
		OUT		PUNICODE_STRING		SymbolicLink,
		OUT		PHANDLE				LinkHandle
	)
{
	NTSTATUS			status;
	NTSTATUS			returnStatus;
	OBJECT_ATTRIBUTES	oa;
	UNICODE_STRING		tmpSymbolicLink;
	HANDLE				tmpLinkHandle;
	ULONG				symbolicLinkLength;

	returnStatus = STATUS_UNSUCCESSFUL; //Sometimes be pessimistic for a happy ending.

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
		symbolicLinkLength				= 0;
		tmpSymbolicLink.Length			= 0;
		tmpSymbolicLink.MaximumLength	= 0;

		status = ZwQuerySymbolicLinkObject(
				tmpLinkHandle,
				&tmpSymbolicLink,
				&symbolicLinkLength);

		if ( (STATUS_BUFFER_TOO_SMALL == status) && (0 <  symbolicLinkLength) )
		{
			// Allocate the memory and get the ZwQuerySymbolicLinkObject
			tmpSymbolicLink.Buffer = (PWSTR)ExAllocatePoolWithTag( NonPagedPool, symbolicLinkLength, TAG_GENERIC_NON_PAGED );
			tmpSymbolicLink.Length = 0;
			tmpSymbolicLink.MaximumLength = (USHORT) symbolicLinkLength;

			status = ZwQuerySymbolicLinkObject(
					tmpLinkHandle,
					&tmpSymbolicLink,
					&symbolicLinkLength);

			if ( STATUS_SUCCESS == status )
			{
				SymbolicLink->Buffer		= tmpSymbolicLink.Buffer;
				SymbolicLink->Length		= tmpSymbolicLink.Length;
				*LinkHandle					= tmpLinkHandle;
				SymbolicLink->MaximumLength = tmpSymbolicLink.MaximumLength;
				returnStatus				= STATUS_SUCCESS;
			}
			else
			{
				ExFreePool( tmpSymbolicLink.Buffer );
			}
		}
	}
	return returnStatus;
}
//*****************************************************************************
//* NAME:			InMageExtractDriveString
//*
//* DESCRIPTION:	Extracts the drive from a string.  Adds a NULL termination and 
//*					adjusts the length of the source string.
//*					
//*	PARAMETERS:		Source				IN OUT
//*
//*	IRQL:			IRQL_PASSIVE_LEVEL.
//*
//*	RETURNS:		STATUS_SUCCESS
//*					STATUS_UNSUCCESSFUL
//*                              Bugzilla id: 26376 - Kartha
//*****************************************************************************
static
NTSTATUS InMageExtractDriveString(
		IN OUT	PUNICODE_STRING		Source
	)
{
	
	ULONG		i;
	ULONG		numSlashes;
	NTSTATUS	status = STATUS_UNSUCCESSFUL; 

	i = 0;
	numSlashes	= 0;

	while ( ( (i * 2) < Source->Length ) && ( 4 != numSlashes ) )
	{
		if ( L'\\' == Source->Buffer[i] )
		{
			numSlashes++;
		}
		i++;
	}

	if ( ( 4 == numSlashes ) && ( 1 < i ) )
	{
		i--;
		Source->Buffer[i]	= L'\0';
		Source->Length		= (USHORT) i * 2;
		status				= STATUS_SUCCESS;
	}

	return status;
}

//*****************************************************************************
//* NAME:			InMageSetBootVolumeChangeNotificationFlag
//*
//* DESCRIPTION:	This is a thread routine which basically tries to identify the boot
//*                 volume (volume where windows system files reside). Once that's found
//*                 the corresponding device object of the volume owner (volume manager)
//                  is grabbed along with its File object.
//*                 The whole exercise is a one time affair during the lifetime of involflt
//*                 till shutdown.
//*
//*					
//*	PARAMETERS:		Pointer to InMage Volume Context.
//*
//*	IRQL:			IRQL_PASSIVE_LEVEL.
//*
//*	RETURNS:		VOID
//*                 
//*                 SUCCESS:
//*                 When this is the case then we reaffirm that 
//*                 DriverContext->bootVolumeDetected is true
//*					and we have already set the pnp notification for boot volume.
//*
//*					FAILURE:
//*                 This means something went wrong, we reset the flag
//*					DriverContext->bootVolumeDetected to zero. We haven't yet retistered
//*					boot volume event notification through IoRegisterPlugPlayNotification.
//*
//* NOTE:			Bugzilla id: 26376 - Written by: M. Kartha.
//*					
//*****************************************************************************
VOID InMageSetBootVolumeChangeNotificationFlag(PVOID pContext)
{
  #define SYSTEM_ROOT	L"\\SystemRoot"
  #define BOOT_DEVICE   L"\\Device\\BootDevice"
  UNICODE_STRING		systemRootName;
  UNICODE_STRING		systemRootSymbolicLink1;
  UNICODE_STRING		systemRootSymbolicLink2;
  PFILE_OBJECT		    fileObject;
  HANDLE				linkHandle;
  PVOLUME_CONTEXT       pVolumeContext;
  PDEVICE_OBJECT        deviceObject;
  PDEVICE_EXTENSION     pDeviceExtension;
  BOOLEAN				IsWindows8AndLater;
  NTSTATUS				status, returnStatus;


  //Initialize the systemroot string.

  IsWindows8AndLater = DriverContext.IsWindows8AndLater;

  if(IsWindows8AndLater){
  	RtlInitUnicodeString(
			&systemRootName,
			BOOT_DEVICE);	
  }else
  
  {
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
  pVolumeContext = NULL;
  pDeviceExtension = NULL;
  returnStatus = STATUS_UNSUCCESSFUL;
  
  pVolumeContext = DriverContext.bootVolNotificationInfo.pBootVolumeContext;

  if(!pVolumeContext){goto rundown;}

  //Lets get the system root directory absolute path.
  status = InMageFetchSymbolicLink(
			&systemRootName,
			&systemRootSymbolicLink1,
			&linkHandle);

  //if we succeeded in getting the full system root path it would look like
  // \Device\Harddisk0\Partition1\WINDOWS. Our next task is to get to the
  //the symbolic name \Device\Harddisk0 from it.
  //Note the exception: OS version starting 2k12, this value can be directly obtained from
  //bootdevice link and shall look like \Device\HarddiskVolume6,
  //so that we can directly invoke IoGetDeviceObjectPointer without any intermediate steps.
  if ( STATUS_SUCCESS == status )
	{
	
        if(!IsWindows8AndLater){
		  ZwClose( linkHandle );	
		  status = InMageExtractDriveString( &systemRootSymbolicLink1 );
		  if(STATUS_SUCCESS == status){
		  	status = InMageFetchSymbolicLink(
					&systemRootSymbolicLink1,
					&systemRootSymbolicLink2,
					&linkHandle);
		  }
        }
		else
		{
		  //Might be for now this else condition is not needed since we always check
		  //for flag IsWindows8AndLater but for the sake of code completeness as well
		  //as being farsighted.
		  ZwClose( linkHandle );	
		 
		  	status = InMageFetchSymbolicLink(
					&systemRootSymbolicLink1,
					&systemRootSymbolicLink2,
					&linkHandle);
		 
		}

		if ( STATUS_SUCCESS == status )
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
				ObReferenceObject( deviceObject );
				
		    	//Here let's make sure that the volume context, which we have, belongs to
				//the boot volume itself. This is to make double sure even though we know
				//that in all normal cicumstances the first IO goes to boot volume in windows
				//in boot phase0.

				pDeviceExtension = (PDEVICE_EXTENSION) pVolumeContext->FilterDeviceObject->DeviceExtension;

				if(pDeviceExtension->PhysicalDeviceObject == fileObject->DeviceObject){
                //This is the fileobject of Volume Manager owned device.
				//Register plug and play Notification for this in our driver.
                pVolumeContext->BootVolumeNotifyFileObject = fileObject;
                pVolumeContext->BootVolumeNotifyDeviceObject = deviceObject;

				status = IoRegisterPlugPlayNotification(
                                  EventCategoryTargetDeviceChange, 0, fileObject,
                                  DriverContext.DriverObject, BootVolumeDriverNotificationCallback, pVolumeContext,
                                  &pVolumeContext->PnPNotificationEntry);
				
                //ASSERT(STATUS_SUCCESS == status);   

				if(STATUS_SUCCESS == status){returnStatus = STATUS_SUCCESS;}
				}

				ObDereferenceObject( deviceObject );
			 }
		 ZwClose( linkHandle );
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
		//use Interlocked bus locking intrinsics.-Kartha
		if(0 == DriverContext.bootVolNotificationInfo.bootVolumeDetected){
			InterlockedIncrement(&DriverContext.bootVolNotificationInfo.bootVolumeDetected);}
	}else{
		InterlockedExchange(&DriverContext.bootVolNotificationInfo.bootVolumeDetected, (LONG)0);//reset.
		InterlockedIncrement(&DriverContext.bootVolNotificationInfo.failureCount);
		if(fileObject) ObDereferenceObject(fileObject);
		//try this only 10 times....
		if(MAX_FAILURE > DriverContext.bootVolNotificationInfo.failureCount){
		KeClearEvent(&DriverContext.bootVolNotificationInfo.bootNotifyEvent);//make if not-signalled or blocking.
		goto waitevnt;
		}
	}
	
  //To the OS its always STATUS_SUCCESS.
  PsTerminateSystemThread(STATUS_SUCCESS);//Good Bye cruel world.
}
//*****************************************************************************
//* NAME:			BootVolumeDriverNotificationCallback
//*
//* DESCRIPTION:	Sevices DeviceChange notifications, namely LOCK/UNLOCK on volume.
//*					
//*	PARAMETERS:		NotificationStructure  pContext
//*
//*	IRQL:			IRQL_PASSIVE_LEVEL.
//*
//*	RETURNS:		STATUS_SUCCESS
//*					STATUS_UNSUCCESSFUL
//*
//*                              Bugzilla id: 26376 : By: Kartha
//*****************************************************************************
extern "C" NTSTATUS
BootVolumeDriverNotificationCallback(
    PVOID NotificationStructure,
    PVOID pContext
    )
{
    KIRQL                               OldIrql;
    PVOLUME_BITMAP                      pVolumeBitmap = NULL;
	PVOLUME_CONTEXT                     pCurrentVolumeContext;
    PVOLUME_CONTEXT                     pVolumeContext;
	PTARGET_DEVICE_CUSTOM_NOTIFICATION  pNotify;

  
    pCurrentVolumeContext = (PVOLUME_CONTEXT)pContext; 

	pNotify = (PTARGET_DEVICE_CUSTOM_NOTIFICATION)NotificationStructure;
	
    pVolumeContext = (PVOLUME_CONTEXT)DriverContext.HeadForVolumeContext.Flink;

	//Note: InvolFlt stores all the bitmap files on System boot volume but individual
	//volume context stores the bitmap context pointer only for that corresponding volume.
	//what I mean to say is that, if we get a LOCK/UNLOCK/LOCK-FAIL notification on boot
	//volume then effectively we have to LOCK all volumes by closing handles to all bitmap
	//files in each context.
	//For instance: Say we have volumes C: and D: but protection is only on D:. Now when
	//we get LOCK notification on boot volume then to release all handles on it we are
	//forced to close the bitmap files on D: too.

    if ((NULL == pVolumeContext) || (NULL == pNotify) || (NULL == pCurrentVolumeContext) )
	{return STATUS_SUCCESS;}

	//This is a restriction we consciously implement inorder to prevent LOCKING/UNLOCKING
	//of boot volume once the system has completed booting and already started running
	//services.exe. We simply retuen STATUS_SUCCESS, meaning it's Okay for us to
	//LOCK/UNLOCK but without releasing/closing any bitmap file which indrectly stops
	//the volume LOCK/UNLOCK process. Also remember that any boot volume dismount/
	//LOCK never succeeds in practice.

	if(0 < DriverContext.bootVolNotificationInfo.LockUnlockBootVolumeDisable){
		return STATUS_SUCCESS;
	}
	
    // IsEqualGUID takes a reference to GUID (GUID &).
    //GUID_IO_VOLUME_LOCK shall always be called by FS before autochk.exe
    //is run at the startup. autochk.exe is a native app.-Kartha.
    //IMP point: Once autochk.exe is successful that current boot up session is
    //aborted by OS that means we should never expect a GUID_IO_VOLUME_UNLOCK.
    if (IsEqualGUID(pNotify->Event, GUID_IO_VOLUME_LOCK))
    {
	  while ( (pVolumeContext) && (pVolumeContext != (PVOLUME_CONTEXT)(&DriverContext.HeadForVolumeContext)) )
	  {
        
		//if we aren't already locked.....
        if (0 == (pVolumeContext->ulFlags & VCF_CV_FS_LOCKED)) {
            LARGE_INTEGER   WaitTime;
            NTSTATUS Status = STATUS_SUCCESS;
            
			InVolDbgPrint( DL_INFO, DM_VOLUME_STATE, ("BootVolumeDriverNotificationCallback::Received GUID_IO_VOLUME_LOCK notification on volume %p\n", pVolumeContext));
            
			if (pVolumeContext->pVolumeBitmap){
				
				KeQuerySystemTime(&WaitTime);
				KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
				pVolumeContext->liDisMountNotifyTimeStamp.QuadPart = WaitTime.QuadPart;
				KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);
				WaitTime.QuadPart += DriverContext.ulSecondsMaxWaitTimeOnFailOver * HUNDRED_NANO_SECS_IN_SEC;
				Status = InMageFltSaveAllChanges(pVolumeContext, TRUE, &WaitTime);
				
				if(pVolumeContext->pVolumeBitmap->pBitmapAPI) {
                pVolumeContext->pVolumeBitmap->pBitmapAPI->ChangeBitmapModeToRawIO();
				// Set FS LOCK flag here.
            	pVolumeContext->ulFlags |= VCF_CV_FS_LOCKED;
			  }//else volume bitmap was never created might be b'coz the volume was never protected.
            }
			InVolDbgPrint( DL_INFO, DM_VOLUME_STATE, ("BootVolumeDriverNotificationCallback::Return from Save All Changes. Status = %#x\n", Status));
            
        } else {
            InVolDbgPrint( DL_INFO, DM_VOLUME_STATE, ("Received GUID_IO_VOLUME_LOCK notification on an already locked volume %p\n", pVolumeContext));
        }

	   pVolumeContext = (PVOLUME_CONTEXT)(pVolumeContext->ListEntry.Flink); 
	  }
     
	  if (pCurrentVolumeContext->BootVolumeNotifyFileObject) {
	  	        ObDereferenceObject(pCurrentVolumeContext->BootVolumeNotifyFileObject);
                pCurrentVolumeContext->BootVolumeNotifyFileObject = NULL;
                if(pCurrentVolumeContext->BootVolumeNotifyDeviceObject)pCurrentVolumeContext->BootVolumeNotifyDeviceObject= NULL;
            }
    //Codes handling VOLUME_UNLOCK and VOLUME_LOCK_FAILED are duplicate but let's not
	//club it for some other reasons.
    } 

	else if (IsEqualGUID(pNotify->Event, GUID_IO_VOLUME_LOCK_FAILED)) {
        InVolDbgPrint( DL_INFO, DM_VOLUME_STATE, ("Received GUID_IO_VOLUME_LOCK_FAILED notification on volume %p\n", pVolumeContext));
		
      while ( (pVolumeContext) && (pVolumeContext != (PVOLUME_CONTEXT)(&DriverContext.HeadForVolumeContext)) )
	  {
        KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
        KeQuerySystemTime(&pVolumeContext->liDisMountFailNotifyTimeStamp);

        if (pVolumeContext->ulFlags & VCF_CV_FS_LOCKED) {
            pVolumeContext->ulFlags &= ~VCF_CV_FS_LOCKED;
            pVolumeBitmap = pVolumeContext->pVolumeBitmap;
            pVolumeContext->pVolumeBitmap = NULL;
        }
        KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);

        if (NULL != pVolumeBitmap) {
            BitmapPersistentValues BitmapPersistentValue(pVolumeContext);
            InVolDbgPrint( DL_INFO, DM_VOLUME_STATE | DM_BITMAP_CLOSE,
                ("Closing bitmap file on GUID_IO_VOLUME_LOCK_FAILED notification for volume %S(%S)\n",
                pVolumeBitmap->VolumeGUID, pVolumeBitmap->VolumeLetter));
            CloseBitmapFile(pVolumeBitmap, false, BitmapPersistentValue, false);
        }

       pVolumeContext = (PVOLUME_CONTEXT)(pVolumeContext->ListEntry.Flink); 
	  }

    //Codes handling VOLUME_UNLOCK and VOLUME_LOCKFAILED are duplicate but let's not
	//club it for some other reasons.
    } 

	else if (IsEqualGUID(pNotify->Event, GUID_IO_VOLUME_UNLOCK)) {
        InVolDbgPrint( DL_INFO, DM_VOLUME_STATE, ("Received GUID_IO_VOLUME_UNLOCK notification on volume %p\n", pVolumeContext));
		

      while ( (pVolumeContext) && (pVolumeContext != (PVOLUME_CONTEXT)(&DriverContext.HeadForVolumeContext)) )
	  {
        KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
        KeQuerySystemTime(&pVolumeContext->liMountNotifyTimeStamp);
        if (pVolumeContext->ulFlags & VCF_CV_FS_LOCKED) {
            pVolumeContext->ulFlags &= ~VCF_CV_FS_LOCKED;
            pVolumeBitmap = pVolumeContext->pVolumeBitmap;
            pVolumeContext->pVolumeBitmap = NULL;
        }
        KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);
        if (NULL != pVolumeBitmap) {
            BitmapPersistentValues BitmapPersistentValue(pVolumeContext);
            InVolDbgPrint( DL_INFO, DM_VOLUME_STATE | DM_BITMAP_CLOSE,
                ("Closing bitmap file on MOUNT notification for volume %S(%S)\n",
                pVolumeBitmap->VolumeGUID, pVolumeBitmap->VolumeLetter));
            CloseBitmapFile(pVolumeBitmap, false, BitmapPersistentValue, false);
        }

       pVolumeContext = (PVOLUME_CONTEXT)(pVolumeContext->ListEntry.Flink); 
	  }

    }else {
        InVolDbgPrint( DL_INFO, DM_VOLUME_STATE, ("Received unhandled notification on volume %p %x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x\n",
                pVolumeContext, pNotify->Event.Data1, pNotify->Event.Data2, pNotify->Event.Data3,
                pNotify->Event.Data4[0], pNotify->Event.Data4[1], pNotify->Event.Data4[2], pNotify->Event.Data4[3], 
                pNotify->Event.Data4[4], pNotify->Event.Data4[5], pNotify->Event.Data4[6], pNotify->Event.Data4[7])); 
    }

    return STATUS_SUCCESS;
}
//*****************************************************************************
//* NAME:			InMageFltOnReinitialize
//*
//* DESCRIPTION:	Invoke this after initialization of all boot drivers and creation of all boot objects.
//*                              We could have used work item but NO since a work item cannot guarantee
//*                              that the routine is not being executed before DriverEntry finishes and doesnot
//*                              ensure that it runs only after all boot drivers are loaded.-Kartha
//*					
//*	PARAMETERS:	       IN DriverObject, IN Context, IN Count (how many times this was called)
//*
//*	IRQL:			IRQL_PASSIVE_LEVEL.
//*
//*	RETURNS:		NOTHING
//*
//*					Bugzilla id: 26376
//*****************************************************************************

VOID InMageFltOnReinitialize(IN PDRIVER_OBJECT pDriverObj,
							 IN PVOID          pContext,
							 IN ULONG          ulCount)
{
  //we donot invoke a re-initialize call here so we will be executed when ulCount == 1 only.

	HANDLE 				notifyThread;


	//Create a transient system thread that would be woken up by the first IO touching the boot volume.
	//This thread is responsible for registring DeviceChange notificaton event on boot volume.
	//Since this is a volume based driver in storage stack, we presume that the driver is not going to get
	//unloaded untill and unless an OS shutdown happens. So no need to do a cleanup.-Kartha
	//The autochk.exe failing issue has been observed only with Windows 2012 or >. So we just limit this
	//functionality or fix to w2k12. BugId: 26376

	if( (DriverContext.IsWindows8AndLater) && \
		(0 >= DriverContext.bootVolNotificationInfo.bootVolumeDetected) &&(1 >= ulCount)){
		if(STATUS_SUCCESS == PsCreateSystemThread(&notifyThread, THREAD_ALL_ACCESS, 0, 0, 0, InMageSetBootVolumeChangeNotificationFlag, NULL))
	    {
	      InterlockedIncrement(&DriverContext.bootVolNotificationInfo.reinitCount); 
	      ZwClose(notifyThread);
		}
		else{}
	}
	

}

//*****************************************************************************
//* NAME:			InMageVerifyNewVolume  (to handle any Duplicity of volumes)
//*
//* DESCRIPTION:		On the presumption that we might miss a PNP surprise or
//*                 		remove event, we have to make sure that the same volume
//*                 		when pops up next time should be handled properly.
//*					
//*	PARAMETERS:	    	IN pDeviceObject, IN pVolumeContext
//*
//*	IRQL:			IRQL_PASSIVE_LEVEL.
//*
//*	RETURNS:		NOTHING
//*
//* NOTES:      		Since we are a PNP compliant driver we are supposed to get
//*                 		all PNP events without any fail. But as far as this bug is conce-
//*					rned it was reported a couple of times (in a sporadic manner) that
//*					volumes which already had protection "on" were coming up as new
//*					ones.This led to the presumption that we could have somehow
//*					missed the PNP remove notifications.
//*
//*                 		If the above said is true then it might lead to our protection
//*                 		pair entering a limbo state. Does this really happen because
//*                 		of a missed PNP SURPRISE or REMOVE? This could not be
//*                 		substantiated, but certain anecdotal evidence suggests something
//*                 		of that sort. Probably this bug could be a consequence of
//*                 		some other non-compliant driver above us. -Kartha. But
//*                 		anyway there is no harm in implementing this for the sake of
//*                 		an unfailing functionality.
//*  
//* Bugzilla id:  		25726
//*****************************************************************************

NTSTATUS InMageCrossCheckNewVolume(IN PDEVICE_OBJECT pDeviceObject,
							 IN PVOLUME_CONTEXT pVolumeContext,
							 IN PMOUNTDEV_NAME pMountDevName)
{

   NTSTATUS 			Status = STATUS_SUCCESS;
   PLIST_ENTRY         	pCurr = DriverContext.HeadForVolumeContext.Flink;
   PVOLUME_CONTEXT 		pVolContext = NULL;
   PDEVICE_EXTENSION   	pDeviceExtension = NULL;

	if((!pDeviceObject) || (!pVolumeContext) || (!pMountDevName)){

		InVolDbgPrint( DL_INFO, DM_VOLUME_STATE,
                ("InMageVerifyNewVolume: Invalid argument parameter passed\n"));
		Status = STATUS_INVALID_PARAMETER;
		goto CrossCheckDone;
	}

	//Note: For a given boot session the primary unique volume name would invariably remain same.

	if (!INMAGE_IS_MOUNTDEV_NAME_VOLUME_GUID(pMountDevName)) {

		InVolDbgPrint( DL_INFO, DM_VOLUME_STATE,
        ("InMageVerifyNewVolume: pMountDevName arg. passed isn't a valid volume GUID Name\n"));
		Status = STATUS_INVALID_PARAMETER;
		goto CrossCheckDone;
	}

	while (pCurr != &DriverContext.HeadForVolumeContext) {

		//crosscheck the new volume against each existing one.
		pVolContext = (PVOLUME_CONTEXT)pCurr;

		pCurr = pCurr->Flink;

		//when IRP_MN_SURPRISE_REMOVE is called we set VCF_DEVICE_SURPRISE_REMOVAL flag.
		//similarly when IRP_MN_REMOVE.. is called we make the FDO pointer in pVolumeContext NULL.
		//Aso in IRP_MN_REMOVE... we remove the VolumeContext list entry and deallocate the same.

		//We are not supposed to do anything where we got a surprise remove but didnot get a remove
		//device.
		if(VCF_DEVICE_SURPRISE_REMOVAL & pVolContext->ulFlags){continue;}

        //As per our records device is in offline state i.e. remove device was invoked on it.
		if(ecDSDBCdeviceOffline == pVolContext->eDSDBCdeviceState){continue;}

		if(0 == (VCF_GUID_OBTAINED & pVolContext->ulFlags)){continue;}

		if(1 > pVolContext->UniqueVolumeName.Length){continue;}

        if(NULL == pVolContext->FilterDeviceObject){
			//Means Remove Device routine was called but somehow the volume context entry
			//still lingers. Nothing much to be done here.
		    continue;
        }

		//check if we are accessing the newly added volume context, if yes then continue.
		if(pDeviceObject == pVolContext->FilterDeviceObject){
			continue;
		}

        pDeviceExtension = (PDEVICE_EXTENSION) pVolContext->FilterDeviceObject->DeviceExtension;

		if(NULL == pDeviceExtension){
			//Owner of DeviceExtension is system.
		    continue;
        }

		if( (RemovePending == pDeviceExtension->DevicePnPState) || \
			(SurpriseRemovePending == pDeviceExtension->DevicePnPState)){
			//We have to log this error. This should not happen.
			InVolDbgPrint( DL_ERROR, DM_VOLUME_STATE,
             ("A PNP SURPRISE OR REMOVE notification was received but not acted upon for volume %wZ\n",
                &pVolContext->UniqueVolumeName));
		}

		//Have we already processed this volume context?
		if( (LimboState == pDeviceExtension->DevicePnPState)){
			continue;
		}

        //we are @ PASSIVE level. We could have used compare memory invariably, but by using
        //compare string we stand a better chance of not getting into erroneous mem size cases.
		if (0 == RtlCompareUnicodeString(
			&pVolumeContext->UniqueVolumeName, &pVolContext->UniqueVolumeName, FALSE)){

			//This means the new volume context as well as the old existing one corresponds to
			//one and the same volume. Let's act.

		    /*Heads-up: if we have missed a PNP surprise or PNP remove device (by any chance)
		         we should not use the corresponding stale volume context in any major function calls
		    	   thenceforth since it might involve the use of the pre-existed stale
		         device object still attached to the context, for instance: like passing an irp down the stack.
		         Remember that there is no valid devstack present for a device which went through
		         a PNP device removal with or without our notice.*/

            //Make sure that we don't process the same entry in the next call to this procedure.

			SET_NEW_PNP_STATE(pDeviceExtension, LimboState);//previous state auto. restored.

            //SetVolumeOutOfSync is called here.
			SetVolumeOutOfSync(pVolContext, ERROR_TO_REG_MISSED_PNP_VOLUME_REMOVE, Status, false);
            InVolDbgPrint(DL_ERROR, DM_VOLUME_STATE,
            ("InMageCrossCheckNewVolume: Setting volume %wZ out of sync due to missed remove PNP\n",
                        &pVolContext->UniqueVolumeName));
			if(pVolContext->pVolumeBitmap){
			  BitmapPersistentValues BitmapPersistentValue(pVolContext);
			  InVolDbgPrint( DL_INFO, DM_VOLUME_STATE | DM_BITMAP_CLOSE,
              ("InMageCrossCheckNewVolume: Closing bitmap file on stale volume %S(%S)\n",
              pVolContext->pVolumeBitmap->VolumeGUID, pVolContext->pVolumeBitmap->VolumeLetter));
			  CloseBitmapFile(pVolContext->pVolumeBitmap, false, BitmapPersistentValue, false);
			}

			goto CrossCheckDone;
			
		}
		
	}


CrossCheckDone:
		
    return Status;
}

