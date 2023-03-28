// This is the code which is written and may be useful later.
// Removed from main stream as not needed for now.

#define DC_FLAGS_CHANGENOTIFY_SENT	0x00000004

PDEVICE_OBJECT
OpenMountMgrDevice(VOID)
{
    NTSTATUS            Status = STATUS_SUCCESS;
    UNICODE_STRING      MountManagerName;
    PFILE_OBJECT        FileObject;
    PDEVICE_OBJECT      DeviceObject;

    RtlInitUnicodeString(&MountManagerName, MOUNTMGR_DEVICE_NAME);
    Status = IoGetDeviceObjectPointer(&MountManagerName,
                                FILE_GENERIC_READ, 
                                &FileObject, 
                                &DeviceObject);
    if (!NT_SUCCESS(Status))
        return NULL;

    ObReferenceObjectByPointer(DeviceObject, FILE_GENERIC_READ, NULL, KernelMode);
    ObDereferenceObject(FileObject);

    return DeviceObject;
}

NTSTATUS
MountMgrChangeNotifyCompletion(
	IN	PDEVICE_OBJECT	DeviceObject,
	IN	PIRP	Irp,
	IN	PVOID	Context
	)
{
	PMOUNTMGR_CHANGE_NOTIFY_INFO	pChangeNotifyInfo;
	PWORK_QUEUE_ENTRY				pWorkQueueEntry;

	UnsetDriverContextFlag(DC_FLAGS_CHANGENOTIFY_SENT);

	if (NT_SUCCESS(Irp->IoStatus.Status))
	{
		pChangeNotifyInfo = (PMOUNTMGR_CHANGE_NOTIFY_INFO)Irp->AssociatedIrp.SystemBuffer;

		DriverContext.EpicNumber = pChangeNotifyInfo->EpicNumber;

		// Queue worker routine to retreive Volume GUIDs
		pWorkQueueEntry = AllocateWorkQueueEntry();
		if (NULL != pWorkQueueEntry)
		{
			pWorkQueueEntry->Context = NULL;
			pWorkQueueEntry->WorkerRoutine = RetrieveVolumeGUIDs;

			AddItemToWorkQueue(&DriverContext.BitMapReaderQueue, pWorkQueueEntry);
		}
	}

	ExFreePool(Irp->AssociatedIrp.SystemBuffer);
	IoFreeIrp(Irp);

	return STATUS_MORE_PROCESSING_REQUIRED;
}

VOID
SendIoctlMountMgrChangeNotify()
{
	NTSTATUS			Status;
	PIRP				Irp;
	PIO_STACK_LOCATION	pIoStackLocation;
	PMOUNTMGR_CHANGE_NOTIFY_INFO	pChangeNotifyInfo;
	ULONG				ulSize;


	if (NULL == DriverContext.MountMgrDeviceObject)
		DriverContext.MountMgrDeviceObject = OpenMountMgrDevice();

	if (NULL == DriverContext.MountMgrDeviceObject)
		return;

	if (FALSE == SetDriverContextFlag(DC_FLAGS_CHANGENOTIFY_SENT))
		return;

	ulSize = sizeof(MOUNTMGR_CHANGE_NOTIFY_INFO);
	pChangeNotifyInfo = (PMOUNTMGR_CHANGE_NOTIFY_INFO)ExAllocatePoolWithTag(NonPagedPool, ulSize, TAG_GENERIC_NON_PAGED);
	if (NULL == pChangeNotifyInfo)
	{
		UnsetDriverContextFlag(DC_FLAGS_CHANGENOTIFY_SENT);
		return;
	}

	pChangeNotifyInfo->EpicNumber	= DriverContext.EpicNumber;

	Irp = IoAllocateIrp(DriverContext.MountMgrDeviceObject->StackSize, FALSE);
	if (NULL == Irp)
	{
        ExFreePoolWithTag(pChangeNotifyInfo, TAG_GENERIC_NON_PAGED);
		UnsetDriverContextFlag(DC_FLAGS_CHANGENOTIFY_SENT);
		return;
	}

	pIoStackLocation = (PIO_STACK_LOCATION)IoGetNextIrpStackLocation(Irp);

	pIoStackLocation->MajorFunction = IRP_MJ_DEVICE_CONTROL;
	pIoStackLocation->Parameters.DeviceIoControl.IoControlCode = IOCTL_MOUNTMGR_CHANGE_NOTIFY;
	pIoStackLocation->Parameters.DeviceIoControl.InputBufferLength = ulSize;
	pIoStackLocation->Parameters.DeviceIoControl.OutputBufferLength = ulSize;
	pIoStackLocation->Parameters.DeviceIoControl.Type3InputBuffer = NULL;
	Irp->AssociatedIrp.SystemBuffer = pChangeNotifyInfo;

	IoSetCompletionRoutine(Irp, MountMgrChangeNotifyCompletion, NULL, TRUE, TRUE, TRUE);

	Status = IoCallDriver(DriverContext.MountMgrDeviceObject, Irp);
	
    DebugPrint((2, "SendIoctlMountMgrChangeNotify: Irp %p Status 0x%X\n",
            Irp, Status));
	return;
}

VOID
RetrieveVolumeGUIDs(PWORK_QUEUE_ENTRY	pWorkQueueEntry)
{
	KIRQL		OldIrql, DeviceContextIrql;
	NTSTATUS	Status;
	PDEVICE_SPECIFIC_DIRTY_BLOCKS_CONTEXT	pDirtyBlocksContext;
	PLIST_ENTRY	pEntry;
	BOOLEAN		bSendChangeNotify = FALSE;
	LIST_ENTRY	BitMapContextHead;
	PBIT_MAP_CONTEXT	pBitMapContext;


	InitializeListHead(&BitMapContextHead);

	// Scan all DeviceSpecific Dirtyblocks.
	KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);

	pEntry = DriverContext.HeadForDeviceSpecificContext.Flink;
	while (pEntry != &DriverContext.HeadForDeviceSpecificContext)
	{
		pDirtyBlocksContext = (PDEVICE_SPECIFIC_DIRTY_BLOCKS_CONTEXT)pEntry;
		pEntry = pEntry->Flink;
		pBitMapContext = NULL;

		KeAcquireSpinLock(&pDirtyBlocksContext->Lock, &DeviceContextIrql);
		if (NULL == pDirtyBlocksContext->pBitMapFile)
		{

			pBitMapContext = AllocateBitMapContext();
			if (pBitMapContext)
			{
				ReferenceDirtyBlocksContext(pDirtyBlocksContext);
				pBitMapContext->pDirtyBlocksContext = pDirtyBlocksContext;
			}
		}
		KeReleaseSpinLock(&pDirtyBlocksContext->Lock, DeviceContextIrql);

		if (pBitMapContext)
			InsertTailList(&BitMapContextHead, &pBitMapContext->ListEntry);

	} // while					

	KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

	while (!IsListEmpty(&BitMapContextHead))
	{
		// go through all BitMapContexts and fire the events.
		pBitMapContext = (PBIT_MAP_CONTEXT)RemoveHeadList(&BitMapContextHead);
		pDirtyBlocksContext = pBitMapContext->pDirtyBlocksContext;
		pDirtyBlocksContext->pBitMapFile = OpenBitMapFile(pDirtyBlocksContext->FilterDeviceObject);
		if (NULL == pDirtyBlocksContext->pBitMapFile)
			bSendChangeNotify = TRUE;
		DereferenceDirtyBlocksContext(pBitMapContext->pDirtyBlocksContext);
		pBitMapContext->pDirtyBlocksContext = NULL;
		DereferenceBitMapContext(pBitMapContext);
	}

	if (bSendChangeNotify)
		SendIoctlMountMgrChangeNotify();

	return;
}

class BitmapAPI *
OpenBitMapFile(PDEVICE_OBJECT	FilterDeviceObject)
{
	class BitmapAPI	*pBitmapAPI = NULL;
	PDEVICE_EXTENSION	pDeviceExtension;
	PIRP				Irp;
	NTSTATUS			Status;
	IO_STATUS_BLOCK		IoStatus;
	KEVENT				Event;
	ULONG				OutputSize = sizeof(MOUNTDEV_NAME);
	PMOUNTDEV_NAME		pMountDevName;
	PMOUNTMGR_MOUNT_POINT	pMountPoint;
	PMOUNTMGR_MOUNT_POINTS	pMountPoints;
	PCHAR				pBuffer;
	ULONG				Iterations;
	BOOLEAN				bGUIDFound = FALSE;
    PARTITION_INFORMATION   partitionInfo;


	if (NULL == FilterDeviceObject)
		return pBitmapAPI;

	if (NULL == DriverContext.MountMgrDeviceObject)
		DriverContext.MountMgrDeviceObject = OpenMountMgrDevice();

	if (NULL == DriverContext.MountMgrDeviceObject)
		return pBitmapAPI;

	pDeviceExtension = (PDEVICE_EXTENSION)FilterDeviceObject->DeviceExtension;
	
	pMountDevName = (PMOUNTDEV_NAME)ExAllocatePoolWithTag(PagedPool, OutputSize, TAG_GENERIC_NON_PAGED);
	if (!pMountDevName) {
		return pBitmapAPI;
	}

	KeInitializeEvent(&Event, NotificationEvent, FALSE);
	Irp = IoBuildDeviceIoControlRequest(
				IOCTL_MOUNTDEV_QUERY_DEVICE_NAME,
				pDeviceExtension->TargetDeviceObject, NULL, 0,
				pMountDevName, OutputSize, FALSE, &Event, &IoStatus);
	if (!Irp) {
        ExFreePoolWithTag(pMountDevName, TAG_GENERIC_NON_PAGED);
		return pBitmapAPI;
	}

	Status = IoCallDriver(pDeviceExtension->TargetDeviceObject, Irp);
	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
		Status = IoStatus.Status;
	}

	if (Status == STATUS_BUFFER_OVERFLOW) 
	{
		OutputSize = sizeof(MOUNTDEV_NAME) + pMountDevName->NameLength;
        ExFreePoolWithTag(pMountDevName, TAG_GENERIC_NON_PAGED);
		pMountDevName = (PMOUNTDEV_NAME)ExAllocatePoolWithTag(PagedPool, OutputSize, 'ihbA');

		if (!pMountDevName) {
			return pBitmapAPI;
		}

		KeInitializeEvent(&Event, NotificationEvent, FALSE);
		Irp = IoBuildDeviceIoControlRequest(
					IOCTL_MOUNTDEV_QUERY_DEVICE_NAME,
					pDeviceExtension->TargetDeviceObject, NULL, 0,
					pMountDevName, OutputSize, FALSE, &Event, &IoStatus);
		if (!Irp) {
            ExFreePoolWithTag(pMountDevName, TAG_GENERIC_NON_PAGED);
			return pBitmapAPI;
		}

		Status = IoCallDriver(pDeviceExtension->TargetDeviceObject, Irp);
		if (Status == STATUS_PENDING) {
			KeWaitForSingleObject(
				&Event,
				Executive,
				KernelMode,
				FALSE,
				NULL
				);
			Status = IoStatus.Status;
		}
	}
	if (!NT_SUCCESS(Status)) {
        ExFreePoolWithTag(pMountDevName, TAG_GENERIC_NON_PAGED);
		return pBitmapAPI;
	}

    // Get the size of the device also
    KeInitializeEvent(&Event, NotificationEvent, FALSE);

    OutputSize = sizeof(PARTITION_INFORMATION);
	Irp = IoBuildDeviceIoControlRequest(
				IOCTL_DISK_GET_PARTITION_INFO,
				pDeviceExtension->TargetDeviceObject, NULL, 0,
				&partitionInfo, OutputSize, FALSE, &Event, &IoStatus);
	if (!Irp) {
        ExFreePoolWithTag(pMountDevName, TAG_GENERIC_NON_PAGED);
		return pBitmapAPI;
	}

	Status = IoCallDriver(pDeviceExtension->TargetDeviceObject, Irp);
	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
		Status = IoStatus.Status;
	}

    if (!NT_SUCCESS(Status)) {
        ExFreePoolWithTag(pMountDevName, TAG_GENERIC_NON_PAGED);
		return pBitmapAPI;
	}

	pBuffer = NULL;
	OutputSize = 4096; // 4K
	Iterations = 0;
	if (OutputSize < sizeof(MOUNTMGR_MOUNT_POINT) + pMountDevName->NameLength)
		OutputSize += pMountDevName->NameLength + sizeof(MOUNTMGR_MOUNT_POINT);

	do
	{
		if (pBuffer)
            ExFreePoolWithTag(pBuffer, TAG_GENERIC_NON_PAGED);

		pBuffer = (PCHAR)ExAllocatePoolWithTag(NonPagedPool, OutputSize, TAG_GENERIC_NON_PAGED);
		if (!pBuffer)
		{
            ExFreePoolWithTag(pMountDevName, TAG_GENERIC_NON_PAGED);
			return pBitmapAPI;
		}
		
		pMountPoint = (PMOUNTMGR_MOUNT_POINT)pBuffer;
		pMountPoints = (PMOUNTMGR_MOUNT_POINTS)pBuffer;
		
		RtlZeroMemory(pMountPoint, sizeof(MOUNTMGR_MOUNT_POINT));
		pMountPoint->DeviceNameLength = pMountDevName->NameLength;
		pMountPoint->DeviceNameOffset = sizeof(MOUNTMGR_MOUNT_POINT);
		RtlCopyMemory(pBuffer + sizeof(MOUNTMGR_MOUNT_POINT), pMountDevName->Name, pMountDevName->NameLength);

		KeInitializeEvent(&Event, NotificationEvent, FALSE);
		Irp = IoBuildDeviceIoControlRequest(
			IOCTL_MOUNTMGR_QUERY_POINTS,
			DriverContext.MountMgrDeviceObject, pMountPoint, sizeof(MOUNTMGR_MOUNT_POINT) + pMountDevName->NameLength, 
			pMountPoints, OutputSize, FALSE, &Event, &IoStatus);

		Status = IoCallDriver(DriverContext.MountMgrDeviceObject, Irp);
		if (Status == STATUS_PENDING) {
			KeWaitForSingleObject(
				&Event,
				Executive,
				KernelMode,
				FALSE,
				NULL
				);
			Status = IoStatus.Status;
		}
		Iterations++;
	} while ((Iterations < 5) && (STATUS_BUFFER_OVERFLOW == Status));

	// Now get the GUID of the name.
	if (NT_SUCCESS(Status))
	{
		Iterations = 0;
		while (Iterations < pMountPoints->NumberOfMountPoints)
		{
			PCHAR	pSymbolicLink;
			ULONG	ulLen;

			if (pMountPoints->MountPoints[Iterations].SymbolicLinkNameLength > 
					(sizeof(WCHAR) * (UNIQUE_GUID_FOR_VOLUME_PREFIX_LENGTH + GUID_SIZE_IN_CHARS)) )
			{
				pSymbolicLink = pBuffer + pMountPoints->MountPoints[Iterations].SymbolicLinkNameOffset;
				if (memcmp(UNIQUE_GUID_FOR_VOLUME_PREFIX, pSymbolicLink, sizeof(WCHAR) * UNIQUE_GUID_FOR_VOLUME_PREFIX_LENGTH) == 0)
				{
					memcpy(pDeviceExtension->pDirtyBlocksContext->GUID, 
							pSymbolicLink + (sizeof(WCHAR) * UNIQUE_GUID_FOR_VOLUME_PREFIX_LENGTH), 
							sizeof(WCHAR) * GUID_SIZE_IN_CHARS);
					bGUIDFound = TRUE;
					break;
				}
			}
			Iterations++;
		}
	}
	
    ExFreePoolWithTag(pMountDevName, TAG_GENERIC_NON_PAGED);
    ExFreePoolWithTag(pBuffer, TAG_GENERIC_NON_PAGED);

	if (bGUIDFound)
	{
		pBitmapAPI = new BitmapAPI();
        pBitmapAPI->Open((WCHAR *)pDeviceExtension->pDirtyBlocksContext->GUID, partitionInfo.PartitionLength.QuadPart);
	}

	return pBitmapAPI;
}

NTSTATUS
InMageFltRegisterDevice(
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    Routine to initialize a proper name for the device object, and
    register it with WMI

Arguments:

    DeviceObject - pointer to a device object to be initialized.

Return Value:

    Status of the initialization. NOTE: If the registration fails,
    the device name in the DeviceExtension will be left as empty.

--*/

{
    NTSTATUS                status;
    IO_STATUS_BLOCK         ioStatus;
    KEVENT                  event;
    PDEVICE_EXTENSION       pDeviceExtension;
    PIRP                    irp;
    STORAGE_DEVICE_NUMBER   number;
    ULONG                   registrationFlag = 0;
    WCHAR                   ntNameBuffer[ABHAIFLT_MAXSTR];
    STRING                  ntNameString;
    UNICODE_STRING          ntUnicodeString;

    DebugPrint((2, "InMageFltRegisterDevice: DeviceObject %X\n",
                    DeviceObject));
    pDeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    //
    // Request for the device number
    //
    irp = IoBuildDeviceIoControlRequest(
                    IOCTL_STORAGE_GET_DEVICE_NUMBER,
                    pDeviceExtension->TargetDeviceObject,
                    NULL,
                    0,
                    &number,
                    sizeof(number),
                    FALSE,
                    &event,
                    &ioStatus);
    if (!irp) {
        InMageFltLogError(
            DeviceObject,
            256,
            STATUS_SUCCESS,
            IO_ERR_INSUFFICIENT_RESOURCES);
        DebugPrint((3, "InMageFltRegisterDevice: Fail to build irp\n"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    status = IoCallDriver(pDeviceExtension->TargetDeviceObject, irp);
    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
        status = ioStatus.Status;
    }

    if (NT_SUCCESS(status)) {

        //
        // Remember the disk number for use as parameter in DiskIoNotifyRoutine
        //
        pDeviceExtension->DiskNumber = number.DeviceNumber;

        //
        // Create device name for each partition
        //

        swprintf(
            pDeviceExtension->PhysicalDeviceNameBuffer,
            L"\\Device\\Harddisk%d\\Partition%d",
            number.DeviceNumber, number.PartitionNumber);
        RtlInitUnicodeString(
            &pDeviceExtension->PhysicalDeviceName,
            &pDeviceExtension->PhysicalDeviceNameBuffer[0]);

        //
        // Set default name for physical disk
        //
        RtlCopyMemory(
            &(pDeviceExtension->StorageManagerName[0]),
            L"PhysDisk",
            8 * sizeof(WCHAR));
        DebugPrint((3, "InMageFltRegisterDevice: Device name %ws\n",
                       pDeviceExtension->PhysicalDeviceNameBuffer));
    }
    else {

        // request for partition's information failed, try volume

        ULONG           outputSize = sizeof(MOUNTDEV_NAME);
        PMOUNTDEV_NAME  output;
        VOLUME_NUMBER   volumeNumber;

        output = (PMOUNTDEV_NAME)ExAllocatePoolWithTag(PagedPool, outputSize, 'ihbA');
        if (!output) {
            InMageFltLogError(
                DeviceObject,
                257,
                STATUS_SUCCESS,
                IO_ERR_INSUFFICIENT_RESOURCES);
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        KeInitializeEvent(&event, NotificationEvent, FALSE);
        irp = IoBuildDeviceIoControlRequest(
                    IOCTL_MOUNTDEV_QUERY_DEVICE_NAME,
                    pDeviceExtension->TargetDeviceObject, NULL, 0,
                    output, outputSize, FALSE, &event, &ioStatus);
        if (!irp) {
            ExFreePoolWithTag(output, 'ihbA');
            InMageFltLogError(
                DeviceObject,
                258,
                STATUS_SUCCESS,
                IO_ERR_INSUFFICIENT_RESOURCES);
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        status = IoCallDriver(pDeviceExtension->TargetDeviceObject, irp);
        if (status == STATUS_PENDING) {
            KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
            status = ioStatus.Status;
        }

        if (status == STATUS_BUFFER_OVERFLOW) {
            outputSize = sizeof(MOUNTDEV_NAME) + output->NameLength;
            ExFreePoolWithTag(output, 'ihbA');
            output = (PMOUNTDEV_NAME)ExAllocatePoolWithTag(PagedPool, outputSize, 'ihbA');

            if (!output) {
                InMageFltLogError(
                    DeviceObject,
                    258,
                    STATUS_SUCCESS,
                    IO_ERR_INSUFFICIENT_RESOURCES);
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            KeInitializeEvent(&event, NotificationEvent, FALSE);
            irp = IoBuildDeviceIoControlRequest(
                        IOCTL_MOUNTDEV_QUERY_DEVICE_NAME,
                        pDeviceExtension->TargetDeviceObject, NULL, 0,
                        output, outputSize, FALSE, &event, &ioStatus);
            if (!irp) {
                ExFreePoolWithTag(output, 'ihbA');
                InMageFltLogError(
                    DeviceObject, 259,
                    STATUS_SUCCESS,
                    IO_ERR_INSUFFICIENT_RESOURCES);
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            status = IoCallDriver(pDeviceExtension->TargetDeviceObject, irp);
            if (status == STATUS_PENDING) {
                KeWaitForSingleObject(
                    &event,
                    Executive,
                    KernelMode,
                    FALSE,
                    NULL
                    );
                status = ioStatus.Status;
            }
        }
        if (!NT_SUCCESS(status)) {
            ExFreePoolWithTag(output, 'ihbA');
            InMageFltLogError(
                DeviceObject,
                260,
                STATUS_SUCCESS,
                IO_ERR_CONFIGURATION_ERROR);
            return status;
        }

        //
        // Since we get the volume name instead of the disk number,
        // set it to a dummy value
        // Todo: Instead of passing the disk number back to the user app.
        // for tracing, pass the STORAGE_DEVICE_NUMBER structure instead.

        pDeviceExtension->DiskNumber = -1;

        pDeviceExtension->PhysicalDeviceName.Length = output->NameLength;
        pDeviceExtension->PhysicalDeviceName.MaximumLength
                = output->NameLength + sizeof(WCHAR);

        RtlCopyMemory(
            pDeviceExtension->PhysicalDeviceName.Buffer,
            output->Name,
            output->NameLength);
        pDeviceExtension->PhysicalDeviceName.Buffer
            [pDeviceExtension->PhysicalDeviceName.Length/sizeof(WCHAR)] = 0;
        ExFreePoolWithTag(output, 'ihbA');

        //
        // Now, get the VOLUME_NUMBER information
        //
        outputSize = sizeof(VOLUME_NUMBER);
        RtlZeroMemory(&volumeNumber, sizeof(VOLUME_NUMBER));

        KeInitializeEvent(&event, NotificationEvent, FALSE);
        irp = IoBuildDeviceIoControlRequest(
                 IOCTL_VOLUME_QUERY_VOLUME_NUMBER,
                 pDeviceExtension->TargetDeviceObject, NULL, 0,
                 &volumeNumber,
                 sizeof(VOLUME_NUMBER),
                 FALSE, &event, &ioStatus);
        if (!irp) {
            InMageFltLogError(
                DeviceObject,
                265,
                STATUS_SUCCESS,
                IO_ERR_INSUFFICIENT_RESOURCES);
            return STATUS_INSUFFICIENT_RESOURCES;
        }
        status = IoCallDriver(pDeviceExtension->TargetDeviceObject, irp);
        if (status == STATUS_PENDING) {
            KeWaitForSingleObject(&event, Executive,
                                  KernelMode, FALSE, NULL);
            status = ioStatus.Status;
        }
        if (!NT_SUCCESS(status) ||
            volumeNumber.VolumeManagerName[0] == (WCHAR) UNICODE_NULL) {

            RtlCopyMemory(
                &pDeviceExtension->StorageManagerName[0],
                L"LogiDisk",
                8 * sizeof(WCHAR));
            if (NT_SUCCESS(status))
                pDeviceExtension->DiskNumber = volumeNumber.VolumeNumber;
        }
        else {
            RtlCopyMemory(
                &pDeviceExtension->StorageManagerName[0],
                &volumeNumber.VolumeManagerName[0],
                8 * sizeof(WCHAR));
            pDeviceExtension->DiskNumber = volumeNumber.VolumeNumber;
        }
        DebugPrint((3, "InMageFltRegisterDevice: Device name %ws\n",
                       pDeviceExtension->PhysicalDeviceNameBuffer));
    }

    //
    // Do not register with WMI
    //
    return status;
}


