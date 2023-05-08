#ifndef INM__FSCLUSTER__INFO__H
#define INM__FSCLUSTER__INFO__H

#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include "svtypes.h"
#include "inmdefinesmajor.h"

class FsClusterInfo
{
    SV_UINT m_Size;
    SV_ULONGLONG m_Start;
    SV_UINT m_Count;
    const SV_BYTE *m_pBitmapBytes;
    char *m_pData;

public:
    FsClusterInfo():
    m_Size(0),
    m_Start(0),
    m_Count(0),
    m_pBitmapBytes(0),
    m_pData(0)
    {
    }

    FsClusterInfo(const SV_UINT Size, const SV_ULONGLONG Start, const SV_UINT Count,
                  const SV_BYTE *pBitmapBytes):
    m_Size(Size),
    m_Start(Start),
    m_Count(Count),
    m_pBitmapBytes(pBitmapBytes),
    m_pData(0)
    {
    }

    FsClusterInfo(char *data, const std::size_t &size);

    void FillClusterInfo(boost::shared_array <char> &buffer, 
                         SV_UINT &Size);
    ~FsClusterInfo();
    void Print(void);
    bool IsFirstUsedClusterFound(const SV_UINT startingcluster, const SV_UINT nclusters, SV_UINT &usedclusternumber);
    SV_UINT GetContinuousUsedClusters(const SV_UINT &startingcluster, const SV_UINT &nclusters);

    SV_UINT GetSize(void)
    {
        return m_Size;
    }

    SV_ULONGLONG GetStart(void)
    {
        return m_Start;
    }

    SV_UINT GetCount(void)
    {
        return m_Count;
    }

    const SV_BYTE *GetBitmapBytes(const SV_UINT &start)
    {
        return m_pBitmapBytes + start;
    }

private:
    void FillReceivedClusterInfo(const std::size_t &size);

};


typedef struct ClustersInfo
{
    SV_UINT m_start;
    SV_UINT m_count;

    ClustersInfo():
    m_start(0), 
    m_count(0)
    {
    }

    ClustersInfo(const SV_UINT &start, const SV_UINT &count):
    m_start(start),
    m_count(count)
    {
    }

    void set(const SV_UINT &start, const SV_UINT &count)
    {
        m_start = start;
        m_count = count;
    }

    void Print(void);

} ClustersInfo_t;


struct ClusterFile
{
    std::string fileName;
    boost::shared_ptr<FsClusterInfo> fsClusterInfo;

    void Print(void);
    ~ClusterFile();
};
typedef struct ClusterFile ClusterFile_t;
typedef boost::shared_ptr<ClusterFile_t> ClusterFilePtr_t;


class FsClusterProcessBatch
{
private:
    std::string m_FileName;
    SV_UINT m_Size;
    SV_ULONGLONG m_VolumeClusterNumber;
    SV_UINT m_TotalCount;
    ClustersInfo_t m_clustersInHcd;
    ClustersInfo_t m_clustersToProcess;
    SV_BYTE *m_pBitmapBytesToProcess;
    SV_UINT m_allocatedBitmapBytes;

public:
    FsClusterProcessBatch();
    bool Allocate(const SV_UINT &nbitmapbytes);
    void Release(void);
    ~FsClusterProcessBatch();
    void SetClusterInfo(ClusterFilePtr_t &pclusterfile);
    void SetClustersInHcd(const ClustersInfo_t &clustersinhcd);
    void SetBitmapToProcess(ClusterFilePtr_t &pclusterfile, const ClustersInfo_t &clusterstoprocess);
    void Print(void);
    SV_UINT GetSize(void);
    SV_UINT GetTotalCount(void);
    SV_UINT GetCountOfClustersToProcess(void);
    SV_UINT GetStartingClusterToProcess(void);
    SV_UINT GetCountOfClustersInHcd(void);
    SV_UINT GetStartingClusterInHcd(void);
    std::string GetFileName(void);
    SV_ULONGLONG GetVolumeClusterNumber(void);
    SV_UINT GetContinuousUsedClusters(const SV_UINT &startingcluster, const SV_UINT &nclusters);
    SV_UINT GetContinuousClusters(const SV_UINT &startingcluster, const SV_UINT &nclusters, bool &bisused);
    SV_UINT GetCountOfContinuousClusters(const SV_UINT &clusternumber,
                                         const SV_UINT &nclusters,
                                         const bool &bisused);
    bool IsAnyClusterUsed(void);
};

#endif /* INM__FSCLUSTER__INFO__H */
