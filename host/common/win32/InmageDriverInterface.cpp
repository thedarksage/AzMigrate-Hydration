#include "portablehelpers.h"

#include <winioctl.h>
#include "InmFltIoctl.h"
#include "InmFltInterface.h"

#include "InmageDriverInterface.h"
#include <vector>

InmageDriverInterface::InmageDriverInterface() :
m_hInmageDevice(INVALID_HANDLE_VALUE)
{
    m_hInmageDevice = CreateFile(INMAGE_DISK_FILTER_DOS_DEVICE_NAME, 
                                 GENERIC_READ,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 NULL,
                                 OPEN_EXISTING,
                                 FILE_ATTRIBUTE_NORMAL,
                                 NULL);
    if (INVALID_HANDLE_VALUE == m_hInmageDevice) {
        DebugPrintf(SV_LOG_ERROR, "Failed to open device %s error=%d\n", INMAGE_DISK_FILTER_DOS_DEVICE_NAME, GetLastError());
    }
}

InmageDriverInterface::~InmageDriverInterface()
{
    if (INVALID_HANDLE_VALUE != m_hInmageDevice) {
        CloseHandle(m_hInmageDevice);
    }

    m_hInmageDevice = INVALID_HANDLE_VALUE;
}

bool InmageDriverInterface::IsDiskRecoveryRequired()
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);

    if (INVALID_HANDLE_VALUE == m_hInmageDevice) {
        DebugPrintf(SV_LOG_ERROR, "%s: Device handle is invalid\n", FUNCTION_NAME);
        return false;
    }

    DRIVER_FLAGS_OUTPUT     driverFlags = { 0 };

    DWORD dwBytes;
    if (!DeviceIoControl(m_hInmageDevice, IOCTL_INMAGE_GET_DRIVER_FLAGS, NULL, 0, &driverFlags, sizeof(driverFlags), &dwBytes, NULL)){
        DebugPrintf(SV_LOG_ERROR, "IOCTL_INMAGE_GET_DRIVER_FLAGS failed with error=%d\n", GetLastError());
        return false;
    }

    DebugPrintf(SV_LOG_DEBUG, "Driver Flags: 0x%08x\n", driverFlags.ulFlags);

    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", FUNCTION_NAME);
    return TEST_FLAG(driverFlags.ulFlags, DRIVER_FLAG_DISK_RECOVERY_NEEDED);
}

void InmageDriverInterface::SetSanPolicyToOnlineForAllDisks()
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);
    if (INVALID_HANDLE_VALUE == m_hInmageDevice) {
        DebugPrintf(SV_LOG_ERROR, "%s: Device handle is invalid\n", FUNCTION_NAME);
        return;
    }


    SET_SAN_POLICY  sanPolicy = { 0 };
    sanPolicy.ulFlags |= SET_SAN_ALL_DEVICES_FLAGS;
    sanPolicy.ulPolicy = 0;

    DWORD dwBytes;
    if (!DeviceIoControl(m_hInmageDevice, IOCTL_INMAGE_SET_SAN_POLICY, &sanPolicy, sizeof(sanPolicy), NULL, 0, &dwBytes, NULL)) {
        DebugPrintf(SV_LOG_ERROR, "IOCTL_INMAGE_SET_SAN_POLICY failed for device %s error=%d\n", INMAGE_DISK_FILTER_DOS_DEVICE_NAME, GetLastError());
        return;
    }

    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", FUNCTION_NAME);
}

bool    InmageDriverInterface::GetDiskIndexList(std::set<SV_ULONG>&  diskIndexList)
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);

    if (INVALID_HANDLE_VALUE == m_hInmageDevice) {
        DebugPrintf(SV_LOG_ERROR, "%s: Device handle is invalid\n", FUNCTION_NAME);
        return false;
    }

    PDISKINDEX_INFO     pDiskIndexList = NULL;
    DWORD               size, dwBytes;
    std::vector<UCHAR>  Buffer(256 * sizeof(SV_ULONG));

    pDiskIndexList = (PDISKINDEX_INFO)&Buffer[0];
    memset(pDiskIndexList, 0, Buffer.size());

    BOOL bSuccess = DeviceIoControl(m_hInmageDevice, IOCTL_INMAGE_GET_DISK_INDEX_LIST, NULL, 0, pDiskIndexList, Buffer.size(), &dwBytes, NULL);

    if (bSuccess){
        for (ULONG ulIndex = 0; ulIndex < pDiskIndexList->ulNumberOfDisks; ulIndex++) {
            DebugPrintf(SV_LOG_DEBUG, "%s: Adding disk %d\n", FUNCTION_NAME, pDiskIndexList->aDiskIndex[ulIndex]);
            diskIndexList.insert(pDiskIndexList->aDiskIndex[ulIndex]);
        }
    }
    else {
        DebugPrintf(SV_LOG_ERROR, "IOCTL_INMAGE_GET_DISK_INDEX_LIST failed for device %s err=%d\n",
            INMAGE_DISK_FILTER_DOS_DEVICE_NAME,
            GetLastError());
    }

    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", FUNCTION_NAME);
    return (bSuccess == TRUE);
}

bool InmageDriverInterface::StopFilteringAll()
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);

    if (INVALID_HANDLE_VALUE == m_hInmageDevice) {
        DebugPrintf(SV_LOG_ERROR, "%s: Device handle is invalid\n", FUNCTION_NAME);
        return false;
    }

    STOP_FILTERING_INPUT stopFilteringInput = { 0 };
    stopFilteringInput.ulFlags = (STOP_ALL_FILTERING_FLAGS | STOP_FILTERING_FLAGS_DELETE_BITMAP);

    DWORD dwBytes;
    BOOL bSuccess = DeviceIoControl(m_hInmageDevice, IOCTL_INMAGE_STOP_FILTERING_DEVICE, &stopFilteringInput, sizeof(stopFilteringInput), NULL, 0, &dwBytes, NULL);
    if (bSuccess) {
        DebugPrintf(SV_LOG_ALWAYS, "Filtering Stopped for all devices\n");
    }
    else {
        DWORD dwError = GetLastError();
        if (ERROR_FILE_NOT_FOUND == dwError) {
            DebugPrintf(SV_LOG_ALWAYS, "None of Devices are protected\n");
            bSuccess = TRUE;
        }
        else {
            DebugPrintf(SV_LOG_ERROR, "IOCTL_INMAGE_STOP_FILTERING_DEVICE failed with err=%d\n", dwError);
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", FUNCTION_NAME);
    return (bSuccess == TRUE);
}

bool    InmageDriverInterface::GetDriverStats()
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);

    ULONG   ulSize = sizeof(VOLUME_STATS_DATA) + MAX_NUM_DISKS_SUPPORTED * sizeof(VOLUME_STATS);
    std::vector<UCHAR>      Buffer(ulSize, 0);

    if (INVALID_HANDLE_VALUE == m_hInmageDevice) {
        DebugPrintf(SV_LOG_ERROR, "%s: Device handle is invalid\n", FUNCTION_NAME);
        return false;
    }

    PVOLUME_STATS_DATA  pVolumeStatsData = (PVOLUME_STATS_DATA) &Buffer[0];

    DWORD dwBytes;
    BOOL bSuccess = DeviceIoControl(m_hInmageDevice, IOCTL_INMAGE_GET_VOLUME_STATS, NULL, 0, pVolumeStatsData, ulSize, &dwBytes, NULL);
    if (bSuccess) {
        DebugPrintf(SV_LOG_ALWAYS, "Received stats for %d devices\n", pVolumeStatsData->ulVolumesReturned);
        ULONG   ulBytesUsed = sizeof(VOLUME_STATS_DATA);

        for (ULONG ulIdx = 0; ulIdx < pVolumeStatsData->ulVolumesReturned; ulIdx++) {
            PVOLUME_STATS   pVolumeStats = (PVOLUME_STATS)((PUCHAR)pVolumeStatsData + ulBytesUsed);
            std::wstring    wDeviceId(pVolumeStats->VolumeGUID);
            std::string     sDeviceId(wDeviceId.begin(), wDeviceId.end());
            DebugPrintf(SV_LOG_DEBUG, "Disk%2d: DeviceId: %s\n", ulIdx, sDeviceId.c_str());
            ulBytesUsed += sizeof(VOLUME_STATS);
        }

    }
    else {
        DWORD dwError = GetLastError();
        DebugPrintf(SV_LOG_ERROR, "IOCTL_INMAGE_GET_VOLUME_STATS failed with err=%d\n", dwError);
    }

    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", FUNCTION_NAME);
    return (bSuccess == TRUE);
}

bool    InmageDriverInterface::GetDriverStats(std::string   sDeviceId)
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);

    ULONG   ulSize = sizeof(VOLUME_STATS_DATA) + MAX_NUM_DISKS_SUPPORTED * sizeof(VOLUME_STATS);
    std::vector<UCHAR>      Buffer(ulSize, 0);
    std::wstring            wsDviceId(sDeviceId.begin(), sDeviceId.end());

    if (INVALID_HANDLE_VALUE == m_hInmageDevice) {
        DebugPrintf(SV_LOG_ERROR, "%s: Device handle is invalid\n", FUNCTION_NAME);
        return false;
    }

    PVOLUME_STATS_DATA  pVolumeStatsData = (PVOLUME_STATS_DATA)&Buffer[0];

    DWORD dwBytes;
    BOOL bSuccess = DeviceIoControl(m_hInmageDevice, IOCTL_INMAGE_GET_VOLUME_STATS, (PVOID) (wsDviceId.c_str()), sizeof(wsDviceId.c_str()), pVolumeStatsData, ulSize, &dwBytes, NULL);
    if (bSuccess) {
        DebugPrintf(SV_LOG_ALWAYS, "Received stats for %d devices\n", pVolumeStatsData->ulVolumesReturned);
    }
    else {
        DWORD dwError = GetLastError();
        DebugPrintf(SV_LOG_ERROR, "IOCTL_INMAGE_GET_VOLUME_STATS failed with err=%d\n", dwError);
    }

    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", FUNCTION_NAME);
    return (bSuccess == TRUE);
}