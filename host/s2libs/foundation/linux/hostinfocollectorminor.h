#ifndef HOST_INFO_COLLECTOR_MINOR_H_
#define HOST_INFO_COLLECTOR_MINOR_H_

#include <string>
#include "host.h"

const char PROCESSOR_TOK[] = "processor";
const char VENDOR_ID_TOK[] = "vendor_id";
const char MODELNAME_TOK[] = "model name";
const char PHYSICALID_TOK[] = "physical id";
const char COREID_TOK[] = "core id";
const char PROCESSORCMD[] = "uname -p";

void CollectCpuInfo(const std::string &cpuinfo, CpuInfos_t *pcpuinfos);

#endif /* HOST_INFO_COLLECTOR_MINOR_H_ */
