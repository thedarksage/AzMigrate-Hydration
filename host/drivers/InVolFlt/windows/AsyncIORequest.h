#pragma once

class AsyncIORequest : public BaseClass {
public:
    static void InitializeMemoryLookasideList();
    static void TerminateMemoryLookasideList();

    void * operator new(size_t size);
    void operator delete(void *mem);

    AsyncIORequest(void);  // constructor
    AsyncIORequest(PDEVICE_OBJECT device, PFILE_OBJECT file);  // constructor
    AsyncIORequest(FileStream * file);
    virtual ~AsyncIORequest();

    static bool AsyncIORequest::IsOurIO(PIRP Irp); // used to filter any write we make
    static bool AsyncIORequest::IsOurIO(PIRP Irp, PFILE_OBJECT file); // test any irp for a file match

    virtual NTSTATUS Read(PVOID buffer, ULONG32 size, ULONG64 offset, ULONG ulFlags=0);
    virtual NTSTATUS Write(PVOID buffer, ULONG32 size, ULONG64 offset, ULONG ulFlags=0);
    virtual NTSTATUS SyncWrite(PVOID buffer, ULONG size, ULONG64 offset);
    virtual NTSTATUS Flush();
//    virtual NTSTATUS IOCtl(ULONG32 ctlCode, PVOID inputBuffer, ULONG32 inputSize, PVOID outputBuffer, ULONG32 outputSize);
    virtual NTSTATUS Wait(PLARGE_INTEGER pliTimeout);
    virtual NTSTATUS Status(void) {return m_IoStatus.Status; };

    NTSTATUS IoCompletion(PDEVICE_OBJECT DeviceObject, PIRP Irp);

protected:
    virtual NTSTATUS ReadWrite(PVOID buffer, ULONG32 size, ULONG64 offset, ULONG32 IrpMJ, ULONG ulFlags=0);
    static NPAGED_LOOKASIDE_LIST lookasideList;
    PDEVICE_OBJECT osDevice;
    PFILE_OBJECT fileObject;
    PIRP    m_Irp;
    KEVENT  m_Event;

private:
    IO_STATUS_BLOCK m_IoStatus;

};

