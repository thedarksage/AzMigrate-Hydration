#ifndef __INMAGE_MISC_H__
#define __INMAGE_MISC_H__

#include "Registry.h"

#define BIG_ENDIAN      0
#define LITTLE_ENDIAN   1

#define NO_CHECKS   0
#define ADD_TO_EVENT_LOG  1
#define BUG_CHECK   2

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
StreamRecordSizeFromDataSize(ULONG ulDataLength);

ULONG
StreamHeaderSizeFromDataSize(ULONG ulDataLength);

ULONG
GetBufferSizeNeededForTags(PUCHAR pTags, ULONG ulNumberOfTags);

bool
IsVolumeClusterResource(PWCHAR pGUID);

bool
IsDiskClusterResource(ULONG ulDiskSignature);

ULONG
DefaultGranularityFromVolumeSize(LONGLONG llVolumeSize);

NTSTATUS
GetVolumeSize(
    IN  PDEVICE_OBJECT  TargetDeviceObject,
    OUT LONGLONG        *pllVolumeSize,
    OUT NTSTATUS        *pInMageStatus
    );

BOOLEAN
IsEntryInList(PLIST_ENTRY pHead, PLIST_ENTRY pTarget);

USHORT
ReturnStringLenWInUSHORT(PWCHAR pSrc);

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
LogMemoryAllocationFailure(size_t size, POOL_TYPE PoolType, WCHAR *File, ULONG Line);

VOID
LogFileOpenFailure(NTSTATUS Status, PUNICODE_STRING pustrFile, WCHAR *File, ULONG Line);

VOID
LogDeleteOnCloseFailure(NTSTATUS Status, PUNICODE_STRING pustrFile, WCHAR *File, ULONG Line);

VOID
GetLastPathComponent(PUNICODE_STRING String, PWCHAR &LastComp, ULONG &uLen);

VOID
GetLastPathComponent(PWCHAR Src, PWCHAR &LastComp, ULONG &uLen);

const char *
WOStateString(etWriteOrderState eWOState);

const char *
VBitmapStateString(etVBitmapState eVBitmapState);

#endif // __INMAGE_MISC_H__