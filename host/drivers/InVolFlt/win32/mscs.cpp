#include "Common.h"
#include "Global.h"
#include "InVolFlt.h"
#include "mscs.h"
#include "Registry.h"
#include "DriverContext.h"
#include "RegMultiSz.h"
#include "ListNode.h"
#include "MntMgr.h"
#include "VBitmap.h"
#include "VContext.h"

NTSTATUS
DeviceIoControlMSCSDiskRemovedFromCluster(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    NTSTATUS            Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulDiskSignature;
    LIST_ENTRY          ListHead;
    PLIST_NODE          pListNode;
    PBASIC_DISK         pBasicDisk;
    PBASIC_VOLUME       pBasicVolume;
    PVOLUME_CONTEXT     pVolumeContext;

    ASSERT(IOCTL_INMAGE_MSCS_DISK_REMOVED_FROM_CLUSTER == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);
    if ((NULL == Irp->AssociatedIrp.SystemBuffer) ||
        (sizeof(ULONG) > IoStackLocation->Parameters.DeviceIoControl.InputBufferLength)) 
    {
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

#if (NTDDI_VERSION >= NTDDI_VISTA)
  
    CDSKFL_INFO  *cDiskInfo = (CDSKFL_INFO *)Irp->AssociatedIrp.SystemBuffer;
    
    if(cDiskInfo->ePartitionStyle == ecPartStyleMBR) {
        pBasicDisk = GetBasicDiskFromDriverContext(cDiskInfo->ulDiskSignature);
    } else if(cDiskInfo->ePartitionStyle == ecPartStyleGPT) {
        pBasicDisk = GetBasicDiskFromDriverContext(cDiskInfo->DiskId);
    } else
        pBasicDisk = NULL;
    
#else
  
    ulDiskSignature = *(PULONG)Irp->AssociatedIrp.SystemBuffer;

    // We have valid ulDiskSignature, now let us get all the device.
    pBasicDisk = GetBasicDiskFromDriverContext(ulDiskSignature);
    
#endif
    if (NULL != pBasicDisk) {
        pBasicDisk->ulFlags &= ~BD_FLAGS_IS_CLUSTER_RESOURCE;
        InitializeListHead(&ListHead);
        GetVolumeListForBasicDisk(pBasicDisk, &ListHead);

        while (!IsListEmpty(&ListHead)) {
            pListNode = (PLIST_NODE)RemoveHeadList(&ListHead);
            ASSERT(NULL != pListNode);

            pBasicVolume = (PBASIC_VOLUME)pListNode->pData;
            ASSERT(NULL != pBasicVolume);

            pVolumeContext = pBasicVolume->pVolumeContext;
            if (NULL != pVolumeContext) {
                PVOLUME_BITMAP  pVolumeBitmap = NULL;

                // By this time already we are in last chance mode as file system is unmounted.
                // we write changes directly to volume and close the file.

                InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DeviceIoControlMSCSDiskRemovedFromCluster: VolumeContext is %p, Volume = %S\n",
                                    pVolumeContext, pVolumeContext->wDriveLetter));

                if (pVolumeContext->ulFlags & VCF_CV_FS_UNMOUTNED) {
                    InVolDbgPrint( DL_INFO, DM_CLUSTER, ("File System is found unmounted at disk removal from cluster\n"));
                } else {
                    InVolDbgPrint( DL_INFO, DM_CLUSTER, ("File System is found mounted at disk removal from cluster\n"));
                }

                pVolumeBitmap = pVolumeContext->pVolumeBitmap;
                pVolumeContext->pVolumeBitmap = NULL;
                if (pVolumeBitmap) {
                    // Disk is being removed, at this point of time disk should have been already released, 
                    // so bitmap would be in raw IO and commited.
                    if (pVolumeBitmap->pVolumeContext) {
                        DereferenceVolumeContext(pVolumeBitmap->pVolumeContext);
                        pVolumeBitmap->pVolumeContext = NULL;
                    }
                    DereferenceVolumeBitmap(pVolumeBitmap);
                }

                pVolumeContext->ulFlags &= ~(VCF_CLUSTER_VOLUME_ONLINE | VCF_VOLUME_ON_CLUS_DISK | VCF_CLUSDSK_RETURNED_OFFLINE);

                if (pVolumeContext->PnPNotificationEntry) {
                    IoUnregisterPlugPlayNotification(pVolumeContext->PnPNotificationEntry);
                    pVolumeContext->PnPNotificationEntry = NULL;
                }
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
    PVOLUME_CONTEXT     pVolumeContext;

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
        GetVolumeListForBasicDisk(pBasicDisk, &ListHead);

        while (!IsListEmpty(&ListHead)) {
            pListNode = (PLIST_NODE)RemoveHeadList(&ListHead);
            ASSERT(NULL != pListNode);

            pBasicVolume = (PBASIC_VOLUME)pListNode->pData;
            ASSERT(NULL != pBasicVolume);

            // Check if bitmap file satisfies cluster conditions.
//            if (FALSE == IsBitmapFileOnSameVolume(pVolumeContext)) {
                // bitmap file is not on same volume
  //          }
            pVolumeContext = pBasicVolume->pVolumeContext;
            if ((NULL != pVolumeContext) && (!(pVolumeContext->ulFlags & VCF_FILTERING_STOPPED))) {
                // We have a volume and have to move the bitmap file if required.
//                if (ecBitMapReadCompleted == pVolumeContext->eBitMapReadState) {
                    // Bitmap is completely read, so we can close the existing bitmap.
  //                  if (NULL != pVolumeContext->pBitMapFile) {
//                        delete pVolumeContext->pBitMapFile;
//                        pVolumeContext->pBitMapFile = NULL;
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

/*-----------------------------------------------------------------------------
 * Initialization Routines
 *-----------------------------------------------------------------------------
 */
#if (NTDDI_VERSION < NTDDI_VISTA)
// This function is for Win2K3, Win2K, WinXP
VOID
LoadClusterConfigFromClustDiskDriverParameters()
{
    NTSTATUS    Status = STATUS_SUCCESS;
    Registry    *pRegClusDisk = NULL;
    ULONG       ulIndex, ulSubKeyLength, ulDiskSignature;
    PKEY_BASIC_INFORMATION pSubKeyInformation;
    UNICODE_STRING          ustrDiskSignature;

    pRegClusDisk = new Registry();
    if (NULL == pRegClusDisk)
        return;

    Status = pRegClusDisk->OpenKeyReadOnly(REG_CLUSDISK_KEY);
    if (STATUS_SUCCESS != Status) {
        InVolDbgPrint(DL_INFO, DM_CLUSTER, ("This System is not a cluster node. Does not have ClusDisk driver installed\n"));
        return;
    }

    pRegClusDisk->CloseKey();

    Status = pRegClusDisk->OpenKeyReadOnly(REG_CLUSDISK_SIGNATURES_KEY);
    if (STATUS_SUCCESS != Status) {
        InVolDbgPrint(DL_INFO, DM_CLUSTER, ("This System does not have any active shared drives on Cluster\n"));
        return;
    }

    ulIndex = 0;
    do {
        pSubKeyInformation = NULL;
        ulSubKeyLength = 0;
        Status = pRegClusDisk->GetSubKey(ulIndex, &pSubKeyInformation, &ulSubKeyLength);

        if ((STATUS_SUCCESS == Status) && (NULL != pSubKeyInformation) && (0 != ulSubKeyLength)) {
            InVolDbgPrint(DL_INFO, DM_CLUSTER, ("SubKey Index = %d, SubKey = %.*S\n", ulIndex, 
                        pSubKeyInformation->NameLength/sizeof(WCHAR),
                        pSubKeyInformation->Name));
            ustrDiskSignature.Length = (USHORT)pSubKeyInformation->NameLength;
            ustrDiskSignature.MaximumLength = (USHORT)pSubKeyInformation->NameLength;
            ustrDiskSignature.Buffer = pSubKeyInformation->Name;

            Status = RtlUnicodeStringToInteger(&ustrDiskSignature, 0x10, &ulDiskSignature);
            if (STATUS_SUCCESS == Status)
            {
                PBASIC_DISK  pBasicDisk;

                InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DiskSignature = 0x%x\n",ulDiskSignature));
                pBasicDisk = AddBasicDiskToDriverContext(ulDiskSignature);
                if (NULL != pBasicDisk) {
                    ULONG       ulDiskNameLength;
                    Registry    *pRegDiskSignature;
                    PWCHAR      pKeyName;
                    PWCHAR      pValue;

                    // This disk is part of cluster
                    pBasicDisk->ulFlags |= BD_FLAGS_IS_CLUSTER_RESOURCE;

                    ulDiskNameLength = (wcslen(REG_CLUSDISK_SIGNATURES_KEY) + 2) * sizeof(WCHAR);
                    ulDiskNameLength += pSubKeyInformation->NameLength;

                    pKeyName = (PWCHAR)ExAllocatePoolWithTag(PagedPool, ulDiskNameLength, TAG_GENERIC_PAGED);
                    if (NULL != pKeyName) {
                        RtlZeroMemory(pKeyName, ulDiskNameLength);
                        RtlStringCbCopyW(pKeyName, ulDiskNameLength, REG_CLUSDISK_SIGNATURES_KEY);
                        RtlStringCbCatW(pKeyName, ulDiskNameLength, L"\\");
                        RtlStringCbCatNW(pKeyName, ulDiskNameLength, pSubKeyInformation->Name, pSubKeyInformation->NameLength);

                        pRegDiskSignature = new Registry();

                        Status = pRegDiskSignature->OpenKeyReadOnly(pKeyName);
                        if (STATUS_SUCCESS == Status) {
                            Status = pRegDiskSignature->ReadWString(REG_CLUSDISK_DISKNAME_VALUE, STRING_LEN_NULL_TERMINATED, &pValue);
                            if ((STATUS_SUCCESS == Status) && (NULL != pValue)) {
                                RtlStringCchCopyW(pBasicDisk->DiskName, MAX_DISKNAME_SIZE_IN_CHAR, pValue);
                                    
                                if(wcslen(DISK_NAME_PREFIX)*sizeof(WCHAR) == RtlCompareMemory(pBasicDisk->DiskName, DISK_NAME_PREFIX, wcslen(DISK_NAME_PREFIX)*sizeof(WCHAR)))
                                {
                                    UNICODE_STRING   ustrDiskName;
                                    ustrDiskName.Buffer = pBasicDisk->DiskName + wcslen(DISK_NAME_PREFIX);
                                    ustrDiskName.MaximumLength = (MAX_DISKNAME_SIZE_IN_CHAR-wcslen(DISK_NAME_PREFIX))*sizeof(WCHAR);
                                    ustrDiskName.Length = (MAX_DISKNAME_SIZE_IN_CHAR-wcslen(DISK_NAME_PREFIX))*sizeof(WCHAR);
                                    RtlUnicodeStringToInteger(&ustrDiskName, 10, &pBasicDisk->ulDiskNumber );
                                }

                            }

                            if (NULL != pValue)
                                ExFreePoolWithTag(pValue, TAG_REGISTRY_DATA);
                        }

                        ExFreePoolWithTag(pKeyName, TAG_GENERIC_PAGED);
                        delete pRegDiskSignature;
                    }
                 
                    DereferenceBasicDisk(pBasicDisk);
                    pBasicDisk = NULL;
                }
                // Reset the Status value, as this can be a failure if DiskName is not present.
                Status = STATUS_SUCCESS;
            }
        }

        if (NULL != pSubKeyInformation) {
            ExFreePoolWithTag(pSubKeyInformation, TAG_REGISTRY_DATA);
            pSubKeyInformation = NULL;
            ulSubKeyLength = 0;
        }

        ulIndex++;
    } while(STATUS_SUCCESS == Status);

    delete pRegClusDisk;
    return;
}

#else

VOID
LoadClusterConfigFromClustDiskDriverParameters()
{
    NTSTATUS    Status = STATUS_SUCCESS;
    Registry    *pRegClusDisk = NULL;
    PWCHAR      *WStringArray = NULL;
    ULONG       ulArraySize = 0;

    pRegClusDisk = new Registry();
    if (NULL == pRegClusDisk)
        return;

    Status = pRegClusDisk->OpenKeyReadOnly(REG_CLUSDISK_KEY);
    if (STATUS_SUCCESS != Status) {
        InVolDbgPrint(DL_INFO, DM_CLUSTER, ("This System is not a cluster node. Does not have ClusDisk driver installed\n"));
        return;
    }

    pRegClusDisk->CloseKey();

    Status = pRegClusDisk->OpenKeyReadOnly(REG_CLUSDISK_PARAMETERS_KEY);
    if (STATUS_SUCCESS != Status) {
        InVolDbgPrint(DL_INFO, DM_CLUSTER, ("This System does not have any active shared drives on Cluster\n"));
        return;
    }

    Status = pRegClusDisk->ReadMultiSzToArray(REG_CLUSTER_ATTACHED_DISKS_VAL, STRING_LEN_NULL_TERMINATED, WStringArray, ulArraySize);
    if (NT_SUCCESS(Status)) {
        for (ULONG ulIndex = 0; ulIndex < ulArraySize; ulIndex += 2) {
            // DiskSignature is in Odd Array Index (ulIndex)
            // I Believe its DiskType GUID is in even ArrayIndex (ulIndex + 1)
            // {026061e9-eeb5-4354-bfd7-17d2cb77d2af} is GUID for iSCSI disks
            // 
            if (NULL != WStringArray[ulIndex]) {
                UNICODE_STRING          ustrDiskSignature;
                ULONG                   ulDiskSignature;
                GUID                    Guid;
                PBASIC_DISK             pBasicDisk = NULL;

                RtlInitUnicodeString(&ustrDiskSignature, WStringArray[ulIndex]);
                // Lets check if ulIndex has diskSignature or GUID.
                // For MBR disks we have DiskSignature. DiskSignature String size is 16 bytes or 8 WCHARS
                // For GPT disks we have a GUID.
                // For now we are only supporting MBR Disks
                if (ustrDiskSignature.Length <= 16) {
                    Status = RtlUnicodeStringToInteger(&ustrDiskSignature, 0x10, &ulDiskSignature);
                    if (STATUS_SUCCESS == Status) {
                        PBASIC_DISK  pBasicDisk;

                        InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DiskSignature = 0x%x\n",ulDiskSignature));
                        pBasicDisk = AddBasicDiskToDriverContext(ulDiskSignature);
                        if (NULL != pBasicDisk) {
                            // This disk is part of cluster
                            pBasicDisk->ulFlags |= BD_FLAGS_IS_CLUSTER_RESOURCE;
                            pBasicDisk->PartitionStyle = ecPartStyleMBR;
    
                            DereferenceBasicDisk(pBasicDisk);
                            pBasicDisk = NULL;
                        }
                    }
                    // Reset the Status value, as this can be a failure if DiskName is not present.
                    Status = STATUS_SUCCESS;
                } else if(ustrDiskSignature.Length == 76) {
                        Status = RtlGUIDFromString(&ustrDiskSignature, &Guid);
                        
                        if (STATUS_SUCCESS == Status) {
                            pBasicDisk = AddBasicDiskToDriverContext(Guid);
                            if (NULL != pBasicDisk) {
                            // This disk is part of cluster
                                pBasicDisk->ulFlags |= BD_FLAGS_IS_CLUSTER_RESOURCE;
                                pBasicDisk->PartitionStyle = ecPartStyleGPT;
                                DereferenceBasicDisk(pBasicDisk);
                                pBasicDisk = NULL;
                            }
                        }
                        Status = STATUS_SUCCESS;
                    
                }
            } // if (NULL != WStringArray[ulIndex]
        } // for loop

        pRegClusDisk->FreeMultiSzArray(WStringArray, ulArraySize);
    }

    delete pRegClusDisk;
    return;
}

#endif // (NTDDI_VERSION < NTDDI_VISTA)

/*-----------------------------------------------------------------------------
 *
 * Functions related to allocation and deallocation of CLUS_VOLUME
 *
 *-----------------------------------------------------------------------------
 */
VOID
DeallocateBasicVolume(PBASIC_VOLUME pBasicVolume)
{
    ASSERT(NULL != pBasicVolume);
    ASSERT(0 == pBasicVolume->lRefCount);
    
    if (pBasicVolume->ulFlags & BV_FLAGS_INSERTED_IN_BASIC_DISK) {
        ASSERT(NULL != pBasicVolume->pDisk);

        RemoveEntryList(&pBasicVolume->ListEntry);
        DereferenceBasicDisk(pBasicVolume->pDisk);
    }

    ASSERT(NULL != pBasicVolume->pVolumeContext);
    DereferenceVolumeContext(pBasicVolume->pVolumeContext);

    ExFreePoolWithTag(pBasicVolume, TAG_BASIC_VOLUME);
}

PBASIC_VOLUME
AllocateBasicVolume()
{
    PBASIC_VOLUME    pBasicVolume;

    pBasicVolume = (PBASIC_VOLUME)ExAllocatePoolWithTag(NonPagedPool, sizeof(BASIC_VOLUME), TAG_BASIC_VOLUME);
    if (NULL == pBasicVolume)
        return NULL;

    RtlZeroMemory(pBasicVolume, sizeof(BASIC_VOLUME));
    pBasicVolume->lRefCount = 1;

    RtlInitializeBitMap(&pBasicVolume->DriveLetterBitMap,
                        &pBasicVolume->BuferForDriveLetterBitMap,
                        MAX_NUM_DRIVE_LETTERS);

    return pBasicVolume;
}

LONG
ReferenceBasicVolume(PBASIC_VOLUME pBasicVolume)
{
    LONG    lRefCount;

    ASSERT(pBasicVolume->lRefCount >= 1);
    lRefCount = InterlockedIncrement(&pBasicVolume->lRefCount);
    return lRefCount;
}

LONG
DereferenceBasicVolume(PBASIC_VOLUME pBasicVolume)
{
    LONG    lRefCount;

    ASSERT(pBasicVolume->lRefCount >= 1);
    lRefCount = InterlockedDecrement(&pBasicVolume->lRefCount);
    if (0 == lRefCount) {
        DeallocateBasicVolume(pBasicVolume);
    }

    return lRefCount;
}

/*-----------------------------------------------------------------------------
 *
 * Functions related to allocation and deallocation of CLUS_DISK
 *
 *-----------------------------------------------------------------------------
 */
VOID
DeallocateBasicDisk(PBASIC_DISK pBasicDisk)
{
    ASSERT(NULL != pBasicDisk);
    ASSERT(0 == pBasicDisk->lRefCount);
    ASSERT(IsListEmpty(&pBasicDisk->VolumeList));

    ExFreePoolWithTag(pBasicDisk, TAG_BASIC_DISK);
    return;
}

PBASIC_DISK
AllocateBasicDisk()
{
    PBASIC_DISK  pBasicDisk = NULL;

    pBasicDisk = (PBASIC_DISK)ExAllocatePoolWithTag(NonPagedPool, sizeof(BASIC_DISK), TAG_BASIC_DISK);
    if (NULL == pBasicDisk)
        return NULL;

    RtlZeroMemory(pBasicDisk, sizeof(BASIC_DISK));
    pBasicDisk->lRefCount = 1;

    pBasicDisk->ulDiskNumber = 0xffffffff;

    InitializeListHead(&pBasicDisk->VolumeList);
    KeInitializeSpinLock(&pBasicDisk->Lock);
    return pBasicDisk;
}

LONG
ReferenceBasicDisk(PBASIC_DISK    pBasicDisk)
{
    LONG    lRefCount;

    ASSERT(pBasicDisk->lRefCount >= 1);
    lRefCount = InterlockedIncrement(&pBasicDisk->lRefCount);
    return lRefCount;
}

LONG
DereferenceBasicDisk(PBASIC_DISK  pBasicDisk)
{
    LONG    lRefCount;

    ASSERT(pBasicDisk->lRefCount >= 1);
    lRefCount = InterlockedDecrement(&pBasicDisk->lRefCount);
    if (0 == lRefCount) {
        DeallocateBasicDisk(pBasicDisk);
    }

    return lRefCount;
}

/*-----------------------------------------------------------------------------------------------------
 *
 * Functions related to BASIC_DISK
 *
 *-----------------------------------------------------------------------------------------------------
 */
PBASIC_VOLUME
GetVolumeFromBasicDisk(PBASIC_DISK pBasicDisk, PWCHAR pGUID)
{
    PBASIC_VOLUME    pBasicVolume;
    KIRQL           OldIrql;
    BOOLEAN         bFound = FALSE;

    if (NULL == pBasicDisk)
        return NULL;

    KeAcquireSpinLock(&pBasicDisk->Lock, &OldIrql);
    pBasicVolume = (PBASIC_VOLUME)pBasicDisk->VolumeList.Flink;
    while (pBasicVolume != (PBASIC_VOLUME)&pBasicDisk->VolumeList) {
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

PBASIC_VOLUME
AddVolumeToBasicDisk(PBASIC_DISK pBasicDisk, PVOLUME_CONTEXT VolumeContext)
{
    PBASIC_VOLUME    pBasicVolume;
    KIRQL           OldIrql;

    if (NULL == pBasicDisk)
        return NULL;

    pBasicVolume = VolumeContext->pBasicVolume;
    if (NULL == pBasicVolume) {
        pBasicVolume = AllocateBasicVolume();
        if(NULL == pBasicVolume)
            return NULL;
        VolumeContext->pBasicVolume = pBasicVolume;
        ReferenceBasicDisk(pBasicDisk);
        pBasicVolume->pDisk = pBasicDisk;
        RtlCopyMemory(pBasicVolume->GUID, VolumeContext->wGUID, (GUID_SIZE_IN_CHARS * sizeof(WCHAR)));
        pBasicVolume->GUID[GUID_SIZE_IN_CHARS] = L'\0';
        pBasicVolume->pVolumeContext = ReferenceVolumeContext(VolumeContext);

        pBasicVolume->ulFlags |= BV_FLAGS_INSERTED_IN_BASIC_DISK;

        // Add reference for adding to the list.
        ReferenceBasicVolume(pBasicVolume);

        KeAcquireSpinLock(&pBasicDisk->Lock, &OldIrql);
        InsertHeadList(&pBasicDisk->VolumeList, &pBasicVolume->ListEntry);
        KeReleaseSpinLock(&pBasicDisk->Lock, OldIrql);
    }

    return pBasicVolume;
}

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

VOID
GetVolumeListForBasicDisk(PBASIC_DISK pBasicDisk, PLIST_ENTRY ListHead)
{
    KIRQL           OldIrql;
    PBASIC_VOLUME    pBasicVolume;
    PLIST_NODE      pListNode;

    ASSERT(NULL != pBasicDisk);
    // For now we are using this function to create a new list.
    // so let us assert on list being empty. If we use this function to add volumes
    // to existing list this assert has to be removed.
    ASSERT(IsListEmpty(ListHead));

    if (NULL != pBasicDisk) {
        KeAcquireSpinLock(&pBasicDisk->Lock, &OldIrql);

        pBasicVolume = (PBASIC_VOLUME)pBasicDisk->VolumeList.Flink;
        while ((PLIST_ENTRY)pBasicVolume != &pBasicDisk->VolumeList) {

            pListNode = AllocateListNode();
            if (NULL != pListNode) {
                ReferenceBasicVolume(pBasicVolume);
                pListNode->pData = pBasicVolume;
                InsertTailList(ListHead, &pListNode->ListEntry);
            }

            pBasicVolume = (PBASIC_VOLUME)pBasicVolume->ListEntry.Flink;
        }

        KeReleaseSpinLock(&pBasicDisk->Lock, OldIrql);
    }

    return;
}


VOID
GetBasicDiskList(PLIST_ENTRY ListHead)
{
    KIRQL           OldIrql;
    PBASIC_DISK pBasicDisk = NULL;
    PLIST_NODE      pListNode;

    ASSERT(IsListEmpty(ListHead));

    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);

    pBasicDisk = (PBASIC_DISK) DriverContext.HeadForBasicDisks.Flink;
    while (pBasicDisk && (PLIST_ENTRY)pBasicDisk != &DriverContext.HeadForBasicDisks) {
        pListNode = AllocateListNode();
        if (NULL != pListNode) {
            ReferenceBasicDisk(pBasicDisk);
            pListNode->pData = pBasicDisk;
            InsertTailList(ListHead, &pListNode->ListEntry);
        }
        pBasicDisk = (PBASIC_DISK)pBasicDisk->ListEntry.Flink;
    }

    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

    return;
}

    
