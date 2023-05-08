#ifndef _LOGICAL__VOLUMES__H_
#define _LOGICAL__VOLUMES__H_

#include <map>
#include <vector>
#include <list>
#include <set>
#include <iterator>
#include <algorithm>
#include <functional>
#include <string>
#include <cstring>
#include <sstream>
#include "voldefs.h"
#include "volumegroupsettings.h"

/* Any lvs who actually do not have VG
 * On Linux, these are dev mappers
 * information from dmsetup table command ; only name, type, dev_t and underlying dev_ts
 * On solaris, these are metadevices of solaris volume manager */
struct Lv
{
    std::string m_Name;               /* full name including "/dev/mapper" or "/dev/md/dsk"; key in map; 
                                       * not using dev_t as key because if some one does
                                       * mknod, this breaks */
    dev_t m_Devt;                     /* to get major and minor; */
    VolumeSummary::Vendor m_Vendor;   /* dm or svm for now. Any vendor lvs not having vg concept */
    std::string m_Type;               /* for dm: linear, multipath; multipath only if disk; 
                                         for svm: may be stripe, concat , raid5 (if possible. from metastat -p we do not get these) */
    /* TODO: volumeinfocollectorinfo.h has type has unsigned. need to make common */
    long long m_Size;                 /* size. from dmt table, start block and number of blocks from table itself */
    std::set<dev_t> m_InsideDevts;    /* underlying devts. can themselves be dms or metadevices of svm */
    bool m_ShouldCollect;             /* should not collect dms given as pvs to lvm and underlying dms of dms (nested dms) 
                                       * or nested metadevices of svm */
    std::string m_VgName;             /* linux: vg name if this is the real name of lvm lv; mask off lv in this case 
                                       * solaris: name of volume itself */
    bool m_Collected;                 /* since dm disks/partitions are first collected then dm volumes. To say already collected */
    bool m_Available;                 /* For linux: lv status. For sun: always true */
    unsigned long long m_NTopLvs;     /* Number of top lvs under which this lv is there; two rules apply:
                                         1. if >= 1, do not report this lv 
                                         2. if > 1, do not report any disk/partitions also under this lv */
    bool m_HasOneTopMostLv;           /* should be interpreted only when m_NTopLvs > 1. set only whne 
                                         vg namess are copied and all top lvs of current lv have same vg */
    VolumeSummary::WriteCacheState m_WCState;
    bool m_SwapVolume;
    bool m_IsValid;
    bool m_IsMadeFromVsnap;
    bool m_Encrypted;
    bool m_BekVolume;
    bool m_IsMountedOnRoot;

    Lv()
    {
        m_Devt = 0;
        m_Vendor = VolumeSummary::UNKNOWN_VENDOR;
        m_Size = 0;
        m_ShouldCollect = true;
        m_Collected = false;
        m_Available = true;
        m_NTopLvs = 0;
        m_HasOneTopMostLv = false;
        m_WCState = VolumeSummary::WRITE_CACHE_DONTKNOW;
        m_SwapVolume = false;
        m_IsValid = true;
        m_IsMadeFromVsnap = false;
        m_Encrypted = false;
        m_BekVolume = false;
        m_IsMountedOnRoot = false;
    }
};
typedef Lv Lv_t;
typedef std::map<std::string, Lv_t> Lvs_t;
typedef std::pair<const std::string, Lv_t> Lv_pair_t;
typedef Lvs_t::const_iterator ConstLvsIter;
typedef Lvs_t::iterator LvsIter;

/* Used only in linux to compares devts of lvm lvs and dms.
 * But should not check vendor type as dm to be generic */
class LvEqDevt: public std::unary_function<Lv_pair_t, bool>
{
    dev_t m_Devt;
public:
    /* refrain from using reference for integer types 
     * since it crashes on solaris for 8 byte integers; reason has 
     * to be found */
    explicit LvEqDevt(const dev_t devt) : m_Devt(devt) { }
    bool operator()(const Lv_pair_t &in) const 
    {
        bool beq = false;
        if (this->m_Devt && in.second.m_Devt)
        {
            beq = (in.second.m_Devt == this->m_Devt);
        }
        return beq;
    }
};

bool IsDmPartition(const Lv_t &disklv,  const Lv_t &partitionlv);

/* used by all disks/partitions and dms/svms to find inside or not;
 * if yes, copy the vgname of dm for disks/partitions; 
 * do not report them */
class LvEqInsideDevt: public std::unary_function<Lv_pair_t, bool>
{
    Lv_t *m_pLv;
public:
    /* refrain from using reference for integer types 
     * since it crashes on solaris for 8 byte integers; reason has 
     * to be found */
    explicit LvEqInsideDevt(Lv_t *plv) : m_pLv(plv) { }
    bool operator()(Lv_pair_t &in) const 
    {
        bool bisinside = false;
        if (m_pLv->m_IsValid)
        {
            bisinside = in.second.m_IsValid && 
                        (in.second.m_InsideDevts.end() != in.second.m_InsideDevts.find(m_pLv->m_Devt));
        }
        return bisinside;
    }
};

/* used by all disks/partitions and dms/svms to find inside or not;
 * if yes, copy the vgname of dm for disks/partitions;
 * do not report them */
class LvEqOnlyInsideDevt: public std::unary_function<Lv_pair_t, bool>
{
    dev_t m_Devt;
public:
    /* refrain from using reference for integer types
     * since it crashes on solaris for 8 byte integers; reason has
     * to be found */
    explicit LvEqOnlyInsideDevt(const dev_t devt) : m_Devt(devt) { }
    bool operator()(const Lv_pair_t &in) const
    {
        bool beq = false;
        if (in.second.m_IsValid)
        {
            beq = (in.second.m_InsideDevts.end() != in.second.m_InsideDevts.find(m_Devt));
        }
        return beq;
    }
};


/* Used only in linux to find dm partitions 
 * But should not check vendor type as dm to be generic */
class LvEqPartitionToCollect: public std::unary_function<Lv_pair_t, bool>
{
    Lv_t *m_pDisklv;
public:
    /* refrain from using reference for integer types 
     * since it crashes on solaris for 8 byte integers; reason has 
     * to be found */
    explicit LvEqPartitionToCollect(Lv_t *plv)
    {
        m_pDisklv = plv;
    }

    bool operator()(const Lv_pair_t &in) const 
    {
        bool bislvpart = false;
        const Lv_t &lvinfo = in.second;
        if (lvinfo.m_ShouldCollect && !lvinfo.m_Collected)
        {
            const char *d = m_pDisklv->m_Name.c_str();
            const char *p = lvinfo.m_Name.c_str();

            size_t dlen = strlen(d);
            size_t plen = strlen(p);

            if ((plen > dlen) && (0 == strncmp(d, p, dlen)) && isdigit(p[plen - 1]))
            {
                bislvpart = (m_pDisklv->m_Devt && (lvinfo.m_InsideDevts.end() != lvinfo.m_InsideDevts.find(m_pDisklv->m_Devt)));
            }
        }
        
        return bislvpart;
    }
};



class CopyVGToLv: public std::unary_function<dev_t, void>
{
    std::string m_VgName;
    Lvs_t *m_PLvs;
public:
    explicit CopyVGToLv(const std::string &vgname, Lvs_t *plvs) : m_VgName(vgname), m_PLvs(plvs) { }
    /* refrain from using reference for integer types 
     * since it crashes on solaris for 8 byte integers; reason has 
     * to be found */
    void operator()(const dev_t indevt) const 
    {
        LvsIter begin = (*m_PLvs).begin();
        LvsIter end = (*m_PLvs).end();
        do
        {
            LvsIter iter = find_if(begin, end, LvEqDevt(indevt));
            if (end != iter)
            {
                Lv_t &lv = iter->second;
                lv.m_VgName = m_VgName;
                /* This ++ is not for immediately above lv but for some ancestor */
                lv.m_NTopLvs++;
                /* This recurses through all inside devts of devts; exit criteria is begin == end */
                for_each(lv.m_InsideDevts.begin(), lv.m_InsideDevts.end(), CopyVGToLv(lv.m_VgName, m_PLvs));
                /* ++ to give next interator as new begin */
                iter++;
            }
            begin = iter;
        } while (begin != end);
    }
};


class UpdateLvFromLvInfo: public std::unary_function<Lv_pair_t, void>
{
    Lvs_t *m_PLvs;
    bool m_ShouldCopyVgDown;
public:
    explicit UpdateLvFromLvInfo(Lvs_t *plvs, const bool &bshouldcp) : m_PLvs(plvs), m_ShouldCopyVgDown(bshouldcp) { }
    void operator()(Lv_pair_t &in) const 
    {
        LvsIter begin = (*m_PLvs).begin();
        LvsIter end = (*m_PLvs).end();
        Lv_t &lv = in.second;
        do
        {
            LvsIter iter = find_if(begin, end, LvEqInsideDevt(&lv));
            if (end != iter)
            {
                if (iter->second.m_Name == iter->second.m_VgName)
                    goto incr_itr;

                lv.m_ShouldCollect = false;
                /* this ++ is for immediately above top lv */
                lv.m_NTopLvs++;
                /* The underlying's vgname becomes the vgname of top one */
                lv.m_VgName = iter->second.m_VgName;
                if (m_ShouldCopyVgDown)
                {
                    /* All the inside devts of lv also should carry same vg name */
                    for_each(lv.m_InsideDevts.begin(), lv.m_InsideDevts.end(), CopyVGToLv(lv.m_VgName, m_PLvs));
                }
                
incr_itr:
                /* ++ to give next interator as new begin */
                iter++;
            }
            begin = iter;
        } while (begin != end);
    }
};


class UpdateWcStateForAll: public std::unary_function<Lv_pair_t, void>
{
    VolumeSummary::WriteCacheState m_WCState;
public:
    explicit UpdateWcStateForAll(VolumeSummary::WriteCacheState wcstate) : m_WCState(wcstate) { }
    void operator()(Lv_pair_t &in) const 
    {
        if ((VolumeSummary::WRITE_CACHE_DONTKNOW == in.second.m_WCState) ||
            (VolumeSummary::WRITE_CACHE_DISABLED == in.second.m_WCState))
        {
            in.second.m_WCState = m_WCState;
        }
    }
};


class UpdateSwapStateForAll: public std::unary_function<Lv_pair_t, void>
{
    bool m_SwapVolume;
public:
    explicit UpdateSwapStateForAll(const bool &bswapvolume) : m_SwapVolume(bswapvolume) { }
    void operator()(Lv_pair_t &in) const 
    {
        if (!in.second.m_SwapVolume)
        {
            in.second.m_SwapVolume = m_SwapVolume;
        }
    }
};

bool ShouldReportLv(const Lv_pair_t &lvpair);
bool IsDmDiskToCollect(const Lv_pair_t &dmdevpair);
void UpdateWCStateForLv(Lvs_t *plvs, VolumeSummary::WriteCacheState wcstate, Lv_t *plv);
void UpdateSwapStateForLv(Lvs_t *plvs, const bool &bswapvolume, Lv_t *plv);
void UpdateTopMostLvInfo(Lvs_t *plvs);
bool HasPluralTopLvs(const Lv_pair_t &in);
bool IsVgSameForTopLvs(const std::string &vgname, Lv_t *plv, Lvs_t *plvs);
bool HasTopLv(const Lv_pair_t &in);
void MarkInValidAndVsnapLvs(Lvs_t &lvs, const std::set<dev_t> &vsnapdevts);
void UpdateMadeFromVsnapForLv(Lvs_t *plvs, Lv_t *plv);

#endif /* _LOGICAL__VOLUMES__H_ */
