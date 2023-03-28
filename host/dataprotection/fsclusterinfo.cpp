#include <cstdio>
#include <string>
#include <sstream>
#include <cstdlib>
#include "fsclusterinfo.h"
#include "inm_md5.h"
#include "dataprotectionexception.h"
#include "portablehelpers.h"
#include "inmsafecapis.h"

void FsClusterInfo::FillClusterInfo(boost::shared_array <char> &buffer, 
                                    SV_UINT &Size)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n",  __FUNCTION__ );
	std::stringstream ss ;
	unsigned char hash[DIGEST_LEN];

	INM_MD5_CTX ctx;

	ss << '\n' <<
		m_Size << '\n' <<
		m_Start << '\n' <<
		m_Count << '\n';

    SV_UINT nbytesforbitmap = (m_Count / NBITSINBYTE) + ((m_Count % NBITSINBYTE) ? 1 : 0);
	DebugPrintf(SV_LOG_DEBUG, "The cluster header is %s, cluster bitmap ptr = %p, number of cluster bytes = %d\n", ss.str().c_str(), 
		m_pBitmapBytes, nbytesforbitmap);

	INM_MD5Init(&ctx);
	INM_MD5Update(&ctx, (unsigned char*)ss.str().c_str(), ss.str().length() );

	if (m_pBitmapBytes && nbytesforbitmap)
	{
		INM_MD5Update(&ctx, (unsigned char*)m_pBitmapBytes, nbytesforbitmap);
	}

	INM_MD5Final(hash, &ctx);

	Size = DIGEST_LEN + ss.str().length() ;
	buffer.reset( new char[Size] ) ;
	
	

	inm_memcpy_s( buffer.get(), Size,hash, DIGEST_LEN ) ;
	inm_memcpy_s(buffer.get() + DIGEST_LEN, (Size - DIGEST_LEN), ss.str().c_str(), Size - DIGEST_LEN);

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n",  __FUNCTION__ );
}


FsClusterInfo::FsClusterInfo(char *data, const std::size_t &size):
m_pData(data)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n",  __FUNCTION__ );
    std::stringstream excmsg;

    /* TODO: validate even further ? */
    if (size > DIGEST_LEN)
    {
	    unsigned char hash[DIGEST_LEN];
        INM_MD5_CTX ctx;
        INM_MD5Init(&ctx);
        INM_MD5Update(&ctx, (unsigned char*)(m_pData + DIGEST_LEN), size - DIGEST_LEN);
        INM_MD5Final(hash, &ctx);
        if( memcmp(m_pData, hash, DIGEST_LEN ) != 0 )
        {
            excmsg << FUNCTION_NAME << ": cluster file is corrupted\n";
            throw CorruptResyncFileException(excmsg.str());
        }
        FillReceivedClusterInfo(size);
    }
    else
    {
        excmsg << FUNCTION_NAME << ": invalid cluster info\n";
        throw DataProtectionException(excmsg.str());
    }

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n",  __FUNCTION__ );
}


FsClusterInfo::~FsClusterInfo()
{
    if (m_pData)
    {
        free(m_pData);
        m_pData = 0;
    }
}


/* TODO: add verification of size later; */
void FsClusterInfo::FillReceivedClusterInfo(const std::size_t &size)
{
    const char *p = m_pData + DIGEST_LEN;
   
    std::string tmp;
    p++;
    for ( /* empty */ ; *p != '\n'; p++)
    {
        tmp.push_back(*p);
    }
    std::stringstream sssize(tmp);
    sssize >> m_Size;

    tmp.clear();
    p++;
    for ( /* empty */ ; *p != '\n'; p++)
    {
        tmp.push_back(*p);
    }
    std::stringstream ssstart(tmp);
    ssstart >> m_Start;
     
    tmp.clear();
    p++;
    for ( /* empty */ ; *p != '\n'; p++)
    {
        tmp.push_back(*p);
    }
    std::stringstream sscount(tmp);
    sscount >> m_Count;

    p++;
    m_pBitmapBytes = (const SV_BYTE *)p;
}


void FsClusterInfo::Print(void)
{
    DebugPrintf(SV_LOG_DEBUG, " ===== fs cluster info: start =====\n");
    DebugPrintf(SV_LOG_DEBUG, "size is %u\n", m_Size);
    DebugPrintf(SV_LOG_DEBUG, "start is " ULLSPEC "\n", m_Start);
    DebugPrintf(SV_LOG_DEBUG, "count is %u\n", m_Count);
    DebugPrintf(SV_LOG_DEBUG, " ===== fs cluster info: end =====\n");
}


bool FsClusterInfo::IsFirstUsedClusterFound(const SV_UINT startingcluster, const SV_UINT nclusters, SV_UINT &usedclusternumber)
{
    bool bfoundused = false;

    SV_UINT c = startingcluster;
    for (SV_UINT i = 0; i < nclusters; i++, c++)
    {
        if (m_pBitmapBytes[c / NBITSINBYTE] & (1 << (c % NBITSINBYTE)))
        {
            usedclusternumber = c;
            bfoundused = true;
            break;
        }
    } 

    return bfoundused;
}


SV_UINT FsClusterInfo::GetContinuousUsedClusters(const SV_UINT &startingcluster, const SV_UINT &nclusters)
{
    SV_UINT count = 0;

    SV_UINT c = startingcluster;
    for (SV_UINT i = 0; i < nclusters; i++, c++)
    {
        if (m_pBitmapBytes[c / NBITSINBYTE] & (1 << (c % NBITSINBYTE)))
        {
            count++;
        }
        else
        {
            break;
        }
    } 

    return count;
}


ClusterFile::~ClusterFile()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}


FsClusterProcessBatch::FsClusterProcessBatch()
:m_Size(0), 
m_VolumeClusterNumber(0),
m_TotalCount(0),
m_pBitmapBytesToProcess(0),
m_allocatedBitmapBytes(0)
{
}


bool FsClusterProcessBatch::Allocate(const SV_UINT &nbitmapbytes)
{
    Release();
    m_pBitmapBytesToProcess = new (std::nothrow) SV_BYTE [nbitmapbytes];
    if (m_pBitmapBytesToProcess)
    {
        m_allocatedBitmapBytes = nbitmapbytes;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "could not allocate %u bytes for cluster bitmap batch\n", nbitmapbytes);
    }

    return m_pBitmapBytesToProcess;
}


FsClusterProcessBatch::~FsClusterProcessBatch()
{
    Release();
    m_allocatedBitmapBytes = 0;
}


void FsClusterProcessBatch::Release(void)
{
    if (m_pBitmapBytesToProcess)
    {
        delete [] m_pBitmapBytesToProcess;
        m_pBitmapBytesToProcess = 0;
    }
}


void FsClusterProcessBatch::SetClusterInfo(ClusterFilePtr_t &pclusterfile)
{
    m_FileName = pclusterfile->fileName;
    m_Size = pclusterfile->fsClusterInfo->GetSize();
    m_VolumeClusterNumber = pclusterfile->fsClusterInfo->GetStart();
    m_TotalCount = pclusterfile->fsClusterInfo->GetCount();
}


void FsClusterProcessBatch::SetClustersInHcd(const ClustersInfo_t &clustersinhcd)
{
    m_clustersInHcd = clustersinhcd;
}
  

void FsClusterProcessBatch::SetBitmapToProcess(ClusterFilePtr_t &pclusterfile, const ClustersInfo_t &clusterstoprocess)
{
    m_clustersToProcess = clusterstoprocess;
    SV_UINT bytestart = m_clustersToProcess.m_start / NBITSINBYTE;
    const SV_BYTE *p = pclusterfile->fsClusterInfo->GetBitmapBytes(bytestart);
    SV_UINT bytecount = (m_clustersToProcess.m_count / NBITSINBYTE) + ((m_clustersToProcess.m_count % NBITSINBYTE) ? 1 : 0);

    /*
    DebugPrintf(SV_LOG_DEBUG, "address of bitmap is: %p, bytestart = %u, bytecount = %u,"
                              " for cluster start: %u, count: %u\n", p, bytestart, bytecount,
                              clusterstoprocess.m_start, clusterstoprocess.m_count);
    */

	inm_memcpy_s(m_pBitmapBytesToProcess, m_allocatedBitmapBytes,p, bytecount);
}


bool FsClusterProcessBatch::IsAnyClusterUsed(void)
{
    bool bisused = false;
   
    /* TODO: use a better (fast) algorithm that does 
     * this check : finding if any element is non zero
     * in an array */
    for (SV_UINT i = 0; i < m_clustersToProcess.m_count; i++)
    {
        if (m_pBitmapBytesToProcess[i / NBITSINBYTE] & (1 << (i % NBITSINBYTE)))
        {
            bisused = true;
            break;
        }
    }

    return bisused;
}


void FsClusterProcessBatch::Print(void)
{
    DebugPrintf(SV_LOG_DEBUG, " ===== fs cluster info batch: start =====\n");
    DebugPrintf(SV_LOG_DEBUG, "cluster file name: %s\n", m_FileName.c_str());
    DebugPrintf(SV_LOG_DEBUG, "cluster size: %u\n", m_Size);
    DebugPrintf(SV_LOG_DEBUG, "volume cluster number: " ULLSPEC "\n", m_VolumeClusterNumber);
    DebugPrintf(SV_LOG_DEBUG, "total count: %u\n", m_TotalCount);
    DebugPrintf(SV_LOG_DEBUG, "clusters in hcd:\n");
    m_clustersInHcd.Print();
    DebugPrintf(SV_LOG_DEBUG, "clusters to process:\n");
    m_clustersToProcess.Print();
    DebugPrintf(SV_LOG_DEBUG, "bitmap bytes address: %p\n", m_pBitmapBytesToProcess);
    DebugPrintf(SV_LOG_DEBUG, "allocated length: %u\n", m_allocatedBitmapBytes);
    DebugPrintf(SV_LOG_DEBUG, " ===== fs cluster info batch: end =====\n");
}


SV_UINT FsClusterProcessBatch::GetSize(void)
{
    return m_Size;
}


SV_UINT FsClusterProcessBatch::GetTotalCount(void)
{
    return m_TotalCount;
}


SV_UINT FsClusterProcessBatch::GetCountOfClustersToProcess(void)
{
    return m_clustersToProcess.m_count;
}


SV_UINT FsClusterProcessBatch::GetStartingClusterToProcess(void)
{
    return m_clustersToProcess.m_start;
}


SV_UINT FsClusterProcessBatch::GetCountOfClustersInHcd(void)
{
    return m_clustersInHcd.m_count;
}


SV_UINT FsClusterProcessBatch::GetStartingClusterInHcd(void)
{
    return m_clustersInHcd.m_start;
}


std::string FsClusterProcessBatch::GetFileName(void)
{
    return m_FileName;
}


SV_ULONGLONG FsClusterProcessBatch::GetVolumeClusterNumber(void)
{
    return m_VolumeClusterNumber;
}


SV_UINT FsClusterProcessBatch::GetContinuousClusters(const SV_UINT &startingcluster, const SV_UINT &nclusters, bool &bisused)
{
    SV_UINT c = startingcluster - GetStartingClusterToProcess();
    bisused = m_pBitmapBytesToProcess[c / NBITSINBYTE] & (1 << (c % NBITSINBYTE));
    return GetCountOfContinuousClusters(c, nclusters, bisused);
}


SV_UINT FsClusterProcessBatch::GetCountOfContinuousClusters(const SV_UINT &clusternumber,
                                                            const SV_UINT &nclusters,
                                                            const bool &bisused)
{
    SV_UINT c = clusternumber;
    SV_UINT count = 0;

    if (bisused)
    {
        for (SV_UINT i = 0; i < nclusters; i++, c++)
        {
            if (m_pBitmapBytesToProcess[c / NBITSINBYTE] & (1 << (c % NBITSINBYTE)))
            {
                count++;
            }
            else
            {
                break;
            }
        }
    }
    else
    {
        for (SV_UINT i = 0; i < nclusters; i++, c++)
        {
            if (m_pBitmapBytesToProcess[c / NBITSINBYTE] & (1 << (c % NBITSINBYTE)))
            {
                break;
            }
            else
            {
                count++;
            }
        }
    }

    return count;
}


SV_UINT FsClusterProcessBatch::GetContinuousUsedClusters(const SV_UINT &startingcluster, const SV_UINT &nclusters)
{
    SV_UINT count = 0;

    SV_UINT c = startingcluster - GetStartingClusterToProcess();
    /* DebugPrintf(SV_LOG_DEBUG, "asking starting cluster %u, upto count %u, actual start %u\n", startingcluster, nclusters, c); */
    for (SV_UINT i = 0; i < nclusters; i++, c++)
    {
        if (m_pBitmapBytesToProcess[c / NBITSINBYTE] & (1 << (c % NBITSINBYTE)))
        {
            count++;
        }
        else
        {
            break;
        }
    } 

    return count;
}

