#ifndef HOST_INFO_COLLECTOR_MAJOR_H_
#define HOST_INFO_COLLECTOR_MAJOR_H_

#include "host.h"


const char SEP = ':';
const char CORE_IDS[] = "core_ids";

void CleanCpuInfos(CpuInfos_t *pcpuinfos);
void UpdateNumberOfCores(const std::string &coreid, Object &o);
bool SetArchitecture(CpuInfos_t *pcpuinfos);

#endif /* HOST_INFO_COLLECTOR_MAJOR_H_ */
