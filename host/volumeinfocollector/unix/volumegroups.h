#ifndef _VOLUME__GROUP__H_
#define _VOLUME__GROUP__H_

#include <map>
#include <vector>
#include <list>
#include <set>
#include <iterator>
#include <algorithm>
#include <functional>
#include <string>
#include <sstream>
#include "logicalvolumes.h"
#include "volumegroupsettings.h"

/* information from vgdisplay -v; vxdisk list, diskset, metadb, metahsps */
struct Vg
{
    std::string m_Name;               /* name of vg; multimap since many vgs with same name can exist */
    VolumeSummary::Vendor m_Vendor;   /* lvm or vxvm for now; can be any vendor providing vgs */
    Lvs_t m_Lvs;                      /* lvm lvs */
    std::set<dev_t> m_InsideDevts;    /* dev_ts of pvs */
    bool m_HasLvmMountedOnRoot;

    Vg()
    {
        m_Vendor = VolumeSummary::UNKNOWN_VENDOR;
        m_HasLvmMountedOnRoot = false;
    }
};
typedef Vg Vg_t;
typedef std::multimap<std::string, Vg_t> Vgs_t;
typedef std::pair<const std::string, Vg_t> Vg_pair_t;
typedef Vgs_t::const_iterator ConstVgsIter;
typedef Vgs_t::iterator VgsIter;

/* used by all disks/partitions and dms to find whether these are pvs; if yes, copy the vgname */
class VgEqInsideDevt: public std::unary_function<Vg_pair_t, bool>
{
    dev_t m_Devt;
public:
    /* refrain from using reference for integer types 
     * since it crashes on solaris for 8 byte integers; reason has 
     * to be found */
    explicit VgEqInsideDevt(const dev_t devt) : m_Devt(devt) { }
    bool operator()(const Vg_pair_t &in) const 
    {
        return (in.second.m_InsideDevts.end() != in.second.m_InsideDevts.find(m_Devt));
    }
};


class EqVgNameAndVendor: public std::unary_function<Vg_pair_t, bool>
{
    std::string m_Name;
    VolumeSummary::Vendor m_Vendor;
public:
    /* refrain from using reference for integer types 
     * since it crashes on solaris for 8 byte integers; reason has 
     * to be found */
    explicit EqVgNameAndVendor(const std::string &vgname, const VolumeSummary::Vendor vendor) : m_Name(vgname), m_Vendor(vendor)  { }
    bool operator()(const Vg_pair_t &in) const 
    {
        const Vg_t &vg = in.second;
        return ((m_Name == vg.m_Name) && (m_Vendor == vg.m_Vendor));
    }
};

void UpdateVgFromVgInfo(Vg_pair_t &vgiter);
void MarkVsnapVgs(Vgs_t &vgs, const std::set<dev_t> &vsnapdevts);

#endif /* _VOLUME__GROUP__H_ */
