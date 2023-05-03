#include <cstring>
#include <string>
#include <sstream>

#include <boost/shared_array.hpp>
#include <boost/lexical_cast.hpp>

#include "filterdrvifmajor.h"
#include "logger.h"
#include "volumeinfocollector.h"
#include "volumereporterimp.h"
#include "filterdrvifminor.h"
#include "inmsafecapis.h"

FilterDrvIfMajor::FilterDrvIfMajor()
{
}


bool FilterDrvIfMajor::FillInmDevInfo(const DeviceFilterFeatures *pinvolfltfeatures, const VolumeSummaries_t *pvolumesummaries,
                                      const VOLUME_SETTINGS &volumesettings, inm_dev_info_t &devinfo, 
                                      VolumeReporter::VolumeReport_t &vr)
{
    bool bfilled = false;

    memset(&devinfo, 0, sizeof devinfo);
    bool bfabric = volumesettings.isFabricVolume();
    devinfo.d_type = bfabric?FILTER_DEV_FABRIC_LUN:FILTER_DEV_HOST_VOLUME;
    devinfo.d_guid[INM_GUID_LEN_MAX - 1] = '\0';
    devinfo.d_mnt_pt[INM_PATH_MAX - 1] = '\0';

    if (pinvolfltfeatures->IsMirrorSupported() && !bfabric)
    {
        bfilled = FillVolumeAttrs(volumesettings.deviceName.c_str(), pvolumesummaries, devinfo, vr);
    }
    else
    {
        bfilled = true;
        devinfo.d_flags = 0;
    }

    const char *src = bfabric ? volumesettings.sanVolumeInfo.virtualName.c_str():
                      (vr.m_bIsReported && !vr.m_DeviceName.empty()) ? vr.m_DeviceName.c_str() : volumesettings.deviceName.c_str();
    inm_strncpy_s(devinfo.d_guid, ARRAYSIZE(devinfo.d_guid), src, INM_GUID_LEN_MAX - 1);
    inm_strncpy_s(devinfo.d_mnt_pt,ARRAYSIZE(devinfo.d_mnt_pt), volumesettings.mountPoint.c_str(), INM_PATH_MAX - 1);
    devinfo.d_bsize = VOL_BSIZE;
    devinfo.d_nblks = volumesettings.sourceCapacity / devinfo.d_bsize;

    return bfilled;
}

bool FilterDrvIfMajor::FillInmDevInfo(inm_dev_info_t &devinfo, 
                    const VolumeReporter::VolumeReport_t &vr)
{
    bool bfilled = false;
    memset(&devinfo, 0, sizeof devinfo);
    devinfo.d_type = FILTER_DEV_HOST_VOLUME;
    devinfo.d_guid[INM_GUID_LEN_MAX - 1] = '\0';
    devinfo.d_mnt_pt[INM_PATH_MAX - 1] = '\0';
    inm_strncpy_s(devinfo.d_guid, ARRAYSIZE(devinfo.d_guid), vr.m_DeviceName.c_str(), INM_GUID_LEN_MAX - 1);
    inm_strncpy_s(devinfo.d_mnt_pt,ARRAYSIZE(devinfo.d_mnt_pt), vr.m_Mountpoint.c_str(), INM_PATH_MAX - 1);
    devinfo.d_bsize = VOL_BSIZE;
    devinfo.d_nblks = vr.m_Size/ devinfo.d_bsize;
    if (VolumeSummary::DISK == vr.m_VolumeType)
        devinfo.d_flags |= FULL_DISK_FLAG;
    else
        devinfo.d_flags = 0;
    bfilled = true;
    return bfilled;
}

void FilterDrvIfMajor::PrintInmDevInfo(const inm_dev_info_t &devinfo)
{
    std::stringstream msg;
    msg << "inm_dev_info_t members:\n"
        << "devinfo.d_guid = " << devinfo.d_guid << ", "
        << "devinfo.d_mnt_pt = " << devinfo.d_mnt_pt << ", "
        << "devinfo.d_type = " << devinfo.d_type << ", "
        << "devinfo.d_bsize = " << devinfo.d_bsize << ", "
        << "devinfo.d_nblks = " << devinfo.d_nblks << ", "
        << "devinfo.d_flags = " << devinfo.d_flags;
    DebugPrintf(SV_LOG_DEBUG, "%s\n", msg.str().c_str());
}


bool FilterDrvIfMajor::FillVolumeAttrs(const char *pdev, const VolumeSummaries_t *pvolumesummaries, inm_dev_info_t &devinfo, VolumeReporter::VolumeReport_t &vr)
{
    VolumeReporter *pvrtr = new VolumeReporterImp();
    vr.m_ReportLevel = VolumeReporter::ReportVolumeLevel;
    pvrtr->GetVolumeReport(pdev, *pvolumesummaries, vr);
    pvrtr->PrintVolumeReport(vr);
    delete pvrtr;

    if (vr.m_bIsReported)
    {
        if (VolumeSummary::DISK == vr.m_VolumeType)
        {
            devinfo.d_flags |= FULL_DISK_FLAG;
        }
        else if (VolumeSummary::DISK_PARTITION == vr.m_VolumeType)
        {
            devinfo.d_flags |= FULL_DISK_PARTITION_FLAG;
        }
    
        if (vr.m_bIsMultipathNode)
        {
            devinfo.d_flags |= INM_IS_DEVICE_MULTIPATH;
        }
    
        if (VolumeSummary::SMI == vr.m_FormatLabel)
        {
            devinfo.d_flags |= FULL_DISK_LABEL_VTOC;
        }
        else if (VolumeSummary::EFI == vr.m_FormatLabel)
        {
            devinfo.d_flags |= FULL_DISK_LABEL_EFI;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "source volume %s is not collected by volumeinfocollector\n", pdev);
    }

    return vr.m_bIsReported;
}

int FilterDrvIfMajor::FillVolumeStats(const FilterDrvVol_t &volume, DeviceStream &devstream, VolumeStats &vs)
{
    //DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    VOLUME_STATS_ADDITIONAL_INFO vsai;
    memset(&vsai, 0, sizeof vsai);
    inm_strncpy_s(vsai.VolumeGuid.volume_guid,ARRAYSIZE(vsai.VolumeGuid.volume_guid), volume.c_str(), sizeof vsai.VolumeGuid.volume_guid - 1);
    int iStatus = devstream.IoControl(IOCTL_INMAGE_GET_ADDITIONAL_VOLUME_STATS, &vsai, sizeof vsai);
    if (SV_SUCCESS != iStatus)
    {
        DebugPrintf(SV_LOG_ERROR, "ioctl to get additional stats failed for volume %s\n", volume.c_str());
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "For %s, the volume stats are:\n", volume.c_str());
        PrintDrvVolumeStats(vsai);
        CopyVolumeStats(vsai, vs);
    }
    //DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return iStatus;
}


bool FilterDrvIfMajor::GetVolumeTrackingSize(const std::string &devicename, const int &devicetype, DeviceStream *pDeviceStream, SV_ULONGLONG &size, std::string &reasonifcant)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with volume name %s\n",FUNCTION_NAME, devicename.c_str());

    SV_ULONGLONG blksz, nblks;
    bool got = GetVolumeBlockSize(devicename, pDeviceStream, blksz, reasonifcant) && 
               GetNumberOfBlocksInVolume(devicename, pDeviceStream, nblks, reasonifcant);
    if (got)
        size = blksz * nblks;
    else
        reasonifcant += ". Hence could not find volume tracking size at filter driver.";

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return got;
}


bool FilterDrvIfMajor::DoesVolumeTrackingSizeMatch(const int &devicetype, const SV_ULONGLONG &trackingsize, const SV_ULONGLONG &capacity, const SV_ULONGLONG &rawsize)
{
	return (trackingsize == capacity);
}


/* TODO: call simply stopfiltering method which has to be implemented */
bool FilterDrvIfMajor::DeleteVolumeTrackingSize(const std::string &devicename, const int &devicetype, DeviceStream *pDeviceStream, std::string &reasonifcant)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with device name %s\n",FUNCTION_NAME, devicename.c_str());

	bool deleted = (SV_SUCCESS == pDeviceStream->IoControl(IOCTL_INMAGE_STOP_FILTERING_DEVICE, (void*)devicename.c_str(), devicename.size(), NULL, 0));
	if (!deleted) {
        int oserrcode = Error::UserToOS(pDeviceStream->GetErrorCode());
		std::stringstream ss;
		ss << "Failed to delete tracking volume size from filter driver, for volume " << devicename 
           << ", with error " << Error::Msg(oserrcode);
		reasonifcant = ss.str();
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return deleted;
}


bool FilterDrvIfMajor::GetVolumeBlockSize(const std::string &devicename, DeviceStream *pDeviceStream, SV_ULONGLONG &blksz, std::string &reasonifcant)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with volume name %s\n",FUNCTION_NAME, devicename.c_str());
    bool got = true;
    FilterDrvIfMinor drvifminor;

    boost::shared_array<char> pbuffer((char *)calloc(GET_SET_ATTR_BUF_LEN, 1),free);
    int attrerr;
    if (pbuffer) {
        if (GetVolumeAttribute(devicename, pDeviceStream, pbuffer.get(), GET_SET_ATTR_BUF_LEN, VolumeBsize, attrerr)) {
            DebugPrintf(SV_LOG_DEBUG, "block size as string from filter driver = %s\n", pbuffer.get());
            blksz = strlen(pbuffer.get()) ?  (boost::lexical_cast<SV_ULONGLONG>(pbuffer.get())) : 0;
            DebugPrintf(SV_LOG_DEBUG, "numeric blocksize = " ULLSPEC "\n", blksz);
        } else if (drvifminor.GetVolumeBlockSize(devicename, pDeviceStream, blksz, attrerr, reasonifcant)) {
            DebugPrintf(SV_LOG_DEBUG, "block size got from filter driver minor interface = " ULLSPEC "\n", blksz);
        } else 
            got = false;
    } else {
        std::stringstream ss;
        ss << "Failed to allocate " << GET_SET_ATTR_BUF_LEN << " bytes to find block size at filter driver for volume " << devicename;
        reasonifcant = ss.str();
        got = false;
    }

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return got;
}


bool FilterDrvIfMajor::GetNumberOfBlocksInVolume(const std::string &devicename, DeviceStream *pDeviceStream, SV_ULONGLONG &nblks, std::string &reasonifcant)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with volume name %s\n",FUNCTION_NAME, devicename.c_str());
    bool got = true;
    FilterDrvIfMinor drvifminor;

    boost::shared_array<char> pbuffer((char *)calloc(GET_SET_ATTR_BUF_LEN, 1),free);
    int attrerr;
    if (pbuffer) {
        if (GetVolumeAttribute(devicename, pDeviceStream, pbuffer.get(), GET_SET_ATTR_BUF_LEN, VolumeNblks, attrerr)) {
            DebugPrintf(SV_LOG_DEBUG, "number of blocks as string from filter driver = %s\n", pbuffer.get());
            nblks = strlen(pbuffer.get()) ?  (boost::lexical_cast<SV_ULONGLONG>(pbuffer.get())) : 0;
            DebugPrintf(SV_LOG_DEBUG, "number of blocks as numeric = " ULLSPEC "\n", nblks);
        } else if (drvifminor.GetNumberOfBlocksInVolume(devicename, pDeviceStream, nblks, attrerr, reasonifcant)) {
            DebugPrintf(SV_LOG_DEBUG, "number of blocks got from filter driver minor interface = " ULLSPEC "\n", nblks);
        } else 
            got = false;
    } else {
        std::stringstream ss;
        ss << "Failed to allocate " << GET_SET_ATTR_BUF_LEN << " bytes to find number of blocks at filter driver for volume " << devicename;
        reasonifcant = ss.str();
        got = false;
    }

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return got;
}


bool FilterDrvIfMajor::GetVolumeAttribute(const std::string &devicename, DeviceStream *pDeviceStream, char *buf, 
                                          const size_t &buflen, const unsigned int &attribute, int &errorcode)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    inm_attribute_t ia;
    memset(&ia, 0, sizeof ia);
    inm_strcpy_s(ia.guid.volume_guid,ARRAYSIZE(ia.guid.volume_guid), devicename.c_str());
    ia.bufp = buf;
    ia.buflen = buflen;
    ia.why = GET_ATTR;
    ia.type = attribute;
    std::stringstream ss;
    PrintInmAttributeToStream(ia, ss);
    DebugPrintf(SV_LOG_DEBUG, "%s\n", ss.str().c_str());
    bool got = (SV_SUCCESS == pDeviceStream->IoControl(IOCTL_INMAGE_GET_SET_ATTR, &ia, sizeof ia));
    if (!got)
        errorcode = pDeviceStream->GetErrorCode();
    
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return got;
}


void FilterDrvIfMajor::PrintInmAttributeToStream(const inm_attribute_t &ia, std::ostream &o)
{
    o << "inm_attribute_t: "
      << "volume = " << ia.guid.volume_guid 
      << ", type = " << ia.type
      << ", request = " << ia.why
      << ", buflen = " << ia.buflen;
}


std::string FilterDrvIfMajor::GetDriverName(const int &deviceType)
{
	//Unix: use involflt
	return INMAGE_FILTER_DEVICE_NAME;
}


FilterDrvVol_t FilterDrvIfMajor::GetFormattedDeviceNameForDriverInput(const std::string &deviceName, const int &deviceType)
{
	//Unix: device name itself is input
	return FilterDrvVol_t(deviceName.begin(), deviceName.end());
}

bool FilterDrvIfMajor::GetDriverVersion(DeviceStream *pDeviceStream, DRIVER_VERSION &stVersion)
{
    int iStatus = pDeviceStream->IoControl(IOCTL_INMAGE_GET_DRIVER_VERSION, &stVersion);
    if (SV_SUCCESS != iStatus)
    {
        iStatus = pDeviceStream->GetErrorCode();
        DebugPrintf(SV_LOG_ERROR, "IOCTL_INMAGE_GET_VERSION failed: %d\n", iStatus);
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "%s Major Version: %d\n Minor Version: %d\n Minor Version2: %d\n Minor Version3:%d\n",
                    FUNCTION_NAME, stVersion.ulDrMajorVersion, stVersion.ulDrMinorVersion,
                    stVersion.ulDrMinorVersion2, stVersion.ulDrMinorVersion3);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return (iStatus == SV_SUCCESS);
}

bool FilterDrvIfMajor::GetDriverStats(const std::string &devicename,
    const int &devicetype,
    DeviceStream *pDeviceStream,
    void* stats,
    int   statslen)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s with volume name %s\n", FUNCTION_NAME, devicename.c_str());

    FilterDrvVol_t fmtddevicename = GetFormattedDeviceNameForDriverInput(devicename, devicetype);
    unsigned long ioctlCode = IOCTL_INMAGE_GET_VOLUME_STATS_V2;

    DebugPrintf(SV_LOG_DEBUG,
                    "ioctlCode = %lu, devicename = %S, driver name = %s.\n",
                    ioctlCode,
                    fmtddevicename.c_str(),
                    pDeviceStream->GetName().c_str());

    TELEMETRY_VOL_STATS *telemetryStats = (TELEMETRY_VOL_STATS *)stats;

    inm_strcpy_s(telemetryStats->vol_stats.VolumeGUID, ARRAYSIZE(telemetryStats->vol_stats.VolumeGUID), devicename.c_str());

    int iStatus = pDeviceStream->IoControl(ioctlCode, stats);
    if (SV_SUCCESS != iStatus)
    {
        iStatus = pDeviceStream->GetErrorCode();
        DebugPrintf(SV_LOG_ERROR, "IOCTL_INMAGE_GET_VOLUME_STATS_V2 failed: %d\n", iStatus);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return (iStatus == SV_SUCCESS);
}

bool FilterDrvIfMajor::GetCxStats(DeviceStream *pDeviceStream,
    void *pCxNotify,
    uint32_t ulInputSize)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    unsigned long ioctlCode = IOCTL_INMAGE_GET_CXSTATS_NOTIFY;
    int iStatus = pDeviceStream->IoControl(ioctlCode, pCxNotify, ulInputSize);

    if (SV_SUCCESS != iStatus)
    {
        iStatus = pDeviceStream->GetErrorCode();
        DebugPrintf(SV_LOG_ERROR, "IOCTL_INMAGE_GET_CXSTATS_NOTIFY failed: %d\n", Error::UserToOS(iStatus));
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: IOCTL_INMAGE_GET_CXSTATS_NOTIFY completed.\n",
            FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return (iStatus == SV_SUCCESS);
}

bool FilterDrvIfMajor::CancelCxStats(DeviceStream *pDeviceStream)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    unsigned long ioctlCode = IOCTL_INMAGE_WAKEUP_GET_CXSTATS_NOTIFY_THREAD;
    int iStatus = pDeviceStream->IoControl(ioctlCode);

    if (SV_SUCCESS != iStatus)
    {
        iStatus = pDeviceStream->GetErrorCode();
        DebugPrintf(SV_LOG_ERROR, "IOCTL_INMAGE_WAKEUP_GET_CXSTATS_NOTIFY_THREAD failed: %d\n", Error::UserToOS(iStatus));
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: IOCTL_INMAGE_WAKEUP_GET_CXSTATS_NOTIFY_THREAD completed.\n",
            FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return (iStatus == SV_SUCCESS);
}

bool FilterDrvIfMajor::NotifyTagCommit(DeviceStream *pDeviceStream,
    void *pCxNotify,
    uint32_t ulInputSize)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    unsigned long ioctlCode = IOCTL_INMAGE_TAG_DRAIN_NOTIFY;
    int iStatus = pDeviceStream->IoControl(ioctlCode, pCxNotify, ulInputSize);

    if (SV_SUCCESS != iStatus)
    {
        iStatus = pDeviceStream->GetErrorCode();
        DebugPrintf(SV_LOG_ERROR, "IOCTL_INMAGE_TAG_DRAIN_NOTIFY failed: %d\n", Error::UserToOS(iStatus));
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: IOCTL_INMAGE_TAG_DRAIN_NOTIFY completed.\n",
            FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return (iStatus == SV_SUCCESS);
}

bool FilterDrvIfMajor::CancelTagCommitNotify(DeviceStream *pDeviceStream)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    unsigned long ioctlCode = IOCTL_INMAGE_WAKEUP_TAG_DRAIN_NOTIFY_THREAD;
    int iStatus = pDeviceStream->IoControl(ioctlCode);

    if (SV_SUCCESS != iStatus)
    {
        iStatus = pDeviceStream->GetErrorCode();
        DebugPrintf(SV_LOG_ERROR, "IOCTL_INMAGE_WAKEUP_TAG_DRAIN_NOTIFY_THREAD failed: %d\n", Error::UserToOS(iStatus));
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: IOCTL_INMAGE_WAKEUP_TAG_DRAIN_NOTIFY_THREAD completed.\n",
            FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return (iStatus == SV_SUCCESS);
}

bool FilterDrvIfMajor::UnblockDrain(DeviceStream *pDeviceStream,
    void *unblockDrainBuffer,
    uint32_t ulInputSize)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    unsigned long ioctlCode = IOCTL_INMAGE_SET_DRAIN_STATE;
    int iStatus = pDeviceStream->IoControl(ioctlCode, unblockDrainBuffer, ulInputSize);

    if (SV_SUCCESS != iStatus)
    {
        iStatus = pDeviceStream->GetErrorCode();
        DebugPrintf(SV_LOG_ERROR, "IOCTL_INMAGE_SET_DRAIN_STATE failed: %d\n",
            Error::UserToOS(iStatus));
    }
    else
    {
        DebugPrintf(SV_LOG_ALWAYS, "%s: IOCTL_INMAGE_SET_DRAIN_STATE completed.\n",
            FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return (iStatus == SV_SUCCESS);
}

bool FilterDrvIfMajor::GetDrainState(DeviceStream *pDeviceStream,
    void *drainStateBuffer,
    uint32_t ulInputSize)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    unsigned long ioctlCode = IOCTL_INMAGE_GET_DRAIN_STATE;
    int iStatus = pDeviceStream->IoControl(ioctlCode, drainStateBuffer, ulInputSize);

    if (SV_SUCCESS != iStatus)
    {
        iStatus = pDeviceStream->GetErrorCode();
        DebugPrintf(SV_LOG_ERROR, "IOCTL_INMAGE_GET_DRAIN_STATE failed: %d\n",
            Error::UserToOS(iStatus));
    }
    else
    {
        DebugPrintf(SV_LOG_ALWAYS, "%s: IOCTL_INMAGE_GET_DRAIN_STATE completed.\n",
            FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return (iStatus == SV_SUCCESS);
}

bool FilterDrvIfMajor::ModifyPersistentDeviceName(DeviceStream *pDeviceStream,
    void *modifyPnameBuffer,
    uint32_t ulInputSize)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    unsigned long ioctlCode = IOCTL_INMAGE_MODIFY_PERSISTENT_DEVICE_NAME;
    int iStatus = pDeviceStream->IoControl(ioctlCode, modifyPnameBuffer, ulInputSize);

    if (SV_SUCCESS != iStatus)
    {
        iStatus = pDeviceStream->GetErrorCode();
        DebugPrintf(SV_LOG_ERROR, "IOCTL_INMAGE_MODIFY_PERSISTENT_DEVICE_NAME failed: %d\n",
            Error::UserToOS(iStatus));
    }
    else
    {
        DebugPrintf(SV_LOG_ALWAYS, "%s: IOCTL_INMAGE_MODIFY_PERSISTENT_DEVICE_NAME completed.\n",
            FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return (iStatus == SV_SUCCESS);
}

bool FilterDrvIfMajor::GetVolNameMap(DeviceStream *pDeviceStream,
    void *volNameMapBuffer,
    uint32_t ulInputSize)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    unsigned long ioctlCode = IOCTL_INMAGE_NAME_MAPPING;
    int iStatus = pDeviceStream->IoControl(ioctlCode, volNameMapBuffer, ulInputSize);

    if (SV_SUCCESS != iStatus)
    {
        iStatus = pDeviceStream->GetErrorCode();
        DebugPrintf(SV_LOG_ERROR, "IOCTL_INMAGE_NAME_MAPPING failed: %d\n",
            Error::UserToOS(iStatus));
    }
    else
    {
        DebugPrintf(SV_LOG_ALWAYS, "%s: IOCTL_INMAGE_NAME_MAPPING completed.\n",
            FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return (iStatus == SV_SUCCESS);
}
