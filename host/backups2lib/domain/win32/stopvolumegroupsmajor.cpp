#include <cstring>
#include <string>
#include "../stopvolumegroups.h"
#include <winioctl.h>
#include "devicestream.h"
#include "devicefilter.h"
#include "portablehelpersmajor.h"
#include "devicefilter.h"
#include "inmsafecapis.h"

bool stopFiltering(DeviceStream *pDeviceStream, PauseTrackingStatus_t& puaseTrackingVolumeInfoMap)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	bool breturnStatus = true;
	PauseTrackingStatus_t::iterator puaseTrackingVolumeInfoMapBegIter = puaseTrackingVolumeInfoMap.begin();
	PauseTrackingStatus_t::iterator puaseTrackingVolumeInfoMapEndIter = puaseTrackingVolumeInfoMap.end();
	while (puaseTrackingVolumeInfoMapBegIter != puaseTrackingVolumeInfoMapEndIter)
	{
		std::stringstream status, message;
		std::string volume = puaseTrackingVolumeInfoMapBegIter->first;
		DebugPrintf(SV_LOG_DEBUG, "Stop Filtering will be issed for %s\n", volume.c_str());
		PauseTrackingUpdateInfo& pstrkinfo = puaseTrackingVolumeInfoMapBegIter->second;
		STOP_FILTERING_INPUT stopFilteringInput;
		memset(&stopFilteringInput, 0, sizeof stopFilteringInput);
		std::wstring device(volume.begin(), volume.end());
		inm_wmemcpy_s(stopFilteringInput.VolumeGUID, NELEMS(stopFilteringInput.VolumeGUID), device.c_str(), device.length());
		stopFilteringInput.ulFlags |= STOP_FILTERING_FLAGS_DELETE_BITMAP;
		if (pDeviceStream->IoControl(IOCTL_INMAGE_STOP_FILTERING_DEVICE, &stopFilteringInput, sizeof(stopFilteringInput), NULL, 0) != SV_SUCCESS)
		{
			DebugPrintf(SV_LOG_ERROR, "FAILED : Stop filtering failed for volume %s. Err:: %s\n ", volume.c_str(), Error::Msg().c_str());
			status << "pause_tracking_status=" << "failed";
			message << "pause_tracking_message=" << "failed. cause: " << Error::Msg();
			breturnStatus = false;
			pstrkinfo.m_statusCode = 1;
		}
		else
		{
			DebugPrintf(SV_LOG_ALWAYS, "StopFiltering succeeded for volume %s\n", volume.c_str());
			pstrkinfo.m_statusCode = 0;
			status << "pause_tracking_status=" << "completed";
			message << "pause_tracking_message=" << "success";
		}
		std::string::size_type index = pstrkinfo.m_statusMsg.find("pause_tracking_status=failed"); // by default we are setting it to failed before calling this function.
		if (index != std::string::npos)
		{
			pstrkinfo.m_statusMsg.replace(index, 28, status.str());
		}
		index = pstrkinfo.m_statusMsg.find("pause_tracking_message=");
		if (index != std::string::npos)
		{
			pstrkinfo.m_statusMsg.replace(index, 23, message.str());
		}
		puaseTrackingVolumeInfoMapBegIter++;
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return breturnStatus;
}
