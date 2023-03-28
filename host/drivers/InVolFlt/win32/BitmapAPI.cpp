// this class handles the the high level interface to the persistent bitmap and change log

//
// Copyright InMage Systems 2005
//

#include <global.h>
#include <misc.h>


BitmapAPI::BitmapAPI()

{
TRC
    m_eBitmapState = ecBitmapStateUnInitialized;
    KeInitializeMutex(&m_BitmapMutex, 0);

    m_ustrBitmapFilename.Buffer = m_wstrBitmapFilenameBuffer;
    m_ustrBitmapFilename.Length = 0;
    m_ustrBitmapFilename.MaximumLength = (MAX_LOG_PATHNAME + 1)*sizeof(WCHAR);

	//Fix For Bug 27337
	pFsInfo = NULL;

    m_bVolumeInSync = FALSE;
    m_ErrorCausingOutOfSync = STATUS_SUCCESS;
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

NTSTATUS BitmapAPI::Open(
    PUNICODE_STRING pBitmapFileName,
    ULONG   ulBitmapGranularity,
    ULONG   ulBitmapOffset,
    LONGLONG mountedSize,
    WCHAR   DriveLetter,
    ULONG   SegmentCacheLimit,
    bool    bClearOnOpen,
    BitmapPersistentValues &BitmapPersistentValue,
    NTSTATUS * pInMageOpenStatus)
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

    // For printing purpose if there is no drive letter print a question mark
    m_wcDriveLetter = DriveLetter ? DriveLetter : L'?';
    m_llVolumeSize = mountedSize;
    m_ulBitmapGranularity = ulBitmapGranularity;
    m_ulBitmapOffset = ulBitmapOffset;

    RtlCopyUnicodeString(&m_ustrBitmapFilename, pBitmapFileName);

    m_ulNumberOfBitsInBitmap = (ULONG)((m_llVolumeSize / (LONGLONG)m_ulBitmapGranularity) + 1);

    // calculate the size of a bitmap log file
    m_ulBitMapSizeInBytes = ((m_ulNumberOfBitsInBitmap + 7) / 8); // round up to full byte
    if ((m_ulBitMapSizeInBytes % BITMAP_FILE_SEGMENT_SIZE) != 0) { // adjust to full buffer
        m_ulBitMapSizeInBytes += BITMAP_FILE_SEGMENT_SIZE - (m_ulBitMapSizeInBytes % BITMAP_FILE_SEGMENT_SIZE); 
    }
    m_ulBitMapSizeInBytes += LOG_HEADER_OFFSET;

    MaxBitmapBufferRequired = min(SegmentCacheLimit * BITMAP_FILE_SEGMENT_SIZE, m_ulBitMapSizeInBytes);
    m_SegmentCacheLimit = SegmentCacheLimit;

    if ((DriverContext.CurrentBitmapBufferMemory + MaxBitmapBufferRequired) > DriverContext.MaximumBitmapBufferMemory) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, 
            ("BitmapAPI::Open - Failed for volume %C:, memory required(%#x) for bitmap exceeds global set limit %#x\n",
                    m_wcDriveLetter, m_ulBitMapSizeInBytes, DriverContext.MaximumBitmapBufferMemory));
        *pInMageOpenStatus = INVOLFLT_ERR_BITMAP_FILE_EXCEEDED_MEMORY_LIMIT;
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Cleanup_And_Return_Failure;
    }

    // check if this is a volume or file
    RtlInitUnicodeString(&namePattern, DOS_DEVICES_VOLUME_PREFIX);
    if ((RtlPrefixUnicodeString(&namePattern, pBitmapFileName, TRUE) == TRUE) &&
        (pBitmapFileName->Length == 56*sizeof(WCHAR))) 
    {
        // log is a raw volume
        InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, ("BitmapAPI::Open - Using raw volume %wZ as bitmap file for volume %C:\n",
                                    pBitmapFileName, m_wcDriveLetter));
        llInitialBitmapFileSize = 0;
        ulCreateDisposition = FILE_OPEN;    // Fail if file already does not exist
        ulSharedAccessToBitmap = FILE_SHARE_READ | FILE_SHARE_WRITE;
		//Fix for bug 24005 - Since it's a raw volume open we don't really care about other CreateOption flags.
		ulCreateOptions = FILE_NO_INTERMEDIATE_BUFFERING;
    } else {
        // log is a file
        InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, ("BitmapAPI::Open - Using file %wZ as bitmap file for volume %C:\n",
                                pBitmapFileName, m_wcDriveLetter));
        // Set the m_ulBitmapOffset to zero, the bitmap is a file on volume with file system. offset is only used for
        // bitmaps on raw volumes, this will help us to save multiple bitmaps on one raw volume.
        m_ulBitmapOffset = 0;
        llInitialBitmapFileSize = m_ulBitMapSizeInBytes;
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
        *pInMageOpenStatus = INVOLFLT_ERR_NO_MEMORY;
        Status = STATUS_NO_MEMORY;
        goto Cleanup_And_Return_Failure;
    }

    //Fix for bug 24005
    Status = m_fsBitmapStream->Open(pBitmapFileName, FILE_READ_DATA | FILE_WRITE_DATA | DELETE,
            ulCreateDisposition, ulCreateOptions, ulSharedAccessToBitmap, llInitialBitmapFileSize);
    if (!NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("BitmapAPI::Open - Unable to open volume bitmap %wZ - Status = %#x\n",
                                        pBitmapFileName, Status));
        *pInMageOpenStatus = INVOLFLT_ERR_BITMAP_FILE_CANT_OPEN;
        goto Cleanup_And_Return_Failure;
    } 

    m_eBitmapState = ecBitmapStateOpened;
    Status = LoadBitmapHeaderFromFileStream(pInMageOpenStatus, bClearOnOpen, BitmapPersistentValue); 

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
        *pInMageOpenStatus = INVOLFLT_ERR_NO_MEMORY;
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("BitmapAPI::Open - Allocation of FileStreamSegmentMapper failed\n"));
        goto Cleanup_And_Return_Failure;
    }
    segmentMapper->Attach(m_fsBitmapStream, m_ulBitmapOffset + LOG_HEADER_OFFSET, (m_ulNumberOfBitsInBitmap / 8) + 1, SegmentCacheLimit);

    // layer the SegmentedBitmap over the FileStreamSegmentMapper
    segmentedBitmap = new SegmentedBitmap(segmentMapper, m_ulNumberOfBitsInBitmap);
    InVolDbgPrint(DL_INFO, DM_MEM_TRACE, ("BitmapAPI::Open - Allocation, segmentedBitmap = %#p\n", segmentedBitmap));
    if (NULL == segmentedBitmap) {
        Status = STATUS_NO_MEMORY;
        *pInMageOpenStatus = INVOLFLT_ERR_NO_MEMORY;
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
    if (NULL != m_iobBitmapHeader) {
        m_iobBitmapHeader->Release();
        m_iobBitmapHeader = NULL;
    }

    if (NULL != m_fsBitmapStream) {
        m_fsBitmapStream->Close();
        m_fsBitmapStream->Release();
        m_fsBitmapStream = NULL;
    }
    if (*pInMageOpenStatus != INVOLFLT_ERR_BITMAP_FILE_CANT_OPEN)
        InMageFltLogError(DriverContext.ControlDevice, __LINE__, INVOLFLT_WARNING_BITMAPSTATE6, STATUS_SUCCESS, Status, *pInMageOpenStatus);

    m_eBitmapState = ecBitmapStateUnInitialized;
    KeReleaseMutex(&m_BitmapMutex, FALSE);
    return Status;
}

NTSTATUS BitmapAPI::IsVolumeInSync(BOOLEAN *pVolumeInSync, NTSTATUS *pOutOfSyncErrorCode)
{
    NTSTATUS    Status = STATUS_SUCCESS;

    if (NULL == pVolumeInSync)
        return STATUS_INVALID_PARAMETER;

    // keep concurrent threads form conflicting
    KeWaitForMutexObject(&m_BitmapMutex, Executive, KernelMode, FALSE, NULL);
    
    switch(m_eBitmapState) {
    case ecBitmapStateOpened:
    case ecBitmapStateRawIO:
        *pVolumeInSync = m_bVolumeInSync;
        *pOutOfSyncErrorCode = m_ErrorCausingOutOfSync;
        InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, ("IsVolumeInSync: returning %s for volumesync with error %#x\n",
            m_bVolumeInSync ? "TRUE" : "FALSE", m_ErrorCausingOutOfSync));
        break;
    default:
        ASSERTMSG("IsVolumeInSync Called in non-opened state\n", FALSE);
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

NTSTATUS BitmapAPI::Close(BitmapPersistentValues &BitmapPersistentValue, bool bDelete, NTSTATUS *pInMageCloseStatus)
{
TRC
    NTSTATUS Status = STATUS_SUCCESS;


    // keep concurrent threads form conflicting
    KeWaitForMutexObject(&m_BitmapMutex, Executive, KernelMode, FALSE, NULL);
    switch(m_eBitmapState) {
    case ecBitmapStateOpened:
    case ecBitmapStateRawIO:
        Status = CommitBitmapInternal(pInMageCloseStatus, BitmapPersistentValue);
        break;
    default:
        ASSERTMSG("IsVolumeInSync Called in non-opened state\n", FALSE);
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
        InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE, ("BitmapAPI::Close - Clean shutdown marker written for volume %C\n", 
            m_wcDriveLetter));
    } else {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE, ("BitmapAPI::Close - Error writing clean shutdown marker for volume %C, Status = %#x\n", 
            m_wcDriveLetter, Status));
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

__inline NTSTATUS BitmapAPI::SetWriteSizeNotToExceedVolumeSize(LONGLONG &llOffset, ULONG &ulLength)
{
    if ((ulLength + llOffset) > m_llVolumeSize) {
        if (llOffset < m_llVolumeSize) {
            InVolDbgPrint(DL_INFO, DM_BITMAP_READ, ("Correcting Length from %#x to %#x\n",
                ulLength, (ULONG32)(m_llVolumeSize - llOffset)));
            ulLength = (ULONG32)(m_llVolumeSize - llOffset);
        } else {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_READ, ("Offset %#I64x is beyond volume size %#I64x\n",
                llOffset, m_llVolumeSize));
            return STATUS_ARRAY_BOUNDS_EXCEEDED;
        }
    }

    return STATUS_SUCCESS;
}

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


NTSTATUS BitmapAPI::SetBitRun(BitRuns * bitRuns, LONGLONG runOffsetArray, ULONG runLengthArray)
{
TRC
    NTSTATUS    Status = STATUS_SUCCESS;
    ULONGLONG   ullScaledOffset, ullScaledSize;
    
    ASSERTMSG("BitmapAPI::SetBits Offset is beyond end of volume\n", (runOffsetArray < m_llVolumeSize));
    ASSERTMSG("BitmapAPI::SetBits Offset is negative\n", (runOffsetArray >= 0));
    ASSERTMSG("BitmapAPI::SetBits detected bit run past volume end\n",
        ((runOffsetArray + runLengthArray) <= m_llVolumeSize));

    GetScaledOffsetAndSizeFromDiskChange(runOffsetArray, runLengthArray, &ullScaledOffset, &ullScaledSize);

    // verify the data is within bounds
    if ((runOffsetArray < m_llVolumeSize) && (runOffsetArray >= 0) &&
        ((runOffsetArray + runLengthArray) <= m_llVolumeSize))
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
                if (index == ulElements) {
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
                if (index == ulElements) {
                    ChangeBlock = ChangeBlock->Next;
                    ASSERT (NULL != ChangeBlock);
                    ulElements = ChangeBlock->usLastIndex - ChangeBlock->usFirstIndex + 1;
                    index = 0;
                }
                runOffset = ChangeBlock->ChangeOffset[index];
                runLength = ChangeBlock->ChangeLength[index];
            }


            ASSERTMSG("BitmapAPI::SetBits Offset is beyond end of volume\n",
                (runOffset < m_llVolumeSize));
            ASSERTMSG("BitmapAPI::SetBits Offset is negative\n",
                (runOffset >= 0));
            ASSERTMSG("BitmapAPI::SetBits detected bit run past volume end\n",
                ((runOffset + runLength) <= m_llVolumeSize));

            InVolDbgPrint(DL_INFO, DM_BITMAP_WRITE, ("SetBits: Writing using RawIO, Offset = %#I64x Length = %#x\n",
                        runOffset, runLength));

            if (m_BitmapHeader.header.lastChanceChanges == (MAX_WRITE_GROUPS_IN_BITMAP_HEADER * MAX_CHANGES_IN_WRITE_GROUP)) {
                m_BitmapHeader.header.changesLost += bitRuns->nbrRuns - ulCurrentRun;
                InVolDbgPrint(DL_WARNING, DM_BITMAP_WRITE, ("SetBits: Incremented lost changes to %#x\n",
                            m_BitmapHeader.header.changesLost));
                break;
            }

            GetScaledOffsetAndSizeFromDiskChange(runOffset, runLength, &ullScaledOffset, &ullScaledSize);

            // if the run is very large we have to chop it up into smaller pieces
            while ((ullScaledSize > 0) && 
                    (m_BitmapHeader.header.lastChanceChanges < (MAX_WRITE_GROUPS_IN_BITMAP_HEADER * MAX_CHANGES_IN_WRITE_GROUP))) 
            {
                // Required for 64 bit shift operations
                m_BitmapHeader.changeGroup[m_BitmapHeader.header.lastChanceChanges / 
                                        MAX_CHANGES_IN_WRITE_GROUP].lengthOffsetPair[m_BitmapHeader.header.lastChanceChanges % 
                                                                                MAX_CHANGES_IN_WRITE_GROUP] =
                        (min(ullScaledSize,0xFFFF) << 48) | (ullScaledOffset & 0xFFFFFFFFFFFF);

                InVolDbgPrint(DL_INFO, DM_BITMAP_WRITE, ("SetBits: WriteRaw: ScaledOffset = %#I64x, ScaledSize = %#I64x, Granularity = %#x\n",
                            (ullScaledOffset & 0xFFFFFFFFFFFF), min(ullScaledSize, 0xFFFF), m_ulBitmapGranularity));
                ullScaledOffset += min(ullScaledSize,0xFFFF);
                ullScaledSize -= min(ullScaledSize,0xFFFF);
                m_BitmapHeader.header.lastChanceChanges++;
            }

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
                (bitRuns->runOffsetArray[ulCurrentRun] < m_llVolumeSize));
            ASSERTMSG("BitmapAPI::SetBits Offset is negative\n",
                (bitRuns->runOffsetArray[ulCurrentRun] >= 0));
            ASSERTMSG("BitmapAPI::SetBits detected bit run past volume end\n",
                ((bitRuns->runOffsetArray[ulCurrentRun] + bitRuns->runLengthArray[ulCurrentRun]) <= m_llVolumeSize));

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
            Status = SetWriteSizeNotToExceedVolumeSize(bitRuns->runOffsetArray[ulCurrentRun], bitRuns->runLengthArray[ulCurrentRun]);

		//Fix For Bug 27337
		if (NT_SUCCESS(Status)){
			Status = SetUserReadSizeNotToExceedFsSize(bitRuns->runOffsetArray[ulCurrentRun],
					bitRuns->runLengthArray[ulCurrentRun]);
		}

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
                    Status = SetWriteSizeNotToExceedVolumeSize(bitRuns->runOffsetArray[ulCurrentRun], bitRuns->runLengthArray[ulCurrentRun]);

				//Fix For Bug 27337
				if (NT_SUCCESS(Status)){
					Status = SetUserReadSizeNotToExceedFsSize(bitRuns->runOffsetArray[ulCurrentRun],
					bitRuns->runLengthArray[ulCurrentRun]);
				}

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
        InMageFltLogError(DriverContext.ControlDevice, __LINE__, INVOLFLT_WARNING_BITMAPSTATE2, STATUS_SUCCESS, m_eBitmapState);
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
                Status = SetWriteSizeNotToExceedVolumeSize(bitRuns->runOffsetArray[ulCurrentRun], bitRuns->runLengthArray[ulCurrentRun]);

			//Fix For Bug 27337
		if (NT_SUCCESS(Status)){
			Status = SetUserReadSizeNotToExceedFsSize(bitRuns->runOffsetArray[ulCurrentRun],
					 bitRuns->runLengthArray[ulCurrentRun]);
		}

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
        InMageFltLogError(DriverContext.ControlDevice, __LINE__, INVOLFLT_WARNING_BITMAPSTATE3, STATUS_SUCCESS, m_eBitmapState);
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
        Status = INVOLFLT_ERR_DELETE_BITMAP_FILE_NO_NAME;
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
    MD5Update(&ctx, (PUCHAR)(&(header->header.endian)), HEADER_CHECKSUM_DATA_SIZE);
    MD5Final(actualChecksum, &ctx);

    switch (header->header.version) {
    case BITMAP_FILE_VERSION_V2:
        bVerified = ((header->header.endian == BITMAP_FILE_ENDIAN_FLAG) &&
                     (header->header.headerSize == DISK_SECTOR_SIZE) &&
                     (header->header.dataOffset == LOG_HEADER_OFFSET) &&
                     (header->header.bitmapOffset == m_ulBitmapOffset) &&
                     (header->header.bitmapSize == m_ulNumberOfBitsInBitmap) &&
                     (header->header.bitmapGranularity == m_ulBitmapGranularity) &&
                     (header->header.volumeSize == m_llVolumeSize) &&
                     (RtlCompareMemory(header->header.validationChecksum,actualChecksum,HEADER_CHECKSUM_SIZE) == HEADER_CHECKSUM_SIZE));
        // Added for OOD Support
        if (!(header->header.FieldFlags & BITMAP_HEADER_OOD_SUPPORT)) {
            header->header.FieldFlags |= BITMAP_HEADER_OOD_SUPPORT;
        }
        break;
    case BITMAP_FILE_VERSION:
        bVerified = ((header->header.endian == BITMAP_FILE_ENDIAN_FLAG) &&
                     (header->header.headerSize == sizeof(tLogHeader) - BITMAP_FILE_VERSION_HEADER_SIZE_INCREASE) &&
                     (header->header.dataOffset == LOG_HEADER_OFFSET) &&
                     (header->header.bitmapOffset == m_ulBitmapOffset) &&
                     (header->header.bitmapSize == m_ulNumberOfBitsInBitmap) &&
                     (header->header.bitmapGranularity == m_ulBitmapGranularity) &&
                     (header->header.volumeSize == m_llVolumeSize) &&
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
            CalculateHeaderIntegrityChecksums(&m_BitmapHeader);
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
    unsigned char actualChecksum[16];
    MD5Context ctx;

    // calculate the checksum
    MD5Init(&ctx);
    MD5Update(&ctx, (PUCHAR)(&(header->header.endian)), HEADER_CHECKSUM_DATA_SIZE);
    MD5Final(header->header.validationChecksum, &ctx);

}

NTSTATUS
BitmapAPI::ChangeBitmapModeToRawIO()
{
    NTSTATUS    Status = STATUS_SUCCESS;

    ASSERT(NULL != m_iobBitmapHeader);

    KeWaitForMutexObject(&m_BitmapMutex, Executive, KernelMode, FALSE, NULL);

    switch(m_eBitmapState) {
    case ecBitmapStateOpened:
        ASSERT(NULL != m_fsBitmapStream);
        if (NULL != m_fsBitmapStream) {
            // Find the physical Offset.
            // Currently SetBits and ClearBits flushes the buffers synchronusly to disk.
            // So we do not have to flush them here.
            segmentedBitmap->SyncFlushAll(); // make sure all the changes are saved
            m_iobBitmapHeader->SetDirty();
            m_iobBitmapHeader->LearnPhysicalIO();

            m_eBitmapState = ecBitmapStateRawIO;

            m_fsBitmapStream->Close();
            m_fsBitmapStream->Release();
            m_fsBitmapStream = NULL;
            m_iobBitmapHeader->SetFileStream(NULL);
            InVolDbgPrint(DL_TRACE_L1, DM_BITMAP_OPEN | DM_BITMAP_WRITE, 
                          ("ChangeBitmapModeToRawIO: Volume %C: Changed bitmap mode to RawIO\n", m_wcDriveLetter));
        } else {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN | DM_BITMAP_WRITE, 
                          ("ChangeBitmapModeToRawIO: Volume %C: BitmapStream is NULL\n", m_wcDriveLetter));
            Status = STATUS_FILE_CLOSED;
        }
        break;
    case ecBitmapStateRawIO:
        InVolDbgPrint(DL_TRACE_L1, DM_BITMAP_OPEN | DM_BITMAP_WRITE, 
                      ("ChangeBitmapModeToRawIO: Volume %C: Already in rawIO mode\n", m_wcDriveLetter));
        break;
    default:
        InMageFltLogError(DriverContext.ControlDevice, __LINE__, INVOLFLT_WARNING_BITMAPSTATE4, STATUS_SUCCESS, m_eBitmapState);
        Status = STATUS_DEVICE_NOT_READY;
        break;
    }

    KeReleaseMutex(&m_BitmapMutex, FALSE);

    return Status;
}

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
BitmapAPI::CommitBitmap(BitmapPersistentValues &BitmapPersistentValue, NTSTATUS *pInMageCloseStatus )
{
    NTSTATUS    Status = STATUS_SUCCESS;

    KeWaitForMutexObject(&m_BitmapMutex, Executive, KernelMode, FALSE, NULL);
    Status = CommitBitmapInternal(pInMageCloseStatus, BitmapPersistentValue);
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
        if (pWriteMetaData->llOffset < m_llVolumeSize) {
            if ((pWriteMetaData->llOffset + pWriteMetaData->ulLength) <= m_llVolumeSize) {
                Status = segmentedBitmap->SetBitRun((UINT32)ullScaledSize, ullScaledOffset);
                segmentedBitmap->SyncFlushAll(); // make sure all the changes are saved
            } else {
                // change runs past end of volume
                InVolDbgPrint(DL_WARNING, DM_BITMAP_WRITE, ("SaveWriteMetaDataToBitmap: write offset(%#I64x) and Length(%#lx) past the end of volume size(%#I64x)\n",
                                pWriteMetaData->llOffset, pWriteMetaData->ulLength, m_llVolumeSize));
            }
        } else {
            // change starts past end of volume
            InVolDbgPrint(DL_WARNING, DM_BITMAP_WRITE, ("SaveWriteMetaDataToBitmap: write offset(%#I64x) past the end of volume size(%#I64x)\n",
                            pWriteMetaData->llOffset, m_llVolumeSize));
        }
        break;
    case ecBitmapStateRawIO:
        ASSERT((NULL != m_iobBitmapHeader) &&(NULL != m_iobBitmapHeader->buffer));

        InVolDbgPrint(DL_INFO, DM_BITMAP_WRITE, ("SaveWriteMetaDataToBitmap: Using raw mode to write Offset = %#I64x Length = %#x\n",
                                pWriteMetaData->llOffset, pWriteMetaData->ulLength));

        if (m_BitmapHeader.header.lastChanceChanges >= (MAX_WRITE_GROUPS_IN_BITMAP_HEADER * MAX_CHANGES_IN_WRITE_GROUP)) {
            // if number of changes in m_BitmapHeader exceed the maximum Value.
            m_BitmapHeader.header.changesLost += 1;
            InVolDbgPrint(DL_WARNING, DM_BITMAP_WRITE, ("SaveWriteMetaDataToBitmap: Number of Changes in logheader exceeded max limit.\n"));
        } else {
            GetScaledOffsetAndSizeFromWriteMetadata(pWriteMetaData, &ullScaledOffset, &ullScaledSize);

            // if the run is very large we have to chop it up into smaller pieces
            while (ullScaledSize > 0) {
                if (m_BitmapHeader.header.lastChanceChanges >= (MAX_WRITE_GROUPS_IN_BITMAP_HEADER * MAX_CHANGES_IN_WRITE_GROUP)) {
                    m_BitmapHeader.header.changesLost += 1;
                    InVolDbgPrint(DL_WARNING, DM_BITMAP_WRITE, ("SaveWriteMetaDataToBitmap: Number of Changes in logheader exceeded max limit.\n"));
                    break;
                } else {
                    m_BitmapHeader.changeGroup[m_BitmapHeader.header.lastChanceChanges / 
                                    MAX_CHANGES_IN_WRITE_GROUP].lengthOffsetPair[m_BitmapHeader.header.lastChanceChanges % 
                                                                            MAX_CHANGES_IN_WRITE_GROUP] =
                                    (min(ullScaledSize,0xFFFF) << 48) | (ullScaledOffset & 0xFFFFFFFFFFFF);
                    InVolDbgPrint(DL_INFO, DM_BITMAP_WRITE, ("SaveWriteMetaDataToBitmap: ScaledOffset = %#I64x, ScaledSize = %#I64x, Granularity = %#x\n",
                            (ullScaledOffset & 0xFFFFFFFFFFFF), min(ullScaledSize, 0xFFFF), m_ulBitmapGranularity));
                    m_BitmapHeader.header.lastChanceChanges++;
                    ullScaledOffset += min(ullScaledSize,0xFFFF);
                    ullScaledSize -= min(ullScaledSize,0xFFFF);
                }
            }

            // Let us write this header to bitmap.
            CalculateHeaderIntegrityChecksums(&m_BitmapHeader);

            if ((NULL != m_iobBitmapHeader) && (NULL != m_iobBitmapHeader->buffer)) {
                RtlCopyMemory(m_iobBitmapHeader->buffer,&m_BitmapHeader, sizeof(m_BitmapHeader));
                m_iobBitmapHeader->SetDirty();
                Status = m_iobBitmapHeader->SyncFlushPhysical(); // write the header
            } else {
                Status = STATUS_DEVICE_NOT_READY;
            }
        } // else of logheader is full for changes.
        break;
    default:
        InMageFltLogError(DriverContext.ControlDevice, __LINE__, INVOLFLT_WARNING_BITMAPSTATE5, STATUS_SUCCESS, m_eBitmapState);
        Status = STATUS_DEVICE_NOT_READY;
        break;
    }
    
    KeReleaseMutex(&m_BitmapMutex, FALSE);

    return Status;
}

/*-----------------------------------------------------------------------------
 * Protected Routines - Will not be called by user.
 * These routines are called with mutex acquired.
 *-----------------------------------------------------------------------------
 */

NTSTATUS BitmapAPI::ReadAndVerifyBitmapHeader(NTSTATUS *pInMageStatus)
{
    NTSTATUS    Status = STATUS_SUCCESS;

    // do this VERY carefully, read and validate destination before we write
    Status = m_iobBitmapHeader->SyncReadPhysical(TRUE); // read the buffer direct from disk
    if (NT_SUCCESS(Status)) {
        if (VerifyHeader((tBitmapHeader *)(m_iobBitmapHeader->buffer))) {
            // our addressing seems to be correct, so proceed with writing
            return STATUS_SUCCESS;
        } else {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE, ("ReadAndVerifyBitmapHeader: m_iobBitmapHeader verification failed\n"));
            if (pInMageStatus)
                *pInMageStatus = INVOLFLT_ERR_FINAL_HEADER_VALIDATE_FAILED;
        }
    } else {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE, ("ReadAndVerifyBitmapHeader: m_iobBitmapHeader physical read failed with Status %#lx\n", Status));
        if (pInMageStatus)
            *pInMageStatus = INVOLFLT_ERR_FINAL_HEADER_READ_FAILED;
    }

    return Status;
}

NTSTATUS BitmapAPI::CommitHeader(BOOLEAN bVerifyExistingHeaderInRawIO, NTSTATUS *pInMageStatus)
{
TRC
    NTSTATUS Status = STATUS_SUCCESS;

    ASSERT(LOG_HEADER_SIZE >= sizeof(m_BitmapHeader));
    ASSERT((NULL != m_iobBitmapHeader) && (NULL != m_iobBitmapHeader->buffer));

    switch(m_eBitmapState) {
    case ecBitmapStateOpened:
        CalculateHeaderIntegrityChecksums(&m_BitmapHeader);
        RtlCopyMemory(m_iobBitmapHeader->buffer, &m_BitmapHeader, sizeof(m_BitmapHeader));
        m_iobBitmapHeader->SetDirty();
        Status = m_iobBitmapHeader->SyncFlush(); // write the header
        if (!NT_SUCCESS(Status)) {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE, ("CommitHeader: writing m_iobBitmapHeader with FS to volume failed with Status %#lx\n", Status));
            if (pInMageStatus)
                *pInMageStatus = INVOLFLT_ERR_FINAL_HEADER_FS_WRITE_FAILED;
			InMageFltLogError(DriverContext.ControlDevice, __LINE__, INVOLFLT_FS_WRITE_ON_BITMAP_HEADER_FAIL, STATUS_SUCCESS, (ULONG)Status);
		} else {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE, ("CommitHeader: writing m_iobBitmapHeader with FS to volume succeeded with Status %#lx\n", Status));

		}
        break;
    case ecBitmapStateRawIO:
        if (bVerifyExistingHeaderInRawIO) {
            Status = ReadAndVerifyBitmapHeader(pInMageStatus);
        } else {
            Status = STATUS_SUCCESS;
        }

        if (NT_SUCCESS(Status)) {
            CalculateHeaderIntegrityChecksums(&m_BitmapHeader);

            RtlCopyMemory(m_iobBitmapHeader->buffer,&m_BitmapHeader, sizeof(m_BitmapHeader));
            m_iobBitmapHeader->SetDirty();
            Status = m_iobBitmapHeader->SyncFlushPhysical(); // write the header
            if (!NT_SUCCESS(Status)) {
                InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE, ("CommitHeader: m_iobBitmapHeader raw write failed with Status %#lx\n", Status));
                if (pInMageStatus)
                    *pInMageStatus = INVOLFLT_ERR_FINAL_HEADER_DIRECT_WRITE_FAILED;
				InMageFltLogError(DriverContext.ControlDevice, __LINE__, INVOLFLT_RAW_WRITE_ON_BITMAP_HEADER_FAIL, STATUS_SUCCESS, (ULONG)Status);
			} else {
				InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE, ("CommitHeader: m_iobBitmapHeader raw write succeeded with Status %#lx\n", Status));
			}
        }
        break;
    default:
        InMageFltLogError(DriverContext.ControlDevice, __LINE__, INVOLFLT_WARNING_BITMAPSTATE1, STATUS_SUCCESS, m_eBitmapState);
        Status = STATUS_DEVICE_NOT_READY;
        break;
    }

    return Status;
}

NTSTATUS
BitmapAPI::CommitBitmapInternal(NTSTATUS *pInMageStatus, BitmapPersistentValues &BitmapPersistentValue)
{
    ASSERT((NULL != m_iobBitmapHeader) && (NULL != m_iobBitmapHeader->buffer));
    NTSTATUS    Status = STATUS_SUCCESS;

    switch(m_eBitmapState) {
    case ecBitmapStateOpened:
        m_BitmapHeader.header.recoveryState = cleanShutdown;
        if (BitmapPersistentValue.IsClusterVolume()) {
            m_BitmapHeader.header.LastSequenceNumber = BitmapPersistentValue.GetGlobalSequenceNumber();
            m_BitmapHeader.header.LastTimeStamp = BitmapPersistentValue.GetGlobalTimeStamp();
            m_BitmapHeader.header.PrevEndTimeStamp = BitmapPersistentValue.GetPrevEndTimeStamp();
            m_BitmapHeader.header.PrevEndSequenceNumber = BitmapPersistentValue.GetPrevEndSequenceNumber();
            m_BitmapHeader.header.PrevSequenceId = BitmapPersistentValue.GetPrevSequenceId();
        }
        segmentedBitmap->SyncFlushAll();
        Status = CommitHeader(FALSE, pInMageStatus);
        if (!NT_SUCCESS(Status)) {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE | DM_BITMAP_OPEN, 
                          ("CommitBitmapInternal: Unable to write header with FS to volume %C:, Status = %#x\n", m_wcDriveLetter, Status));
            Status = INVOLFLT_ERR_FINAL_HEADER_FS_WRITE_FAILED;
        } else {
            InVolDbgPrint(DL_TRACE_L1, DM_BITMAP_WRITE | DM_BITMAP_OPEN,
                ("CommitBitmapInternal: Succeeded to write clean shutdown marker to bimtap file %wZ with FS to volume %C:\n",
                 m_ustrBitmapFilename, m_wcDriveLetter));
        }
        break;
    case ecBitmapStateRawIO:
        if (m_iobBitmapHeader->IsDirty() || (cleanShutdown != m_BitmapHeader.header.recoveryState)) {
            m_BitmapHeader.header.recoveryState = cleanShutdown;
           if (BitmapPersistentValue.IsClusterVolume()) {
                m_BitmapHeader.header.LastSequenceNumber = BitmapPersistentValue.GetGlobalSequenceNumber();
                m_BitmapHeader.header.LastTimeStamp = BitmapPersistentValue.GetGlobalTimeStamp();
                m_BitmapHeader.header.PrevEndTimeStamp = BitmapPersistentValue.GetPrevEndTimeStamp();
                m_BitmapHeader.header.PrevEndSequenceNumber = BitmapPersistentValue.GetPrevEndSequenceNumber();
                m_BitmapHeader.header.PrevSequenceId = BitmapPersistentValue.GetPrevSequenceId();
            }
            Status = CommitHeader(TRUE, pInMageStatus);
            if (!NT_SUCCESS(Status)) {
                InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE | DM_BITMAP_OPEN, 
                              ("CommitBitmapInternal: Unable to write header with raw IO to volume %C:, Status = %#x\n", m_wcDriveLetter, Status));
                Status = INVOLFLT_ERR_FINAL_HEADER_FS_WRITE_FAILED;
            } else {
                InVolDbgPrint(DL_TRACE_L1, DM_BITMAP_WRITE | DM_BITMAP_OPEN,
                    ("CommitBitmapInternal: Succeeded to write clean shutdown marker to bimtap file %wZ with raw IO to volume %C:\n",
                     m_ustrBitmapFilename, m_wcDriveLetter));
            }
        } else {
            // Already bitmap has clean shutdown marker.
            InVolDbgPrint( DL_TRACE_L1, DM_BITMAP_OPEN | DM_BITMAP_WRITE,
                           ("CommitBitmapInternal: Volume %C already has clean shutdown marker and bitmapheader is not dirty\n", m_wcDriveLetter));
        }

        break;
    default:
        Status = STATUS_DEVICE_NOT_READY;
        break;
    }

    return Status;
}

NTSTATUS BitmapAPI::InitBitmapFile(NTSTATUS *pInMageStatus, BitmapPersistentValues &BitmapPersistentValue)
{
TRC
    NTSTATUS Status;
    bool IsVolOnClus, VolFilteringState;

    Status = FastZeroBitmap();
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    // init the new header
    RtlZeroMemory(m_BitmapHeader.sectorFill, DISK_SECTOR_SIZE);
    m_BitmapHeader.header.endian = BITMAP_FILE_ENDIAN_FLAG; // little endian    UINT32 endian; // read as 0xFF is little endian and 0xFF000000 means big endian
    m_BitmapHeader.header.headerSize = DISK_SECTOR_SIZE;
    m_BitmapHeader.header.version = BITMAP_FILE_VERSION_V2;   // 0x10000 was initial version
    m_BitmapHeader.header.dataOffset = LOG_HEADER_OFFSET;
    m_BitmapHeader.header.bitmapOffset = m_ulBitmapOffset;
    m_BitmapHeader.header.bitmapSize = m_ulNumberOfBitsInBitmap;
    m_BitmapHeader.header.bitmapGranularity = m_ulBitmapGranularity;
    m_BitmapHeader.header.volumeSize = m_llVolumeSize;
    m_BitmapHeader.header.recoveryState = dirtyShutdown; // unless we shutdown clean, assume dirty
    m_BitmapHeader.header.lastChanceChanges = 0;
    m_BitmapHeader.header.bootCycles = 0;
    m_BitmapHeader.header.changesLost = 0;
    m_BitmapHeader.header.LastTimeStamp = 0;
    m_BitmapHeader.header.LastSequenceNumber = 0;
    m_BitmapHeader.header.ControlFlags = 0;
    m_BitmapHeader.header.FieldFlags = 0;
    m_BitmapHeader.header.PrevEndTimeStamp = 0;
    m_BitmapHeader.header.PrevEndSequenceNumber = 0;
    m_BitmapHeader.header.PrevSequenceId = 0;
    m_BitmapHeader.header.FieldFlags |= BITMAP_HEADER_OOD_SUPPORT;

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
    for(i = m_ulBitmapOffset + LOG_HEADER_OFFSET; i < (m_ulBitmapOffset + m_ulBitMapSizeInBytes); i += BITMAP_FILE_SEGMENT_SIZE) {
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


NTSTATUS BitmapAPI::LoadBitmapHeaderFromFileStream(NTSTATUS *pInMageOpenStatus, bool bClearOnOpen, BitmapPersistentValues &BitmapPersistentValue)
{
    NTSTATUS    Status = STATUS_SUCCESS;

    ASSERT(NULL != m_fsBitmapStream);

    *pInMageOpenStatus = STATUS_SUCCESS;
    // Set initial state of sync to lost sync. If bitmap header is verified successfully
    // we will set the sync state to TRUE.
    m_bVolumeInSync = FALSE;
    m_ErrorCausingOutOfSync = STATUS_SUCCESS;

    // setup the header. We are creating one IOBuffer with 16K Size?
    m_iobBitmapHeader = new (LOG_HEADER_SIZE,0) IOBuffer;
    if (NULL == m_iobBitmapHeader) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, 
            ("LoadBitmapHeaderFromFileStream: Memory allocation failed for bitmap header buffer. BitmapFile = %wZ, Volume = %C:\n",
                m_ustrBitmapFilename, m_wcDriveLetter));
        *pInMageOpenStatus = m_ErrorCausingOutOfSync = INVOLFLT_ERR_NO_MEMORY;
        return STATUS_NO_MEMORY;
    }

    m_iobBitmapHeader->SetFileStream(m_fsBitmapStream);
    m_iobBitmapHeader->SetFileOffset(m_ulBitmapOffset + 0); // header is at start of log area

    if (bClearOnOpen) {
        m_bVolumeInSync = TRUE;
    } else if (FALSE == m_fsBitmapStream->WasCreated()) {
        Status = m_iobBitmapHeader->SyncRead(); // get the header
        if (!NT_SUCCESS(Status) && (Status != STATUS_END_OF_FILE)) {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, 
                ("LoadBitmapHeaderFromFileStream: Header buffer read failed with Status %#x, BitmapFile = %wZ, Volume = %C:\n",
                    Status, m_ustrBitmapFilename, m_wcDriveLetter));
            *pInMageOpenStatus = m_ErrorCausingOutOfSync = INVOLFLT_ERR_BITMAP_FILE_CANT_READ;
            goto Cleanup_And_Return_Failure;
        }
        
        if (Status == STATUS_END_OF_FILE) {
            // If the file size was zero length, reading header returns STATUS_END_OF_FILE.
            InVolDbgPrint(DL_WARNING, DM_BITMAP_OPEN, 
                ("LoadBitmapHeaderFromFileStream: Header buffer return STATUS_END_OF_FILE, BitmapFile = %wZ, Volume = %C:\n",
                    m_ustrBitmapFilename, m_wcDriveLetter));
            m_bEmptyBitmapFile = TRUE;
            *pInMageOpenStatus = m_ErrorCausingOutOfSync = INVOLFLT_ERR_BITMAP_FILE_LOG_FIXED;
        } else {
            ASSERT(NT_SUCCESS(Status));

            RtlCopyMemory(&m_BitmapHeader, m_iobBitmapHeader->buffer, sizeof(m_BitmapHeader));
            if (VerifyHeader(&m_BitmapHeader, true)) {
                // header valid for current volume
                if (m_BitmapHeader.header.recoveryState == cleanShutdown) {
                    // everything is good
                    InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, 
                        ("LoadBitmapHeaderFromFileStream: Bitmap file %wZfor\nvolume %C: indicates normal previous shutdown\n",
                            &m_ustrBitmapFilename, m_wcDriveLetter));
                    m_bVolumeInSync = TRUE;
                } else {
                    // we crashed or shutdown without proper closing
                    InVolDbgPrint(DL_WARNING, DM_BITMAP_OPEN, 
                        ("LoadBitmapHeaderFromFileStream: Bitmap file %wZ for\nvolume %C: indicates unexpected previous shutdown\n",
                            &m_ustrBitmapFilename, m_wcDriveLetter));
                    *pInMageOpenStatus = m_ErrorCausingOutOfSync = INVOLFLT_ERR_LOST_SYNC_SYSTEM_CRASHED;
                }

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
                m_BitmapHeader.header.bootCycles++;

                // modified header is saved after rawIO changes are moved into bitmap.
                return STATUS_SUCCESS;
            }
            
            // Verify header has failed, bitmap file is corrupt.
            // leave VolumeSyncLost to TRUE and init bitmap file.
            // Log the INVOLFLT_ERR_BITMAP_FILE_LOG_DAMAGED message to event viewer.

            *pInMageOpenStatus = m_ErrorCausingOutOfSync = INVOLFLT_ERR_BITMAP_FILE_LOG_FIXED;
            m_bCorruptBitmapFile = TRUE;
            InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, 
                ("LoadBitmapHeaderFromFileStream: Verify header failed. Bitmap file = %wZ for\nVolume = %C: is corrupt\n",
                    m_ustrBitmapFilename, m_wcDriveLetter));
        }
    } else {
        m_bNewBitmapFile = TRUE;
        *pInMageOpenStatus = m_ErrorCausingOutOfSync = INVOLFLT_ERR_BITMAP_FILE_CREATED;
    }

    // File was created new, or the file was empty, or the file has to be cleared.
    Status = InitBitmapFile(pInMageOpenStatus, BitmapPersistentValue);
    if (NT_SUCCESS(Status)) {
        if (bClearOnOpen) {
            InVolDbgPrint(DL_WARNING, DM_BITMAP_OPEN, 
                ("LoadBitmapHeaderFromFileStream - Cleared bitmap file %wZ for volume %C:\n",
                        &m_ustrBitmapFilename, m_wcDriveLetter));
        } else if (TRUE == m_bEmptyBitmapFile) {
            InVolDbgPrint(DL_WARNING, DM_BITMAP_OPEN, 
                ("LoadBitmapHeaderFromFileStream - Repaired empty bitmap file %wZ for volume %C:\n",
                        &m_ustrBitmapFilename, m_wcDriveLetter));
        } else if (TRUE == m_bCorruptBitmapFile) {
            InVolDbgPrint(DL_WARNING, DM_BITMAP_OPEN, 
                ("LoadBitmapHeaderFromFileStream - Repaired corrupt bitmap file %wZ for volume %C:\n",
                        &m_ustrBitmapFilename, m_wcDriveLetter));        
        } else {
            ASSERT(TRUE == m_bNewBitmapFile);
            InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, 
                ("LoadBitmapHeaderFromFileStream - new bitmap file %wZ successfully created for volume %C:\n",
                        &m_ustrBitmapFilename, m_wcDriveLetter));
        }
    } else {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, 
            ("LoadBitmapHeaderFromFileStream - Can't initialize bitmap file %wZ volume %C:, Status = %#X\n",
                        &m_ustrBitmapFilename, m_wcDriveLetter, Status));
        *pInMageOpenStatus = INVOLFLT_ERR_BITMAP_FILE_CANT_INIT;
        goto Cleanup_And_Return_Failure;
    }

    return Status;

Cleanup_And_Return_Failure:
    ASSERT(STATUS_SUCCESS != *pInMageOpenStatus);

    if (NULL != m_iobBitmapHeader) {
        m_iobBitmapHeader->Release();
        m_iobBitmapHeader = NULL;
    }

    return Status;
}

NTSTATUS
BitmapAPI::MoveRawIOChangesToBitmap(NTSTATUS  *pInMageOpenStatus)
{
    ULONG       i, ulScaledSize, ulWriteSize;
    ULONGLONG   ullSizeOffsetPair, ullScaledOffset, ullWriteOffset, ullRoundedVolumeSize;
    NTSTATUS    Status = STATUS_SUCCESS;

    // Bitmap has to be in opened state. This operation can not be performed in Commited, RawIO or Closed mode.
    if (ecBitmapStateOpened != m_eBitmapState)
        return STATUS_INVALID_PARAMETER;

    ullRoundedVolumeSize = ((ULONGLONG)m_llVolumeSize + m_ulBitmapGranularity - 1) / m_ulBitmapGranularity;
    ullRoundedVolumeSize = ullRoundedVolumeSize * m_ulBitmapGranularity;

    // now sweep through and save any last chance changes into the bitmap
    for(i = 0; i < m_BitmapHeader.header.lastChanceChanges; i++) {
        ullSizeOffsetPair = m_BitmapHeader.changeGroup[i / MAX_CHANGES_IN_WRITE_GROUP].lengthOffsetPair[i % MAX_CHANGES_IN_WRITE_GROUP];
        ulScaledSize = (ULONG)(ullSizeOffsetPair >> 48);
        ullScaledOffset = ullSizeOffsetPair & 0xFFFFFFFFFFFF;
        ullWriteOffset = ullScaledOffset * m_ulBitmapGranularity;
        ulWriteSize = ulScaledSize * m_ulBitmapGranularity;
        if (ullWriteOffset < (ULONGLONG)m_llVolumeSize) {
            if ((ullWriteOffset + ulWriteSize) > ullRoundedVolumeSize) {
                // As the granularity used to save changes in RawIO mode is same as bitmap granularity. This should not happen.
                InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, 
                    ("MoveRawIOChangesToBitmap: Correcting WriteOffset %#I64x ScaledOffset %#I64x Size %#x ScaledSize %#x VolumeSize %#I64x RoundedVolumeSize %#I64x\n",
                                ullWriteOffset, ullScaledOffset, ulWriteSize, ulScaledSize, m_llVolumeSize, ullRoundedVolumeSize));
                ulScaledSize = (ULONG)(((ULONGLONG)m_llVolumeSize) - ullWriteOffset);
                ulScaledSize = (ulScaledSize + m_ulBitmapGranularity - 1) / m_ulBitmapGranularity;
                InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("MoveRawIOChangesToBitmap: Corrected to ulScaledSize %#x\n",
                                ulScaledSize));
            }
            Status = segmentedBitmap->SetBitRun((UINT32)ulScaledSize, ullScaledOffset);
            if (!NT_SUCCESS(Status)) {
                *pInMageOpenStatus = m_ErrorCausingOutOfSync = INVOLFLT_ERR_BITMAP_FILE_CANT_APPLY_SHUTDOWN_CHANGES;
                m_bVolumeInSync = FALSE;
                InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("MoveRawIOChangesToBitmap: Error writing Raw IO changes to bitmap, Status = %#x\n",
                    Status));
                return Status;
            }
        } else {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, ("MoveRawIOChangesToBitmap: Discarding change - Offset %#I64x Size %#x VolumeSize %I64x\n",
                            ullWriteOffset, ulWriteSize, m_llVolumeSize));
        }
    }

    if (0 != m_BitmapHeader.header.changesLost) {
        *pInMageOpenStatus = m_ErrorCausingOutOfSync = INVOLFLT_ERR_TOO_MANY_LAST_CHANCE;
        m_bVolumeInSync = FALSE;
        InVolDbgPrint(DL_INFO, DM_BITMAP_OPEN, ("MoveRawIOChangesToBitmap: Lost Changes %#x, Setting Out of sync.\n",
                        m_BitmapHeader.header.changesLost));
    }

    // Save it for debugging purposes
    ULONG       ulLastChanceChanges = m_BitmapHeader.header.lastChanceChanges;
    m_BitmapHeader.header.lastChanceChanges = 0;
    m_BitmapHeader.header.changesLost = 0;
    m_BitmapHeader.header.recoveryState = dirtyShutdown; // unless we shutdown clean, assume dirty

    // We have to update the header even if there are no raw io changes.
    // We have to do this to save the header with new state indicating the bitmap is dirty.
    Status = CommitHeader(FALSE, pInMageOpenStatus);
    if (!NT_SUCCESS(Status)) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN, 
            ("MoveRawIOChangesToBitmap: Failed to move RawIO changes(%d) and write dirty shutdown marker in bitmap file = %wZ for Volume = %C: returned Status %#x\n",
                ulLastChanceChanges, m_ustrBitmapFilename, m_wcDriveLetter, Status));
        *pInMageOpenStatus = m_ErrorCausingOutOfSync = INVOLFLT_ERR_BITMAP_FILE_CANT_UPDATE_HEADER;
        m_bVolumeInSync = FALSE; // Reset this as we failed to save the header back to bitmap file.
        return Status;
    } else {
        InVolDbgPrint(DL_TRACE_L1, DM_BITMAP_WRITE | DM_BITMAP_OPEN,
            ("MoveRawIOChangesToBitmap: Moved RawIO changes(%d) and written dirty shutdown marker in bitmap file = %wZ for Volume = %C:\n",
             ulLastChanceChanges, m_ustrBitmapFilename, m_wcDriveLetter));
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
    KIRQL   OldIrql;
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
                    ("%s: Unable to write header with Persistent Values to volume %C:\n",
                    __FUNCTION__ , m_wcDriveLetter));
            } else {
                InVolDbgPrint(DL_TRACE_L1, DM_BITMAP_WRITE ,
                    ("%s: Succeeded to write Header to bimtap file %wZ with Persistent Values to volume %C:\n",
                    __FUNCTION__, m_ustrBitmapFilename, m_wcDriveLetter));
            }
            break;

        case ecBitmapStateRawIO:
            Status = CommitHeader(TRUE, &InMageStatus);
            if (!NT_SUCCESS(Status) || (InMageStatus != STATUS_SUCCESS)) {
                InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE , 
                    ("%s: Unable to write header with Persistent Values to volume %C:\n",
                    __FUNCTION__, m_wcDriveLetter));
            } else {
                InVolDbgPrint(DL_TRACE_L1, DM_BITMAP_WRITE ,
                    ("%s: Succeeded to write Header to bimtap file %wZ with Persistent Values to volume %C:\n",
                    __FUNCTION__, m_ustrBitmapFilename, m_wcDriveLetter));
            }
            break;

        default:
            Status = STATUS_DEVICE_NOT_READY;
            InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE , 
                ("%s: Unable to write header with Persistent Values to volume %C:\n",
                __FUNCTION__, m_wcDriveLetter));
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
                ("%s: Unable to write header with Resync Required to volume %C:\n",
                __FUNCTION__ , m_wcDriveLetter));
        } else {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE ,
                ("%s: Succeeded to write Header to bimtap file %wZ with Resync Required to volume %C:\n",
                __FUNCTION__, m_ustrBitmapFilename, m_wcDriveLetter));
        }
        break;

    case ecBitmapStateRawIO:
        Status = CommitHeader(TRUE, &InMageStatus);
        if (!NT_SUCCESS(Status) || (InMageStatus != STATUS_SUCCESS)) {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE , 
                ("%s: Unable to write header with Resync Required to volume %C:\n",
                __FUNCTION__, m_wcDriveLetter));
        } else {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE ,
                ("%s: Succeeded to write Header to bimtap file %wZ with Resync Required to volume %C:\n",
                __FUNCTION__, m_ustrBitmapFilename, m_wcDriveLetter));
        }
        break;

    default:
        Status = STATUS_DEVICE_NOT_READY;
        InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE , 
            ("%s: Unable to write header with Resync Required to volume %C:\n",
            __FUNCTION__, m_wcDriveLetter));
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
                ("%s: Unable to write header with Reset Resync Required to volume %C:\n",
                __FUNCTION__ , m_wcDriveLetter));
        } else {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE ,
                ("%s: Succeeded to write Header to bimtap file %wZ with Reset Resync Required to volume %C:\n",
                __FUNCTION__, m_ustrBitmapFilename, m_wcDriveLetter));
        }
        break;

    case ecBitmapStateRawIO:
        Status = CommitHeader(TRUE, &InMageStatus);
        if (!NT_SUCCESS(Status) || (InMageStatus != STATUS_SUCCESS)) {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE , 
                ("%s: Unable to write header with Reset Resync Required to volume %C:\n",
                __FUNCTION__, m_wcDriveLetter));
        } else {
            InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE ,
                ("%s: Succeeded to write Header to bimtap file %wZ with Reset Resync Required to volume %C:\n",
                __FUNCTION__, m_ustrBitmapFilename, m_wcDriveLetter));
        }
        break;

    default:
        Status = STATUS_DEVICE_NOT_READY;
        InVolDbgPrint(DL_ERROR, DM_BITMAP_WRITE , 
            ("%s: Unable to write header with Reset Resync Required to volume %C:\n",
            __FUNCTION__, m_wcDriveLetter));
        break;
    }


    KeReleaseMutex(&m_BitmapMutex, FALSE);

    return InMageStatus;
}


