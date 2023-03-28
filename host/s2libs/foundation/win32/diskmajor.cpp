///
///  \file diskmajor.cpp
///
///  \brief windows implementation
///



#include "disk.h"
#include "logger.h"
#include "portable.h"
#include "volumereporter.h"
#include "volumereporterimp.h"
#include "portablehelpersmajor.h"

#include <Windows.h>
#include <WinIoCtl.h>

SV_INT Disk::Open(BasicIo::BioOpenMode mode)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

	SV_INT iStatus = SV_FAILURE;

	if (m_displayName.empty()) {

		if (m_pVolumeSummariesCache) {
			//Get volume report from vic cache
			VolumeReporter::VolumeReport_t vr;
			VolumeReporterImp rptr;
			vr.m_ReportLevel = VolumeReporter::ReportVolumeLevel;
			rptr.GetVolumeReport(GetName(), *m_pVolumeSummariesCache, vr);
			rptr.PrintVolumeReport(vr);

			m_nameBasedOn = vr.m_nameBasedOn;
			
			//Verify the disk is reported
			if (vr.m_bIsReported) {
				DebugPrintf(SV_LOG_DEBUG, "disk %s is present in vic cache.\n", GetName().c_str());
				//Verify the disk device name
				if (!vr.m_DeviceName.empty()) {
					m_displayName = vr.m_DeviceName;
				}
				else {
					//Set last error of stream explicitly
					SetLastError(ERROR_FILE_NOT_FOUND);
					DebugPrintf(SV_LOG_ERROR, "Failed to find device name for disk %s.\n", GetName().c_str());
				}
			}
			else {
				//Set last error of stream explicitly
				SetLastError(ERROR_FILE_NOT_FOUND);
				DebugPrintf(SV_LOG_DEBUG, "Failed to find disk %s.\n", GetName().c_str());
			}
		}
		else {
			//Set last error of stream explicitly
			SetLastError(ERROR_INVALID_PARAMETER);
			DebugPrintf(SV_LOG_ERROR, "Volume infocollector information not specified to open disk %s.\n", GetName().c_str());
		}
	}

	if (!m_displayName.empty()){
		iStatus = VerifyAndOpen(m_displayName, mode, m_nameBasedOn);
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return iStatus;
}


SV_INT Disk::VerifyAndOpen(const std::string &diskDeviceName, BasicIo::BioOpenMode &mode, const VolumeSummary::NameBasedOn & nameBasedOn)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	SV_INT iStatus = SV_FAILURE;

	//Open the disk device name
	if (SV_SUCCESS == m_Bio.Open(diskDeviceName.c_str(), mode)) {
		DebugPrintf(SV_LOG_DEBUG, "Opened disk device name %s.\n", diskDeviceName.c_str());

		
		std::string id;
		if (VolumeSummary::SCSIID == nameBasedOn){
			//Verify if we get same scsi id
			id = GetDiskScsiId(diskDeviceName);
		}
		else{
		//Verify if the handle gives same MBR signature/GPT GUID
			id = GetDiskGuid(GetHandle());
		}

		DebugPrintf(SV_LOG_DEBUG, "Got id %s from above disk device name.\n", id.c_str());
		if (id == GetName()) {
			iStatus = SV_SUCCESS;
			DebugPrintf(SV_LOG_DEBUG, "Verified that disk device name %s gives correct id %s.\n", diskDeviceName.c_str(), id.c_str());
		} else {
			//Set last error of stream explicitly
			SetLastError(ERROR_FILE_NOT_FOUND);
			DebugPrintf(SV_LOG_ERROR, "From disk device name %s, id got %s does not match required id %s.\n", diskDeviceName.c_str(), id.c_str(), GetName().c_str());
		} 
	} else {
		//Do not need to set last error because Open already sets this
		DebugPrintf(SV_LOG_ERROR, "Failed to open disk device name %s for disk %s.\n", diskDeviceName.c_str(), GetName().c_str());
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return iStatus;
}


SV_ULONGLONG Disk::GetSize(void)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	SV_ULONGLONG size;
	std::string errMsg;
	size = GetDiskSize(GetHandle(), errMsg);
	if (size)
		DebugPrintf(SV_LOG_DEBUG, "Disk size got is " ULLSPEC ".\n", size);
	else
		DebugPrintf(SV_LOG_ERROR, "Failed to get disk size for %s with error %s.\n", GetName().c_str(), errMsg.c_str());
	
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return size;
}

bool Disk::OfflineRW(void)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	bool rv = OnlineOffline(false, false, true);

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return rv;
}


bool Disk::OnlineRW(void)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	bool rv = OnlineOffline(true, false, true);
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return rv;
}

bool Disk::OnlineOffline(bool online, bool readOnly, bool persist)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

	bool rv = false;
	bool usingAlreadyOpenHandle = isOpen();

	if (!usingAlreadyOpenHandle) {

		BasicIo::BioOpenMode openMode = BasicIo::BioRWExisitng |
			BasicIo::BioShareAll | 
			BasicIo::BioBinary | 
			BasicIo::BioNoBuffer | 
			BasicIo::BioWriteThrough;

		if (SV_SUCCESS != Open(openMode))
		{
			return false;
		}
	}

	SET_DISK_ATTRIBUTES attributes = { 0 };
	attributes.Version = sizeof(SET_DISK_ATTRIBUTES);
	attributes.Persist = persist;
	attributes.Attributes = (online) ? 0L : DISK_ATTRIBUTE_OFFLINE;
	attributes.Attributes |= (readOnly) ? DISK_ATTRIBUTE_READ_ONLY : 0L;
	attributes.AttributesMask = DISK_ATTRIBUTE_OFFLINE | DISK_ATTRIBUTE_READ_ONLY;

	DWORD dwBytesReturned;

	if (TRUE == DeviceIoControl(GetHandle(), 
		IOCTL_DISK_SET_DISK_ATTRIBUTES, 
		&attributes, 
		sizeof(SET_DISK_ATTRIBUTES), 
		NULL, 
		0, 
		&dwBytesReturned, 
		NULL)) {
		
		DebugPrintf(SV_LOG_DEBUG, 
			"Disk %s is now in %s %s mode.\n", 
			GetName().c_str(),
			(online)?"Online":"Offline",
			(readOnly)?"Read-Only":"Read-Write");

		rv = true;
	}
	else {

		SetLastError(GetLastError());
		DebugPrintf(SV_LOG_ERROR, 
			"Function:%s IOCTL_DISK_SET_DISK_ATTRIBUTES on Disk %s failed with error code %lu.\n", 
			FUNCTION_NAME, 
			GetName().c_str(), 
			GetLastError());
	}

	if (!usingAlreadyOpenHandle)
	{
		Close();
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return rv;
}

bool Disk::InitializeAsRawDisk(void)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

	bool rv = false;
	bool usingAlreadyOpenHandle = isOpen();

	if (!usingAlreadyOpenHandle) {

		BasicIo::BioOpenMode openMode = BasicIo::BioRWExisitng |
			BasicIo::BioShareAll |
			BasicIo::BioBinary |
			BasicIo::BioNoBuffer |
			BasicIo::BioWriteThrough;

		if (SV_SUCCESS != Open(openMode))
		{
			return false;
		}
	}

	CREATE_DISK attributes;
	attributes.PartitionStyle = PARTITION_STYLE_RAW;


	DWORD dwBytesReturned;

	if (TRUE == DeviceIoControl(GetHandle(),   // handle to device
		IOCTL_DISK_CREATE_DISK,                // dwIoControlCode
		&attributes,                          // input buffer
		sizeof CREATE_DISK,                   // size of input buffer
		NULL,                                 // lpOutBuffer
		0,                                    // nOutBufferSize
		&dwBytesReturned,                     // number of bytes returned
		NULL
		)) {

		DebugPrintf(SV_LOG_DEBUG,
			"Disk %s is converted to raw disk.\n",
			GetName().c_str());
		rv = true;
	}
	else {

		SetLastError(GetLastError());
		DebugPrintf(SV_LOG_ERROR,
			"Function:%s IOCTL_DISK_CREATE_DISK on Disk %s failed with error code %lu.\n",
			FUNCTION_NAME,
			GetName().c_str(),
			GetLastError());
	}

	if (!usingAlreadyOpenHandle)
	{
		Close();
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return rv;
}

VOLUME_STATE Disk::GetDeviceState()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

	VOLUME_STATE rv = VOLUME_UNKNOWN;
	bool usingAlreadyOpenHandle = isOpen();

	if (!usingAlreadyOpenHandle) {

		BasicIo::BioOpenMode openMode = BasicIo::BioReadExisting |
			BasicIo::BioShareAll |
			BasicIo::BioBinary |
			BasicIo::BioNoBuffer |
			BasicIo::BioWriteThrough;

		if (SV_SUCCESS != Open(openMode))
		{
			return VOLUME_UNKNOWN;
		}
	}

	GET_DISK_ATTRIBUTES attributes = { 0 };
	attributes.Version = sizeof(GET_DISK_ATTRIBUTES);

	DWORD dwBytesReturned;

	if (TRUE == DeviceIoControl(GetHandle(),
		IOCTL_DISK_GET_DISK_ATTRIBUTES,
		NULL,
		0,
		&attributes,
		sizeof attributes,
		&dwBytesReturned,
		NULL)) {

		bool offline = (attributes.Attributes & DISK_ATTRIBUTE_OFFLINE);
		bool readonly = (attributes.Attributes & DISK_ATTRIBUTE_READ_ONLY);

		DebugPrintf(SV_LOG_DEBUG,
			"Disk %s is in %s %s mode.\n",
			GetName().c_str(),
			(offline) ? "Offline" : "Online",
			(readonly) ? "Read-Only" : "Read-Write");

		rv = (offline) ? (readonly ? VOLUME_HIDDEN_RO : VOLUME_HIDDEN) : (readonly ? VOLUME_VISIBLE_RO : VOLUME_VISIBLE_RW);
	}
	else {

		SetLastError(GetLastError());
		DebugPrintf(SV_LOG_ERROR,
			"Function:%s IOCTL_DISK_GET_DISK_ATTRIBUTES on Disk %s failed with error code %lu.\n",
			FUNCTION_NAME,
			GetName().c_str(),
			GetLastError());
	}

	if (!usingAlreadyOpenHandle)
	{
		Close();
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return rv;

}

bool Disk::OpenExclusive()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	bool rv = false;

	rv = OfflineRW();
	
	if (rv){
		BasicIo::BioOpenMode openMode = BasicIo::BioRWExisitng |
			BasicIo::BioShareAll |
			BasicIo::BioBinary |
			BasicIo::BioNoBuffer |
			BasicIo::BioWriteThrough;

		rv = Open(openMode);
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return rv;
}