#include <global.h>
#include <DataFileWriterMgr.h>
#include <VContext.h>

#define COMMON_WRITER_BIT   0x40000000

CDataFileWriterManager::CDataFileWriterManager(ULONG ulNumThreadsPerFileWriter, ULONG ulNumCommonFileWriters) : 
    m_ulNumberOfThreadsPerFileWriter(ulNumThreadsPerFileWriter),
    m_ulNumberOfCommonFileWriters(ulNumCommonFileWriters)
{
    if (0 == m_ulNumberOfThreadsPerFileWriter) {
        // If number of file writers are zero, then we have to use file writers eqaul to number of processors.
        KAFFINITY Affinitiy = KeQueryActiveProcessors();
        ULONG       ulNumberOfBits = sizeof(KAFFINITY) * 0x08;

        for (unsigned long i = 0; i < ulNumberOfBits; i++) {
            KAFFINITY   CurrentBit = 0x01;
            CurrentBit = CurrentBit << i;
            if (Affinitiy & CurrentBit) {
                m_ulNumberOfThreadsPerFileWriter++;
                m_ulNumberOfThreadsPerFileWriter++;
            }
        }

        InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, 
                      ("%s: Number of processors = %#x\n", __FUNCTION__, m_ulNumberOfThreadsPerFileWriter));
        if (0 == m_ulNumberOfThreadsPerFileWriter) {
            m_ulNumberOfThreadsPerFileWriter = 0x04;
            InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING,
                          ("%s: Failed to get number of processors setting to %#x\n", __FUNCTION__, m_ulNumberOfThreadsPerFileWriter));
        }
    }

    if (0 == m_ulNumberOfCommonFileWriters) {
        m_ulNumberOfCommonFileWriters = DEFAULT_NUMBER_OF_COMMON_FILE_WRITERS;
    }

    // Not using the most significant bit as it would be a sign bit.
    m_lNextWriterId = COMMON_WRITER_BIT;
    InitializeListHead(&m_WriterList);
    KeInitializeMutex(&m_Mutex, 0);

    for (unsigned long ul = 0; ul < m_ulNumberOfCommonFileWriters; ul++) {
        ULONG ulWriterId = InterlockedIncrement(&m_lNextWriterId);
        PWRITER_NODE WriterNode = AllocateWriterNode(ulWriterId);
        if (NULL != WriterNode) {
            InsertToWriterList(WriterNode, false);
        }
    }

    m_WriterNodeUsedForLastVolume = NULL;
}

CDataFileWriterManager::~CDataFileWriterManager()
{
    return;
}

NTSTATUS
CDataFileWriterManager::AddWorkItem(PVOLUME_NODE VolumeNode,  PCHANGE_NODE ChangeNode, PVOLUME_CONTEXT VolumeContext)
{
    NTSTATUS    Status = STATUS_SUCCESS;

    ASSERT(NULL != VolumeNode);
    ASSERT(NULL != VolumeNode->WriterNode);
    ASSERT(NULL != VolumeNode->WriterNode->writer);

    if (NULL == VolumeNode) {
        InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("%s: InvalidParameter VolumeNode is NULL\n", __FUNCTION__));
        return STATUS_INVALID_PARAMETER;
    }

    if (NULL == VolumeNode->WriterNode) {
        InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("%s: InvalidParameter - WriteNode is NULL. VolumeNode = %#p\n", __FUNCTION__, VolumeNode));
        return STATUS_INVALID_PARAMETER;
    }

    if (NULL == VolumeNode->WriterNode->writer) {
        InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, 
                      ("%s: InvalidParameter writer is NULL. VolumeNode = %#p, WriterNode = %#p\n",
                        __FUNCTION__, VolumeNode, VolumeNode->WriterNode));
        return STATUS_INVALID_PARAMETER;
    }

    VolumeNode->WriterNode->writer->AddChangeNode(ChangeNode, VolumeContext);

    return STATUS_SUCCESS;
}

CDataFileWriterManager::PVOLUME_NODE
CDataFileWriterManager::NewVolume(PVOLUME_CONTEXT VolumeContext)
{
    NTSTATUS        Status;
    PVOLUME_NODE    VolumeNode = NULL;
    PWRITER_NODE    WriterNode = NULL;

    VolumeNode = AllocateVolumeNode(VolumeContext);
    if (NULL != VolumeNode) {
        Status = KeWaitForSingleObject(&m_Mutex, Executive, KernelMode, FALSE, NULL);
        ASSERT(STATUS_SUCCESS == Status);

        WriterNode = GetWriterNode(VolumeContext->ulWriterId);
        if (NULL == WriterNode) {
            ASSERT(VolumeContext->ulWriterId);
            WriterNode = AllocateWriterNode(VolumeContext->ulWriterId);
            ASSERT(NULL != WriterNode);
            if (NULL != WriterNode) {
                InsertToWriterList(WriterNode, true);
            }
        }

        if (NULL != WriterNode) {
            // Add volume node to writer.
            if (0 == VolumeContext->ulWriterId) {
                VolumeContext->ulWriterId = (ULONG)WriterNode->lWriterId;
            }
            AddVolumeToWriter(WriterNode, VolumeNode);
        }

        KeReleaseMutex(&m_Mutex, FALSE);
    }

    if (NULL != WriterNode) {
        DereferenceWriterNode(WriterNode);
    }

    return VolumeNode;
}

VOID
CDataFileWriterManager::AddVolumeToWriter(PWRITER_NODE WriterNode, PVOLUME_NODE VolumeNode)
{
    ASSERT(NULL != WriterNode);
    ASSERT(NULL != VolumeNode);

    NTSTATUS Status = KeWaitForSingleObject(&WriterNode->Mutex, Executive, KernelMode, FALSE, NULL);
    ASSERT(STATUS_SUCCESS == Status);

    InsertTailList(&WriterNode->VolumeList, &VolumeNode->ListEntry);
    ReferenceVolumeNode(VolumeNode);
    WriterNode->ulNumVolumes++;
    VolumeNode->WriterNode = ReferenceWriterNode(WriterNode);

    SetWriterNodeThreadPriority(WriterNode, true);
    KeReleaseMutex(&WriterNode->Mutex, FALSE);

    // Set Thread priority
    return;
}
void
CDataFileWriterManager::SetWriterNodeThreadPriority(PWRITER_NODE WriterNode, bool bMutexAcquired)
{
    ULONG   ulPriority = 0;

    if (!bMutexAcquired) {
        NTSTATUS Status = KeWaitForSingleObject(&WriterNode->Mutex, Executive, KernelMode, FALSE, NULL);
        ASSERT(STATUS_SUCCESS == Status);
    }

    PLIST_ENTRY ListEntry = WriterNode->VolumeList.Flink;
    while (ListEntry != &WriterNode->VolumeList) {
        PVOLUME_NODE    Node = (PVOLUME_NODE)ListEntry;
        ASSERT(NULL != Node->VolumeContext);

        if (ulPriority < Node->VolumeContext->FileWriterThreadPriority) {
            ulPriority = Node->VolumeContext->FileWriterThreadPriority;
        }
        ListEntry = ListEntry->Flink;
    }

    WriterNode->writer->SetPriority(ulPriority);

    if (!bMutexAcquired) {
        KeReleaseMutex(&WriterNode->Mutex, FALSE);
    }
}

void
CDataFileWriterManager::SetFileWriterThreadPriority(PVOLUME_NODE VolumeNode)
{
    ASSERT(NULL != VolumeNode);
    ASSERT(NULL != VolumeNode->WriterNode);

    if (NULL == VolumeNode) {
        InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, ("%s: VolumeNode is NULL\n", __FUNCTION__));
        return;
    }

    if (NULL == VolumeNode->WriterNode) {
        InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, ("%s: WriterNode is NULL. VolumeNode = %#p\n", __FUNCTION__, VolumeNode));
        return;
    }

    SetWriterNodeThreadPriority(VolumeNode->WriterNode, false);
    return;
}

void
CDataFileWriterManager::FlushAllWriterQueues()
{
    NTSTATUS        Status;
    PLIST_ENTRY     Temp = NULL;

    Status = KeWaitForSingleObject(&m_Mutex, Executive, KernelMode, FALSE, NULL);
    ASSERT(STATUS_SUCCESS == Status);
    for (Temp = m_WriterList.Flink; Temp != &m_WriterList; Temp = Temp->Flink) {
        PWRITER_NODE    WriterNode = (PWRITER_NODE)Temp;

        if (WriterNode->writer) {
            WriterNode->writer->FlushQueue();
        }
    }

    KeReleaseMutex(&m_Mutex, FALSE);
    return;
}
// This function is called with mutex acquired.
CDataFileWriterManager::PWRITER_NODE
CDataFileWriterManager::GetWriterNode(LONG lWriterId)
{
    KIRQL           OldIrql;
    PWRITER_NODE    Node = NULL;
    PLIST_ENTRY     Temp = NULL;

    if (0 == lWriterId) {
        if (NULL == m_WriterNodeUsedForLastVolume) {
            for (Temp = m_WriterList.Flink; Temp != &m_WriterList; Temp = Temp->Flink) {
                if (((PWRITER_NODE)Temp)->lWriterId & COMMON_WRITER_BIT) {
                    Node = (PWRITER_NODE) Temp;
                    m_WriterNodeUsedForLastVolume = ReferenceWriterNode(Node);
                    ReferenceWriterNode(Node);
                    break;
                }
            }
        } else {
            if (0x01 == m_ulNumberOfCommonFileWriters) {
                Node = ReferenceWriterNode(m_WriterNodeUsedForLastVolume);
            } else {
                for (Temp = m_WriterNodeUsedForLastVolume->ListEntry.Flink; 
                      Temp != &m_WriterNodeUsedForLastVolume->ListEntry; Temp = Temp->Flink)
                {
                    if (&m_WriterList == Temp) {
                        continue;
                    }

                    if (((PWRITER_NODE)Temp)->lWriterId & COMMON_WRITER_BIT) {
                        Node = (PWRITER_NODE) Temp;
                        DereferenceWriterNode(m_WriterNodeUsedForLastVolume);
                        m_WriterNodeUsedForLastVolume = ReferenceWriterNode(Node);
                        ReferenceWriterNode(Node);
                        break;
                    }
                }
            }
        }
    } else {
        for (Temp = m_WriterList.Flink; Temp != &m_WriterList; Temp = Temp->Flink) {
            if (lWriterId == ((PWRITER_NODE)Temp)->lWriterId) {
                Node = (PWRITER_NODE) Temp;
                ReferenceWriterNode(Node);
                break;
            }
        }
    }

    return Node;
}

CDataFileWriterManager::PWRITER_NODE
CDataFileWriterManager::ReferenceWriterNode(PWRITER_NODE WriterNode)
{
    ASSERT(NULL != WriterNode);
    ASSERT(0 != WriterNode->lRefCount);
    InterlockedIncrement(&WriterNode->lRefCount);

    return WriterNode;
}

VOID
CDataFileWriterManager::DereferenceWriterNode(PWRITER_NODE WriterNode)
{
    ASSERT(NULL != WriterNode);
    ASSERT(0 != WriterNode->lRefCount);

    if (0 == InterlockedDecrement(&WriterNode->lRefCount)) {
        DeallocateWriterNode(WriterNode);
    }

    return;
}

VOID
CDataFileWriterManager::DeallocateWriterNode(PWRITER_NODE WriterNode)
{
    ASSERT(NULL != WriterNode);
    ASSERT(0 == WriterNode->lRefCount);

    if (NULL != WriterNode) {
        // TODO: Free writer object?
    }

    ExFreePoolWithTag(WriterNode, TAG_WRITER_NODE);
}

CDataFileWriterManager::PWRITER_NODE
CDataFileWriterManager::AllocateWriterNode(LONG lWriterId)
{
    PWRITER_NODE    Node;

    ASSERT(0 != lWriterId);

    Node = (PWRITER_NODE) ExAllocatePoolWithTag(NonPagedPool, sizeof(WRITER_NODE), TAG_WRITER_NODE);
    if (NULL != Node) {
        RtlZeroMemory(Node, sizeof(WRITER_NODE));
        Node->lWriterId = lWriterId;
        Node->lRefCount = 0x01;
        InitializeListHead(&Node->VolumeList);
        KeInitializeMutex(&Node->Mutex, 0);
        Node->ulNumVolumes = 0;
        Node->writer = new CDataFileWriter(m_ulNumberOfThreadsPerFileWriter);
        if (NULL == Node->writer) {
            ExFreePoolWithTag(Node, TAG_WRITER_NODE);
            Node = NULL;
        }
    }

    return Node;
}

VOID
CDataFileWriterManager::InsertToWriterList(PWRITER_NODE WriterNode, bool bMutexAcquired)
{
    NTSTATUS Status;

    if (!bMutexAcquired) {
        Status = KeWaitForSingleObject(&m_Mutex, Executive, KernelMode, FALSE, NULL);
        ASSERT(STATUS_SUCCESS == Status);
    }

    ASSERT(0 == (WriterNode->ulFlags & m_ulFlagWriterInWriterList));
    if (0 == (WriterNode->ulFlags & m_ulFlagWriterInWriterList)) {
        ReferenceWriterNode(WriterNode);
        InsertTailList(&m_WriterList, &WriterNode->ListEntry);
    }

    if (!bMutexAcquired) {
        KeReleaseMutex(&m_Mutex, FALSE);
    }

    return;
}

VOID
CDataFileWriterManager::RemoveFromWriterList(PWRITER_NODE WriterNode, bool bMutexAcquired)
{
    NTSTATUS Status;

    if (!bMutexAcquired) {
        Status = KeWaitForSingleObject(&m_Mutex, Executive, KernelMode, FALSE, NULL);
        ASSERT(STATUS_SUCCESS == Status);
    }

    ASSERT(WriterNode->ulFlags & m_ulFlagWriterInWriterList);
    if (WriterNode->ulFlags & m_ulFlagWriterInWriterList) {
        RemoveEntryList(&WriterNode->ListEntry);
        WriterNode->ulFlags &= ~m_ulFlagWriterInWriterList;
        DereferenceWriterNode(WriterNode);
    }

    if (!bMutexAcquired) {
        KeReleaseMutex(&m_Mutex, FALSE);
    }
}

CDataFileWriterManager::PVOLUME_NODE
CDataFileWriterManager::AllocateVolumeNode(struct _VOLUME_CONTEXT *VolumeContext)
{
    PVOLUME_NODE    Node;

    Node = (PVOLUME_NODE)ExAllocatePoolWithTag(NonPagedPool, sizeof(VOLUME_NODE), TAG_VOLUME_NODE);
    if (NULL != Node) {
        RtlZeroMemory(Node, sizeof(VOLUME_NODE));
        Node->lRefCount = 0x01;
        Node->VolumeContext = ReferenceVolumeContext(VolumeContext);
    }

    return Node;
}

CDataFileWriterManager::PVOLUME_NODE
CDataFileWriterManager::ReferenceVolumeNode(PVOLUME_NODE Node)
{
    ASSERT(NULL != Node);
    ASSERT(0 != Node->lRefCount);

    InterlockedIncrement(&Node->lRefCount);

    return Node;
}

VOID
CDataFileWriterManager::DereferenceVolumeNode(PVOLUME_NODE Node)
{
    ASSERT(NULL != Node);
    ASSERT(0 != Node->lRefCount);

    if (0 == InterlockedDecrement(&Node->lRefCount)) {
        DeallocateVolumeNode(Node);
    }

    return;
}

VOID
CDataFileWriterManager::DeallocateVolumeNode(PVOLUME_NODE Node)
{
    ASSERT(NULL != Node);
    ASSERT(0 == Node->lRefCount);

    DereferenceVolumeContext(Node->VolumeContext);
    if (NULL != Node) {
        ExFreePoolWithTag(Node, TAG_VOLUME_NODE);
    }

    return;
}

//This function is called holding WriterManager mutex
VOID
CDataFileWriterManager::ReleaseWriterNode(PWRITER_NODE WriterNode)
{
    if(m_WriterNodeUsedForLastVolume == WriterNode)
    {
        DereferenceWriterNode(WriterNode);
        m_WriterNodeUsedForLastVolume = NULL;
    }
    
    RemoveFromWriterList(WriterNode, TRUE);
    WriterNode->writer->Release();
    DereferenceWriterNode(WriterNode);
}

VOID
CDataFileWriterManager::FreeVolumeWriter(PVOLUME_CONTEXT VolumeContext)
{
    CDataFileWriterManager::PVOLUME_NODE VolumeNode = VolumeContext->VolumeFileWriter;
    if (NULL == VolumeNode) {
        return;
    }

    CDataFileWriterManager::PWRITER_NODE WriterNode = VolumeNode->WriterNode;
    
    KIRQL OldIrql;
    KeAcquireSpinLock(&VolumeContext->Lock, &OldIrql);
    VolumeContext->VolumeFileWriter = NULL;
    KeReleaseSpinLock(&VolumeContext->Lock, OldIrql);

    WriterNode->writer->FlushQueue();

    RemoveEntryList(&VolumeNode->ListEntry);
    DereferenceVolumeNode(VolumeNode);      //Decrement reference count.
    WriterNode->ulNumVolumes--;
    
    DereferenceWriterNode(VolumeNode->WriterNode);
    DereferenceVolumeNode(VolumeNode);      //Remove volume Context Reference.
    
    SetWriterNodeThreadPriority(WriterNode, true);      //Recalculate Priority for remaining volumes

    NTSTATUS Status = KeWaitForSingleObject(&m_Mutex, Executive, KernelMode, FALSE, NULL);
    ASSERT(STATUS_SUCCESS == Status);
    if(WriterNode->ulNumVolumes == 0)
        ReleaseWriterNode(WriterNode);
    KeReleaseMutex(&m_Mutex, FALSE);
}
