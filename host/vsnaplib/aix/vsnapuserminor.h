#ifndef VSNAPUSER__MINOR_H__
#define VSNAPUSER__MINOR_H__

#include <functional>
#include <list>
#include <iostream>
#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include <cstring>

#include <sys/types.h>
//typedef minor_t sv_minor_t;

#define TOTALSECS_TO_WAITFOR_VSNAP 30
#define CHECK_FREQUENCY_INSECS 1

#define TOTALSECS_TO_WAITFOR_SYNCDEV 10
#define CHECK_FREQ_FOR_SYNCDEV 2

#define TOTALSECS_TO_WAITFOR_ZPOOL 6
#define CHECK_FREQ_FOR_ZPOOL 2

#define ZPOOL_EXPORT "/usr/sbin/zpool export"
#define ZPOOL_EXPORT_FORCE "/usr/sbin/zpool export -f"

#define ZPOOL_DESTROY "/usr/sbin/zpool destroy"
#define ZPOOL_DESTROY_FORCE "/usr/sbin/zpool destroy -f"

#define LINVSNAP_SYNC_DRV "/dev/vblkcontrol"

#define AGENT_SERVICE "svagents"

typedef std::vector<std::string> vs_t;
typedef std::map<std::string, vs_t> zpool_vs_map_t;
typedef std::pair<std::string, vs_t> zpool_vs_pair_t;

//void CleanUpIncompleteVsnap(VsnapVirtualVolInfo* VirtualInfo);

class Eq_vsnap: public std::unary_function<zpool_vs_pair_t, bool>
{
    std::string vsnapname;
public:
    explicit Eq_vsnap(const std::string &ss) : vsnapname(ss) { }
    bool operator()(const zpool_vs_pair_t &in) const 
    {
        const vs_t &vsnaps = in.second;
        bool bretval = (vsnaps.end() != find(vsnaps.begin(), vsnaps.end(), vsnapname));
        if (!bretval)
        {
            size_t sizeofvsnap = vsnapname.size();  
            size_t vsnapsuffixsize = strlen(VSNAP_SUFFIX);
            size_t sizeofvsnapwithoutsuffix = sizeofvsnap - vsnapsuffixsize;

            const char *p = vsnapname.c_str() + sizeofvsnapwithoutsuffix;
            
            if (!strcmp(p, VSNAP_SUFFIX))
            { 
                std::string vsnapexcludingsuffix = vsnapname.substr(0, sizeofvsnapwithoutsuffix);
                bretval = (vsnaps.end() != find(vsnaps.begin(), vsnaps.end(), vsnapexcludingsuffix));
            }
        }
        return bretval;
    }
};

#endif /* VSNAPUSER__MINOR_H__ */
