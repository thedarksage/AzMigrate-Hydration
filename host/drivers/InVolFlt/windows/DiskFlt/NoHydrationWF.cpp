
#include <initguid.h>

#include "FltFunc.h"

#include <ioevent.h>
#include "IoctlInfo.h"
#include "ListNode.h"
#include "misc.h"
#include "VBitmap.h"
#include "MetaDataMode.h"
#include "svdparse.h"
#include "HelperFunctions.h"
#include "VContext.h"
#include <version.h>

#ifndef GUID_NULL
DEFINE_GUID(GUID_NULL, 0L, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
#endif


VOID ResourceDiskDetectionWorkItem(
    _In_     PDEVICE_OBJECT DeviceObject,
    _In_opt_ PVOID          Context
    )
    /*++

    Routine Description:
    This routine is for workitem to detect resource disk in no-hydration workflow.

    No-Hydration is detected by driver as follows
    Boot Device
    has switched from scsi/sas to atapi
    previous vendor was vmware

    On Azure we expect a resource disk. This resource disk comes first during enumeration. If storvsc is not boot driver
    on failover, resource disk occupies driver letter D: This breaks source drive letter mapping.

    To avoid this scenario we need to make sure resource disk is offline on failover.

    To detect resource disk following is WF
    it is on atapi bus
    It doesn't appear on source
    It is the only data disk on atapi bus

    As resource disk can be enumerated before boot disk, we do open a work item.
    Once boot disk is detected, failover is marked.
    Post failover resource disk is detected.

    Arguments:
    DeviceObject    - Supplies the device object.
    Irp             - Supplies the IO request packet.

    Return Value:
    None

    --*/
{
    PDEVICE_EXTENSION       pDeviceExtension = NULL;
    PDEV_CONTEXT            pDevContext = NULL;
    LARGE_INTEGER           liTimeout;
    NTSTATUS                Status = STATUS_SUCCESS;
    PIRP                    Irp = (PIRP)Context;
    KIRQL                   oldIrql;

    liTimeout.QuadPart = MILLISECONDS_RELATIVE(DriverContext.ulMaxResDiskDetectionTimeInMS);

    pDeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;

    pDevContext = pDeviceExtension->pDevContext;
    InDskFltWriteEvent(INDSKFLT_INFO_WAITING_FOR_FO_DETECTION);

    Status = KeWaitForSingleObject(&DriverContext.hFailoverDetectionOver, Executive, KernelMode, FALSE, &liTimeout);
    if (STATUS_SUCCESS == Status) {
        if (DriverContext.isFailoverDetected) {
            InDskFltDetectResourceDiskOnFOVM(pDevContext);
        }
    }
    else {
        InDskFltWriteEvent(INDSKFLT_INFO_FO_DETECTION_FAILED, Status);
    }

    if (pDevContext->isResourceDisk) {
        InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_RESOURCE_DISK, pDevContext);
        InDskFltUpdateSanPolicy(pDevContext, SAN_ATTRIB_NAME, VDS_SAN_POLICY::VDS_SP_OFFLINE);
    }

    if (Irp->PendingReturned) {
        IoMarkIrpPending(Irp);
    }

    Irp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    KeAcquireSpinLock(&pDevContext->Lock, &oldIrql);
    if (pDevContext->ResourceDiskWorkItem) {
        IoFreeWorkItem(pDevContext->ResourceDiskWorkItem);
        pDevContext->ResourceDiskWorkItem = NULL;
    }

    KeReleaseSpinLock(&pDevContext->Lock, oldIrql);
    return;
}

NTSTATUS
QueueResourceDiskDetectionWFIfRequired
(
_In_ PDEV_CONTEXT DevContext,
_In_ PIRP         Irp
)
{
    BOOLEAN     isResourceDiskDetectionInProgress;
    KIRQL       OldIrql;

    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    isResourceDiskDetectionInProgress = DriverContext.isResourceDiskDetectionInProgress;
    DriverContext.isResourceDiskDetectionInProgress = TRUE;
    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

    if (!isResourceDiskDetectionInProgress) {
        PIO_WORKITEM    ResourceDiskWorkItem = IoAllocateWorkItem(DevContext->FilterDeviceObject);
        if (ResourceDiskWorkItem == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        IoQueueWorkItem(ResourceDiskWorkItem, ResourceDiskDetectionWorkItem, DelayedWorkQueue, Irp);
        IoMarkIrpPending(Irp);
        return STATUS_PENDING;
    }
    else {
        KeSetEvent(&DriverContext.hFailoverDetectionOver, IO_NO_INCREMENT, FALSE);
    }

    return STATUS_SUCCESS;
}

NTSTATUS
InDskFltQueryDriverAttribute(
_In_    PCWSTR              wszDriverPath,
_In_    PWCHAR              ParameterName,
_Out_   PVOID              ParameterValue
)
{
    NTSTATUS                 Status;
    RTL_QUERY_REGISTRY_TABLE QueryTable[2];

    // Query the requested parameter
    RtlZeroMemory(QueryTable, sizeof(QueryTable));

    QueryTable[0].QueryRoutine = NULL;
    QueryTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT | RTL_QUERY_REGISTRY_REQUIRED;
    QueryTable[0].Name = ParameterName;
    QueryTable[0].EntryContext = ParameterValue;

    Status = RtlQueryRegistryValues(RTL_REGISTRY_SERVICES,
        wszDriverPath,
        QueryTable,
        NULL,
        NULL);

    return Status;
}

NTSTATUS
QueryPreviousBootInformation(Registry ParametersKey)
{
    NTSTATUS                            Status = STATUS_SUCCESS;
    Registry                            BootDiskKey;
    UNICODE_STRING                      PrevBootDiskDeviceId = { 0 };

    Status = ParametersKey.OpenSubKey(BOOT_DISK_KEY, STRING_LEN_NULL_TERMINATED, BootDiskKey, false);
    if (!NT_SUCCESS(Status)) {
        // Failed to open registry key
        InDskFltWriteEvent(INDSKFLT_WARN_BOOT_DISK_KEY_OPEN_FAILED, Status);
        return Status;
    }

    RtlZeroMemory(&DriverContext.bootDiskInfo, sizeof(DriverContext.bootDiskInfo));

    Status = BootDiskKey.ReadUnicodeString(PREV_BOOTDEV_ID, STRING_LEN_NULL_TERMINATED,
        &PrevBootDiskDeviceId, L"", true, PagedPool);

    if (!NT_SUCCESS(Status)) {
        // Failed to read Previous Boot Device ID
        InDskFltWriteEvent(INDSKFLT_WARN_PREV_BOOT_DEVICEID_FAILED, Status);
        return Status;
    }

    RtlCopyMemory(DriverContext.prevBootDiskInfo.wDeviceId, PrevBootDiskDeviceId.Buffer, PrevBootDiskDeviceId.Length*sizeof(WCHAR));

    SafeFreeUnicodeString(PrevBootDiskDeviceId);

    Status = BootDiskKey.ReadUnicodeString(PREV_BOOTDEV_VENDOR_ID, STRING_LEN_NULL_TERMINATED,
        &DriverContext.prevBootDiskInfo.usVendorId, DEFAULT_BOOT_VENDOR_ID, true, PagedPool);
    if (!NT_SUCCESS(Status)) {
        // Failed to read Previous Boot Vendor ID
        InDskFltWriteEvent(INDSKFLT_WARN_PREV_BOOT_VENDORID_FAILED, Status);
        return Status;
    }

    Status = BootDiskKey.ReadULONG(PREV_BOOTDEV_BUS, &DriverContext.prevBootDiskInfo.BusType, BusTypeMax, FALSE);
    if (!NT_SUCCESS(Status)) {
        // Failed to read Previous Boot device bus
        InDskFltWriteEvent(INDSKFLT_WARN_PREV_BOOT_BUSTYPE_FAILED, Status);
        return Status;
    }

    InDskFltWriteEvent(INDSKFLT_INFO_PREV_BOOT_INFO,
        DriverContext.prevBootDiskInfo.wDeviceId,
        DriverContext.prevBootDiskInfo.usVendorId.Buffer,
        DriverContext.prevBootDiskInfo.BusType);

    return STATUS_SUCCESS;
}

NTSTATUS InDskFltHandleSanAndNoHydWF()
{
    NTSTATUS            Status;
    ULONG               ulDriverStartType;
    UNICODE_STRING      vmwareIdStr;
    ULONG32             ulDisableNoHydWF = 0;
    Registry            ParametersKey;
    WCHAR               *wszAzureScsiDrivers[] = { DRIVER_STORVSC_NAME, DRIVER_VMBUS_NAME };

    DriverContext.isNoHydWFActive = FALSE;
    DriverContext.isSanWFActive = TRUE;

    Status = ParametersKey.OpenKeyReadOnly(&DriverContext.DriverParameters);
    if (!NT_SUCCESS(Status)) {
        InDskFltWriteEvent(INDSKFLT_WARN_DRIVER_KEY_RONLY_OPEN_FAILED, Status);
        return Status;
    }

    Status = ParametersKey.ReadULONG(DISABLE_NO_HYDRATION_WF, &ulDisableNoHydWF, 0, FALSE);
    if (NT_SUCCESS(Status) && (0 != ulDisableNoHydWF)) {
        return STATUS_SUCCESS;
    }

    RtlInitUnicodeString(&vmwareIdStr, VENDOR_VMWARE);

    Status = QueryPreviousBootInformation(ParametersKey);
    if (!NT_SUCCESS(Status)) {
        return STATUS_SUCCESS;
    }

    if (!RtlEqualUnicodeString(&DriverContext.prevBootDiskInfo.usVendorId, &vmwareIdStr, TRUE)) {
        return STATUS_SUCCESS;
    }

    if ((BusTypeSas != DriverContext.prevBootDiskInfo.BusType) &&
        (BusTypeScsi != DriverContext.prevBootDiskInfo.BusType)) {
        return STATUS_SUCCESS;
    }

    for (ULONG ulDrvIdx = 0; ulDrvIdx < ARRAYSIZE(wszAzureScsiDrivers); ulDrvIdx++) {
        Status = InDskFltQueryDriverAttribute(wszAzureScsiDrivers[ulDrvIdx], DRIVER_START_KEY, (PVOID)&ulDriverStartType);

        if (!NT_SUCCESS(Status)) {
            InDskFltWriteEvent(INDSKFLT_ERROR_QUERY_DRIVER_START_TYPE, wszAzureScsiDrivers[ulDrvIdx], Status);
            return Status;
        }

        if (SERVICE_BOOT_START != ulDriverStartType) {
            DriverContext.isNoHydWFActive = TRUE;
        }
    }

    if (DriverContext.isNoHydWFActive) {
        InDskFltWriteEvent(INDSKFLT_INFO_NOHYDWF_ACTIVE);
    }
    return STATUS_SUCCESS;
}

NTSTATUS
InDskFltDetectFailover(
_In_    PDEV_CONTEXT    pDevContext
)
{
    NTSTATUS            Status = STATUS_SUCCESS;
    KIRQL               oldIrql;
    BOOLEAN             isBootDisk = false;

    KeAcquireSpinLock(&pDevContext->Lock, &oldIrql);
    isBootDisk = pDevContext->isBootDisk;
    KeReleaseSpinLock(&pDevContext->Lock, oldIrql);

    if (!isBootDisk) {
        return STATUS_SUCCESS;
    }

    if (RebootMode != DriverContext.eDiskFilterMode) {
        goto Cleanup;
    }


    if (!DriverContext.isNoHydWFActive) {
        goto Cleanup;
    }

    if (NULL == pDevContext->pDeviceDescriptor) {
        goto Cleanup;
    }

    // On Azure Gen1 VM boot device is always on IDE controller
    if (BusTypeAta != pDevContext->pDeviceDescriptor->BusType) {
        goto Cleanup;
    }

    // Boot Disk should be protected
    if (TEST_FLAG(pDevContext->ulFlags, DCF_FILTERING_STOPPED)) {
        goto Cleanup;
    }

    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
    DriverContext.isFailoverDetected = true;
    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);

    if (DriverContext.isFailoverDetected) {
        InDskFltWriteEvent(INDSKFLT_INFO_FAILOVER_DETECTED);
    }

Cleanup:
    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
    DriverContext.isNoHydWFActive = DriverContext.isFailoverDetected;
    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);

    KeSetEvent(&DriverContext.hFailoverDetectionOver, IO_NO_INCREMENT, FALSE);
    return Status;
}

NTSTATUS
InDskFltDetectResourceDiskOnFOVM(
_In_    PDEV_CONTEXT    pDevContext
)
{
    NTSTATUS            Status = STATUS_INVALID_DEVICE_STATE;
    KIRQL               oldIrql;
    UNICODE_STRING      vmwareIdStr;
    UNICODE_STRING      CurrentBusVendorId = { 0 };
    ANSI_STRING         cCurrentBusVendorId = { 0 };
    BOOLEAN             isResourceDisk = FALSE;

    if (RebootMode != DriverContext.eDiskFilterMode) {
        goto Cleanup;
    }

    if (!DriverContext.isNoHydWFActive) {
        goto Cleanup;
    }

    if (NULL == pDevContext->pDeviceDescriptor) {
        goto Cleanup;
    }

    // Resource disk are always on IDE controller
    if (BusTypeAta != pDevContext->pDeviceDescriptor->BusType) {
        goto Cleanup;
    }

    RtlInitUnicodeString(&vmwareIdStr, VENDOR_VMWARE);

    RtlInitAnsiString(&cCurrentBusVendorId, (char*)pDevContext->pDeviceDescriptor + pDevContext->pDeviceDescriptor->VendorIdOffset);
    RtlAnsiStringToUnicodeString(&CurrentBusVendorId, &cCurrentBusVendorId, TRUE);

    if (RtlEqualUnicodeString(&CurrentBusVendorId, &vmwareIdStr, TRUE)) {
        goto Cleanup;
    }

    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
    if (DriverContext.isResourceDiskDetectedOnFOVM) {
        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
        goto Cleanup;
    }

    KeAcquireSpinLockAtDpcLevel(&pDevContext->Lock);

    // Resource disk are never filtered
    if (!TEST_FLAG(pDevContext->ulFlags, DCF_FILTERING_STOPPED)) {
        KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
        goto Cleanup;
    }

    pDevContext->isResourceDisk = TRUE;
    DriverContext.isResourceDiskDetectedOnFOVM = TRUE;
    isResourceDisk = TRUE;
    KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
    SET_FLAG(DriverContext.ulFlags, DRIVER_FLAG_DISK_RECOVERY_NEEDED);
    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);

Cleanup:
    if (CurrentBusVendorId.Buffer) {
        ExFreePool(CurrentBusVendorId.Buffer);
    }

    if (isResourceDisk) {
        InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_RESOURCE_DISK_DETECTED, pDevContext);
    }

    return Status;
}

NTSTATUS
QueryNoHydration(
PDEV_CONTEXT   pDevContext,
PIRP Irp)
{
    NTSTATUS                    Status = STATUS_SUCCESS;
    KIRQL                       OldIrql = 0;
    ULONG                       ulDevIdLength;

    ulDevIdLength = (ULONG)(DEVID_LENGTH * sizeof(WCHAR));

    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    if (!DriverContext.isBootDeviceStarted) {
        KeAcquireSpinLockAtDpcLevel(&pDevContext->Lock);
        // Detect Boot
        if ((wcslen(DriverContext.prevBootDiskInfo.wDeviceId) == wcslen(pDevContext->wDevID)) &&
            (ulDevIdLength == RtlCompareMemory(pDevContext->wDevID,
            DriverContext.prevBootDiskInfo.wDeviceId,
            ulDevIdLength))) {
            pDevContext->isBootDisk = true;
            DriverContext.isBootDeviceStarted = true;
        }
        KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
    }
    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);

    if (!DriverContext.isNoHydWFActive) {
        return STATUS_SUCCESS;
    }

    // On Azure during boot when storvsc is not boot driver
    // Only ata bus devices should be enumerated during boot
    if (pDevContext->pDeviceDescriptor && pDevContext->pDeviceDescriptor->BusType != BusTypeAta) {
        KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
        DriverContext.isNoHydWFActive = FALSE;
        KeReleaseSpinLock(&DriverContext.Lock, OldIrql);
        return STATUS_SUCCESS;
    }

    if (pDevContext->isBootDisk) {
        InDskFltDetectFailover(pDevContext);
        return STATUS_SUCCESS;
    }

    if (IsResourceDiskSuspected(pDevContext) && IsDiskSeenFirstTime(pDevContext))
    {
        Status = QueueResourceDiskDetectionWFIfRequired(pDevContext, Irp);
    }

    return Status;
}

// By default driver holds resposibility of updating its registry for boot disk information.
// To handle upgrade and no-reboot cases
//      User mode no-hydration WF updates driver boot information when it doesn't exist
//      Apart user mode checks if this information exists, then only it marks for no-hydration

NTSTATUS
UpdateBootInformation()
{
    NTSTATUS        Status = STATUS_SUCCESS;
    Registry        ParametersKey;
    Registry        BootDiskKey;
    UNICODE_STRING  BootDeviceID;

    Status = QueryBootDiskInformation();
    if (STATUS_SUCCESS != Status) {
        InDskFltWriteEvent(INDSKFLT_ERROR_GET_BOOTDISK_FAILED, Status);
        return Status;
    }

    RtlInitUnicodeString(&BootDeviceID, DriverContext.bootDiskInfo.wDeviceId);

    Status = ParametersKey.OpenKeyReadWrite(&DriverContext.DriverParameters);
    if (!NT_SUCCESS(Status)) {
        // Failed to open the key
        InDskFltWriteEvent(INDSKFLT_WARN_DRIVER_KEY_RW_OPEN_FAILED, Status);
        return Status;
    }

    Status = ParametersKey.OpenSubKey(BOOT_DISK_KEY, STRING_LEN_NULL_TERMINATED, BootDiskKey, true);
    if (!NT_SUCCESS(Status)) {
        // Failed to open registry key
        InDskFltWriteEvent(INDSKFLT_WARN_BOOT_DISK_KEY_OPEN_FAILED, Status);
        return Status;
    }

    Status = BootDiskKey.WriteUnicodeString(PREV_BOOTDEV_ID, &BootDeviceID);
    if (!NT_SUCCESS(Status)) {
        // Failed to write Boot Device ID
        InDskFltWriteEvent(INDSKFLT_WARN_UPDATE_BOOT_DEVICEID_FAILED, BootDeviceID.Buffer, Status);
        return Status;
    }

    Status = BootDiskKey.WriteUnicodeString(PREV_BOOTDEV_VENDOR_ID, &DriverContext.bootDiskInfo.usVendorId);
    if (!NT_SUCCESS(Status)) {
        // Failed to update Boot Vendor ID
        InDskFltWriteEvent(INDSKFLT_WARN_UPDATE_BOOT_VENDORID_FAILED, DriverContext.bootDiskInfo.usVendorId.Buffer, Status);
        return Status;
    }

    Status = BootDiskKey.WriteULONG(PREV_BOOTDEV_BUS, DriverContext.bootDiskInfo.BusType);
    if (!NT_SUCCESS(Status)) {
        // Failed to update Previous Boot device bus
        InDskFltWriteEvent(INDSKFLT_WARN_UPDATE_BOOT_BUSTYPE_FAILED, DriverContext.bootDiskInfo.BusType, Status);
        return Status;
    }

    InDskFltWriteEvent(INDSKFLT_INFO_UPDATED_BOOT_INFO,
        BootDeviceID.Buffer,
        DriverContext.bootDiskInfo.usVendorId.Buffer,
        DriverContext.bootDiskInfo.BusType);

    return STATUS_SUCCESS;
}

BOOLEAN
IsDiskSeenFirstTime(PDEV_CONTEXT pDevContext)
{
    HANDLE      hParametersKey = NULL;
    NTSTATUS    Status;
    BOOLEAN     bRetValue = FALSE;
    ULONG       ulSanPolicy;
    KIRQL               oldIrql;
    BOOLEAN     bReleaseRemoveLock = FALSE;
    PDEVICE_EXTENSION DevExt = NULL;

    KeAcquireSpinLock(&pDevContext->Lock, &oldIrql);
    if (!pDevContext->FilterDeviceObject || !pDevContext->FilterDeviceObject->DeviceExtension) {
        KeReleaseSpinLock(&pDevContext->Lock, oldIrql);
        goto cleanup;
    }

    DevExt = (PDEVICE_EXTENSION)pDevContext->FilterDeviceObject->DeviceExtension;

    Status = IoAcquireRemoveLock(&DevExt->RemoveLock, DevExt);
    if (!NT_SUCCESS(Status)) {
        KeReleaseSpinLock(&pDevContext->Lock, oldIrql);
        goto cleanup;
    }

    bReleaseRemoveLock = TRUE;
    KeReleaseSpinLock(&pDevContext->Lock, oldIrql);

    Status = IoOpenDeviceRegistryKey(DevExt->PhysicalDeviceObject,
        PLUGPLAY_REGKEY_DEVICE,
        KEY_READ,
        &hParametersKey);

    if (!NT_SUCCESS(Status)) {
        InDskFltWriteEventWithDevCtxt(INFLTDRV_ERROR_CURRENT_HARDWARE_ID_OPEN_FAILED, pDevContext, Status);
        goto cleanup;
    }

    Status = InDskFltQuerySanPolicy(hParametersKey, SAN_ATTRIB_NAME, &ulSanPolicy);
    ZwClose(hParametersKey);

    bRetValue = (STATUS_OBJECT_NAME_NOT_FOUND == Status);
cleanup:
    if (bReleaseRemoveLock) {
        IoReleaseRemoveLock(&DevExt->RemoveLock, DevExt);
    }

    return bRetValue;
}

NTSTATUS
QueryBootDiskInformation()
{
    NTSTATUS    Status;
    BOOTDISK_INFORMATION_EX     bootDiskInformationEx;
    UNICODE_STRING              usDeviceId;
    WCHAR                       wDeviceId[DEVID_LENGTH];
    PDEV_CONTEXT                pDevContext = NULL;
    UNICODE_STRING              GUIDString = { 0 };
    USHORT                      usGuidLength = 0;

    ANSI_STRING                 cCurrentBusVendorId = { 0 };
    ULONG                       ulDeviceIdLength = ARRAYSIZE(wDeviceId) * sizeof(WCHAR);

    Status = IoGetBootDiskInformation((PBOOTDISK_INFORMATION)&bootDiskInformationEx, sizeof(bootDiskInformationEx));
    if (Status != STATUS_SUCCESS) {
        InDskFltWriteEvent(INDSKFLT_ERROR_GET_BOOTDISK_FAILED, Status);
        return Status;
    }

    RtlInitEmptyUnicodeString(&usDeviceId, wDeviceId, (USHORT)ulDeviceIdLength);

    if (bootDiskInformationEx.SystemDeviceIsGpt) {
        if (IsEqualGUID(bootDiskInformationEx.SystemDeviceGuid, GUID_NULL)) {
            InDskFltWriteEvent(INDSKFLT_ERROR_GUIDNULL_BOOTDISK);
            return STATUS_NOT_FOUND;
        }

        RtlStringFromGUID(bootDiskInformationEx.SystemDeviceGuid, &GUIDString);
        usGuidLength = GUIDString.Length - (sizeof(WCHAR) * 2);
        if (usGuidLength > ulDeviceIdLength) {
            ExFreePool(GUIDString.Buffer);
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        //Skip opening braces
        RtlCopyMemory(wDeviceId, GUIDString.Buffer + 1, usGuidLength);

        ExFreePool(GUIDString.Buffer);
    }
    else if (0 == bootDiskInformationEx.SystemDeviceSignature) {
        InDskFltWriteEvent(INDSKFLT_ERROR_ZERO_BOOTDISK);
        return STATUS_NOT_FOUND;
    }
    else {
        RtlIntegerToUnicodeString(bootDiskInformationEx.SystemDeviceSignature, 10, &usDeviceId);
    }

    RtlCopyMemory(DriverContext.bootDiskInfo.wDeviceId, wDeviceId, ulDeviceIdLength);

    InDskFltWriteEvent(INDSKFLT_INFO_BOOTDISK_DEVICEID, wDeviceId);

    if (ulDeviceIdLength != RtlCompareMemory(wDeviceId, DriverContext.prevBootDiskInfo.wDeviceId, ulDeviceIdLength)) {
        DriverContext.isNoHydWFActive = FALSE;
    }

    pDevContext = GetDevContextWithGUID(wDeviceId, (ULONG)(wcslen(wDeviceId)*sizeof(WCHAR)));
    if (!pDevContext) {
        InDskFltWriteEvent(INDSKFLT_ERROR_FAILED_TO_GET_BOOTDEV);
        return STATUS_NO_SUCH_DEVICE;
    }

    if (!pDevContext->pDeviceDescriptor) {
        DereferenceDevContext(pDevContext);
        InDskFltWriteEvent(INDSKFLT_ERROR_BOOTDEV_NO_DEVDESC);
        return STATUS_NO_SUCH_DEVICE;
    }

    RtlInitAnsiString(&cCurrentBusVendorId, (char*)pDevContext->pDeviceDescriptor + pDevContext->pDeviceDescriptor->VendorIdOffset);
    RtlAnsiStringToUnicodeString(&DriverContext.bootDiskInfo.usVendorId, &cCurrentBusVendorId, TRUE);

    DriverContext.bootDiskInfo.BusType = pDevContext->pDeviceDescriptor->BusType;
    DereferenceDevContext(pDevContext);
    return STATUS_SUCCESS;
}