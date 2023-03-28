/* File       : volumeopmajor.cpp
*
* Description: Windows specific code for volume operations.
*/
#include "volumeop.h"
#include "error.h"
#include "cxmediator.h"
#include "logger.h"
#include "filterdrvifmajor.h"
#include "volume.h"

namespace VolumeOps
{
	//GetTotalTrackedBytes
	int VolumeOperator::GetTotalTrackedBytes(DeviceStream& stream, const FilterDrvVol_t &sourceName, SV_ULONGLONG& ullTotalTrackedBytes)
	{
		DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
		int status = SV_FAILURE;
		ullTotalTrackedBytes = 0;

		long inBufLen = (long)MonStatInputCumulativeTrackedIOSizeMinSize;
		auto inBuffer = reinterpret_cast<PMONITORING_STATS_INPUT>(new (std::nothrow) byte[inBufLen]);

		if (nullptr == inBuffer)
		{
			DebugPrintf(SV_LOG_ERROR, "%s - Failed to allocate buffer of length : " LLSPEC "\n", FUNCTION_NAME, inBufLen);
			goto Cleanup;
		}

		RtlZeroMemory(inBuffer, inBufLen);
		inBuffer->Statistic = MONITORING_STATISTIC::CumulativeTrackedIOSize;
		size_t exclNullCpyLen = std::min(sizeof(inBuffer->CumulativeTrackedIOSize.DeviceId),
										 sizeof(wchar_t) * sourceName.size());
		RtlCopyMemory(inBuffer->CumulativeTrackedIOSize.DeviceId, sourceName.c_str(), exclNullCpyLen);

		MON_STAT_CUMULATIVE_TRACKED_IO_SIZE_OUTPUT outBuffer = {};

		status = stream.IoControl(IOCTL_INMAGE_GET_MONITORING_STATS, inBuffer, inBufLen, &outBuffer, sizeof(outBuffer));

		if (SV_SUCCESS != status)
			goto Cleanup;

		ullTotalTrackedBytes = outBuffer.ullTotalTrackedBytes;

	Cleanup:
		if (nullptr != inBuffer)
			delete[] inBuffer;

		DebugPrintf(SV_LOG_DEBUG, "EXITED %s with value " ULLSPEC "\n", FUNCTION_NAME, ullTotalTrackedBytes);
		return status;
	}

	int VolumeOperator::GetDroppedTagsCount(DeviceStream& stream, const FilterDrvVol_t &sourceName, SV_ULONGLONG& ullDroppedTagsCount)
	{
		DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
		int status = SV_FAILURE;
		ullDroppedTagsCount = 0;

		long inBufLen = (long)MonStatInputCumulativeDroppedTagsCountMinSize;
		auto inBuffer = reinterpret_cast<PMONITORING_STATS_INPUT>(new (std::nothrow) byte[inBufLen]);

		if (nullptr == inBuffer)
		{
			DebugPrintf(SV_LOG_ERROR, "%s - Failed to allocate buffer of length : " LLSPEC "\n", FUNCTION_NAME, inBufLen);
			goto Cleanup;
		}

		RtlZeroMemory(inBuffer, inBufLen);
		inBuffer->Statistic = MONITORING_STATISTIC::CumulativeDroppedTagsCount;
		size_t exclNullCpyLen = std::min(sizeof(inBuffer->CumulativeDroppedTagsCount.DeviceId),
										 sizeof(wchar_t) * sourceName.size());
		RtlCopyMemory(inBuffer->CumulativeDroppedTagsCount.DeviceId, sourceName.c_str(), exclNullCpyLen);

		MON_STAT_CUMULATIVE_DROPPED_TAGS_COUNT_OUTPUT outBuffer = {};

		status = stream.IoControl(IOCTL_INMAGE_GET_MONITORING_STATS, inBuffer, inBufLen, &outBuffer, sizeof(outBuffer));

		if (SV_SUCCESS != status)
			goto Cleanup;

		ullDroppedTagsCount = outBuffer.ullDroppedTagsCount;

	Cleanup:
		if (nullptr != inBuffer)
			delete[] inBuffer;

		DebugPrintf(SV_LOG_DEBUG, "EXITED %s with value " ULLSPEC "\n", FUNCTION_NAME, ullDroppedTagsCount);
		return status;
	}
}
