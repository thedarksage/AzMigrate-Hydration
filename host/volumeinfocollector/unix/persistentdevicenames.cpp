
#include "persistentdevicenames.h"

std::string GetDiskNameForPersistentDeviceName(const std::string& deviceName, 
                const VolumeSummaries_t& volumeSummaries)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    std::string diskName = deviceName;

    VolumeReporterImp vrimp;
    VolumeReporter::VolumeReport_t vr;
    vr.m_ReportLevel = VolumeReporter::ReportVolumeLevel;
    vrimp.GetVolumeReport(deviceName, volumeSummaries, vr);
    vrimp.PrintVolumeReport(vr);

    if (vr.m_bIsReported && !vr.m_DeviceName.empty())
    {
        DebugPrintf(SV_LOG_DEBUG, "%s for device name %s disk name is %s \n", FUNCTION_NAME, deviceName.c_str(), vr.m_DeviceName.c_str());
        diskName = vr.m_DeviceName;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "%s for device name %s GetVolumeReport failed.\n", FUNCTION_NAME, deviceName.c_str());
    }

    return diskName;
}

SVSTATUS GetVolumeReportForPersistentDeviceName(const std::string& deviceName,
    const VolumeSummaries_t& volumeSummaries,
    VolumeReporter::VolumeReport_t& vr)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    VolumeReporterImp vrimp;
    vr.m_ReportLevel = VolumeReporter::ReportVolumeLevel;
    vrimp.GetVolumeReport(deviceName, volumeSummaries, vr);
    vrimp.PrintVolumeReport(vr);

    if (vr.m_bIsReported && !vr.m_DeviceName.empty())
    {
        DebugPrintf(SV_LOG_DEBUG, "%s for device name %s disk name is %s \n", FUNCTION_NAME, deviceName.c_str(), vr.m_DeviceName.c_str());
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "%s for device name %s GetVolumeReport failed.\n", FUNCTION_NAME, deviceName.c_str());
        status = SVE_FAIL;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

