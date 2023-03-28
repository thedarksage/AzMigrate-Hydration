#ifndef _INVOLFLT_DATA_FILE_WRITER_MANAGER_H_
#define _INVOLFLT_DATA_FILE_WRITER_MANAGER_H_
#include "BaseClass.h"
#include "Registry.h"
#include "DataFileWriter.h"

class CDataFileWriterManager : public BaseClass
{
public:
    static const ULONG m_ulFlagWriterInWriterList   = 0x0001;

    typedef struct _WRITER_NODE {
        LIST_ENTRY  ListEntry;
        LIST_ENTRY  VolumeList;
        KMUTEX      Mutex;
        ULONG       ulFlags;
        tInterlockedLong   lRefCount;
        LONG        lWriterId;
        ULONG       ulNumVolumes;
        CDataFileWriter *writer;
    } WRITER_NODE, *PWRITER_NODE;

    typedef struct _VOLUME_NODE {
        LIST_ENTRY      ListEntry;
        LONG            lRefCount;
        PWRITER_NODE    WriterNode;
        struct _VOLUME_CONTEXT  *VolumeContext;
    } VOLUME_NODE, *PVOLUME_NODE;

    CDataFileWriterManager(ULONG ulNumThreadsPerFileWriter, ULONG ulNumCommonFileWriters);
    ~CDataFileWriterManager();

    PVOLUME_NODE NewVolume(struct _VOLUME_CONTEXT *VolumeContext);
    NTSTATUS AddWorkItem(PVOLUME_NODE VolumeNode,  PCHANGE_NODE ChangeNode, struct _VOLUME_CONTEXT *VolumeContext);
    void    SetFileWriterThreadPriority(PVOLUME_NODE VolumeNode);
    void    FlushAllWriterQueues();
    VOID    FreeVolumeWriter(struct _VOLUME_CONTEXT *VolumeContext);

private:
    PWRITER_NODE    GetWriterNodeForNewVolume(LONG lWriterId);
    PWRITER_NODE    GetWriterNode(LONG lWriterId);
    PWRITER_NODE    AllocateWriterNode(LONG lWriterId);
    PWRITER_NODE    ReferenceWriterNode(PWRITER_NODE WriterNode);
    void            SetWriterNodeThreadPriority(PWRITER_NODE WriterNode, bool bMutexAcquired);
    VOID            DereferenceWriterNode(PWRITER_NODE WriterNode);
    VOID            DeallocateWriterNode(PWRITER_NODE WriterNode);
    VOID            InsertToWriterList(PWRITER_NODE WriterNode, bool bMutexAcquired);
    VOID            RemoveFromWriterList(PWRITER_NODE WriterNode, bool bMutexAcquired);
    VOID            AddVolumeToWriter(PWRITER_NODE WriterNode, PVOLUME_NODE VolumeNode);
    PVOLUME_NODE    AllocateVolumeNode(struct _VOLUME_CONTEXT *VolumeContext);
    PVOLUME_NODE    ReferenceVolumeNode(PVOLUME_NODE VolumeNode);
    VOID            DereferenceVolumeNode(PVOLUME_NODE VolumeNode);
    VOID            DeallocateVolumeNode(PVOLUME_NODE VolumeNode);
    VOID            ReleaseWriterNode(PWRITER_NODE WriterNode);
private:
    ULONG           m_ulNumberOfThreadsPerFileWriter;
    ULONG           m_ulNumberOfCommonFileWriters;
    LONG            m_lNextWriterId;

    // This lock protects access to m_WriterNodeOpenForVolumes and m_WriterList
    KMUTEX          m_Mutex;  
    LIST_ENTRY      m_WriterList;
    PWRITER_NODE    m_WriterNodeUsedForLastVolume;
};

#endif // _INVOLFLT_DATA_FILE_WRITER_MANAGER_H_