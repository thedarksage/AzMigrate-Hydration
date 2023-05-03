#include "RegistryHelper.h"


NTSTATUS OpenKeyReadWrite(PUNICODE_STRING keyName, HANDLE  *hRegistry,BOOLEAN bCreateIfFail)
{
NTSTATUS status;
OBJECT_ATTRIBUTES attr;

    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    InitializeObjectAttributes(&attr,
        keyName,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL, NULL);

    status = ZwOpenKey(hRegistry, GENERIC_READ, &attr);
    
    if (!NT_SUCCESS(status) && bCreateIfFail) {
        // May be the key is not present try creating it.
        status = ZwCreateKey(hRegistry, GENERIC_READ | GENERIC_WRITE, &attr,
                            0, NULL, REG_OPTION_NON_VOLATILE, NULL);
    }
 
    return status;
}

NTSTATUS ReadULONG(HANDLE hRegistry,PCWSTR nameString, ULONG * value, ULONG32 ulDefaultValue, BOOLEAN bCreateIfFail)
{
NTSTATUS status;
UNICODE_STRING name;
unsigned long actualSize;
UCHAR buffer[64];
PKEY_VALUE_PARTIAL_INFORMATION regValue;

    ASSERT(NULL != value);
    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    *value = ulDefaultValue;

    
    regValue = (PKEY_VALUE_PARTIAL_INFORMATION)buffer;

    RtlInitUnicodeString(&name, nameString);

    status = ZwQueryValueKey(hRegistry,
                              &name, 
                              KeyValuePartialInformation,
                              (PVOID)regValue,
                              sizeof(buffer),
                              &actualSize);
    if (status == STATUS_SUCCESS) {
        if(regValue->Type == REG_DWORD) {
            RtlMoveMemory((PVOID)value,regValue->Data,regValue->DataLength);
        } else {
            status = STATUS_OBJECT_TYPE_MISMATCH;
        }
    } else {
        if (bCreateIfFail) {
            status = ZwSetValueKey(hRegistry, &name, 0, REG_DWORD, &ulDefaultValue, sizeof(ULONG32));
            
        }
    }

    return status;
}


NTSTATUS WriteULONG(HANDLE hRegistry, PCWSTR nameString, ULONG32 value)
{
NTSTATUS status;
UNICODE_STRING name;

    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());


    RtlInitUnicodeString(&name, nameString);

    status = ZwSetValueKey(hRegistry,
                            &name,
                            0,
                            REG_DWORD,
                            (PVOID)&value,
                            sizeof(value));

    return status;
}



NTSTATUS CloseKey(HANDLE hRegistry)
{
NTSTATUS status = STATUS_SUCCESS;
    
    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    status = ZwClose(hRegistry);
    
    return status;
}