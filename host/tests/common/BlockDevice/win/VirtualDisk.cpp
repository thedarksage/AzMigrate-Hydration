// CVirtualDisk.cpp
//CVirtualDisk implementations


#include "CVirtualDisk.h"
#include "PlatformAPIs.h"
#include "CDiskDevice.h"
#include "InmageTestException.h"
#include <strsafe.h>

void CVirtualDisk::CreateVDisk()
{
    VIRTUAL_STORAGE_TYPE vhdtypeParams = { 0 };
    CREATE_VIRTUAL_DISK_PARAMETERS vhdcreateParams;

    vhdtypeParams.DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_VHD;
    vhdtypeParams.VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT;

    vhdcreateParams.Version1.UniqueId = GUID_NULL;
    vhdcreateParams.Version1.BlockSizeInBytes = CREATE_VIRTUAL_DISK_PARAMETERS_DEFAULT_BLOCK_SIZE; // Default value representing 2MB block size.  
    vhdcreateParams.Version1.MaximumSize = m_ullMaxVhdSize;
    vhdcreateParams.Version1.SectorSizeInBytes = 512;
    vhdcreateParams.Version1.ParentPath = NULL;
    vhdcreateParams.Version1.SourcePath = NULL;
    vhdcreateParams.Version = CREATE_VIRTUAL_DISK_VERSION_1;

    SV_ULONG result = CreateVirtualDisk(&vhdtypeParams,
        m_vDiskPath,
        _VIRTUAL_DISK_ACCESS_MASK::VIRTUAL_DISK_ACCESS_ALL,
        NULL,
        _CREATE_VIRTUAL_DISK_FLAG::CREATE_VIRTUAL_DISK_FLAG_FULL_PHYSICAL_ALLOCATION,
        NULL,
        &vhdcreateParams,
        NULL,
        &m_hVDisk);

    if (ERROR_SUCCESS != result)
    {
        SV_ULONG dwErr = m_platformAPIs->GetLastError();
        if (ERROR_FILE_EXISTS != dwErr)
        {
            throw CBlockDeviceException("CreateVirtualDisk failed with err=0x%x", dwErr);
        }
    }
}

CVirtualDisk::~CVirtualDisk()
{
    m_platformAPIs->SafeClose(m_hVDisk);
}

CVirtualDisk::CVirtualDisk(const WCHAR* wszVDiskPath, ULONGLONG ullMaxSize, IPlatformAPIs* platformAPIs) :
m_mountedVDisk(NULL), m_hVDisk(INVALID_HANDLE_VALUE), m_ullMaxVhdSize(ullMaxSize), m_platformAPIs(platformAPIs)
{
    StringCchPrintfW(m_vDiskPath, MAX_PATH, L"%s", wszVDiskPath);
}

void	CVirtualDisk::Open()
{
    VIRTUAL_STORAGE_TYPE StorageType;
    StorageType.DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_VHD;
    StorageType.VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT;

    SV_ULONG dwErr = ::OpenVirtualDisk(&StorageType,
        m_vDiskPath,
        VIRTUAL_DISK_ACCESS_READ,
        OPEN_VIRTUAL_DISK_FLAG_NONE,
        NULL,
        &m_hVDisk);
    if (ERROR_SUCCESS != dwErr)
    {
        throw CVirtualDiskException("OpenVirtualDisk failed with error 0x%x", GetLastError());
    }
}

CDiskDevice*	CVirtualDisk::MountVDisk()
{
    ATTACH_VIRTUAL_DISK_PARAMETERS params;
    ZeroMemory(&params, sizeof(params));

    params.Version = ATTACH_VIRTUAL_DISK_VERSION_1;

    // Mount VDisk
    SV_ULONG dwErr = ::AttachVirtualDisk(
        m_hVDisk,
        NULL,
        ATTACH_VIRTUAL_DISK_FLAG_NONE,
        0,
        &params,
        NULL);

    if (ERROR_SUCCESS != dwErr)
    {
        throw CVirtualDiskException("AttachVirtualDisk failed with error 0x%x", m_platformAPIs->GetLastError());
    }

    ULONG ulVdiskSize = MAX_PATH;
    ZeroMemory(m_diskPath, MAX_PATH * sizeof(WCHAR));

    dwErr = ::GetVirtualDiskPhysicalPath(m_hVDisk, &ulVdiskSize, m_diskPath);
    dwErr = ::GetLastError();
    if (ERROR_SUCCESS != dwErr)
    {
        throw CVirtualDiskException("GetVirtualDiskPhysicalPath failed with error=0x%x", m_platformAPIs->GetLastError());
    }

	//TODO: Satish mount
    //m_mountedVDisk = new CDiskDevice(m_diskPath, m_platformAPIs);

    //return m_mountedVDisk;
	return NULL;
}

void	CVirtualDisk::UnMountVDisk()
{
    SV_ULONG dwErr = ::DetachVirtualDisk(m_hVDisk, DETACH_VIRTUAL_DISK_FLAG_NONE, NULL);
    if (ERROR_SUCCESS != dwErr)
    {
        throw CVirtualDiskException("DetachVirtualDisk failed with error 0x%x", GetLastError());
    }
}
