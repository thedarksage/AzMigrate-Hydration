#include "volumeclusterinfoimp.h"
#include "inmstrcmp.h"
#include "ntfsclusterinfo.h"
#include "logger.h"
#include "portablehelpers.h"

bool VolumeClusterInformerImp::InitClusterInfoIf(const VolumeDetails &vd)
{
    bool brval = true;

    if (0 == InmStrCmp<NoCaseCharCmp>(vd.m_FileSystem, NTFS))
    {
        m_pVciIf = new (std::nothrow) NtfsClusterInfo(vd);
        brval = m_pVciIf;
        if (false == brval)
        {
            DebugPrintf(SV_LOG_ERROR, "could not allocate memory for no file system cluster info\n");
        }
    }

    return brval;
}
