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
#include "MntMgr.h"
#include "mscs.h"
#include "IoctlInfo.h"
#include "ListNode.h"
#include "misc.h"
#include "VBitmap.h"
#include "TagNode.h"
#include "MetaDataMode.h"
#include "svdparse.h"
#include "HelperFunctions.h"
#include "VContext.h"
#include <version.h>


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
InMageFltWrite(
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

#ifdef VOLUME_NO_REBOOT_SUPPORT 
VOID GetDeviceNumberAndSignature(PDEV_CONTEXT pDevContext);
VOID GetVolumeGuidAndDriveLetter(PDEV_CONTEXT pDevContext);
VOID AttachToVolumeWithoutReboot(IN PVOID Context);
#endif

DeviceIoControlVolumeOnline(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    );
DeviceIoControlVolumeOffline(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    );
NTSTATUS
DeviceControlCompletionRoutine(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );
DeviceIoControlCommitVolumeSnapshot(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    );

#ifdef VOLUME_CLUSTER_SUPPORT
extern "C" NTSTATUS
UpdateGlobalWithPersistentValuesReadFromBitmap(BOOLEAN bDevInSync, BitmapPersistentValues &BitmapPersistentValue) ;

extern "C" PWCHAR
GetVolumeOverride(PDEV_CONTEXT pDevContext);

extern "C" void 
InitializeClusterDiskAcquireParams(PDEV_CONTEXT pDevContext);

extern "C" NTSTATUS
DriverNotificationCallback(
    PVOID NotificationStructure,
    PVOID pContext
    );

VOID GetClusterInfoForThisVolume(PDEV_CONTEXT pDevContext);
#endif

NTSTATUS
DeviceIoControlGetVolumeFlags(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    );

//
// GetNextDeviceID would match GUID from input parameter DevID.
// Search is started with primary GUID in DevContext and continues in
// GUIDList. If a matching GUID is found, it would return the next GUID in list
// by copying the next GUID into DeviceGUID input buffer.
// If matching GUID is the last GUID (i.e, no next GUID) it returns STATUS_NOT_FOUND
// If matching GUID is not found in list, then first/primary GUID is copied into
// DeviceGUID and STATUS_SUCCESS is returned.
// If next GUID is returned Status returned is STATUS_SUCCESS
// If next GUID is not present Status returned is STATUS_NOT_FOUND

NTSTATUS
GetNextDeviceID(PDEV_CONTEXT DevContext, PWCHAR pDeviceID);

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
#ifdef VOLUME_NO_REBOOT_SUPPORT
    PWCHAR              SymbolicLinkList = NULL;
    UNICODE_STRING      ustrVolumeName;
#endif

    // we want this printed if we are printing anything
    InVolDbgPrint(DL_ERROR, DM_DRIVER_INIT, ("InDevFlt!DriverEntry: entered\nCompiled %s  %s\n", __DATE__,  __TIME__));

    if (BypassCheckBootIni() == STATUS_SUCCESS) {
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
#ifdef VOLUME_CLUSTER_SUPPORT
    LoadClusterConfigFromClustDiskDriverParameters();
#endif

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

#ifdef VOLUME_NO_REBOOT_SUPPORT
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
            InMageFltLogError(DriverContext.ControlDevice, __LINE__, INFLTDRV_ERR_ENUMERATE_DEVICE, STATUS_SUCCESS, (ULONG)Status);
        }
    }
#endif
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

NTSTATUS
InMageFltDispatchPower(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    PDEVICE_EXTENSION pDeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION  currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    PDEV_CONTEXT   pDevContext = NULL;
#ifdef VOLUME_CLUSTER_SUPPORT
    PDEV_BITMAP    pDevBitmap = NULL;
    KIRQL             OldIrql = NULL;
#endif
    NTSTATUS          Status = STATUS_SUCCESS;

    
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
            pDevContext = pDeviceExtension->pDevContext;
            if ((pDevContext) && (!(pDevContext->ulFlags & DCF_FILTERING_STOPPED))) {
#ifdef VOLUME_CLUSTER_SUPPORT
                if (IS_CLUSTER_VOLUME(pDevContext)) {
                    // For Cluster Volume, Power IRP may come before FS Unmount and Disk Release(Bug # 3453) 
                    // Let's store the changes to either Bitmap or Disk.
                    if (pDevContext->ChangeList.ulTotalChangesPending || pDevContext->ulChangesPendingCommit) {
                        if (NULL == pDevContext->pDevBitmap) {
                            pDevBitmap = OpenBitmapFile(pDevContext, Status);
                            if (pDevBitmap) {
                                KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
                                pDevContext->pDevBitmap = ReferenceDevBitmap(pDevBitmap);
                                if(pDevContext->bQueueChangesToTempQueue) {
                                    MoveUnupdatedDirtyBlocks(pDevContext);
                                }
                                KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
                            }
                        }
                        if (NULL != pDevContext->pDevBitmap) {
                            InMageFltSaveAllChanges(pDevContext, TRUE, NULL);
                            if (pDevContext->pDevBitmap->pBitmapAPI) {
                                BitmapPersistentValues BitmapPersistentValue(pDevContext);
                                pDevContext->pDevBitmap->pBitmapAPI->CommitBitmap(cleanShutdown, BitmapPersistentValue, NULL);
                            }
                        } else {
                            InVolDbgPrint( DL_ERROR, DM_CLUSTER, ("InMageFltDispatchPower Opening Bitmap File for Volume %c: has failed\n", 
                                    (UCHAR)pDevContext->wDevNum[0]));
                            SetBitmapOpenFailDueToLossOfChanges(pDevContext, false);
                        }
                    }
                    ASSERT((0 == pDevContext->ChangeList.ulTotalChangesPending) && (0 == pDevContext->ChangeList.ullcbTotalChangesPending));
                    if (pDevContext->ChangeList.ulTotalChangesPending) {
                        InVolDbgPrint( DL_ERROR, DM_CLUSTER, ("Volume DiskSig = 0x%x has ChangesPending = 0x%x ChangesPendingCommit = 0x%x\n",
                                pDevContext->ulDiskSignature, pDevContext->ChangeList.ulTotalChangesPending,
                                pDevContext->ulChangesPendingCommit));
                    }
                } else {
#endif
                    // This volume is not a cluster volume.
                    if (pDevContext->pDevBitmap) {
                        InVolDbgPrint(DL_INFO, DM_POWER, ("InMageFltDispatchPower Flushing final volume changes Device = %#p \n",
                                DeviceObject));
                        FlushAndCloseBitmapFile(pDevContext);
                    }
#ifdef VOLUME_CLUSTER_SUPPORT
                }
#endif
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
    PDEV_CONTEXT        pDevContext = NULL;
    PIO_STACK_LOCATION  currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION  topIrpStack;

    NTSTATUS            Status = STATUS_SUCCESS;
#ifdef VOLUME_NO_REBOOT_SUPPORT
    PDRIVER_DISPATCH     DispatchEntryForWrite = NULL;
  
    
    if(DriverContext.IsDispatchEntryPatched) {
        pDevContext = GetDevContextForThisIOCTL(DeviceObject, NULL);
        if(pDevContext) {
            //
            // Call came to a Lower-level DeviceObject. Since its dispacth routines are taken-over by
            // driver, Inmage filter got the call. Process the IRP.
            //
            DeviceObject = pDevContext->FilterDeviceObject;
            pDeviceExtension = (PDEVICE_EXTENSION)pDevContext->FilterDeviceObject->DeviceExtension;
#ifdef VOLUME_CLUSTER_SUPPORT
            if(pDevContext->IsVolumeCluster) {
                DispatchEntryForWrite = DriverContext.MajorFunctionForClusDrives[IRP_MJ_WRITE];
            } else {
#endif
                DispatchEntryForWrite = DriverContext.MajorFunction[IRP_MJ_WRITE];
#ifdef VOLUME_CLUSTER_SUPPORT
            }
#endif

            //
            // This is a case of new volumes that are created after filter driver
            // is loaded. Such volumes come through "AddDevice" (not enumerated).
            // For such volumes, filter driver will be in the stack and IRPs will be
            // processed at filter level. But because, dispatch routines of lower-level
            // DeviceObject are taken-over by filter, the IRP again comes to filter
            // through this route/path. Need to ignore the IRP (do not process it again)
            // and pass it down to lower-level DeviceObject.
            //
            if(pDevContext->IsDevUsingAddDevice)
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
                // Call came to filter, when it was not expected, as filter is loaded
                // at run-time and its not in the stack.
                //
                // But this is possible for new volumes for which invoflt driver will
                // be in the stack and also for existing volumes which have been
                // unmounted/re-mounted.
                //
                // In such cases Filter driver will receive same IRP multiple
                // times. At one point, IRP will directly come to filter, as it is in
                // the stack. Second time, IRP will again come to filter, as it
                // has taken-over dispatch routines of lower-level DeviceObject.
                // In case of cluster volumes, IRP may again come to filter
                // as it might have taken-over dispatch routines of "clusdisk" driver too.
                //
                // In such cases IRP is handled at invofllt level and ignored at other
                // levels.
                //
                
                pDeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
                pDevContext = pDeviceExtension->pDevContext;

                //
                // Bug Fix: 14969: System stop responding with NoReboot Driver
                // Please see the bug for details
                // This is a case of existing device (volume existed before
                // filter driver was loaded) getiing unmounted/remounted.
                // This time for this volume filter is in the stack but this volume
                // came through enumeration (not through "AddDevice").
                // Because filter is in the stack, this IRP came to filter.
                // Since, device came by enumeration and since for this
                // device dispacth routines of lower-level DeviceObjects are
                // taken-over by filter, we know that this IRP will again come
                // to filter at next/lower DeviceObjects level. We will process
                // the IRP at that point and not here.
                //
                if (pDevContext->IsDevUsingAddDevice != true) {
                    IoSkipCurrentIrpStackLocation(Irp);
                    return IoCallDriver(pDeviceExtension->TargetDeviceObject, Irp);
                }

                if (pDevContext){
#ifdef VOLUME_CLUSTER_SUPPORT
                    if(pDevContext->IsVolumeCluster) {
                        DispatchEntryForWrite = DriverContext.MajorFunctionForClusDrives[IRP_MJ_WRITE];
                    } else {
#endif
                        DispatchEntryForWrite = DriverContext.MajorFunction[IRP_MJ_WRITE];
#ifdef VOLUME_CLUSTER_SUPPORT
                    }
#endif
                }
            }
#ifdef VOLUME_CLUSTER_SUPPORT
       	    else if (DriverContext.pDriverObject == DeviceObject->DriverObject ) {
                //
                // IRP has been already proceesed at filter level or lower-level DeviceObjects level.
                // Should not process it again. Ignore it. Call came here because this guys/drivers
                // dispatch-routines are taken-over by filter.
                //
                return (*DriverContext.MajorFunction[IRP_MJ_WRITE])(DeviceObject, Irp);                    
            } else
                //
                // IRP has been already proceesed at filter level or lower-level DeviceObjects level.
                // Should not process it again. Ignore it. Call came here because this guys/drivers (clusdisk)
                // dispatch-routines are taken-over by filter.
                //
                return (*DriverContext.MajorFunctionForClusDrives[IRP_MJ_WRITE])(DeviceObject, Irp);   
#else
                return (*DriverContext.MajorFunction[IRP_MJ_WRITE])(DeviceObject, Irp);
#endif
        }
    } else {
#endif
        pDeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
#ifdef VOLUME_NO_REBOOT_SUPPORT
    }

    if (DriverContext.IsDispatchEntryPatched && (pDevContext->IsDevUsingAddDevice != true)) {
        // Bug Fix: 15364: filter handles only DIRECT IO
        if (NULL == Irp->MdlAddress) {
            return (*DispatchEntryForWrite)(pDeviceExtension->TargetDeviceObject, Irp);
        }
    }
#endif
    InVolDbgPrint(DL_VERBOSE, DM_WRITE, ("InMageFltWrite: Device = %#p  Irp = %#p Length = %#x  Offset = %#I64x\n", 
            DeviceObject, Irp, currentIrpStack->Parameters.Write.Length,
            currentIrpStack->Parameters.Write.ByteOffset.QuadPart));

    if (DeviceObject == DriverContext.ControlDevice) {
        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }

    pDevContext = pDeviceExtension->pDevContext;
 
   //This is a fix specific to windows 2012 OS where it was observed that autochk.exe failed
   //at boot time while attempting to fix volume corruption. It was found to be caused due to Inmage filter
   //locking bitmap files on boot volume which incapacitated autochk.exe from doing exclusive locks
   //on files.BUG.ID: 26376 | http://bugzilla/show_bug.cgi?id=26376 - Kartha
   //Here the intention is to capture first write IO (by default filter starts in metadata mode) and then
   //confirm that it's to boot device (usually always yes) followed by registering devicechange notification
   //for boot volume. 
   //First use a simple cmp instruction before using lock on bus instruction for speed. We are in IO path.
   if((!DriverContext.bootVolNotificationInfo.bootVolumeDetected) && \
   	(DriverContext.bootVolNotificationInfo.reinitCount) && (DriverContext.IsWindows8AndLater) ){

    if( 
#ifdef VOLUME_NO_REBOOT_SUPPORT
        (pDevContext->IsDevUsingAddDevice) && (!DriverContext.IsDispatchEntryPatched) &&
#endif
		(MAX_FAILURE > DriverContext.bootVolNotificationInfo.failureCount)){
     if(!InterlockedCompareExchange(&DriverContext.bootVolNotificationInfo.bootVolumeDetected, (LONG)1, (LONG)0)){
		 InterlockedExchangePointer((PVOID *)&DriverContext.bootVolNotificationInfo.pBootDevContext, pDevContext);
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
#ifdef VOLUME_NO_REBOOT_SUPPORT
        if(!pDevContext->IsDevUsingAddDevice && DriverContext.IsDispatchEntryPatched)
            return (*DispatchEntryForWrite)(pDeviceExtension->TargetDeviceObject, Irp);
        else
            return InMageFltSendToNextDriver(DeviceObject, Irp);
#else
		return InMageFltSendToNextDriver(DeviceObject, Irp);
#endif
    }




    //
    //Check if the volume is to be treated as read only.
    //If yes complete the irp without sending it down or processing it
    //
    if((NULL != pDevContext) && (pDevContext->ulFlags & DCF_READ_ONLY))
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

    if ((NULL == pDevContext) || (pDevContext->ulFlags & DCF_FILTERING_STOPPED)){
        //if(IsDispatchEntryPatched) 
#ifdef VOLUME_NO_REBOOT_SUPPORT
        if(!pDevContext->IsDevUsingAddDevice && DriverContext.IsDispatchEntryPatched)
            return (*DispatchEntryForWrite)(pDeviceExtension->TargetDeviceObject, Irp);
        else
            return InMageFltSendToNextDriver(DeviceObject, Irp);
#else
        return InMageFltSendToNextDriver(DeviceObject, Irp);
#endif
    }
    bool PagingIO = false;
    //walk down the irp and find the originating file in the first irp stack slot
    topIrpStack = (PIO_STACK_LOCATION)((PCHAR)Irp + sizeof(IRP)) + (Irp->StackCount - 1);

    // Bug#23913 : On Windows 2012, Some Hyper-v writes have invalid fileobject leading to crash. Tests showed that all 
    // paging writes have NTFS as the top of the stack. Let's verify valid deviceobject and match with NTFS deviceobject
    // This IRP would be verified further to see paging file I/O or not.
    // check to see if it's the system paging file
    if ((NULL != pDeviceExtension->pagingFile) && (DCF_PAGEFILE_WRITES_IGNORED & pDevContext->ulFlags) && 
        (pDeviceExtension->pagingFile == topIrpStack->FileObject)) {
        PagingIO = true;
    } 
#ifdef VOLUME_NO_REBOOT_SUPPORT
    else if (!pDevContext->IsDevUsingAddDevice && DriverContext.IsDispatchEntryPatched){ //No-reboot handling case
        bool VerifyPagingWrite = false;
        if (DriverContext.IsWindows8AndLater) {

            // Let's find the NTFS device object for this filter device object on first filtered I/O, Assuming this deviceobject 
            // always valid and unmount unlikely to happen. In the worst case driver captures the changes.
            if (NULL == pDevContext->NTFSDevObj) { // Let's get NTFS Device object on first write
                if (topIrpStack->DeviceObject) {
                    PDEVICE_OBJECT pDevObj = topIrpStack->DeviceObject;
                    if ((NULL != pDevObj) && (NULL != pDevObj->DriverObject) && (NULL != pDevObj->DriverObject->DriverExtension)) {
                        if (pDevObj->DriverObject->DriverExtension->ServiceKeyName.Length ==
                            RtlCompareMemory(pDevObj->DriverObject->DriverExtension->ServiceKeyName.Buffer, L"Ntfs", 
                                             pDevObj->DriverObject->DriverExtension->ServiceKeyName.Length))
                            pDevContext->NTFSDevObj = pDevObj; 
                    }
               } 
            }
            if ((NULL != pDevContext->NTFSDevObj) && (pDevContext->NTFSDevObj == topIrpStack->DeviceObject))
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
#ifdef VOLUME_NO_REBOOT_SUPPORT
        // do NOT capture dirty blocks for the paging file, prevents many extra dirty blocks at shutdown
        if(!pDevContext->IsDevUsingAddDevice && DriverContext.IsDispatchEntryPatched) {
            return (*DispatchEntryForWrite)(pDeviceExtension->TargetDeviceObject, Irp);
        } else
            return InMageFltSendToNextDriver(DeviceObject, Irp);
#else
		return InMageFltSendToNextDriver(DeviceObject, Irp);
#endif
    }
#ifdef VOLUME_CLUSTER_SUPPORT
    if (IS_VOLUME_OFFLINE(pDevContext)) {
	
        InVolDbgPrint( DL_ERROR, DM_CLUSTER, 
            ("InMageFltWrite: Device = %p Write Irp with ByteOffset %I64X and Length 0x%x on OFFLINE cluster volume\n",
            DeviceObject, currentIrpStack->Parameters.Write.ByteOffset.QuadPart, currentIrpStack->Parameters.Write.Length));


// Above Win2k (Win2k is 0x0500, WinXP is 0x0501, Server 2003 is 0x0502)
        if (APC_LEVEL >= KeGetCurrentIrql()) {
            // Save the change to bitmap
            if ((pDevContext->pDevBitmap) && (pDevContext->pDevBitmap->pBitmapAPI)) {
                if (0 == (pDevContext->ulFlags & DCF_CLUSDSK_RETURNED_OFFLINE)) {
                    WRITE_META_DATA WriteMetaData;

                    WriteMetaData.llOffset = currentIrpStack->Parameters.Write.ByteOffset.QuadPart;
                    WriteMetaData.ulLength = currentIrpStack->Parameters.Write.Length;
                    Status = pDevContext->pDevBitmap->pBitmapAPI->SaveWriteMetaDataToBitmap(&WriteMetaData);
                    if (NT_SUCCESS(Status)) {
                        InVolDbgPrint(DL_INFO, DM_CLUSTER | DM_BITMAP_WRITE,
                            ("InMageFltWrite: SaveWriteMetaData succeeded\n"));
                    } else {
                        if (STATUS_DEVICE_OFF_LINE == Status)
                            pDevContext->ulFlags |= DCF_CLUSDSK_RETURNED_OFFLINE;

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
#ifdef VOLUME_NO_REBOOT_SUPPORT
        //if(IsDispatchEntryPatched) {
        if(!pDevContext->IsDevUsingAddDevice && DriverContext.IsDispatchEntryPatched) {

            COMPLETION_CONTEXT  *pCompRoutine = NULL;
            pCompRoutine = (PCOMPLETION_CONTEXT)ExAllocatePoolWithTag(NonPagedPool,
                                                                      sizeof(COMPLETION_CONTEXT),
                                                                      TAG_GENERIC_NON_PAGED);
            if (pCompRoutine) {
                pCompRoutine->eContextType = ecCompletionContext;
                pCompRoutine->DevContext = pDevContext;
                
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
#endif
            IoCopyCurrentIrpStackLocationToNext(Irp);
            IoSetCompletionRoutine(Irp,
                            InMageFltWriteOnOfflineVolumeCompletion,
                            pDevContext,
                            TRUE,
                            TRUE,
                            TRUE);
            return IoCallDriver(pDeviceExtension->TargetDeviceObject, Irp);
#ifdef VOLUME_NO_REBOOT_SUPPORT
        }
#endif
    }
#endif
    // validate the write parameters, prevents other areas from having problems
    if (pDevContext->llDevSize > 0) {
        if ((currentIrpStack->Parameters.Write.ByteOffset.QuadPart >= pDevContext->llDevSize) ||
            (currentIrpStack->Parameters.Write.ByteOffset.QuadPart < 0) ||
            ((currentIrpStack->Parameters.Write.ByteOffset.QuadPart + currentIrpStack->Parameters.Write.Length) > pDevContext->llDevSize)) 
        {
            InVolDbgPrint(DL_ERROR, DM_WRITE, ("InMageFltWrite: Device = %#p  Write Irp with ByteOffset %I64X outside volume range\n", 
                DeviceObject, currentIrpStack->Parameters.Write.ByteOffset.QuadPart));
			QueueWorkerRoutineForSetDevOutOfSync(pDevContext, (ULONG)INFLTDRV_ERR_DEV_WRITE_PAST_EOD, STATUS_NOT_SUPPORTED, false);
            InMageFltLogError(DeviceObject,__LINE__, INFLTDRV_ERR_DEV_WRITE_PAST_EOD1, STATUS_SUCCESS, 
				    pDevContext, (ULONGLONG)pDevContext->llDevSize);
#ifdef VOLUME_NO_REBOOT_SUPPORT
            //if(IsDispatchEntryPatched) {
            if(!pDevContext->IsDevUsingAddDevice && DriverContext.IsDispatchEntryPatched) {
                return (*DispatchEntryForWrite)(pDeviceExtension->TargetDeviceObject, Irp);                    
            } else
                return InMageFltSendToNextDriver(DeviceObject, Irp);
#else
			return InMageFltSendToNextDriver(DeviceObject, Irp);
#endif
            
        }
    }

    if (0 == currentIrpStack->Parameters.Write.Length) {
        InVolDbgPrint(DL_INFO, DM_WRITE, ("InMageFltWrite: Device = %#p Write Irp with length 0 and Offset %#I64x. Skipping...\n",
                                          DeviceObject, currentIrpStack->Parameters.Write.ByteOffset.QuadPart));
#ifdef VOLUME_NO_REBOOT_SUPPORT
        //if(IsDispatchEntryPatched) 
        if(!pDevContext->IsDevUsingAddDevice && DriverContext.IsDispatchEntryPatched)
            return (*DispatchEntryForWrite)(pDeviceExtension->TargetDeviceObject, Irp);
        else
            return InMageFltSendToNextDriver(DeviceObject, Irp);
#else
		return InMageFltSendToNextDriver(DeviceObject, Irp);
#endif
            
    }

    bool bIrpProcessed = false;

    if (ecCaptureModeData != pDevContext->eCaptureMode) {
        WriteDispatchInMetaDataMode(DeviceObject, Irp); 
        bIrpProcessed = true;
    }
    
    if (bIrpProcessed) { 
#ifdef VOLUME_NO_REBOOT_SUPPORT
        //if ( IsDispatchEntryPatched ) {
        if(!pDevContext->IsDevUsingAddDevice && DriverContext.IsDispatchEntryPatched) {
            return (*DispatchEntryForWrite)(pDeviceExtension->TargetDeviceObject, Irp);
        } else                         
            Status = IoCallDriver(pDeviceExtension->TargetDeviceObject, Irp);
#else
		Status = IoCallDriver(pDeviceExtension->TargetDeviceObject, Irp);
#endif
        
    } else {
        Status = WriteDispatchInDataFilteringMode(DeviceObject, Irp);
    }

    return Status;
} // end InMageFltWrite()

PDEV_CONTEXT
GetDevContextForThisIOCTL(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{                                  
    PDEV_CONTEXT     pDevContext = NULL;
#ifdef VOLUME_NO_REBOOT_SUPPORT
    if(Irp != NULL) {
#endif
        PDEVICE_EXTENSION   DeviceExtension;
        PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
            
        ULONG               ulGUIDLength;
                                      
        DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
        ASSERT(NULL != DeviceExtension);
        if (NULL == DeviceExtension->TargetDeviceObject) {
            // The IOCTL has come on control device object. So we need to find the volume GUID required to clear the bitmap.
            ulGUIDLength = sizeof(WCHAR) * GUID_SIZE_IN_CHARS;
            if (ulGUIDLength <= IoStackLocation->Parameters.DeviceIoControl.InputBufferLength) {
                pDevContext = GetDevContextWithGUID((PWCHAR)Irp->AssociatedIrp.SystemBuffer);
            }
        } else {
            pDevContext = DeviceExtension->pDevContext;
            ASSERT(NULL != pDevContext);
            ReferenceDevContext(pDevContext);
        }        
#ifdef VOLUME_NO_REBOOT_SUPPORT
    } else {
        
        PLIST_ENTRY         pCurr = DriverContext.HeadForDevContext.Flink;
        KIRQL               OldIrql; 

        KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
        while (pCurr != &DriverContext.HeadForDevContext) {
            pDevContext = (PDEV_CONTEXT)pCurr;
            if(pDevContext->TargetDeviceObject == DeviceObject) {
                break;
            }
            pCurr = pCurr->Flink;
            pDevContext = NULL;
        }

        KeReleaseSpinLock(&DriverContext.Lock, OldIrql);
    }
#endif
    return pDevContext;
}

NTSTATUS StopFiltering(PDEV_CONTEXT  pDevContext, bool bDeleteBitmapFile) {
    NTSTATUS            Status = STATUS_SUCCESS;
    KIRQL               OldIrql;
    bool                bChanged = false;
    PDEV_BITMAP         pDevBitmap = NULL;
    bool                bFilteringAlreadyStopped = false;
	bool                bBitmapOpened = false;
    
    
    if (NULL == pDevContext) {
        Status = STATUS_UNSUCCESSFUL;
        return Status;
    }
    // This IOCTL should be treated as nop if already volume is stopped from filtering.
    // no registry writes should happen in nop condition
    KeWaitForMutexObject(&pDevContext->BitmapOpenCloseMutex, Executive, KernelMode, FALSE, NULL);
    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
    if (0 == (pDevContext->ulFlags & DCF_FILTERING_STOPPED)) {
        if (bDeleteBitmapFile) {
            StopFilteringDevice(pDevContext, true, false, true, &pDevBitmap);
        } else {
            StopFilteringDevice(pDevContext, true, false, false, &pDevBitmap);
        }
        bChanged = true;
    } else if (bDeleteBitmapFile) {
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
#ifdef VOLUME_CLUSTER_SUPPORT
    if ((IS_CLUSTER_VOLUME(pDevContext)) && (!bDeleteBitmapFile) && (NULL == pDevContext->pDevBitmap)) {
        pDevContext->ulFlags |= DCF_SAVE_FILTERING_STATE_TO_BITMAP;
        if (false == bChanged)
            bChanged = true;
    }
#endif
    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);

    if (bChanged) {
        if (NULL != pDevBitmap) {
            if (bDeleteBitmapFile) {
                KeQuerySystemTime(&pDevContext->liDeleteBitmapTimeStamp);
                CloseBitmapFile(pDevBitmap, true, pDevContext, cleanShutdown, true);
            } else {
#ifdef VOLUME_CLUSTER_SUPPORT
                if (IS_CLUSTER_VOLUME(pDevContext)) {
                    pDevContext->pDevBitmap->pBitmapAPI->SaveFilteringStateToBitmap(true, true, true);
                }
#endif
                ClearBitmapFile(pDevBitmap);
            }
#ifdef VOLUME_CLUSTER_SUPPORT
        } else {
            if (IS_CLUSTER_VOLUME(pDevContext) && (!bDeleteBitmapFile)) {
                pDevBitmap = OpenBitmapFile(pDevContext, Status);
                if (pDevBitmap) {
                    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
                    pDevContext->pDevBitmap = ReferenceDevBitmap(pDevBitmap);
                    if(pDevContext->bQueueChangesToTempQueue) {
                        MoveUnupdatedDirtyBlocks(pDevContext);
                    }
                    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
                    pDevContext->pDevBitmap->pBitmapAPI->SaveFilteringStateToBitmap(true, true, true);
                    bBitmapOpened = true;
                } else {
                    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
                    SetBitmapOpenError(pDevContext, true, Status);
                    pDevContext->ulFlags |= DCF_OPEN_BITMAP_REQUESTED;
                    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
                    KeSetEvent(&DriverContext.ServiceThreadWakeEvent, IO_DISK_INCREMENT, FALSE);
                }
                Status = STATUS_SUCCESS;
            }
#endif
        }
#ifdef VOLUME_CLUSTER_SUPPORT
        if (!bFilteringAlreadyStopped && IS_NOT_CLUSTER_VOLUME(pDevContext))
#else
		if (!bFilteringAlreadyStopped)
#endif
        {
            SetDWORDInRegVolumeCfg(pDevContext, DEVICE_FILTERING_DISABLED, DEVICE_FILTERING_DISABLED_SET, 1);
        }
    } else {
        InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                        ("DeviceIoControlStopFiltering: Filtering already disabled on device %S(%S)\n",
                            pDevContext->wDevID, pDevContext->wDevNum));
    }
    KeReleaseMutex(&pDevContext->BitmapOpenCloseMutex, FALSE);
    if (bBitmapOpened) {
        DereferenceDevBitmap(pDevBitmap);
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
    PDEV_CONTEXT     pDevContext = NULL;
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
        pDevContext = GetDevContextForThisIOCTL(DeviceObject, Irp);
        if (NULL == pDevContext) {
            FinalStatus = STATUS_NO_SUCH_DEVICE;
            goto Cleanup;
        }
    } else {
        Status = GetListOfDevs(&VolumeNodeList);
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
    while ((NULL != pDevContext) || (!IsListEmpty(&VolumeNodeList))) {
        if (NULL == pDevContext) {
            PLIST_NODE      pNode;
            pNode = (PLIST_NODE)RemoveHeadList(&VolumeNodeList);
	        if (NULL != pNode) {
                pDevContext = (PDEV_CONTEXT)pNode->pData;
                DereferenceListNode(pNode);
            }
        }
        Status = StopFiltering(pDevContext,  bDeleteBitmapFile);
	if (NULL != pDevContext) {
            DereferenceDevContext(pDevContext);
        }
        if ((!NT_SUCCESS(Status)) && (NT_SUCCESS(FinalStatus))) {
            FinalStatus = Status;
        }
        pDevContext = NULL;
    }
Cleanup:
    Irp->IoStatus.Status = FinalStatus;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return FinalStatus;
}


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
DeviceIoControlStartFiltering(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status = STATUS_SUCCESS;
    KIRQL               OldIrql;
    PDEV_CONTEXT     pDevContext = NULL;
#if DBG
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
#endif
    bool                bChanged = false;

    // This IOCTL should be treated as nop if already volume is actively filtered.
    // no registry writes should happen in nop condition
    ASSERT(IOCTL_INMAGE_START_FILTERING_DEVICE == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

    pDevContext = GetDevContextForThisIOCTL(DeviceObject, Irp);
    if (NULL == pDevContext) {
        Status = STATUS_UNSUCCESSFUL;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

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
#ifdef VOLUME_CLUSTER_SUPPORT
    if (IS_CLUSTER_VOLUME(pDevContext) && (NULL == pDevContext->pDevBitmap)) {
        pDevContext->ulFlags |= DCF_SAVE_FILTERING_STATE_TO_BITMAP;

        InMageFltLogError(pDevContext->FilterDeviceObject, __LINE__,
                INFLTDRV_START_FILTERING_BEFORE_BITMAP_OPEN, STATUS_SUCCESS,
                pDevContext, (ULONGLONG)pDevContext->liStartFilteringTimeStampByUser.QuadPart);

        if (false == bChanged)
            bChanged = true;
    }
#endif
    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
      
    if (bChanged) {
#ifdef _WIN64	    
       /* By default Filtering is started on all cluster volume and bitmap file is opened for reading. 
        * If in bitmap filtering is not configured for volume then filtering is stoped. 
        * Data pool is only allocated in openbitmap if filtering is on for cluster volume. 
        * So if later pair is configured then Data Pool should be allocated as bitmap is not reopen */
 
        SetDataPool(1, DriverContext.ullDataPoolSize);

#endif	    
#ifdef VOLUME_CLUSTER_SUPPORT
        if (IS_CLUSTER_VOLUME(pDevContext)) {
            PDEV_BITMAP   pDevBitmap = NULL;

            if (NULL == pDevContext->pDevBitmap) {
                pDevBitmap = OpenBitmapFile(pDevContext, Status);
                if (pDevBitmap) {
                    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);  
                    pDevContext->pDevBitmap = ReferenceDevBitmap(pDevBitmap);
                    if (pDevContext->bQueueChangesToTempQueue) {
                        MoveUnupdatedDirtyBlocks(pDevContext);
                    }
                    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
                } else {
                    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);  
                    SetBitmapOpenError(pDevContext, true, Status );
                    pDevContext->ulFlags |= DCF_OPEN_BITMAP_REQUESTED;
                    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
                    KeSetEvent(&DriverContext.ServiceThreadWakeEvent, IO_DISK_INCREMENT, FALSE);
                }
            } else {
                KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
                if (NULL != pDevContext->pDevBitmap) {
            	    pDevBitmap = ReferenceDevBitmap(pDevContext->pDevBitmap);
                }
            	KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
            	if (NULL != pDevBitmap) {
                    ExAcquireFastMutex(&pDevBitmap->Mutex);                
                    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
                    if (ecVBitmapStateReadCompleted == pDevBitmap->eVBitmapState) {
                        pDevBitmap->eVBitmapState = ecVBitmapStateOpened;
                    }
                    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
                    ExReleaseFastMutex(&pDevBitmap->Mutex);
                }
            }

            if (pDevContext->pDevBitmap) {
                pDevContext->pDevBitmap->pBitmapAPI->SaveFilteringStateToBitmap(false, false, true);
            }

            if (NULL != pDevBitmap) {
                DereferenceDevBitmap(pDevBitmap);
            }
        } else {
#endif
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
            if (NULL != pDevBitmap){
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
            if (NULL != pDevBitmap){
                ExReleaseFastMutex(&pDevBitmap->Mutex);
                DereferenceDevBitmap(pDevBitmap);
            }
            KeSetEvent(&DriverContext.ServiceThreadWakeEvent, IO_DISK_INCREMENT, FALSE);            
#ifdef VOLUME_CLUSTER_SUPPORT
        }
#endif
    } else {
        InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                        ("DeviceIoControlStartFiltering: Filtering already enabled on device %S(%S)\n",
                            pDevContext->wDevID, pDevContext->wDevNum));
    }

    DereferenceDevContext(pDevContext);

    Status = STATUS_SUCCESS;
    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = 0;
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
            if (ulInputFlags & VOLUME_FLAG_IGNORE_PAGEFILE_WRITES) {
                if (0 == (pDevContext->ulFlags & DCF_PAGEFILE_WRITES_IGNORED)) {
                    pDevContext->ulFlags |= DCF_PAGEFILE_WRITES_IGNORED;
                } else {
                    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                                  ("DeviceIoControlSetVolumeFlags: Already pagefile write ignored flag is set\n"));
                    ulInputFlags &= ~VOLUME_FLAG_IGNORE_PAGEFILE_WRITES;
                }
            }
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

            if (ulInputFlags & VOLUME_FLAG_READ_ONLY)
            {
                if (0 == (pDevContext->ulFlags & DCF_READ_ONLY)) {
                    if((pDevContext->ulFlags & DCF_FILTERING_STOPPED) || (ulInputFlags & VOLUME_FLAG_ENABLE_SOURCE_ROLLBACK)) {
                        pDevContext->ulFlags |= DCF_READ_ONLY;
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
                if (0 == (pDevContext->ulFlags & DCF_DATA_CAPTURE_DISABLED)) {
                    pDevContext->ulFlags |= DCF_DATA_CAPTURE_DISABLED;
                    VCSetCaptureModeToMetadata(pDevContext, true);
                    VCSetWOState(pDevContext,true,__FUNCTION__);
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
            if (ulInputFlags & VOLUME_FLAG_IGNORE_PAGEFILE_WRITES) {
                if (pDevContext->ulFlags & DCF_PAGEFILE_WRITES_IGNORED) {
                    pDevContext->ulFlags &= ~DCF_PAGEFILE_WRITES_IGNORED;
                } else {
                    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                                  ("DeviceIoControlSetVolumeFlags: Already pagefile write flag is unset\n"));
                    ulInputFlags &= ~VOLUME_FLAG_IGNORE_PAGEFILE_WRITES;
                }
            }
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
            if (ulInputFlags & VOLUME_FLAG_READ_ONLY) {
                if (pDevContext->ulFlags & DCF_READ_ONLY) {
                    pDevContext->ulFlags &= ~DCF_READ_ONLY;
                } else {
                    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                                  ("DeviceIoControlSetVolumeFlags: Already volume read only flag is unset\n"));
                    ulInputFlags &= ~VOLUME_FLAG_READ_ONLY;
                }
            }

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
            if (pDevContext->ulFlags & DCF_PAGEFILE_WRITES_IGNORED)
                pVolumeFlagsOutput->ulVolumeFlags |= VOLUME_FLAG_IGNORE_PAGEFILE_WRITES;
            if (pDevContext->ulFlags & DCF_BITMAP_READ_DISABLED)
                pVolumeFlagsOutput->ulVolumeFlags |= VOLUME_FLAG_DISABLE_BITMAP_READ;

            if (pDevContext->ulFlags & DCF_BITMAP_WRITE_DISABLED)
                pVolumeFlagsOutput->ulVolumeFlags |= VOLUME_FLAG_DISABLE_BITMAP_WRITE;

            if (pDevContext->ulFlags & DCF_READ_ONLY)
                pVolumeFlagsOutput->ulVolumeFlags |= VOLUME_FLAG_READ_ONLY;

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
            if (ulInputFlags & VOLUME_FLAG_IGNORE_PAGEFILE_WRITES)
                SetDWORDInRegVolumeCfg(pDevContext, VOLUME_PAGEFILE_WRITES_IGNORED, VOLUME_PAGEFILE_WRITES_IGNORED_SET);

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
            if (ulInputFlags & VOLUME_FLAG_IGNORE_PAGEFILE_WRITES)
                SetDWORDInRegVolumeCfg(pDevContext, VOLUME_PAGEFILE_WRITES_IGNORED, VOLUME_PAGEFILE_WRITES_IGNORED_UNSET);
            
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
        Status = STATUS_UNSUCCESSFUL;
    }

    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}


NTSTATUS
DeviceIoControlTagVolume(
    PDEVICE_OBJECT DeviceObject, 
    PIRP Irp
    )
{
    NTSTATUS Status                     = STATUS_SUCCESS;
    ULONG ulNumofDevs             = 0;
    ULONG ulTagStartOffset              = 0;
    PUCHAR pBuffer                      = (PUCHAR) Irp->AssociatedIrp.SystemBuffer;
    PIO_STACK_LOCATION IoStackLocation  = IoGetCurrentIrpStackLocation(Irp);
    ULONG IoctlTagVolumeFlags           = 0;
    bool    bCompleteIrp = true;

	UNREFERENCED_PARAMETER(DeviceObject);

    ASSERT(IOCTL_INMAGE_TAG_VOLUME == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);
    
    Irp->IoStatus.Information = 0;

    if(!IsValidIoctlTagDeviceInputBuffer(pBuffer, IoStackLocation->Parameters.DeviceIoControl.InputBufferLength)) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_TAGS, 
            ("DeviceIoControlTagVolume: IOCTL rejected, invalid input buffer \n"));
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    IoctlTagVolumeFlags = *(PULONG)pBuffer;
    ulNumofDevs = *(PULONG)((PCHAR)pBuffer + sizeof(ULONG));

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

    if ((ulNumofDevs == 0) || (ulNumofDevs > DriverContext.ulNumDevs)) {
        InVolDbgPrint(DL_ERROR, DM_INMAGE_IOCTL | DM_TAGS, 
            ("DeviceIoControlTagVolume: IOCTL rejected, invalid number of volumes(%d), ulNumVolumes(%d)\n", 
             ulNumofDevs, DriverContext.ulNumDevs));
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    ULONG   ulSizeOfTagInfo = sizeof(TAG_INFO) + (ulNumofDevs * sizeof(TAG_STATUS));
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
    pTagInfo->ulNumberOfDevices = ulNumofDevs;
    pTagInfo->ulFlags |= TAG_INFO_FLAGS_IGNORE_IF_FILTERING_STOPPED;
    InVolDbgPrint(DL_FUNC_TRACE, DM_TAGS, ("DeviceIoControlTagVolume: Called on %d Volumes\n", ulNumofDevs));
    for (ULONG i = 0; i < ulNumofDevs; i++) {
        pTagInfo->TagStatus[i].DevContext = GetDevContextWithGUID((PWCHAR)(pBuffer + i * GUID_SIZE_IN_BYTES + 2 * sizeof(ULONG)));
        if (NULL != pTagInfo->TagStatus[i].DevContext) {
            pTagInfo->TagStatus[i].Status = STATUS_PENDING;
            InVolDbgPrint(DL_FUNC_TRACE, DM_TAGS, ("\tGUID=%S Volume=%S\n", pTagInfo->TagStatus[i].DevContext->wDevID, pTagInfo->TagStatus[i].DevContext->wDevNum));
        } else {
            pTagInfo->TagStatus[i].Status = STATUS_NOT_FOUND;
			pTagInfo->ulNumberOfDevicesIgnored++;
        }
    }

	if (pTagInfo->ulNumberOfDevicesIgnored && (0 == (IoctlTagVolumeFlags & TAG_VOLUME_INPUT_FLAGS_NO_ERROR_IF_GUID_NOT_FOUND))) {
        Status = STATUS_INVALID_PARAMETER;
    } else {    
        ulTagStartOffset = (2 * sizeof(ULONG)) + (ulNumofDevs * GUID_SIZE_IN_BYTES);
        pTagInfo->pBuffer = (PVOID)(pBuffer + ulTagStartOffset);
        if (0 == (IoctlTagVolumeFlags & TAG_VOLUME_INPUT_FLAGS_DEFER_TILL_COMMIT_SNAPSHOT)) {
            if (IoctlTagVolumeFlags & TAG_VOLUME_INPUT_FLAGS_ATOMIC_TO_VOLUME_GROUP) {
                Status = TagVolumesAtomically(pTagInfo, pBuffer + ulTagStartOffset);
            } else {
                Status = TagVolumesInSequence(pTagInfo, pBuffer + ulTagStartOffset);
            }
        } else {            

            Status = AddTagNodeToList(pTagInfo, Irp, ecTagStateWaitForCommitSnapshot);
            if (STATUS_SUCCESS == Status)
                bCompleteIrp = false;

        }
    }

    if(bCompleteIrp){
        for (ULONG i = 0; i < ulNumofDevs; i++) {
        if (NULL != pTagInfo->TagStatus[i].DevContext) {
            DereferenceDevContext(pTagInfo->TagStatus[i].DevContext);
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
    case IOCTL_INMAGE_GET_VOLUME_WRITE_ORDER_STATE:
        Status = DeviceIoControlGetVolumeWriteOrderState(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_GET_VOLUME_SIZE:
		Status = DeviceIoControlGetDeviceTrackingSize(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_GET_VOLUME_FLAGS:
        Status = DeviceIoControlGetVolumeFlags(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_GET_VOLUME_STATS:
        Status = DeviceIoControlGetVolumeStats(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_GET_ADDITIONAL_VOLUME_STATS:
        Status = DeviceIoControlGetAdditionalDeviceStats(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_GET_DATA_MODE_INFO:
        Status = DeviceIoControlGetDMinfo(DeviceObject, Irp);
        break;
#ifdef VOLUME_CLUSTER_SUPPORT
    case IOCTL_INMAGE_MSCS_DISK_REMOVED_FROM_CLUSTER:
        Status = DeviceIoControlMSCSDiskRemovedFromCluster(DeviceObject, Irp);
        break;
//        case IOCTL_INMAGE_MSCS_DISK_ADDED_TO_CLUSTER:
//            Status = DeviceIoControlMSCSDiskAddedToCluster(DeviceObject, Irp);
//            break;
    case IOCTL_INMAGE_MSCS_DISK_ACQUIRE:
        Status = DeviceIoControlMSCSDiskAcquire(DeviceObject,Irp);
        break;
    case IOCTL_INMAGE_MSCS_DISK_RELEASE:
        Status = DeviceIoControlMSCSDiskRelease(DeviceObject, Irp);
        break;
    case IOCTL_INMAGE_MSCS_GET_ONLINE_DISKS:
        Status = DeviceIoControlMSCSGetOnlineDisks(DeviceObject, Irp);
        break;
#endif
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
        Status = DeviceIoControlClearDevStats(DeviceObject, Irp);
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
    PDEV_CONTEXT        pDevContext;
    NTSTATUS            Status;
#ifdef VOLUME_NO_REBOOT_SUPPORT
    PDRIVER_DISPATCH     DispatchEntryForDevIOControl = NULL;
#endif
    if (DeviceObject == DriverContext.ControlDevice)
    {
        return ProcessLocalDeviceControlRequests(DeviceObject, Irp);
    }

    ulIoControlCode = IoStackLocation->Parameters.DeviceIoControl.IoControlCode;

#ifdef VOLUME_NO_REBOOT_SUPPORT
     if(DriverContext.IsDispatchEntryPatched) {
        pDevContext = GetDevContextForThisIOCTL(DeviceObject, NULL);
        if(pDevContext) {       
            DeviceObject = pDevContext->FilterDeviceObject;
            DeviceExtension = (PDEVICE_EXTENSION)pDevContext->FilterDeviceObject->DeviceExtension;
#ifdef VOLUME_CLUSTER_SUPPORT
            if(pDevContext->IsVolumeCluster) {
                DispatchEntryForDevIOControl = DriverContext.MajorFunctionForClusDrives[IRP_MJ_DEVICE_CONTROL];
            } else {
#endif
                DispatchEntryForDevIOControl = DriverContext.MajorFunction[IRP_MJ_DEVICE_CONTROL];
#ifdef VOLUME_CLUSTER_SUPPORT
            }
#endif
            
            if(pDevContext->IsDevUsingAddDevice)
                return (*DispatchEntryForDevIOControl)(DeviceExtension->TargetDeviceObject, Irp);

        } else {
            if(DeviceObject->DriverObject == DriverContext.DriverObject) {
                
                DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
                pDevContext = DeviceExtension->pDevContext;

                // Bug Fix: 14969: System stop responding with NoReboot Driver
                if (pDevContext->IsDevUsingAddDevice != true) {
                    IoSkipCurrentIrpStackLocation(Irp);
                    return IoCallDriver(DeviceExtension->TargetDeviceObject, Irp);
                }
  
                if(pDevContext){
#ifdef VOLUME_CLUSTER_SUPPORT

                    if(pDevContext->IsVolumeCluster) {
                        DispatchEntryForDevIOControl = DriverContext.MajorFunctionForClusDrives[IRP_MJ_DEVICE_CONTROL];
                    } else {
#endif
                        DispatchEntryForDevIOControl = DriverContext.MajorFunction[IRP_MJ_DEVICE_CONTROL];
#ifdef VOLUME_CLUSTER_SUPPORT
                    }
#endif
                }

                
            } 
#ifdef VOLUME_CLUSTER_SUPPORT
	     else if (DriverContext.pDriverObject == DeviceObject->DriverObject ) {
                
                return (*DriverContext.MajorFunction[IRP_MJ_DEVICE_CONTROL])(DeviceObject, Irp);      
                
            } else
                return (*DriverContext.MajorFunctionForClusDrives[IRP_MJ_DEVICE_CONTROL])(DeviceObject, Irp);   
#else
		  return (*DriverContext.MajorFunction[IRP_MJ_DEVICE_CONTROL])(DeviceObject, Irp);      
#endif
        }
    } else {
#endif
        DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
#ifdef VOLUME_NO_REBOOT_SUPPORT
    }
#endif
#ifdef INMAGE_DBG
    //DumpIoctlInformation(DeviceObject, Irp);
#endif
    ulDeviceType = IOCTL_DEVICE_TYPE(ulIoControlCode);
    
    
    pDevContext = DeviceExtension->pDevContext;

    InVolDbgPrint(DL_FUNC_TRACE, DM_INMAGE_IOCTL, ("InMageDeviceControl: Process = %.16s(%x.%x) DeviceObject = %#p  Irp = %#p  CtlCode = %#x\n",
                        PsGetProcessImageFileName(PsGetCurrentProcess()), PtrToUlong(PsGetCurrentProcessId()), PtrToUlong(PsGetCurrentThreadId()), DeviceObject, Irp, ulIoControlCode));

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
#ifdef VOLUME_NO_REBOOT_SUPPORT
        if( !pDevContext->IsDevUsingAddDevice && DriverContext.IsDispatchEntryPatched) {    
            Status = (*DispatchEntryForDevIOControl)(DeviceExtension->TargetDeviceObject, Irp);
        } else {
#endif
            Status = DeviceIoControlMountDevQueryUniqueID(DeviceObject, Irp);
#ifdef VOLUME_NO_REBOOT_SUPPORT
        }
#endif
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
    case IOCTL_VOLUME_OFFLINE:
        Status = DeviceIoControlVolumeOffline(DeviceObject, Irp);
        break;
    case IOCTL_VOLUME_ONLINE:
        Status = DeviceIoControlVolumeOnline(DeviceObject, Irp);
        break;
    case IOCTL_VOLSNAP_COMMIT_SNAPSHOT:
        Status = DeviceIoControlCommitVolumeSnapshot(DeviceObject, Irp);
        break;
#if DBG
//    case IOCTL_INMAGE_ADD_DIRTY_BLOCKS_TEST:
//        Status = DeviceIoControlAddDirtyBlocksTest(DeviceObject, Irp);
//        break;
#endif // DBG
    default:
#ifdef VOLUME_NO_REBOOT_SUPPORT
        if (!pDevContext->IsDevUsingAddDevice && DriverContext.IsDispatchEntryPatched) {
            if ((!(pDevContext->ulFlags & DCF_FILTERING_STOPPED))  && ulDeviceType == IOCTL_DISK_BASE
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
                    ReferenceDevContext(pDevContext);
                    pCompRoutine->eContextType = ecCompletionContext;
                    pCompRoutine->DevContext = pDevContext;
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
                    QueueWorkerRoutineForSetDevOutOfSync(pDevContext, (ULONG)INFLTDRV_ERR_NO_GENERIC_NPAGED_POOL, STATUS_NO_MEMORY, false);            
                }
                Status = (*DispatchEntryForDevIOControl)(DeviceExtension->TargetDeviceObject, Irp) ;
            } else {
                Status = (*DispatchEntryForDevIOControl)(DeviceExtension->TargetDeviceObject, Irp);
            }
        } else {
#endif
            if ((!(pDevContext->ulFlags & DCF_FILTERING_STOPPED)) && ulDeviceType == IOCTL_DISK_BASE
                    && ulIoControlCode == IOCTL_DISK_COPY_DATA) {
                ReferenceDevContext(pDevContext);
                IoCopyCurrentIrpStackLocationToNext(Irp);
                IoSetCompletionRoutine(Irp, DeviceControlCompletionRoutine, pDevContext, TRUE, TRUE, TRUE);
                Status = IoCallDriver(DeviceExtension->TargetDeviceObject, Irp);
            } else {
                IoSkipCurrentIrpStackLocation(Irp);
                Status = IoCallDriver(DeviceExtension->TargetDeviceObject, Irp);
            }
#ifdef VOLUME_NO_REBOOT_SUPPORT
        }
#endif
        
        break;
    }

    return Status;

} // end InMageDeviceControl()

NTSTATUS
InMageFltShutdown(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    PDEVICE_EXTENSION   pDeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PDEV_CONTEXT     pDevContext = NULL;

    InVolDbgPrint(DL_INFO, DM_SHUTDOWN, 
        ("InMageFltShutdown: DeviceObject %#p Irp %#p\n", DeviceObject, Irp));
    
    if ((DeviceObject == DriverContext.ControlDevice) || (pDeviceExtension->shutdownSeen)) {
        if (DeviceObject == DriverContext.ControlDevice) {
            InVolDbgPrint(DL_INFO, DM_SHUTDOWN, 
                ("InMageFltShutdown: Shutdown IRP received on control device\n"));
            // as we may be about to shutdown, prepare low level code for physical I/O
            pDevContext = (PDEV_CONTEXT)DriverContext.HeadForDevContext.Flink;
            while (pDevContext != (PDEV_CONTEXT)(&DriverContext.HeadForDevContext)) {
                // try to save all data now, while we are in early shutdown
                if (!(pDevContext->ulFlags & DCF_FILTERING_STOPPED)) {
#ifdef VOLUME_CLUSTER_SUPPORT
                    if (IS_CLUSTER_VOLUME(pDevContext)) {
                        // For cluster volumes, bitmap is saved when the disk is released.
                        // bitmap file is not closed or delted as Win2K sees some writes after release
                        // and we do all the attempt to save that changes to bitmap file.
                        // by the time shutdown has reached, file system is unmounted 
                        // and volume is taken offline.
#if (NTDDI_VERSION >= NTDDI_VISTA)

                        
                        if((0 == (pDevContext->ulFlags & DCF_CV_FS_UNMOUTNED))||
                            ((pDevContext->ulFlags & DCF_CV_FS_UNMOUTNED) && (pDevContext->ulFlags & DCF_CLUSTER_VOLUME_ONLINE))){
                              InMageFltSaveAllChanges(pDevContext, TRUE, NULL);
                              if(pDevContext->pDevBitmap && pDevContext->pDevBitmap->pBitmapAPI) {
                                  BitmapPersistentValues BitmapPersistentValue(pDevContext);
                                  pDevContext->pDevBitmap->pBitmapAPI->ChangeBitmapModeToRawIO();
                                  pDevContext->pDevBitmap->pBitmapAPI->CommitBitmap(cleanShutdown, BitmapPersistentValue, NULL);
                                
                            }
                        }
#endif
                        InVolDbgPrint(DL_INFO, DM_SHUTDOWN, 
                            ("InMageFltShutdown: Cluster volume, pDevContext = %#p, Drive = %S\n", pDevContext, pDevContext->wDevNum));

                        //ASSERT((0 == pDevContext->ChangeList.ulTotalChangesPending) &&
                        //    (0 == pDevContext->ChangeList.ullcbTotalChangesPending) &&
                        //        (0 == pDevContext->ulChangesPendingCommit));

                        if (pDevContext->ChangeList.ulTotalChangesPending) {
                            InVolDbgPrint( DL_ERROR, DM_SHUTDOWN | DM_CLUSTER, 
                                ("Volume %S DiskSig = 0x%x has ChangesPending = 0x%x ChangesPendingCommit = 0x%x\n",
                                        pDevContext->wDevNum, pDevContext->ulDiskSignature, 
                                        pDevContext->ChangeList.ulTotalChangesPending, pDevContext->ulChangesPendingCommit));
                        }
                    } else {
#endif
                        // This volume is not a cluster volume.
                        InVolDbgPrint(DL_INFO, DM_SHUTDOWN, 
                            ("InMageFltShutdown: pDevContext = %#p, Flushing final volume changes for Drive %S\n",
                            pDevContext, pDevContext->wDevNum));
                        InMageFltSaveAllChanges(pDevContext, TRUE, NULL);
                        // Persist the Last TimeStamp/Sequence Number/Continuation Id for non cluster Volume
                        PersistFDContextFields(pDevContext);
                        if (pDevContext->pDevBitmap && pDevContext->pDevBitmap->pBitmapAPI) {
                            pDevContext->pDevBitmap->pBitmapAPI->ChangeBitmapModeToRawIO();
                        }
#ifdef VOLUME_CLUSTER_SUPPORT
                    }
#endif
                }
                pDevContext = (PDEV_CONTEXT)(pDevContext->ListEntry.Flink);
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

    pDevContext = pDeviceExtension->pDevContext;


    if ((NULL != pDevContext) && (!(pDevContext->ulFlags & DCF_FILTERING_STOPPED))) {
#ifdef VOLUME_CLUSTER_SUPPORT
        if (IS_CLUSTER_VOLUME(pDevContext)) {
            // For cluster volumes, bitmap is saved when the disk is released.
            // bitmap file is not closed or delted as Win2K sees some writes after release
            // and we do all the attempt to save that changes to bitmap file.
            // by the time shutdown has reached, file system is unmounted 
            // and volume is taken offline.

            InVolDbgPrint(DL_INFO, DM_SHUTDOWN, 
                ("InMageFltShutdown: Cluster volume, pDevContext = %#p, Drive = %S\n", pDevContext, pDevContext->wDevNum));

            ASSERT((0 == pDevContext->ChangeList.ulTotalChangesPending) && (0 == pDevContext->ChangeList.ullcbTotalChangesPending));

            if (pDevContext->ChangeList.ulTotalChangesPending) {
                InVolDbgPrint( DL_ERROR, DM_SHUTDOWN | DM_CLUSTER, 
                    ("Volume %S DiskSig = 0x%x has ChangesPending = 0x%x ChangesPendingCommit = 0x%x\n",
                            pDevContext->wDevNum, pDevContext->ulDiskSignature, 
                            pDevContext->ChangeList.ulTotalChangesPending, pDevContext->ulChangesPendingCommit));
            }
        } else {
#endif
            // This volume is not a cluster volume.
            InVolDbgPrint(DL_INFO, DM_SHUTDOWN,
                ("InMageFltShutdown: Device = %S ulTotalChangesPending = %#x eServiceState = %i\n",   
                pDevContext->wDevNum, pDevContext->ChangeList.ulTotalChangesPending,
                DriverContext.eServiceState));

            if (pDevContext->pDevBitmap) {
                InVolDbgPrint(DL_INFO, DM_SHUTDOWN, 
                    ("InMageFltShutdown: pDevContext = %#p, Flushing final volume changes for Drive %S\n",
                    pDevContext, pDevContext->wDevNum));
                FlushAndCloseBitmapFile(pDevContext);
            }
#ifdef VOLUME_CLUSTER_SUPPORT
        }
#endif
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
    PDEV_CONTEXT   pDevContext = NULL;
#ifdef VOLUME_NO_REBOOT_SUPPORT
    PDRIVER_DISPATCH     DispatchEntryForFlush = NULL;
#endif

    InVolDbgPrint(DL_FUNC_TRACE, DM_FLUSH,
        ("InMageFltFlush: DeviceObject %#p Irp %#p\n", DeviceObject, Irp));
#ifdef VOLUME_NO_REBOOT_SUPPORT
    if(DriverContext.IsDispatchEntryPatched) {
        pDevContext = GetDevContextForThisIOCTL(DeviceObject, NULL);
        if(pDevContext) {       
            DeviceObject = pDevContext->FilterDeviceObject;
            pDeviceExtension = (PDEVICE_EXTENSION)pDevContext->FilterDeviceObject->DeviceExtension;
#ifdef VOLUME_CLUSTER_SUPPORT
            if(pDevContext->IsVolumeCluster) {
                DispatchEntryForFlush = DriverContext.MajorFunctionForClusDrives[IRP_MJ_FLUSH_BUFFERS];
            } else {
#endif
                DispatchEntryForFlush = DriverContext.MajorFunction[IRP_MJ_FLUSH_BUFFERS];
#ifdef VOLUME_CLUSTER_SUPPORT
            }
#endif
            
            if(pDevContext->IsDevUsingAddDevice)
                return (*DispatchEntryForFlush)(pDeviceExtension->TargetDeviceObject, Irp);

        } else {
            if(DeviceObject->DriverObject == DriverContext.DriverObject) {
                
                pDeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
                pDevContext = pDeviceExtension->pDevContext;

                // Bug Fix: 14969: system stops responding with NoReboot Driver
                if (pDevContext->IsDevUsingAddDevice != true) {
                    IoSkipCurrentIrpStackLocation(Irp);
                    return IoCallDriver(pDeviceExtension->TargetDeviceObject, Irp);
                }

                if(pDevContext){
#ifdef VOLUME_CLUSTER_SUPPORT
                    if(pDevContext->IsVolumeCluster) {
                        DispatchEntryForFlush = DriverContext.MajorFunctionForClusDrives[IRP_MJ_FLUSH_BUFFERS];
                    } else {
#endif
                        DispatchEntryForFlush = DriverContext.MajorFunction[IRP_MJ_FLUSH_BUFFERS];
#ifdef VOLUME_CLUSTER_SUPPORT
                    }
#endif
                }
            }
#ifdef VOLUME_CLUSTER_SUPPORT
	     else if (DriverContext.pDriverObject == DeviceObject->DriverObject ) {
                return (*DriverContext.MajorFunction[IRP_MJ_FLUSH_BUFFERS])(DeviceObject, Irp);                    
            } else
                return (*DriverContext.MajorFunctionForClusDrives[IRP_MJ_FLUSH_BUFFERS])(DeviceObject, Irp);   
#else 
		  return (*DriverContext.MajorFunction[IRP_MJ_FLUSH_BUFFERS])(DeviceObject, Irp);                    
#endif
        }
    } else {
#endif
        pDeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
#ifdef VOLUME_NO_REBOOT_SUPPORT
    }
#endif
    if (DeviceObject == DriverContext.ControlDevice) {
        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }


    pDevContext = pDeviceExtension->pDevContext;

    if (NULL != pDevContext) {
        KIRQL       OldIrql;
        KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
        pDevContext->ulFlushCount++;
        KeQuerySystemTime(&pDevContext->liLastFlushTime);
        KeReleaseSpinLock(&pDevContext->Lock, OldIrql);

        InVolDbgPrint(DL_INFO, DM_FLUSH,
            ("InMageFltFlush: Device = %S(%S) FlushCount = %#x, ulChangesPending %#x ulChangesPendingCommit %#x\n",
            pDevContext->wDevID, pDevContext->wDevNum, pDevContext->ulFlushCount, 
             pDevContext->ChangeList.ulTotalChangesPending, pDevContext->ulChangesPendingCommit));

        if ((!(pDevContext->ulFlags & DCF_FILTERING_STOPPED)) &&
            (ecServiceShutdown == DriverContext.eServiceState))
        {
            InMageFltSaveAllChanges(pDevContext, FALSE, NULL);
        }
    }

    //
    // Set current stack back one.
    //
#ifdef VOLUME_NO_REBOOT_SUPPORT
    if(!pDevContext->IsDevUsingAddDevice && DriverContext.IsDispatchEntryPatched) {
        return (*DispatchEntryForFlush)(pDeviceExtension->TargetDeviceObject, Irp);
    } else {
#endif
        IoSkipCurrentIrpStackLocation(Irp);
    
        return IoCallDriver(pDeviceExtension->TargetDeviceObject, Irp);
#ifdef VOLUME_NO_REBOOT_SUPPORT
    }
#endif

} // end InMageFltFlush()

/*
Description: Creates and initializes a new filter device object FiDO for the corresponding PDO. Then it attaches the device object to the device
                  stack of the drivers for the device.
				  
Args			: DriverObject - filter filter driver's Driver Object
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
    PDEV_CONTEXT        pDevContext = NULL;
    KIRQL               OldIrql;

    //
    // Create a filter device object for this device (partition).
    //

    InVolDbgPrint(DL_FUNC_TRACE, DM_DEV_ARRIVAL,
        ("InMageFltAddDevice: Driver = %#p  PDO = %#p\n", DriverObject, PhysicalDeviceObject));


    pDevContext = AllocateDevContext();
    if (NULL == pDevContext)
        return STATUS_INSUFFICIENT_RESOURCES;

    Status = IoCreateDevice(DriverObject,
                            DEVICE_EXTENSION_SIZE,
                            NULL,
                            FILE_DEVICE_DISK,
                            0,
                            FALSE,
                            &FilterDeviceObject);

    if (!NT_SUCCESS(Status)) {
       InVolDbgPrint(DL_ERROR, DM_DEV_ARRIVAL,
           ("InMageFltAddDevice: IoCreateDevice failed with Status %#x - Cannot create filterDeviceObject\n",
           Status));
       DereferenceDevContext(pDevContext);
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
        DereferenceDevContext(pDevContext);
        InVolDbgPrint(DL_ERROR, DM_DEV_ARRIVAL,
            ("InMageFltAddDevice: IoAttachDeviceToDeviceStack failed - Unable to attach %#p to target %#p\n",
            FilterDeviceObject, PhysicalDeviceObject));
        return STATUS_NO_SUCH_DEVICE;
    }

    ReferenceDevContext(pDevContext);
    pDeviceExtension->pDevContext = pDevContext;
    pDeviceExtension->PhysicalDeviceObject = PhysicalDeviceObject;
    pDeviceExtension->DeviceObject = FilterDeviceObject;
    pDevContext->FilterDeviceObject = FilterDeviceObject;
    pDevContext->TargetDeviceObject = pDeviceExtension->TargetDeviceObject;
	KeQuerySystemTime(&pDevContext->liDevContextCreationTS);
    InVolDbgPrint(DL_INFO, DM_DEV_ARRIVAL, 
        ("InMageFltAddDevice: Successfully added PhysicalDO = %#p, FilterDO = %#p, LowerDO = %#p, VC = %#p\n",
        PhysicalDeviceObject, FilterDeviceObject, pDeviceExtension->TargetDeviceObject, pDevContext));
#ifdef VOLUME_NO_REBOOT_SUPPORT
    if(DriverContext.IsVolumeAddedByEnumeration) {
#ifdef VOLUME_CLUSTER_SUPPORT
        if(DriverContext.IsClusterVolume) {
           pDevContext->IsVolumeCluster = true;
        }
#endif
        GetDeviceNumberAndSignature(pDevContext);
#ifdef VOLUME_CLUSTER_SUPPORT
        GetClusterInfoForThisVolume(pDevContext);
        if (!IS_CLUSTER_VOLUME(pDevContext))
            pDevContext->bQueueChangesToTempQueue = false;
#endif
        GetVolumeGuidAndDriveLetter(pDevContext);
        LoadDeviceConfigFromRegistry(pDevContext);
    } else 
        pDevContext->IsDevUsingAddDevice = true;
#endif
    KeInitializeEvent(&pDeviceExtension->PagingPathCountEvent,
                      SynchronizationEvent, TRUE);
    //Fix for Bug 28568
    INITIALIZE_PNP_STATE(pDeviceExtension);
    IoInitializeRemoveLock(&pDeviceExtension->RemoveLock,TAG_REMOVE_LOCK,0,0);
	
	//Fix For Bug 27337
	RtlZeroMemory(&pDevContext->FsInfoWrapper.FsInfo, sizeof(FSINFORMATION));
	pDevContext->FsInfoWrapper.FsInfoFetchRetries = 0;
	pDevContext->FsInfoWrapper.FsInfoNtStatus = STATUS_UNSUCCESSFUL;
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

    //
    // default to DO_POWER_PAGABLE
    //

    FilterDeviceObject->Flags |=  DO_POWER_PAGABLE;

    //
    // Clear the DO_DEVICE_INITIALIZING flag
    //

    FilterDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    // Free the reference of pDevContext
    DereferenceDevContext(pDevContext);

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
    PDEV_CONTEXT        pDevContext = NULL;
    
    PIRP                            tempIrp;
    STORAGE_DEVICE_NUMBER           StDevNum;
    IO_STATUS_BLOCK                 IoStatus;
    BOOLEAN                         bLoopAlwaysFalse = FALSE;

    pDeviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    pDevContext = pDeviceExtension->pDevContext;

    Status = InMageFltForwardIrpSynchronous(DeviceObject, Irp);

    InMageFltSyncFilterWithTarget(DeviceObject,
                                 pDeviceExtension->TargetDeviceObject);

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
           pDevContext->ulDiskNumber = StDevNum.DeviceNumber;
  
       }
    }while(bLoopAlwaysFalse);

    
    InVolDbgPrint(DL_INFO, DM_DRIVER_PNP | DM_DEV_ARRIVAL,
        ("InMageFltStartDevice: Disk Number is %x\n",pDevContext->ulDiskNumber));

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
    PDEV_CONTEXT DevContext = DeviceExtension->pDevContext;


    ASSERT(NULL != DevContext);

    InVolDbgPrint(DL_INFO, DM_SHUTDOWN,
        ("InMageFltSurpriseRemoveDevice: DeviceObject %#p Irp %#p\n",
        DeviceObject, Irp));

    DevContext->ulFlags |= DCF_SURPRISE_REMOVAL;
#ifdef VOLUME_CLUSTER_SUPPORT
	//Fix for Bug 27670
	if ( (IS_CLUSTER_VOLUME(DevContext)) && (DevContext->PnPNotificationEntry)) {
        IoUnregisterPlugPlayNotification(DevContext->PnPNotificationEntry);
        DevContext->PnPNotificationEntry = NULL;
    }

	if (DevContext->LogVolumeFileObject) {
        ObDereferenceObject(DevContext->LogVolumeFileObject);
        DevContext->LogVolumeFileObject = NULL;
        DevContext->LogVolumeDeviceObject = NULL;
     }
#endif
    InVolDbgPrint(DL_INFO, DM_CLUSTER, ("InMageFltSurpriseRemoveDevice: DevContext is %p, DevNum=%S DevBitmap is %p VolumeFlags is 0x%x\n",
        DevContext, DevContext->wDevNum, DevContext->pDevBitmap , DevContext->ulFlags));

    InMageFltSaveAllChanges(DevContext, TRUE, NULL);
    if (DevContext->pDevBitmap && DevContext->pDevBitmap->pBitmapAPI) {
#ifdef VOLUME_CLUSTER_SUPPORT
        BitmapPersistentValues BitmapPersistentValue(DevContext);
        DevContext->pDevBitmap->pBitmapAPI->CommitBitmap(cleanShutdown, BitmapPersistentValue, NULL);
#else
	    DevContext->pDevBitmap->pBitmapAPI->CommitBitmap(cleanShutdown, NULL);
#endif
        DevContext->pDevBitmap->pBitmapAPI->ChangeBitmapModeToRawIO();
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
    PDEV_CONTEXT        pDevContext;
    PTAG_NODE           pHead = NULL;
    LIST_ENTRY          *pCurr = NULL;
    

    InVolDbgPrint(DL_FUNC_TRACE, DM_SHUTDOWN,
        ("InMageFltRemoveDevice: DeviceObject %#p Irp %#p\n",
        DeviceObject, Irp));

    pDevContext = pDeviceExtension->pDevContext;
#ifdef VOLUME_CLUSTER_SUPPORT
	//Fix for Bug 27670
	if ( (IS_CLUSTER_VOLUME(pDevContext)) && (pDevContext->PnPNotificationEntry)) {
        IoUnregisterPlugPlayNotification(pDevContext->PnPNotificationEntry);
        pDevContext->PnPNotificationEntry = NULL;
    }
	if (pDevContext->LogVolumeFileObject) {
        ObDereferenceObject(pDevContext->LogVolumeFileObject);
        pDevContext->LogVolumeFileObject = NULL;
        pDevContext->LogVolumeDeviceObject = NULL;
     }
#endif
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
            if (pHead->pTagInfo->TagStatus[i].DevContext && (pHead->pTagInfo->TagStatus[i].DevContext == pDevContext)) {
                bRemoveFromList = true;
                break;
            }
        }

        if (bRemoveFromList) {
            for (ULONG i = 0; i < pHead->ulNoOfVolumes; i++) {
                if (pHead->pTagInfo->TagStatus[i].DevContext ) {
                DereferenceDevContext(pHead->pTagInfo->TagStatus[i].DevContext);
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


    if (pDevContext)
    {
        PBASIC_VOLUME BasicVolume = NULL;

        InVolDbgPrint(DL_INFO, DM_SHUTDOWN,
            ("InMageFltRemoveDevice: Removing Device - pDevContext = %#p Volume = %S(%S)\n", 
            pDevContext, pDevContext->wDevID, pDevContext->wDevNum));

        KeWaitForSingleObject(&pDevContext->Mutex, Executive, KernelMode, FALSE, NULL);
        if (NULL != pDevContext->pBasicVolume && pDevContext->pBasicVolume->pDevContext) {
            ASSERT(pDevContext == pDevContext->pBasicVolume->pDevContext);
            BasicVolume = pDevContext->pBasicVolume;
            pDevContext->pBasicVolume = NULL;
        }
#ifdef VOLUME_CLUSTER_SUPPORT
        if ( IS_CLUSTER_VOLUME(pDevContext) &&
             pDevContext->pDevBitmap && 
             pDevContext->pDevBitmap->pBitmapAPI ) 
        {
            // Reset physical disk as the stack is being removed.
            pDevContext->pDevBitmap->pBitmapAPI->ResetPhysicalDO();
        }
#endif
        KeReleaseMutex(&pDevContext->Mutex, FALSE);
        if (NULL != BasicVolume) {
            DereferenceBasicVolume(BasicVolume);    //Volume Context Reference
            DereferenceBasicVolume(BasicVolume);    //Delete Basic Volume.
        }
        // this Debugprint has to run outside the spinlock as the Unicode string table is paged
        InVolDbgPrint(DL_INFO, DM_SHUTDOWN,
            ("InMageFltRemoveDevice: Device = %S ulTotalChangesPending = %i\n", 
            pDevContext->wDevNum, pDevContext->ChangeList.ulTotalChangesPending));

        ReferenceDevContext(pDevContext);
        // Acquire SyncEvent so that user mode process are not processing getdbtrans
        KeWaitForSingleObject(&pDevContext->SyncEvent, Executive, KernelMode, FALSE, NULL);
        KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
        KeAcquireSpinLockAtDpcLevel(&pDevContext->Lock);

        OrphanMappedDataBlocksAndMoveCommitPendingDirtyBlockToMainQueue(pDevContext);

        pDevContext->eDSDBCdeviceState = ecDSDBCdeviceOffline;
        pDevContext->FilterDeviceObject = NULL;

        DereferenceDevContext(pDeviceExtension->pDevContext);
        pDeviceExtension->pDevContext = NULL;

        if (pDevContext->ulFlags & DCF_DEVID_OBTAINED) {
            // Trigger activity to start writing existing data to bit map file.
            KeSetEvent(&DriverContext.ServiceThreadWakeEvent, IO_DISK_INCREMENT, FALSE);
        } else {
            // GUID for this volume is not obtained. So the volume is unmounted even before mountdev links are created.
            ASSERT(NULL == pDevContext->pDevBitmap);
            RemoveEntryList((PLIST_ENTRY)pDevContext);
            DriverContext.ulNumDevs--;
            InitializeListHead((PLIST_ENTRY)pDevContext);
            //pDevContext->ulFlags |= DCF_REMOVED_FROM_LIST; // Helps in debugging to know if it is removed from list.
            DereferenceDevContext(pDevContext);
        }

        KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
        KeReleaseSpinLock(&DriverContext.Lock, OldIrql);
        KeSetEvent(&pDevContext->SyncEvent, IO_NO_INCREMENT, FALSE);

        DereferenceDevContext(pDevContext);
        pDevContext = NULL;
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

    switch(currentIrpStack->MinorFunction) {

        case IRP_MN_START_DEVICE:
            // Get the respective disk number to faciliate the disk-volume mapping
           
            InVolDbgPrint(DL_INFO, DM_DRIVER_PNP | DM_DEV_ARRIVAL,
                ("InMageFltDispatchPnp: pDevContext = %#p, Minor Function - START_DEVICE\n",
                pDeviceExtension->pDevContext));
            Status = InMageFltStartDevice(DeviceObject, Irp);

            if (NT_SUCCESS(Status)){
              SET_NEW_PNP_STATE(pDeviceExtension,Started);
            }
            
            break;

        case IRP_MN_QUERY_STOP_DEVICE:
            InVolDbgPrint(DL_INFO, DM_DRIVER_PNP | DM_SHUTDOWN,
                ("InMageFltDispatchPnp: pDevContext = %#p - Query shutdown, flushing diry blocks\n", pDeviceExtension->pDevContext));

            if ((pDeviceExtension->pDevContext) && (!(pDeviceExtension->pDevContext->ulFlags & DCF_FILTERING_STOPPED))) {
                InVolDbgPrint(DL_INFO, DM_DRIVER_PNP | DM_SHUTDOWN,
                    ("InMageFltDispatchPnp: Saving changes for device %S ulTotalChangesPending = %#x ulChangesPendingCommit = %#x\n",
                    pDeviceExtension->pDevContext->wDevNum, pDeviceExtension->pDevContext->ChangeList.ulTotalChangesPending,
                    pDeviceExtension->pDevContext->ulChangesPendingCommit));

                InMageFltSaveAllChanges(pDeviceExtension->pDevContext, TRUE, NULL);
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
                ("InMageFltDispatchPnp: pDevContext = %#p SURPRISE_REMOVAL\n",
                pDeviceExtension->pDevContext));
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
                ("InMageFltDispatchPnp: pDevContext = %#p Minor Function - REMOVE_DEVICE\n",
                pDeviceExtension->pDevContext));
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


NTSTATUS
InMageFltSaveAllChanges(PDEV_CONTEXT   pDevContext, BOOLEAN bWaitRequired, PLARGE_INTEGER pliTimeOut, bool bMarkVolumeOffline)
{
    LIST_ENTRY          ListHead;
    PWORK_QUEUE_ENTRY   pWorkItem = NULL;
    KIRQL               OldIrql;
#ifndef VOLUME_CLUSTER_SUPPORT
	UNREFERENCED_PARAMETER(bMarkVolumeOffline);
#endif
    if (NULL == pDevContext)
        return STATUS_SUCCESS;

    if (pDevContext->ulFlags & DCF_BITMAP_WRITE_DISABLED)
        return STATUS_SUCCESS;

#ifdef VOLUME_CLUSTER_SUPPORT
    if (IS_VOLUME_OFFLINE(pDevContext)) {
        // If Volume is on cluster disk, and volume is released.
        // we should not have any changes here.
        ASSERT((0 == pDevContext->ChangeList.ulTotalChangesPending) && (0 == pDevContext->ChangeList.ullcbTotalChangesPending));

        if (pDevContext->ChangeList.ulTotalChangesPending || pDevContext->ulChangesPendingCommit) {
            InVolDbgPrint( DL_ERROR, DM_BITMAP_WRITE | DM_CLUSTER, 
                ("InMageFltSaveAllChanges: Device %s offline has ChangesPending = 0x%x ChangesPendingCommit = 0x%x\n",
                        pDevContext->chDevNum, pDevContext->ChangeList.ulTotalChangesPending,
                        pDevContext->ulChangesPendingCommit));
        } else {
            InVolDbgPrint( DL_TRACE_L1, DM_BITMAP_OPEN | DM_BITMAP_WRITE | DM_CLUSTER,
                           ("InMageFltSaveAllChanges: Device %s offline: Pending = 0x%x, PendingCommit = 0x%x\n",
                            pDevContext->chDevNum,
                            pDevContext->ChangeList.ulTotalChangesPending, pDevContext->ulChangesPendingCommit));
        }

        return STATUS_SUCCESS;
    }
#endif

    InitializeListHead(&ListHead);

    if (0 == (pDevContext->ulFlags & DCF_BITMAP_WRITE_DISABLED)) {
        KeWaitForSingleObject(&pDevContext->SyncEvent, Executive, KernelMode, FALSE, NULL);
        KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);

        // Device is being shutdown soon, write all pending dirty blocks to bit map.
        OrphanMappedDataBlocksAndMoveCommitPendingDirtyBlockToMainQueue(pDevContext);
        if (!IsListEmpty(&pDevContext->ChangeList.Head)) {
            if (!AddVCWorkItemToList(ecWorkItemBitmapWrite, pDevContext, 0, TRUE, &ListHead)) {
            	//Mark for resync if failed to add work item
                QueueWorkerRoutineForSetDevOutOfSync(pDevContext, (ULONG)INFLTDRV_ERR_NO_GENERIC_NPAGED_POOL, STATUS_NO_MEMORY, true);
            }
        }

#ifdef VOLUME_CLUSTER_SUPPORT
        if (bMarkVolumeOffline)
            pDevContext->ulFlags &= ~DCF_CLUSTER_VOLUME_ONLINE;
#endif

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


NTSTATUS
GetDevSize(
    IN  PDEVICE_OBJECT  TargetDeviceObject,
    OUT LONGLONG        *pllDevSize,
    OUT NTSTATUS        *pInMageStatus
    )
{
    KEVENT              Event;
    ULONG               OutputSize;
    PIRP                Irp;
    IO_STATUS_BLOCK     IoStatus;
    NTSTATUS            Status;

    ASSERT(NULL != pllDevSize);

    if (NULL == pllDevSize) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("GetDevSize: pllDevSize is NULL!!\n"));
        return STATUS_INVALID_PARAMETER;
    }

	
        GET_LENGTH_INFORMATION  lengthInfo;

        KeInitializeEvent(&Event, NotificationEvent, FALSE);

        OutputSize = sizeof(GET_LENGTH_INFORMATION);
        Irp = IoBuildDeviceIoControlRequest(IOCTL_DISK_GET_LENGTH_INFO, TargetDeviceObject, NULL, 0,
                                &lengthInfo, OutputSize, FALSE, &Event, &IoStatus);
        if (!Irp) {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("GetDevSize: Failed in IoBuildDeviceIoControlRequest\n"));
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        Status = IoCallDriver(TargetDeviceObject, Irp);
        if (Status == STATUS_PENDING) {
            KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
            Status = IoStatus.Status;
        }

        if (NT_SUCCESS(Status)) {
            *pllDevSize = lengthInfo.Length.QuadPart;
            InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, ("GetDevSize: VolumeSize is %#I64x using IOCTL_DISK_GET_LENGTH_INFO\n", 
                        lengthInfo.Length.QuadPart));
        } else {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("GetDevSize: Failed to determine volume size using IOCTL_DISK_GET_LENGTH_INFO, Status = %#x\n", Status));
            if (pInMageStatus)
                *pInMageStatus = INFLTDRV_ERR_DEV_GET_LENGTH_INFO_FAILED;
            return Status;
        }
    
#if (DBG)
    {
    PVOLUME_DISK_EXTENTS diskExtents; // dynamically sized allocation based on number of extents

// if we are compiling for WinXP or later in debug mode, add size validation
    LONGLONG       debugVolumeSize = *pllDevSize;
    BOOLEAN 	   bLoopAlwaysTrue = TRUE;

    OutputSize = sizeof(VOLUME_DISK_EXTENTS); // start with allocation for 1 extent
    diskExtents = (PVOLUME_DISK_EXTENTS)new UCHAR[OutputSize];
    if (NULL == diskExtents) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("GetDevSize: Failed to allocate memory\n"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    KeInitializeEvent(&Event, NotificationEvent, FALSE);

    Irp = IoBuildDeviceIoControlRequest(
                    IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                    TargetDeviceObject, NULL, 0,
                    diskExtents, OutputSize, FALSE, &Event, &IoStatus);
    if (!Irp) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("GetDevSize: Failed in IoBuildDeviceIoControlRequest\n"));
        delete[] diskExtents;
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Status = IoCallDriver(TargetDeviceObject, Irp);
    if (Status == STATUS_PENDING) {
        KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
        Status = IoStatus.Status;
    }

    if (STATUS_BUFFER_OVERFLOW == Status) {
        // must be more than one extent, do IOCTL again with correct buffer size
        OutputSize = sizeof(VOLUME_DISK_EXTENTS) + ((diskExtents->NumberOfDiskExtents - 1) * sizeof(DISK_EXTENT));
        delete[] diskExtents;

        diskExtents = (PVOLUME_DISK_EXTENTS)new UCHAR[OutputSize];
        if (NULL == diskExtents) {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("GetDevSize: Failed to allocate memory\n"));
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        KeInitializeEvent(&Event, NotificationEvent, FALSE);

        Irp = IoBuildDeviceIoControlRequest(
                        IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                        TargetDeviceObject, NULL, 0,
                        diskExtents, OutputSize, FALSE, &Event, &IoStatus);
        if (!Irp) {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("GetDevSize: Failed in IoBuildDeviceIoControlRequest\n"));
            delete[] diskExtents;
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        Status = IoCallDriver(TargetDeviceObject, Irp);
        if (Status == STATUS_PENDING) {
            KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
            Status = IoStatus.Status;
        }
    }

    if ((!NT_SUCCESS(Status)) || (diskExtents->NumberOfDiskExtents != 1)) {
        // binary search the volume size if there is more than 1 extent
        AsyncIORequest      *readRequest;
        UINT64              currentGap;
        UINT64              currentOffset;
        PUCHAR              sectorBuffer;
        ULONG               extentIdx;

        if (NT_SUCCESS(Status)) {
            currentOffset = 0;
            // add together all the extents to calculate an upper bound on volume size
            // on RAID-5 and Mirror volumes the real size will be smaller that this sum
            for(extentIdx = 0;extentIdx < diskExtents->NumberOfDiskExtents;extentIdx++) {
                currentOffset += diskExtents->Extents[extentIdx].ExtentLength.QuadPart;
            }
            currentOffset += DISK_SECTOR_SIZE; // these two are needed to make the search algorithm work
            currentOffset /= DISK_SECTOR_SIZE;
        } else {
            currentOffset = MAXIMUM_SECTORS_ON_VOLUME; // if IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS failed, do a binary search with a large limit
        }
        // the search gap MUST be a power of two, so find an appropriate value
        currentGap = 1i64;
        while ((currentGap * 2i64) < currentOffset) {
            currentGap *= 2i64;
        }
        sectorBuffer = new UCHAR[DISK_SECTOR_SIZE];

        do {
            readRequest = new AsyncIORequest(TargetDeviceObject, NULL);
            readRequest->Read(sectorBuffer, DISK_SECTOR_SIZE, currentOffset * DISK_SECTOR_SIZE);
            Status = readRequest->Wait(NULL);
            readRequest->Release();

            if (NT_SUCCESS(Status)) {
                InVolDbgPrint(DL_VERBOSE, DM_BITMAP_OPEN,
                    ("GetDevSize: Volumes Size binary search successful probe at offset %#I64x  gap %#I64x \n",currentOffset,currentGap)); 
                currentOffset += currentGap;
                if (currentGap == 0) {
                    Status = STATUS_SUCCESS;
                    *pllDevSize = (currentOffset * DISK_SECTOR_SIZE) + DISK_SECTOR_SIZE;
                    break;
                }
            } else {
                if ((Status != STATUS_INVALID_PARAMETER) &&
                    (Status != STATUS_INVALID_DEVICE_REQUEST)) 
                {
                    InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("GetDevSize: Volumes Size binary search failed at offset %#I64X  gap %#I64X Status = %#X\n",
                                currentOffset, currentGap, Status)); 
                    if (NULL != pInMageStatus)
                        *pInMageStatus = INFLTDRV_ERR_DEV_SIZE_SEARCH_FAILED;
                    break;
                }

                InVolDbgPrint(DL_VERBOSE, DM_BITMAP_OPEN, ("GetDevSize: Volumes Size binary search failed probe at offset %#I64X  gap %#I64X Status = %#X\n",
                                currentOffset, currentGap, Status)); 
                currentOffset -= currentGap;
                if (currentGap == 0) {
                    Status = STATUS_SUCCESS;
                    *pllDevSize = currentOffset * DISK_SECTOR_SIZE;
                    break;
                }
            }

            currentGap /= 2;

        } while (bLoopAlwaysTrue); //Fixed W4 Warnings. AI P3:- Code need to be rewritten to set loop condition appropriately

        delete[] sectorBuffer;
    } else {
        // must be a single extent, use that size for the volume size
        *pllDevSize = diskExtents->Extents[0].ExtentLength.QuadPart;
    }

    delete[] diskExtents;

    // verify the two ways to get the volume size match while debugging on W2K3
    ASSERT(debugVolumeSize == *pllDevSize);
    }

#endif
    return Status;
}

PDEV_BITMAP
OpenBitmapFile(
    PDEV_CONTEXT pDevContext,
    NTSTATUS        &Status  
    )
{
    PDEV_BITMAP      pDevBitmap = NULL;
    PDEVICE_EXTENSION   pDeviceExtension;
    NTSTATUS            InMageOpenStatus = STATUS_SUCCESS;
    ULONG               ulBitmapGranularity;
    BOOLEAN             bDevInSync = TRUE;
    BOOLEAN             bLocFound=FALSE;
    BOOLEAN 		    bLoopAlwaysTrue = TRUE;
    BitmapAPI           *pBitmapAPI = NULL;
#ifdef VOLUME_CLUSTER_SUPPORT
    BitmapPersistentValues BitmapPersistentValue(pDevContext);
    KIRQL               OldIrql;
#endif
#ifdef _WIN64
    // Flag to indicate whether to allocate DATA Pool.
    BOOLEAN             bAllocMemory = FALSE;
#endif
    PAGED_CODE();

    InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, ("OpenBitmapFile: Called on device %p\n", pDevContext));

    if ((NULL == pDevContext) || (NULL == pDevContext->FilterDeviceObject)) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("OpenBitmapFile: Invalid parameter %p\n", pDevContext));
        goto Cleanup_And_Return_Error;
    }

    if (0 == (pDevContext->ulFlags & DCF_DEVID_OBTAINED)) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("OpenBitmapFile: DevContext %p does not have GUID obtained yet\n", pDevContext));
        InMageFltLogError(pDevContext->FilterDeviceObject, __LINE__, INFLTDRV_INFO_OPEN_BITMAP_CALLED_PRIOR_TO_OBTAINING_DEVICEID, STATUS_SUCCESS);
        goto Cleanup_And_Return_Error;
    }

    pDevBitmap = AllocateDevBitmap();
    if (NULL == pDevBitmap) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("OpenBitmapFile: AllocteDevBitmap failed for %S\n", pDevContext->wDevNum));
        goto Cleanup_And_Return_Error;
    }

    pDeviceExtension = (PDEVICE_EXTENSION)pDevContext->FilterDeviceObject->DeviceExtension; 

	Status = GetDevSize( pDeviceExtension->TargetDeviceObject, 
                            &pDevContext->llDevSize,
                            &InMageOpenStatus);
    if (!NT_SUCCESS(Status)) {
        if (STATUS_SUCCESS != InMageOpenStatus) {
            InMageFltLogError(pDeviceExtension->DeviceObject,__LINE__, InMageOpenStatus, Status, 
                pDevContext->wDevNum, DEVICE_NUM_LENGTH * sizeof(WCHAR), pDevContext->wDevID, GUID_SIZE_IN_BYTES);
        }
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN,
            ("OpenBitmapFile: Failed in GetVolumeSize Status = %#x, InMageStatus = %#x\n", Status, InMageOpenStatus));
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

    pBitmapAPI = new BitmapAPI();
    InVolDbgPrint(DL_INFO, DM_MEM_TRACE, ("OpenBitmapFile: Allocation %#p\n", pBitmapAPI));
    if (pBitmapAPI) {
        PWCHAR  Loc = pDevContext->BitmapFileName.Buffer;
        ULONG   LenTocheck = 0;
        InMageOpenStatus = STATUS_SUCCESS;
        pDevBitmap->pBitmapAPI = pBitmapAPI;

        KeWaitForMutexObject(&pDevContext->BitmapOpenCloseMutex, Executive, KernelMode, FALSE, NULL);
        do
        {
            Status = pBitmapAPI->Open(&pDevContext->BitmapFileName, ulBitmapGranularity, 
                        pDevContext->ulBitmapOffset, 
                        pDevContext->llDevSize, 
#if DBG
                        pDevContext->chDevNum,
#endif
                        pDevBitmap->SegmentCacheLimit,
                        ((0 == (pDevContext->ulFlags & DCF_CLEAR_BITMAP_ON_OPEN)) ? false : true),
#ifdef VOLUME_CLUSTER_SUPPORT
                        BitmapPersistentValue,
#endif
                        &InMageOpenStatus);

            if (STATUS_SUCCESS != Status) {
                while (!bLocFound && (LenTocheck + sizeof(WCHAR)*GUID_SIZE_IN_CHARS) <=  pDevContext->BitmapFileName.Length){
                    if (sizeof(WCHAR)*GUID_SIZE_IN_CHARS == RtlCompareMemory(Loc,pDevContext->wDevID,sizeof(WCHAR)*GUID_SIZE_IN_CHARS)){
                        bLocFound = TRUE;
                        break;
                    }
                    Loc++; 
                    LenTocheck++;
                    LenTocheck++;
                }

                if (bLocFound) {
                    if (STATUS_SUCCESS != GetNextDeviceID(pDevContext, Loc)) {
                        break;
                    }
                } else {
                    break;
                }
            } else {
#ifdef VOLUME_CLUSTER_SUPPORT
                if (IS_CLUSTER_VOLUME(pDevContext)) {
                    bool bFilteringStopped = false;
                    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
                    if (pDevContext->ulFlags & DCF_SAVE_FILTERING_STATE_TO_BITMAP) {
                        pDevContext->ulFlags &= ~DCF_SAVE_FILTERING_STATE_TO_BITMAP;
                    } else {
                        if (BitmapPersistentValue.GetVolumeFilteringStateFromBitmap()) {
                            pDevContext->ulFlags |= DCF_FILTERING_STOPPED;
                        } else {
                            pDevContext->ulFlags &= ~DCF_FILTERING_STOPPED;
                        }
                    }
                    if (pDevContext->ulFlags & DCF_FILTERING_STOPPED) {
                        StopFilteringDevice(pDevContext, true, false, false, NULL);
                        bFilteringStopped = true;
#ifdef _WIN64			
                    } else {
                        //Set when volume is cluster and filtering is started on any volume
                        bAllocMemory = 1;
#endif
                    }
                    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
                    if (bFilteringStopped) {
                        // Driver marks clean, as there are no changes on stop filtering
                        pBitmapAPI->SaveFilteringStateToBitmap(true, true, true);
                    }
					pDevContext->pBasicVolume->pDisk->IsAccessible = 1;
#ifdef _WIN64		    
                } else {
                    //Set when volume is non-cluster and filtering is started on any volume
                    bAllocMemory = 1;
#endif
                }
#else
#ifdef _WIN64
		        //Set when volume is non-cluster and filtering is started on any volume
	            bAllocMemory = 1;
#endif
#endif
                break;
            }
        } while(bLoopAlwaysTrue); //Fixed W4 Warnings. AI P3:- Code need to be rewritten to set loop condition appropriately

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
            if (STATUS_SUCCESS != InMageOpenStatus) {
                InMageFltLogError(pDeviceExtension->DeviceObject,__LINE__, InMageOpenStatus, Status,
                        pDevContext);
            }
            goto Cleanup_And_Return_Error;
        }

        Status = pBitmapAPI->IsDevInSync(&bDevInSync, &InMageOpenStatus);
        if (Status != STATUS_SUCCESS) {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("OpenBitmapFile: bitmap file %wZ open for device %S failed to get sync status, Status(%#x)\n", 
                                &pDevContext->BitmapFileName, pDevContext->wDevNum, Status));
            goto Cleanup_And_Return_Error;
        }

        if (FALSE == bDevInSync) {
            SetDevOutOfSync(pDevContext, InMageOpenStatus, InMageOpenStatus, false);
            // Cleanup data files from previous boot.
            CDataFile::DeleteDataFilesInDirectory(&pDevContext->DataLogDirectory);
#ifdef VOLUME_CLUSTER_SUPPORT
        } else if (IS_CLUSTER_VOLUME(pDevContext) && (pDevContext->ulFlags & DCF_CLUSTER_VOLUME_ONLINE)) {
            ULONG     ulOutOfSyncErrorCode =0;
            ULONG     ulOutOfSyncErrorStatus =0;
            ULONGLONG ullOutOfSyncTimeStamp =0;
            ULONG     ulOutOfSyncCount =0;
            ULONG     ulOutOfSyncIndicatedAtCount =0;
            ULONGLONG ullOutOfSyncIndicatedTimeStamp =0;
            ULONGLONG ullOutOfSyncResetTimeStamp =0;

            KeWaitForSingleObject(&pDevContext->Mutex, Executive, KernelMode, FALSE, NULL);
            if (BitmapPersistentValue.GetResynRequiredValues(ulOutOfSyncErrorCode, 
                ulOutOfSyncErrorStatus, 
                ullOutOfSyncTimeStamp, 
                ulOutOfSyncCount,
                ulOutOfSyncIndicatedAtCount,
                ullOutOfSyncIndicatedTimeStamp,
                ullOutOfSyncResetTimeStamp)) {
                pDevContext->bResyncRequired = true;
                InVolDbgPrint(DL_WARNING, DM_BITMAP_OPEN, ("OpenBitmapFile: Bitmap file %wZ device %S Resync is TRUE)\n", 
                                &pDevContext->BitmapFileName, pDevContext->wDevNum));
            } else {
                pDevContext->bResyncRequired = false;
                pDevContext->liLastOutOfSyncTimeStamp.QuadPart = ullOutOfSyncTimeStamp;
                InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, ("OpenBitmapFile: Bitmap file %wZ device %S Resync is FALSE)\n", 
                                &pDevContext->BitmapFileName, pDevContext->wDevNum));
            }

            pDevContext->ulOutOfSyncCount = ulOutOfSyncCount;
            pDevContext->liOutOfSyncTimeStamp.QuadPart = ullOutOfSyncTimeStamp;
            pDevContext->ulOutOfSyncErrorStatus = ulOutOfSyncErrorStatus;
            pDevContext->ulOutOfSyncErrorCode = ulOutOfSyncErrorCode;
            pDevContext->ulOutOfSyncIndicatedAtCount = ulOutOfSyncIndicatedAtCount;
            pDevContext->liOutOfSyncIndicatedTimeStamp.QuadPart = ullOutOfSyncIndicatedTimeStamp;
            pDevContext->liOutOfSyncResetTimeStamp.QuadPart = ullOutOfSyncResetTimeStamp;

            KeReleaseMutex(&pDevContext->Mutex, FALSE);
            InVolDbgPrint( DL_INFO, DM_CLUSTER, ("Updated the volume context with resync information read from Bitmap\n"));
#endif
        }

#ifdef VOLUME_CLUSTER_SUPPORT
        //update the Globals with the values read.
        UpdateGlobalWithPersistentValuesReadFromBitmap(bDevInSync, BitmapPersistentValue);

        //update the DevContext with the values read from Bitmap.
        BitmapPersistentValue.UpdateDevContextWithValuesReadFromBitmap(bDevInSync);
#endif

        // Unset ignore bitmap creation .
        pDevContext->ulFlags &= ~DCF_IGNORE_BITMAP_CREATION;
        pDevContext->ulFlags &= ~DCF_CLEAR_BITMAP_ON_OPEN;

#ifdef VOLUME_CLUSTER_SUPPORT
        if (pDevContext->ulFlags & DCF_VOLUME_ON_CLUS_DISK) {
        // Try to open volume object.
            InVolDbgPrint( DL_INFO, DM_CLUSTER, 
                    ("Open Bitmap file called on cluster device %p\n", pDevContext));

            if (NULL == pDevContext->PnPNotificationEntry) {
                if (0 != pDevContext->UniqueVolumeName.Length) {
                    Status = IoGetDeviceObjectPointer(&pDevContext->UniqueVolumeName, FILE_READ_DATA, 
                                        &pDevContext->LogVolumeFileObject, &pDevContext->LogVolumeDeviceObject);
                    if (!NT_SUCCESS(Status)) {
                        Status = IoGetDeviceObjectPointer(&pDevContext->UniqueVolumeName, FILE_READ_ATTRIBUTES, 
                                        &pDevContext->LogVolumeFileObject, &pDevContext->LogVolumeDeviceObject);
                    }

                    if (NT_SUCCESS(Status)) {
                        Status = IoRegisterPlugPlayNotification(
                                        EventCategoryTargetDeviceChange, 0, pDevContext->LogVolumeFileObject,
                                        DriverContext.DriverObject, DriverNotificationCallback, pDevContext,
                                        &pDevContext->PnPNotificationEntry);
                        ASSERT(STATUS_SUCCESS == Status);
                        if (STATUS_SUCCESS == Status) {
                            InVolDbgPrint( DL_INFO, DM_CLUSTER, 
                                    ("PlugPlayNotification Registered for FileSystem on device %p\n", pDevContext));
                        } else {
                            InVolDbgPrint( DL_ERROR, DM_CLUSTER, ("Failed to RegisterPlugPlayNotification for FileSystem Unmount\n"));
                        }
                    } else {
                        InVolDbgPrint( DL_ERROR, DM_CLUSTER, ("Failed to open log volume for PnP Notification\n"));
                    }
                } else {
                    InVolDbgPrint( DL_ERROR, DM_CLUSTER, ("UniqueVolumeName is Empty. Failed to set PnP Notification on Volume\n"));
                }
                Status = STATUS_SUCCESS;
            } else {
                InVolDbgPrint(DL_INFO, DM_CLUSTER, ("PnP Notification exists on this device %p\n", pDevContext));
            }
        }
#endif
    } else {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("OpenBitmapFile: Failed to allocate BitmapAPI\n"));
    }

    pDevBitmap->eVBitmapState = ecVBitmapStateOpened;
    pDevBitmap->pDevContext = ReferenceDevContext(pDevContext);
  
	RtlCopyMemory(pDevBitmap->DevID, pDevContext->wDevID, GUID_SIZE_IN_BYTES + sizeof(WCHAR));

    if (pDevContext->ulFlags & DCF_VOLUME_LETTER_OBTAINED) {
  		RtlCopyMemory(pDevBitmap->wDevNum, pDevContext->wDevNum, DEVICE_NUM_LENGTH * sizeof(WCHAR));
        pDevBitmap->ulFlags |= VOLUME_BITMAP_FLAGS_HAS_VOLUME_LETTER;
    }

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
	    bool bDeleteBitmap)

{
    BOOLEAN         bWaitForNotification = FALSE;
    BOOLEAN         bSetBitsWorkItemListIsEmpty;
    NTSTATUS        Status;
    KIRQL           OldIrql;
    class BitmapAPI *pBitmapAPI = NULL;
#ifndef VOLUME_CLUSTER_SUPPORT
	UNREFERENCED_PARAMETER(pDevContext);
#endif
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
#ifdef VOLUME_CLUSTER_SUPPORT
                BitmapPersistentValues BitmapPersistentValue(pDevContext);
                pBitmapAPI->Close(recoveryState, BitmapPersistentValue, bDeleteBitmap, NULL);
#else
                pBitmapAPI->Close(recoveryState, bDeleteBitmap, NULL);
#endif
                delete pBitmapAPI;
                pBitmapAPI = NULL;
            }
        } while (bWaitForNotification);

        
        DereferenceDevBitmap(pDevBitmap);
    }

    return;
}

PDEV_CONTEXT
GetDevContextWithGUID(PWCHAR    pGUID)
{
    PDEV_CONTEXT     pDevContext = NULL;
    PLIST_ENTRY         pCurr;
    KIRQL               OldIrql;

    // Lowercase the GUID;
    ConvertGUIDtoLowerCase(pGUID);

    if (GUID_SIZE_IN_BYTES == RtlCompareMemory(pGUID, DriverContext.ZeroGUID, GUID_SIZE_IN_BYTES)) {
        ASSERTMSG("GUID passed to GetDevContextWithGUID is zero", 0);
        return NULL;
    }

    // GUID should not be all zeros.
    // Find the VolumeContext;
    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);

    pCurr = DriverContext.HeadForDevContext.Flink;
    while (pCurr != &DriverContext.HeadForDevContext) {
        PDEV_CONTEXT pTemp = (PDEV_CONTEXT)pCurr;
        PDEVICE_ID    pDeviceID = NULL;

        if (!(pTemp->ulFlags & DCF_SURPRISE_REMOVAL)) {
            if (GUID_SIZE_IN_BYTES == RtlCompareMemory(pGUID, pTemp->wDevID, GUID_SIZE_IN_BYTES)) {
                pDevContext = pTemp;
                ReferenceDevContext(pDevContext);
                break;
            }

            if (NULL != pTemp->GUIDList) {
                KeAcquireSpinLockAtDpcLevel(&pTemp->Lock);

                pDeviceID = pTemp->GUIDList;
                while (NULL != pDeviceID) {
                    if (GUID_SIZE_IN_BYTES == RtlCompareMemory(pGUID, pDeviceID->GUID, GUID_SIZE_IN_BYTES)) {
                        break;
                    }
                    pDeviceID = pDeviceID->NextGUID;
                }

                KeReleaseSpinLockFromDpcLevel(&pTemp->Lock);
            }

            if (NULL != pDeviceID) {
                pDevContext = pTemp;
                ReferenceDevContext(pDevContext);
                break;
            }
        }

        pCurr = pCurr->Flink;
    }

    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

    return pDevContext;
}

NTSTATUS
FillBitmapFilenameInDevContext(PDEV_CONTEXT pDevContext)
{
TRC
    NTSTATUS        Status = STATUS_SUCCESS;
    Registry        reg;
    UNICODE_STRING  uszOverrideFilename;
#ifdef VOLUME_CLUSTER_SUPPORT
    PWSTR           pVolumeOverride = NULL;
    PBASIC_VOLUME   pBasicVolume = NULL;
#endif
    
    PAGED_CODE();

    if (NULL == pDevContext)
        return STATUS_INVALID_PARAMETER;

    InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, ("FillBitmapFilenameInDevContext: For device %S\n", pDevContext->wDevNum));

    ASSERT((MAX_LOG_PATHNAME * sizeof(WCHAR)) <= pDevContext->BitmapFileName.MaximumLength);
    pDevContext->BitmapFileName.Length = 0;
    uszOverrideFilename.Buffer = NULL;
#ifdef VOLUME_CLUSTER_SUPPORT
    // See if there is a volume override.
    pVolumeOverride = GetVolumeOverride(pDevContext);
    if (NULL != pVolumeOverride) {
        // Found valid volume override.
        if ((TRUE == IS_DRIVE_LETTER(pVolumeOverride[0])) && (L':' == pVolumeOverride[1])) {
            RtlAppendUnicodeToString(&pDevContext->BitmapFileName, DOS_DEVICES_PATH);
        }

        RtlAppendUnicodeToString(&pDevContext->BitmapFileName, pVolumeOverride);
        ExFreePoolWithTag(pVolumeOverride, TAG_REGISTRY_DATA);
        goto cleanup;
    }
#endif
    // There is no valid volume override.
    uszOverrideFilename.Length = 0;
    uszOverrideFilename.MaximumLength = MAX_LOG_PATHNAME*sizeof(WCHAR);
    uszOverrideFilename.Buffer = new WCHAR[MAX_LOG_PATHNAME];
    InVolDbgPrint(DL_INFO, DM_MEM_TRACE, ("FillBitmapFilenameInDevContext: Allocation %p\n", uszOverrideFilename.Buffer));
    if (NULL == uszOverrideFilename.Buffer) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("FillBitmapFilenameInDevContext: Allocation failed for Override filename\n"));
        Status = STATUS_NO_MEMORY;
        goto cleanup;
    }
#ifdef VOLUME_CLUSTER_SUPPORT
    // Check if this volume is a cluster volume.
    pBasicVolume = pDevContext->pBasicVolume;
    if (NULL != pBasicVolume) {
        ASSERT(NULL != pBasicVolume->pDisk);

        if ((NULL != pBasicVolume->pDisk) && 
            (pBasicVolume->pDisk->ulFlags & BD_FLAGS_IS_CLUSTER_RESOURCE))
        {
            // Volume on clusdisk. So create LogPathName.
            pDevContext->BitmapFileName.Length = 0;
            RtlAppendUnicodeStringToString(&pDevContext->BitmapFileName, &pDevContext->UniqueVolumeName);
            RtlAppendUnicodeToString(&pDevContext->BitmapFileName, DEFAULT_LOG_DIR_FOR_CLUSDISKS);
            RtlAppendUnicodeToString(&pDevContext->BitmapFileName, LOG_FILE_NAME_FOR_CLUSTER_VOLUME);

            // Write this path back to registry
            SetStringInRegVolumeCfg(pDevContext, DEVICE_BITMAPLOG_PATH_NAME, &pDevContext->BitmapFileName);

            // We have a valid path name, let us return
            goto cleanup;
        }
    }
#endif
    // build a default log filename
    ASSERT((NULL != DriverContext.DefaultLogDirectory.Buffer) && (0 != DriverContext.DefaultLogDirectory.Length));
    pDevContext->BitmapFileName.Length = 0;
    uszOverrideFilename.Length = 0;

    RtlAppendUnicodeStringToString(&uszOverrideFilename, &DriverContext.DefaultLogDirectory);
    if (L'\\' != uszOverrideFilename.Buffer[(uszOverrideFilename.Length / sizeof(WCHAR)) - 1]) {
        RtlAppendUnicodeToString(&uszOverrideFilename, L"\\");
    }
    RtlAppendUnicodeToString(&uszOverrideFilename, LOG_FILE_NAME_PREFIX); // so things sort by name nice
    RtlAppendUnicodeToString(&uszOverrideFilename, pDevContext->wDevID);
    RtlAppendUnicodeToString(&uszOverrideFilename, LOG_FILE_NAME_SUFFIX);

    // Write this path back to registry
    SetStringInRegVolumeCfg(pDevContext, DEVICE_BITMAPLOG_PATH_NAME, &uszOverrideFilename);

    RtlAppendUnicodeToString(&pDevContext->BitmapFileName, DOS_DEVICES_PATH);
    RtlAppendUnicodeStringToString(&pDevContext->BitmapFileName, &uszOverrideFilename);

    Status = STATUS_SUCCESS;

cleanup:
    if (NULL != uszOverrideFilename.Buffer)
        delete uszOverrideFilename.Buffer;

    ValidatePathForFileName(&pDevContext->BitmapFileName);

    return Status;
}

NTSTATUS
GetNextDeviceID(PDEV_CONTEXT DevContext, PWCHAR pDeviceID)
{
    NTSTATUS    Status = STATUS_SUCCESS;
    bool        bFound = false;
    KIRQL       OldIrql;
    ASSERT(NULL != DevContext);

    KeAcquireSpinLock(&DevContext->Lock, &OldIrql);
    PDEVICE_ID    GUIDList = DevContext->GUIDList;
    // Lets first check if DeviceGUID matches primary GUID
    if (GUID_SIZE_IN_BYTES == RtlCompareMemory(pDeviceID, DevContext->wDevID, GUID_SIZE_IN_BYTES)) {
        // Matched primary GUID
        bFound = true;
        if (NULL != GUIDList) {
            RtlCopyMemory(pDeviceID, GUIDList->GUID, GUID_SIZE_IN_BYTES);
        } else {
            Status = STATUS_NOT_FOUND;
        }
    } else {
        while (NULL != GUIDList) {
            if (GUID_SIZE_IN_BYTES == RtlCompareMemory(pDeviceID, GUIDList->GUID, GUID_SIZE_IN_BYTES)) {
                // Found GUID
                bFound = true;
                PDEVICE_ID   NextGUID = GUIDList->NextGUID;
                if (NULL != NextGUID) {
                    RtlCopyMemory(pDeviceID, NextGUID->GUID, GUID_SIZE_IN_BYTES);
                } else {
                    Status = STATUS_NOT_FOUND;
                }
                break;
            }
            GUIDList = GUIDList->NextGUID;
        }
    }

    if (!bFound) {
        // GUID may not have been in the list, or GUID may have been deleted in between
        RtlCopyMemory(pDeviceID, DevContext->wDevID, GUID_SIZE_IN_BYTES);
    }

    KeReleaseSpinLock(&DevContext->Lock, OldIrql);

    return Status;
}

extern "C" ULONG
FillAdditionalDeviceIDs(
    PDEV_CONTEXT DevContext,
    ULONG           &ulGUIDsWritten,
    PUCHAR          pBuffer,
    ULONG           ulLength
    )
{
    ULONG           ulBytesWritten = 0;
    KIRQL           OldIrql;

    KeAcquireSpinLock(&DevContext->Lock, &OldIrql);
    PDEVICE_ID    Node = DevContext->GUIDList;

    while ((NULL != Node) && (ulLength >= GUID_SIZE_IN_BYTES)) {
		RtlCopyMemory(pBuffer, Node->GUID, GUID_SIZE_IN_BYTES);
        ulGUIDsWritten++;
        pBuffer += GUID_SIZE_IN_BYTES;
        ulBytesWritten += GUID_SIZE_IN_BYTES;
        ulLength -= GUID_SIZE_IN_BYTES;
        Node = Node->NextGUID;
    }
    KeReleaseSpinLock(&DevContext->Lock, OldIrql);

    return ulBytesWritten;
}

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
    KIRQL           OldIrql;

    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(ULONG)) {
        Status = STATUS_BUFFER_TOO_SMALL;
        Irp->IoStatus.Information = 0;
        goto CleanUp;
    }
    
    
    if (0 != IoStackLocation->Parameters.DeviceIoControl.InputBufferLength) {
        pDevContext = GetDevContextForThisIOCTL(DeviceObject, Irp);
    }

    if(NULL == pDevContext) {
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Information = 0;
        goto CleanUp;
    }

    ulFlags = (ULONG *)Irp->AssociatedIrp.SystemBuffer;
    *ulFlags = 0;

    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
    ulVolFlags = pDevContext->ulFlags;
    if (pDevContext->bNotify)
        (*ulFlags) |= VSF_DATA_NOTIFY_SET;
    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);

    if (ulVolFlags & DCF_READ_ONLY)
        (*ulFlags) |= VSF_READ_ONLY;

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

    if (ulVolFlags & DCF_PAGEFILE_WRITES_IGNORED)
        (*ulFlags) |= VSF_PAGEFILE_WRITES_IGNORED;

#ifdef VOLUME_CLUSTER_SUPPORT
    if (ulVolFlags & DCF_CLUSTER_VOLUME_ONLINE)
        (*ulFlags) |= VSF_CLUSTER_VOLUME_ONLINE;
#endif

    if (ulVolFlags & DCF_VOLUME_ON_BASIC_DISK)
        (*ulFlags) |= VSF_VOLUME_ON_BASIC_DISK;

#ifdef VOLUME_CLUSTER_SUPPORT
    if (ulVolFlags & DCF_VOLUME_ON_CLUS_DISK)
        (*ulFlags) |= VSF_VOLUME_ON_CLUS_DISK;
#endif
    Irp->IoStatus.Information = sizeof(ULONG);    

CleanUp:
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;

}


DeviceIoControlCommitVolumeSnapshot(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION   DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;;
#if DBG
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
#endif
    PDEV_CONTEXT     DevContext = DeviceExtension->pDevContext;
    PTAG_NODE           pHead = NULL;
    PDATA_BLOCK         DataBlock = NULL;
    KIRQL               OldIrql = 0;
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

        for (ULONG ulCounter = 0; ulCounter < pHead->pTagInfo->ulNumberOfDevices; ulCounter++) {
            if (DevContext != pHead->pTagInfo->TagStatus[ulCounter].DevContext) {
                continue;
            }
                
            if (DevContext->ulFlags & DCF_FILTERING_STOPPED) {
                if (pHead->pTagInfo->ulFlags & TAG_INFO_FLAGS_IGNORE_IF_FILTERING_STOPPED) {
                    pHead->pTagInfo->ulNumberOfDevicesIgnored++;
                    pHead->pTagInfo->TagStatus[ulCounter].Status = STATUS_INVALID_DEVICE_STATE;
                    Status = STATUS_SUCCESS;
                } else {
                    Status = STATUS_INVALID_DEVICE_STATE;
                    pHead->pTagInfo->Status = Status;
                    pHead->pTagInfo->TagStatus[ulCounter].Status = Status;
                }             
            } else {
                KeAcquireSpinLockAtDpcLevel(&DevContext->Lock);
                AddLastChangeTimestampToCurrentNode(DevContext, &DataBlock);
                
                    if (pHead->pTagInfo->ulFlags & TAG_INFO_FLAGS_WAIT_FOR_COMMIT) {
                    Status = AddUserDefinedTags(DevContext, (PUCHAR)pHead->pTagInfo->pBuffer, NULL, NULL , &pHead->pTagInfo->TagGUID, ulCounter);
                    } else {
                    Status = AddUserDefinedTags(DevContext, (PUCHAR)pHead->pTagInfo->pBuffer, NULL, NULL, NULL, 0);
                    }
                KeReleaseSpinLockFromDpcLevel(&DevContext->Lock);
                    pHead->pTagInfo->TagStatus[ulCounter].Status = Status;
                if (STATUS_SUCCESS == Status)
                        pHead->pTagInfo->ulNumberOfDevicesTagsApplied++;
                    }
                  
            if (STATUS_SUCCESS == Status) {
				if (pHead->pTagInfo->ulNumberOfDevices == (pHead->pTagInfo->ulNumberOfDevicesTagsApplied + pHead->pTagInfo->ulNumberOfDevicesIgnored)) {
                    if (pHead->pTagInfo->ulFlags & TAG_INFO_FLAGS_WAIT_FOR_COMMIT) {
                        for (ULONG i = 0; i < pHead->ulNoOfVolumes; i++) {
                            if (pHead->pTagInfo->TagStatus[i].DevContext != NULL) {
                                DereferenceDevContext(pHead->pTagInfo->TagStatus[i].DevContext);
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
            if (pHead->pTagInfo->TagStatus[i].DevContext != NULL) {
                DereferenceDevContext(pHead->pTagInfo->TagStatus[i].DevContext);
            }
        }

        if (pHead->pTagInfo) {
            ExFreePoolWithTag(pHead->pTagInfo, TAG_GENERIC_NON_PAGED);
            pHead->pTagInfo = NULL;
        }
        DeallocateTagNode(pHead);
    }


#ifdef VOLUME_NO_REBOOT_SUPPORT
    if(!DevContext->IsDevUsingAddDevice && DriverContext.IsDispatchEntryPatched) {
#ifdef VOLUME_CLUSTER_SUPPORT
        if(DevContext->IsVolumeCluster) {
            Status = (*DriverContext.MajorFunctionForClusDrives[IRP_MJ_DEVICE_CONTROL])(DeviceExtension->TargetDeviceObject, Irp);
        } else {
#endif
            Status = (*DriverContext.MajorFunction[IRP_MJ_DEVICE_CONTROL])(DeviceExtension->TargetDeviceObject, Irp);
#ifdef VOLUME_CLUSTER_SUPPORT
        }
#endif
    } else {    
#endif
        IoSkipCurrentIrpStackLocation(Irp);
        Status = IoCallDriver(DeviceExtension->TargetDeviceObject, Irp);
    
#ifdef VOLUME_NO_REBOOT_SUPPORT
    }
#endif
    return Status;
}

DeviceIoControlVolumeOffline(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION   DeviceExtension=(PDEVICE_EXTENSION)DeviceObject->DeviceExtension;;
#if DBG
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
#endif
#ifdef VOLUME_CLUSTER_SUPPORT
    PDEV_CONTEXT     pDevContext=DeviceExtension->pDevContext;
#else
#ifdef VOLUME_NO_REBOOT_SUPPORT
	PDEV_CONTEXT     pDevContext=DeviceExtension->pDevContext;
#endif
#endif


    ASSERT(IOCTL_VOLUME_OFFLINE == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);
#ifdef VOLUME_CLUSTER_SUPPORT
    if ((NULL != pDevContext) && 
        (!(pDevContext->ulFlags & DCF_FILTERING_STOPPED)) &&
        (pDevContext->ulFlags & DCF_CLUSTER_VOLUME_ONLINE)) 
    {
        // By this time already we are in last chance mode as file system is unmounted.
        // we write changes directly to volume and close the file.
        InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DeviceIoControlVolumeOffline: DevContext is %p, DevNum=%S\n",
                                pDevContext, pDevContext->wDevNum));

        if (pDevContext->ulFlags & DCF_CV_FS_UNMOUTNED) {
            InVolDbgPrint( DL_INFO, DM_CLUSTER, ("File System is found unmounted at VolumeOffline!!!\n"));
        } else {
            InVolDbgPrint( DL_WARNING, DM_CLUSTER, ("File System is found mounted at VolumeOffline!!!\n"));
        }

        if (pDevContext->pDevBitmap) {
            InMageFltSaveAllChanges(pDevContext, TRUE, NULL, true);
            if (pDevContext->pDevBitmap->pBitmapAPI) {
                BitmapPersistentValues BitmapPersistentValue(pDevContext);
                pDevContext->pDevBitmap->pBitmapAPI->CommitBitmap(cleanShutdown, BitmapPersistentValue, NULL);
                InVolDbgPrint( DL_TRACE_L1, DM_CLUSTER, ("DeviceIoControlVolumeOffline: CommitBitmap completed.\n"));
            }
        }

    }
#endif
    InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DeviceIoControlVolumeOffline: DevContext is %p, DevNum=%S DevBitmap is %p VolumeFlags is 0x%x\n",
        pDevContext, pDevContext->wDevNum, pDevContext->pDevBitmap , pDevContext->ulFlags));
#ifdef VOLUME_NO_REBOOT_SUPPORT
    if(!pDevContext->IsDevUsingAddDevice && DriverContext.IsDispatchEntryPatched) {    
#ifdef VOLUME_CLUSTER_SUPPORT
        if(pDevContext->IsVolumeCluster) {
            Status = (*DriverContext.MajorFunctionForClusDrives[IRP_MJ_DEVICE_CONTROL])(DeviceExtension->TargetDeviceObject, Irp);
        } else {
#endif
            Status = (*DriverContext.MajorFunction[IRP_MJ_DEVICE_CONTROL])(DeviceExtension->TargetDeviceObject, Irp);
#ifdef VOLUME_CLUSTER_SUPPORT
        }
#endif
    } else {
#endif
        IoSkipCurrentIrpStackLocation(Irp);
        Status = IoCallDriver(DeviceExtension->TargetDeviceObject, Irp);
#ifdef VOLUME_NO_REBOOT_SUPPORT
    }
#endif
    return Status;
}


DeviceIoControlVolumeOnline(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    NTSTATUS            Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION   DeviceExtension=(PDEVICE_EXTENSION)DeviceObject->DeviceExtension;;
#if DBG
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
#endif
    PDEV_CONTEXT     pDevContext=DeviceExtension->pDevContext;
    PBASIC_DISK         pBasicDisk = NULL;
    PBASIC_VOLUME       pBasicVolume = NULL;
    KIRQL               OldIrql;
    bool                bAddVolume = false;
    ASSERT(IOCTL_VOLUME_ONLINE == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

    //Fix for Bug 27670
    //pDevContext->ulDiskSignature cannot be zero but for GPT it's zero since wDevID is always valid.
    //Again disk number cannot be 0xffffffff after a successful call to AllocateDevContext.
    if((pDevContext->ulDiskNumber != 0xffffffff) ) {

		//In a clustered with no-reboot/no-failover case pDevContext->ulDiskNumber might not get
		//populated as per the current design so we can outsmart it by passing ulDiskSignature as the
		//first parameter.
        pBasicDisk = GetBasicDiskFromDriverContext(pDevContext->ulDiskSignature,
                               pDevContext->ulDiskNumber);
		
        if(NULL != pBasicDisk) {

            KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);

#if (NTDDI_VERSION >= NTDDI_VISTA)
            pDevContext->PartitionStyle = pBasicDisk->PartitionStyle;
            if(pDevContext->PartitionStyle == ecPartStyleMBR) 
                pDevContext->ulDiskSignature = pBasicDisk->ulDiskSignature;
            else
                RtlCopyMemory(&pDevContext->DiskId, &pBasicDisk->DiskId, sizeof(GUID));
#else
            pDevContext->ulDiskSignature = pBasicDisk->ulDiskSignature;
#endif
#ifdef VOLUME_CLUSTER_SUPPORT
            if (pBasicDisk->ulFlags & BD_FLAGS_IS_CLUSTER_RESOURCE) {
                if (!(pDevContext->ulFlags & DCF_VOLUME_ON_CLUS_DISK)) {
                    pDevContext->ulFlags |= DCF_VOLUME_ON_BASIC_DISK;
                    pDevContext->ulFlags |= DCF_VOLUME_ON_CLUS_DISK;
                    if (pBasicDisk->ulFlags & BD_FLAGS_CLUSTER_ONLINE) {
                        InitializeClusterDiskAcquireParams(pDevContext);
                    }

                    bAddVolume = true;
                }
				//Fix for Bug 27670
				//A case where volume went offline and came back without underlying device removal.
				//Runs only when pDevContext->ulFlags & DCF_VOLUME_ON_CLUS_DISK is TRUE.
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
                      if ((pDevContext->ulFlags & DCF_VOLUME_ON_CLUS_DISK) &&
                         !(pDevContext->ulFlags & DCF_CLUSTER_VOLUME_ONLINE)) {

						  //After this call we should be getting an PNP notification for GUID_IO_VOLUME_MOUNT.					  

                          pDevContext->ulFlags |= DCF_CLUSTER_VOLUME_ONLINE;
                    	  pDevContext->ulFlags &= ~DCF_CLUSDSK_RETURNED_OFFLINE;


                       }
                
                   }
                
            }
			else {
#endif
                pDevContext->bQueueChangesToTempQueue = false;//this case will not occur. 
#ifdef VOLUME_CLUSTER_SUPPORT
            }
#endif

            KeReleaseSpinLock(&pDevContext->Lock, OldIrql);

            if(bAddVolume) {
#if (NTDDI_VERSION >= NTDDI_VISTA)
            if(pDevContext->PartitionStyle == ecPartStyleMBR) 
                pBasicVolume = AddBasicVolumeToDriverContext(pDevContext->ulDiskSignature, pDevContext);
            else
                pBasicVolume = AddBasicVolumeToDriverContext(pDevContext->DiskId, pDevContext);
#else
                pBasicVolume = AddBasicVolumeToDriverContext(pDevContext->ulDiskSignature, pDevContext);
#endif

               
            }

            DereferenceBasicDisk(pBasicDisk);
        } else {
            pDevContext->bQueueChangesToTempQueue = false;//for mbr & gpt non cluster disk
        }
    
    }

    InVolDbgPrint(DL_INFO, DM_INMAGE_IOCTL,
                        ("nDeviceIoControlVolumeOnline: DevContext is %p Disk Signature is %x Disk Number is %d\n",
                            pDevContext, pDevContext->ulDiskSignature, pDevContext->ulDiskNumber));

#ifdef VOLUME_NO_REBOOT_SUPPORT
    if(!pDevContext->IsDevUsingAddDevice && DriverContext.IsDispatchEntryPatched) {    
#ifdef VOLUME_CLUSTER_SUPPORT
        if(pDevContext->IsVolumeCluster) {
            Status = (*DriverContext.MajorFunctionForClusDrives[IRP_MJ_DEVICE_CONTROL])(DeviceExtension->TargetDeviceObject, Irp);
        } else {
#endif
            Status = (*DriverContext.MajorFunction[IRP_MJ_DEVICE_CONTROL])(DeviceExtension->TargetDeviceObject, Irp);
#ifdef VOLUME_CLUSTER_SUPPORT
        }
#endif
    } else {
#endif
        IoSkipCurrentIrpStackLocation(Irp);
        Status = IoCallDriver(DeviceExtension->TargetDeviceObject, Irp);
#ifdef VOLUME_NO_REBOOT_SUPPORT
    }
#endif
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
    KIRQL OldIrql;
    PDEV_CONTEXT DevContext;
#ifndef VOLUME_NO_REBOOT_SUPPORT
	UNREFERENCED_PARAMETER(DeviceObject);
#else
    PCOMPLETION_CONTEXT pCompletionContext = NULL;
    etContextType eContextType = (etContextType)*((PCHAR )Context + sizeof(LIST_ENTRY));
#endif
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
#ifdef VOLUME_NO_REBOOT_SUPPORT
    if (eContextType == ecCompletionContext) {
        pCompletionContext = (PCOMPLETION_CONTEXT)Context;
        DevContext = pCompletionContext->DevContext;
        ASSERT(pCompletionContext->llOffset == DiskCopyDataParameters->DestinationOffset.QuadPart);
        ASSERT(pCompletionContext->llLength == DiskCopyDataParameters->CopyLength.QuadPart);
        DataOffset = pCompletionContext->llOffset;
        DataLength = pCompletionContext->llLength;
        SourceDataOffset = DiskCopyDataParameters->SourceOffset.QuadPart;
    } else {
#endif
        PIO_STACK_LOCATION IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
        DevContext = (PDEV_CONTEXT)Context;
        if (Irp->PendingReturned) {
            IoMarkIrpPending(Irp);
        }
        Bufferlength = IoStackLocation->Parameters.DeviceIoControl.InputBufferLength;
        ASSERT(Bufferlength == sizeof (DISK_COPY_DATA_PARAMETERS));
        DataOffset = DiskCopyDataParameters->DestinationOffset.QuadPart;
        DataLength = DiskCopyDataParameters->CopyLength.QuadPart;
        SourceDataOffset = DiskCopyDataParameters->SourceOffset.QuadPart;
#ifdef VOLUME_NO_REBOOT_SUPPORT
    }
#endif

    if (DevContext->llDevSize > 0) {
        if ((DataOffset >= DevContext->llDevSize) ||
            (SourceDataOffset >= DevContext->llDevSize) ||
            (DataOffset < 0) ||
            (SourceDataOffset < 0) ||
            (DataLength <= 0) ||
            ((DataOffset + DataLength) > DevContext->llDevSize) ||
            ((SourceDataOffset + DataLength) > DevContext->llDevSize)) {
                bTrackData = FALSE;
                // Log an error message in event log.
                InMageFltLogError(DevContext->FilterDeviceObject, __LINE__, INFLTDRV_IOCTL_DISK_COPY_DATA_RECEIVED_VALIDATION_FAILED, STATUS_SUCCESS,
                    DevContext->wDevNum, DEVICE_NUM_LENGTH * sizeof(WCHAR), DevContext->wDevID, GUID_SIZE_IN_BYTES,
                    DevContext->IoctlDiskCopyDataCount, DevContext->IoctlDiskCopyDataCountSuccess);
        }
    }

    KeAcquireSpinLock(&DevContext->Lock, &OldIrql);

    if (NT_SUCCESS(Status)) {
#if DBG
        if (DevContext->IoctlDiskCopyDataCountSuccess == 0) {
            bPrintStat = TRUE;
        }
#endif
        DevContext->IoctlDiskCopyDataCountSuccess += 1;
    } else {
#if DBG
        if (DevContext->IoctlDiskCopyDataCountFailure == 0) {
            bPrintStat = TRUE;
        }
#endif
        DevContext->IoctlDiskCopyDataCountFailure += 1;
    }
    DevContext->IoctlDiskCopyDataCount += 1;

    ULONGLONG RetStatus = 0;
    if (NT_SUCCESS(Status)) {
        RetStatus = 1;
    }

    if ((DataOffset % 512 != 0) || (DataLength % 512 != 0) || (SourceDataOffset % 512 != 0)) {
        // Print a message in log.
        InMageFltLogError(DevContext->FilterDeviceObject, __LINE__, INFLTDRV_IOCTL_DISK_COPY_DATA_RECEIVED_VALIDATION_FAILED2, STATUS_SUCCESS,
            DevContext->wDevNum, DEVICE_NUM_LENGTH * sizeof(WCHAR), DevContext->wDevID, GUID_SIZE_IN_BYTES,
            DevContext->IoctlDiskCopyDataCount, RetStatus);        
    }

    if (DataOffset == SourceDataOffset) {
        InMageFltLogError(DevContext->FilterDeviceObject, __LINE__, INFLTDRV_IOCTL_DISK_COPY_DATA_RECEIVED_VALIDATION_FAILED3, STATUS_SUCCESS,
            DevContext->wDevNum, DEVICE_NUM_LENGTH * sizeof(WCHAR), DevContext->wDevID, GUID_SIZE_IN_BYTES,
            DevContext->IoctlDiskCopyDataCount, RetStatus);                
    }
    if (DataOffset > SourceDataOffset) {
        if ((SourceDataOffset + DataLength) > DataOffset) {
            InMageFltLogError(DevContext->FilterDeviceObject, __LINE__, INFLTDRV_IOCTL_DISK_COPY_DATA_RECEIVED_VALIDATION_FAILED3, STATUS_SUCCESS,
                DevContext->wDevNum, DEVICE_NUM_LENGTH * sizeof(WCHAR), DevContext->wDevID, GUID_SIZE_IN_BYTES,
                DevContext->IoctlDiskCopyDataCount, RetStatus);                    
        }
    } else {
        if ((DataOffset + DataLength) > SourceDataOffset) {
            InMageFltLogError(DevContext->FilterDeviceObject, __LINE__, INFLTDRV_IOCTL_DISK_COPY_DATA_RECEIVED_VALIDATION_FAILED3, STATUS_SUCCESS,
                DevContext->wDevNum, DEVICE_NUM_LENGTH * sizeof(WCHAR), DevContext->wDevID, GUID_SIZE_IN_BYTES,
                DevContext->IoctlDiskCopyDataCount, RetStatus);                    
        }
    }

    InVolDbgPrint(DL_VERBOSE, DM_IOCTL_TRACE, ("IOCTL = (IOCTL_DISK_COPY_DATA Completion)"));
#if DBG
    if (bPrintStat == TRUE) {
        InMageFltLogError(DevContext->FilterDeviceObject, __LINE__, INFLTDRV_IOCTL_DISK_COPY_DATA_RECEIVED, STATUS_SUCCESS,
            DevContext->wDevNum, DEVICE_NUM_LENGTH * sizeof(WCHAR), DevContext->wDevID, GUID_SIZE_IN_BYTES,
            DevContext->IoctlDiskCopyDataCount, DevContext->IoctlDiskCopyDataCountSuccess);
    }
#endif
    if (NT_SUCCESS(Status) && bTrackData == TRUE && (0 == (DevContext->ulFlags & DCF_FILTERING_STOPPED))) {
        // etCaptureMode VolumeCaptureMode = DevContext->eCaptureMode;

        while (DataLength != 0 && NT_SUCCESS(NewStatus)) {
            if (DataLength > MaxUlongVal) {
                WriteMetaData.llOffset = DataOffset;
                WriteMetaData.ulLength = MaxUlongVal;
                DataOffset += MaxUlongVal;
                DataLength -= MaxUlongVal;
#if DBG
                InMageFltLogError(DevContext->FilterDeviceObject, __LINE__, INFLTDRV_IOCTL_DISK_COPY_DATA_RECEIVED_INFO, STATUS_SUCCESS,
                    DevContext->wDevNum, DEVICE_NUM_LENGTH * sizeof(WCHAR), DevContext->wDevID, GUID_SIZE_IN_BYTES,
                    DevContext->IoctlDiskCopyDataCount, DevContext->IoctlDiskCopyDataCountSuccess);
#endif
            } else {
                WriteMetaData.llOffset = DataOffset;
                WriteMetaData.ulLength = (ULONG) DataLength;
                ASSERT(DataLength == WriteMetaData.ulLength);
                if (DataLength != WriteMetaData.ulLength) {
                    InMageFltLogError(DevContext->FilterDeviceObject, __LINE__, INFLTDRV_IOCTL_DISK_COPY_DATA_RECEIVED_ERROR2, STATUS_SUCCESS,
                        DevContext->wDevNum, DEVICE_NUM_LENGTH * sizeof(WCHAR), DevContext->wDevID, GUID_SIZE_IN_BYTES,
                        DevContext->IoctlDiskCopyDataCount, DevContext->IoctlDiskCopyDataCountSuccess);
                }
                DataLength = 0;
            }
            
            NewStatus = AddMetaDataToDevContextNew(DevContext, &WriteMetaData);
            if (!NT_SUCCESS(NewStatus)) {
                break;
            }
        }
        if (!NT_SUCCESS(NewStatus)) {
            InMageFltLogError(DevContext->FilterDeviceObject, __LINE__, INFLTDRV_IOCTL_DISK_COPY_DATA_RECEIVED_ERROR, STATUS_SUCCESS,
                DevContext->wDevNum, DEVICE_NUM_LENGTH * sizeof(WCHAR), DevContext->wDevID, GUID_SIZE_IN_BYTES,
                DevContext->IoctlDiskCopyDataCount, DevContext->IoctlDiskCopyDataCountSuccess);            
            QueueWorkerRoutineForSetDevOutOfSync(DevContext, (ULONG)ERROR_TO_REG_OUT_OF_MEMORY_FOR_DIRTY_BLOCKS, STATUS_NO_MEMORY, true);
        }

        ASSERT(ecCaptureModeMetaData == DevContext->eCaptureMode);
        ASSERT(ecWriteOrderStateData != DevContext->eWOState);

        // if (VolumeCaptureMode == ecCaptureModeData) {
        //    VCSetCaptureModeToData(DevContext);
        // }
        // ASSERT(ecWriteOrderStateData != DevContext->eWOState);
    }

    KeReleaseSpinLock(&DevContext->Lock, OldIrql);
    DereferenceDevContext(DevContext);
#ifdef VOLUME_NO_REBOOT_SUPPORT
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
#endif
    return Status;
}


/////////////////////////////////////////////////////////////////
//Check if this is a W2k system. if Yes let's not proceed with, since most of
//the MS FS utility calls are not supported in this OS version.- Kartha.

////////////////////////////////////////////////////////////////
/*
 Fix For Bug 27337
 This procedure takes a single input which is the unique volume object link
 and from there tries to get the start sector of first LCN known to FS.
 From which we also get the Max Byte Offset known to FS which is again
 one of the output parameters.
 The procedure is intended to be used only on OS >= Windows 2K3
 Only NTFS, REFS and FAT32 filesystems are supported.

 Notes:
 * Any error condition will take the highest precedence i.e. either a fool-proof
    solution or Resync.
 * We will not support windows 2000 OS since none of the major API's (must have)
    are not supported here.

 Arguments:

  Input:

  pUniqueVolumeName.
  >>should be in format L"\\??\\Volume{aaf1f466-194b-11e2-88cf-806e6f6e6963}"

  Output:

  pFsInfo.
  >>This structure is allocated and input by the caller. If Success, this structure shall
      be populated with relevant values.
      
 */

NTSTATUS 
InMageFltFetchFsEndClusterOffset(
 __in PUNICODE_STRING pUniqueVolumeName,
 __inout PFSINFORMATION pFsInfo
 )

{
         
  NTSTATUS Status = STATUS_UNSUCCESSFUL;
  OBJECT_ATTRIBUTES objectAttributes;
  FILE_FS_SIZE_INFORMATION FsSizeInformation;
  IO_STATUS_BLOCK ioStatusBlock;
  HANDLE volumeHandle = NULL;
  LARGE_INTEGER ByteOffset;
  LARGE_INTEGER NumberSectors;
  LARGE_INTEGER TotalClusters;
  LARGE_INTEGER SectorOfFirstLCN;
  ULONG SectorsPerCluster;
  ULONG BytesPerSector;
  ULONG BytesPerCluster;
  FileSystemTypeList FsType;
  RTL_OSVERSIONINFOW osVersion;
  ULONG Fat32RootSectors, Fat32FatSectors, Fat32DataSectors;
  ULONG Fat32RootDirectoryFirstSector, Fat32FirstDataSectorOfFirstLCN;
  FAT32_BPB         fat32Bpb; 
  FAT32_BOOT_SECTOR bootSector;
  NTFS_VOLUME_DATA_BUFFER  NtfsData;
  REFS_VOLUME_DATA_BUFFER  RefsData;
  FILE_FS_ATTRIBUTE_INFORMATION *pfai;
  LARGE_INTEGER faibuf[CONSTRUCT(1, FILE_FS_ATTRIBUTE_INFORMATION, 32)];


  ASSERT(NULL != pFsInfo);
  ASSERT(NULL != pUniqueVolumeName->Buffer);
    
  if( (NULL == pUniqueVolumeName->Buffer) || (NULL == pFsInfo)){
  	
	Status = STATUS_INVALID_PARAMETER;
	goto ClusterOffsetEpilogue;
  }
  
  pfai = (FILE_FS_ATTRIBUTE_INFORMATION *) faibuf;
  
  RtlZeroMemory(pFsInfo, sizeof(FSINFORMATION));
  RtlZeroMemory(&osVersion, sizeof(RTL_OSVERSIONINFOW));

  osVersion.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOW);
  RtlGetVersion(&osVersion);

  //win2k. 5.0
  if(osVersion.dwMajorVersion <= 5 && osVersion.dwMinorVersion == 0){

	 Status = STATUS_NOT_IMPLEMENTED;
	 goto ClusterOffsetEpilogue;
  	}

  InitializeObjectAttributes( &objectAttributes, 
                                pUniqueVolumeName, 
                                OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, 
                                NULL, 
                                NULL );

  Status = ZwOpenFile( &volumeHandle,
                             FILE_READ_DATA | SYNCHRONIZE,
                             &objectAttributes,
                             &ioStatusBlock,
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             FILE_SYNCHRONOUS_IO_NONALERT );

  if (!NT_SUCCESS(Status)){
  	
		InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT,\
			("InMageFltFetchFsEndClusterOffset::ZwOpenFile() failed (Status %lx)\n",Status));
        goto ClusterOffsetEpilogue;
    }
	
  //Lets try to get the FileFsSizeInformation on the volume in question. - Kartha
  
  Status = ZwQueryVolumeInformationFile(volumeHandle,
                                             &ioStatusBlock,
                                             &FsSizeInformation,
                                             sizeof(FsSizeInformation),
                                             FileFsSizeInformation);
											 
  if (!NT_SUCCESS(Status)){
  	
        InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT,\
			("ZwQueryVolumeInformationFile() failed for FileFsSizeInformation (Status %lx)\n",Status));
		goto ClusterOffsetEpilogue;
    }

  //Now we have : FsSizeInformation.SectorsPerAllocationUnit (This gives number of sectors per cluster i.e. FS BLOCK)
  //FsSizeInformation.BytesPerSector
	
  /*Let's use ZwQueryVolumeInformationFile-FileFsAttributeInformation combination to get our hands on the
    File System type we have. First call to ZwQueryVolumeInformationFile is bound to fail with
    STATUS_BUFFER_OVERFLOW since we would have passed a smaller buffer as argument b'coz
    we didn't know the real file system name size. To avoid calling ZwQueryVolumeInformationFile
    twice we can cleverly implement it using a fixed size buffer on stack- Kartha*/


  RtlZeroMemory(faibuf, sizeof(faibuf));

  Status = ZwQueryVolumeInformationFile(volumeHandle,
                                        &ioStatusBlock,
                                        pfai,
                                        sizeof(faibuf),
                                        FileFsAttributeInformation);

  if (!NT_SUCCESS(Status)){
  	
	  InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT,\
			("ZwQueryVolumeInformationFile() failed for FileFsAttributeInformation (Status %lx)\n",Status));
	  goto ClusterOffsetEpilogue;
    }


  //We only support NTFS, REFS and FAT32 for this functionality.
  if( (sizeof(WCHAR) << 2) == RtlCompareMemory(pfai->FileSystemName, L"NTFS", sizeof(WCHAR) << 2)){

		FsType = FSTYPE_NTFS;
    }
  else if( (sizeof(WCHAR) << 2) == RtlCompareMemory(pfai->FileSystemName, L"ReFS", sizeof(WCHAR) << 2)){

	    FsType = FSTYPE_REFS;
  	}
  else if( (sizeof(WCHAR) * 5) == RtlCompareMemory(pfai->FileSystemName, L"FAT32", sizeof(WCHAR) * 5)){

	    FsType = FSTYPE_FAT;
  	}
  else{
  	 FsType = FSTYPE_UNKNOWN;
	 Status = STATUS_UNRECOGNIZED_MEDIA;
	 goto ClusterOffsetEpilogue;
	}

  /*Note: Unlike FATx variants NTFS doesnot have anything like sector offset to the first logical
      cluster number (LCN) of the file system relative to the start of the volume.
      i.e. In case of NTFS ::  first FS LCN offset == first Volume LCN offset == 0th Volume sector.
      While writing this code, it's nowhere elucidated by MS if this is true in case of REFS. Thus for
      REFS we shall use FSCTL_GET_RETRIEVAL_POINTER_BASE the first sector of FS LCN relative
      to volume start.- Kartha.*/
      
  switch(FsType)
  {
    case FSTYPE_NTFS:
		
		Status = ZwFsControlFile(
                   volumeHandle,
                   0, 
                   NULL,
                   NULL,
                   &ioStatusBlock,
                   FSCTL_GET_NTFS_VOLUME_DATA,
                   NULL,
                   0,
                   (PVOID)&NtfsData,
                   sizeof(NTFS_VOLUME_DATA_BUFFER)
                  );

		if (!NT_SUCCESS(Status)){

			InVolDbgPrint(DL_WARNING, DM_DEV_CONTEXT,\
			("ZwFsControlFile() failed for FSCTL_GET_NTFS_VOLUME_DATA on NTFS (Status %lx)\n",Status));
			goto ClusterOffsetEpilogue;
		}

		NumberSectors.QuadPart = NtfsData.NumberSectors.QuadPart;
		TotalClusters.QuadPart = NtfsData.TotalClusters.QuadPart;
		BytesPerSector = NtfsData.BytesPerSector;
		BytesPerCluster = NtfsData.BytesPerCluster;

		Status = ZwFsControlFile(
                   volumeHandle,
                   NULL, 
                   NULL,
                   NULL,
                   &ioStatusBlock,
                   FSCTL_GET_RETRIEVAL_POINTER_BASE,
                   NULL,
                   0,
                   &SectorOfFirstLCN,
                   8
                  );

		if (!NT_SUCCESS(Status)){

     	    InVolDbgPrint(DL_WARNING, DM_DEV_CONTEXT,\
			("ZwFsControlFile() failed for FSCTL_GET_RETRIEVAL_POINTER_BASE on NTFS (Status %lx)\n",Status));	
			//NTFS sector of first FS data LCN always starts at zero, so no issues.
			SectorOfFirstLCN.QuadPart = 0;
		}

		SectorsPerCluster = (ULONG)(NumberSectors.QuadPart / TotalClusters.QuadPart);

		pFsInfo->TotalClustersInFS.QuadPart = TotalClusters.QuadPart;
		pFsInfo->SizeOfFsBlockInBytes = BytesPerCluster;
		pFsInfo->SectorOffsetOfFirstDataLCN.QuadPart = SectorOfFirstLCN.QuadPart;
		pFsInfo->SectorsPerCluster = SectorsPerCluster;
		pFsInfo->BytesPerSector = BytesPerSector;

		pFsInfo->MaxVolumeByteOffsetForFs = (SectorOfFirstLCN.QuadPart * BytesPerSector) + \
			(TotalClusters.QuadPart * SectorsPerCluster * BytesPerSector);

		Status = STATUS_SUCCESS;
        goto ClusterOffsetEpilogue;
		
		break;
	case FSTYPE_REFS:

		Status = ZwFsControlFile(
                   volumeHandle,
                   0, 
                   NULL,
                   NULL,
                   &ioStatusBlock,
                   FSCTL_GET_REFS_VOLUME_DATA,
                   NULL,
                   0,
                   (PVOID)&RefsData,
                   sizeof(REFS_VOLUME_DATA_BUFFER)
                  );

		if (!NT_SUCCESS(Status)){

			InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT,\
			("ZwFsControlFile() failed for FSCTL_GET_REFS_VOLUME_DATA on ReFS (Status %lx)\n",Status));
			goto ClusterOffsetEpilogue;
		}

		NumberSectors.QuadPart = RefsData.NumberSectors.QuadPart;
		TotalClusters.QuadPart = RefsData.TotalClusters.QuadPart;
		BytesPerSector = RefsData.BytesPerSector;
		BytesPerCluster = RefsData.BytesPerCluster;

		Status = ZwFsControlFile(
                   volumeHandle,
                   NULL,
                   NULL,
                   NULL,
                   &ioStatusBlock,
                   FSCTL_GET_RETRIEVAL_POINTER_BASE,
                   NULL,
                   0,
                   &SectorOfFirstLCN,
                   8
                  );

		if (!NT_SUCCESS(Status)){

			InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT,\
			("ZwFsControlFile() failed for FSCTL_GET_RETRIEVAL_POINTER_BASE on ReFS (Status %lx)\n",Status));
			//NTFS sector of first FS data LCN always starts at zero, so no issues, but not sure
			//about REFS case. so rather than taking chance we set error.
			goto ClusterOffsetEpilogue;
		}

		SectorsPerCluster = (ULONG)(NumberSectors.QuadPart / TotalClusters.QuadPart);

		pFsInfo->TotalClustersInFS.QuadPart = TotalClusters.QuadPart;
		pFsInfo->SizeOfFsBlockInBytes = BytesPerCluster;
		pFsInfo->SectorOffsetOfFirstDataLCN.QuadPart = SectorOfFirstLCN.QuadPart;
		pFsInfo->SectorsPerCluster = SectorsPerCluster;
		pFsInfo->BytesPerSector = BytesPerSector;

		pFsInfo->MaxVolumeByteOffsetForFs = (SectorOfFirstLCN.QuadPart * BytesPerSector) + \
			(TotalClusters.QuadPart * SectorsPerCluster * BytesPerSector);

		Status = STATUS_SUCCESS;
        goto ClusterOffsetEpilogue;
		
	break;

	case FSTYPE_FAT:

        ByteOffset.QuadPart = 0;
		
        Status = ZwReadFile(
                   volumeHandle,
                   NULL,
                   NULL,
                   NULL,
                   &ioStatusBlock,
                   &bootSector,
                   sizeof(FAT32_BOOT_SECTOR),
                   &ByteOffset,
                   NULL
                  );

		if (!NT_SUCCESS(Status)){

			InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT,\
			("ZwReadFile() failed reading FAT32 BIOS PARAMETER BLOCK (Status %lx)\n",Status));
			goto ClusterOffsetEpilogue;
		}

		Fat32UnpackBios(&fat32Bpb, &bootSector.bpb.ondiskBpb);

		//Now we have FAT32 Bios Parameter Block ready for our FS max volume offset calculation -Kartha.
		//In fat all LCN counting starts from Starting data area i.e. after FAT. Formula for fat
        //should be StartingDataSector *bytes/sector+Total LCN*sectors/cluster*bytes/sector.
               
        Fat32RootDirectoryFirstSector = Fat32FirstDataSectorOfFirstLCN = (ULONG)0;

		Fat32RootDirectoryFirstSector = fat32Bpb.ReservedSectors + \
			(fat32Bpb.NumberOfFats * fat32Bpb.BigSectorsPerFat);

		Fat32FirstDataSectorOfFirstLCN = Fat32RootDirectoryFirstSector + \
			((fat32Bpb.RootEntries * 32) / 512);//Each directory entry is 32 bytes long fixed.

		if ((fat32Bpb.RootEntries * 32) % 512) {
            Fat32FirstDataSectorOfFirstLCN++;
        }

		//Let's find total clusters that FS is aware of.- Kartha.
		
        Fat32RootSectors = (fat32Bpb.RootEntries * 32) + \
        ( (fat32Bpb.BytesPerSector - 1)/fat32Bpb.BytesPerSector);

		Fat32FatSectors = fat32Bpb.NumberOfFats * fat32Bpb.SectorsPerFat;

		Fat32DataSectors = fat32Bpb.BigNumberOfSectors - \
		(fat32Bpb.ReservedSectors + Fat32FatSectors + Fat32RootSectors);

        //NOTE: This is for entire Volume which includes FAT32 agnostic areas at the end.
		TotalClusters.QuadPart = Fat32DataSectors / fat32Bpb.SectorsPerCluster;

        //FAT32 FS aware clusters FsSizeInformation.TotalAllocationUnits.QuadPart
		pFsInfo->TotalClustersInFS.QuadPart = FsSizeInformation.TotalAllocationUnits.QuadPart;
		pFsInfo->SizeOfFsBlockInBytes = fat32Bpb.SectorsPerCluster * fat32Bpb.BytesPerSector;
		pFsInfo->SectorOffsetOfFirstDataLCN.QuadPart = Fat32FirstDataSectorOfFirstLCN;
		pFsInfo->SectorsPerCluster = fat32Bpb.SectorsPerCluster;
		pFsInfo->BytesPerSector = fat32Bpb.BytesPerSector;

		pFsInfo->MaxVolumeByteOffsetForFs = (Fat32FirstDataSectorOfFirstLCN * fat32Bpb.BytesPerSector) + \
		(FsSizeInformation.TotalAllocationUnits.QuadPart* fat32Bpb.SectorsPerCluster * fat32Bpb.BytesPerSector);

		Status = STATUS_SUCCESS;
        goto ClusterOffsetEpilogue;
			

    break;
	
	default:

	break;
		
  }


  
  ClusterOffsetEpilogue:

	if(volumeHandle)ZwClose(volumeHandle);

  return Status;
}

#ifdef VOLUME_NO_REBOOT_SUPPORT
VOID GetDeviceNumberAndSignature(PDEV_CONTEXT pDevContext)
{
    
   
    KEVENT                          Event;
    NTSTATUS                        Status;
    PIRP                            tempIrp;
    IO_STATUS_BLOCK                 IoStatus;
	BOOLEAN                         bLoopAlwaysFalse = FALSE;

    KeInitializeEvent(&Event, NotificationEvent, FALSE);

#if (NTDDI_VERSION >= NTDDI_VISTA)

    PDISK_GEOMETRY_EX               DiskGeometry = NULL;
    do {
        DiskGeometry = (PDISK_GEOMETRY_EX)ExAllocatePoolWithTag(PagedPool, 
                                                             sizeof(DISK_GEOMETRY_EX) + sizeof(DISK_PARTITION_INFO ) + sizeof(DISK_DETECTION_INFO),                                                                 TAG_GENERIC_NON_PAGED);
        if(DiskGeometry ) {

            PDISK_PARTITION_INFO DiskParInfo = NULL;

            tempIrp = IoBuildDeviceIoControlRequest(IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, pDevContext->TargetDeviceObject, NULL, 0, 
                     DiskGeometry, sizeof(DISK_GEOMETRY_EX) + sizeof(DISK_PARTITION_INFO ) + sizeof(DISK_DETECTION_INFO), FALSE, &Event, &IoStatus);

            if(!tempIrp)
              break;

            Status = IoCallDriver(pDevContext->TargetDeviceObject, tempIrp);
            if (Status == STATUS_PENDING) {
              KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
              Status = IoStatus.Status;
            }

            if(NT_SUCCESS(Status)) {
                DiskParInfo = (PDISK_PARTITION_INFO)DiskGeometry->Data;

                if(PARTITION_STYLE_MBR == DiskParInfo->PartitionStyle) {
                 pDevContext->ulDiskSignature = DiskParInfo->Mbr.Signature;
                 pDevContext->PartitionStyle = ecPartStyleMBR;
                } else if (PARTITION_STYLE_GPT == DiskParInfo->PartitionStyle) {
                  RtlCopyMemory(&pDevContext->DiskId, &DiskParInfo->Gpt.DiskId, sizeof(GUID));       //gpt disk
                  pDevContext->PartitionStyle = ecPartStyleGPT;
                }
            }
        } else {
            InVolDbgPrint(DL_ERROR, DM_CLUSTER, ("GetDeviceNumberAndSignature: Failed to allocate paged memory\n\n"));
        }
     } while(bLoopAlwaysFalse); //Fixed W4 Warnings. AI P3:- Code need to be rewritten to get rid of loop
     if(DiskGeometry)
         ExFreePoolWithTag(DiskGeometry, TAG_GENERIC_NON_PAGED);
    //get the signature or guid
#else
    STORAGE_DEVICE_NUMBER           StDevNum;
    do{
        tempIrp = IoBuildDeviceIoControlRequest(IOCTL_STORAGE_GET_DEVICE_NUMBER, pDevContext->TargetDeviceObject, NULL, 0, 
                &StDevNum, sizeof(STORAGE_DEVICE_NUMBER), FALSE, &Event, &IoStatus);

        if(!tempIrp)
            break;

        Status = IoCallDriver(pDevContext->TargetDeviceObject, tempIrp);
        if (Status == STATUS_PENDING) {
            KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
            Status = IoStatus.Status;
        }

        if(NT_SUCCESS(Status)) {
            pDevContext->ulDiskNumber = StDevNum.DeviceNumber;
        }
    }while(bLoopAlwaysFalse); //Fixed W4 Warnings. AI P3:- Code need to be rewritten to get rid of loop
    //get the disk number
#endif
}


VOID GetVolumeGuidAndDriveLetter(PDEV_CONTEXT pDevContext)
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
	int                             i = 0;



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


        tempIrp = IoBuildDeviceIoControlRequest(IOCTL_MOUNTDEV_QUERY_DEVICE_NAME, pDevContext->TargetDeviceObject, NULL, 0, 
                    MountDevName, sizeof(MOUNTDEV_NAME)+ MountDevNameLength, FALSE, &Event, &IoStatus);
        
        if(!tempIrp)
            break;
        
        Status = IoCallDriver(pDevContext->TargetDeviceObject, tempIrp);
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
                    if(!(pDevContext->ulFlags & DCF_DEVID_OBTAINED)) {
                        pDevContext->UniqueVolumeName.Buffer = pDevContext->BufferForUniqueVolumeName;
                        pDevContext->UniqueVolumeName.MaximumLength = 100;
                        pDevContext->UniqueVolumeName.Length = 0x60;
                        RtlCopyMemory(pDevContext->UniqueVolumeName.Buffer, (char *)MountMgrMountPoints + Offset,pDevContext->UniqueVolumeName.Length);
                        RtlCopyMemory(pDevContext->wDevID, pDevContext->UniqueVolumeName.Buffer + 0xb, GUID_SIZE_IN_BYTES);
                        pDevContext->ulFlags |= DCF_DEVID_OBTAINED;
                    } else {
                        if (GUID_SIZE_IN_BYTES != RtlCompareMemory(VolumeGUID, pDevContext->wDevID, GUID_SIZE_IN_BYTES)) {
                            PDEVICE_ID    pDeviceID = NULL;
                            
                            InVolDbgPrint(DL_INFO, DM_CLUSTER, ("GetVolumeGuidAndDriveLetter: Found more than one GUID for volume\n%S\n%S\n",
                                                        pDevContext->wDevID, VolumeGUID));
                    
                            pDeviceID = pDevContext->GUIDList;
                            while(NULL != pDeviceID) {
                                if (GUID_SIZE_IN_BYTES == RtlCompareMemory(VolumeGUID, pDeviceID->GUID, GUID_SIZE_IN_BYTES)) {
                                    InVolDbgPrint(DL_INFO, DM_CLUSTER, ("GetVolumeGuidAndDriveLetter: Found same GUID again\n"));
                                break;
                            }
                            pDeviceID = pDeviceID->NextGUID;
                        }

                        if (NULL == pDeviceID) {
                            pDeviceID = (PDEVICE_ID)ExAllocatePoolWithTag(NonPagedPool, sizeof(DEVICE_ID), TAG_GENERIC_NON_PAGED);
                            if (NULL != pDeviceID) {
                                InterlockedIncrement(&DriverContext.lAdditionalGUIDs);
                                pDevContext->lAdditionalGUIDs++;
                                RtlZeroMemory(pDeviceID, sizeof(DEVICE_ID));
                                RtlCopyMemory(pDeviceID->GUID, VolumeGUID, GUID_SIZE_IN_BYTES);
                                pDeviceID->NextGUID = pDevContext->GUIDList;
                                pDevContext->GUIDList = pDeviceID;
                            } else {
                                InVolDbgPrint(DL_INFO, DM_CLUSTER, ("GetVolumeGuidAndDriveLetter: Failed to allocate VOLUMEGUID Struct\n"));
                            }
                        }

                    } else {
                        InVolDbgPrint(DL_INFO, DM_CLUSTER, ("GetVolumeGuidAndDriveLetter: Found same GUID again %S\n", VolumeGUID));
                    }

                    
                    }

                } else if(Length == 0x1c){
                    if(!(pDevContext->ulFlags & DCF_VOLUME_LETTER_OBTAINED)) {

                        SetBitRepresentingDriveLetter(*((char *)MountMgrMountPoints + Offset + 0x18), 
                                              &pDevContext->DriveLetterBitMap);
                        pDevContext->wDevNum[0] = *((char *)MountMgrMountPoints + Offset + 0x18);
#if DBG
                        pDevContext->chDevNum[0] = (char)pDevContext->wDevNum[0];
#endif
                        pDevContext->ulFlags |= DCF_VOLUME_LETTER_OBTAINED;

                    }
                }
            }
        
            RtlCopyUnicodeString(&pDevContext->DevParameters, &DriverContext.DriverParameters);
            VolumeNameWithGUID.MaximumLength = VolumeNameWithGUID.Length = VOLUME_NAME_SIZE_IN_BYTES;
            VolumeNameWithGUID.Buffer = &pDevContext->UniqueVolumeName.Buffer[VOLUME_NAME_START_INDEX_IN_MOUNTDEV_NAME];
            RtlAppendUnicodeStringToString(&pDevContext->DevParameters, &VolumeNameWithGUID);


       }


    }while(i < 0);

    
    if(MountDevName)
        ExFreePoolWithTag(MountDevName, TAG_GENERIC_PAGED);

    
    if(MountMgrMountPoint)
        ExFreePoolWithTag(MountMgrMountPoint, TAG_GENERIC_PAGED);

    
    if(MountMgrMountPoints)
        ExFreePoolWithTag(MountMgrMountPoints, TAG_GENERIC_PAGED);






}

VOID AttachToVolumeWithoutReboot(IN PVOID Context)
{
    PWCHAR              SymbolicLinkList = (PWCHAR)Context, Buf = NULL;
    UNICODE_STRING      ustrVolumeName;
    PFILE_OBJECT        FileObj;
    PDEVICE_OBJECT      DevObj;
    
    NTSTATUS            Status;
	BOOLEAN 		    bLoopAlwaysTrue = TRUE;
    
    Buf = SymbolicLinkList;
    while(bLoopAlwaysTrue) { //Fixed W4 Warnings. AI P3:- Code need to be rewritten to set loop condition appropriately
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
#ifdef VOLUME_CLUSTER_SUPPORT
                if (DevObj->DriverObject->DriverExtension->ServiceKeyName.Length == 
                        RtlCompareMemory(DevObj->DriverObject->DriverExtension->ServiceKeyName.Buffer, L"ClusDisk", 
                        DevObj->DriverObject->DriverExtension->ServiceKeyName.Length)) {
                    if(!DriverContext.pDriverObjectForCluster) {
                        DriverContext.pDriverObjectForCluster = DevObj->DriverObject;
                        ASSERT(DriverContext.pDriverObjectForCluster);                    
                    }
                    DriverContext.IsClusterVolume = true;
                } else {
#endif
                    if (!DriverContext.pDriverObject) {
                        DriverContext.pDriverObject = DevObj->DriverObject;
                        ASSERT(DriverContext.pDriverObject);                    
                    }
#ifdef VOLUME_CLUSTER_SUPPORT
                }
#endif
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
#ifdef VOLUME_CLUSTER_SUPPORT
    if(DriverContext.pDriverObjectForCluster) {

        DriverContext.IsDispatchEntryPatched = true;
        InterlockedExchangePointer((PVOID *)&DriverContext.MajorFunctionForClusDrives[IRP_MJ_WRITE], DriverContext.pDriverObjectForCluster->MajorFunction[IRP_MJ_WRITE]);
        
        InterlockedExchangePointer((PVOID *)&DriverContext.pDriverObjectForCluster->MajorFunction[IRP_MJ_WRITE], InMageFltWrite);


        InterlockedExchangePointer((PVOID *)&DriverContext.MajorFunctionForClusDrives[IRP_MJ_FLUSH_BUFFERS], DriverContext.pDriverObjectForCluster->MajorFunction[IRP_MJ_FLUSH_BUFFERS]);
        
        InterlockedExchangePointer((PVOID *)&DriverContext.pDriverObjectForCluster->MajorFunction[IRP_MJ_FLUSH_BUFFERS], InMageFltFlush);


        InterlockedExchangePointer((PVOID *)&DriverContext.MajorFunctionForClusDrives[IRP_MJ_DEVICE_CONTROL], DriverContext.pDriverObjectForCluster->MajorFunction[IRP_MJ_DEVICE_CONTROL]);
        
        InterlockedExchangePointer((PVOID *)&DriverContext.pDriverObjectForCluster->MajorFunction[IRP_MJ_DEVICE_CONTROL], InMageFltDeviceControl);

    }
#endif
    ExFreePool(Buf);
}
#endif
