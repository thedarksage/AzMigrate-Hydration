#include "cdptargetapply.h"
#include "groupinfo.h"
#include "genericprofiler.h"

CDPTargetApply::CDPTargetApply(const std::string & volumename,
                               const SV_ULONGLONG & source_capacity,
                               const CDP_SETTINGS & settings,
                               bool bdiffsync)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    m_Cdpwriter.reset(new CDPV2Writer(volumename,source_capacity,settings,bdiffsync));
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

bool CDPTargetApply::Applychanges(const std::string & filename,
                                 long long& totalBytesApplied,
                                 char* source,
                                 const SV_ULONG sourceLength,
                                 DifferentialSync * diffSyncObj)
{ 

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool bRet = false;
    GroupInfo::VolumeInfos_t::iterator iter(diffSyncObj->GetTargets().begin());
    GroupInfo::VolumeInfos_t::iterator end(diffSyncObj->GetTargets().end());

    for (/* empty */; iter != end; ++iter)
    {
        if(!m_Cdpwriter ->Applychanges(filename,totalBytesApplied, source, sourceLength, 0, 0,1 ))
        {
            DebugPrintf(SV_LOG_DEBUG,"Failed to apply the changes\n");
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG,"Successfully applied the changes\n");
            bRet = true;
        }
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return bRet;
}
