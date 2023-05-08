#ifndef LUN_CAPACITY_INFORMER_H
#define LUN_CAPACITY_INFORMER_H

#include "inmdefines.h"
#include "scsiluncapacityinformer.h"

class LunCapacityInformer
{
public:
    LunCapacityInformer();
    LunCapacityInformer(ScsiCommandIssuer *pscsicommandissuer);
    SV_ULONGLONG GetCapacity(const std::string &disk, const SV_ULONGLONG &capacity);
    SV_ULONGLONG GetLengthForLunMakeup(void);

private:
    void GetBlocks(const std::string &device, Capacity_t *pluncap);
    SV_ULONGLONG GetCapacity(const std::string &device);

private:
    ScsiLunCapacityInformer m_ScsiLunCapacityInformer;
};

#endif /* LUN_CAPACITY_INFORMER_H */
