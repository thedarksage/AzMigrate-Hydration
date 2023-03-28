#include "stdafx.h"
#include "diskutils.h"

#include <diskguid.h>
#include "..\common\win32\DiskHelpers.h"

DiskDeviceStream::DiskDeviceStream() :
DeviceStream() {}

DiskDeviceStream::DiskDeviceStream(ULONG ulIndex) : m_ulIndex(ulIndex) {
    std::stringstream   ss;
    ss << "\\\\.\\PHYSICALDRIVE" << ulIndex;
    m_deviceName = ss.str();
}

DiskDeviceStream::DiskDeviceStream(std::string diskName) :
DeviceStream(diskName)
{
}


DWORD DiskDeviceStream::Open()
{
    return DeviceStream::Open(m_deviceName);
}

DRIVE_LAYOUT_INFORMATION_EX* DiskDeviceStream::IoctlDiskGetDriveLayoutEx()
{
    PDRIVE_LAYOUT_INFORMATION_EX    pLayoutEx = NULL;
    ULONG                           ulNumPartitions = 2;
    ULONG                           ulBufferSize = 0;
    DWORD                           dwNumRetries = 0;
    std::stringstream               ssErrorMessage;

    do
    {
        ulNumPartitions *= 2;

        ulBufferSize = sizeof(DRIVE_LAYOUT_INFORMATION_EX) + ((ulNumPartitions - 1) * sizeof(PARTITION_INFORMATION_EX));
        pLayoutEx = (PDRIVE_LAYOUT_INFORMATION_EX)calloc(1, ulBufferSize);

        if (NULL == pLayoutEx)
        {
            ssErrorMessage << "Failed to allocate %u bytes for %d expected partitions, "
                "for IOCTL_DISK_GET_DRIVE_LAYOUT_EX\n" << ulBufferSize
                << ulNumPartitions;
            m_errorMessage = ssErrorMessage.str();
            break;
        }

        DWORD Status = DeviceStream::Ioctl(IOCTL_DISK_GET_DRIVE_LAYOUT_EX, NULL, 0, pLayoutEx, ulBufferSize);
        if ((ERROR_SUCCESS == Status) || (ERROR_INSUFFICIENT_BUFFER != Status)){
            if (ERROR_SUCCESS != Status) {
                ssErrorMessage << "IOCTL_DISK_GET_DRIVE_LAYOUT_EX for disk " << m_deviceName <<
                    "Failed with err=" << GetLastError();
                m_errorMessage = ssErrorMessage.str();
            }
            break;
        }

        SafeFree(pLayoutEx);
    } while (++dwNumRetries < 100);

    return pLayoutEx;
}

DWORD DiskDeviceStream::GetDeviceId(std::wstring& deviceId)
{
    std::wstringstream              deviceIdStream;
    PDRIVE_LAYOUT_INFORMATION_EX    pLayoutEx = NULL;
    std::vector<WCHAR>              deviceIdStr(MAX_GUID_LEN);
    DWORD                           dwError = ERROR_SUCCESS;
    std::stringstream               ssErrorMessage;

    deviceId = L"";

    pLayoutEx = IoctlDiskGetDriveLayoutEx();
    if (NULL == pLayoutEx) {
        dwError = GetLastError();
        goto Cleanup;
    }

    if (PARTITION_STYLE_MBR == pLayoutEx->PartitionStyle) {
        deviceIdStream << pLayoutEx->Mbr.Signature;
        deviceId = deviceIdStream.str();
    }
    else if (PARTITION_STYLE_GPT == pLayoutEx->PartitionStyle) {
        if (0 == StringFromGUID2(pLayoutEx->Gpt.DiskId, &deviceIdStr[0], MAX_GUID_LEN)) {
            dwError = ERROR_INSUFFICIENT_BUFFER;
            ssErrorMessage << "StringFromGUID2 on disk " << m_deviceName << " failed with err=" << dwError;
            m_errorMessage = ssErrorMessage.str();
            goto Cleanup;
        }

        // Remove leading { and trailing }
        deviceIdStream << &deviceIdStr[1];
        deviceId = deviceIdStream.str();
        deviceId.erase(deviceId.length() - 1);
    }

Cleanup:
    SafeFree(pLayoutEx);
    return dwError;
}


BOOL DiskDeviceStream::IsDynamic()
{
    PDRIVE_LAYOUT_INFORMATION_EX    pLayoutEx = NULL;
    BOOL                            isDynamic = FALSE;

    pLayoutEx = IoctlDiskGetDriveLayoutEx();
    if (NULL == pLayoutEx) {
        DebugPrintf("IOCTL_DISK_GET_DRIVE_LAYOUT_EX on disk %s failed with err=%d\n", m_deviceName.c_str(), GetLastError());
        goto Cleanup;
    }

    if (PARTITION_STYLE_MBR == pLayoutEx->PartitionStyle) {
        for (UINT32 ulIdx = 0; ulIdx < pLayoutEx->PartitionCount; ulIdx++) {
            if (PARTITION_LDM == pLayoutEx->PartitionEntry[ulIdx].Mbr.PartitionType){
                isDynamic = TRUE;
                break;
            }
        }
    }
    else if (PARTITION_STYLE_GPT == pLayoutEx->PartitionStyle) {
        for (UINT32 ulIdx = 0; ulIdx < pLayoutEx->PartitionCount; ulIdx++) {
            if (IsEqualGUID(PARTITION_LDM_METADATA_GUID, pLayoutEx->PartitionEntry[ulIdx].Gpt.PartitionType)){
                isDynamic = TRUE;
                break;
            }
        }
    }

Cleanup:
    SafeFree(pLayoutEx);
    return isDynamic;
}


BOOL  DiskDeviceStream::GetBusType(STORAGE_BUS_TYPE& busType)
{
    std::string errorMessage;
    return ::GetBusType(m_handle, busType, errorMessage);
}

BOOL DiskDeviceStream::IsStoragePoolDisk()
{
    PDRIVE_LAYOUT_INFORMATION_EX    pLayoutEx = NULL;
    BOOL                            isStoragePoolDisk = FALSE;

    if (!IsWindows8OrGreater()) {
        return FALSE;
    }

    pLayoutEx = IoctlDiskGetDriveLayoutEx();
    if (NULL == pLayoutEx) {
        DebugPrintf("IOCTL_DISK_GET_DRIVE_LAYOUT_EX on disk %s failed with err=%d\n", m_deviceName.c_str(), GetLastError());
        goto Cleanup;
    }

    if (PARTITION_STYLE_MBR == pLayoutEx->PartitionStyle) {
        for (UINT32 ulIdx = 0; ulIdx < pLayoutEx->PartitionCount; ulIdx++) {
            if (PARTITION_SPACES == pLayoutEx->PartitionEntry[ulIdx].Mbr.PartitionType){
                isStoragePoolDisk = TRUE;
                break;
            }
        }
    }
    else if (PARTITION_STYLE_GPT == pLayoutEx->PartitionStyle) {
        for (UINT32 ulIdx = 0; ulIdx < pLayoutEx->PartitionCount; ulIdx++) {
            if (IsEqualGUID(PARTITION_SPACES_GUID, pLayoutEx->PartitionEntry[ulIdx].Gpt.PartitionType)){
                isStoragePoolDisk = TRUE;
                break;
            }
        }
    }


Cleanup:
    SafeFree(pLayoutEx);
    return isStoragePoolDisk;
}

DiskIndexEnumerator::DiskIndexEnumerator(std::set<ULONG>    diskIndices)

{
    std::stringstream   ssDeviceName;
    ULONG ulDiskIndex = 0;
    ULONG ulConsMissingIndices = 0;

    for (auto diskIndex : diskIndices) {
        DiskDeviceStream    diskDevice(diskIndex);
        STORAGE_BUS_TYPE    busType;

        if (ERROR_SUCCESS != diskDevice.Open()) {
            continue;
        }

        ssDeviceName.str("");
        ssDeviceName << DISK_NAME_PREFIX << diskIndex;

        std::string strPhysicalDiskName = ssDeviceName.str();
        std::transform(strPhysicalDiskName.begin(), strPhysicalDiskName.end(), strPhysicalDiskName.begin(), ::toupper);

        m_diskIndexSet.insert(strPhysicalDiskName);
        if (!diskDevice.GetBusType(busType)) {
            busType = BusTypeMax;
        }

        if (diskDevice.IsDynamic()) {
            m_dynamicDiskIndex.insert(strPhysicalDiskName);
        }

        if (!IsWindows8OrGreater()) {
            continue;
        }

        if (BusTypeSpaces == busType) {
            m_storageSpaceDiskIndex.insert(strPhysicalDiskName);
        }

        if (diskDevice.IsStoragePoolDisk()) {
            m_storagePoolDiskIndex.insert(strPhysicalDiskName);
        }
    }
}


