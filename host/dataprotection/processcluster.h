#ifndef PROCESS_CLUSTER_H
#define PROCESS_CLUSTER_H

#include "fsclusterinfo.h"
#include "svtypes.h"
#include "readinfo.h"

struct ClusterInfoForHcd
{
    FsClusterProcessBatch *m_pClusterBatch;   /* cluster batch to process */
    ReadInfo m_ReadInfo;                      /* read info */

    ClusterInfoForHcd():
    m_pClusterBatch(0)
    {
    }

    bool Init(const SV_UINT &readlen, const SV_UINT &alignment);
    bool AllocateClusterBatch(const SV_UINT &readbuffersize, const SV_UINT &clustersize);
    void Print(void);
    void ReleaseClusterBatch(void);
    ~ClusterInfoForHcd();
};


struct ClusterInfosForHcd
{
    ClusterInfoForHcd *m_pClusterInfos;
    unsigned int m_NumClusterInfos;
    unsigned int m_FreeIndex;
    bool m_bAllocated;

    ClusterInfosForHcd()
    :m_pClusterInfos(0),
     m_NumClusterInfos(0),
     m_FreeIndex(0),
     m_bAllocated(false)
    {
    }

    void Release(void);
    bool Init(const unsigned int &nclusterinfos, const SV_UINT &readlen, const SV_UINT &alignment);
    ~ClusterInfosForHcd();
};

#endif /* PROCESS_CLUSTER_H */
