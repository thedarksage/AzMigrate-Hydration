#include "XenServerDiscovery.h"
using namespace std;

void printXenPoolInfo(ClusterInfo &xenPoolInfo, VMInfos_t &vmInfos, VDIInfos_t &vdinfos)
{
	//void
}

int getXenPoolInfo(ClusterInfo &clusInfo, VMInfos_t &vmInfos, VDIInfos_t &vdinfos)
{
    return 0; //success
}

int isXenPoolMember(bool& isMember, PoolInfo& pInfo)
{
	isMember = false;
	return 0; //success
}

int getAvailableVDIs(VDIInfos_t &vdiInfos)
{
	return 0; //success
}
int isPoolMaster(bool& isMaster)
{
	isMaster = false;
	return 0; //success
}

int is_xsowner(const string device, bool& owner)
{
	owner = false; //Non-owner
        return 0; //success
}
