#include "cdpresyncapply.h"
#include "genericprofiler.h"

CDPResyncApply::CDPResyncApply(const std::string & volumename,
                               const SV_ULONGLONG & source_capacity,
                               const CDP_SETTINGS & settings,
                               bool bdiffsync,
                               cdp_resync_txn * cdp_resynctxn_mgr)
    :m_cdpresynctxn_ptr(cdp_resynctxn_mgr)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    m_Cdpwriter.reset(new CDPV2Writer(volumename,source_capacity,settings,bdiffsync,cdp_resynctxn_mgr));
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

bool CDPResyncApply::Applychanges(const std::string & filename,
                                  long long& totalBytesApplied,
                                  char* source,
                                  const SV_ULONG sourceLength,
                                  DifferentialSync * diffSync)
{ 

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool bRet = true;
    if(!m_Cdpwriter ->Applychanges(filename,totalBytesApplied, source, sourceLength))
    {
        DebugPrintf(SV_LOG_DEBUG,"Failed to apply the changes\n");
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG,"Successfully applied the changes\n");
        if(m_cdpresynctxn_ptr)
        {
            std::string fileBeingApplied = filename;
            std::string::size_type idx = fileBeingApplied.rfind(".gz");
            if(std::string::npos != idx && (fileBeingApplied.length() - idx) == 3)
                fileBeingApplied.erase(idx, fileBeingApplied.length());
            std::string fileBeingAppliedBaseName = basename_r(fileBeingApplied.c_str());
            DebugPrintf(SV_LOG_DEBUG, "removing %s entry to resync_txn file.\n", fileBeingAppliedBaseName.c_str());
            m_cdpresynctxn_ptr -> delete_entry(fileBeingAppliedBaseName);
        }
        bRet = true;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return bRet;
}
