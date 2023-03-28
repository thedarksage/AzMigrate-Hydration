#ifndef _DISK_UTILS_H_
#define _DISK_UTILS_H_

#include <set>

#ifndef INITGUID
#define INITGUID
#endif

#include <guiddef.h>
#include "deviceutils.h"

#define GUID_LEN        36
#define MAX_GUID_LEN    40

class DiskDeviceStream : public DeviceStream {
private:
    ULONG                           m_ulIndex;

    PDRIVE_LAYOUT_INFORMATION_EX    IoctlDiskGetDriveLayoutEx();

public:
    DiskDeviceStream();

    DiskDeviceStream(ULONG ulIndex);

    DiskDeviceStream(std::string diskName);

    DWORD Open();

    DWORD GetDeviceId(std::wstring& deviceId);

    BOOL  GetBusType(STORAGE_BUS_TYPE& busType);
    BOOL    IsDynamic();
    BOOL    IsStoragePoolDisk();
};

class DiskIndexEnumerator
{
private:
    std::set<std::string>     m_diskIndexSet;
    std::set<std::string>     m_storagePoolDiskIndex;
    std::set<std::string>     m_storageSpaceDiskIndex;
    std::set<std::string>     m_dynamicDiskIndex;
public:
    DiskIndexEnumerator(std::set<ULONG> diskIndices);
    ~DiskIndexEnumerator(){}

    std::set<std::string>         GetStoragePoolDisks() { return m_storagePoolDiskIndex; }
    std::set<std::string>         GetStorageSpaceDisks() { return m_storageSpaceDiskIndex; }
    std::set<std::string>         GetDisks() { return m_diskIndexSet; }
    std::set<std::string>         GetDynamicDisks() { return m_dynamicDiskIndex; }
};

BOOL GetBusType(std::string deviceName, STORAGE_BUS_TYPE& busType, std::string& errorMessage);
BOOL GetBusType(HANDLE hDisk, STORAGE_BUS_TYPE& busType, std::string& errorMessage);

std::set<ULONG>
GetAvailableDiskIndices(
    ULONG   ulMaxDiskIndex,
    ULONG   ulMaxConsMissingIndices);

#endif