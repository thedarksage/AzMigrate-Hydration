/* File       : volumeopmajor.cpp
*
* Description: unix specific code for getting volume operations.
*/

#include "volumeop.h"
#include "error.h"
#include "cxmediator.h"
#include "logger.h"
#include "filterdrvifmajor.h"
#include "volume.h"

namespace VolumeOps
{
	// Get Tracked IO bytes which is nothing but, total churned bytes - skipped bytes 
	int VolumeOperator::GetTotalTrackedBytes(DeviceStream& stream, const FilterDrvVol_t &sourceName, SV_ULONGLONG& ullTotalTrackedBytes)
	{
		int iStatus = SV_SUCCESS;
		SV_ULONGLONG ullTrackedBytes = 0;

		MONITORING_STATS vms;

		memset(&vms, 0, sizeof(MONITORING_STATS));
		if (strncpy_s(vms.VolumeGuid.volume_guid,
			GUID_SIZE_IN_CHARS,
			sourceName.c_str(),
			GUID_SIZE_IN_CHARS - 1))
		{
			DebugPrintf(SV_LOG_ERROR, "\nstrncpy_s failed to copy Device Name to the buffer.\n");
			iStatus = SV_FAILURE;
			return iStatus;
		}
		vms.ReqStat = GET_CHURN_STATS;
		if ((iStatus = stream.IoControl(IOCTL_INMAGE_GET_MONITORING_STATS, &(vms), sizeof(MONITORING_STATS))) == SV_SUCCESS)
		{
			ullTotalTrackedBytes = (long long int)vms.ChurnStats.NumCommitedChangesInBytes;
			DebugPrintf(SV_LOG_DEBUG, "SUCCESS: Got the total number of tracked bytes cumulatively from the time the system booted. Tracked Bytes = %lld\n", ullTotalTrackedBytes);

			iStatus = SV_SUCCESS;
		}
		else
		{
			ullTotalTrackedBytes = 0;
			iStatus = SV_FAILURE;
			DebugPrintf(SV_LOG_ERROR, "Failed to get the total number of bytes tracked until now for the device %s.\n",sourceName.c_str());
		}
		return iStatus;
	}

	// Get the total dropped tags count from the driver
	int VolumeOperator::GetDroppedTagsCount(DeviceStream& stream, const FilterDrvVol_t &sourceName, SV_ULONGLONG& ullDroppedTagsCount)
	{
		int iStatus = SV_SUCCESS;
		SV_ULONGLONG  ulDroppedTagsCount = 0;
		MONITORING_STATS vms;

		memset(&vms, 0, sizeof(MONITORING_STATS));
		if (strncpy_s(vms.VolumeGuid.volume_guid,
			GUID_SIZE_IN_CHARS,
			sourceName.c_str(),
			GUID_SIZE_IN_CHARS - 1))
		{
			DebugPrintf(SV_LOG_ERROR, "\nstrncpy_s failed to copy Device Name to the buffer.\n");
			iStatus = SV_FAILURE;
			return iStatus;
		}
		vms.ReqStat = GET_TAG_STATS;
		if ((iStatus = stream.IoControl(IOCTL_INMAGE_GET_MONITORING_STATS,
			&(vms), sizeof(MONITORING_STATS))) == SV_SUCCESS)
		{
			ulDroppedTagsCount = (unsigned long long)vms.TagStats.TagsDropped;
			DebugPrintf(SV_LOG_DEBUG, "SUCCESS:Got the total number of dropped tags for device %s: %lld\n", sourceName.c_str(), ulDroppedTagsCount);
			iStatus = SV_SUCCESS;
		}
		else
		{
			ulDroppedTagsCount = 0;
			iStatus = SV_FAILURE;
			DebugPrintf(SV_LOG_ERROR, "Failed to get the total number of dropped tags for the device %s.\n",sourceName.c_str());
		}
		return  iStatus;
	}

}