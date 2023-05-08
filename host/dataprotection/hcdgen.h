#ifndef GENERATE_HCD_H
#define GENERATE_HCD_H

#include "processcluster.h"
#include "inmdefines.h"

struct ClusterFileInfo
{
    std::string m_fileName; 
    SV_UINT m_size;           /* size */
    SV_UINT m_count;          /* only count, start is implicitly 0 */
    SV_ULONGLONG m_volumeclusternumber;

    ClusterFileInfo():
    m_size(0),
    m_count(0),
    m_volumeclusternumber(0)
    {
    }

    void Print(void);
};

struct HcdNodes
{
    char *m_pHcdData;
    unsigned int m_allocatedLength;
    unsigned int m_hcdDataLength;
    SV_ULONG m_UsedBytes;
    SV_ULONG m_UnUsedBytes;

    HcdNodes()
    {
        m_pHcdData = 0;
        m_allocatedLength = 0;
        Reset();
    }

    void Reset(void)
    {
        m_hcdDataLength = 0;
        m_UsedBytes = 0;
        m_UnUsedBytes = 0;
    }

    void Print(void);
};
 
struct HcdsToSend
{
    SV_UINT m_NumHcdBuffers;
    HcdNodes *m_pHcdNodes;

    HcdsToSend():
    m_NumHcdBuffers(0),
    m_pHcdNodes(0)
    {
    }
     
    bool Init(const unsigned int &ngenhcdtasks);
    void Release(void);
    ~HcdsToSend();
    void Print(void);
};

/* information exchanged between generate hcd and send thread */
struct HcdInfoToSend
{
    ClusterFileInfo *m_pClusterFileInfo;  /* has cluster file name and count of clusters; start is implicitly 0 */
    ClustersInfo_t m_clustersInHcd;       /* clusters to be processed in hcd; just make a copy from whatever received from read thread */
    ClustersInfo_t m_clustersToProcess;   /* clusters to be processed in single read; just make a copy from whatever received from read thread */
    HcdsToSend m_HcdsToSend;              /* hcd data */
    DataAndLength m_HcdBuffer;            /* hcd buffer that rotates between generate and send thread */

    HcdInfoToSend():
    m_pClusterFileInfo(0)
    {
    }

    bool AllocateHcdBuffer(const SV_UINT &readbuffersize, const SV_UINT &clustersize);
    bool Init(const unsigned int &ngenhcdtasks);
    ~HcdInfoToSend();
    void ReleaseHcdBuffer(void);
    void GiveBufferToHcdNodes(const SV_UINT &nhcdsinworstcase);
    void ReleaseClusterInfo(void);
    void Print(void);
};


struct HcdInfosToSend
{
    HcdInfoToSend *m_pHcdInfos;
    unsigned int m_NumHcdInfos;
    unsigned int m_FreeIndex;
    bool m_bAllocated;

    HcdInfosToSend()
    :m_pHcdInfos(0),
     m_NumHcdInfos(0),
     m_FreeIndex(0),
     m_bAllocated(false)
    {
    }

    void Release(void);
    bool Init(const unsigned int &nhcdinfos, const unsigned int &ngenhcdtasks);
    ~HcdInfosToSend();
};


/* information exchanged between primary and worker checksum generate threads */
struct HcdGenInfo
{
    FsClusterProcessBatch *m_pClusterBatch;
    ClustersInfo_t m_clustersToProcess;   /* this is clusters in read / number of hcd generators */
    DataAndLength m_volumeData;           /* volume data for above clusters */
    HcdNodes *m_pHcdNodes;

    HcdGenInfo():
    m_pClusterBatch(0),
    m_pHcdNodes(0)
    {
    }

    void Print(void);
};

#endif /* GENERATE_HCD_H */
