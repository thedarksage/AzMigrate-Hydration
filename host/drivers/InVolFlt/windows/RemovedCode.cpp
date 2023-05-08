
//
// Copyright InMage Systems 2004
//
//
/*
//VOLUME_CLUSTER_SUPPORT
BOOLEAN
IsBitmapFileOnSameVolume(PDEV_CONTEXT pDevContext)
{
    UNICODE_STRING  usDosDevices;
    WCHAR           wcDriveLetter;

    if ((MINIMUM_PATH_LENGTH_IN_DOSDEVICES_PATH * sizeof(WCHAR)) > pDevContext->BitmapFileName.Length)
        return FALSE;

    RtlInitUnicodeString(&usDosDevices, DOS_DEVICES_PATH);
    // If bitmap file is \SystemRoot we will return false.
    // SystemRoot can never be a cluster drive. So returning false if bitmap file
    // is on system driver is good.

    if (FALSE == RtlPrefixUnicodeString(&usDosDevices, &pDevContext->BitmapFileName, TRUE)) {
        // Volume name does not start with drive letter.
        return FALSE;
    }

    wcDriveLetter = pDevContext->BitmapFileName.Buffer[DRIVE_LETTER_INDEX_IN_DOSDEVICES_PATH];

    if (FALSE == IS_DRIVE_LETTER(wcDriveLetter))
        return FALSE;

    if (L':' != pDevContext->BitmapFileName.Buffer[DRIVE_LETTER_INDEX_IN_DOSDEVICES_PATH + 1])
        return FALSE;

    if (wcDriveLetter != pDevContext->wDevNum[0])
        return FALSE;

    return TRUE;
}
#endif
*/
	/*
	Registry *
	OpenDeviceParametersRegKey(PWCHAR pVolumeGUID)
	{
		Registry		*pVolumeParam;
		UNICODE_STRING	uszVolumeParameters;
		NTSTATUS		Status;
	
		pVolumeParam = new Registry();
		if (NULL == pVolumeParam)
			return NULL;
	
		uszVolumeParameters.Length = 0;
		uszVolumeParameters.MaximumLength = DriverContext.DriverParameters.Length +
											VOLUME_NAME_SIZE_IN_BYTES + sizeof(WCHAR);
		uszVolumeParameters.Buffer = (PWSTR)ExAllocatePoolWithTag(PagedPool, uszVolumeParameters.MaximumLength, TAG_GENERIC_PAGED);
		if (NULL == uszVolumeParameters.Buffer) {
			delete pVolumeParam;
			return NULL;
		}
	
		RtlAppendUnicodeStringToString(&uszVolumeParameters, &DriverContext.DriverParameters);
		RtlAppendUnicodeToString(&uszVolumeParameters, L"\\Volume{");
		RtlAppendUnicodeToString(&uszVolumeParameters, pVolumeGUID);
		RtlAppendUnicodeToString(&uszVolumeParameters, L"}");
	
		Status = pVolumeParam->OpenKeyReadWrite(&uszVolumeParameters);
		if (!NT_SUCCESS(Status)) {
			delete pVolumeParam;
			pVolumeParam = NULL;
		}
	
		ExFreePoolWithTag(uszVolumeParameters.Buffer, TAG_GENERIC_PAGED);
		return pVolumeParam;
	}
	
	NTSTATUS
	GetLogFilenameForVolume(PWCHAR VolumeGUID, PUNICODE_STRING puszLogFileName)
	{
	TRC
		NTSTATUS		Status;
		Registry		reg;
		UNICODE_STRING	uszOverrideFilename;
		Registry		*pVolumeParam = NULL;
	
		InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, ("GetLogFilenameForVolume: For GUID %S\n", VolumeGUID));
	
		if (NULL == puszLogFileName)
			return STATUS_INVALID_PARAMETER;
	
		if ((NULL == puszLogFileName->Buffer) || (0 == puszLogFileName->MaximumLength))
			return STATUS_INVALID_PARAMETER;
	
		puszLogFileName->Length = 0;
	
		// build a default log filename
		RtlAppendUnicodeToString(puszLogFileName, SYSTEM_ROOT_PATH);
		RtlAppendUnicodeToString(puszLogFileName, LOG_FILE_NAME_PREFIX); // so things sort by name nice
		RtlAppendUnicodeToString(puszLogFileName, VolumeGUID);
		RtlAppendUnicodeToString(puszLogFileName, LOG_FILE_NAME_SUFFIX);
	
		// now see if the registry has values to use
		uszOverrideFilename.Buffer = new WCHAR[MAX_LOG_PATHNAME];
		InVolDbgPrint(DL_INFO, DM_MEM_TRACE, ("BitmapAPI::FindLogFilename Allocation %p\n", uszOverrideFilename.Buffer));
		if (NULL == uszOverrideFilename.Buffer) {
			return STATUS_NO_MEMORY;
		}
		uszOverrideFilename.Length = 0;
		uszOverrideFilename.MaximumLength = MAX_LOG_PATHNAME*sizeof(WCHAR);
	
		Status = reg.OpenKeyReadOnly(&DriverContext.DriverParameters);
		if (NT_SUCCESS(Status)) {
			PWSTR  pDefaultLogDirectory = NULL;
	
			Status = reg.ReadWString(DEFALUT_LOG_DIRECTORY, STRING_LEN_NULL_TERMINATED, &pDefaultLogDirectory);
			if (NT_SUCCESS(Status)) {
				puszLogFileName->Length = 0;
				RtlAppendUnicodeToString(puszLogFileName, DOS_DEVICES_PATH);
				RtlAppendUnicodeToString(puszLogFileName, pDefaultLogDirectory);
				RtlAppendUnicodeToString(puszLogFileName, L"\\");
				RtlAppendUnicodeToString(puszLogFileName, LOG_FILE_NAME_PREFIX); // so things sort by name nice
				RtlAppendUnicodeToString(puszLogFileName, VolumeGUID);
				RtlAppendUnicodeToString(puszLogFileName, LOG_FILE_NAME_SUFFIX);
			}
	
			if (NULL != pDefaultLogDirectory)
				ExFreePoolWithTag(pDefaultLogDirectory, TAG_REGISTRY_DATA);
		}
	
		reg.CloseKey();
	
		// check for override per volume
		pVolumeParam = OpenDeviceParametersRegKey(VolumeGUID);
		if (NULL != pVolumeParam) {
			Status = pVolumeParam->ReadUnicodeString(VOLUME_LOG_PATH_NAME, &uszOverrideFilename);
			if (NT_SUCCESS(Status)) {
				if (uszOverrideFilename.Length > 0) {
					puszLogFileName->Length = 0;
					RtlAppendUnicodeToString(puszLogFileName, DOS_DEVICES_PATH);
					RtlAppendUnicodeStringToString(puszLogFileName, &uszOverrideFilename);
				}
			} else {
				// Default LogPathname not present. See if this is a volume on clusdisk.
				PBASIC_VOLUME	pBasicVolume = NULL;
	
				pBasicVolume = GetBasicVolumeFromDriverContext(VolumeGUID);
				if (NULL != pBasicVolume) {
					ASSERT(NULL != pBasicVolume->pDisk);
	
					if ((NULL != pBasicVolume->pDisk) && 
						(pBasicVolume->pDisk->ulFlags & BD_FLAGS_IS_CLUSTER_RESOURCE) &&
						(pBasicVolume->ulFlags & BV_FLAGS_HAS_DRIVE_LETTER))
					{
						WCHAR				DriveLetter[0x03];
	
						// Volume on clusdisk. So create LogPathName.
						puszLogFileName->Length = 0;
						uszOverrideFilename.Length = 0;
						DriveLetter[0x00] = pBasicVolume->DriveLetter;
						DriveLetter[0x01] = L':';
						DriveLetter[0x02] = L'\0';
						RtlAppendUnicodeToString(&uszOverrideFilename, DriveLetter);
						RtlAppendUnicodeToString(&uszOverrideFilename, DEFAULT_LOG_DIR_FOR_CLUSDISKS);
						RtlAppendUnicodeToString(&uszOverrideFilename, LOG_FILE_NAME_PREFIX); // so things sort by name nice
						DriveLetter[0x01] = L'\0';
						RtlAppendUnicodeToString(&uszOverrideFilename, DriveLetter);
						RtlAppendUnicodeToString(&uszOverrideFilename, LOG_FILE_NAME_SUFFIX);
	
						pVolumeParam->WriteUnicodeString(VOLUME_LOG_PATH_NAME, &uszOverrideFilename);
						RtlAppendUnicodeToString(puszLogFileName, DOS_DEVICES_PATH);
						RtlAppendUnicodeStringToString(puszLogFileName, &uszOverrideFilename);
					}
	
					DereferenceBasicVolume(pBasicVolume);
				}
			}
		}
	
		delete uszOverrideFilename.Buffer;
	
		if (NULL != pVolumeParam)
			delete pVolumeParam;
	
		ValidatePathForFileName(puszLogFileName);
	
		return STATUS_SUCCESS;
	}
	*/
		/*
		PBASIC_VOLUME
		GetBasicVolumeFromDriverContext(PWCHAR pGUID)
		{
			PBASIC_DISK 	pBasicDisk = NULL;
			PBASIC_VOLUME	pBasicVolume = NULL;
			BOOLEAN 		bFound = FALSE;
			KIRQL			OldIrql;
		
			if (IsListEmpty(&DriverContext.HeadForBasicDisks))
				return NULL;
		
			KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
			pBasicDisk = (PBASIC_DISK)DriverContext.HeadForBasicDisks.Flink;
			while ((PLIST_ENTRY)pBasicDisk != &DriverContext.HeadForBasicDisks) {
				pBasicVolume = (PBASIC_VOLUME)pBasicDisk->DevList.Flink;
				while ((PLIST_ENTRY)pBasicVolume != &pBasicDisk->DevList) {
					if (GUID_SIZE_IN_BYTES == RtlCompareMemory(pGUID, pBasicVolume->GUID, GUID_SIZE_IN_BYTES)) {
						ReferenceBasicVolume(pBasicVolume);
						bFound = TRUE;
						break;
					}
		
					pBasicVolume = (PBASIC_VOLUME)pBasicVolume->ListEntry.Flink;
				}
		
				if (TRUE == bFound)
					break;
		
				pBasicDisk = (PBASIC_DISK)pBasicDisk->ListEntry.Flink;
			}
		
			KeReleaseSpinLock(&DriverContext.Lock, OldIrql);
		
			if (TRUE == bFound)
				return pBasicVolume;
		
			return NULL;
		}
		*/
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
    PDEV_CONTEXT        pDevContext;
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

    pDevContext = DeviceExtension->pDevContext;
    if (NULL == pDevContext) {
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
    pDirtyBlock->TagDataSource.ulDataSource = INFLTDRV_DATA_SOURCE_META_DATA;
        // Sum up the changes.
    for (unsigned int i = 0; i < pDirtyBlock->cChanges; i++) {
        pDirtyBlock->ulicbChanges.QuadPart += pDirtyBlock->Changes[i].Length;
    }

    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);

    // Add to main list
    pDirtyBlock->next = pDevContext->DirtyBlocksList;
    pDevContext->DirtyBlocksList = pDirtyBlock;
    pDirtyBlock->uliTransactionID.QuadPart = pDevContext->uliTransactionId.QuadPart++;

    if (pDevContext->pLastDirtyBlock == NULL) {
        ASSERT(NULL == pDirtyBlock->next);
        pDevContext->pLastDirtyBlock = pDirtyBlock;
    }
    pDevContext->lDirtyBlocksInQueue++;
        
    // change the statistics data
    pDevContext->ulTotalChangesPending += pDirtyBlock->cChanges;

    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);

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
/*
NTSTATUS
DeviceIoControlMSCSDiskAddedToCluster(
    PDEVICE_OBJECT  DeviceObject,
    PIRP            Irp
    )
{
    NTSTATUS    Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulDiskSignature;
    LIST_ENTRY          ListHead;
    PLIST_NODE          pListNode;
    PBASIC_DISK         pBasicDisk;
    PBASIC_VOLUME       pBasicVolume;
    PDEV_CONTEXT        pDevContext;

    UNREFERENCED_PARAMETER(DeviceObject);

    ASSERT(IOCTL_INMAGE_MSCS_DISK_ADDED_TO_CLUSTER == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);
    if ((NULL == Irp->AssociatedIrp.SystemBuffer) ||
        (sizeof(ULONG) > IoStackLocation->Parameters.DeviceIoControl.InputBufferLength)) 
    {
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    ulDiskSignature = *(PULONG)Irp->AssociatedIrp.SystemBuffer;

    // We have valid ulDiskSignature, now let us get all the volumes
    pBasicDisk = GetBasicDiskFromDriverContext(ulDiskSignature);
    if (NULL != pBasicDisk) {
        pBasicDisk->ulFlags |= BD_FLAGS_IS_CLUSTER_RESOURCE;
        InitializeListHead(&ListHead);
        GetDevListForBasicDisk(pBasicDisk, &ListHead);

        while (!IsListEmpty(&ListHead)) {
            pListNode = (PLIST_NODE)RemoveHeadList(&ListHead);
            ASSERT(NULL != pListNode);

            pBasicVolume = (PBASIC_VOLUME)pListNode->pData;
            ASSERT(NULL != pBasicVolume);

            // Check if bitmap file satisfies cluster conditions.
//            if (FALSE == IsBitmapFileOnSameVolume(pDevContext)) {
                // bitmap file is not on same volume
  //          }
            pDevContext = pBasicVolume->pDevContext;
            if ((NULL != pDevContext) && (!(pDevContext->ulFlags & DCF_FILTERING_STOPPED))) {
                // We have a volume and have to move the bitmap file if required.
//                if (ecBitMapReadCompleted == pDevContext->eBitMapReadState) {
                    // Bitmap is completely read, so we can close the existing bitmap.
  //                  if (NULL != pDevContext->pBitMapFile) {
//                        delete pDevContext->pBitMapFile;
//                        pDevContext->pBitMapFile = NULL;
//                    }
//                } else {
                    // Bitmap is being read. So we have to move the bitmap file, once the read in progress is completed.
//                }

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
*/
/*-----------------------------------------------------------------------------
 * Initialization Routines
 *-----------------------------------------------------------------------------
 */

/*-----------------------------------------------------------------------------------------------------
 *
 * Functions related to BASIC_DISK
 *
 *-----------------------------------------------------------------------------------------------------
 */
/*
#ifdef VOLUME_CLUSTER_SUPPORT
PBASIC_VOLUME
GetVolumeFromBasicDisk(PBASIC_DISK pBasicDisk, PWCHAR pGUID)
{
    PBASIC_VOLUME    pBasicVolume;
    KIRQL           OldIrql;
    BOOLEAN         bFound = FALSE;

    if (NULL == pBasicDisk)
        return NULL;

    KeAcquireSpinLock(&pBasicDisk->Lock, &OldIrql);
    pBasicVolume = (PBASIC_VOLUME)pBasicDisk->DevList.Flink;
    while (pBasicVolume != (PBASIC_VOLUME)&pBasicDisk->DevList) {
        if (GUID_SIZE_IN_BYTES == RtlCompareMemory(pGUID, &pBasicVolume->GUID, GUID_SIZE_IN_BYTES)) 
        {
            ReferenceBasicVolume(pBasicVolume);
            bFound = TRUE;
            break;
        }
        pBasicVolume = (PBASIC_VOLUME)pBasicVolume->ListEntry.Flink;
    }

    KeReleaseSpinLock(&pBasicDisk->Lock, OldIrql);

    if (FALSE == bFound)
        pBasicVolume = NULL;

    return pBasicVolume;
}
#endif
*/
/*
VOID
RemoveVolumeFromBasicDisk(PBASIC_VOLUME pBasicVolume)
{

    ASSERT(pBasicVolume->ulFlags & BV_FLAGS_INSERTED_IN_BASIC_DISK);
    if (pBasicVolume->ulFlags & BV_FLAGS_INSERTED_IN_BASIC_DISK) {
        pBasicVolume->ulFlags &= ~BV_FLAGS_INSERTED_IN_BASIC_DISK;
        RemoveEntryList(&pBasicVolume->ListEntry);
        DereferenceBasicVolume(pBasicVolume);
    }
}
*/
#if 0
	NTSTATUS
	InDskFltDispatchFlush(
		IN PDEVICE_OBJECT DeviceObject,
		IN PIRP Irp
		)
	{
	
		NTSTATUS Status = STATUS_UNSUCCESSFUL;
	
		Status = ValidateRequestForDeviceAndHandle(DeviceObject, Irp);
		if (!NT_SUCCESS(Status)) {
			goto Cleanup;
		}
	
	
		Status = InMageFltDispatchFlush(DeviceObject, Irp);
	Cleanup:
		return Status;
	}
	
	
	/*
	Description:  This routine is called as part of file system flush
	
	Args			:	DeviceObject - Encapsulates the filter driver's respective volume context
						Irp 			   - The i/o request meant for lower level drivers, usually.
	
	Return Value:	NT Status
	*/
	
	NTSTATUS
	InMageFltDispatchFlush(
		IN PDEVICE_OBJECT DeviceObject,
		IN PIRP Irp,
		IN BOOLEAN bNoRebootLayer,
		IN PDRIVER_DISPATCH pDispatchEntryFunction
		)
	{
		PDEVICE_EXTENSION  pDeviceExtension = NULL;//(PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
		PDEV_CONTEXT   pDevContext = NULL;
	
		InVolDbgPrint(DL_FUNC_TRACE, DM_FLUSH,
			("InMageFltFlush: DeviceObject %#p Irp %#p\n", DeviceObject, Irp));
		pDeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
	
		pDevContext = pDeviceExtension->pDevContext;
	
		if (NULL != pDevContext) {
			KIRQL		OldIrql;
			KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
			// Ignore Flush in case bitmap device is off
			if (TEST_FLAG(pDevContext->ulFlags, EXCLUDE_BITMAPOFF_PREOP)) {
				KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
				goto Cleanup;
			}
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
	
	Cleanup:
		//
		// Set current stack back one.
		//
		InSkipCurrentIrpStackLocation(Irp, bNoRebootLayer);
		return	InCallDriver(pDeviceExtension->TargetDeviceObject, 
							 Irp, 
							 bNoRebootLayer, 
							 pDispatchEntryFunction);
	} // end InMageFltFlush()
#endif
