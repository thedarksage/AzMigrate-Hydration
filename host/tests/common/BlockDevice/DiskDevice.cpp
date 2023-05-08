// DiskDevice.cpp : CDiskDevice implementation 
//

#include "PlatformAPIs.h"
#include "CDiskDevice.h"
#include "InmageTestException.h"
#include <iostream>
#include <string.h> 
#include <cstdlib>

using namespace std;

void CDiskDevice::Open()
{
    m_hDisk = m_platformAPIs->OpenDefault(m_diskName);
    if (!m_platformAPIs->IsValidHandle(m_hDisk))
    {
        throw CBlockDeviceException("m_platformAPIs->OpenDefault failed with error=%d", m_platformAPIs->GetLastError());
    }
}

SV_ULONG CDiskDevice::GetDeviceNumber()
{
    return m_diskNumber;
}

SV_ULONG CDiskDevice::GetMaxRwSizeInBytes()
{
    return (SV_ULONG)(8*1024 * 1024);
}


SV_ULONG CDiskDevice::GetCopyBlockSize()
{
    return m_dwBlockSize;
}

SV_ULONGLONG CDiskDevice::GetDeviceSize()
{
    return m_ullDiskSize;
}

bool CDiskDevice::Read(void* buffer, SV_ULONG ulBytesToRead, SV_ULONG& ulBytesRead)
{
    bool bRet = m_platformAPIs->Read(m_hDisk, buffer, ulBytesToRead, ulBytesRead);
    if (!bRet )
    {
        throw  CBlockDeviceException(
            "%s Error: Disk read failed with error=0x%x ulBytesToRead=0x%x ulBytesRead=0x%x",
            __FUNCTION__, m_platformAPIs->GetLastError(), ulBytesToRead, ulBytesRead);
    }

    return bRet;
}

#ifdef SV_UNIX
SV_ULONG CDiskDevice::GetDevBlockSize()
{
    return m_dwBlockSize;
}

bool CDiskDevice::Fsync()
{
    return m_platformAPIs->Fsync(m_hDisk);
}
#endif

bool CDiskDevice::Write(const void* buffer, SV_ULONG ulBytesToWrite, SV_ULONG& ulBytesWritten)
{
    bool bRet = m_platformAPIs->Write(m_hDisk, buffer, ulBytesToWrite, ulBytesWritten);
    if (!bRet)
    {
        throw  CBlockDeviceException(
            "%s Error: Disk Write failed with error=0x%x ulBytesToWrite=0x%x ulBytesWritten=0x%x",
            __FUNCTION__, m_platformAPIs->GetLastError(), ulBytesToWrite, ulBytesWritten);
    }
    return bRet;
}

bool CDiskDevice::DeviceIoControlSync(
        SV_ULONG    ulIoControlCode,
        void*       lpInBuffer,
        SV_ULONG    nInBufferSize,
        void*       lpOutBuffer,
        SV_ULONG    nOutBufferSize,
        SV_ULONG*   lpBytesReturned
    )
{
    return m_platformAPIs->DeviceIoControlSync(m_hDisk, ulIoControlCode,
        lpInBuffer, nInBufferSize, lpOutBuffer, nOutBufferSize, lpBytesReturned);
}

bool CDiskDevice::SeekFile(FileMoveMethod ulMoveMethod, SV_ULONGLONG ullOffset)
{
    return m_platformAPIs->SeekFile(m_hDisk, ulMoveMethod, ullOffset);
}

std::string CDiskDevice::GetDeviceName()
{
    return m_diskName;
}

bool CDiskDevice::Online(bool readOnly)
{
    return DiskOnlineOffline(true, readOnly, false);
}

bool CDiskDevice::Offline(bool readOnly)
{
    return DiskOnlineOffline(false, readOnly, false);
}

CDiskDevice::~CDiskDevice()
{
    m_platformAPIs->SafeClose(m_hDisk);
}

