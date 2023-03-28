#include <string>
#include <sstream>
#include "volumeclusterinfo.h"
#include "logger.h"
#include "portablehelpers.h"


void VolumeClusterInformer::PrintVolumeClusterInfo(const VolumeClusterInfo &vci)
{
    std::stringstream ss;
    DebugPrintf(SV_LOG_DEBUG, "volume cluster info:\n");
    ss << "start = " << vci.m_Start
       << ", count requested = " << vci.m_CountRequested
       << ", count filled = " << vci.m_CountFilled;
	DebugPrintf(SV_LOG_DEBUG, "%s, bitmap address = %p\n", ss.str().c_str(), vci.m_pBitmap);
   
    std::stringstream ssbmp;
    for (SV_UINT i = 0; i < vci.m_CountFilled; i++)
    {
        ssbmp << ((vci.m_pBitmap[i / NBITSINBYTE] & (1 << (i % NBITSINBYTE))) ? 1 : 0);
    } 
    DebugPrintf(SV_LOG_DEBUG, "cluster bitmap:\n");
    DebugPrintf(SV_LOG_DEBUG, "%s\n", ssbmp.str().c_str());
}


