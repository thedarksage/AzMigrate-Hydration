#include <algorithm>
#include "volumegroups.h"

void UpdateVgFromVgInfo(Vg_pair_t &vgiter)
{
    Lvs_t &lvs = vgiter.second.m_Lvs;
    /* since vgs are carrying devts */
    bool bcopyvgdown = false;
    for_each(lvs.begin(), lvs.end(), UpdateLvFromLvInfo(&lvs, bcopyvgdown));
}


void MarkVsnapVgs(Vgs_t &vgs, const std::set<dev_t> &vsnapdevts)
{
    for (VgsIter vgiter = vgs.begin(); vgiter != vgs.end(); vgiter++)
    {
        Vg_t &vg = vgiter->second;

        if (vg.m_InsideDevts.end() != std::find_first_of(vg.m_InsideDevts.begin(), vg.m_InsideDevts.end(),
                                                         vsnapdevts.begin(), vsnapdevts.end()))
        {
            DebugPrintf(SV_LOG_DEBUG, "volume group %s is made from vsnap\n", vg.m_Name.c_str());
            for (LvsIter lviter = vg.m_Lvs.begin(); lviter != vg.m_Lvs.end(); lviter++)
            {
                Lv_t &lv = lviter->second;
                lv.m_IsMadeFromVsnap = true;
            }
        }
    }
}
