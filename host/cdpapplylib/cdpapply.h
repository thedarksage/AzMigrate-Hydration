#ifndef __CDPAPPLY_
#define __CDPAPPLY_

#include <string>
#include "svtypes.h"
#include "cdpv2writer.h"
#include "differentialsync.h"

class CDPApply
{
public:
    typedef boost::shared_ptr<CDPApply> ApplyPtr;
    CDPV2Writer::Ptr m_Cdpwriter;
    CDPApply();
    virtual ~CDPApply() {m_Cdpwriter.reset() ; }
    virtual bool Applychanges(const std::string & filename,
                              long long& totalBytesApplied,
		                      char* source = 0,
		                      const SV_ULONG sourceLength = 0,
                              DifferentialSync * diffSync = 0) = 0;
private:

	
};

#endif
