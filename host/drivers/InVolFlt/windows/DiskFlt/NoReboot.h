#pragma once

#define INVOKE_ON_ALL     (SL_INVOKE_ON_SUCCESS | \
                           SL_INVOKE_ON_ERROR | \
                           SL_INVOKE_ON_CANCEL)

#ifndef DD_PARTMGR_CONTROL_DEVICE_NAME
#define DD_PARTMGR_CONTROL_DEVICE_NAME            L"\\Device\\PartmgrControl"
#endif
#define IS_IRP_PAGING_FLAG_SET(Irp)     ((Irp) && (TEST_FLAG(Irp->Flags, IRP_PAGING_IO) || TEST_FLAG(Irp->Flags, IRP_SYNCHRONOUS_PAGING_IO)))
#define IS_PAGEFILE_DEVICE(p)           ((p) && (p->IsPageFileDevicePossible))

struct _DEV_CONTEXT;

typedef struct _WRITE_COMPLETION_FIELDS
{
    LONGLONG              llOffset;
    ULONG                 ulLength;
    struct _DEV_CONTEXT   *pDevContext;
} WRITE_COMPLETION_FIELDS, *PWRITE_COMPLETION_FIELDS;

typedef struct _COMPLETION_CONTEXT
{
    PDEVICE_OBJECT FltDeviceObject;                  // Filter Device Object to Pass to Completion Routine
    PIO_COMPLETION_ROUTINE CompletionRoutine;        // Store Inmage Completion Routine
    PVOID Context;                                   // Context to be passed to Inmage Completion Routine
    WRITE_COMPLETION_FIELDS WriteCompFields;         // Used for IRP_MJ_WRITE
    PIO_COMPLETION_ROUTINE CompletionRoutinePatched; // Store Completion Routine of Upper Driver
    PVOID ContextPatched;                            // Store Context of Upper Driver 
    BOOLEAN bMemAllocFromPool;                       // Set for dynamic memory allocation
} COMPLETION_CONTEXT, *PCOMPLETION_CONTEXT;

NTSTATUS
InSetCompletionRoutine (
    __in PIRP                   Irp,
    __in PIO_COMPLETION_ROUTINE pCompletionRoutine,
    __in PDEVICE_OBJECT         pFltDeviceObject
    );

IO_COMPLETION_ROUTINE InNoRebootCompletionRoutine;

NTSTATUS
GetDeviceSymLinkList(
_In_ LPGUID     deviceIntfGuid,
_Out_ PWCHAR*   symbolicNameList
);

NTSTATUS
GetDeviceObjectList(PDRIVER_OBJECT  pDriverObject,
    PDEVICE_OBJECT **pDeviceObjectList,
    PULONG   pulDevices);

VOID
CreateFilterDeviceWithoutReboot(
_In_z_ PWCHAR SymLinkList
);

PDEVICE_OBJECT
GetTargetDiskDeviceObject(
_In_   PDEVICE_OBJECT DeviceObject,
_Out_  BOOLEAN *bPagingDevice
);

void
ReplaceDiskDriverMajorFunctions(
);


NTSTATUS
AddDiskDeviceObjectsIfNotAdded();

bool
IsWriteCanbePageable(_In_ struct _DEV_CONTEXT *pDevContext, 
                     _In_ PIRP Irp, 
                     _In_ BOOLEAN bNoRebootLayer);

FORCEINLINE
NTSTATUS 
InCallDriver (
    __in PDEVICE_OBJECT pTargetDeviceObject,
    __in PIRP Irp,
    __in BOOLEAN bNoRebootLayer,
    __in PDRIVER_DISPATCH pDispatchEntryFunction
    )
/*++
 
    Routine Description:
    Call IoCallDriver for Reboot Driver
    Call LowerDriver DispatchEntryFunction store in device Enumeration
    IRQL <=DISPATCH_LEVEL

    Arguments:
        pTargetDeviceObject - Target Device Object
        Irp - IRP sent by IoManager
        bNoRebootLayer - Flag to indicate caller stack layer
        pDispatchEntryFunction - DispatchEntryFunction stored of lower driver

     Return Value:
        Return Status send  by lower driver

 --*/
 
{
    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    if (bNoRebootLayer) {
        Status = (*pDispatchEntryFunction)(pTargetDeviceObject, Irp);
    } else {
        Status = IoCallDriver(pTargetDeviceObject, Irp);
    }
    return Status;
}

FORCEINLINE
VOID InSkipCurrentIrpStackLocation (
    PIRP Irp,
    BOOLEAN bNoRebootLayer
    ) 
/*++

Routine Description:
    Skip the current IRP stack for Reboot Driver
    NO OP for No Reboot Driver
    IRQL <=DISPATCH_LEVEL

Arguments:
    Irp - IRP sent by IoManager
    bNoRebootLayer - Flag to indicate caller stack layer

Return Value:
    VOID

--*/
{
    if (!bNoRebootLayer) {
        IoSkipCurrentIrpStackLocation(Irp);
    }
}
