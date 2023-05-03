#include "VVCommon.h"
#include <DrvDefines.h>

#include <ntdddisk.h>
#include "VVDriverContext.h"
#include <VVCmdQueue.h>
#include "IoctlInfo.h"
#include "VVolumeRetentionLog.h"
#include "VVolumeImage.h"
#include "VVDispatch.h"
#include <VVDevControl.h>
#include <VVIDevControl.h>
#include <VsnapKernel.h>
#include "VVthreadPool.h"

extern THREAD_POOL		  *VsnapIOThreadPool;
extern THREAD_POOL_INFO	  *VsnapIOThreadPoolInfo;

extern THREAD_POOL		  *vdev_update_pool;
extern THREAD_POOL_INFO	  *vdev_update_pool_info;

VOID
VVolumeFromRetentionLogCmdRoutine(
    PDEVICE_EXTENSION DeviceExtension, 
    PCOMMAND_STRUCT pCommand
)
{
    NTSTATUS Status = STATUS_SUCCESS;

    switch(pCommand->ulCommand) {
    case VVOLUME_CMD_READ:
    case VVOLUME_CMD_WRITE:
        DriverContext.VVolumeRetentionCmdRoutines[pCommand->ulCommand](DeviceExtension, pCommand);
        break;
    default:
        ASSERT(0);
        break;
    }

    return;
}

NTSTATUS  VVolumeRetentionLogWrite(    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    )
{
  NTSTATUS    Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION   DevExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    PIO_COMMAND_STRUCT     pIoCommand = NULL;
    VsnapInfoTag           *Vsnap = NULL;
    bool                bFreeTask = false;
    

    
    InVolDbgPrint(DL_VV_VERBOSE, DM_VV_WRITE, 
                  ("VVolumeRetentionLogWrite: Called - Length %#x, Offset %#I64x\n",
                   IoStackLocation->Parameters.Write.Length,
                   IoStackLocation->Parameters.Write.ByteOffset.QuadPart));

	if(DevExtension->ulFlags & FLAG_FREE_VSNAP)
	{
        InVolDbgPrint(DL_VV_ERROR, DM_VV_WRITE, ("VVolumeRetentionLogWrite: Marked For deletion\n"));
		Status = STATUS_SUCCESS;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        IoReleaseRemoveLock(&DevExtension->IoRemoveLock, Irp);        
        
        return Status;
	}
    if (0 == IoStackLocation->Parameters.Write.Length) {
        InVolDbgPrint(DL_VV_ERROR, DM_VV_WRITE, ("VVolumeRetentionLogWrite: Write Length is zero\n"));
        Status = STATUS_SUCCESS;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        IoReleaseRemoveLock(&DevExtension->IoRemoveLock, Irp);
        
        return Status;
    }
/*
    if(((struct VsnapInfoTag *)DevExtension->RetentionContext)->AccessType == VSNAP_READ_ONLY)
    {
        InVolDbgPrint(DL_VV_ERROR, DM_VV_WRITE, ("VVolumeRetentionLogWrite: Read Only Vsnap\n"));
        Status = STATUS_SUCCESS;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = IoStackLocation->Parameters.Write.Length;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        IoReleaseRemoveLock(&DevExtension->IoRemoveLock, Irp);        
        return Status;
        
    }
  */  ASSERT(NULL != Irp->MdlAddress);

    pIoCommand = AllocateIOCommand();
    if (NULL == pIoCommand) {
        InVolDbgPrint(DL_VV_ERROR, DM_VV_WRITE, ("VVolumeRetentionLogWrite: AllocateIOCommand failed\n"));
        Status = STATUS_NO_MEMORY;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        IoReleaseRemoveLock(&DevExtension->IoRemoveLock, Irp);
        
        return Status;
    }
/*
    if(DevExtension->ulDeviceType == DEVICE_TYPE_VVOLUME_FROM_RETENTION_LOG && DevExtension->CmdQueue != NULL && 
        KeGetCurrentThread() == DevExtension->CmdQueue->Thread)
    {
        pCommand->ulCommand = VVOLUME_CMD_WRITE;
        pCommand->Cmd.Write.Irp = Irp;
        pCommand->Cmd.Write.ulLength = IoStackLocation->Parameters.Write.Length;
        pCommand->Cmd.Write.ByteOffset.QuadPart = IoStackLocation->Parameters.Write.ByteOffset.QuadPart;

        DriverContext.VVolumeRetentionCmdRoutines[VVOLUME_CMD_WRITE](DevExtension, pCommand);
        Status = pCommand->CmdStatus;
        DereferenceCommand(pCommand);
        return Status;
    }
*/

    IoMarkIrpPending(Irp);     
    pIoCommand->ulCommand = VVOLUME_CMD_WRITE;
    pIoCommand->Cmd.Write.Irp = Irp;
    pIoCommand->Cmd.Write.ulLength = IoStackLocation->Parameters.Write.Length;
    pIoCommand->Cmd.Write.ByteOffset.QuadPart = IoStackLocation->Parameters.Write.ByteOffset.QuadPart;
    Vsnap = pIoCommand->Vsnap = (struct VsnapInfoTag *)DevExtension->VolumeContext;
#if DBG
    pIoCommand->Vsnap->ullNumberOfWrites++;
#endif
    
  
    
    VSNAP_TASK     *task = AllocateTask();
    if(!task) {
        InVolDbgPrint(DL_VV_ERROR, DM_VV_WRITE, ("VVolumeRetentionLogWrite: AllocateTask failed Write Command Queued in local queue\n"));
               
        ExAcquireFastMutex(&Vsnap->hMutex); 
        //InterlockedIncrement(&pCommand->Vsnap->IOPending);
        Vsnap->IOPending++;
        QueueIOCmdToCmdQueue(DevExtension->IOCmdQueue, pIoCommand);
        ExReleaseFastMutex(&Vsnap->hMutex);
    
        DereferenceIOCommand(pIoCommand);
        return STATUS_PENDING;
    }

        
    RtlZeroMemory(task, sizeof(VSNAP_TASK));
    task->usTaskId = TASK_WRITE;
    task->TaskData = (PVOID)pIoCommand;
    
    ExAcquireFastMutex(&Vsnap->hMutex);    
    if(0 == Vsnap->IOPending) {            
        if((Vsnap->IOWriteInProgress == false) &&(0 == Vsnap->IOsInFlight)) {
            Vsnap->IOWriteInProgress = true;
            Vsnap->IOsInFlight++;
            ReferenceIOCommand(pIoCommand);
            ADD_TASK_TO_POOL(VsnapIOThreadPool, task);
        } else {
            QueueIOCmdToCmdQueue(DevExtension->IOCmdQueue, pIoCommand);
            Vsnap->IOPending++;
            bFreeTask = true;
        }
    } else {
        QueueIOCmdToCmdQueue(DevExtension->IOCmdQueue, pIoCommand);
        Vsnap->IOPending++;
        bFreeTask = true;
    }
        
    ExReleaseFastMutex(&Vsnap->hMutex);

    if(bFreeTask) {
        
        DeAllocateTask(task);
    }
     
    DereferenceIOCommand(pIoCommand);
     
    
    return STATUS_PENDING;
}


NTSTATUS   VVolumeRetentionLogRead( IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    )
{
    NTSTATUS    Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION   DevExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    PIO_COMMAND_STRUCT     pIoCommand = NULL;
    VsnapInfoTag           *Vsnap = NULL;
    bool bFreeTask = false;

    
    InVolDbgPrint(DL_VV_VERBOSE, DM_VV_READ, 
                  ("VVolumeRetentionLogRead: Called - Length %#x, Offset %#I64x\n",
                   IoStackLocation->Parameters.Read.Length,
                   IoStackLocation->Parameters.Read.ByteOffset.QuadPart));

    if (0 == IoStackLocation->Parameters.Read.Length) {
        Status = STATUS_SUCCESS;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        InVolDbgPrint(DL_VV_ERROR, DM_VV_READ, ("VVolumeRetentionLogRead: Read Length is zero\n"));
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        IoReleaseRemoveLock(&DevExtension->IoRemoveLock, Irp);
        
        return Status;
    }

    if (DevExtension->ulFlags & FLAG_FREE_VSNAP){
        Status = STATUS_SUCCESS;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Information = IoStackLocation->Parameters.Read.Length;
        InVolDbgPrint(DL_VV_ERROR, DM_VV_READ, ("VVolumeRetentionLogRead: Vsnap marked for deletion\n"));        
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        IoReleaseRemoveLock(&DevExtension->IoRemoveLock, Irp);
        
        return Status;
    }

    ASSERT(NULL != Irp->MdlAddress);

    pIoCommand = AllocateIOCommand();
    if (NULL == pIoCommand) {
        InVolDbgPrint(DL_VV_ERROR, DM_VV_READ, ("VVolumeRetentionLogRead: AllocateIOCommand failed\n"));
        Status = STATUS_NO_MEMORY;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        IoReleaseRemoveLock(&DevExtension->IoRemoveLock, Irp);
        
        return Status;
    }

/*
    if(DevExtension->ulDeviceType == DEVICE_TYPE_VVOLUME_FROM_RETENTION_LOG && DevExtension->CmdQueue != NULL && 
        KeGetCurrentThread() == DevExtension->CmdQueue->Thread)
    {
        pCommand->ulCommand = VVOLUME_CMD_READ;
        pCommand->Cmd.Read.Irp = Irp;
        pCommand->Cmd.Read.ulLength = IoStackLocation->Parameters.Read.Length;
        pCommand->Cmd.Read.ByteOffset.QuadPart = IoStackLocation->Parameters.Read.ByteOffset.QuadPart;

        DriverContext.VVolumeRetentionCmdRoutines[VVOLUME_CMD_READ](DevExtension, pCommand);
        Status = pCommand->CmdStatus;
        DereferenceCommand(pCommand);
        return Status;
    }
*/

    IoMarkIrpPending(Irp); 
    pIoCommand->ulCommand = VVOLUME_CMD_READ;
    pIoCommand->Cmd.Read.Irp = Irp;
    pIoCommand->Cmd.Read.ulLength = IoStackLocation->Parameters.Read.Length;
    pIoCommand->Cmd.Read.ByteOffset.QuadPart = IoStackLocation->Parameters.Read.ByteOffset.QuadPart;
    Vsnap = pIoCommand->Vsnap = (struct VsnapInfoTag *)DevExtension->VolumeContext;
#if DBG
    pIoCommand->Vsnap->ullNumberOfReads++;
#endif
    InitializeEntryList(&pIoCommand->HeadForVsnapRetentionData);
    pIoCommand->TargetReadRequired = false;

    
    VSNAP_TASK *task = AllocateTask();
    if(!task) {
        InVolDbgPrint(DL_VV_ERROR, DM_VV_READ, ("VVolumeRetentionLogRead: AllocateTask failed Read Command Queued in local queue\n"));
             
        ExAcquireFastMutex(&Vsnap->hMutex); 
        //InterlockedIncrement(&pCommand->Vsnap->IOPending);
        Vsnap->IOPending++;
        QueueIOCmdToCmdQueue(DevExtension->IOCmdQueue, pIoCommand);
        ExReleaseFastMutex(&Vsnap->hMutex); 

        
        DereferenceIOCommand(pIoCommand);
        return STATUS_PENDING;
    }
    
    
    RtlZeroMemory(task, sizeof(VSNAP_TASK));
    task->usTaskId = TASK_BTREE_LOOKUP;
    task->TaskData = (PVOID)pIoCommand;
    

    ExAcquireFastMutex(&Vsnap->hMutex); 
    if(0 == Vsnap->IOPending) {    
        if(Vsnap->IOWriteInProgress == false) {
            Vsnap->IOsInFlight++;
            ReferenceIOCommand(pIoCommand);
            ADD_TASK_TO_POOL(VsnapIOThreadPool, task);
        } else {
            QueueIOCmdToCmdQueue(DevExtension->IOCmdQueue, pIoCommand);
            Vsnap->IOPending++;
            bFreeTask = true;
        }
    } else {
        QueueIOCmdToCmdQueue(DevExtension->IOCmdQueue, pIoCommand);
        Vsnap->IOPending++;
        bFreeTask = true;
    }
    ExReleaseFastMutex(&Vsnap->hMutex); 
    
    
    if(bFreeTask) {
        
        DeAllocateTask(task);
    }

    DereferenceIOCommand(pIoCommand);

    
    



    return STATUS_PENDING;

}
VOID
VVolumeRetentionProcessReadRequest(PDEVICE_EXTENSION DevExtension, PCOMMAND_STRUCT pCommand)
{
    PVOID           pReadBuffer;
    ULONG           BytesRead; 
    PIRP            Irp = pCommand->Cmd.Read.Irp;
    IO_STATUS_BLOCK IoStatus;
    NTSTATUS        Status = STATUS_SUCCESS;
    int iSuccess = true;

    pReadBuffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
    if (NULL == pReadBuffer) {
        Irp->IoStatus.Status = pCommand->CmdStatus = STATUS_INSUFFICIENT_RESOURCES;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        IoReleaseRemoveLock(&DevExtension->IoRemoveLock, Irp);
        return;
    }

    //iSuccess = VsnapVolumeRead(pReadBuffer, (ULONGLONG)pCommand->Cmd.Read.ByteOffset.QuadPart, 
      //  pCommand->Cmd.Read.ulLength, &BytesRead, DevExtension->RetentionContext);
    
    if(!iSuccess)
    {
        Status = STATUS_UNSUCCESSFUL;
        BytesRead = 0;
    }
    
    Irp->IoStatus.Status = pCommand->CmdStatus = Status;
    Irp->IoStatus.Information = BytesRead;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    IoReleaseRemoveLock(&DevExtension->IoRemoveLock, Irp);

    return;
}
NTSTATUS
VVHandleDeviceControlForVolumeRetentionDevice(
    PDEVICE_OBJECT  DeviceObject,
    PIRP            Irp
    )
{
    NTSTATUS    Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION   DevExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulIoControlCode;

    ASSERT(DEVICE_TYPE_VVOLUME_FROM_RETENTION_LOG == DevExtension->ulDeviceType);

    Status = VVHandleDeviceControlForCommonIOCTLCodes(DeviceObject, Irp);
    if (STATUS_INVALID_DEVICE_REQUEST != Status) {
        return Status;
    }

    ulIoControlCode = IoStackLocation->Parameters.DeviceIoControl.IoControlCode;
    InVolDbgPrint(DL_VV_INFO, DM_VV_DEVICE_CONTROL, 
                  ("VVHandleDeviceControlForVolumeRetentionDevice: ulIoControlCode = %#x\n", ulIoControlCode));

    switch(ulIoControlCode) {
    case IOCTL_MOUNTDEV_LINK_DELETED:
        {
            PMOUNTDEV_NAME pMountDevName;
            pMountDevName = (PMOUNTDEV_NAME) Irp->AssociatedIrp.SystemBuffer;
            InVolDbgPrint(DL_VV_WARNING, DM_VV_DEVICE_CONTROL, 
                      ("VVHandleDeviceControlForVolumeImageDevice: IOCTL_MOUNTDEV_LINK_DELETED\n"));

            if(DevExtension->usMountDevDeviceName.Buffer)
            {
                FREE_MEMORY(DevExtension->usMountDevDeviceName.Buffer);
                RtlInitUnicodeString(&DevExtension->usMountDevDeviceName, NULL);
            }

            DevExtension->ulFlags &= ~VV_DE_MOUNT_DOS_DEVICE_NAME_CREATED;

            break;
        }
    case IOCTL_MOUNTDEV_LINK_CREATED_W2K:
    case IOCTL_MOUNTDEV_LINK_CREATED_WNET:
        {
            PMOUNTDEV_NAME      pMountDevName;
            pMountDevName = (PMOUNTDEV_NAME) Irp->AssociatedIrp.SystemBuffer;
            // The MOUNTDEV_NAME members are not NULL-terminated. Allocate space for NULL character also.
            if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength >= 0x62)
            {
                if (INMAGE_IS_MOUNTDEV_NAME_VOLUME_GUID(pMountDevName))
                {
                    if(!(DevExtension->ulFlags & VV_DE_MOUNT_LINK_CREATED))
                    {   
                        DevExtension->usMountDevLink.Buffer = (PWCHAR)ALLOC_MEMORY((pMountDevName->NameLength + sizeof(WCHAR)), NonPagedPool);
                        if(NULL == DevExtension->usMountDevLink.Buffer)
                            break;
                        DevExtension->usMountDevLink.Length = pMountDevName->NameLength;
                        DevExtension->usMountDevLink.MaximumLength = pMountDevName->NameLength + sizeof(WCHAR);
                        RtlCopyMemory(DevExtension->usMountDevLink.Buffer, pMountDevName->Name, pMountDevName->NameLength);
                        DevExtension->usMountDevLink.Buffer[DevExtension->usMountDevLink.Length / sizeof(WCHAR)] = L'\0';
                        InVolDbgPrint(DL_VV_INFO, DM_VV_DEVICE_CONTROL, 
                            ("VVHandleDeviceControlForVolumeImageDevice: IOCTL_MOUNTDEV_LINK_CREATED_WNET :%wZ\n", &DevExtension->usMountDevLink));
                        DevExtension->ulFlags |= VV_DE_MOUNT_LINK_CREATED;
                        InVirVolLogError(DriverContext.ControlDevice, __LINE__, INVIRVOL_VSNAP_MOUNT_SUCCESS, STATUS_SUCCESS,
                                         &DevExtension->usMountDevLink.Buffer[DEV_LINK_GUID_OFFSET], 
                                         GUID_LEN_WITH_BRACE);
                    }
                }
                else
                    ASSERT(0);
            }
            else
            {
                if(!(DevExtension->ulFlags & VV_DE_MOUNT_DOS_DEVICE_NAME_CREATED))
                {
                    DevExtension->usMountDevDeviceName.Buffer = (PWCHAR)ALLOC_MEMORY((pMountDevName->NameLength + sizeof(WCHAR)), NonPagedPool);
                    if(NULL == DevExtension->usMountDevDeviceName.Buffer)
                        break;
                    DevExtension->usMountDevDeviceName.Length = pMountDevName->NameLength;
                    DevExtension->usMountDevDeviceName.MaximumLength = pMountDevName->NameLength + sizeof(WCHAR);
                    RtlCopyMemory(DevExtension->usMountDevDeviceName.Buffer, pMountDevName->Name, pMountDevName->NameLength);
                    DevExtension->usMountDevDeviceName.Buffer[DevExtension->usMountDevDeviceName.Length / sizeof(WCHAR)] = L'\0';
                    InVolDbgPrint(DL_VV_WARNING, DM_VV_DEVICE_CONTROL, 
                        ("VVHandleDeviceControlForVolumeImageDevice: IOCTL_MOUNTDEV_LINK_CREATED_WNET :%wZ\n", &DevExtension->usMountDevDeviceName));
                    DevExtension->ulFlags |= VV_DE_MOUNT_DOS_DEVICE_NAME_CREATED;
                }
            }
            break;
        }
    case IOCTL_MOUNTDEV_QUERY_DEVICE_NAME:
        {
            InVolDbgPrint(DL_VV_WARNING, DM_VV_DEVICE_CONTROL, 
                      ("VVHandleDeviceControlForVolumeImageDevice: IOCTL_MOUNTDEV_QUERY_DEVICE_NAME\n"));

            PMOUNTDEV_NAME pMountDevName = NULL;

            if(IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(MOUNTDEV_NAME))
            {
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            pMountDevName = (PMOUNTDEV_NAME) Irp->AssociatedIrp.SystemBuffer;
            RtlZeroMemory(pMountDevName, IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength);
            pMountDevName->NameLength = DevExtension->usDeviceName.Length;

            if(IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(USHORT) + DevExtension->usDeviceName.Length)
            {

                Irp->IoStatus.Information = sizeof(MOUNTDEV_NAME);
                Status = STATUS_BUFFER_OVERFLOW;
                break;
            }

            RtlCopyMemory(pMountDevName->Name, DevExtension->usDeviceName.Buffer, DevExtension->usDeviceName.Length);
            Status = STATUS_SUCCESS;
            Irp->IoStatus.Information = sizeof(USHORT) + DevExtension->usDeviceName.Length;
            break;
        }
#if (_WIN32_WINNT >= 0x0501)
    case IOCTL_MOUNTDEV_QUERY_STABLE_GUID:
#endif
    case IOCTL_MOUNTDEV_QUERY_UNIQUE_ID:
        {
            InVolDbgPrint(DL_VV_WARNING, DM_VV_DEVICE_CONTROL, 
                      ("VVHandleDeviceControlForVolumeImageDevice: IOCTL_MOUNTDEV_QUERY_UNIQUE_ID\n"));

            if(IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(MOUNTDEV_UNIQUE_ID))
            {
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            PMOUNTDEV_UNIQUE_ID pUniqueId = (PMOUNTDEV_UNIQUE_ID) Irp->AssociatedIrp.SystemBuffer;
            RtlZeroMemory(pUniqueId, IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength);

            pUniqueId->UniqueIdLength = DevExtension->MountDevUniqueId.Length;
            
            if(IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < (pUniqueId->UniqueIdLength + sizeof(USHORT)))
            {
                Irp->IoStatus.Information = sizeof(MOUNTDEV_UNIQUE_ID);
                Status = STATUS_BUFFER_OVERFLOW;
                break;
            }

            RtlCopyMemory(pUniqueId->UniqueId, DevExtension->MountDevUniqueId.Buffer, DevExtension->MountDevUniqueId.Length);
            Status = STATUS_SUCCESS;
            Irp->IoStatus.Information = sizeof(USHORT) + DevExtension->MountDevUniqueId.Length;
            break;
        }
    case IOCTL_MOUNTDEV_QUERY_SUGGESTED_LINK_NAME:
        {
            Status = STATUS_NOT_SUPPORTED;
            InVolDbgPrint(DL_VV_WARNING, DM_VV_DEVICE_CONTROL, 
                      ("VVHandleDeviceControlForVolumeImageDevice: IOCTL_MOUNTDEV_QUERY_SUGGESTED_LINK_NAME\n"));
            break;
        }
	// ToDo : Documentation says that following ioctl is not supported starting windows 7, but with WDK 7.1..the ioctl is failed for windows 2003 
    //and windows vista builds as well. Ideally this ioctl should not be issued on Windows Server 2003 with Service Pack 2 (SP2) and 
	//Windows Vista with Service Pack 1 (SP1). So We need to test what is the beaviour on plain prior versions. For now, it is comments to get the build roleout
    //case IOCTL_MOUNTDEV_UNIQUE_ID_CHANGE_NOTIFY:
    //    {
    //        PMOUNTDEV_UNIQUE_ID_CHANGE_NOTIFY_OUTPUT pMountDevUniquedIdChangeNotify = NULL;

    //        Status = STATUS_NOT_SUPPORTED;

    //        InVolDbgPrint(DL_VV_WARNING, DM_VV_DEVICE_CONTROL, 
    //                  ("VVHandleDeviceControlForVolumeImageDevice: IOCTL_MOUNTDEV_UNIQUE_ID_CHANGE_NOTIFY\n"));
    //        break;
    //    }
    case IOCTL_VOLUME_ONLINE:
        Status = STATUS_SUCCESS;
        break;
    default:
        InVolDbgPrint(DL_VV_WARNING, DM_VV_DEVICE_CONTROL, 
                      ("VVHandleDeviceControlForVolumeRetentionDevice: Failing IOCTL %#x with STATUS_INVALID_DEVICE_REQUEST\n",
                       ulIoControlCode));
        Status = STATUS_INVALID_DEVICE_REQUEST;
        Irp->IoStatus.Information = 0;
        break;
    }

    Irp->IoStatus.Status = Status;
    if (STATUS_PENDING != Status) {
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        IoReleaseRemoveLock(&DevExtension->IoRemoveLock, Irp);
    }

    return Status;
}

VOID
VVolumeRetentionProcessWriteRequest(PDEVICE_EXTENSION DevExtension, PCOMMAND_STRUCT pCommand)
{
    NTSTATUS        Status = STATUS_SUCCESS;
    PVOID           pWriteBuffer;
    IO_STATUS_BLOCK IoStatus;
    PIRP            Irp = pCommand->Cmd.Write.Irp;
    KIRQL           OldIrql;
	ULONG			BytesWritten = 0;
	int				iSuccess = true;

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = Status;

    if(pCommand->Cmd.Write.ulLength != 0)
    {
        pWriteBuffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
        if (NULL == pWriteBuffer) {
            Irp->IoStatus.Status = pCommand->CmdStatus = STATUS_INSUFFICIENT_RESOURCES;
            Irp->IoStatus.Information = 0;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            IoReleaseRemoveLock(&DevExtension->IoRemoveLock, Irp);
            return;
        }
		
        //if(pCommand->Cmd.Write.ByteOffset.QuadPart < 512)   //Boot Sector
        //{
        //    //Replicated volume length could be different than the target volume Legnth.
        //    //Calculate new volume Length.
        //    DevExtension->uliVolumeSize.QuadPart = *(PLONGLONG ((PCHAR)pWriteBuffer + 40)) * 512;
        //}

        iSuccess =  VsnapVolumeWrite(pWriteBuffer, (ULONGLONG)pCommand->Cmd.Write.ByteOffset.QuadPart, 
			pCommand->Cmd.Write.ulLength, &BytesWritten, DevExtension->VolumeContext);

		if(!iSuccess)
		{
			Status = STATUS_UNSUCCESSFUL;
			BytesWritten = 0;
		}

        Irp->IoStatus.Status = pCommand->CmdStatus = Status;
        Irp->IoStatus.Information = BytesWritten;
    }

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    IoReleaseRemoveLock(&DevExtension->IoRemoveLock, Irp);
    return;
}
