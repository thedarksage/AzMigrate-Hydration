#pragma once


class SegmentedBitmap : public BaseClass {
public:
    void * __cdecl operator new(size_t size);
    SegmentedBitmap(SegmentMapper * mapper, ULONG64 bitmapSizeInBits) { segmentMapper = mapper; bitsInBitmap = bitmapSizeInBits;};
    virtual NTSTATUS ClearBitRun(ULONG32 size, ULONG64 offset);
    virtual NTSTATUS SetBitRun(ULONG32 size, ULONG64 offset);
    virtual NTSTATUS InvertBitRun(ULONG32 size, ULONG64 offset);
    virtual NTSTATUS GetFirstBitRun(ULONG32 * size, ULONG64 *offset);
    virtual NTSTATUS GetNextBitRun(ULONG32 * size, ULONG64 *offset);
    virtual NTSTATUS ClearAllBits();
    virtual ULONG64  GetNumberOfBitsSet();
    virtual NTSTATUS SyncFlushAll(void) { return segmentMapper->SyncFlushAll();};

     unsigned long long  GetCacheHit()
    {
    	return segmentMapper->GetCacheHit();
    }
    unsigned long long GetCacheMiss()
    {
    	 return segmentMapper->GetCacheMiss();
    }
protected:
    enum tagBitmapOperation {
        setBits,
        clearBits,
        invertBits};
    SegmentMapper * segmentMapper;
    ULONG64 nextSearchOffset; // where GetNextBitRun will start looking next
    ULONG64 bitsInBitmap; // bitmaps will not always be an integral nbr of bytes
    virtual NTSTATUS ProcessBitRun(ULONG32 size, ULONG64 offset, enum tagBitmapOperation operation);
}; 


