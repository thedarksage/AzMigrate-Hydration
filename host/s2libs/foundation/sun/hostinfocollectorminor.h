#ifndef HOST_INFO_COLLECTOR_MINOR_H
#define HOST_INFO_COLLECTOR_MINOR_H

#include <string>
#include "host.h"

const char PROCESSORCMD[] = "/usr/bin/uname -p";
const char BRAND_TOK[] = "brand";           /* used as name; since closest approximation */
const char CHIP_ID_TOK[] = "chip_id";
const char CORE_ID_TOK[] = "core_id";
const char CPU_TYPE_TOK[] = "cpu_type";    /* some times brand is not printed at all; use this as name then*/
const char VENDOR_ID_TOK[] = "vendor_id"; /* intel chips give this */
const char IMPL_TOK[] = "implementation"; /* sparc chips do not give vendor_id; hence using implementation */
const char NAME_TOK[] = "name:";

void CollectCpuInfo(const std::string &cpuinfo, CpuInfos_t *pcpuinfos);

#endif /* HOST_INFO_COLLECTOR_MINOR_H */
