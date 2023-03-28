#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "luncapacityinformer.h"
#include "logger.h"
#include "portablehelpers.h"


LunCapacityInformer::LunCapacityInformer()
{
    /* TODO: check return value ? */
    m_ScsiLunCapacityInformer.Init();
}


LunCapacityInformer::LunCapacityInformer(ScsiCommandIssuer *pscsicommandissuer)
: m_ScsiLunCapacityInformer(pscsicommandissuer)
{
    /* TODO: check return value ? */
    m_ScsiLunCapacityInformer.Init();
}


SV_ULONGLONG LunCapacityInformer::GetCapacity(const std::string &disk, const SV_ULONGLONG &capacity)
{
    SV_ULONGLONG rawlc = GetCapacity(disk); 
    if (0 == rawlc)
    {
        DebugPrintf(SV_LOG_WARNING, "For device %s, could not find lun capacity. Adding predefined value to capacity\n", disk.c_str());
        rawlc = capacity + GetLengthForLunMakeup();
    }
    
    return rawlc;
}


SV_ULONGLONG LunCapacityInformer::GetCapacity(const std::string &device)
{
    Capacity_t lc;
    GetBlocks(device, &lc);
    SV_ULONGLONG c = lc.m_Nblks * lc.m_BlkSz;

    return c;
}


void LunCapacityInformer::GetBlocks(const std::string &device, Capacity_t *pluncap)
{
    m_ScsiLunCapacityInformer.GetCapacity(device, pluncap);
}
