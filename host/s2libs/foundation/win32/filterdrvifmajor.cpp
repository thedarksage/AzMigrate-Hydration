#include <cstring>
#include <string>
#include <sstream>
#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include "filterdrvifmajor.h"
#include "logger.h"
#include "devicestream.h"
#include "inmsafecapis.h"

#include "volumereporter.h"
#include "volumereporterimp.h"


FilterDrvIfMajor::FilterDrvIfMajor()
{
}


int FilterDrvIfMajor::FillVolumeStats(const FilterDrvVol_t &volume, DeviceStream &devstream, VolumeStats &vs)
{
    VOLUME_STATS_ADDITIONAL_INFO vsai;

    //DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    std::string s(volume.begin(), volume.end());
    memset(&vsai, 0, sizeof vsai);
	unsigned long ioctlCode = (INMAGE_DISK_FILTER_DOS_DEVICE_NAME == devstream.GetName()) ? IOCTL_INMAGE_GET_ADDITIONAL_DEVICE_STATS : IOCTL_INMAGE_GET_ADDITIONAL_VOLUME_STATS;
    int iStatus = devstream.IoControl(ioctlCode,
                                    static_cast<void*>(const_cast<wchar_t*>(volume.c_str())),
                                    sizeof(wchar_t) * volume.length(),
                                    &vsai,
                                    (long)sizeof vsai  ) ;
    if (SV_SUCCESS != iStatus)
    {
        DebugPrintf(SV_LOG_ERROR, "ioctl to get additional stats failed for volume %s\n", s.c_str());
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "For %s, the volume stats are:\n", s.c_str());
        PrintDrvVolumeStats(vsai);
        CopyVolumeStats(vsai, vs);
    }

    //DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return iStatus;
}

/* Fills output parameter pTime with the time of the driver pointed to by first param devstream */
int FilterDrvIfMajor::getDriverTime(DeviceStream &devStream, PDRIVER_GLOBAL_TIMESTAMP pTime)
{
  memset(pTime, 0, sizeof(DRIVER_GLOBAL_TIMESTAMP));
  int iStatus = devStream.IoControl(IOCTL_INMAGE_GET_GLOBAL_TIMESTAMP, NULL, 0, pTime, sizeof(DRIVER_GLOBAL_TIMESTAMP));

  if (SV_SUCCESS != iStatus)
  {
    iStatus = devStream.GetErrorCode();
    DebugPrintf(SV_LOG_ERROR, "IOCTL_INMAGE_GET_GLOBAL_TIMESTAMP failed: %d\n", Error::UserToOS(iStatus));
  }
  return iStatus;
}


bool FilterDrvIfMajor::GetVolumeTrackingSize(const std::string &devicename, const int &devicetype, DeviceStream *pDeviceStream, SV_ULONGLONG &size, std::string &reasonifcant)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with volume name %s\n",FUNCTION_NAME, devicename.c_str());

	ULARGE_INTEGER ulsize;
    FilterDrvVol_t fmtddevicename = GetFormattedDeviceNameForDriverInput(devicename, devicetype);
	unsigned long ioctlCode = (INMAGE_DISK_FILTER_DOS_DEVICE_NAME == pDeviceStream->GetName()) ? IOCTL_INMAGE_GET_DEVICE_TRACKING_SIZE : IOCTL_INMAGE_GET_VOLUME_SIZE;
    DebugPrintf(SV_LOG_DEBUG, "ioctlCode = %lu, devicename = %S, driver name = %s.\n", ioctlCode, fmtddevicename.c_str(), pDeviceStream->GetName().c_str());

    ulsize.QuadPart = 0;    /* TODO: is this for safe case, when driver succeeds, but does not modify ulsize ? */
	bool got = (SV_SUCCESS == pDeviceStream->IoControl(ioctlCode, static_cast<void *>(const_cast<wchar_t *>(fmtddevicename.c_str())), 
                                                       sizeof(wchar_t) * fmtddevicename.length(),
                                                       &ulsize, sizeof ulsize));
	if (got) {
		/*
		INFO: TODO: Capture in document
		1. The above IOCTL does not calculate the size..It would fill the calculated size while opening bitmap.
		2. If a volume is never protected, volume size woudl be '0' and ioctl will succeed.
		3. If a volume is protected at least one time, it would be first time calculated one..Any resize does not change the bitmap/volume size.
		*/
		size = ulsize.QuadPart;
	} else {
		std::stringstream ss;
        int oserrcode = Error::UserToOS(pDeviceStream->GetErrorCode());
		ss << "Failed to get tracking volume size from filter driver, for volume " << devicename 
           << ", with error " << Error::Msg(oserrcode);
		reasonifcant = ss.str();
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return got;
}


bool FilterDrvIfMajor::DoesVolumeTrackingSizeMatch(const int &devicetype, const SV_ULONGLONG &trackingsize, const SV_ULONGLONG &capacity, const SV_ULONGLONG &rawsize)
{
	//For disk, compare against capacity
	SV_ULONGLONG cmpSize = (VolumeSummary::DISK == devicetype) ? capacity : rawsize;
	return (trackingsize == cmpSize);
}


/* TODO: call simply stopfiltering method which has to be implemented; 
 * Just take input as flags or bool, enum etc, for delete bitmap flags etc */
bool FilterDrvIfMajor::DeleteVolumeTrackingSize(const std::string &devicename, const int &devicetype, DeviceStream *pDeviceStream, std::string &reasonifcant)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with device name %s\n",FUNCTION_NAME, devicename.c_str());

	STOP_FILTERING_INPUT sfi;
	memset(&sfi, 0, sizeof sfi);
	FilterDrvVol_t fmtddevicename = GetFormattedDeviceNameForDriverInput(devicename, devicetype);
	inm_wmemcpy_s(sfi.VolumeGUID, NELEMS(sfi.VolumeGUID), fmtddevicename.c_str(), fmtddevicename.length());
	sfi.ulFlags |= STOP_FILTERING_FLAGS_DELETE_BITMAP;
	bool deleted = (SV_SUCCESS == pDeviceStream->IoControl(IOCTL_INMAGE_STOP_FILTERING_DEVICE, &sfi, sizeof(sfi), NULL, 0));
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

std::string FilterDrvIfMajor::GetDriverName(const int &deviceType)
{
	//For windows: used InDskFlt for disk type; Use InVolFlt for all other cases
	return (VolumeSummary::DISK == deviceType) ? INMAGE_DISK_FILTER_DOS_DEVICE_NAME : INMAGE_FILTER_DEVICE_NAME;
}


FilterDrvVol_t FilterDrvIfMajor::GetFormattedDeviceNameForDriverInput(const std::string &deviceName, const int &deviceType)
{
	std::string formattedName(deviceName);
	//For windows volumes, convert to GUID
	if (VolumeSummary::DISK != deviceType) {
		FormatVolumeNameToGuid(formattedName);
		ExtractGuidFromVolumeName(formattedName);
	}
	else {
		//For windows disk, remove {} for gpt guid
		Trim(formattedName, "{}");
	}

	return FilterDrvVol_t(formattedName.begin(), formattedName.end());
}


bool FilterDrvIfMajor::FillStartFilteringInput(const FilterDrvVol_t &driverInputDeviceName, 
	const DeviceFilterFeatures *pDeviceFilterFeatures, 
	const VolumeSummaries_t *pvolumesummaries, 
	const VOLUME_SETTINGS &volumesettings, 
	START_FILTERING_INPUT &sfi
	)
{
	//Get device information from vic cache
	VolumeReporter::VolumeReport_t vr;
	vr.m_ReportLevel = VolumeReporter::ReportVolumeLevel;
	
	VolumeReporterImp vrimp;
	vrimp.GetVolumeReport(volumesettings.deviceName, *pvolumesummaries, vr);
	vrimp.PrintVolumeReport(vr);

    return FillStartFilteringInput(driverInputDeviceName, vr, sfi);
}
bool FilterDrvIfMajor::FillStartFilteringInput(const FilterDrvVol_t &driverInputDeviceName,
    const VolumeReporter::VolumeReport_t &vr,
    START_FILTERING_INPUT &sfi
    )
{
	if (vr.m_bIsReported)
	{
		//Fill device id
		inm_wmemcpy_s(sfi.DeviceId, NELEMS(sfi.DeviceId), driverInputDeviceName.c_str(), driverInputDeviceName.length());

		//Fill capacity
        sfi.DeviceSize.QuadPart = vr.m_Size;

		//Fill label
		if (VolumeSummary::MBR == vr.m_FormatLabel)
			sfi.ePartStyle = ePartStyleMBR;
		else if (VolumeSummary::GPT == vr.m_FormatLabel)
			sfi.ePartStyle = ePartStyleGPT;
		else
			sfi.ePartStyle = ePartStyleUnknown;

		//Fill multipath
		if (vr.m_bIsMultipathNode)
			sfi.ulFlags |= IS_MULTIPATH_DISK;

		//Fill is_shared
		if (vr.m_IsShared)
			sfi.ulFlags |= IS_CLUSTERED_DISK;

		//Fill basic / dynamic storage type
		if (NsVolumeAttributes::BASIC == vr.m_StorageType)
			sfi.ulFlags |= IS_BASIC_DISK;
		else if (NsVolumeAttributes::DYNAMIC == vr.m_StorageType)
			sfi.ulFlags |= IS_DYNAMIC_DISK;
	}
	else
	{
		//vic did not collect. log error
        DebugPrintf(SV_LOG_ERROR,
            "source volume %s is not collected by volumeinfocollector\n",
            vr.m_DeviceName.c_str());
	}

	return vr.m_bIsReported;
}

void FilterDrvIfMajor::PrintStartFilteringInput(const START_FILTERING_INPUT &sfi)
{
	DebugPrintf(SV_LOG_DEBUG, "START_FILTERING_INPUT: \n");
	std::wstring wsdevid(sfi.DeviceId, GUID_SIZE_IN_CHARS);
	DebugPrintf(SV_LOG_DEBUG, "DeviceId: %S\n", wsdevid.c_str());
	std::stringstream ss;
	ss << "DeviceSize = " << sfi.DeviceSize.QuadPart
		<< ", Partition Style = " << sfi.ePartStyle
		<< ", ulFlags = " << std::hex << sfi.ulFlags;
	DebugPrintf(SV_LOG_DEBUG, "%s\n", ss.str().c_str());
}

bool FilterDrvIfMajor::GetDriverStats(const std::string &devicename,
    const int &devicetype,
    DeviceStream *pDeviceStream,
    void* stats,
    DWORD statsLen)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s with volume name %s\n", FUNCTION_NAME, devicename.c_str());

    FilterDrvVol_t fmtddevicename = GetFormattedDeviceNameForDriverInput(devicename, devicetype);
    unsigned long ioctlCode = IOCTL_INMAGE_GET_VOLUME_STATS;

    DebugPrintf(SV_LOG_DEBUG,
        "ioctlCode = %lu, devicename = %S, driver name = %s.\n",
        ioctlCode,
        fmtddevicename.c_str(),
        pDeviceStream->GetName().c_str());

    int iStatus = pDeviceStream->IoControl(ioctlCode,
        static_cast<void *>(const_cast<wchar_t *>(fmtddevicename.c_str())),
        sizeof(wchar_t) * fmtddevicename.length(),
         stats,
         statsLen);

    if (SV_SUCCESS != iStatus)
    {
        iStatus = pDeviceStream->GetErrorCode();
        DebugPrintf(SV_LOG_ERROR, "IOCTL_INMAGE_GET_VOLUME_STATS failed: %d\n", Error::UserToOS(iStatus));
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return (iStatus == SV_SUCCESS);
}

bool FilterDrvIfMajor::GetDriverVersion(DeviceStream *pDeviceStream, DRIVER_VERSION &stVersion)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    unsigned long ioctlCode = IOCTL_INMAGE_GET_VERSION;
    int iStatus = pDeviceStream->IoControl(ioctlCode,
        NULL,
        0,
        &stVersion,
        sizeof(stVersion));

    if (SV_SUCCESS != iStatus)
    {
        iStatus = pDeviceStream->GetErrorCode();
        DebugPrintf(SV_LOG_ERROR, "IOCTL_INMAGE_GET_VERSION failed: %d\n", Error::UserToOS(iStatus));
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

bool FilterDrvIfMajor::GetCxStats(DeviceStream *pDeviceStream,
    GET_CXFAILURE_NOTIFY *pCxNotify,
    uint32_t ulInputSize,
    VM_CXFAILURE_STATS *pCxStats,
    uint32_t ulOutputSize)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    unsigned long ioctlCode = IOCTL_INMAGE_GET_CXSTATS_NOTIFY;
    int iStatus = pDeviceStream->IoControl(ioctlCode,
        pCxNotify,
        ulInputSize,
        pCxStats,
        ulOutputSize);

    if (SV_SUCCESS != iStatus)
    {
        DebugPrintf(SV_LOG_ERROR, "IOCTL_INMAGE_GET_CXSTATS_NOTIFY failed: %d\n", Error::UserToOS(iStatus));
    }
    else
    {
        int iStatusEx = pDeviceStream->GetErrorCode();
        if (iStatusEx == ERR_IO_PENDING)
        {
            DebugPrintf(SV_LOG_DEBUG, "IOCTL_INMAGE_GET_CXSTATS_NOTIFY is pending\n");
            iStatus = pDeviceStream->WaitForEvent(INFINITE);
            DebugPrintf(SV_LOG_DEBUG, "IOCTL_INMAGE_GET_CXSTATS_NOTIFY is completed\n");
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "IOCTL_INMAGE_GET_CXSTATS_NOTIFY returned immediately\n");
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return (iStatus == SV_SUCCESS);
}

bool FilterDrvIfMajor::CancelCxStats(DeviceStream *pDeviceStream)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    int iStatus = pDeviceStream->CancelIOEx();

    if (SV_SUCCESS != iStatus)
    {
        DebugPrintf(SV_LOG_ERROR, "cancel of IOCTL_INMAGE_GET_CXSTATS_NOTIFY failed: %d\n", Error::UserToOS(iStatus));
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return (iStatus == SV_SUCCESS);
}

bool FilterDrvIfMajor::NotifyTagCommit(DeviceStream *pDeviceStream,
    TAG_COMMIT_NOTIFY_INPUT *pTagCommitNotifyInput,
    uint32_t ulInputSize,
    TAG_COMMIT_NOTIFY_OUTPUT *pTagCommitNotifyOutput,
    uint32_t ulOutputSize)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    unsigned long ioctlCode = IOCTL_INMAGE_TAG_COMMIT_NOTIFY;

    DebugPrintf(SV_LOG_ALWAYS, "IOCTL_INMAGE_TAG_COMMIT_NOTIFY Disks: %d buffer size input: %d output: %d TAG_COMMIT_NOTIFY_INPUT = %d\n", 
            pTagCommitNotifyInput->ulNumDisks, ulInputSize, ulOutputSize, sizeof(TAG_COMMIT_NOTIFY_INPUT));
 
    int iStatus = pDeviceStream->IoControl(ioctlCode,
        pTagCommitNotifyInput,
        ulInputSize,
        pTagCommitNotifyOutput,
        ulOutputSize);

    if (SV_SUCCESS != iStatus)
    {
        DebugPrintf(SV_LOG_ERROR, "IOCTL_INMAGE_TAG_COMMIT_NOTIFY failed: %d\n", Error::UserToOS(iStatus));
    }
    else
    {
        int iStatusEx = pDeviceStream->GetErrorCode();
        if (iStatusEx == ERR_IO_PENDING)
        {
            DebugPrintf(SV_LOG_ALWAYS, "IOCTL_INMAGE_TAG_COMMIT_NOTIFY is pending\n");
            iStatus = pDeviceStream->WaitForEvent(INFINITE);
            DebugPrintf(SV_LOG_ALWAYS, "IOCTL_INMAGE_TAG_COMMIT_NOTIFY is completed\n");
        }
        else
        {
            DebugPrintf(SV_LOG_ALWAYS, "IOCTL_INMAGE_TAG_COMMIT_NOTIFY returned immediately\n");
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return (iStatus == SV_SUCCESS);
}

bool FilterDrvIfMajor::CancelTagCommitNotify(DeviceStream *pDeviceStream)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    int iStatus = pDeviceStream->CancelIOEx();

    if (SV_SUCCESS != iStatus)
    {
        DebugPrintf(SV_LOG_ERROR, "cancel of IOCTL_INMAGE_TAG_COMMIT_NOTIFY failed: %d\n", Error::UserToOS(iStatus));
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return (iStatus == SV_SUCCESS);
}

bool FilterDrvIfMajor::UnblockDrain(DeviceStream *pDeviceStream,
    SET_DRAIN_STATE_INPUT *pSetDrainStateInput,
    uint32_t ulInputSize,
    SET_DRAIN_STATE_OUTPUT *pSetDrainStateOutput,
    uint32_t ulOutputSize)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    unsigned long ioctlCode = IOCTL_INMAGE_SET_DRAIN_STATE;

    DebugPrintf(SV_LOG_ALWAYS,
        "IOCTL_INMAGE_SET_DRAIN_STATE Disks: "
        "%d buffer size input: %d output: %d SET_DRAIN_STATE_INPUT = %d\n",
        pSetDrainStateInput->ulNumDisks, ulInputSize, ulOutputSize,
        sizeof(SET_DRAIN_STATE_INPUT));

    int iStatus = pDeviceStream->IoControl(ioctlCode,
        pSetDrainStateInput,
        ulInputSize,
        pSetDrainStateOutput,
        ulOutputSize);

    if (SV_SUCCESS != iStatus)
    {
        DebugPrintf(SV_LOG_ERROR, "IOCTL_INMAGE_SET_DRAIN_STATE failed: %d\n",
            Error::UserToOS(iStatus));
    }
    else
    {
        int iStatusEx = pDeviceStream->GetErrorCode();
        if (iStatusEx == ERR_IO_PENDING)
        {
            DebugPrintf(SV_LOG_ALWAYS, "IOCTL_INMAGE_SET_DRAIN_STATE is pending\n");
            iStatus = pDeviceStream->WaitForEvent(INFINITE);
            DebugPrintf(SV_LOG_ALWAYS, "IOCTL_INMAGE_SET_DRAIN_STATE is completed\n");
        }
        else
        {
            DebugPrintf(SV_LOG_ALWAYS, "IOCTL_INMAGE_SET_DRAIN_STATE returned immediately\n");
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return (iStatus == SV_SUCCESS);
}

bool FilterDrvIfMajor::GetDrainState(DeviceStream *pDeviceStream,
    GET_DISK_STATE_INPUT *pGetDiskStateInput,
    uint32_t ulInputSize,
    GET_DISK_STATE_OUTPUT *pGetDiskStateOutput,
    uint32_t ulOutputSize)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    unsigned long ioctlCode = IOCTL_INMAGE_GET_DEVICE_STATE;

    DebugPrintf(SV_LOG_ALWAYS,
        "IOCTL_INMAGE_GET_DEVICE_STATE Disks: "
        "%d buffer size input: %d output: %d GET_DISK_STATE_INPUT = %d\n",
        pGetDiskStateInput->ulNumDisks, ulInputSize, ulOutputSize,
        sizeof(GET_DISK_STATE_INPUT));

    int iStatus = pDeviceStream->IoControl(ioctlCode,
        pGetDiskStateInput,
        ulInputSize,
        pGetDiskStateOutput,
        ulOutputSize);

    if (SV_SUCCESS != iStatus)
    {
        DebugPrintf(SV_LOG_ERROR, "IOCTL_INMAGE_GET_DEVICE_STATE failed: %d\n",
            Error::UserToOS(iStatus));
    }
    else
    {
        int iStatusEx = pDeviceStream->GetErrorCode();
        if (iStatusEx == ERR_IO_PENDING)
        {
            DebugPrintf(SV_LOG_ALWAYS, "IOCTL_INMAGE_GET_DEVICE_STATE is pending\n");
            iStatus = pDeviceStream->WaitForEvent(INFINITE);
            DebugPrintf(SV_LOG_ALWAYS, "IOCTL_INMAGE_GET_DEVICE_STATE is completed\n");
        }
        else
        {
            DebugPrintf(SV_LOG_ALWAYS, "IOCTL_INMAGE_GET_DEVICE_STATE returned immediately\n");
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return (iStatus == SV_SUCCESS);
}

bool
FilterDrvIfMajor::GetVolumeFlags(
    const std::string &devicename,
    int devicetype,
    DeviceStream *pDeviceStream,
    ULONG &flags)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    FilterDrvVol_t fmtddevicename = GetFormattedDeviceNameForDriverInput(devicename, devicetype);

    int iStatus = pDeviceStream->IoControl(
        IOCTL_INMAGE_GET_VOLUME_FLAGS,
        const_cast<wchar_t *>(fmtddevicename.c_str()),
        sizeof(wchar_t) * (fmtddevicename.length() + 1),
        &flags,
        sizeof(flags));

    if (SV_SUCCESS != iStatus)
    {
        iStatus = pDeviceStream->GetErrorCode();
        DebugPrintf(SV_LOG_ERROR,
            "%s - IOCTL_INMAGE_GET_VOLUME_FLAGS failed. Flags : %#x. Error : %d.\n",
            FUNCTION_NAME, flags, Error::UserToOS(iStatus));
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG,
            "%s - IOCTL_INMAGE_GET_VOLUME_FLAGS succeeded. Flags : %#x.\n",
            FUNCTION_NAME, flags);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return (iStatus == SV_SUCCESS);
}

bool
FilterDrvIfMajor::NotifySystemState(
    DeviceStream *pDeviceStream,
    bool bAreBitmapFilesEncryptedOnDisk)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    NOTIFY_SYSTEM_STATE_INPUT notifyInput = { 0 };
    NOTIFY_SYSTEM_STATE_OUTPUT notifyOutput = { 0 };

    if (bAreBitmapFilesEncryptedOnDisk) {
        notifyInput.Flags |= ssFlagsAreBitmapFilesEncryptedOnDisk;
    }

    int iStatus = pDeviceStream->IoControl(
        IOCTL_INMAGE_NOTIFY_SYSTEM_STATE,
        &notifyInput,
        sizeof(notifyInput),
        &notifyOutput,
        sizeof(notifyOutput));

    if (SV_SUCCESS != iStatus)
    {
        iStatus = pDeviceStream->GetErrorCode();
        DebugPrintf(SV_LOG_ERROR,
            "%s - IOCTL_INMAGE_NOTIFY_SYSTEM_STATE failed. AreBitmapFilesEncryptedOnDisk : %d. Error : %d.\n",
            FUNCTION_NAME, bAreBitmapFilesEncryptedOnDisk, Error::UserToOS(iStatus));
    }
    else
    {
        BOOST_ASSERT(notifyOutput.ResultFlags == notifyInput.Flags);

        DebugPrintf(SV_LOG_DEBUG,
            "%s - IOCTL_INMAGE_NOTIFY_SYSTEM_STATE succeeded. AreBitmapFilesEncryptedOnDisk : %d.\n",
            FUNCTION_NAME, bAreBitmapFilesEncryptedOnDisk);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return (iStatus == SV_SUCCESS);
}
