#ifndef SCOUT_CONFIGURATION_
#define SCOUT_CONFIGURATION_

#include <iostream>
#include <fstream>
#include <string>
#include <map>

#include "localconfigurator.h"

#define OPT_VERSION			"-version"
#define OPT_ROLE			"-role"
#define OPT_HELP			"-h"

void Usage();

class ScoutConfiguration: FileConfigurator
{
public:
	std::string m_drscoutConfFilePath;
	virtual int RoleConfigChanges() = 0;
	void setRegisterLayoutOnDisks(bool bVal = true);
	void setReportFullDeviceNames(bool bVal = false);
	void setDPCacheVolumeHandle(bool bVal = true);
};
#endif