#include "Common.h"
#include "FltFunc.h"

NTSTATUS InSetCompletionRoutine (
    PIRP                        Irp,
    PIO_COMPLETION_ROUTINE      pCompletionRoutine,
    PDEVICE_OBJECT              pFltDeviceObject
    )
/*++

Routine Description:
    Set the completion routine for Reboot Driver
    Fill the completion context required to call completion routine
    IRQL <=DISPATCH_LEVEL

Arguments:
    Irp - IRP sent by IoManager
    pCompletionRoutine - 
        Reboot Driver:- Refer IoSetCompletionRoutine 
        NoReboot:- Completion Routine to be called from wrapper
    pFltDeviceObject - 
        Reboot Driver:- No OP
        NoReboot:- Filter Device Object

Return Value:
    Error is return in case completion routine not set for NoReboot

--*/

{
    NTSTATUS                Status = STATUS_SUCCESS;
    PCOMPLETION_CONTEXT     pNoRebootCompContext = NULL;
    PDEV_CONTEXT            pDevContext = NULL;
    PDEVICE_EXTENSION       pDeviceExtension = NULL;
    PIO_STACK_LOCATION      IoStackLocation = NULL;

    IoStackLocation = IoGetCurrentIrpStackLocation(Irp);

    // Expected to set minimum one invoke value
    ASSERT(TEST_ALL_FLAGS(IoStackLocation->Control, INVOKE_ON_ALL));
    ASSERT(NULL != pFltDeviceObject);
    ASSERT(NULL != IoStackLocation->CompletionRoutine);

    pDeviceExtension = (PDEVICE_EXTENSION)pFltDeviceObject->DeviceExtension;
    pDevContext = pDeviceExtension->pDevContext;

    pNoRebootCompContext = (PCOMPLETION_CONTEXT)ExAllocateFromNPagedLookasideList(&DriverContext.CompletionContextPool);
    if (NULL == pNoRebootCompContext) {
        Status = STATUS_NO_MEMORY;
        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_COMPLETION_CONTEXT_ALLOC, pDevContext);
        goto Cleanup;
    }

    RtlZeroMemory(pNoRebootCompContext, sizeof(PCOMPLETION_CONTEXT));
    pNoRebootCompContext->bMemAllocFromPool = TRUE;

    // Set the Context Info. 
    // As completion routine would have different irp stack location
    pNoRebootCompContext->WriteCompFields.llOffset = IoStackLocation->Parameters.Write.ByteOffset.QuadPart;
    pNoRebootCompContext->WriteCompFields.ulLength = IoStackLocation->Parameters.Write.Length;
    pNoRebootCompContext->WriteCompFields.pDevContext = pDevContext;

    //As pCompletionContext is a wrapper, Store actual completion routine detail
    pNoRebootCompContext->CompletionRoutine = pCompletionRoutine;
    pNoRebootCompContext->Context = &pNoRebootCompContext->WriteCompFields;
    pNoRebootCompContext->FltDeviceObject = pFltDeviceObject;

    //Store the completion detail required to call upper layer completion
    pNoRebootCompContext->CompletionRoutinePatched = IoStackLocation->CompletionRoutine;
    pNoRebootCompContext->ContextPatched = IoStackLocation->Context;

    //Exchange the Completion routine of upper driver
    IoStackLocation->CompletionRoutine = InNoRebootCompletionRoutine;
    IoStackLocation->Context = pNoRebootCompContext;

Cleanup:
    return Status;
}

NTSTATUS InNoRebootCompletionRoutine (
    IN PDEVICE_OBJECT UpperDeviceObject,
    IN PIRP           Irp,
    IN PVOID          Context
    ) 
/*++

Routine Description:
    Completion Routine for NoReboot Driver
    IRQL <=DISPATCH_LEVEL

Arguments:
    UpperDeviceObject - Device Object sent by IoManager
    Irp - IRP sent by IoManager
    Context - pCompletionContext set during IoSetCompletion Routine

Return Value:
    Return Status send by upper driver

--*/

{
    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    PIO_STACK_LOCATION IoNextStackLocation = NULL;

    // Don’t call IoMarkPending
    PCOMPLETION_CONTEXT pCompletionContext = (PCOMPLETION_CONTEXT) Context;

    // Call actual completion routine
    Status = (*pCompletionContext->CompletionRoutine)
             (pCompletionContext->FltDeviceObject,
              Irp, 
              pCompletionContext->Context);

    // Reset the Info in Irp
    IoNextStackLocation = IoGetNextIrpStackLocation(Irp);
    IoNextStackLocation->CompletionRoutine = pCompletionContext->CompletionRoutinePatched;
    IoNextStackLocation->Context = pCompletionContext->ContextPatched;

    // Call UpperDriver completion routine
    Status = (*pCompletionContext->CompletionRoutinePatched)(UpperDeviceObject, Irp, pCompletionContext->ContextPatched);

    // Free Memory in case allocated
    if (pCompletionContext->bMemAllocFromPool) {
        ExFreeToNPagedLookasideList(&DriverContext.CompletionContextPool, pCompletionContext);
    }
    return Status;
}

PDEVICE_OBJECT InGetFltDeviceObject (
    PDEVICE_OBJECT LowerDeviceObject)
/*++

Routine Description:
    Map the LowerDeviceObject to FilterDeviceObject
    IRQL <=DISPATCH_LEVEL

Arguments:
    LowerDeviceObject:- Device Object sent by IoManager

Return Value:
    Return Filter Device Object

--*/

{
    PDEVICE_OBJECT FltDeviceObject = NULL;
    PLIST_ENTRY    pDevContListEntry = NULL;
    PDEV_CONTEXT   pDevContext = NULL;
    KIRQL          OldIrql = 0;

    ASSERT(DriverContext.IsDispatchEntryPatched);
    ASSERT(LowerDeviceObject->DriverObject == DriverContext.pTargetDriverObject);

    // This function should only get called for Dispatch Entry of Patched Driver
    // Invalid Case, ASSERT
    if (LowerDeviceObject->DriverObject != DriverContext.pTargetDriverObject) {
        // Limit the EventLog
        if (!DriverContext.bNoRebootWrongDevObj) {
            InDskFltWriteEvent(INDSKFLT_ERROR_MISMATCH_LOWER_DRIVER_OBJECT);
            DriverContext.bNoRebootWrongDevObj = true;
        }
    }

    // Find the match
    KeAcquireSpinLock(&DriverContext.Lock, &OldIrql);
    for (pDevContListEntry = DriverContext.HeadForDevContext.Flink; 
         pDevContListEntry != &DriverContext.HeadForDevContext;
         pDevContListEntry = pDevContListEntry->Flink) {
        pDevContext = (PDEV_CONTEXT)pDevContListEntry;
        KeAcquireSpinLockAtDpcLevel(&pDevContext->Lock);
        if (pDevContext->TargetDeviceObject == LowerDeviceObject) {
            // Should get match only with enumerated device
            // Invalid Case, ASSERT
            // DTAP:- LowerDeviceObject can be older i.e Remove Device followed by any AddDevice of disk 
            //        Indskflt do not set LowerDeviceobject to null in remove irp
            //        New Devices are added to the head and IsDeviceEnumerated is set to false
            //        As Devcontext is cleanup through thread there is possibility of two devices 
            //        with same LowerDeviceObject
            //        In Such scneraion, New context will be skipped due to IsDeviceEnumerated
            //        and older context would have pDevContext->FilterDeviceObject as NULL.
            //        It would result in bypass mode
            if (pDevContext->IsDeviceEnumerated) {
                FltDeviceObject = pDevContext->FilterDeviceObject;
                KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
                break;
            }
        }
        KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
    }
    KeReleaseSpinLock(&DriverContext.Lock, OldIrql);  
    return FltDeviceObject;
}


NTSTATUS InNoRebootDispatchFunction(
    IN PDEVICE_OBJECT LowerDeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

    Map the Dispatch Function
    IRQL <=DISPATCH_LEVEL

Arguments:
    LowerDeviceObject:- Device Object sent by IoManager
    Irp - IRP sent by IoManager

Return Value:
    Return Value send by Lower Driver

--*/
{
    PDRIVER_DISPATCH pDispatchEntryFunction = NULL;
    PDEVICE_OBJECT FltDeviceObject = NULL;
    BOOLEAN bNoRebootLayer = FALSE;
    NTSTATUS Status = STATUS_UNSUCCESSFUL;

    PIO_STACK_LOCATION IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    pDispatchEntryFunction = DriverContext.TargetMajorFunction[IoStackLocation->MajorFunction];

    ASSERT(NULL != pDispatchEntryFunction);

    // Invalid Case, ASSERT
    if (NULL == *pDispatchEntryFunction) {
        // Limit the EventLog
        if (!DriverContext.bNoRebootWrongDispatchEntry) {
            InDskFltWriteEvent(INDSKFLT_ERROR_INCORRECT_DISPATCH_ENTRY, IoStackLocation->MajorFunction);
            DriverContext.bNoRebootWrongDispatchEntry = true;
        }

        return Status;
    }

    FltDeviceObject = InGetFltDeviceObject(LowerDeviceObject);

    //Call came to Indskflt. False case
    if (NULL == FltDeviceObject) {
        Status = (*pDispatchEntryFunction)(LowerDeviceObject, Irp);
        goto Cleanup;
    }
    bNoRebootLayer = TRUE;
    switch (IoStackLocation->MajorFunction) {
        case IRP_MJ_WRITE:
            Status = InMageFltDispatchWrite(FltDeviceObject, Irp, bNoRebootLayer, pDispatchEntryFunction);
            break;
        case IRP_MJ_PNP:
            Status = InMageFltDispatchPnp(FltDeviceObject, Irp, bNoRebootLayer, pDispatchEntryFunction);
            break;
        case IRP_MJ_POWER:
            Status = InMageFltDispatchPower(FltDeviceObject, Irp, bNoRebootLayer, pDispatchEntryFunction);
            break;
        case IRP_MJ_CLEANUP:
            Status = InMageFltCleanup(FltDeviceObject, Irp, bNoRebootLayer, pDispatchEntryFunction);
            break;
        case IRP_MJ_DEVICE_CONTROL:
            Status = InMageFltDeviceControl(FltDeviceObject, Irp, bNoRebootLayer, pDispatchEntryFunction);
            break;
        default:
            // Invalid Case
            break;
    }
Cleanup:
    return Status;
}

/*
Description     : This function returns the list of symbolic links for Generic disk device interface class. Also, retries on failure.

Input           : Nothing

Output          : Pointer to set of WCHAR symbolic links

returns         : NTSTATUS

Comments :
*/
NTSTATUS
GetDeviceSymLinkList(
_In_    LPGUID   DeviceInterfaceGuid,
_Out_ PWCHAR *ppSymLinkList
)
{
    UNICODE_STRING      usDiskDeviceSymLinkList;
    NTSTATUS            Status;

    Status = IoGetDeviceInterfaces(DeviceInterfaceGuid, NULL, 0, (PWCHAR*)ppSymLinkList);

    RtlInitUnicodeString(&usDiskDeviceSymLinkList, *ppSymLinkList);

    if (!NT_SUCCESS(Status) || (usDiskDeviceSymLinkList.Length == 0)) {
        // Attempting one more time in case of No Devices found for a given criteria or if this call fails
        Status = IoGetDeviceInterfaces(DeviceInterfaceGuid, NULL, 0, (PWCHAR *)ppSymLinkList);

        RtlInitUnicodeString(&usDiskDeviceSymLinkList, *ppSymLinkList);

        if (!NT_SUCCESS(Status) || (usDiskDeviceSymLinkList.Length == 0)) {
            InVolDbgPrint(DL_INFO, DM_DRIVER_INIT, ("GetDeviceSymLinkList : Failed to get the list of symbolic links \n"));
            InDskFltWriteEvent(INDSKFLT_ERROR_ENUMERATE_DEVICE, Status);
        }
    }
    return Status;
}

bool IS_GENERIC_DISK_STACK(PDEVICE_OBJECT pDevObj) {
    UNICODE_STRING str;
    RtlInitUnicodeString(&str, L"Disk");
    if (pDevObj) {
        InDskFltWriteEvent(INDSKFLT_INFO_DEVICE_DRIVER_NAME, pDevObj->DriverObject->DriverExtension->ServiceKeyName.Buffer);
        if (0 == RtlCompareUnicodeString(&(pDevObj->DriverObject->DriverExtension->ServiceKeyName), &str, TRUE))
            return true;
    }
    return false;
}

/*
Description : This function accepts the top device object, parse through until generic disk stack and returns the targeet device object. 
              This function also verifies the pagable flag on all devices in the stack.

Parameters :  Top of the device object
              bPagingDevice - TRUE or FALSE    

Return values :  Target disk device object or NULL

Comments :
DO_POWER_PAGABLE is set on all devices                -> bPagingDevice = FALSE
DO_POWER_PAGABLE flag is unset on at least one device -> bPagingDevice = TRUE

DO_POWER_PAGABLE flag is not set/unset by the lower drivers below generic disk driver, so limiting the search upto generic disk driver
*/
PDEVICE_OBJECT
GetTargetDiskDeviceObject(PDEVICE_OBJECT pDevObj, BOOLEAN *bPagingDevice)
{
    PDEVICE_OBJECT pTempDevObj = NULL;
    BOOLEAN bPagingDev = FALSE;// Start with no paging device
            
    ASSERT((NULL != pDevObj));

    if (NULL == pDevObj) {
        goto Cleanup;
    }

    // Reference device Object. As caller derefs it post this call
    ObReferenceObject(pDevObj);

    while (NULL != pDevObj) {
        if (!bPagingDev && !TEST_FLAG(pDevObj->Flags, DO_POWER_PAGABLE)) // flag is unset, paging device is TRUE
            bPagingDev = TRUE;

        if (IS_GENERIC_DISK_STACK(pDevObj)) {
            // Note that reference is held
            if (!DriverContext.pTargetDriverObject) {
                ASSERT(NULL != pDevObj->DriverObject);
                DriverContext.pTargetDriverObject = pDevObj->DriverObject;
                InVolDbgPrint(DL_INFO, DM_DRIVER_INIT, ("GetTargetDiskDeviceObject : Target Driver object %I64X\n", 
                              pDevObj->DriverObject));
            }
            break;
        }
        pTempDevObj = pDevObj;
        pDevObj = IoGetLowerDeviceObject(pDevObj);
        ObDereferenceObject(pTempDevObj);
        pTempDevObj = NULL;
    }
    *bPagingDevice = bPagingDev;
Cleanup:
    return pDevObj;
}

void
ReplaceDiskDriverMajorFunctions()
{
    DriverContext.IsDispatchEntryPatched = true;

    InterlockedExchangePointer((PVOID *)&DriverContext.TargetMajorFunction[IRP_MJ_PNP], DriverContext.pTargetDriverObject->MajorFunction[IRP_MJ_PNP]);

    InterlockedExchangePointer((PVOID *)&DriverContext.pTargetDriverObject->MajorFunction[IRP_MJ_PNP], InNoRebootDispatchFunction);

    InterlockedExchangePointer((PVOID *)&DriverContext.TargetMajorFunction[IRP_MJ_POWER], DriverContext.pTargetDriverObject->MajorFunction[IRP_MJ_POWER]);

    InterlockedExchangePointer((PVOID *)&DriverContext.pTargetDriverObject->MajorFunction[IRP_MJ_POWER], InNoRebootDispatchFunction);

    InterlockedExchangePointer((PVOID *)&DriverContext.TargetMajorFunction[IRP_MJ_WRITE], DriverContext.pTargetDriverObject->MajorFunction[IRP_MJ_WRITE]);

    InterlockedExchangePointer((PVOID *)&DriverContext.pTargetDriverObject->MajorFunction[IRP_MJ_WRITE], InNoRebootDispatchFunction);

    InterlockedExchangePointer((PVOID *)&DriverContext.TargetMajorFunction[IRP_MJ_DEVICE_CONTROL], DriverContext.pTargetDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]);

    InterlockedExchangePointer((PVOID *)&DriverContext.pTargetDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL], InNoRebootDispatchFunction);

    InterlockedExchangePointer((PVOID *)&DriverContext.TargetMajorFunction[IRP_MJ_CLEANUP], DriverContext.pTargetDriverObject->MajorFunction[IRP_MJ_CLEANUP]);

    InterlockedExchangePointer((PVOID *)&DriverContext.pTargetDriverObject->MajorFunction[IRP_MJ_CLEANUP], InNoRebootDispatchFunction);
}

/*
Description: 
    This function enumerates disk device objects in no-reboot mode and device objects which are not already added to disk list. 

Return Value: 
    STATUS_SUCCESS                : If all disk device objects are added successfully.
    STATUS_INVALID_DEVICE_REQUEST : Function is called in other than no-reboot mode.
    STATUS_NOT_FOUND              : Target Driver Object is not set.
    STATUS_UNSUCCESSFUL           : IoEnumerateDeviceObjectList generic failures
    STATUS_INSUFFICIENT_RESOURCES : Failed to allocate memory.
*/

NTSTATUS
AddDiskDeviceObjectsIfNotAdded()
{
    NTSTATUS    Status = STATUS_SUCCESS;

    ULONG           ulActualNumberDeviceObjects = 0;
    ULONG           ulNumberDeviceObjects = 0;
    ULONG           ulDeviceObjectListSize = 0;
    PDEVICE_OBJECT  *DeviceObjectList = NULL;
    PLIST_ENTRY     pDevExtList = NULL;
    PDEV_CONTEXT    pDevContext = NULL;
    BOOLEAN         bFound = FALSE;
    ULONG           ulIdx = 0;
    PDEVICE_OBJECT  DeviceObject = NULL;
    KIRQL           oldIrql;
    BOOLEAN         bPagingDevice = FALSE;
    PDEVICE_OBJECT  pCurrentDeviceObject = NULL;
    PDEVICE_OBJECT  pNextDeviceObject = NULL;

    if (NoRebootMode != DriverContext.eDiskFilterMode) {
        Status = STATUS_INVALID_DEVICE_REQUEST;
        goto Cleanup;
    }

    if (NULL == DriverContext.pTargetDriverObject){
        Status = STATUS_NOT_FOUND;
        goto Cleanup;
    }

    // Get total number of disk devices
    Status = IoEnumerateDeviceObjectList(
        DriverContext.pTargetDriverObject,
        NULL,
        0,
        &ulActualNumberDeviceObjects
        );

    if ((STATUS_BUFFER_TOO_SMALL != Status) || (0 == ulActualNumberDeviceObjects)) {
        if (STATUS_SUCCESS == Status) {
            Status = STATUS_UNSUCCESSFUL;
        }
        goto Cleanup;
    }

    ulNumberDeviceObjects = ulActualNumberDeviceObjects;
    ulDeviceObjectListSize = ulActualNumberDeviceObjects * sizeof(PDEVICE_OBJECT);
    DeviceObjectList = (PDEVICE_OBJECT*)ExAllocatePoolWithTag(NonPagedPool, ulDeviceObjectListSize, TAG_GENERIC_NON_PAGED);

    if (NULL == DeviceObjectList) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Cleanup;
    }

    RtlZeroMemory(DeviceObjectList, ulDeviceObjectListSize);

    Status = IoEnumerateDeviceObjectList(
        DriverContext.pTargetDriverObject,
        DeviceObjectList,
        ulDeviceObjectListSize,
        &ulActualNumberDeviceObjects
        );

    if (!NT_SUCCESS(Status)) {
        goto Cleanup;
    }

    for (ulIdx = 0; ulIdx < ulNumberDeviceObjects; ulIdx++) {

        DeviceObject = DeviceObjectList[ulIdx];

        if (NULL == DeviceObject) {
            continue;
        }

        if (FILE_DEVICE_DISK != DeviceObject->DeviceType) {
            ObDereferenceObject(DeviceObject);
            continue;
        }

        bPagingDevice = FALSE;
        pCurrentDeviceObject = IoGetAttachedDeviceReference(DeviceObject);

        while (NULL != pCurrentDeviceObject)
        {
            if (!TEST_FLAG(pCurrentDeviceObject->Flags, DO_POWER_PAGABLE))
            {
                ObDereferenceObject(pCurrentDeviceObject);
                bPagingDevice = TRUE;
                break;
            }

            if (DeviceObject == pCurrentDeviceObject){
                ObDereferenceObject(pCurrentDeviceObject);
                break;
            }

            pNextDeviceObject = IoGetLowerDeviceObject(pCurrentDeviceObject);
            ObDereferenceObject(pCurrentDeviceObject);
            pCurrentDeviceObject = pNextDeviceObject;
            pNextDeviceObject = NULL;
        }

        bFound = FALSE;

        KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
        pDevExtList = DriverContext.HeadForDevContext.Flink;

        for (; pDevExtList != &DriverContext.HeadForDevContext; pDevExtList = pDevExtList->Flink)
        {
            pDevContext = (PDEV_CONTEXT)pDevExtList;
            if (pDevContext->TargetDeviceObject == DeviceObject) {
                bFound = TRUE;
                break;
            }
        }
        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);

        if (!bFound) {
            Status = InMageFltAddDevice(DriverContext.DriverObject, DeviceObject, TRUE, bPagingDevice);
            if (!NT_SUCCESS(Status)) {
                InDskFltWriteEvent(INDSKFLT_NOREBOOT_ADD_DISK_FAILURE2, Status);
            }
        }

        ObDereferenceObject(DeviceObject);
    }

Cleanup:
    if (NULL != DeviceObjectList) {
        ExFreePoolWithTag(DeviceObjectList, TAG_GENERIC_NON_PAGED);
    }

    return Status;
}

/*
Description: This function creates the indskflt device object for given symbolic link in no-reboot driver mode.

Parameters : Set of NULL-terminated symbolic links

Return Value: None

Note : This function frees the buffer allocated for symbolic link list
*/

VOID 
CreateFilterDeviceWithoutReboot(
    _In_z_ PWCHAR SymLinkList
    )
{
    PWCHAR                     ListBuffer = NULL;
    UNICODE_STRING             usDiskDeviceSymbolicLink;
    PFILE_OBJECT               pFileObj = NULL;
    PDEVICE_OBJECT             pDevObj = NULL;
    PDEVICE_OBJECT             pDiskDevObj = NULL; 
    NTSTATUS                   Status;
    BOOLEAN                    bPagingDevice = TRUE; // Let's assume paging

    ListBuffer = SymLinkList;

    RtlInitUnicodeString(&usDiskDeviceSymbolicLink, SymLinkList);
    InDskFltWriteEvent(INDSKFLT_INFO_SYMLIST_NAME, SymLinkList);

    if (0 == usDiskDeviceSymbolicLink.Length) {
        ListBuffer = NULL;
        goto cleanup;
    }

    do {
        InVolDbgPrint(DL_INFO, DM_DRIVER_INIT, ("CreateFilterDeviceWithoutReboot : Get Device Object pointer for UC string : %ws Length : %d \n", 
                      usDiskDeviceSymbolicLink.Buffer, usDiskDeviceSymbolicLink.Length));

        Status = IoGetDeviceObjectPointer(&usDiskDeviceSymbolicLink, FILE_READ_DATA, &pFileObj, &pDevObj);
        if (!NT_SUCCESS(Status)) {
            InDskFltWriteEvent(INDSKFLT_ERROR_FAILURE_ENUMERATING_DISK_DEVICE,
                usDiskDeviceSymbolicLink.Buffer,
                Status,
                SourceLocationNoRebootCreateFiltDevGetObjFailed);

            InVolDbgPrint(DL_ERROR, DM_DRIVER_INIT, ("CreateFilterDeviceWithoutReboot : Failed to get top dev object : %ws, Length : %d \n",
                usDiskDeviceSymbolicLink.Buffer, usDiskDeviceSymbolicLink.Length));
        }
        else {
            // Refcount: pFileObj       1
            // Refcount: pDevObj        0

            InDskFltWriteEvent(INDSKFLT_INFO_DEVICE_SYMLINK_NAME, usDiskDeviceSymbolicLink.Buffer);
            InDskFltWriteEvent(INDSKFLT_INFO_DEVICE_DRIVER_NAME, pDevObj->DriverObject->DriverName.Buffer);

            ASSERT(NULL != pDevObj);
            ASSERT(NULL != pFileObj);

            // Identify disk stack and retrieve pointer to target disk device object
            // verify whether the stack is on paging path
            // Following call will also det Disk Driver Object
            pDiskDevObj = GetTargetDiskDeviceObject(pDevObj, &bPagingDevice);
            // Refcount: pFileObj       1
            // Refcount: pDevObj        0
            // Refcount: pDiskDevObj    1
            if (NULL != pDiskDevObj) {
                ObDereferenceObject(pFileObj);
                ObDereferenceObject(pDiskDevObj);
                // Refcount: pFileObj       0
                // Refcount: pDevObj        0
                // Refcount: pDiskDevObj    0
                break;
            }

            // Refcount: pFileObj       1
            // Refcount: pDevObj        0
            // Refcount: pDiskDevObj    0

            // On windows 10 pro we get volume stack with given disk device interface
            // In these case look at realdevice inside VPB
            // Real Device inside VPB points to physical device on disk stack
            // We need to get top of physical device stack and walk
            // through the stack to figure disk device
            if (NULL != pFileObj) {
                // Refcount: pFileObj       1
                // Refcount: pDevObj        0
                // Refcount: pDiskDevObj    0

                if (pFileObj->Vpb && pFileObj->Vpb->RealDevice) {
                    // Get Top of disk device stack
                    // This call will take a reference to top of real device which will be
                    // released later.
                    PDEVICE_OBJECT  DeviceObject = IoGetAttachedDeviceReference(pFileObj->Vpb->RealDevice);
                    // Refcount: pFileObj       1
                    // Refcount: pDevObj        0
                    // Refcount: pDiskDevObj    0
                    // Refcount: pDiskDevObj    0
                    // Refcount: DeviceObject   1

                    pDiskDevObj = GetTargetDiskDeviceObject(DeviceObject, &bPagingDevice);
                    ObDereferenceObject(DeviceObject);
                    // Refcount: pFileObj       1
                    // Refcount: pDevObj        0
                    // Refcount: pDiskDevObj    1
                    // Refcount: pDiskDevObj    0
                    // Refcount: DeviceObject   0

                    if (NULL != pDiskDevObj) {
                        ObDereferenceObject(pDiskDevObj);
                        ObDereferenceObject(pFileObj);
                        // Refcount: pFileObj       0
                        // Refcount: pDevObj        0
                        // Refcount: pDiskDevObj    0
                        // Refcount: pDiskDevObj    0
                        // Refcount: DeviceObject   0

                        break;
                    }
                }

                ObDereferenceObject(pFileObj);
                // Refcount: pFileObj       0
                // Refcount: pDevObj        0
                // Refcount: pDiskDevObj    0
                // Refcount: pDiskDevObj    0
                // Refcount: DeviceObject   0

            }
        }
        SymLinkList = SymLinkList + usDiskDeviceSymbolicLink.Length / sizeof(WCHAR)+1;
        usDiskDeviceSymbolicLink.Length = usDiskDeviceSymbolicLink.MaximumLength = 0;
        usDiskDeviceSymbolicLink.Buffer = NULL;
        RtlInitUnicodeString(&usDiskDeviceSymbolicLink, SymLinkList);
    } while (usDiskDeviceSymbolicLink.Length);

    // None of above calls succeeded in getting disk driver object
    // We will be using following method to find disk driver object
    // Open a handle to partmgr control device
    // Through partmgr control device, get partmgr driver object
    // Now look at disk devices with partmgr
    // Enumerate through these devices and find disk driver context.
    if (NULL == DriverContext.pTargetDriverObject) {
        UNICODE_STRING              PartmgrControlDeviceString;

        RtlInitUnicodeString(&PartmgrControlDeviceString, DD_PARTMGR_CONTROL_DEVICE_NAME);
        InDskFltWriteEvent(INDSKFLT_INFO_NOREBOOT_PARTMGRDEVICE_ENUMERATION, PartmgrControlDeviceString.Buffer);
        Status = IoGetDeviceObjectPointer(&PartmgrControlDeviceString, FILE_READ_DATA, &pFileObj, &pDevObj);

        if (STATUS_SUCCESS != Status) {
            goto cleanup;
        }

        PDRIVER_OBJECT  pDrvObject = pDevObj->DriverObject;

        PDEVICE_OBJECT *DeviceObjectList = NULL;
        ULONG   ulDevices = 0;
        Status = GetDeviceObjectList(pDrvObject, &DeviceObjectList, &ulDevices);
        ObDereferenceObject(pFileObj);

        if (STATUS_SUCCESS != Status) {
            goto cleanup;
        }

        for (ULONG ulIdx = 0; ulIdx < ulDevices; ulIdx++) {

            pDevObj = DeviceObjectList[ulIdx];

            if (NULL == pDevObj) {
                continue;
            }

            // If disk driver context is already figured only release the reference for
            // current device object and continue.
            if (NULL != DriverContext.pTargetDriverObject) {
                ObDereferenceObject(pDevObj);
                continue;
            }

            if (FILE_DEVICE_DISK != pDevObj->DeviceType) {
                ObDereferenceObject(pDevObj);
                continue;
            }

            pDiskDevObj = GetTargetDiskDeviceObject(pDevObj, &bPagingDevice);

            if (NULL != pDiskDevObj) {
                ObDereferenceObject(pDiskDevObj);
            }
        }
    }

    // Following phase works in 2 manners
    //  a. Add storage pool physical disks
    //  b. Add missing disks not enumerated till now.
    Status = AddDiskDeviceObjectsIfNotAdded();
    if (!NT_SUCCESS(Status)) {
        InDskFltWriteEvent(INDSKFLT_NOREBOOT_ENUM_DISKDRIVER_FAILURE, Status);
    }

cleanup :
    if (NULL != ListBuffer)
       ExFreePool(ListBuffer);
}

/*
Description: 
        This function opens a handle to given device stack. This prevents device stack from getting IRP_MN_REMOVE_DEVICE
        from PNP Manager.

Parameters : 
        DeviceObject: Disk Device Object
        FileHandle:     Handle that is returned as part of opening device.

Return Value: 
    STATUS_INVALID_PARAMETER: If DeviceObject or, FileHandle is NULL
    Status: Return status of IOCTL_STORAGE_GET_DEVICE_NUMBER and ZwOpenFile.
*/
NTSTATUS
OpenHandleToDevice(
    _In_  PDEVICE_OBJECT DeviceObject,
    _Out_ PHANDLE FileHandle
)
{

    WCHAR                   diskName[50];
    UNICODE_STRING          diskNameStr;
    NTSTATUS                Status;
    IO_STATUS_BLOCK         IoStatus;
    STORAGE_DEVICE_NUMBER   StorageDevNum = { 0 };

    if ((NULL == DeviceObject) || (NULL == FileHandle)) {
        Status = STATUS_INVALID_PARAMETER;
        goto Cleanup;
    }

    *FileHandle = NULL;

    Status = GetDeviceNumber(DeviceObject,
        &StorageDevNum,
        sizeof(StorageDevNum));

    if (!NT_SUCCESS(Status)) {
        goto Cleanup;
    }

    // Prepare unicode string for Disk name string
    RtlStringCbPrintfW(diskName, sizeof(diskName), DISK_NAME_FORMAT, StorageDevNum.DeviceNumber);
    RtlUnicodeStringInit(&diskNameStr, diskName);

    OBJECT_ATTRIBUTES   ObjectAttributes;
    InitializeObjectAttributes(&ObjectAttributes,
        &diskNameStr,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL,
        NULL);


    Status = ZwOpenFile(FileHandle,
        GENERIC_READ,
        &ObjectAttributes,
        &IoStatus,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        FILE_NON_DIRECTORY_FILE);

    if (!NT_SUCCESS(Status)) {
        InDskFltWriteEvent(INDSKFLT_FAILURE_OPEN_DEVICE, 
                           diskNameStr.Buffer,
                           IoStatus.Status,
                           SourceLocationNoRebootCreateFiltDevGetObjFailed);
    }
Cleanup:
    return Status;
}

/*
Description:
    This function closes all open handles to devices that were opened as part of no-reboot mode.

Parameters :
    None

Return Value:
    None.
*/
VOID
CloseDeviceHandles()
{
    KIRQL           oldIrql;
    PLIST_ENTRY     pDevExtList;
    PDEV_CONTEXT    pDevContext;
    HANDLE          DeviceHandle;

    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
    pDevExtList = DriverContext.HeadForDevContext.Flink;

    for (; pDevExtList != &DriverContext.HeadForDevContext; pDevExtList = pDevExtList->Flink)
    {
        pDevContext = (PDEV_CONTEXT)pDevExtList;
        KeAcquireSpinLockAtDpcLevel(&pDevContext->Lock);
        if (NULL == pDevContext->DeviceHandle) {
            KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
            continue;
        }

        DeviceHandle = pDevContext->DeviceHandle;
        pDevContext->DeviceHandle = NULL;
        KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);
        KeReleaseSpinLock(&DriverContext.Lock, oldIrql);

         ZwClose(DeviceHandle);

         KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);
         pDevExtList = &DriverContext.HeadForDevContext;
    }
    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);
}


NTSTATUS
GetDeviceObjectList(PDRIVER_OBJECT  pDriverObject,
    PDEVICE_OBJECT **pDeviceObjectList,
    PULONG   pulDevices)
{

    ULONG   ulActualNumberDeviceObjects;
    PDEVICE_OBJECT *DeviceObjectList = NULL;

    // Get total number of disk devices
    NTSTATUS Status = IoEnumerateDeviceObjectList(
        pDriverObject,
        NULL,
        0,
        &ulActualNumberDeviceObjects
    );

    if ((STATUS_BUFFER_TOO_SMALL != Status) || (0 == ulActualNumberDeviceObjects)) {
        if (STATUS_SUCCESS == Status) {
            Status = STATUS_UNSUCCESSFUL;
        }
        goto Cleanup;
    }

    ULONG ulDeviceObjectListSize = ulActualNumberDeviceObjects * sizeof(PDEVICE_OBJECT);
    DeviceObjectList = (PDEVICE_OBJECT*)ExAllocatePoolWithTag(NonPagedPool, ulDeviceObjectListSize, TAG_GENERIC_NON_PAGED);

    if (NULL == DeviceObjectList) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Cleanup;
    }

    RtlZeroMemory(DeviceObjectList, ulDeviceObjectListSize);

    Status = IoEnumerateDeviceObjectList(
        pDriverObject,
        DeviceObjectList,
        ulDeviceObjectListSize,
        &ulActualNumberDeviceObjects
    );

    if (!NT_SUCCESS(Status)) {
        ExFreePoolWithTag(DeviceObjectList, TAG_GENERIC_NON_PAGED);
        goto Cleanup;
    }

    *pDeviceObjectList = DeviceObjectList;
    *pulDevices = ulActualNumberDeviceObjects;
Cleanup:
    return Status;
}