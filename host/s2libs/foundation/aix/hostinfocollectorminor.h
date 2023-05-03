#ifndef HOST_INFO_COLLECTOR_MINOR_H
#define HOST_INFO_COLLECTOR_MINOR_H

#include <string>
#include "host.h"

const char PROCESSORCMD[] = "/usr/bin/uname -p";

void CollectCpuInfo(const std::string &cpu, CpuInfos_t *pcpuinfos);

#endif /* HOST_INFO_COLLECTOR_MINOR_H */
