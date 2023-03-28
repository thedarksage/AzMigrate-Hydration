/* ------------------------------------------------------------------------ *
*                                                                           *
*  Copyright (c) Microsoft Corporation                                      *
*                                                                           *
*  Module Name:  SanPolicyFunctions.cpp                                     *
*                                                                           *
*  Abstract   :  San Policy updation routines.                              *
*                                                                           *
* ------------------------------------------------------------------------- */

#include "VContext.h"
#include "FltFunc.h"
#include "misc.h"
#include "svdparse.h"
#include "ListNode.h"

NTSTATUS
DeviceIoControlSetSanPolicy(
PDEVICE_OBJECT  DeviceObject,
PIRP            Irp
)
{
    NTSTATUS                Status = STATUS_SUCCESS;
    NTSTATUS                FinalStatus = STATUS_SUCCESS;
    PDEV_CONTEXT            pDevContext = NULL;
    PIO_STACK_LOCATION      IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    PSET_SAN_POLICY   Input = (PSET_SAN_POLICY)Irp->AssociatedIrp.SystemBuffer;
    LIST_ENTRY              VolumeNodeList;

    InitializeListHead(&VolumeNodeList);

    ASSERT(IOCTL_INMAGE_SET_SAN_POLICY == IoStackLocation->Parameters.DeviceIoControl.IoControlCode);

    if (IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < sizeof(SET_SAN_POLICY)) {
        FinalStatus = STATUS_BUFFER_TOO_SMALL;
        goto Cleanup;
    }

    if (TEST_FLAG(Input->ulFlags, SET_SAN_ALL_DEVICES_FLAGS))
    {
        Status = GetListOfDevs(&VolumeNodeList);
        if (!NT_SUCCESS(Status)) {
            FinalStatus = Status;
            goto Cleanup;
        }

        if (IsListEmpty(&VolumeNodeList)) {
            FinalStatus = STATUS_NO_SUCH_DEVICE;
            goto Cleanup;
        }
    }
    else
    {
        pDevContext = GetDevContextForThisIOCTL(DeviceObject, Irp);
        if (NULL == pDevContext) {
            FinalStatus = STATUS_NO_SUCH_DEVICE;
            goto Cleanup;
        }
    }

    while ((NULL != pDevContext) || !IsListEmpty(&VolumeNodeList)) {
        while (NULL == pDevContext) {
            PLIST_NODE      pNode;
            pNode = (PLIST_NODE)RemoveHeadList(&VolumeNodeList);
            if (NULL != pNode) {
                pDevContext = (PDEV_CONTEXT)pNode->pData;
                DereferenceListNode(pNode);
            }
        }

        if (NULL == pDevContext)    break;

        Status = InDskFltUpdateSanPolicy(pDevContext, SAN_ATTRIB_NAME, Input->ulPolicy);
        if (!NT_SUCCESS(FinalStatus))
        {
            InDskFltWriteEventWithDevCtxt(INFLTDRV_ERROR_UPDATE_SANPOLICY_FAILED, pDevContext, Input->ulPolicy, Status);
            // Ideally we want status for each of disk for which driver is stopping filtering.
            // That may be changed later. Currently retruning first error that we see while stopping filtering.
            FinalStatus = Status;
        }
        else {
            InDskFltWriteEventWithDevCtxt(INFLTDRV_INFO_SANPOLICY_UPDATED, pDevContext, Input->ulPolicy);
        }

        DereferenceDevContext(pDevContext);

        pDevContext = NULL;
    }
Cleanup:
    Irp->IoStatus.Status = FinalStatus;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return FinalStatus;

}

NTSTATUS
InDskFltUpdateSanPolicy(
_In_    PDEV_CONTEXT       pDevContext,
_In_    PWCHAR              ParameterName,
_In_    ULONG              ParameterValue
)
{
    HANDLE              hParametersKey = NULL;
    KIRQL               oldIrql;
    PDEVICE_EXTENSION   DevExt = NULL;
    NTSTATUS            Status = STATUS_SUCCESS;
    BOOLEAN             bReleaseRemoveLock = FALSE;

    KeAcquireSpinLock(&pDevContext->Lock, &oldIrql);
    if (!pDevContext->FilterDeviceObject || !pDevContext->FilterDeviceObject->DeviceExtension) {
        KeReleaseSpinLock(&pDevContext->Lock, oldIrql);
        Status = STATUS_INVALID_DEVICE_STATE;
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

    // Query current Hardware Id key
    Status = IoOpenDeviceRegistryKey(DevExt->PhysicalDeviceObject,
        PLUGPLAY_REGKEY_DEVICE,
        KEY_READ | KEY_WRITE,
        &hParametersKey);

    if (!NT_SUCCESS(Status)) {
        InDskFltWriteEventWithDevCtxt(INFLTDRV_ERROR_CURRENT_HARDWARE_ID_OPEN_FAILED, pDevContext, Status);
        hParametersKey = NULL;
        goto cleanup;
    }

    Status = InDskFltUpdateSanPolicy(hParametersKey, ParameterName, ParameterValue);
    if (!NT_SUCCESS(Status)) {
        InDskFltWriteEventWithDevCtxt(INFLTDRV_ERROR_UPDATE_SANPOLICY_FAILED, pDevContext, ParameterValue, Status);
    }
    else {
        InDskFltWriteEventWithDevCtxt(INFLTDRV_INFO_SANPOLICY_UPDATED, pDevContext, ParameterValue);
    }

cleanup:
    if (bReleaseRemoveLock) {
        IoReleaseRemoveLock(&DevExt->RemoveLock, DevExt);
    }

    if (hParametersKey) {
        ZwClose(hParametersKey);
    }

    return Status;
}

NTSTATUS
InDskFltSetSanPolicy(
    _Inout_ PDEVICE_EXTENSION DevExt
)
/*
Routine Description :
    This routine copies san policy for protected disk before reboot into current partmgr hive.
    Algorithm:
        Query current hardware ID and Previous Hardware ID.
        If Previous Hardware Id is Empty
            If current Hardware Id is empty
                Set San Policy for protected disk to online.
            Update hardware Id in own hive
        If previous and Current Hardware Id is same return.
        If Previous and Current Sanpolicy is same
            Update hardware Id in own hive
        Else
            update sanpolicy for protected disk.
            Update hardware Id in own hive

Arguments :
    DevExt:
        Device extension for current disk.

Return Value :
    NTSTATUS

*/
{
    HANDLE                   hParametersKey = NULL;

    NTSTATUS                Status = STATUS_SUCCESS;
    PDEV_CONTEXT            DevContext = DevExt->pDevContext;
    ULONG                   ulSize;
    UNICODE_STRING          PrevHardwareIdKeyPath;
    UNICODE_STRING          CurrentHardwareIdKeyPath;
    BOOLEAN                 updateHardwareIdKey = FALSE;
    BOOLEAN                 updateSanPolicy = FALSE;
    ULONG                   ulPrevSanPolicy = ULONG_MAX;
    ULONG                   ulCurrentSanPolicy = ULONG_MAX;
    PWCHAR                  HardwareIdName = NULL;
    ULONG                   ulSanPolicy = 0;
    KIRQL                   oldIrql = 0;

    KeAcquireSpinLock(&DevContext->Lock, &oldIrql);
    if (NULL != DevContext->CurrentHardwareIdName) {
        // Hardware Id already set. Ignore rest of function.
        KeReleaseSpinLock(&DevContext->Lock, oldIrql);
        goto cleanup;
    }
    KeReleaseSpinLock(&DevContext->Lock, oldIrql);

    // Query current Hardware Id key
    Status = IoOpenDeviceRegistryKey(DevExt->PhysicalDeviceObject,
        PLUGPLAY_REGKEY_DEVICE,
        KEY_READ | KEY_WRITE,
        &hParametersKey);

    if (!NT_SUCCESS(Status)) {
        InDskFltWriteEventWithDevCtxt(INFLTDRV_ERROR_CURRENT_HARDWARE_ID_OPEN_FAILED, DevContext, Status);
        hParametersKey = NULL;
        goto cleanup;
    }

    // Query current hardware Id name
    Status = InDskFltQueryNameForRegistryHandle(hParametersKey, &HardwareIdName, &ulSize);
    if (!NT_SUCCESS(Status)) {
        InDskFltWriteEventWithDevCtxt(INFLTDRV_ERROR_CURRENT_HARDWARE_ID_QUERY_FAILED, DevContext, Status);
        goto cleanup;
    }

    // Set current Hardware Id
    KeAcquireSpinLock(&DevContext->Lock, &oldIrql);
    if (NULL != DevContext->CurrentHardwareIdName) {
        // Hardware Id already set. Ignore rest of function.
        KeReleaseSpinLock(&DevContext->Lock, oldIrql);
        goto cleanup;
    }
    DevContext->CurrentHardwareIdName = HardwareIdName;
    HardwareIdName = NULL;
    KeReleaseSpinLock(&DevContext->Lock, oldIrql);

    // Log Current Hardware ID
    InDskFltWriteEventWithDevCtxt(INFLTDRV_INFO_CURRENT_HARDWARE_ID, DevContext, DevContext->CurrentHardwareIdName);

    updateHardwareIdKey = TRUE;

    // Handle only 2 conditions
    //      Query for Hardware Id passed
    //      Current SanPolicy doesn't exist. This can be true if driver has been
    //      upgraded and reboot is not done. In this case on failover, we are 
    //      going to get san policy as empty. We will force protected disk to
    //      come online.
    Status = InDskFltQuerySanPolicy(hParametersKey, SAN_ATTRIB_NAME, &ulCurrentSanPolicy);
    if (!NT_SUCCESS(Status) && (STATUS_OBJECT_NAME_NOT_FOUND != Status)) {
        InDskFltWriteEventWithDevCtxt(INFLTDRV_ERROR_QUERY_CURRENT_SANPOLICY_FAILED, DevContext, Status);
        goto cleanup;
    }

    // At this point Update San Policy only in 2 conditions
    //      Disk is protected
    //      Current SanPolicy doesn't exist
    updateSanPolicy =  (DriverContext.isSanWFActive && !DevContext->isResourceDisk) &&
                        !TEST_FLAG(DevContext->ulFlags, DCF_FILTERING_STOPPED) &&
                        (STATUS_OBJECT_NAME_NOT_FOUND == Status);

    Status = IndskfltQueryPreviousHardwareIdName(DevContext);
    if (!NT_SUCCESS(Status)) {
       // Ignore any failure for previous query
    }

    // If previous san policy doesn't exist skip next checks
    if (NULL == DevContext->PrevHardwareIdName) {
        // Previous Hardware Id Name is empty
        InDskFltWriteEventWithDevCtxt(INFLTDRV_INFO_HARDWARE_ID_PREV_EMPTY, DevContext);
        goto cleanup;
    }

    // Log Previous Hardware ID
    InDskFltWriteEventWithDevCtxt(INFLTDRV_INFO_PREV_HARDWARE_ID, DevContext, DevContext->PrevHardwareIdName);

    RtlUnicodeStringInit(&PrevHardwareIdKeyPath, DevContext->PrevHardwareIdName);
    RtlUnicodeStringInit(&CurrentHardwareIdKeyPath, DevContext->CurrentHardwareIdName);

    // In case hardware ID has not changed skip san update 
    if (0 == RtlCompareUnicodeString(&PrevHardwareIdKeyPath, &CurrentHardwareIdKeyPath, TRUE)) {
        updateHardwareIdKey = FALSE;
        updateSanPolicy = FALSE;
        InDskFltWriteEventWithDevCtxt(INFLTDRV_INFO_HARDWARE_ID_NAME_NO_CHANGE, DevContext);
        goto cleanup;
    }

    // Skip non-resource disks which are not protected
    if (TEST_FLAG(DevContext->ulFlags, DCF_FILTERING_STOPPED) && !DevContext->isResourceDisk) {
        updateSanPolicy = FALSE;
        goto cleanup;
    }

    // Query previous san policy
    Status = InDskFltQueryPreviousSanPolicy(DevContext->PrevHardwareIdName, SAN_ATTRIB_NAME, &ulPrevSanPolicy);
    if (!NT_SUCCESS(Status)) {
        InDskFltWriteEventWithDevCtxt(INFLTDRV_ERROR_QUERY_PREV_SANPOLICY_FAILED, DevContext, Status);
        goto cleanup;
    }

    // If san policy for disk has not changed skip san update
    if (ulCurrentSanPolicy == ulPrevSanPolicy) {
        updateSanPolicy = FALSE;
        InDskFltWriteEventWithDevCtxt(INFLTDRV_INFO_SANPOLICY_MATCHED, DevContext, ulCurrentSanPolicy);
        goto cleanup;
    }

    updateSanPolicy = TRUE;
    ulSanPolicy = ulPrevSanPolicy;

cleanup:
    if (updateSanPolicy && DriverContext.isSanWFActive && !IsWindows2008()) {
        InDskFltWriteEventWithDevCtxt(INFLTDRV_INFO_SANPOLICY_UPDATING, DevContext, ulSanPolicy);

        Status = InDskFltUpdateSanPolicy(hParametersKey, SAN_ATTRIB_NAME, ulSanPolicy);
        if (!NT_SUCCESS(Status)) {
            InDskFltWriteEventWithDevCtxt(INFLTDRV_ERROR_UPDATE_SANPOLICY_FAILED, DevContext, ulSanPolicy, Status);
        }
        else {
            InDskFltWriteEventWithDevCtxt(INFLTDRV_INFO_SANPOLICY_UPDATED, DevContext, ulSanPolicy);
        }
    }

    if (updateHardwareIdKey) {
        Status = InDskFltUpdateHardwareId(DevExt, DevContext->CurrentHardwareIdName);
        if (!NT_SUCCESS(Status)) {
            InDskFltWriteEventWithDevCtxt(INFLTDRV_ERROR_HARDWARE_ID_FAIL, DevContext, Status);
        }
    }

    if (hParametersKey) {
        ZwClose(hParametersKey);
    }

    if (HardwareIdName) {
        ExFreePoolWithTag(HardwareIdName, TAG_GENERIC_NON_PAGED);
    }
    return Status;
}

NTSTATUS
InDskFltUpdateHardwareId(
_In_    PDEVICE_EXTENSION  DeviceExtension,
_In_    PWCHAR             HardwareIdName
)
/*
Routine Description :
    Updates Hardware ID name in driver's own hive.

Arguments :
    DevExt:
        Device extension for current disk.

Return Value :
    NTSTATUS
*/
{
    PDEV_CONTEXT    DevContext = DeviceExtension->pDevContext;
    NTSTATUS        Status = STATUS_SUCCESS;
    Registry        *pDevParam = NULL;

    // Let us open/create the registry key for this device.
    pDevParam = new Registry();
    if (NULL == pDevParam) {
        goto cleanup;
    }

    Status = pDevParam->OpenKeyReadWrite(&DevContext->DevParameters);
    if (!NT_SUCCESS(Status)) {
        goto cleanup;
    }

    Status = pDevParam->WriteWString(HARDWARE_ID_KEY_NAME, HardwareIdName);

cleanup:
    if (NULL != pDevParam) {
        delete pDevParam;
    }
    return Status;
}

NTSTATUS
InDskFltQuerySanPolicy(
    _In_    HANDLE              hParametersKey,
    _In_    PWCHAR              ParameterName,
    _Out_   PULONG              ParameterValue
)
/*
Routine Description :
    Query SanPolicy for given handle.

Arguments :
    hParametersKey:
        Handle to given Device hardware Id
    ParameterName:
        Registry Value Name to be queried inside partmgr key.
    ParameterValue:
        Value for Registry Value Name.

Return Value :
    NTSTATUS
*/
{
    HANDLE                   hParametersSubKey = NULL;
    NTSTATUS                 Status;
    UNICODE_STRING           Name = { 0 };
    OBJECT_ATTRIBUTES        ObjectAttributes = { 0 };
    RTL_QUERY_REGISTRY_TABLE QueryTable[2];

    //
    // Now open partmgr subkey
    //
    RtlInitUnicodeString(&Name, PARTMGR_REG_KEY);

    InitializeObjectAttributes(&ObjectAttributes,
        &Name,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        hParametersKey,
        NULL);

    Status = ZwOpenKey(&hParametersSubKey,
        KEY_READ,
        &ObjectAttributes);

    if (!NT_SUCCESS(Status)) {

        hParametersSubKey = NULL;
        goto cleanup;
    }

    // Query the requested parameter
    RtlZeroMemory(QueryTable, sizeof(QueryTable));

    QueryTable[0].QueryRoutine = NULL;
    QueryTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT | RTL_QUERY_REGISTRY_REQUIRED;
    QueryTable[0].Name = ParameterName;
    QueryTable[0].EntryContext = ParameterValue;

    Status = RtlQueryRegistryValues(RTL_REGISTRY_HANDLE,
        (PCWSTR)hParametersSubKey,
        QueryTable,
        NULL,
        NULL);

cleanup:
    if (hParametersSubKey) {
        ZwClose(hParametersSubKey);
    }

    return Status;

}

NTSTATUS
InDskFltQueryPreviousSanPolicy(
    _In_    PWCHAR              PrevHardwareIdName,
    _In_    PWCHAR              ParameterName,
    _Out_   PULONG              ParameterValue
)
/*
Routine Description :
    Queries Previous SanPolicy.

Arguments :
    PrevHardwareIdName:
        Previous Hardware Id Name.

    ParameterName:
        Registry Value name to be read inside Partmgr registry name.

    ParameterValue:
        Value for Registry Value Name.

Return Value :
    NTSTATUS
*/
{
    HANDLE                   hParametersKey = NULL;
    NTSTATUS                 Status;
    UNICODE_STRING           Name = { 0 };
    OBJECT_ATTRIBUTES        ObjectAttributes = { 0 };

    RtlUnicodeStringInit(&Name, PrevHardwareIdName);

    InitializeObjectAttributes(&ObjectAttributes,
        &Name,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL,
        NULL);

    // Open a handle to hardware Id name Handle
    Status = ZwOpenKey(&hParametersKey, KEY_READ, &ObjectAttributes);
    if (!NT_SUCCESS(Status)) {
        hParametersKey = NULL;
        goto cleanup;
    }

    // Query san policy for given handle.
    Status = InDskFltQuerySanPolicy(hParametersKey, ParameterName, ParameterValue);

cleanup:
    if (hParametersKey) {
        ZwClose(hParametersKey);
    }
    return Status;
}

NTSTATUS
InDskFltUpdateSanPolicy(
    _In_    HANDLE              hParametersKey,
    _In_    PWCHAR              ParameterName,
    _In_    ULONG              ParameterValue
)
/*
Routine Description :
    Update San SanPolicy.

Arguments :
    hParametersKey:
        Handle to current Hardware ID.

    ParameterName:
        Registry Value name to be read inside Partmgr registry name.

    ParameterValue:
        Value for Registry Value Name.

Return Value :
    NTSTATUS
*/
{
    HANDLE                   hParametersSubKey = NULL;
    NTSTATUS                 Status;
    UNICODE_STRING           Name = { 0 };
    OBJECT_ATTRIBUTES        ObjectAttributes = { 0 };

    // Open partmgr subkey
    RtlUnicodeStringInit(&Name, PARTMGR_REG_KEY);

    InitializeObjectAttributes(&ObjectAttributes,
        &Name,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        hParametersKey,
        NULL);

    Status = ZwCreateKey(&hParametersSubKey,
        KEY_READ | KEY_WRITE,
        &ObjectAttributes,
        0,
        NULL,
        0,
        NULL);

    if (!NT_SUCCESS(Status)) {

        hParametersSubKey = NULL;
        goto cleanup;
    }

    Status = RtlWriteRegistryValue(RTL_REGISTRY_HANDLE,
        (PCWSTR)hParametersSubKey,
        ParameterName,
        REG_DWORD,
        &ParameterValue,
        sizeof(ParameterValue));

cleanup:
    if (NULL != hParametersSubKey) {
        ZwClose(hParametersSubKey);
    }
    return Status;
}


NTSTATUS
InDskFltQueryNameForRegistryHandle(
    _In_    HANDLE              hParametersKey,
    _Out_   PWCHAR*             RegistryKeyName,
    _Out_   PULONG              ulSize
)
/*
Routine Description :
    Queries Registry name for given handle.

Arguments :
    hParametersKey:
        Handle to Registry Key.

    RegistryKeyName:
        Registry Key Name.

    ulSize:
        Size of Registry key name.

Return Value :
    NTSTATUS
*/
{
    NTSTATUS                Status;
    ULONG                   ulBufferLength;
    PKEY_NAME_INFORMATION   RegistryKeyNameInformation = NULL;
    PWCHAR                  RegistryKeyNameTemp = NULL;

    *RegistryKeyName = NULL;
    *ulSize = 0;

    Status = ZwQueryKey(hParametersKey, KeyNameInformation, NULL, 0, &ulBufferLength);
    if (STATUS_BUFFER_TOO_SMALL != Status) {
        if (NT_SUCCESS(Status)) {
            Status = STATUS_UNSUCCESSFUL;
        }
        goto cleanup;
    }

    RegistryKeyNameInformation = (PKEY_NAME_INFORMATION)ExAllocatePoolWithTag(NonPagedPool, ulBufferLength, TAG_GENERIC_NON_PAGED);
    if (NULL == RegistryKeyNameInformation) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto cleanup;
    }

    RtlZeroMemory(RegistryKeyNameInformation, ulBufferLength);

    Status = ZwQueryKey(hParametersKey, KeyNameInformation, RegistryKeyNameInformation, ulBufferLength, &ulBufferLength);
    if (!NT_SUCCESS(Status)) {
        goto cleanup;
    }

    RegistryKeyNameTemp = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool, RegistryKeyNameInformation->NameLength + sizeof(WCHAR), TAG_GENERIC_NON_PAGED);
    if (NULL == RegistryKeyNameTemp) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto cleanup;
    }

    RtlCopyMemory(RegistryKeyNameTemp, RegistryKeyNameInformation->Name, RegistryKeyNameInformation->NameLength);
    RegistryKeyNameTemp[RegistryKeyNameInformation->NameLength / sizeof(WCHAR)] = L'\0';

    *RegistryKeyName = RegistryKeyNameTemp;
    *ulSize = RegistryKeyNameInformation->NameLength + sizeof(WCHAR);

cleanup:
    if (NULL != RegistryKeyNameInformation) {
        ExFreePoolWithTag(RegistryKeyNameInformation, TAG_GENERIC_NON_PAGED);
    }

    if (!NT_SUCCESS(Status)){
        if (NULL != RegistryKeyNameTemp) {
            ExFreePoolWithTag(RegistryKeyNameTemp, TAG_GENERIC_NON_PAGED);
        }
    }

    return Status;
}

NTSTATUS
IndskfltQueryPreviousHardwareIdName(
    _In_    PDEV_CONTEXT DevContext
)
{
    Registry        *pDevParam = NULL;
    NTSTATUS        Status = STATUS_SUCCESS;
    PWCHAR          PrevHardwareIdName = NULL;
    ULONG           ulHardwareIdNameLength = 0;
    KIRQL           oldIrql = 0;

    KeAcquireSpinLock(&DevContext->Lock, &oldIrql);
    if (NULL != DevContext->PrevHardwareIdName) {
        KeReleaseSpinLock(&DevContext->Lock, oldIrql);
        goto cleanup;
    }
    KeReleaseSpinLock(&DevContext->Lock, oldIrql);

    // Let us open/create the registry key for this device.
    pDevParam = new Registry();
    if (NULL == pDevParam) {
        goto cleanup;
    }

    Status = pDevParam->OpenKeyReadWrite(&DevContext->DevParameters);
    if (!NT_SUCCESS(Status)) {
        goto cleanup;
    }

    ulHardwareIdNameLength = (ULONG)(wcslen(HARDWARE_ID_KEY_NAME) * sizeof(WCHAR));
    Status = pDevParam->ReadWString(HARDWARE_ID_KEY_NAME,
        ulHardwareIdNameLength,
        &PrevHardwareIdName,
        L"",
        FALSE,
        FALSE,
        NonPagedPool);

    if (!NT_SUCCESS(Status)) {
        goto cleanup;
    }

    KeAcquireSpinLock(&DevContext->Lock, &oldIrql);
    if (NULL != DevContext->PrevHardwareIdName) {
        KeReleaseSpinLock(&DevContext->Lock, oldIrql);
        goto cleanup;
    }
    DevContext->PrevHardwareIdName = PrevHardwareIdName;
    PrevHardwareIdName = NULL;
    KeReleaseSpinLock(&DevContext->Lock, oldIrql);

cleanup:
    if (NULL != pDevParam) {
        delete pDevParam;
    }

    if (NULL != PrevHardwareIdName) {
        ExFreePoolWithTag(PrevHardwareIdName, TAG_REGISTRY_DATA);
    }

    return Status;
}