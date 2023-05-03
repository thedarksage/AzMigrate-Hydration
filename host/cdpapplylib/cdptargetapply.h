#ifndef __CDPTARGETAPPLY_
#define __CDPTARGETAPPLY_

#include "cdpapply.h"

class CDPTargetApply : public CDPApply
{
 
public:
    CDPTargetApply(const std::string & volumename, const SV_ULONGLONG & source_capacity, const CDP_SETTINGS & settings, bool bdiffsync);
    bool Applychanges(const std::string & filename,
                              long long& totalBytesApplied,
		                      char* source = 0,
		                      const SV_ULONG sourceLength = 0,
                              DifferentialSync * diffSync = 0);
     void SetTargets();
};

#endif