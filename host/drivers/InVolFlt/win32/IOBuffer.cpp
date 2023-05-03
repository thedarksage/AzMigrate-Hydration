//
// Copyright InMage Systems 2004
//

#include <global.h>

NPAGED_LOOKASIDE_LIST IOBuffer::objectLookasideList;
PAGED_LOOKASIDE_LIST IOBuffer::dataLookasideList;

void IOBuffer::InitializeMemoryLookasideList()
{ 
TRC

    ExInitializeNPagedLookasideList(&IOBuffer::objectLookasideList,
                                    NULL, 
                                    NULL, 
                                    0, 
                                    sizeof(class IOBuffer), 
                                    TAG_IOBUFFER_OBJECT, 
                                    0);

    ExInitializePagedLookasideList(&IOBuffer::dataLookasideList,
                                    NULL, 
                                    NULL, 
                                    0, 
                                    BITMAP_FILE_SEGMENT_SIZE,
                                    TAG_IOBUFFER_DATA, 
                                    0);
}

void IOBuffer::TerminateMemoryLookasideList()
{
TRC
    ExDeleteNPagedLookasideList(&IOBuffer::objectLookasideList);
    ExDeletePagedLookasideList(&IOBuffer::dataLookasideList);
}



void * __cdecl IOBuffer::operator new(size_t baseSize)
{
TRC
IOBuffer * memory;

    memory = (IOBuffer *)ExAllocateFromNPagedLookasideList(&IOBuffer::objectLookasideList);

    if (memory) {
        RtlZeroMemory((void *)memory, baseSize);
        memory->allocationPointer = NULL;
        memory->size = 0;
        memory->buffer = NULL;
    }

    return (PVOID)memory;
}

void * __cdecl IOBuffer::operator new(size_t baseSize, size_t bufferSize, ULONG32 alignment)
{
TRC
IOBuffer * memory;

    memory = (IOBuffer *)ExAllocateFromNPagedLookasideList(&IOBuffer::objectLookasideList);
    
    if (memory) {
        RtlZeroMemory((void *)memory, baseSize);
        if (BITMAP_FILE_SEGMENT_SIZE == bufferSize) {
            memory->allocationPointer = ExAllocateFromPagedLookasideList(&IOBuffer::dataLookasideList);
        } else {
            memory->allocationPointer = ExAllocatePoolWithTag(PagedPool, bufferSize, TAG_IOBUFFER_DATA);
        }
        if (memory->allocationPointer) {
            RtlZeroMemory(memory->allocationPointer, bufferSize);
            memory->size = bufferSize;
            memory->buffer = (UCHAR *)memory->allocationPointer;
        } else {
            ExFreeToNPagedLookasideList(&IOBuffer::objectLookasideList, (void *)memory);
            memory = NULL;
        }
   }

    return (PVOID)memory;
}

void __cdecl IOBuffer::operator delete(void * ptr)
{
TRC
IOBuffer * memory;

    if (ptr) { // passing null is allowed
        memory = (IOBuffer *)ptr;
        if (NULL != memory->allocationPointer) {
            if (BITMAP_FILE_SEGMENT_SIZE == memory->size) {
                ExFreeToPagedLookasideList(&IOBuffer::dataLookasideList, memory->allocationPointer);
            } else {
                ExFreePoolWithTag(memory->allocationPointer, TAG_IOBUFFER_DATA);
            }
        }
        ExFreeToNPagedLookasideList(&IOBuffer::objectLookasideList, ptr);
    }
}

IOBuffer::IOBuffer()
{
TRC
    locked = 0;
    dirty = false;
}

IOBuffer::IOBuffer(FileStreamSegmentMapper * parent, ULONG32 index)
{
TRC
    locked = 0;
    dirty = false;
    owner = parent;
    ownerIndex = index;
}

IOBuffer::~IOBuffer()
{
TRC
}

NTSTATUS IOBuffer::SyncRead()
{
TRC
NTSTATUS status;
AsyncIORequest * ioRequest;

    if (dirty) {
        status = STATUS_WAS_LOCKED;
    } else if (locked > 0) {
        status = STATUS_WAS_LOCKED;
    } else {
        ioRequest = new AsyncIORequest(fileStream);
        InVolDbgPrint(DL_INFO, DM_MEM_TRACE,
            ("IOBuffer::SyncRead: Allocation %p\n", ioRequest));
        if (ioRequest != NULL) {
            status = ioRequest->Read(buffer, size, startingOffset); // initiate the read
            if (status == STATUS_PENDING) {
                status = ioRequest->Wait(NULL);
            }
            ioRequest->Release();
        } else {
            status = STATUS_NO_MEMORY;
        }
    }

    return status;
}

NTSTATUS IOBuffer::SyncFlush()
{
TRC
NTSTATUS status;
AsyncIORequest * ioRequest;

    if (!dirty) {
        return STATUS_SUCCESS;
    }
    
    if (locked > 0) {
        return STATUS_DEVICE_BUSY;
    }

    ASSERT(NULL != fileStream);

    if (NULL == fileStream)
        return STATUS_INVALID_PARAMETER;

    ioRequest = new AsyncIORequest(fileStream);
    InVolDbgPrint(DL_INFO, DM_MEM_TRACE,
        ("IOBuffer::SyncFlush: Allocation %p\n", ioRequest));
    if (ioRequest != NULL) {
        status = ioRequest->Write(buffer, size, startingOffset); // initiate the write
        if (status == STATUS_PENDING) {
            status = ioRequest->Wait(NULL);
        }
        ioRequest->Release();
        if (NT_SUCCESS(status)) {
            dirty = FALSE;
        }
    } else {
        status = STATUS_NO_MEMORY;
    }

    return status;
}

// read the buffer on the disk bypassing the file system
// do this CAREFULLY
NTSTATUS IOBuffer::SyncReadPhysical(BOOLEAN bForce /* Default to FALSE */)
{
TRC
    NTSTATUS Status = STATUS_SUCCESS;
    AsyncIORequest * ioRequest;

    ioRequest = NULL;

    if ((FALSE == dirty) && (FALSE == bForce)) {
        goto cleanup;
    }
    
    if (locked > 0) {
        Status = STATUS_DEVICE_BUSY;
        goto cleanup;
    }
    
    if (NULL == physicalDisk) {
        Status = STATUS_DEVICE_NOT_READY;
        goto cleanup;
    }

    ioRequest = new AsyncIORequest(physicalDisk, NULL);
    InVolDbgPrint(DL_INFO, DM_MEM_TRACE,
        ("IOBuffer::SyncReadPhysical: Allocation %p\n", ioRequest));
    if (ioRequest == NULL) {
        Status = STATUS_NO_MEMORY;
        goto cleanup;
    }

    Status = ioRequest->Read(buffer, size, physicalOffset); // initiate the write
    if (Status == STATUS_PENDING) {
        Status = ioRequest->Wait(NULL);
    }

    ioRequest->Release();
    if (NT_SUCCESS(Status)) {
        dirty = FALSE;
    }

cleanup:
    return Status;
}

// write the buffer to the disk bypassing the file system
// do this CAREFULLY
NTSTATUS IOBuffer::SyncFlushPhysical()
{
TRC
NTSTATUS status;
AsyncIORequest * ioRequest;

    ioRequest = NULL;

    if (!dirty) {
        status = STATUS_SUCCESS;
    } else if (locked > 0) {
        status = STATUS_DEVICE_BUSY;
    } else {
          if (NULL != physicalDisk) {
            ioRequest = new AsyncIORequest(physicalDisk, NULL);
            InVolDbgPrint(DL_INFO, DM_MEM_TRACE,
                ("IOBuffer::SyncFlushPhysical: Allocation %p\n", ioRequest));
            if (ioRequest != NULL) {
                status = ioRequest->Write(buffer, size, physicalOffset); // initiate the write
                if (status == STATUS_PENDING) {
                    status = ioRequest->Wait(NULL);
                }
                ioRequest->Release();
                if (NT_SUCCESS(status)) {
                    dirty = FALSE;
                }
            } else {
                status = STATUS_NO_MEMORY;
            }
          } else {
              status = STATUS_DEVICE_NOT_READY;
          }
    }

    return status;
}

// this makes a well defined write while watching all writes from above
// it's used to find the physical location of a file block to use at shutdown
void IOBuffer::LearnPhysicalIO()
{
TRC
NTSTATUS status;

    KeWaitForMutexObject(&DriverContext.WriteFilterMutex, 
                         Executive,
                         KernelMode,
                         FALSE,
                         NULL);

    DriverContext.WriteFilteringObject = this;

    // destroy the old values, prevents using stale data if we can't get new data
    if (NULL != physicalDisk) {
        ObDereferenceObject(physicalDisk);
        physicalDisk = NULL;
    }

    physicalOffset = 0;
    // force a write
    SetDirty();
    status = SyncFlush();

    DriverContext.WriteFilteringObject = NULL;

    if ((NULL != physicalDisk) && (0 != physicalOffset)) {
        InVolDbgPrint(DL_INFO, DM_SHUTDOWN,
            ("IOBuffer::LearnPhysicalIO matched a logical with a volume write, disk = %#p  offset = %I64x\n",
            physicalDisk, physicalOffset));
    }
    
    KeReleaseMutex(&DriverContext.WriteFilterMutex, FALSE);
}

void IOBuffer::ResetPhysicalDO()
{
    KeWaitForMutexObject(&DriverContext.WriteFilterMutex, 
                         Executive,
                         KernelMode,
                         FALSE,
                         NULL);
    if (NULL != physicalDisk) {
        ObDereferenceObject(physicalDisk);
        physicalDisk = NULL;
    }

    physicalOffset = 0;

    KeReleaseMutex(&DriverContext.WriteFilterMutex, FALSE);
}

// look at an IRP to decide if it's ours
void IOBuffer::LearnPhysicalIOFilter(IN PDEVICE_OBJECT DeviceObject,
                                     IN PIRP Irp)
{
TRC
PIO_STACK_LOCATION currentIrpStack;

    if (fileStream) {
        if (fileStream->IsOurWriteAtSizeAndOffset(Irp, size, startingOffset)) {
            currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
            if (currentIrpStack->MajorFunction == IRP_MJ_WRITE) {
                if (currentIrpStack->Parameters.Write.Length == size) {
                    physicalDisk = IoGetAttachedDeviceReference(DeviceObject);
                    physicalOffset = currentIrpStack->Parameters.Write.ByteOffset.QuadPart;
                    InVolDbgPrint(DL_INFO, DM_SHUTDOWN,
                        ("IOBuffer::LearnPhysicalIOFilter found an appropriate write in Irp %p\n", Irp));
                } else {
                    InVolDbgPrint(DL_INFO, DM_SHUTDOWN,
                        ("IOBuffer::LearnPhysicalIOFilter Volume write of size %x is different size than logical FS write\n",
                        currentIrpStack->Parameters.Write.Length ));
                }
            }
        }
    }
}





