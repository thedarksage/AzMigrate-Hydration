#pragma once

#include "BaseClass.h"
#include "DataFile.h"

struct _DEV_CONTEXT;

class CDataFileWriter : public BaseClass
{
public:
    CDataFileWriter(ULONG ulNumberOfThreads);
    ~CDataFileWriter();

    void StopAndWaitToComplete();
    NTSTATUS AddChangeNode(PCHANGE_NODE ChangeNode, _DEV_CONTEXT *DevContext);
    NTSTATUS SetPriority(ULONG ulPriority);
    ULONG   GetPriority() { return m_ulThreadPriority;}
    void    FlushQueue();
    static void ThreadFunction(PVOID FileWriterThread);
    static void InitializeLAL();
    static void CleanLAL();
    static NTSTATUS WriteChangeNodeToFile(CDataFile& File, PCHANGE_NODE ChangeNode);
    static NPAGED_LOOKASIDE_LIST    sm_WorkItemLAL;
private:
    LIST_ENTRY  m_WorkItemList;
    LIST_ENTRY  m_ThreadList;
    // m_Mutex protects m_ThreadList
    KMUTEX      m_Mutex;  
    KEVENT      m_ReadyEvent;
    bool        m_bTerminating;
    bool        m_bFlush;
    KSPIN_LOCK  m_Lock;
    ULONG       m_ulThreadPriority;
    ULONG       m_ulNumberOfThreads;
    typedef struct _WORK_ITEM {
        LIST_ENTRY          ListEntry;
        PCHANGE_NODE        ChangeNode;
        _DEV_CONTEXT        *DevContext;
    } WORK_ITEM, *PWORK_ITEM;

    typedef struct _FILE_WRITER_THREAD {
        LIST_ENTRY          ListEntry;
        PKTHREAD            ThreadObject;
        KEVENT              TerminateEvent;
        KEVENT              FlushEvent;
        KEVENT              FlushCompletedEvent;
        ULONG               ThreadPriority;
        class CDataFileWriter *parent;
    } FILE_WRITER_THREAD, *PFILE_WRITER_THREAD;

    void ProcessChangeNode(PCHANGE_NODE ChangeNode, _DEV_CONTEXT *DevContext);
    void AbortChangeNode(PCHANGE_NODE ChangeNode, _DEV_CONTEXT *DevContext);
    static void FlushQueue(PFILE_WRITER_THREAD FileWriterThread);

    void FreeWorkItem(PWORK_ITEM WorkItem);
    NTSTATUS CreateFileWriterThread(ULONG &ulThreadPriority);

    PFILE_WRITER_THREAD AllocateFileWriterThreadEntry();
    void DeallocateFileWriterThreadEntry(PFILE_WRITER_THREAD FileWriterThread);
};


