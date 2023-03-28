#ifndef _SCSI__LUN__CAPACITY__INFORMER_H_
#define _SCSI__LUN__CAPACITY__INFORMER_H_

#include "inmdefines.h"
#include "scsicommandissuer.h"

class ScsiLunCapacityInformer
{
public:
    ScsiLunCapacityInformer();
    ScsiLunCapacityInformer(ScsiCommandIssuer *pscsicommandissuer);
    bool Init(void);
    ~ScsiLunCapacityInformer();

    void GetCapacity(const std::string &disk, Capacity_t *pluncap);

private:
    ScsiCommandIssuer *m_pScsiCommandIssuer;
    bool m_IsScsiCommandIssuerExternal;
};

#endif /* _SCSI__LUN__CAPACITY__INFORMER_H_ */
