#ifndef _DEVICE_UTILS_H_
#define _DEVICE_UTILS_H_

#include "stdafx.h"

#include <Windows.h>
#include <string>
#include <sstream>
#include <vector>

#define SafeCloseHandle(h)          if (INVALID_HANDLE_VALUE != h) {CloseHandle(h); h = INVALID_HANDLE_VALUE;}
#define SafeFree(x)                 if (NULL != x) {free(x); x = NULL;}
#define DISK_NAME_PREFIX           "\\\\.\\PhysicalDrive"

class DeviceStream {
protected:
    HANDLE          m_handle;
    std::string     m_deviceName;
    std::string     m_errorMessage;

public:
    DeviceStream();

    DeviceStream(std::string deviceName);

    virtual ~DeviceStream();

    DWORD Open(std::string deviceName);

    DWORD Open();

    VOID  Close();

    DWORD Ioctl(DWORD dwIoCtlCtrlCode);

    DWORD Ioctl(DWORD dwIoCtlCtrlCode, PVOID pBuffer, DWORD dwBufferLength);

    DWORD Ioctl(DWORD dwIoCtlCtrlCode, PVOID pInBuffer, DWORD dwInBufferLength, PVOID pOutBuffer, DWORD dwOutBufferLength);
    virtual std::string GetErrorMessage();
};

#endif