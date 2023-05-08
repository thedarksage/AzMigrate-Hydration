#ifndef _INVOLFLT_HELPER_FUNCTIONS_H_
#define _INVOLFLT_HELPER_FUNCTIONS_H_

#define UNICODE_STRING_ADDITIONAL_BYTES     0x80

#define DOS_DEVICES_PATH2               L"\\??\\"
#define DOS_DEVICES_PATH                L"\\DosDevices\\"
#define IS_DRIVE_LETTER(x)      ((('A' <= (x)) && ('Z' >= (x))) || (('a' <= (x)) && ('z' >= (x))))

bool inline
InMageUnicodeStringEmpty(PUNICODE_STRING Dst)
{
    ASSERT(NULL != Dst);
    if (0 == Dst->Length) {
        return true;
    }

    ASSERT(NULL != Dst->Buffer);
    return false;
}

/**
 * This funciton initializes unicodes string
 * Dest->Buffer = Buffer
 * Dest->Length = usLength
 * Dest->MaximumLength = usMaxLength
 * */
void
InMageInitUnicodeString(
    PUNICODE_STRING Dest,
    PWCHAR          Buffer,
    USHORT          usMaxLength,
    USHORT          usLength
    );

void
InMageInitUnicodeString(PUNICODE_STRING Dest);

/**
 * This function allocates memory if required.
 * Dest -   Destination unicode string, this string should not be initialized with
 *          const data from read only data section.
 * Src -    Source unicode string, this string can be initialized to any data
 * PoolType - PagedPool or NonPagedPool. This provides type of memory that has
 *              to be allocated, if memory is allocated.
 * */
NTSTATUS
InMageCopyUnicodeString(
    PUNICODE_STRING Dest,
    PUNICODE_STRING Src,
    POOL_TYPE       PoolType = PagedPool
    );

NTSTATUS
InMageCopyUnicodeString(
    PUNICODE_STRING Dest,
    PWCHAR          Src,
    POOL_TYPE       PoolType = PagedPool
    );

/**
 * This function appends wchar string to unicode string
 * */
NTSTATUS
InMageAppendUnicodeString(
    PUNICODE_STRING Dest,
    PCWSTR          Src,
    POOL_TYPE       PoolType = PagedPool
    );

NTSTATUS
InMageAppendUnicodeString(
    PUNICODE_STRING Dest,
    PUNICODE_STRING Src,
    POOL_TYPE       PoolType = PagedPool
    );

NTSTATUS
InMageAppendUnicodeString(
    PUNICODE_STRING Dest,
    WCHAR           wChar,
    POOL_TYPE       PoolType = PagedPool
    );

NTSTATUS
InMageAppendUnicodeString(
    PUNICODE_STRING Dest,
    PCWSTR          Src,
    ULONG           ulLen,
    POOL_TYPE       PoolType = PagedPool
    );

NTSTATUS
InMageTerminateUnicodeStringWith(
    PUNICODE_STRING Dst, 
    WCHAR wChar, 
    POOL_TYPE PoolType = PagedPool
    );

void
InMageFreeUnicodeString(
    PUNICODE_STRING String
    );

NTSTATUS
InMageIncreaseUnicodeStringMaxLength(
    PUNICODE_STRING Dst,
    USHORT  usIncrement,
    POOL_TYPE   PoolType = PagedPool
    );

NTSTATUS
InMageAppendUnicodeStringWithFormat(
    PUNICODE_STRING Dst,
    POOL_TYPE   PoolType,
    LPCWSTR     Format,
    ...
    );

NTSTATUS
InMageCopyDosPathAsNTPath(
    PUNICODE_STRING Dst,
    PUNICODE_STRING Src,
    POOL_TYPE   PoolType = PagedPool
    );

char *
PoolTypeToString(POOL_TYPE PoolType);

WCHAR *
PoolTypeToStringW(POOL_TYPE PoolType);

#endif // _INVOLFLT_HELPER_FUNCTIONS_H_
