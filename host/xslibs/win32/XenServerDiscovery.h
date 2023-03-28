#ifndef XENSERVERDISCOVERY_H
#define XENSERVERDISCOVERY_H

#include <string.h>

#include "volumegroupsettings.h"

int is_xsowner(const std::string device, bool& owner);

int isXenPoolMember(bool& isMember, PoolInfo&);

int isPoolMaster(bool& isMaster);

int getAvailableVDIs(VDIInfos_t &);

int getXenPoolInfo(ClusterInfo &, VMInfos_t &, VDIInfos_t &);

void printXenPoolInfo(ClusterInfo &, VMInfos_t &, VDIInfos_t &);

#endif //XENSERVERDISCOVERY_H
