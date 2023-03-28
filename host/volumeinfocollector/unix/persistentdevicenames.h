#ifndef _PERSISTENT_DEVICE_NAMES_H_
#define _PERSISTENT_DEVICE_NAMES_H_

#include <string>
#include "svstatus.h"
#include "volumereporterimp.h"

std::string GetDiskNameForPersistentDeviceName(const std::string& deviceName, const VolumeSummaries_t& volumeSummaries);

SVSTATUS GetVolumeReportForPersistentDeviceName(const std::string& deviceName,
	const VolumeSummaries_t& volumeSummaries,
	VolumeReporter::VolumeReport_t& vr);

#endif
