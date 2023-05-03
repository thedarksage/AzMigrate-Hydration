// this class handles the common parts of async IRP's

//
// Copyright InMage Systems 2004
//

#include <global.h>

NPAGED_LOOKASIDE_LIST AsyncIORequest::lookasideList;

void AsyncIORequest::InitializeMemoryLookasideList()
{
TRC
    ExInitializeNPagedLookasideList(&AsyncIORequest::lookasideList,
                                    NULL, 
                                    NULL, 
                                    0, 
                                    sizeof(class AsyncIORequest), 
                                    TAG_ASYNCIO_OBJECT, 
                                    0);
}

void AsyncIORequest::TerminateMemoryLookasideList()
{ 
TRC
    ExDeleteNPagedLookasideList(&AsyncIORequest::lookasideList);
}

void * AsyncIORequest::operator new(size_t size) 
{
TRC
PVOID mem; 

    ASSERT(size == sizeof(class AsyncIORequest));
    
    mem = ExAllocateFromNPagedLookasideList(&AsyncIORequest::lookasideList);
    if (mem) 
        RtlZeroMemory(mem, sizeof(class AsyncIORequest));
    
    return mem;
}

void AsyncIORequest::operator delete(void *mem) 
{ 
TRC
    if (mem) 
        ExFreeToNPagedLookasideList(&AsyncIORequest::lookasideList, mem);
}

AsyncIORequest::AsyncIORequest(void)   // constructor
{
TRC

    m_IoStatus.Status = STATUS_PENDING;
    m_IoStatus.Information = 0;
    KeInitializeEvent(&m_Event, NotificationEvent, FALSE);
}

AsyncIORequest::AsyncIORequest(PDEVICE_OBJECT device, PFILE_OBJECT file) // constructor
{
TRC
    m_IoStatus.Status = STATUS_PENDING;
    m_IoStatus.Information = 0;
    KeInitializeEvent(&m_Event, NotificationEvent, FALSE);

    osDevice = device;
    fileObject = file;
}

AsyncIORequest::AsyncIORequest(FileStream * file)
{
TRC
    m_IoStatus.Status = STATUS_PENDING;
    m_IoStatus.Information = 0;
    KeInitializeEvent(&m_Event, NotificationEvent, FALSE);

    osDevice = file->GetDeviceObject();
    fileObject = file->GetFileObject();
}

AsyncIORequest::~AsyncIORequest()
{
TRC
}

extern "C" AsyncIORequestReadWriteCompletion(
    PDEVICE_OBJECT  DeviceObject,
    PIRP            Irp,
    AsyncIORequest* IoRequest)
{
TRC
    NTSTATUS Status;

    ASSERT(NULL != IoRequest);

    Status = IoRequest->IoCompletion(DeviceObject, Irp);
    IoRequest->Release(); // undo reference in the irp stack

    return Status;
}

NTSTATUS
AsyncIORequest::IoCompletion(
    PDEVICE_OBJECT  DeviceObject,
    PIRP            Irp
    )
{
    PIO_STACK_LOCATION  topIrpStack;

    // we don't pass an IoStatus block with the original irp because the following UnRef may delete storage
    m_IoStatus.Information = Irp->IoStatus.Information;
    m_IoStatus.Pointer = Irp->IoStatus.Pointer;
    m_IoStatus.Status = Irp->IoStatus.Status;

    KeSetEvent(&m_Event, IO_NO_INCREMENT, FALSE);

    m_Irp = NULL; // prevent any accesses to the discarded irp

    // be SURE out completion routine address is zero in case this IRP get's reused and not cleared
    // we depend on the completion routine address to determine if an Irp is our's

    // walk down the irp and find the first irp stack slot
    topIrpStack = (PIO_STACK_LOCATION)((PCHAR)Irp + sizeof(IRP)) + (Irp->StackCount - 1);
    if (topIrpStack->CompletionRoutine == (PVOID)AsyncIORequestReadWriteCompletion) {
        topIrpStack->CompletionRoutine = NULL;
    }

    // do NOT call IoMarkIrpPending here as we are a top level completion routine

    return STATUS_SUCCESS;
}

NTSTATUS AsyncIORequest::ReadWrite(PVOID buffer, ULONG32 size, ULONG64 offset, ULONG32 IrpMJ)
{
TRC
PIO_STACK_LOCATION irpSp;
NTSTATUS status;

    m_IoStatus.Status = STATUS_PENDING;
    m_IoStatus.Information = 0;
    KeInitializeEvent(&m_Event, NotificationEvent, FALSE);

    m_Irp = IoBuildAsynchronousFsdRequest(IrpMJ,
                                        osDevice,
                                        buffer,
                                        size,
                                        (PLARGE_INTEGER)&offset,
                                        NULL);
    if (m_Irp == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    this->AddRef(); // we are creating a reference in the irp stack
    IoSetCompletionRoutine(
        m_Irp,
        (PIO_COMPLETION_ROUTINE)AsyncIORequestReadWriteCompletion,
        (PVOID)this,
        TRUE,
        TRUE,
        TRUE);

    irpSp = IoGetNextIrpStackLocation(m_Irp);
    irpSp->FileObject = fileObject;
    m_Irp->Flags |= IRP_NOCACHE;  // this is REQUIRED to get unbuffered I/O. otherwise it get's buffered

    status = IoCallDriver(osDevice, m_Irp);

    return status;
}

NTSTATUS
AsyncIORequest::SyncWrite(PVOID buffer, ULONG cbWrite, ULONG64 ullOffset)
{
TRC
    PIO_STACK_LOCATION irpSp;
    NTSTATUS    Status;
    ULONG       cbWritten = 0;
    ULONG       cbRemaining;
    ULONGLONG   ullCurrentOffset;

    KeInitializeEvent(&m_Event, NotificationEvent, FALSE);

    do {
        ullCurrentOffset = ullOffset + cbWritten;
        cbRemaining = cbWrite - cbWritten;
        m_Irp = IoBuildAsynchronousFsdRequest(IRP_MJ_WRITE,
                                            osDevice,
                                            (PVOID)((PUCHAR)buffer + cbWritten),
                                            cbRemaining,
                                            (PLARGE_INTEGER)&ullCurrentOffset,
                                            NULL);
        if (m_Irp == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        this->AddRef(); // we are creating a reference in the irp stack
        IoSetCompletionRoutine(
            m_Irp,
            (PIO_COMPLETION_ROUTINE)AsyncIORequestReadWriteCompletion,
            (PVOID)this,
            TRUE,
            TRUE,
            TRUE);

        irpSp = IoGetNextIrpStackLocation(m_Irp);
        irpSp->FileObject = fileObject;
        m_Irp->Flags |= IRP_NOCACHE;  // this is REQUIRED to get unbuffered I/O. otherwise it get's buffered

        Status = IoCallDriver(osDevice, m_Irp);
        if (STATUS_PENDING == Status) {
            Status = KeWaitForSingleObject(&m_Event, Executive, KernelMode, FALSE, NULL);
            ASSERT(STATUS_SUCCESS == Status);
            Status = m_IoStatus.Status;
        }

        if (STATUS_SUCCESS != Status) {
            return Status;
        }

        ASSERT(0 != m_IoStatus.Information);
        if (0 == m_IoStatus.Information) {
            return STATUS_DISK_FULL;
        }
        // 64BIT : As length of IO written would not be greater than 4GB casting ULONG_PTR to ULONG
        cbWritten += (ULONG)m_IoStatus.Information;

    } while (cbWritten < cbWrite);
    
    return STATUS_SUCCESS;
}

NTSTATUS AsyncIORequest::Write(PVOID buffer, ULONG32 size, ULONG64 offset)
{
TRC
    return ReadWrite(buffer, size, offset, IRP_MJ_WRITE);
}

NTSTATUS AsyncIORequest::Read(PVOID buffer, ULONG32 size, ULONG64 offset)
{
TRC
    return ReadWrite(buffer, size, offset, IRP_MJ_READ);
}

NTSTATUS AsyncIORequest::Flush()
{
TRC
    return ReadWrite(NULL, 0, 0, IRP_MJ_FLUSH_BUFFERS);
}

NTSTATUS AsyncIORequest::Wait(PLARGE_INTEGER pliTimeOut)
// if you set a completion notify object, the event will not get signaled
{
TRC
NTSTATUS waitStatus;
NTSTATUS status;

#if DBG
    if ((NULL == pliTimeOut) || (pliTimeOut->QuadPart != 0)) {
        ASSERT(KeGetCurrentIrql() < DISPATCH_LEVEL);
    }
#endif // DBG

    if (m_IoStatus.Status == STATUS_PENDING) {
        waitStatus = KeWaitForSingleObject(&m_Event, Executive, KernelMode, FALSE, pliTimeOut);
        if (waitStatus == STATUS_SUCCESS) {
            status = m_IoStatus.Status;
        } else {
            status = waitStatus;
        }
    } else {
        status = m_IoStatus.Status;
    }

    if (!NT_SUCCESS(status)) {
        InVolDbgPrint(DL_ERROR, DM_BITMAP_READ | DM_BITMAP_WRITE | DM_BITMAP_OPEN,
            ("AsyncIORequest::Wait I/O error occured, status = %X\n", status));
    }

    return status;
}

// decide if an IRP use the standard completion routine
bool AsyncIORequest::IsOurIO(PIRP Irp)
{
TRC
PIO_STACK_LOCATION topIrpStack;

    // walk down the irp and find the first irp stack slot
    topIrpStack = (PIO_STACK_LOCATION)((PCHAR)Irp + sizeof(IRP)) + (Irp->StackCount - 1);

    return (topIrpStack->CompletionRoutine == (PIO_COMPLETION_ROUTINE)AsyncIORequestReadWriteCompletion);
}

// decide if an IRP is from a specific file object
bool AsyncIORequest::IsOurIO(PIRP Irp, PFILE_OBJECT file)
{
TRC
PIO_STACK_LOCATION topIrpStack;

    // walk down the irp and find the first irp stack slot
    topIrpStack = (PIO_STACK_LOCATION)((PCHAR)Irp + sizeof(IRP)) + (Irp->StackCount - 1);

    return (topIrpStack->FileObject == file);
}


/*
NTSTATUS AsyncIORequest::IOCtl(ULONG32 ctlCode, PVOID inputBuffer, ULONG32 inputSize, PVOID outputBuffer, ULONG32 outputSize)
{
TRC
PIO_STACK_LOCATION irpSp;
NTSTATUS status;

    ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

    m_IoStatus.Status = STATUS_PENDING;
    m_IoStatus.Information = 0;
    KeInitializeEvent(&event, NotificationEvent, FALSE);

    m_Irp = IoBuildDeviceIoControlRequest(
        ctlCode,
        osDevice,
        inputBuffer,
        inputSize,
        outputBuffer,
        outputSize,
        FALSE,
        NULL,
        NULL);
    if (m_Irp == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
        }
    
    this->AddRef(); // we are creating a reference in the irp stack
    IoSetCompletionRoutine(
        m_Irp,
        (PIO_COMPLETION_ROUTINE)AsyncIORequestReadWriteCompletion,
        (PVOID)this,
        TRUE,
        TRUE,
        TRUE);

    irpSp = IoGetNextIrpStackLocation(m_Irp);
    irpSp->FileObject = fileObject;

    status = IoCallDriver(osDevice, m_Irp);

    return status;
}
*/
