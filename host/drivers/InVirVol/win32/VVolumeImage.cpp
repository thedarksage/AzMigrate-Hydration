#include <VVCommon.h>
#include <VVCmdQueue.h>
#include <VVolumeImage.h>
#include <ntdddisk.h>
#include <VVDriverContext.h>
#include "VVDispatch.h"
#include "IoctlInfo.h"
#include "VVDevControl.h"


extern THREAD_POOL		*VVolumeIOThreadPool ;
extern THREAD_POOL_INFO	*VVolumeIOThreadPoolInfo ;

#define FILE_DEVICE_FILE_SYSTEM         0x00000009
#define FSCTL_SET_ZERO_DATA             CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 50, METHOD_BUFFERED, FILE_WRITE_DATA) // FILE_ZERO_DATA_INFORMATION,

NTSTATUS
VVSendMountMgrVolumeArrivalNotification(PDEVICE_EXTENSION DeviceExtension);

NTSTATUS
VVolumeImageRead(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    NTSTATUS    Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION   DevExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    PIO_COMMAND_STRUCT     pIoCommand = NULL;
    bool                bFreeTask = false;
    VVolumeInfoTag      *VVolume = NULL;
    


    InVolDbgPrint(DL_VV_VERBOSE, DM_VV_READ, 
                  ("VVolumeImageRead: Called - Length %#x, Offset %#I64x\n",
                   IoStackLocation->Parameters.Read.Length,
                   IoStackLocation->Parameters.Read.ByteOffset.QuadPart));

    if (0 == IoStackLocation->Parameters.Read.Length) {
        Status = STATUS_SUCCESS;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        IoReleaseRemoveLock(&DevExtension->IoRemoveLock, Irp);
        InVolDbgPrint(DL_VV_ERROR, DM_VV_READ, ("VVolumeImageRead: Length is zero\n"));
        return Status;
    }

    ASSERT(NULL != Irp->MdlAddress);

    pIoCommand = AllocateIOCommand();
    if (NULL == pIoCommand) {
        Status = STATUS_NO_MEMORY;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        IoReleaseRemoveLock(&DevExtension->IoRemoveLock, Irp);
        InVolDbgPrint(DL_VV_ERROR, DM_VV_READ, ("VVolumeImageRead: AllocateCommand failed\n"));
        return Status;
    }

    IoMarkIrpPending(Irp);
    pIoCommand->ulCommand = VVOLUME_CMD_READ;
    pIoCommand->Cmd.Read.Irp = Irp;
    pIoCommand->Cmd.Read.ulLength = IoStackLocation->Parameters.Read.Length;
    pIoCommand->Cmd.Read.ByteOffset.QuadPart = IoStackLocation->Parameters.Read.ByteOffset.QuadPart;
    VVolume = pIoCommand->VVolume = (struct VVolumeInfoTag *)DevExtension->VolumeContext;
    pIoCommand->ulFlags |= FLAG_VIRTUAL_VOLUME;

  /*  if(DevExtension->ulDeviceType == DEVICE_TYPE_VVOLUME_FROM_RETENTION_LOG && DevExtension->CmdQueue != NULL && 
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
    VSNAP_TASK     *task = AllocateTask();
    if(!task) {
        InVolDbgPrint(DL_VV_ERROR, DM_VV_WRITE, ("VVolumeRetentionLogWrite: AllocateTask failed Write Command Queued in local queue\n"));
             
        ExAcquireFastMutex(&VVolume->hMutex);    
        VVolume->IOPending++;
        QueueIOCmdToCmdQueue(DevExtension->IOCmdQueue, pIoCommand);
        ExReleaseFastMutex(&VVolume->hMutex);
    
        DereferenceIOCommand(pIoCommand);
        return STATUS_PENDING;
    }

    RtlZeroMemory(task, sizeof(VSNAP_TASK));
    task->usTaskId = TASK_READ_VVOLUME;
    task->TaskData = (PVOID)pIoCommand;
    

    ExAcquireFastMutex(&VVolume->hMutex);    
    if(0 == VVolume->IOPending) {    
        if(VVolume->IOWriteInProgress == false) {
            VVolume->IOsInFlight++;
            ReferenceIOCommand(pIoCommand);
            ADD_TASK_TO_POOL(VVolumeIOThreadPool, task);
        } else {
            QueueIOCmdToCmdQueue(DevExtension->IOCmdQueue, pIoCommand);
            VVolume->IOPending++;
            bFreeTask = true;
        }
    } else {
        QueueIOCmdToCmdQueue(DevExtension->IOCmdQueue, pIoCommand);
        VVolume->IOPending++;
        bFreeTask = true;
    }
    ExReleaseFastMutex(&VVolume->hMutex);
    
    
    if(bFreeTask) {
        
        DeAllocateTask(task);
    }
    DereferenceIOCommand(pIoCommand);
       
    return STATUS_PENDING;
}

NTSTATUS
VVolumeImageWrite(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    NTSTATUS    Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION   DevExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    PIO_COMMAND_STRUCT     pIoCommand = NULL;
    bool                bFreeTask = false;
    VVolumeInfoTag      *VVolume = NULL;
    
    InVolDbgPrint(DL_VV_VERBOSE, DM_VV_WRITE, 
                  ("VVolumeImageWrite: Called - Length %#x, Offset %#I64x\n",
                   IoStackLocation->Parameters.Write.Length,
                   IoStackLocation->Parameters.Write.ByteOffset.QuadPart));

	if( DevExtension->ulFlags & VVOLUME_FLAG_READ_ONLY)
	{
		Status = STATUS_SUCCESS;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        IoReleaseRemoveLock(&DevExtension->IoRemoveLock, Irp);        
        return Status;
	}
    if (0 == IoStackLocation->Parameters.Write.Length) {
        Status = STATUS_SUCCESS;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        IoReleaseRemoveLock(&DevExtension->IoRemoveLock, Irp);
        InVolDbgPrint(DL_VV_ERROR, DM_VV_WRITE, ("VVolumeImageWrite: Length is zero\n"));
        return Status;
    }

    ASSERT(NULL != Irp->MdlAddress);

    pIoCommand = AllocateIOCommand();
    if (NULL == pIoCommand) {
        Status = STATUS_NO_MEMORY;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        IoReleaseRemoveLock(&DevExtension->IoRemoveLock, Irp);
        InVolDbgPrint(DL_VV_ERROR, DM_VV_WRITE, ("VVolumeImageWrite: AllocateCommand failed\n"));
        return Status;
    }

    
    IoMarkIrpPending(Irp);
    pIoCommand->ulCommand = VVOLUME_CMD_WRITE;
    pIoCommand->Cmd.Write.Irp = Irp;
    pIoCommand->Cmd.Write.ulLength = IoStackLocation->Parameters.Write.Length;
    pIoCommand->Cmd.Write.ByteOffset.QuadPart = IoStackLocation->Parameters.Write.ByteOffset.QuadPart;
    VVolume = pIoCommand->VVolume = (struct VVolumeInfoTag *)DevExtension->VolumeContext;
    pIoCommand->ulFlags |= FLAG_VIRTUAL_VOLUME;

/*    if(DevExtension->ulDeviceType == DEVICE_TYPE_VVOLUME_FROM_RETENTION_LOG && DevExtension->CmdQueue != NULL && 
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
    
    VSNAP_TASK     *task = AllocateTask();
    if(!task) {
        InVolDbgPrint(DL_VV_ERROR, DM_VV_WRITE, ("VVolumeRetentionLogWrite: AllocateTask failed Write Command Queued in local queue\n"));
               
        ExAcquireFastMutex(&VVolume->hMutex); 
        VVolume->IOPending++;
        QueueIOCmdToCmdQueue(DevExtension->IOCmdQueue, pIoCommand);
        ExReleaseFastMutex(&VVolume->hMutex);
        DereferenceIOCommand(pIoCommand);
        return STATUS_PENDING;
    }

        
    RtlZeroMemory(task, sizeof(VSNAP_TASK));
    task->usTaskId = TASK_WRITE_VVOLUME;
    task->TaskData = (PVOID)pIoCommand;
    
    ExAcquireFastMutex(&VVolume->hMutex);    
    if(0 == VVolume->IOPending) {            
        if((VVolume->IOWriteInProgress == false) &&(0 == VVolume->IOsInFlight)) {
            VVolume->IOWriteInProgress = true;
            VVolume->IOsInFlight++;
            ReferenceIOCommand(pIoCommand);
            ADD_TASK_TO_POOL(VVolumeIOThreadPool, task);
        } else {
            QueueIOCmdToCmdQueue(DevExtension->IOCmdQueue, pIoCommand);
            VVolume->IOPending++;
            bFreeTask = true;
        }
    } else {
        QueueIOCmdToCmdQueue(DevExtension->IOCmdQueue, pIoCommand);
        VVolume->IOPending++;
        bFreeTask = true;
    }
        
    ExReleaseFastMutex(&VVolume->hMutex);

    if(bFreeTask) {
        
        DeAllocateTask(task);
    }

    DereferenceIOCommand(pIoCommand);  


    return STATUS_PENDING;
}

NTSTATUS
VVHandleDeviceControlForVolumeImageDevice(
    PDEVICE_OBJECT  DeviceObject,
    PIRP            Irp
    )
{
    NTSTATUS    Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION   DevExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulIoControlCode;

    ASSERT(DEVICE_TYPE_VVOLUME_FROM_VOLUME_IMAGE == DevExtension->ulDeviceType);

    Status = VVHandleDeviceControlForCommonIOCTLCodes(DeviceObject, Irp);
    if (STATUS_INVALID_DEVICE_REQUEST != Status) {
        return Status;
    }

    ulIoControlCode = IoStackLocation->Parameters.DeviceIoControl.IoControlCode;
    InVolDbgPrint(DL_VV_INFO, DM_VV_DEVICE_CONTROL, 
                  ("VVHandleDeviceControlForVolumeImageDevice: ulIoControlCode = %#x\n", ulIoControlCode));
#if DBG
    DumpIoctlInformation(DeviceObject, Irp);
#endif // DBG

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

            if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength >= 0x62)
            {
                if (INMAGE_IS_MOUNTDEV_NAME_VOLUME_GUID(pMountDevName))
                {
                    if(!(DevExtension->ulFlags & VV_DE_MOUNT_LINK_CREATED))
                    {
                        DevExtension->usMountDevLink.Buffer = (PWCHAR)ALLOC_MEMORY(pMountDevName->NameLength, NonPagedPool);
                        if(NULL == DevExtension->usMountDevLink.Buffer)
                            break;
                        DevExtension->usMountDevLink.Length = pMountDevName->NameLength;
                        DevExtension->usMountDevLink.MaximumLength = pMountDevName->NameLength;
                        RtlCopyMemory(DevExtension->usMountDevLink.Buffer, pMountDevName->Name, pMountDevName->NameLength);
                        InVolDbgPrint(DL_VV_WARNING, DM_VV_DEVICE_CONTROL, 
                            ("VVHandleDeviceControlForVolumeImageDevice: IOCTL_MOUNTDEV_LINK_CREATED_WNET :%wZ\n", &DevExtension->usMountDevLink));
                        DevExtension->ulFlags |= VV_DE_MOUNT_LINK_CREATED;
                    }
                }
            }
            else
            {
                if(!(DevExtension->ulFlags & VV_DE_MOUNT_DOS_DEVICE_NAME_CREATED))
                {
                    DevExtension->usMountDevDeviceName.Buffer = (PWCHAR)ALLOC_MEMORY(pMountDevName->NameLength, NonPagedPool);
                    if(NULL == DevExtension->usMountDevDeviceName.Buffer)
                        break;
                    DevExtension->usMountDevDeviceName.Length = pMountDevName->NameLength;
                    DevExtension->usMountDevDeviceName.MaximumLength = pMountDevName->NameLength;
                    RtlCopyMemory(DevExtension->usMountDevDeviceName.Buffer, pMountDevName->Name, pMountDevName->NameLength);
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
            
            if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < (pUniqueId->UniqueIdLength + sizeof(USHORT)))
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
    case IOCTL_VOLUME_IS_OFFLINE:
        Status = VVSendMountMgrVolumeArrivalNotification(DevExtension);
        InVolDbgPrint(DL_VV_WARNING, DM_VV_DEVICE_CONTROL, 
                      ("VVSendMountMgrVolumeArrivalNotification: IOCTL_VOLUME_IS_OFFLINE %x\n", Status));
        break;
    default:
        InVolDbgPrint(DL_VV_WARNING, DM_VV_DEVICE_CONTROL, 
                      ("VVHandleDeviceControlForVolumeImageDevice: Failing IOCTL %#x with STATUS_INVALID_DEVICE_REQUEST\n",
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
VVolumeFromVolumeImageCmdRoutine(PDEVICE_EXTENSION DevExtension, PCOMMAND_STRUCT pCommand)
{
    switch(pCommand->ulCommand) {
    case VVOLUME_CMD_READ:
    case VVOLUME_CMD_WRITE:
        DriverContext.VVolumeImageCmdRoutines[pCommand->ulCommand](DevExtension, pCommand);
        break;
    default:
        ASSERT(0);
        break;
    }

    return;
}

VOID
VVolumeImageProcessReadRequest(PDEVICE_EXTENSION DevExtension, PCOMMAND_STRUCT pCommand)
{
    PVOID           pReadBuffer,pBuffer = NULL;
    PIRP            Irp = pCommand->Cmd.Read.Irp;
    IO_STATUS_BLOCK IoStatus;
    NTSTATUS        Status;

    IoStatus.Status = STATUS_SUCCESS;
    IoStatus.Information = 0;

    pReadBuffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
    if (NULL == pReadBuffer) {
        Irp->IoStatus.Status = pCommand->CmdStatus = STATUS_INSUFFICIENT_RESOURCES;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        IoReleaseRemoveLock(&DevExtension->IoRemoveLock, Irp);
        return;
    }

    pBuffer = (PUCHAR) ALLOC_MEMORY(pCommand->Cmd.Read.ulLength, PagedPool);

    if (pBuffer == NULL)
    {
    Status = ZwReadFile(DevExtension->hFile, NULL, NULL, NULL, &IoStatus, pReadBuffer, 
               pCommand->Cmd.Read.ulLength, &pCommand->Cmd.Read.ByteOffset, NULL);
    } else {
        Status = ZwReadFile(DevExtension->hFile, NULL, NULL, NULL, &IoStatus, pBuffer, 
              pCommand->Cmd.Read.ulLength, &pCommand->Cmd.Read.ByteOffset, NULL);
        RtlCopyMemory(pReadBuffer, pBuffer,  pCommand->Cmd.Read.ulLength);
        FREE_MEMORY(pBuffer);
    }

    Irp->IoStatus.Status = pCommand->CmdStatus = IoStatus.Status;
    Irp->IoStatus.Information = IoStatus.Information;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    IoReleaseRemoveLock(&DevExtension->IoRemoveLock, Irp);

    return;
}

VOID
VVolumeImageProcessWriteRequest(PDEVICE_EXTENSION DevExtension, PCOMMAND_STRUCT pCommand)
{
    NTSTATUS        Status = STATUS_SUCCESS;
    PVOID           pWriteBuffer;
    IO_STATUS_BLOCK IoStatus;
    PIRP            Irp = pCommand->Cmd.Write.Irp;

    IoStatus.Status = STATUS_SUCCESS;
    IoStatus.Information = 0;

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = Status;


    if(pCommand->Cmd.Write.ulLength != 0)
    {
        pWriteBuffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
        if (NULL == pWriteBuffer) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup_and_exit;
            return;
        }

        BOOLEAN ZeroBlock = TRUE;
        ULONG WriteLength = pCommand->Cmd.Write.ulLength;
        UCHAR AllOnes = 0xFF;
        while(WriteLength)
        {
            if(((PUCHAR)pWriteBuffer)[--WriteLength] & AllOnes)
            {
                ZeroBlock = FALSE;
                break;
            }
        }

        if(pCommand->Cmd.Write.ByteOffset.QuadPart < 512)   //Boot Sector
        {
            //Replicated volume length could be different than the target volume Legnth.
            //Calculate new volume Length.
            DevExtension->uliVolumeSize.QuadPart = *(PLONGLONG ((PCHAR)pWriteBuffer + 40)) * 512;
        }

        if(ZeroBlock)
        {
            PIO_STACK_LOCATION IoStackLocation;
            KEVENT Event;
            PDEVICE_OBJECT FileSystemDeviceObject = NULL;
            PFILE_OBJECT FileSystemFileObject = NULL;
            HANDLE hFile = NULL;
            OBJECT_ATTRIBUTES ObjectAttributes;
            PIRP FileSystemIrp = NULL;
            
            InitializeObjectAttributes(&ObjectAttributes, &DevExtension->usFileName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE , NULL, NULL);

            Status = ZwCreateFile(&hFile, GENERIC_READ | GENERIC_WRITE, &ObjectAttributes, &IoStatus, NULL, 
                            FILE_ATTRIBUTE_NORMAL | FILE_WRITE_ATTRIBUTES,
                            FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_OPEN,
                            FILE_NON_DIRECTORY_FILE | FILE_NO_INTERMEDIATE_BUFFERING | FILE_SYNCHRONOUS_IO_NONALERT,
                            NULL, 0);

            if (!NT_SUCCESS(Status)) {
                InVolDbgPrint(DL_VV_WARNING, DM_VV_DEVICE_CONTROL, ("ZwCreateFile Failed: Status:%x\n", Status));
                goto cleanup_and_exit;
            }

            KeInitializeEvent(&Event, NotificationEvent, FALSE);

            FILE_ZERO_DATA_INFORMATION FileZeroDataInformation;
            FileZeroDataInformation.FileOffset.QuadPart = pCommand->Cmd.Write.ByteOffset.QuadPart;
            FileZeroDataInformation.BeyondFinalZero.QuadPart = pCommand->Cmd.Write.ByteOffset.QuadPart + pCommand->Cmd.Write.ulLength;

            Status = ObReferenceObjectByHandle(hFile, GENERIC_READ | GENERIC_WRITE, *IoFileObjectType ,
                        KernelMode,
                        (PVOID *)&FileSystemFileObject,
                        NULL
                    );

			if (!NT_SUCCESS(Status)) {
                InVolDbgPrint(DL_VV_WARNING, DM_VV_DEVICE_CONTROL, ("ObReferenceObjectByHandle Failed: Status:%x\n", Status));
				ZwClose(hFile);
                goto cleanup_and_exit;
            }

            FileSystemDeviceObject = IoGetRelatedDeviceObject(FileSystemFileObject);

            FileSystemIrp = IoBuildDeviceIoControlRequest(FSCTL_SET_ZERO_DATA, 
                            FileSystemDeviceObject,
                            &FileZeroDataInformation,
                            sizeof(FILE_ZERO_DATA_INFORMATION),
                            NULL,
                            0,
                            FALSE,
                            &Event,
                            &IoStatus);

            if(FileSystemIrp)
            {
                IoStackLocation                                 = IoGetNextIrpStackLocation(FileSystemIrp);
                IoStackLocation->MajorFunction                  = (UCHAR)IRP_MJ_FILE_SYSTEM_CONTROL;
                IoStackLocation->MinorFunction                  = IRP_MN_KERNEL_CALL;
                IoStackLocation->FileObject                     = FileSystemFileObject;
                
                Status = IoCallDriver(FileSystemDeviceObject, FileSystemIrp);

                if(STATUS_PENDING == Status)
                {
                    KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
                }

                if(!NT_SUCCESS(Status))
                {
                    InVolDbgPrint(DL_VV_WARNING, DM_VV_DEVICE_CONTROL, ("FSCTL_SET_ZERO_DATA Failed: Status:%x\n", Status));
                }
                else
                {
                    InVolDbgPrint(DL_VV_WARNING, DM_VV_DEVICE_CONTROL, 
                        ("FSCTL_SET_ZERO_DATA Status Success %x BytesReturned:%x\n", Status, IoStatus.Information));
                    IoStatus.Information = pCommand->Cmd.Write.ulLength;
                }
            }
			else
			{
				 Status = STATUS_INSUFFICIENT_RESOURCES; 

			}

            ObDereferenceObject(FileSystemFileObject);
            ZwClose(hFile);
        }
        else
        {
            Status = ZwWriteFile(DevExtension->hFile, NULL, NULL, NULL, &IoStatus, pWriteBuffer, 
                pCommand->Cmd.Write.ulLength, &pCommand->Cmd.Write.ByteOffset, NULL);
        }

        Irp->IoStatus.Status = pCommand->CmdStatus = Status;
        Irp->IoStatus.Information = IoStatus.Information;
    }
cleanup_and_exit:
    Irp->IoStatus.Status = pCommand->CmdStatus = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    IoReleaseRemoveLock(&DevExtension->IoRemoveLock, Irp);
    return;
}
