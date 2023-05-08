#ifndef _WRITE__CACHE__INFORMER__H_
#define _WRITE__CACHE__INFORMER__H_

#include <string>
#include "volumegroupsettings.h"
#include "scsiwritecacheinformer.h"
#include "atawritecacheinformer.h"


/* WriteCacheState can be queried for disks only */
class WriteCacheInformer
{
public:
    WriteCacheInformer();
    WriteCacheInformer(ScsiCommandIssuer *pscsicommandissuer);
    VolumeSummary::WriteCacheState 
     GetWriteCacheState(const std::string &disk);

private:
    ScsiWriteCacheInformer m_ScsiWriteCacheInformer;
    AtaWriteCacheInformer m_AtaWriteCacheInformer;
};

#endif /* _WRITE__CACHE__INFORMER__H_ */
