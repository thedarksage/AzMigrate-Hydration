// implementation for concrete segment mapper for files/volumes
//
// Copyright InMage Systems 2004
//


#include "global.h"

// here is a list of prime numbers used to size a future cache hash table
ULONG32 primeTable[32] = 
{      7,     17,     29,     41,     59,     89,    137,    211,    293,
     449,    673,    997,   1493,   2003,   3001,   4507,   6779,   9311,
     13933,  19819,  29863,  44987,  66973,  90019, 130069, 195127, 301237, 0};


FileStreamSegmentMapper::FileStreamSegmentMapper()
{
TRC
}

FileStreamSegmentMapper::~FileStreamSegmentMapper()
{
TRC
ULONG index;

    if (bufferCache != NULL) {

        // release all the IOBuffers
        for (index = 0; index < cacheSize; index++) {
            if (bufferCache[index] != NULL) {
                bufferCache[index]->Release();
            }
        }
        delete bufferCache;
    }
}

NTSTATUS FileStreamSegmentMapper::Attach(FileStream * file, ULONG64 offsetInFile, ULONG64 minimumSizeInFile, ULONG SegmentCacheLimit)
{
TRC
NTSTATUS status;

    fileStream = file;
    startingOffset = offsetInFile;
    segmentSize = BITMAP_FILE_SEGMENT_SIZE;  // this is the commonality between a sector size and fitting in a page with the header
    cacheSize = (ULONG32)((minimumSizeInFile / segmentSize) + 1);
    InitializeListHead(&segmentList);
    numFreeBuffers = SegmentCacheLimit;
    bufferCache = (class IOBuffer **)(new char[cacheSize * sizeof(IOBuffer *)]);
    InVolDbgPrint(DL_INFO, DM_MEM_TRACE,
        ("FileStreamSegmentMapper::Attach: Allocation %#p\n", bufferCache));
    if (bufferCache) {
        status = STATUS_SUCCESS;
    } else {
        status = STATUS_NO_MEMORY;
    }

    return status;
}

NTSTATUS FileStreamSegmentMapper::Detach()
{
TRC
NTSTATUS status;

    status = STATUS_SUCCESS;

    return status;
}

NTSTATUS FileStreamSegmentMapper::ReadAndLock(ULONG64 offset, UCHAR **returnBufferPointer, ULONG32 *returnSegmentSize)
{
TRC
NTSTATUS status;
UCHAR * dataBuffer;
ULONG32 dataSize;
ULONG32 bufferIndex;
IOBuffer * ioBuffer = NULL;
PLIST_ENTRY segmentEntry;

    ASSERT(bufferCache != NULL);
    dataBuffer = NULL;
    dataSize = 0;
    status = STATUS_SUCCESS;

    // first check the cache for the correct buffer
    bufferIndex = (ULONG32)(offset / (ULONG64)segmentSize);
    if (bufferCache[bufferIndex] == NULL) {
        // it's not in the cache, read it
        cacheMiss++;
        // the segment is not in memory, try to bring it in
        ioBuffer = NULL;
        if(numFreeBuffers) {
            // We can allocate few more ioBuffers
            ioBuffer = new (segmentSize, 0)IOBuffer(this, bufferIndex);
            InVolDbgPrint(DL_INFO, DM_MEM_TRACE,
                ("FileStreamSegmentMapper::ReadAndLock: Allocation %p\n", ioBuffer));
            if(ioBuffer) {
                numFreeBuffers--;
            }
        }

        if(ioBuffer == NULL) {
            // The ioBuffer at the tail contains the segment which is LRU and this
            // can be replaced with the required segment.
            if(!IsListEmpty(&segmentList)) {
                segmentEntry = RemoveTailList(&segmentList);
                ioBuffer = CONTAINING_RECORD(segmentEntry,IOBuffer,ListEntry);
            }
            if(ioBuffer != NULL) {
                // Since we are evacuating this segment from the ioBuffer, we need
                // to flush it
                status = ioBuffer->SyncFlush();
                if(!NT_SUCCESS(status)) {
                    // We are unsuccesful in evacuating this segment from the 
                    // IOBuffer, so insert this ioBuffer back at the tail of 
                    // the list
                    InsertTailList(&segmentList,&ioBuffer->ListEntry);
                    ioBuffer = NULL;
                }

                if(ioBuffer != NULL) {
                    // Things went right in evacuating the existing segment from the 
                    // IOBuffer, erase this segment off from the mapping in bufferCache, 
                    // as we will be evacuating this segment
                    bufferCache[ioBuffer->GetOwnerIndex()] = NULL;
                    // Reuse this iobuffer for new bufferIndex
                    ioBuffer->SetOwnerIndex(bufferIndex);
                }
            }
        }

        if(ioBuffer == NULL) {
            status = STATUS_NO_MEMORY;
        } else {
            ioBuffer->SetFileStream(fileStream);
            ioBuffer->SetFileOffset(startingOffset + (bufferIndex * segmentSize));
            status = ioBuffer->SyncRead();
            if (!NT_SUCCESS(status)) {
                ioBuffer->Release(); // this should make it go away
                ioBuffer = NULL;
                numFreeBuffers++;
            } else {
                bufferCache[bufferIndex] = ioBuffer;
                // Since this is the segment that is referenced RIGHT NOW, it will
                // be placed at the head of the list to signify that it is the Most-
                // Recently referenced segment
                InsertHeadList(&segmentList,&ioBuffer->ListEntry);
            }
        }
    } else {
        cacheHits++;
    }

    if (bufferCache[bufferIndex] != NULL) {
        // it's in the cache
        dataBuffer = bufferCache[bufferIndex]->buffer;
        dataBuffer += (offset % (ULONG64)segmentSize); // make return pointer be at correct byte
        dataSize = (ULONG32)(segmentSize - (offset % (ULONG64)segmentSize)); // size is now max - above offset
        bufferCache[bufferIndex]->LockBuffer();
        status = STATUS_SUCCESS;
    } 

    *returnBufferPointer = dataBuffer;
    *returnSegmentSize = dataSize;

    return status;
}


NTSTATUS FileStreamSegmentMapper::UnlockAndMarkDirty(ULONG64 offset)
{
TRC
NTSTATUS status;
ULONG32 bufferIndex;

    bufferIndex = (ULONG32)(offset / (ULONG64)segmentSize);
    if (bufferCache[bufferIndex] != NULL) {
        bufferCache[bufferIndex]->SetDirty();
        bufferCache[bufferIndex]->UnlockBuffer();
        status = STATUS_SUCCESS;
    } else {
        status = STATUS_INVALID_PARAMETER;
    }

    return status;
}

NTSTATUS FileStreamSegmentMapper::Unlock(ULONG64 offset)
{
TRC
NTSTATUS status;
ULONG32 bufferIndex;

    bufferIndex = (ULONG32)(offset / (ULONG64)segmentSize);
    if (bufferCache[bufferIndex] != NULL) {
        bufferCache[bufferIndex]->UnlockBuffer();
        status = STATUS_SUCCESS;
    } else {
        status = STATUS_INVALID_PARAMETER;
    }

    return status;
}

NTSTATUS FileStreamSegmentMapper::Flush(ULONG64 offset)
{
TRC
NTSTATUS Status = STATUS_SUCCESS;
ULONG32 bufferIndex;

    bufferIndex = (ULONG32)(offset / (ULONG64)segmentSize);
    if (bufferCache[bufferIndex] != NULL) {
        if (bufferCache[bufferIndex]->IsDirty()) {
            Status = bufferCache[bufferIndex]->SyncFlush();
        }
    } else {
        Status = STATUS_INVALID_PARAMETER;
    }

    return Status;
}

NTSTATUS FileStreamSegmentMapper::SyncFlushAll()
{
TRC
NTSTATUS status;
NTSTATUS finalStatus;
ULONG bufferIndex;

    finalStatus = STATUS_SUCCESS;
    
    if (bufferCache != NULL) {
        // flush all the IOBuffers
        for (bufferIndex = 0; bufferIndex < cacheSize; bufferIndex++) {
            status = STATUS_SUCCESS;
            if (bufferCache[bufferIndex] != NULL) {
                if (bufferCache[bufferIndex]->IsDirty()) {
                    status = bufferCache[bufferIndex]->SyncFlush();
                }
                if (!NT_SUCCESS(status)) {
                    finalStatus = status;
                }
            }
        }
    }

    return finalStatus;
}

void * __cdecl FileStreamSegmentMapper::operator new(size_t size) {
	return (PVOID)malloc((ULONG32)size, (ULONG32)TAG_FSSEGMENTMAPPER_OBJECT, NonPagedPool);
}



