#include <global.h>
#include "DataFileWriter.h"
#include "VContext.h"
#include "FileStream.h"
#include "HelperFunctions.h"
#include "DataFile.h"
#include "svdparse.h"
#include "DataFlt.h"
#include "misc.h"
#include "MntMgr.h"

NPAGED_LOOKASIDE_LIST   CDataFileWriter::sm_WorkItemLAL;

void
CDataFileWriter::InitializeLAL()
{
    ExInitializeNPagedLookasideList(&sm_WorkItemLAL,
                                    NULL, 
                                    NULL, 
                                    0, 
                                    sizeof(struct WORK_ITEM), 
                                    TAG_FILE_WRITER_ENTRY, 
                                    0);

    return;
}

void
CDataFileWriter::CleanLAL()
{
    ExDeleteNPagedLookasideList(&sm_WorkItemLAL);
    return;
}

void
CDataFileWriter::FreeWorkItem(PWORK_ITEM WorkItem)
{
    DereferenceChangeNode(WorkItem->ChangeNode);
    DereferenceVolumeContext(WorkItem->VolumeContext);

    ExFreeToNPagedLookasideList(&sm_WorkItemLAL, WorkItem);
    return;
}

CDataFileWriter::CDataFileWriter(ULONG ulNumberOfThreads) : 
    m_ulThreadPriority(0),
    m_ulNumberOfThreads(ulNumberOfThreads),
    m_bTerminating(false),
    m_bFlush(false)
{
    NTSTATUS    Status;

    ASSERT(0 != m_ulNumberOfThreads);
    if (0 == m_ulNumberOfThreads) {
        m_ulNumberOfThreads = 0x01;
    }

    KeInitializeMutex(&m_Mutex, 0);
    KeInitializeSpinLock(&m_Lock);
    InitializeListHead(&m_WorkItemList);
    InitializeListHead(&m_ThreadList);
    KeInitializeEvent(&m_ReadyEvent, SynchronizationEvent, FALSE);

    for (unsigned long ul = 0; ul < m_ulNumberOfThreads; ul++) {
        Status = CreateFileWriterThread(m_ulThreadPriority);
        ASSERT(STATUS_SUCCESS == Status);
    }

    return;
}

CDataFileWriter::~CDataFileWriter()
{
    StopAndWaitToComplete();
}

NTSTATUS
CDataFileWriter::SetPriority(ULONG ulPriority)
{
    NTSTATUS    Status;
    if (0 == ulPriority) {
        InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, 
                      ("%s: ignoring as priority is %#d\n", __FUNCTION__, ulPriority));
        return STATUS_SUCCESS;
    }

    if (ulPriority == m_ulThreadPriority) {
        return STATUS_SUCCESS;
    }

    Status = KeWaitForSingleObject(&m_Mutex, Executive, KernelMode, FALSE, NULL);
    ASSERT(STATUS_SUCCESS == Status);

    PLIST_ENTRY ListEntry = m_ThreadList.Flink;
    while (&m_ThreadList != ListEntry) {
        PFILE_WRITER_THREAD FileWriterThread = (PFILE_WRITER_THREAD)ListEntry;

        Status = KeSetPriorityThread(FileWriterThread->ThreadObject, ulPriority);
        if (NT_SUCCESS(Status)) {
            FileWriterThread->ThreadPriority = ulPriority;
            m_ulThreadPriority = ulPriority;
            InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, 
                          ("%s: thread(%#p) priority is set to %#d\n", __FUNCTION__, 
                           FileWriterThread->ThreadObject, ulPriority));
        } else {
            InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, 
                          ("%s: Failed to set thread(%#p) priority to %#d\n", __FUNCTION__,
                            FileWriterThread->ThreadObject, ulPriority));
        }

        ListEntry = ListEntry->Flink;
    }

    KeReleaseMutex(&m_Mutex, FALSE);

    return Status;
}

void
CDataFileWriter::FlushQueue()
{
    NTSTATUS    Status;
    KIRQL       OldIrql;

    KeAcquireSpinLock(&m_Lock, &OldIrql);
    m_bFlush = true;
    KeReleaseSpinLock(&m_Lock, OldIrql);

    Status = KeWaitForSingleObject(&m_Mutex, Executive, KernelMode, FALSE, NULL);
    ASSERT(STATUS_SUCCESS == Status);

    PLIST_ENTRY ListEntry = m_ThreadList.Flink;
    while (&m_ThreadList != ListEntry) {
        PFILE_WRITER_THREAD FileWriterThread = (PFILE_WRITER_THREAD)ListEntry;

        // Set FlushEvent, so the thread can complete the current node, and wake up on flush event
        KeSetEvent(&FileWriterThread->FlushEvent, EVENT_INCREMENT, FALSE);

        Status = KeWaitForSingleObject(&FileWriterThread->FlushCompletedEvent, Executive, KernelMode, FALSE, NULL);
        ASSERT(STATUS_SUCCESS == Status);

        ListEntry = ListEntry->Flink;
    }

    KeReleaseMutex(&m_Mutex, FALSE);

    KeAcquireSpinLock(&m_Lock, &OldIrql);
    m_bFlush = false;
    KeReleaseSpinLock(&m_Lock, OldIrql);
    return;
}

void
CDataFileWriter::StopAndWaitToComplete()
{
    KIRQL       OldIrql;
    bool     bWait = false;

    KeAcquireSpinLock(&m_Lock, &OldIrql);
    if (!m_bTerminating) {
        m_bTerminating = true;
        bWait = true;
    }
    KeReleaseSpinLock(&m_Lock, OldIrql);

    if (bWait) {
        NTSTATUS Status = KeWaitForSingleObject(&m_Mutex, Executive, KernelMode, FALSE, NULL);
        ASSERT(STATUS_SUCCESS == Status);

        while (!IsListEmpty(&m_ThreadList)) {
            PFILE_WRITER_THREAD FileWriterThread = (PFILE_WRITER_THREAD)RemoveHeadList(&m_ThreadList);

            KeSetEvent(&FileWriterThread->TerminateEvent, EVENT_INCREMENT, FALSE);

            KeWaitForSingleObject(FileWriterThread->ThreadObject, Executive, KernelMode, FALSE, NULL);
            ObDereferenceObject(FileWriterThread->ThreadObject);
            FileWriterThread->ThreadObject = NULL;
            DeallocateFileWriterThreadEntry(FileWriterThread);
        }

        KeReleaseMutex(&m_Mutex, FALSE);
    }

    return;
}

NTSTATUS
CDataFileWriter::AddChangeNode(PCHANGE_NODE ChangeNode, PVOLUME_CONTEXT VolumeContext)
{
    PWORK_ITEM  WorkItem;
    KIRQL       OldIrql;
    NTSTATUS    Status;

    InVolDbgPrint(DL_FUNC_TRACE, DM_DATA_FILTERING, ("%s: Called\n", __FUNCTION__));

    KeAcquireSpinLock(&m_Lock, &OldIrql);
    if (m_bTerminating) {
        // Queue is terminating, do not add any more nodes to this list.
        Status = INVOLFLT_THREAD_SHUTDOWN_IN_PROGRESS;
        InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, ("%s: Thread shutdown in progress\n", __FUNCTION__));
    } else {
        WorkItem = (PWORK_ITEM) ExAllocateFromNPagedLookasideList(&sm_WorkItemLAL);
        if (NULL != WorkItem) {
            WorkItem->ChangeNode = ReferenceChangeNode(ChangeNode);
            WorkItem->VolumeContext = ReferenceVolumeContext(VolumeContext);

            InsertTailList(&m_WorkItemList, &WorkItem->ListEntry);
            Status = STATUS_SUCCESS;
        } else {
            InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("%s: Failed to allocate workitem\n", __FUNCTION__));
            Status = STATUS_NO_MEMORY;
        }
    }
    KeReleaseSpinLock(&m_Lock, OldIrql);

    if (STATUS_SUCCESS == Status) {
        InVolDbgPrint(DL_TRACE_L1, DM_DATA_FILTERING, ("%s: setting ready event\n", __FUNCTION__));
        KeSetEvent(&m_ReadyEvent, EVENT_INCREMENT, FALSE);
    }

    return Status;
}

NTSTATUS
CDataFileWriter::WriteChangeNodeToFile(CDataFile& File, PCHANGE_NODE ChangeNode)
{
    NTSTATUS        Status = STATUS_SUCCESS;
    PKDIRTY_BLOCK   DirtyBlock = ChangeNode->DirtyBlock;

    PDATA_BLOCK DataBlock = (PDATA_BLOCK)DirtyBlock->DataBlockList.Flink;
    while (&DataBlock->ListEntry != &DirtyBlock->DataBlockList) {
        Status = File.Write(DataBlock->KernelAddress, DataBlock->ulcbMaxData, 
                        DataBlock->ulcbMaxData - DataBlock->ulcbDataFree);
        if (!NT_SUCCESS(Status)) {
            return Status;
        }
        DataBlock = (PDATA_BLOCK)DataBlock->ListEntry.Flink;
    }

    return Status;
}

void
CDataFileWriter::AbortChangeNode(
    PCHANGE_NODE ChangeNode,
    PVOLUME_CONTEXT VolumeContext
    )
{
    NTSTATUS        Status = STATUS_SUCCESS;
    ULONG           ulDataBlocksProcessed = 0;
    KIRQL           OldIrql;

    InVolDbgPrint(DL_TRACE_L1, DM_DATA_FILTERING, 
                  ("%s: Called for ChangeNode = %#p, VC = %#p\n", __FUNCTION__, ChangeNode, VolumeContext));
    Status = KeWaitForSingleObject(&ChangeNode->Mutex, Executive, KernelMode, FALSE, NULL);
    ASSERT(STATUS_SUCCESS == Status);

    do {
        if (!(ChangeNode->ulFlags & CHANGE_NODE_FLAGS_QUEUED_FOR_DATA_WRITE)) {
            // This change node is already processed by get dirty block thread.
            // Do not write this change node to file.
            InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, ("%s: This Change node is already processed by GetDBTrans\n", __FUNCTION__));
            break;
        }

        KeAcquireSpinLock(&VolumeContext->Lock, &OldIrql);
        VolumeContext->lDataBlocksQueuedToFileWrite -= ChangeNode->DirtyBlock->ulDataBlocksAllocated;;
        // Remove the flag as we processed this change node.
        // This flag has to be set/unset after acquiring the volume context lock
        ChangeNode->ulFlags &= ~CHANGE_NODE_FLAGS_QUEUED_FOR_DATA_WRITE;
        KeReleaseSpinLock(&VolumeContext->Lock, OldIrql);

    } while(0);

    KeReleaseMutex(&ChangeNode->Mutex, FALSE);

    return;
}

void
CDataFileWriter::ProcessChangeNode(
    PFILE_WRITER_THREAD FileWriterThread,
    PCHANGE_NODE ChangeNode,
    PVOLUME_CONTEXT VolumeContext
    )
{
    NTSTATUS        Status = STATUS_SUCCESS;
    ULONG           ulDataBlocksProcessed = 0;
    KIRQL           OldIrql;

    InVolDbgPrint(DL_TRACE_L1, DM_DATA_FILTERING, 
                  ("%s: Called for ChangeNode = %#p, VC = %#p\n", __FUNCTION__, ChangeNode, VolumeContext));
    Status = KeWaitForSingleObject(&ChangeNode->Mutex, Executive, KernelMode, FALSE, NULL);
    ASSERT(STATUS_SUCCESS == Status);

    do {
        if (!(ChangeNode->ulFlags & CHANGE_NODE_FLAGS_QUEUED_FOR_DATA_WRITE)) {
            // This change node is already processed by get dirty block thread.
            // Do not write this change node to file.
            InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, ("%s: This Change node is already processed by GetDBTrans\n", __FUNCTION__));
            break;
        }

        KeAcquireSpinLock(&VolumeContext->Lock, &OldIrql);
        VolumeContext->lDataBlocksQueuedToFileWrite -= ChangeNode->DirtyBlock->ulDataBlocksAllocated;;
        KeReleaseSpinLock(&VolumeContext->Lock, OldIrql);

        Status = KeWaitForSingleObject(&VolumeContext->Mutex, Executive, KernelMode, FALSE, NULL);
        ASSERT(STATUS_SUCCESS == Status);
        if (VolumeContext->ullcbDataWrittenToDisk >= VolumeContext->ullcbDataToDiskLimit) {
            // Already data limit on disk has been reached for this volume
            // do not write this data to disk.
            InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, 
                          ("%s: DataLimit(%#I64x) is reached on this disk(%#I64x)\n", __FUNCTION__,
                           VolumeContext->ullcbDataToDiskLimit, VolumeContext->ullcbDataWrittenToDisk));            
            VolumeContext->bDataLogLimitReached = true;
            KeReleaseMutex(&VolumeContext->Mutex, FALSE);
            break;
        }

        FinalizeDataFileStreamInDirtyBlock(ChangeNode->DirtyBlock);
        Status = GenerateDataFileName(ChangeNode->DirtyBlock);

        KeReleaseMutex(&VolumeContext->Mutex, FALSE);
        if (!NT_SUCCESS(Status)) {
            // Failed to generate the file name.
            InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("%s: Failed to generate data file name\n", __FUNCTION__));
            break;
        }

        // Open the file
        CDataFile   File;

        Status = File.Open(&ChangeNode->DirtyBlock->FileName, ChangeNode->DirtyBlock->ulMaxDataSizePerDirtyBlock, DriverContext.ulDataBlockSize);
        if (STATUS_SUCCESS != Status) {
            InMageFltLogError(VolumeContext->FilterDeviceObject,__LINE__, INVOLFLT_DATA_FILE_OPEN_FAILED, STATUS_SUCCESS,
                VolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES, VolumeContext->wGUID, GUID_SIZE_IN_BYTES, (ULONG)Status);
            InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("%s: file open failed\n", __FUNCTION__));
            break;
        }

        // write data
        Status = WriteChangeNodeToFile(File, ChangeNode);
        if (STATUS_SUCCESS != Status) {
            InMageFltLogError(VolumeContext->FilterDeviceObject,__LINE__, INVOLFLT_WRITE_TO_DATA_FILE_FAILED, STATUS_SUCCESS,
                VolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES, VolumeContext->wGUID, GUID_SIZE_IN_BYTES,
                ChangeNode->DirtyBlock->FileName.Buffer,  ChangeNode->DirtyBlock->FileName.Length, Status);
            InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("%s: write change node to file failed\n", __FUNCTION__));
            break;
        }

        ChangeNode->DirtyBlock->ullFileSize = File.GetFileSize();

        InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, 
                      ("%s: Data written to disk %#I64x, FileSize = %#I64x\n", __FUNCTION__, VolumeContext->ullcbDataWrittenToDisk, File.GetFileSize()));

        // close file
        Status = File.Close();
        if (STATUS_SUCCESS != Status) {
            
            ChangeNode->DirtyBlock->ullFileSize = 0;

            InMageFltLogError(VolumeContext->FilterDeviceObject,__LINE__, INVOLFLT_WRITE_TO_DATA_FILE_FAILED, STATUS_SUCCESS,
                VolumeContext->wDriveLetter, VOLUME_LETTER_SIZE_IN_BYTES, VolumeContext->wGUID, GUID_SIZE_IN_BYTES,
                ChangeNode->DirtyBlock->FileName.Buffer,  ChangeNode->DirtyBlock->FileName.Length, Status);
            InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("%s: write change node to file failed\n", __FUNCTION__));
            break;
        }

        // Free data blocks.
        KeAcquireSpinLock(&VolumeContext->Lock, &OldIrql);
        DBFreeDataBlocks(ChangeNode->DirtyBlock, true);
        ASSERT(ecChangeNodeDirtyBlock == ChangeNode->eChangeNode);
        ChangeNode->eChangeNode = ecChangeNodeDataFile;
        KeReleaseSpinLock(&VolumeContext->Lock, OldIrql);

        Status = KeWaitForSingleObject(&VolumeContext->Mutex, Executive, KernelMode, FALSE, NULL);
        ASSERT(STATUS_SUCCESS == Status);
        VolumeContext->ChangeList.ulDataFilesPending++;
        VolumeContext->ullcbDataWrittenToDisk += ChangeNode->DirtyBlock->ullFileSize;
        KeReleaseMutex(&VolumeContext->Mutex, FALSE);

    } while(0);

    KeAcquireSpinLock(&VolumeContext->Lock, &OldIrql);

    // Remove the flag as we processed this change node.
    // This flag has to be set/unset after acquiring the volume context lock
    ChangeNode->ulFlags &= ~CHANGE_NODE_FLAGS_QUEUED_FOR_DATA_WRITE;
    if (!NT_SUCCESS(Status)) {
        // If failed in data write do not keep trying for this dirty block again.
        ChangeNode->ulFlags |= CHANGE_NODE_FLAGS_ERROR_IN_DATA_WRITE;
    }

    KeReleaseSpinLock(&VolumeContext->Lock, OldIrql);
    KeReleaseMutex(&ChangeNode->Mutex, FALSE);

    return;
}

void 
CDataFileWriter::FlushQueue(PFILE_WRITER_THREAD FileWriterThread)
{
    KIRQL           OldIrql;
    CDataFileWriter *writer = FileWriterThread->parent;

    KeAcquireSpinLock(&writer->m_Lock, &OldIrql);
    while (!IsListEmpty(&writer->m_WorkItemList)) {
        PWORK_ITEM  WorkItem = (PWORK_ITEM) RemoveHeadList(&writer->m_WorkItemList);
        writer->AbortChangeNode(WorkItem->ChangeNode, WorkItem->VolumeContext);
        writer->FreeWorkItem(WorkItem);
    }
    KeReleaseSpinLock(&writer->m_Lock, OldIrql);

    return;
}

void
CDataFileWriter::ThreadFunction(PVOID Context)
{
    PFILE_WRITER_THREAD FileWriterThread = (PFILE_WRITER_THREAD)Context;
    CDataFileWriter *writer = FileWriterThread->parent;
    PVOID           WaitObjects[3];
    NTSTATUS        Status;
    bool            bShutdownThread = false;
    PKTHREAD        CurrentThread = KeGetCurrentThread();
    WaitObjects[0] = &FileWriterThread->FlushEvent;
    WaitObjects[1] = &FileWriterThread->TerminateEvent;
    WaitObjects[2] = &writer->m_ReadyEvent;

    do {
        Status = KeWaitForMultipleObjects(3, WaitObjects, WaitAny, Executive, KernelMode, FALSE, NULL, NULL);
        InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, ("%s Out of wait %p %#x\n", __FUNCTION__, CurrentThread, Status));
        if (!NT_SUCCESS(Status)) {
            InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("%s wait has failed with status %#x\n", __FUNCTION__, Status));
            return;
        }

        if (STATUS_WAIT_0 == Status) {
            // Flush all work items.
            InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, ("%s: Queue is flushing down.\n", __FUNCTION__));
            FlushQueue(FileWriterThread);
            KeSetEvent(&FileWriterThread->FlushCompletedEvent, EVENT_INCREMENT, FALSE);
        } else if (STATUS_WAIT_1 == Status) {
            KIRQL           OldIrql;

            bShutdownThread = true;
            InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, ("%s: as thread is shutting down, freeing all work items\n", __FUNCTION__));

            // As we are shutting down the thread, we have to free all the nodes in the list.
            FlushQueue(FileWriterThread);
            // exit from loop
            break;
        } else if (STATUS_WAIT_2 == Status) {
            KIRQL       OldIrql;
            PWORK_ITEM  WorkItem;
            bool        bRepeat;
            bool        bFlush = false;

            InVolDbgPrint(DL_TRACE_L1, DM_DATA_FILTERING, ("%s: Processing work items\n", __FUNCTION__));

            do {
                WorkItem = NULL;
                bRepeat = false;

                InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, ("%s(%p): Getting change node\n", __FUNCTION__, CurrentThread));
                KeAcquireSpinLock(&writer->m_Lock, &OldIrql);
                if (!IsListEmpty(&writer->m_WorkItemList)) {
                    WorkItem = (PWORK_ITEM) RemoveHeadList(&writer->m_WorkItemList);
                    bFlush = writer->m_bFlush;
                }
                KeReleaseSpinLock(&writer->m_Lock, OldIrql);

                if (NULL != WorkItem) {
                    InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, ("%s(%p): Got a change node\n", __FUNCTION__, CurrentThread));
                    // Process the item
                    if (bFlush) {
                        writer->AbortChangeNode(WorkItem->ChangeNode, WorkItem->VolumeContext);
                    } else {
                        writer->ProcessChangeNode(FileWriterThread, WorkItem->ChangeNode, WorkItem->VolumeContext);
                        InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, ("%s(%p): Processed change node\n", __FUNCTION__, CurrentThread));
                    }
                    writer->FreeWorkItem(WorkItem);
                    InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, ("%s(%p): Freed work item\n", __FUNCTION__, CurrentThread));
                    bRepeat = true;
                } else {
                    InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, ("%s(%p): No change node\n", __FUNCTION__, CurrentThread));
                }
            } while ( bRepeat );
        } // (STATUS_WAIT_0 == Status)
        
        // This condition would never be true. When it is true anyway we break out of the loop.
        // This is used just to avoid compilation warning in strict error checking.
    } while (!bShutdownThread);

    return;
}

NTSTATUS
CDataFileWriter::CreateFileWriterThread(ULONG &ulThreadPriority)
{
    PFILE_WRITER_THREAD FileWriterThread = AllocateFileWriterThreadEntry();

    if (NULL == FileWriterThread) {
        return STATUS_NO_MEMORY;
    }

    HANDLE      hThread;
    NTSTATUS    Status;

    Status = PsCreateSystemThread(&hThread, THREAD_ALL_ACCESS, NULL, NULL, NULL, CDataFileWriter::ThreadFunction, FileWriterThread);
    if (NT_SUCCESS(Status)) {
        Status = ObReferenceObjectByHandle(hThread,
                                           SYNCHRONIZE,
                                           NULL,
                                           KernelMode,
                                           (PVOID *)&FileWriterThread->ThreadObject,
                                           NULL);
        if (NT_SUCCESS(Status)) {
            ZwClose(hThread);
        }

        FileWriterThread->ThreadPriority = KeQueryPriorityThread(FileWriterThread->ThreadObject);
        ulThreadPriority = FileWriterThread->ThreadPriority;
        Status = KeWaitForSingleObject(&m_Mutex, Executive, KernelMode, FALSE, NULL);
        ASSERT(STATUS_SUCCESS == Status);
        InsertTailList(&m_ThreadList, &FileWriterThread->ListEntry);
        KeReleaseMutex(&m_Mutex, FALSE);
    } else {
        DeallocateFileWriterThreadEntry(FileWriterThread);
    }

    return Status;
}

CDataFileWriter::PFILE_WRITER_THREAD
CDataFileWriter::AllocateFileWriterThreadEntry()
{
    PFILE_WRITER_THREAD FileWriterThread = (PFILE_WRITER_THREAD) ExAllocatePoolWithTag(NonPagedPool, 
                                                                                       sizeof(FILE_WRITER_THREAD), 
                                                                                       TAG_FILE_WRITER_THREAD_ENTRY);

    if (NULL != FileWriterThread) {
        RtlZeroMemory(FileWriterThread, sizeof(FILE_WRITER_THREAD));

        KeInitializeEvent(&FileWriterThread->TerminateEvent, NotificationEvent, FALSE);
        KeInitializeEvent(&FileWriterThread->FlushEvent, SynchronizationEvent, FALSE);
        KeInitializeEvent(&FileWriterThread->FlushCompletedEvent, SynchronizationEvent, FALSE);

        FileWriterThread->parent = this;
    }

    return FileWriterThread;
}

void
CDataFileWriter::DeallocateFileWriterThreadEntry(PFILE_WRITER_THREAD FileWriterThread)
{
    ASSERT(NULL != FileWriterThread);

    if (NULL != FileWriterThread) {
        ASSERT(NULL == FileWriterThread->ThreadObject);
        ExFreePoolWithTag(FileWriterThread, TAG_FILE_WRITER_THREAD_ENTRY);
    }
}
