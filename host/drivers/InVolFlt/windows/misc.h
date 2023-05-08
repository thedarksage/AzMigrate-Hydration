#pragma once

#include "Registry.h"
#include "DiskFltEvents.h"

#define BIG_ENDIAN      0
#define LITTLE_ENDIAN   1

#define NO_CHECKS   0
#define ADD_TO_EVENT_LOG  1
#define BUG_CHECK   2

#define SafeObDereferenceObject(x)          { if (NULL != (x)) {ObDereferenceObject(x); x = NULL;} }
#define SafeExFreePool(x)                   { if (NULL != (x)) {ExFreePool(x); x = NULL;} }
#define SafeExFreePoolWithTag(x, t)         { if (NULL != (x)) {ExFreePoolWithTag((x), t); x = NULL;} }
#define SafeCloseHandle(h)                  { if (NULL != (h)) {ZwClose(h); h = NULL;} }
#define SafeFreeUnicodeString(us)           { SafeExFreePool((us).Buffer); RtlInitUnicodeString(&(us), NULL);}
#define SafeFreeUnicodeStringWithTag(us, t) { SafeExFreePoolWithTag((us).Buffer, t); RtlInitUnicodeString(&(us), NULL);}
#define DeclareUnicodeString(_StringName, _StringValue)   UNICODE_STRING (_StringName) = {sizeof (_StringValue) - sizeof (UNICODE_NULL), sizeof (_StringValue), (_StringValue)} 
NTSTATUS
ValidatePathForFileName(PUNICODE_STRING pusFileName);

NTSTATUS
GenerateDataFileName(PKDIRTY_BLOCK DirtyBlock);

bool
TestByteOrder();

VOID
ASSERT_CHECK(BOOLEAN Condition, ULONG Level);

void
GenerateTimeStamp(ULONGLONG &TimeInHundNanoSecondsFromJan1601, ULONGLONG &ullSequenceNumber, BOOLEAN bIsLockAcquired = FALSE);

bool
GenerateTimeStampDelta(PKDIRTY_BLOCK DirtyBlock, ULONG &TimeDelta, ULONG &SequenceDelta);

void
GenerateTimeStampDeltaForUnupdatedDB(PKDIRTY_BLOCK DirtyBlock, ULONG &TimeDelta, ULONG &SequenceDelta);

void
GenerateTimeStampForUnupdatedDB(PKDIRTY_BLOCK DirtyBlock);

void
GenerateLastTimeStampTag(PKDIRTY_BLOCK DirtyBlock);

NTSTATUS
GenerateTimeStampTag(PTIME_STAMP_TAG_V2 pTimeStamp);

NTSTATUS
CopyStreamRecord(PVOID pDestionation, PVOID pSource, ULONG ulLength);

ULONG
StreamHeaderSizeFromDataSize(ULONG ulDataLength);

ULONG
GetBufferSizeNeededForTags(PUCHAR pTags, ULONG ulNumberOfTags);

ULONG
DefaultGranularityFromDevSize(LONGLONG llDevSize);

VOID
ConvertGUIDtoLowerCase(PWCHAR   pGUID);
#if DBG
BOOLEAN
IsEntryInList(PLIST_ENTRY pHead, PLIST_ENTRY pTarget);
#endif
//USHORT
//ReturnStringLenWInUSHORT(PWCHAR pSrc);

ULONG
SVRecordSizeForIOSize(ULONG ulSize, ULONG ulMaxDataSizePerDataModeDirtyBlock);

ULONG
SVRecordSizeForTags(PUCHAR pTags);

ULONG
SVFileFormatTrailerSize();

ULONG
SVFileFormatChangesHeaderSize();

ULONG
SVFileFormatHeaderSize();

ULONG
DataFileOffsetToFirstChangeTimeStamp();

ULONG
GetMaxChangeSizeFromBufferSize(ULONG ulBufferSize);

USHORT
ReturnStringLenAInUSHORT(PCHAR pSrc);

VOID
LogMemoryAllocationFailure(size_t size, POOL_TYPE PoolType, SourceLocation SourceLocation);

VOID
LogFileOpenFailure(NTSTATUS Status, PUNICODE_STRING pustrFile, SourceLocation SourceLocation);

VOID
LogDeleteOnCloseFailure(NTSTATUS Status, PUNICODE_STRING pustrFile, SourceLocation SourceLocation);

VOID
GetLastPathComponent(PUNICODE_STRING String, PWCHAR &LastComp, ULONG &uLen);

VOID
GetLastPathComponent(PWCHAR Src, PWCHAR &LastComp, ULONG &uLen);
#if DBG
const char *
WOStateString(etWriteOrderState eWOState);

const char *
VBitmapStateString(etVBitmapState eVBitmapState);
#endif

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
GetVolumeHandle(
    _In_ PDEVICE_OBJECT fileSystemDevObj,
    _In_ bool shouldOpenDirectDevHandle,
    _Outptr_result_nullonfailure_ HANDLE *volumeHandle,
    _Outptr_opt_result_nullonfailure_ PDEVICE_OBJECT *volDevObjTos);