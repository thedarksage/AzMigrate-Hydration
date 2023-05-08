#pragma once


class SegmentMapper : public BaseClass { // abstract
public:
    SegmentMapper(void){};
    ~SegmentMapper(void){};
    // return a pointer and size for a buffer that starts at the requested offset
    virtual NTSTATUS ReadAndLock(ULONG64 offset, UCHAR **returnBufferPointer, ULONG32 *partialBufferSize) = 0;
    // only a single locker at a time
    virtual NTSTATUS UnlockAndMarkDirty(ULONG64 offset) = 0; // useful if we modified data
    virtual NTSTATUS Unlock(ULONG64 offset) = 0; // useful if we only read data
    virtual NTSTATUS Flush(ULONG64 offset) = 0;
    virtual NTSTATUS SyncFlushAll(void) = 0;
    virtual ULONGLONG GetCacheHit() = 0;
    virtual ULONGLONG GetCacheMiss() = 0;
protected:
    ULONG32 segmentSize;
    ULONG64 startingOffset;
};


