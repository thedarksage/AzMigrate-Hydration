#ifndef __STOP_VOLUMEGROUPS_H__
#define __STOP_VOLUMEGROUPS_H__

#include "volumegroupsettings.h"
#include "devicestream.h"

#include <set>

struct PauseTrackingUpdateInfo
{
	int m_statusCode;
	std::string m_statusMsg;
	PauseTrackingUpdateInfo()
	{
		m_statusCode = 1;
		m_statusMsg = "Failed";
	}
};

typedef std::map<std::string, PauseTrackingUpdateInfo> PauseTrackingStatus_t; //key is volumenamepath.

/// \brief stops filtering on device specified in statuInfo
bool stopFiltering(DeviceStream *pDeviceStream, ///< driver stream
	PauseTrackingStatus_t& statusInfo           ///< statusInfo (includes device name on which to do stop filtering)
	);

#endif