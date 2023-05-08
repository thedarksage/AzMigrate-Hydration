#include "VVCommon.h"
#include "VVDriverContext.h"
#include "VVIDevControl.h"
#include "VVolumeExport.h"
#include "VVolumeImageTest.h"
#include "mountmgr.h"
#include "VsnapKernel.h"
#include "DrvDefines.h"
#include "RegistryHelper.h"

extern LIST_ENTRY	*ParentVolumeListHead;
extern VsnapRWLock *ParentVolumeListLock;
#if (NTDDI_VERSION >= NTDDI_WS03)
NTSTATUS
VVImpersonateCIFS(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp     
    )
{
    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    PETHREAD pThread = PsGetCurrentThread();
    Status = VVCreateClientSecurity(&DriverContext.ImpersonatioContext, pThread);
    if (!NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_VV_ERROR, DM_VV_DEVICE_CONTROL, 
                  ("Couldn't build impersonation context!\n"));
    } else {
        InVolDbgPrint(DL_VV_ERROR, DM_VV_DEVICE_CONTROL,
                  ("InMageScurityContext %p\n", DriverContext.ImpersonatioContext.pSecurityContext));
    }
    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}
#endif
NTSTATUS
VVHandleDeviceControlForControlDevice(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    )
{
    NTSTATUS            Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION   DevExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulIoControlCode;

    ASSERT(DEVICE_TYPE_CONTROL == DevExtension->ulDeviceType);

    ulIoControlCode = IoStackLocation->Parameters.DeviceIoControl.IoControlCode;
    InVolDbgPrint(DL_VV_INFO, DM_VV_DEVICE_CONTROL, 
                  ("VVHandleDeviceControlForControlDevice: ulIoControlCode = %#x\n", ulIoControlCode));
    switch(ulIoControlCode) {
        case IOCTL_INMAGE_ADD_VIRTUAL_VOLUME_FROM_FILE:
            Status = VVHandleAddVolumeFromVolumeImageIOCTL(DeviceObject, Irp);
            break;
        case IOCTL_INMAGE_ADD_VIRTUAL_VOLUME_FROM_RETENTION_LOG:
            Status = VVHandleAddVolumeFromRetentionLogIOCTL(DeviceObject, Irp);
            break;
        case IOCTL_INMAGE_ADD_VIRTUAL_VOLUME_FROM_MULTIPART_SPARSE_FILE:
            Status = VVHandleAddVolumeFromMultiPartSparseFile(DeviceObject, Irp);
            break;  
        case IOCTL_INMAGE_REMOVE_VIRTUAL_VOLUME_FROM_RETENTION_LOG:
            Status = VVHandleRemoveVolumeFromRetentionLogIOCTL(DeviceObject, Irp);
            break;
        case IOCTL_INMAGE_GET_VOLUME_SYMLINK_FOR_RETENTION_POINT:
            Status = VVGetVolumeNameLinkForRetentionPoint(DeviceObject, Irp);
            break;
        case IOCTL_INMAGE_SET_CONTROL_FLAGS:
            Status = VVHandleSetControlFlagsIOCTL(DeviceObject, Irp);
            break;
        case IOCTL_INMAGE_CAN_UNLOAD_DRIVER:
            Status = VVCanUnloadDriver(DeviceObject, Irp);
            break;
        case IOCTL_INMAGE_GET_MOUNTED_VOLUME_LIST:
            Status = VVGetMountedVolumeList(DeviceObject, Irp);
            break;
        case IOCTL_INMAGE_UPDATE_VSNAP_VOLUME:
            Status = VVUpdateVsnapVolumes(DeviceObject, Irp);
            break;
        case IOCTL_INMAGE_GET_VOLUME_INFORMATION:
            Status = VVGetVolumeInformation(DeviceObject, Irp);
            break;
        case IOCTL_INMAGE_ENABLE_TRACKING_FOR_VSNAP:
        case IOCTL_INMAGE_DISABLE_TRACKING_FOR_VSNAP:
            Status = VVSetOrResetTrackingForVsnap(DeviceObject, Irp);
            break;
        case IOCTL_INMAGE_SET_VVVOLUME_FLAG:
            Status = VVSetVolumeFlags(DeviceObject, Irp);
            break;
        case IOCTL_INMAGE_VVOLUME_CONFIGURE_DRIVER:
            Status = VVConfigureDriver(DeviceObject, Irp);
            break;
#if (NTDDI_VERSION >= NTDDI_WS03)
        case IOCTL_IMPERSONATE_CIFS:
            Status = VVImpersonateCIFS(DeviceObject, Irp);
            break;
#endif
        default: 
            Status = STATUS_INVALID_DEVICE_REQUEST;
            Irp->IoStatus.Status = Status;
            Irp->IoStatus.Information = 0;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            IoReleaseRemoveLock(&DevExtension->IoRemoveLock, Irp);
            break;
    }
    return Status;
}

NTSTATUS
VVSetOrResetTrackingForVsnap(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    NTSTATUS            Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    PDEVICE_EXTENSION   DeviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    ULONG               InputBufferLength = IoStackLocation->Parameters.DeviceIoControl.InputBufferLength;
    PDEVICE_EXTENSION   VsnapDeviceExtension = NULL;
    PVOID               InputBuffer = NULL;
    int                 TrackingId = 0;
    bool                Enable = true;
    
    Irp->IoStatus.Information = 0;

    if(IoStackLocation->Parameters.DeviceIoControl.InputBufferLength == 0)
    {
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }
    
    InputBuffer = Irp->AssociatedIrp.SystemBuffer;
    
    KeWaitForSingleObject(&DriverContext.hMutex, Executive, KernelMode, FALSE, NULL);
    
    VsnapDeviceExtension = GetVolumeFromVolumeID(InputBuffer, InputBufferLength);

    if(NULL == VsnapDeviceExtension)
    {
        Status = STATUS_NO_SUCH_DEVICE;
        goto Exit;
    }

    if( IoStackLocation->Parameters.DeviceIoControl.IoControlCode == IOCTL_INMAGE_ENABLE_TRACKING_FOR_VSNAP)
        Enable = true;
    else
        Enable = false;

    bool rv = VsnapSetOrResetTracking(VsnapDeviceExtension->VolumeContext, Enable, &TrackingId);
    
    if(Enable && rv && IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength >= sizeof(int))
    {
        *(int *)Irp->AssociatedIrp.SystemBuffer = TrackingId;
        Irp->IoStatus.Information = sizeof(int);
    }
    
    if(!rv)
        Status = STATUS_UNSUCCESSFUL;
    
Exit:
    KeReleaseMutex(&DriverContext.hMutex, FALSE);

    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    IoReleaseRemoveLock(&DeviceExtension->IoRemoveLock, Irp);
    
    return Status;
}

NTSTATUS
VVUpdateVsnapVolumes(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    NTSTATUS Status = STATUS_PENDING;
    bool             bReturnValue = true, IsIOComplete = false;
    PIO_STACK_LOCATION IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    PDEVICE_EXTENSION DeviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    PVOID InputBuffer = NULL;
    
    if(IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < sizeof(UPDATE_VSNAP_VOLUME_INPUT))
    {
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    InputBuffer = Irp->AssociatedIrp.SystemBuffer;
    
    bReturnValue = VsnapVolumesUpdateMaps(InputBuffer, Irp, &IsIOComplete);
    if((IsIOComplete == true) ) { 
        if(bReturnValue ==false)
            Status = STATUS_UNSUCCESSFUL;
    } else {
        //ASSERT(bReturnValue == false);
        if(bReturnValue == true)
            Irp->IoStatus.Status = Status = STATUS_SUCCESS;
        else
            Irp->IoStatus.Status = Status = STATUS_INVALID_PARAMETER;

            Irp->IoStatus.Information = 0;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            IoReleaseRemoveLock(&DeviceExtension->IoRemoveLock, Irp);

    }
    
    return Status;
}

NTSTATUS
VVGetMountedVolumeList(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    PDEVICE_EXTENSION ControlDeviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    ULONG OutputBufferLength = IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength;
    
    KIRQL OldIrql;
    PLIST_ENTRY ListEntry = NULL;
    ULONG RequiredSize = 0;
    ULONG NumberOfMountedVolumes = 0;
    ULONG ByteOffset = 0;

    KeWaitForSingleObject(&DriverContext.hMutex, Executive, KernelMode, FALSE, NULL);

    if(OutputBufferLength < sizeof(ULONG))
    {
        Status = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    ListEntry = DriverContext.DeviceContextHead.Flink;
    while(ListEntry != &DriverContext.DeviceContextHead)
    {
        PDEVICE_EXTENSION DeviceExtension = (PDEVICE_EXTENSION) ListEntry;
        if(DEVICE_TYPE_VVOLUME_FROM_RETENTION_LOG == DeviceExtension->ulDeviceType)
        {
            RequiredSize += DeviceExtension->usMountDevLink.Length + sizeof(WCHAR); //Terminating NULL character
            NumberOfMountedVolumes++;
        }
        ListEntry = ListEntry->Flink;
    }

    if(NumberOfMountedVolumes)
        RequiredSize += sizeof(ULONG);      //OutputBuffer = NumberOfVolumes + VolumeString1 + VolumeString2 + ...

    if(OutputBufferLength < RequiredSize)
    {
        Status = STATUS_BUFFER_OVERFLOW;
        *(PULONG) Irp->AssociatedIrp.SystemBuffer = RequiredSize;
        Irp->IoStatus.Information = sizeof(ULONG);
        goto Exit;
    }

    RtlZeroMemory(Irp->AssociatedIrp.SystemBuffer, OutputBufferLength);

    *(PULONG) Irp->AssociatedIrp.SystemBuffer = NumberOfMountedVolumes;
    ByteOffset += sizeof(ULONG);
    ListEntry = DriverContext.DeviceContextHead.Flink;
    while(ListEntry != &DriverContext.DeviceContextHead)
    {
        PDEVICE_EXTENSION DeviceExtension = (PDEVICE_EXTENSION) ListEntry;
        if(DEVICE_TYPE_VVOLUME_FROM_RETENTION_LOG == DeviceExtension->ulDeviceType)
        {
            RtlCopyMemory((PCHAR)Irp->AssociatedIrp.SystemBuffer + ByteOffset,
                                DeviceExtension->usMountDevLink.Buffer, DeviceExtension->usMountDevLink.Length);
            ByteOffset += DeviceExtension->usMountDevLink.Length + sizeof(WCHAR);
        }
        ListEntry = ListEntry->Flink;
    }

    Irp->IoStatus.Information = RequiredSize;

Exit:
    KeReleaseMutex(&DriverContext.hMutex, FALSE);
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    IoReleaseRemoveLock(&ControlDeviceExtension->IoRemoveLock, Irp);
    return Status;
}

NTSTATUS
VVCanUnloadDriver(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    NTSTATUS Status = STATUS_SUCCESS;
    BOOLEAN CanUnloadDriver = FALSE;
    PIO_STACK_LOCATION IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    PDEVICE_EXTENSION DeviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    KIRQL OldIrql;

    Irp->IoStatus.Information = 0;

    if(IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(ULONG))
    {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto Exit;
    }

    KeWaitForSingleObject(&DriverContext.hMutex, Executive, KernelMode, FALSE, NULL);

    if(DriverContext.DeviceContextHead.Flink->Flink == &DriverContext.DeviceContextHead)
        CanUnloadDriver = TRUE;

    KeReleaseMutex(&DriverContext.hMutex, FALSE);

    *(PULONG) Irp->AssociatedIrp.SystemBuffer = CanUnloadDriver;

    Irp->IoStatus.Information = sizeof(ULONG);

Exit:
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    IoReleaseRemoveLock(&DeviceExtension->IoRemoveLock, Irp);
    return Status;
}

NTSTATUS
VVGetVolumeNameLinkForRetentionPoint(PDEVICE_OBJECT ControlDeviceObject, PIRP Irp)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION IoStackLocation = (PIO_STACK_LOCATION) IoGetCurrentIrpStackLocation(Irp);
    ULONG InputBufferLength = IoStackLocation->Parameters.DeviceIoControl.InputBufferLength;
    PVOID InputBuffer = NULL;
    ULONG OutputBufferLength = 0;
    PVOID OutputBuffer = NULL;
    PDEVICE_EXTENSION DeviceExtension = NULL;
    PDEVICE_EXTENSION ControlDeviceExtension = (PDEVICE_EXTENSION) ControlDeviceObject->DeviceExtension;
    BOOLEAN bFound = FALSE;
    PLIST_ENTRY CurEntry = NULL;

    Irp->IoStatus.Information = 0;

    if(InputBufferLength <= 0)
    {
        Status = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    InputBuffer = Irp->AssociatedIrp.SystemBuffer;

    KeWaitForSingleObject(&DriverContext.hMutex, Executive, KernelMode, FALSE, NULL);
    
    DeviceExtension = GetVolumeFromVolumeID(InputBuffer, InputBufferLength);

    if(NULL == DeviceExtension)
    {
        Status = STATUS_NO_SUCH_DEVICE;
        goto Exit;
    }

    OutputBufferLength = IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength;

    if(OutputBufferLength < sizeof(ULONG))
    {
        Status = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    OutputBuffer = Irp->AssociatedIrp.SystemBuffer;

    ULONG RequiredOutputBufferSize = DeviceExtension->usMountDevLink.Length + sizeof(WCHAR) 
                                            + DeviceExtension->usMountDevDeviceName.Length + sizeof(WCHAR);

    if(OutputBufferLength < RequiredOutputBufferSize)
    {
        Status = STATUS_BUFFER_OVERFLOW;
        *(PULONG)OutputBuffer = RequiredOutputBufferSize;
        Irp->IoStatus.Information = sizeof(ULONG);
        goto Exit;
    }

    RtlZeroMemory(OutputBuffer, OutputBufferLength);
    RtlCopyMemory(OutputBuffer, DeviceExtension->usMountDevLink.Buffer, DeviceExtension->usMountDevLink.Length);
    RtlCopyMemory((PCHAR)OutputBuffer + DeviceExtension->usMountDevLink.Length + sizeof(WCHAR), 
                        DeviceExtension->usMountDevDeviceName.Buffer, DeviceExtension->usMountDevDeviceName.Length);
    
    Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = RequiredOutputBufferSize;

Exit:
    KeReleaseMutex(&DriverContext.hMutex, FALSE);
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    IoReleaseRemoveLock(&ControlDeviceExtension->IoRemoveLock, Irp);
    return Status;
}

NTSTATUS
VVGetVolumeInformation(PDEVICE_OBJECT ControlDeviceObject, PIRP Irp)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION IoStackLocation = (PIO_STACK_LOCATION) IoGetCurrentIrpStackLocation(Irp);
    ULONG InputBufferLength = IoStackLocation->Parameters.DeviceIoControl.InputBufferLength;
    PVOID InputBuffer = NULL;
    ULONG OutputBufferLength = 0;
    PVOID OutputBuffer = NULL;
    PDEVICE_EXTENSION DeviceExtension = NULL;
    PDEVICE_EXTENSION ControlDeviceExtension = (PDEVICE_EXTENSION) ControlDeviceObject->DeviceExtension;
    BOOLEAN bFound = FALSE;
    PLIST_ENTRY CurEntry = NULL;
    ULONG VolumeInformationSize = 0;
    ULONG RequiredOutputBufferSize = 0;
    ULONG ByteOffset = 0;

    Irp->IoStatus.Information = 0;

    if(InputBufferLength < ((GUID_OFFSET + GUID_SIZE_IN_CHARS)*sizeof(WCHAR)))
    {
        Status = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    InputBuffer = Irp->AssociatedIrp.SystemBuffer;
    
    DeviceExtension = GetVolumeFromMountDevLink((PWCHAR)InputBuffer + GUID_OFFSET, GUID_SIZE_IN_CHARS * sizeof(WCHAR));

    if(NULL == DeviceExtension)
    {
        Status = STATUS_NO_SUCH_DEVICE;
        goto Exit;
    }

    OutputBufferLength = IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength;

    if(OutputBufferLength < sizeof(ULONG))
    {
        Status = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    OutputBuffer = Irp->AssociatedIrp.SystemBuffer;

    if(DeviceExtension->ulDeviceType == DEVICE_TYPE_VVOLUME_FROM_RETENTION_LOG)
        VolumeInformationSize = VsnapGetVolumeInformationLength(DeviceExtension->VolumeContext);

    RequiredOutputBufferSize = sizeof(ULONG) + DeviceExtension->UniqueVolumeIDLength + VolumeInformationSize;

    if(OutputBufferLength < RequiredOutputBufferSize)
    {
        Status = STATUS_BUFFER_OVERFLOW;
        *(PULONG)OutputBuffer = RequiredOutputBufferSize;
        Irp->IoStatus.Information = sizeof(ULONG);
        goto Exit;
    }

    RtlZeroMemory(OutputBuffer, OutputBufferLength);
    *(PULONG)OutputBuffer = DeviceExtension->UniqueVolumeIDLength;
    ByteOffset += sizeof(ULONG);

    RtlCopyMemory((PCHAR)OutputBuffer + ByteOffset, DeviceExtension->UniqueVolumeID, DeviceExtension->UniqueVolumeIDLength);
    ByteOffset += DeviceExtension->UniqueVolumeIDLength;

    if(DeviceExtension->ulDeviceType == DEVICE_TYPE_VVOLUME_FROM_RETENTION_LOG)
        VsnapGetVolumeInformation(DeviceExtension->VolumeContext, (PCHAR)OutputBuffer + ByteOffset, VolumeInformationSize);

    Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = RequiredOutputBufferSize;

Exit:
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    IoReleaseRemoveLock(&ControlDeviceExtension->IoRemoveLock, Irp);
    return Status;
}

NTSTATUS
VVHandleRemoveVolumeFromRetentionLogIOCTL(
    IN PDEVICE_OBJECT   ControlDeviceObject,
    IN PIRP             Irp
)
{
    NTSTATUS            Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION  IoStackLocation = (PIO_STACK_LOCATION) IoGetCurrentIrpStackLocation(Irp);
    ULONG               InputBufferLength = IoStackLocation->Parameters.DeviceIoControl.InputBufferLength;
    PDEVICE_EXTENSION   ControlDeviceExtension = (PDEVICE_EXTENSION) ControlDeviceObject->DeviceExtension;
    PVOID               InputBuffer = NULL;
    PLIST_ENTRY         CurEntry = NULL;
    PDEVICE_EXTENSION   VolumeDeviceExtension = NULL;
    KIRQL               OldIrql;
    PCOMMAND_STRUCT     pCommand                = NULL;


    InVolDbgPrint(DL_VV_INFO, DM_VV_DEVICE_CONTROL, 
        ("VVHandleRemoveVolumeFromRetentionLogIOCTL Called: DeviceObject%#p Irp:%#p\n", ControlDeviceObject, Irp));

    Irp->IoStatus.Information = 0;

    if(0 == InputBufferLength)
    {
        Status = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    
    pCommand = AllocateCommand();
    if (NULL == pCommand) {
        Status = STATUS_NO_MEMORY;
        goto Exit;
    }

    pCommand->Cmd.RemoveVolumeFromRetentionOrImage.Irp = Irp;
    pCommand->Cmd.RemoveVolumeFromRetentionOrImage.VolumeInfo = Irp->AssociatedIrp.SystemBuffer;
    pCommand->Cmd.RemoveVolumeFromRetentionOrImage.VolumeInfoLength = InputBufferLength;

    pCommand->ulFlags |= COMMAND_FLAGS_WAITING_FOR_COMPLETION;
    pCommand->ulCommand = VE_CMD_REMOVE_VOLUME;

    
    Status = QueueCmdToCmdQueue(ControlDeviceExtension->CmdQueue, pCommand);
    if(!NT_SUCCESS(Status)) {
        DereferenceCommand(pCommand);
        goto Exit;

    }

    KeWaitForSingleObject(&pCommand->WaitEvent, Executive, KernelMode, FALSE, NULL);

    Status = pCommand->CmdStatus;

    DereferenceCommand(pCommand);
Exit:
    
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    IoReleaseRemoveLock(&ControlDeviceExtension->IoRemoveLock, Irp);
    return Status;
}

PDEVICE_EXTENSION
GetVolumeFromVolumeID(PVOID VolumeID, ULONG VolumeIDLength)
{
    PLIST_ENTRY CurEntry = NULL;
    PDEVICE_EXTENSION DeviceExtension = NULL;

    if(VolumeID == NULL || 0 == VolumeIDLength)
        return DeviceExtension;

    CurEntry = DriverContext.DeviceContextHead.Flink;
    while(CurEntry != &DriverContext.DeviceContextHead)
    {
        DeviceExtension = (PDEVICE_EXTENSION) CurEntry;
        if(DeviceExtension->UniqueVolumeID != NULL 
            && DeviceExtension->UniqueVolumeIDLength == VolumeIDLength
            && DeviceExtension->UniqueVolumeIDLength == RtlCompareMemory(DeviceExtension->UniqueVolumeID , VolumeID, VolumeIDLength))
            break;
        CurEntry = CurEntry->Flink;
    }

    if(CurEntry == &DriverContext.DeviceContextHead)
        DeviceExtension = NULL;

    return DeviceExtension;
}

PDEVICE_EXTENSION
GetVolumeFromMountDevLink(PVOID MountDevLink, ULONG MountDevLinkLength)
{
    PLIST_ENTRY CurEntry = NULL;
    PDEVICE_EXTENSION DeviceExtension = NULL;

    if(MountDevLink == NULL || 0 >= MountDevLinkLength)
        return DeviceExtension;

    CurEntry = DriverContext.DeviceContextHead.Flink;
    while(CurEntry != &DriverContext.DeviceContextHead)
    {
        DeviceExtension = (PDEVICE_EXTENSION) CurEntry;
        if(DeviceExtension->usMountDevLink.Buffer != NULL 
            && DeviceExtension->usMountDevLink.Length != 0
            && MountDevLinkLength == RtlCompareMemory(
            DeviceExtension->usMountDevLink.Buffer + GUID_OFFSET ,
            MountDevLink, MountDevLinkLength))
            break;
        CurEntry = CurEntry->Flink;
    }

    if(CurEntry == &DriverContext.DeviceContextHead)
        DeviceExtension = NULL;

    return DeviceExtension;
}

VOID
VVCleanupVolume(PDEVICE_EXTENSION DeviceExtension)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PCOMMAND_STRUCT pCommand = NULL;
    KIRQL OldIrql;
    PDEVICE_EXTENSION  ControlDevExtension = (PDEVICE_EXTENSION)DriverContext.ControlDevice->DeviceExtension;

    if(NULL == DeviceExtension)
        return;

    if(DeviceExtension->ulFlags & VV_DE_MOUNT_SUCCESS)
        Status = VVDeleteVolumeMountPoint(DeviceExtension);

    if(!NT_SUCCESS(Status))
        return;
    
    CleanupVolumeContext(DeviceExtension);

    if(DeviceExtension->IOCmdQueue)
    {
        // Removing the IoCmdQueue from DriverContext cmdqueue list that is added as part of initialization
        KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
        RemoveEntryList(&DeviceExtension->IOCmdQueue->ListEntry);
        KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

        // Decrease the ref. count to reflect the removal of entry list from cmdqueuelist of DriverContext
        DereferenceIOCmdQueue(DeviceExtension->IOCmdQueue);
		// Decrease the ref. count that is hold as part of creating this queue
        DereferenceIOCmdQueue(DeviceExtension->IOCmdQueue);
        DeviceExtension->IOCmdQueue = NULL;
    }



    if(DeviceExtension->MountDevUniqueId.Buffer)
    {
        FREE_MEMORY(DeviceExtension->MountDevUniqueId.Buffer);
        RtlInitUnicodeString(&DeviceExtension->MountDevUniqueId, NULL);
    } 

    if(DeviceExtension->usDeviceName.Buffer)
    {
        FREE_MEMORY(DeviceExtension->usDeviceName.Buffer);
        RtlInitUnicodeString(&DeviceExtension->usDeviceName, NULL);
    }

    if(DeviceExtension->usMountDevLink.Buffer)
    {
        FREE_MEMORY(DeviceExtension->usMountDevLink.Buffer);
        RtlInitUnicodeString(&DeviceExtension->usMountDevLink, NULL);
    }

    if(DeviceExtension->UniqueVolumeID)
    {
        FREE_MEMORY(DeviceExtension->UniqueVolumeID);
        DeviceExtension->UniqueVolumeID = NULL;
    }

    if(DeviceExtension->usFileName.Buffer)
    {
        FREE_MEMORY(DeviceExtension->usFileName.Buffer);
        RtlInitUnicodeString(&DeviceExtension->usFileName, NULL);
    }

    IoDeleteDevice(DeviceExtension->DeviceObject);
}

NTSTATUS
VVDeleteVolumeMountPoint(PDEVICE_EXTENSION DeviceExtension)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PMOUNTMGR_MOUNT_POINT pMountMgrMountPoint = NULL;
    PMOUNTMGR_MOUNT_POINTS pMountMgrMountPoints = NULL;
    ULONG FieldOffset = 0;
    ULONG MountMgrMountPointLength = 0;
    ULONG MountMgrMountPointsLength = sizeof(MOUNTMGR_MOUNT_POINTS);
    UNICODE_STRING MountManagerName;
    PFILE_OBJECT MountMgrFileObject = NULL;
    PDEVICE_OBJECT MountMgrDeviceObject = NULL;
    IO_STATUS_BLOCK  IoStatusBlock;
    KEVENT  Event;
    PIRP MntMgrIrp = NULL;
    
    MountMgrMountPointLength =  sizeof(MOUNTMGR_MOUNT_POINT) +
                            DeviceExtension->usMountDevLink.Length +
                            DeviceExtension->MountDevUniqueId.Length +
                            DeviceExtension->usDeviceName.Length;

    pMountMgrMountPoint = (PMOUNTMGR_MOUNT_POINT) ALLOC_MEMORY(MountMgrMountPointLength, PagedPool);
    if(NULL == pMountMgrMountPoint)
    {
        Status = STATUS_NO_MEMORY;
        goto cleanup_and_exit;
    }
    RtlZeroMemory(pMountMgrMountPoint, MountMgrMountPointLength);

    FieldOffset = sizeof(MOUNTMGR_MOUNT_POINT);

    pMountMgrMountPoint->UniqueIdOffset = FieldOffset;
    pMountMgrMountPoint->UniqueIdLength = DeviceExtension->MountDevUniqueId.Length;
    FieldOffset += DeviceExtension->MountDevUniqueId.Length;

    pMountMgrMountPoint->DeviceNameOffset = FieldOffset;
    pMountMgrMountPoint->DeviceNameLength = DeviceExtension->usDeviceName.Length;
    FieldOffset += DeviceExtension->usDeviceName.Length;

    pMountMgrMountPoint->SymbolicLinkNameOffset = FieldOffset;
    pMountMgrMountPoint->SymbolicLinkNameLength = DeviceExtension->usMountDevLink.Length;


    RtlCopyMemory(((PCHAR)pMountMgrMountPoint) + pMountMgrMountPoint->SymbolicLinkNameOffset, 
            DeviceExtension->usMountDevLink.Buffer, 
            pMountMgrMountPoint->SymbolicLinkNameLength
            );

    RtlCopyMemory(((PCHAR)pMountMgrMountPoint) + pMountMgrMountPoint->UniqueIdOffset, 
            DeviceExtension->MountDevUniqueId.Buffer, 
            pMountMgrMountPoint->UniqueIdLength
            );

    RtlCopyMemory(((PCHAR)pMountMgrMountPoint) + pMountMgrMountPoint->DeviceNameOffset, 
            DeviceExtension->usDeviceName.Buffer, 
            pMountMgrMountPoint->DeviceNameLength
            );

    RtlInitUnicodeString(&MountManagerName, MOUNTMGR_DEVICE_NAME);
    Status = IoGetDeviceObjectPointer(&MountManagerName, FILE_READ_ATTRIBUTES, &MountMgrFileObject, &MountMgrDeviceObject);
    if(!NT_SUCCESS(Status))
    {
        InVolDbgPrint(DL_VV_WARNING, DM_VV_DRIVER_INIT, ("VVHandleRemoveVolumeFromRetentionLogIOCTL: Could not obtain mount manager reference\n"));
        goto cleanup_and_exit;
    }

    pMountMgrMountPoints = (PMOUNTMGR_MOUNT_POINTS)ALLOC_MEMORY(MountMgrMountPointsLength, PagedPool);
    if(NULL == pMountMgrMountPoints)
    {
        Status =  STATUS_NO_MEMORY;
        goto cleanup_and_exit;
    }
    RtlZeroMemory(pMountMgrMountPoints, MountMgrMountPointsLength);
    pMountMgrMountPoints->Size = MountMgrMountPointsLength;

    KeInitializeEvent(&Event, NotificationEvent, FALSE);

    MntMgrIrp = IoBuildDeviceIoControlRequest(
                    IOCTL_MOUNTMGR_QUERY_POINTS,
                    MountMgrDeviceObject,
                    pMountMgrMountPoint,
                    MountMgrMountPointLength,
                    pMountMgrMountPoints,
                    MountMgrMountPointsLength,
                    FALSE,
                    &Event,
                    &IoStatusBlock
                    );

    if(NULL == MntMgrIrp)
    {
        Status =  STATUS_NO_MEMORY;
        goto cleanup_and_exit;
    }
    
    Status = IoCallDriver(MountMgrDeviceObject, MntMgrIrp);

    if(STATUS_BUFFER_OVERFLOW != Status)
    {
        InVolDbgPrint(DL_VV_WARNING, DM_VV_DEVICE_CONTROL, ("IOCTL_MOUNTMGR_QUERY_POINTS Failed Status:%x\n", Status));
        goto cleanup_and_exit;
    }

    MountMgrMountPointsLength = pMountMgrMountPoints->Size;
    FREE_MEMORY(pMountMgrMountPoints);

    pMountMgrMountPoints = (PMOUNTMGR_MOUNT_POINTS) ALLOC_MEMORY(MountMgrMountPointsLength, PagedPool);
    if(NULL == pMountMgrMountPoints)
    {
        Status =  STATUS_NO_MEMORY;
        goto cleanup_and_exit;
    }
    RtlZeroMemory(pMountMgrMountPoints, MountMgrMountPointsLength);
    pMountMgrMountPoints->Size = MountMgrMountPointsLength;

    KeResetEvent(&Event);

    MntMgrIrp = IoBuildDeviceIoControlRequest(
                    IOCTL_MOUNTMGR_DELETE_POINTS,
                    MountMgrDeviceObject,
                    pMountMgrMountPoint,
                    MountMgrMountPointLength,
                    pMountMgrMountPoints,
                    MountMgrMountPointsLength,
                    FALSE,
                    &Event,
                    &IoStatusBlock
                    );

    ASSERT(KeGetCurrentIrql() < 2);
    Status = IoCallDriver(MountMgrDeviceObject, MntMgrIrp);
    if (STATUS_PENDING == Status) {
        KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
        Status = IoStatusBlock.Status;
    }

    if(!NT_SUCCESS(Status))
    {
        InVolDbgPrint(DL_VV_WARNING, DM_VV_DRIVER_INIT, 
                    ("VVHandleRemoveVolumeFromRetentionLogIOCTL: IOCTL_MOUNTMGR_DELETE_POINTS Failed Status:%x\n", Status));
    }

cleanup_and_exit:
    if(MountMgrFileObject)
        ObDereferenceObject(MountMgrFileObject);

    if(pMountMgrMountPoint)
        FREE_MEMORY(pMountMgrMountPoint);

    if(pMountMgrMountPoints)
        FREE_MEMORY(pMountMgrMountPoints);

    return Status;
}

NTSTATUS
VVHandleSetControlFlagsIOCTL(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    )
{
    NTSTATUS Status                     = STATUS_SUCCESS;
    PDEVICE_EXTENSION DeviceExtension   = NULL;
    PCONTROL_FLAGS_INPUT pFlagsInput    = NULL;
    PIO_STACK_LOCATION IoStackLocation  = IoGetCurrentIrpStackLocation(Irp);
    ULONG ulDeviceContextFlags          = 0;

    pFlagsInput = (PCONTROL_FLAGS_INPUT) Irp->AssociatedIrp.SystemBuffer;
    
   if(IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < sizeof(CONTROL_FLAGS_INPUT)) {
        InVolDbgPrint(DL_VV_ERROR, DM_VV_DEVICE_CONTROL, ("VVHandleSetControlFlagsIOCTL: Invalid parameters\n"));
        Status                      = STATUS_BUFFER_TOO_SMALL;
        Irp->IoStatus.Information   = 0;
        Irp->IoStatus.Status        = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    KeWaitForSingleObject(&DriverContext.hMutex, Executive, KernelMode, FALSE, NULL);

    switch(pFlagsInput->eOperation)
    {
    case ecBitOpSet:
        if(pFlagsInput->ulControlFlags & VV_CONTROL_FLAG_OUT_OF_ORDER_WRITE)
            ulDeviceContextFlags = VVSetOutOfOrderWrite();
        break;
    case ecBitOpReset:
        if(pFlagsInput->ulControlFlags & VV_CONTROL_FLAG_OUT_OF_ORDER_WRITE)
            ulDeviceContextFlags = VVResetOutOfOrderWrite();
        break;
    }

    KeReleaseMutex(&DriverContext.hMutex, FALSE);

    Irp->IoStatus.Information = 0;
    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength <= sizeof(CONTROL_FLAGS_INPUT)) {
        PCONTROL_FLAGS_OUTPUT pFlagsOutput = NULL;

        pFlagsOutput = (PCONTROL_FLAGS_OUTPUT) Irp->AssociatedIrp.SystemBuffer;
        pFlagsOutput->ulControlFlags = 0;

        if (ulDeviceContextFlags & VV_DC_FLAGS_SIMULATE_OUT_OF_ORDER_WRITE) {
            pFlagsOutput->ulControlFlags |= VV_CONTROL_FLAG_OUT_OF_ORDER_WRITE;
        }

        Irp->IoStatus.Information = sizeof(CONTROL_FLAGS_OUTPUT);
    }

    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return Status;
}

ULONG
VVSetOutOfOrderWrite()
{   
    NTSTATUS Status = STATUS_SUCCESS;
    ULONG ulFlags = 0;
    HANDLE hThread = NULL;
    
    ulFlags = DriverContext.ulControlFlags;

    if(ulFlags & VV_DC_FLAGS_SIMULATE_OUT_OF_ORDER_WRITE)
        return ulFlags;

    ASSERT(DriverContext.OutOfOrderTestThread == NULL);

    KeClearEvent(&DriverContext.hTestThreadShutdown);

    Status = PsCreateSystemThread(&hThread, THREAD_ALL_ACCESS, NULL, NULL, NULL, CommandExpiryMonitor, NULL);

    if (NT_SUCCESS(Status))
    {
        Status = ObReferenceObjectByHandle(hThread, SYNCHRONIZE, NULL, KernelMode, (PVOID *)&(DriverContext.OutOfOrderTestThread), NULL);
        if (NT_SUCCESS(Status)) {
            ZwClose(hThread);
        } else {
            InVolDbgPrint(DL_VV_ERROR, DM_VV_CMD_QUEUE, ("VVSetOutOfOrderWrite: ObReferenceObjectByHandle failed\n"));
            return ulFlags;
        }
    }
    else
    {
        InVolDbgPrint(DL_VV_WARNING, DM_VV_DEVICE_CONTROL, ("VVHandleSetControlFlagsIOCTL: PsCreateSystemThread failed\n"));
        return ulFlags;
    }

    InterlockedExchangePointer((PVOID *)&DriverContext.VVolumeImageCmdRoutines[VVOLUME_CMD_WRITE], VVolumeImageProcessWriteRequestTest);

    ulFlags |= VV_DC_FLAGS_SIMULATE_OUT_OF_ORDER_WRITE;
    DriverContext.ulControlFlags = ulFlags;

    return ulFlags;
}

ULONG
VVResetOutOfOrderWrite()
{
    ULONG ulFlags = 0;
    
    ulFlags = DriverContext.ulControlFlags;

    if(!(ulFlags & VV_DC_FLAGS_SIMULATE_OUT_OF_ORDER_WRITE))
        return ulFlags;

    InterlockedExchangePointer((PVOID *)&DriverContext.VVolumeImageCmdRoutines[VVOLUME_CMD_WRITE], VVolumeImageProcessWriteRequest);

    KeSetEvent(&DriverContext.hTestThreadShutdown, IO_NO_INCREMENT, FALSE);
    
    ulFlags &= ~VV_DC_FLAGS_SIMULATE_OUT_OF_ORDER_WRITE;
    DriverContext.ulControlFlags = ulFlags;

    KeWaitForSingleObject(DriverContext.OutOfOrderTestThread, Executive, KernelMode, FALSE, NULL);
    
    ObDereferenceObject(DriverContext.OutOfOrderTestThread);
    DriverContext.OutOfOrderTestThread = NULL;

    return ulFlags;
}

VOID CopyUnicodeString(PUNICODE_STRING Target, PUNICODE_STRING Source, POOL_TYPE PoolType)
{
    // TODO: Atleast have to do an ASSERT check for Target->Buffer == NULL
    ASSERT(Target != NULL && Source != NULL);
    ASSERT(Target->Buffer == NULL && Source->Buffer != NULL);
    
    if(Source->Length == 0)
        return; //Nothing to copy

    Target->Buffer = (PWCHAR) ALLOC_MEMORY(Source->Length, PoolType );
    if(NULL == Target->Buffer)
    {
        Target->Length = 0;
        Target->MaximumLength = 0;
    }
    else
    {
        Target->Length = Source->Length;
        Target->MaximumLength = Source->Length;
        RtlCopyMemory(Target->Buffer, Source->Buffer, Source->Length);
    }
}


NTSTATUS
VVSetVolumeFlags(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    NTSTATUS Status                            = STATUS_SUCCESS;
    PIO_STACK_LOCATION IoStackLocation         = IoGetCurrentIrpStackLocation(Irp);
    PDEVICE_EXTENSION ControlDeviceExtension   = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    ULONG OutputBufferLength                   = IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength;
    ULONG InputBufferLength                    = IoStackLocation->Parameters.DeviceIoControl.InputBufferLength;
    PVVVOLUME_FLAGS_INPUT pVVolumeFlagsInput   = NULL;
    PVVVOLUME_FLAGS_OUTPUT pVVolumeFlagsOutput = NULL;
    PLIST_ENTRY ListEntry                      = NULL;
    ULONG ByteOffset                           = 0;
    PDEVICE_EXTENSION DeviceExtension          = NULL;

    KeWaitForSingleObject(&DriverContext.hMutex, Executive, KernelMode, FALSE, NULL);

    if(InputBufferLength != sizeof(VVVOLUME_FLAGS_INPUT))
    {
        Status = STATUS_INVALID_PARAMETER;
        goto Exit;
    }
    pVVolumeFlagsInput = (PVVVOLUME_FLAGS_INPUT)Irp->AssociatedIrp.SystemBuffer;

    DeviceExtension = GetVolumeFromMountDevLink((PVOID)pVVolumeFlagsInput->VolumeGUID, GUID_SIZE_IN_CHARS * sizeof(WCHAR));

    if( DeviceExtension )
    {
        switch(pVVolumeFlagsInput->eOperation)
        {
        case ecBitOpSet:
            DeviceExtension->ulFlags  |= VVOLUME_FLAG_READ_ONLY;
            break;
        case ecBitOpReset:
            DeviceExtension->ulFlags  &= ~VVOLUME_FLAG_READ_ONLY;   
            break;                  
        }               

        if(OutputBufferLength >= sizeof(VVVOLUME_FLAGS_OUTPUT))
        {       
            pVVolumeFlagsOutput = (PVVVOLUME_FLAGS_OUTPUT)Irp->AssociatedIrp.SystemBuffer;
            pVVolumeFlagsOutput->ulControlFlags = DeviceExtension->ulFlags;
            Irp->IoStatus.Information = sizeof(VVVOLUME_FLAGS_OUTPUT);
            goto Exit;
        }
    }
    Irp->IoStatus.Information = 0;

Exit:
    KeReleaseMutex(&DriverContext.hMutex, FALSE);
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    IoReleaseRemoveLock(&ControlDeviceExtension->IoRemoveLock, Irp);
    return Status;
}

NTSTATUS
VVConfigureDriver(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    NTSTATUS Status                            = STATUS_SUCCESS;
    PIO_STACK_LOCATION IoStackLocation         = IoGetCurrentIrpStackLocation(Irp);
    PDEVICE_EXTENSION  ControlDeviceExtension  = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    PDEVICE_EXTENSION  DeviceExtension         = NULL;
    HANDLE             hRegistry               = NULL;
    ULONG OutputBufferLength                   = IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength;
    ULONG InputBufferLength                    = IoStackLocation->Parameters.DeviceIoControl.InputBufferLength;
    ULONG VsnapCount                           = 0;
    VsnapParentVolumeList    *Parent           = NULL;
    PVVOLUME_CD_INPUT  VVolumeCDInput          = (PVVOLUME_CD_INPUT)Irp->AssociatedIrp.SystemBuffer; 
    VsnapInfo          *Vsnap                  = NULL;

 
    KeWaitForSingleObject(&DriverContext.FileHandleCacheMutex, Executive, KernelMode, FALSE, NULL);

    if(InputBufferLength < sizeof(VVOLUME_CD_INPUT))
    {
        Status = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    
	
	if(NULL == ParentVolumeListHead)
		return NULL;

	Parent = (VsnapParentVolumeList *)ParentVolumeListHead->Flink;
	while((Parent != NULL) && (Parent != (VsnapParentVolumeList *)ParentVolumeListHead))
	{
        Vsnap = (VsnapInfo *)Parent->VsnapHead.Flink;
        while((Vsnap != NULL) && (Vsnap != (VsnapInfo *)&Parent->VsnapHead))
        {
            VsnapCount++;
            Vsnap = (VsnapInfo*)Vsnap->LinkNext.Flink;        //reference the buffer update task for every iteration of this loop
        }		

		Parent = (VsnapParentVolumeList *)(Parent->LinkNext.Flink);
	}

    if(VsnapCount)
    {
        Status = STATUS_INVALID_DEVICE_REQUEST ;
        goto Exit;
    }
    
    Status = OpenKeyReadWrite(&DriverContext.DriverParameters, &hRegistry, TRUE);

    if(NT_SUCCESS(Status)) {
        if(VVolumeCDInput->ulFlags & FLAG_MEM_FOR_FILE_HANDLE_CACHE) {
            if((VVolumeCDInput->ulMemForFileHandleCacheInKB < MIN_PAGED_MEM_FOR_FILE_HANDLE_CACHE_KB) || (VVolumeCDInput->ulMemForFileHandleCacheInKB > MAX_PAGED_MEM_FOR_FILE_HANDLE_CACHE_KB)) {
                WriteULONG(hRegistry, MEMORY_FOR_FILE_HANDLE_CACHE_KB, DEFAULT_PAGED_MEM_FILE_HANDLE_CACHE_KB);
                DriverContext.ulMemoryForFileHandleCache = DEFAULT_PAGED_MEM_FILE_HANDLE_CACHE_KB * KILO_BYTE - MEMORY_FOR_TAG;
            } else {
                WriteULONG(hRegistry, MEMORY_FOR_FILE_HANDLE_CACHE_KB, VVolumeCDInput->ulMemForFileHandleCacheInKB);
                DriverContext.ulMemoryForFileHandleCache = VVolumeCDInput->ulMemForFileHandleCacheInKB * KILO_BYTE - MEMORY_FOR_TAG;
            }
        }
        
        CloseKey(hRegistry);
    }


 
Exit:   
    KeReleaseMutex(&DriverContext.FileHandleCacheMutex, FALSE);
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    IoReleaseRemoveLock(&ControlDeviceExtension->IoRemoveLock, Irp);
    return Status;
}