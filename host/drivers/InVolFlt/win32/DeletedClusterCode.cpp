// This is the cluster code written expecting user level code would indicate the group offline and online.

NTSTATUS
DeviceIoControlMSCSGroupOffline(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    NTSTATUS            Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulGUIDLength;
    LIST_ENTRY          ListHead;
    PLIST_NODE          pListNode;
    PCLUS_DISK          pClusDisk;
    PCLUS_VOLUME        pClusVolume;

    ASSERT(IOCTL_INMAGE_MSCS_GROUP_OFFLINE == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);
    ulGUIDLength = sizeof(WCHAR) * GUID_SIZE_IN_CHARS;
    if (ulGUIDLength > IoStackLocation->Parameters.DeviceIoControl.InputBufferLength) {
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    // We have valid GUID, now let us get all the disk resources.
    InitializeListHead(&ListHead);
    Status = GetClusDisksForClusGroup((PWCHAR)Irp->AssociatedIrp.SystemBuffer, ulGUIDLength, &ListHead);
    if (STATUS_SUCCESS != Status) {
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    pListNode = (PLIST_NODE)ListHead.Flink;
    while ((PLIST_ENTRY)pListNode != &ListHead) {
        pClusDisk = (PCLUS_DISK)pListNode->pData;

        pClusVolume = (PCLUS_VOLUME)pClusDisk->VolumeList.Flink;
        while ((PLIST_ENTRY)pClusVolume != &pClusDisk->VolumeList) {
            PVOLUME_CONTEXT     pVolumeContext = pClusVolume->pVolumeContext;

            if (pVolumeContext) {
                InMageFltSaveAllChanges(pVolumeContext);
                if (pVolumeContext->pBitMapFile) {
                    pVolumeContext->pBitMapFile->UpdateDiskOffsets();
                    pVolumeContext->pBitMapFile->PrepareForShutdown(); // all changes go through last chance strategy after this point
                }
            }

            pClusVolume = (PCLUS_VOLUME)pClusVolume->ListEntry.Flink;
        }

        pListNode = (PLIST_NODE)pListNode->ListEntry.Flink;
    }

    while (!IsListEmpty(&ListHead)) {
        pListNode = (PLIST_NODE)RemoveHeadList(&ListHead);
        DereferenceClusDisk((PCLUS_DISK)pListNode->pData);
        DereferenceListNode(pListNode);
    }

    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS
GetClusDisksForClusGroup(
    PWCHAR  pClusGroupName,
    ULONG   ulClusGroupNameLength,
    PLIST_ENTRY pListHead
    )
{
    NTSTATUS            Status = STATUS_SUCCESS;    
    Registry            *pRegClusGroup = NULL;
    PWCHAR              pGroupMultiSz = NULL, pNextResource = NULL;
    ULONG               ulMultiSzCb = 0;
    CRegMultiSz         *pCRegMultiSz = NULL;

    Status = OpenGroupKey(pClusGroupName, ulClusGroupNameLength, &pRegClusGroup);
    if (STATUS_SUCCESS != Status)
        return Status;

    Status = pRegClusGroup->ReadMultiSz(REG_CLUSTER_GROUP_CONTAINS_VAL, STRING_LEN_NULL_TERMINATED, &pGroupMultiSz, &ulMultiSzCb);
    if (STATUS_SUCCESS != Status) {
        delete pRegClusGroup;
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    pCRegMultiSz = new CRegMultiSz();

    Status = pCRegMultiSz->SetMultiSz(pGroupMultiSz, ulMultiSzCb);
    if (STATUS_SUCCESS != Status) {
        if (NULL != pGroupMultiSz) 
            ExFreePoolWithTag(pGroupMultiSz, TAG_REGISTRY_DATA);
        delete pRegClusGroup;
        delete pCRegMultiSz;
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    while ((Status = pCRegMultiSz->GetNextSz(&pNextResource)) == STATUS_SUCCESS) {
        Registry *pRegResource;

        Status = OpenResourceKey(pNextResource, STRING_LEN_NULL_TERMINATED, &pRegResource);
        if (STATUS_SUCCESS == Status) {
            PWCHAR   pType = NULL;

            // Check if this resource is Physical Disk.
            Status = pRegResource->ReadWString(REG_CLUSTER_RES_TYPE_VAL, STRING_LEN_NULL_TERMINATED, &pType);
            if ((STATUS_SUCCESS == Status) && 
                (0 == wcsncmp(pType, REG_CLUSTER_RES_PHYSICAL_DISK, wcslen(REG_CLUSTER_RES_PHYSICAL_DISK) + 1)))
            {
                Registry    *pRegResParameters;
                ULONG       ulDiskSignature;
                // Resource is physical disk. Get the signature.

                Status = pRegResource->OpenSubKeyReadOnly(REG_CLUSTER_RES_PARAMETERS, 
                                                    STRING_LEN_NULL_TERMINATED, &pRegResParameters);
                if (STATUS_SUCCESS == Status) {
                    Status = pRegResParameters->ReadULONG(REG_CLUSTER_RES_SIGNATURE_VAL, (ULONG32 *)&ulDiskSignature);
                    if (STATUS_SUCCESS == Status) {
                        PCLUS_DISK  pClusDisk;
                        PLIST_NODE  pListNode;

                        // Found the disk signature
                        pClusDisk = GetClusterDiskFromDriverContext(ulDiskSignature);
                        if (NULL != pClusDisk) {
                            pListNode = AllocateListNode();

                            if (NULL != pListNode) {
                                pListNode->pData = (PVOID)pClusDisk;
                                InsertHeadList(pListHead, &pListNode->ListEntry);
                            } else {
                                DereferenceClusDisk(pClusDisk);
                            }
                        }
                    }

                    delete pRegResParameters;
                }
            }

            if (NULL != pType)
                ExFreePoolWithTag(pType, TAG_REGISTRY_DATA);
        }

        delete pRegResource;
    }

    if (NULL != pGroupMultiSz)
        ExFreePoolWithTag(pGroupMultiSz, TAG_REGISTRY_DATA);

    delete pRegClusGroup;
    delete pCRegMultiSz;

    if (STATUS_NO_MORE_ENTRIES == Status)
        Status = STATUS_SUCCESS;

    return Status;
}

NTSTATUS
OpenGroupKey(
    PWCHAR  pClusGroupName,
    ULONG   ulClusGroupNameLength,
    Registry **ppRegGroupKey
    )
{
    UNICODE_STRING      uszGroupKey;
    NTSTATUS            Status = STATUS_SUCCESS;    
    Registry            *pRegClusGroup = NULL;

    if (NULL == ppRegGroupKey)
        return STATUS_INVALID_PARAMETER;

    *ppRegGroupKey = NULL;

    uszGroupKey.MaximumLength = (wcslen(REG_CLUSTER_GROUPS) + 1 + GUID_SIZE_IN_CHARS + 1) * sizeof(WCHAR);
    uszGroupKey.Length = 0;
    uszGroupKey.Buffer = (PWSTR)ExAllocatePoolWithTag(PagedPool, uszGroupKey.MaximumLength, TAG_GENERIC_PAGED);

    if (NULL == uszGroupKey.Buffer)
        return STATUS_INSUFFICIENT_RESOURCES;

    Status = RtlAppendUnicodeToString(&uszGroupKey, REG_CLUSTER_GROUPS);
    if (STATUS_SUCCESS == Status) {
        Status = RtlAppendUnicodeToString(&uszGroupKey, L"\\");
        if (STATUS_SUCCESS == Status) {
            if (STRING_LEN_NULL_TERMINATED == ulClusGroupNameLength) {
                Status = RtlAppendUnicodeToString(&uszGroupKey, pClusGroupName);
            } else {
                UNICODE_STRING uszGroupName;

                uszGroupName.MaximumLength = uszGroupName.Length = (USHORT)ulClusGroupNameLength;
                uszGroupName.Buffer = pClusGroupName;
                Status = RtlAppendUnicodeStringToString(&uszGroupKey, &uszGroupName);
            }
        }
    }

    ASSERT(STATUS_SUCCESS == Status);
    if (STATUS_SUCCESS != Status) {
        ExFreePoolWithTag(uszGroupKey.Buffer, TAG_GENERIC_PAGED);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    pRegClusGroup = new Registry();
    if (NULL == pRegClusGroup) {
        ExFreePoolWithTag(uszGroupKey.Buffer, TAG_GENERIC_PAGED);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
        
    Status = pRegClusGroup->OpenKeyReadOnly(&uszGroupKey);
    if (STATUS_SUCCESS != Status) {
        delete pRegClusGroup;
        ExFreePoolWithTag(uszGroupKey.Buffer, TAG_GENERIC_PAGED);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    *ppRegGroupKey = pRegClusGroup;
    return STATUS_SUCCESS;
}

NTSTATUS
DeviceIoControlMSCSGetVolumes(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    NTSTATUS    Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulGUIDLength, ulNumVolumes, ulOutputLength, ulRequiredLength;
    ULONG               ulHeaderSize;
    PCLUS_DISK          pClusDisk = NULL;
    PCLUS_VOLUME        pClusVolume = NULL;
    PWCHAR              pClusResName;
    KIRQL               OldIrql;
    PMSCS_VOLUMES_IN_DISK   pVolumesInDisk;

    ASSERT(IOCTL_INMAGE_MSCS_GET_VOLUMES == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

    ulGUIDLength = sizeof(WCHAR) * GUID_SIZE_IN_CHARS;
    pClusResName = (PWCHAR)Irp->AssociatedIrp.SystemBuffer;
    ulOutputLength = IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength;
    ulHeaderSize = FIELD_OFFSET(MSCS_VOLUMES_IN_DISK, VolumeArray);

    if ((ulGUIDLength > IoStackLocation->Parameters.DeviceIoControl.InputBufferLength) ||
        (NULL == pClusResName) || (ulHeaderSize > ulOutputLength))
    {
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    pClusDisk = GetClusDiskForResourceGUID(pClusResName, ulGUIDLength);
    if (NULL != pClusDisk) {
        RtlZeroMemory(Irp->AssociatedIrp.SystemBuffer, ulOutputLength);
        pVolumesInDisk = (PMSCS_VOLUMES_IN_DISK)Irp->AssociatedIrp.SystemBuffer;

        pVolumesInDisk->ulDiskSignature = pClusDisk->ulDiskSignature;
        pVolumesInDisk->ulcbSizeReturned = ulHeaderSize;
        pVolumesInDisk->ulcbRequiredSize = ulHeaderSize;
        pVolumesInDisk->ulNumVolumes = 0;

        KeAcquireSpinLock(&pClusDisk->Lock, &OldIrql);
        pClusVolume = (PCLUS_VOLUME)pClusDisk->VolumeList.Flink;
        while((PLIST_ENTRY)pClusVolume != &pClusDisk->VolumeList) {
            pVolumesInDisk->ulcbRequiredSize += sizeof(MSCS_VOLUME);
            if (ulOutputLength >= pVolumesInDisk->ulcbRequiredSize) {
                pVolumesInDisk->ulcbSizeReturned += sizeof(MSCS_VOLUME);
                RtlStringCchCopyW(pVolumesInDisk->VolumeArray[pVolumesInDisk->ulNumVolumes].VolumeGUID, 
                                 GUID_SIZE_IN_CHARS + 1, pClusVolume->GUID);
                pVolumesInDisk->VolumeArray[pVolumesInDisk->ulNumVolumes].ulDriveLetterBitMap = pClusVolume->BuferForDriveLetterBitMap;
                pVolumesInDisk->ulNumVolumes++;
            }

            pClusVolume = (PCLUS_VOLUME)pClusVolume->ListEntry.Flink;
        }
        KeReleaseSpinLock(&pClusDisk->Lock, OldIrql);

        Irp->IoStatus.Information = pVolumesInDisk->ulcbSizeReturned;
        DereferenceClusDisk(pClusDisk);
    } else {
        Status = STATUS_NOT_FOUND;
        Irp->IoStatus.Information = 0;
    }
    
    Irp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

PCLUS_DISK
GetClusDiskForResourceGUID(
    PWCHAR  pClusResName,
    ULONG   ulcbClusResNameLength
    )
{
    NTSTATUS    Status;
    PCLUS_DISK  pClusDisk = NULL;
    Registry    *pRegResource = NULL;
    PWCHAR      pType = NULL;
    Registry    *pRegResParameters = NULL;

    Status = OpenResourceKey(pClusResName, ulcbClusResNameLength, &pRegResource);
    if (STATUS_SUCCESS != Status)
        return pClusDisk;

    // Check if this resource is Physical Disk.
    Status = pRegResource->ReadWString(REG_CLUSTER_RES_TYPE_VAL, STRING_LEN_NULL_TERMINATED, &pType);
    if ((STATUS_SUCCESS == Status) && 
        (0 == wcsncmp(pType, REG_CLUSTER_RES_PHYSICAL_DISK, wcslen(REG_CLUSTER_RES_PHYSICAL_DISK) + 1)))
    {
        ULONG       ulDiskSignature;
        // Resource is physical disk. Get the signature.

        Status = pRegResource->OpenSubKeyReadOnly(REG_CLUSTER_RES_PARAMETERS, 
                                            STRING_LEN_NULL_TERMINATED, &pRegResParameters);
        if (STATUS_SUCCESS == Status) {
            Status = pRegResParameters->ReadULONG(REG_CLUSTER_RES_SIGNATURE_VAL, (ULONG32 *)&ulDiskSignature);
            if (STATUS_SUCCESS == Status) {

                // Found the disk signature
                pClusDisk = GetClusterDiskFromDriverContext(ulDiskSignature);
            }
        }
    }

    if (NULL != pType)
        ExFreePoolWithTag(pType, TAG_REGISTRY_DATA);

    if (NULL != pRegResource)
        delete pRegResource;

    if (NULL != pRegResParameters)
        delete pRegResParameters;

    return pClusDisk;
}

NTSTATUS
OpenResourceKey(
    PWCHAR  pResGroupName,
    ULONG   ulClusResNameLength,
    Registry **ppRegResKey
    )
{
    UNICODE_STRING      uszResKey;
    NTSTATUS            Status = STATUS_SUCCESS;    
    Registry            *pRegClusRes = NULL;

    if (NULL == ppRegResKey)
        return STATUS_INVALID_PARAMETER;

    *ppRegResKey = NULL;

    uszResKey.MaximumLength = (wcslen(REG_CLUSTER_RESOURCES) + 1 + GUID_SIZE_IN_CHARS + 1) * sizeof(WCHAR);
    uszResKey.Length = 0;
    uszResKey.Buffer = (PWSTR)ExAllocatePoolWithTag(PagedPool, uszResKey.MaximumLength, TAG_GENERIC_PAGED);

    if (NULL == uszResKey.Buffer)
        return STATUS_INSUFFICIENT_RESOURCES;

    Status = RtlAppendUnicodeToString(&uszResKey, REG_CLUSTER_RESOURCES);
    if (STATUS_SUCCESS == Status) {
        Status = RtlAppendUnicodeToString(&uszResKey, L"\\");
        if (STATUS_SUCCESS == Status) {
            if (STRING_LEN_NULL_TERMINATED == ulClusResNameLength) {
                Status = RtlAppendUnicodeToString(&uszResKey, pResGroupName);
            } else {
                UNICODE_STRING uszResName;

                uszResName.MaximumLength = uszResName.Length = (USHORT)ulClusResNameLength;
                uszResName.Buffer = pResGroupName;
                Status = RtlAppendUnicodeStringToString(&uszResKey, &uszResName);
            }
        }
    }

    ASSERT(STATUS_SUCCESS == Status);
    if (STATUS_SUCCESS != Status) {
        ExFreePoolWithTag(uszResKey.Buffer, TAG_GENERIC_PAGED);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    pRegClusRes = new Registry();
    if (NULL == pRegClusRes) {
        ExFreePoolWithTag(uszResKey.Buffer, TAG_GENERIC_PAGED);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
        
    Status = pRegClusRes->OpenKeyReadOnly(&uszResKey);
    if (STATUS_SUCCESS != Status) {
        delete pRegClusRes;
        ExFreePoolWithTag(uszResKey.Buffer, TAG_GENERIC_PAGED);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    *ppRegResKey = pRegClusRes;
    return STATUS_SUCCESS;
}

