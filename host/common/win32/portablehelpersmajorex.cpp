#ifndef PORTABLEHELPERS__MAJOREX__H_
#define PORTABLEHELPERS__MAJOREX__H_

#define _WIN32_WINNT 0x0600
#define FUNCTION_NAME __FUNCTION__

#include <windows.h>
#include <devioctl.h>
#include <ntdddisk.h>
#include <sstream>

#include "portablehelpersmajor.h"
#include "logger.h"

#include "scopeguard.h"

SVSTATUS GetDiskHandle(HANDLE& hDisk, const std::string& diskName, std::string& errMsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    std::stringstream   ssError;

    hDisk = SVCreateFile(diskName.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (INVALID_HANDLE_VALUE == hDisk)
    {
        ssError << "Failed to open disk " << diskName << " error: " << GetLastError();
        DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, ssError.str().c_str());
        errMsg = ssError.str();
        status = SVE_FAIL;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

bool IsDiskClustered(const std::string& diskName, bool& bClustered, std::string& errMsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    errMsg.clear();

    bool bRet = false;
    bClustered = false;
    std::stringstream   ssError;

    DWORD  bytesReturned = 0;
    DWORD dwSize = sizeof(bClustered);

    HANDLE hDisk;
    
    do {

        if (SVS_OK != GetDiskHandle(hDisk, diskName, errMsg))
        {
            DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
            break;
        }

        ON_BLOCK_EXIT(&CloseHandle, hDisk);

        if (!DeviceIoControl(hDisk, IOCTL_DISK_IS_CLUSTERED, NULL, 0, &bClustered, dwSize, &bytesReturned, NULL))
        {
            ssError << "IOCTL_DISK_IS_CLUSTERED failed for the disk : " << diskName << " with error : " << GetLastError();
            DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, ssError.str().c_str());

            errMsg = ssError.str();

            break;
        }
        else {

            bRet = true;
        }

    } while (0);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return bRet;
}

bool IsDiskOnline(const std::string& diskName, bool& bOnline, std::string& errMsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    errMsg.clear();

    bool bRet = false;
    bOnline = false;
    std::stringstream   ssError;
    HANDLE hDisk;

    do {

        if (SVS_OK != GetDiskHandle(hDisk, diskName, errMsg))
        {
            DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
            break;
        }

        ON_BLOCK_EXIT(&CloseHandle, hDisk);

        GET_DISK_ATTRIBUTES attributes = { 0 };
        attributes.Version = sizeof(GET_DISK_ATTRIBUTES);

        DWORD dwBytesReturned;

        if (!DeviceIoControl(hDisk, IOCTL_DISK_GET_DISK_ATTRIBUTES, NULL, 0, &attributes, sizeof attributes,
            &dwBytesReturned, NULL))
        {
            ssError << "IOCTL_DISK_GET_DISK_ATTRIBUTES failed for the disk : " << diskName << " with error : " << GetLastError();
            DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, ssError.str().c_str());
            break;
        }
        
        bOnline = !(attributes.Attributes & DISK_ATTRIBUTE_OFFLINE);
        bRet = true;

        DebugPrintf(SV_LOG_DEBUG, " DiskName=%s , IsOnline=%d\n", diskName.c_str(), bOnline);

    } while (0);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bRet;
}
#endif