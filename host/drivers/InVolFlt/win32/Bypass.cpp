//
// Copyright InMage Systems 2005
//

// As our driver is a boot driver, this file implements bypass functions so the driver can
// still be loaded and be assured it will not break
//
//

#include "Common.h"
#include "Global.h"

typedef struct _BYPASS_DEVICE_EXTENSION
{
    PDEVICE_OBJECT  Self;
    PDEVICE_OBJECT  NextLowerDriver;
    DEVICE_PNP_STATE  DevicePnPState;
    DEVICE_PNP_STATE    PreviousPnPState;
    IO_REMOVE_LOCK RemoveLock; 
} BYPASS_DEVICE_EXTENSION, *PBYPASS_DEVICE_EXTENSION;

// This function checks to see if the boot.ini line used to start things contains 'INVOLFLT=BYPASS'
NTSTATUS
BypassCheckBootIni()
{
    TRC
    NTSTATUS        Status;
    Registry        reg;
    UNICODE_STRING  string;
    UNICODE_STRING  ucString;
    UNICODE_STRING  pattern;
    PWCHAR          pwStartOptions = NULL;
    int             i;

    string.Buffer = NULL;
    string.Length = string.MaximumLength = 0;
    ucString.Buffer = NULL;
    ucString.Length = ucString.MaximumLength = 0;

    Status = reg.OpenKeyReadOnly(L"\\Registry\\Machine\\System\\CurrentControlSet\\Control");

    if (NT_SUCCESS(Status)) {
        Status = reg.ReadWString(L"SystemStartOptions", STRING_LEN_NULL_TERMINATED, &pwStartOptions);
        reg.CloseKey();

        if (NT_SUCCESS(Status)) {
            RtlInitUnicodeString(&string, pwStartOptions);
            InVolDbgPrint(DL_INFO, DM_DRIVER_INIT, ("InVolFlt!BypassCheckBootIni Startup string = %wZ\n", &string));

            RtlInitUnicodeString(&pattern, L"INVOLFLT=BYPASS");
            RtlUpcaseUnicodeString(&ucString, &string, TRUE);
            Status = STATUS_UNSUCCESSFUL;

            for(i = 0; i <= ((int)(ucString.Length/sizeof(WCHAR)) - 15); i++) {
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
                        Status = STATUS_SUCCESS;
                        break;
                    } else {
                        Status = STATUS_UNSUCCESSFUL;
                    }
            }
        } else {
            Status = STATUS_UNSUCCESSFUL;
        }
    }

    if (NULL != pwStartOptions) {
        ExFreePoolWithTag(pwStartOptions, TAG_REGISTRY_DATA);
    }

    if (NULL != ucString.Buffer) {
        RtlFreeUnicodeString(&ucString);
    }

    return Status;
}

NTSTATUS
BypassAddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PhysicalDeviceObject
    )
{
    NTSTATUS                Status = STATUS_SUCCESS;
    PDEVICE_OBJECT          deviceObject = NULL;
    PBYPASS_DEVICE_EXTENSION       deviceExtension;
    ULONG                   deviceType = FILE_DEVICE_UNKNOWN;

    if (!IoIsWdmVersionAvailable(1, 0x20)) {
        deviceObject = IoGetAttachedDeviceReference(PhysicalDeviceObject);
        deviceType = deviceObject->DeviceType;
        ObDereferenceObject(deviceObject);
    }

    Status = IoCreateDevice (DriverObject,
                             sizeof (BYPASS_DEVICE_EXTENSION),
                             NULL,  // No Name
                             deviceType,
                             FILE_DEVICE_SECURE_OPEN,
                             FALSE,
                             &deviceObject);


    if (!NT_SUCCESS (Status)) {
        return Status;
    }

    deviceExtension = (PBYPASS_DEVICE_EXTENSION) deviceObject->DeviceExtension;

    deviceExtension->NextLowerDriver = IoAttachDeviceToDeviceStack (
                                       deviceObject,
                                       PhysicalDeviceObject);

    if(NULL == deviceExtension->NextLowerDriver) {

        IoDeleteDevice(deviceObject);
        return STATUS_UNSUCCESSFUL;
    }

    deviceObject->Flags |= deviceExtension->NextLowerDriver->Flags &
                            (DO_BUFFERED_IO | DO_DIRECT_IO |
                            DO_POWER_PAGABLE );


    deviceObject->DeviceType = deviceExtension->NextLowerDriver->DeviceType;

    deviceObject->Characteristics =
                          deviceExtension->NextLowerDriver->Characteristics;

    deviceExtension->Self = deviceObject;

    IoInitializeRemoveLock (&deviceExtension->RemoveLock , 
                            TAG_GENERIC_NON_PAGED,
                            1, // MaxLockedMinutes 
                            100);                                 

    INITIALIZE_PNP_STATE(deviceExtension);

    deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    return STATUS_SUCCESS;
}


VOID
BypassPrintMajorAndMinorFunction(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp
	)
{
	PIO_STACK_LOCATION	IoStackLocation = IoGetCurrentIrpStackLocation(Irp);

	switch (IoStackLocation->MajorFunction) {
		case  IRP_MJ_CREATE :
			InVolDbgPrint(DL_INFO, DM_BYPASS, ("InVolFlt:Bypass: MajorFunction - IRP_MJ_CREATE\n"));
			break;
		case  IRP_MJ_CREATE_NAMED_PIPE :
			InVolDbgPrint(DL_INFO, DM_BYPASS, ("InVolFlt:Bypass: MajorFunction - IRP_MJ_CREATE_NAMED_PIPE\n"));
			break;
		case  IRP_MJ_CLOSE :
			InVolDbgPrint(DL_INFO, DM_BYPASS, ("InVolFlt:Bypass: MajorFunction - IRP_MJ_CLOSE\n"));
			break;
		case  IRP_MJ_READ :
			InVolDbgPrint(DL_INFO, DM_BYPASS, ("InVolFlt:Bypass: MajorFunction - IRP_MJ_READ\n"));
			break;
		case  IRP_MJ_WRITE :
			InVolDbgPrint(DL_INFO, DM_BYPASS, ("InVolFlt:Bypass: MajorFunction - IRP_MJ_WRITE\n"));
			break;
		case  IRP_MJ_QUERY_INFORMATION :
			InVolDbgPrint(DL_INFO, DM_BYPASS, ("InVolFlt:Bypass: MajorFunction - IRP_MJ_QUERY_INFORMATION\n"));
			break;
		case  IRP_MJ_SET_INFORMATION :
			InVolDbgPrint(DL_INFO, DM_BYPASS, ("InVolFlt:Bypass: MajorFunction - IRP_MJ_SET_INFORMATION\n"));
			break;
		case  IRP_MJ_QUERY_EA :
			InVolDbgPrint(DL_INFO, DM_BYPASS, ("InVolFlt:Bypass: MajorFunction - IRP_MJ_QUERY_EA\n"));
			break;
		case  IRP_MJ_SET_EA :
			InVolDbgPrint(DL_INFO, DM_BYPASS, ("InVolFlt:Bypass: MajorFunction - IRP_MJ_SET_EA\n"));
			break;
		case  IRP_MJ_FLUSH_BUFFERS :
			InVolDbgPrint(DL_INFO, DM_BYPASS, ("InVolFlt:Bypass: MajorFunction - IRP_MJ_FLUSH_BUFFERS\n"));
			break;
		case  IRP_MJ_QUERY_VOLUME_INFORMATION :
			InVolDbgPrint(DL_INFO, DM_BYPASS, ("InVolFlt:Bypass: MajorFunction - IRP_MJ_QUERY_VOLUME_INFORMATION\n"));
			break;
		case  IRP_MJ_SET_VOLUME_INFORMATION :
			InVolDbgPrint(DL_INFO, DM_BYPASS, ("InVolFlt:Bypass: MajorFunction - IRP_MJ_SET_VOLUME_INFORMATION\n"));
			break;
		case  IRP_MJ_DIRECTORY_CONTROL :
			InVolDbgPrint(DL_INFO, DM_BYPASS, ("InVolFlt:Bypass: MajorFunction - IRP_MJ_DIRECTORY_CONTROL\n"));
			break;
		case  IRP_MJ_FILE_SYSTEM_CONTROL :
			InVolDbgPrint(DL_INFO, DM_BYPASS, ("InVolFlt:Bypass: MajorFunction - IRP_MJ_FILE_SYSTEM_CONTROL\n"));
			break;
		case  IRP_MJ_DEVICE_CONTROL :
			InVolDbgPrint(DL_INFO, DM_BYPASS, ("InVolFlt:Bypass: MajorFunction - IRP_MJ_DEVICE_CONTROL\n"));
			break;
		case  IRP_MJ_INTERNAL_DEVICE_CONTROL :
			InVolDbgPrint(DL_INFO, DM_BYPASS, ("InVolFlt:Bypass: MajorFunction - IRP_MJ_INTERNAL_DEVICE_CONTROL\n"));
			break;
		case  IRP_MJ_SHUTDOWN :
			InVolDbgPrint(DL_INFO, DM_BYPASS, ("InVolFlt:Bypass: MajorFunction - IRP_MJ_SHUTDOWN\n"));
			break;
		case  IRP_MJ_LOCK_CONTROL :
			InVolDbgPrint(DL_INFO, DM_BYPASS, ("InVolFlt:Bypass: MajorFunction - IRP_MJ_LOCK_CONTROL\n"));
			break;
		case  IRP_MJ_CLEANUP :
			InVolDbgPrint(DL_INFO, DM_BYPASS, ("InVolFlt:Bypass: MajorFunction - IRP_MJ_CLEANUP\n"));
			break;
		case  IRP_MJ_CREATE_MAILSLOT :
			InVolDbgPrint(DL_INFO, DM_BYPASS, ("InVolFlt:Bypass: MajorFunction - IRP_MJ_CREATE_MAILSLOT\n"));
			break;
		case  IRP_MJ_QUERY_SECURITY :
			InVolDbgPrint(DL_INFO, DM_BYPASS, ("InVolFlt:Bypass: MajorFunction - IRP_MJ_QUERY_SECURITY\n"));
			break;
		case  IRP_MJ_SET_SECURITY :
			InVolDbgPrint(DL_INFO, DM_BYPASS, ("InVolFlt:Bypass: MajorFunction - IRP_MJ_SET_SECURITY\n"));
			break;
		case  IRP_MJ_POWER :
			InVolDbgPrint(DL_INFO, DM_BYPASS, ("InVolFlt:Bypass: MajorFunction - IRP_MJ_POWER\n"));
			break;
		case  IRP_MJ_SYSTEM_CONTROL :
			InVolDbgPrint(DL_INFO, DM_BYPASS, ("InVolFlt:Bypass: MajorFunction - IRP_MJ_SYSTEM_CONTROL\n"));
			break;
		case  IRP_MJ_DEVICE_CHANGE :
			InVolDbgPrint(DL_INFO, DM_BYPASS, ("InVolFlt:Bypass: MajorFunction - IRP_MJ_DEVICE_CHANGE\n"));
			break;
		case  IRP_MJ_QUERY_QUOTA :
			InVolDbgPrint(DL_INFO, DM_BYPASS, ("InVolFlt:Bypass: MajorFunction - IRP_MJ_QUERY_QUOTA\n"));
			break;
		case  IRP_MJ_SET_QUOTA :
			InVolDbgPrint(DL_INFO, DM_BYPASS, ("InVolFlt:Bypass: MajorFunction - IRP_MJ_SET_QUOTA\n"));
			break;
		case  IRP_MJ_PNP :
			InVolDbgPrint(DL_INFO, DM_BYPASS, ("InVolFlt:Bypass: MajorFunction - IRP_MJ_PNP\n"));
			break;
		default:
			InVolDbgPrint(DL_INFO, DM_BYPASS, ("InVolFlt:Bypass: Unknown MajorFunction - %d\n", IoStackLocation->MajorFunction));
	}

	return;
}

NTSTATUS
BypassPass (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    PBYPASS_DEVICE_EXTENSION           deviceExtension;
    NTSTATUS    Status;
    
    deviceExtension = (PBYPASS_DEVICE_EXTENSION) DeviceObject->DeviceExtension;
    Status = IoAcquireRemoveLock (&deviceExtension->RemoveLock, Irp);
    if (!NT_SUCCESS (Status)) {
		InVolDbgPrint(DL_ERROR, DM_BYPASS, ("InVolFlt:BypassPass:IoAcquireRemoveLock failed\n"));
        Irp->IoStatus.Status = Status;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        return Status;
    }

	BypassPrintMajorAndMinorFunction(DeviceObject, Irp);
	
   IoSkipCurrentIrpStackLocation (Irp);
   Status = IoCallDriver (deviceExtension->NextLowerDriver, Irp);
   IoReleaseRemoveLock(&deviceExtension->RemoveLock, Irp); 
   return Status;
}

NTSTATUS
BypassDispatchPnp (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    PBYPASS_DEVICE_EXTENSION           deviceExtension;
    PIO_STACK_LOCATION         irpStack;
    NTSTATUS                            Status;
    KEVENT                               event;

    PAGED_CODE();

    deviceExtension = (PBYPASS_DEVICE_EXTENSION) DeviceObject->DeviceExtension;
    irpStack = IoGetCurrentIrpStackLocation(Irp);

   Status = IoAcquireRemoveLock (&deviceExtension->RemoveLock, Irp);
    if (!NT_SUCCESS (Status)) {
        Irp->IoStatus.Status = Status;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        return Status;
    }
    

    switch (irpStack->MinorFunction) {
    case IRP_MN_START_DEVICE:

        KeInitializeEvent(&event, NotificationEvent, FALSE);
        IoCopyCurrentIrpStackLocationToNext(Irp);
        IoSetCompletionRoutine(Irp,
                               (PIO_COMPLETION_ROUTINE) BypassStartCompletionRoutine,
                               &event,
                               TRUE,
                               TRUE,
                               TRUE);

        Status = IoCallDriver(deviceExtension->NextLowerDriver, Irp);
        
        if (Status == STATUS_PENDING) {

           KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);          
           Status = Irp->IoStatus.Status;
        }

        if (NT_SUCCESS (Status)) {

            SET_NEW_PNP_STATE(deviceExtension, Started);

            if (deviceExtension->NextLowerDriver->Characteristics & FILE_REMOVABLE_MEDIA) {

                DeviceObject->Characteristics |= FILE_REMOVABLE_MEDIA;
            }

        }
        
        Irp->IoStatus.Status = Status;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        IoReleaseRemoveLock(&deviceExtension->RemoveLock, Irp); 
        return Status;

    case IRP_MN_REMOVE_DEVICE:

        IoReleaseRemoveLockAndWait(&deviceExtension->RemoveLock, Irp);
        IoSkipCurrentIrpStackLocation(Irp);

        Status = IoCallDriver(deviceExtension->NextLowerDriver, Irp);

        SET_NEW_PNP_STATE(deviceExtension, Deleted);
        
        IoDetachDevice(deviceExtension->NextLowerDriver);
        IoDeleteDevice(DeviceObject);
        return Status;


    case IRP_MN_QUERY_STOP_DEVICE:
        SET_NEW_PNP_STATE(deviceExtension, StopPending);
        Status = STATUS_SUCCESS;
        break;

    case IRP_MN_CANCEL_STOP_DEVICE:

        if(StopPending == deviceExtension->DevicePnPState)
        {
            RESTORE_PREVIOUS_PNP_STATE(deviceExtension);
        }
        Status = STATUS_SUCCESS; // We must not fail this IRP.
        break;

    case IRP_MN_STOP_DEVICE:
        SET_NEW_PNP_STATE(deviceExtension, Stopped);
        Status = STATUS_SUCCESS;
        break;

    case IRP_MN_QUERY_REMOVE_DEVICE:

        SET_NEW_PNP_STATE(deviceExtension, RemovePending);
        Status = STATUS_SUCCESS;
        break;

    case IRP_MN_SURPRISE_REMOVAL:

        SET_NEW_PNP_STATE(deviceExtension, SurpriseRemovePending);
        Status = STATUS_SUCCESS;
        break;

    case IRP_MN_CANCEL_REMOVE_DEVICE:

        if(RemovePending == deviceExtension->DevicePnPState)
        {
            RESTORE_PREVIOUS_PNP_STATE(deviceExtension);
        }

        Status = STATUS_SUCCESS; // We must not fail this IRP.
        break;

    case IRP_MN_DEVICE_USAGE_NOTIFICATION:

        if ((DeviceObject->AttachedDevice == NULL) ||
            (DeviceObject->AttachedDevice->Flags & DO_POWER_PAGABLE)) {

            DeviceObject->Flags |= DO_POWER_PAGABLE;
        }

        IoCopyCurrentIrpStackLocationToNext(Irp);

        IoSetCompletionRoutine(
            Irp,
            BypassDeviceUsageNotificationCompletionRoutine,
            NULL,
            TRUE,
            TRUE,
            TRUE
            );

        return IoCallDriver(deviceExtension->NextLowerDriver, Irp);

    default:
        Status = Irp->IoStatus.Status;

        break;
    }

    Irp->IoStatus.Status = Status;
    IoSkipCurrentIrpStackLocation (Irp);
    Status = IoCallDriver (deviceExtension->NextLowerDriver, Irp);
    IoReleaseRemoveLock(&deviceExtension->RemoveLock, Irp); 
    return Status;
}

NTSTATUS
BypassStartCompletionRoutine(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN PVOID            Context
    )
{
    PKEVENT             event = (PKEVENT)Context;

    UNREFERENCED_PARAMETER (DeviceObject);

    if (Irp->PendingReturned == TRUE) {
        KeSetEvent (event, IO_NO_INCREMENT, FALSE);
    }

    return STATUS_MORE_PROCESSING_REQUIRED;

}

NTSTATUS
BypassDeviceUsageNotificationCompletionRoutine(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN PVOID            Context
    )
{
    PBYPASS_DEVICE_EXTENSION       deviceExtension;

    UNREFERENCED_PARAMETER(Context);

    deviceExtension = (PBYPASS_DEVICE_EXTENSION) DeviceObject->DeviceExtension;


    if (Irp->PendingReturned) {

        IoMarkIrpPending(Irp);
    }

    if (!(deviceExtension->NextLowerDriver->Flags & DO_POWER_PAGABLE)) {

        DeviceObject->Flags &= ~DO_POWER_PAGABLE;
    }

    IoReleaseRemoveLock(&deviceExtension->RemoveLock, Irp); 

    return STATUS_CONTINUE_COMPLETION;
}

NTSTATUS
BypassDispatchPower(
    IN PDEVICE_OBJECT    DeviceObject,
    IN PIRP              Irp
    )
{
    PBYPASS_DEVICE_EXTENSION   deviceExtension;
    NTSTATUS    Status;
    
    deviceExtension = (PBYPASS_DEVICE_EXTENSION) DeviceObject->DeviceExtension;
    Status = IoAcquireRemoveLock (&deviceExtension->RemoveLock, Irp);
    if (!NT_SUCCESS (Status)) { // may be device is being removed.
        Irp->IoStatus.Status = Status;
        PoStartNextPowerIrp(Irp);
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        return Status;
    }

    PoStartNextPowerIrp(Irp);
    IoSkipCurrentIrpStackLocation(Irp);
    Status = PoCallDriver(deviceExtension->NextLowerDriver, Irp);
    IoReleaseRemoveLock(&deviceExtension->RemoveLock, Irp); 
    return Status;
}

VOID
BypassUnload(
    IN PDRIVER_OBJECT DriverObject
    )
{
    return;
}

