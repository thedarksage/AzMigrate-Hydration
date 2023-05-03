#ifndef __CDPRESYNCAPPLY_
#define __CDPRESYNCAPPLY_

#include "cdpapply.h"
#include "usecfs.h"

class CDPResyncApply : public CDPApply
{
public:    
    CDPResyncApply(const std::string & volumename,
                   const SV_ULONGLONG & source_capacity,
                   const CDP_SETTINGS & settings,
                   bool bdiffsync,
                   cdp_resync_txn * cdp_resynctxn_mgr);
    bool Applychanges(const std::string & filename,
                      long long& totalBytesApplied,
                      char* source = 0,
                      const SV_ULONG sourceLength = 0,
                      DifferentialSync * diffSync = 0);
private:
    cdp_resync_txn * m_cdpresynctxn_ptr;
   
};


#endif
