#include "stdafx.h"
#include "deviceutils.h"

DeviceStream::DeviceStream() :
m_handle(INVALID_HANDLE_VALUE)
{}

DeviceStream::DeviceStream(std::string deviceName) :
m_handle(INVALID_HANDLE_VALUE),
m_deviceName(deviceName)
{}

DeviceStream::~DeviceStream()
{
    SafeCloseHandle(m_handle);
}

DWORD DeviceStream::Open(std::string deviceName)
{
    m_deviceName = deviceName;
    return Open();
}

DWORD DeviceStream::Open()
{
    DWORD               dwStatus = ERROR_SUCCESS;
    std::stringstream   ssErrorMessage;

    if (INVALID_HANDLE_VALUE != m_handle) {
        return dwStatus;
    }

    m_handle = CreateFile(m_deviceName.c_str(), 
                         GENERIC_READ,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL,
                         OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL,
                         NULL);

    if (INVALID_HANDLE_VALUE == m_handle) {
        dwStatus = GetLastError();
        ssErrorMessage << "Failed to open handle for device " << m_deviceName << " err=" << dwStatus;
        m_errorMessage = ssErrorMessage.str();
    }

    return dwStatus;
}

VOID DeviceStream::Close()
{
    SafeCloseHandle(m_handle);
}

DWORD DeviceStream::Ioctl(DWORD dwIoCtlCtrlCode)
{
    return Ioctl(dwIoCtlCtrlCode, NULL, 0);
}

DWORD DeviceStream::Ioctl(DWORD dwIoCtlCtrlCode, PVOID pBuffer, DWORD dwBufferLength)
{
    return Ioctl(dwIoCtlCtrlCode, pBuffer, dwBufferLength, NULL, 0);
}

DWORD DeviceStream::Ioctl(DWORD dwIoCtlCtrlCode, PVOID pInBuffer, DWORD dwInBufferLength, PVOID pOutBuffer, DWORD dwOutBufferLength)
{
    DWORD dwBytes;
    DWORD               dwStatus = ERROR_SUCCESS;
    std::stringstream   ssErrorMessage;

    if (!DeviceIoControl(m_handle, dwIoCtlCtrlCode, pInBuffer, dwInBufferLength, pOutBuffer, dwOutBufferLength, &dwBytes, NULL)) {
        dwStatus = GetLastError();
        ssErrorMessage << "Ioctl " << dwIoCtlCtrlCode << " failed for device " << m_deviceName << " err=" << dwStatus;
        m_errorMessage = ssErrorMessage.str();
    }

    return dwStatus;
}

std::string DeviceStream::GetErrorMessage()
{
    return m_errorMessage;
}