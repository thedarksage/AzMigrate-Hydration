#ifndef _INMAGE_REGISTRY_CPP_H_
#define _INMAGE_REGISTRY_CPP_H_

// This is used the scan for the NULL character at the end of the string.
// If NULL Is not found in 4K, the string is assumed to be corrupted and
// returned an error.
#define CREGISTRY_MAX_WCHAR_STRING_SIZE     0x1000  // 4K

class Registry : public BaseClass {
public:
    Registry(void); // constructor
    virtual ~Registry(); 

    void * __cdecl operator new(size_t size) {return malloc(size, TAG_REGISTRY_OBJECT, NonPagedPool);};
    virtual NTSTATUS OpenKeyReadOnly(PCWSTR keyName);
    virtual NTSTATUS OpenKeyReadOnly(PUNICODE_STRING keyName);
    virtual NTSTATUS OpenKeyReadWrite(PCWSTR keyName);
    virtual NTSTATUS OpenKeyReadWrite(PUNICODE_STRING keyName, BOOLEAN bCreateIfFail = FALSE);

    virtual NTSTATUS OpenSubKey(PCWSTR KeyName, ULONG ulKeyNameLenCb, Registry &RegSubKey, bool bCreateIfFail = false);

    virtual NTSTATUS CreateKeyReadWrite(PUNICODE_STRING keyName);

    virtual NTSTATUS GetSubKey(ULONG ulIndex, PKEY_BASIC_INFORMATION *ppSubKeyInformation, ULONG *pSubKeyLength);
    virtual NTSTATUS CloseKey();
    virtual NTSTATUS FlushKey();

    virtual NTSTATUS 
    ReadULONG(
        PCWSTR valueName,
        ULONG32 * value,
        ULONG32 ulDefaultvalue = 0,
        BOOLEAN bCreateIfFail = FALSE
        );

    virtual NTSTATUS 
    ReadUnicodeString(
        PCWSTR pwcValueName, 
        ULONG ulValueNameLength, 
        PUNICODE_STRING value, 
        PCWSTR pDefaultValue = NULL,
        bool bCreateIfFail = false,
        POOL_TYPE PoolType = PagedPool
        );

    virtual NTSTATUS 
    ReadWString(
        PCWSTR pwcValueName, 
        ULONG ulValueNameLength, 
        PWSTR *ppValue, 
        PWSTR pDefaultValue = NULL, 
        BOOLEAN bCreateIfFail = FALSE, 
        POOL_TYPE PoolType = PagedPool
        );

    virtual NTSTATUS 
    ReadAString(
        PCWSTR pwcValueName,
        ULONG ulValueNameLength, 
        PSTR *ppValue, 
        PSTR pDefaultValue = NULL, 
        BOOLEAN bCreateIfFail = FALSE, 
        POOL_TYPE PoolType = PagedPool
        );
    virtual NTSTATUS WriteULONG(PCWSTR valueName, ULONG32 value);
    virtual NTSTATUS WriteUnicodeString(PCWSTR valueName, PUNICODE_STRING value);
    virtual NTSTATUS WriteWString(PCWSTR valueName, PCWSTR value);
    virtual NTSTATUS WriteBinary(PCWSTR NameString, PVOID Value, ULONG ValueSize);

    virtual NTSTATUS ReadBinary(
        PCWSTR pwcValueName, 
        ULONG ulValueNameLength, 
        PVOID *ppValue,
        ULONG  ulcbSize,
        ULONG *pulcbCopied,
        PVOID pDefaultValue = NULL,
        ULONG ulDefaultValueLength = 0,
        BOOLEAN bCreateIfFail = FALSE,
        POOL_TYPE PoolType = PagedPool
        );

    virtual NTSTATUS ReadMultiSz(PCWSTR pwcValueName, ULONG ulValueNameLength, PWCHAR *ppValue, ULONG *pulValueCb);
    virtual NTSTATUS ReadMultiSzToArray(PCWSTR pwcValueName, ULONG ulValueNameLength, PWCHAR* &WStringArray, ULONG &ulArraySize);
    virtual NTSTATUS DeleteValue(PCWSTR pwcValueName, ULONG ulValueNameLength);
    virtual NTSTATUS FreeMultiSzArray(PWCHAR* &WStringArray, ULONG &ulArraySize);

private:
    void Init(HANDLE hReg);

    NTSTATUS
    CopyValue(
        PVOID *ppBuffer,
        ULONG ulcbSize,
        ULONG *pulcbCopied, 
        PVOID pSource,
        ULONG ulLength,
        POOL_TYPE PoolType
        );

    NTSTATUS
    ConvertMultiSzToArray(
        PCWSTR pwcMultiSz, 
        size_t MultiSzCbLength, 
        PWCHAR* &WStringArray, 
        ULONG &ulArraySize
        );

protected:
    HANDLE  m_hRegistry;
    BOOLEAN m_bCloseHandle;
};

#endif // _INMAGE_REGISTRY_CPP_H_