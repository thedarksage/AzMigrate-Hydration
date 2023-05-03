#include "filterdrvif.h"
#include "logger.h"


FilterDrvIf::FilterDrvIf()
{
}


SV_ULONGLONG FilterDrvIf::GetSrcStartOffset(const DeviceFilterFeatures *pdevicefilterfeatures, const VOLUME_SETTINGS &volumesettings)
{
    SV_ULONGLONG off = 0;
 
    if (pdevicefilterfeatures->IsMirrorSupported())
    {
        off = volumesettings.srcStartOffset;
    }

    return off;
}


void FilterDrvIf::CopyVolumeStats(const VOLUME_STATS_ADDITIONAL_INFO &vsai, VolumeStats &vs)
{
    vs.totalChangesPending = vsai.ullTotalChangesPending;
    vs.oldestChangeTimeStamp = vsai.ullOldestChangeTimeStamp;
    vs.driverCurrentTimeStamp = vsai.ullDriverCurrentTimeStamp;
}

void FilterDrvIf::PrintDrvVolumeStats(const VOLUME_STATS_ADDITIONAL_INFO &vsai)
{
    std::stringstream msg;
    msg << "total changes pending: " << vsai.ullTotalChangesPending << ", "
        << "oldest change timestamp: " << vsai.ullOldestChangeTimeStamp << ", "
        << "driver current time stamp: " << vsai.ullDriverCurrentTimeStamp;
    DebugPrintf(SV_LOG_DEBUG, "%s\n", msg.str().c_str());
}


