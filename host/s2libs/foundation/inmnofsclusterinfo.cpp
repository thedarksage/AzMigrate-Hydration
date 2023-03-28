#include <sstream>
#include <cstdlib>
#include "logger.h"
#include "portablehelpers.h"
#include "inmnofsclusterinfo.h"
#include "inmsafeint.h"
#include "inmageex.h"


InmNoFsClusterInfo::InmNoFsClusterInfo():
m_Size(0), 
m_clusterSize(0)
{
}


InmNoFsClusterInfo::InmNoFsClusterInfo(const SV_LONGLONG size, const SV_LONGLONG startoffset, const SV_UINT minimumclustersizepossible):
m_Size(size),
m_clusterSize(0),
m_MinClusterSizePossible(minimumclustersizepossible)
{
    /* TODO: this should be a good try or configurable ? */
    INM_SAFE_ARITHMETIC(m_MaxClusterSizeToTry = 8 * InmSafeInt<SV_UINT>::Type(m_MinClusterSizePossible), INMAGE_EX(m_MinClusterSizePossible))
    SV_UINT adjustedmaxclustersize = startoffset ? GetAdjustedMaxClusterSize(m_MaxClusterSizeToTry, startoffset) : m_MaxClusterSizeToTry;
    DetermineClusterSize(adjustedmaxclustersize);
}


SV_UINT InmNoFsClusterInfo::GetAdjustedMaxClusterSize(const SV_UINT &candidateclustersize, const SV_LONGLONG startoffset)
{
    SV_UINT c = 0;

    if (candidateclustersize >= m_MinClusterSizePossible)
    {
        SV_LONGLONG rem;
        INM_SAFE_ARITHMETIC(rem = InmSafeInt<SV_LONGLONG>::Type(startoffset)%candidateclustersize, INMAGE_EX(startoffset)(candidateclustersize))
        SV_UINT halfcandidateclustersize;
        INM_SAFE_ARITHMETIC(halfcandidateclustersize = InmSafeInt<SV_UINT>::Type(candidateclustersize)/2, INMAGE_EX(candidateclustersize))
        c = rem ? GetAdjustedMaxClusterSize(halfcandidateclustersize, startoffset) : candidateclustersize;
    }
    else
    {
        /* TODO: throw some very big ERROR ? Atleast record 
         * in logs. But this should never occur as start offset
         * has to be multiple of atleast 512 */
        DebugPrintf(SV_LOG_ERROR, "For nofs cluster info, could not calculate cluster size as "
                                  "start offset " LLSPEC " is not multiple from "
                                  "max %u to min %u candidate cluster sizes\n", 
                                  startoffset, m_MaxClusterSizeToTry, m_MinClusterSizePossible);
    }

    return c;
}


SV_ULONGLONG InmNoFsClusterInfo::GetNumberOfClusters(void)
{
    SV_ULONGLONG nclusters = 0;

    if (m_clusterSize)
    {
        INM_SAFE_ARITHMETIC(nclusters = InmSafeInt<SV_LONGLONG>::Type(m_Size) / m_clusterSize, INMAGE_EX(m_Size)(m_clusterSize))
    }
    return nclusters; 
}


bool InmNoFsClusterInfo::GetVolumeClusterInfo(VolumeClusterInformer::VolumeClusterInfo &vci)
{
    SV_UINT rem;
    INM_SAFE_ARITHMETIC(rem = InmSafeInt<SV_UINT>::Type(vci.m_CountRequested) % NBITSINBYTE, INMAGE_EX(vci.m_CountRequested))
    SV_UINT add;
    add = rem ? 1 : 0;
	size_t nbytesforbitmap;
    INM_SAFE_ARITHMETIC(nbytesforbitmap = (InmSafeInt<SV_UINT>::Type(vci.m_CountRequested) / NBITSINBYTE) + add, INMAGE_EX(vci.m_CountRequested)(add))
    bool bgotinfo = AllocateBitmapIfReq(nbytesforbitmap, vci);

    if (bgotinfo)
    {
		SV_BYTE *p = vci.GetAllocatedData();
        memset(p, 0XFF, vci.GetAllocatedLength());
        vci.m_CountFilled = vci.m_CountRequested;
        vci.m_pBitmap = p;
    }
     
    return bgotinfo;
}


void InmNoFsClusterInfo::DetermineClusterSize(const SV_UINT candidateclustersize)
{
    if (candidateclustersize >= m_MinClusterSizePossible)
    {
        SV_LONGLONG rem;
        INM_SAFE_ARITHMETIC(rem = InmSafeInt<SV_LONGLONG>::Type(m_Size) % candidateclustersize, INMAGE_EX(m_Size)(candidateclustersize))
        if (0 == rem)
        {
            m_clusterSize = candidateclustersize;
        }
        else
        {
            SV_UINT halfcandidateclustersize;
            INM_SAFE_ARITHMETIC(halfcandidateclustersize = InmSafeInt<SV_UINT>::Type(candidateclustersize)/2, INMAGE_EX(candidateclustersize))
            DetermineClusterSize(halfcandidateclustersize);
        }
    }
}


bool InmNoFsClusterInfo::AllocateBitmapIfReq(const size_t &outsize, VolumeClusterInformer::VolumeClusterInfo &vci)
{
	bool brval = false;

	std::stringstream sstmp;
	size_t bitmapdatasize = vci.GetAllocatedLength();
	sstmp << "allocated bitmap data size  = " << bitmapdatasize
          << ", outsize = " << outsize;
	DebugPrintf(SV_LOG_DEBUG, "%s\n", sstmp.str().c_str());
    if (outsize)
    {
        if (outsize > bitmapdatasize)
        {
			vci.Release();
			brval = vci.Allocate(outsize);
			if (!brval)
			{
			    std::stringstream ss;
                ss << outsize;
                DebugPrintf(SV_LOG_ERROR, "could not allocate %s for cluster bitmap\n", ss.str().c_str());
			}
        }
        else
        {
            brval = true;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "size requested is zero for volume bitmap buffer\n");
    }

    return brval;
}


InmNoFsClusterInfo::~InmNoFsClusterInfo()
{
}
