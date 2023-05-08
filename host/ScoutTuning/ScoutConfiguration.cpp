//All the functions which would be common to all projects are included here.
#ifndef _INM_SCOUT_CONFIGURATION_H
#define _INM_SCOUT_CONFIGURATION_H

#include "ScoutConfiguration.h"
#include <boost/lexical_cast.hpp>
#include "logger.h"
#include "portable.h"

#ifdef WIN32
#include <windows.h>
#endif

extern std::string VERSION;
extern std::string ROLE;
// Sections
static const char SECTION_VXAGENT[] = "vxagent";

//Keys
static const char KEY_REGISTER_LAYOUT_ON_DISKS[] = "RegisterLayoutOnDisks";
static const char KEY_REPORT_FULL_DEVICE_NAMES[] = "ReportFullDeviceNamesOnly";
static const char KEY_DP_CACHE_VOLUME_HANDLE[]	 = "DPCacheVolumeHandle";

using namespace std;

void ScoutConfiguration::setRegisterLayoutOnDisks(bool bVal)
{
	set(SECTION_VXAGENT,KEY_REGISTER_LAYOUT_ON_DISKS,boost::lexical_cast<std::string>(bVal));
}


void ScoutConfiguration::setReportFullDeviceNames(bool bVal)
{
	set(SECTION_VXAGENT,KEY_REPORT_FULL_DEVICE_NAMES,boost::lexical_cast<std::string>(bVal));
}

void ScoutConfiguration::setDPCacheVolumeHandle(bool bVal)
{
	set(SECTION_VXAGENT,KEY_DP_CACHE_VOLUME_HANDLE,boost::lexical_cast<std::string>(bVal));
}



#endif //~ _INM_SCOUT_CONFIGURATION_H