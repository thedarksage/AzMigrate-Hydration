#pragma once


#define BITMAP_FILE_SEGMENT_SIZE (0x1000)

// 65 Segments
#ifdef _WIN64
#define MAX_BITMAP_SEGMENT_BUFFERS 0x100
#else
#define MAX_BITMAP_SEGMENT_BUFFERS 0x41
#endif
class FileStreamSegmentMapper : public SegmentMapper {
public:
    void * __cdecl operator new(size_t size);
    FileStreamSegmentMapper();
    virtual ~FileStreamSegmentMapper();
    virtual NTSTATUS Attach(FileStream * file, ULONG64 offsetInFile, ULONG64 minimumSizeInFile, ULONG SegmentCacheLimit);
    virtual NTSTATUS Detach();
    // return a pointer and size for a buffer that starts at the requested offset
    virtual NTSTATUS ReadAndLock(ULONG64 offset, UCHAR **returnBufferPointer, ULONG32 *segmentSize);
    // only a single locker at a time
    virtual NTSTATUS UnlockAndMarkDirty(ULONG64 offset); // useful if we modified data
    virtual NTSTATUS Unlock(ULONG64 offset); // useful if we only read data
    virtual NTSTATUS Flush(ULONG64 offset); // write the buffer if needed and keep
    virtual NTSTATUS SyncFlushAll();
   
    ULONGLONG GetCacheHit() 
    {
        return cacheHits;
    }
    ULONGLONG GetCacheMiss()
    {
        return cacheMiss;
    }
    
protected:
    FileStream * fileStream;
    ULONG32 cacheSize; // in entries
    class IOBuffer * * bufferCache; // for now, full size array of pointers to buffers
    LIST_ENTRY  segmentList;
    ULONG32 numFreeBuffers;
    ULONGLONG cacheHits;
    ULONGLONG cacheMiss;
};


