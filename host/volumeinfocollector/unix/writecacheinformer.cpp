#include <string>
#include "writecacheinformer.h"


WriteCacheInformer::WriteCacheInformer()
{
    /* TODO: check return value ? */
    m_ScsiWriteCacheInformer.Init();
}


WriteCacheInformer::WriteCacheInformer(ScsiCommandIssuer *pscsicommandissuer)
: m_ScsiWriteCacheInformer(pscsicommandissuer)
{
    /* TODO: check return value ? */
    m_ScsiWriteCacheInformer.Init();
}


VolumeSummary::WriteCacheState 
 WriteCacheInformer::GetWriteCacheState(const std::string &disk)
{
    /* info: spc2, sbc2, ata8 */
    VolumeSummary::WriteCacheState state = m_ScsiWriteCacheInformer.GetWriteCacheState(disk);
    if (VolumeSummary::WRITE_CACHE_DONTKNOW == state)
    {
        state = m_AtaWriteCacheInformer.GetWriteCacheState(disk);
    }
    
    return state;
}
