#pragma once
#include "BaseClass.h"
#include "Registry.h"
#include "DataFileWriter.h"

class CDataFileWriterManager : public BaseClass
{
public:
    static const ULONG m_ulFlagWriterInWriterList   = 0x0001;

    typedef struct _WRITER_NODE {
        LIST_ENTRY  ListEntry;
        LIST_ENTRY  DevList;
        KMUTEX      Mutex;
        ULONG       ulFlags;
        tInterlockedLong   lRefCount;
        LONG        lWriterId;
        ULONG       ulNumDevs;
        CDataFileWriter *writer;
    } WRITER_NODE, *PWRITER_NODE;

    typedef struct _DEV_NODE {
        LIST_ENTRY      ListEntry;
        LONG            lRefCount;
        PWRITER_NODE    WriterNode;
        struct _DEV_CONTEXT  *DevContext;
    } DEV_NODE, *PDEV_NODE;

    CDataFileWriterManager(ULONG ulNumThreadsPerFileWriter, ULONG ulNumCommonFileWriters);
    ~CDataFileWriterManager();

    PDEV_NODE NewDev(struct _DEV_CONTEXT *DevContext);
    NTSTATUS AddWorkItem(PDEV_NODE DevNode,  PCHANGE_NODE ChangeNode, struct _DEV_CONTEXT *DevContext);
    void    SetFileWriterThreadPriority(PDEV_NODE DevNode);
    void    FlushAllWriterQueues();
    VOID    FreeDevWriter(struct _DEV_CONTEXT *DevContext);

private:
    PWRITER_NODE    GetWriterNodeForNewDev(LONG lWriterId);
    PWRITER_NODE    GetWriterNode(LONG lWriterId);
    PWRITER_NODE    AllocateWriterNode(LONG lWriterId);
    PWRITER_NODE    ReferenceWriterNode(PWRITER_NODE WriterNode);
    void            SetWriterNodeThreadPriority(PWRITER_NODE WriterNode, bool bMutexAcquired);
    VOID            DereferenceWriterNode(PWRITER_NODE WriterNode);
    VOID            DeallocateWriterNode(PWRITER_NODE WriterNode);
    VOID            InsertToWriterList(PWRITER_NODE WriterNode, bool bMutexAcquired);
    VOID            RemoveFromWriterList(PWRITER_NODE WriterNode, bool bMutexAcquired);
    VOID            AddDevToWriter(PWRITER_NODE WriterNode, PDEV_NODE DevNode);
    PDEV_NODE       AllocateDevNode(struct _DEV_CONTEXT *DevContext);
    PDEV_NODE       ReferenceDevNode(PDEV_NODE DevNode);
    VOID            DereferenceDevNode(PDEV_NODE DevNode);
    VOID            DeallocateDevNode(PDEV_NODE DevNode);
    VOID            ReleaseWriterNode(PWRITER_NODE WriterNode);
private:
    ULONG           m_ulNumberOfThreadsPerFileWriter;
    ULONG           m_ulNumberOfCommonFileWriters;
    LONG            m_lNextWriterId;

    // This lock protects access to m_WriterNodeOpenForDev and m_WriterList
    KMUTEX          m_Mutex;  
    LIST_ENTRY      m_WriterList;
    PWRITER_NODE    m_WriterNodeUsedForLastDev;
};

