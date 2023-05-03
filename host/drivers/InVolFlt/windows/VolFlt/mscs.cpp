#include "Common.h"
#include "Global.h"
#include "InmFltInterface.h"
#include "mscs.h"
#include "Registry.h"
#include "DriverContext.h"
#include "ListNode.h"
#include "MntMgr.h"
#include "VBitmap.h"
#include "VContext.h"
#include "HelperFunctions.h"

#ifdef VOLUME_CLUSTER_SUPPORT
#ifdef VOLUME_NO_REBOOT_SUPPORT
ULONG
GetOnlineDisksSignature(S_GET_DISKS  *sDiskSignature);
VOID
GetDevListForBasicDisk(PBASIC_DISK pBasicDisk, PLIST_ENTRY ListHead);
#endif

extern "C" void 
InitializeClusterDiskAcquireParams(PDEV_CONTEXT pDevContext);

NTSTATUS
DeviceIoControlMSCSDiskRemovedFromCluster(
	PDEVICE_OBJECT	DeviceObject,
    PIRP Irp
    )
{
    NTSTATUS            Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
#if (NTDDI_VERSION < NTDDI_VISTA)
    ULONG               ulDiskSignature;
#endif
    LIST_ENTRY          ListHead;
    PLIST_NODE          pListNode;
    PBASIC_DISK         pBasicDisk;
    PBASIC_VOLUME       pBasicVolume;
    PDEV_CONTEXT        pDevContext;

    UNREFERENCED_PARAMETER(DeviceObject);

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
        GetDevListForBasicDisk(pBasicDisk, &ListHead);

        while (!IsListEmpty(&ListHead)) {
            pListNode = (PLIST_NODE)RemoveHeadList(&ListHead);
            ASSERT(NULL != pListNode);

            pBasicVolume = (PBASIC_VOLUME)pListNode->pData;
            ASSERT(NULL != pBasicVolume);

            pDevContext = pBasicVolume->pDevContext;
            if (NULL != pDevContext) {
                PDEV_BITMAP  pDevBitmap = NULL;

                // By this time already we are in last chance mode as file system is unmounted.
                // we write changes directly to volume and close the file.

                InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DeviceIoControlMSCSDiskRemovedFromCluster: DevContext is %p, Device = %S\n",
                                    pDevContext, pDevContext->wDevNum));

                if (pDevContext->ulFlags & DCF_CV_FS_UNMOUTNED) {
                    InVolDbgPrint( DL_INFO, DM_CLUSTER, ("File System is found unmounted at disk removal from cluster\n"));
                } else {
                    InVolDbgPrint( DL_INFO, DM_CLUSTER, ("File System is found mounted at disk removal from cluster\n"));
                }

                pDevBitmap = pDevContext->pDevBitmap;
                pDevContext->pDevBitmap = NULL;
                if (pDevBitmap) {
                    // Disk is being removed, at this point of time disk should have been already released, 
                    // so bitmap would be in raw IO and commited.
                    if (pDevBitmap->pDevContext) {
                        DereferenceDevContext(pDevBitmap->pDevContext);
                        pDevBitmap->pDevContext = NULL;
                    }
                    DereferenceDevBitmap(pDevBitmap);
                }

                pDevContext->ulFlags &= ~(DCF_CLUSTER_VOLUME_ONLINE | DCF_VOLUME_ON_CLUS_DISK | DCF_CLUSDSK_RETURNED_OFFLINE);

                if (pDevContext->PnPNotificationEntry) {
                    IoUnregisterPlugPlayNotification(pDevContext->PnPNotificationEntry);
                    pDevContext->PnPNotificationEntry = NULL;
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


NTSTATUS DeviceIoControlMSCSDiskAcquire(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    NTSTATUS            Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
#if (NTDDI_VERSION < NTDDI_VISTA)
    ULONG               ulDiskSignature = 0,pKeyNameLength = 0;
#endif
    PBASIC_DISK         pBasicDisk = NULL;
    PBASIC_VOLUME       pBasicVolume = NULL;
    KIRQL               OldIrql;
#if (NTDDI_VERSION < NTDDI_VISTA)
    Registry            *pRegDiskSignature = NULL;
    PWCHAR              pKeyName, pValue;
    WCHAR               BufferDiskSig[9];
    UNICODE_STRING      ustrDiskSig ;
#endif

    UNREFERENCED_PARAMETER(DeviceObject);


        
    ASSERT(IOCTL_INMAGE_MSCS_DISK_ACQUIRE == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

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

    if(pBasicDisk)
        pBasicDisk->ulDiskNumber = cDiskInfo->DeviceNumber;
#else

    ulDiskSignature = *(PULONG)Irp->AssociatedIrp.SystemBuffer;

    // We have valid ulDiskSignature, now let us get all the device.
    pBasicDisk = GetBasicDiskFromDriverContext(ulDiskSignature);
    
#endif

#if (NTDDI_VERSION < NTDDI_VISTA)

    if(pBasicDisk && IsListEmpty(&pBasicDisk->DevList)) {

        ustrDiskSig.Buffer = BufferDiskSig;
        ustrDiskSig.MaximumLength = 18;
        ustrDiskSig.Length = 0;
        
        RtlIntegerToUnicodeString(ulDiskSignature, 0x10, &ustrDiskSig);

        ustrDiskSig.Buffer[8] = 0;

        pRegDiskSignature = new Registry();
        if (pRegDiskSignature) {
            pKeyNameLength = (ULONG)((wcslen(REG_CLUSDISK_SIGNATURES_KEY) + wcslen(L"\\XXXXXXXX")) * sizeof(WCHAR) + sizeof(WCHAR));
            pKeyName = (PWCHAR)ExAllocatePoolWithTag(PagedPool, pKeyNameLength, TAG_GENERIC_PAGED);

            if(pKeyName) {
                RtlZeroMemory(pKeyName, pKeyNameLength);
                RtlStringCbCopyW(pKeyName, pKeyNameLength, REG_CLUSDISK_SIGNATURES_KEY);
                RtlStringCbCatW(pKeyName, pKeyNameLength, L"\\");
                RtlStringCbCatNW(pKeyName, pKeyNameLength, ustrDiskSig.Buffer, ustrDiskSig.MaximumLength);
                Status = pRegDiskSignature->OpenKeyReadOnly(pKeyName);

                if (STATUS_SUCCESS == Status) {
                    Status = pRegDiskSignature->ReadWString(REG_CLUSDISK_DISKNAME_VALUE, STRING_LEN_NULL_TERMINATED, &pValue);
                    if ((STATUS_SUCCESS == Status) && (NULL != pValue)) {
                        RtlStringCchCopyW(pBasicDisk->DiskName, MAX_DISKNAME_SIZE_IN_CHAR, pValue);
                    }
                    if (NULL != pValue)
                        ExFreePoolWithTag(pValue, TAG_REGISTRY_DATA);
                }        
                if (NULL != pKeyName)
                    ExFreePoolWithTag(pKeyName, TAG_GENERIC_PAGED);                    
            }

        
            if(wcslen(DISK_NAME_PREFIX)*sizeof(WCHAR) == RtlCompareMemory(pBasicDisk->DiskName, DISK_NAME_PREFIX, wcslen(DISK_NAME_PREFIX)*sizeof(WCHAR)))
            {
                UNICODE_STRING   ustrDiskName;
                ustrDiskName.Buffer = pBasicDisk->DiskName + wcslen(DISK_NAME_PREFIX);
                ustrDiskName.MaximumLength = (USHORT)((MAX_DISKNAME_SIZE_IN_CHAR-wcslen(DISK_NAME_PREFIX))*sizeof(WCHAR));
                ustrDiskName.Length = (USHORT)((MAX_DISKNAME_SIZE_IN_CHAR-wcslen(DISK_NAME_PREFIX))*sizeof(WCHAR));
                RtlUnicodeStringToInteger(&ustrDiskName, 10, &pBasicDisk->ulDiskNumber );
            }

            
            InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DeviceIoControlMSCSDiskAcquire: Diskname is %.*S Disk Number is %d\n\n",
                                        MAX_DISKNAME_SIZE_IN_CHAR, pBasicDisk->DiskName, pBasicDisk->ulDiskNumber));
        } else {
            InMageFltLogError(DriverContext.ControlDevice, __LINE__, INFLTDRV_ERR_NO_GENERIC_PAGED_POOL, STATUS_SUCCESS);
            InVolDbgPrint(DL_ERROR, DM_CLUSTER, ("DeviceIoControlMSCSDiskAcquire: Failed to allocate memory from paged pool for Basic Disk %#x\n\n",
                          "DiskSignature %#x\n\n",pBasicDisk, ulDiskSignature));
        }
        
    }

#endif
    ASSERT(NULL != pBasicDisk);
    if (NULL != pBasicDisk) {
        KeAcquireSpinLock(&pBasicDisk->Lock, &OldIrql);

        pBasicDisk->ulFlags |= BD_FLAGS_CLUSTER_ONLINE;
        pBasicVolume = (PBASIC_VOLUME)pBasicDisk->DevList.Flink;
        while ((PLIST_ENTRY)pBasicVolume != &pBasicDisk->DevList) {
            PDEV_CONTEXT     pDevContext = pBasicVolume->pDevContext;

            if (pDevContext) {
                InitializeClusterDiskAcquireParams(pDevContext);

                InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DeviceIoControlMSCSDiskAcquire: DevContext is %p, DevNum=%s\n",
                                    pDevContext, pDevContext->chDevNum));
            } else {
#if (NTDDI_VERSION >= NTDDI_VISTA)
                if(cDiskInfo->ePartitionStyle == ecPartStyleMBR) {
                    InVolDbgPrint(DL_WARNING, DM_CLUSTER, ("DeviceIoControlMSCSDiskAcquire: DevContext is NULL, DiskSignature=%#x\n",
                                    cDiskInfo->ulDiskSignature));
                } else {
                //Can not print unicode_string at dispatch level
                }
#else
                InVolDbgPrint(DL_WARNING, DM_CLUSTER, ("DeviceIoControlMSCSDiskAcquire: DevContext is NULL, DiskSignature=%#x\n",
                                    ulDiskSignature));
#endif

            }

            pBasicVolume = (PBASIC_VOLUME)pBasicVolume->ListEntry.Flink;
        }

        KeReleaseSpinLock(&pBasicDisk->Lock, OldIrql);
        DereferenceBasicDisk(pBasicDisk);
    } else {
#if (NTDDI_VERSION >= NTDDI_VISTA)

        if(cDiskInfo->ePartitionStyle == ecPartStyleGPT) {
            UNICODE_STRING      GUIDString;
            if(STATUS_SUCCESS == RtlStringFromGUID(cDiskInfo->DiskId, &GUIDString)) {
            
                InVolDbgPrint(DL_WARNING, DM_CLUSTER, ("DeviceIoControlMSCSDiskAcquire: BasicDisk not found for DiskGUID %wZ\n",
                    GUIDString));        

                RtlFreeUnicodeString(&GUIDString);
            }
        } else {
            InVolDbgPrint(DL_WARNING, DM_CLUSTER, ("DeviceIoControlMSCSDiskAcquire: BasicDisk not found for DiskSignature = %#x\n",
                            cDiskInfo->ulDiskSignature));        

        }
        
#else
                
        InVolDbgPrint(DL_WARNING, DM_CLUSTER, ("DeviceIoControlMSCSDiskAcquire: BasicDisk not found for DiskSignature = %#x\n",
                            ulDiskSignature));        
#endif
    }

    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS DeviceIoControlMSCSDiskRelease(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    NTSTATUS            Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
#if (NTDDI_VERSION < NTDDI_VISTA)
    ULONG               ulDiskSignature;
#endif
    LIST_ENTRY          ListHead;
    PLIST_NODE          pListNode;
    PBASIC_DISK         pBasicDisk;
    PBASIC_VOLUME       pBasicVolume;
    PDEV_CONTEXT        pDevContext;

    UNREFERENCED_PARAMETER(DeviceObject);

    ASSERT(IOCTL_INMAGE_MSCS_DISK_RELEASE == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);
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
        pBasicDisk->ulFlags &= ~BD_FLAGS_CLUSTER_ONLINE;
        InitializeListHead(&ListHead);
        GetDevListForBasicDisk(pBasicDisk, &ListHead); 

        while (!IsListEmpty(&ListHead)) {
            pListNode = (PLIST_NODE)RemoveHeadList(&ListHead);
            ASSERT(NULL != pListNode);

            pBasicVolume = (PBASIC_VOLUME)pListNode->pData;
            ASSERT(NULL != pBasicVolume);

            pDevContext = pBasicVolume->pDevContext;
            if ((NULL != pDevContext) &&(!(pDevContext->ulFlags & DCF_FILTERING_STOPPED))) {
                InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DeviceIoControlMSCSDiskRelease: DevContext is %p, DevNum=%S DevBitmap is %p VolumeFlags is 0x%x\n",
                    pDevContext, pDevContext->wDevNum, pDevContext->pDevBitmap , pDevContext->ulFlags));

                if(pDevContext->ulFlags & DCF_CLUSTER_VOLUME_ONLINE){
                   
                    // By this time already we are in last chance mode as file system is unmounted.
                    // we write changes directly to volume and close the file.

                    if (pDevContext->ulFlags & DCF_CV_FS_UNMOUTNED) {
                        InVolDbgPrint( DL_INFO, DM_CLUSTER, ("File System is found unmounted at Release Disk!!!\n"));
                    } else {
                        InVolDbgPrint( DL_WARNING, DM_CLUSTER, ("File System is found mounted at Release Disk!!!\n"));
                    }

                    if (pDevContext->pDevBitmap) {
                        InMageFltSaveAllChanges(pDevContext, TRUE, NULL, true);
                        if (pDevContext->pDevBitmap->pBitmapAPI) {
                            BitmapPersistentValues BitmapPersistentValue(pDevContext);
                            pDevContext->pDevBitmap->pBitmapAPI->CommitBitmap(cleanShutdown, BitmapPersistentValue, NULL);
                        }
                    }

                    if (pDevContext->PnPNotificationEntry) {
                        IoUnregisterPlugPlayNotification(pDevContext->PnPNotificationEntry);
                        pDevContext->PnPNotificationEntry = NULL;
                    }
                    ASSERT(pDevContext->ChangeList.ulTotalChangesPending == 0);
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
DeviceIoControlMSCSGetOnlineDisks(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    NTSTATUS            Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);

    UNREFERENCED_PARAMETER(DeviceObject);

    ASSERT(IOCTL_INMAGE_MSCS_GET_ONLINE_DISKS == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);
    if ((NULL == Irp->AssociatedIrp.SystemBuffer) ||
        (sizeof (S_GET_DISKS) > IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength)) 
    {
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    S_GET_DISKS  *sDiskSignature = (S_GET_DISKS *) Irp->AssociatedIrp.SystemBuffer;
#ifdef VOLUME_NO_REBOOT_SUPPORT
    if (DriverContext.IsDispatchEntryPatched != true) {
        sDiskSignature->IsVolumeAddedByEnumeration = false;
        Status = STATUS_SUCCESS;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = sizeof (S_GET_DISKS) + sDiskSignature->DiskSignaturesInputArrSize * sizeof(ULONG);
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    } else {
        sDiskSignature->IsVolumeAddedByEnumeration = true;
    }

    sDiskSignature->DiskSignaturesOutputArrSize = GetOnlineDisksSignature(sDiskSignature);

    ASSERT(sDiskSignature->DiskSignaturesOutputArrSize <= sDiskSignature->DiskSignaturesInputArrSize);

    if (sDiskSignature->DiskSignaturesOutputArrSize > sDiskSignature->DiskSignaturesInputArrSize) 
    {
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }
    // Can there be duplicate entries? i.e. multiple BasicDisk structures having same Signature?
    // Not possible
#endif
    Status = STATUS_SUCCESS;
    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = sizeof (S_GET_DISKS) + sDiskSignature->DiskSignaturesInputArrSize * sizeof(ULONG);
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return Status;
}

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

                    ulDiskNameLength = (ULONG)((wcslen(REG_CLUSDISK_SIGNATURES_KEY) + 2) * sizeof(WCHAR));
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
                                    ustrDiskName.MaximumLength = (USHORT)((MAX_DISKNAME_SIZE_IN_CHAR-wcslen(DISK_NAME_PREFIX))*sizeof(WCHAR));
                                    ustrDiskName.Length = (USHORT)((MAX_DISKNAME_SIZE_IN_CHAR-wcslen(DISK_NAME_PREFIX))*sizeof(WCHAR));
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
#endif
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

    ASSERT(NULL != pBasicVolume->pDevContext);
    DereferenceDevContext(pBasicVolume->pDevContext);

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
    ASSERT(IsListEmpty(&pBasicDisk->DevList));

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

    InitializeListHead(&pBasicDisk->DevList);
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

PBASIC_VOLUME
AddVolumeToBasicDisk(PBASIC_DISK pBasicDisk, PDEV_CONTEXT DevContext)
{
    PBASIC_VOLUME    pBasicVolume;
    KIRQL           OldIrql;

    if (NULL == pBasicDisk)
        return NULL;

    pBasicVolume = DevContext->pBasicVolume;
    if (NULL == pBasicVolume) {
        pBasicVolume = AllocateBasicVolume();
        if(NULL == pBasicVolume)
            return NULL;
        DevContext->pBasicVolume = pBasicVolume;
        ReferenceBasicDisk(pBasicDisk);
        pBasicVolume->pDisk = pBasicDisk;
        RtlCopyMemory(pBasicVolume->GUID, DevContext->wDevID, (GUID_SIZE_IN_CHARS * sizeof(WCHAR)));
        pBasicVolume->GUID[GUID_SIZE_IN_CHARS] = L'\0';
        pBasicVolume->pDevContext = ReferenceDevContext(DevContext);

        pBasicVolume->ulFlags |= BV_FLAGS_INSERTED_IN_BASIC_DISK;

        // Add reference for adding to the list.
        ReferenceBasicVolume(pBasicVolume);

        KeAcquireSpinLock(&pBasicDisk->Lock, &OldIrql);
        InsertHeadList(&pBasicDisk->DevList, &pBasicVolume->ListEntry);
        KeReleaseSpinLock(&pBasicDisk->Lock, OldIrql);
    }

    return pBasicVolume;
}

VOID
GetDevListForBasicDisk(PBASIC_DISK pBasicDisk, PLIST_ENTRY ListHead)
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

        pBasicVolume = (PBASIC_VOLUME)pBasicDisk->DevList.Flink;
        while ((PLIST_ENTRY)pBasicVolume != &pBasicDisk->DevList) {

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


#ifdef VOLUME_CLUSTER_SUPPORT
VOID GetClusterInfoForThisVolume(PDEV_CONTEXT pDevContext)
{
    
    PBASIC_DISK         pBasicDisk = NULL;
    PBASIC_VOLUME       pBasicVolume = NULL;

    
#if (NTDDI_VERSION >= NTDDI_VISTA)
        
        
        if(pDevContext->PartitionStyle == ecPartStyleMBR) 
            pBasicDisk = GetBasicDiskFromDriverContext(pDevContext->ulDiskSignature);
        else
            pBasicDisk = GetBasicDiskFromDriverContext(pDevContext->DiskId);
if(pBasicDisk != NULL) {

        if(pDevContext->PartitionStyle == ecPartStyleMBR) 
            pBasicVolume = AddBasicVolumeToDriverContext(pDevContext->ulDiskSignature, pDevContext);
        else
            pBasicVolume = AddBasicVolumeToDriverContext(pDevContext->DiskId, pDevContext);
        
    
#else    
        pBasicDisk = GetBasicDiskFromDriverContext(0, pDevContext->ulDiskNumber);
    
    if(pBasicDisk != NULL) {

        pDevContext->ulDiskSignature = pBasicDisk->ulDiskSignature;
        pBasicVolume = AddBasicVolumeToDriverContext(pDevContext->ulDiskSignature, pDevContext);
        
#endif
        
        pBasicDisk->ulFlags |= BD_FLAGS_CLUSTER_ONLINE;
        pDevContext->ulFlags |= DCF_VOLUME_ON_BASIC_DISK;
        pDevContext->ulFlags |= DCF_VOLUME_ON_CLUS_DISK;
        pDevContext->ulFlags |= DCF_CLUSTER_VOLUME_ONLINE;
        StartFilteringDevice(pDevContext, false);
        DereferenceBasicDisk(pBasicDisk);
    }
}

#ifdef VOLUME_NO_REBOOT_SUPPORT
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

ULONG
GetOnlineDisksSignature(S_GET_DISKS  *sDiskSignature)
{
    LIST_ENTRY          DiskListHead;
    PLIST_NODE          pDiskListNode;
    PBASIC_DISK         pBasicDisk;
    ULONG index = 0;

    // Get all Basic Disks
    InitializeListHead(&DiskListHead);
    GetBasicDiskList(&DiskListHead);
    while (!IsListEmpty(&DiskListHead)) {
        pDiskListNode = (PLIST_NODE)RemoveHeadList(&DiskListHead);
        ASSERT(NULL != pDiskListNode);

        pBasicDisk = (PBASIC_DISK) pDiskListNode->pData;
        ASSERT(NULL != pBasicDisk);

        // Check if this BasicDisk is a cluster disk
        if (!(pBasicDisk->ulFlags & BD_FLAGS_IS_CLUSTER_RESOURCE) ||
                pBasicDisk->IsAccessible != 1) {
            DereferenceBasicDisk(pBasicDisk);
            DereferenceListNode(pDiskListNode);
            continue;
        }

        if (index < sDiskSignature->DiskSignaturesInputArrSize) {
            sDiskSignature->DiskSignatures[index] = pBasicDisk->ulDiskSignature;
        }

        index++;

        DereferenceBasicDisk(pBasicDisk);
        DereferenceListNode(pDiskListNode);
    }            

    return index;
}
#endif

extern "C"
void InitializeClusterDiskAcquireParams(PDEV_CONTEXT pDevContext)
{
    PDEV_BITMAP  pDevBitmap;

    pDevBitmap = pDevContext->pDevBitmap;
    pDevContext->pDevBitmap = NULL;

    if (pDevBitmap) {
        // This is the bitmap file in Raw IO mode from previous acquire.
        // Delete this bitmap file and create a new one.
        if (pDevBitmap->pDevContext) {
            DereferenceDevContext(pDevBitmap->pDevContext);
            pDevBitmap->pDevContext = NULL;
        }
        DereferenceDevBitmap(pDevBitmap);
    }
    StartFilteringDevice(pDevContext, true);
    pDevContext->ulFlags |= DCF_CLUSTER_VOLUME_ONLINE;
    pDevContext->ulFlags &= ~DCF_CV_FS_UNMOUTNED;
    pDevContext->ulFlags &= ~DCF_CLUSDSK_RETURNED_OFFLINE;
    pDevContext->liDisMountFailNotifyTimeStamp.QuadPart = 0;
    pDevContext->liDisMountNotifyTimeStamp.QuadPart = 0;
    pDevContext->liMountNotifyTimeStamp.QuadPart = 0;
    pDevContext->liFirstBitmapOpenErrorAtTickCount.QuadPart = 0;
    pDevContext->liLastBitmapOpenErrorAtTickCount.QuadPart = 0;
    pDevContext->lNumBitmapOpenErrors=0;
    pDevContext->lNumBitMapClearErrors=0;
    pDevContext->lNumBitMapReadErrors=0;
    pDevContext->lNumBitMapWriteErrors=0;
    //Making this flag as true so that it will update the globals again.
    pDevContext->bQueueChangesToTempQueue = true;
}

extern "C" PWCHAR
GetVolumeOverride(PDEV_CONTEXT pDevContext)
{
    Registry        *pDevParam = NULL;
    PWSTR           pVolumeOverride = NULL;
    PBASIC_VOLUME   pBasicVolume = NULL;
    NTSTATUS        Status;

    PAGED_CODE();

    // See if there is a volume override.
    Status = OpenDeviceParametersRegKey(pDevContext, &pDevParam);
    if (NULL != pDevParam) {
        Status = pDevParam->ReadWString(DEVICE_BITMAPLOG_PATH_NAME, STRING_LEN_NULL_TERMINATED, &pVolumeOverride);
        if (NT_SUCCESS(Status) && (NULL != pVolumeOverride)) {
            // We have volume override. Lets check to see if the volume is a cluster volume.
            pBasicVolume = pDevContext->pBasicVolume;
            
            if ((NULL != pBasicVolume) && (NULL != pBasicVolume->pDisk) &&
                (pBasicVolume->pDisk->ulFlags & BD_FLAGS_IS_CLUSTER_RESOURCE))
            {
                UNICODE_STRING  usClusterPrefix, usVolumeOverride;
                const unsigned long ulNumCharsInClusterBuffer = 150;
                WCHAR           ClusterPrefixBuffer[ulNumCharsInClusterBuffer];

                RtlInitUnicodeString(&usVolumeOverride, pVolumeOverride);

                InMageInitUnicodeString(&usClusterPrefix, ClusterPrefixBuffer, ulNumCharsInClusterBuffer*sizeof(WCHAR), 0);
                RtlAppendUnicodeToString(&usClusterPrefix, L"\\??\\Volume{");
                RtlAppendUnicodeToString(&usClusterPrefix, pDevContext->wDevID);
                RtlAppendUnicodeToString(&usClusterPrefix, L"}\\");
                if (FALSE == RtlPrefixUnicodeString(&usClusterPrefix, &usVolumeOverride, TRUE)) {
                    InMageInitUnicodeString(&usClusterPrefix, ClusterPrefixBuffer, ulNumCharsInClusterBuffer*sizeof(WCHAR), 0);
                    RtlAppendUnicodeToString(&usClusterPrefix, L"\\DosDevices\\Volume{");
                    RtlAppendUnicodeToString(&usClusterPrefix, pDevContext->wDevID);
                    RtlAppendUnicodeToString(&usClusterPrefix, L"}\\");
                    if (FALSE == RtlPrefixUnicodeString(&usClusterPrefix, &usVolumeOverride, TRUE)) {
                        if (pBasicVolume->ulFlags & BV_FLAGS_HAS_DRIVE_LETTER) {
                            // ClusterVolume has drive letter
                            if ((FALSE == IS_DRIVE_LETTER(pVolumeOverride[0])) ||
                                (L':' != pVolumeOverride[1]) || (pBasicVolume->DriveLetter != pVolumeOverride[0]))
                            {
                                // Volume override path is NOT on the same volume.
                                ExFreePoolWithTag(pVolumeOverride, TAG_REGISTRY_DATA);
                                pVolumeOverride = NULL;
                            }
                        } else {
                            // Volume does not have a drive letter, and drive letter independent path did not match
                            // Considering this path is not on the same volume.
                            ExFreePoolWithTag(pVolumeOverride, TAG_REGISTRY_DATA);
                            pVolumeOverride = NULL;
                        }
                    }
                }
            }
        } else {
            // Getting override has failed.
            if (NULL != pVolumeOverride) {
                ExFreePoolWithTag(pVolumeOverride, TAG_REGISTRY_DATA);
                pVolumeOverride = NULL;
            }
        }

        delete pDevParam;
    }

    return pVolumeOverride;
}

extern "C" NTSTATUS
UpdateGlobalWithPersistentValuesReadFromBitmap(BOOLEAN bDevInSync, BitmapPersistentValues &BitmapPersistentValue) 
{
    PDEV_CONTEXT pDevContext = BitmapPersistentValue.m_pDevContext;

    if (IS_CLUSTER_VOLUME(pDevContext) && pDevContext->bQueueChangesToTempQueue) {
        KIRQL   OldIrql;
        ULONGLONG TimeStamp = BitmapPersistentValue.GetGlobalTimeStampFromBitmap();
        ULONGLONG SequenceNumber = BitmapPersistentValue.GetGlobalSequenceNumberFromBitmap();

        InVolDbgPrint(DL_ERROR, DM_TIME_STAMP, ("%s: Time Stamp Read from Bitmap=%#I64x \n", __FUNCTION__,TimeStamp));
        InVolDbgPrint(DL_ERROR, DM_TIME_STAMP, ("%s: Seq Number Read from Bitmap=%#I64x \n", __FUNCTION__,SequenceNumber));

        if (!bDevInSync) { //system crashed or improper shutdown
            TimeStamp += TIME_STAMP_BUMP;
            SequenceNumber += SEQ_NO_BUMP;
        }

        InVolDbgPrint(DL_ERROR, DM_TIME_STAMP, ("%s: Time Stamp After Bump=%#I64x \n", __FUNCTION__,TimeStamp));
        InVolDbgPrint(DL_ERROR, DM_TIME_STAMP, ("%s: Seq Number After Bump=%#I64x \n", __FUNCTION__,SequenceNumber));

        KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
        KeAcquireSpinLockAtDpcLevel(&DriverContext.TimeStampLock);

        InVolDbgPrint(DL_ERROR, DM_TIME_STAMP, ("%s: Global Time Stamp Before Update=%#I64x \n", __FUNCTION__,DriverContext.ullLastTimeStamp));

        //Update the Global Time Stamp
        if (TimeStamp > DriverContext.ullLastTimeStamp) {
            DriverContext.ullLastTimeStamp = TimeStamp;
        } else if(TimeStamp == DriverContext.ullLastTimeStamp) {
            //As the time stamp is same increment the global time stamp by 1
            //It is done to resolve in the case of failover in the 32 bit cluster system 
            //where the time stamp is same and current sequence number running on this node
            //might be less than the sequence number running on the failing node.
            ++DriverContext.ullLastTimeStamp;
        }
        InVolDbgPrint(DL_ERROR, DM_TIME_STAMP, ("%s: Global Time Stamp After Update=%#I64x \n", __FUNCTION__,DriverContext.ullLastTimeStamp));

        InVolDbgPrint(DL_ERROR, DM_TIME_STAMP, ("%s: Global Seq Number before Update=%#I64x \n", __FUNCTION__,DriverContext.ullTimeStampSequenceNumber));
        //Update the Global Sequence Number
        if (SequenceNumber > DriverContext.ullTimeStampSequenceNumber) {
           DriverContext.ullTimeStampSequenceNumber = SequenceNumber;
        }
        InVolDbgPrint(DL_ERROR, DM_TIME_STAMP, ("%s: Global Seq Number After Update=%#I64x \n", __FUNCTION__,DriverContext.ullTimeStampSequenceNumber));

        KeReleaseSpinLockFromDpcLevel(&DriverContext.TimeStampLock);
        KeReleaseSpinLock(&pDevContext->Lock, OldIrql);

    }

    return STATUS_SUCCESS;
}

extern "C" NTSTATUS
DriverNotificationCallback(
    PVOID NotificationStructure,
    PVOID pContext
    )
{
    PTARGET_DEVICE_CUSTOM_NOTIFICATION  pNotify = (PTARGET_DEVICE_CUSTOM_NOTIFICATION)NotificationStructure;
    PDEV_CONTEXT                        pDevContext = (PDEV_CONTEXT)pContext;
    KIRQL                               OldIrql;
    PDEV_BITMAP                         pDevBitmap = NULL;

    if ((NULL == pDevContext) || (pDevContext->ulFlags & DCF_FILTERING_STOPPED))
        return STATUS_SUCCESS;

    // IsEqualGUID takes a reference to GUID (GUID &)
    if (IsEqualGUID(pNotify->Event, GUID_IO_VOLUME_DISMOUNT))
    {
        if (0 == (pDevContext->ulFlags & DCF_CV_FS_UNMOUTNED)) {
            LARGE_INTEGER   WaitTime;
            NTSTATUS Status = STATUS_SUCCESS;
            
            KeQuerySystemTime(&WaitTime);
            KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
            pDevContext->liDisMountNotifyTimeStamp.QuadPart = WaitTime.QuadPart;
            KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
            WaitTime.QuadPart += DriverContext.ulSecondsMaxWaitTimeOnFailOver * HUNDRED_NANO_SECS_IN_SEC;

            InVolDbgPrint( DL_INFO, DM_CLUSTER, ("Received DISMOUNT notification on volume %p\n", pDevContext));
            Status = InMageFltSaveAllChanges(pDevContext, TRUE, &WaitTime);
            if (pDevContext->pDevBitmap && pDevContext->pDevBitmap->pBitmapAPI) {
                pDevContext->pDevBitmap->pBitmapAPI->ChangeBitmapModeToRawIO();
            }
            InVolDbgPrint( DL_INFO, DM_CLUSTER, ("Return from Save All Changes. Status = %#x\n", Status));

            // File System is unmounted.
            pDevContext->ulFlags |= DCF_CV_FS_UNMOUTNED;

			InVolDbgPrint(DL_INFO, DM_CLUSTER, ("Dismount Notification: DevContext is %p, DevNum=%S DevBitmap is %p VolumeFlags is 0x%x\n",
				pDevContext, pDevContext->wDevNum, pDevContext->pDevBitmap , pDevContext->ulFlags));

        } else {
            InVolDbgPrint( DL_INFO, DM_CLUSTER, ("Received DISMOUNT notification on already unmounted volume %p\n", pDevContext));
        }
    } else if (IsEqualGUID(pNotify->Event, GUID_IO_VOLUME_DISMOUNT_FAILED)) {
        InVolDbgPrint( DL_INFO, DM_CLUSTER, ("Received DISMOUNT FAILED failed notification on volume %p\n", pDevContext));
        KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
        KeQuerySystemTime(&pDevContext->liDisMountFailNotifyTimeStamp);
        if (pDevContext->ulFlags & DCF_CV_FS_UNMOUTNED) {
            pDevContext->ulFlags &= ~DCF_CV_FS_UNMOUTNED;
            pDevBitmap = pDevContext->pDevBitmap;
            pDevContext->pDevBitmap = NULL;
        }
        KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
        if (NULL != pDevBitmap) {
            InVolDbgPrint( DL_INFO, DM_CLUSTER | DM_BITMAP_CLOSE,
                ("Closing bitmap file on DISMOUNT FAILED failed notification for volume %S(%S)\n",
                pDevBitmap->DevID, pDevBitmap->wDevNum));
            CloseBitmapFile(pDevBitmap, false, pDevContext, cleanShutdown, false);
        }

    } else if (IsEqualGUID(pNotify->Event, GUID_IO_VOLUME_MOUNT)) {
        InVolDbgPrint( DL_INFO, DM_CLUSTER, ("Received MOUNT notification on volume %p\n", pDevContext));
        KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
        KeQuerySystemTime(&pDevContext->liMountNotifyTimeStamp);
        if (pDevContext->ulFlags & DCF_CV_FS_UNMOUTNED) {
            pDevContext->ulFlags &= ~DCF_CV_FS_UNMOUTNED;
            pDevBitmap = pDevContext->pDevBitmap;
            pDevContext->pDevBitmap = NULL;
        }
        KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
        if (NULL != pDevBitmap) {
                InVolDbgPrint( DL_INFO, DM_CLUSTER | DM_BITMAP_CLOSE,
                ("Closing bitmap file on MOUNT notification for volume %S(%S)\n",
                pDevBitmap->DevID, pDevBitmap->wDevNum));
            CloseBitmapFile(pDevBitmap, false, pDevContext, cleanShutdown, false);
        }
    } else if (IsEqualGUID(pNotify->Event, GUID_IO_VOLUME_LOCK)) {
        InVolDbgPrint( DL_INFO, DM_CLUSTER, ("Received LOCK notification on volume %p\n", pDevContext));    
    } else if (IsEqualGUID(pNotify->Event, GUID_IO_VOLUME_LOCK_FAILED)) {
        InVolDbgPrint( DL_INFO, DM_CLUSTER, ("Received LOCK FAILED notification on volume %p\n", pDevContext));    
    } else if (IsEqualGUID(pNotify->Event, GUID_IO_VOLUME_UNLOCK)) {
        InVolDbgPrint( DL_INFO, DM_CLUSTER, ("Received LOCK notification on volume %p\n", pDevContext));    
    } else if (IsEqualGUID(pNotify->Event, GUID_IO_VOLUME_CHANGE)) {
        InVolDbgPrint( DL_INFO, DM_CLUSTER, ("Received VOLUME CHANGE notification on volume %p\n", pDevContext));    
    } else if (IsEqualGUID(pNotify->Event, GUID_IO_VOLUME_UNIQUE_ID_CHANGE)) {
        InVolDbgPrint( DL_INFO, DM_CLUSTER, ("Received GUID_IO_VOLUME_UNIQUE_ID_CHANGE notification on volume %p\n", pDevContext));    
    } else {
        InVolDbgPrint( DL_INFO, DM_CLUSTER, ("Recived unhandled notification on volume %p %x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x\n",
                pDevContext, pNotify->Event.Data1, pNotify->Event.Data2, pNotify->Event.Data3,
                pNotify->Event.Data4[0], pNotify->Event.Data4[1], pNotify->Event.Data4[2], pNotify->Event.Data4[3], 
                pNotify->Event.Data4[4], pNotify->Event.Data4[5], pNotify->Event.Data4[6], pNotify->Event.Data4[7])); 
    }

    return STATUS_SUCCESS;
}

NTSTATUS
InMageFltWriteOnOfflineVolumeCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp,
    IN PVOID          Context
    )
{
    NTSTATUS    Status = STATUS_SUCCESS;
	PIO_STACK_LOCATION irpStack;
#ifndef VOLUME_NO_REBOOT_SUPPORT
	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Context);
#else
    PIO_STACK_LOCATION IoNextStackLocation = IoGetNextIrpStackLocation(Irp);
    etContextType       eContextType = (etContextType)*((PCHAR )Context + sizeof(LIST_ENTRY));

    if (eContextType == ecCompletionContext) {
        PCOMPLETION_CONTEXT pCompletionContext = (PCOMPLETION_CONTEXT)Context;
        //irpStack = IoGetCurrentIrpStackLocation(Irp); // Cloned Irp
        
        //ASSERT(irpStack->MajorFunction == IRP_MJ_WRITE);

        InVolDbgPrint(DL_INFO, DM_CLUSTER, ("InMageFltWriteOnOfflineVolumeCompletion:  Info = %#x, Status = %#x\n",
                Irp->IoStatus.Information, Irp->IoStatus.Status ));
        if(pCompletionContext->CompletionRoutine) {
            IoNextStackLocation->CompletionRoutine = pCompletionContext->CompletionRoutine;
            IoNextStackLocation->Context = pCompletionContext->Context;
            Status = (*pCompletionContext->CompletionRoutine)(DeviceObject, Irp, pCompletionContext->Context);

        }
        ExFreePoolWithTag(pCompletionContext, TAG_GENERIC_NON_PAGED);

       
    } else {
#endif
        irpStack = IoGetCurrentIrpStackLocation(Irp);
    //
    // Update counters
    //

        ASSERT(irpStack->MajorFunction == IRP_MJ_WRITE);

        InVolDbgPrint(DL_INFO, DM_CLUSTER, ("InMageFltWriteOnOfflineVolumeCompletion: length=%#x, offset=%#I64x, Info = %#x, Status = %#x\n",
            irpStack->Parameters.Write.Length,
            irpStack->Parameters.Write.ByteOffset.QuadPart, Irp->IoStatus.Information, Irp->IoStatus.Status ));

        if (Irp->PendingReturned) {
            IoMarkIrpPending(Irp);
        }
#ifdef VOLUME_NO_REBOOT_SUPPORT
    }
#endif
   return Status;
} // InMageFltWriteOnOfflineVolumeCompletion
#endif
