#include "Common.h"
#include "MntMgr.h"
#include "BaseClass.h"
#include "Registry.h"
#include "VContext.h"
#include "VBitmap.h"
#include "misc.h"
#include "Ioctlinfo.h"
#include "FltFunc.h"


#include <initguid.h>
#include <guiddef.h>

#ifdef VOLUME_CLUSTER_SUPPORT
extern "C" void 
InitializeClusterDiskAcquireParams(PDEV_CONTEXT pDevContext);
#endif


NTSTATUS
AddVolumeLetterToRegVolumeCfg(PDEV_CONTEXT pDevContext);


NTSTATUS
SetBitRepresentingDriveLetter(WCHAR DriveLetter, PRTL_BITMAP pBitmap)
{
    ULONG   ulBitIndex;

    if (NULL  == pBitmap)
        return STATUS_INVALID_PARAMETER;

    if ((DriveLetter >= L'a') && (DriveLetter <= L'z')) {
        ulBitIndex = DriveLetter - L'a';
    } else if ((DriveLetter >= 'A') && (DriveLetter <= 'Z')) {
        ulBitIndex = DriveLetter - L'A';
    } else {
        return STATUS_INVALID_PARAMETER;
    }

    RtlSetBits(pBitmap, ulBitIndex, 0x01);

    return STATUS_SUCCESS;
}

NTSTATUS
ClearBitRepresentingDriveLetter(WCHAR DriveLetter, PRTL_BITMAP pBitmap)
{
    ULONG   ulBitIndex;

    if (NULL  == pBitmap)
        return STATUS_INVALID_PARAMETER;

    if ((DriveLetter >= L'a') && (DriveLetter <= L'z')) {
        ulBitIndex = DriveLetter - L'a';
    } else if ((DriveLetter >= 'A') && (DriveLetter <= 'Z')) {
        ulBitIndex = DriveLetter - L'A';
    } else {
        return STATUS_INVALID_PARAMETER;
    }

    RtlClearBits(pBitmap, ulBitIndex, 0x01);

    return STATUS_SUCCESS;
}

NTSTATUS
ConvertDriveLetterBitmapToDriveLetterString(PRTL_BITMAP pBitmap, PWCHAR pwString, ULONG ulStringSizeInBytes)
{
    ULONG       ulIndex, ulSizeUsed, ulStringSizeInCch;


    if ((NULL == pwString) || (0 == ulStringSizeInBytes))
        return STATUS_INVALID_PARAMETER;

    RtlZeroMemory(pwString, ulStringSizeInBytes);

    ulStringSizeInCch = ulStringSizeInBytes/sizeof(WCHAR);

    ulSizeUsed = 0;
    for (ulIndex = 0; ulIndex < MAX_NUM_DRIVE_LETTERS; ulIndex++) {
        if (RtlCheckBit(pBitmap, ulIndex)) {
            if (ulSizeUsed < ulStringSizeInCch) {
                pwString[ulSizeUsed] = (UCHAR)('A' + ulIndex);
                ulSizeUsed++;
            } else {
                return STATUS_BUFFER_TOO_SMALL;
            }
        }
    }

    return STATUS_SUCCESS;
}
PBASIC_DISK
GetBasicDiskFromDriverContext(ULONG ulDiskSignature, ULONG ulDiskNumber)
{
    KIRQL       OldIrql;
    PBASIC_DISK  pBasicDisk;
    BOOLEAN     bFound = FALSE;

    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    pBasicDisk = (PBASIC_DISK)DriverContext.HeadForBasicDisks.Flink;
    while(pBasicDisk != (PBASIC_DISK)&DriverContext.HeadForBasicDisks) {
        if (ulDiskSignature != 0) {
            if(pBasicDisk->ulDiskSignature == ulDiskSignature) {
                 bFound = TRUE;
                ReferenceBasicDisk(pBasicDisk);
                break;
            }
        } else if((pBasicDisk->ulDiskNumber != 0xfffffff) && (pBasicDisk->ulDiskNumber == ulDiskNumber)) {
            bFound = TRUE;
            ReferenceBasicDisk(pBasicDisk);
            break;
        }
        pBasicDisk = (PBASIC_DISK)pBasicDisk->ListEntry.Flink;
    }
    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

    if (FALSE == bFound)
        pBasicDisk = NULL;

    return pBasicDisk;
}

PBASIC_DISK
AddBasicDiskToDriverContext(ULONG ulDiskSignature)
{
    KIRQL       OldIrql;
    PBASIC_DISK  pBasicDisk;

    pBasicDisk = GetBasicDiskFromDriverContext(ulDiskSignature);
    if (NULL == pBasicDisk) {
        pBasicDisk = AllocateBasicDisk();
        pBasicDisk->ulDiskSignature = ulDiskSignature;
        ReferenceBasicDisk(pBasicDisk);
        KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
        InsertHeadList(&DriverContext.HeadForBasicDisks, (PLIST_ENTRY)pBasicDisk);
        KeReleaseSpinLock(&DriverContext.Lock, OldIrql);
    }

    return pBasicDisk;
}

PBASIC_VOLUME
AddBasicVolumeToDriverContext(ULONG ulDiskSignature, PDEV_CONTEXT DevContext)
{
    PBASIC_DISK      pBasicDisk;
    PBASIC_VOLUME    pBasicVolume;

    pBasicDisk = GetBasicDiskFromDriverContext(ulDiskSignature);
    if (NULL == pBasicDisk)
        return NULL;

    pBasicVolume = AddVolumeToBasicDisk(pBasicDisk, DevContext);

    return pBasicVolume;
}


#if (NTDDI_VERSION >= NTDDI_VISTA)

PBASIC_DISK
GetBasicDiskFromDriverContext(GUID  Guid)
{
    KIRQL       OldIrql;
    PBASIC_DISK  pBasicDisk;
    BOOLEAN     bFound = FALSE;

    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    pBasicDisk = (PBASIC_DISK)DriverContext.HeadForBasicDisks.Flink;
    while(pBasicDisk != (PBASIC_DISK)&DriverContext.HeadForBasicDisks) {
        if (sizeof(GUID) == RtlCompareMemory(&pBasicDisk->DiskId, &Guid, sizeof(GUID)) ) {
            bFound = TRUE;
            ReferenceBasicDisk(pBasicDisk);
            break;
        }
        pBasicDisk = (PBASIC_DISK)pBasicDisk->ListEntry.Flink;
    }
    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

    if (FALSE == bFound)
        pBasicDisk = NULL;

    return pBasicDisk;
}

PBASIC_DISK
AddBasicDiskToDriverContext(GUID  Guid)
{
    KIRQL       OldIrql;
    PBASIC_DISK  pBasicDisk;

    pBasicDisk = GetBasicDiskFromDriverContext(Guid);
    if (NULL == pBasicDisk) {
        pBasicDisk = AllocateBasicDisk();
        RtlCopyMemory(&pBasicDisk->DiskId, &Guid, sizeof(GUID));
        ReferenceBasicDisk(pBasicDisk);
        KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
        InsertHeadList(&DriverContext.HeadForBasicDisks, (PLIST_ENTRY)pBasicDisk);
        KeReleaseSpinLock(&DriverContext.Lock, OldIrql);
    }

    return pBasicDisk;
}


PBASIC_VOLUME
AddBasicVolumeToDriverContext(GUID  Guid, PDEV_CONTEXT DevContext)
{
    PBASIC_DISK      pBasicDisk = NULL;
    PBASIC_VOLUME    pBasicVolume = NULL;

    pBasicDisk = GetBasicDiskFromDriverContext(Guid);
    if (NULL == pBasicDisk)
        return NULL;

    pBasicVolume = AddVolumeToBasicDisk(pBasicDisk, DevContext);

    return pBasicVolume;
}
#endif

//*****************************************************************************
//* NAME:			InMageVerifyNewVolume  (to handle any Duplicity of volumes)
//*
//* DESCRIPTION:		On the presumption that we might miss a PNP surprise or
//*                 		remove event, we have to make sure that the same volume
//*                 		when pops up next time should be handled properly.
//*					
//*	PARAMETERS:	    	IN pDeviceObject, IN pDevContext
//*
//*	IRQL:			IRQL_PASSIVE_LEVEL.
//*
//*	RETURNS:		NOTHING
//*
//* NOTES:      		Since we are a PNP compliant driver we are supposed to get
//*                 		all PNP events without any fail. But as far as this bug is conce-
//*					rned it was reported a couple of times (in a sporadic manner) that
//*					volumes which already had protection "on" were coming up as new
//*					ones.This led to the presumption that we could have somehow
//*					missed the PNP remove notifications.
//*
//*                 		If the above said is true then it might lead to our protection
//*                 		pair entering a limbo state. Does this really happen because
//*                 		of a missed PNP SURPRISE or REMOVE? This could not be
//*                 		substantiated, but certain anecdotal evidence suggests something
//*                 		of that sort. Probably this bug could be a consequence of
//*                 		some other non-compliant driver above us. -Kartha. But
//*                 		anyway there is no harm in implementing this for the sake of
//*                 		an unfailing functionality.
//*  
//* Bugzilla id:  		25726
//*****************************************************************************

NTSTATUS InMageCrossCheckNewVolume(IN PDEVICE_OBJECT pDeviceObject,
							 IN PDEV_CONTEXT pDevContext,
							 IN PMOUNTDEV_NAME pMountDevName)
{

   NTSTATUS 			Status = STATUS_SUCCESS;
   PLIST_ENTRY         	pCurr = DriverContext.HeadForDevContext.Flink;
   PDEV_CONTEXT 		pVolContext = NULL;
   PDEVICE_EXTENSION   	pDeviceExtension = NULL;

	if((!pDeviceObject) || (!pDevContext) || (!pMountDevName)){

		InVolDbgPrint( DL_INFO, DM_DEVICE_STATE,
                ("InMageVerifyNewVolume: Invalid argument parameter passed\n"));
		Status = STATUS_INVALID_PARAMETER;
		goto CrossCheckDone;
	}

	//Note: For a given boot session the primary unique volume name would invariably remain same.

	if (!INMAGE_IS_MOUNTDEV_NAME_VOLUME_GUID(pMountDevName)) {

		InVolDbgPrint( DL_INFO, DM_DEVICE_STATE,
        ("InMageVerifyNewVolume: pMountDevName arg. passed isn't a valid volume GUID Name\n"));
		Status = STATUS_INVALID_PARAMETER;
		goto CrossCheckDone;
	}

	while (pCurr != &DriverContext.HeadForDevContext) {

		//crosscheck the new volume against each existing one.
		pVolContext = (PDEV_CONTEXT)pCurr;

		pCurr = pCurr->Flink;

		//when IRP_MN_SURPRISE_REMOVE is called we set DCF_SURPRISE_REMOVAL flag.
		//similarly when IRP_MN_REMOVE.. is called we make the FDO pointer in pDevContext NULL.
		//Aso in IRP_MN_REMOVE... we remove the DevContext list entry and deallocate the same.

		//We are not supposed to do anything where we got a surprise remove but didnot get a remove
		//device.
		if(DCF_SURPRISE_REMOVAL & pVolContext->ulFlags){continue;}

        //As per our records device is in offline state i.e. remove device was invoked on it.
		if(ecDSDBCdeviceOffline == pVolContext->eDSDBCdeviceState){continue;}

		if(0 == (DCF_DEVID_OBTAINED & pVolContext->ulFlags)){continue;}

		if(1 > pVolContext->UniqueVolumeName.Length){continue;}

        if(NULL == pVolContext->FilterDeviceObject){
			//Means Remove Device routine was called but somehow the volume context entry
			//still lingers. Nothing much to be done here.
		    continue;
        }

		//check if we are accessing the newly added volume context, if yes then continue.
		if(pDeviceObject == pVolContext->FilterDeviceObject){
			continue;
		}

        pDeviceExtension = (PDEVICE_EXTENSION) pVolContext->FilterDeviceObject->DeviceExtension;

		if(NULL == pDeviceExtension){
			//Owner of DeviceExtension is system.
		    continue;
        }

		if( (RemovePending == pDeviceExtension->DevicePnPState) || \
			(SurpriseRemovePending == pDeviceExtension->DevicePnPState)){
			//We have to log this error. This should not happen.
			InVolDbgPrint( DL_ERROR, DM_DEVICE_STATE,
             ("A PNP SURPRISE OR REMOVE notification was received but not acted upon for volume %wZ\n",
                &pVolContext->UniqueVolumeName));
		}

		//Have we already processed this volume context?
		if( (LimboState == pDeviceExtension->DevicePnPState)){
			continue;
		}

        //we are @ PASSIVE level. We could have used compare memory invariably, but by using
        //compare string we stand a better chance of not getting into erroneous mem size cases.
		if (0 == RtlCompareUnicodeString(
			&pDevContext->UniqueVolumeName, &pVolContext->UniqueVolumeName, FALSE)){

			//This means the new volume context as well as the old existing one corresponds to
			//one and the same volume. Let's act.

		    /*Heads-up: if we have missed a PNP surprise or PNP remove device (by any chance)
		         we should not use the corresponding stale volume context in any major function calls
		    	   thenceforth since it might involve the use of the pre-existed stale
		         device object still attached to the context, for instance: like passing an irp down the stack.
		         Remember that there is no valid devstack present for a device which went through
		         a PNP device removal with or without our notice.*/

            //Make sure that we don't process the same entry in the next call to this procedure.

			SET_NEW_PNP_STATE(pDeviceExtension, LimboState);//previous state auto. restored.

            //SetDevOutOfSync is called here.
			SetDevOutOfSync(pVolContext, ERROR_TO_REG_MISSED_PNP_VOLUME_REMOVE, Status, false);
            InVolDbgPrint(DL_ERROR, DM_DEVICE_STATE,
            ("InMageCrossCheckNewVolume: Setting volume %wZ out of sync due to missed remove PNP\n",
                        &pVolContext->UniqueVolumeName));
			if(pVolContext->pDevBitmap){
			  InVolDbgPrint( DL_INFO, DM_DEVICE_STATE | DM_BITMAP_CLOSE,
              ("InMageCrossCheckNewVolume: Closing bitmap file on stale volume %S(%S)\n",
              pVolContext->pDevBitmap->DevID, pVolContext->pDevBitmap->wDevNum));
			  CloseBitmapFile(pVolContext->pDevBitmap, false, pDevContext, cleanShutdown, false);
			}

			goto CrossCheckDone;
			
		}
		
	}


CrossCheckDone:
		
    return Status;
}

NTSTATUS
DeviceIoControlMountDevLinkCreated(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    PDEVICE_EXTENSION   DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS            Status;
    PMOUNTDEV_NAME      pMountDevName;
    PDEV_CONTEXT        pDevContext;
    UNICODE_STRING      VolumeNameWithGUID;
    WCHAR               VolumeGUID[GUID_SIZE_IN_CHARS + 1] = {0};
    pDevContext = DeviceExtension->pDevContext;

    ASSERT(DISPATCH_LEVEL > KeGetCurrentIrql());

    if (NULL == pDevContext) {
        IoSkipCurrentIrpStackLocation(Irp);
        Status = IoCallDriver(DeviceExtension->TargetDeviceObject, Irp);
        return Status;
    }

    // See if the mount manager is informing the GUID of the volume if so
    // save the GUID.
    pMountDevName = (PMOUNTDEV_NAME)Irp->AssociatedIrp.SystemBuffer;
    InVolDbgPrint(DL_VERBOSE, DM_CLUSTER, 
                  ("DeviceIoControlMountDevLinkCreated: %.*S\n", pMountDevName->NameLength / sizeof(WCHAR), pMountDevName->Name))
    // Minimum 0x60 for name and 0x02 for length.
    if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength >= 0x62)
    {
         if (INMAGE_IS_MOUNTDEV_NAME_VOLUME_GUID(pMountDevName)) {
            RtlCopyMemory(VolumeGUID, &pMountDevName->Name[GUID_START_INDEX_IN_MOUNTDEV_NAME], 
                        GUID_SIZE_IN_BYTES);
            ConvertGUIDtoLowerCase(VolumeGUID);
            if (pDevContext->ulFlags & DCF_DEVID_OBTAINED) {
                // GUID is already obtained.
                if (GUID_SIZE_IN_BYTES != RtlCompareMemory(VolumeGUID, pDevContext->wDevID, GUID_SIZE_IN_BYTES)) {
                    PDEVICE_ID    pDeviceID = NULL;
                    KIRQL           OldIrql;

                    InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DeviceIoControlMountDevLinkCreated: Found more than one GUID for volume\n%S\n%S\n",
                                                        pDevContext->wDevID, VolumeGUID));
                    KeAcquireSpinLock(&pDevContext->Lock, &OldIrql);
                    pDeviceID = pDevContext->GUIDList;
                    while(NULL != pDeviceID) {
                        if (GUID_SIZE_IN_BYTES == RtlCompareMemory(VolumeGUID, pDeviceID->GUID, GUID_SIZE_IN_BYTES)) {
                            InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DeviceIoControlMountDevLinkCreated: Found same GUID again\n"));
                            break;
                        }
                        pDeviceID = pDeviceID->NextGUID;
                    }

                    if (NULL == pDeviceID) {
                        pDeviceID = (PDEVICE_ID)ExAllocatePoolWithTag(NonPagedPool, sizeof(DEVICE_ID), TAG_GENERIC_NON_PAGED);
                        if (NULL != pDeviceID) {
                            InterlockedIncrement(&DriverContext.lAdditionalGUIDs);
                            pDevContext->lAdditionalGUIDs++;
                            RtlZeroMemory(pDeviceID, sizeof(DEVICE_ID));
                            RtlCopyMemory(pDeviceID->GUID, VolumeGUID, GUID_SIZE_IN_BYTES);
                            pDeviceID->NextGUID = pDevContext->GUIDList;
                            pDevContext->GUIDList = pDeviceID;
                        } else {
                            InMageFltLogError(pDevContext->FilterDeviceObject, __LINE__, INFLTDRV_ERR_NO_GENERIC_NPAGED_POOL, STATUS_SUCCESS,
                                    pDevContext);
                            InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DeviceIoControlMountDevLinkCreated: Failed to allocate VOLUMEGUID Struct\n"));
                        }
                    }
                    KeReleaseSpinLock(&pDevContext->Lock, OldIrql);
                } else {
                    InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DeviceIoControlMountDevLinkCreated: Found same GUID again %S\n", VolumeGUID));
                }
            } else {
                // Copy VolumeDeviceName to DevContext
                RtlCopyMemory(pDevContext->BufferForUniqueVolumeName, 
                                pMountDevName->Name, 
                                MIN_UNIQUE_VOLUME_NAME_SIZE_IN_WCHARS * sizeof(WCHAR));
                pDevContext->BufferForUniqueVolumeName[MIN_UNIQUE_VOLUME_NAME_SIZE_IN_WCHARS] = '\0';
                pDevContext->UniqueVolumeName.Length = MIN_UNIQUE_VOLUME_NAME_SIZE_IN_WCHARS * sizeof(WCHAR);

				RtlCopyMemory(pDevContext->wDevID, VolumeGUID, GUID_SIZE_IN_BYTES);
                ASSERT (0 != pDevContext->DevParameters.MaximumLength);

                InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DeviceIoControlMountDevLinkCreated: DevContext is %p, GUID=%S\n",
                                    pDevContext, pDevContext->wDevID));

                RtlCopyUnicodeString(&pDevContext->DevParameters, &DriverContext.DriverParameters);
                VolumeNameWithGUID.MaximumLength = VolumeNameWithGUID.Length = VOLUME_NAME_SIZE_IN_BYTES;
                VolumeNameWithGUID.Buffer = &pMountDevName->Name[VOLUME_NAME_START_INDEX_IN_MOUNTDEV_NAME];
                RtlAppendUnicodeStringToString(&pDevContext->DevParameters, &VolumeNameWithGUID);
                pDevContext->ulFlags |= DCF_DEVID_OBTAINED;

				//Fix for bug 25726
				Status = InMageCrossCheckNewVolume(DeviceObject, pDevContext, pMountDevName);

                Status = LoadDeviceConfigFromRegistry(pDevContext);

                if (pDevContext->ulFlags & DCF_VOLUME_ON_BASIC_DISK) {
                    PBASIC_VOLUME    pBasicVolume;

                    InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DeviceIoControlMountDevLinkCreated: Volume(%S) is on BasicDisk\n",
                                        pDevContext->wDevID));

                    // Volume is on basic disk.
                    pBasicVolume = AddBasicVolumeToDriverContext(pDevContext->ulDiskSignature, pDevContext);

                    if (NULL != pBasicVolume) {
                        ASSERT(NULL != pBasicVolume->pDisk);
#ifdef VOLUME_CLUSTER_SUPPORT
                        if ((NULL != pBasicVolume->pDisk) && (pBasicVolume->pDisk->ulFlags & BD_FLAGS_CLUSTER_ONLINE)) {
                            InitializeClusterDiskAcquireParams(pDevContext);
                        }
#endif
                        if (pDevContext->ulFlags & DCF_VOLUME_LETTER_OBTAINED) {
                            pBasicVolume->BuferForDriveLetterBitMap = pDevContext->BufferForDriveLetterBitMap;
                            pBasicVolume->DriveLetter = pDevContext->wDevNum[0];
                            pBasicVolume->ulFlags |= BV_FLAGS_HAS_DRIVE_LETTER;
                        }

                    }
                }
            }  // if (pDevContext->ulFlags & DCF_DEVID_OBTAINED) else ...
        }
    } else {
        // 0x1C for Drive letter and 0x02 for length
        // \DosDevices\X:
        if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength >= 0x1E) {
            if (INMAGE_IS_MOUNTDEV_NAME_VOLUME_LETTER(pMountDevName)) {
                SetBitRepresentingDriveLetter(pMountDevName->Name[VOLUME_LETTER_START_INDEX_IN_MOUNTDEV_NAME], 
                                              &pDevContext->DriveLetterBitMap);
                pDevContext->ulFlags |= DCF_VOLUME_LETTER_OBTAINED;
                pDevContext->wDevNum[0] = pMountDevName->Name[VOLUME_LETTER_START_INDEX_IN_MOUNTDEV_NAME];
#if DBG
                pDevContext->chDevNum[0] = (CHAR)pMountDevName->Name[VOLUME_LETTER_START_INDEX_IN_MOUNTDEV_NAME];
#endif
                InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DeviceIoControlMountDevLinkCreated: DevContext is %p, DevNum=%S\n",
                                    pDevContext, pDevContext->wDevNum));
              
                if (pDevContext->ulFlags & DCF_DEVID_OBTAINED) {
                    AddVolumeLetterToRegVolumeCfg(pDevContext);

                    if (pDevContext->pBasicVolume) {
                        pDevContext->pBasicVolume->BuferForDriveLetterBitMap = pDevContext->BufferForDriveLetterBitMap;
                        pDevContext->pBasicVolume->DriveLetter = pDevContext->wDevNum[0];
                        pDevContext->pBasicVolume->ulFlags |= BV_FLAGS_HAS_DRIVE_LETTER;
                    }
                }
            }
        }
    }

    
#ifdef VOLUME_NO_REBOOT_SUPPORT
    if(!pDevContext->IsDevUsingAddDevice && DriverContext.IsDispatchEntryPatched) {
#ifdef VOLUME_CLUSTER_SUPPORT
        if(pDevContext->IsVolumeCluster) {
            Status = (*DriverContext.MajorFunctionForClusDrives[IRP_MJ_DEVICE_CONTROL])(DeviceExtension->TargetDeviceObject, Irp);
        } else {
#endif
            Status = (*DriverContext.MajorFunction[IRP_MJ_DEVICE_CONTROL])(DeviceExtension->TargetDeviceObject, Irp);
#ifdef VOLUME_CLUSTER_SUPPORT
        }
#endif
    } else {
#endif
        IoSkipCurrentIrpStackLocation(Irp);
        Status = IoCallDriver(DeviceExtension->TargetDeviceObject, Irp);
#ifdef VOLUME_NO_REBOOT_SUPPORT
    }
#endif
    return Status;
}

NTSTATUS
DeviceIoControlMountDevLinkDeleted(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    PDEVICE_EXTENSION   DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ulIoControlCode;
    NTSTATUS            Status;
    PMOUNTDEV_NAME      pMountDevName;
    PDEV_CONTEXT        pDevContext;
 
    pDevContext = DeviceExtension->pDevContext;
    ulIoControlCode = IoStackLocation->Parameters.DeviceIoControl.IoControlCode;

    ASSERT(DISPATCH_LEVEL > KeGetCurrentIrql());
    ASSERT((IOCTL_MOUNTDEV_LINK_DELETED_WNET_AND_ABOVE == ulIoControlCode) || (IOCTL_MOUNTDEV_LINK_DELETED_WXP_AND_BELOW == ulIoControlCode));

    if (NULL == pDevContext) {
        IoSkipCurrentIrpStackLocation(Irp);
        Status = IoCallDriver(DeviceExtension->TargetDeviceObject, Irp);
        return Status;
    }

    // See if the mount manager is informing the GUID of the volume if so
    // save the GUID.
    pMountDevName = (PMOUNTDEV_NAME)Irp->AssociatedIrp.SystemBuffer;
    InVolDbgPrint(DL_VERBOSE, DM_CLUSTER, 
                  ("DeviceIoControlMountDevLinkDeleted: %.*S\n", pMountDevName->NameLength / sizeof(WCHAR), pMountDevName->Name))

    if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength >= 0x1E) {
        if (INMAGE_IS_MOUNTDEV_NAME_VOLUME_LETTER(pMountDevName)) {
            ClearBitRepresentingDriveLetter(pMountDevName->Name[VOLUME_LETTER_START_INDEX_IN_MOUNTDEV_NAME], 
                                            &pDevContext->DriveLetterBitMap);
            if (0 == pDevContext->BufferForDriveLetterBitMap)
            {
                pDevContext->ulFlags &= ~DCF_VOLUME_LETTER_OBTAINED;
            }
        }
    }
#ifdef VOLUME_NO_REBOOT_SUPPORT
    if(!pDevContext->IsDevUsingAddDevice && DriverContext.IsDispatchEntryPatched) {
#ifdef VOLUME_CLUSTER_SUPPORT
        if(pDevContext->IsVolumeCluster) {
            Status = (*DriverContext.MajorFunctionForClusDrives[IRP_MJ_DEVICE_CONTROL])(DeviceExtension->TargetDeviceObject, Irp);
        } else {
#endif
            Status = (*DriverContext.MajorFunction[IRP_MJ_DEVICE_CONTROL])(DeviceExtension->TargetDeviceObject, Irp);
#ifdef VOLUME_CLUSTER_SUPPORT
        }
#endif
    } else {    
#endif
        IoSkipCurrentIrpStackLocation(Irp);
        Status = IoCallDriver(DeviceExtension->TargetDeviceObject, Irp);
#ifdef VOLUME_NO_REBOOT_SUPPORT
    }
#endif
    return Status;
}

NTSTATUS
DeviceIoControlMountDevQueryUniqueID(
    PDEVICE_OBJECT  DeviceObject, 
    PIRP            Irp
    )
{
    PDEVICE_EXTENSION   DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    NTSTATUS            Status;
    PDEV_CONTEXT        pDevContext;
    PMOUNTDEV_UNIQUE_ID pMountDevUniqueID = NULL;
    
    
    ASSERT(DISPATCH_LEVEL > KeGetCurrentIrql());
 
    pDevContext = DeviceExtension->pDevContext;
    if (NULL == pDevContext) {
        InVolDbgPrint(DL_ERROR, DM_CLUSTER, ("DeviceIoControlMountDevQueryUniqueID: DevContext is NULL\n"));
        IoSkipCurrentIrpStackLocation(Irp);
        Status = IoCallDriver(DeviceExtension->TargetDeviceObject, Irp);
        return Status;
    }

    Status = InMageFltForwardIrpSynchronous(DeviceObject, Irp);

    // IOCTL is method buffered. So dump the info being returned.
    if (Irp->IoStatus.Information >= 2) {
        pMountDevUniqueID  = (PMOUNTDEV_UNIQUE_ID)Irp->AssociatedIrp.SystemBuffer;
        if ((NULL != pMountDevUniqueID) && (pMountDevUniqueID->UniqueIdLength == 12) &&
            (Irp->IoStatus.Information >= (ULONG)(FIELD_OFFSET(MOUNTDEV_UNIQUE_ID, UniqueId) + pMountDevUniqueID->UniqueIdLength)))
        {
            ULONG           ulDiskSignature;
            PBASIC_DISK     pBasicDisk;

            if (pDevContext->ulFlags & DCF_DEVID_OBTAINED) {
                InVolDbgPrint(DL_INFO, DM_CLUSTER, ("%.*S DeviceObject = %p, ", GUID_SIZE_IN_CHARS, pDevContext->wDevID, DeviceObject));
            } else {
                InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DeviceObject = %p, ", DeviceObject));
            }

            ulDiskSignature = *(ULONG UNALIGNED *)pMountDevUniqueID->UniqueId;
            for (int i = 0; i < pMountDevUniqueID->UniqueIdLength; i++) {
                InVolDbgPrint(DL_INFO, DM_CLUSTER, ("0x%x ", pMountDevUniqueID->UniqueId[i]));
            }
            InVolDbgPrint(DL_INFO, DM_CLUSTER, ("\n"));
            InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DiskSignature = 0x%x\n", ulDiskSignature));

            pDevContext->ulDiskSignature = ulDiskSignature;
            pDevContext->ulFlags |= DCF_VOLUME_ON_BASIC_DISK;

            pBasicDisk = AddBasicDiskToDriverContext(ulDiskSignature);
            if (NULL != pBasicDisk) {
#ifdef VOLUME_CLUSTER_SUPPORT
                if (pBasicDisk->ulFlags & BD_FLAGS_IS_CLUSTER_RESOURCE) {
                    pDevContext->ulFlags |= DCF_VOLUME_ON_CLUS_DISK;
                } else {
#endif
                    pDevContext->bQueueChangesToTempQueue = false;
#ifdef VOLUME_CLUSTER_SUPPORT
                }
#endif
                DereferenceBasicDisk(pBasicDisk);
            }
        }
        if ((NULL != pMountDevUniqueID) && (pMountDevUniqueID->UniqueIdLength == 24) &&
            (Irp->IoStatus.Information >= (ULONG)(FIELD_OFFSET(MOUNTDEV_UNIQUE_ID, UniqueId) + pMountDevUniqueID->UniqueIdLength)))
        {
            
            PBASIC_DISK     pBasicDisk = NULL;

            pBasicDisk = GetBasicDiskFromDriverContext(0, pDevContext->ulDiskNumber);

            if(pBasicDisk == NULL) {//for GPT non cluster volumes and dynamic disk volumes
                pDevContext->bQueueChangesToTempQueue = false;
            } else {
                DereferenceBasicDisk(pBasicDisk);
            }

            InVolDbgPrint(DL_INFO, DM_CLUSTER, ("DeviceIoControlMountDevQueryUniqueID: Unique Id length is %d\n", pMountDevUniqueID->UniqueIdLength));
            
            
        }
    }

    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return Status;
}


/*-----------------------------------------------------------------------------
 * Function : AddVolumeLetterToRegVolumeCfg
 *
 * Args -
 *  pDevContext - Points to volume for which drive letter has to be added.
 *
 * Notes 
 *  This function uses DevParameters field in DEV_CONTEXT. VolumeParamets
 *  specifies the registry key under which the volume configuration is stored.
 *  
 *  As this function does registry operations, IRQL should be < DISPATCH_LEVEL
 *-----------------------------------------------------------------------------
 */
NTSTATUS
AddVolumeLetterToRegVolumeCfg(PDEV_CONTEXT pDevContext)
{
    NTSTATUS    Status = STATUS_SUCCESS;
    Registry    *pDevParam = NULL;
    WCHAR       DriveLetters[MAX_NUM_DRIVE_LETTERS + 1] = {0};

    ASSERT((NULL != pDevContext) && 
            (pDevContext->ulFlags & DCF_DEVID_OBTAINED) &&
            (pDevContext->ulFlags & DCF_VOLUME_LETTER_OBTAINED));
    PAGED_CODE();

    InVolDbgPrint(DL_FUNC_TRACE, DM_DEV_CONTEXT, 
        ("AddVolumeLetterToRegVolumeCfg: Called - DevContext(%p)\n", pDevContext));

    if (NULL == pDevContext) {
        InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT, ("AddVolumeLetterToRegVolumeCfg: pDevContext NULL\n"));
        return STATUS_INVALID_PARAMETER_1;
    }

    if (!(pDevContext->ulFlags & DCF_DEVID_OBTAINED)) {
        InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT, ("AddVolumeLetterToRegVolumeCfg: Called before GUID is obtained\n"));
        return STATUS_INVALID_PARAMETER_1;
    }

    if (!(pDevContext->ulFlags & DCF_VOLUME_LETTER_OBTAINED)) {
        InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT, ("AddVolumeLetterToRegVolumeCfg: Called before volume letter is obtained\n"));
        return STATUS_INVALID_PARAMETER_1;
    }

    Status = ConvertDriveLetterBitmapToDriveLetterString(&pDevContext->DriveLetterBitMap, 
                                                DriveLetters,
                                                (MAX_NUM_DRIVE_LETTERS + 1)*sizeof(WCHAR));
    if (!NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT, 
            ("AddVolumeLetterToRegVolumeCfg: ConvertDriveLetterBitmapToDriveLetterString Failed with Status 0x%x\n",
                                    Status));
        return STATUS_INVALID_PARAMETER_1;
    }
    // Let us open/create the registry key for this volume.
    Status = OpenDeviceParametersRegKey(pDevContext, &pDevParam);
    if (NULL == pDevParam) {
        InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT, 
                ("AddVolumeLetterToRegVolumeCfg: Failed to open volume parameters key"));
        return Status;
    }    
    
    Status = pDevParam->WriteWString(VOLUME_LETTER, DriveLetters);
    if (NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_INFO, DM_DEV_CONTEXT, 
            ("AddVolumeLetterToRegVolumeCfg: Added DriveLetter(s) %S for GUID %S\n",
                                    DriveLetters, pDevContext->wDevID));
    } else {
        InVolDbgPrint(DL_ERROR, DM_DEV_CONTEXT, 
            ("AddVolumeLetterToRegVolumeCfg: Adding DriveLetter(s) %S for GUID %S Failed with Status 0x%x\n",
                                    DriveLetters, pDevContext->wDevID, Status));
    }

    delete pDevParam;
    
    return STATUS_SUCCESS;
}
