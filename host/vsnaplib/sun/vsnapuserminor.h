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
#include "VsnapCommon.h"
#include "VsnapUser.h"
#include "portablehelpers.h"
#include "portablehelpersminor.h"

typedef minor_t sv_minor_t;

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

bool PerformZpoolDestroyIfReq(const std::string &devicefile);
bool createLinkToDevice(VsnapVirtualVolInfo *VirtualInfo, const std::string & devicename,const std::string & linkname);

class Eq_vsnap: public std::unary_function<ZpoolWithStorageDevices_t, bool>
{
    std::string vsnapname;
public:
    explicit Eq_vsnap(const std::string &ss) : vsnapname(ss) { }
    bool operator()(const ZpoolWithStorageDevices_t &in) const 
    {
        const svector_t &devices = in.second;
        bool bretval = (devices.end() != find(devices.begin(), devices.end(), vsnapname));
        if (!bretval)
        {
            size_t sizeofvsnap = vsnapname.size();  
            size_t vsnapsuffixsize = strlen(VSNAP_SUFFIX);
            size_t sizeofvsnapwithoutsuffix = sizeofvsnap - vsnapsuffixsize;

            const char *p = vsnapname.c_str() + sizeofvsnapwithoutsuffix;
            
            if (!strcmp(p, VSNAP_SUFFIX))
            { 
                std::string vsnapexcludingsuffix = vsnapname.substr(0, sizeofvsnapwithoutsuffix);
                bretval = (devices.end() != find(devices.begin(), devices.end(), vsnapexcludingsuffix));
            }
        }
        return bretval;
    }
};

#endif /* VSNAPUSER__MINOR_H__ */
