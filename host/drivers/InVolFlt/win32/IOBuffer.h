#ifndef _INMAGE_IO_BUFFER_H_
#define _INMAGE_IO_BUFFER_H_

class IOBuffer : public BaseClass {
public:
    LIST_ENTRY ListEntry;
    static void InitializeMemoryLookasideList();
    static void TerminateMemoryLookasideList();

    void * __cdecl operator new(size_t baseSize);
    void * __cdecl operator new(size_t size, size_t bufferSize, ULONG32 alignment);
    void __cdecl operator delete(void * ptr);


    IOBuffer();
    IOBuffer(FileStreamSegmentMapper * parent, ULONG32 index);
    virtual ~IOBuffer();

    virtual void SetFileStream(FileStream * stream) {fileStream = stream;};
    virtual void SetFileOffset(ULONG64 offset) {startingOffset = offset;};
    virtual void SetOwnerIndex(ULONG32 index) {ownerIndex = index;};
    virtual ULONG32 GetOwnerIndex() {return ownerIndex; };

    virtual NTSTATUS SyncRead(void); // load buffer from disk
    virtual NTSTATUS SyncFlush(void); // write to disk if dirty
    virtual NTSTATUS SyncReadPhysical(BOOLEAN bForce = FALSE); // read from volume directly
    virtual NTSTATUS SyncFlushPhysical(void); // write to volume directly
    __inline void LockBuffer(void){ InterlockedIncrement(&locked);};
    __inline void UnlockBuffer(void) { InterlockedDecrement(&locked);};
    __inline bool IsDirty(void) { return (dirty);};
    __inline bool IsLocked(void) { return (locked > 0);};
    __inline void SetDirty(void) { dirty = true;};
    virtual void LearnPhysicalIO();
    virtual void LearnPhysicalIOFilter(IN PDEVICE_OBJECT DeviceObject,
                                       IN PIRP Irp);
    virtual void ResetPhysicalDO();

    ULONG32 size;
    UCHAR * buffer; // this MAY point into the same memory block that holds this header object
protected:
    static NPAGED_LOOKASIDE_LIST objectLookasideList;

    // data used paged pool.
    static PAGED_LOOKASIDE_LIST dataLookasideList;
    FileStreamSegmentMapper * owner;
    ULONG32 ownerIndex;
    PVOID allocationPointer; // to meet the alignment requirements, the operator new may shuffle things in the allocated memory block
    ULONG64 startingOffset; // where in the FileStream is this a buffer for
    FileStream * fileStream;
    PDEVICE_OBJECT physicalDisk; // used for writes outside the file system
    ULONG64 physicalOffset; // offset to use for physical I/O

    bool dirty;
#if(_WIN32_WINNT > 0x0500)
    volatile long  locked; // do not delete or write this buffer
#else
    long  locked;
#endif
};

#endif // _INMAGE_IO_BUFFER_H_
