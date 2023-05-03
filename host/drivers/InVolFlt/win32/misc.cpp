#include "Common.h"
#include "Global.h"
#include "MntMgr.h"
#include <misc.h>
#include "VContext.h"
#include "HelperFunctions.h"
#include "svdparse.h"
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
StreamRecordSizeFromDataSize(ULONG ulDataLength)
{
    return (ulDataLength + sizeof(STREAM_REC_HDR_4B)) < 0xFF ? (sizeof(STREAM_REC_HDR_4B) + ulDataLength) : (sizeof(STREAM_REC_HDR_8B) + ulDataLength); 
}

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
    KIRQL   OldIrql;
    if(!bIsLockAcquired)
        KeAcquireSpinLock(&DriverContext.TimeStampLock, &OldIrql);

    if (DriverContext.ulValidationLevel >= ADD_TO_EVENT_LOG) {
        if (GlobalSeqNum > DriverContext.ullTimeStampSequenceNumber) {
            ULONG One = 1;
            InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                INVOLFLT_ERR_SEQUENCE_MISMATCH, STATUS_SUCCESS,
                GlobalSeqNum, DriverContext.ullTimeStampSequenceNumber,
                One,
                One);
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
            ULONG Two = 2;
            InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                INVOLFLT_ERR_SEQUENCE_MISMATCH, STATUS_SUCCESS,
                GlobalSeqNum, DriverContext.ullTimeStampSequenceNumber,
                Two,
                Two);
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
            ULONG Three = 3;
            InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                INVOLFLT_ERR_SEQUENCE_MISMATCH, STATUS_SUCCESS,
                GlobalSeqNum, DriverContext.ullTimeStampSequenceNumber,
                Three,
                Three);
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
            ULONG Four = 4;
            InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                INVOLFLT_ERR_SEQUENCE_MISMATCH, STATUS_SUCCESS,
                GlobalSeqNum, DriverContext.ullTimeStampSequenceNumber,
                Four,
                Four);
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
    //this will give the deltas local to the volume

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
            ULONG Five = 5;
            InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                INVOLFLT_ERR_SEQUENCE_MISMATCH, STATUS_SUCCESS,
                GlobalSeqNum, DriverContext.ullTimeStampSequenceNumber,
                Five,
                Five);
        }   
    }
    ASSERT_CHECK(GlobalSeqNum <= DriverContext.ullTimeStampSequenceNumber, 0);

    DriverContext.ullTimeStampSequenceNumber = pLastTimeStamp->ullSequenceNumber;

    if (DriverContext.ulValidationLevel >= ADD_TO_EVENT_LOG) {
        if (GlobalSeqNum > DriverContext.ullTimeStampSequenceNumber) {
            ULONG Six = 6;
            InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                INVOLFLT_ERR_SEQUENCE_MISMATCH, STATUS_SUCCESS,
                GlobalSeqNum, DriverContext.ullTimeStampSequenceNumber,
                Six,
                Six);
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
            ULONG One = 1;
            InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                INVOLFLT_ERR_FIRST_LAST_SEQUENCE_MISMATCH, STATUS_SUCCESS,
                pFirstTimeStamp->ullSequenceNumber,
                pLastTimeStamp->ullSequenceNumber,
                One,
                One);
        }   
    }
    ASSERT_CHECK(pLastTimeStamp->ullSequenceNumber >= pFirstTimeStamp->ullSequenceNumber, 0);

    if (DriverContext.ulValidationLevel >= ADD_TO_EVENT_LOG) {
        if (pLastTimeStamp->TimeInHundNanoSecondsFromJan1601 < pFirstTimeStamp->TimeInHundNanoSecondsFromJan1601) {
            ULONG One = 1;
            InMageFltLogError(DriverContext.ControlDevice, __LINE__,
                INVOLFLT_ERR_FIRST_LAST_TIME_MISMATCH, STATUS_SUCCESS,
                pFirstTimeStamp->TimeInHundNanoSecondsFromJan1601,
                pLastTimeStamp->TimeInHundNanoSecondsFromJan1601,
                One,
                One);
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
    PVOLUME_CONTEXT VolumeContext = DirtyBlock->VolumeContext;
    NTSTATUS    Status = STATUS_SUCCESS;

    ASSERT(0 == DirtyBlock->FileName.Length);

    if (0 == VolumeContext->DataLogDirectory.Length) {
        // Log failure to save data to file due to failure to find DataLogDirectory
        InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("%s: data log directory is empty", __FUNCTION__));
        return STATUS_INVALID_PARAMETER;
    }

    if (0 == (VolumeContext->ulFlags & VCF_DATALOG_DIRECTORY_VALIDATED)) {
        Status = ValidatePathForFileName(&VolumeContext->DataLogDirectory);
        if (NT_SUCCESS(Status)) {
            VolumeContext->ulFlags |= VCF_DATALOG_DIRECTORY_VALIDATED;
        } else {
            InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("%s: Failed to validate data log directory", __FUNCTION__));
        }
    }

    Status = InMageCopyUnicodeString(&DirtyBlock->FileName, 
                                     &VolumeContext->DataLogDirectory,
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
DefaultGranularityFromVolumeSize(LONGLONG llVolumeSize)
{
    ULONG   ulBitmapGranularity = 0;

/*    if ((llVolumeSize / GIGABYTES) > DriverContext.Bitmap32KGranularitySize) {
        ulBitmapGranularity = THIRTY_TWO_K_SIZE;
    } else if ((llVolumeSize / GIGABYTES) > DriverContext.Bitmap16KGranularitySize) {
        ulBitmapGranularity = SIXTEEN_K_SIZE;
    } else if ((llVolumeSize / GIGABYTES) > DriverContext.Bitmap8KGranularitySize) {
        ulBitmapGranularity = EIGHT_K_SIZE;
    } else {
        ulBitmapGranularity = FOUR_K_SIZE;
    } */

    if ((llVolumeSize / GIGABYTES) > DriverContext.Bitmap512KGranularitySize) {
        ulBitmapGranularity = SIXTEEN_K_SIZE;
    } else {
        ulBitmapGranularity = FOUR_K_SIZE;
    }

    return ulBitmapGranularity;
}

NTSTATUS
GetVolumeSize(
    IN  PDEVICE_OBJECT  TargetDeviceObject,
    OUT LONGLONG        *pllVolumeSize,
    OUT NTSTATUS        *pInMageStatus
    )
{
    KEVENT              Event;
    ULONG               OutputSize;
    PIRP                Irp;
    IO_STATUS_BLOCK     IoStatus;
    NTSTATUS            Status;

    ASSERT(NULL != pllVolumeSize);

    if (NULL == pllVolumeSize) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("GetVolumeSize: pllVolumeSize is NULL!!\n"));
        return STATUS_INVALID_PARAMETER;
    }

#if (_WIN32_WINNT > 0x0500)
    {
        GET_LENGTH_INFORMATION  lengthInfo;

        KeInitializeEvent(&Event, NotificationEvent, FALSE);

        OutputSize = sizeof(GET_LENGTH_INFORMATION);
        Irp = IoBuildDeviceIoControlRequest(IOCTL_DISK_GET_LENGTH_INFO, TargetDeviceObject, NULL, 0,
                                &lengthInfo, OutputSize, FALSE, &Event, &IoStatus);
        if (!Irp) {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("GetVolumeSize: Failed in IoBuildDeviceIoControlRequest\n"));
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        Status = IoCallDriver(TargetDeviceObject, Irp);
        if (Status == STATUS_PENDING) {
            KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
            Status = IoStatus.Status;
        }

        if (NT_SUCCESS(Status)) {
            *pllVolumeSize = lengthInfo.Length.QuadPart;
            InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, ("GetVolumeSize: VolumeSize is %#I64x using IOCTL_DISK_GET_LENGTH_INFO\n", 
                        lengthInfo.Length.QuadPart));
        } else {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("GetVolumeSize: Failed to determine volume size using IOCTL_DISK_GET_LENGTH_INFO, Status = %#x\n", Status));
            if (pInMageStatus)
                *pInMageStatus = INVOLFLT_ERR_VOLUME_GET_LENGTH_INFO_FAILED;
            return Status;
        }
    }
#endif

#if ((_WIN32_WINNT <= 0x0500) || DBG)
    {
    PVOLUME_DISK_EXTENTS diskExtents; // dynamically sized allocation based on number of extents

// if we are compiling for WinXP or later in debug mode, add size validation
#if ((_WIN32_WINNT > 0x0500) && DBG)
    LONGLONG       debugVolumeSize = *pllVolumeSize;
#endif

    OutputSize = sizeof(VOLUME_DISK_EXTENTS); // start with allocation for 1 extent
    diskExtents = (PVOLUME_DISK_EXTENTS)new UCHAR[OutputSize];
    if (NULL == diskExtents) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("GetVolumeSize: Failed to allocate memory\n"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    KeInitializeEvent(&Event, NotificationEvent, FALSE);

    Irp = IoBuildDeviceIoControlRequest(
                    IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                    TargetDeviceObject, NULL, 0,
                    diskExtents, OutputSize, FALSE, &Event, &IoStatus);
    if (!Irp) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("GetVolumeSize: Failed in IoBuildDeviceIoControlRequest\n"));
        delete[] diskExtents;
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Status = IoCallDriver(TargetDeviceObject, Irp);
    if (Status == STATUS_PENDING) {
        KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
        Status = IoStatus.Status;
    }

    if (STATUS_BUFFER_OVERFLOW == Status) {
        // must be more than one extent, do IOCTL again with correct buffer size
        OutputSize = sizeof(VOLUME_DISK_EXTENTS) + ((diskExtents->NumberOfDiskExtents - 1) * sizeof(DISK_EXTENT));
        delete[] diskExtents;

        diskExtents = (PVOLUME_DISK_EXTENTS)new UCHAR[OutputSize];
        if (NULL == diskExtents) {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("GetVolumeSize: Failed to allocate memory\n"));
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        KeInitializeEvent(&Event, NotificationEvent, FALSE);

        Irp = IoBuildDeviceIoControlRequest(
                        IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                        TargetDeviceObject, NULL, 0,
                        diskExtents, OutputSize, FALSE, &Event, &IoStatus);
        if (!Irp) {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("GetVolumeSize: Failed in IoBuildDeviceIoControlRequest\n"));
            delete[] diskExtents;
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        Status = IoCallDriver(TargetDeviceObject, Irp);
        if (Status == STATUS_PENDING) {
            KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
            Status = IoStatus.Status;
        }
    }

    if ((!NT_SUCCESS(Status)) || (diskExtents->NumberOfDiskExtents != 1)) {
        // binary search the volume size if there is more than 1 extent
        AsyncIORequest      *readRequest;
        UINT64              currentGap;
        UINT64              currentOffset;
        PUCHAR              sectorBuffer;
        ULONG               extentIdx;

        if (NT_SUCCESS(Status)) {
            currentOffset = 0;
            // add together all the extents to calculate an upper bound on volume size
            // on RAID-5 and Mirror volumes the real size will be smaller that this sum
            for(extentIdx = 0;extentIdx < diskExtents->NumberOfDiskExtents;extentIdx++) {
                currentOffset += diskExtents->Extents[extentIdx].ExtentLength.QuadPart;
            }
            currentOffset += DISK_SECTOR_SIZE; // these two are needed to make the search algorithm work
            currentOffset /= DISK_SECTOR_SIZE;
        } else {
            currentOffset = MAXIMUM_SECTORS_ON_VOLUME; // if IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS failed, do a binary search with a large limit
        }
        // the search gap MUST be a power of two, so find an appropriate value
        currentGap = 1i64;
        while ((currentGap * 2i64) < currentOffset) {
            currentGap *= 2i64;
        }
        sectorBuffer = new UCHAR[DISK_SECTOR_SIZE];

        do {
            readRequest = new AsyncIORequest(TargetDeviceObject, NULL);
            readRequest->Read(sectorBuffer, DISK_SECTOR_SIZE, currentOffset * DISK_SECTOR_SIZE);
            Status = readRequest->Wait(NULL);
            readRequest->Release();

            if (NT_SUCCESS(Status)) {
                InVolDbgPrint(DL_VERBOSE, DM_BITMAP_OPEN,
                    ("GetVolumeSize: Volumes Size binary search successful probe at offset %#I64x  gap %#I64x \n",currentOffset,currentGap)); 
                currentOffset += currentGap;
                if (currentGap == 0) {
                    Status = STATUS_SUCCESS;
                    *pllVolumeSize = (currentOffset * DISK_SECTOR_SIZE) + DISK_SECTOR_SIZE;
                    break;
                }
            } else {
                if ((Status != STATUS_INVALID_PARAMETER) &&
                    (Status != STATUS_INVALID_DEVICE_REQUEST)) 
                {
                    InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("GetVolumeSize: Volumes Size binary search failed at offset %#I64X  gap %#I64X Status = %#X\n",
                                currentOffset, currentGap, Status)); 
                    if (NULL != pInMageStatus)
                        *pInMageStatus = INVOLFLT_ERR_VOLUME_SIZE_SEARCH_FAILED;
                    break;
                }

                InVolDbgPrint(DL_VERBOSE, DM_BITMAP_OPEN, ("GetVolumeSize: Volumes Size binary search failed probe at offset %#I64X  gap %#I64X Status = %#X\n",
                                currentOffset, currentGap, Status)); 
                currentOffset -= currentGap;
                if (currentGap == 0) {
                    Status = STATUS_SUCCESS;
                    *pllVolumeSize = currentOffset * DISK_SECTOR_SIZE;
                    break;
                }
            }

            currentGap /= 2;

        } while (TRUE);

        delete[] sectorBuffer;
    } else {
        // must be a single extent, use that size for the volume size
        *pllVolumeSize = diskExtents->Extents[0].ExtentLength.QuadPart;
    }

    delete[] diskExtents;

#if ((_WIN32_WINNT > 0x0500) && DBG)
    // verify the two ways to get the volume size match while debugging on W2K3
    ASSERT(debugVolumeSize == *pllVolumeSize);
#endif
    }

#endif

    return Status;
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

BOOLEAN
IsBitmapFileOnSameVolume(PVOLUME_CONTEXT pVolumeContext)
{
    UNICODE_STRING  usDosDevices;
    WCHAR           wcDriveLetter;
    USHORT          usMinLength;

    if ((MINIMUM_PATH_LENGTH_IN_DOSDEVICES_PATH * sizeof(WCHAR)) > pVolumeContext->BitmapFileName.Length)
        return FALSE;

    RtlInitUnicodeString(&usDosDevices, DOS_DEVICES_PATH);
    // If bitmap file is \SystemRoot we will return false.
    // SystemRoot can never be a cluster drive. So returning false if bitmap file
    // is on system driver is good.

    if (FALSE == RtlPrefixUnicodeString(&usDosDevices, &pVolumeContext->BitmapFileName, TRUE)) {
        // Volume name does not start with drive letter.
        return FALSE;
    }

    wcDriveLetter = pVolumeContext->BitmapFileName.Buffer[DRIVE_LETTER_INDEX_IN_DOSDEVICES_PATH];

    if (FALSE == IS_DRIVE_LETTER(wcDriveLetter))
        return FALSE;

    if (L':' != pVolumeContext->BitmapFileName.Buffer[DRIVE_LETTER_INDEX_IN_DOSDEVICES_PATH + 1])
        return FALSE;

    if (wcDriveLetter != pVolumeContext->wDriveLetter[0])
        return FALSE;

    return TRUE;
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

/*
Registry *
OpenVolumeParametersRegKey(PWCHAR pVolumeGUID)
{
    Registry        *pVolumeParam;
    UNICODE_STRING  uszVolumeParameters;
    NTSTATUS        Status;

    pVolumeParam = new Registry();
    if (NULL == pVolumeParam)
        return NULL;

    uszVolumeParameters.Length = 0;
    uszVolumeParameters.MaximumLength = DriverContext.DriverParameters.Length +
                                        VOLUME_NAME_SIZE_IN_BYTES + sizeof(WCHAR);
    uszVolumeParameters.Buffer = (PWSTR)ExAllocatePoolWithTag(PagedPool, uszVolumeParameters.MaximumLength, TAG_GENERIC_PAGED);
    if (NULL == uszVolumeParameters.Buffer) {
        delete pVolumeParam;
        return NULL;
    }

    RtlAppendUnicodeStringToString(&uszVolumeParameters, &DriverContext.DriverParameters);
    RtlAppendUnicodeToString(&uszVolumeParameters, L"\\Volume{");
    RtlAppendUnicodeToString(&uszVolumeParameters, pVolumeGUID);
    RtlAppendUnicodeToString(&uszVolumeParameters, L"}");

    Status = pVolumeParam->OpenKeyReadWrite(&uszVolumeParameters);
    if (!NT_SUCCESS(Status)) {
        delete pVolumeParam;
        pVolumeParam = NULL;
    }

    ExFreePoolWithTag(uszVolumeParameters.Buffer, TAG_GENERIC_PAGED);
    return pVolumeParam;
}

NTSTATUS
GetLogFilenameForVolume(PWCHAR VolumeGUID, PUNICODE_STRING puszLogFileName)
{
TRC
    NTSTATUS        Status;
    Registry        reg;
    UNICODE_STRING  uszOverrideFilename;
    Registry        *pVolumeParam = NULL;

    InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, ("GetLogFilenameForVolume: For GUID %S\n", VolumeGUID));

    if (NULL == puszLogFileName)
        return STATUS_INVALID_PARAMETER;

    if ((NULL == puszLogFileName->Buffer) || (0 == puszLogFileName->MaximumLength))
        return STATUS_INVALID_PARAMETER;

    puszLogFileName->Length = 0;

    // build a default log filename
    RtlAppendUnicodeToString(puszLogFileName, SYSTEM_ROOT_PATH);
    RtlAppendUnicodeToString(puszLogFileName, LOG_FILE_NAME_PREFIX); // so things sort by name nice
    RtlAppendUnicodeToString(puszLogFileName, VolumeGUID);
    RtlAppendUnicodeToString(puszLogFileName, LOG_FILE_NAME_SUFFIX);

    // now see if the registry has values to use
    uszOverrideFilename.Buffer = new WCHAR[MAX_LOG_PATHNAME];
    InVolDbgPrint(DL_INFO, DM_MEM_TRACE, ("BitmapAPI::FindLogFilename Allocation %p\n", uszOverrideFilename.Buffer));
    if (NULL == uszOverrideFilename.Buffer) {
        return STATUS_NO_MEMORY;
    }
    uszOverrideFilename.Length = 0;
    uszOverrideFilename.MaximumLength = MAX_LOG_PATHNAME*sizeof(WCHAR);

    Status = reg.OpenKeyReadOnly(&DriverContext.DriverParameters);
    if (NT_SUCCESS(Status)) {
        PWSTR  pDefaultLogDirectory = NULL;

        Status = reg.ReadWString(DEFALUT_LOG_DIRECTORY, STRING_LEN_NULL_TERMINATED, &pDefaultLogDirectory);
        if (NT_SUCCESS(Status)) {
            puszLogFileName->Length = 0;
            RtlAppendUnicodeToString(puszLogFileName, DOS_DEVICES_PATH);
            RtlAppendUnicodeToString(puszLogFileName, pDefaultLogDirectory);
            RtlAppendUnicodeToString(puszLogFileName, L"\\");
            RtlAppendUnicodeToString(puszLogFileName, LOG_FILE_NAME_PREFIX); // so things sort by name nice
            RtlAppendUnicodeToString(puszLogFileName, VolumeGUID);
            RtlAppendUnicodeToString(puszLogFileName, LOG_FILE_NAME_SUFFIX);
        }

        if (NULL != pDefaultLogDirectory)
            ExFreePoolWithTag(pDefaultLogDirectory, TAG_REGISTRY_DATA);
    }

    reg.CloseKey();

    // check for override per volume
    pVolumeParam = OpenVolumeParametersRegKey(VolumeGUID);
    if (NULL != pVolumeParam) {
        Status = pVolumeParam->ReadUnicodeString(VOLUME_LOG_PATH_NAME, &uszOverrideFilename);
        if (NT_SUCCESS(Status)) {
            if (uszOverrideFilename.Length > 0) {
                puszLogFileName->Length = 0;
                RtlAppendUnicodeToString(puszLogFileName, DOS_DEVICES_PATH);
                RtlAppendUnicodeStringToString(puszLogFileName, &uszOverrideFilename);
            }
        } else {
            // Default LogPathname not present. See if this is a volume on clusdisk.
            PBASIC_VOLUME   pBasicVolume = NULL;

            pBasicVolume = GetBasicVolumeFromDriverContext(VolumeGUID);
            if (NULL != pBasicVolume) {
                ASSERT(NULL != pBasicVolume->pDisk);

                if ((NULL != pBasicVolume->pDisk) && 
                    (pBasicVolume->pDisk->ulFlags & BD_FLAGS_IS_CLUSTER_RESOURCE) &&
                    (pBasicVolume->ulFlags & BV_FLAGS_HAS_DRIVE_LETTER))
                {
                    WCHAR               DriveLetter[0x03];

                    // Volume on clusdisk. So create LogPathName.
                    puszLogFileName->Length = 0;
                    uszOverrideFilename.Length = 0;
                    DriveLetter[0x00] = pBasicVolume->DriveLetter;
                    DriveLetter[0x01] = L':';
                    DriveLetter[0x02] = L'\0';
                    RtlAppendUnicodeToString(&uszOverrideFilename, DriveLetter);
                    RtlAppendUnicodeToString(&uszOverrideFilename, DEFAULT_LOG_DIR_FOR_CLUSDISKS);
                    RtlAppendUnicodeToString(&uszOverrideFilename, LOG_FILE_NAME_PREFIX); // so things sort by name nice
                    DriveLetter[0x01] = L'\0';
                    RtlAppendUnicodeToString(&uszOverrideFilename, DriveLetter);
                    RtlAppendUnicodeToString(&uszOverrideFilename, LOG_FILE_NAME_SUFFIX);

                    pVolumeParam->WriteUnicodeString(VOLUME_LOG_PATH_NAME, &uszOverrideFilename);
                    RtlAppendUnicodeToString(puszLogFileName, DOS_DEVICES_PATH);
                    RtlAppendUnicodeStringToString(puszLogFileName, &uszOverrideFilename);
                }

                DereferenceBasicVolume(pBasicVolume);
            }
        }
    }

    delete uszOverrideFilename.Buffer;

    if (NULL != pVolumeParam)
        delete pVolumeParam;

    ValidatePathForFileName(puszLogFileName);

    return STATUS_SUCCESS;
}
*/
bool
IsVolumeClusterResource(PWCHAR pGUID)
{
    PBASIC_VOLUME   pBasicVolume = NULL;
    bool         bClusterResource = false;

    pBasicVolume = GetBasicVolumeFromDriverContext(pGUID);
    if (NULL != pBasicVolume) {
        ASSERT(NULL != pBasicVolume->pDisk);

        if ((NULL != pBasicVolume->pDisk) && 
            (pBasicVolume->pDisk->ulFlags & BD_FLAGS_IS_CLUSTER_RESOURCE))
        {
            bClusterResource = true;
        }

        DereferenceBasicVolume(pBasicVolume);
    }

    return bClusterResource;
}

bool
IsDiskClusterResource(ULONG ulDiskSignature)
{
    PBASIC_DISK  pBasicDisk = NULL;
    bool         bClusterResource = false;

    pBasicDisk = GetBasicDiskFromDriverContext(ulDiskSignature);
    if (NULL != pBasicDisk) {
        if (pBasicDisk->ulFlags & BD_FLAGS_IS_CLUSTER_RESOURCE) {
            bClusterResource = true;
        }

        DereferenceBasicDisk(pBasicDisk);
    }

    return bClusterResource;
}

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

bool
TestByteOrder()
{
   ULONG word = 0x1;
   char *byte = (char *) &word;
   return(byte[0] ? LITTLE_ENDIAN : BIG_ENDIAN);
}

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
LogMemoryAllocationFailure(size_t size, POOL_TYPE PoolType, WCHAR *File, ULONG Line)
{
    WCHAR   *PoolTypeW = PoolTypeToStringW(PoolType);
    WCHAR   *FileName = NULL;
    ULONG   cbFileNameLength = 0;

    GetLastPathComponent(File, FileName, cbFileNameLength);
    InVolDbgPrint(DL_ERROR, DM_MEM_TRACE, 
        ("%s: Failed to allocate %#x size of type %s in file %S line %#x\n", __FUNCTION__,
            size, PoolTypeToString(PoolType), File, Line));

    InMageFltLogError(DriverContext.ControlDevice,__LINE__, INVOLFLT_WARNING_STATUS_NO_MEMORY, STATUS_SUCCESS,
        PoolTypeW, wcslen(PoolTypeW)*sizeof(WCHAR), FileName, cbFileNameLength, (ULONG) size, (ULONG) Line);

    return;
}

VOID
LogFileOpenFailure(NTSTATUS Status, PUNICODE_STRING pustrFile, WCHAR *File, ULONG Line)
{
    WCHAR   *FileName = NULL, *OpenFileName = NULL;
    ULONG   cbFileNameLength = 0, cbOpenFileNameLength;

    GetLastPathComponent(File, FileName, cbFileNameLength);
    GetLastPathComponent(pustrFile, OpenFileName, cbOpenFileNameLength);
    InVolDbgPrint(DL_ERROR, DM_MEM_TRACE, 
        ("%s: Failed to open %wZ with status %#x in file %S line %#x\n", __FUNCTION__,
            pustrFile, Status, File, Line));

    InMageFltLogError(DriverContext.ControlDevice,__LINE__, INVOLFLT_WARNING_FILE_OPEN_FAILED, STATUS_SUCCESS,
        OpenFileName, cbOpenFileNameLength, FileName, cbFileNameLength, Status, Line);

    return;
}

VOID
LogDeleteOnCloseFailure(NTSTATUS Status, PUNICODE_STRING pustrFile, WCHAR *File, ULONG Line)
{
    WCHAR   *FileName = NULL, *OpenFileName = NULL;
    ULONG   cbFileNameLength = 0, cbOpenFileNameLength;

    GetLastPathComponent(File, FileName, cbFileNameLength);
    GetLastPathComponent(pustrFile, OpenFileName, cbOpenFileNameLength);
    InVolDbgPrint(DL_ERROR, DM_MEM_TRACE, 
        ("%s: Failed to set delete on close for %wZ with status %#x in file %S line %#x\n", __FUNCTION__,
            pustrFile, Status, File, Line));

    InMageFltLogError(DriverContext.ControlDevice,__LINE__, INVOLFLT_WARNING_DELETE_ON_CLOSE_FAILED, STATUS_SUCCESS,
        OpenFileName, cbOpenFileNameLength, FileName, cbFileNameLength, Status, Line);

    return;
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

