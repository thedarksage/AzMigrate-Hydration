// DIUtilMajor.cpp : unix functions
//

#include "CLocalMachineWithDisks.h"
#include "ILogger.h"

void GetDiskNames(const CLocalMachineWithDisks::Disks_t &diskIds, ILoggerPtr pILogger, CLocalMachineWithDisks::Disks_t &diskNames)
{
	// For unix, disk Ids are same as names
	diskNames = diskIds;
}

boost::shared_ptr<CPlatformAPIs> GetPlatformAPIs(void)
{
	boost::shared_ptr<CPlatformAPIs> p;
	p.reset(new CUnixAPIs());

	return p;
}