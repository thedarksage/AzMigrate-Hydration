extern "C"
{
#include "ntddk.h"
#define NTSTRSAFE_LIB
#include <ntstrsafe.h>
}

#include "DrvDefines.h"
#include "HelperFunctions.h"
#include "DiskFltEvents.h"

// These are defined in common.h
// Added them here again so that this file can be
// shared between different drivers.
#define TAG_GENERIC_NON_PAGED           'NGnI'
#define TAG_GENERIC_PAGED               'PGnI'

VOID
LogMemoryAllocationFailure(size_t size, POOL_TYPE PoolType, SourceLocation SourceLocation);

NTSTATUS
InMageIncreaseUnicodeStringMaxLength(
    PUNICODE_STRING Dst,
    USHORT  usIncrement,
    POOL_TYPE   PoolType
    )
{
    USHORT  usMaxLength = (0 != usIncrement) ? usIncrement : UNICODE_STRING_ADDITIONAL_BYTES;
    usMaxLength = (USHORT)(usMaxLength + Dst->MaximumLength);

    ASSERT((NULL != Dst->Buffer) || ((0 == Dst->Length) && (0 == Dst->MaximumLength)));
    ASSERT((NonPagedPool == PoolType) || (PagedPool == PoolType));

    PWCHAR  pBuffer = (PWCHAR)ExAllocatePoolWithTag(PoolType, usMaxLength, (NonPagedPool == PoolType) ? TAG_GENERIC_NON_PAGED : TAG_GENERIC_PAGED);
    if (NULL == pBuffer) {
        LogMemoryAllocationFailure(usMaxLength, PoolType, SourceLocationIncUniStrMaxLen);
        return STATUS_NO_MEMORY;
    }
    RtlZeroMemory(pBuffer, usMaxLength);
    if (NULL != Dst->Buffer) {
        if (0 != Dst->Length) {
            RtlMoveMemory(pBuffer, Dst->Buffer, Dst->Length);
        }

        ExFreePool(Dst->Buffer);
    }

    Dst->Buffer = pBuffer;
    Dst->MaximumLength = usMaxLength;

    return STATUS_SUCCESS;
}

NTSTATUS
InMageCopyUnicodeString(
    PUNICODE_STRING Dest,
    PUNICODE_STRING Src,
    POOL_TYPE       PoolType
    )
{
    NTSTATUS    Status = STATUS_SUCCESS;

    ASSERT(NULL != Dest);
    ASSERT(NULL != Src);

    if ((Dest == NULL) || (Src == NULL)) {
        return STATUS_INVALID_PARAMETER;
    }

    if (Dest->MaximumLength >= Src->Length) {
        RtlCopyUnicodeString(Dest, Src);
    } else {
        USHORT usMaxLength = (USHORT)(Src->Length + UNICODE_STRING_ADDITIONAL_BYTES);

        if (Dest->Buffer) {
            ExFreePool(Dest->Buffer);
            RtlInitEmptyUnicodeString(Dest, NULL, 0);
        }

        ASSERT((NonPagedPool == PoolType) || (PagedPool == PoolType));
        PWCHAR  pBuffer = (PWCHAR)ExAllocatePoolWithTag(PoolType, 
                                                usMaxLength, 
                                                (NonPagedPool == PoolType) ? TAG_GENERIC_NON_PAGED : TAG_GENERIC_PAGED);

        if (NULL != pBuffer) {
            RtlZeroMemory(pBuffer, usMaxLength);
            RtlInitEmptyUnicodeString(Dest, pBuffer, usMaxLength);
            // RtlCopyUnicodeString sets null terminator at the end of buffer
            // if MaximumLength is greater than Length.
            RtlCopyUnicodeString(Dest, Src);
        } else {
            LogMemoryAllocationFailure(usMaxLength, PoolType, SourceLocationCopyUnicodeString);
            Status = STATUS_NO_MEMORY;
        }
    }

    return Status;
}

NTSTATUS
InMageCopyUnicodeString(
    PUNICODE_STRING Dest,
    PWCHAR          Src,
    POOL_TYPE       PoolType
    )
{
    NTSTATUS    Status = STATUS_SUCCESS;
    size_t      length;

    ASSERT(NULL != Dest);
    ASSERT(NULL != Src);

    if ((Dest == NULL) || (Src == NULL)) {
        return STATUS_INVALID_PARAMETER;
    }

    Status = RtlStringCbLengthW(Src, NTSTRSAFE_UNICODE_STRING_MAX_CCH * sizeof(WCHAR), &length);
    if (STATUS_SUCCESS == Status) {
        if (0 == length) {
            Dest->Length = 0;
        } else if (Dest->MaximumLength >= length) {
            Dest->Length = 0;
            RtlAppendUnicodeToString(Dest, Src);
        } else {
            USHORT usMaxLength = (USHORT)(length + UNICODE_STRING_ADDITIONAL_BYTES);

            if (Dest->Buffer) {
                ExFreePool(Dest->Buffer);
                RtlInitEmptyUnicodeString(Dest, NULL, 0);
            }

            ASSERT((NonPagedPool == PoolType) || (PagedPool == PoolType));
            PWCHAR  pBuffer = (PWCHAR)ExAllocatePoolWithTag(PoolType, 
                                                    usMaxLength, 
                                                    (NonPagedPool == PoolType) ? TAG_GENERIC_NON_PAGED : TAG_GENERIC_PAGED);

            if (NULL != pBuffer) {
                RtlZeroMemory(pBuffer, usMaxLength);
                RtlInitEmptyUnicodeString(Dest, pBuffer, usMaxLength);
                // RtlCopyUnicodeString sets null terminator at the end of buffer
                // if MaximumLength is greater than Length.
                RtlAppendUnicodeToString(Dest, Src);
            } else {
                LogMemoryAllocationFailure(usMaxLength, PoolType, SourceLocationInmCopyUniString);
                Status = STATUS_NO_MEMORY;
            }
        }
    }

    return Status;
}

NTSTATUS
InMageAppendUnicodeString(
    PUNICODE_STRING Dest,
    PUNICODE_STRING Src,
    POOL_TYPE       PoolType
    )
{
    NTSTATUS    Status = STATUS_SUCCESS;

    ASSERT(NULL != Dest);
    ASSERT(NULL != Src);
    ASSERT((NULL == Dest->Buffer) || (0 != Dest->MaximumLength));

    if ((Dest == NULL) || (Src == NULL)) {
        return STATUS_INVALID_PARAMETER;
    }

    if (0 == Src->Length) {
        return STATUS_SUCCESS;
    }

    if (Dest->MaximumLength < Src->Length + Dest->Length + sizeof(WCHAR)) {
        ASSERT((NonPagedPool == PoolType) || (PagedPool == PoolType));
        USHORT  usLength = Src->Length + Dest->Length + UNICODE_STRING_ADDITIONAL_BYTES;
        PWCHAR  pBuffer = (PWCHAR)ExAllocatePoolWithTag(PoolType, 
                                                usLength, 
                                                (NonPagedPool == PoolType) ? TAG_GENERIC_NON_PAGED : TAG_GENERIC_PAGED);
        if (NULL == pBuffer) {
            LogMemoryAllocationFailure(usLength, PoolType, SourceLocationInmAppendUniString);
            return STATUS_NO_MEMORY;
        }

        RtlZeroMemory(pBuffer, usLength);

        if (NULL != Dest->Buffer) {
            if (0 != Dest->Length) {
                RtlCopyMemory(pBuffer, Dest->Buffer, Dest->Length);
            }
            ExFreePool(Dest->Buffer);
        }

        InMageInitUnicodeString(Dest, pBuffer, usLength, Dest->Length);
    }
    
    Status = RtlAppendUnicodeStringToString(Dest, Src);
    ASSERT(STATUS_SUCCESS == Status);

    return Status;
}

NTSTATUS
InMageAppendUnicodeString(
    PUNICODE_STRING Dest,
    PCWSTR          Src,
    POOL_TYPE       PoolType
    )
{
    NTSTATUS    Status = STATUS_SUCCESS;
    size_t      length;

    ASSERT(NULL != Dest);
    ASSERT(NULL != Src);
    ASSERT((NULL == Dest->Buffer) || (0 != Dest->MaximumLength));

    if ((Dest == NULL) || (Src == NULL)) {
        return STATUS_INVALID_PARAMETER;
    }

    Status = RtlStringCbLengthW(Src, NTSTRSAFE_UNICODE_STRING_MAX_CCH * sizeof(WCHAR), &length);
    if ((STATUS_SUCCESS == Status) && (0 != length)) {
        if (Dest->MaximumLength < (length + Dest->Length + sizeof(WCHAR))) {
            ASSERT((NonPagedPool == PoolType) || (PagedPool == PoolType));
            USHORT  usMaxLength = (USHORT)(length + Dest->Length + UNICODE_STRING_ADDITIONAL_BYTES);
            PWCHAR  pBuffer = (PWCHAR)ExAllocatePoolWithTag(PoolType, 
                                                    usMaxLength, 
                                                    (NonPagedPool == PoolType) ? TAG_GENERIC_NON_PAGED : TAG_GENERIC_PAGED);
            if (NULL == pBuffer) {
                LogMemoryAllocationFailure(usMaxLength, PoolType, SourceLocationInmAppendWstrString);
                return STATUS_NO_MEMORY;
            }

            RtlZeroMemory(pBuffer, usMaxLength);
            if (NULL != Dest->Buffer) {
                if (0 != Dest->Length) {

                    RtlCopyMemory(pBuffer, Dest->Buffer, Dest->Length);
                }
                ExFreePool(Dest->Buffer);
            }

            InMageInitUnicodeString(Dest, pBuffer, usMaxLength, Dest->Length);
        }

        RtlCopyMemory(((PUCHAR)Dest->Buffer) + Dest->Length, Src, length);
        Dest->Length = (USHORT)(Dest->Length + length);
        ASSERT(Dest->MaximumLength >= Dest->Length);
    }

    return Status;
}

NTSTATUS
InMageAppendUnicodeString(
    PUNICODE_STRING Dest,
    PCWSTR          Src,
    ULONG           ulLen,
    POOL_TYPE       PoolType
    )
{
    NTSTATUS    Status = STATUS_SUCCESS;

    ASSERT(NULL != Dest);
    ASSERT(NULL != Src);
    ASSERT((NULL == Dest->Buffer) || (0 != Dest->MaximumLength));

    if (0 == ulLen) {
        return STATUS_SUCCESS;
    }

    if ((Dest == NULL) || (Src == NULL)) {
        return STATUS_INVALID_PARAMETER;
    }

    if (Dest->MaximumLength < (ulLen + Dest->Length + sizeof(WCHAR))) {
        ASSERT((NonPagedPool == PoolType) || (PagedPool == PoolType));
        USHORT  usMaxLength = (USHORT)(ulLen + Dest->Length + UNICODE_STRING_ADDITIONAL_BYTES);
        PWCHAR  pBuffer = (PWCHAR)ExAllocatePoolWithTag(PoolType, 
                                                usMaxLength, 
                                                (NonPagedPool == PoolType) ? TAG_GENERIC_NON_PAGED : TAG_GENERIC_PAGED);
        if (NULL == pBuffer) {
            LogMemoryAllocationFailure(usMaxLength, PoolType, SourceLocationInmAppendWstrLenStr);
            return STATUS_NO_MEMORY;
        }

        RtlZeroMemory(pBuffer, usMaxLength);
        if (NULL != Dest->Buffer) {
            if (0 != Dest->Length) {
                RtlCopyMemory(pBuffer, Dest->Buffer, Dest->Length);
            }
            ExFreePool(Dest->Buffer);
        }

        InMageInitUnicodeString(Dest, pBuffer, usMaxLength, Dest->Length);
    }


    RtlCopyMemory(((PUCHAR)Dest->Buffer) + Dest->Length, Src, ulLen);
    Dest->Length = (USHORT)(Dest->Length + ulLen);
    ASSERT(Dest->MaximumLength >= Dest->Length);

    return Status;
}

NTSTATUS
InMageAppendUnicodeString(
    PUNICODE_STRING Dest,
    WCHAR           wChar,
    POOL_TYPE       PoolType
    )
{
    NTSTATUS    Status = STATUS_SUCCESS;

    ASSERT(NULL != Dest);
    ASSERT((NULL == Dest->Buffer) || (0 != Dest->MaximumLength));

    if (Dest == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    if (Dest->MaximumLength < (Dest->Length + sizeof(WCHAR))) {
        ASSERT((NonPagedPool == PoolType) || (PagedPool == PoolType));
        USHORT  usMaxLength = (USHORT)(sizeof(WCHAR) + Dest->Length + UNICODE_STRING_ADDITIONAL_BYTES);
        PWCHAR  pBuffer = (PWCHAR)ExAllocatePoolWithTag(PoolType, 
                                                    usMaxLength, 
                                                    (NonPagedPool == PoolType) ? TAG_GENERIC_NON_PAGED : TAG_GENERIC_PAGED);
        if (NULL == pBuffer) {
            LogMemoryAllocationFailure(usMaxLength, PoolType, SourceLocationInmAppendWchString);
            return STATUS_NO_MEMORY;
        }

        RtlZeroMemory(pBuffer, usMaxLength);
        if (NULL != Dest->Buffer) {
            if (0 != Dest->Length) {
                RtlCopyMemory(pBuffer, Dest->Buffer, Dest->Length);
            }
            ExFreePool(Dest->Buffer);
        }

        InMageInitUnicodeString(Dest, pBuffer, usMaxLength, Dest->Length);
    }

 
    RtlCopyMemory(((PUCHAR)Dest->Buffer) + Dest->Length, &wChar, sizeof(WCHAR));
    Dest->Length += (USHORT)sizeof(WCHAR);
    ASSERT(Dest->MaximumLength >= Dest->Length);

    return Status;
}

void
InMageInitUnicodeString(
    PUNICODE_STRING Dest
    )
{
    Dest->Buffer = NULL;
    Dest->Length = 0;
    Dest->MaximumLength = 0;
}

void
InMageInitUnicodeString(
    PUNICODE_STRING Dest,
    PWCHAR          Buffer,
    USHORT          usMaxLength,
    USHORT          usLength
    )
{
    Dest->Buffer = Buffer;
    Dest->Length = usLength;
    Dest->MaximumLength = usMaxLength;
}

NTSTATUS
InMageInitUnicodeString(
    PUNICODE_STRING Dest,
    PWCHAR          Src,
    POOL_TYPE       PoolType
    )
{
    NTSTATUS    Status;
    size_t      length;

    RtlInitEmptyUnicodeString(Dest, NULL, 0);

    if (NULL == Src) {
        return STATUS_SUCCESS;
    }

    Status = RtlStringCbLengthW((NTSTRSAFE_PCWSTR)Src, NTSTRSAFE_UNICODE_STRING_MAX_CCH * sizeof(WCHAR), &length);
    if (STATUS_SUCCESS == Status) {
        ASSERT((NonPagedPool == PoolType) || (PagedPool == PoolType));
        USHORT  usLength = (USHORT)(length + UNICODE_STRING_ADDITIONAL_BYTES);
        PWCHAR  pBuffer = (PWCHAR)ExAllocatePoolWithTag(PoolType, 
                                                usLength, 
                                                (NonPagedPool == PoolType) ? TAG_GENERIC_NON_PAGED : TAG_GENERIC_PAGED);
        if (NULL == pBuffer) {
            LogMemoryAllocationFailure(usLength, PoolType, SourceLocationInitUnicodeString);
            return STATUS_NO_MEMORY;
        }

        RtlZeroMemory(pBuffer, usLength);
 
        RtlCopyMemory(pBuffer, Src, length);
        InMageInitUnicodeString(Dest, pBuffer, usLength, (USHORT)length);
    }

    return STATUS_SUCCESS;
}

void
InMageFreeUnicodeString(
    PUNICODE_STRING String
    )
{
    if (NULL != String->Buffer) {
        ExFreePool(String->Buffer);
        String->Buffer = NULL;
        String->Length = 0;
        String->MaximumLength = 0;
    }

    return;
}

NTSTATUS
InMageTerminateUnicodeStringWith(
    PUNICODE_STRING Dst, 
    WCHAR wChar, 
    POOL_TYPE PoolType
    )
{
    NTSTATUS    Status = STATUS_SUCCESS;

    ASSERT(NULL != Dst);

    if ((NULL == Dst->Buffer) || (0 == Dst->Length)) {
        Status = InMageAppendUnicodeString(Dst, wChar, PoolType);
        return Status;
    }

    if (wChar != Dst->Buffer[Dst->Length/sizeof(WCHAR) - 1]) {
        Status = InMageAppendUnicodeString(Dst, wChar, PoolType);
        return Status;
    }

    return Status;
}

NTSTATUS
InMageAppendUnicodeStringWithFormat(
    PUNICODE_STRING Dst,
    POOL_TYPE   PoolType,
    LPCWSTR     Format,
    ...
    )
{
    NTSTATUS Status = STATUS_SUCCESS;
    va_list arglist;
    const ULONG ulMaxRetryCount = 10;
    ULONG   ulRetryCount = 0;

    va_start(arglist, Format);

    if (NULL == Dst->Buffer) {
        Status = InMageIncreaseUnicodeStringMaxLength(Dst, UNICODE_STRING_ADDITIONAL_BYTES * 4, PoolType);
        if (!NT_SUCCESS(Status)) {
            return Status;
        }
    }

    do {
        PWCHAR  pBuffer = (PWCHAR)((PUCHAR)Dst->Buffer + Dst->Length);
        size_t  CchRemaining = 0;

        ASSERT(NULL != pBuffer); // Disables PreFast warning 28183 about pBuffer
        Status = RtlStringCchVPrintfExW(pBuffer, 
                                        (Dst->MaximumLength - Dst->Length) / sizeof(WCHAR), 
                                        NULL, 
                                        &CchRemaining, 
                                        STRSAFE_NO_TRUNCATION,
                                        Format,
                                        arglist);
        if (NT_SUCCESS(Status)) {
			Dst->Length = (USHORT)(Dst->MaximumLength - (CchRemaining * sizeof(WCHAR)));
        } else {
            NTSTATUS AllocStatus = InMageIncreaseUnicodeStringMaxLength(Dst, 0, PoolType);
            if (!NT_SUCCESS(AllocStatus)) {
                return AllocStatus;
            }
        }

        ulRetryCount++;
    } while ((STATUS_SUCCESS != Status) && (ulRetryCount < ulMaxRetryCount));

    va_end(arglist);

    return Status;
}

NTSTATUS
InMageCopyDosPathAsNTPath(
    PUNICODE_STRING Dst,
    PUNICODE_STRING Src
    )
{
    NTSTATUS    Status = STATUS_SUCCESS;

    if ( (Src->Length >= (2*sizeof(WCHAR))) &&
         (TRUE == IS_DRIVE_LETTER(Src->Buffer[0])) &&
         (L':' == Src->Buffer[1]) )
    {
        Status = InMageCopyUnicodeString(Dst, DOS_DEVICES_PATH);
        if (!NT_SUCCESS(Status)) {
            return Status;
        }

        Status = InMageAppendUnicodeString(Dst, Src);
        if (!NT_SUCCESS(Status)) {
            return Status;
        }
    } else {
        Status = InMageCopyUnicodeString(Dst, Src);
        if (!NT_SUCCESS(Status)) {
            return Status;
        }
    }

    return Status;
}

WCHAR HexDigitArray[16] = {L'0', L'1', L'2', L'3', L'4', L'5', L'6', L'7', L'8', L'9', L'a', L'b', L'c', L'd', L'e', L'f'};

NTSTATUS
ConvertUlongToWchar(
    ULONG UNum,
    WCHAR *WStr,
    ULONG WStrLen
    )
{
    LONG Size = 0;
    ULONG Val = UNum;
    ULONG HexDigit = 0;
	BOOLEAN bLoopAlwaysTrue = TRUE;

    while (bLoopAlwaysTrue) { //Fixed W4 Warnings. AI P3:- Code need to be rewritten to set loop condition appropriately
        Val = Val / 16;
        Size++;
        if (Val == 0)
            break;
    }
    
    ASSERT(Val == 0);
    ASSERT(Size >= 1);
    ASSERT(WStrLen > (ULONG)Size);

    if (((ULONG)Size >= WStrLen) || (Size < 1)) {
        if (WStrLen >= 1) {
            WStr[0] = L'\0';
        }
        return STATUS_UNSUCCESSFUL;
    }

    WStr[Size] = L'\0';
    Size--;
    
    while (Size >= 0) {
        HexDigit = UNum % 16;
        UNum = UNum / 16;
        WStr[Size] = HexDigitArray[HexDigit];
        Size--;
    }
    
    ASSERT(UNum == 0);

    return STATUS_SUCCESS;
}

NTSTATUS
ConvertUlonglongToWchar(
    ULONGLONG UNum,
    WCHAR *WStr,
    ULONG WStrLen
    )
{
    LONG Size = 0;
    ULONGLONG Val = UNum;
    ULONG HexDigit = 0;
	BOOLEAN bLoopAlwaysTrue = TRUE;

    while (bLoopAlwaysTrue) { //Fixed W4 Warnings. AI P3:- Code need to be rewritten to set loop condition appropriately
        Val = Val / 16;
        Size++;
        if (Val == 0)
            break;        
    }
    
    ASSERT(Val == 0);
    ASSERT(Size >= 1);
    ASSERT(WStrLen > (ULONG)Size);

    if (((ULONG)Size >= WStrLen) || (Size < 1)) {
        if (WStrLen >= 1) {
            WStr[0] = L'\0';
        }
        return STATUS_UNSUCCESSFUL;
    }    

    WStr[Size] = L'\0';
    Size--;
    
    while (Size >= 0) {
        HexDigit = (ULONG) (UNum % 16);
        UNum = UNum / 16;
        WStr[Size] = HexDigitArray[HexDigit];
        Size--;
    }
    
    ASSERT(UNum == 0);

    return STATUS_SUCCESS;
}

NTSTATUS
ConvertUlongToWchar (
    __in    ULONG ulValue,
    __in    USHORT usBase,
    __out   PWSTR wBuffer,
    __in    USHORT usBufferLength
    )
/*++

Routine Description:

    Convert Ulong to wchar string.

    This function must be called at IRQL < DISPATCH_LEVEL

Arguments:

    wBuffer:- Pointer to wchar buffer
    usBase:- Base value to be use for converting Ulong to WChar
    chBuffer:- Pointer to char buffer
    usBufferLength:- Number of characters, It not number of bytes. 
                     Buffer should have similar length

Return Value:

    NTSTATUS

--*/
{
    UNICODE_STRING  UniString;
    USHORT          usBufferSize = usBufferLength * sizeof(WCHAR);
    NTSTATUS        Status = STATUS_SUCCESS;

    ASSERT(NULL != wBuffer);
    ASSERT(0 != usBufferLength);
    RtlInitEmptyUnicodeString(&UniString, wBuffer, usBufferSize);
    Status = RtlIntegerToUnicodeString(ulValue,
                                       usBase,
                                       &UniString);
    if (!NT_SUCCESS(Status)) {
        goto Cleanup;
    }
Cleanup:
    return Status;
}

#if DBG
NTSTATUS
ConvertWcharToChar (
    __in    PWSTR wBuffer,
    __inout PCHAR chBuffer,
    __in    USHORT usBufferLength
    )
/*++

Routine Description:

    Convert Wide Character string to Ansi Char string.

    This function must be called at IRQL < DISPATCH_LEVEL

Arguments:

    wBuffer:- Pointer to wchar buffer
    chBuffer:- Pointer to char buffer
    usBufferLength:- Number of characters, It not number of bytes. 
                     Buffer should have similar length

Return Value:

    NTSTATUS

--*/
{
    UNICODE_STRING   UniString;
    ANSI_STRING      AnsiString;
    NTSTATUS         Status = STATUS_SUCCESS;

    ASSERT(NULL != wBuffer);
    ASSERT(NULL != chBuffer);
    ASSERT(0 != usBufferLength);
    
    if (L'\0' != wBuffer[usBufferLength - 1]) {
        Status = STATUS_INVALID_PARAMETER_1;
        goto Cleanup;
    }
    RtlUnicodeStringInit(&UniString, wBuffer);
    RtlInitEmptyAnsiString(&AnsiString, chBuffer, usBufferLength * sizeof(CHAR));
    Status = RtlUnicodeStringToAnsiString(&AnsiString, &UniString, 0);
    if (!NT_SUCCESS(Status)) {
        goto Cleanup;
    }
Cleanup:
    return Status;
}
#endif


PCCHAR
PoolTypeToString(POOL_TYPE PoolType)
{
    switch (PoolType) {
    case PagedPool:
        return "PagedPool";
    case NonPagedPool:
        return "NonPagedPool";
    case NonPagedPoolMustSucceed:
        return "NonPagedPoolMustSucceed";
    case NonPagedPoolCacheAligned:
        return "NonPagedPoolCacheAligned";
    case PagedPoolCacheAligned:
        return "PagedPoolCacheAligned";
    case NonPagedPoolCacheAlignedMustS:
        return "NonPagedPoolCacheAlignedMustS";
    case NonPagedPoolSession:
        return "NonPagedPoolSession";
    case PagedPoolSession:
        return "PagedPoolSession";
    case NonPagedPoolMustSucceedSession:
        return "NonPagedPoolMustSucceedSession";
    case DontUseThisTypeSession:
        return "DontUseThisTypeSession";
    case NonPagedPoolCacheAlignedSession:
        return "NonPagedPoolCacheAlignedSession";
    case PagedPoolCacheAlignedSession:
        return "PagedPoolCacheAlignedSession";
    case NonPagedPoolCacheAlignedMustSSession:
        return "NonPagedPoolCacheAlignedMustSSession";
    }

    return "Unknown";
}

PCWCHAR
PoolTypeToStringW(POOL_TYPE PoolType)
{
    switch (PoolType) {
    case PagedPool:
        return L"PagedPool";
    case NonPagedPool:
        return L"NonPagedPool";
    case NonPagedPoolMustSucceed:
        return L"NonPagedPoolMustSucceed";
    case NonPagedPoolCacheAligned:
        return L"NonPagedPoolCacheAligned";
    case PagedPoolCacheAligned:
        return L"PagedPoolCacheAligned";
    case NonPagedPoolCacheAlignedMustS:
        return L"NonPagedPoolCacheAlignedMustS";
    case NonPagedPoolSession:
        return L"NonPagedPoolSession";
    case PagedPoolSession:
        return L"PagedPoolSession";
    case NonPagedPoolMustSucceedSession:
        return L"NonPagedPoolMustSucceedSession";
    case DontUseThisTypeSession:
        return L"DontUseThisTypeSession";
    case NonPagedPoolCacheAlignedSession:
        return L"NonPagedPoolCacheAlignedSession";
    case PagedPoolCacheAlignedSession:
        return L"PagedPoolCacheAlignedSession";
    case NonPagedPoolCacheAlignedMustSSession:
        return L"NonPagedPoolCacheAlignedMustSSession";
    }

    return L"Unknown";
}
