#include "global.h"
#include "DataFileWriterMgr.h"
#include "VContext.h"

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

    m_WriterNodeUsedForLastDev = NULL;
}

CDataFileWriterManager::~CDataFileWriterManager()
{
    return;
}

NTSTATUS
CDataFileWriterManager::AddWorkItem(PDEV_NODE DevNode,  PCHANGE_NODE ChangeNode, PDEV_CONTEXT DevContext)
{
    ASSERT(NULL != DevNode);
    ASSERT(NULL != DevNode->WriterNode);
    ASSERT(NULL != DevNode->WriterNode->writer);

    if (NULL == DevNode) {
        InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("%s: InvalidParameter DevNode is NULL\n", __FUNCTION__));
        return STATUS_INVALID_PARAMETER;
    }

    if (NULL == DevNode->WriterNode) {
        InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, ("%s: InvalidParameter - WriteNode is NULL. DevNode = %#p\n", __FUNCTION__, DevNode));
        return STATUS_INVALID_PARAMETER;
    }

    if (NULL == DevNode->WriterNode->writer) {
        InVolDbgPrint(DL_ERROR, DM_DATA_FILTERING, 
                      ("%s: InvalidParameter writer is NULL. DevNode = %#p, WriterNode = %#p\n",
                        __FUNCTION__, DevNode, DevNode->WriterNode));
        return STATUS_INVALID_PARAMETER;
    }

    DevNode->WriterNode->writer->AddChangeNode(ChangeNode, DevContext);

    return STATUS_SUCCESS;
}

CDataFileWriterManager::PDEV_NODE
CDataFileWriterManager::NewDev(PDEV_CONTEXT DevContext)
{
    NTSTATUS        Status;
    PDEV_NODE    DevNode = NULL;
    PWRITER_NODE    WriterNode = NULL;

    DevNode = AllocateDevNode(DevContext);
    if (NULL != DevNode) {
        Status = KeWaitForSingleObject(&m_Mutex, Executive, KernelMode, FALSE, NULL);
        ASSERT(STATUS_SUCCESS == Status);

        WriterNode = GetWriterNode(DevContext->ulWriterId);
        if (NULL == WriterNode) {
            ASSERT(DevContext->ulWriterId);
            WriterNode = AllocateWriterNode(DevContext->ulWriterId);
            ASSERT(NULL != WriterNode);
            if (NULL != WriterNode) {
                InsertToWriterList(WriterNode, true);
            }
        }

        if (NULL != WriterNode) {
            // Add volume node to writer.
            if (0 == DevContext->ulWriterId) {
                DevContext->ulWriterId = (ULONG)WriterNode->lWriterId;
            }
            AddDevToWriter(WriterNode, DevNode);
        }

        KeReleaseMutex(&m_Mutex, FALSE);
    }

    if (NULL != WriterNode) {
        DereferenceWriterNode(WriterNode);
    }

    return DevNode;
}

VOID
CDataFileWriterManager::AddDevToWriter(PWRITER_NODE WriterNode, PDEV_NODE DevNode)
{
    ASSERT(NULL != WriterNode);
    ASSERT(NULL != DevNode);
#if !DBG
	KeWaitForSingleObject(&WriterNode->Mutex, Executive, KernelMode, FALSE, NULL);
#else
    NTSTATUS Status = KeWaitForSingleObject(&WriterNode->Mutex, Executive, KernelMode, FALSE, NULL);
    ASSERT(STATUS_SUCCESS == Status);
#endif
    InsertTailList(&WriterNode->DevList, &DevNode->ListEntry);
    ReferenceDevNode(DevNode);
    WriterNode->ulNumDevs++;
    DevNode->WriterNode = ReferenceWriterNode(WriterNode);

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
#if !DBG
		KeWaitForSingleObject(&WriterNode->Mutex, Executive, KernelMode, FALSE, NULL);
#else
        NTSTATUS Status = KeWaitForSingleObject(&WriterNode->Mutex, Executive, KernelMode, FALSE, NULL);
        ASSERT(STATUS_SUCCESS == Status);
#endif
    }

    PLIST_ENTRY ListEntry = WriterNode->DevList.Flink;
    while (ListEntry != &WriterNode->DevList) {
        PDEV_NODE    Node = (PDEV_NODE)ListEntry;
        ASSERT(NULL != Node->DevContext);

        if (ulPriority < Node->DevContext->FileWriterThreadPriority) {
            ulPriority = Node->DevContext->FileWriterThreadPriority;
        }
        ListEntry = ListEntry->Flink;
    }

    WriterNode->writer->SetPriority(ulPriority);

    if (!bMutexAcquired) {
        KeReleaseMutex(&WriterNode->Mutex, FALSE);
    }
}

void
CDataFileWriterManager::SetFileWriterThreadPriority(PDEV_NODE DevNode)
{
    ASSERT(NULL != DevNode);
    ASSERT(NULL != DevNode->WriterNode);

    if (NULL == DevNode) {
        InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, ("%s: DevNode is NULL\n", __FUNCTION__));
        return;
    }

    if (NULL == DevNode->WriterNode) {
        InVolDbgPrint(DL_INFO, DM_DATA_FILTERING, ("%s: WriterNode is NULL. DevNode = %#p\n", __FUNCTION__, DevNode));
        return;
    }

    SetWriterNodeThreadPriority(DevNode->WriterNode, false);
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
    PWRITER_NODE    Node = NULL;
    PLIST_ENTRY     Temp = NULL;

    if (0 == lWriterId) {
        if (NULL == m_WriterNodeUsedForLastDev) {
            for (Temp = m_WriterList.Flink; Temp != &m_WriterList; Temp = Temp->Flink) {
                if (((PWRITER_NODE)Temp)->lWriterId & COMMON_WRITER_BIT) {
                    Node = (PWRITER_NODE) Temp;
                    m_WriterNodeUsedForLastDev = ReferenceWriterNode(Node);
                    ReferenceWriterNode(Node);
                    break;
                }
            }
        } else {
            if (0x01 == m_ulNumberOfCommonFileWriters) {
                Node = ReferenceWriterNode(m_WriterNodeUsedForLastDev);
            } else {
                for (Temp = m_WriterNodeUsedForLastDev->ListEntry.Flink; 
                      Temp != &m_WriterNodeUsedForLastDev->ListEntry; Temp = Temp->Flink)
                {
                    if (&m_WriterList == Temp) {
                        continue;
                    }

                    if (((PWRITER_NODE)Temp)->lWriterId & COMMON_WRITER_BIT) {
                        Node = (PWRITER_NODE) Temp;
                        DereferenceWriterNode(m_WriterNodeUsedForLastDev);
                        m_WriterNodeUsedForLastDev = ReferenceWriterNode(Node);
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
        InitializeListHead(&Node->DevList);
        KeInitializeMutex(&Node->Mutex, 0);
        Node->ulNumDevs = 0;
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

CDataFileWriterManager::PDEV_NODE
CDataFileWriterManager::AllocateDevNode(struct _DEV_CONTEXT *DevContext)
{
    PDEV_NODE    Node;

    Node = (PDEV_NODE)ExAllocatePoolWithTag(NonPagedPool, sizeof(DEV_NODE), TAG_DEV_NODE);
    if (NULL != Node) {
        RtlZeroMemory(Node, sizeof(DEV_NODE));
        Node->lRefCount = 0x01;
        Node->DevContext = ReferenceDevContext(DevContext);
    }

    return Node;
}

CDataFileWriterManager::PDEV_NODE
CDataFileWriterManager::ReferenceDevNode(PDEV_NODE Node)
{
    ASSERT(NULL != Node);
    ASSERT(0 != Node->lRefCount);

    InterlockedIncrement(&Node->lRefCount);

    return Node;
}

VOID
CDataFileWriterManager::DereferenceDevNode(PDEV_NODE Node)
{
    ASSERT(NULL != Node);
    ASSERT(0 != Node->lRefCount);

    if (0 == InterlockedDecrement(&Node->lRefCount)) {
        DeallocateDevNode(Node);
    }

    return;
}

VOID
CDataFileWriterManager::DeallocateDevNode(PDEV_NODE Node)
{
    ASSERT(NULL != Node);
    ASSERT(0 == Node->lRefCount);

    DereferenceDevContext(Node->DevContext);
    if (NULL != Node) {
        ExFreePoolWithTag(Node, TAG_DEV_NODE);
    }

    return;
}

//This function is called holding WriterManager mutex
VOID
CDataFileWriterManager::ReleaseWriterNode(PWRITER_NODE WriterNode)
{
    if(m_WriterNodeUsedForLastDev == WriterNode)
    {
        DereferenceWriterNode(WriterNode);
        m_WriterNodeUsedForLastDev = NULL;
    }
    
    RemoveFromWriterList(WriterNode, TRUE);
    WriterNode->writer->Release();
    DereferenceWriterNode(WriterNode);
}

VOID
CDataFileWriterManager::FreeDevWriter(PDEV_CONTEXT DevContext)
{
    CDataFileWriterManager::PDEV_NODE DevNode = DevContext->DevFileWriter;
    if (NULL == DevNode) {
        return;
    }

    CDataFileWriterManager::PWRITER_NODE WriterNode = DevNode->WriterNode;
    
    KIRQL OldIrql;
    KeAcquireSpinLock(&DevContext->Lock, &OldIrql);
    DevContext->DevFileWriter = NULL;
    KeReleaseSpinLock(&DevContext->Lock, OldIrql);

    WriterNode->writer->FlushQueue();

    RemoveEntryList(&DevNode->ListEntry);
    DereferenceDevNode(DevNode);      //Decrement reference count.
    WriterNode->ulNumDevs--;
    
    DereferenceWriterNode(DevNode->WriterNode);
    DereferenceDevNode(DevNode);      //Remove volume Context Reference.
    
    SetWriterNodeThreadPriority(WriterNode, true);      //Recalculate Priority for remaining volumes
#if !DBG
	KeWaitForSingleObject(&m_Mutex, Executive, KernelMode, FALSE, NULL);
#else
    NTSTATUS Status = KeWaitForSingleObject(&m_Mutex, Executive, KernelMode, FALSE, NULL);
    ASSERT(STATUS_SUCCESS == Status);
#endif
    if(WriterNode->ulNumDevs == 0)
        ReleaseWriterNode(WriterNode);
    KeReleaseMutex(&m_Mutex, FALSE);
}
