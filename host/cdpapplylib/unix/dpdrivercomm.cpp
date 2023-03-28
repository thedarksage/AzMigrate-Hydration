//
// Copyright (c) 2006 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : dpdrivercomm.cpp
//
// Description: 
//

#include <sstream>

#include "dpdrivercomm.h"
#include "dataprotectionexception.h"
#include "portablehelpers.h"
#include "svdparse.h"
#include <ace/OS_NS_unistd.h>
#include <boost/algorithm/string/replace.hpp>

#include "devicefilter.h"
#include "filterdrvifmajor.h"
#include "filterdrvifminor.h"
#include "volumegroupsettings.h"
#include "datacacher.h"
#include "volumeinfocollector.h"
#include "volumereporter.h"
#include "portablehelpersmajor.h"
#include "inmsafecapis.h"

#include "involfltfeatures.h"
#include "inmageex.h"
#include "filterdrvifmajor.h"
#include "filterdrvifminor.h"

DpDriverComm::DpDriverComm(std::string const & deviceName, const int &deviceType) 
: m_DeviceName(deviceName)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    Init(deviceType);
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

DpDriverComm::DpDriverComm(std::string const & deviceName, const int &deviceType, std::string const & persistentName) 
 : m_DeviceName(deviceName),
   m_PersistentName(persistentName)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    Init(deviceType);
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void DpDriverComm::Init(const int &deviceType)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

	std::stringstream msg;
	FilterDrvIfMajor fdif;

	// Create device stream
	try {
		m_pDeviceStream.reset(new DeviceStream(fdif.GetDriverName(deviceType)));
		DebugPrintf(SV_LOG_DEBUG, "Created device stream.\n");
	}
	catch (std::bad_alloc &e) {
		msg << "FILE: " << __FILE__ << ", LINE: " << __LINE__ << ", ERROR: " << "Memory allocation failed for creating driver stream object with exception " << e.what();
		throw DataProtectionException(msg.str());
	}

	// Open device stream
	if (SV_SUCCESS == m_pDeviceStream->Open(DeviceStream::Mode_Open | DeviceStream::Mode_RW | DeviceStream::Mode_ShareRW))
		DebugPrintf(SV_LOG_DEBUG, "Opened driver %s.\n", m_pDeviceStream->GetName().c_str());
	else {
		msg << "FILE: " << __FILE__ << ", LINE: " << __LINE__ << ", ERROR: " << "Failed to open driver " << m_pDeviceStream->GetName() << " with error " << m_pDeviceStream->GetErrorCode();
		throw DataProtectionException(msg.str());
	}

	// Get source device input name to use in all ioctls
	m_DeviceNameForDriverInput = fdif.GetFormattedDeviceNameForDriverInput(m_DeviceName, deviceType);
	DebugPrintf(SV_LOG_DEBUG, "Source name for driver input is %S\n", std::wstring(m_DeviceNameForDriverInput.begin(), m_DeviceNameForDriverInput.end()).c_str());

	// Get version
	DRIVER_VERSION dv;
	int iStatus = m_pDeviceStream->IoControl(IOCTL_INMAGE_GET_DRIVER_VERSION, &dv);
    if ( iStatus != SV_SUCCESS )
    {
		msg << "FILE: " << __FILE__ << ", LINE: " << __LINE__ << ", ERROR: " << "Failed to find version of driver " << m_pDeviceStream->GetName() << " with error " << m_pDeviceStream->GetErrorCode();
		throw DataProtectionException(msg.str());
    }
    else
    {
		DebugPrintf(SV_LOG_DEBUG, "Major Version: %d\n Minor Version: %d\n Minor Version2: %d\n MinorVersion3:%d\n",
			dv.ulDrMajorVersion, dv.ulDrMinorVersion,
			dv.ulDrMinorVersion2, dv.ulDrMinorVersion3);
		// Instantiate driver features
		try {
			//Get features
			DeviceFilterFeatures *pf;
            pf = new InVolFltFeatures(dv);
			m_pDeviceFilterFeatures.reset(pf);
			DebugPrintf(SV_LOG_DEBUG, "Created device filter features object.\n");
		}
		catch (std::bad_alloc &e) {
			msg << "FILE: " << __FILE__ << ", LINE: " << __LINE__ << ", ERROR: " << "Memory allocation failed for involflt filter driver features object with exception " << e.what();
			throw DataProtectionException(msg.str());
		}
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

DpDriverComm::~DpDriverComm()
{    
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void DpDriverComm::StartFiltering(const VOLUME_SETTINGS &pVolumeSettings, const VolumeSummaries_t *pvs)
{
    int iStatus = 0;
    std::stringstream msg;
    void *pvInParamToStartFilter = NULL;
    long int liInParamLen = 0;
    inm_dev_info_t devInfo;
    unsigned long int startfilterioctlcode = IOCTL_INMAGE_START_FILTERING_DEVICE;
    VolumeReporter::VolumeReport_t vr;
         
	if (!m_pDeviceFilterFeatures->IsDiffOrderSupported())
    {
		pvInParamToStartFilter = (void*)(m_DeviceNameForDriverInput.c_str());
		liInParamLen = m_DeviceNameForDriverInput.length() + 1;
    }
    else
    {
        /* driver having diff order support */
        startfilterioctlcode = IOCTL_INMAGE_START_FILTERING_DEVICE_V2;
        FilterDrvIfMajor drvif;
		bool bfilleddevinfo = drvif.FillInmDevInfo(m_pDeviceFilterFeatures.get(), pvs, pVolumeSettings, devInfo, vr);
        if (bfilleddevinfo)
        {
            // persistent name for /dev/sda is _dev_sda
            std::string pname = m_PersistentName.empty() ?  vr.m_DeviceName : m_PersistentName;
            boost::replace_all(pname, "/", "_");
            inm_strncpy_s(devInfo.d_pname, ARRAYSIZE(devInfo.d_pname), pname.c_str(), INM_GUID_LEN_MAX - 1);
            drvif.PrintInmDevInfo(devInfo);
        }
        else 
        {
			msg << "FILE: " << __FILE__ << ", LINE: " << __LINE__ << ", ERROR: unable to fill start filtering input for device " << m_DeviceName;
            throw DataProtectionException(msg.str());
        }
        pvInParamToStartFilter = &devInfo;
        liInParamLen = sizeof devInfo;
    }

	FilterDrvIfMinor drvifminor;
	iStatus = drvifminor.PassMaxTransferSizeIfNeeded(m_pDeviceFilterFeatures.get(), m_DeviceNameForDriverInput, *m_pDeviceStream, vr);
	if (SV_SUCCESS != iStatus) {
		msg << "FILE: " << __FILE__ << ", LINE: " << __LINE__ << ", ERROR: " << "Failed to pass max transfer size for device " << std::string(m_DeviceNameForDriverInput.begin(), m_DeviceNameForDriverInput.end()) 
			<< " to filter driver " << m_pDeviceStream->GetName() << " with error " << m_pDeviceStream->GetErrorCode();
		throw DataProtectionException(msg.str());
	}

    iStatus = m_pDeviceStream->IoControl(startfilterioctlcode, pvInParamToStartFilter, liInParamLen);
    if (SV_SUCCESS != iStatus)
    {
		msg << "FILE: " << __FILE__ << ", LINE: " << __LINE__ << ", ERROR: " << "Failed to issue start filtering for device " << std::string(m_DeviceNameForDriverInput.begin(), m_DeviceNameForDriverInput.end())
			<< " to filter driver " << m_pDeviceStream->GetName() << " with error " << m_pDeviceStream->GetErrorCode();
        throw DataProtectionException(msg.str());
    }
    // only log this locally as there are too many to send to cx
    DebugPrintf(SV_LOG_INFO, "NOTE: this is not an error DpDriverComm::StartFiltering for volume %s\n", m_DeviceName.c_str());
}


void DpDriverComm::StartFiltering(const VolumeReporter::VolumeReport_t &vr,
                    const VolumeSummaries_t *pvs)
{
    int iStatus = 0;
    std::stringstream msg;
    void *pvInParamToStartFilter = NULL;
    long int liInParamLen = 0;
    inm_dev_info_t devInfo;
    unsigned long int startfilterioctlcode = IOCTL_INMAGE_START_FILTERING_DEVICE;
	if (!m_pDeviceFilterFeatures->IsDiffOrderSupported())
    {
		pvInParamToStartFilter = (void*)(m_DeviceNameForDriverInput.c_str());
		liInParamLen = m_DeviceNameForDriverInput.length() + 1;
    }
    else
    {
        startfilterioctlcode = IOCTL_INMAGE_START_FILTERING_DEVICE_V2;
        FilterDrvIfMajor drvif;
		bool bfilleddevinfo = drvif.FillInmDevInfo(devInfo, vr);
        if (bfilleddevinfo)
        {
            // persistent name for /dev/sda is _dev_sda
            std::string pname = m_PersistentName;
            boost::replace_all(pname, "/", "_");
            inm_strncpy_s(devInfo.d_pname, ARRAYSIZE(devInfo.d_pname), pname.c_str(), INM_GUID_LEN_MAX - 1);
            drvif.PrintInmDevInfo(devInfo);
        }
        else 
        {
			msg << "FILE: " << __FILE__ << ", LINE: " << __LINE__ << ", ERROR: unable to fill start filtering input for device " << m_DeviceName;
            throw DataProtectionException(msg.str());
        }
        pvInParamToStartFilter = &devInfo;
        liInParamLen = sizeof devInfo;
    }
	FilterDrvIfMinor drvifminor;
	iStatus = drvifminor.PassMaxTransferSizeIfNeeded(m_pDeviceFilterFeatures.get(), m_DeviceNameForDriverInput, *m_pDeviceStream, vr);
	if (SV_SUCCESS != iStatus) {
		msg << "FILE: " << __FILE__ << ", LINE: " << __LINE__ << ", ERROR: " << "Failed to pass max transfer size for device " << std::string(m_DeviceNameForDriverInput.begin(), m_DeviceNameForDriverInput.end()) 
			<< " to filter driver " << m_pDeviceStream->GetName() << " with error " << m_pDeviceStream->GetErrorCode();
		throw DataProtectionException(msg.str());
	}
    iStatus = m_pDeviceStream->IoControl(startfilterioctlcode, pvInParamToStartFilter, liInParamLen);
    if (SV_SUCCESS != iStatus)
    {
		msg << "FILE: " << __FILE__ << ", LINE: " << __LINE__ << ", ERROR: " << "Failed to issue start filtering for device " << std::string(m_DeviceNameForDriverInput.begin(), m_DeviceNameForDriverInput.end())
			<< " to filter driver " << m_pDeviceStream->GetName() << " with error " << m_pDeviceStream->GetErrorCode();
        throw DataProtectionException(msg.str());
    }
    DebugPrintf(SV_LOG_INFO, "NOTE: this is not an error DpDriverComm::StartFiltering for volume %s\n", m_DeviceName.c_str());
}
void DpDriverComm::StopFiltering()
{
    int iStatus = 0;
    std::stringstream msg;
    iStatus = m_pDeviceStream->IoControl(IOCTL_INMAGE_STOP_FILTERING_DEVICE, (void*)m_DeviceNameForDriverInput.c_str(), m_DeviceNameForDriverInput.size(), NULL, 0);
    if (SV_SUCCESS != iStatus)
    {
		msg << "FILE: " << __FILE__ << ", LINE: " << __LINE__ << ", ERROR: " << "Failed to issue stop filtering for device " << std::string(m_DeviceNameForDriverInput.begin(), m_DeviceNameForDriverInput.end())
			<< " to filter driver " << m_pDeviceStream->GetName() << " with error " << m_pDeviceStream->GetErrorCode();
        throw DataProtectionException(msg.str());
    }
    DebugPrintf(SV_LOG_INFO, "NOTE: this is not an error DpDriverComm::StopFiltering for volume %s\n", m_DeviceName.c_str());
}
void DpDriverComm::ClearDifferentials()
{
    std::stringstream msg;

	int iStatus = m_pDeviceStream->IoControl(IOCTL_INMAGE_CLEAR_DIFFERENTIALS, const_cast<char *>(m_DeviceNameForDriverInput.c_str()), m_DeviceNameForDriverInput.length()+1, NULL, 0);
    if(SV_SUCCESS != iStatus)
    {
		msg << "FILE: " << __FILE__ << ", LINE: " << __LINE__ << ", ERROR: " << "Failed to issue clear differentials for device " << std::string(m_DeviceNameForDriverInput.begin(), m_DeviceNameForDriverInput.end())
			<< " to filter driver " << m_pDeviceStream->GetName() << " with error " << m_pDeviceStream->GetErrorCode();
        throw DataProtectionException(msg.str());
    }
    // FIX_LOG_LEVEL: note for now we are going to use SV_LOG_ERROR even on success so that we can
    // track this at the cx, eventually a new  SV_LOG_LEVEL will be added that is not an error 
    // but still allows this to be sent to the cx
    DebugPrintf(SV_LOG_ERROR, "NOTE: this is not an error DpDriverComm::ClearDifferentials for volume %s\n", m_DeviceName.c_str());
}

bool DpDriverComm::ResyncStartNotify(ResyncTimeSettings& rts)
{
    SV_ULONG    ulError = 0;
    return ResyncStartNotify(rts, ulError);
}

bool DpDriverComm::ResyncStartNotify(ResyncTimeSettings& rts, SV_ULONG& ulError)
{
    bool rv = true;
    int iStatus;

    //
    // Algorithm:
    //  
    // Get driver version
    //  If driver version ioctl suceeds, 
    //    if version >= perio_version
    //       pass RESYNC_START_V2 as parameter to IOCTL_INMAGE_RESYNC_START_NOTIFICATION ioctl
    //    else
    //       pass RESYNC_START as parameter to IOCTL_INMAGE_RESYNC_START_NOTIFICATION ioctl
    //    use the ioctl for obtaining resync start time
    //  else
    //      get resync time from system
    //  caller will send the resync time to cx
    //

	if (!m_pDeviceFilterFeatures->IsPerIOTimestampSupported())
	{
		RESYNC_START resyncStart;

		inm_strcpy_s((char *)resyncStart.VolumeGUID, NELEMS(resyncStart.VolumeGUID), m_DeviceNameForDriverInput.c_str());
		iStatus = m_pDeviceStream->IoControl(IOCTL_INMAGE_RESYNC_START_NOTIFICATION, &resyncStart);
		rts.time = resyncStart.TimeInHundNanoSecondsFromJan1601;
		rts.seqNo = resyncStart.ulSequenceNumber;
	}
	else
	{
		RESYNC_START_V2 resyncStart;

		inm_strcpy_s((char *)resyncStart.VolumeGUID, NELEMS(resyncStart.VolumeGUID), m_DeviceNameForDriverInput.c_str());
		iStatus = m_pDeviceStream->IoControl(IOCTL_INMAGE_RESYNC_START_NOTIFICATION, &resyncStart);
		rts.time = resyncStart.TimeInHundNanoSecondsFromJan1601;
		rts.seqNo = resyncStart.ullSequenceNumber;
	}

	if (iStatus != SV_SUCCESS)
	{
        std::stringstream msg;
		msg << "FILE: " << __FILE__ << ", LINE: " << __LINE__ << ", ERROR: " << "Failed to issue resync start notify for device " << std::string(m_DeviceNameForDriverInput.begin(), m_DeviceNameForDriverInput.end())
			<< " to filter driver " << m_pDeviceStream->GetName() << " with error " << m_pDeviceStream->GetErrorCode();
		DebugPrintf(SV_LOG_ERROR, "%s\n", msg.str().c_str());
		ulError = m_pDeviceStream->GetErrorCode();
		rv = false;
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG,
			"Returned Success from resync start notification for volume %s. TimeStamp [" ULLSPEC"] Sequence [" ULLSPEC"] \n",
			m_DeviceName.c_str(), rts.time, rts.seqNo);
	}

    return rv;
}

bool DpDriverComm::ResyncEndNotify(ResyncTimeSettings& rts)
{
    SV_ULONG    ulError = 0;
    return ResyncEndNotify(rts, ulError);
}

bool DpDriverComm::ResyncEndNotify(ResyncTimeSettings & rts, SV_ULONG& ulError)
{
    bool rv = true;
    int iStatus;

    //
    // Algorithm:
    //  
    // Get driver version
    //  If driver version ioctl suceeds,
    //     if version >= perio_version
    //       pass RESYNC_END_V2 as parameter to IOCTL_INMAGE_RESYNC_END_NOTIFICATION ioctl
    //     else
    //       pass RESYNC_END as parameter to IOCTL_INMAGE_RESYNC_END_NOTIFICATION ioctl
    //     use the ioctl for obtaining resync start time
    //  else
    //      get resync time from system
    // caller will now send resync time to cx
    //
	if (!m_pDeviceFilterFeatures->IsPerIOTimestampSupported())
	{
		RESYNC_END resyncEnd;

		inm_strcpy_s((char *)resyncEnd.VolumeGUID, NELEMS(resyncEnd.VolumeGUID), m_DeviceNameForDriverInput.c_str());
		iStatus = m_pDeviceStream->IoControl(IOCTL_INMAGE_RESYNC_END_NOTIFICATION, &resyncEnd);
		rts.time = resyncEnd.TimeInHundNanoSecondsFromJan1601;
		rts.seqNo = resyncEnd.ulSequenceNumber;
	}
	else
	{
		RESYNC_END_V2 resyncEnd;

		inm_strcpy_s((char *)resyncEnd.VolumeGUID, NELEMS(resyncEnd.VolumeGUID), m_DeviceNameForDriverInput.c_str());
		iStatus = m_pDeviceStream->IoControl(IOCTL_INMAGE_RESYNC_END_NOTIFICATION, &resyncEnd);
		rts.time = resyncEnd.TimeInHundNanoSecondsFromJan1601;
		rts.seqNo = resyncEnd.ullSequenceNumber;
	}

	if (iStatus != SV_SUCCESS)
	{
        std::stringstream msg;
		msg << "FILE: " << __FILE__ << ", LINE: " << __LINE__ << ", ERROR: " << "Failed to issue resync end notify for device " << std::string(m_DeviceNameForDriverInput.begin(), m_DeviceNameForDriverInput.end())
			<< " to filter driver " << m_pDeviceStream->GetName() << " with error " << m_pDeviceStream->GetErrorCode();
		DebugPrintf(SV_LOG_ERROR, "%s\n", msg.str().c_str());
		ulError = m_pDeviceStream->GetErrorCode();
		rv = false;
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG,
			"Returned Success from resync end notification for volume %s. TimeStamp [" ULLSPEC"] Sequence [" ULLSPEC"] \n",
			m_DeviceName.c_str(), rts.time, rts.seqNo);
	}

    return rv;
}


int DpDriverComm::GetFilesystemClusters(const std::string &filename, FileSystem::ClusterRanges_t &clusterranges)
{
    return SV_SUCCESS;
}
