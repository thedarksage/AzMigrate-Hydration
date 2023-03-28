// DiskDeviceMajor.cpp : Contains the Windows DiskDevice implementation 
//


#include "PlatformAPIs.h"
#include "CDiskDevice.h"
#include "InmageTestException.h"
#include "inmsafecapis.h"
#include <iostream>
#include <vector>
#include <atlbase.h>

CDiskDevice::CDiskDevice(int diskNumber, IPlatformAPIs *platformAPIs)
{
	char diskName[100];
    m_diskNumber = diskNumber;
    m_platformAPIs = platformAPIs;
	inm_sprintf_s(diskName, ARRAYSIZE(diskName), "\\\\.\\PhysicalDrive%d", m_diskNumber);
	m_diskName = diskName;
	Open();
    PopulateDiskInfo();
}

CDiskDevice::CDiskDevice(wchar_t* wszDiskName, IPlatformAPIs* platformAPIs)
{
    m_diskName = CW2A(wszDiskName);
    Open();
    PopulateDiskInfo();
}

void CDiskDevice::PopulateDiskInfo()
{
	DWORD dwBytes = 0;
	DWORD dwErr = ERROR_SUCCESS;
	DWORD size = 4*1024;
	PDRIVE_LAYOUT_INFORMATION_EX layoutEx = NULL;

	while (size < 10 * 1024)
	{	
		SafeFree(layoutEx);
		layoutEx = (PDRIVE_LAYOUT_INFORMATION_EX)malloc(size);

		if (NULL == layoutEx)
		{
			throw CBlockDeviceException("%s: %s Failed to allocate layout size = %ul\n", __FUNCTION__, m_diskName, size);
		}


		if (m_platformAPIs->DeviceIoControlSync(m_hDisk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, NULL, 0, layoutEx, size, &dwBytes))
		{
			dwErr = ERROR_SUCCESS;
			break;
		}

		dwErr = m_platformAPIs->GetLastError();
		if (ERROR_INSUFFICIENT_BUFFER != dwErr)
		{
			SafeFree(layoutEx);
			throw CBlockDeviceException("%s: IOCTL_DISK_GET_DRIVE_LAYOUT_EX on Disk %S failed with error=0x%x", __FUNCTION__, m_diskName, dwErr);
		}
		size += 1024;
	}

	if (ERROR_SUCCESS != dwErr)
	{
		SafeFree(layoutEx);
		throw CBlockDeviceException("%s: IOCTL_DISK_GET_DRIVE_LAYOUT_EX on Disk %S failed with error=0x%x", __FUNCTION__, m_diskName, dwErr);
	}

	vector<char> diskId(MAX_PATH);
	if (layoutEx->PartitionStyle == PARTITION_STYLE_MBR)
	{
		inm_sprintf_s(&diskId[0], MAX_STRING_SIZE, "%lu", layoutEx->Mbr.Signature);
		m_diskId = &diskId[0];
	}
	else if (layoutEx->PartitionStyle == PARTITION_STYLE_GPT)
	{
		// TODO: Satish
		//::StringFromGUID2(layoutEx->Gpt.DiskId, &diskId[0], (GUID_SIZE_IN_CHARS * sizeof(WCHAR)));
	}

	SafeFree(layoutEx);

	GET_LENGTH_INFORMATION lengthInfo = { 0 };

	if (!m_platformAPIs->DeviceIoControlSync(m_hDisk, IOCTL_DISK_GET_LENGTH_INFO, NULL, 0, &lengthInfo, sizeof(lengthInfo), &dwBytes))
	{
		dwErr = m_platformAPIs->GetLastError();
		if (ERROR_MORE_DATA != dwErr)
		{
			throw CBlockDeviceException("%s: IOCTL_DISK_GET_LENGTH_INFO on Disk %S failed with error=0x%x", __FUNCTION__, m_diskName, dwErr);
		}
	}

	m_ullDiskSize = lengthInfo.Length.QuadPart;

	DISK_GEOMETRY geometry = { 0 };
	if (!m_platformAPIs->DeviceIoControlSync(m_hDisk, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0, &geometry, sizeof(geometry), &dwBytes))
	{
		dwErr = m_platformAPIs->GetLastError();
		if (ERROR_MORE_DATA != dwErr)
		{
			throw CBlockDeviceException("%s: IOCTL_DISK_GET_LENGTH_INFO on Disk %S failed with error=0x%x", __FUNCTION__, m_diskName, dwErr);
		}
	}

	m_dwBlockSize = geometry.BytesPerSector;
	m_ullDiskSize = geometry.Cylinders.QuadPart * (SV_ULONG)geometry.TracksPerCylinder *
		(SV_ULONG)geometry.SectorsPerTrack * (SV_ULONG)geometry.BytesPerSector;
}

bool CDiskDevice::DiskOnlineOffline(bool online, bool readOnly, bool persist)
{
    SET_DISK_ATTRIBUTES attributes = { 0 };
    attributes.Version = sizeof(SET_DISK_ATTRIBUTES);
    attributes.Persist = persist;
    attributes.Attributes = (online) ? 0L : DISK_ATTRIBUTE_OFFLINE;
    attributes.Attributes |= (readOnly) ? DISK_ATTRIBUTE_READ_ONLY : 0L;
    attributes.AttributesMask = DISK_ATTRIBUTE_OFFLINE | DISK_ATTRIBUTE_READ_ONLY;

    DWORD dwBytesReturned;

    return m_platformAPIs->DeviceIoControlSync(m_hDisk, IOCTL_DISK_SET_DISK_ATTRIBUTES, &attributes, sizeof(SET_DISK_ATTRIBUTES), NULL, 0, &dwBytesReturned);
}

std::string	CDiskDevice::GetDeviceId()
{
	return m_diskId;

}


