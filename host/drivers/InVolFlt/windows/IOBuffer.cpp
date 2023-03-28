//
// Copyright InMage Systems 2004
//

#include "global.h"
#include "VContext.h"
#include "misc.h"

NPAGED_LOOKASIDE_LIST IOBuffer::objectLookasideList;
NPAGED_LOOKASIDE_LIST IOBuffer::dataLookasideList;

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

    ExInitializeNPagedLookasideList(&IOBuffer::dataLookasideList,
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
    ExDeleteNPagedLookasideList(&IOBuffer::dataLookasideList);
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
UNREFERENCED_PARAMETER(alignment);

    memory = (IOBuffer *)ExAllocateFromNPagedLookasideList(&IOBuffer::objectLookasideList);
    
    if (memory) {
        RtlZeroMemory((void *)memory, baseSize);
        //Allocate NonPage Pool to avoid page fault on shutdown
        if (BITMAP_FILE_SEGMENT_SIZE == bufferSize) {
            memory->allocationPointer = ExAllocateFromNPagedLookasideList(&IOBuffer::dataLookasideList);
        } else {
            memory->allocationPointer = ExAllocatePoolWithTag(NonPagedPool, bufferSize, TAG_IOBUFFER_DATA);
        }
        if (memory->allocationPointer) {
            RtlZeroMemory(memory->allocationPointer, bufferSize);
            memory->size = (ULONG32) bufferSize;
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
                ExFreeToNPagedLookasideList(&IOBuffer::dataLookasideList, memory->allocationPointer);
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
    physicalDisk = NULL;
    BitMapDevObject = NULL;
}

IOBuffer::IOBuffer(FileStreamSegmentMapper * parent, ULONG32 index)
{
TRC
    locked = 0;
    dirty = false;
    owner = parent;
    ownerIndex = index;
    physicalDisk = NULL;
    BitMapDevObject = NULL;
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

    Status = rawIoWrapper.RawReadFile(physicalDisk, buffer, startingOffset, size);

    if (!NT_SUCCESS(Status)) {
        goto cleanup;
    }

    dirty = FALSE;

cleanup:
    return Status;
}

// write the buffer to the disk bypassing the file system
// do this CAREFULLY
NTSTATUS IOBuffer::SyncFlushPhysical()
{
TRC
    NTSTATUS status;

    if (FALSE == dirty) {
        status = STATUS_SUCCESS;
        goto Cleanup;
    }

    if (locked > 0) {
        status = STATUS_DEVICE_BUSY;
        goto Cleanup;
    }

    if (NULL == physicalDisk) {
        status = STATUS_DEVICE_NOT_READY;
        goto Cleanup;
    }

    status = rawIoWrapper.RawWriteFile(physicalDisk, buffer, startingOffset, size);

    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

    dirty = FALSE;

Cleanup:
    return status;
}

// learns the physical disk offsets of the file using a set of well-defined FSCTLs
// and volmgr IOCTLs.
NTSTATUS IOBuffer::LearnPhysicalIO()
{
TRC
    NTSTATUS        status;
    ULONG           ulDiskNumber;
    PDEV_CONTEXT    pDevContext = NULL;
    PLIST_ENTRY     pDevCtxtListEntry;
    KIRQL           oldIrql;
    bool            bFoundPhysicalDevice = false;

    if (physicalDisk != NULL || BitMapDevObject != NULL) {
        status = STATUS_DEVICE_ALREADY_ATTACHED;

        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN | DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": Already learnt/stale device objects found. physicalDisk %#p. BitmapDevObject %#p. Status Code %#x.\n",
            physicalDisk, BitMapDevObject, status));

        NT_ASSERT(FALSE);
        goto Cleanup;
    }

    status = rawIoWrapper.LearnDiskOffsets(
        fileStream->GetFileHandle(),
        startingOffset,
        startingOffset + size);

    if (!NT_SUCCESS(status)) {
        InDskFltWriteEvent(INDSKFLT_ERROR_LEARNPHYSICALIO_LEARNING,
            fileStream->GetFileHandle(),
            startingOffset,
            startingOffset + size,
            status);

        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN | DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": Error in learning disk offsets of bitmap file. File object %#p. Status Code %#x.\n",
            fileStream->GetFileObject(), status));

        goto Cleanup;
    }

    InDskFltWriteEvent(INDSKFLT_INFO_LEARNPHYSICALIO_LEARNING,
        fileStream->GetFileHandle(),
        startingOffset,
        startingOffset + size,
        status);

    status = rawIoWrapper.GetDiskDeviceNumber(&ulDiskNumber);

    if (!NT_SUCCESS(status)) {
        InDskFltWriteEvent(INDSKFLT_ERROR_LEARNPHYSICALIO_DISK_OFFSET,
            fileStream->GetFileHandle(),
            status);

        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN | DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": Error in getting disk number of device storing bitmap file. File object %#p. Status Code %#x.\n",
            fileStream->GetFileObject(), status));

        NT_ASSERT(FALSE);
        goto Cleanup;
    }

    KeAcquireSpinLock(&DriverContext.Lock, &oldIrql);

    pDevCtxtListEntry = DriverContext.HeadForDevContext.Flink;

    while (NT_SUCCESS(status) && !bFoundPhysicalDevice && pDevCtxtListEntry != &DriverContext.HeadForDevContext) {

        pDevContext = CONTAINING_RECORD(pDevCtxtListEntry, DEV_CONTEXT, ListEntry);

        KeAcquireSpinLockAtDpcLevel(&pDevContext->Lock);

        if (TEST_FLAG(pDevContext->ulFlags, DCF_DEVNUM_OBTAINED) && pDevContext->ulDevNum == ulDiskNumber) {

            // TODO-SanKumar-1805: If the bitmap is not on the boot disk and if
            // that data disk has been surprise removed and another disk is added
            // back, we might end up writing to the wrong disk.
            // To handle this, we should run through the entire list and check if
            // there's another disk with duplicate disk number. If so, we should
            // fail this method stating we can't determine among multiple disks.
            // Another way might be to implement the tracking of removed disk numbers
            // at the beginning of call and then check if the disk number is
            // present in the removed list after coming into the DriverContext lock.
            bFoundPhysicalDevice = true;

            // TODO-SanKumar-1805: Bad referencing (as there's no corresponding
            // dereference) - continuing this same method which has been there
            // for 2 years. Since we assume that the bitmap can reside on boot
            // disk only for which remove never comes, this holds good. But for
            // completeness, we should take the reference on both the physical
            // and the filter device object. Then, on every disk removal, we should
            // check if that disk is the bitmap device object for any of the other
            // connected disks and if so, we should dereference these objects then.
            // Also in the destructor of the IOBuffer class.

            // Take reference to object
            status = ObReferenceObjectByPointer(pDevContext->TargetDeviceObject,
                FILE_ALL_ACCESS,
                NULL,
                KernelMode);

            if (NT_SUCCESS(status)) {
                if (pDevContext->TargetDeviceObject != NULL && pDevContext->FilterDeviceObject != NULL) {
                    // Store the target object to issue direct writes.
                    physicalDisk = pDevContext->TargetDeviceObject;
                    BitMapDevObject = pDevContext->FilterDeviceObject;

                    SetDevCntFlag(pDevContext, DCF_BITMAP_DEV, true);
                } else {
                    NT_ASSERT(FALSE);
                    status = STATUS_INVALID_DEVICE_STATE;

                    InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN | DM_FILE_RAW_WRAPPER,
                        (__FUNCTION__ ": Either or both of TargetDeviceObject %#p and FilterDeviceObject %#p are invalid. DevContext %#p. Status Code %#x.\n",
                        pDevContext->TargetDeviceObject, pDevContext->FilterDeviceObject, pDevContext, status));
                }
            } else {
                InDskFltWriteEventWithDevCtxt(INDSKFLT_ERROR_FAILED_TO_GET_REF_BITMAPDEV, pDevContext, status);
            }
        }

        KeReleaseSpinLockFromDpcLevel(&pDevContext->Lock);

        pDevCtxtListEntry = pDevCtxtListEntry->Flink;
    }

    KeReleaseSpinLock(&DriverContext.Lock, oldIrql);

    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

    // TODO-SanKumar-1805: Device number is only acquired on best effort basis.
    // Should we retry the retrieval of the disk number again for the devnum
    // missing devices?

    if (!bFoundPhysicalDevice) {

        InDskFltWriteEvent(INDSKFLT_ERROR_BITMAP_DISK_NOT_FOUND, ulDiskNumber);

        status = STATUS_DEVICE_DOES_NOT_EXIST;

        InVolDbgPrint(DL_ERROR, DM_BITMAP_OPEN | DM_FILE_RAW_WRAPPER,
            (__FUNCTION__ ": Couldn't find disk device with number %lu storing bitmap file. File object %#p. Status Code %#x.\n",
            ulDiskNumber, fileStream->GetFileObject(), status));

        goto Cleanup;
    }

    status = STATUS_SUCCESS;

Cleanup:

    if (!NT_SUCCESS(status)) {
        UnlearnPhysicalIO();
    }

    return status;
}

void IOBuffer::UnlearnPhysicalIO()
{
TRC
    SafeObDereferenceObject(physicalDisk);

    BitMapDevObject = NULL;

    // Cleanup the learnt info, so LearnPhysicalIO() could be invoked again.
    rawIoWrapper.CleanupLearntInfo();

    // Not clearing the DCF_BITMAP_DEV from the pDevContext of the BitMapDevObject,
    // as it could've been set by other disks for their bitmaps.
}

NTSTATUS
IOBuffer::RecheckLearntInfo()
{
TRC
    return rawIoWrapper.RecheckLearntInfo();
}

PDEVICE_OBJECT
IOBuffer::GetBitmapDevObjLearn (
    )
{
TRC
    return BitMapDevObject;
}

#ifdef VOLUME_FLT
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

    if (NULL != BitMapDevObject) {
        BitMapDevObject = NULL;
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
                        ("IOBuffer::LearnPhysicalIOFilter device write of size %x is different size than logical FS write\n",
                        currentIrpStack->Parameters.Write.Length ));
                }
            }
        }
    }
}
#endif