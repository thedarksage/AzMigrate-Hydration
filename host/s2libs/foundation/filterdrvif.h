#ifndef _FILTERDRV__IF__H_
#define _FILTERDRV__IF__H_

#include "volumegroupsettings.h"
#include "devicefilterfeatures.h"

#ifdef SV_WINDOWS
#include <windows.h>
#include <winioctl.h>
#include <atlbase.h>
#endif
#include "devicefilter.h"

class FilterDrvIf
{
public:
    FilterDrvIf();
    SV_ULONGLONG GetSrcStartOffset(const DeviceFilterFeatures *pdevicefilterfeatures, const VOLUME_SETTINGS &volumesettings);
    void CopyVolumeStats(const VOLUME_STATS_ADDITIONAL_INFO &vsai, VolumeStats &vs);
    void PrintDrvVolumeStats(const VOLUME_STATS_ADDITIONAL_INFO &vsai);
};

#endif /* _FILTERDRV__IF__H_ */

