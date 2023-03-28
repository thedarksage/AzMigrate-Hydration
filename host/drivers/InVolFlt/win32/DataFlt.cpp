#include "Common.h"
#include "DirtyBlock.h"
#include "DataFlt.h"
#include "misc.h"
#include "VContext.h"
#include "MetaDataMode.h"
#include "svdparse.h"
#include "MntMgr.h"
#include "WinSvdParse.h"

NTSTATUS
MapDataBlocksForUsermodeUse(PKDIRTY_BLOCK DirtyBlock, HANDLE VolumeId)
{
    NTSTATUS        Status = STATUS_SUCCESS;
    
    // We have a dirty block that has data.
    // Map all data blocks to user address space.

    PDATA_BLOCK     DataBlock = (PDATA_BLOCK)DirtyBlock->DataBlockList.Flink;

    while((PLIST_ENTRY)DataBlock != &DirtyBlock->DataBlockList) {        
        if (0 == (DataBlock->ulFlags & DATABLOCK_FLAGS_MAPPED)) {
#if DBG
            if (DataBlock->ulFlags & DATABLOCK_FLAGS_LOCKED) {
                KIRQL       Irql;
                KeAcquireSpinLock(&DriverContext.DataBlockCounterLock, &Irql);
                DriverContext.LockedErrorCounter++;
                InVolDbgPrint(DL_ERROR, DM_MEM_TRACE,("Error MapDataBlocksForUsermodeUse %p Count %d\n", DirtyBlock, DriverContext.LockedErrorCounter));
                KeReleaseSpinLock(&DriverContext.DataBlockCounterLock, Irql);
            }
#endif
            Status = LockDataBlock(DataBlock);
            if (!NT_SUCCESS(Status)) {
                break;
            }
            // Map data block to user address.
            __try {
                    DataBlock->MappedUserAddress = (PUCHAR)MmMapLockedPagesSpecifyCache(DataBlock->Mdl,
                                                                             UserMode,
                                                                             MmCached,
                                                                             NULL,
                                                                             FALSE,
                                                                             NormalPagePriority);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("CopyKDirtyBlockToUDirtyBlock: Failed to map pages"));
                DataBlock->MappedUserAddress = NULL;
            }
            if (NULL == DataBlock->MappedUserAddress) {
                Status = STATUS_UNSUCCESSFUL;
	            UnLockDataBlock(DataBlock);
                break;
            }
            DataBlock->ulFlags |= DATABLOCK_FLAGS_MAPPED;
            KIRQL       Irql;
            KeAcquireSpinLock(&DriverContext.DataBlockCounterLock, &Irql);
            DriverContext.MappedDataBlockCounter++;
//          InVolDbgPrint(DL_INFO, DM_MEM_TRACE,("++MappedDataBlockCounter %d\n", DriverContext.MappedDataBlockCounter));
            if (DriverContext.MaxMappedDataBlockCounter <  DriverContext.MappedDataBlockCounter) {
                DriverContext.MaxMappedDataBlockCounter = DriverContext.MappedDataBlockCounter;
            }
            KeReleaseSpinLock(&DriverContext.DataBlockCounterLock, Irql);
        }

        DataBlock->ProcessId = PsGetCurrentProcessId();
        DataBlock->VolumeId = VolumeId;
        DataBlock = (PDATA_BLOCK)DataBlock->ListEntry.Flink;
    }

    return Status;
}

void
WriteDataFileHeader(PUCHAR  Header)
{
    //prefix added to find the endianess of the system
    SV_UINT *FPrefix = (SV_UINT *)Header;
    if(DriverContext.bEndian == LITTLE_ENDIAN)
    {
        *FPrefix = SVD_TAG_LEFORMAT;
    }
    else
    {
        *FPrefix = SVD_TAG_BEFORMAT;
    }

    ULONG SvdPrefixOffset = sizeof(SV_UINT);
    
    // Prefix and header would be in contiguous address space.
    SVD_PREFIX *Prefix = (SVD_PREFIX *)(Header + SvdPrefixOffset);
    Prefix->tag = W_SVD_TAG_HEADER;
    Prefix->count = 1;
    Prefix->Flags = 0;
    
    ULONG ulHeaderOffset = sizeof(SV_UINT) + sizeof(SVD_PREFIX);

    W_SVD_HEADER *Header1 = (W_SVD_HEADER *)(Header + ulHeaderOffset);
    RtlZeroMemory(Header1, sizeof(W_SVD_HEADER));

    return;
}

ULONG
CopyDataToDaB(
    PDATA_BLOCK         DataBlock,
    PUCHAR              pSource,
    ULONG               ulLength
    )
{
    ULONG   ulcbToCopy;

    InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, ("CopyDataToDaB: Len = %#x Free = %#x\n",
                                               ulLength, DataBlock->ulcbDataFree));
    if (DataBlock->ulcbDataFree >= ulLength) {
        ulcbToCopy = ulLength;
    } else {
        ulcbToCopy = DataBlock->ulcbDataFree;
    }

    RtlMoveMemory(DataBlock->CurrentPosition, pSource, ulcbToCopy);

    DataBlock->ulcbDataFree -= ulcbToCopy;
    DataBlock->CurrentPosition += ulcbToCopy;

    InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, 
                  ("CopyDataToDaB: Returning Free = %#x DataCopied = %#x\n",
                   DataBlock->ulcbDataFree, ulcbToCopy));
    return ulcbToCopy;
}

ULONG
IncrementCurrentPositionInDaB(
    PDATA_BLOCK     DataBlock,
    ULONG           ulIncrement
    )
{
    ULONG   ulcbToIncrement;

    InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, ("IncrementCurrentPositionInDaB: Len = %#x Free = %#x\n",
                                               ulIncrement, DataBlock->ulcbDataFree));
    if (DataBlock->ulcbDataFree >= ulIncrement) {
        ulcbToIncrement = ulIncrement;
    } else {
        ulcbToIncrement = DataBlock->ulcbDataFree;
    }

    DataBlock->ulcbDataFree -= ulcbToIncrement;
    DataBlock->CurrentPosition += ulcbToIncrement;

    InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, 
                  ("CopyDataToDaB: Returning Free = %#x DataCopied = %#x\n",
                   DataBlock->ulcbDataFree, ulcbToIncrement));
    return ulcbToIncrement;
}

ULONG
IncrementCurrentPosition(
    PKDIRTY_BLOCK   DirtyBlock,
    ULONG           ulIncrement
    )
{
    NTSTATUS        Status = STATUS_SUCCESS;
    PDATA_BLOCK     DataBlock;
    ULONG           ulTotalIncremented = 0;

    InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, 
                  ("IncrementCurrentPosition: Len = %#x Free = %#x Used = %#x ulDaBAlloc = %#x\n",
                   ulIncrement, DirtyBlock->ulcbDataFree,
                   DirtyBlock->ulcbDataUsed, DirtyBlock->ulDataBlocksAllocated));

    while (DirtyBlock->ulcbDataFree && ulIncrement) {
        DataBlock = DirtyBlock->CurrentDataBlock;
        ULONG ulIncremented = IncrementCurrentPositionInDaB(DataBlock, ulIncrement);
        ulTotalIncremented += ulIncremented;
        ulIncrement -= ulIncremented;
        DirtyBlock->ulcbDataFree -= ulIncremented;
        DirtyBlock->ulcbDataUsed += ulIncremented;

        if (0 == DataBlock->ulcbDataFree) {
            UnLockDataBlock(DataBlock);
            if (DirtyBlock->ulcbDataFree) {
                DirtyBlock->CurrentDataBlock = (PDATA_BLOCK)DataBlock->ListEntry.Flink;
                InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, ("CopyDataToDB: Moving current data block to next\n"));
            }
        }
        InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, 
                      ("After IncrementCurrentPosition: Len = %#x Free = %#x Used = %#x ulDaBAlloc = %#x\n",
                       ulIncrement, DirtyBlock->ulcbDataFree,
                       DirtyBlock->ulcbDataUsed, DirtyBlock->ulDataBlocksAllocated));
    }

    InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, 
                  ("%s: Retuned bytes Incremented as %#x, Len %#x\n", __FUNCTION__, ulTotalIncremented, ulIncrement));

    return ulTotalIncremented;
}

ULONG
CopyDataToDaB(
    PDATA_BLOCK         DataBlock,
    PUCHAR              pDest,
    PUCHAR              pSource,
    ULONG               ulLength
    )
{
    ULONG   ulcbToCopy;

    InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, 
        ("CopyDataToDaB: Len = %#x KernelAddress = %#p, Dest = %#p, MaxData = %#x\n",
          ulLength, DataBlock->KernelAddress, pDest, DataBlock->ulcbMaxData));
    ASSERT((pDest >= DataBlock->KernelAddress) && (pDest < (DataBlock->KernelAddress + DataBlock->ulcbMaxData)));

    ULONG   ulBytesAfterAddress = (ULONG)(DataBlock->KernelAddress + DataBlock->ulcbMaxData - pDest);
    if (ulBytesAfterAddress >= ulLength) {
        ulcbToCopy = ulLength;
    } else {
        ulcbToCopy = ulBytesAfterAddress;
    }

    RtlMoveMemory(pDest, pSource, ulcbToCopy);

    InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, 
        ("CopyDataToDaB: BytesCopied = %#x\n", ulcbToCopy));

    return ulcbToCopy;
}

ULONG
CopyDataToDirtyBlock(
    PKDIRTY_BLOCK       DirtyBlock,
    PUCHAR              pSource,
    ULONG               ulLength
    )
{
    NTSTATUS        Status = STATUS_SUCCESS;
    PDATA_BLOCK     DataBlock;
    ULONG           ulTotalDataCopied = 0;

    InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, 
                  ("CopyDataToDB: Len = %#x Free = %#x Used = %#x ulDaBAlloc = %#x\n",
                   ulLength, DirtyBlock->ulcbDataFree,
                   DirtyBlock->ulcbDataUsed, DirtyBlock->ulDataBlocksAllocated));

    while (DirtyBlock->ulcbDataFree && ulLength) {
        DataBlock = DirtyBlock->CurrentDataBlock;
        ULONG ulDataCopied = CopyDataToDaB(DataBlock, pSource, ulLength);
        ulTotalDataCopied += ulDataCopied;
        pSource += ulDataCopied;
        ulLength -= ulDataCopied;
        DirtyBlock->ulcbDataFree -= ulDataCopied;
        DirtyBlock->ulcbDataUsed += ulDataCopied;

        if (0 == DataBlock->ulcbDataFree) {
            UnLockDataBlock(DataBlock);
            if (DirtyBlock->ulcbDataFree) {
                DirtyBlock->CurrentDataBlock = (PDATA_BLOCK)DataBlock->ListEntry.Flink;
                InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, ("CopyDataToDB: Moving current data block to next\n"));
            }
        }
        InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, 
                      ("After CopyDataToDaBlock: Len = %#x Free = %#x Used = %#x ulDaBAlloc = %#x\n",
                       ulLength, DirtyBlock->ulcbDataFree,
                       DirtyBlock->ulcbDataUsed, DirtyBlock->ulDataBlocksAllocated));
    }

    InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, 
                  ("%s: Retuned bytes copied as %#x, Len %#x\n", __FUNCTION__, ulTotalDataCopied, ulLength));

    return ulTotalDataCopied;
}

ULONG
CopyDataToDirtyBlock(
    PKDIRTY_BLOCK       DirtyBlock,
    PUCHAR              pDest,
    PUCHAR              pSource,
    ULONG               ulLength
    )
{
    NTSTATUS        Status = STATUS_SUCCESS;
    PDATA_BLOCK     DataBlock = NULL, DaB;
    PLIST_ENTRY     Temp;
    ULONG           ulTotalDataCopied = 0;

    InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, 
                  ("CopyDataToDB: Len = %#x Free = %#x Used = %#x ulDaBAlloc = %#x\n",
                   ulLength, DirtyBlock->ulcbDataFree,
                   DirtyBlock->ulcbDataUsed, DirtyBlock->ulDataBlocksAllocated));

    // Find the data block that has the address.
    DaB = (PDATA_BLOCK)DirtyBlock->DataBlockList.Flink;
    while ((PLIST_ENTRY)DaB != &DirtyBlock->DataBlockList) {
        if ((pDest >= DaB->KernelAddress) && (pDest < (DaB->KernelAddress + DaB->ulcbMaxData))) {
            DataBlock = DaB;
            break;
        }
        DaB = (PDATA_BLOCK)DaB->ListEntry.Flink;
    }

    while ((NULL != DataBlock) && ulLength) {
        ULONG ulDataCopied = CopyDataToDaB(DataBlock, pDest, pSource, ulLength);
        ulTotalDataCopied += ulDataCopied;
        pSource += ulDataCopied;
        ulLength -= ulDataCopied;
        // Get the new Dest
        DataBlock = (PDATA_BLOCK)DataBlock->ListEntry.Flink;
        if (((PLIST_ENTRY)DataBlock != &DirtyBlock->DataBlockList)) {
            pDest = DataBlock->KernelAddress;
        } else {
            break;
        }
    }

    ASSERT(0 == ulLength);
    InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, 
                  ("%s: Retuned bytes copied as %#x, Len %#x\n", __FUNCTION__, ulTotalDataCopied, ulLength));

    return ulTotalDataCopied;
}


void
FinalizeDataFileStreamInDirtyBlock(
    PKDIRTY_BLOCK DirtyBlock
    )
{
#define MAX_RECORD_SIZE     512
    UCHAR           RecordBuffer[MAX_RECORD_SIZE];
    ULONG           ulRecordLength;

    if (INVOLFLT_DATA_SOURCE_DATA != DirtyBlock->ulDataSource)
        return;

    if (DirtyBlock->ulFlags & KDIRTY_BLOCK_FLAG_FINALIZED_DATA_STREAM)
        return;

    // We have a dirty block that has data in SVD stream format.
    // We need to do last operations as adding timestamp, change sizes, etc

    ASSERT(NULL != DirtyBlock->SVDFirstChangeTimeStamp);
    ASSERT(NULL != DirtyBlock->SVDcbChangesTag);

    // Write first change timestamp
    SVD_PREFIX  *Prefix = (SVD_PREFIX *)DirtyBlock->SVDFirstChangeTimeStamp;
    Prefix->tag = W_SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE;
    Prefix->count = 1;
    Prefix->Flags = 0;

    W_SVD_TIME_STAMP UNALIGNED *TimeStamp = (W_SVD_TIME_STAMP UNALIGNED *)(((PUCHAR)Prefix) + sizeof(SVD_PREFIX));
    TimeStamp->SequenceNumber = DirtyBlock->TagTimeStampOfFirstChange.ullSequenceNumber;
    TimeStamp->Header.usStreamRecType = DirtyBlock->TagTimeStampOfFirstChange.Header.usStreamRecType;
    TimeStamp->Header.ucLength = DirtyBlock->TagTimeStampOfFirstChange.Header.ucLength;
    TimeStamp->Header.ucFlags = DirtyBlock->TagTimeStampOfFirstChange.Header.ucFlags;
    TimeStamp->TimeInHundNanoSecondsFromJan1601 = DirtyBlock->TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601;

    // Write SVD_TAG_LENGTH_OF_DRTD_CHANGES
    // TODO: If user defined tags are larger and pushes this tag on to a boundary between data blocks,
    // this would cause corruption on use of direct address. So cache the tag in contigous address and
    // copy using CopyToDirtyBlock routine.
    Prefix = (SVD_PREFIX *)RecordBuffer;
    Prefix->tag = SVD_TAG_LENGTH_OF_DRTD_CHANGES;
    Prefix->count = 1;
    Prefix->Flags = 0;
    ulRecordLength = sizeof(SVD_PREFIX);

    ULONGLONG UNALIGNED *pull = (ULONGLONG UNALIGNED *)&RecordBuffer[ulRecordLength];

    *pull = DirtyBlock->ullDataChangesSize;
    ulRecordLength += sizeof(ULONGLONG);
    
    ULONG ulDataCopied = CopyDataToDirtyBlock(DirtyBlock, DirtyBlock->SVDcbChangesTag, 
                                                RecordBuffer, ulRecordLength);
    ASSERT(ulDataCopied == ulRecordLength);

    // Write the last change time stamp
    Prefix = (SVD_PREFIX *)RecordBuffer;
    ASSERT(MAX_RECORD_SIZE >= (sizeof(SVD_PREFIX) + sizeof(W_SVD_TIME_STAMP)));
    Prefix->tag = W_SVD_TAG_TIME_STAMP_OF_LAST_CHANGE;
    Prefix->count = 1;
    Prefix->Flags = 0;
    ulRecordLength = sizeof(SVD_PREFIX);

    TimeStamp = (W_SVD_TIME_STAMP UNALIGNED *)&RecordBuffer[ulRecordLength];
    TimeStamp->SequenceNumber = DirtyBlock->TagTimeStampOfLastChange.ullSequenceNumber;
    TimeStamp->Header.usStreamRecType = DirtyBlock->TagTimeStampOfLastChange.Header.usStreamRecType;
    TimeStamp->Header.ucLength = DirtyBlock->TagTimeStampOfLastChange.Header.ucLength;
    TimeStamp->Header.ucFlags = DirtyBlock->TagTimeStampOfLastChange.Header.ucFlags;
    TimeStamp->TimeInHundNanoSecondsFromJan1601 = DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601;
    ulRecordLength += sizeof(W_SVD_TIME_STAMP);

    ASSERT(NULL != DirtyBlock->CurrentDataBlock);
    ulDataCopied = CopyDataToDirtyBlock(DirtyBlock, DirtyBlock->SVDLastChangeTimeStamp, RecordBuffer, ulRecordLength);
    ASSERT(ulDataCopied == ulRecordLength);

    DirtyBlock->ulFlags |= KDIRTY_BLOCK_FLAG_FINALIZED_DATA_STREAM;

    return;
}

NTSTATUS
PrepareDirtyBlockForUsermodeUse(PKDIRTY_BLOCK DirtyBlock, HANDLE VolumeID)
{
    NTSTATUS        Status = STATUS_SUCCESS;
    ULONG           ulIndex = 0;
    PVOID           *pBufferArray = NULL;

    if (INVOLFLT_DATA_SOURCE_DATA != DirtyBlock->ulDataSource)
        return Status;

    if (DirtyBlock->ulFlags & KDIRTY_BLOCK_FLAG_PREPARED_FOR_USERMODE)
        return Status;

    Status = MapDataBlocksForUsermodeUse(DirtyBlock, VolumeID);
    ASSERT(STATUS_SUCCESS == Status);
    if (STATUS_SUCCESS != Status) {
        return Status;
    }

    ASSERT(!IsListEmpty(&DirtyBlock->DataBlockList));
    if (!IsListEmpty(&DirtyBlock->DataBlockList)) {
        PDATA_BLOCK     DataBlock = (PDATA_BLOCK)DirtyBlock->DataBlockList.Flink;
    
        // Allocate memory for BufferArray
        ASSERT(NULL == DataBlock->pUserBufferArray);
        USHORT usMaxDataBlocksInDirtyBlock = (USHORT) ( DirtyBlock->ulMaxDataSizePerDirtyBlock / DriverContext.ulDataBlockSize);
        DataBlock->szcbBufferArraySize = sizeof(PVOID) * usMaxDataBlocksInDirtyBlock;
        __try {
            Status = ZwAllocateVirtualMemory( NtCurrentProcess(),
                                              (PVOID *)&pBufferArray,
                                              0, // High order zero bits
                                              &DataBlock->szcbBufferArraySize,
                                              MEM_COMMIT,
                                              PAGE_READWRITE);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Status = STATUS_UNSUCCESSFUL;
        }
    
        ASSERT(STATUS_SUCCESS == Status);
        ASSERT(NULL != pBufferArray);
        if ((STATUS_SUCCESS != Status) || (NULL == pBufferArray)) {
            return STATUS_UNSUCCESSFUL;
        }

        DataBlock->pUserBufferArray = pBufferArray;
        ulIndex = 0;
        while (&DirtyBlock->DataBlockList != &DataBlock->ListEntry) {
            ASSERT(ulIndex < DirtyBlock->ulDataBlocksAllocated);
            pBufferArray[ulIndex] = DataBlock->MappedUserAddress;
            DataBlock = (PDATA_BLOCK)DataBlock->ListEntry.Flink;
            ulIndex++;
        }

        ASSERT(ulIndex == DirtyBlock->ulDataBlocksAllocated);
        FinalizeDataFileStreamInDirtyBlock(DirtyBlock);
    }

    DirtyBlock->ulFlags |= KDIRTY_BLOCK_FLAG_PREPARED_FOR_USERMODE;

    return STATUS_SUCCESS;
}

NTSTATUS
PrepareDirtyBlockFor32bitUsermodeUse(PKDIRTY_BLOCK DirtyBlock, HANDLE VolumeID)
{
    NTSTATUS        Status = STATUS_SUCCESS;
    ULONG           ulIndex = 0;
    ULONG           *pBufferArray = NULL;

    if (INVOLFLT_DATA_SOURCE_DATA != DirtyBlock->ulDataSource)
        return Status;

    if (DirtyBlock->ulFlags & KDIRTY_BLOCK_FLAG_PREPARED_FOR_USERMODE)
        return Status;

    Status = MapDataBlocksForUsermodeUse(DirtyBlock, VolumeID);
    ASSERT(STATUS_SUCCESS == Status);
    if (STATUS_SUCCESS != Status) {
        return Status;
    }

    ASSERT(!IsListEmpty(&DirtyBlock->DataBlockList));
    if (!IsListEmpty(&DirtyBlock->DataBlockList)) {
        PDATA_BLOCK     DataBlock = (PDATA_BLOCK)DirtyBlock->DataBlockList.Flink;
    
        // Allocate memory for BufferArray
        ASSERT(NULL == DataBlock->pUserBufferArray);
        USHORT usMaxDataBlocksInDirtyBlock = (USHORT) (DirtyBlock->ulMaxDataSizePerDirtyBlock / DriverContext.ulDataBlockSize);
        DataBlock->szcbBufferArraySize = sizeof(ULONG) * usMaxDataBlocksInDirtyBlock;
        __try {
            Status = ZwAllocateVirtualMemory( NtCurrentProcess(),
                                              (PVOID *)&pBufferArray,
                                              0, // High order zero bits
                                              &DataBlock->szcbBufferArraySize,
                                              MEM_COMMIT,
                                              PAGE_READWRITE);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Status = STATUS_UNSUCCESSFUL;
        }
    
        ASSERT(STATUS_SUCCESS == Status);
        ASSERT(NULL != pBufferArray);
        if ((STATUS_SUCCESS != Status) || (NULL == pBufferArray)) {
            return STATUS_UNSUCCESSFUL;
        }
    
        DataBlock->pUserBufferArray = (PVOID *)pBufferArray;
        ulIndex = 0;
        while (&DirtyBlock->DataBlockList != &DataBlock->ListEntry) {
            ASSERT(ulIndex < DirtyBlock->ulDataBlocksAllocated);
            pBufferArray[ulIndex] = (ULONG)(ULONG_PTR)(DataBlock->MappedUserAddress);
            DataBlock = (PDATA_BLOCK)DataBlock->ListEntry.Flink;
            ulIndex++;
        }
    
        ASSERT(ulIndex == DirtyBlock->ulDataBlocksAllocated);
    
        FinalizeDataFileStreamInDirtyBlock(DirtyBlock);
    }

    DirtyBlock->ulFlags |= KDIRTY_BLOCK_FLAG_PREPARED_FOR_USERMODE;

    return STATUS_SUCCESS;
}

NTSTATUS
AddDataBlockToDirtyBlock(
    PKDIRTY_BLOCK   DirtyBlock,
    PVOLUME_CONTEXT VolumeContext,
    bool            bSplitIO,
    bool            bTags = false
    )
{
    PDATA_BLOCK     DataBlock;
    // This is set to number of bytes skipped in data block, that will be used for header and first time stamp
    ULONG           ulBytesLostInDB = 0;

    InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, ("AddDaBToDB: Called. DB Free = %#x Used = %#x NumDaBsAlloc = %#x\n",
                                               DirtyBlock->ulcbDataFree,
                                               DirtyBlock->ulcbDataUsed,
                                               DirtyBlock->ulDataBlocksAllocated));
    DataBlock = VCGetReservedDataBlock(VolumeContext);

    if (NULL == DataBlock) {
        InVolDbgPrint(DL_WARNING, DM_DATA_FILTERING, ("AddDaBToDB: VCGetReservedDataBlock failed\n"));
        return STATUS_NO_MEMORY;
    }

    InterlockedIncrement(&VolumeContext->lDataBlocksInUse);
    InsertTailList(&DirtyBlock->DataBlockList, &DataBlock->ListEntry);
    DirtyBlock->ulDataBlocksAllocated++;
    DirtyBlock->ulcbDataFree += DriverContext.ulDataBlockSize;

    if (NULL == DirtyBlock->CurrentDataBlock) {
        // Write data file header.
        WriteDataFileHeader(DataBlock->KernelAddress);
        DirtyBlock->SVDFirstChangeTimeStamp = 
                DataBlock->KernelAddress + DataFileOffsetToFirstChangeTimeStamp();
        if (bTags) {
            ulBytesLostInDB = SVFileFormatHeaderSize();
        } else {
            DirtyBlock->SVDcbChangesTag = DataBlock->KernelAddress + SVFileFormatHeaderSize();
            ulBytesLostInDB = SVFileFormatHeaderSize() + SVFileFormatChangesHeaderSize();
        }
        if (!bSplitIO && !bTags)
            VCAdjustCountersForLostDataBytes(VolumeContext, NULL, ulBytesLostInDB, true, false);

        DirtyBlock->ulcbDataUsed += ulBytesLostInDB;
        DirtyBlock->ulcbDataFree -= ulBytesLostInDB;
        DirtyBlock->CurrentDataBlock = DataBlock;
    } else if (0 == DirtyBlock->CurrentDataBlock->ulcbDataFree) {
        DirtyBlock->CurrentDataBlock = DataBlock;
    }

    DataBlock->CurrentPosition = DataBlock->KernelAddress + ulBytesLostInDB;
    DataBlock->ulcbDataFree = DriverContext.ulDataBlockSize - ulBytesLostInDB;

    InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, ("AddDaBToDB: Returning. DB Free = %#x Used = %#x NumDaBsAlloc = %#x\n",
                                               DirtyBlock->ulcbDataFree,
                                               DirtyBlock->ulcbDataUsed,
                                               DirtyBlock->ulDataBlocksAllocated));
    return STATUS_SUCCESS;
}

// This function is called with volume context lock held.
PKDIRTY_BLOCK
GetDirtyBlockToCopyChange(
    PVOLUME_CONTEXT     VolumeContext,
    ULONG               ulLength,
    bool                bTags,
    PTIME_STAMP_TAG_V2  *ppTimeStamp,
    PLARGE_INTEGER      pliTickCount,
    PWRITE_META_DATA    pWriteMetaData
    )
{
    PKDIRTY_BLOCK       DirtyBlock = NULL;
    NTSTATUS            Status;
    ULONG               ulRecordLength = bTags ? ulLength : SVRecordSizeForIOSize(ulLength, VolumeContext->ulMaxDataSizePerDataModeDirtyBlock);
    ULONG               ulMaxTrailerOffsetInDirtyBlock = VolumeContext->ulMaxDataSizePerDataModeDirtyBlock - SVFileFormatTrailerSize();
    ULONG               TimeDelta = 0;
    ULONG               SequenceNumberDelta = 0;
    ULONG ulMaxChangeSizeInDirtyBlock = GetMaxChangeSizeFromBufferSize(VolumeContext->ulMaxDataSizePerDataModeDirtyBlock);
    DirtyBlock = GetCurrentDirtyBlockFromChangeList(VolumeContext);
    if (NULL != DirtyBlock) {
        if (0 != DirtyBlock->TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601) {
            // This dirty block is closed. So do not use this.
            InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, ("GetDBToCopyData: LastChangeTimeStamp is set\n"));
            DirtyBlock = NULL;
        } else if (INVOLFLT_DATA_SOURCE_DATA != DirtyBlock->ulDataSource) {
            // The last dirty block is not data dirty block. So have to add a new dirty block
            InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, ("GetDBToCopyData: Last DB source is (%d) not data\n",
                                                       DirtyBlock->ulDataSource));
            DirtyBlock = NULL;
        } else if (DirtyBlock->eWOState != VolumeContext->eWOState) {
            // Write order state has changed.
            InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, ("GetDBToCopyData: DB WOS(%d) does not match VC_WOS(%d)\n",
                                                       DirtyBlock->eWOState, VolumeContext->eWOState));
            DirtyBlock = NULL;
        } else if (DirtyBlock->ulFlags & KDIRTY_BLOCK_FLAG_SPLIT_CHANGE_MASK) {
            // This is part of split change. Split change is not merged with other changes.
            DirtyBlock = NULL;
            InVolDbgPrint(DL_TRACE_L2, DM_DATA_FILTERING, ("GetDBToCopyData: Last DB is part of split change\n"));
        } else if (0 != DirtyBlock->ulcbDataUsed) {
            // This is a partially filled data block
            ASSERT(false == bTags);
            InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, ("GetDBToCopyData: Last DB is partialy filled\n"));
            if (((ulRecordLength + DirtyBlock->ulcbDataUsed) <= ulMaxTrailerOffsetInDirtyBlock) &&
                (DirtyBlock->ulMaxDataSizePerDirtyBlock == VolumeContext->ulMaxDataSizePerDataModeDirtyBlock)
                && (GenerateTimeStampDelta(DirtyBlock, TimeDelta, SequenceNumberDelta))
                )
            {
                if (pWriteMetaData) {
                    pWriteMetaData->TimeDelta = TimeDelta;
                    pWriteMetaData->SequenceNumberDelta = SequenceNumberDelta;
                }
                // Make sure dirty block has space to save meta-data
                if (!ReserveSpaceForChangeInDirtyBlock(DirtyBlock)) {
                    return NULL;
                }

                // Write fits in existing dirty block.
                InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, ("GetDBToCopyData: Change fits in existing DB\n"));
                // If free memory in volume context isn't enough for trailer, allocate data block.
                if ((0 == VolumeContext->ullcbDataAlocMissed) && (VolumeContext->ulcbDataFree < SVFileFormatTrailerSize())) {
                    // If we can allocate more data blocks, allocate one more.
                    if (VolumeContext->ulcbDataAllocated < DriverContext.ulMaxDataSizePerVolume) {
                        PDATA_BLOCK DataBlock = AllocateLockedDataBlock(FALSE);
                        if (NULL != DataBlock) {
                            VCAddToReservedDataBlockList(VolumeContext, DataBlock);
                            VolumeContext->ulcbDataFree += DriverContext.ulDataBlockSize;
                        }
                    }
                }

                if ((ulRecordLength + SVFileFormatTrailerSize()) <= ((VolumeContext->ulDataBlocksReserved * DriverContext.ulDataBlockSize)+ DirtyBlock->ulcbDataFree)) {
                    while (DirtyBlock->ulcbDataFree < (ulRecordLength + SVFileFormatTrailerSize())) {
                        Status = AddDataBlockToDirtyBlock(DirtyBlock, VolumeContext, false);
                        if (!NT_SUCCESS(Status)) {
                            InVolDbgPrint(DL_WARNING, DM_DATA_FILTERING, ("GetDBToCopyData: AddDataBlockToDirtyBlock Failed\n"));
                            // We can not copy the data into this dirty block.
                            // TODO: Dont we have to free the data blocks???
                            ASSERT((VolumeContext->ulcbDataReserved + VolumeContext->ulcbDataFree) == DirtyBlock->ulcbDataFree);
                            ASSERT(VolumeContext->ullNetWritesSeenInBytes == VolumeContext->ullUnaccountedBytes + VolumeContext->ulcbDataReserved + VolumeContext->ullcbDataAlocMissed);
                            // Last change time stamp is added when new dirty block is added.
                            return NULL;
                        }
                    }
                    return DirtyBlock;
                }
            } 
            // Write does not fit existing dirty block. So have to start with a new one.
            InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, ("GetDBToCopyData: Change does not fit in existing DB\n"));
            // Adjust volumecontext counters to take care of skipped buffers in data block.
            // Adjusting if not done here would be taken care when next dirty block is added to list.
            // But this causes in border conditions to assume we have enough reserved for the IO.
            bool bAllocate = (VolumeContext->ulcbDataAllocated < DriverContext.ulMaxDataSizePerVolume) ? true : false;
            AddLastChangeTimestampToCurrentNode(VolumeContext, NULL, bAllocate);
            //Block is closed as new data does not fit here
            DirtyBlock = NULL;            
        }
    }

    ASSERT(NULL == DirtyBlock);

    // if ulRecordLength data does not fit in reserved data section, return NULL
    if (ulRecordLength > VolumeContext->ulcbDataReserved) {
        InVolDbgPrint(DL_WARNING, DM_DATA_FILTERING, ("GetDBToCopyData: Reserved(%#x) is not enough for DataLen(%#x), ulRecordLen(%#x)\n",
                                                      VolumeContext->ulcbDataReserved, ulLength, ulRecordLength));
        return NULL;
    }

    // if Non Page pool consumption exceeds 80% of Max. Non Page Pool Limit, return NULL. This is to avoid
    // pair resync which occurs if we reach max. non-page limit, in such cases its better to go in bitmap mode
    // rather than doing pair resync. If user wants to stay in data mode then he needs to increase NP pool
    // limit depending on his system configuration.
    if ((0 != DriverContext.ulMaxNonPagedPool) && (DriverContext.lNonPagedMemoryAllocated >= 0)) {
        ULONGLONG NonPagePoolLimit = DriverContext.ulMaxNonPagedPool * 8;
        NonPagePoolLimit /= 10;
        if ((ULONG) DriverContext.lNonPagedMemoryAllocated >= (ULONG) NonPagePoolLimit) {
            InVolDbgPrint(DL_WARNING, DM_DATA_FILTERING, ("GetDBToCopyData: Non Page Pool consumption is high, Limit (%#x), Used (%#x)\n",
                                                          DriverContext.ulMaxNonPagedPool, DriverContext.lNonPagedMemoryAllocated));
            return NULL;
        }    
    }

    //If this code is processing means that the new dirty block will be allocated
    //Making the deltas as explicitly zero
    if (pWriteMetaData) {
        pWriteMetaData->TimeDelta = 0;
        pWriteMetaData->SequenceNumberDelta = 0;
    }

    bool bSplitIO   = false;
    if (ulLength > ulMaxChangeSizeInDirtyBlock)
        bSplitIO = true;

    LIST_ENTRY  ChangeNodeList;
    InitializeListHead(&ChangeNodeList);
    Status = STATUS_SUCCESS;

    while (ulLength) {
        // Check if Change fits in one dirty block, if not we have to split this change
        PKDIRTY_BLOCK   Temp = AllocateDirtyBlock(VolumeContext, VolumeContext->ulMaxDataSizePerDataModeDirtyBlock);
        if (NULL == Temp) {
            Status = STATUS_NO_MEMORY;
            break;
        }

        PCHANGE_NODE    ChangeNode = Temp->ChangeNode;
        InsertTailList(&ChangeNodeList, &ChangeNode->ListEntry);

        if (!ReserveSpaceForChangeInDirtyBlock(Temp)) {
            Status = STATUS_NO_MEMORY;
            break;
        }

        if (!IsListEmpty(&ChangeNodeList)) {
            InVolDbgPrint(DL_TRACE_L2, DM_DATA_FILTERING, 
                ("%s: Allocated new DirtyBlock for split IO\n", __FUNCTION__));
        } else {
            InVolDbgPrint(DL_TRACE_L2, DM_DATA_FILTERING, 
                ("%s: Allocated new DirtyBlock\n", __FUNCTION__));
        }

        if (ulLength <= ulMaxChangeSizeInDirtyBlock) {
            // Change fits in one dirty block.
            ulRecordLength = SVRecordSizeForIOSize(ulLength, Temp->ulMaxDataSizePerDirtyBlock) + SVFileFormatTrailerSize();
            ulLength = 0;
        } else {
            ulRecordLength = SVRecordSizeForIOSize(ulMaxChangeSizeInDirtyBlock, Temp->ulMaxDataSizePerDirtyBlock);
            ulLength -= ulMaxChangeSizeInDirtyBlock;
        }

        ULONG ulNumDaBPerDirtyBlock = Temp->ulMaxDataSizePerDirtyBlock / DriverContext.ulDataBlockSize;
        while (Temp->ulcbDataFree < ulRecordLength) {
            ASSERT(Temp->ulDataBlocksAllocated < ulNumDaBPerDirtyBlock);
            Status = AddDataBlockToDirtyBlock(Temp, VolumeContext, bSplitIO, bTags);
            ASSERT(STATUS_SUCCESS == Status);
            if (STATUS_SUCCESS != Status) {
                break;
            }
        }
    }

    bool bContinuation = false;

    while(!IsListEmpty(&ChangeNodeList)) {
        PCHANGE_NODE    ChangeNode = (PCHANGE_NODE)RemoveHeadList(&ChangeNodeList);
        PKDIRTY_BLOCK   Temp = ChangeNode->DirtyBlock;

        if (STATUS_SUCCESS == Status) {
            if (NULL != ppTimeStamp)
                AddDirtyBlockToVolumeContext(VolumeContext, Temp, ppTimeStamp, pliTickCount, NULL);
            else 
                AddDirtyBlockToVolumeContext(VolumeContext, Temp, INVOLFLT_DATA_SOURCE_DATA, bContinuation);

            if (NULL == DirtyBlock) {
                DirtyBlock = Temp;
            }

            // Set bContinuation for next iteration
            bContinuation = true;
        } else {
            DeallocateDirtyBlock(Temp, true);
        }
    }

    return DirtyBlock;
}

ULONG
CopyChangeToDB(
    PKDIRTY_BLOCK       DirtyBlock,
    PUCHAR              pSource,
    PWRITE_META_DATA    pWriteMetaData
    )
{
    NTSTATUS        Status = STATUS_SUCCESS;
    // We need this buffer to write SVD_PREFIX and SVD_DIRTY_BLOCK
    // SVD_PREFIX size is 12 and SVD_DIRTY_BLOCK size is 16
    const unsigned long ulBufferSize = 50;
    UCHAR           Buffer[ulBufferSize];
    SVD_PREFIX      *Prefix = (SVD_PREFIX *)Buffer;
    W_SVD_DIRTY_BLOCK *ChangeData = (W_SVD_DIRTY_BLOCK *)&Buffer[sizeof(SVD_PREFIX)];
    ULONG           ulRecordLength = sizeof(SVD_PREFIX) + sizeof(W_SVD_DIRTY_BLOCK);
    ULONG           ulDataCopied, ulTotalDataCopied = 0;
    BOOLEAN         Coalesced = FALSE;

    ASSERT(ulBufferSize >= sizeof(SVD_PREFIX) + sizeof(W_SVD_DIRTY_BLOCK));
    Prefix->tag = W_SVD_TAG_DIRTY_BLOCK_DATA;
    Prefix->count = 1;
    Prefix->Flags = 0;
    
    ChangeData->ByteOffset = pWriteMetaData->llOffset;
    ChangeData->Length = pWriteMetaData->ulLength;
    
    ChangeData->uiSequenceNumberDelta= pWriteMetaData->SequenceNumberDelta;
    ChangeData->uiTimeDelta = pWriteMetaData->TimeDelta;
    ulDataCopied = CopyDataToDirtyBlock(DirtyBlock, (PUCHAR)Buffer, ulRecordLength);
    ASSERT(ulDataCopied == ulRecordLength);
    ulTotalDataCopied += ulDataCopied;

    InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, 
                  ("%s: Off = %#I64x Len = %#x Free = %#x Used = %#x ulDaBAlloc = %#x\n",
                   __FUNCTION__, pWriteMetaData->llOffset, pWriteMetaData->ulLength,
                   DirtyBlock->ulcbDataFree,
                   DirtyBlock->ulcbDataUsed, DirtyBlock->ulDataBlocksAllocated));
    Status = AddChangeToDirtyBlock(DirtyBlock, 
                                   pWriteMetaData->llOffset,
                                   pWriteMetaData->ulLength,
                                   pWriteMetaData->TimeDelta,
                                   pWriteMetaData->SequenceNumberDelta,
                                   INVOLFLT_DATA_SOURCE_DATA,
                                   Coalesced);
    ASSERT(STATUS_SUCCESS == Status);

    ulDataCopied = CopyDataToDirtyBlock(DirtyBlock, pSource, pWriteMetaData->ulLength);
    ASSERT(ulDataCopied == pWriteMetaData->ulLength);
    ASSERT(DirtyBlock->ulcbDataFree >= SVFileFormatTrailerSize());
    ulTotalDataCopied += ulDataCopied;

    DirtyBlock->ulicbChanges.QuadPart += pWriteMetaData->ulLength;
    DirtyBlock->ullDataChangesSize += ulTotalDataCopied;

    InVolDbgPrint(DL_TRACE_L1, DM_DATA_FILTERING, 
                  ("%s: Off = %I64x Len = %#x\n", __FUNCTION__, pWriteMetaData->llOffset, pWriteMetaData->ulLength));
    InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, 
                  ("%s: Retuned bytes copied as %#x\n", __FUNCTION__, ulTotalDataCopied));
    return ulTotalDataCopied;
}

// This function is called with volume context lock held.
NTSTATUS
CopyDataToVolumeContext(
    PVOLUME_CONTEXT     VolumeContext,
    PIRP                Irp,
    PWRITE_META_DATA    pWriteMetaData
    )
{
    NTSTATUS            Status = STATUS_SUCCESS;
    PUCHAR              pSource;
    PKDIRTY_BLOCK       DirtyBlock;
    PDATA_BLOCK         DataBlock;
    bool                bContinuation = false;
    ULONG               ulSequenceID = 0x02;

    ASSERT((NULL != Irp) && (NULL != Irp->MdlAddress) && (NULL != VolumeContext) && (NULL != pWriteMetaData));
    ASSERT(ecCaptureModeData == VolumeContext->eCaptureMode);

    InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, ("CopyDataToVC: %s(%s) Irp = %#p, Off = %#I64x Len = %#x\n",
                                               VolumeContext->DriveLetter, VolumeContext->GUIDinANSI, Irp,
                                               pWriteMetaData->llOffset, pWriteMetaData->ulLength));
    // Let us see if we can copy the write to current dirty block.
    DirtyBlock = GetDirtyBlockToCopyChange(VolumeContext, pWriteMetaData->ulLength, false, NULL, NULL, pWriteMetaData);
    if (NULL == DirtyBlock) {
        InVolDbgPrint(DL_WARNING, DM_DATA_FILTERING, ("CopyDataToVC: GetDirtyBlockToCopyChange failed\n"));
        Status = AddMetaDataToVolumeContext(VolumeContext, pWriteMetaData, TRUE);
        return Status;
    }

    // DirtyBlock points to a chain of dirty blocks to copy data to.
    pSource = (PUCHAR)MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
    ASSERT(NULL != pSource);
    while (pWriteMetaData->ulLength) {
        ULONG   ulCopiedData;
        ULONG   ulBytesUsed = 0;
        WRITE_META_DATA WriteMetaData;

        if (bContinuation) {
            DirtyBlock = GetNextDirtyBlock(DirtyBlock, VolumeContext);
        }
        
        ULONG ulMaxChangeSizeInDirtyBlock = GetMaxChangeSizeFromBufferSize(DirtyBlock->ulMaxDataSizePerDirtyBlock);
        if (pWriteMetaData->ulLength > ulMaxChangeSizeInDirtyBlock) {
            WriteMetaData.ulLength = ulMaxChangeSizeInDirtyBlock;
            WriteMetaData.llOffset = pWriteMetaData->llOffset;
            WriteMetaData.SequenceNumberDelta = pWriteMetaData->SequenceNumberDelta;
            WriteMetaData.TimeDelta = pWriteMetaData->TimeDelta;
            pWriteMetaData->llOffset += ulMaxChangeSizeInDirtyBlock;
            pWriteMetaData->ulLength -= ulMaxChangeSizeInDirtyBlock;
            ulBytesUsed = CopyChangeToDB(DirtyBlock, pSource, &WriteMetaData);
            ulCopiedData = ulMaxChangeSizeInDirtyBlock;
            pSource += ulMaxChangeSizeInDirtyBlock;
        } else {
            ulBytesUsed = CopyChangeToDB(DirtyBlock, pSource, pWriteMetaData);
            ulCopiedData = pWriteMetaData->ulLength;
            pWriteMetaData->llOffset += pWriteMetaData->ulLength;
            pWriteMetaData->ulLength = 0;
        }

        if (bContinuation) {
            DirtyBlock->ulSequenceIdForSplitIO = ulSequenceID++;
            if (pWriteMetaData->ulLength) {
                DirtyBlock->ulFlags |= KDIRTY_BLOCK_FLAG_PART_OF_SPLIT_CHANGE;
                InVolDbgPrint(DL_TRACE_L2, DM_DATA_FILTERING, ("CopyDataToDB: Setting PART of split change flag\n"));
            } else {
                InVolDbgPrint(DL_TRACE_L2, DM_DATA_FILTERING, ("CopyDataToDB: Setting END of split change flag\n"));
                DirtyBlock->ulFlags |= KDIRTY_BLOCK_FLAG_END_OF_SPLIT_CHANGE;                
                InVolDbgPrint(DL_TRACE_L1, DM_DATA_FILTERING, 
                            ("After CopyDataToDaBlock adjustment: Free = %#x Used = %#x ulDaBAlloc = %#x\n",
                            DirtyBlock->ulcbDataFree, DirtyBlock->ulcbDataUsed,
                            DirtyBlock->ulDataBlocksAllocated));
            }
            // This is part of split change, so unused data buffer in data block is accounted for as used.
            SetSVDLastChangeTimeStamp(DirtyBlock);
            // This Datablock can be middle or last. and as db is getting closed its good idea to remove locked buffers
            ulBytesUsed = DirtyBlock->ulDataBlocksAllocated * DriverContext.ulDataBlockSize;
            DirtyBlock->ulcbDataUsed += DirtyBlock->ulcbDataFree;
            DirtyBlock->ulcbDataFree = 0;
        } else {
            if (pWriteMetaData->ulLength) {
                DirtyBlock->ulFlags |= KDIRTY_BLOCK_FLAG_START_OF_SPLIT_CHANGE;
                InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, ("CopyDataToDB: Setting START of split change flag\n"));

                // This is part of split change, so unused data buffer in data block is accounted for as used.
                SetSVDLastChangeTimeStamp(DirtyBlock);
	            // This is first Datablock and as it is getting closed its good idea to remove locked buffers
                ulBytesUsed = DirtyBlock->ulDataBlocksAllocated * DriverContext.ulDataBlockSize;
                DirtyBlock->ulcbDataUsed += DirtyBlock->ulcbDataFree;
                DirtyBlock->ulcbDataFree = 0;
            }
        }

        InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, 
                      ("After CopyDataToDB Off = %#I64x Len = %#x Free = %#x Used = %#x ulDaBAlloc = %#x\n",
                       pWriteMetaData->llOffset, pWriteMetaData->ulLength, DirtyBlock->ulcbDataFree,
                       DirtyBlock->ulcbDataUsed, DirtyBlock->ulDataBlocksAllocated));
        ASSERT(VolumeContext->ulcbDataReserved >= ulBytesUsed);
        if (VolumeContext->ulcbDataReserved > ulBytesUsed) {
            VolumeContext->ulcbDataReserved -= ulBytesUsed;
        } else {
            VolumeContext->ulcbDataReserved = 0;
        }

        if (VolumeContext->ullNetWritesSeenInBytes > ulBytesUsed) {
            VolumeContext->ullNetWritesSeenInBytes -= ulBytesUsed;
        } else {
            VolumeContext->ullNetWritesSeenInBytes = 0;
        }        
        ASSERT(VolumeContext->ullNetWritesSeenInBytes == VolumeContext->ullUnaccountedBytes + VolumeContext->ulcbDataReserved + VolumeContext->ullcbDataAlocMissed);
        AddChangeCountersOnDataSource(&VolumeContext->ChangeList, INVOLFLT_DATA_SOURCE_DATA, 0x01, ulCopiedData);

        bContinuation = true;

        if ((NULL != VolumeContext->DBNotifyEvent) && (VolumeContext->bNotify)) {
            ASSERT(VolumeContext->ChangeList.ullcbTotalChangesPending >= VolumeContext->ullcbChangesPendingCommit);
            ULONGLONG   ullcbTotalChangesPending = VolumeContext->ChangeList.ullcbTotalChangesPending - VolumeContext->ullcbChangesPendingCommit;
            if (ullcbTotalChangesPending >= VolumeContext->ulcbDataNotifyThreshold) {
                VolumeContext->bNotify = false;
                VolumeContext->liTickCountAtNotifyEventSet = KeQueryPerformanceCounter(NULL);
                KeSetEvent(VolumeContext->DBNotifyEvent, EVENT_INCREMENT, FALSE);
            }
        }
    }

    return Status;
}

NTSTATUS
DataFilteringCompletionRoutine(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp,
    IN PVOID          Context
    )
{
    
    NTSTATUS            Status = Irp->IoStatus.Status;
    // 64BIT : As length of IO would not be greater than 4GB casting ULONG_PTR to ULONG
    ULONG               ulDataWritten = (ULONG)Irp->IoStatus.Information;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp), IoNextStackLocation = IoGetNextIrpStackLocation(Irp);
    WRITE_META_DATA     WriteMetaData;
    KIRQL               OldIrql;
    PMDL                Mdl;
    PIRP				OrigIrp; 
	PCOMPLETION_CONTEXT pCompletionContext = NULL;
	PVOLUME_CONTEXT     VolumeContext;
    etContextType       eContextType = (etContextType)*((PCHAR )Context + sizeof(LIST_ENTRY));

    if (eContextType == ecCompletionContext) {
        pCompletionContext = (PCOMPLETION_CONTEXT)Context;
        WriteMetaData.llOffset = pCompletionContext->llOffset;
        WriteMetaData.ulLength = pCompletionContext->ulLength;
        
        VolumeContext = pCompletionContext->VolumeContext;

    } else {
        VolumeContext = (PVOLUME_CONTEXT)Context;
        
        WriteMetaData.llOffset = IoStackLocation->Parameters.Write.ByteOffset.QuadPart;
        WriteMetaData.ulLength = IoStackLocation->Parameters.Write.Length;
        
        if (Irp->PendingReturned) {
            IoMarkIrpPending(Irp);
        }
    }

    Mdl = Irp->MdlAddress;

    InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, ("DFCRtn: Irp = %#p, Volume = %s(%s)\n",
                   Irp, VolumeContext->DriveLetter, VolumeContext->GUIDinANSI));
    // 0 byte lengths are skipping in write dispatch funciton.
    // So need not check fo 0 byte writes.

    KeAcquireSpinLock(&VolumeContext->Lock, &OldIrql);
    InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, ("DFCRtn: Before Irp procesed. VC: Alloced = %#x Missed = %#I64x Free = %#x Reserved = %#x\n",
                                               VolumeContext->ulcbDataAllocated, VolumeContext->ullcbDataAlocMissed, 
                                               VolumeContext->ulcbDataFree, VolumeContext->ulcbDataReserved));

    if ((NT_SUCCESS(Status)) && (0 == (VolumeContext->ulFlags & VCF_FILTERING_STOPPED))) {
        for (ULONG ul = 0; ul < MAX_VC_IO_SIZE_BUCKETS; ul++) {
            if (VolumeContext->ulIoSizeArray[ul]) {
                if (ulDataWritten <= VolumeContext->ulIoSizeArray[ul]) {
                    VolumeContext->ullIoSizeCounters[ul]++;
                    break;
                }
            } else {
                VolumeContext->ullIoSizeCounters[ul]++;
                break;
            }
        }

        // Io has succeded
        if (ecCaptureModeMetaData == VolumeContext->eCaptureMode) {
            // Capture mode has changed from data to meta data.
            InVolDbgPrint(DL_TRACE_L2, DM_DATA_FILTERING, ("DFCRtn: Capture mode is not data\n"));
            Status = AddMetaDataToVolumeContext(VolumeContext, &WriteMetaData, TRUE);
            VCFreeReservedDataBlockList(VolumeContext);
            VolumeContext->ulcbDataReserved = 0;
        } else {
            ASSERT(NULL != Mdl);
            if (NULL == Mdl) {
                InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING,
                              ("DFCRtn: Mdl = %#p, Offset = %#I64x, length = %#x\n",
                               Mdl, WriteMetaData.llOffset, WriteMetaData.ulLength));
                InVolDbgPrint(DL_WARNING, DM_DATA_FILTERING, ("%s: Changing mode to meta data as Mdl is NULL\n", __FUNCTION__));
                Status = AddMetaDataToVolumeContext(VolumeContext, &WriteMetaData, TRUE);
            } else if (0 == (Mdl->MdlFlags & (MDL_MAPPED_TO_SYSTEM_VA | MDL_SOURCE_IS_NONPAGED_POOL))) {
                InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING,
                              ("DFCRtn: Mdl with out system address mapped, Mdl = %#p, Offset = %#I64x, length = %#x\n",
                               Mdl, WriteMetaData.llOffset, WriteMetaData.ulLength));
                InVolDbgPrint(DL_WARNING, DM_DATA_FILTERING, ("%s: Changing mode to meta data as Mdl is not mapped\n", __FUNCTION__));
                Status = AddMetaDataToVolumeContext(VolumeContext, &WriteMetaData, TRUE);
            } else {
                Status = CopyDataToVolumeContext(VolumeContext, Irp, &WriteMetaData);
                if(!NT_SUCCESS(Status))
                {
                    ULONG   ulRecordSize = SVRecordSizeForIOSize(WriteMetaData.ulLength, VolumeContext->ulMaxDataSizePerDataModeDirtyBlock);

                    InVolDbgPrint(DL_TRACE_L2, DM_DATA_FILTERING, ("DFCRtn: CopyDataToVolumeContext failed with status = %#x offset = %#I64x length = %#x ulRecordSize = %#x\n",
                                                            Status, WriteMetaData.llOffset, WriteMetaData.ulLength, ulRecordSize));

                    VolumeContext->ullNetWritesSeenInBytes -= ulRecordSize;
                    ASSERT(VolumeContext->ullNetWritesSeenInBytes == VolumeContext->ullUnaccountedBytes + VolumeContext->ulcbDataReserved + VolumeContext->ullcbDataAlocMissed);
                } else {
                    // Now check if data copied has crossed threshold if so queue a dirty block for write.
                    // Write only if Status is success
                    if (NeedWritingToDataFile(VolumeContext))
                    {
                        PCHANGE_NODE ChangeNode = GetChangeNodeToSaveAsFile(VolumeContext);
                        if (NULL != ChangeNode) {
                            Status = DriverContext.DataFileWriterManager->AddWorkItem(VolumeContext->VolumeFileWriter, ChangeNode, VolumeContext);
                            ASSERT(STATUS_SUCCESS == Status);
                            DereferenceChangeNode(ChangeNode);
                        }
                    }
                }
            }
        }

        if(!NT_SUCCESS(Status)) {
            QueueWorkerRoutineForSetVolumeOutOfSync(VolumeContext, ERROR_TO_REG_OUT_OF_MEMORY_FOR_DIRTY_BLOCKS, STATUS_NO_MEMORY, true);
        }
    } else {
        // Io has failed or filtering is stopped
        ULONG   ulRecordSize = SVRecordSizeForIOSize(WriteMetaData.ulLength, VolumeContext->ulMaxDataSizePerDataModeDirtyBlock);

        InVolDbgPrint(DL_TRACE_L2, DM_DATA_FILTERING, ("DFCRtn: Irp failed with status = %#x offset = %#I64x length = %#x ulRecordSize = %#x\n",
                                                   Status, WriteMetaData.llOffset, WriteMetaData.ulLength, ulRecordSize));

        VolumeContext->ullNetWritesSeenInBytes -= ulRecordSize;
        VCAdjustCountersToUndoReserve(VolumeContext, ulRecordSize);
        ASSERT((VolumeContext->eCaptureMode != ecCaptureModeData) || 
               (VolumeContext->ullNetWritesSeenInBytes == VolumeContext->ullUnaccountedBytes + VolumeContext->ulcbDataReserved + VolumeContext->ullcbDataAlocMissed));
    }

    VolumeContext->ulNumPendingIrps--;
    InVolDbgPrint(DL_TRACE_L2, DM_DATA_FILTERING, 
                  ("DFCRtn: After Irp(%#p) procesed. VC: PenIrp = %#x Alloced = %#x DaBUsed = %#x, DaBQueued = %#x, Missed = %#I64x Free = %#x Reserved = %#x\n",
                   Irp, VolumeContext->ulNumPendingIrps, VolumeContext->ulcbDataAllocated, 
                   VolumeContext->lDataBlocksInUse, VolumeContext->lDataBlocksQueuedToFileWrite, VolumeContext->ullcbDataAlocMissed, 
                   VolumeContext->ulcbDataFree, VolumeContext->ulcbDataReserved));

    ASSERT((VolumeContext->ulNumPendingIrps) || (0 == VolumeContext->ulcbDataReserved));
    KeReleaseSpinLock(&VolumeContext->Lock, OldIrql);
    DereferenceVolumeContext(VolumeContext);
    if (eContextType == ecCompletionContext) {
        if(pCompletionContext->CompletionRoutine) {
            IoNextStackLocation->CompletionRoutine = pCompletionContext->CompletionRoutine;
            IoNextStackLocation->Context = pCompletionContext->Context;
            Status = (*pCompletionContext->CompletionRoutine)(DeviceObject, Irp, pCompletionContext->Context);

        }
        ExFreePoolWithTag(pCompletionContext, TAG_GENERIC_NON_PAGED);
    

    } else {
        // Actual IRP Status should be returned.
        Status = Irp->IoStatus.Status;    
    }
    return Status;
}

void
ReserveDataBytes(
    PVOLUME_CONTEXT VolumeContext,
    bool            bCanRetry,
    ULONG           &ulBytes,
    bool            bFirstTime,
    bool            bLockAcquired,
    bool            btag
    )
{
    KIRQL       OldIrql;
   

    if (!bLockAcquired) {
        KeAcquireSpinLock(&VolumeContext->Lock, &OldIrql);
    }

    if (bFirstTime) {
        ASSERT((VolumeContext->ulNumPendingIrps) || (0 == VolumeContext->ulcbDataReserved));
        if ((0 == VolumeContext->ulNumPendingIrps) && (VolumeContext->ulMaxDataSizePerDataModeDirtyBlock != DriverContext.ulMaxDataSizePerDataModeDirtyBlock)) {          
            AddLastChangeTimestampToCurrentNode(VolumeContext, NULL);
            VolumeContext->ulMaxDataSizePerDataModeDirtyBlock = DriverContext.ulMaxDataSizePerDataModeDirtyBlock;
        }
        if (!btag) {
            ulBytes = SVRecordSizeForIOSize(ulBytes, VolumeContext->ulMaxDataSizePerDataModeDirtyBlock);            
        }
        VolumeContext->ulNumPendingIrps++;
        VolumeContext->ullNetWritesSeenInBytes += ulBytes;
        VolumeContext->ullUnaccountedBytes += ulBytes;
    }

     ULONG       WriteLength = ulBytes;  //Used for statistical reason.

    if (VolumeContext->ullcbDataAlocMissed) {
        VolumeContext->ullcbDataAlocMissed += ulBytes;
        ulBytes = 0;
        InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, ("%#p: (missed > 0) missed = %#I64x\n", VolumeContext, VolumeContext->ullcbDataAlocMissed));
    } else if (VolumeContext->ulcbDataFree >= ulBytes) {
        // We already have enough memory to copy this data
        VolumeContext->ulcbDataFree -= ulBytes;
        VolumeContext->ulcbDataReserved += ulBytes;
        ulBytes = 0;
        InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, ("%#p: Using Free memory. Free = %#x Reserved = %#x\n",
                                                   VolumeContext, VolumeContext->ulcbDataFree, VolumeContext->ulcbDataReserved));
    } else {
        // Add the free data buffer to reserved. Let us see if we could allocate more.
        ulBytes -= VolumeContext->ulcbDataFree;
        VolumeContext->ulcbDataReserved += VolumeContext->ulcbDataFree;
        VolumeContext->ulcbDataFree = 0;
        InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, ("%#p: Needed = %#x, Reserved = %#x, Free = %#x\n",
                                                   VolumeContext, ulBytes, VolumeContext->ulcbDataReserved,
                                                   VolumeContext->ulcbDataFree));
        // Allocate if required.
        while (ulBytes && (VolumeContext->ulcbDataAllocated < DriverContext.ulMaxDataSizePerVolume)) {
            PDATA_BLOCK DataBlock = AllocateLockedDataBlock(FALSE);
            if (NULL == DataBlock)
                break;

            VCAddToReservedDataBlockList(VolumeContext, DataBlock);
            InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, ("%#p: Allocated locked Data Block. Alloc = %#x\n", VolumeContext, VolumeContext->ulcbDataAllocated));
            if (ulBytes > DriverContext.ulDataBlockSize) {
                ulBytes -= DriverContext.ulDataBlockSize;
                VolumeContext->ulcbDataReserved += DriverContext.ulDataBlockSize;
                InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, ("%#p: Needed = %#x, Reserved = %#x\n", VolumeContext, ulBytes, VolumeContext->ulcbDataReserved));
            } else {
                VolumeContext->ulcbDataFree += DriverContext.ulDataBlockSize - ulBytes;
                VolumeContext->ulcbDataReserved += ulBytes;
                ulBytes = 0;
                InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, ("%#p: Needed = %#x, Reserved = %#x, Free = %#x\n", 
                                                               VolumeContext, ulBytes, VolumeContext->ulcbDataReserved, VolumeContext->ulcbDataFree));
                break;
            }
        }

        if (ulBytes && (!bCanRetry || (VolumeContext->ulcbDataAllocated >= DriverContext.ulMaxDataSizePerVolume))) {
            // Already maximum data buffers allocated for this volume.
            VolumeContext->ullcbDataAlocMissed += ulBytes;
            ulBytes = 0;
            InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, ("%#p: (Alloc >= MaxPerVol) Needed = %#x, missed = %#I64x\n",
                                                       VolumeContext, ulBytes, VolumeContext->ullcbDataAlocMissed));
        }
    }

    VolumeContext->ullUnaccountedBytes -= WriteLength - ulBytes;
    ASSERT((VolumeContext->eCaptureMode != ecCaptureModeData) || 
           (VolumeContext->ullNetWritesSeenInBytes == VolumeContext->ullUnaccountedBytes + VolumeContext->ulcbDataReserved + VolumeContext->ullcbDataAlocMissed));

    if (!bLockAcquired) {
        KeReleaseSpinLock(&VolumeContext->Lock, OldIrql);
    }

    return;
}


NTSTATUS
WriteDispatchInDataFilteringModeAtPassiveLevel(
    PDEVICE_OBJECT   DeviceObject,
    PIRP            Irp
    
    )
{
    ASSERT((NULL != DeviceObject) && (NULL != Irp));

    NTSTATUS            Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION   DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PVOLUME_CONTEXT     VolumeContext = DeviceExtension->pVolumeContext;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);

    
    PDRIVER_DISPATCH     DispatchEntryForWrite = NULL;

    if(VolumeContext->IsVolumeCluster) {
        DispatchEntryForWrite = DriverContext.MajorFunctionForClusDrives[IRP_MJ_WRITE];
    } else {
        DispatchEntryForWrite = DriverContext.MajorFunction[IRP_MJ_WRITE];
    }

    
    
    ASSERT(NULL != DeviceExtension->TargetDeviceObject);

    InVolDbgPrint(DL_TRACE_L3, DM_DATA_FILTERING, ("WDInDFModeAtPassiveLevel: Irp=%#p Of=%#I64x L=%#x Called on Volume %S(%S)\n",
                                               Irp, IoStackLocation->Parameters.Write.ByteOffset.QuadPart,
                                               IoStackLocation->Parameters.Write.Length,
                                               VolumeContext->wGUID, VolumeContext->wDriveLetter));
    // Get size need to be reserved
    ULONG   ulReserveNeeded = IoStackLocation->Parameters.Write.Length;


    ReserveDataBytes(VolumeContext, true, ulReserveNeeded, true);

    InVolDbgPrint(DL_TRACE_L1, DM_DATA_FILTERING, ("%#p: Needed = %#x, Allocated = %#x, Free = %#x, Reserved = %#x,missed = %I64x\n", 
                                               Irp, ulReserveNeeded, VolumeContext->ulcbDataAllocated, VolumeContext->ulcbDataFree,
                                               VolumeContext->ulcbDataReserved, VolumeContext->ullcbDataAlocMissed));


    while (ulReserveNeeded) {
        if (STATUS_SUCCESS == AddNewDataBlockToLockedDataBlockList()) {
            ReserveDataBytes(VolumeContext, true, ulReserveNeeded, false);
        } else {
            ReserveDataBytes(VolumeContext, false, ulReserveNeeded, false);
            ASSERT(0 == ulReserveNeeded);
        }
    }

    UpdateLockedDataBlockList(DriverContext.ulMaxLockedDataBlocks);

    //if (IsRebootNotDone) {
    if(!VolumeContext->IsVolumeUsingAddDevice && DriverContext.IsDispatchEntryPatched) {
        COMPLETION_CONTEXT  *pCompRoutine = (PCOMPLETION_CONTEXT)ExAllocatePoolWithTag(NonPagedPool, sizeof(COMPLETION_CONTEXT), TAG_GENERIC_NON_PAGED);
        if (pCompRoutine) {
            RtlZeroMemory(pCompRoutine, sizeof(COMPLETION_CONTEXT));
            MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
            
            ReferenceVolumeContext(VolumeContext);
            pCompRoutine->eContextType = ecCompletionContext;
            pCompRoutine->VolumeContext = VolumeContext;
            pCompRoutine->CompletionRoutine = IoStackLocation->CompletionRoutine;
            pCompRoutine->Context = IoStackLocation->Context;
            pCompRoutine->llOffset = IoStackLocation->Parameters.Write.ByteOffset.QuadPart;
            pCompRoutine->ulLength = IoStackLocation->Parameters.Write.Length;

            IoStackLocation->CompletionRoutine = DataFilteringCompletionRoutine;
            IoStackLocation->Context = pCompRoutine;
        } else {
            // Volumes detected by enumeration needs to patch the completion routine to Filter in the completion path
            // If driver fail to patch due to non availablity of non-paged pool, We miss IO. Marking for resync
            QueueWorkerRoutineForSetVolumeOutOfSync(VolumeContext, INVOLFLT_ERR_NO_GENERIC_NPAGED_POOL, STATUS_NO_MEMORY, false);
        }
        Status = (*DispatchEntryForWrite)(DeviceExtension->TargetDeviceObject, Irp) ;

    } else {
        // Make sure data is mapped to system address
        MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);

        // Set IoCompletion routine for data filtering.
        IoCopyCurrentIrpStackLocationToNext(Irp);
        ReferenceVolumeContext(VolumeContext);
        IoSetCompletionRoutine(Irp, DataFilteringCompletionRoutine, VolumeContext, TRUE, TRUE, TRUE);

        Status = IoCallDriver(DeviceExtension->TargetDeviceObject, Irp);
    }
    return Status;
}

NTSTATUS
WriteDispatchInDataFilteringModeAtDispatchLevel(
    PDEVICE_OBJECT   DeviceObject,
    PIRP            Irp
    
    )
{
    ASSERT((NULL != DeviceObject) && (NULL != Irp));

    NTSTATUS            Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION   DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PVOLUME_CONTEXT     VolumeContext = DeviceExtension->pVolumeContext;
    PIO_STACK_LOCATION  IoStackLocation = IoGetCurrentIrpStackLocation(Irp);

    PDRIVER_DISPATCH     DispatchEntryForWrite = NULL;

    if(VolumeContext->IsVolumeCluster) {
        DispatchEntryForWrite = DriverContext.MajorFunctionForClusDrives[IRP_MJ_WRITE];
    } else {
        DispatchEntryForWrite = DriverContext.MajorFunction[IRP_MJ_WRITE];
    }


    ASSERT(NULL != DeviceExtension->TargetDeviceObject);

    InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, ("WDInDFModeAtDispatchLevel: Irp=%#p Of=%#I64x L=%#x Called on Volume %s(%s)\n",
                                               Irp, IoStackLocation->Parameters.Write.ByteOffset.QuadPart,
                                               IoStackLocation->Parameters.Write.Length,
                                               VolumeContext->GUIDinANSI, VolumeContext->DriveLetter));

    // Check if we need to reserve more memory.
    ULONG   ulReserveNeeded = IoStackLocation->Parameters.Write.Length;
    
    ReserveDataBytes(VolumeContext, false, ulReserveNeeded, true);

    InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, ("%#p: Needed = %#x, Allocated = %#x, Free = %#x, Reserved = %#x, missed = %I64x\n", 
                                               Irp, ulReserveNeeded, VolumeContext->ulcbDataAllocated, VolumeContext->ulcbDataFree,
                                               VolumeContext->ulcbDataReserved, VolumeContext->ullcbDataAlocMissed));

    if(!VolumeContext->IsVolumeUsingAddDevice && DriverContext.IsDispatchEntryPatched) {
        COMPLETION_CONTEXT  *pCompRoutine = (PCOMPLETION_CONTEXT)ExAllocatePoolWithTag(NonPagedPool, sizeof(COMPLETION_CONTEXT), TAG_GENERIC_NON_PAGED);
        if (pCompRoutine) {
            RtlZeroMemory(pCompRoutine, sizeof(COMPLETION_CONTEXT));
            ReferenceVolumeContext(VolumeContext);
            pCompRoutine->eContextType = ecCompletionContext;
            pCompRoutine->VolumeContext = VolumeContext;
            pCompRoutine->llOffset = IoStackLocation->Parameters.Write.ByteOffset.QuadPart;
            pCompRoutine->ulLength = IoStackLocation->Parameters.Write.Length;

            pCompRoutine->CompletionRoutine = IoStackLocation->CompletionRoutine;
            pCompRoutine->Context = IoStackLocation->Context;

            IoStackLocation->CompletionRoutine = DataFilteringCompletionRoutine;
            IoStackLocation->Context = pCompRoutine;
        } else {
            // Volumes detected by enumeration needs to patch the completion routine to Filter in the completion path
            // If driver fail to patch due to non availablity of non-paged pool, We miss IO. Marking for resync
            QueueWorkerRoutineForSetVolumeOutOfSync(VolumeContext, INVOLFLT_ERR_NO_GENERIC_NPAGED_POOL, STATUS_NO_MEMORY, false);            
        }

        Status = (*DispatchEntryForWrite)(DeviceExtension->TargetDeviceObject, Irp) ;

    } else {
        // Set IoCompletion routine for data filtering.
        IoCopyCurrentIrpStackLocationToNext(Irp);
        ReferenceVolumeContext(VolumeContext);
        IoSetCompletionRoutine(Irp, DataFilteringCompletionRoutine, VolumeContext, TRUE, TRUE, TRUE);
        
        Status = IoCallDriver(DeviceExtension->TargetDeviceObject, Irp);
    }
    return Status;
}

NTSTATUS
WriteDispatchInDataFilteringMode(
    PDEVICE_OBJECT   DeviceObject,
    PIRP            Irp
    
    )
{
    NTSTATUS    Status;

    if (KeGetCurrentIrql() >= DISPATCH_LEVEL ) {
        ASSERT(NULL != Irp->MdlAddress);
        ASSERT(Irp->MdlAddress->MdlFlags & (MDL_MAPPED_TO_SYSTEM_VA | MDL_SOURCE_IS_NONPAGED_POOL));
        Status = WriteDispatchInDataFilteringModeAtDispatchLevel(DeviceObject, Irp);
    } else {
        Status = WriteDispatchInDataFilteringModeAtPassiveLevel(DeviceObject, Irp);
    }

    return Status;
}

/*-------------------------------------------------------------------------------------------------
 * DATA_POOL, DATA_BLOCK, and DATA_BUFFER routines
 *-------------------------------------------------------------------------------------------------
 */
VOID
CleanDataPool()
{  
    // Free any locked data blocks.
    FreeLockedDataBlockList();    
    // Free free data block list
    /* Bug 23428 - Tunable Testing caused BSOD */
    if (KeGetCurrentIrql() == PASSIVE_LEVEL) {
        FreeFreeDataBlockList(0);
    }	

    return;
}

NTSTATUS
AllocateAndInitializeDataPool()
{
    NTSTATUS    Status = STATUS_SUCCESS;
    // Seq of initialization should not be change as there are inter dependecies
    //Reboot Required
    DriverContext.ulDataBufferSize = ValidateDataBufferSize(DriverContext.ulDataBufferSize);

    DriverContext.ulDataBlockSize = ValidateDataBlockSize(DriverContext.ulDataBlockSize);
    //Reboot Not Requied

    DriverContext.ullDataPoolSize = ValidateDataPoolSize(DriverContext.ullDataPoolSize);
    DriverContext.ulMaxDataSizePerDataModeDirtyBlock = ValidateMaxDataSizePerDataModeDirtyBlock(DriverContext.ulMaxDataSizePerDataModeDirtyBlock);    

    DriverContext.ulMaxDataSizePerNonDataModeDirtyBlock =  ValidateMaxDataSizePerNonDataModeDirtyBlock(DriverContext.ulMaxDataSizePerNonDataModeDirtyBlock); 

    //------ 
    //Derivated Variables
    
    
    

    

    

    DriverContext.DataFileWriterManager = new CDataFileWriterManager(DriverContext.ulNumberOfThreadsPerFileWriter,
                                                                     DriverContext.ulNumberOfCommonFileWriters);

    //Reboot Not Requied

    

    DriverContext.ulMaxDataSizePerVolume = ValidateMaxDataSizePerVolume(DriverContext.ulMaxDataSizePerVolume);

    DriverContext.ulMaxSetLockedDataBlocks = ValidateMaxSetLockedDataBlocks(DriverContext.ulMaxSetLockedDataBlocks);

    DriverContext.ulFlags |= DC_FLAGS_DATA_POOL_VARIABLES_INTIALIZED;
    //Data Pool getting preallocated only on 32bit machines.
#ifndef _WIN64
    if (DriverContext.bEnableDataCapture) {
        Status = SetDataPool(1, DriverContext.ullDataPoolSize);
        if (!NT_SUCCESS(Status)) {
            CleanDataPool();
            DriverContext.bEnableDataCapture = false;
        }
    } 	
#endif

    return Status;
}

NTSTATUS
UpdateLockedDataBlockList(ULONG ulDataBlocks)
{
    NTSTATUS    Status = STATUS_SUCCESS;
    PDATA_BLOCK DataBlock = NULL;
    KIRQL       OldIrql;

    PAGED_CODE();

    while (DriverContext.ulDataBlocksInLockedDataBlockList < ulDataBlocks) 
    {
        DataBlock = AllocateDataBlock();
        if (NULL != DataBlock) {
            ASSERT(NULL != DataBlock->Mdl);
            Status = LockDataBlock(DataBlock);
            if (STATUS_SUCCESS != Status) {
                DeallocateDataBlock(DataBlock);
                DataBlock = NULL;
                break;
            }

            KeAcquireSpinLock(&DriverContext.DataBlockListLock, &OldIrql);
            InsertTailList(&DriverContext.LockedDataBlockList, &DataBlock->ListEntry);
            DriverContext.ulDataBlocksInLockedDataBlockList++;
            KeReleaseSpinLock(&DriverContext.DataBlockListLock, OldIrql);
        } else {
            InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, ("UpdateLockedDataBlockList: Out of data blocks\n"));
            Status = STATUS_NO_MEMORY;
            break;
        }
    }

    return Status;
}

VOID
FreeLockedDataBlockList()
{
    PDATA_BLOCK DataBlock = NULL;
    KIRQL       OldIrql;
    bool free = 0;
    bool bPartial = 1;
    PAGED_CODE();

    KeAcquireSpinLock(&DriverContext.DataBlockListLock, &OldIrql);

    while (!IsListEmpty(&DriverContext.LockedDataBlockList)) {
        ASSERT(0 != DriverContext.ulDataBlocksInLockedDataBlockList);

        // Remove entry from locked data block list.
        DataBlock = (PDATA_BLOCK)RemoveHeadList(&DriverContext.LockedDataBlockList);
        DriverContext.ulDataBlocksInLockedDataBlockList--;

        ASSERT(DataBlock->ulFlags & DATABLOCK_FLAGS_LOCKED);

        // unlock data block.
        UnLockDataBlock(DataBlock);
        
        // Insert into free data block list.
        InsertTailList(&DriverContext.FreeDataBlockList, &DataBlock->ListEntry);
        DriverContext.ulDataBlocksInFreeDataBlockList++;
    }
    //Additional Validation to avoid DIVIDE_BY_ZERO 
    if (0 == DriverContext.ulDataBlockSize) {
        free = 0;
    } else {
    	//Free Data blocks if Allocated are more than DataPoolSize or DataCapture is disabled
        ULONG ulMaxDataBlocks = (ULONG)((DriverContext.ullDataPoolSize) / DriverContext.ulDataBlockSize);
        if (DriverContext.ulDataBlockAllocated > ulMaxDataBlocks) {
            free = 1;
        } else if (!DriverContext.bEnableDataCapture) {
            bPartial = 0;
            free = 1;
        }         
    }
    KeReleaseSpinLock(&DriverContext.DataBlockListLock, OldIrql);
    if (free && (DriverContext.ulDataBlocksInFreeDataBlockList > 4)) {
        FreeFreeDataBlockList(bPartial);
    }
    return;
}
/* This function should be called at Passive Level */
VOID
FreeFreeDataBlockList(bool bPartial)
{
    PDATA_BLOCK DataBlock = NULL;
    KIRQL       OldIrql;

    PAGED_CODE();

    LIST_ENTRY  ListEntry;

    InitializeListHead(&ListEntry);

    KeAcquireSpinLock(&DriverContext.DataBlockListLock, &OldIrql);
    //Additional Validation to avoid DIVIDE_BY_ZERO
    if (0 == DriverContext.ulDataBlockSize) {
        KeReleaseSpinLock(&DriverContext.DataBlockListLock, OldIrql);
        return;
    }
    
    ULONG ulMaxDataBlocks = (ULONG)((DriverContext.ullDataPoolSize) / DriverContext.ulDataBlockSize);
    // Divide data pool into data blocks
    while (!IsListEmpty(&DriverContext.FreeDataBlockList)) {
        ASSERT(0 != DriverContext.ulDataBlocksInFreeDataBlockList);
        // Free the partial list if asked.
        if ((DriverContext.ulDataBlockAllocated <= ulMaxDataBlocks) && (bPartial)) {
            break;
        }
        // Remove entry from locked data block list.
        DataBlock = (PDATA_BLOCK)RemoveHeadList(&DriverContext.FreeDataBlockList);
        DriverContext.ulDataBlocksInFreeDataBlockList--;
        DriverContext.ulDataBlockAllocated--;
        InsertTailList(&ListEntry, &DataBlock->ListEntry);
        ASSERT(0 == (DataBlock->ulFlags & DATABLOCK_FLAGS_LOCKED));
        ASSERT(0 == (DataBlock->ulFlags & DATABLOCK_FLAGS_MAPPED));
    }
    // As MaxLockedDataBlock shd not be greater than DataBlockAllocated
    SetMaxLockedDataBlocks();
    KeReleaseSpinLock(&DriverContext.DataBlockListLock, OldIrql);

    while (!IsListEmpty(&ListEntry)) {
        DataBlock = (PDATA_BLOCK)RemoveHeadList(&ListEntry);
        // Free mdl
        IoFreeMdl(DataBlock->Mdl);
        ExFreePoolWithTag(DataBlock->KernelAddress, TAG_DATA_POOL_CHUNK);
        // Free data block
        ExFreePoolWithTag(DataBlock, TAG_DATA_BLOCK);
    }
    
    return;
}

NTSTATUS SetDataPool(bool bInitialAlloc, ULONGLONG ullDataPoolSize) 
{
    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    PDATA_BLOCK DataBlock;
    PUCHAR      NextAddress;
    
    ASSERT(0 != DriverContext.ulDataBlockSize);
    ASSERT(0 != DriverContext.ulDataBufferSize);       
    
    if (!DriverContext.bEnableDataCapture) {
        return Status;
    }    
    
    ULONG ulMaxDataBlocks = (ULONG)((ullDataPoolSize) / DriverContext.ulDataBlockSize);
    ULONG ulDataBlockAllocated = 0;

    KIRQL       OldIrql;
    LIST_ENTRY  ListEntry;
    InitializeListHead(&ListEntry);
    //Allocate required data block in temp list
    KeAcquireSpinLock(&DriverContext.DataBlockListLock, &OldIrql);
    bool lock = 1;
    while ((DriverContext.ulDataBlockAllocated + ulDataBlockAllocated) < ulMaxDataBlocks) {
        KeReleaseSpinLock(&DriverContext.DataBlockListLock, OldIrql);
	lock = 0;
        NextAddress = (PUCHAR)ExAllocatePoolWithTag(PagedPool, DriverContext.ulDataBlockSize, TAG_DATA_POOL_CHUNK);
        if (NULL == NextAddress) {
            // memory allocation failed here.
            InVolDbgPrint(DL_ERROR, DM_DRIVER_INIT | DM_DATA_FILTERING, 
                ("Failed to allocate memory for Data Pool of size %#I64x\n", DriverContext.ullDataPoolSize));
            break;
        }

        DataBlock = (PDATA_BLOCK)ExAllocatePoolWithTag(NonPagedPool, sizeof(DATA_BLOCK), TAG_DATA_BLOCK);
        if (NULL == DataBlock) {
            ExFreePoolWithTag(NextAddress, TAG_DATA_POOL_CHUNK);
            break;
        }
    	
        RtlZeroMemory(DataBlock, sizeof(DATA_BLOCK));
            // DATA_BLOCK consists of the following fields
            // LIST_ENTRY  ListEntry;
            // PVOID       KernelAddress;
            // PVOID       MappedUserAddress;
            // PMDL        Mdl;
            // ULONG       ulFlags;
            // ULONG       ulcbDataFree;
            // ULONG       ulcbMaxData;
            // PVOID       CurrentPosition;
    
        DataBlock->KernelAddress = NextAddress;
        DataBlock->Mdl = IoAllocateMdl(DataBlock->KernelAddress, DriverContext.ulDataBlockSize, FALSE, FALSE,OPTIONAL NULL);
        if (NULL == DataBlock->Mdl) {
            ExFreePoolWithTag(DataBlock, TAG_DATA_BLOCK);
            ExFreePoolWithTag(NextAddress, TAG_DATA_POOL_CHUNK);
            break;
        }
        DataBlock->ulcbMaxData = DriverContext.ulDataBlockSize;
        InsertTailList(&ListEntry, &DataBlock->ListEntry);
        ulDataBlockAllocated++;
        KeAcquireSpinLock(&DriverContext.DataBlockListLock, &OldIrql);
        lock = 1;
    }

    bool free = 0;
    Status = STATUS_SUCCESS;
    if (!lock) {
        //Due to error cond Spin lock is not acquired
        KeAcquireSpinLock(&DriverContext.DataBlockListLock, &OldIrql);
    }
    //bInitialAlloc is unset when datapool size is reconfigured by user
    if (DriverContext.ulDataBlockAllocated > ulMaxDataBlocks) {
        if (!bInitialAlloc) {
            //Data Pool size reconfigured to lower value
            free = 1;
        }
    } else if (bInitialAlloc || ((DriverContext.ulDataBlockAllocated + ulDataBlockAllocated) >= ulMaxDataBlocks)) {
        //Data Pool size configured to higher value or Getting allocated first time
        if (ulDataBlockAllocated) {
            // Add Data Blocks to global list from temp list	    
            while (!IsListEmpty(&ListEntry) && (DriverContext.ulDataBlockAllocated < ulMaxDataBlocks)) {
                DataBlock = (PDATA_BLOCK)RemoveHeadList(&ListEntry);
                InsertTailList(&DriverContext.FreeDataBlockList, &DataBlock->ListEntry);
                DriverContext.ulDataBlocksInFreeDataBlockList++;
                DriverContext.ulDataBlockAllocated++;
            }
            //MaxLockedDataBlock is dependent on DataBlockAllocated
            SetMaxLockedDataBlocks();
        }	
        if (bInitialAlloc) {
            //During initialization set the flag accordingly
            if (0 == DriverContext.ulDataBlockAllocated) {
                DriverContext.ulFlags |= DC_FLAGS_DATAPOOL_ALLOCATION_FAILED;
                Status = STATUS_NO_MEMORY;	    
            } else {            
                DriverContext.ulFlags &= ~DC_FLAGS_DATAPOOL_ALLOCATION_FAILED;
            }
        }
    } else {
        // Could not allocated req Data pool.
        Status = STATUS_NO_MEMORY;
    }

    if ((!bInitialAlloc) && NT_SUCCESS(Status)) {
        //Set DataPoolSize, MaxLockDataBlock, MaxDataSizePerVol
        DriverContext.ullDataPoolSize = ullDataPoolSize;
        ULONG ulMaxDataSizePerVolume = ULONG((((ullDataPoolSize / MEGABYTES) * MAX_DATA_SIZE_PER_VOL_PERC) / 100) * MEGABYTES);
        DriverContext.ulMaxDataSizePerVolume = ValidateMaxDataSizePerVolume(ulMaxDataSizePerVolume);
        SetMaxLockedDataBlocks();
    }

    KeReleaseSpinLock(&DriverContext.DataBlockListLock, OldIrql);

    // Free additional memory. This condition would only arise if DataPoolSize is getting set by multiple thread
    while (!IsListEmpty(&ListEntry)) {
        DataBlock = (PDATA_BLOCK)RemoveHeadList(&ListEntry);
        // Free mdl
        IoFreeMdl(DataBlock->Mdl);
        ExFreePoolWithTag(DataBlock->KernelAddress, TAG_DATA_POOL_CHUNK);
        // Free data block
        ExFreePoolWithTag(DataBlock, TAG_DATA_BLOCK);
    }

    if (free) {
        FreeFreeDataBlockList(1);
    }

    if (!NT_SUCCESS(Status)) {
        InMageFltLogError(DriverContext.ControlDevice, __LINE__, INVOLFLT_ERR_FAILED_TO_ALLOCATE_DATA_POOL, Status);
    } else {
        if (ulDataBlockAllocated) {	    
            Status = UpdateLockedDataBlockList(DriverContext.ulMaxLockedDataBlocks);
            //UpdateLockedDataBlockList return value is ignored if Data Pool is reconfigured by user
            if (!bInitialAlloc) {
                Status = STATUS_SUCCESS;
            }
        }	    
    }	
    return Status;
}

/*-------------------------------------------------------------------------------------------------
 * DATA_BLOCK related functions
 *-------------------------------------------------------------------------------------------------
 */
PDATA_BLOCK
AllocateDataBlock()
{
    PDATA_BLOCK DataBlock = NULL;
    KIRQL   OldIrql;

    KeAcquireSpinLock(&DriverContext.DataBlockListLock, &OldIrql);

    if (!IsListEmpty(&DriverContext.FreeDataBlockList)) {
        ASSERT(0 != DriverContext.ulDataBlocksInFreeDataBlockList);

        DataBlock = (PDATA_BLOCK)RemoveTailList(&DriverContext.FreeDataBlockList);
        // DATA_BLOCK consists of the following fields
        // LIST_ENTRY  ListEntry;
        // PVOID       KernelAddress;   Already Initialized and should never change
        // PMDL        Mdl;             Already Initialized and should never change
        // PVOID       MappedUserAddress;   Should be zero
        // ULONG       ulFlags;             Should be zero
        // ULONG       ulcbDataFree;    
        // ULONG       ulcbMaxData;
        // PVOID       CurrentPosition;
        DriverContext.ulDataBlocksInFreeDataBlockList--;
        ASSERT((NULL != DataBlock->KernelAddress) && (NULL != DataBlock->Mdl));
        DataBlock->MappedUserAddress = NULL;
        DataBlock->ProcessId = NULL;
        DataBlock->VolumeId = NULL;
        DataBlock->ulFlags = 0;
        DataBlock->ulcbDataFree = DriverContext.ulDataBlockSize;
    }

    KeReleaseSpinLock(&DriverContext.DataBlockListLock, OldIrql);

    return DataBlock;
}

VOID
DeallocateDataBlock(PDATA_BLOCK DataBlock, bool locked)
{
    KIRQL       OldIrql;
    bool        free = 0;
    bool        bPartial = 1;

    UnMapDataBlock(DataBlock);

    if (NULL != DataBlock->pUserBufferArray) {
        SIZE_T   ulFreeSize = 0;
        ASSERT(IoGetCurrentProcess() == DriverContext.UserProcess);
        ZwFreeVirtualMemory(NtCurrentProcess(), (PVOID *)&DataBlock->pUserBufferArray, &ulFreeSize, MEM_RELEASE);
        DataBlock->pUserBufferArray = NULL;
    }

    KeAcquireSpinLock(&DriverContext.DataBlockListLock, &OldIrql);
    if (DataBlock->ulFlags & DATABLOCK_FLAGS_LOCKED) {
        // Data block is locked. Add to locked list if possible when Data Capture is enabled
        if ((DriverContext.ulDataBlocksInLockedDataBlockList < DriverContext.ulMaxLockedDataBlocks) && 
                  (DriverContext.bEnableDataCapture)) {
            DataBlock->ulcbDataFree = DriverContext.ulDataBlockSize;

            ASSERT((NULL != DataBlock->KernelAddress) && (NULL != DataBlock->Mdl) && 
                   (NULL == DataBlock->MappedUserAddress) && (DATABLOCK_FLAGS_LOCKED == DataBlock->ulFlags));

            InsertTailList(&DriverContext.LockedDataBlockList, &DataBlock->ListEntry);
            DriverContext.ulDataBlocksInLockedDataBlockList++;
            DataBlock = NULL;
        } else {
            UnLockDataBlock(DataBlock);
        }
    }

    if (NULL != DataBlock) {
        InsertTailList(&DriverContext.FreeDataBlockList, &DataBlock->ListEntry);
        DriverContext.ulDataBlocksInFreeDataBlockList++;
    }
    /* Bug 23428 - Tunable Testing caused BSOD */
    if (!locked) {
        ULONG ulMaxDataBlocks = (ULONG)((DriverContext.ullDataPoolSize) / DriverContext.ulDataBlockSize);
        if (DriverContext.ulDataBlockAllocated > ulMaxDataBlocks) {
            free = 1;
        } else if (!DriverContext.bEnableDataCapture) {
            free = 1;
            //As Data Capture is disabled. All memory can be freed
            bPartial = 0;
        }
    }        
    KeReleaseSpinLock(&DriverContext.DataBlockListLock, OldIrql);
    /* Bug 23428 - Tunable Testing caused BSOD */
    if (KeGetCurrentIrql() == PASSIVE_LEVEL) {
         if (free && ((DriverContext.ulDataBlocksInFreeDataBlockList > 4) || !bPartial)) {
             FreeFreeDataBlockList(bPartial);
         }
    }         
    return;
}

VOID
UnMapDataBlock(PDATA_BLOCK DataBlock) {
    if (DataBlock) {
        if (DataBlock->ulFlags & DATABLOCK_FLAGS_MAPPED) {
            ASSERT(IoGetCurrentProcess() == DriverContext.UserProcess);
            MmUnmapLockedPages(DataBlock->MappedUserAddress, DataBlock->Mdl);
            DataBlock->ulFlags &= ~DATABLOCK_FLAGS_MAPPED;
            DataBlock->MappedUserAddress = NULL;
            DataBlock->ProcessId = NULL;
            DataBlock->VolumeId = NULL;
            KIRQL       Irql;
            KeAcquireSpinLock(&DriverContext.DataBlockCounterLock, &Irql);
            //InVolDbgPrint(DL_INFO, DM_MEM_TRACE,("MappedDataBlockCounter -- %d\n", DriverContext.MappedDataBlockCounter));
            if (DriverContext.MappedDataBlockCounter) {
                DriverContext.MappedDataBlockCounter--;
            }
            KeReleaseSpinLock(&DriverContext.DataBlockCounterLock, Irql);
        }
    }
}

NTSTATUS
LockDataBlock(PDATA_BLOCK DataBlock)
{
    NTSTATUS    Status = STATUS_SUCCESS;

    PAGED_CODE();

    if (0 == (DataBlock->ulFlags & DATABLOCK_FLAGS_LOCKED)) {
        __try {
            MmProbeAndLockPages(DataBlock->Mdl, KernelMode, IoWriteAccess);
            DataBlock->ulFlags |= DATABLOCK_FLAGS_LOCKED;
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING,
                ("LockDataBlock: MmProbeAndLockPages caused an exception\n"));
            Status = STATUS_UNSUCCESSFUL;
        }
        if (DataBlock->ulFlags & DATABLOCK_FLAGS_LOCKED) {             			
            KIRQL       Irql;
            KeAcquireSpinLock(&DriverContext.DataBlockCounterLock, &Irql);
            DriverContext.LockedDataBlockCounter++;
            //InVolDbgPrint(DL_INFO, DM_MEM_TRACE,("LockedDataBlockCounter ++ %d\n", DriverContext.LockedDataBlockCounter));
            if(DriverContext.MaxLockedDataBlockCounter < DriverContext.LockedDataBlockCounter) {
                DriverContext.MaxLockedDataBlockCounter = DriverContext.LockedDataBlockCounter;
            }
            KeReleaseSpinLock(&DriverContext.DataBlockCounterLock, Irql);
        }
    }

    return Status;
}

VOID
UnLockDataBlock(PDATA_BLOCK DataBlock)
{
    if (NULL != DataBlock) {
        if (DataBlock->ulFlags & DATABLOCK_FLAGS_LOCKED) {
            if (NULL != DataBlock->Mdl) {
                KIRQL       Irql;
                MmUnlockPages(DataBlock->Mdl);
                DataBlock->ulFlags &= ~DATABLOCK_FLAGS_LOCKED;
                KeAcquireSpinLock(&DriverContext.DataBlockCounterLock, &Irql);
                //DbgPrint("LockedDataBlockCounter -- %d\n", DriverContext.LockedDataBlockCounter);
                if (DriverContext.LockedDataBlockCounter) {
		            DriverContext.LockedDataBlockCounter--;
                }
                KeReleaseSpinLock(&DriverContext.DataBlockCounterLock, Irql);
            }
        }
    }
}

PDATA_BLOCK
AllocateLockedDataBlock(BOOLEAN CanLockDataBlock)
{
    PDATA_BLOCK DataBlock = NULL;
    KIRQL       OldIrql;
    NTSTATUS    Status = STATUS_SUCCESS;

    if (DriverContext.ulDataBlocksInLockedDataBlockList > DriverContext.ulMaxLockedDataBlocks) {
        KeAcquireSpinLock(&DriverContext.DataBlockListLock, &OldIrql);
        if ((DriverContext.ulDataBlocksInLockedDataBlockList > DriverContext.ulMaxLockedDataBlocks) &&
            (!IsListEmpty(&DriverContext.LockedDataBlockList)) ) 
        {
            DataBlock = (PDATA_BLOCK)RemoveHeadList(&DriverContext.LockedDataBlockList);
            DriverContext.ulDataBlocksInLockedDataBlockList--;
        }
        KeReleaseSpinLock(&DriverContext.DataBlockListLock, OldIrql);
    }

    if (NULL != DataBlock) {
        return DataBlock;
    }

    if (CanLockDataBlock) {
        DataBlock = AllocateDataBlock();
        if (NULL != DataBlock) {
            ASSERT(NULL != DataBlock->Mdl);
            Status = LockDataBlock(DataBlock);
            if (STATUS_SUCCESS != Status) {
                DeallocateDataBlock(DataBlock);
                DataBlock = NULL;
            }
        }
    }

    if (NULL == DataBlock) {
        KeAcquireSpinLock(&DriverContext.DataBlockListLock, &OldIrql);
        if (!IsListEmpty(&DriverContext.LockedDataBlockList)) {
            DataBlock = (PDATA_BLOCK)RemoveHeadList(&DriverContext.LockedDataBlockList);
            DriverContext.ulDataBlocksInLockedDataBlockList--;
        }
        KeReleaseSpinLock(&DriverContext.DataBlockListLock, OldIrql);
    }

    return DataBlock;
}

VOID
FreeOrphanedMappedDataBlockList()
{
    LIST_ENTRY  ListHead;
    KIRQL       OldIrql;

    InitializeListHead(&ListHead);
    KeAcquireSpinLock(&DriverContext.DataBlockListLock, &OldIrql);
    while(!IsListEmpty(&DriverContext.OrphanedMappedDataBlockList)) {
        ASSERT(NULL != DriverContext.UserProcess);
        ASSERT(DriverContext.ulDataBlocksInOrphanedMappedDataBlockList > 0);
        PLIST_ENTRY pEntry = RemoveHeadList(&DriverContext.OrphanedMappedDataBlockList);
        DriverContext.ulDataBlocksInOrphanedMappedDataBlockList--;
        InsertHeadList(&ListHead, pEntry);
    }
    KeReleaseSpinLock(&DriverContext.DataBlockListLock, OldIrql);

    while(!IsListEmpty(&ListHead)) {
        PDATA_BLOCK DataBlock = (PDATA_BLOCK)RemoveHeadList(&ListHead);

        DeallocateDataBlock(DataBlock);
    }
}

VOID
FreeOrphanedMappedDataBlocksWithVolumeID(HANDLE VolumeId, HANDLE ProcessId)
{
    LIST_ENTRY  ListHead;
    KIRQL       OldIrql;

    InitializeListHead(&ListHead);

    KeAcquireSpinLock(&DriverContext.DataBlockListLock, &OldIrql);
    PLIST_ENTRY Entry = DriverContext.OrphanedMappedDataBlockList.Flink;
    while(Entry != &DriverContext.OrphanedMappedDataBlockList) {
        PDATA_BLOCK DataBlock = (PDATA_BLOCK)Entry;
        Entry = Entry->Flink;
        if ((VolumeId == DataBlock->VolumeId) && (ProcessId == DataBlock->ProcessId)) {
            ASSERT(DriverContext.ulDataBlocksInOrphanedMappedDataBlockList > 0);
            RemoveEntryList(&DataBlock->ListEntry);
            DriverContext.ulDataBlocksInOrphanedMappedDataBlockList--;

            InsertHeadList(&ListHead, &DataBlock->ListEntry);
        }
    }
    KeReleaseSpinLock(&DriverContext.DataBlockListLock, OldIrql);

    while(!IsListEmpty(&ListHead)) {
        PDATA_BLOCK DataBlock = (PDATA_BLOCK)RemoveHeadList(&ListHead);

        DeallocateDataBlock(DataBlock);
    }
}

VOID
AddDataBlockToLockedDataBlockList(PDATA_BLOCK DataBlock)
{
    KIRQL   OldIrql;

    ASSERT(NULL != DataBlock);

    if (NULL == DataBlock) {
        return;
    }

    ASSERT(DataBlock->ulFlags & DATABLOCK_FLAGS_LOCKED);
    ASSERT(0 == (DataBlock->ulFlags & DATABLOCK_FLAGS_MAPPED));
    KeAcquireSpinLock(&DriverContext.DataBlockListLock, &OldIrql);
    ASSERT((NULL != DataBlock->KernelAddress) && (NULL != DataBlock->Mdl) && 
           (NULL == DataBlock->MappedUserAddress) && (DATABLOCK_FLAGS_LOCKED == DataBlock->ulFlags) &&
           (DriverContext.ulDataBlockSize == DataBlock->ulcbDataFree));

    InsertHeadList(&DriverContext.LockedDataBlockList, &DataBlock->ListEntry);
    DriverContext.ulDataBlocksInLockedDataBlockList++;
    KeReleaseSpinLock(&DriverContext.DataBlockListLock, OldIrql);

    return;
}

NTSTATUS
AddNewDataBlockToLockedDataBlockList()
{
    NTSTATUS        Status;
    KIRQL           OldIrql;
    PDATA_BLOCK     DataBlock = AllocateDataBlock();

    if (NULL == DataBlock) {
        return STATUS_UNSUCCESSFUL;
    }

    Status = LockDataBlock(DataBlock);
    if (STATUS_SUCCESS != Status) {
        DeallocateDataBlock(DataBlock);
        return STATUS_UNSUCCESSFUL;
    }

    KeAcquireSpinLock(&DriverContext.DataBlockListLock, &OldIrql);
    ASSERT((NULL != DataBlock->KernelAddress) && (NULL != DataBlock->Mdl) && 
           (NULL == DataBlock->MappedUserAddress) && (DATABLOCK_FLAGS_LOCKED == DataBlock->ulFlags) &&
           (DriverContext.ulDataBlockSize == DataBlock->ulcbDataFree));

    InsertHeadList(&DriverContext.LockedDataBlockList, &DataBlock->ListEntry);
    DriverContext.ulDataBlocksInLockedDataBlockList++;
    KeReleaseSpinLock(&DriverContext.DataBlockListLock, OldIrql);

    return STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------------------------
 * 
 * ChangeNodeCleanupWorkerRoutine : This function is used to delete change node
 * change node mutex has to be held when deleting
 *
 *--------------------------------------------------------------------------------------------
 */
VOID
ChangeNodeCleanupWorkerRoutine(PWORK_QUEUE_ENTRY   pWorkQueueEntry)
{
    PCHANGE_NODE        ChangeNode = (PCHANGE_NODE)pWorkQueueEntry->Context;

    PAGED_CODE();
    // WorkQueueEntry for the given changenode is not in use
    ASSERT(1 == ChangeNode->bCleanupWorkQueueRef);
    ChangeNode->bCleanupWorkQueueRef = 0;

    ChangeNodeCleanup(ChangeNode);
    return;
}


bool
QueueWorkerRoutineForChangeNodeCleanup(
    PCHANGE_NODE        ChangeNode
    )
{

    PWORK_QUEUE_ENTRY   pWorkItem = NULL;
    // Check the usage status of Cleanup WorkQueueEntry
    if (!ChangeNode->bCleanupWorkQueueRef) {
        pWorkItem = ChangeNode->pCleanupWorkQueueEntry;
        ChangeNode->bCleanupWorkQueueRef = 1;
        //Cleanup of the given WorkQueuEntry required for reusing
        InitializeWorkQueueEntry(pWorkItem);
    }

    if (NULL != pWorkItem) {
        pWorkItem->Context = ChangeNode;
        pWorkItem->WorkerRoutine = ChangeNodeCleanupWorkerRoutine;

        AddItemToWorkQueue(&DriverContext.WorkItemQueue, pWorkItem);        
    } else {
        InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING,
            ("%s: Failed to allocate WorkQueueEntry\n", __FUNCTION__));

        // TODO: If failed to queue worker routine, we have to atleast free the dirty
        // block, this would leave the file undeleted, but atleast we would reclaim our
        // non paged memory used for dirty block.
        return false;
    }

    return false;
}

