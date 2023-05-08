#include <VVCommon.h>
#include <VVCmdQueue.h>
#include <VVolumeExport.h>
#include <VVDriverContext.h>
#include <VVIDevControl.h>
#include <VVolumeImage.h>
#include <VVolumeRetentionLog.h>
#include <VsnapKernel.h>
#include <VVolumeRetentionTest.h>
#include <mountmgr.h>

#define INMAGE_KERNEL_MODE
#include <FileIO.h>

#define MAX_RETRY_FOR_CREATE_GUID 3

#define FILE_DEVICE_FILE_SYSTEM         0x00000009

#define FSCTL_SET_ZERO_DATA             CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 50, METHOD_BUFFERED, FILE_WRITE_DATA) // FILE_ZERO_DATA_INFORMATION,

NTSTATUS
VVSendMountMgrVolumeArrivalNotification(PDEVICE_EXTENSION DeviceExtension);

/**
 * This IOCTL takes the file name in dos format string.
 * File name would be of format "X:\Images\VolumeImage1.img", and the file is
 * opened from system thread context. Due to this if drive letter is only
 * visible in user ms-dos name space \DosDevices but not in global ms-dos
 * name space \DosDevices\Global, file open from system thread context
 * will fail.
 * This can be solved by resolving the name into NT name space instead
 * of ms-dos name space before command is queued.
 * */
NTSTATUS
VVHandleAddVolumeFromVolumeImageIOCTL(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    )
{
    NTSTATUS                    Status                  = STATUS_SUCCESS;
    PDEVICE_EXTENSION           DevExtension            = NULL;
    PIO_STACK_LOCATION          IoStackLocation         = NULL;
    PWCHAR                      FileName                = NULL;
    PCOMMAND_STRUCT             pCommand                = NULL;
    PWCHAR                      UniqueVolumeID          = NULL;
    PADD_VOLUME_IMAGE_INPUT     pAddVolumeImageInput    = NULL;
    PCHAR                       pVolumeInfo             = NULL;
    ULONG                       UniqueVolumeIDLength    = 0;
    ULONG                       ExpectedInputSize       = 0;
    size_t                      cbInputSize             = 0;
    size_t                      cbFileNameSize          = 0;

    DevExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    IoStackLocation = IoGetCurrentIrpStackLocation(Irp);

    ASSERT(IOCTL_INMAGE_ADD_VIRTUAL_VOLUME_FROM_FILE == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

    // Validate input buffer.
    do
    {
        pAddVolumeImageInput = (PADD_VOLUME_IMAGE_INPUT)Irp->AssociatedIrp.SystemBuffer;

        ExpectedInputSize += FIELD_OFFSET(ADD_VOLUME_IMAGE_INPUT, VolumeInfo);
        if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < ExpectedInputSize) {
            Status = STATUS_INVALID_PARAMETER;
            break;
        }
        pVolumeInfo = pAddVolumeImageInput->VolumeInfo;
        
        ExpectedInputSize += sizeof(ULONG);
        if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < ExpectedInputSize) {
            Status = STATUS_INVALID_PARAMETER;
            break;
        }
        UniqueVolumeIDLength = *(PULONG)pVolumeInfo;
        pVolumeInfo += sizeof(ULONG);

        ExpectedInputSize += UniqueVolumeIDLength;
        if(IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < ExpectedInputSize) {
            Status = STATUS_INVALID_PARAMETER;
            break;
        }
        UniqueVolumeID = (PWCHAR) ALLOC_MEMORY(UniqueVolumeIDLength, PagedPool);
        if(NULL == UniqueVolumeID)
        {
            Status = STATUS_UNSUCCESSFUL;
            break;
        }
        RtlCopyMemory(UniqueVolumeID, pVolumeInfo, UniqueVolumeIDLength);
        pVolumeInfo += UniqueVolumeIDLength;
        
        ExpectedInputSize += sizeof(ULONG);
        if(IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < ExpectedInputSize) {
            Status = STATUS_INVALID_PARAMETER;
            break;
        }
        cbInputSize = *(PULONG) pVolumeInfo;
        pVolumeInfo += sizeof(ULONG);

        ExpectedInputSize += cbInputSize;
        if(IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < ExpectedInputSize) {
            Status = STATUS_INVALID_PARAMETER;
            break;
        }
        
        if ((STATUS_SUCCESS != Status) || (0 == cbInputSize)) {
            // 0 == cbInputSize takes care of a condition where file name of zero length is passed.
            Status = STATUS_INVALID_PARAMETER;
            break;
        }

        // Add null terminator to cbFileNameSize
        cbFileNameSize = cbInputSize + wcslen(DOS_DEVICES_PATH) * sizeof(WCHAR) + sizeof(WCHAR);

        pCommand = AllocateCommand();
        if (NULL == pCommand) {
            Status = STATUS_NO_MEMORY;
            break;
        }

        FileName = (PWCHAR)ALLOC_MEMORY(cbFileNameSize, NonPagedPool);
        if (NULL == FileName) {
            Status = STATUS_NO_MEMORY;
            break;
        }
        RtlZeroMemory(FileName, cbFileNameSize);
        
        Status = RtlStringCbCopyW(FileName, cbFileNameSize, DOS_DEVICES_PATH);
        ASSERT(STATUS_SUCCESS == Status);
        if (Status != STATUS_SUCCESS) {
            Status = STATUS_UNSUCCESSFUL;
            break;
        }

        Status = RtlStringCbCatNW(FileName, cbFileNameSize, (PWCHAR)pVolumeInfo, cbInputSize);
        ASSERT(STATUS_SUCCESS == Status);
        if (Status != STATUS_SUCCESS) {
            Status = STATUS_UNSUCCESSFUL;
            break;
        }
        
        pCommand->ulFlags |= COMMAND_FLAGS_WAITING_FOR_COMPLETION;
        pCommand->ulCommand = VE_CMD_EXPORT_VOLUME_IMAGE;
        pCommand->Cmd.ExportVolumeFromFile.ulFlags = pAddVolumeImageInput->ulFlags;
        RtlInitUnicodeString(&pCommand->Cmd.ExportVolumeFromFile.usFileName, FileName);
        FileName = NULL;
        pCommand->Cmd.ExportVolumeFromFile.UniqueVolumeID = (PUCHAR)UniqueVolumeID;
        UniqueVolumeID = NULL;
        pCommand->Cmd.ExportVolumeFromFile.UniqueVolumeIDLength = UniqueVolumeIDLength;

        Status = QueueCmdToCmdQueue(DevExtension->CmdQueue, pCommand);

        KeWaitForSingleObject(&pCommand->WaitEvent, Executive, KernelMode, FALSE, NULL);
    } while (0);

    Irp->IoStatus.Information = 0;

    if (NULL != pCommand) {
        // Before freeing the command, copy the device name to output buffer.
        Status = pCommand->CmdStatus;

        if (NT_SUCCESS(pCommand->CmdStatus) && (0 != pCommand->Cmd.ExportVolumeFromFile.usDeviceName.Length)) {
            if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength) {
                RtlZeroMemory(Irp->AssociatedIrp.SystemBuffer, IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength);
                if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength >= (pCommand->Cmd.ExportVolumeFromFile.usDeviceName.Length + sizeof(WCHAR))) {
                    RtlCopyMemory(Irp->AssociatedIrp.SystemBuffer, pCommand->Cmd.ExportVolumeFromFile.usDeviceName.Buffer, 
                            pCommand->Cmd.ExportVolumeFromFile.usDeviceName.Length);
                    RtlZeroMemory(((PUCHAR)Irp->AssociatedIrp.SystemBuffer) + pCommand->Cmd.ExportVolumeFromFile.usDeviceName.Length, sizeof(WCHAR));
                    Irp->IoStatus.Information = pCommand->Cmd.ExportVolumeFromFile.usDeviceName.Length + sizeof(WCHAR);
                } else {
                    RtlZeroMemory(Irp->AssociatedIrp.SystemBuffer, sizeof(WCHAR));
                    Irp->IoStatus.Information = sizeof(WCHAR);
                }

                ULONG NameAndLinkLength = (pCommand->Cmd.ExportVolumeFromFile.usDeviceName.Length + sizeof(WCHAR)) +
                            pCommand->Cmd.ExportVolumeFromFile.usDeviceLinkName.Length + sizeof(WCHAR);

                ULONG LinkOffset = pCommand->Cmd.ExportVolumeFromFile.usDeviceName.Length + sizeof(WCHAR);
                ASSERT(0 < pCommand->Cmd.ExportVolumeFromFile.usDeviceLinkName.Length);
                if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength >= NameAndLinkLength) {
                    RtlCopyMemory((PCHAR)Irp->AssociatedIrp.SystemBuffer + LinkOffset, pCommand->Cmd.ExportVolumeFromFile.usDeviceLinkName.Buffer, 
                        pCommand->Cmd.ExportVolumeFromFile.usDeviceLinkName.Length);
                Irp->IoStatus.Information = NameAndLinkLength;
                }
            }
        }

        FreeAddVolumeImageCmd(pCommand);
    }

    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    if(NULL != UniqueVolumeID)
    {
        FREE_MEMORY(UniqueVolumeID);
        UniqueVolumeID = NULL;
    }

    if(NULL != FileName)
    {
        FREE_MEMORY(FileName);
        FileName = NULL;
    }

    IoReleaseRemoveLock(&DevExtension->IoRemoveLock, Irp);   
    return Status;
}

VOID
FreeAddVolumeImageCmd(PCOMMAND_STRUCT pCommand)
{
    PWCHAR  FileName;
    PWCHAR  DeviceName;

    FileName = pCommand->Cmd.ExportVolumeFromFile.usFileName.Buffer;
    RtlInitUnicodeString(&pCommand->Cmd.ExportVolumeFromFile.usFileName, NULL);
    if (NULL != FileName) {
        FREE_MEMORY(FileName);
    }

    DeviceName = pCommand->Cmd.ExportVolumeFromFile.usDeviceName.Buffer;
    RtlInitUnicodeString(&pCommand->Cmd.ExportVolumeFromFile.usDeviceName, NULL);
    if (NULL != DeviceName) {
        FREE_MEMORY(DeviceName);
    }

    if(pCommand->Cmd.ExportVolumeFromFile.usDeviceLinkName.Buffer)
    {
        FREE_MEMORY(pCommand->Cmd.ExportVolumeFromFile.usDeviceLinkName.Buffer);
        RtlInitUnicodeString(&pCommand->Cmd.ExportVolumeFromFile.usDeviceLinkName, NULL);
    }

    if (NULL != pCommand->Cmd.ExportVolumeFromFile.UniqueVolumeID) {
        FREE_MEMORY(pCommand->Cmd.ExportVolumeFromFile.UniqueVolumeID);
        pCommand->Cmd.ExportVolumeFromFile.UniqueVolumeID = NULL;
    }

    DereferenceCommand(pCommand);
}

VOID
VolumeExportCmdRoutine(PDEVICE_EXTENSION DeviceExtension, PCOMMAND_STRUCT pCommand)
{
    NTSTATUS    Status;

    switch (pCommand->ulCommand) {
        case VE_CMD_EXPORT_VOLUME_IMAGE:
            Status = ExportVolumeImage(DeviceExtension, pCommand);
            pCommand->CmdStatus = Status;
            break;
        case VE_CMD_EXPORT_VOLUME_FROM_RETENTION_LOG:
            Status = ExportVolumeFromRetentionLog(DeviceExtension, pCommand);
            pCommand->CmdStatus = Status;
            break;
        case VE_CMD_REMOVE_VOLUME:
            Status = RemoveVolumeForRetentionOrVolumeImage(DeviceExtension, pCommand);
            pCommand->CmdStatus = Status;
            break;
        default:
            ASSERT(0);
            break;
    }

    return;
}

NTSTATUS
ExportVolumeImage(PDEVICE_EXTENSION CtrlDevExtension, PCOMMAND_STRUCT pCommand)
{
    NTSTATUS                    Status = STATUS_SUCCESS;
    OBJECT_ATTRIBUTES           ObjectAttributes;
    HANDLE                      hFile = NULL;
    PWCHAR                      wDeviceName = NULL;
    PDEVICE_OBJECT              DeviceObject = NULL;
    PDEVICE_EXTENSION           DeviceExtension = NULL;
    IO_STATUS_BLOCK             IoStatus;
    FILE_STANDARD_INFORMATION   FileStandardInfo;
    ULONG                       ulVolumeNumber;
    GUID                        VolumeGuid;
    UNICODE_STRING              VolumeGuidString;
    VVolumeInfo                *VVolume = NULL;
    

    ulVolumeNumber = RtlFindClearBitsAndSet(&DriverContext.BitMap, 1, 0);
    if (0xFFFFFFFF == ulVolumeNumber)
        return STATUS_UNSUCCESSFUL;

    InitializeObjectAttributes(&ObjectAttributes, &pCommand->Cmd.ExportVolumeFromFile.usFileName, OBJ_CASE_INSENSITIVE, NULL, NULL);

    Status = ZwCreateFile(&hFile, GENERIC_READ | GENERIC_WRITE, &ObjectAttributes, &IoStatus, NULL, 
                    FILE_ATTRIBUTE_NORMAL | FILE_WRITE_ATTRIBUTES,
                    FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_OPEN,
                    FILE_NON_DIRECTORY_FILE | FILE_NO_INTERMEDIATE_BUFFERING | FILE_SYNCHRONOUS_IO_NONALERT,
                    NULL, 0);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    do
    {
        Status = ZwQueryInformationFile(hFile, &IoStatus, &FileStandardInfo, sizeof(FILE_STANDARD_INFORMATION),
                    FileStandardInformation);

        if (!NT_SUCCESS(Status))
            break;

        Status = ExUuidCreate(&VolumeGuid);
        if (!NT_SUCCESS(Status))
            break;

        Status = RtlStringFromGUID(VolumeGuid, &VolumeGuidString);
        if (!NT_SUCCESS(Status))
            break;

        wDeviceName = (PWCHAR)ALLOC_MEMORY(VV_MAX_CB_DEVICE_NAME, NonPagedPool);
        if (NULL == wDeviceName){
            Status = STATUS_NO_MEMORY;
            break;
        }

        Status = RtlStringCbPrintfW(wDeviceName, VV_MAX_CB_DEVICE_NAME, VV_VOLUME_NAME_PREFIX L"%u%wZ", ulVolumeNumber, &VolumeGuidString);
        if (STATUS_SUCCESS != Status)
            break;

        RtlInitUnicodeString(&pCommand->Cmd.ExportVolumeFromFile.usDeviceName, wDeviceName);
        wDeviceName = NULL;
        
        Status = IoCreateDevice(DriverContext.DriverObject, sizeof(DEVICE_EXTENSION), 
                                &pCommand->Cmd.ExportVolumeFromFile.usDeviceName, FILE_DEVICE_DISK,
                                0, FALSE, &DeviceObject);
        
        if (!NT_SUCCESS(Status))
            break;

        DeviceObject->Flags |= DO_DIRECT_IO;

        DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
        RtlZeroMemory(DeviceExtension, sizeof(DEVICE_EXTENSION));
        CopyUnicodeString(&DeviceExtension->usDeviceName, &pCommand->Cmd.ExportVolumeFromFile.usDeviceName, NonPagedPool);
        DeviceExtension->DeviceObject = DeviceObject;
        DeviceExtension->ulDeviceType = DEVICE_TYPE_VVOLUME_FROM_VOLUME_IMAGE;
        IoInitializeRemoveLock(&DeviceExtension->IoRemoveLock, VVTAG_VVOLUME_DEV_REMOVE_LOCK, 0, 0);

        DeviceExtension->IOCmdQueue = AllocateIOCmdQueue(DeviceExtension);//AllocateCmdQueue(DeviceExtension, VVOLUME_IMAGE_CMD_QUEUE_DESCRIPTION, VVolumeFromVolumeImageCmdRoutine);
        if (NULL == DeviceExtension->IOCmdQueue) {
            Status = STATUS_UNSUCCESSFUL;
            break;
        }
        DeviceExtension->hFile = hFile;
        hFile = NULL;
        DeviceExtension->uliVolumeSize.QuadPart = FileStandardInfo.EndOfFile.QuadPart;
        // Store the file name would help while debugging to relate device object to file.
        CopyUnicodeString(&DeviceExtension->usFileName, &pCommand->Cmd.ExportVolumeFromFile.usFileName, NonPagedPool);
        
        DeviceExtension->UniqueVolumeID = NULL;
        DeviceExtension->UniqueVolumeIDLength = 0;
        DeviceExtension->UniqueVolumeID = ALLOC_MEMORY(pCommand->Cmd.ExportVolumeFromFile.UniqueVolumeIDLength, 
                                                        NonPagedPool);

        if(NULL == DeviceExtension->UniqueVolumeID)
        {
            Status = STATUS_UNSUCCESSFUL;
            break;
        }
        DeviceExtension->UniqueVolumeIDLength = pCommand->Cmd.ExportVolumeFromFile.UniqueVolumeIDLength;
        
        RtlCopyMemory(DeviceExtension->UniqueVolumeID, pCommand->Cmd.ExportVolumeFromFile.UniqueVolumeID, 
                                                                            DeviceExtension->UniqueVolumeIDLength);
        CopyUnicodeString(&DeviceExtension->MountDevUniqueId, &VolumeGuidString, NonPagedPool);

        DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

        VVolume = (VVolumeInfo *)ALLOC_MEMORY_WITH_TAG(sizeof(VVolumeInfo), NonPagedPool, VVTAG_VVOLUME_STRUCT);
        
        if( !VVolume ) {
            
            Status = STATUS_UNSUCCESSFUL;
            break;
        }

        RtlZeroMemory(VVolume, sizeof(VVolumeInfo));

        ExInitializeFastMutex(&VVolume->hMutex);

        VVolume->DevExtension = DeviceExtension;

        DeviceExtension->VolumeContext = VVolume;

        Status = VVSendMountMgrVolumeArrivalNotification(DeviceExtension);

        if(!(NT_SUCCESS(Status)))
            break;

        CopyUnicodeString(&pCommand->Cmd.ExportVolumeFromFile.usDeviceLinkName, &DeviceExtension->usMountDevLink, PagedPool);
        DeviceExtension->ulFlags |= VV_DE_MOUNT_SUCCESS;
        AddDeviceToDeviceList(&DriverContext, DeviceObject);

    } while (0);

    RtlFreeUnicodeString(&VolumeGuidString);
    pCommand->CmdStatus = Status;

    if(NT_SUCCESS(Status))
        return Status;

    KeWaitForSingleObject(&DriverContext.hMutex, Executive, KernelMode, FALSE, NULL);

    if(DeviceExtension)
    {
        if(DeviceExtension->ListEntry.Flink)
            RemoveEntryList(&DeviceExtension->ListEntry);   //Remove Device Extension
        
        DeviceExtension->ListEntry.Flink = NULL;
        DeviceExtension->ListEntry.Blink = NULL;
    }

    KeReleaseMutex(&DriverContext.hMutex, FALSE);

    //Some Error occurred, Free Resources
    if(DeviceObject)
        VVCleanupVolume(DeviceExtension);

    if (NULL != hFile) {
        ZwClose(hFile);
        hFile = NULL;
    }

    return Status;
}

NTSTATUS
VVHandleAddVolumeFromRetentionLogIOCTL(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
)
{
    NTSTATUS            Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION   DeviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION  IoStackLocation = (PIO_STACK_LOCATION) IoGetCurrentIrpStackLocation(Irp);
    ULONG               InputBufferLength = IoStackLocation->Parameters.DeviceIoControl.InputBufferLength;
    PCOMMAND_STRUCT     pCommand = NULL;
    PADD_RETENTION_IOCTL_DATA    InputData = (PADD_RETENTION_IOCTL_DATA)Irp->AssociatedIrp.SystemBuffer;

    Irp->IoStatus.Information = 0;

    do {
        if (InputBufferLength < (sizeof(ADD_RETENTION_IOCTL_DATA))) {
            Status = STATUS_INVALID_PARAMETER;
            break;
        }

        if (InputData->UniqueVolumeIdLength > (FIELD_OFFSET(ADD_RETENTION_IOCTL_DATA,VolumeInfo) - 
                                                            FIELD_OFFSET(ADD_RETENTION_IOCTL_DATA,UniqueVolumeID))) {
            Status = STATUS_INVALID_PARAMETER;
            break;
        }

        pCommand = AllocateCommand();
        if (pCommand == NULL) {
            Status = STATUS_NO_MEMORY;
            break;
        }
        pCommand->Cmd.ExportVolumeFromRetentionLog.UniqueVolumeIdLength = InputData->UniqueVolumeIdLength;
        RtlCopyMemory(pCommand->Cmd.ExportVolumeFromRetentionLog.UniqueVolumeID,InputData->UniqueVolumeID,InputData->UniqueVolumeIdLength);
        pCommand->ulFlags |= COMMAND_FLAGS_WAITING_FOR_COMPLETION;
        pCommand->ulCommand = VE_CMD_EXPORT_VOLUME_FROM_RETENTION_LOG;
        // pCommand->Cmd.ExportVolumeFromRetentionLog.ulFlags is initialized to zero
        // Recovery point can be timestamp or tag
        pCommand->Cmd.ExportVolumeFromRetentionLog.VolumeInfoLength = InputBufferLength - FIELD_OFFSET(ADD_RETENTION_IOCTL_DATA,VolumeInfo);
        pCommand->Cmd.ExportVolumeFromRetentionLog.VolumeInfo = (PUCHAR) ALLOC_MEMORY(pCommand->Cmd.ExportVolumeFromRetentionLog.VolumeInfoLength, NonPagedPool);
        if (NULL == pCommand->Cmd.ExportVolumeFromRetentionLog.VolumeInfo) {
            DereferenceCommand(pCommand);
            pCommand = NULL;
            Status = STATUS_NO_MEMORY;
            break;
        }       
        RtlCopyMemory(pCommand->Cmd.ExportVolumeFromRetentionLog.VolumeInfo, InputData->VolumeInfo, pCommand->Cmd.ExportVolumeFromRetentionLog.VolumeInfoLength);
        // By default VolumeExportCmdRoutine is executed. pCommand->CmdRoutine is set if a different routine has to be executed.
        // pCommand->CmdRoutine = VolumeExportCmdRoutine;
        Status = QueueCmdToCmdQueue(DeviceExtension->CmdQueue, pCommand);
        if (Status == STATUS_SUCCESS) {
            Status = KeWaitForSingleObject(&pCommand->WaitEvent, Executive, KernelMode, FALSE, NULL);
        } else {
            pCommand->CmdStatus = Status;
        }

    } while (0);

    bool bReported = false;

    if (NULL != pCommand) {
        Status = pCommand->CmdStatus;

        if (NT_SUCCESS(pCommand->CmdStatus) && (0 != pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceName.Length)) {
            if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength) {
                RtlZeroMemory(Irp->AssociatedIrp.SystemBuffer, IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength);
                if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength >= (pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceName.Length + sizeof(WCHAR))) {
                    RtlCopyMemory(Irp->AssociatedIrp.SystemBuffer, pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceName.Buffer, 
                        pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceName.Length);
                    RtlZeroMemory(((PUCHAR)Irp->AssociatedIrp.SystemBuffer) + pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceName.Length, sizeof(WCHAR));
                    Irp->IoStatus.Information = pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceName.Length + sizeof(WCHAR);
                } else {
                    RtlZeroMemory(Irp->AssociatedIrp.SystemBuffer, sizeof(WCHAR));
                    Irp->IoStatus.Information = sizeof(WCHAR);
                }

                ULONG NameAndLinkLength = (pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceName.Length + sizeof(WCHAR)) +
                                            pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceLinkName.Length + sizeof(WCHAR);

                ULONG LinkOffset = pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceName.Length + sizeof(WCHAR);
                
                ASSERT(0 < pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceLinkName.Length);

                if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength >= NameAndLinkLength) {
                    RtlCopyMemory((PCHAR)Irp->AssociatedIrp.SystemBuffer + LinkOffset, pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceLinkName.Buffer, 
                        pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceLinkName.Length);
                    Irp->IoStatus.Information = NameAndLinkLength;
                }
            }
        } else {
            InVirVolLogError(DriverContext.ControlDevice, __LINE__, INVIRVOL_VSNAP_MOUNT_FAILURE, STATUS_SUCCESS, pCommand->CmdStatus);
             bReported = true;
        }

        FreeAddVolumeFromRetentionLogCmd(pCommand);
    }
    if (!bReported && (!NT_SUCCESS(Status))) {
        InVirVolLogError(DriverContext.ControlDevice, __LINE__, INVIRVOL_VSNAP_MOUNT_FAILURE, STATUS_SUCCESS, Status);
    }
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    IoReleaseRemoveLock(&DeviceExtension->IoRemoveLock, Irp);
    return Status;
}

NTSTATUS
ExportVolumeFromRetentionLog(
    PDEVICE_EXTENSION CtrlDevExtension, 
    PCOMMAND_STRUCT pCommand
)
{
    NTSTATUS                Status                      = STATUS_SUCCESS;
    ULONG                   ulVolumeNumber              = 0;
    PWCHAR                  wDeviceName                 = NULL;
    PDEVICE_OBJECT          DeviceObject                = NULL;
    PDEVICE_EXTENSION       DeviceExtension             = NULL;
    PDEVICE_EXTENSION       VolumeDeviceExtension       = NULL;
    ULONG                   ulRequestBufferLength       = 0;
    PVOID                   UniqueIDBuffer              = NULL;
    LONGLONG                VolumeSize                  = 0;
    ULONG                   RetryCount                  = 0;
    KIRQL                   oldIrql;
    GUID                    VolumeGuid;
    UNICODE_STRING          VolumeGuidString;
    HANDLE              hThread;
    

    ulVolumeNumber = RtlFindClearBitsAndSet(&DriverContext.BitMap, 1, 0);
    if (0xFFFFFFFF == ulVolumeNumber)
        return STATUS_UNSUCCESSFUL;

    UniqueIDBuffer = pCommand->Cmd.ExportVolumeFromRetentionLog.UniqueVolumeID;

    //If there is already a device with the passed ID return without mounting device.
    KeWaitForSingleObject(&DriverContext.hMutex, Executive, KernelMode, FALSE, NULL);
    VolumeDeviceExtension = GetVolumeFromVolumeID((PCHAR)UniqueIDBuffer , (ULONG)pCommand->Cmd.ExportVolumeFromRetentionLog.UniqueVolumeIdLength);

    if(NULL != VolumeDeviceExtension)
    {
        CopyUnicodeString(&pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceName, &VolumeDeviceExtension->usDeviceName, PagedPool);
        CopyUnicodeString(&pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceLinkName, &VolumeDeviceExtension->usMountDevLink, PagedPool);
        KeReleaseMutex(&DriverContext.hMutex, FALSE);
        return STATUS_SUCCESS;
    }
    
    KeReleaseMutex(&DriverContext.hMutex, FALSE);

    //Allocate memory to all blocks
    do{
        Status = ExUuidCreate(&VolumeGuid);
        RetryCount++;
    }while(Status == STATUS_RETRY && RetryCount < MAX_RETRY_FOR_CREATE_GUID);

    if(!NT_SUCCESS(Status)) {
        return Status;
    }

    Status = RtlStringFromGUID(VolumeGuid, &VolumeGuidString);
    if(!NT_SUCCESS(Status)) {
        return Status;
    }

    wDeviceName = (PWCHAR)ALLOC_MEMORY(VV_MAX_CB_DEVICE_NAME, NonPagedPool);
    if (NULL == wDeviceName) {
        InVolDbgPrint(DL_VV_WARNING, DM_VV_DEVICE_CONTROL, ("ExportVolumeFromRetentionLog: Could not allocate memory for DeviceName\n"));
        Status = STATUS_NO_MEMORY;
        goto cleanup_and_exit;
    }

    Status = RtlStringCbPrintfW(wDeviceName, VV_MAX_CB_DEVICE_NAME, VV_VOLUME_NAME_PREFIX L"%u%wZ", ulVolumeNumber, &VolumeGuidString);
    if (!NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_VV_WARNING, DM_VV_DEVICE_CONTROL, ("ExportVolumeFromRetentionLog: RtlStringCbPrintfW Failed for Device Name\n"));
        goto cleanup_and_exit;
    }
    RtlInitUnicodeString(&pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceName, wDeviceName);
    wDeviceName = NULL;

    Status = IoCreateDevice(
                DriverContext.DriverObject, 
                sizeof(DEVICE_EXTENSION), 
                &pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceName, 
                FILE_DEVICE_DISK, 
                0,
                FALSE, 
                &DeviceObject
                );

    if(!NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_VV_WARNING, DM_VV_DEVICE_CONTROL, 
            ("ExportVolumeFromRetentionLog: IoCreateDevice Failed Device Name:%wZ\n", &pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceName));
        goto cleanup_and_exit;
    }

    InVolDbgPrint(DL_VV_WARNING, DM_VV_DEVICE_CONTROL, 
            ("ExportVolumeFromRetentionLog: Device Name:%wZ\n", &pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceName));

    DeviceObject->Flags |= DO_DIRECT_IO;
    DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    RtlZeroMemory(DeviceExtension, sizeof(DEVICE_EXTENSION));

    DeviceExtension->DeviceObject = DeviceObject;
    CopyUnicodeString(&DeviceExtension->usDeviceName, &pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceName, PagedPool);
    DeviceExtension->ulDeviceType = DEVICE_TYPE_VVOLUME_FROM_RETENTION_LOG;
    IoInitializeRemoveLock(&DeviceExtension->IoRemoveLock, VVTAG_VVOLUME_DEV_REMOVE_LOCK, 0, 0);

    //Call Retention API and notify about volume mount.
    CopyUnicodeString(&DeviceExtension->MountDevUniqueId, &VolumeGuidString, NonPagedPool);
    DeviceExtension->UniqueVolumeIDLength = pCommand->Cmd.ExportVolumeFromRetentionLog.UniqueVolumeIdLength;
    DeviceExtension->UniqueVolumeID = ALLOC_MEMORY(DeviceExtension->UniqueVolumeIDLength, 
                                                    NonPagedPool);
    if (NULL == DeviceExtension->UniqueVolumeID) {
        Status = STATUS_NO_MEMORY;
        goto cleanup_and_exit;
    }   

    RtlCopyMemory(DeviceExtension->UniqueVolumeID, pCommand->Cmd.ExportVolumeFromRetentionLog.UniqueVolumeID , 
                                                                        DeviceExtension->UniqueVolumeIDLength);
    DeviceExtension->ulVolumeNumber = ulVolumeNumber;
    InitializeListHead(&DeviceExtension->DiskChangeListHead);

    MultiPartSparseFileInfo SparseFileInfo;
    SparseFileInfo.IsMultiPartSparseFile = false;

    if (sizeof(VsnapMountInfo_V2) == (ULONG)pCommand->Cmd.ExportVolumeFromRetentionLog.VolumeInfoLength) {
        VsnapMountInfo_V2 *MountInfo = (VsnapMountInfo_V2 *)pCommand->Cmd.ExportVolumeFromRetentionLog.VolumeInfo;
        SparseFileInfo.IsMultiPartSparseFile = true;
        SparseFileInfo.ChunkSize = MountInfo->SparseFileChunkSize;
        if(-1 == IsStringNULLTerminated(MountInfo->FileExtension , MAX_FILE_EXT_LENGTH ))
        {
            InVolDbgPrint(DL_VV_ERROR, DM_VV_DEVICE_CONTROL, ("FileExtension is not correct \n"));
            return STATUS_INVALID_PARAMETER;
            goto cleanup_and_exit;
       }
		RtlCopyMemory(&SparseFileInfo.FileExt, &MountInfo->FileExtension, MAX_FILE_EXT_LENGTH);
    }

    Status = VsnapMount((PCHAR)pCommand->Cmd.ExportVolumeFromRetentionLog.VolumeInfo , 
                        pCommand->Cmd.ExportVolumeFromRetentionLog.VolumeInfoLength,
                        &VolumeSize, DeviceExtension,
                        (SparseFileInfo.IsMultiPartSparseFile == true) ? &SparseFileInfo : NULL);
    if (!NT_SUCCESS(Status)) 
    {
        goto cleanup_and_exit;
    }
    
    DeviceExtension->IOCmdQueue = AllocateIOCmdQueue(DeviceExtension);//, VVOLUME_RETENTION_LOG_CMD_QUEUE_DESCRIPTION, VVolumeFromRetentionLogCmdRoutine);
    if (NULL == DeviceExtension->IOCmdQueue) {
        InVolDbgPrint(DL_VV_WARNING, DM_VV_DEVICE_CONTROL, ("ExportVolumeFromRetentionLog: Could not create command queue: %wZ\n", 
                                                            &pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceName));
        Status = STATUS_UNSUCCESSFUL;
        goto cleanup_and_exit;
    }

    DeviceExtension->uliVolumeSize.QuadPart = VolumeSize;

    DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

     Status = PsCreateSystemThread(&hThread, THREAD_ALL_ACCESS, NULL, NULL, NULL, VsnapUpdateWorkerThread, DeviceExtension->VolumeContext);
    if (NT_SUCCESS(Status))
    {
        Status = ObReferenceObjectByHandle(hThread, SYNCHRONIZE, NULL, KernelMode, (PVOID *)&(((VsnapInfo *)(DeviceExtension->VolumeContext))->UpdateThread), NULL);
        if (NT_SUCCESS(Status))
            ZwClose(hThread);
        else 
            goto cleanup_and_exit;
    }else {
        goto cleanup_and_exit;
    }
    
    Status = VVSendMountMgrVolumeArrivalNotification(DeviceExtension);

    if(!(NT_SUCCESS(Status)))
        goto cleanup_and_exit;

    if(!(DeviceExtension->ulFlags & VV_DE_MOUNT_LINK_CREATED))
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;     //Possible low memory condition.
        goto cleanup_and_exit;
    }

    CopyUnicodeString(&pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceLinkName, &DeviceExtension->usMountDevLink, PagedPool);

    DeviceExtension->ulFlags |= VV_DE_MOUNT_SUCCESS;
    AddDeviceToDeviceList(&DriverContext, DeviceObject);

cleanup_and_exit:

    ASSERT(NULL != pCommand);
    pCommand->CmdStatus = Status;

    RtlFreeUnicodeString(&VolumeGuidString);
    
    if(NT_SUCCESS(Status))  //Normal Exit
        return Status;

    //Some Error occurred, Free Resources
    KeWaitForSingleObject(&DriverContext.hMutex, Executive, KernelMode, FALSE, NULL);

    if(DeviceExtension)
    {
        if(DeviceExtension->ListEntry.Flink)
            RemoveEntryList(&DeviceExtension->ListEntry);   //Remove Device Extension

        DeviceExtension->ListEntry.Flink = NULL;
        DeviceExtension->ListEntry.Blink = NULL;
    }

    KeReleaseMutex(&DriverContext.hMutex, FALSE);

    if(DeviceObject)
        VVCleanupVolume(DeviceExtension);
    
    return Status;
}


NTSTATUS
VVHandleAddVolumeFromMultiPartSparseFile(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
)
{
    NTSTATUS            Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION   DeviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION  IoStackLocation = (PIO_STACK_LOCATION) IoGetCurrentIrpStackLocation(Irp);
    ULONG               InputBufferLength = IoStackLocation->Parameters.DeviceIoControl.InputBufferLength;
    PCOMMAND_STRUCT     pCommand = NULL;
    PADD_RETENTION_IOCTL_DATA    InputData = (PADD_RETENTION_IOCTL_DATA)Irp->AssociatedIrp.SystemBuffer;

    Irp->IoStatus.Information = 0;

    do {
        if (InputBufferLength < (sizeof(ADD_RETENTION_IOCTL_DATA))) {
            Status = STATUS_INVALID_PARAMETER;
            break;
        }

        if (InputData->UniqueVolumeIdLength > (FIELD_OFFSET(ADD_RETENTION_IOCTL_DATA,VolumeInfo) - 
                                                          FIELD_OFFSET(ADD_RETENTION_IOCTL_DATA,UniqueVolumeID))) {
            Status = STATUS_INVALID_PARAMETER;
            break;
        }

        pCommand = AllocateCommand();
        if (pCommand == NULL) {
            Status = STATUS_NO_MEMORY;
            break;
        }
        pCommand->Cmd.ExportVolumeFromRetentionLog.UniqueVolumeIdLength = InputData->UniqueVolumeIdLength;
        RtlCopyMemory(pCommand->Cmd.ExportVolumeFromRetentionLog.UniqueVolumeID,InputData->UniqueVolumeID,InputData->UniqueVolumeIdLength);
        pCommand->ulFlags |= COMMAND_FLAGS_WAITING_FOR_COMPLETION;
        pCommand->ulCommand = VE_CMD_EXPORT_VOLUME_FROM_RETENTION_LOG;
        pCommand->Cmd.ExportVolumeFromRetentionLog.VolumeInfoLength = InputBufferLength - FIELD_OFFSET(ADD_RETENTION_IOCTL_DATA,VolumeInfo);
        pCommand->Cmd.ExportVolumeFromRetentionLog.VolumeInfo = (PUCHAR) ALLOC_MEMORY(pCommand->Cmd.ExportVolumeFromRetentionLog.VolumeInfoLength,                                                                                           NonPagedPool);
        if (NULL == pCommand->Cmd.ExportVolumeFromRetentionLog.VolumeInfo) {
            if (NULL != pCommand) {
                DereferenceCommand(pCommand);
            }
            pCommand = NULL;
            Status = STATUS_NO_MEMORY;
            break;
        }       
        RtlCopyMemory(pCommand->Cmd.ExportVolumeFromRetentionLog.VolumeInfo, InputData->VolumeInfo, 
                                 pCommand->Cmd.ExportVolumeFromRetentionLog.VolumeInfoLength);
        Status = QueueCmdToCmdQueue(DeviceExtension->CmdQueue, pCommand);
        if (Status == STATUS_SUCCESS) {
            Status = KeWaitForSingleObject(&pCommand->WaitEvent, Executive, KernelMode, FALSE, NULL);
        } else {
            pCommand->CmdStatus = Status;
        }

    } while (0);

    bool bReported = false;

    if (NULL != pCommand) {
        Status = pCommand->CmdStatus;

        if (NT_SUCCESS(pCommand->CmdStatus) && (0 != pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceName.Length)) {
            if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength) {
                RtlZeroMemory(Irp->AssociatedIrp.SystemBuffer, IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength);
                if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength >= (
                                       pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceName.Length + sizeof(WCHAR))) {
                    RtlCopyMemory(Irp->AssociatedIrp.SystemBuffer, pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceName.Buffer, 
                        pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceName.Length);
                    RtlZeroMemory(((PUCHAR)Irp->AssociatedIrp.SystemBuffer) + pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceName.Length, 
                                   sizeof(WCHAR));
                    Irp->IoStatus.Information = pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceName.Length + sizeof(WCHAR);
                } else {
                    RtlZeroMemory(Irp->AssociatedIrp.SystemBuffer, sizeof(WCHAR));
                    Irp->IoStatus.Information = sizeof(WCHAR);
                }

                ULONG NameAndLinkLength = (pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceName.Length + sizeof(WCHAR)) +
                                            pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceLinkName.Length + sizeof(WCHAR);

                ULONG LinkOffset = pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceName.Length + sizeof(WCHAR);
                
                ASSERT(0 < pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceLinkName.Length);

                if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength >= NameAndLinkLength) {
                    RtlCopyMemory((PCHAR)Irp->AssociatedIrp.SystemBuffer + LinkOffset, 
                                 pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceLinkName.Buffer, 
                                 pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceLinkName.Length);
                    Irp->IoStatus.Information = NameAndLinkLength;
                }
            }
        }
        if (!NT_SUCCESS(pCommand->CmdStatus)) {
            bReported = true;
            InVirVolLogError(DriverContext.ControlDevice, __LINE__, INVIRVOL_VSNAP_MOUNT_FAILURE, STATUS_SUCCESS, pCommand->CmdStatus);
        }
        FreeAddVolumeFromRetentionLogCmd(pCommand);
    }
    if (!bReported && (!NT_SUCCESS(Status))) {
        InVirVolLogError(DriverContext.ControlDevice, __LINE__, INVIRVOL_VSNAP_MOUNT_FAILURE, STATUS_SUCCESS, Status);
    }
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    IoReleaseRemoveLock(&DeviceExtension->IoRemoveLock, Irp);
    return Status;
}


NTSTATUS
VVSendMountMgrVolumeArrivalNotification(PDEVICE_EXTENSION DeviceExtension)
{
    NTSTATUS                Status                          = STATUS_SUCCESS;
    PVOID                   InputBuffer                     = NULL;
    size_t                  InputBufferSize                 = 0;
    PFILE_OBJECT            MountMgrFileObject              = NULL;
    PDEVICE_OBJECT          MountMgrDeviceObject            = NULL;
    PMOUNTMGR_TARGET_NAME   MountMgrTargetName              = NULL;
    PIRP                    MountMgrVolumeArrivalNotifyIrp  = NULL;
    IO_STATUS_BLOCK         IoStatus;
    UNICODE_STRING          MountMgrName;
    KEVENT                  Event;
    KIRQL                   OldIrql;

    InVolDbgPrint(DL_VV_INFO, DM_VV_DEVICE_CONTROL, ("VVSendMountMgrVolumeArrivalNotification: Called - DeviceObject: %#p\n", DeviceExtension));

    RtlInitUnicodeString(&MountMgrName, MOUNTMGR_DEVICE_NAME);
    Status = IoGetDeviceObjectPointer(&MountMgrName, FILE_READ_ATTRIBUTES, &MountMgrFileObject, &MountMgrDeviceObject);
    if(!NT_SUCCESS(Status))
    {
        InVolDbgPrint(DL_VV_WARNING, DM_VV_DRIVER_INIT, ("VVSendMountMgrVolumeArrivalNotification: Could not obtain mount manager device object\n"));
        return Status;
    }

    //Input MOUNTMGR_TARGET_NAME = (DeviceNameLength -> USHORT) + (DeviceName -> WCHAR[DeviceNameLength])
    InputBufferSize = sizeof(USHORT) + DeviceExtension->usDeviceName.Length;    
    InputBuffer     = (PVOID) ALLOC_MEMORY(InputBufferSize, PagedPool);
    if(NULL == InputBuffer)
    {
        ObDereferenceObject(MountMgrFileObject);
        return STATUS_NO_MEMORY;
    }

    RtlZeroMemory(InputBuffer, InputBufferSize);
    
    MountMgrTargetName                   = (PMOUNTMGR_TARGET_NAME)InputBuffer;
    MountMgrTargetName->DeviceNameLength = DeviceExtension->usDeviceName.Length;
    RtlCopyMemory(MountMgrTargetName->DeviceName, DeviceExtension->usDeviceName.Buffer, DeviceExtension->usDeviceName.Length);

    KeInitializeEvent(&Event, NotificationEvent, FALSE);
    MountMgrVolumeArrivalNotifyIrp = IoBuildDeviceIoControlRequest(
                    IOCTL_MOUNTMGR_VOLUME_ARRIVAL_NOTIFICATION,
                    MountMgrDeviceObject,
                    InputBuffer,
                    InputBufferSize,
                    NULL,
                    0,
                    FALSE,
                    &Event,
                    &IoStatus
                    );

    Status = IoCallDriver(MountMgrDeviceObject, MountMgrVolumeArrivalNotifyIrp);

    InVolDbgPrint(DL_VV_ERROR, DM_VV_DRIVER_INIT, 
            ("VVSendMountMgrVolumeArrivalNotification IOCTL_MOUNTMGR_VOLUME_ARRIVAL_NOTIFICATION Status:%x IoStatus:%x\n", 
            Status, IoStatus.Status));

    if(Status == STATUS_PENDING)
    {
        InVolDbgPrint(DL_VV_INFO, DM_VV_DRIVER_INIT, ("VVSendMountMgrVolumeArrivalNotification: \
                    Waiting for IOCTL_MOUNTMGR_VOLUME_ARRIVAL_NOTIFICATION\n"));
        KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
    }

    if(MountMgrFileObject)
        ObDereferenceObject(MountMgrFileObject);

    FREE_MEMORY(InputBuffer);

    ASSERT(NT_SUCCESS(Status));
    ASSERT(NT_SUCCESS(IoStatus.Status));

    Status = IoStatus.Status;
    return Status;
}

NTSTATUS
CleanupVolumeContext(PDEVICE_EXTENSION DeviceExtension)
{
    NTSTATUS Status = STATUS_SUCCESS;
    if(DeviceExtension->ulDeviceType == DEVICE_TYPE_VVOLUME_FROM_RETENTION_LOG)
    {
        if(DeviceExtension->VolumeContext) {
            VsnapUnmount(DeviceExtension->VolumeContext);
            DeviceExtension->VolumeContext = NULL;
        }        

    } else if(DeviceExtension->ulDeviceType == DEVICE_TYPE_VVOLUME_FROM_VOLUME_IMAGE) {
        FREE_MEMORY(DeviceExtension->VolumeContext);
    }

    if(DeviceExtension->hFile)
    {
        ZwClose(DeviceExtension->hFile);
        DeviceExtension->hFile = NULL;
    }

    return Status;
}

VOID
FreeAddVolumeFromRetentionLogCmd(PCOMMAND_STRUCT pCommand)
{
    if(pCommand == NULL)
        return;
    
    if(pCommand->Cmd.ExportVolumeFromRetentionLog.VolumeInfo != NULL)
        FREE_MEMORY(pCommand->Cmd.ExportVolumeFromRetentionLog.VolumeInfo);

    if(pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceLinkName.Buffer)
    {
        FREE_MEMORY(pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceLinkName.Buffer);
        RtlInitUnicodeString(&pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceLinkName, NULL);
    }

    if(pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceName.Buffer)
    {
        FREE_MEMORY(pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceName.Buffer);
        RtlInitUnicodeString(&pCommand->Cmd.ExportVolumeFromRetentionLog.usDeviceName, NULL);
    }

    DereferenceCommand(pCommand);
}


NTSTATUS
RemoveVolumeForRetentionOrVolumeImage(
    PDEVICE_EXTENSION CtrlDevExtension, 
    PCOMMAND_STRUCT pCommand
)
{
    NTSTATUS            Status = STATUS_SUCCESS;
    ULONG               InputBufferLength = pCommand->Cmd.RemoveVolumeFromRetentionOrImage.VolumeInfoLength;
    PVOID               InputBuffer = pCommand->Cmd.RemoveVolumeFromRetentionOrImage.VolumeInfo;
    PLIST_ENTRY         CurEntry = NULL;
    PDEVICE_EXTENSION   VolumeDeviceExtension = NULL;

    
    KeWaitForSingleObject(&DriverContext.hMutex, Executive, KernelMode, FALSE, NULL);

    VolumeDeviceExtension = GetVolumeFromVolumeID(InputBuffer, InputBufferLength);
    
    if(VolumeDeviceExtension == NULL)
    {
        Status = STATUS_NO_SUCH_DEVICE;
        goto Exit;
    }

    VolumeDeviceExtension->ulFlags |= FLAG_FREE_VSNAP;

    if(VolumeDeviceExtension->ListEntry.Flink)
    {
        RemoveEntryList(&VolumeDeviceExtension->ListEntry);
        VolumeDeviceExtension->ListEntry.Flink = NULL;
        VolumeDeviceExtension->ListEntry.Blink = NULL;
    }

    RtlFindSetBitsAndClear(&DriverContext.BitMap, 1, VolumeDeviceExtension->ulVolumeNumber);

    Status = IoAcquireRemoveLock(&VolumeDeviceExtension->IoRemoveLock, VolumeDeviceExtension);
    if(!NT_SUCCESS(Status))
        goto Exit;
    
    IoReleaseRemoveLockAndWait(&VolumeDeviceExtension->IoRemoveLock, VolumeDeviceExtension);
    
    VVCleanupVolume(VolumeDeviceExtension);    

Exit:
    
    KeReleaseMutex(&DriverContext.hMutex, FALSE);
    return Status;
}