//
// Copyright InMage Systems 2004
//

#include "Common.h"
#include "HelperFunctions.h"
#include <global.h>

#if(_WIN32_WINNT <= 0x0500)

extern "C" {
    NTSYSAPI
    NTSTATUS
    NTAPI
    ZwDeleteValueKey(
        IN HANDLE KeyHandle,
        IN PUNICODE_STRING ValueName
        );
}

#endif

Registry::Registry()
{
    m_bCloseHandle = FALSE;
}

Registry::~Registry()
{
    if (TRUE == m_bCloseHandle) {
        ZwClose(m_hRegistry);
    }
}

void
Registry::Init(HANDLE hRegKey)
{
    m_hRegistry = hRegKey;
    m_bCloseHandle = TRUE;
}

NTSTATUS Registry::OpenKeyReadOnly(PCWSTR key)
{
NTSTATUS status;
OBJECT_ATTRIBUTES attr;
UNICODE_STRING keyName;

    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    CloseKey();
    RtlInitUnicodeString(&keyName,key);
    InitializeObjectAttributes(&attr,
        &keyName,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL, NULL);

    status = ZwOpenKey(&m_hRegistry, KEY_READ, &attr);
    if (NT_SUCCESS(status)) {
        m_bCloseHandle = TRUE;
    }

    return status;
}

NTSTATUS Registry::OpenKeyReadOnly(PUNICODE_STRING keyName)
{
NTSTATUS status;
OBJECT_ATTRIBUTES attr;

    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    CloseKey();
    InitializeObjectAttributes(&attr,
        keyName,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL, NULL);

    status = ZwOpenKey(&m_hRegistry, KEY_READ, &attr);
    if (NT_SUCCESS(status)) {
        m_bCloseHandle = TRUE;
    }

    return status;
}

NTSTATUS Registry::OpenSubKey(
    PCWSTR pKeyName,
    ULONG ulKeyNameLenCb, 
    Registry &RegSubKey,
    bool bCreateIfFail)
{
    HANDLE              hSubKey = NULL;
    OBJECT_ATTRIBUTES   attr;
    UNICODE_STRING      uszSubKeyName;
    NTSTATUS            Status;

    if (STRING_LEN_NULL_TERMINATED == ulKeyNameLenCb) {
        RtlInitUnicodeString(&uszSubKeyName, pKeyName);
    } else {
        uszSubKeyName.MaximumLength = uszSubKeyName.Length = (USHORT)ulKeyNameLenCb;
        uszSubKeyName.Buffer = (PWSTR)pKeyName;
    }

    InitializeObjectAttributes(&attr, &uszSubKeyName,
                OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
                m_hRegistry, NULL);

    Status = ZwOpenKey(&hSubKey, KEY_READ | KEY_WRITE, &attr);
    if (!NT_SUCCESS(Status) && bCreateIfFail) {
        // May be the key is not present try creating it.
        Status = ZwCreateKey(&hSubKey, KEY_READ | KEY_WRITE, &attr,
                            0, NULL, REG_OPTION_NON_VOLATILE, NULL);
    }

    if (STATUS_SUCCESS == Status) {
        RegSubKey.Init(hSubKey);
    }

    return Status;
}

/*---------------------------------------------------------------------------------
 * Function GetSubKey
 * Parameters
 *  ulIndex - This is zero based index for enumeration of all sub keys.
 *  ppSubKeyInformation - This should not be NULL. If *ppSubKeyInformation is NULL
 *                          function allocates memory required for the function.
 *                          If *ppSubKeyInformation is not NULL, then *pSubKeyLength
 *                          should be non zero and contain the size of buffer.
 *  pSubKeyLength       - This can be NULL if *ppSubKeyInformation is NULL.
 *
 * Notes:
 *      This function can be used by either passing buffer to the function or 
 * allowing the function to allocate the memory. If function allocates the memory
 * caller has to free the memory using ExFreePoolWithTag. Memory allocated is from
 * PagedPool.
 * If user is allocating memory pass the buffer in *ppSubKeyInformation and length
 * in *pSubKeyLength
 * If user wants the funciton to allocate memory set *ppSubKeyInformation to NULL
 * and either pSubKeyLength as NULL or *pSubKeyLength as 0.
 *---------------------------------------------------------------------------------
 */
NTSTATUS Registry::GetSubKey(
    ULONG ulIndex,
    PKEY_BASIC_INFORMATION *ppSubKeyInformation,
    ULONG *pSubKeyLength
    )
{
    NTSTATUS    Status;
    BOOLEAN     bMemoryAllocedByCaller = TRUE;
    ULONG       ulResultLength = 0, ulSubKeyLength = 0;
    PKEY_BASIC_INFORMATION  pSubKeyInformation = NULL;

    if (FALSE == m_bCloseHandle)
        return STATUS_INVALID_HANDLE;

    if (NULL == ppSubKeyInformation)
        return STATUS_INVALID_PARAMETER_2;

    if ((NULL != pSubKeyLength) && (*pSubKeyLength != 0) && (NULL == *ppSubKeyInformation))
        return STATUS_INVALID_PARAMETER_3;

    if ((NULL == pSubKeyLength) || (0 == *pSubKeyLength)) {
        bMemoryAllocedByCaller = FALSE;
        pSubKeyInformation = NULL;
        ulSubKeyLength = 0;
        *ppSubKeyInformation = NULL;
    } else {
        pSubKeyInformation = *ppSubKeyInformation;
        ulSubKeyLength = *pSubKeyLength;
    }

    Status = ZwEnumerateKey(m_hRegistry, ulIndex, KeyBasicInformation, pSubKeyInformation, ulSubKeyLength, &ulResultLength);
    if (STATUS_NO_MORE_ENTRIES == Status)
        return Status;

    if (FALSE == bMemoryAllocedByCaller) {
        // Let us allocate the memory.
        if ( ((STATUS_BUFFER_OVERFLOW == Status) || (STATUS_BUFFER_TOO_SMALL == Status)) &&
             (0 != ulResultLength))
        {
            pSubKeyInformation = (PKEY_BASIC_INFORMATION)ExAllocatePoolWithTag(PagedPool, ulResultLength, TAG_REGISTRY_DATA);
            if (NULL == pSubKeyInformation)
                return STATUS_INSUFFICIENT_RESOURCES;

            ulSubKeyLength = ulResultLength;
            Status = ZwEnumerateKey(m_hRegistry, ulIndex, KeyBasicInformation, pSubKeyInformation, ulSubKeyLength, &ulResultLength);
            if (STATUS_SUCCESS != Status) {
                ExFreePoolWithTag(pSubKeyInformation, TAG_REGISTRY_DATA);
                return Status;
            }

            *ppSubKeyInformation = pSubKeyInformation;
            if (NULL != pSubKeyLength)
                *pSubKeyLength = ulSubKeyLength;
        }
    }

    return Status;
}

NTSTATUS Registry::OpenKeyReadWrite(PCWSTR key)
{
NTSTATUS status;
OBJECT_ATTRIBUTES attr;
UNICODE_STRING keyName;

    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    CloseKey();
    RtlInitUnicodeString(&keyName,key);
    InitializeObjectAttributes(&attr,
        &keyName,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL, NULL);

    status = ZwOpenKey(&m_hRegistry, KEY_READ | KEY_WRITE, &attr);
    if (NT_SUCCESS(status)) {
        m_bCloseHandle = TRUE;
    }

    return status;
}

NTSTATUS Registry::OpenKeyReadWrite(PUNICODE_STRING keyName, BOOLEAN bCreateIfFail)
{
NTSTATUS status;
OBJECT_ATTRIBUTES attr;
    
    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    CloseKey();
    InitializeObjectAttributes(&attr,
        keyName,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL, NULL);

    status = ZwOpenKey(&m_hRegistry, KEY_READ | KEY_WRITE, &attr);
    if (!NT_SUCCESS(status) && bCreateIfFail) {
        // May be the key is not present try creating it.
        status = ZwCreateKey(&m_hRegistry, KEY_READ | KEY_WRITE, &attr,
                            0, NULL, REG_OPTION_NON_VOLATILE, NULL);
    }

    if (NT_SUCCESS(status)) {
        m_bCloseHandle = TRUE;
    }

    return status;
}

NTSTATUS Registry::CreateKeyReadWrite(PUNICODE_STRING keyName)
{
NTSTATUS status;
OBJECT_ATTRIBUTES attr;

    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    CloseKey();
    InitializeObjectAttributes(&attr,
        keyName,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL, NULL);

    status = ZwCreateKey(&m_hRegistry, KEY_READ | KEY_WRITE, &attr,
                        0, NULL, REG_OPTION_NON_VOLATILE, NULL);
    if (NT_SUCCESS(status)) {
        m_bCloseHandle = TRUE;
    }

    return status;
}

NTSTATUS Registry::CloseKey()
{
NTSTATUS status = STATUS_SUCCESS;
    
    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    if (m_bCloseHandle) {
        status = ZwClose(m_hRegistry);
        m_bCloseHandle = FALSE;
    }

    return status;
}

NTSTATUS 
Registry::FlushKey()
{
    return ZwFlushKey(m_hRegistry);
}

NTSTATUS Registry::ReadULONG(PCWSTR nameString, ULONG32 * value, ULONG32 ulDefaultValue, BOOLEAN bCreateIfFail)
{
NTSTATUS status;
UNICODE_STRING name;
unsigned long actualSize;
UCHAR buffer[64];
PKEY_VALUE_PARTIAL_INFORMATION regValue;

    ASSERT(NULL != value);
    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    *value = ulDefaultValue;

    if (FALSE == m_bCloseHandle)
        return STATUS_INVALID_HANDLE;

    regValue = (PKEY_VALUE_PARTIAL_INFORMATION)buffer;

    RtlInitUnicodeString(&name, nameString);

    status = ZwQueryValueKey(m_hRegistry,
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
            status = ZwSetValueKey(m_hRegistry, &name, 0, REG_DWORD, &ulDefaultValue, sizeof(ULONG32));
            if (NT_SUCCESS(status))
                status = STATUS_INMAGE_OBJECT_CREATED;
        }
    }

    return status;
}

NTSTATUS Registry::ReadWString(
    PCWSTR pwcValueName, 
    ULONG ulValueNameLength, 
    PWSTR *ppValue, 
    PWSTR pDefaultValue,
    BOOLEAN bCreateIfFail,
    POOL_TYPE PoolType
    )
{
    NTSTATUS        Status;
    UNICODE_STRING  ustrValue;
    ULONG           ulResultLength = 0, ulKeyValueLength = 0;
    PKEY_VALUE_PARTIAL_INFORMATION pKeyValuePartialInformation = NULL;
    PWSTR           pValue = NULL;

    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    if (FALSE == m_bCloseHandle)
        return STATUS_INVALID_HANDLE;

    if (NULL == ppValue)
        return STATUS_INVALID_PARAMETER_3;

    *ppValue = NULL;

    if (STRING_LEN_NULL_TERMINATED == ulValueNameLength) {
        RtlInitUnicodeString(&ustrValue, pwcValueName);
    } else {
        ustrValue.MaximumLength = ustrValue.Length = (USHORT)ulValueNameLength;
        ustrValue.Buffer = (PWSTR)pwcValueName;
    }

    Status = ZwQueryValueKey(m_hRegistry, &ustrValue, KeyValuePartialInformation, NULL, 0, &ulResultLength);
    if ((STATUS_BUFFER_OVERFLOW == Status) || (STATUS_BUFFER_TOO_SMALL == Status)) {
        pKeyValuePartialInformation = (PKEY_VALUE_PARTIAL_INFORMATION) ExAllocatePoolWithTag(PagedPool, ulResultLength, TAG_REGISTRY_DATA);
        if (NULL == pKeyValuePartialInformation)
            return STATUS_NO_MEMORY;

        ulKeyValueLength = ulResultLength;
        Status = ZwQueryValueKey(m_hRegistry, &ustrValue, KeyValuePartialInformation, 
                        pKeyValuePartialInformation, ulKeyValueLength, &ulResultLength);
        if ((STATUS_SUCCESS == Status) && (pKeyValuePartialInformation->Type == REG_SZ) &&
            (0 != pKeyValuePartialInformation->DataLength))
        {
            ASSERT((NonPagedPool == PoolType) || (PagedPool == PoolType));
            pValue = (PWSTR)ExAllocatePoolWithTag(PoolType, 
                                        pKeyValuePartialInformation->DataLength + sizeof(WCHAR),
                                        TAG_REGISTRY_DATA);
            if (NULL == pValue)
            {
                ExFreePoolWithTag(pKeyValuePartialInformation, TAG_REGISTRY_DATA);
                return STATUS_NO_MEMORY;
            }

            RtlMoveMemory((PVOID)pValue, pKeyValuePartialInformation->Data, pKeyValuePartialInformation->DataLength);
            // String returned from registry should be NULL terminated for saftey let us add another NULL.
            pValue[pKeyValuePartialInformation->DataLength / sizeof(WCHAR)] = L'\0';
            *ppValue = pValue;
        }
        ExFreePoolWithTag(pKeyValuePartialInformation, TAG_REGISTRY_DATA);
    } else {
        size_t  ValueSize = 0;
        ULONG   ulDataSize = 0;

        Status = RtlStringCbLengthW(pDefaultValue, NTSTRSAFE_UNICODE_STRING_MAX_CCH * sizeof(WCHAR), &ValueSize);
        if (NT_SUCCESS(Status)) {
            ulDataSize = (ULONG)ValueSize + sizeof(WCHAR);
            if (bCreateIfFail) {
                Status = ZwSetValueKey(m_hRegistry, &ustrValue, 0, REG_SZ, pDefaultValue, ulDataSize);
                if (NT_SUCCESS(Status))
                    Status = STATUS_INMAGE_OBJECT_CREATED;
            }        

            ASSERT((NonPagedPool == PoolType) || (PagedPool == PoolType));
            pValue = (PWSTR)ExAllocatePoolWithTag(PoolType, ulDataSize, TAG_REGISTRY_DATA);
            if (NULL == pValue) {
                return STATUS_NO_MEMORY;
            }
            RtlMoveMemory((PVOID)pValue, pDefaultValue, ulDataSize);
            *ppValue = pValue;         
        }
    }

    return Status;
}

NTSTATUS Registry::ReadAString(
    PCWSTR pwcValueName, 
    ULONG ulValueNameLength, 
    PSTR *ppValue, 
    PSTR pDefaultValue,
    BOOLEAN bCreateIfFail,
    POOL_TYPE PoolType
    )
{
    NTSTATUS        Status;
    UNICODE_STRING  ustrValue;
    ULONG           ulResultLength = 0, ulKeyValueLength = 0;
    PKEY_VALUE_PARTIAL_INFORMATION pKeyValuePartialInformation = NULL;

    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    if (FALSE == m_bCloseHandle)
        return STATUS_INVALID_HANDLE;

    if (NULL == ppValue)
        return STATUS_INVALID_PARAMETER_3;

    *ppValue = NULL;

    if (STRING_LEN_NULL_TERMINATED == ulValueNameLength) {
        RtlInitUnicodeString(&ustrValue, pwcValueName);
    } else {
        ustrValue.MaximumLength = ustrValue.Length = (USHORT)ulValueNameLength;
        ustrValue.Buffer = (PWSTR)pwcValueName;
    }

    Status = ZwQueryValueKey(m_hRegistry, &ustrValue, KeyValuePartialInformation, NULL, 0, &ulResultLength);
    if ((STATUS_BUFFER_OVERFLOW == Status) || (STATUS_BUFFER_TOO_SMALL == Status)) {
        pKeyValuePartialInformation = (PKEY_VALUE_PARTIAL_INFORMATION) ExAllocatePoolWithTag(PagedPool, ulResultLength, TAG_REGISTRY_DATA);
        if (NULL == pKeyValuePartialInformation)
            return STATUS_NO_MEMORY;

        ulKeyValueLength = ulResultLength;
        Status = ZwQueryValueKey(m_hRegistry, &ustrValue, KeyValuePartialInformation, 
                        pKeyValuePartialInformation, ulKeyValueLength, &ulResultLength);
        if ((STATUS_SUCCESS == Status) && (pKeyValuePartialInformation->Type == REG_SZ) &&
            (0 != pKeyValuePartialInformation->DataLength))
        {
            UNICODE_STRING  usDataValue;
            ANSI_STRING     asDataValue = {0};

            RtlInitUnicodeString(&usDataValue, (PCWSTR)pKeyValuePartialInformation->Data);
            // Instead of finding the length required for ANSI string, just allocated the size as unicode string and NULL character
            asDataValue.MaximumLength = usDataValue.MaximumLength + sizeof(CHAR);
            asDataValue.Buffer = (PSTR)ExAllocatePoolWithTag(PoolType, asDataValue.MaximumLength, TAG_REGISTRY_DATA);
            if (NULL == asDataValue.Buffer)
            {
                ExFreePoolWithTag(pKeyValuePartialInformation, TAG_REGISTRY_DATA);
                return STATUS_NO_MEMORY;
            }
            Status = RtlUnicodeStringToAnsiString(&asDataValue, &usDataValue, FALSE);
            if (NT_SUCCESS(Status)) {
                *ppValue = asDataValue.Buffer;
                asDataValue.Buffer[asDataValue.Length] = '\0';
            } else {
                ExFreePoolWithTag(asDataValue.Buffer, TAG_REGISTRY_DATA);
            }
        }
        ExFreePoolWithTag(pKeyValuePartialInformation, TAG_REGISTRY_DATA);
    }

    if ((NULL == *ppValue) && (NULL != pDefaultValue)) {
        size_t  ValueSize = 0;
        ULONG   ulDataSize = 0;
        ANSI_STRING     asValue;
        PSTR            pValue = NULL;

        RtlInitAnsiString(&asValue, pDefaultValue);
        if (bCreateIfFail) {
            UNICODE_STRING  usValue = {0};
            // This function returns the size that included null terminator.
            usValue.MaximumLength = (USHORT)RtlAnsiStringToUnicodeSize(&asValue);
            usValue.Buffer = (PWSTR)ExAllocatePoolWithTag(PagedPool, usValue.MaximumLength, TAG_REGISTRY_DATA);
            if (NULL != usValue.Buffer) {
                Status = RtlAnsiStringToUnicodeString(&usValue, &asValue, FALSE);
                if (NT_SUCCESS(Status)) {
                    usValue.Buffer[usValue.Length / sizeof(WCHAR)] = L'\0';
                    Status = ZwSetValueKey(m_hRegistry, &ustrValue, 0, REG_SZ, usValue.Buffer, usValue.MaximumLength);
                    if (NT_SUCCESS(Status))
                        Status = STATUS_INMAGE_OBJECT_CREATED;
                }

                ExFreePoolWithTag(usValue.Buffer, TAG_REGISTRY_DATA);
            }
        }

        ASSERT((NonPagedPool == PoolType) || (PagedPool == PoolType));
        pValue = (PSTR)ExAllocatePoolWithTag(PoolType, asValue.MaximumLength, TAG_REGISTRY_DATA);
        RtlMoveMemory((PVOID)pValue, pDefaultValue, asValue.MaximumLength);
        *ppValue = pValue;
    }

    return Status;
}

NTSTATUS
Registry::FreeMultiSzArray(PWCHAR* &WStringArray, ULONG &ulArraySize)
{
    if (NULL == WStringArray) {
        return STATUS_SUCCESS;
    }

    ASSERT(NULL == WStringArray[ulArraySize]);

    for (ULONG ulIndex = 0; ulIndex < ulArraySize; ulIndex++) {
        if (WStringArray[ulIndex]) {
            ExFreePoolWithTag(WStringArray[ulIndex], TAG_REGISTRY_DATA);
        }
    }

    ExFreePoolWithTag(WStringArray, TAG_REGISTRY_DATA);

    WStringArray = NULL;
    ulArraySize = 0;
    return STATUS_SUCCESS;
}

NTSTATUS
Registry::ConvertMultiSzToArray(PCWSTR pwcMultiSz, size_t MultiSzCbLength, PWCHAR* &WStringArray, ULONG &ulArraySize)
{
    NTSTATUS    Status = STATUS_SUCCESS;
    size_t      cbTemp = MultiSzCbLength;
    PCWSTR      pTemp = pwcMultiSz;
    ULONG       ulElementsInArray = 0;

    if (NULL != WStringArray) {
        return STATUS_INVALID_PARAMETER_3;
    }

    ulArraySize = 0;
    do {
        size_t      cbStringSize = 0;

        Status = RtlStringCbLengthW(pTemp, cbTemp, &cbStringSize);
        if (NT_SUCCESS(Status)) {
            if (cbStringSize) {
                ulElementsInArray++;
            }

            cbStringSize += sizeof(WCHAR);
            cbTemp -= cbStringSize;
            pTemp   = (PCWSTR)((PUCHAR)pTemp + cbStringSize);
        } else {
            break;
        }
        
    } while (cbTemp);

    WStringArray = (PWCHAR *)ExAllocatePoolWithTag(PagedPool, sizeof(PWCHAR) * (ulElementsInArray + 1), TAG_REGISTRY_DATA);
    if (NULL == WStringArray) {
        return STATUS_NO_MEMORY;
    }

    RtlZeroMemory(WStringArray, sizeof(PWCHAR) * (ulElementsInArray + 1));
    pTemp = pwcMultiSz;
    cbTemp = MultiSzCbLength;

    do {
        size_t      cbStringSize = 0;

        Status = RtlStringCbLengthW(pTemp, cbTemp, &cbStringSize);
        if (NT_SUCCESS(Status)) {
            if (cbStringSize) {
                cbStringSize += sizeof(WCHAR);
                WStringArray[ulArraySize] = (PWCHAR)ExAllocatePoolWithTag(PagedPool, cbStringSize, TAG_REGISTRY_DATA);
                if (NULL == WStringArray[ulArraySize]) {
                    FreeMultiSzArray(WStringArray, ulArraySize);
                    return STATUS_NO_MEMORY;
                }
                Status = RtlStringCbCopyW(WStringArray[ulArraySize], cbStringSize, pTemp);
                ulArraySize++;
                ASSERT(STATUS_SUCCESS == Status);
            } else {
                // Terminating NULL string reached in MULTI_VALUE
                break;
            }

            cbTemp -= cbStringSize;
            pTemp   = (PCWSTR)((PUCHAR)pTemp + cbStringSize);
        } else {
            break;
        }
    } while (cbTemp);

    return STATUS_SUCCESS;
}

NTSTATUS Registry::ReadMultiSzToArray(PCWSTR pwcValueName, ULONG ulValueNameLength, PWCHAR* &WStringArray, ULONG &ulArraySize)
{
    NTSTATUS        Status;
    UNICODE_STRING  ustrValue;
    ULONG           ulResultLength = 0, ulKeyValueLength = 0;
    PKEY_VALUE_PARTIAL_INFORMATION pKeyValuePartialInformation = NULL;
    PWSTR           pValue = NULL;

    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    if (FALSE == m_bCloseHandle)
        return STATUS_INVALID_HANDLE;

    if (NULL != WStringArray)
        return STATUS_INVALID_PARAMETER_3;

    ulArraySize = 0;

    if (STRING_LEN_NULL_TERMINATED == ulValueNameLength) {
        RtlInitUnicodeString(&ustrValue, pwcValueName);
    } else {
        ustrValue.MaximumLength = ustrValue.Length = (USHORT)ulValueNameLength;
        ustrValue.Buffer = (PWSTR)pwcValueName;
    }

    Status = ZwQueryValueKey(m_hRegistry, &ustrValue, KeyValuePartialInformation, NULL, 0, &ulResultLength);
    if ((STATUS_BUFFER_OVERFLOW == Status) || (STATUS_BUFFER_TOO_SMALL == Status)) {
        pKeyValuePartialInformation = (PKEY_VALUE_PARTIAL_INFORMATION) ExAllocatePoolWithTag(PagedPool, ulResultLength, TAG_REGISTRY_DATA);
        if (NULL == pKeyValuePartialInformation)
            return STATUS_INSUFFICIENT_RESOURCES;

        ulKeyValueLength = ulResultLength;
        Status = ZwQueryValueKey(m_hRegistry, &ustrValue, KeyValuePartialInformation, 
                        pKeyValuePartialInformation, ulKeyValueLength, &ulResultLength);
        if ((STATUS_SUCCESS == Status) && (pKeyValuePartialInformation->Type == REG_MULTI_SZ) &&
            (0 != pKeyValuePartialInformation->DataLength))
        {
            Status = ConvertMultiSzToArray((PCWSTR)pKeyValuePartialInformation->Data, pKeyValuePartialInformation->DataLength, WStringArray, ulArraySize);
        }
        ExFreePoolWithTag(pKeyValuePartialInformation, TAG_REGISTRY_DATA);
    }

    return Status;
}

NTSTATUS Registry::ReadMultiSz(PCWSTR pwcValueName, ULONG ulValueNameLength, PWCHAR *ppValue, ULONG *pulValueCb)
{
    NTSTATUS        Status;
    UNICODE_STRING  ustrValue;
    ULONG           ulResultLength = 0, ulKeyValueLength = 0;
    PKEY_VALUE_PARTIAL_INFORMATION pKeyValuePartialInformation = NULL;
    PWSTR           pValue = NULL;

    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    if (FALSE == m_bCloseHandle)
        return STATUS_INVALID_HANDLE;

    if (NULL == ppValue)
        return STATUS_INVALID_PARAMETER_3;

    if (NULL == pulValueCb)
        return STATUS_INVALID_PARAMETER_4;

    *ppValue = NULL;
    *pulValueCb = 0;

    if (STRING_LEN_NULL_TERMINATED == ulValueNameLength) {
        RtlInitUnicodeString(&ustrValue, pwcValueName);
    } else {
        ustrValue.MaximumLength = ustrValue.Length = (USHORT)ulValueNameLength;
        ustrValue.Buffer = (PWSTR)pwcValueName;
    }

    Status = ZwQueryValueKey(m_hRegistry, &ustrValue, KeyValuePartialInformation, NULL, 0, &ulResultLength);
    if ((STATUS_BUFFER_OVERFLOW == Status) || (STATUS_BUFFER_TOO_SMALL == Status)) {
        pKeyValuePartialInformation = (PKEY_VALUE_PARTIAL_INFORMATION) ExAllocatePoolWithTag(PagedPool, ulResultLength, TAG_REGISTRY_DATA);
        if (NULL == pKeyValuePartialInformation)
            return STATUS_INSUFFICIENT_RESOURCES;

        ulKeyValueLength = ulResultLength;
        Status = ZwQueryValueKey(m_hRegistry, &ustrValue, KeyValuePartialInformation, 
                        pKeyValuePartialInformation, ulKeyValueLength, &ulResultLength);
        if ((STATUS_SUCCESS == Status) && (pKeyValuePartialInformation->Type == REG_MULTI_SZ) &&
            (0 != pKeyValuePartialInformation->DataLength))
        {
            pValue = (PWSTR)ExAllocatePoolWithTag(PagedPool, pKeyValuePartialInformation->DataLength, TAG_REGISTRY_DATA);
            if (NULL == pValue)
            {
                ExFreePoolWithTag(pKeyValuePartialInformation, TAG_REGISTRY_DATA);
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            RtlMoveMemory((PVOID)pValue, pKeyValuePartialInformation->Data, pKeyValuePartialInformation->DataLength);
            *ppValue = pValue;
            *pulValueCb = pKeyValuePartialInformation->DataLength;
        }
        ExFreePoolWithTag(pKeyValuePartialInformation, TAG_REGISTRY_DATA);
    }

    return Status;
}

NTSTATUS Registry::ReadUnicodeString(
    PCWSTR pwcValueName, 
    ULONG ulValueNameLength,
    PUNICODE_STRING value,
    PCWSTR pDefaultValue,
    bool bCreateIfFail,
    POOL_TYPE PoolType
    )
{
    NTSTATUS        Status;
    UNICODE_STRING  ustrValue;
    ULONG           ulResultLength = 0, ulKeyValueLength = 0;
    PKEY_VALUE_PARTIAL_INFORMATION pKeyValuePartialInformation = NULL;
    PWSTR           pValue = NULL;
    size_t          ValueSize = 0;
    ULONG           ulDataSize = 0;

    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    value->Length = 0;

    if (FALSE == m_bCloseHandle)
        return STATUS_INVALID_HANDLE;

    if (STRING_LEN_NULL_TERMINATED == ulValueNameLength) {
        RtlInitUnicodeString(&ustrValue, pwcValueName);
    } else {
        ustrValue.MaximumLength = ustrValue.Length = (USHORT)ulValueNameLength;
        ustrValue.Buffer = (PWSTR)pwcValueName;
    }

    Status = ZwQueryValueKey(m_hRegistry, &ustrValue, KeyValuePartialInformation, NULL, 0, &ulResultLength);
    if ((STATUS_BUFFER_OVERFLOW == Status) || (STATUS_BUFFER_TOO_SMALL == Status)) {
        pKeyValuePartialInformation = (PKEY_VALUE_PARTIAL_INFORMATION) ExAllocatePoolWithTag(PagedPool, ulResultLength, TAG_REGISTRY_DATA);
        if (NULL == pKeyValuePartialInformation)
            return STATUS_NO_MEMORY;

        ulKeyValueLength = ulResultLength;
        Status = ZwQueryValueKey(m_hRegistry, &ustrValue, KeyValuePartialInformation, 
                        pKeyValuePartialInformation, ulKeyValueLength, &ulResultLength);
        if (STATUS_SUCCESS == Status) {
            if (pKeyValuePartialInformation->Type != REG_SZ) {
                Status = STATUS_OBJECT_TYPE_MISMATCH;
            } else {
                if (0 == pKeyValuePartialInformation->DataLength) {
                    Status = STATUS_NOT_FOUND;
                } else {
                    if ('\0' == pKeyValuePartialInformation->Data[0]) { // Check whether Registry Key's Data Present
                        Status = RtlStringCbLengthW(pDefaultValue, NTSTRSAFE_UNICODE_STRING_MAX_CCH * sizeof(WCHAR), &ValueSize);
                        if (NT_SUCCESS(Status)) {
                            if (bCreateIfFail)  {
                                ulDataSize = (ULONG)ValueSize + sizeof(WCHAR);
                                Status = ZwSetValueKey(m_hRegistry, &ustrValue, 0, REG_SZ, (PVOID)pDefaultValue, ulDataSize);
                            }
                            Status = InMageAppendUnicodeString(value, pDefaultValue, PoolType);
                        }
                    }
                    else {
                      Status = InMageAppendUnicodeString(value, (PCWSTR)pKeyValuePartialInformation->Data, PoolType);
                    }
                }
            }
        }
        ExFreePoolWithTag(pKeyValuePartialInformation, TAG_REGISTRY_DATA);
    } else {
         Status = RtlStringCbLengthW(pDefaultValue, NTSTRSAFE_UNICODE_STRING_MAX_CCH * sizeof(WCHAR), &ValueSize);
         if (NT_SUCCESS(Status)) {
            ulDataSize = (ULONG)ValueSize + sizeof(WCHAR);
            if (bCreateIfFail) {
                Status = ZwSetValueKey(m_hRegistry, &ustrValue, 0, REG_SZ, (PVOID)pDefaultValue, ulDataSize);
                if (NT_SUCCESS(Status))
                    Status = STATUS_INMAGE_OBJECT_CREATED;
            }

            Status = InMageAppendUnicodeString(value, pDefaultValue, PoolType);
        }
    }

    return Status;
}

NTSTATUS Registry::WriteULONG(PCWSTR nameString, ULONG32 value)
{
NTSTATUS status;
UNICODE_STRING name;

    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    if (FALSE == m_bCloseHandle)
        return STATUS_INVALID_HANDLE;

    RtlInitUnicodeString(&name, nameString);

    status = ZwSetValueKey(m_hRegistry,
                            &name,
                            0,
                            REG_DWORD,
                            (PVOID)&value,
                            sizeof(value));

    return status;
}

NTSTATUS Registry::WriteBinary(PCWSTR NameString, PVOID Value, ULONG ValueSize)
{
    NTSTATUS Status;
    UNICODE_STRING Name;

    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    if (FALSE == m_bCloseHandle)
        return STATUS_INVALID_HANDLE;

    RtlInitUnicodeString(&Name, NameString);

    Status = ZwSetValueKey(m_hRegistry,
                            &Name,
                            0,
                            REG_BINARY,
                            Value,
                            ValueSize);

    return Status;
}

NTSTATUS Registry::CopyValue(
    PVOID *ppBuffer,
    ULONG ulcbSize,
    ULONG *pulcbCopied,
    PVOID pSource,
    ULONG ulLength,
    POOL_TYPE PoolType
    )
{
    if ((NULL == pSource) || (0 == ulLength)) {
        return STATUS_SUCCESS;
    }

    if (NULL == ppBuffer) {
        return STATUS_INVALID_PARAMETER;
    }

    if ((NULL != *ppBuffer) && (0 != ulcbSize)) {
        // Buffer is provided by caller.
        if (ulcbSize < ulLength) {
            return STATUS_BUFFER_OVERFLOW;
        }

		RtlCopyMemory(*ppBuffer, pSource, ulLength);
        if (NULL != pulcbCopied) {
            *pulcbCopied = ulLength;
        }
    } else {
        ASSERT((NonPagedPool == PoolType) || (PagedPool == PoolType));
        // Buffer is not provided by caller.
        *ppBuffer = ExAllocatePoolWithTag(PoolType, ulLength, TAG_REGISTRY_DATA);
        if (NULL == *ppBuffer) {
            return STATUS_NO_MEMORY;
        }


		RtlCopyMemory(*ppBuffer, pSource, ulLength);
        if (NULL != pulcbCopied) {
            *pulcbCopied = ulLength;
        }
    }

    return STATUS_SUCCESS;
}

NTSTATUS Registry::ReadBinary(
        PCWSTR pwcValueName, 
        ULONG ulValueNameLength, 
        PVOID *ppBuffer,
        ULONG ulcbSize,
        ULONG *pulcbCopied,
        PVOID pDefaultValue,
        ULONG ulDefaultValueLength,
        BOOLEAN bCreateIfFail,
        POOL_TYPE PoolType
        )
{
    NTSTATUS        Status;
    UNICODE_STRING  ustrValue;
    ULONG           ulResultLength = 0, ulKeyValueLength = 0;
    PKEY_VALUE_PARTIAL_INFORMATION pKeyValuePartialInformation = NULL;
    PWSTR           pValue = NULL;

    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    if (NULL != *pulcbCopied) {
        *pulcbCopied = 0;
    }

    if (FALSE == m_bCloseHandle)
        return STATUS_INVALID_HANDLE;

    if (NULL == pwcValueName) {
        return STATUS_INVALID_PARAMETER;
    }

    if (STRING_LEN_NULL_TERMINATED == ulValueNameLength) {
        RtlInitUnicodeString(&ustrValue, pwcValueName);
    } else {
        ustrValue.MaximumLength = ustrValue.Length = (USHORT)ulValueNameLength;
        ustrValue.Buffer = (PWSTR)pwcValueName;
    }

    Status = ZwQueryValueKey(m_hRegistry, &ustrValue, KeyValuePartialInformation, NULL, 0, &ulResultLength);
    if ((STATUS_BUFFER_OVERFLOW == Status) || (STATUS_BUFFER_TOO_SMALL == Status)) {
        pKeyValuePartialInformation = (PKEY_VALUE_PARTIAL_INFORMATION) ExAllocatePoolWithTag(PagedPool, ulResultLength, TAG_REGISTRY_DATA);
        if (NULL == pKeyValuePartialInformation)
            return STATUS_NO_MEMORY;

        ulKeyValueLength = ulResultLength;
        Status = ZwQueryValueKey(m_hRegistry, &ustrValue, KeyValuePartialInformation, 
                        pKeyValuePartialInformation, ulKeyValueLength, &ulResultLength);
        if (STATUS_SUCCESS == Status) {
            if (pKeyValuePartialInformation->Type != REG_BINARY) {
                Status = STATUS_OBJECT_TYPE_MISMATCH;
                CopyValue(ppBuffer, ulcbSize, pulcbCopied, pDefaultValue, ulDefaultValueLength, PoolType);
                return Status;
            } else {
                CopyValue(ppBuffer, 
                          ulcbSize, 
                          pulcbCopied, 
                          pKeyValuePartialInformation->Data, 
                          pKeyValuePartialInformation->DataLength, 
                          PoolType);
            }
        }
        ExFreePoolWithTag(pKeyValuePartialInformation, TAG_REGISTRY_DATA);
    } else {
        if (bCreateIfFail) {
            Status = ZwSetValueKey(m_hRegistry, &ustrValue, 0, REG_BINARY, (PVOID)pDefaultValue, ulDefaultValueLength);
            if (NT_SUCCESS(Status))
                Status = STATUS_INMAGE_OBJECT_CREATED;
        }

        CopyValue(ppBuffer, ulcbSize, pulcbCopied, pDefaultValue, ulDefaultValueLength, PoolType);
    }

    return Status;
}

NTSTATUS Registry::WriteUnicodeString(PCWSTR nameString, PUNICODE_STRING valueString)
{
NTSTATUS status;
UNICODE_STRING name;
UNICODE_STRING value;

    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    RtlInitUnicodeString(&name, nameString);
    
    if (FALSE == m_bCloseHandle)
        return STATUS_INVALID_HANDLE;

    // add zero termination
    value.MaximumLength = valueString->Length + sizeof(WCHAR);
    value.Buffer = (WCHAR *)new CHAR[value.MaximumLength];
    value.Length = 0;
    InVolDbgPrint(DL_INFO, DM_MEM_TRACE,
        ("Registry::WriteUnicodeString Allocation %p\n", value.Buffer));
    if (NULL == value.Buffer) {
        return STATUS_NO_MEMORY;
    }
    RtlCopyUnicodeString(&value,valueString);
    value.Buffer[valueString->Length/sizeof(WCHAR)] = 0;

    status = ZwSetValueKey(m_hRegistry,
                            &name,
                            0,
                            REG_SZ,
                            (PVOID)value.Buffer,
                            value.Length + sizeof(WCHAR));
    if (!(NT_SUCCESS(status))) {
        InVolDbgPrint(DL_ERROR, DM_REGISTRY,
            ("Registry::WriteUnicodeString ZwSetValueKey Status = %X\n", status));
    }

    delete value.Buffer;

    return status;
}


NTSTATUS Registry::WriteWString(PCWSTR pwcNameString, PCWSTR pwcValueString)
{
    NTSTATUS        Status;
    UNICODE_STRING  ustrName;
    size_t          DataSize = 0;

    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    RtlInitUnicodeString(&ustrName, pwcNameString);
    
    if (FALSE == m_bCloseHandle)
        return STATUS_INVALID_HANDLE;

    if ((NULL == pwcNameString) || (NULL == pwcValueString))
        return STATUS_INVALID_PARAMETER;

    Status = RtlStringCbLengthW(pwcValueString, CREGISTRY_MAX_WCHAR_STRING_SIZE, &DataSize);
    if ((STATUS_SUCCESS != Status) || (0 == DataSize))
        return STATUS_INVALID_PARAMETER;

    DataSize += sizeof(WCHAR);

    Status = ZwSetValueKey(m_hRegistry,
                            &ustrName,
                            0,
                            REG_SZ,
                            (PVOID)pwcValueString,
                            DataSize);
    if (!(NT_SUCCESS(Status))) {
        InVolDbgPrint(DL_WARNING, DM_REGISTRY,
            ("Registry::WriteWString ZwSetValueKey Status = %X\n", Status));
    }

    return Status;
}

NTSTATUS
Registry::DeleteValue(
    PCWSTR pwcValueName,
    ULONG ulValueNameLength
    )
{
    NTSTATUS        Status;
    UNICODE_STRING  ustrValue;
    ULONG           ulResultLength = 0, ulKeyValueLength = 0;
    PKEY_VALUE_PARTIAL_INFORMATION pKeyValuePartialInformation = NULL;
    PWSTR           pValue = NULL;

    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    if (FALSE == m_bCloseHandle)
        return STATUS_INVALID_HANDLE;

    if (NULL == pwcValueName) {
        return STATUS_INVALID_PARAMETER;
    }

    if (STRING_LEN_NULL_TERMINATED == ulValueNameLength) {
        RtlInitUnicodeString(&ustrValue, pwcValueName);
    } else {
        ustrValue.MaximumLength = ustrValue.Length = (USHORT)ulValueNameLength;
        ustrValue.Buffer = (PWSTR)pwcValueName;
    }

    Status = ZwDeleteValueKey(m_hRegistry, &ustrValue);

    return Status;
}

