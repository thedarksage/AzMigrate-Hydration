#include "Common.h"
#include "MntMgr.h"
#include "global.h"
#include "BaseClass.h"
#include "Registry.h"
#include "VContext.h"
#include "VBitmap.h"
#include "misc.h"

#include <initguid.h>
#include <guiddef.h>


extern "C" NTSTATUS
DriverNotificationCallback(
    PVOID NotificationStructure,
    PVOID pContext
    )
{
    PTARGET_DEVICE_CUSTOM_NOTIFICATION  pNotify = (PTARGET_DEVICE_CUSTOM_NOTIFICATION)NotificationStructure;
    PVOLUME_CONTEXT                     pVolumeContext = (PVOLUME_CONTEXT)pContext;
    KIRQL                               OldIrql;
    PVOLUME_BITMAP                      pVolumeBitmap = NULL;

    if ((NULL == pVolumeContext) || (pVolumeContext->ulFlags & VCF_FILTERING_STOPPED))
        return STATUS_SUCCESS;

    // IsEqualGUID takes a reference to GUID (GUID &)
    if (IsEqualGUID(pNotify->Event, GUID_IO_VOLUME_DISMOUNT))
    {
        if (0 == (pVolumeContext->ulFlags & VCF_CV_FS_UNMOUTNED)) {
            LARGE_INTEGER   WaitTime;
            NTSTATUS Status = STATUS_SUCCESS;
            
            KeQuerySystemTime(&WaitTime);
            KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
            pVolumeContext->liDisMountNotifyTimeStamp.QuadPart = WaitTime.QuadPart;
            KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);
            WaitTime.QuadPart += DriverContext.ulSecondsMaxWaitTimeOnFailOver * HUNDRED_NANO_SECS_IN_SEC;

            InVolDbgPrint( DL_INFO, DM_CLUSTER, ("Received DISMOUNT notification on volume %p\n", pVolumeContext));
            Status = InMageFltSaveAllChanges(pVolumeContext, TRUE, &WaitTime);
            if (pVolumeContext->pVolumeBitmap && pVolumeContext->pVolumeBitmap->pBitmapAPI) {
                pVolumeContext->pVolumeBitmap->pBitmapAPI->ChangeBitmapModeToRawIO();
            }
            InVolDbgPrint( DL_INFO, DM_CLUSTER, ("Return from Save All Changes. Status = %#x\n", Status));

            // File System is unmounted.
            pVolumeContext->ulFlags |= VCF_CV_FS_UNMOUTNED;

			InVolDbgPrint(DL_INFO, DM_CLUSTER, ("Dismount Notification: VolumeContext is %p, DriveLetter=%S VolumeBitmap is %p VolumeFlags is 0x%x\n",
				pVolumeContext, pVolumeContext->wDriveLetter, pVolumeContext->pVolumeBitmap , pVolumeContext->ulFlags));

        } else {
            InVolDbgPrint( DL_INFO, DM_CLUSTER, ("Received DISMOUNT notification on already unmounted volume %p\n", pVolumeContext));
        }
    } else if (IsEqualGUID(pNotify->Event, GUID_IO_VOLUME_DISMOUNT_FAILED)) {
        InVolDbgPrint( DL_INFO, DM_CLUSTER, ("Received DISMOUNT FAILED failed notification on volume %p\n", pVolumeContext));
        KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
        KeQuerySystemTime(&pVolumeContext->liDisMountFailNotifyTimeStamp);
        if (pVolumeContext->ulFlags & VCF_CV_FS_UNMOUTNED) {
            pVolumeContext->ulFlags &= ~VCF_CV_FS_UNMOUTNED;
            pVolumeBitmap = pVolumeContext->pVolumeBitmap;
            pVolumeContext->pVolumeBitmap = NULL;
        }
        KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);
        if (NULL != pVolumeBitmap) {
            BitmapPersistentValues BitmapPersistentValue(pVolumeContext);
            InVolDbgPrint( DL_INFO, DM_CLUSTER | DM_BITMAP_CLOSE,
                ("Closing bitmap file on DISMOUNT FAILED failed notification for volume %S(%S)\n",
                pVolumeBitmap->VolumeGUID, pVolumeBitmap->VolumeLetter));
            CloseBitmapFile(pVolumeBitmap, false, BitmapPersistentValue, false);
        }

    } else if (IsEqualGUID(pNotify->Event, GUID_IO_VOLUME_MOUNT)) {
        InVolDbgPrint( DL_INFO, DM_CLUSTER, ("Received MOUNT notification on volume %p\n", pVolumeContext));
        KeAcquireSpinLock(&pVolumeContext->Lock, &OldIrql);
        KeQuerySystemTime(&pVolumeContext->liMountNotifyTimeStamp);
        if (pVolumeContext->ulFlags & VCF_CV_FS_UNMOUTNED) {
            pVolumeContext->ulFlags &= ~VCF_CV_FS_UNMOUTNED;
            pVolumeBitmap = pVolumeContext->pVolumeBitmap;
            pVolumeContext->pVolumeBitmap = NULL;
        }
        KeReleaseSpinLock(&pVolumeContext->Lock, OldIrql);
        if (NULL != pVolumeBitmap) {
            BitmapPersistentValues BitmapPersistentValue(pVolumeContext);
            InVolDbgPrint( DL_INFO, DM_CLUSTER | DM_BITMAP_CLOSE,
                ("Closing bitmap file on MOUNT notification for volume %S(%S)\n",
                pVolumeBitmap->VolumeGUID, pVolumeBitmap->VolumeLetter));
            CloseBitmapFile(pVolumeBitmap, false, BitmapPersistentValue, false);
        }
    } else if (IsEqualGUID(pNotify->Event, GUID_IO_VOLUME_LOCK)) {
        InVolDbgPrint( DL_INFO, DM_CLUSTER, ("Received LOCK notification on volume %p\n", pVolumeContext));    
    } else if (IsEqualGUID(pNotify->Event, GUID_IO_VOLUME_LOCK_FAILED)) {
        InVolDbgPrint( DL_INFO, DM_CLUSTER, ("Received LOCK FAILED notification on volume %p\n", pVolumeContext));    
    } else if (IsEqualGUID(pNotify->Event, GUID_IO_VOLUME_UNLOCK)) {
        InVolDbgPrint( DL_INFO, DM_CLUSTER, ("Received LOCK notification on volume %p\n", pVolumeContext));    
    } else if (IsEqualGUID(pNotify->Event, GUID_IO_VOLUME_CHANGE)) {
        InVolDbgPrint( DL_INFO, DM_CLUSTER, ("Received VOLUME CHANGE notification on volume %p\n", pVolumeContext));    
    } else if (IsEqualGUID(pNotify->Event, GUID_IO_VOLUME_UNIQUE_ID_CHANGE)) {
        InVolDbgPrint( DL_INFO, DM_CLUSTER, ("Received GUID_IO_VOLUME_UNIQUE_ID_CHANGE notification on volume %p\n", pVolumeContext));    
    } else {
        InVolDbgPrint( DL_INFO, DM_CLUSTER, ("Recived unhandled notification on volume %p %x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x\n",
                pVolumeContext, pNotify->Event.Data1, pNotify->Event.Data2, pNotify->Event.Data3,
                pNotify->Event.Data4[0], pNotify->Event.Data4[1], pNotify->Event.Data4[2], pNotify->Event.Data4[3], 
                pNotify->Event.Data4[4], pNotify->Event.Data4[5], pNotify->Event.Data4[6], pNotify->Event.Data4[7])); 
    }

    return STATUS_SUCCESS;
}

VOID
ConvertGUIDtoLowerCase(PWCHAR   pGUID)
{
    int i;

    for (i = 0; i < GUID_SIZE_IN_CHARS; i++) {
        if ((pGUID[i] >= 'A') && (pGUID[i] <= 'Z'))
            pGUID[i] += 'a' - 'A';
    }

    return;
}

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
    NTSTATUS    Status;
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