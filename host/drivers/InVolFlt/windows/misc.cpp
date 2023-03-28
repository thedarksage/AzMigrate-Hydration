#include "Common.h"

#include "misc.h"
#include "VContext.h"
#include "HelperFunctions.h"
#include <svdparse.h>
#include "WinSvdParse.h"

// Minimumpath is \DosDevices\C:\z
// Minimumpath is \DosDevices\Volume{da175ce3-ee10-11d8-ac16-806d6172696f}\z
#define MINIMUM_PATH_LENGTH_IN_DOSDEVICES_PATH      16
#define MINIMUM_PATH_LENGTH_IN_DOSDEVICES_PATH2     58
#define DRIVE_LETTER_INDEX_IN_DOSDEVICES_PATH       12
#define COLON_INDEX_IN_DOSDEVICES_PATH              13
#define PATH_SEPARATOR_INDEX_IN_DOSDEVICES_PATH     14

// Minimumpath is \??\C:\z
// Minimumpath is \??\Volume{da175ce3-ee10-11d8-ac16-806d6172696f}\z
#define MINIMUM_PATH_LENGTH_IN_DOSDEVICES2_PATH     8
#define MINIMUM_PATH_LENGTH_IN_DOSDEVICES2_PATH2    50
#define DRIVE_LETTER_INDEX_IN_DOSDEVICES2_PATH      4
#define COLON_INDEX_IN_DOSDEVICES2_PATH             5
#define PATH_SEPARATOR_INDEX_IN_DOSDEVICES2_PATH    6

// Minimumpath is C:\z
#define MINIMUM_PATH_LENGTH_IN_DRIVE_LETTER_PATH    4
#define DRIVE_LETTER_INDEX_IN_DRIVE_LETTER_PATH     0
#define COLON_INDEX_IN_DRIVE_LETTER_PATH            1
#define PATH_SEPARATOR_INDEX_IN_DRIVE_LETTER_PATH   2

// MinimumPath is \SystemRoot\x
#define MINIMUM_PATH_LENGTH_IN_SYSTEM_ROOT_PATH     13
#define PATH_SEPARATOR_INDEX_IN_SYSTEM_ROOT_PATH    11

STREAM_REC_HDR_4B   StreamRecEndOfListTag = {STREAM_REC_TYPE_END_OF_TAG_LIST, 0, 4};
STREAM_REC_HDR_4B   StreamRecStartOfListTag = {STREAM_REC_TYPE_START_OF_TAG_LIST, 0, 4};

static ULONGLONG GlobalSeqNum = 0;

ULONG
StreamHeaderSizeFromDataSize(ULONG ulDataLength)
{
    return (ulDataLength + sizeof(STREAM_REC_HDR_4B)) < 0xFF ? sizeof(STREAM_REC_HDR_4B) : sizeof(STREAM_REC_HDR_8B); 
}

NTSTATUS
CopyStreamRecord(PVOID pDestination, PVOID pSource, ULONG ulLength)
{
    NTSTATUS            Status = STATUS_SUCCESS;
    ULONG               ulRecLength = 0;

    ulRecLength = STREAM_REC_SIZE(pSource);
    ASSERT(ulRecLength <= ulLength);
    if (ulRecLength <= ulLength) {
        RtlCopyMemory(pDestination, pSource, ulRecLength);
    } else {
        Status = STATUS_BUFFER_TOO_SMALL;
    }

    return Status;
}

VOID
ASSERT_CHECK(BOOLEAN Condition, ULONG Level)
{
    // Usually, level is set 0. If user wants to cause "BUG_CHECK" for some situation
    // though "ulValidationLevel" is set to "ADD_TO_EVENT_LOG" or "NO_CHECKS", then user can
    // set Level to 2 = BUG_CHECK, in that case this function will cause "BUG_CHECK".

    if (Condition == TRUE) {
        return;
    }

    if (DriverContext.ulValidationLevel == NO_CHECKS && Level == 0) {
        return;
    }

    ASSERT(Condition); // For Debug/Checked build
    
    if (DriverContext.ulValidationLevel >= BUG_CHECK || Level >= BUG_CHECK) {
        ULONG BugCheckCode = ((ULONG)0xDEADDEAD); // MANUALLY_INITIATED_CRASH1
        KeBugCheck(BugCheckCode);
    }
    
    return;
}

void
GenerateTimeStamp(ULONGLONG &TimeInHundNanoSecondsFromJan1601, ULONGLONG &ullSequenceNumber, BOOLEAN bIsLockAcquired)
{
    KIRQL   OldIrql = 0;
    if(!bIsLockAcquired)
        KeAcquireSpinLock(&DriverContext.TimeStampLock, &OldIrql);

    if (DriverContext.ulValidationLevel >= ADD_TO_EVENT_LOG) {
        if (GlobalSeqNum > DriverContext.ullTimeStampSequenceNumber) {
            InDskFltWriteEvent(INDSKFLT_ERROR_SEQUENCE_MISMATCH, GlobalSeqNum, DriverContext.ullTimeStampSequenceNumber, SourceLocationGenTimeStampBeforeQuery);
        }
    }
    ASSERT_CHECK(GlobalSeqNum <= DriverContext.ullTimeStampSequenceNumber, 0);

    InVolDbgPrint(DL_TRACE_L3, DM_TIME_STAMP, ("GenerateTimeStampV2: CurrTimeStamp=%#I64x.%#x ",
        DriverContext.ullLastTimeStamp, (DriverContext.ullTimeStampSequenceNumber)));
    KeQuerySystemTime((PLARGE_INTEGER)&TimeInHundNanoSecondsFromJan1601);
    if (TimeInHundNanoSecondsFromJan1601 <= DriverContext.ullLastTimeStamp) {
        //TODO: The wrapping of the sequence number has to be decided
        //The Highest Value allowed for the sequence number will be 
        // 0xFFFFFFFFFFFFFFFF - (7D00000) = 0xFFFFFFFFF82FFFFF
        // where 128(Max volume/disk)*1000(Number of disk)*1024(delta allowed in dirty block)= 7D00000
        if (DriverContext.ullTimeStampSequenceNumber >= 0xFFFFFFFFF82FFFFF) {
            DriverContext.ullLastTimeStamp++;
            DriverContext.ullTimeStampSequenceNumber = 0;
            GlobalSeqNum = 0;
        }
        InVolDbgPrint(DL_INFO, DM_TIME_STAMP, ("GenerateTimeStamp: CurrTimeStamp=%#I64x.%#x NewTimeStamp=%I64x.%#x\n",
           TimeInHundNanoSecondsFromJan1601, ullSequenceNumber,
            DriverContext.ullLastTimeStamp, (DriverContext.ullTimeStampSequenceNumber + 1)));
        TimeInHundNanoSecondsFromJan1601 = DriverContext.ullLastTimeStamp;
    } else {
        if (DriverContext.ullTimeStampSequenceNumber >= 0xFFFFFFFFF82FFFFF) {
            DriverContext.ullTimeStampSequenceNumber = 0;
            GlobalSeqNum = 0;
        }
        DriverContext.ullLastTimeStamp = TimeInHundNanoSecondsFromJan1601;
    }
    ullSequenceNumber = ++DriverContext.ullTimeStampSequenceNumber;
    InVolDbgPrint(DL_TRACE_L3, DM_TIME_STAMP, ("  NewTimeStamp=%I64x.%#x\n",
        DriverContext.ullLastTimeStamp, (DriverContext.ullTimeStampSequenceNumber)));

    if (DriverContext.ulValidationLevel >= ADD_TO_EVENT_LOG) {
        if (GlobalSeqNum > DriverContext.ullTimeStampSequenceNumber) {
            InDskFltWriteEvent(INDSKFLT_ERROR_SEQUENCE_MISMATCH, GlobalSeqNum, DriverContext.ullTimeStampSequenceNumber, SourceLocationGenTimeStampAfterQuery);
        }   
    }
    ASSERT_CHECK(GlobalSeqNum <= DriverContext.ullTimeStampSequenceNumber, 0);

    if (GlobalSeqNum < DriverContext.ullTimeStampSequenceNumber) {
        GlobalSeqNum = DriverContext.ullTimeStampSequenceNumber;
    }

    if(!bIsLockAcquired)
        KeReleaseSpinLock(&DriverContext.TimeStampLock, OldIrql);

    return;
}

//This Function return false if the deltas have overflowed.
//In that case we close the current dirty block.
bool
GenerateTimeStampDelta(PKDIRTY_BLOCK DirtyBlock, ULONG &TimeDelta, ULONG &SequenceDelta)
{
    KIRQL   OldIrql;
    ULONGLONG TimeInHundNanoSecondsFromJan1601;
    bool UseCurrentDirtyBlock = TRUE;
    KeAcquireSpinLock(&DriverContext.TimeStampLock, &OldIrql);

    if (DriverContext.ulValidationLevel >= ADD_TO_EVENT_LOG) {
        if (GlobalSeqNum > DriverContext.ullTimeStampSequenceNumber) {
            InDskFltWriteEvent(INDSKFLT_ERROR_SEQUENCE_MISMATCH, GlobalSeqNum, DriverContext.ullTimeStampSequenceNumber, SourceLocationGenTimeStampDeltaBeforeQuery);
        }   
    }
    ASSERT_CHECK(GlobalSeqNum <= DriverContext.ullTimeStampSequenceNumber, 0);

    KeQuerySystemTime((PLARGE_INTEGER)&TimeInHundNanoSecondsFromJan1601);
    //Update the global time only if the current time increased wrt to the global time.
    if (TimeInHundNanoSecondsFromJan1601 > DriverContext.ullLastTimeStamp) {
        DriverContext.ullLastTimeStamp = TimeInHundNanoSecondsFromJan1601;
    }
    ASSERT(DriverContext.ullLastTimeStamp >= DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
    //Check for the time delta overlflow
    if (((LONGLONG)DriverContext.ullLastTimeStamp -
        (LONGLONG)(DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601) > 0xFFFFFFFF) ||
        ((LONGLONG)(DriverContext.ullTimeStampSequenceNumber + 1) -
        (LONGLONG)(DirtyBlock->TagTimeStampOfFirstChange.ullSequenceNumber) > 0xFFFFFFFF)) {
        UseCurrentDirtyBlock = FALSE;
        InVolDbgPrint(DL_ERROR, DM_TIME_STAMP, ("%s:Delta has been wrapped. Closing the current Dirty Block\n", __FUNCTION__));
    } else {

        // Lets make TimeDelta and Seq# for MetaData DirtyBlock as Zero.

        if (DirtyBlock->eWOState != ecWriteOrderStateData) {
            TimeDelta = 0;
            SequenceDelta = 0;
        } else {
            TimeDelta = (ULONG)((LONGLONG)DriverContext.ullLastTimeStamp - 
                (LONGLONG)(DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601));
            SequenceDelta = (ULONG)((LONGLONG)(++DriverContext.ullTimeStampSequenceNumber) -
                (LONGLONG)(DirtyBlock->TagTimeStampOfFirstChange.ullSequenceNumber));
        }
    }

    if (DriverContext.ulValidationLevel >= ADD_TO_EVENT_LOG) {
        if (GlobalSeqNum > DriverContext.ullTimeStampSequenceNumber) {
            InDskFltWriteEvent(INDSKFLT_ERROR_SEQUENCE_MISMATCH, GlobalSeqNum, DriverContext.ullTimeStampSequenceNumber, SourceLocationGenTimeStampDeltaAfterQuery);
        }   
    }
    ASSERT_CHECK(GlobalSeqNum <= DriverContext.ullTimeStampSequenceNumber, 0);
    if (GlobalSeqNum < DriverContext.ullTimeStampSequenceNumber) {
        GlobalSeqNum = DriverContext.ullTimeStampSequenceNumber;
    }
    
    KeReleaseSpinLock(&DriverContext.TimeStampLock, OldIrql);

    return UseCurrentDirtyBlock;
}

void
GenerateTimeStampDeltaForUnupdatedDB(PKDIRTY_BLOCK DirtyBlock, ULONG &TimeDelta, ULONG &SequenceDelta)
{
    //we cannot calculate the deltas using the global as they are not updated yet
    TimeDelta = 0;
    //this will give the deltas local to the Dev

    // Lets make SeqDelta for non-Data mode (non-WOS) DirtyBlock as Zero.
    if (DirtyBlock->eWOState != ecWriteOrderStateData) {
        SequenceDelta = 0;
    } else {
        SequenceDelta = DirtyBlock->cChanges;
    }
    InVolDbgPrint(DL_ERROR, DM_TIME_STAMP, ("%s:Generated time stamp of Temp Queue blocks\n", __FUNCTION__));

    return;
}

void
GenerateTimeStampForUnupdatedDB(PKDIRTY_BLOCK DirtyBlock)
{
    KIRQL   OldIrql;
    PTIME_STAMP_TAG_V2 pFirstTimeStamp = &DirtyBlock->TagTimeStampOfFirstChange ;
    PTIME_STAMP_TAG_V2 pLastTimeStamp = &DirtyBlock->TagTimeStampOfLastChange ;

    FILL_STREAM_HEADER_4B(&pFirstTimeStamp->Header, STREAM_REC_TYPE_TIME_STAMP_TAG, sizeof(TIME_STAMP_TAG_V2));
    FILL_STREAM_HEADER_4B(&pLastTimeStamp->Header, STREAM_REC_TYPE_TIME_STAMP_TAG, sizeof(TIME_STAMP_TAG_V2));

    KeAcquireSpinLock(&DriverContext.TimeStampLock, &OldIrql);

    //Getting the current timestamp and Sequence Number for first time stamp
    GenerateTimeStamp(pFirstTimeStamp->TimeInHundNanoSecondsFromJan1601, pFirstTimeStamp->ullSequenceNumber, TRUE);

    //Updating the last time stamp also the same as all the time deltas are zero
    pLastTimeStamp->TimeInHundNanoSecondsFromJan1601 = pFirstTimeStamp->TimeInHundNanoSecondsFromJan1601;

    //Now updating the last sequence number
    if (DirtyBlock->cChanges) {
        pLastTimeStamp->ullSequenceNumber = pFirstTimeStamp->ullSequenceNumber + DirtyBlock->cChanges - 1;
    } else {
        // If there are no changes in this dirty block, say only tags..
        pLastTimeStamp->ullSequenceNumber = pFirstTimeStamp->ullSequenceNumber + DirtyBlock->cChanges;
    }

    //Update the global sequence number
    if (DriverContext.ulValidationLevel >= ADD_TO_EVENT_LOG) {
        if (GlobalSeqNum > DriverContext.ullTimeStampSequenceNumber) {
            InDskFltWriteEvent(INDSKFLT_ERROR_SEQUENCE_MISMATCH, GlobalSeqNum, DriverContext.ullTimeStampSequenceNumber, SourceLocationGenTimeStampUnupdDBBeforeUpd);
        }   
    }
    ASSERT_CHECK(GlobalSeqNum <= DriverContext.ullTimeStampSequenceNumber, 0);

    DriverContext.ullTimeStampSequenceNumber = pLastTimeStamp->ullSequenceNumber;

    if (DriverContext.ulValidationLevel >= ADD_TO_EVENT_LOG) {
        if (GlobalSeqNum > DriverContext.ullTimeStampSequenceNumber) {
            InDskFltWriteEvent(INDSKFLT_ERROR_SEQUENCE_MISMATCH, GlobalSeqNum, DriverContext.ullTimeStampSequenceNumber, SourceLocationGenTimeStampUnupdDBAfterUpd);
        }   
    }
    ASSERT_CHECK(GlobalSeqNum <= DriverContext.ullTimeStampSequenceNumber, 0);
    if (GlobalSeqNum < DriverContext.ullTimeStampSequenceNumber) {
        GlobalSeqNum = DriverContext.ullTimeStampSequenceNumber;
    }

    
    KeReleaseSpinLock(&DriverContext.TimeStampLock, OldIrql);

    return ;
}


void
GenerateLastTimeStampTag(PKDIRTY_BLOCK DirtyBlock)
{
    PTIME_STAMP_TAG_V2 pFirstTimeStamp = &DirtyBlock->TagTimeStampOfFirstChange ;
    PTIME_STAMP_TAG_V2 pLastTimeStamp = &DirtyBlock->TagTimeStampOfLastChange ;
    ULONG ulIndex = DirtyBlock->cChanges;

    FILL_STREAM_HEADER_4B(&pLastTimeStamp->Header, STREAM_REC_TYPE_TIME_STAMP_TAG, sizeof(TIME_STAMP_TAG_V2));
    if(ulIndex > 0){//This means that the Dirty block has the changes
        --ulIndex;//going back to the actual index which was filed in the last io for this dirty block
        if (ulIndex < MAX_CHANGES_EMB_IN_DB) {
            pLastTimeStamp->ullSequenceNumber = pFirstTimeStamp->ullSequenceNumber + DirtyBlock->SequenceDelta[ulIndex];
            pLastTimeStamp->TimeInHundNanoSecondsFromJan1601 = pFirstTimeStamp->TimeInHundNanoSecondsFromJan1601 + 
                                                               DirtyBlock->TimeDelta[ulIndex];
        } else {
            PCHANGE_BLOCK ChangeBlock = DirtyBlock->CurrentChangeBlock;

            ASSERT(ChangeBlock != NULL);
            //Change due to Bug #9935
            //ASSERT(ulIndex >= ChangeBlock->usFirstIndex);
            if(ulIndex < ChangeBlock->usFirstIndex){
                PCHANGE_BLOCK TempChangeBlock = DirtyBlock->ChangeBlockList;
                while(TempChangeBlock->Next != ChangeBlock)
                    TempChangeBlock = TempChangeBlock->Next;
                
                ChangeBlock = TempChangeBlock;
                ASSERT(ulIndex == ChangeBlock->usLastIndex);
            }
            

            ulIndex -= ChangeBlock->usFirstIndex;
            pLastTimeStamp->ullSequenceNumber = pFirstTimeStamp->ullSequenceNumber + ChangeBlock->SequenceDelta[ulIndex];
            pLastTimeStamp->TimeInHundNanoSecondsFromJan1601 = pFirstTimeStamp->TimeInHundNanoSecondsFromJan1601 + 
                                                               ChangeBlock->TimeDelta[ulIndex];
        }
    }else{//this means that the Dirty block is closed without any changes in it
        //As there is no change so make them equal to the first time stamp
        pLastTimeStamp->ullSequenceNumber = pFirstTimeStamp->ullSequenceNumber;
        pLastTimeStamp->TimeInHundNanoSecondsFromJan1601 = pFirstTimeStamp->TimeInHundNanoSecondsFromJan1601;
    }

    if (DriverContext.ulValidationLevel >= ADD_TO_EVENT_LOG) {
        if (pLastTimeStamp->ullSequenceNumber < pFirstTimeStamp->ullSequenceNumber) {
            InDskFltWriteEvent(INDSKFLT_ERROR_FIRST_LAST_SEQUENCE_MISMATCH, 
                pFirstTimeStamp->ullSequenceNumber,
                pLastTimeStamp->ullSequenceNumber,
                SourceLocationGenLastTimeStampTag);
        }
    }
    ASSERT_CHECK(pLastTimeStamp->ullSequenceNumber >= pFirstTimeStamp->ullSequenceNumber, 0);

    if (DriverContext.ulValidationLevel >= ADD_TO_EVENT_LOG) {
        if (pLastTimeStamp->TimeInHundNanoSecondsFromJan1601 < pFirstTimeStamp->TimeInHundNanoSecondsFromJan1601) {
            InDskFltWriteEvent(INDSKFLT_ERROR_FIRST_LAST_TIME_MISMATCH,
                pFirstTimeStamp->TimeInHundNanoSecondsFromJan1601,
                pLastTimeStamp->TimeInHundNanoSecondsFromJan1601,
                SourceLocationGenLastTimeStampTag);
        }
    }
    ASSERT_CHECK(pLastTimeStamp->TimeInHundNanoSecondsFromJan1601 >= pFirstTimeStamp->TimeInHundNanoSecondsFromJan1601, 0);    
    
    return ;
}

NTSTATUS
GenerateTimeStampTag(PTIME_STAMP_TAG_V2 pTimeStamp)
{

    ASSERT(NULL != pTimeStamp);

    if (NULL == pTimeStamp)
        return STATUS_INVALID_PARAMETER;

    FILL_STREAM_HEADER_4B(&pTimeStamp->Header, STREAM_REC_TYPE_TIME_STAMP_TAG, sizeof(TIME_STAMP_TAG_V2));
    GenerateTimeStamp(pTimeStamp->TimeInHundNanoSecondsFromJan1601, pTimeStamp->ullSequenceNumber);

    return STATUS_SUCCESS;
}

NTSTATUS
GenerateDataFileName(PKDIRTY_BLOCK DirtyBlock)
{
    ASSERT(NULL != DirtyBlock);
    PDEV_CONTEXT DevContext = DirtyBlock->DevContext;
    NTSTATUS    Status = STATUS_SUCCESS;

    ASSERT(0 == DirtyBlock->FileName.Length);

    if (0 == DevContext->DataLogDirectory.Length) {
        // Log failure to save data to file due to failure to find DataLogDirectory
        InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("%s: data log directory is empty", __FUNCTION__));
        return STATUS_INVALID_PARAMETER;
    }

    if (0 == (DevContext->ulFlags & DCF_DATALOG_DIRECTORY_VALIDATED)) {
        Status = ValidatePathForFileName(&DevContext->DataLogDirectory);
        if (NT_SUCCESS(Status)) {
            DevContext->ulFlags |= DCF_DATALOG_DIRECTORY_VALIDATED;
        } else {
            InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("%s: Failed to validate data log directory", __FUNCTION__));
        }
    }

    Status = InMageCopyUnicodeString(&DirtyBlock->FileName, 
                                     &DevContext->DataLogDirectory,
                                     NonPagedPool);
    if (!NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("%s: Failed to copy data log directory\n", __FUNCTION__));
        return Status;
    }

    ASSERT('\\' == DirtyBlock->FileName.Buffer[DirtyBlock->FileName.Length/sizeof(WCHAR) - 1]);

    PWCHAR ContinuationTag;
    if(DirtyBlock->eWOState == ecWriteOrderStateData)
    {
        if (DirtyBlock->ulFlags & (KDIRTY_BLOCK_FLAG_PART_OF_SPLIT_CHANGE | KDIRTY_BLOCK_FLAG_START_OF_SPLIT_CHANGE)) {
            ContinuationTag = L"WC";
        } else {
            ContinuationTag = L"WE";
        }
    } else {
        if (DirtyBlock->ulFlags & (KDIRTY_BLOCK_FLAG_PART_OF_SPLIT_CHANGE | KDIRTY_BLOCK_FLAG_START_OF_SPLIT_CHANGE)) {
            ContinuationTag = L"MC";
        } else {
            ContinuationTag = L"ME";
        }
    }
    Status = InMageAppendUnicodeStringWithFormat(&DirtyBlock->FileName,
                                                 NonPagedPool,
                                                 L"pre_completed_diff_S%I64d_%I64d_E%I64d_%I64d_%s%d.dat",
                                                 DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601,
                                                 DirtyBlock->TagTimeStampOfFirstChange.ullSequenceNumber,
                                                 DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601,
                                                 DirtyBlock->TagTimeStampOfLastChange.ullSequenceNumber,
                                                 ContinuationTag,
                                                 DirtyBlock->ulSequenceIdForSplitIO);

    InVolDbgPrint(DL_TRACE_L1, DM_DATA_FILTERING, ("%s: Data file name : %wZ\n", __FUNCTION__, &DirtyBlock->FileName));

    return Status;
}

ULONG
DefaultGranularityFromDevSize(LONGLONG llDevSize)
{
    ULONG   ulBitmapGranularity = 0;

    if ((llDevSize / GIGABYTES) > DriverContext.Bitmap512KGranularitySize) {
        ulBitmapGranularity = SIXTEEN_K_SIZE;
    } else {
        ulBitmapGranularity = FOUR_K_SIZE;
    }

    return ulBitmapGranularity;
}

NTSTATUS
ValidatePathForFileName(PUNICODE_STRING pusFileName)
{
    UNICODE_STRING  usLocalFileName;
    UNICODE_STRING  usDirName;
    UNICODE_STRING  usDosDevices;
    UNICODE_STRING  usSystemDir;
    NTSTATUS        Status;
    WCHAR           DriveLetter;
    ULONG           IndexOfPathSeparator, MaxChars;

    if (NULL == pusFileName)
        return STATUS_INVALID_PARAMETER;

    if ((0 == pusFileName->Length) || (pusFileName->MaximumLength < pusFileName->Length))
        return STATUS_INVALID_PARAMETER;

    RtlInitEmptyUnicodeString(&usLocalFileName, NULL, 0);

    RtlInitUnicodeString(&usDosDevices, DOS_DEVICES_PATH2);
    if (FALSE == RtlPrefixUnicodeString(&usDosDevices, pusFileName, TRUE)) {
        RtlInitUnicodeString(&usDosDevices, DOS_DEVICES_PATH);
        if (FALSE == RtlPrefixUnicodeString(&usDosDevices, pusFileName, TRUE)) {
            // Path does not have //DosDevices// as prefix
            RtlInitUnicodeString(&usSystemDir, SYSTEM_ROOT_PATH);
            if (FALSE == RtlPrefixUnicodeString(&usSystemDir, pusFileName, TRUE)) {
                if (MINIMUM_PATH_LENGTH_IN_DRIVE_LETTER_PATH > pusFileName->Length)
                    return STATUS_SUCCESS;
    
                DriveLetter = pusFileName->Buffer[DRIVE_LETTER_INDEX_IN_DRIVE_LETTER_PATH];
                if (FALSE == IS_DRIVE_LETTER(DriveLetter))
                    return STATUS_SUCCESS;
    
                if (L':' != pusFileName->Buffer[COLON_INDEX_IN_DRIVE_LETTER_PATH])
                    return STATUS_SUCCESS;
    
                if (L'\\' != pusFileName->Buffer[COLON_INDEX_IN_DRIVE_LETTER_PATH + 1])
                    return STATUS_SUCCESS;
    
                // Volume name starts with drive letter.
                Status = InMageAppendUnicodeString(&usLocalFileName, DOS_DEVICES_PATH);
                if (!NT_SUCCESS(Status))
                    return STATUS_SUCCESS;
    
                // Copy the DosDevices\*:\DirName
                IndexOfPathSeparator = MINIMUM_PATH_LENGTH_IN_DOSDEVICES_PATH + 1;
            } else {
                // Path starts with //SystemRoot//
                if (MINIMUM_PATH_LENGTH_IN_SYSTEM_ROOT_PATH > pusFileName->Length) {
                    return STATUS_SUCCESS;
                }
    
                // Copy the \SystemRoot\DirName
                IndexOfPathSeparator = MINIMUM_PATH_LENGTH_IN_SYSTEM_ROOT_PATH + 1;
            }
        } else {
            if (MINIMUM_PATH_LENGTH_IN_DOSDEVICES_PATH > pusFileName->Length)
                return STATUS_SUCCESS;
    
            UNICODE_STRING  usVolumePrefix;
            RtlInitUnicodeString(&usVolumePrefix, L"\\DosDevices\\Volume{");

            if (RtlPrefixUnicodeString(&usVolumePrefix, pusFileName, TRUE)) {
                IndexOfPathSeparator = MINIMUM_PATH_LENGTH_IN_DOSDEVICES_PATH2 + 1;
            } else {
                DriveLetter = pusFileName->Buffer[DRIVE_LETTER_INDEX_IN_DOSDEVICES_PATH];
                if (FALSE == IS_DRIVE_LETTER(DriveLetter))
                    return STATUS_SUCCESS;
        
                if (L':' != pusFileName->Buffer[COLON_INDEX_IN_DOSDEVICES_PATH])
                    return STATUS_SUCCESS;
        
                if (L'\\' != pusFileName->Buffer[COLON_INDEX_IN_DOSDEVICES_PATH + 1])
                    return STATUS_SUCCESS;
        
                // Copy the DosDevices\*:\DirName
                IndexOfPathSeparator = MINIMUM_PATH_LENGTH_IN_DOSDEVICES_PATH + 1;
            }
        }
    } else {
        if (MINIMUM_PATH_LENGTH_IN_DOSDEVICES2_PATH > pusFileName->Length)
            return STATUS_SUCCESS;

        UNICODE_STRING  usVolumePrefix;
        RtlInitUnicodeString(&usVolumePrefix, L"\\??\\Volume{");

        if (RtlPrefixUnicodeString(&usVolumePrefix, pusFileName, TRUE)) {
            IndexOfPathSeparator = MINIMUM_PATH_LENGTH_IN_DOSDEVICES2_PATH2 + 1;
        } else {
            DriveLetter = pusFileName->Buffer[DRIVE_LETTER_INDEX_IN_DOSDEVICES2_PATH];
            if (FALSE == IS_DRIVE_LETTER(DriveLetter))
                return STATUS_SUCCESS;

            if (L':' != pusFileName->Buffer[COLON_INDEX_IN_DOSDEVICES2_PATH])
                return STATUS_SUCCESS;

            if (L'\\' != pusFileName->Buffer[COLON_INDEX_IN_DOSDEVICES2_PATH + 1])
                return STATUS_SUCCESS;

            // Copy the DosDevices\*:\DirName
            IndexOfPathSeparator = MINIMUM_PATH_LENGTH_IN_DOSDEVICES2_PATH + 1;
        }
    }

    Status = InMageAppendUnicodeString(&usLocalFileName, pusFileName);
    if (!NT_SUCCESS(Status)) {
        return STATUS_SUCCESS;
    }

    usDirName.MaximumLength = usLocalFileName.MaximumLength;
    usDirName.Buffer = usLocalFileName.Buffer;
    usDirName.Length = 0;

    Status = STATUS_SUCCESS;

    MaxChars = usLocalFileName.Length / sizeof(WCHAR);
    while(IndexOfPathSeparator < MaxChars) {
        if (L'\\' == usDirName.Buffer[IndexOfPathSeparator]) {
            OBJECT_ATTRIBUTES   ObjectAttributes;
            HANDLE              hDir;
            IO_STATUS_BLOCK     IoStatus;

            // This is path separator
            usDirName.Length = (USHORT) (IndexOfPathSeparator * sizeof(WCHAR));

            InitializeObjectAttributes ( &ObjectAttributes,
                                            &usDirName,
                                            OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
                                            NULL,
                                            NULL );
            Status = ZwOpenFile(&hDir, FILE_LIST_DIRECTORY, &ObjectAttributes, &IoStatus, FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_DIRECTORY_FILE);
            if (STATUS_SUCCESS != Status) {
                Status = ZwCreateFile(&hDir, FILE_LIST_DIRECTORY, &ObjectAttributes, &IoStatus, NULL, 
                                FILE_ATTRIBUTE_NORMAL, FILE_SHARE_WRITE, FILE_CREATE, FILE_DIRECTORY_FILE, NULL, 0);
                if (NT_SUCCESS(Status)) {
                    InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, ("Created Directory %wZ", &usDirName));
                }
            }

            if (NT_SUCCESS(Status))
            {
                ZwClose(hDir);
            }
            else
                break;
        }

        IndexOfPathSeparator++;
    }

    InMageFreeUnicodeString(&usLocalFileName);

    return Status;
}

ULONG
SVFileFormatChangeHeaderSize()
{
    // In case of 64bit :-0x0C + 0x14 = 0x20
    // In case of 32bit :-0x0C + 0x10 = 0x1C
    return sizeof(SVD_PREFIX) + sizeof(W_SVD_DIRTY_BLOCK);
}

ULONG
SVRecordSizeForIOSize(ULONG ulSize, ULONG ulMaxDataSizePerDataModeDirtyBlock)
{
    ULONG   ulRecordSize;
    ULONG ulMaxChangeSizeInDirtyBlock = GetMaxChangeSizeFromBufferSize(ulMaxDataSizePerDataModeDirtyBlock);
    ULONG ulNumDaBPerDirtyBlock = ulMaxDataSizePerDataModeDirtyBlock / DriverContext.ulDataBlockSize;
    if (ulSize <= ulMaxChangeSizeInDirtyBlock) {
        ulRecordSize = ulSize + SVFileFormatChangeHeaderSize();
    } else {
        // We have to find the memory required for a SplitIO
        // In this case we find the number of data blocks required for this split IO
        ULONG   ulNumDBs = ulSize / ulMaxChangeSizeInDirtyBlock; // Finds full dirty blocks
        // ulNumDaBsInLastDB has the DaBs in last partially filled DB.
        ULONG   ulNumDaBsInLastDB = (ulSize - (ulMaxChangeSizeInDirtyBlock * ulNumDBs)) + 
                                    SVFileFormatHeaderSize() + SVFileFormatChangesHeaderSize() +
                                    SVFileFormatChangeHeaderSize() + SVFileFormatTrailerSize();

        ulNumDaBsInLastDB = (ulNumDaBsInLastDB + DriverContext.ulDataBlockSize - 1) / DriverContext.ulDataBlockSize;
        ULONG ulNumDaBs = (ulNumDBs * ulNumDaBPerDirtyBlock) + ulNumDaBsInLastDB;
        ulRecordSize = ulNumDaBs * DriverContext.ulDataBlockSize;
    }

    return ulRecordSize;
}

ULONG
SVRecordSizeForTags(PUCHAR pTags)
{
    ULONG   ulRecordSize = SVFileFormatHeaderSize() + SVFileFormatChangesHeaderSize();
    ULONG   ulNumberOfTags = *(PULONG)pTags;
    pTags += sizeof(ULONG);

    for(ULONG ul = 0; ul < ulNumberOfTags; ul++) {
        USHORT  usSize = *(USHORT UNALIGNED *)pTags;
        pTags += usSize + sizeof(USHORT);

        ulRecordSize += sizeof(SVD_PREFIX) + usSize;
    }

    return ulRecordSize;
}

ULONG
SVFileFormatHeaderSize()
{
    ULONG   ulHeaderSize = sizeof(SV_UINT) + sizeof(SVD_PREFIX) + sizeof(W_SVD_HEADER);

    // Add First Change Time stamp;
    ulHeaderSize += sizeof(SVD_PREFIX) + sizeof(W_SVD_TIME_STAMP);

    return ulHeaderSize;
}

ULONG
DataFileOffsetToFirstChangeTimeStamp()
{
    return sizeof(SV_UINT) + sizeof(SVD_PREFIX) + sizeof(W_SVD_HEADER);
}

ULONG
SVFileFormatChangesHeaderSize()
{
    // Add SVD_TAG_LENGTH_OF_DRTD_CHANGES (Total length of changes in stream format with headers)
    return sizeof(SVD_PREFIX) + sizeof(ULONGLONG);
}

ULONG
SVFileFormatTrailerSize()
{
    return sizeof(SVD_PREFIX) + sizeof(W_SVD_TIME_STAMP);
}

ULONG
GetMaxChangeSizeFromBufferSize(ULONG ulBufferSize)
{
    ULONG   ulMaxChangeSize = ulBufferSize;
    // Change Size has to be a granular of 4K.

    ulMaxChangeSize -= SVFileFormatChangeHeaderSize() + SVFileFormatChangesHeaderSize() + 
                       SVFileFormatHeaderSize() + SVFileFormatTrailerSize();
    ulMaxChangeSize = (ulMaxChangeSize / FOUR_K_SIZE) * FOUR_K_SIZE;

    return ulMaxChangeSize;
}

ULONG
GetBufferSizeNeededForTags(PUCHAR pTags, ULONG ulNumberOfTags)
{
    ULONG ulMemNeeded = 0;

    for (ULONG ulIndex = 0; ulIndex < ulNumberOfTags; ulIndex++) {
        USHORT  usTagDataLength = *(USHORT UNALIGNED *)pTags;
        ULONG   ulTagLengthWoHeaderWoAlign = sizeof(USHORT) + usTagDataLength;
        ULONG   ulTagLengthWoHeader = GET_ALIGNMENT_BOUNDARY(ulTagLengthWoHeaderWoAlign, sizeof(ULONG));
        ULONG   ulTagHeaderLength = StreamHeaderSizeFromDataSize(ulTagLengthWoHeader);

        ulMemNeeded += ulTagHeaderLength + ulTagLengthWoHeader;
        pTags += ulTagLengthWoHeaderWoAlign;
    }

    return ulMemNeeded;
}

#if DBG
BOOLEAN
IsEntryInList(PLIST_ENTRY pHead, PLIST_ENTRY pTarget)
{
    BOOLEAN         bFound = FALSE;
    PLIST_ENTRY     pEntry;

    pEntry = pHead->Flink;
    while (pHead != pEntry) {
        if (pEntry == pTarget) {
            bFound = TRUE;
            break;
        }

        pEntry = pEntry->Flink;
    }

    return bFound;
}
#endif

bool
TestByteOrder()
{
   ULONG word = 0x1;
   char *byte = (char *) &word;
   return(byte[0] ? LITTLE_ENDIAN : BIG_ENDIAN);
}

/*
USHORT
ReturnStringLenWInUSHORT(PWCHAR pSrc)
{
    size_t  ValueSize = 0;
    USHORT  usDataSize = 0;
    NTSTATUS    Status;

    Status = RtlStringCbLengthW(pSrc, NTSTRSAFE_UNICODE_STRING_MAX_CCH * sizeof(WCHAR), &ValueSize);
    if (NT_SUCCESS(Status)) {
        usDataSize = (USHORT)ValueSize + sizeof(WCHAR);
    }

    return usDataSize;
}
*/

USHORT
ReturnStringLenAInUSHORT(PCHAR pSrc)
{
    size_t  ValueSize = 0;
    USHORT  usDataSize = 0;
    NTSTATUS    Status;

    Status = RtlStringCbLengthA(pSrc, NTSTRSAFE_UNICODE_STRING_MAX_CCH, &ValueSize);
    if (NT_SUCCESS(Status)) {
        usDataSize = (USHORT)ValueSize + sizeof(CHAR);
    }

    return usDataSize;
}

VOID
LogMemoryAllocationFailure(size_t size, POOL_TYPE PoolType, SourceLocation SourceLocation)
{
    PCWCHAR PoolTypeW = PoolTypeToStringW(PoolType);

    InVolDbgPrint(DL_ERROR, DM_MEM_TRACE, 
        ("%s: Failed to allocate %#x size of type %s at source location %#x\n", __FUNCTION__,
            size, PoolTypeToString(PoolType), SourceLocation));

    InDskFltWriteEvent(INDSKFLT_WARN_STATUS_NO_MEMORY, PoolTypeW, size, SourceLocation);
}

VOID
LogFileOpenFailure(NTSTATUS Status, PUNICODE_STRING pustrFile, SourceLocation SourceLocation)
{
    WCHAR   *OpenFileName = NULL;
    ULONG   cbOpenFileNameLength;

    GetLastPathComponent(pustrFile, OpenFileName, cbOpenFileNameLength);

    InVolDbgPrint(DL_ERROR, DM_MEM_TRACE, 
        ("%s: Failed to open %wZ with status %#x at source location %#x\n", __FUNCTION__,
            pustrFile, Status, SourceLocation));

    InDskFltWriteEvent(INDSKFLT_WARN_FILE_OPEN_FAILED, OpenFileName, Status, SourceLocation);
}

VOID
LogDeleteOnCloseFailure(NTSTATUS Status, PUNICODE_STRING pustrFile, SourceLocation SourceLocation)
{
    WCHAR   *OpenFileName = NULL;
    ULONG   cbOpenFileNameLength;

    GetLastPathComponent(pustrFile, OpenFileName, cbOpenFileNameLength);

    InVolDbgPrint(DL_ERROR, DM_MEM_TRACE, 
        ("%s: Failed to set delete on close for %wZ with status %#x at source location %#x\n", __FUNCTION__,
            pustrFile, Status, SourceLocation));

    InDskFltWriteEvent(INDSKFLT_WARN_DELETE_ON_CLOSE_FAILED, OpenFileName, Status, SourceLocation);
}

VOID
GetLastPathComponent(PUNICODE_STRING String, PWCHAR &LastComp, ULONG &uLen)
{
    ASSERT(NULL != String);

    uLen = 0;
    if (0 == String->Length) {
        LastComp = String->Buffer;
        return;
    }

    // LastComp points to beyond the string.
    LastComp = (PWCHAR)((PCHAR)String->Buffer + String->Length);
    LastComp--;

    while (LastComp >= String->Buffer) {
        if ((L'\\' == *LastComp) || (L'/' == *LastComp)) {
            if (uLen) {
                LastComp++;
            } else {
                LastComp = NULL;
            }
            return;
        }

        uLen += sizeof(WCHAR);
        LastComp--;
    }

    return;
}

VOID
GetLastPathComponent(PWCHAR Src, PWCHAR &LastComp, ULONG &uLen)
{
    ASSERT(NULL != Src);
    uLen = 0;

    if (NULL == Src) {
        LastComp = NULL;
        return;
    }

    size_t  SrcSize = wcslen(Src) * sizeof(WCHAR);

    // LastComp points to beyond the string.
    LastComp = (PWCHAR)((PCHAR)Src + SrcSize);
    LastComp--;

    while (LastComp >= Src) {
        if ((L'\\' == *LastComp) || (L'/' == *LastComp)) {
            if (uLen) {
                LastComp++;
            } else {
                LastComp = NULL;
            }
            return;
        }

        uLen += sizeof(WCHAR);
        LastComp--;
    }

    return;
}

VOID
ConvertGUIDtoLowerCase(PWCHAR   pGUID)
{
    int i;

    for (i = 0; i < GUID_SIZE_IN_CHARS; i++) {
        if ((pGUID[i] >= 'A') && (pGUID[i] <= 'Z'))
            pGUID[i] += 'a' - 'A';
    }

    return;
}

#if DBG
const char *
WOStateString(etWriteOrderState eWOState)
{
    switch (eWOState) {
    case ecWriteOrderStateBitmap:
        return "ecWOStateBitmap";
    case ecWriteOrderStateMetadata:
        return "ecWOStateMetaData";
    case ecWriteOrderStateData:
        return "ecWOStateData";
    case ecWriteOrderStateUnInitialized:
        return "ecWOStateUnInitialized";
    }

    return "ecWOStateUnknown";
}

const char *
VBitmapStateString(etVBitmapState eVBitmapState)
{
    switch (eVBitmapState) {
    case ecVBitmapStateUnInitialized:
        return "ecVBitmapStateUnInitialized";
    case ecVBitmapStateOpened:
        return "ecVBitmapStateOpened";
    case ecVBitmapStateReadStarted:
        return "ecVBitmapStateReadStarted";
    case ecVBitmapStateReadPaused:
        return "ecVBitmapStateReadPaused";
    case ecVBitmapStateAddingChanges:
        return "ecVBitmapStateAddingChanges";
    case ecVBitmapStateReadCompleted:
        return "ecVBitmapStateReadCompleted";
    case ecVBitmapStateClosed:
        return "ecVBitmapStateClosed";
    case ecVBitmapStateReadError:
        return "ecVBitmapStateReadError";
    case ecVBitmapStateInternalError:
        return "ecVBitmapStateInternalError";
    }

    return "ecVBitmapStateUnknown";
}
#endif

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
GetVolumeHandle(
    _In_ PDEVICE_OBJECT fileSystemDevObj,
    _In_ bool shouldOpenDirectDevHandle,
    _Outptr_result_nullonfailure_ HANDLE *pVolumeHandle,
    _Outptr_opt_result_nullonfailure_ PDEVICE_OBJECT *pVolDevObjTos)
{
    NTSTATUS status;
    PDEVICE_OBJECT bottomMostFsDevObj;
    PDEVICE_OBJECT volDevObj;
    PDEVICE_OBJECT volDevObjTosLocal;
    UNICODE_STRING volumeDosName;
    UNICODE_STRING volumeDosPath;
    PFILE_OBJECT cmpVolFileObj;
    PDEVICE_OBJECT cmpDevObj;
    ACCESS_MASK accessMask;
    OBJECT_ATTRIBUTES oa;
    IO_STATUS_BLOCK iosb;

    // TODO-SanKumar-1804: Add PAGED_CODE() check in all the methods touched...
    PAGED_CODE();

    bottomMostFsDevObj = NULL;
    volDevObj = NULL;
    volDevObjTosLocal = NULL;
    RtlInitUnicodeString(&volumeDosName, NULL);
    RtlInitUnicodeString(&volumeDosPath, NULL);
    cmpVolFileObj = NULL;
    cmpDevObj = NULL;

    if (fileSystemDevObj == NULL || (pVolumeHandle == NULL && pVolDevObjTos == NULL)) {
        status = STATUS_INVALID_PARAMETER;

        InVolDbgPrint(DL_ERROR, DM_FILE_OPEN,
            (__FUNCTION__ ": Invalid parameter. fileSystemDevObj %#p. pVolumeHandle %#p. pVolDevObjTos %#p. Status Code %#x.\n",
            fileSystemDevObj, pVolumeHandle, pVolDevObjTos, status));

        NT_ASSERT(FALSE);
        goto Cleanup;
    }

    if (pVolumeHandle != NULL) {
        *pVolumeHandle = NULL;
    }

    if (pVolDevObjTos != NULL) {
        *pVolDevObjTos = NULL;
    }

    // The given device object may (or) may not be the bottom-most device in FS stack.
    bottomMostFsDevObj = IoGetDeviceAttachmentBaseRef(fileSystemDevObj);

    // Get the corresponding volume device object.
    status = IoGetDiskDeviceObject(bottomMostFsDevObj, &volDevObj);

    if (!NT_SUCCESS(status)) {
        InDskFltWriteEvent(INDSKFLT_ERROR_GETVOLHANDLE_VOLMGR_DEV_OBJ, bottomMostFsDevObj, status);

        InVolDbgPrint(DL_ERROR, DM_FILE_OPEN,
            (__FUNCTION__ ": Error in getting volDevObj from bottomMostFsDevObj %#p. Status Code %#x.\n",
            bottomMostFsDevObj, status));

        NT_ASSERT(FALSE);
        goto Cleanup;
    }

    // Get the top-most device object in the volume stack.
    volDevObjTosLocal = IoGetAttachedDeviceReference(volDevObj);

    if (pVolDevObjTos != NULL) {
        *pVolDevObjTos = volDevObjTosLocal;
        ObReferenceObject(*pVolDevObjTos);
    }

    if (pVolumeHandle == NULL) {
        status = STATUS_SUCCESS;
        goto Cleanup;
    }

    // Looking into the windows implementation of the call, it in turn calls
    // private MountMgr IOCTL - IOCTL_MOUNTMGR_QUERY_DOS_VOLUME_PATH.
    // This returns whichever is present in the following search order:
    // 1. Drive letter - C:
    // 2. Mount path - D:\MountFolder
    // 3. Unique volume name - Volume{GUID}
    status = IoVolumeDeviceToDosName(volDevObjTosLocal, &volumeDosName);

    if (status != STATUS_SUCCESS) {
        InDskFltWriteEvent(INDSKFLT_ERROR_GETVOLHANDLE_VOL_DOS_NAME, volDevObjTosLocal, status);

        InVolDbgPrint(DL_ERROR, DM_FILE_OPEN,
            (__FUNCTION__ ": Error in getting Dos name of volDevObjTosLocal %#p. Status Code %#x.\n",
            volDevObjTosLocal, status));

        NT_ASSERT(FALSE);
        goto Cleanup;
    }

    volumeDosPath.Length = 0;
    volumeDosPath.MaximumLength =
        sizeof(wchar_t) * DOS_DEVICES_PATH2_LENGTH +
        volumeDosName.Length +
        sizeof(UNICODE_NULL);

    volumeDosPath.Buffer = (wchar_t*)ExAllocatePoolWithTag(
        PagedPool,
        volumeDosPath.MaximumLength,
        TAG_GENERIC_PAGED);

    InVolDbgPrint(DL_INFO, DM_MEM_TRACE,
        (__FUNCTION__ ": Dos-device name creation for volume %wZ. Allocation %#p.\n",
        &volumeDosName, volumeDosPath.Buffer));

    if (volumeDosPath.Buffer == NULL) {
        status = STATUS_NO_MEMORY;
        goto Cleanup;
    }

    status = RtlAppendUnicodeToString(&volumeDosPath, DOS_DEVICES_PATH2);

    if (status != STATUS_SUCCESS) {
        InVolDbgPrint(DL_ERROR, DM_FILE_OPEN,
            (__FUNCTION__ ": Error in appending dos devices path prefix for volume %wZ. Status Code %#x.\n",
            &volumeDosName, status));

        goto Cleanup;
    }

    status = RtlAppendUnicodeStringToString(&volumeDosPath, &volumeDosName);

    if (status != STATUS_SUCCESS) {
        InVolDbgPrint(DL_ERROR, DM_FILE_OPEN,
            (__FUNCTION__ ": Error in appending dos device name of volume %wZ. Status Code %#x.\n",
            &volumeDosName, status));

        goto Cleanup;
    }

    InitializeObjectAttributes(
        &oa,
        &volumeDosPath,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL,
        NULL);

    // When given the volume dos path for CreateFile(), the file system if not
    // mounted, will be mounted and handle to its stack is returned. FO_VOLUME_OPEN
    // is set in the FILE_OBJECT of the handle.
    // But if the access mask is restricted to a certain set, we can force the
    // skipping of file system and the create requrest will be passed to the
    // volume stack. FO_DIRECT_DEVICE_OPEN is set in the FILE_OBJECT of the handle.
    // 
    // For more, refer "Relative Open Requests for Direct Device Open Handles" under
    // https://msdn.microsoft.com/en-us/library/ms809962.aspx#drvrreliab_topic14.
    accessMask = shouldOpenDirectDevHandle ? 0 : FILE_GENERIC_READ;

    status = ZwOpenFile(
        pVolumeHandle,
        accessMask,
        &oa,
        &iosb,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        FILE_SYNCHRONOUS_IO_NONALERT | FILE_NO_INTERMEDIATE_BUFFERING | FILE_NON_DIRECTORY_FILE);

    if (status != STATUS_SUCCESS) {
        InDskFltWriteEvent(INDSKFLT_ERROR_GETVOLHANDLE_OPEN_VOLUME, volumeDosPath.Buffer, status);

        InVolDbgPrint(DL_ERROR, DM_FILE_OPEN,
            (__FUNCTION__ ": Error in opening volume %wZ. Status Code %#x.\n",
            &volumeDosPath, status));

        NT_ASSERT(FALSE);
        goto Cleanup;
    }

    // A symbolic link returned could be removed and assigned to another volume,
    // before we use it to open volume handle. Once acquiring the handle, we
    // compare the handle's device object to assert that it indeed belongs to
    // the same volume as expected.

    status = ObReferenceObjectByHandle(
        *pVolumeHandle,
        accessMask,
        *IoFileObjectType,
        KernelMode,
        (PVOID*)&cmpVolFileObj,
        NULL);

    if (status != STATUS_SUCCESS) {
        InDskFltWriteEvent(INDSKFLT_ERROR_GETVOLHANDLE_OPEN_VOL_FILEOBJ, volumeDosPath.Buffer, status);

        InVolDbgPrint(DL_ERROR, DM_FILE_OPEN,
            (__FUNCTION__ ": Error in opening the cmpVolFileObj for volume %wZ with handle %#p. Status Code %#x.\n",
            volumeDosPath, *pVolumeHandle, status));

        NT_ASSERT(FALSE);
        goto Cleanup;
    }

    cmpDevObj = IoGetDeviceAttachmentBaseRef(IoGetRelatedDeviceObject(cmpVolFileObj));

    if (cmpDevObj != (shouldOpenDirectDevHandle ? volDevObj : bottomMostFsDevObj)) {
        status = STATUS_FAIL_CHECK;

        InDskFltWriteEvent(INDSKFLT_ERROR_GETVOLHANDLE_DEVOBJ_CHANGED,
            cmpDevObj,
            volumeDosPath.Buffer,
            (shouldOpenDirectDevHandle ? volDevObj : bottomMostFsDevObj),
            shouldOpenDirectDevHandle,
            cmpVolFileObj->Flags,
            status);

        InVolDbgPrint(DL_ERROR, DM_FILE_OPEN,
            (__FUNCTION__ ": The device object of volume %wZ is %#p but not as expected %#p. shouldOpenDirectDevHandle: %d. FO Flags: %lu. Status Code %#x.\n",
            volumeDosPath,
            cmpDevObj,
            (shouldOpenDirectDevHandle ? volDevObj : bottomMostFsDevObj),
            shouldOpenDirectDevHandle,
            cmpVolFileObj->Flags,
            status));

        NT_ASSERT(FALSE);
        goto Cleanup;
    }

    status = STATUS_SUCCESS;

Cleanup:
    SafeObDereferenceObject(cmpDevObj);
    SafeObDereferenceObject(cmpVolFileObj);
    SafeFreeUnicodeStringWithTag(volumeDosPath, TAG_GENERIC_PAGED);
    SafeFreeUnicodeString(volumeDosName);
    SafeObDereferenceObject(volDevObjTosLocal);
    SafeObDereferenceObject(volDevObj);
    SafeObDereferenceObject(bottomMostFsDevObj);

    if (status != STATUS_SUCCESS) {

        if (pVolDevObjTos != NULL) {
            SafeObDereferenceObject(*pVolDevObjTos);
        }

        if (pVolumeHandle != NULL) {
            SafeCloseHandle(*pVolumeHandle);
        }
    }

    return status;
}