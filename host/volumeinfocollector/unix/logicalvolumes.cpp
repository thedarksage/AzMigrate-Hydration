#include <cstring>
#include <algorithm>
#include "logicalvolumes.h"
#include "voldefs.h"
#include "logger.h"

bool IsDmDiskToCollect(const Lv_pair_t &dmdevpair)
{
    bool bisdmdisktocol = false;
    /* DebugPrintf(SV_LOG_DEBUG, "For %s, should collect = %s, collected = %s\n", dmdevpair.second.m_Name.c_str(), 
                              dmdevpair.second.m_ShouldCollect?"true":"false", dmdevpair.second.m_Collected?"true":"false"); */
    if (dmdevpair.second.m_ShouldCollect && !dmdevpair.second.m_Collected && (VolumeSummary::DEVMAPPER == dmdevpair.second.m_Vendor))
    {
        bisdmdisktocol = (DMMULTIPATHDISKTYPE==dmdevpair.second.m_Type) || (DMRAIDDISKTYPE==dmdevpair.second.m_Type);
    }
    return bisdmdisktocol;
}


bool ShouldReportLv(const Lv_pair_t &lvpair)
{
    bool bshouldreport = false;

    const Lv_t &lv = lvpair.second;
    if (lv.m_IsValid && lv.m_ShouldCollect && !lv.m_Collected && (0 == lv.m_NTopLvs) && !lv.m_IsMadeFromVsnap)
    {
        bshouldreport = true;
    }

    return bshouldreport;
}


void UpdateMadeFromVsnapForLv(Lvs_t *plvs, Lv_t *plv)
{
    LvsIter begin = plvs->begin();
    LvsIter end = plvs->end();

    do
    {
        LvsIter iter = find_if(begin, end, LvEqInsideDevt(plv));

        if (end != iter)
        {
            iter->second.m_IsMadeFromVsnap = true;
            Lv_t &lv = iter->second;
            UpdateMadeFromVsnapForLv(plvs, &lv);
            iter++;
        }
        begin = iter;
    } while (begin != end);
}


void UpdateWCStateForLv(Lvs_t *plvs, VolumeSummary::WriteCacheState wcstate, Lv_t *plv)
{
    LvsIter begin = plvs->begin();
    LvsIter end = plvs->end();

    do
    {
        LvsIter iter = find_if(begin, end, LvEqInsideDevt(plv));

        if (end != iter)
        {
            if (iter->second.m_WCState == VolumeSummary::WRITE_CACHE_DONTKNOW)
            {
                iter->second.m_WCState = wcstate;
            }
            Lv_t &lv = iter->second;
            UpdateWCStateForLv(plvs, wcstate, &lv);
            iter++;
        }
        begin = iter;
    } while (begin != end);
}


void UpdateSwapStateForLv(Lvs_t *plvs, const bool &bswapvolume, Lv_t *plv)
{
    LvsIter begin = plvs->begin();
    LvsIter end = plvs->end();

    do
    {
        LvsIter iter = find_if(begin, end, LvEqInsideDevt(plv));

        if (end != iter)
        {
            if (!(iter->second.m_SwapVolume))
            {
                iter->second.m_SwapVolume = bswapvolume;
            }
            Lv_t &lv = iter->second;
            UpdateSwapStateForLv(plvs, bswapvolume, &lv);
            iter++;
        }
        begin = iter;
    } while (begin != end);
}


bool HasPluralTopLvs(const Lv_pair_t &in)
{
    return (in.second.m_NTopLvs > 1);
}


bool HasTopLv(const Lv_pair_t &in)
{
    return (in.second.m_NTopLvs > 0);
}


void UpdateTopMostLvInfo(Lvs_t *plvs)
{
    LvsIter begin = plvs->begin();
    LvsIter end = plvs->end();

    do
    {
        LvsIter iter = find_if(begin, end, HasPluralTopLvs);

        if (end != iter)
        {
            Lv_t &lv = iter->second;
            iter->second.m_HasOneTopMostLv = IsVgSameForTopLvs(iter->second.m_VgName, &lv, plvs);
            iter++;
        }
        begin = iter;
    } while (begin != end);
}


/* TODO: optimize recursion */
bool IsVgSameForTopLvs(const std::string &vgname, Lv_t *plv, Lvs_t *plvs)
{
    /* There are no top lvs for this: 
    * send as true */
    bool bisvgsame = true;

    LvsIter begin = plvs->begin();
    LvsIter end = plvs->end();

    do
    {
        LvsIter iter = find_if(begin, end, LvEqInsideDevt(plv));

        if (end != iter)
        {
            Lv_t &lv = iter->second;
            bisvgsame = (lv.m_VgName == vgname); 
            if (bisvgsame)
            {
                bisvgsame = IsVgSameForTopLvs(vgname, &lv, plvs);
            }
            if (!bisvgsame)
            {
                break;
            }
            iter++;
        }
        
        begin = iter;
    } while (begin != end);

    return bisvgsame;
}


void MarkInValidAndVsnapLvs(Lvs_t &lvs, const std::set<dev_t> &vsnapdevts)
{
    for (LvsIter lviter = lvs.begin(); lviter != lvs.end(); lviter++)
    {
        Lv_t &lv = lviter->second;
        if ((VolumeSummary::DEVMAPPER == lv.m_Vendor) &&
            ((DMMULTIPATHDISKTYPE == lv.m_Type) || (DMRAIDDISKTYPE == lv.m_Type)))
        {
            for (LvsIter tlviter = lvs.begin(); tlviter != lvs.end(); tlviter++)
            {
                Lv_t &tlv = tlviter->second;
                if ((VolumeSummary::DEVMAPPER == tlv.m_Vendor) &&
                    IsDmPartition(lv, tlv))
                {
                    /*
                    DebugPrintf(SV_LOG_DEBUG, "marking dm partition %s of disk %s as invalid\n", 
                                              tlv.m_Name.c_str(), lv.m_Name.c_str());
                    */
                    tlv.m_IsValid = false;
                }
                else
                {
                    /*
                    DebugPrintf(SV_LOG_DEBUG, "lv %s is not partition of lv %s\n", 
                                              tlv.m_Name.c_str(), lv.m_Name.c_str());
                    */
                }
            }
        }

        if (lv.m_InsideDevts.end() != std::find_first_of(lv.m_InsideDevts.begin(), lv.m_InsideDevts.end(),
                                                         vsnapdevts.begin(), vsnapdevts.end()))
        {
            DebugPrintf(SV_LOG_DEBUG, "lv %s is made from vsnap\n", lv.m_Name.c_str());
            lv.m_IsMadeFromVsnap = true;
            UpdateMadeFromVsnapForLv(&lvs, &lv);
        }
    }
}
