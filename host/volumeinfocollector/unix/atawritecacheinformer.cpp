#include "atawritecacheinformer.h"
#include "atacmd.h"
#include "utilfunctionsmajor.h"


VolumeSummary::WriteCacheState
 AtaWriteCacheInformer::GetWriteCacheState(const std::string &disk)
{
    VolumeSummary::WriteCacheState state = GetWriteCacheStateDirectly(disk);

    if (VolumeSummary::WRITE_CACHE_DONTKNOW == state)
    {
        if (!m_AtaCommandIssuer.StartSession(disk))
        {
            return state;
        }

        uint8_t resp[LENOFATACMD + LENOFATADATA] = {0};
    
        resp[0] = IDENTIFYATACODE;
        resp[3] = 1;
      
        int execstatus = m_AtaCommandIssuer.Issue(resp);
        if (0 != execstatus)
        {
            memset(resp, 0, sizeof resp);
            resp[0] = PIDENTIFYATACODE;
            resp[3] = 1;
            execstatus = m_AtaCommandIssuer.Issue(resp);
        }
        
        if (0 == execstatus)
        {
            uint16_t *data = (uint16_t *)(resp + LENOFATACMD);
            if (false == IsLittleEndian())
            {
                /* TODO: not sure how to support mixed endian machine (NUXI machines) */
                unsigned int len = LENOFATADATA / 2;
                for (int i = 0; i < len; ++i)
                    swap16bits(&data[i]);
            }
    
            if (data[82]&0x20)
            {
                state = (data[85]&0x20) ? VolumeSummary::WRITE_CACHE_ENABLED:
                                          VolumeSummary::WRITE_CACHE_DISABLED;
            }
        }

        m_AtaCommandIssuer.EndSession();
    }

    return state;
}

