// this class handles the the high level interface to the persistent bitmap and change log

//
// Copyright InMage Systems 2005
//

#include "BitmapAPI.h"
#include "misc.h"

#include <bcrypt.h>

BitmapAPI::BitmapAPI(const tDevLogContext *pDevLogContext)

{
TRC
    RtlCopyMemory(&m_DevLogContext, pDevLogContext, sizeof(m_DevLogContext));
    m_eBitmapState = ecBitmapStateUnInitialized;
    KeInitializeMutex(&m_BitmapMutex, 0);

    m_ustrBitmapFilename.Buffer = m_wstrBitmapFilenameBuffer;
    m_ustrBitmapFilename.Length = 0;
    m_ustrBitmapFilename.MaximumLength = (MAX_LOG_PATHNAME + 1)*sizeof(WCHAR);
#ifdef VOLUME_FLT
    //Fix For Bug 27337
    pFsInfo = NULL;
#endif
    m_bDevInSync = FALSE;
    m_ErrorCausingOutOfSync = STATUS_SUCCESS;

    segmentedBitmap = NULL;
    m_fsBitmapStream = NULL;
    segmentMapper = NULL;
    m_BitmapHeader = NULL;
    m_iobBitmapHeader = NULL;

    m_MaxBitmapHdrWriteGroups = 0;
    m_BitMapHeaderEndOffset = 0;
}

BitmapAPI::~BitmapAPI()
{
TRC
    NT_ASSERT(NULL == segmentedBitmap);
    NT_ASSERT(NULL == m_fsBitmapStream);
    NT_ASSERT(NULL == segmentMapper);
    NT_ASSERT(NULL == m_BitmapHeader);
    NT_ASSERT(NULL == m_iobBitmapHeader);
}

NTSTATUS BitmapAPI::InitializeBitmapAPI()
{
TRC

    AsyncIORequest::InitializeMemoryLookasideList();
    IOBuffer::InitializeMemoryLookasideList();

    return STATUS_SUCCESS;
}

NTSTATUS BitmapAPI::TerminateBitmapAPI()
{
TRC

    AsyncIORequest::TerminateMemoryLookasideList();
    IOBuffer::TerminateMemoryLookasideList();

    return STATUS_SUCCESS;
}

_Requires_exclusive_lock_held_(pDevContext->BitmapOpenCloseMutex)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
BitmapAPI::Open(
    _In_ _Notnull_  PUNICODE_STRING     pBitmapFileName,
    _In_            ULONG               ulBitmapGranularity,
    _In_            ULONG               ulBitmapOffset,
    _In_            LONGLONG            mountedSize,
#if DBG
    _In_ _Notnull_  PCHAR               pcDevNum,
#endif
    _In_            ULONG               SegmentCacheLimit,
    _In_            bool                bClearOnOpen,
#ifdef VOLUME_CLUSTER_SUPPORT
    BitmapPersistentValues &BitmapPersistentValue,
#endif
    _Out_           NTSTATUS            *pInMageOpenStatus,
    _Out_           tagLogRecoveryState *pBitmapRecoveryState,
    _In_            ULONG               MaxBitmapHeaderWriteGroups)
{
TRC

    NTSTATUS    Status = STATUS_SUCCESS;
    // If bitmap file is created for the first time, llInitialBitmapFileSize is used to allocate for the file.
    // llInitialBitmapFileSize is zero if bitmap is on raw volume.
    LONGLONG    llInitialBitmapFileSize;
    // ulCreateDisposition is FILE_OPEN if bitmap is on raw volume.
    // ulCreateDisposition is FILE_OPEN_IF if bitmap is on volume with file system.
    ULONG       ulCreateDisposition;
    // ulSharedAccessToBitmap is FILE_SHARE_READ | FILE_SHARE_WRITE for bitmaps on raw volumes.
    // ulSharedAccessToBitmap is currently FILE_SHARE_DELETE for bitmaps on volume with file system, but this should be 0.
    ULONG       ulSharedAccessToBitmap;
    UNICODE_STRING namePattern;
    ULONG MaxBitmapBufferRequired = 0;
    //Fix for bug 24005. To make sure that we pass FILE_NON_DIRECTORY_FILE flag while creating/opening
    //a data file; a logical, virtual, or physical device; or a volume.
    ULONG       ulCreateOptions = 0;

    ASSERT(0 != ulBitmapGranularity);
    ASSERT(NULL != pInMageOpenStatus);

    *pInMageOpenStatus = STATUS_SUCCESS;

    // keep concurrent threads form conflicting
    KeWaitForMutexObject(&m_BitmapMutex, Executive, KernelMode, FALSE, NULL);
#if DBG
    // For printing purpose if there is no drive letter print a question mark
    if (pcDevNum[0]) {
        RtlCopyMemory(m_chDevNum, pcDevNum, DEVICE_NUM_LENGTH * sizeof(CHAR));
    } else {
        m_chDevNum[0] = L'?';
    }
#endif
    m_llDevSize = mountedSize;
    m_ulBitmapGranularity = ulBitmapGranularity;
    m_ulBitmapOffset = ulBitmapOffset;

    RtlCopyUnicodeString(&m_ustrBitmapFilename, pBitmapFileName);

    m_ulNumberOfBitsInBitmap = (ULONG)((m_llDevSize / (LONGLONG)m_ulBitmapGranularity) + 1);

    // calculate the size of a bitmap log file
    m_ulBitMapSizeInBytes = ((m_ulNumberOfBitsInBitmap + 7) / 8); // round up to full byte
    if ((m_ulBitMapSizeInBytes % BITMAP_FILE_SEGMENT_SIZE) != 0) { // adjust to full buffer
        m_ulBitMapSizeInBytes += BITMAP_FILE_SEGMENT_SIZE - (m_ulBitMapSizeInBytes % BITMAP_FILE_SEGMENT_SIZE); 
    }
    m_MaxBitmapHdrWriteGroups = MaxBitmapHeaderWriteGroups;
    m_BitMapHeaderEndOffset = LOG_HEADER_SIZE(MaxBitmapHeaderWriteGroups);
    m_ulBitMapSizeInBytes += m_BitMapHeaderEndOffset;

    MaxBitmapBufferRequired = min(SegmentCacheLimit * BITMAP_FILE_SEGMENT_SIZE, m_ulBitMapSizeInBytes);
    m_SegmentCacheLimit = SegmentCacheLimit;

    if ((DriverContext.CurrentBitmapBufferMemory + MaxBitmapBufferRequired) > DriverContext.MaximumBitmapBufferMemory) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, 
            ("BitmapAPI::Open - Failed for device %s, memory required(%#x) for bitmap exceeds global set limit %#x\n",
                    m_chDevNum, m_ulBitMapSizeInBytes, DriverContext.MaximumBitmapBufferMemory));
        SET_INMAGE_BITMAP_API_STATUS(pInMageOpenStatus, INDSKFLT_ERROR_BITMAP_FILE_EXCEEDED_MEMORY_LIMIT);
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Cleanup_And_Return_Failure;
    }

    // check if this is a volume or file
    RtlInitUnicodeString(&namePattern, DOS_DEVICES_VOLUME_PREFIX);
    if ((RtlPrefixUnicodeString(&namePattern, pBitmapFileName, TRUE) == TRUE) &&
        (pBitmapFileName->Length == 56*sizeof(WCHAR))) 
    {
        // log is a raw volume
        InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, ("BitmapAPI::Open - Using raw device %wZ as bitmap file for device %s\n",
                                    pBitmapFileName, m_chDevNum));
        llInitialBitmapFileSize = 0;
        ulCreateDisposition = FILE_OPEN;    // Fail if file already does not exist
        ulSharedAccessToBitmap = FILE_SHARE_READ | FILE_SHARE_WRITE;
        //Fix for bug 24005 - Since it's a raw volume open we don't really care about other CreateOption flags.
        ulCreateOptions = FILE_NO_INTERMEDIATE_BUFFERING;
    } else {
        // log is a file
        InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, ("BitmapAPI::Open - Using file %wZ as bitmap file for device %s\n",
                                pBitmapFileName, m_chDevNum));
        // Set the m_ulBitmapOffset to zero, the bitmap is a file on volume with file system. offset is only used for
        // bitmaps on raw volumes, this will help us to save multiple bitmaps on one raw volume.
        m_ulBitmapOffset = 0;
        llInitialBitmapFileSize = m_ulBitMapSizeInBytes;

        // TODO-SanKumar-1805: If the file is already present, check it if it's verified against
        // the file size. Otherwise, it may be 0'd out on open due to the allocation size. The right
        // thing would be to use 2 different dispositions if the disk is protected / disk is to be
        // protected (load/open) = (open/create)
        ulCreateDisposition = FILE_OPEN_IF; // Create new file, if file already does not exist.

        // TODO: This has to be changed to zero, and delete code has to be modified to delete on close of existing handle.
        ulSharedAccessToBitmap = 0;//FILE_SHARE_DELETE;
        //Fix for bug 24005 
        ulCreateOptions = FILE_NON_DIRECTORY_FILE | FILE_NO_INTERMEDIATE_BUFFERING;
    }

    m_fsBitmapStream = new FileStream;
    InVolDbgPrint(DL_INFO, DM_MEM_TRACE, ("BitmapAPI::Open - Allocation, m_fsBitmapStream = %p\n", m_fsBitmapStream));
    if (NULL == m_fsBitmapStream) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("BitmapAPI::Open - m_fsBitmapStream allocation failed\n"));
        SET_INMAGE_BITMAP_API_STATUS(pInMageOpenStatus, INDSKFLT_ERROR_NO_MEMORY, SourceLocationOpenBitmapFSNew);
        Status = STATUS_NO_MEMORY;
        goto Cleanup_And_Return_Failure;
    }

    //Fix for bug 24005
    Status = m_fsBitmapStream->Open(pBitmapFileName, FILE_READ_DATA | FILE_WRITE_DATA | DELETE,
            ulCreateDisposition, ulCreateOptions, ulSharedAccessToBitmap, llInitialBitmapFileSize);
    if (!NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("BitmapAPI::Open - Unable to open volume bitmap %wZ - Status = %#x\n",
                                        pBitmapFileName, Status));
        SET_INMAGE_BITMAP_API_STATUS(pInMageOpenStatus, INDSKFLT_ERROR_BITMAP_FILE_CANT_OPEN);
        goto Cleanup_And_Return_Failure;
    }

    m_eBitmapState = ecBitmapStateOpened;

#ifdef VOLUME_CLUSTER_SUPPORT
    Status = LoadBitmapHeaderFromFileStream(pInMageOpenStatus, pBitmapRecoveryState, bClearOnOpen, BitmapPersistentValue); 
#else
    Status = LoadBitmapHeaderFromFileStream(pInMageOpenStatus, pBitmapRecoveryState, bClearOnOpen);
#endif

    if (!NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, 
            ("BitmapAPI::Open - LoadBitmapHeaderFromFileStream failed with Status = %#x, InMageStatus = %#x\n",
                    Status, *pInMageOpenStatus));
        goto Cleanup_And_Return_Failure;
    }

    // create the segment mapper on the file
    segmentMapper = new FileStreamSegmentMapper;
    InVolDbgPrint(DL_INFO, DM_MEM_TRACE, ("BitmapAPI::Open - Allocation, segmentMapper = %#p\n", segmentMapper));
    if (NULL == segmentMapper) {
        Status = STATUS_NO_MEMORY;
        SET_INMAGE_BITMAP_API_STATUS(pInMageOpenStatus, INDSKFLT_ERROR_NO_MEMORY, SourceLocationOpenBitmapSegMapperNew);
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("BitmapAPI::Open - Allocation of FileStreamSegmentMapper failed\n"));
        goto Cleanup_And_Return_Failure;
    }
	Status = segmentMapper->Attach(m_fsBitmapStream, m_ulBitmapOffset + m_BitMapHeaderEndOffset, (m_ulNumberOfBitsInBitmap / 8) + 1, SegmentCacheLimit);
    if (!NT_SUCCESS(Status)) {
        if (STATUS_NO_MEMORY == Status)
            SET_INMAGE_BITMAP_API_STATUS(pInMageOpenStatus, INDSKFLT_ERROR_NO_MEMORY, SourceLocationOpenBitmapSegMapperAtt);
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("BitmapAPI::Open - Attach of FileStreamSegmentMapper failed\n"));
        goto Cleanup_And_Return_Failure;
    }

    // layer the SegmentedBitmap over the FileStreamSegmentMapper
    segmentedBitmap = new SegmentedBitmap(segmentMapper, m_ulNumberOfBitsInBitmap);
    InVolDbgPrint(DL_INFO, DM_MEM_TRACE, ("BitmapAPI::Open - Allocation, segmentedBitmap = %#p\n", segmentedBitmap));
    if (NULL == segmentedBitmap) {
        Status = STATUS_NO_MEMORY;
        SET_INMAGE_BITMAP_API_STATUS(pInMageOpenStatus, INDSKFLT_ERROR_NO_MEMORY, SourceLocationOpenBitmapSegBitmapNew);
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("BitmapAPI::Open - Allocation of SegmentedBitmap failed\n"));
        goto Cleanup_And_Return_Failure;
    }

    Status = MoveRawIOChangesToBitmap(pInMageOpenStatus);
    if (!NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, 
            ("BitmapAPI::Open - MoveRawIOChangesToBitmap failed with Status = %#x, InMageStatus = %#x\n",
                    Status, *pInMageOpenStatus));
        goto Cleanup_And_Return_Failure;
    }

    DriverContext.CurrentBitmapBufferMemory += MaxBitmapBufferRequired;
    KeReleaseMutex(&m_BitmapMutex, FALSE);
    return STATUS_SUCCESS;

Cleanup_And_Return_Failure:
    if (NULL != segmentedBitmap) {
        segmentedBitmap->Release();
        segmentedBitmap = NULL;
    }

    if (NULL != segmentMapper) {
        segmentMapper->Release();
        segmentMapper = NULL;
    }

    if (NULL != m_iobBitmapHeader) {
        m_iobBitmapHeader->Release();
        m_iobBitmapHeader = NULL;
    }

    if (NULL != m_fsBitmapStream) {
        m_fsBitmapStream->Close();
        m_fsBitmapStream->Release();
        m_fsBitmapStream = NULL;
    }

    if (NULL != m_BitmapHeader) {
        ExFreePoolWithTag(m_BitmapHeader, TAG_BITMAP_HEADER);
        m_BitmapHeader = NULL;
    }

    if (*pInMageOpenStatus != MSG_INDSKFLT_ERROR_BITMAP_FILE_CANT_OPEN_Message)
        InDskFltWriteEvent(INDSKFLT_WARN_BITMAPSTATE6, Status, *pInMageOpenStatus);

    m_eBitmapState = ecBitmapStateUnInitialized;
    KeReleaseMutex(&m_BitmapMutex, FALSE);
    return Status;
}

NTSTATUS BitmapAPI::IsDevInSync(BOOLEAN *pDevInSync, NTSTATUS *pOutOfSyncErrorCode)
{
    NTSTATUS    Status = STATUS_SUCCESS;

    if (NULL == pDevInSync)
        return STATUS_INVALID_PARAMETER;

    // keep concurrent threads form conflicting
    KeWaitForMutexObject(&m_BitmapMutex, Executive, KernelMode, FALSE, NULL);
    
    switch(m_eBitmapState) {
    case ecBitmapStateOpened:
    case ecBitmapStateRawIO:
        *pDevInSync = m_bDevInSync;
        *pOutOfSyncErrorCode = m_ErrorCausingOutOfSync;
        InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, ("IsDevInSync: returning %s for volumesync with error %#x\n",
            m_bDevInSync ? "TRUE" : "FALSE", m_ErrorCausingOutOfSync));
        break;
    default:
        ASSERTMSG("IsDevInSync Called in non-opened state\n", FALSE);
        Status = STATUS_INVALID_PARAMETER;
        break;
    }

    KeReleaseMutex(&m_BitmapMutex, FALSE);

    return Status;
}

BOOLEAN  BitmapAPI::IsBitmapClosed()
{
    BOOLEAN bReturn;
    // keep concurrent threads form conflicting
    KeWaitForMutexObject(&m_BitmapMutex, Executive, KernelMode, FALSE, NULL);
    
    switch(m_eBitmapState) {
    case ecBitmapStateOpened:
    case ecBitmapStateRawIO:
        bReturn = FALSE;
        break;
    default:
        bReturn = TRUE;
        break;
    }

    KeReleaseMutex(&m_BitmapMutex, FALSE);

    return bReturn;
}


NTSTATUS BitmapAPI::Close(
    tagLogRecoveryState recoveryState,
#ifdef VOLUME_CLUSTER_SUPPORT
    BitmapPersistentValues &BitmapPersistentValue,
#endif
    bool bDelete,
    NTSTATUS *pInMageCloseStatus,
    ULONG resyncerrorcode,
    ULONGLONG ResyncTimeStamp)
{
TRC
    NTSTATUS Status = STATUS_SUCCESS;


    // keep concurrent threads form conflicting
    KeWaitForMutexObject(&m_BitmapMutex, Executive, KernelMode, FALSE, NULL);
    switch(m_eBitmapState) {
    case ecBitmapStateOpened:
    case ecBitmapStateRawIO:
#ifdef VOLUME_CLUSTER_SUPPORT
        Status = CommitBitmapInternal(recoveryState, pInMageCloseStatus, resyncerrorcode, ResyncTimeStamp,BitmapPersistentValue);
#else
        Status = CommitBitmapInternal(recoveryState, pInMageCloseStatus,resyncerrorcode, ResyncTimeStamp);
#endif	
        break;
    default:
        ASSERTMSG("IsDevInSync Called in non-opened state\n", FALSE);
        Status = STATUS_INVALID_PARAMETER;
        break;
    }

    if (NULL != m_iobBitmapHeader) {
        m_iobBitmapHeader->Release();
        m_iobBitmapHeader = NULL;
    }

    if (segmentedBitmap) {
        segmentedBitmap->Release();
        segmentedBitmap = NULL;
    }

    if (segmentMapper) {
        segmentMapper->Release();
        segmentMapper = NULL;
    }

    // this can be called at shutdown or device removal
    if (m_fsBitmapStream) {
        if (bDelete) {
            m_fsBitmapStream->DeleteOnClose();
        }
        Status = m_fsBitmapStream->Close();
        m_fsBitmapStream->Release();
        m_fsBitmapStream = NULL;
    }

    DriverContext.CurrentBitmapBufferMemory -= min(m_SegmentCacheLimit * BITMAP_FILE_SEGMENT_SIZE, m_ulBitMapSizeInBytes);
    m_eBitmapState = ecBitmapStateClosed;

    if (NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE, ("BitmapAPI::Close - Clean shutdown marker written for device %s\n", 
            m_chDevNum));
    } else {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE, ("BitmapAPI::Close - Error writing clean shutdown marker for device %s, Status = %#x\n", 
            m_chDevNum, Status));
    }

    if (NULL != m_BitmapHeader) {
        ExFreePoolWithTag(m_BitmapHeader, TAG_BITMAP_HEADER);
        m_BitmapHeader = NULL;
    }

    KeReleaseMutex(&m_BitmapMutex, FALSE);
    return Status;
}

// Returns Scaled Size and updates Scaled Offset parameter.
__inline void BitmapAPI::GetScaledOffsetAndSizeFromDiskChange(LONGLONG llOffset, ULONG ulLength, ULONGLONG *pullScaledOffset, ULONGLONG *pullScaledSize)
{
    *pullScaledOffset = (llOffset / m_ulBitmapGranularity) * m_ulBitmapGranularity; // round down offset
    *pullScaledSize = llOffset - *pullScaledOffset; // 1st, calculate how much size grew from rounding down
    *pullScaledSize += ulLength + (m_ulBitmapGranularity - 1); // 2nd, add in the actual size plus rounding factor
    *pullScaledSize /= m_ulBitmapGranularity; // 3rd, now scale it to the granularity
    *pullScaledOffset /= m_ulBitmapGranularity;

    return;
}

// Returns Scaled Size and updates Scaled Offset parameter.
__inline void BitmapAPI::GetScaledOffsetAndSizeFromWriteMetadata(PWRITE_META_DATA pWriteMetaData, ULONGLONG *pullScaledOffset, ULONGLONG *pullScaledSize)
{
    *pullScaledOffset = (pWriteMetaData->llOffset / m_ulBitmapGranularity) * m_ulBitmapGranularity; // round down offset
    *pullScaledSize = pWriteMetaData->llOffset - *pullScaledOffset; // 1st, calculate how much size grew from rounding down
    *pullScaledSize += pWriteMetaData->ulLength + (m_ulBitmapGranularity - 1); // 2nd, add in the actual size plus rounding factor
    *pullScaledSize /= m_ulBitmapGranularity; // 3rd, now scale it to the granularity
    *pullScaledOffset /= m_ulBitmapGranularity;

    return;
}

__inline NTSTATUS BitmapAPI::SetWriteSizeNotToExceedDevSize(LONGLONG &llOffset, ULONG &ulLength)
{
    if ((ulLength + llOffset) > m_llDevSize) {
        if (llOffset < m_llDevSize) {
            InVolDbgPrint(DL_INFO, DM_BITMAP_READ, ("Correcting Length from %#x to %#x\n",
                ulLength, (ULONG32)(m_llDevSize - llOffset)));
            ulLength = (ULONG32)(m_llDevSize - llOffset);
        } else {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_READ, ("Offset %#I64x is beyond volume size %#I64x\n",
                llOffset, m_llDevSize));
            return STATUS_ARRAY_BOUNDS_EXCEEDED;
        }
    }

    return STATUS_SUCCESS;
}

#ifdef VOLUME_FLT
  //Fix For Bug 27337
__inline NTSTATUS BitmapAPI::SetUserReadSizeNotToExceedFsSize(LONGLONG &llOffset, ULONG &ulLength)
{
    NTSTATUS Status = STATUS_SUCCESS;//Always
    ULONG64  TotalReadLength = 0;

    //For any error condition let's simply return Success and poceed as if
    //we never had called this function. By doing this the current logic i.e resync.
    //wouldn't be affected.
	if(NULL == pFsInfo){return Status;}
	if( (0 >= pFsInfo->MaxVolumeByteOffsetForFs)){return Status;}
	if(m_ulBitmapGranularity <= pFsInfo->SizeOfFsBlockInBytes){return Status;}

	TotalReadLength = ulLength + llOffset;

	if (TotalReadLength > pFsInfo->MaxVolumeByteOffsetForFs) {

		InVolDbgPrint(DL_INFO, DM_BITMAP_READ, ("SetUserReadSizeNotToExceedFsSize::\
		Correcting Offset Length from %#x to %#x\n", ulLength, \
		(ULONG)(pFsInfo->MaxVolumeByteOffsetForFs - llOffset) ));
		
		ulLength = (ULONG)(pFsInfo->MaxVolumeByteOffsetForFs - llOffset);
	}
	
    return Status;
}
#endif

NTSTATUS BitmapAPI::SetBitRun(BitRuns * bitRuns, LONGLONG runOffsetArray, ULONG runLengthArray)
{
TRC
    NTSTATUS    Status = STATUS_SUCCESS;
    ULONGLONG   ullScaledOffset, ullScaledSize;
    
    ASSERTMSG("BitmapAPI::SetBits Offset is beyond end of volume\n", (runOffsetArray < m_llDevSize));
    ASSERTMSG("BitmapAPI::SetBits Offset is negative\n", (runOffsetArray >= 0));
    ASSERTMSG("BitmapAPI::SetBits detected bit run past volume end\n",
        ((runOffsetArray + runLengthArray) <= m_llDevSize));

    GetScaledOffsetAndSizeFromDiskChange(runOffsetArray, runLengthArray, &ullScaledOffset, &ullScaledSize);

    // verify the data is within bounds
    if ((runOffsetArray < m_llDevSize) && (runOffsetArray >= 0) &&
        ((runOffsetArray + runLengthArray) <= m_llDevSize))
    {
        bitRuns->finalStatus = segmentedBitmap->SetBitRun((ULONG)ullScaledSize, ullScaledOffset);
        Status = bitRuns->finalStatus;
        if (NT_SUCCESS(bitRuns->finalStatus)) {
            bitRuns->nbrRunsProcessed++;
        }
    }

    return Status;
}

NTSTATUS BitmapAPI::SetBits(BitRuns * bitRuns)
{
TRC
    NTSTATUS    Status = STATUS_SUCCESS;
    ULONG       ulCurrentRun;
    ULONGLONG   ullScaledOffset, ullScaledSize;

    PKDIRTY_BLOCK DirtyBlock = bitRuns->pdirtyBlock;
    PCHANGE_BLOCK   ChangeBlock = DirtyBlock->ChangeBlockList;
    LONGLONG runOffset = 0;
    ULONG runLength = 0;    
    ULONG ulElements = 0;
    LONG index = -1;

    if (bitRuns->nbrRuns > MAX_CHANGES_EMB_IN_DB) {
        ASSERT (NULL != ChangeBlock);
    }
    
    if (NULL != ChangeBlock) {
        ulElements = ChangeBlock->usLastIndex - ChangeBlock->usFirstIndex + 1;
    }
    
    bitRuns->nbrRunsProcessed = 0;
    bitRuns->finalStatus = STATUS_SUCCESS;

    // keep concurrent threads form conflicting
    KeWaitForMutexObject(&m_BitmapMutex, Executive, KernelMode, FALSE, NULL);

    switch(m_eBitmapState) {
    case ecBitmapStateOpened:
        // initially let's just run this synchronously
        
        // Read bitruns from DirtyBlock upto MAX_CHANGES_EMB_IN_DB, then read bitruns from ChangeBlock(s)
        for (ulCurrentRun = 0; ulCurrentRun < bitRuns->nbrRuns; ulCurrentRun++) {
            if (ulCurrentRun < MAX_CHANGES_EMB_IN_DB) {
                Status = SetBitRun(bitRuns, DirtyBlock->ChangeOffset[ulCurrentRun], DirtyBlock->ChangeLength[ulCurrentRun]);
            } else {
                index++;
                if ((ULONG)index == ulElements) {
                    ChangeBlock = ChangeBlock->Next;
                    ASSERT (NULL != ChangeBlock);
                    ulElements = ChangeBlock->usLastIndex - ChangeBlock->usFirstIndex + 1;
                    index = 0;
                }
                Status = SetBitRun(bitRuns, ChangeBlock->ChangeOffset[index], ChangeBlock->ChangeLength[index]);
            }
            if (! NT_SUCCESS(Status)) {
                break; // end if we get an error
            }                
        }

        if (NT_SUCCESS(Status)) {
            ASSERT(ulCurrentRun == bitRuns->nbrRuns);
        }
        
//        segmentedBitmap->SyncFlushAll(); // make sure all the changes are saved
        break;

    case ecBitmapStateRawIO:
        // do last chance processing on runs

        // Read bitruns from DirtyBlock upto MAX_CHANGES_EMB_IN_DB, then read bitruns from ChangeBlock(s)
        for (ulCurrentRun = 0; ulCurrentRun < bitRuns->nbrRuns; ulCurrentRun++) {

            if (ulCurrentRun < MAX_CHANGES_EMB_IN_DB) {
                runOffset = DirtyBlock->ChangeOffset[ulCurrentRun];
                runLength = DirtyBlock->ChangeLength[ulCurrentRun];
            } else {
                index++;
                if ((ULONG)index == ulElements) {
                    ChangeBlock = ChangeBlock->Next;
                    ASSERT (NULL != ChangeBlock);
                    ulElements = ChangeBlock->usLastIndex - ChangeBlock->usFirstIndex + 1;
                    index = 0;
                }
                runOffset = ChangeBlock->ChangeOffset[index];
                runLength = ChangeBlock->ChangeLength[index];
            }


            ASSERTMSG("BitmapAPI::SetBits Offset is beyond end of volume\n",
                (runOffset < m_llDevSize));
            ASSERTMSG("BitmapAPI::SetBits Offset is negative\n",
                (runOffset >= 0));
            ASSERTMSG("BitmapAPI::SetBits detected bit run past volume end\n",
                ((runOffset + runLength) <= m_llDevSize));

            InVolDbgPrint(DL_INFO, DM_BITMAP_WRITE, ("SetBits: Writing using RawIO, Offset = %#I64x Length = %#x\n",
                        runOffset, runLength));

            if (m_BitmapHeader->header.lastChanceChanges == (m_MaxBitmapHdrWriteGroups * MAX_CHANGES_IN_WRITE_GROUP)) {
                m_BitmapHeader->header.changesLost += bitRuns->nbrRuns - ulCurrentRun;
                InVolDbgPrint(DL_WARNING, DM_BITMAP_WRITE, ("SetBits: Incremented lost changes to %#x\n",
                            m_BitmapHeader->header.changesLost));
                break;
            }

            GetScaledOffsetAndSizeFromDiskChange(runOffset, runLength, &ullScaledOffset, &ullScaledSize);

            // if the run is very large we have to chop it up into smaller pieces
            while ((ullScaledSize > 0) && 
                (m_BitmapHeader->header.lastChanceChanges < (m_MaxBitmapHdrWriteGroups * MAX_CHANGES_IN_WRITE_GROUP)))
            {
                // Required for 64 bit shift operations
                m_BitmapHeader->changeGroup[m_BitmapHeader->header.lastChanceChanges / 
                                        MAX_CHANGES_IN_WRITE_GROUP].lengthOffsetPair[m_BitmapHeader->header.lastChanceChanges % 
                                                                                MAX_CHANGES_IN_WRITE_GROUP] =
                        (min(ullScaledSize,0xFFFF) << 48) | (ullScaledOffset & 0xFFFFFFFFFFFF);

                InVolDbgPrint(DL_INFO, DM_BITMAP_WRITE, ("SetBits: WriteRaw: ScaledOffset = %#I64x, ScaledSize = %#I64x, Granularity = %#x\n",
                            (ullScaledOffset & 0xFFFFFFFFFFFF), min(ullScaledSize, 0xFFFF), m_ulBitmapGranularity));
                ullScaledOffset += min(ullScaledSize,0xFFFF);
                ullScaledSize -= min(ullScaledSize,0xFFFF);
                m_BitmapHeader->header.lastChanceChanges++;
            }
            /*DTAP-Review  and test
              Currently Max size of Dirty block in Metadata mode is 64MB. 
              FFFF is 256MB for 4k granularity
              Writen sample code for further reference
            if ((ullScaledSize > 0)) {
                m_BitmapHeader.header.changesLost += bitRuns->nbrRuns - ulCurrentRun;
                DbgPrint("Bitmap error ullScaledSize %I64X C %I64X\n", ullScaledSize, m_BitmapHeader.header.lastChanceChanges );
                InVolDbgPrint(DL_WARNING, DM_BITMAP_WRITE, ("SetBits: Incremented lost changes to %#x\n",
                            m_BitmapHeader.header.changesLost));
                break;
            }*/

            bitRuns->nbrRunsProcessed++;
        }

        bitRuns->finalStatus = STATUS_SUCCESS; // not much we can do if this fails
        break;

    default:
        Status = STATUS_DEVICE_NOT_READY;
        bitRuns->finalStatus = Status;
        break;
    }

    KeReleaseMutex(&m_BitmapMutex, FALSE);

    if (bitRuns->completionCallback) {
        bitRuns->completionCallback(bitRuns);
    }

    return Status;
}

NTSTATUS BitmapAPI::ClearBits(BitRuns * bitRuns)
{
TRC
    NTSTATUS    Status = STATUS_SUCCESS;
    ULONG       ulCurrentRun;
    ULONGLONG   ullScaledOffset, ullScaledSize;

    bitRuns->nbrRunsProcessed = 0;
    bitRuns->finalStatus = STATUS_SUCCESS;

    // keep concurrent threads form conflicting
    KeWaitForMutexObject(&m_BitmapMutex, Executive, KernelMode, FALSE, NULL);

    switch(m_eBitmapState) {
    case ecBitmapStateOpened:
        // initially let's just run this synchronously
        for (ulCurrentRun = 0; ulCurrentRun < bitRuns->nbrRuns; ulCurrentRun++) {
            ASSERTMSG("BitmapAPI::SetBits Offset is beyond end of volume\n",
                (bitRuns->runOffsetArray[ulCurrentRun] < m_llDevSize));
            ASSERTMSG("BitmapAPI::SetBits Offset is negative\n",
                (bitRuns->runOffsetArray[ulCurrentRun] >= 0));
            ASSERTMSG("BitmapAPI::SetBits detected bit run past volume end\n",
                ((bitRuns->runOffsetArray[ulCurrentRun] + bitRuns->runLengthArray[ulCurrentRun]) <= m_llDevSize));

            GetScaledOffsetAndSizeFromDiskChange(bitRuns->runOffsetArray[ulCurrentRun], bitRuns->runLengthArray[ulCurrentRun], &ullScaledOffset, &ullScaledSize);

            // prefetch goes here
            bitRuns->finalStatus = segmentedBitmap->ClearBitRun((ULONG)ullScaledSize, ullScaledOffset);
            Status = bitRuns->finalStatus;
            if (NT_SUCCESS(Status)) {
                bitRuns->nbrRunsProcessed++;
            } else {
                InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("BitampAPI::ClearBits - Failed with status %#x\n", bitRuns->finalStatus));
                break; // end if we get an error
            }
        }
        
        if (0 != bitRuns->nbrRunsProcessed)
            segmentedBitmap->SyncFlushAll(); // make sure all the changes are saved
        break;
    default:
        Status = STATUS_DEVICE_NOT_READY;
        bitRuns->finalStatus = Status;
        break;
    }

    KeReleaseMutex(&m_BitmapMutex, FALSE);

    if (bitRuns->completionCallback) {
        bitRuns->completionCallback(bitRuns);
    }

    return Status;
}

NTSTATUS BitmapAPI::GetFirstRuns(BitRuns * bitRuns)
{
TRC
    NTSTATUS    Status = STATUS_SUCCESS;
    ULONG       ulCurrentRun = 0;

    bitRuns->nbrRunsProcessed = 0;
    bitRuns->nbrRuns = 0;
    bitRuns->finalStatus = STATUS_SUCCESS;

    // keep concurrent threads form conflicting
    KeWaitForMutexObject(&m_BitmapMutex, Executive, KernelMode, FALSE, NULL);

    switch(m_eBitmapState) {
    case ecBitmapStateOpened:
        // get first run
        Status = segmentedBitmap->GetFirstBitRun((ULONG32 *)&(bitRuns->runLengthArray[ulCurrentRun]),
                                                 (ULONG64 *)&(bitRuns->runOffsetArray[ulCurrentRun]));
        bitRuns->runLengthArray[ulCurrentRun] *= m_ulBitmapGranularity; // apply granularity
        bitRuns->runOffsetArray[ulCurrentRun] *= m_ulBitmapGranularity; // apply granularity

        if (NT_SUCCESS(Status))
            Status = SetWriteSizeNotToExceedDevSize(bitRuns->runOffsetArray[ulCurrentRun], bitRuns->runLengthArray[ulCurrentRun]);
#ifdef VOLUME_FLT
		//Fix For Bug 27337
		if (NT_SUCCESS(Status)){
			Status = SetUserReadSizeNotToExceedFsSize(bitRuns->runOffsetArray[ulCurrentRun],
					bitRuns->runLengthArray[ulCurrentRun]);
		}
#endif
        if (NT_SUCCESS(Status)) {
            ulCurrentRun++;
            bitRuns->nbrRunsProcessed++;
            bitRuns->nbrRuns++;

            // process some more runs
            while (NT_SUCCESS(Status) && (ulCurrentRun < MAX_BITRUN_CHANGES)) {
                Status = segmentedBitmap->GetNextBitRun((ULONG32 *)&(bitRuns->runLengthArray[ulCurrentRun]),
                                                        (ULONG64 *)&(bitRuns->runOffsetArray[ulCurrentRun]));
                (bitRuns->runLengthArray[ulCurrentRun]) *= m_ulBitmapGranularity; // apply granularity
                (bitRuns->runOffsetArray[ulCurrentRun]) *= m_ulBitmapGranularity; // apply granularity

                if (NT_SUCCESS(Status))
                    Status = SetWriteSizeNotToExceedDevSize(bitRuns->runOffsetArray[ulCurrentRun], bitRuns->runLengthArray[ulCurrentRun]);
#ifdef VOLUME_FLT
				//Fix For Bug 27337
				if (NT_SUCCESS(Status)){
					Status = SetUserReadSizeNotToExceedFsSize(bitRuns->runOffsetArray[ulCurrentRun],
					bitRuns->runLengthArray[ulCurrentRun]);
				}
#endif
                if (NT_SUCCESS(Status)) {
                    // possibly coalesce entries that crossed bitmap pages
                    if ((ulCurrentRun > 0) &&
                        ((bitRuns->runOffsetArray[ulCurrentRun  - 1] + bitRuns->runLengthArray[ulCurrentRun - 1]) ==
                        (bitRuns->runOffsetArray[ulCurrentRun])) &&
                        (bitRuns->runLengthArray[ulCurrentRun - 1] < 0x1000000) &&
                        (bitRuns->runLengthArray[ulCurrentRun] < 0x1000000))
                    {
                        // don't merge if already large size, prevents int32 overflow
                        bitRuns->runLengthArray[ulCurrentRun - 1] += bitRuns->runLengthArray[ulCurrentRun];
                    } else {
                        bitRuns->nbrRunsProcessed++;
                        bitRuns->nbrRuns++;
                        ulCurrentRun++;
                     }
                } 
            }
        }
        if (NT_SUCCESS(Status) && (ulCurrentRun == MAX_BITRUN_CHANGES)) {
            Status = STATUS_MORE_PROCESSING_REQUIRED;
        } else if (Status == STATUS_ARRAY_BOUNDS_EXCEEDED) {
            // no more runs
            Status = STATUS_SUCCESS;
        }
        break;
    default:
        InDskFltWriteEvent(INDSKFLT_WARN_BITMAPSTATE2, m_eBitmapState);
        Status = STATUS_DEVICE_NOT_READY;
        break;
    }

    KeReleaseMutex(&m_BitmapMutex, FALSE);

    bitRuns->finalStatus = Status;
    if (bitRuns->completionCallback) {
        bitRuns->completionCallback(bitRuns);
    }

    return Status;
}
 
NTSTATUS BitmapAPI::GetNextRuns(BitRuns * bitRuns)
{
TRC
    NTSTATUS    Status = STATUS_SUCCESS;
    ULONG       ulCurrentRun = 0;

    bitRuns->nbrRunsProcessed = 0;
    bitRuns->nbrRuns = 0;
    bitRuns->finalStatus = STATUS_SUCCESS;

    // keep concurrent threads form conflicting
    KeWaitForMutexObject(&m_BitmapMutex, Executive, KernelMode, FALSE, NULL);

    switch(m_eBitmapState) {
    case ecBitmapStateOpened:
        while (NT_SUCCESS(Status) && (ulCurrentRun < MAX_BITRUN_CHANGES)) {
            Status = segmentedBitmap->GetNextBitRun((ULONG32 *)&(bitRuns->runLengthArray[ulCurrentRun]),
                                                    (ULONG64 *)&(bitRuns->runOffsetArray[ulCurrentRun]));
            bitRuns->runLengthArray[ulCurrentRun] *= m_ulBitmapGranularity; // apply granularity
            bitRuns->runOffsetArray[ulCurrentRun] *= m_ulBitmapGranularity; // apply granularity
            
            if (NT_SUCCESS(Status))
                Status = SetWriteSizeNotToExceedDevSize(bitRuns->runOffsetArray[ulCurrentRun], bitRuns->runLengthArray[ulCurrentRun]);
#ifdef VOLUME_FLT
			//Fix For Bug 27337
		if (NT_SUCCESS(Status)){
			Status = SetUserReadSizeNotToExceedFsSize(bitRuns->runOffsetArray[ulCurrentRun],
					 bitRuns->runLengthArray[ulCurrentRun]);
		}
#endif
            if (NT_SUCCESS(Status)) {
                // possibly coalesce entries that crossed bitmap pages
                if ((ulCurrentRun > 0) &&
                    ((bitRuns->runOffsetArray[ulCurrentRun  - 1] + bitRuns->runLengthArray[ulCurrentRun - 1]) ==
                    bitRuns->runOffsetArray[ulCurrentRun]) &&
                    (bitRuns->runLengthArray[ulCurrentRun - 1] < 0x1000000) &&
                    (bitRuns->runLengthArray[ulCurrentRun] < 0x1000000)) 
                {
                    // don't merge if already large size, prevents int32 overflow
                    bitRuns->runLengthArray[ulCurrentRun - 1] += bitRuns->runLengthArray[ulCurrentRun];
                } else {
                    bitRuns->nbrRunsProcessed++;
                    bitRuns->nbrRuns++;
                    ulCurrentRun++;
                }
            }
            if (NT_SUCCESS(Status) && (ulCurrentRun == MAX_BITRUN_CHANGES)) {
                Status = STATUS_MORE_PROCESSING_REQUIRED;
            } else if (Status == STATUS_ARRAY_BOUNDS_EXCEEDED) {
                // no more runs
                Status = STATUS_SUCCESS;
                break;
            }
        }
        break;
    default:
        InDskFltWriteEvent(INDSKFLT_WARN_BITMAPSTATE3, m_eBitmapState);
        Status = STATUS_DEVICE_NOT_READY;
        break;
    }

    KeReleaseMutex(&m_BitmapMutex, FALSE);

    bitRuns->finalStatus = Status;
    if (bitRuns->completionCallback) {
        bitRuns->completionCallback(bitRuns);
    }

    return Status;
}

NTSTATUS BitmapAPI::ClearAllBits()
{
TRC
    NTSTATUS    Status = STATUS_SUCCESS;

    // keep concurrent threads form conflicting
    KeWaitForMutexObject(&m_BitmapMutex, Executive, KernelMode, FALSE, NULL);

    switch(m_eBitmapState) {
    case ecBitmapStateOpened:
        // initially let's just run this synchronously
        Status = segmentedBitmap->ClearAllBits();
        segmentedBitmap->SyncFlushAll(); // make sure all the changes are saved
        break;
    default:
        Status = STATUS_DEVICE_NOT_READY;
        break;
    }

    KeReleaseMutex(&m_BitmapMutex, FALSE);

    return Status;
}

NTSTATUS BitmapAPI::DeleteLog()
{
TRC
    NTSTATUS Status;
    FileStream * fileStream;

    fileStream = new FileStream();
    if (NULL == fileStream) {
        return STATUS_NO_MEMORY;
    }

    if (0 == m_ustrBitmapFilename.Length) {
        Status = MSG_INDSKFLT_ERROR_DELETE_BITMAP_FILE_NO_NAME_Message;
    } else {
        //Fix for bug 24005 - Eventhough this function DeleteLog() is not called at this time
        //let's make the call to open file consistent by passing FILE_NON_DIRECTORY_FILE flag.
        Status = fileStream->Open(&m_ustrBitmapFilename,
                                DELETE,
                                FILE_OPEN,
                                FILE_NON_DIRECTORY_FILE | FILE_NO_INTERMEDIATE_BUFFERING,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                0);

        if (NT_SUCCESS(Status)) {
            Status = fileStream->DeleteOnClose();
            fileStream->Close(); // ignore return Status
        }
    }

    fileStream->Release();

    if (!NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("BitmapAPI::DeleteLog failed with Status = %#x\n", Status));
    } 

    return Status;
}

bool BitmapAPI::VerifyHeader(tBitmapHeader * header, bool bUpdateOnUpgrade)
{
TRC
    unsigned char actualChecksum[16];
    MD5Context ctx;
    bool    bVerified = false;

    // recalculate the checksum
    MD5Init(&ctx);
    MD5Update(&ctx, (PUCHAR)(&(header->header.endian)), HEADER_CHECKSUM_DATA_SIZE(m_MaxBitmapHdrWriteGroups));
    MD5Final(actualChecksum, &ctx);

    switch (header->header.version) {
    case BITMAP_FILE_VERSION_V2:
        bVerified = ((header->header.endian == BITMAP_FILE_ENDIAN_FLAG) &&
                     (header->header.headerSize == DISK_SECTOR_SIZE) &&
                     (header->header.dataOffset == m_BitMapHeaderEndOffset) &&
                     (header->header.bitmapOffset == m_ulBitmapOffset) &&
                     (header->header.bitmapSize == m_ulNumberOfBitsInBitmap) &&
                     (header->header.bitmapGranularity == m_ulBitmapGranularity) &&
                     (header->header.fltDevSize == m_llDevSize) &&
                     (RtlCompareMemory(header->header.validationChecksum,actualChecksum,HEADER_CHECKSUM_SIZE) == HEADER_CHECKSUM_SIZE));
        // Added for OOD Support
        if (!(header->header.FieldFlags & BITMAP_HEADER_OOD_SUPPORT)) {
            header->header.FieldFlags |= BITMAP_HEADER_OOD_SUPPORT;
        }
        break;
    case BITMAP_FILE_VERSION:
        bVerified = ((header->header.endian == BITMAP_FILE_ENDIAN_FLAG) &&
                     (header->header.headerSize == sizeof(tLogHeader) - BITMAP_FILE_VERSION_HEADER_SIZE_INCREASE) &&
                     (header->header.dataOffset == m_BitMapHeaderEndOffset) &&
                     (header->header.bitmapOffset == m_ulBitmapOffset) &&
                     (header->header.bitmapSize == m_ulNumberOfBitsInBitmap) &&
                     (header->header.bitmapGranularity == m_ulBitmapGranularity) &&
                     (header->header.fltDevSize == m_llDevSize) &&
                     (RtlCompareMemory(header->header.validationChecksum,actualChecksum,HEADER_CHECKSUM_SIZE) == HEADER_CHECKSUM_SIZE));
        if (bVerified && bUpdateOnUpgrade) {
            //zero the whole memory after the last variable of old header
            RtlZeroMemory(&header->sectorFill[header->header.headerSize], (DISK_SECTOR_SIZE - header->header.headerSize));
            // Lets update the header to match the new format.
            header->header.version = BITMAP_FILE_VERSION_V2;
            //making the header size as the total header size.
            header->header.headerSize = DISK_SECTOR_SIZE;
            // Added for OOD support
            if (!(header->header.FieldFlags & BITMAP_HEADER_OOD_SUPPORT)) {
                header->header.FieldFlags |= BITMAP_HEADER_OOD_SUPPORT;
            }
            CalculateHeaderIntegrityChecksums(header);
        }
        break;
    default:
        bVerified = false;
        break;
    }

    return bVerified;
}

void BitmapAPI::CalculateHeaderIntegrityChecksums(tBitmapHeader * header)
{
    MD5Context ctx;

    // calculate the checksum
    MD5Init(&ctx);
    MD5Update(&ctx, (PUCHAR)(&(header->header.endian)), HEADER_CHECKSUM_DATA_SIZE(m_MaxBitmapHdrWriteGroups));
    MD5Final(header->header.validationChecksum, &ctx);

}

NTSTATUS
BitmapAPI::ChangeBitmapModeToRawIO(LONG *OutOfSyncErrorCode)
{
    NTSTATUS    Status = STATUS_SUCCESS;

    ASSERT(NULL != m_iobBitmapHeader);
    ASSERT(NULL != OutOfSyncErrorCode);

    KeWaitForMutexObject(&m_BitmapMutex, Executive, KernelMode, FALSE, NULL);

    switch(m_eBitmapState) {
    case ecBitmapStateOpened:
        ASSERT(NULL != m_fsBitmapStream);
        if (NULL != m_fsBitmapStream) {
            // TODO-SanKumar-1805: m_errorCausingOutOfSync member is not set in this function, unlike
            // other methods of BitmapAPI class.

            // Find the physical Offset.
            // Currently SetBits and ClearBits flushes the buffers synchronusly to disk.
            // So we do not have to flush them here.
            Status = segmentedBitmap->SyncFlushAll(); // make sure all the changes are saved
            if (!NT_SUCCESS(Status)) {
                InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_PRESHUTDOWN_BITMAP_FLUSH_FAILURE_DETAILED, (&m_DevLogContext), Status);
                *OutOfSyncErrorCode = MSG_INDSKFLT_ERROR_PRESHUTDOWN_BITMAP_FLUSH_FAILURE_Message;
            } else {
                Status = m_iobBitmapHeader->RecheckLearntInfo();

                if (!NT_SUCCESS(Status)) {
                    InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN | DM_BITMAP_WRITE,
                        (__FUNCTION__ ": Device %s failed in rechecking the learnt mapping info from the time of start filtering. Status Code %#x.\n",
                        m_chDevNum, Status));

                    NT_ASSERT(FALSE);

                    // The guards in place negotiated by us with the FS must not
                    // have led to remapping of the file clusters but it happened
                    // due to a gap in our logic. So, ensuring that the previously
                    // learnt data won't be used anymore.
                    m_iobBitmapHeader->UnlearnPhysicalIO();

                    InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_PRESHUTDOWN_LEARNT_MAPPING_RECHECK, (&m_DevLogContext), Status);
                    *OutOfSyncErrorCode = MSG_INDSKFLT_ERROR_PRESHUTDOWN_LEARNT_MAPPING_RECHECK_Message;
                }
            }

            Status = STATUS_SUCCESS;

            m_eBitmapState = ecBitmapStateRawIO;

            m_fsBitmapStream->Close();
            m_fsBitmapStream->Release();
            m_fsBitmapStream = NULL;
            m_iobBitmapHeader->SetFileStream(NULL);
            InVolDbgPrint(DL_TRACE_L1, DM_BITMAP_OPEN | DM_BITMAP_WRITE, 
                          ("ChangeBitmapModeToRawIO: Device %s Changed bitmap mode to RawIO\n", m_chDevNum));
        } else {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN | DM_BITMAP_WRITE, 
                          ("ChangeBitmapModeToRawIO: Device %s BitmapStream is NULL\n", m_chDevNum));
            Status = STATUS_FILE_CLOSED;
        }
        break;
    case ecBitmapStateRawIO:
        InVolDbgPrint(DL_TRACE_L1, DM_BITMAP_OPEN | DM_BITMAP_WRITE, 
                      ("ChangeBitmapModeToRawIO: Device %s Already in rawIO mode\n", m_chDevNum));
        break;
    default:
        InDskFltWriteEvent(INDSKFLT_WARN_BITMAPSTATE4, m_eBitmapState);
        Status = STATUS_DEVICE_NOT_READY;
        break;
    }

    KeReleaseMutex(&m_BitmapMutex, FALSE);

    return Status;
}


PDEVICE_OBJECT
BitmapAPI::GetBitmapDevObj (
   )
/*++
Routine Description:

    Retrieve Bitmap Device Object stored in lower object

    This function must be called at IRQL < DISPATCH_LEVEL
--*/
{
    PDEVICE_OBJECT pBitmapDevObj = NULL;
    KeWaitForMutexObject(&m_BitmapMutex, Executive, KernelMode, FALSE, NULL);
    pBitmapDevObj = m_iobBitmapHeader->GetBitmapDevObjLearn();
    KeReleaseMutex(&m_BitmapMutex, FALSE);
    return pBitmapDevObj;
}

#ifdef VOLUME_CLUSTER_SUPPORT
void
BitmapAPI::ResetPhysicalDO()
{
    KeWaitForMutexObject(&m_BitmapMutex, Executive, KernelMode, FALSE, NULL);
    if (m_iobBitmapHeader) {
        m_iobBitmapHeader->ResetPhysicalDO();
    }
    KeReleaseMutex(&m_BitmapMutex, FALSE);

    return;
}

NTSTATUS
BitmapAPI::CommitBitmap(
    tagLogRecoveryState recoveryState,
#ifdef VOLUME_CLUSTER_SUPPORT
	BitmapPersistentValues &BitmapPersistentValue, 
#endif
    NTSTATUS *pInMageStatus)
{
    NTSTATUS    Status = STATUS_SUCCESS;

    KeWaitForMutexObject(&m_BitmapMutex, Executive, KernelMode, FALSE, NULL);
#ifdef VOLUME_CLUSTER_SUPPORT
    Status = CommitBitmapInternal(recoveryState, pInMageStatus, BitmapPersistentValue);
#else
    Status = CommitBitmapInternal(recoveryState, pInMageStatus);
#endif
    KeReleaseMutex(&m_BitmapMutex, FALSE);

    return Status;
}

NTSTATUS
BitmapAPI::SaveWriteMetaDataToBitmap(PWRITE_META_DATA pWriteMetaData)
{
    NTSTATUS    Status = STATUS_SUCCESS;
    ULONGLONG   ullScaledOffset, ullScaledSize;

    InVolDbgPrint(DL_FUNC_TRACE, DM_BITMAP_WRITE, ("SaveWriteMetaDataToBitmap: Offset = %#I64x, Length = %#lx\n",
                    pWriteMetaData->llOffset, pWriteMetaData->ulLength));
    ASSERT(pWriteMetaData->llOffset >= 0);

    if (pWriteMetaData->llOffset < 0) {
        InVolDbgPrint(DL_WARNING, DM_BITMAP_WRITE, ("SaveWriteMetaDataToBitmap: write offset(%#I64x) is negative????\n",
                        pWriteMetaData->llOffset));
        Status = STATUS_INVALID_PARAMETER;
        return Status;
    }

    KeWaitForMutexObject(&m_BitmapMutex, Executive, KernelMode, FALSE, NULL);
    switch(m_eBitmapState) {
    case ecBitmapStateOpened:
        InVolDbgPrint(DL_INFO, DM_BITMAP_WRITE, ("SaveWriteMetaDataToBitmap: Using file system to write\n"));

        GetScaledOffsetAndSizeFromWriteMetadata(pWriteMetaData, &ullScaledOffset, &ullScaledSize);

        // verify the data is within bounds.
        if (pWriteMetaData->llOffset < m_llDevSize) {
            if ((pWriteMetaData->llOffset + pWriteMetaData->ulLength) <= m_llDevSize) {
                Status = segmentedBitmap->SetBitRun((UINT32)ullScaledSize, ullScaledOffset);
                segmentedBitmap->SyncFlushAll(); // make sure all the changes are saved
            } else {
                // change runs past end of volume
                InVolDbgPrint(DL_WARNING, DM_BITMAP_WRITE, ("SaveWriteMetaDataToBitmap: write offset(%#I64x) and Length(%#lx) past the end of volume size(%#I64x)\n",
                                pWriteMetaData->llOffset, pWriteMetaData->ulLength, m_llDevSize));
            }
        } else {
            // change starts past end of volume
            InVolDbgPrint(DL_WARNING, DM_BITMAP_WRITE, ("SaveWriteMetaDataToBitmap: write offset(%#I64x) past the end of volume size(%#I64x)\n",
                            pWriteMetaData->llOffset, m_llDevSize));
        }
        break;
    case ecBitmapStateRawIO:
        ASSERT((NULL != m_iobBitmapHeader) &&(NULL != m_iobBitmapHeader->buffer));

        InVolDbgPrint(DL_INFO, DM_BITMAP_WRITE, ("SaveWriteMetaDataToBitmap: Using raw mode to write Offset = %#I64x Length = %#x\n",
                                pWriteMetaData->llOffset, pWriteMetaData->ulLength));

		if (m_BitmapHeader->header.lastChanceChanges >= (m_MaxBitmapHdrWriteGroups * MAX_CHANGES_IN_WRITE_GROUP)) {
            // if number of changes in m_BitmapHeader exceed the maximum Value.
            m_BitmapHeader->header.changesLost += 1;
            InVolDbgPrint(DL_WARNING, DM_BITMAP_WRITE, ("SaveWriteMetaDataToBitmap: Number of Changes in logheader exceeded max limit.\n"));
        } else {
            GetScaledOffsetAndSizeFromWriteMetadata(pWriteMetaData, &ullScaledOffset, &ullScaledSize);

            // if the run is very large we have to chop it up into smaller pieces
            while (ullScaledSize > 0) {
				if (m_BitmapHeader->header.lastChanceChanges >= (m_MaxBitmapHdrWriteGroups * MAX_CHANGES_IN_WRITE_GROUP)) {
                    m_BitmapHeader->header.changesLost += 1;
                    InVolDbgPrint(DL_WARNING, DM_BITMAP_WRITE, ("SaveWriteMetaDataToBitmap: Number of Changes in logheader exceeded max limit.\n"));
                    break;
                } else {
                    m_BitmapHeader->changeGroup[m_BitmapHeader->header.lastChanceChanges / 
                                    MAX_CHANGES_IN_WRITE_GROUP].lengthOffsetPair[m_BitmapHeader->header.lastChanceChanges % 
                                                                            MAX_CHANGES_IN_WRITE_GROUP] =
                                    (min(ullScaledSize,0xFFFF) << 48) | (ullScaledOffset & 0xFFFFFFFFFFFF);
                    InVolDbgPrint(DL_INFO, DM_BITMAP_WRITE, ("SaveWriteMetaDataToBitmap: ScaledOffset = %#I64x, ScaledSize = %#I64x, Granularity = %#x\n",
                            (ullScaledOffset & 0xFFFFFFFFFFFF), min(ullScaledSize, 0xFFFF), m_ulBitmapGranularity));
                    m_BitmapHeader->header.lastChanceChanges++;
                    ullScaledOffset += min(ullScaledSize,0xFFFF);
                    ullScaledSize -= min(ullScaledSize,0xFFFF);
                }
            }

            // Let us write this header to bitmap.
            CalculateHeaderIntegrityChecksums(m_BitmapHeader);

            if ((NULL != m_iobBitmapHeader) && (NULL != m_iobBitmapHeader->buffer)) {
                RtlCopyMemory(m_iobBitmapHeader->buffer, m_BitmapHeader, LOG_HEADER_SIZE(m_MaxBitmapHdrWriteGroups));
                m_iobBitmapHeader->SetDirty();
                Status = m_iobBitmapHeader->SyncFlushPhysical(); // write the header
            } else {
                Status = STATUS_DEVICE_NOT_READY;
            }
        } // else of logheader is full for changes.
        break;
    default:
        InDskFltWriteEvent(INDSKFLT_WARN_BITMAPSTATE5, m_eBitmapState);
        Status = STATUS_DEVICE_NOT_READY;
        break;
    }
    
    KeReleaseMutex(&m_BitmapMutex, FALSE);

    return Status;
}
#endif // VOLUME_CLUSTER_SUPPORT

/*-----------------------------------------------------------------------------
 * Protected Routines - Will not be called by user.
 * These routines are called with mutex acquired.
 *-----------------------------------------------------------------------------
 */

NTSTATUS BitmapAPI::ReadAndVerifyBitmapHeader(NTSTATUS *pInMageStatus)
{
    NTSTATUS Status;

    // do this VERY carefully, read and validate destination before we write
    Status = m_iobBitmapHeader->SyncReadPhysical(TRUE); // read the buffer direct from disk
    if (NT_SUCCESS(Status)) {
        if (!VerifyHeader((tBitmapHeader *)(m_iobBitmapHeader->buffer))) {
            // our addressing doesn't seem to be correct, so don't proceed with writing
            InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE,
                ("ReadAndVerifyBitmapHeader: m_iobBitmapHeader verification failed\n"));

            SET_INMAGE_BITMAP_API_STATUS(pInMageStatus, INDSKFLT_ERROR_FINAL_HEADER_VALIDATE_FAILED);
            Status = MSG_INDSKFLT_ERROR_FINAL_HEADER_VALIDATE_FAILED_Message;
        }
    } else {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE,
            ("ReadAndVerifyBitmapHeader: m_iobBitmapHeader physical read failed with Status %#lx\n", Status));

        SET_INMAGE_BITMAP_API_STATUS(pInMageStatus, INDSKFLT_ERROR_FINAL_HEADER_READ_FAILED);
    }

    return Status;
}

NTSTATUS BitmapAPI::CommitHeader(BOOLEAN bVerifyExistingHeaderInRawIO, NTSTATUS *pInMageStatus)
{
TRC
    NTSTATUS Status = STATUS_SUCCESS;

    ASSERT((NULL != m_iobBitmapHeader) && (NULL != m_iobBitmapHeader->buffer));

    switch(m_eBitmapState) {

    case ecBitmapStateOpened:

        if (!TEST_FLAG(DriverContext.SystemStateFlags, ssFlagsAreBitmapFilesEncryptedOnDisk)) {

            CalculateHeaderIntegrityChecksums(m_BitmapHeader);
            RtlCopyMemory(m_iobBitmapHeader->buffer, m_BitmapHeader, LOG_HEADER_SIZE(m_MaxBitmapHdrWriteGroups));

            m_iobBitmapHeader->SetDirty();
            Status = m_iobBitmapHeader->SyncFlush(); // write the header

            if (!NT_SUCCESS(Status)) {
                InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE, ("CommitHeader: writing m_iobBitmapHeader with FS to volume failed with Status %#lx\n", Status));
                if (pInMageStatus)
                    SET_INMAGE_BITMAP_API_STATUS(pInMageStatus, INDSKFLT_ERROR_FINAL_HEADER_FS_WRITE_FAILED);
                InDskFltWriteEvent(INDSKFLT_ERROR_FS_WRITE_ON_BITMAP_HEADER_FAIL, Status);
            } else {
                InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE, ("CommitHeader: writing m_iobBitmapHeader with FS to volume succeeded with Status %#lx\n", Status));
            }

            break;
        }

        // Always write the header in Raw mode, since the data written through
        // the filesystem will not be the same as data read from the disk, in
        // case bitlocker is enabled on the volume containing the bitmap file.
        __fallthrough;

    case ecBitmapStateRawIO:

        if (bVerifyExistingHeaderInRawIO) {
            // Note-SanKumar-1805: ByDesign - This would fail once in the
            // subsequent reboots after enabling/disabling bitlocker on system
            // volume.
            Status = ReadAndVerifyBitmapHeader(pInMageStatus);
        } else {
            InVolDbgPrint(DL_VERBOSE, DM_BITMAP_WRITE,
                ("CommitHeader: ignoring validation of m_iobBitmapHeader in Raw mode. "
                 "VerifyExistingHeader: %d.\n",
                 bVerifyExistingHeaderInRawIO));

            Status = STATUS_SUCCESS;
        }

        if (NT_SUCCESS(Status)) {

            CalculateHeaderIntegrityChecksums(m_BitmapHeader);
            RtlCopyMemory(m_iobBitmapHeader->buffer, m_BitmapHeader, LOG_HEADER_SIZE(m_MaxBitmapHdrWriteGroups));

            m_iobBitmapHeader->SetDirty();
            Status = m_iobBitmapHeader->SyncFlushPhysical(); // write the header

            if (!NT_SUCCESS(Status)) {
                InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE, ("CommitHeader: m_iobBitmapHeader raw write failed with Status %#lx\n", Status));
                if (pInMageStatus)
                    SET_INMAGE_BITMAP_API_STATUS(pInMageStatus, INDSKFLT_ERROR_FINAL_HEADER_DIRECT_WRITE_FAILED);
                InDskFltWriteEvent(INDSKFLT_ERROR_RAW_WRITE_ON_BITMAP_HEADER_FAIL, Status);
            } else {
                InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE, ("CommitHeader: m_iobBitmapHeader raw write succeeded with Status %#lx\n", Status));
            }
        }

        break;

    default:
        InDskFltWriteEvent(INDSKFLT_WARN_BITMAPSTATE1, m_eBitmapState);

        Status = STATUS_DEVICE_NOT_READY;
        break;
    }

    return Status;
}


NTSTATUS
BitmapAPI::CommitBitmapInternal (
    tagLogRecoveryState recoveryState,
    NTSTATUS *pInMageStatus,
#ifdef VOLUME_CLUSTER_SUPPORT
    BitmapPersistentValues &BitmapPersistentValue,
#endif
    ULONG ResyncErrorCode,
    ULONGLONG ResyncTimeStamp
    )

{
    ASSERT((NULL != m_iobBitmapHeader) && (NULL != m_iobBitmapHeader->buffer));
    NTSTATUS    Status = STATUS_SUCCESS;

    switch(m_eBitmapState) {
    case ecBitmapStateOpened:
        // InVolflt by default sets to clearshutdown
        // Indskflt is extended to store error value too
        m_BitmapHeader->header.recoveryState = recoveryState;
        m_BitmapHeader->header.ulOutOfSyncErrorCode = ResyncErrorCode;
        m_BitmapHeader->header.ullOutOfSyncTimeStamp = ResyncTimeStamp;
#ifdef VOLUME_CLUSTER_SUPPORT
        if (BitmapPersistentValue.IsClusterVolume()) {
            m_BitmapHeader.header.LastSequenceNumber = BitmapPersistentValue.GetGlobalSequenceNumber();
            m_BitmapHeader.header.LastTimeStamp = BitmapPersistentValue.GetGlobalTimeStamp();
            m_BitmapHeader.header.PrevEndTimeStamp = BitmapPersistentValue.GetPrevEndTimeStamp();
            m_BitmapHeader.header.PrevEndSequenceNumber = BitmapPersistentValue.GetPrevEndSequenceNumber();
            m_BitmapHeader.header.PrevSequenceId = BitmapPersistentValue.GetPrevSequenceId();
        }
#endif
        segmentedBitmap->SyncFlushAll();
        Status = CommitHeader(TRUE, pInMageStatus);
        if (!NT_SUCCESS(Status)) {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE | DM_BITMAP_OPEN, 
                          ("CommitBitmapInternal: Unable to write header with FS to device %s, Status = %#x\n", m_chDevNum, Status));
            Status = MSG_INDSKFLT_ERROR_FINAL_HEADER_FS_WRITE_FAILED_Message;
        } else {
            InVolDbgPrint(DL_TRACE_L1, DM_BITMAP_WRITE | DM_BITMAP_OPEN,
                ("CommitBitmapInternal: Succeeded to write clean shutdown marker to bimtap file %wZ with FS to device %s\n",
                 m_ustrBitmapFilename, m_chDevNum));
        }
        break;
    case ecBitmapStateRawIO:
#ifdef VOLUME_FLT 
        if (m_iobBitmapHeader->IsDirty() || (cleanShutdown != m_BitmapHeader.header.recoveryState)) 
#else
        //Store error info set by caller
        if (m_iobBitmapHeader->IsDirty() || 
           (cleanShutdown != m_BitmapHeader->header.recoveryState) || 
           (cleanShutdown != recoveryState)) 
#endif
        {
            m_BitmapHeader->header.recoveryState = recoveryState;
            m_BitmapHeader->header.ulOutOfSyncErrorCode = ResyncErrorCode;
            m_BitmapHeader->header.ullOutOfSyncTimeStamp = ResyncTimeStamp;
#ifdef VOLUME_CLUSTER_SUPPORT
           if (BitmapPersistentValue.IsClusterVolume()) {
                m_BitmapHeader.header.LastSequenceNumber = BitmapPersistentValue.GetGlobalSequenceNumber();
                m_BitmapHeader.header.LastTimeStamp = BitmapPersistentValue.GetGlobalTimeStamp();
                m_BitmapHeader.header.PrevEndTimeStamp = BitmapPersistentValue.GetPrevEndTimeStamp();
                m_BitmapHeader.header.PrevEndSequenceNumber = BitmapPersistentValue.GetPrevEndSequenceNumber();
                m_BitmapHeader.header.PrevSequenceId = BitmapPersistentValue.GetPrevSequenceId();
            }
#endif
           Status = CommitHeader(TRUE, pInMageStatus);
            if (!NT_SUCCESS(Status)) {
                InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE | DM_BITMAP_OPEN, 
                              ("CommitBitmapInternal: Unable to write header with raw IO to device %s, Status = %#x\n", m_chDevNum, Status));
                Status = MSG_INDSKFLT_ERROR_FINAL_HEADER_FS_WRITE_FAILED_Message;
            } else {
                InVolDbgPrint(DL_TRACE_L1, DM_BITMAP_WRITE | DM_BITMAP_OPEN,
                    ("CommitBitmapInternal: Succeeded to write clean shutdown marker to bimtap file %wZ with raw IO to device %s\n",
                     m_ustrBitmapFilename, m_chDevNum));
            }
        } else {
            // Already bitmap has clean shutdown marker.
            InVolDbgPrint( DL_TRACE_L1, DM_BITMAP_OPEN | DM_BITMAP_WRITE,
                           ("CommitBitmapInternal: Device %s already has clean shutdown marker and bitmapheader is not dirty\n", m_chDevNum));
        }

        break;
    default:
        Status = STATUS_DEVICE_NOT_READY;
        break;
    }

    return Status;
}


NTSTATUS BitmapAPI::InitBitmapFile(NTSTATUS *pInMageStatus
#ifdef VOLUME_CLUSTER_SUPPORT
	, BitmapPersistentValues &BitmapPersistentValue
#endif
)
{
TRC
    NTSTATUS Status;
#ifdef VOLUME_CLUSTER_SUPPORT
    bool IsVolOnClus, VolFilteringState;
#endif

    PAGED_CODE();

    Status = FastZeroBitmap();
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    // For 1804 release, where we are piloting new learning code, we will
    // not fail the bitmap open on raw handling failures. As we will be
    // reliant on it only at the time of shutdown, we will fail the corresponding
    // raw reads and writes. As we don't fail here and we will be using the
    // existing file-based handling, the user has the issue of going into
    // resync "only" at every reboot, just in case we have shipped a bug.

    // Throw away the previously learnt info and learn again, as there could have
    // been negative cases, such as truncated file, etc.
    m_iobBitmapHeader->UnlearnPhysicalIO();

    Status = m_iobBitmapHeader->LearnPhysicalIO();
    if (!NT_SUCCESS(Status)) {
        NT_ASSERT(FALSE);

        if (TEST_FLAG(DriverContext.SystemStateFlags, ssFlagsAreBitmapFilesEncryptedOnDisk)) {

            SET_INMAGE_BITMAP_API_STATUS(pInMageStatus, INDSKFLT_ERROR_BITMAP_HEADER_LEARN_PHYSICAL_IO_FAILED,
                m_ustrBitmapFilename.Buffer,
                m_fsBitmapStream->GetFileHandle(),
                Status,
                SourceLocationInitBitmapFileLearnPhyFailEncOn);

            m_ErrorCausingOutOfSync = *pInMageStatus;
            return Status;
        } else {
            InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_BITMAP_HEADER_LEARN_PHYSICAL_IO_FAILED,
                (&m_DevLogContext),
                m_ustrBitmapFilename.Buffer,
                m_fsBitmapStream->GetFileHandle(),
                Status,
                SourceLocationInitBitmapFileLearnPhyFailEncOff);

            // Ignore this error. Even if we have shipped with a bug in the learning code,
            // only the post-shutdown header write would fail. If not ignored, the load
            // bitmap would never succeed, failing the eanble protection/resync forever.
            Status = STATUS_SUCCESS;
        }
    }

    if (!TEST_FLAG(DriverContext.SystemStateFlags, ssFlagsAreBitmapFilesEncryptedOnDisk)) {

        // Writes the header portion of the file with random data and verifies it through raw IO.
        Status = ValidateLearntHeaderPhysicalOffsets(true);

        if (!NT_SUCCESS(Status)) {
            InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_BITMAP_VALIDATE_LEARNT_HEADER_PHYSICAL_OFFSETS,
                (&m_DevLogContext),
                m_ustrBitmapFilename.Buffer,
                m_fsBitmapStream->GetFileHandle(),
                Status,
                SourceLocationInitBitmapFileValidateLearntPhyOff);

            NT_ASSERT(FALSE);

            // Ensuring that the invalid learnt data isn't used beyond this point.
            m_iobBitmapHeader->UnlearnPhysicalIO();

            // Ignore this error. Even if we have shipped with a bug in the learning code,
            // only the post-shutdown header write would fail. If not ignored, the load
            // bitmap would never succeed, failing the eanble protection/resync forever.
            Status = STATUS_SUCCESS;
        }
    } else {
        InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_BITMAP_OPEN_VALIDATE_LEARNT_HEADER_PHYSICAL_OFFSETS_SKIPPED_ENCRYPTED,
            (&m_DevLogContext),
            m_ustrBitmapFilename.Buffer,
            m_fsBitmapStream->GetFileHandle(),
            STATUS_SUCCESS,
            SourceLocationInitBitmapFileValidateLearntPhyOff);
    }

    // init the new header
    RtlZeroMemory(m_BitmapHeader->sectorFill, DISK_SECTOR_SIZE);
    m_BitmapHeader->header.endian = BITMAP_FILE_ENDIAN_FLAG; // little endian    UINT32 endian; // read as 0xFF is little endian and 0xFF000000 means big endian
    m_BitmapHeader->header.headerSize = DISK_SECTOR_SIZE;
    m_BitmapHeader->header.version = BITMAP_FILE_VERSION_V2;   // 0x10000 was initial version
    m_BitmapHeader->header.dataOffset = m_BitMapHeaderEndOffset;
    m_BitmapHeader->header.bitmapOffset = m_ulBitmapOffset;
    m_BitmapHeader->header.bitmapSize = m_ulNumberOfBitsInBitmap;
    m_BitmapHeader->header.bitmapGranularity = m_ulBitmapGranularity;
    m_BitmapHeader->header.fltDevSize = m_llDevSize;
    m_BitmapHeader->header.recoveryState = dirtyShutdown; // unless we shutdown clean, assume dirty
    m_BitmapHeader->header.lastChanceChanges = 0;
    m_BitmapHeader->header.bootCycles = 0;
    m_BitmapHeader->header.changesLost = 0;
    m_BitmapHeader->header.LastTimeStamp = 0;
    m_BitmapHeader->header.LastSequenceNumber = 0;
    m_BitmapHeader->header.ControlFlags = 0;
    m_BitmapHeader->header.FieldFlags = 0;
    m_BitmapHeader->header.PrevEndTimeStamp = 0;
    m_BitmapHeader->header.PrevEndSequenceNumber = 0;
    m_BitmapHeader->header.PrevSequenceId = 0;
    m_BitmapHeader->header.FieldFlags |= BITMAP_HEADER_OOD_SUPPORT;
#ifdef VOLUME_CLUSTER_SUPPORT
    VolFilteringState = BitmapPersistentValue.GetVolumeFilteringState(&IsVolOnClus);
    if (IsVolOnClus) {
        if ((TRUE == m_bCorruptBitmapFile) || (TRUE == m_bEmptyBitmapFile)) { // Update with Current Filtering State
            if (VolFilteringState) {
                SetFilteringStopped();
            }
        }

        if (TRUE == m_bNewBitmapFile) {
            if (BitmapPersistentValue.SetVolumeFilteringState(DriverContext.bDisableVolumeFilteringForNewClusterVolumes)) {
                if (DriverContext.bDisableVolumeFilteringForNewClusterVolumes)
                    SetFilteringStopped();
            } else {
                if (BitmapPersistentValue.GetVolumeFilteringState(NULL))
                    SetFilteringStopped();
            }
        }
    }
#endif
    Status = CommitHeader(FALSE, pInMageStatus);

    return Status;
}

NTSTATUS BitmapAPI::FastZeroBitmap()
{
TRC
    IOBuffer * buffer;
    UINT64 i;
    NTSTATUS Status;

    Status = STATUS_SUCCESS;

    buffer = new (BITMAP_FILE_SEGMENT_SIZE, 0x1000)IOBuffer;
    InVolDbgPrint(DL_INFO, DM_MEM_TRACE, ("BitmapAPI::FastZeroBitmap Allocation %#p\n", buffer));
    if (NULL == buffer) {
        return STATUS_NO_MEMORY;
    }

    buffer->SetFileStream(m_fsBitmapStream);
    for (i = m_ulBitmapOffset + m_BitMapHeaderEndOffset; i < (m_ulBitmapOffset + m_ulBitMapSizeInBytes); i += BITMAP_FILE_SEGMENT_SIZE) {
        buffer->SetFileOffset(i);
        buffer->SetDirty();
        Status = buffer->SyncFlush();
        if (!NT_SUCCESS(Status)) {
            break;
        }
    }
    buffer->Release();

return Status;
}

// Following way #ifdef was written to have better view in source insight editor
NTSTATUS BitmapAPI::LoadBitmapHeaderFromFileStream(NTSTATUS *pInMageStatus, 
    tagLogRecoveryState* BitmapRecoveryState,
    bool bClearOnOpen
#ifdef VOLUME_CLUSTER_SUPPORT 
    ,BitmapPersistentValues &BitmapPersistentValue
#endif
)
{
    NTSTATUS    Status = STATUS_SUCCESS;

    ASSERT(NULL != m_fsBitmapStream);

    *pInMageStatus = STATUS_SUCCESS;
    // Set initial state of sync to lost sync. If bitmap header is verified successfully
    // we will set the sync state to TRUE.
    m_bDevInSync = FALSE;
    m_ErrorCausingOutOfSync = STATUS_SUCCESS;

    m_iobBitmapHeader = new (LOG_HEADER_SIZE(m_MaxBitmapHdrWriteGroups), 0) IOBuffer;
    if (NULL == m_iobBitmapHeader) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, 
            ("LoadBitmapHeaderFromFileStream: Memory allocation failed for bitmap header buffer. BitmapFile = %wZ, Device = %s\n",
                m_ustrBitmapFilename, m_chDevNum));
        SET_INMAGE_BITMAP_API_STATUS(pInMageStatus, INDSKFLT_ERROR_NO_MEMORY, SourceLocationLoadBitmapHeaderAlloc);
        m_ErrorCausingOutOfSync = *pInMageStatus;
        Status = STATUS_NO_MEMORY;
        goto Cleanup_And_Return_Failure;
    }

    m_iobBitmapHeader->SetFileStream(m_fsBitmapStream);
    m_iobBitmapHeader->SetFileOffset(m_ulBitmapOffset + 0); // header is at start of log area

    m_BitmapHeader = (tBitmapHeader *)ExAllocatePoolWithTag(NonPagedPool, LOG_HEADER_SIZE(m_MaxBitmapHdrWriteGroups), TAG_BITMAP_HEADER);
    if (NULL == m_BitmapHeader) {
        SET_INMAGE_BITMAP_API_STATUS(pInMageStatus, INDSKFLT_WARN_BITMAP_HEADER_ALLOC_FAILURE);
        m_ErrorCausingOutOfSync = *pInMageStatus;
        Status = STATUS_NO_MEMORY;
        goto Cleanup_And_Return_Failure;
    }
    RtlZeroMemory(m_BitmapHeader, LOG_HEADER_SIZE(m_MaxBitmapHdrWriteGroups));

    if (bClearOnOpen) {
        m_bDevInSync = TRUE;
    } else if (FALSE == m_fsBitmapStream->WasCreated()) {

        // For 1804 release, where we are piloting new learning code, we will
        // not fail the bitmap open on raw handling failures. As we will be
        // reliant on it only at the time of shutdown, we will fail the corresponding
        // raw reads and writes. As we don't fail here and we will be using the
        // existing file-based handling, the user has the issue of going into
        // resync "only" at every reboot, just in case we have shipped a bug.

        Status = m_iobBitmapHeader->LearnPhysicalIO();
        if (!NT_SUCCESS(Status) && (Status != STATUS_END_OF_FILE)) {
            NT_ASSERT(FALSE);

            if (TEST_FLAG(DriverContext.SystemStateFlags, ssFlagsAreBitmapFilesEncryptedOnDisk)) {

                SET_INMAGE_BITMAP_API_STATUS(pInMageStatus, INDSKFLT_ERROR_BITMAP_HEADER_LEARN_PHYSICAL_IO_FAILED,
                    m_ustrBitmapFilename.Buffer,
                    m_fsBitmapStream->GetFileHandle(),
                    Status,
                    SourceLocationLoadBmapHdrFromFStreamLearnPhyFailEncOn);

                m_ErrorCausingOutOfSync = *pInMageStatus;
                goto Cleanup_And_Return_Failure;
            } else {
                InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_BITMAP_HEADER_LEARN_PHYSICAL_IO_FAILED,
                    (&m_DevLogContext),
                    m_ustrBitmapFilename.Buffer,
                    m_fsBitmapStream->GetFileHandle(),
                    Status,
                    SourceLocationLoadBmapHdrFromFStreamLearnPhyFailEncOff);

                // Ignore this error. Even if we have shipped with a bug in the learning code,
                // only the post-shutdown header write would fail. If not ignored, the load
                // bitmap would never succeed, failing the eanble protection/resync forever.
                Status = STATUS_SUCCESS;
            }
        }

        if (NT_SUCCESS(Status)) {

            // get the header
            if (TEST_FLAG(DriverContext.SystemStateFlags, ssFlagsAreBitmapFilesEncryptedOnDisk)) {
                Status = m_iobBitmapHeader->SyncReadPhysical(TRUE);
            } else {
                Status = m_iobBitmapHeader->SyncRead();
            }

            NT_ASSERT(Status != STATUS_END_OF_FILE);
        }

        if (!NT_SUCCESS(Status) && (Status != STATUS_END_OF_FILE)) {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, 
                ("LoadBitmapHeaderFromFileStream: Header buffer read failed with Status %#x, BitmapFile = %wZ, Device = %s\n",
                    Status, m_ustrBitmapFilename, m_chDevNum));
            SET_INMAGE_BITMAP_API_STATUS(pInMageStatus, INDSKFLT_ERROR_BITMAP_FILE_CANT_READ);
            m_ErrorCausingOutOfSync = *pInMageStatus;
            goto Cleanup_And_Return_Failure;
        }

        if (Status == STATUS_END_OF_FILE) {
            // If the file size was less than header size, reading header returns STATUS_END_OF_FILE.
            InVolDbgPrint(DL_WARNING, DM_BITMAP_OPEN, 
                ("LoadBitmapHeaderFromFileStream: Header buffer return STATUS_END_OF_FILE, BitmapFile = %wZ, Device = %s\n",
                    m_ustrBitmapFilename, m_chDevNum));
            m_bEmptyBitmapFile = TRUE;
            SET_INMAGE_BITMAP_API_STATUS(pInMageStatus, INDSKFLT_WARN_BITMAP_FILE_LOG_FIXED, SourceLocationLoadBitmapHeaderEmpty);
            m_ErrorCausingOutOfSync = *pInMageStatus;
        } else {
            ASSERT(NT_SUCCESS(Status));

            RtlCopyMemory(m_BitmapHeader, m_iobBitmapHeader->buffer, LOG_HEADER_SIZE(m_MaxBitmapHdrWriteGroups));

            if (!TEST_FLAG(DriverContext.SystemStateFlags, ssFlagsAreBitmapFilesEncryptedOnDisk)) {

                // Writes the header portion of the file with random data and verifies it through raw IO.
                Status = ValidateLearntHeaderPhysicalOffsets(true);

                if (!NT_SUCCESS(Status)) {
                    InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_BITMAP_VALIDATE_LEARNT_HEADER_PHYSICAL_OFFSETS,
                        (&m_DevLogContext),
                        m_ustrBitmapFilename.Buffer,
                        m_fsBitmapStream->GetFileHandle(),
                        Status,
                        SourceLocationLoadBmapHdrFromFStreamValidateLearntPhyOff);

                    NT_ASSERT(FALSE);

                    // Ensuring that the invalid learnt data isn't used beyond this point.
                    m_iobBitmapHeader->UnlearnPhysicalIO();

                    // Ignore this error. Even if we have shipped with a bug in the learning code,
                    // only the post-shutdown header write would fail. If not ignored, the load
                    // bitmap would never succeed, failing the eanble protection/resync forever.
                    Status = STATUS_SUCCESS;
                }
            } else {
                InDskFltWriteEventWithDevCtxt(INDSKFLT_INFO_BITMAP_OPEN_VALIDATE_LEARNT_HEADER_PHYSICAL_OFFSETS_SKIPPED_ENCRYPTED,
                    (&m_DevLogContext),
                    m_ustrBitmapFilename.Buffer,
                    m_fsBitmapStream->GetFileHandle(),
                    STATUS_SUCCESS,
                    SourceLocationLoadBmapHdrFromFStreamValidateLearntPhyOff);
            }

            if (VerifyHeader(m_BitmapHeader, true)) {
                // retrieve the recovery state only for valid header
                *BitmapRecoveryState = m_BitmapHeader->header.recoveryState;
                // header valid for current volume
                if (m_BitmapHeader->header.recoveryState == cleanShutdown) {
                    // everything is good
                    InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, 
                        ("LoadBitmapHeaderFromFileStream: Bitmap file %wZfor\nvolume %s indicates normal previous shutdown\n",
                            &m_ustrBitmapFilename, m_chDevNum));
                    m_bDevInSync = TRUE;
                } else {
                    // we crashed or shutdown without proper closing
                    InVolDbgPrint(DL_WARNING, DM_BITMAP_OPEN, 
                        ("LoadBitmapHeaderFromFileStream: Bitmap file %wZ for\nvolume %s indicates unexpected previous shutdown\n",
                            &m_ustrBitmapFilename, m_chDevNum));
                    switch(m_BitmapHeader->header.recoveryState) {
                        case dirtyShutdown:
                            SET_INMAGE_BITMAP_API_STATUS(pInMageStatus, INDSKFLT_WARN_LOST_SYNC_SYSTEM_CRASHED);
                            break;
                        case pendingChanges:
                            SET_INMAGE_BITMAP_API_STATUS(pInMageStatus, INDSKFLT_WARN_COULD_NOT_PERSIST_CHANGES);
                            break;
                        case resyncMarked:
                            SET_INMAGE_BITMAP_API_STATUS(pInMageStatus, INDSKFLT_WARN_RESYNC_CARRY_FORWARD);
                            InDskFltWriteEventWithDevCtxt(INDSKFLT_WARN_RESYNC_CARRY_FORWARD_ADDITIONAL_DETAILS, (&m_DevLogContext), m_BitmapHeader->header.ulOutOfSyncErrorCode, m_BitmapHeader->header.ullOutOfSyncTimeStamp);
                            break;
                        default:
                            SET_INMAGE_BITMAP_API_STATUS(pInMageStatus, INDSKFLT_WARN_IN_BITMAP_INFO);
                            break;
                    }
                    m_ErrorCausingOutOfSync = *pInMageStatus;
                }
#ifdef VOLUME_CLUSTER_SUPPORT
                //Read the persistent values from the header
                if (BitmapPersistentValue.IsClusterVolume()) {
                    BitmapPersistentValue.SetGlobalSequenceNumber(m_BitmapHeader.header.LastSequenceNumber);
                    BitmapPersistentValue.SetGlobalTimeStamp(m_BitmapHeader.header.LastTimeStamp);
                    BitmapPersistentValue.SetPrevEndTimeStamp(m_BitmapHeader.header.PrevEndTimeStamp);
                    BitmapPersistentValue.SetPrevEndSequenceNumber(m_BitmapHeader.header.PrevEndSequenceNumber);
                    BitmapPersistentValue.SetPrevSequenceId(m_BitmapHeader.header.PrevSequenceId);
                    if (!BitmapPersistentValue.SetVolumeFilteringState(IsFilteringStopped() )) {
                        if (BitmapPersistentValue.GetVolumeFilteringState(NULL)) {
                            SetFilteringStopped();
                        } else {
                            SetFilteringStarted();
                        }
                    }
                    BitmapPersistentValue.SetResynRequiredValues(m_BitmapHeader.header.ulOutOfSyncErrorCode,
                                                                 m_BitmapHeader.header.ulOutOfSyncErrorStatus,
                                                                 m_BitmapHeader.header.ControlFlags,
                                                                 m_BitmapHeader.header.ullOutOfSyncTimeStamp,
                                                                 m_BitmapHeader.header.ulOutOfSyncCount,
                                                                 m_BitmapHeader.header.ulOutOfSyncIndicatedAtCount,
                                                                 m_BitmapHeader.header.ullOutOfSyncIndicatedTimeStamp,
                                                                 m_BitmapHeader.header.ullOutOfSyncResetTimeStamp);
                }
#endif
                m_BitmapHeader->header.bootCycles++;

                // modified header is saved after rawIO changes are moved into bitmap.
                return STATUS_SUCCESS;
            }
            
            // Verify header has failed, bitmap file is corrupt.
            // leave DevSyncLost to TRUE and init bitmap file.
            // Log the INDSKFLT_ERROR_BITMAP_FILE_LOG_DAMAGED message to event viewer.

            SET_INMAGE_BITMAP_API_STATUS(pInMageStatus,
                INDSKFLT_ERROR_BITMAP_FILE_LOG_DAMAGED,
                SourceLocationLoadBitmapHeaderNotEmpty);
            m_ErrorCausingOutOfSync = *pInMageStatus;
            m_bCorruptBitmapFile = TRUE;
            InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, 
                ("LoadBitmapHeaderFromFileStream: Verify header failed. Bitmap file = %wZ for\nVolume = %s is corrupt\n",
                    m_ustrBitmapFilename, m_chDevNum));
        }
    } else {
        m_bNewBitmapFile = TRUE;
        SET_INMAGE_BITMAP_API_STATUS(pInMageStatus, INDSKFLT_INFO_BITMAP_FILE_CREATED);
        m_ErrorCausingOutOfSync = *pInMageStatus;
    }

    // File was created new, or the file was empty, or the file has to be cleared.
#ifdef VOLUME_CLUSTER_SUPPORT
    Status = InitBitmapFile(pInMageStatus, BitmapPersistentValue);
#else
    Status = InitBitmapFile(pInMageStatus);
#endif
    if (NT_SUCCESS(Status)) {
        if (bClearOnOpen) {
            InVolDbgPrint(DL_WARNING, DM_BITMAP_OPEN, 
                ("LoadBitmapHeaderFromFileStream - Cleared bitmap file %wZ for device %s\n",
                        &m_ustrBitmapFilename, m_chDevNum));
        } else if (TRUE == m_bEmptyBitmapFile) {
            InVolDbgPrint(DL_WARNING, DM_BITMAP_OPEN, 
                ("LoadBitmapHeaderFromFileStream - Repaired empty bitmap file %wZ for device %s\n",
                        &m_ustrBitmapFilename, m_chDevNum));
        } else if (TRUE == m_bCorruptBitmapFile) {
            InVolDbgPrint(DL_WARNING, DM_BITMAP_OPEN, 
                ("LoadBitmapHeaderFromFileStream - Repaired corrupt bitmap file %wZ for device %s\n",
                        &m_ustrBitmapFilename, m_chDevNum));        
        } else {
            ASSERT(TRUE == m_bNewBitmapFile);
            InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, 
                ("LoadBitmapHeaderFromFileStream - new bitmap file %wZ successfully created for device %s\n",
                        &m_ustrBitmapFilename, m_chDevNum));
        }
    } else {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, 
            ("LoadBitmapHeaderFromFileStream - Can't initialize bitmap file %wZ device %s, Status = %#X\n",
                        &m_ustrBitmapFilename, m_chDevNum, Status));
        SET_INMAGE_BITMAP_API_STATUS(pInMageStatus, INDSKFLT_ERROR_BITMAP_FILE_CANT_INIT);
        m_ErrorCausingOutOfSync = *pInMageStatus;
        goto Cleanup_And_Return_Failure;
    }

    return Status;

Cleanup_And_Return_Failure:
    ASSERT(STATUS_SUCCESS != *pInMageStatus);

    if (NULL != m_iobBitmapHeader) {
        m_iobBitmapHeader->Release();
        m_iobBitmapHeader = NULL;
    }

    if (NULL != m_BitmapHeader) {
        ExFreePoolWithTag(m_BitmapHeader, TAG_BITMAP_HEADER);
        m_BitmapHeader = NULL;
    }

    return Status;
}

NTSTATUS
BitmapAPI::MoveRawIOChangesToBitmap(NTSTATUS *pInMageStatus)
{
    ULONG       i, ulScaledSize, ulWriteSize;
    ULONGLONG   ullSizeOffsetPair, ullScaledOffset, ullWriteOffset, ullRoundedDevSize;
    NTSTATUS    Status = STATUS_SUCCESS;

    // Bitmap has to be in opened state. This operation can not be performed in Commited, RawIO or Closed mode.
    if (ecBitmapStateOpened != m_eBitmapState)
        return STATUS_INVALID_PARAMETER;

    ullRoundedDevSize = ((ULONGLONG)m_llDevSize + m_ulBitmapGranularity - 1) / m_ulBitmapGranularity;
    ullRoundedDevSize = ullRoundedDevSize * m_ulBitmapGranularity;

    // now sweep through and save any last chance changes into the bitmap
    for(i = 0; i < m_BitmapHeader->header.lastChanceChanges; i++) {
        ullSizeOffsetPair = m_BitmapHeader->changeGroup[i / MAX_CHANGES_IN_WRITE_GROUP].lengthOffsetPair[i % MAX_CHANGES_IN_WRITE_GROUP];
        ulScaledSize = (ULONG)(ullSizeOffsetPair >> 48);
        ullScaledOffset = ullSizeOffsetPair & 0xFFFFFFFFFFFF;
        ullWriteOffset = ullScaledOffset * m_ulBitmapGranularity;
        ulWriteSize = ulScaledSize * m_ulBitmapGranularity;
        if (ullWriteOffset < (ULONGLONG)m_llDevSize) {
            if ((ullWriteOffset + ulWriteSize) > ullRoundedDevSize) {
                // As the granularity used to save changes in RawIO mode is same as bitmap granularity. This should not happen.
                InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, 
                    ("MoveRawIOChangesToBitmap: Correcting WriteOffset %#I64x ScaledOffset %#I64x Size %#x ScaledSize %#x DevSize %#I64x RoundedfltDevSize %#I64x\n",
                                ullWriteOffset, ullScaledOffset, ulWriteSize, ulScaledSize, m_llDevSize, ullRoundedDevSize));
                ulScaledSize = (ULONG)(((ULONGLONG)m_llDevSize) - ullWriteOffset);
                ulScaledSize = (ulScaledSize + m_ulBitmapGranularity - 1) / m_ulBitmapGranularity;
                InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("MoveRawIOChangesToBitmap: Corrected to ulScaledSize %#x\n",
                                ulScaledSize));
            }
            Status = segmentedBitmap->SetBitRun((UINT32)ulScaledSize, ullScaledOffset);
            if (!NT_SUCCESS(Status)) {
                SET_INMAGE_BITMAP_API_STATUS(pInMageStatus, INDSKFLT_ERROR_BITMAP_FILE_CANT_APPLY_SHUTDOWN_CHANGES);
                m_ErrorCausingOutOfSync = *pInMageStatus;
                m_bDevInSync = FALSE;
                InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("MoveRawIOChangesToBitmap: Error writing Raw IO changes to bitmap, Status = %#x\n",
                    Status));
                return Status;
            }
        } else {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("MoveRawIOChangesToBitmap: Discarding change - Offset %#I64x Size %#x DevSize %I64x\n",
                            ullWriteOffset, ulWriteSize, m_llDevSize));
        }
    }

    if (0 != m_BitmapHeader->header.changesLost) {
        SET_INMAGE_BITMAP_API_STATUS(pInMageStatus, INDSKFLT_ERROR_TOO_MANY_LAST_CHANCE, m_BitmapHeader->header.changesLost);
        m_ErrorCausingOutOfSync = *pInMageStatus;
        m_bDevInSync = FALSE;
        InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, ("MoveRawIOChangesToBitmap: Lost Changes %#x, Setting Out of sync.\n",  m_BitmapHeader->header.changesLost));
    }

    if (0 != m_BitmapHeader->header.lastChanceChanges) {
        SET_INMAGE_BITMAP_API_STATUS(pInMageStatus, INDSKFLT_INFO_COUNT_LAST_CHANCE_CHANGES, m_BitmapHeader->header.lastChanceChanges);
    }

    // Save it for debugging purposes
#if DBG
    ULONG       ulLastChanceChanges = m_BitmapHeader->header.lastChanceChanges;
#endif
    m_BitmapHeader->header.lastChanceChanges = 0;
    m_BitmapHeader->header.changesLost = 0;
    m_BitmapHeader->header.recoveryState = dirtyShutdown; // unless we shutdown clean, assume dirty
    m_BitmapHeader->header.ulOutOfSyncErrorCode = 0;
    m_BitmapHeader->header.ullOutOfSyncTimeStamp = 0;

    // We have to update the header even if there are no raw io changes.
    // We have to do this to save the header with new state indicating the bitmap is dirty.
    Status = CommitHeader(TRUE, pInMageStatus);
    if (!NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, 
            ("MoveRawIOChangesToBitmap: Failed to move RawIO changes(%d) and write dirty shutdown marker in bitmap file = %wZ for Volume = %s returned Status %#x\n",
                ulLastChanceChanges, m_ustrBitmapFilename, m_chDevNum, Status));
        SET_INMAGE_BITMAP_API_STATUS(pInMageStatus, INDSKFLT_ERROR_BITMAP_FILE_CANT_UPDATE_HEADER);
        m_ErrorCausingOutOfSync = *pInMageStatus;
        m_bDevInSync = FALSE; // Reset this as we failed to save the header back to bitmap file.
        return Status;
    } else {
        InVolDbgPrint(DL_TRACE_L1, DM_BITMAP_WRITE | DM_BITMAP_OPEN,
            ("MoveRawIOChangesToBitmap: Moved RawIO changes(%d) and written dirty shutdown marker in bitmap file = %wZ for Volume = %s\n",
             ulLastChanceChanges, m_ustrBitmapFilename, m_chDevNum));
    }

    return STATUS_SUCCESS;
}

ULONG64
BitmapAPI::GetDatBytesInBitmap()
{
    ULONG64 ullDataBytes = 0;
    KeWaitForMutexObject(&m_BitmapMutex, Executive, KernelMode, FALSE, NULL);

    switch(m_eBitmapState) {
    case ecBitmapStateOpened:
        if (segmentedBitmap) {
            ullDataBytes = segmentedBitmap->GetNumberOfBitsSet();
            ullDataBytes *= m_ulBitmapGranularity;
        }
        break;
    default:
        break;
    }
    KeReleaseMutex(&m_BitmapMutex, FALSE);
    return ullDataBytes;
}

#ifdef VOLUME_CLUSTER_SUPPORT
VOID 
BitmapAPI::SaveFilteringStateToBitmap(bool FilteringStopped, bool IsCleanShutdownNeeded, bool IsCommitNeeded) {
    NTSTATUS InMageStatus = STATUS_SUCCESS;

    KeWaitForMutexObject(&m_BitmapMutex, Executive, KernelMode, FALSE, NULL);

    if (FilteringStopped) {
        SetFilteringStopped();
    } else {
        SetFilteringStarted();
    }

    if (IsCleanShutdownNeeded) {
        m_BitmapHeader.header.PrevEndTimeStamp = 0;
        m_BitmapHeader.header.PrevSequenceId = 0;
        m_BitmapHeader.header.PrevEndSequenceNumber = 0;
        m_BitmapHeader.header.recoveryState = cleanShutdown;
    } else {
        m_BitmapHeader.header.recoveryState = dirtyShutdown;
    }

    if (IsCommitNeeded) {
        CommitHeader(FALSE, &InMageStatus);
    }

    KeReleaseMutex(&m_BitmapMutex, FALSE);
}


//Added for persistent time stamp
NTSTATUS
BitmapAPI::FlushTimeAndSequence(ULONGLONG TimeStamp, 
                                ULONGLONG SequenceNo, 
                                bool IsCommitNeeded)
{
    NTSTATUS InMageStatus = STATUS_SUCCESS;
    NTSTATUS Status = STATUS_SUCCESS;

    KeWaitForMutexObject(&m_BitmapMutex, Executive, KernelMode, FALSE, NULL);

    m_BitmapHeader.header.LastTimeStamp = TimeStamp;
    m_BitmapHeader.header.LastSequenceNumber = SequenceNo;

    if (IsCommitNeeded) {
        switch(m_eBitmapState) {
        case ecBitmapStateOpened:
            Status = CommitHeader(FALSE, &InMageStatus);
            if (!NT_SUCCESS(Status) || (InMageStatus != STATUS_SUCCESS)) {
                InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE , 
                    ("%s: Unable to write header with Persistent Values to device %s\n",
                    __FUNCTION__ , m_chDevNum));
            } else {
                InVolDbgPrint(DL_TRACE_L1, DM_BITMAP_WRITE ,
                    ("%s: Succeeded to write Header to bimtap file %wZ with Persistent Values to device %s\n",
                    __FUNCTION__, m_ustrBitmapFilename, m_chDevNum));
            }
            break;

        case ecBitmapStateRawIO:
            Status = CommitHeader(TRUE, &InMageStatus);
            if (!NT_SUCCESS(Status) || (InMageStatus != STATUS_SUCCESS)) {
                InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE , 
                    ("%s: Unable to write header with Persistent Values to device %s\n",
                    __FUNCTION__, m_chDevNum));
            } else {
                InVolDbgPrint(DL_TRACE_L1, DM_BITMAP_WRITE ,
                    ("%s: Succeeded to write Header to bimtap file %wZ with Persistent Values to device %s\n",
                    __FUNCTION__, m_ustrBitmapFilename, m_chDevNum));
            }
            break;

        default:
            Status = STATUS_DEVICE_NOT_READY;
            InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE , 
                ("%s: Unable to write header with Persistent Values to device %s\n",
                __FUNCTION__, m_chDevNum));
            break;

        }
    }

    KeReleaseMutex(&m_BitmapMutex, FALSE);

    return InMageStatus;
}

//Added for persistent time stamp
NTSTATUS
BitmapAPI::AddResynRequiredToBitmap(ULONG     ulOutOfSyncErrorCode, 
                                    ULONG     ulOutOfSyncErrorStatus ,
                                    ULONGLONG ullOutOfSyncTimeStamp, 
                                    ULONG     ulOutOfSyncCount,
                                    ULONG     ulOutOfSyncIndicatedAtCount)
{
    NTSTATUS InMageStatus = STATUS_SUCCESS;
    NTSTATUS Status = STATUS_SUCCESS;

    KeWaitForMutexObject(&m_BitmapMutex, Executive, KernelMode, FALSE, NULL);

    m_BitmapHeader.header.ulOutOfSyncCount = ulOutOfSyncCount;
    m_BitmapHeader.header.ulOutOfSyncErrorCode = ulOutOfSyncErrorCode;
    m_BitmapHeader.header.ulOutOfSyncErrorStatus = ulOutOfSyncErrorStatus;
    m_BitmapHeader.header.ullOutOfSyncTimeStamp = ullOutOfSyncTimeStamp;
    m_BitmapHeader.header.ulOutOfSyncIndicatedAtCount = ulOutOfSyncIndicatedAtCount;
    m_BitmapHeader.header.ControlFlags |= BITMAP_RESYNC_REQUIRED;

    switch(m_eBitmapState) {
    case ecBitmapStateOpened:
        Status = CommitHeader(FALSE, &InMageStatus);
        if (!NT_SUCCESS(Status) || (InMageStatus != STATUS_SUCCESS)) {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE , 
                ("%s: Unable to write header with Resync Required to device %s\n",
                __FUNCTION__ , m_chDevNum));
        } else {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE ,
                ("%s: Succeeded to write Header to bimtap file %wZ with Resync Required to device %s\n",
                __FUNCTION__, m_ustrBitmapFilename, m_chDevNum));
        }
        break;

    case ecBitmapStateRawIO:
        Status = CommitHeader(TRUE, &InMageStatus);
        if (!NT_SUCCESS(Status) || (InMageStatus != STATUS_SUCCESS)) {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE , 
                ("%s: Unable to write header with Resync Required to device %s\n",
                __FUNCTION__, m_chDevNum));
        } else {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE ,
                ("%s: Succeeded to write Header to bimtap file %wZ with Resync Required to device %s\n",
                __FUNCTION__, m_ustrBitmapFilename, m_chDevNum));
        }
        break;

    default:
        Status = STATUS_DEVICE_NOT_READY;
        InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE , 
            ("%s: Unable to write header with Resync Required to device %s\n",
            __FUNCTION__, m_chDevNum));
        break;
    }


    KeReleaseMutex(&m_BitmapMutex, FALSE);

    return InMageStatus;
}


//Added for persistent time stamp
NTSTATUS
BitmapAPI::ResetResynRequiredInBitmap(ULONG ulOutOfSyncCount,
                                    ULONG           ulOutOfSyncIndicatedAtCount,
                                    ULONGLONG   ullOutOfSyncIndicatedTimeStamp,
                                    ULONGLONG   ullOutOfSyncResetTimeStamp,
                                    bool        ResetResyncRequired)
{
    NTSTATUS InMageStatus = STATUS_SUCCESS;
    NTSTATUS Status = STATUS_SUCCESS;

    KeWaitForMutexObject(&m_BitmapMutex, Executive, KernelMode, FALSE, NULL);

    m_BitmapHeader.header.ulOutOfSyncCount = ulOutOfSyncCount;
    m_BitmapHeader.header.ulOutOfSyncIndicatedAtCount = ulOutOfSyncIndicatedAtCount;
    m_BitmapHeader.header.ullOutOfSyncIndicatedTimeStamp = ullOutOfSyncIndicatedTimeStamp;

    if (ResetResyncRequired) {
        m_BitmapHeader.header.ulOutOfSyncErrorCode = 0;
        m_BitmapHeader.header.ulOutOfSyncErrorStatus = 0;
        m_BitmapHeader.header.ullOutOfSyncResetTimeStamp = ullOutOfSyncResetTimeStamp;
        m_BitmapHeader.header.ControlFlags &= ~BITMAP_RESYNC_REQUIRED;
    }

    switch(m_eBitmapState) {
    case ecBitmapStateOpened:
        Status = CommitHeader(FALSE, &InMageStatus);
        if (!NT_SUCCESS(Status) || (InMageStatus != STATUS_SUCCESS)) {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE , 
                ("%s: Unable to write header with Reset Resync Required to device %s\n",
                __FUNCTION__ , m_chDevNum));
        } else {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE ,
                ("%s: Succeeded to write Header to bimtap file %wZ with Reset Resync Required to device %s\n",
                __FUNCTION__, m_ustrBitmapFilename, m_chDevNum));
        }
        break;

    case ecBitmapStateRawIO:
        Status = CommitHeader(TRUE, &InMageStatus);
        if (!NT_SUCCESS(Status) || (InMageStatus != STATUS_SUCCESS)) {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE , 
                ("%s: Unable to write header with Reset Resync Required to device %s\n",
                __FUNCTION__, m_chDevNum));
        } else {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE ,
                ("%s: Succeeded to write Header to bimtap file %wZ with Reset Resync Required to device %s\n",
                __FUNCTION__, m_ustrBitmapFilename, m_chDevNum));
        }
        break;

    default:
        Status = STATUS_DEVICE_NOT_READY;
        InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE , 
            ("%s: Unable to write header with Reset Resync Required to device %s\n",
            __FUNCTION__, m_chDevNum));
        break;
    }


    KeReleaseMutex(&m_BitmapMutex, FALSE);

    return InMageStatus;
}
#endif

// Note that this method overwrites the header with random data. Beware of data loss!
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BitmapAPI::ValidateLearntHeaderPhysicalOffsets(bool retainLogHeader)
{
    NTSTATUS        Status;
    SIZE_T          CmpResult;
    PUCHAR          RandBuffer = NULL;
    PUCHAR          CmpBuffer = NULL;
    ULONG           RandInsertOffset = 0;
    const ULONG     RandBufferLength = 16 * 1024; //16 KB
    const ULONG     LogHeaderPaddedSize = sizeof(m_BitmapHeader->sectorFill);

    PAGED_CODE();

    CmpBuffer = (PUCHAR)ExAllocatePoolWithTag(
        PagedPool,
        m_iobBitmapHeader->size,
        TAG_FILE_RAW_IO_VALIDATION);

    InVolDbgPrint(DL_INFO, DM_MEM_TRACE,
        (__FUNCTION__ ": Comparison buffer allocation %#p.\n",
        CmpBuffer));

    if (CmpBuffer == NULL) {
        Status = STATUS_NO_MEMORY;
        goto Cleanup;
    }

    // TODO-SanKumar-1805: CR Comment from Satish: We can achieve this by locking
    // the above paged pool - that would improve the performance, avoid this
    // allocation and reduce LOC.
    RandBuffer = (PUCHAR)ExAllocatePoolWithTag(
        NonPagedPool,
        RandBufferLength,
        TAG_FILE_RAW_IO_VALIDATION);

    InVolDbgPrint(DL_INFO, DM_MEM_TRACE,
        (__FUNCTION__ ": Random buffer allocation %#p.\n",
        RandBuffer));

    if (RandBuffer == NULL) {
        Status = STATUS_NO_MEMORY;
        goto Cleanup;
    }

    if (retainLogHeader) {
        C_ASSERT(sizeof(m_BitmapHeader->header) <= LogHeaderPaddedSize);

        if (m_iobBitmapHeader->size < LogHeaderPaddedSize) {
            NT_ASSERT(false);
            Status = STATUS_BUFFER_TOO_SMALL;

            InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN,
                (__FUNCTION__ ": Bitmap header IOBuffer's total size %#x shouldn't be lesser than the log header's padded size %#x. BitmapFile = %wZ. Device = %s. Status Code %#x.\n",
                m_iobBitmapHeader->size, LogHeaderPaddedSize, m_ustrBitmapFilename, m_chDevNum, Status));

            goto Cleanup;
        }

        // Retain the padded header region
        RtlCopyBytes(CmpBuffer, m_iobBitmapHeader->buffer, LogHeaderPaddedSize);
        RandInsertOffset = LogHeaderPaddedSize;
    }

    while (RandInsertOffset < m_iobBitmapHeader->size) {

        Status = BCryptGenRandom(
            NULL,
            RandBuffer,
            RandBufferLength,
            BCRYPT_USE_SYSTEM_PREFERRED_RNG);

        if (Status != STATUS_SUCCESS) {

            InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN,
                (__FUNCTION__ ": Error in generating random bytes into buffer %#p of length %#x. BitmapFile = %wZ. Device = %s. Status Code %#x.\n",
                RandBuffer, RandBufferLength, m_ustrBitmapFilename, m_chDevNum, Status));

            goto Cleanup;
        }

        RtlCopyBytes(
            CmpBuffer + RandInsertOffset,
            RandBuffer,
            min(RandBufferLength, m_iobBitmapHeader->size - RandInsertOffset));

        RandInsertOffset += RandBufferLength;
    }

    // Opportunistically free the non-paged pool buffer, which wouldn't be used after this point.
    SafeExFreePoolWithTag(RandBuffer, TAG_FILE_RAW_IO_VALIDATION);

    // Write the random bytes into the header region using File IO
    RtlCopyBytes(m_iobBitmapHeader->buffer, CmpBuffer, m_iobBitmapHeader->size);
    m_iobBitmapHeader->SetDirty();
    Status = m_iobBitmapHeader->SyncFlush();

    if (Status != STATUS_SUCCESS) {

        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN,
            (__FUNCTION__ ": Error in writing the random bytes to the header through File IO. BitmapFile = %wZ. Device = %s. Status Code %#x.\n",
            m_ustrBitmapFilename, m_chDevNum, Status));

        goto Cleanup;
    }

    // Read the header region using Raw IO
    RtlZeroBytes(m_iobBitmapHeader->buffer, m_iobBitmapHeader->size);
    m_iobBitmapHeader->SetDirty();

    Status = m_iobBitmapHeader->SyncReadPhysical();

    if (Status != STATUS_SUCCESS) {

        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN,
            (__FUNCTION__ ": Error in reading the random bytes from the header through Raw IO. BitmapFile = %wZ. Device = %s. Status Code %#x.\n",
            m_ustrBitmapFilename, m_chDevNum, Status));

        goto Cleanup;
    }

    // Compare the file written and raw read bytes
    CmpResult = RtlCompareMemory(m_iobBitmapHeader->buffer, CmpBuffer, m_iobBitmapHeader->size);

    if (CmpResult != m_iobBitmapHeader->size) {
        InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_BITMAP_VALIDATE_RAW_OFFSETS_MATCHING_FILE_IO, (&m_DevLogContext), CmpResult);

        Status = STATUS_FAIL_CHECK;

        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN,
            (__FUNCTION__ ": Comparison of data read through Raw IO against data written through File IO has failed. Matching bytes = %#x. BitmapFile = %wZ. Device = %s. Status Code %#x.\n",
            CmpResult, m_ustrBitmapFilename, m_chDevNum, Status));

        goto Cleanup;
    }

    Status = STATUS_SUCCESS;

Cleanup:
    SafeExFreePoolWithTag(RandBuffer, TAG_FILE_RAW_IO_VALIDATION);
    SafeExFreePoolWithTag(CmpBuffer, TAG_FILE_RAW_IO_VALIDATION);

    return Status;
}