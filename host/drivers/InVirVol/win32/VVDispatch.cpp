#define INITGUID
#include "VVDriverContext.h"
#include "VVIDevControl.h"
#include "VVDispatch.h"
#include "VVolumeImage.h"
#include "VVolumeRetentionLog.h"
#include "IoctlInfo.h"
#include <VsnapKernelAPI.h>
#include <mountmgr.h>
#include <DiskChange.h>
#include "VVthreadPool.h"
#include "invirvollog.h"

#define INMAGE_KERNEL_MODE
#include <Fileio.h>

// ulDebugLevelForAll specifies the debug level for all modules
// if this is non zero all modules info upto this level is displayed.
ULONG   ulDebugLevelForAll = DL_VV_WARNING;
// ulDebugLevel specifies the debug level for modules specifed in
// ulDebugMask
ULONG   ulDebugLevel = DL_VV_TRACE_L2;
// ulDebugMask specifies the modules whose debug level is specified
// in ulDebugLevel
ULONG   ulDebugMask = DM_VV_WRITE | DM_VV_DRIVER_INIT | DM_VV_DRIVER_UNLOAD | DM_VV_DEVICE_CONTROL | DM_VV_READ | DM_VV_IOCTL_TRACE |DM_VV_PNP;

DRIVER_CONTEXT  DriverContext;

/*++

Routine Description:

    Installable driver initialization entry point.
    This entry point is called directly by the I/O manager to set up the driver

Arguments:

    DriverObject - Inmage virtual volume driver object.

    RegistryPath - pointer to a unicode string representing the path,
                   to driver-specific key in the registry.

Return Value:

    STATUS_SUCCESS if successful

--*/

extern "C" NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
{
    NTSTATUS    Status = STATUS_SUCCESS;

    InVolDbgPrint(DL_VV_INFO, DM_VV_DRIVER_INIT, ("DriverEntry: Called - DriverObject %#p\nCompiled %s %s\n", DriverObject, __DATE__, __TIME__));
    if(STATUS_SUCCESS == BypassCheckBootIni())
    {
        //VsnapLogError(DriverObject,__LINE__,L"INVIRVOL in bypass mode");
        Status= STATUS_UNSUCCESSFUL;
        return Status;
    }

    Status = InitializeDriverContext(&DriverContext, DriverObject, RegistryPath);
    if (!NT_SUCCESS(Status)) {
        CleanupDriverContext(&DriverContext);
        return Status;
    }

    DriverObject->MajorFunction[IRP_MJ_CREATE] = VVDispatchCreate;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = VVDispatchClose;
    DriverObject->MajorFunction[IRP_MJ_READ] = VVDispatchRead;
    DriverObject->MajorFunction[IRP_MJ_WRITE] = VVDispatchWrite;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = VVDispatchDeviceControl;
	DriverObject->MajorFunction[IRP_MJ_FLUSH_BUFFERS] = VVDispatchFlushBuffers;

    DriverObject->DriverUnload = VVDriverUnload;
    return Status;
}

NTSTATUS
VVDispatchCreate(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    )
{
    NTSTATUS    Status = STATUS_SUCCESS;
    PAGED_CODE();

    InVolDbgPrint(DL_VV_INFO, DM_VV_CREATE, ("VVDispatchCreate: Called - DeviceObject %#p\n", DeviceObject));

    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = FILE_OPENED;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS
VVDispatchClose(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    )
{
    NTSTATUS    Status = STATUS_SUCCESS;
    PAGED_CODE();

    InVolDbgPrint(DL_VV_INFO, DM_VV_CLOSE, ("VVDispatchClose: Called - DeviceObject %#p\n", DeviceObject));

    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = 0;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS
VVDispatchRead(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    )
{
    NTSTATUS    Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION   DevExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;

    InVolDbgPrint(DL_VV_VERBOSE, DM_VV_READ, ("VVDispatchRead: Called - DeviceObject %#p\n", DeviceObject));

    Status = IoAcquireRemoveLock(&DevExtension->IoRemoveLock, Irp);
    if (STATUS_SUCCESS != Status) {
        // Device is about to be removed. So do not continue any operations on device
        // returning STATUS_DELETE_PENDING returned from IoAcquireRemoveLock
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    switch(DevExtension->ulDeviceType) {
    case DEVICE_TYPE_VVOLUME_FROM_VOLUME_IMAGE:
        Status = VVolumeImageRead(DeviceObject, Irp);
        break;
    case DEVICE_TYPE_VVOLUME_FROM_RETENTION_LOG:
        Status = VVolumeRetentionLogRead(DeviceObject, Irp);
        break;
    default:
        Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        IoReleaseRemoveLock(&DevExtension->IoRemoveLock, Irp);
        break;
    }

    return Status;
}

NTSTATUS
VVDispatchWrite(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    )
{
    NTSTATUS    Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION   DevExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;

    InVolDbgPrint(DL_VV_INFO, DM_VV_WRITE, ("VVDispatchWrite: Called - DeviceObject %#p\n", DeviceObject));

    Status = IoAcquireRemoveLock(&DevExtension->IoRemoveLock, Irp);

    if (STATUS_SUCCESS != Status) {
        // Device is about to be removed. So do not continue any operations on device
        // returning STATUS_DELETE_PENDING returned from IoAcquireRemoveLock
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    switch(DevExtension->ulDeviceType) {
    case DEVICE_TYPE_VVOLUME_FROM_VOLUME_IMAGE:
        Status = VVolumeImageWrite(DeviceObject, Irp);
        break;
    case DEVICE_TYPE_VVOLUME_FROM_RETENTION_LOG:
        Status = VVolumeRetentionLogWrite(DeviceObject, Irp);
        break;
    default:
        Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        IoReleaseRemoveLock(&DevExtension->IoRemoveLock, Irp);
        break;
    }

    return Status;
}

NTSTATUS
VVDispatchDeviceControl(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    )
{
    NTSTATUS            Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION   DevExtension = NULL;

    PAGED_CODE();

    DevExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;

    Status = IoAcquireRemoveLock(&DevExtension->IoRemoveLock, Irp);
    if (STATUS_SUCCESS != Status) {
        // Device is about to be removed. So do not continue any operations on device
        // returning STATUS_DELETE_PENDING returned from IoAcquireRemoveLock
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    switch (DevExtension->ulDeviceType) {
    case DEVICE_TYPE_CONTROL:
        Status = VVHandleDeviceControlForControlDevice(DeviceObject, Irp);
        break;
    case DEVICE_TYPE_VVOLUME_FROM_VOLUME_IMAGE:
        Status = VVHandleDeviceControlForVolumeImageDevice(DeviceObject, Irp);
        break;
    case DEVICE_TYPE_VVOLUME_FROM_RETENTION_LOG:
        Status = VVHandleDeviceControlForVolumeRetentionDevice(DeviceObject, Irp);
        break;
    default:
        InVolDbgPrint(DL_VV_INFO, DM_VV_DEVICE_CONTROL, ("VVDispatchDeviceControl: Called - DeviceObject %#p\n", DeviceObject));
        ASSERT(0);
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        IoReleaseRemoveLock(&DevExtension->IoRemoveLock, Irp);
        break;
    }

    return Status;
}

NTSTATUS VVDispatchFlushBuffers(PDEVICE_OBJECT DeviceObject,PIRP Irp)
{
    PDEVICE_EXTENSION   DevExtension = NULL;
    NTSTATUS Status = STATUS_SUCCESS;

    DevExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    
    Status = IoAcquireRemoveLock(&DevExtension->IoRemoveLock, Irp);

    if (STATUS_SUCCESS != Status) {
        // Device is about to be removed. So do not continue any operations on device
        // returning STATUS_DELETE_PENDING returned from IoAcquireRemoveLock
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }
    // Since vsnap driver does not maintain any internal cache for data. Just returning success.
    InVolDbgPrint(DL_VV_INFO, DM_VV_DEVICE_CONTROL, ("VVDispatchFlushBuffers: Called - DeviceObject %#p\n", DeviceObject));

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    IoReleaseRemoveLock(&DevExtension->IoRemoveLock, Irp);
    return Status;
}
VOID
VVDriverUnload(IN PDRIVER_OBJECT DriverObject)
{
    PAGED_CODE();

    InVolDbgPrint(DL_VV_INFO, DM_VV_DRIVER_UNLOAD, ("VVDriverUnload: Called - DriverObject %#p\n", DriverObject));

    CleanupDriverContext(&DriverContext);

    return;
}

NTSTATUS
VVHandleDeviceControlForCommonIOCTLCodes(
    PDEVICE_OBJECT  DeviceObject,
    PIRP            Irp
    )
{
    NTSTATUS            Status;
    PDEVICE_EXTENSION   DevExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulIoControlCode;

    ulIoControlCode = IoStackLocation->Parameters.DeviceIoControl.IoControlCode;

    InVolDbgPrint(DL_VV_INFO, DM_VV_DEVICE_CONTROL, 
                  ("VVHandleDeviceControlForCommonIOCTLCodes: ulIoControlCode = %#x\n", ulIoControlCode));
#if DBG
    DumpIoctlInformation(DeviceObject, Irp);
#endif // DBG

    switch(ulIoControlCode) {
    case IOCTL_DISK_CHECK_VERIFY:
    case IOCTL_STORAGE_CHECK_VERIFY:
    case IOCTL_STORAGE_CHECK_VERIFY2:
    case IOCTL_DISK_IS_WRITABLE:
    case IOCTL_STORAGE_MEDIA_REMOVAL:
    case IOCTL_DISK_MEDIA_REMOVAL:
        Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = 0;
        break;
    case IOCTL_DISK_VERIFY:
        {
            // This IOCTL has VERIFY_INFORMATION as input data specifying the starting offset
            // and length to be verified.
            if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < sizeof(VERIFY_INFORMATION)) {
                Status = STATUS_INVALID_PARAMETER;
                Irp->IoStatus.Information = 0;
                break;
            }
            
            PVERIFY_INFORMATION VerifyInformation = (PVERIFY_INFORMATION) Irp->AssociatedIrp.SystemBuffer;

            Status = STATUS_SUCCESS;
            Irp->IoStatus.Information = VerifyInformation->Length;
            break;
        }
    case IOCTL_DISK_SET_PARTITION_INFO:
        if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < sizeof(SET_PARTITION_INFORMATION)) {
            Status = STATUS_INVALID_PARAMETER;
            Irp->IoStatus.Information = 0;
            break;
        }
        Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = 0;
        break;
    case IOCTL_DISK_GET_PARTITION_INFO:
        {
            PPARTITION_INFORMATION  pPartitionInformation = NULL;
            if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(PARTITION_INFORMATION)) {
                Status = STATUS_BUFFER_TOO_SMALL;
                Irp->IoStatus.Information = 0;
                break;
            }

            pPartitionInformation = (PPARTITION_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

            pPartitionInformation->StartingOffset.QuadPart = 0;
            pPartitionInformation->PartitionLength.QuadPart = DevExtension->uliVolumeSize.QuadPart;
            pPartitionInformation->HiddenSectors = 1;
            pPartitionInformation->BootIndicator = FALSE;
            pPartitionInformation->PartitionNumber = 0;
            pPartitionInformation->PartitionType = 0;
            pPartitionInformation->RecognizedPartition = FALSE;
            pPartitionInformation->RewritePartition = FALSE;

            Status = STATUS_SUCCESS;
            Irp->IoStatus.Information = sizeof(PARTITION_INFORMATION);
        }
        break;
#if (_WIN32_WINNT > 0x0500)
    case IOCTL_DISK_GET_PARTITION_INFO_EX:
        {
            PPARTITION_INFORMATION_EX   pPartitionInformationEx = NULL;

            if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(PARTITION_INFORMATION_EX)) {
                Status = STATUS_BUFFER_TOO_SMALL;
                Irp->IoStatus.Information = 0;
                break;
            }

            pPartitionInformationEx = (PPARTITION_INFORMATION_EX)Irp->AssociatedIrp.SystemBuffer;

            pPartitionInformationEx->PartitionStyle = PARTITION_STYLE_MBR;
            pPartitionInformationEx->StartingOffset.QuadPart = 0;
            pPartitionInformationEx->PartitionLength.QuadPart = DevExtension->uliVolumeSize.QuadPart;
            pPartitionInformationEx->PartitionNumber = 0;
            pPartitionInformationEx->RewritePartition = FALSE;
            pPartitionInformationEx->Mbr.PartitionType = 0;
            pPartitionInformationEx->Mbr.BootIndicator = FALSE;
            pPartitionInformationEx->Mbr.RecognizedPartition = FALSE;
            pPartitionInformationEx->Mbr.HiddenSectors = 1;

            Status = STATUS_SUCCESS;
            Irp->IoStatus.Information = sizeof(PARTITION_INFORMATION_EX);
        }
        break;
    case IOCTL_DISK_GET_LENGTH_INFO:
        {
            PGET_LENGTH_INFORMATION pGetLengthInformation = NULL;
            if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(GET_LENGTH_INFORMATION)) {
                Status = STATUS_BUFFER_TOO_SMALL;
                Irp->IoStatus.Information = 0;
                break;
            }

            pGetLengthInformation = (PGET_LENGTH_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
            pGetLengthInformation->Length.QuadPart = DevExtension->uliVolumeSize.QuadPart;

            Status = STATUS_SUCCESS;
            Irp->IoStatus.Information = sizeof(GET_LENGTH_INFORMATION);
        }
        break;
#endif // (_WIN32_WINNT > 0x0500)
    case IOCTL_DISK_GET_DRIVE_GEOMETRY:
        {
            PDISK_GEOMETRY  pDiskGeometry = NULL;

            if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof(DISK_GEOMETRY)) {
                Status = STATUS_BUFFER_TOO_SMALL;
                Irp->IoStatus.Information = 0;
                break;
            }

            pDiskGeometry = (PDISK_GEOMETRY) Irp->AssociatedIrp.SystemBuffer;
            ULONG SectorSize = DevExtension->ulSectorSize;
			// For strange reason, if sectorsize is zero..Just set it to 512 to avoid divide-by-zero error
            if (0 == SectorSize)
                SectorSize = SECTOR_SIZE;
            pDiskGeometry->Cylinders.QuadPart = DevExtension->uliVolumeSize.QuadPart / SectorSize / 32 / 64; // 2;
            pDiskGeometry->MediaType = FixedMedia;
            pDiskGeometry->TracksPerCylinder = 64; // 2;
            pDiskGeometry->SectorsPerTrack = 32;
            pDiskGeometry->BytesPerSector = SectorSize;

            Status = STATUS_SUCCESS;
            Irp->IoStatus.Information = sizeof(DISK_GEOMETRY);
        }
        break;
    default:
        Status = STATUS_INVALID_DEVICE_REQUEST;
    }

    if (STATUS_INVALID_DEVICE_REQUEST != Status) {
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        IoReleaseRemoveLock(&DevExtension->IoRemoveLock, Irp);
    }

    return Status;
}

NTSTATUS BypassCheckBootIni()
{
    NTSTATUS        Status= STATUS_SUCCESS;
    UNICODE_STRING  string;
    UNICODE_STRING  ucString;
    UNICODE_STRING  pattern;
    int             i;
    UNICODE_STRING  ustrValue;
    ULONG           ulResultLength = 0, ulKeyValueLength = 0;
    PKEY_VALUE_PARTIAL_INFORMATION pKeyValuePartialInformation = NULL;
    PWSTR           pValue = NULL;
    HANDLE   hRegistry;
    string.Buffer = NULL;
    string.Length = string.MaximumLength = 0;
    ucString.Buffer = NULL;
    ucString.Length = ucString.MaximumLength = 0;
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING keyName;

    RtlInitUnicodeString(&keyName,L"\\Registry\\Machine\\System\\CurrentControlSet\\Control");
    InitializeObjectAttributes(&attr,&keyName,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL, NULL);

    Status = ZwOpenKey(&hRegistry, GENERIC_READ, &attr);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    RtlInitUnicodeString(&ustrValue, L"SystemStartOptions");

    Status = ZwQueryValueKey(hRegistry, &ustrValue, KeyValuePartialInformation, NULL, 0, &ulResultLength);
    if ((STATUS_BUFFER_OVERFLOW == Status) || (STATUS_BUFFER_TOO_SMALL == Status)) {
        pKeyValuePartialInformation = (PKEY_VALUE_PARTIAL_INFORMATION) ALLOC_MEMORY(ulResultLength, PagedPool);
        if (NULL == pKeyValuePartialInformation)
            return STATUS_NO_MEMORY;
        ulKeyValueLength = ulResultLength;
        Status = ZwQueryValueKey(hRegistry, &ustrValue, KeyValuePartialInformation, 
                        pKeyValuePartialInformation, ulKeyValueLength, &ulResultLength);
        if ((STATUS_SUCCESS == Status) && (pKeyValuePartialInformation->Type == REG_SZ) &&
            (0 != pKeyValuePartialInformation->DataLength))
        {
            pValue = (PWSTR)ALLOC_MEMORY(pKeyValuePartialInformation->DataLength + sizeof(WCHAR),
                                        PagedPool);
            if (NULL == pValue)
            {
                FREE_MEMORY(pKeyValuePartialInformation);
                return STATUS_NO_MEMORY;
            }

            RtlMoveMemory((PVOID)pValue, pKeyValuePartialInformation->Data, pKeyValuePartialInformation->DataLength);
            // String returned from registry should be NULL terminated for saftey let us add another NULL.
            pValue[pKeyValuePartialInformation->DataLength / sizeof(WCHAR)] = L'\0';
           
        }
        FREE_MEMORY(pKeyValuePartialInformation);
    } 
    ZwClose(hRegistry);

    if(STATUS_SUCCESS == Status)
    {
         RtlInitUnicodeString(&string, pValue);
         RtlInitUnicodeString(&pattern, L"INVIRVOL=BYPASS");
         RtlUpcaseUnicodeString(&ucString, &string, TRUE);
         Status = STATUS_UNSUCCESSFUL;
         for(i = 0; i <= ((int)(ucString.Length/sizeof(WCHAR)) - 15); i++) {
                if ((ucString.Buffer[i +  0] == L'I') &&
                    (ucString.Buffer[i +  1] == L'N') &&
                    (ucString.Buffer[i +  2] == L'V') &&
                    (ucString.Buffer[i +  3] == L'I') &&
                    (ucString.Buffer[i +  4] == L'R') &&
                    (ucString.Buffer[i +  5] == L'V') &&
                    (ucString.Buffer[i +  6] == L'O') &&
                    (ucString.Buffer[i +  7] == L'L') &&
                    (ucString.Buffer[i +  8] == L'=') &&
                    (ucString.Buffer[i +  9] == L'B') &&
                    (ucString.Buffer[i + 10] == L'Y') &&
                    (ucString.Buffer[i + 11] == L'P') &&
                    (ucString.Buffer[i + 12] == L'A') &&
                    (ucString.Buffer[i + 13] == L'S') &&
                    (ucString.Buffer[i + 14] == L'S')) {
                        Status = STATUS_SUCCESS;
                        break;
                    } else {
                        Status = STATUS_UNSUCCESSFUL;
                    }
            }

    }
    if (NULL != pValue) {
        FREE_MEMORY(pValue);
    }

    if (NULL != ucString.Buffer) {
        RtlFreeUnicodeString(&ucString);
    }

    return Status;
}


void VsnapLogError(PDRIVER_OBJECT DriverObject,ULONG uniqueId, PWCHAR str)
{
    PIO_ERROR_LOG_PACKET errorLogEntry = NULL;
    LONG totalLength = sizeof(IO_ERROR_LOG_PACKET);
    UNICODE_STRING uStr;

    RtlInitUnicodeString(&uStr,str);
    
    errorLogEntry = (PIO_ERROR_LOG_PACKET) IoAllocateErrorLogEntry(
                                            DriverObject, // or driver object
                                            (UCHAR)(totalLength+uStr.Length));

    errorLogEntry->MajorFunctionCode=0;
    errorLogEntry->IoControlCode = 0;
    errorLogEntry->FinalStatus=0;
    errorLogEntry->RetryCount = 0;
    errorLogEntry->DumpDataSize = 0;
    if(NULL != str)
    {
        errorLogEntry->DumpDataSize = uStr.Length;
        RtlCopyMemory(errorLogEntry->DumpData,str,errorLogEntry->DumpDataSize);
        
    }
    errorLogEntry->SequenceNumber = 0;
    errorLogEntry->StringOffset=0;
    errorLogEntry->NumberOfStrings=0;
    errorLogEntry->UniqueErrorValue = uniqueId;
    errorLogEntry->ErrorCode = 0;

    IoWriteErrorLogEntry(errorLogEntry);


}

VOID
InVirVolLogError(
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
    IN ULONG maxLength4 /* = 0 */
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
    const unsigned long ulArraySize = 4;
    ULONG   ulMaxLengthArr[ulArraySize] = {maxLength1, maxLength2, maxLength3, maxLength4};
    ULONG   ulLengthArr[ulArraySize] = {0};
    PCWSTR  pStringArr[ulArraySize] = {string1, string2, string3, string4};

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
			DriverContext.NbrMissingEventLogMessages++;
            return;
        } else if (DriverContext.NbrRecentEventLogMessages == EVENTLOG_SQUELCH_MAXIMUM_EVENTS) {
            // write a message saying we are throwing away messages
            totalLength = sizeof(IO_ERROR_LOG_PACKET);
            pStringArr[0] = pStringArr[1] = pStringArr[2] = pStringArr[3] = NULL;
            nbrStrings = 0;
            messageId = INVIRVOL_ERR_TOO_MANY_EVENT_LOG_EVENTS;
            uniqueId = 0;
            finalStatus = 0;
            DriverContext.NbrRecentEventLogMessages++;
        } else {
            // process the log message normally
            DriverContext.NbrRecentEventLogMessages++;
        }
    } else {
		if (DriverContext.NbrMissingEventLogMessages) {
            uniqueId = DriverContext.NbrMissingEventLogMessages;
            DriverContext.NbrMissingEventLogMessages = 0;
		}

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
InVirVolLogError(   
    IN PDEVICE_OBJECT deviceObject,
    IN ULONG uniqueId,
    IN NTSTATUS messageId,
    IN NTSTATUS finalStatus,
    IN ULONG InsertionValue1,
    IN ULONG InsertionValue2
    )
{
    NTSTATUS        Status;
	WCHAR           BufferForInsertionValue1[20] = {0};
	WCHAR           BufferForInsertionValue2[20] = {0};

    Status = RtlStringCbPrintfExW(BufferForInsertionValue1, sizeof(BufferForInsertionValue1), NULL, NULL,
                                 STRSAFE_NULL_ON_FAILURE, L"%#x", InsertionValue1);
    ASSERT(STATUS_SUCCESS == Status);
    Status = RtlStringCbPrintfExW(BufferForInsertionValue2, sizeof(BufferForInsertionValue2), NULL, NULL,
                                 STRSAFE_NULL_ON_FAILURE, L"%#x", InsertionValue2);
    ASSERT(STATUS_SUCCESS == Status);

    InVirVolLogError(deviceObject, uniqueId, messageId, finalStatus, 
                      BufferForInsertionValue1, sizeof(BufferForInsertionValue1), BufferForInsertionValue2, sizeof(BufferForInsertionValue2));

    return;
}

VOID
InVirVolLogError(
    IN PDEVICE_OBJECT deviceObject,
    IN ULONG uniqueId,
    IN NTSTATUS messageId,
    IN NTSTATUS finalStatus,
    IN PCWSTR string1,
    IN ULONG maxLength1,
    IN PCWSTR string2,
    IN ULONG maxLength2,
    IN ULONGLONG InsertionValue3
    )
{
    NTSTATUS        Status;
    WCHAR           BufferForInsertionValue3[20] = {0};

    Status = RtlStringCbPrintfExW(BufferForInsertionValue3, sizeof(BufferForInsertionValue3), NULL, NULL,
                                 STRSAFE_NULL_ON_FAILURE, L"%#d", InsertionValue3);
    ASSERT(STATUS_SUCCESS == Status);

    InVirVolLogError(deviceObject, uniqueId, messageId, finalStatus,
        string1, maxLength1, string2, maxLength2, BufferForInsertionValue3, sizeof(BufferForInsertionValue3));

    return;
}

VOID
InVirVolLogError(
    IN PDEVICE_OBJECT deviceObject,
    IN ULONG uniqueId,
    IN NTSTATUS messageId,
    IN NTSTATUS finalStatus,
    IN PCWSTR string1,
    IN ULONG maxLength1,
    IN ULONG InsertionValue2,
    IN ULONGLONG InsertionValue3,
    IN ULONG InsertionValue4
    )
{
    NTSTATUS        Status;
    WCHAR           BufferForInsertionValue2[20] = {0};
    WCHAR           BufferForInsertionValue3[20] = {0};
    WCHAR           BufferForInsertionValue4[20]= {0};

    Status = RtlStringCbPrintfExW(BufferForInsertionValue2, sizeof(BufferForInsertionValue2), NULL, NULL,
                                 STRSAFE_NULL_ON_FAILURE, L"%#x", InsertionValue2);
    ASSERT(STATUS_SUCCESS == Status);

    Status = RtlStringCbPrintfExW(BufferForInsertionValue3, sizeof(BufferForInsertionValue3), NULL, NULL,
                                 STRSAFE_NULL_ON_FAILURE, L"%#d", InsertionValue3);
    ASSERT(STATUS_SUCCESS == Status);
    Status = RtlStringCbPrintfExW(BufferForInsertionValue4, sizeof(BufferForInsertionValue4), NULL, NULL,
                                 STRSAFE_NULL_ON_FAILURE, L"%#x", InsertionValue4);
    ASSERT(STATUS_SUCCESS == Status);

    InVirVolLogError(deviceObject, uniqueId, messageId, finalStatus,
        string1, maxLength1, BufferForInsertionValue2, sizeof(BufferForInsertionValue2),
                      BufferForInsertionValue3, sizeof(BufferForInsertionValue3), BufferForInsertionValue4, sizeof(BufferForInsertionValue4));

    return;
}

VOID
InVirVolLogError(
    IN PDEVICE_OBJECT deviceObject,
    IN ULONG uniqueId,
    IN NTSTATUS messageId,
    IN NTSTATUS finalStatus,
    IN PCWSTR string1,
    IN ULONG maxLength1,
    IN ULONGLONG InsertionValue2,
    IN ULONGLONG InsertionValue3,
    IN ULONGLONG InsertionValue4
    )
{
    NTSTATUS        Status;
    WCHAR           BufferForInsertionValue2[20] = {0};
    WCHAR           BufferForInsertionValue3[20] = {0};
    WCHAR           BufferForInsertionValue4[20]= {0};

    Status = RtlStringCbPrintfExW(BufferForInsertionValue2, sizeof(BufferForInsertionValue2), NULL, NULL,
                                 STRSAFE_NULL_ON_FAILURE, L"%#x", InsertionValue2);
    ASSERT(STATUS_SUCCESS == Status);

    Status = RtlStringCbPrintfExW(BufferForInsertionValue3, sizeof(BufferForInsertionValue3), NULL, NULL,
                                 STRSAFE_NULL_ON_FAILURE, L"%#d", InsertionValue3);
    ASSERT(STATUS_SUCCESS == Status);
    Status = RtlStringCbPrintfExW(BufferForInsertionValue4, sizeof(BufferForInsertionValue4), NULL, NULL,
                                 STRSAFE_NULL_ON_FAILURE, L"%#x", InsertionValue4);
    ASSERT(STATUS_SUCCESS == Status);

    InVirVolLogError(deviceObject, uniqueId, messageId, finalStatus,
        string1, maxLength1, BufferForInsertionValue2, sizeof(BufferForInsertionValue2),
                      BufferForInsertionValue3, sizeof(BufferForInsertionValue3), BufferForInsertionValue4, sizeof(BufferForInsertionValue4));

    return;
}