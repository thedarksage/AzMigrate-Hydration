#ifndef _ATA_WRITECACHEINFORMER__H_
#define _ATA_WRITECACHEINFORMER__H_

#include <string>
#include "volumegroupsettings.h"
#include "atacommandissuer.h"


class AtaWriteCacheInformer
{
public:
    VolumeSummary::WriteCacheState 
     GetWriteCacheState(const std::string &disk);

private:
    VolumeSummary::WriteCacheState
     GetWriteCacheStateDirectly(const std::string &disk);

private:
    AtaCommandIssuer m_AtaCommandIssuer;
};

#endif /* _ATA_WRITECACHEINFORMER__H_ */
