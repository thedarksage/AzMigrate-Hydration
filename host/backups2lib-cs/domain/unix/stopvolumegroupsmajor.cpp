#include "stopvolumegroups.h"
#include "device.h"
#include "devicestream.h"
#include "devicefilter.h"
#include "volumegroupsettings.h"

bool stopFiltering(DeviceStream *pDeviceStream, PauseTrackingStatus_t& puaseTrackingVolumeInfoMap)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	bool breturnStatus = true;

	PauseTrackingStatus_t::iterator puaseTrackingVolumeInfoMapBegIter = puaseTrackingVolumeInfoMap.begin();
	PauseTrackingStatus_t::iterator puaseTrackingVolumeInfoMapEndIter = puaseTrackingVolumeInfoMap.end();
	while (puaseTrackingVolumeInfoMapBegIter != puaseTrackingVolumeInfoMapEndIter)
	{
		std::stringstream status, message;
		PauseTrackingUpdateInfo& pstrkinfo = puaseTrackingVolumeInfoMapBegIter->second;;
		std::string volume = puaseTrackingVolumeInfoMapBegIter->first;
		if (SV_SUCCESS != pDeviceStream->IoControl(IOCTL_INMAGE_STOP_FILTERING_DEVICE, (void*)volume.c_str(), volume.size(), NULL, 0))
		{
			DebugPrintf(SV_LOG_ERROR, "FAILED : Stop filtering failed for volume %s. Err:: %s\n ", volume.c_str(), Error::Msg().c_str());
			breturnStatus = false;
		}
		else
		{
			DebugPrintf(SV_LOG_ALWAYS, "StopFiltering succeeded for volume %s\n", volume.c_str());
		}

		if (SV_SUCCESS != pDeviceStream->IoControl(IOCTL_INMAGE_CLEAR_DIFFERENTIALS, (void*)volume.c_str(), volume.size(), NULL, 0))
		{
			DebugPrintf(SV_LOG_ERROR, "FAILED : Stop filtering (clear diffs) failed for volume %s. Err:: %s\n ", volume.c_str(), Error::Msg().c_str());
			pstrkinfo.m_statusCode = 1;
			status << "pause_tracking_status=" << "failed";
			message << "pause_tracking_message=" << "Volume:" << volume << Error::Msg();
			breturnStatus = false;
		}
		else
		{
			DebugPrintf(SV_LOG_ALWAYS, "Clear diffs succeeded for volume %s\n", volume.c_str());
			pstrkinfo.m_statusCode = 0;
			status << "pause_tracking_status=" << "completed";
			message << "pause_tracking_message=" << "success";

		}
		std::string::size_type index = pstrkinfo.m_statusMsg.find("pause_tracking_status=failed");
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
