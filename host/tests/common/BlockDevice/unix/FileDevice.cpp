// BlockDevice.cpp : Defines the exported functions for the DLL application.
//


#include <iostream>
#include <string.h> 
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "boost/thread/mutex.hpp"
#include "boost/filesystem.hpp"
#include "PlatformAPIs.h"
#include "CFileDevice.h"
#include "InmageTestException.h"

volatile SV_ULONG CRawFile::s_gFileNumber = 1000;

CRawFile::CRawFile(std::string szFilePath, SV_ULONGLONG ullMaxSize, IPlatformAPIs* platformAPIs):
m_fileName(szFilePath),
m_ullFileSize(ullMaxSize),
m_platformAPIs(platformAPIs)
{ 
    boost::mutex::scoped_lock lock(m_mutex);
    m_fileNumber = ++s_gFileNumber; 
    Open();
}

CRawFile::~CRawFile()
{
    m_platformAPIs->SafeClose(m_hFile);
}

void CRawFile::Open()
{
    unsigned long long fileSize = 0;
    if (m_platformAPIs->PathExists(m_fileName.c_str()))
    {
        m_hFile = m_platformAPIs->Open(m_fileName, O_RDWR | O_CREAT, 0, 0, 0);

        if (!m_platformAPIs->IsValidHandle(m_hFile))
        {
            throw CBlockDeviceException("Failed to open file %s err=0x%x",
                m_fileName.c_str(), m_platformAPIs->GetLastError());
        }

        if (m_ullFileSize == (fileSize = GetDeviceSize()))
        {
            return;
        }

        m_platformAPIs->SafeClose(m_hFile);
        if (remove(m_fileName.c_str()))
        {
            throw CBlockDeviceException(
                "%s: Error in removing/deleting the file and also File Size mismatch.."
                "file: %s size=0x%I64x DiskSize=0x%I64x err=%d",
                __FUNCTION__, m_fileName.c_str(), m_ullFileSize,
                m_ullDiskSize, m_platformAPIs->GetLastError());
        }
    } 
    
    m_hFile = m_platformAPIs->Open(m_fileName, O_RDWR | O_CREAT, 0, 0, 0);
    if (!m_platformAPIs->IsValidHandle(m_hFile))
    {
        throw CBlockDeviceException("Failed to open file %s err=0x%x",
            m_fileName.c_str(), m_platformAPIs->GetLastError());
    }

    off_t location = lseek(m_hFile, BEGIN, SEEK_SET);
    if (-1 == location)
    {
        throw CBlockDeviceException("%s SeekFile Failed for file %s size=0x%x err=0x%x",
            __FUNCTION__, m_fileName.c_str(), m_ullDiskSize, m_platformAPIs->GetLastError());
    }

    if(0 != ftruncate(m_hFile,m_ullFileSize))
    {
        throw CBlockDeviceException("%s SetEndOfFile Failed for file %s size=0x%x err=0x%x",
            __FUNCTION__, m_fileName.c_str(), m_ullDiskSize, m_platformAPIs->GetLastError());
    }

    m_ullDiskSize = m_ullFileSize;
}


SV_ULONG CRawFile::GetDeviceNumber()
{
    return m_fileNumber;
}

SV_ULONG CRawFile::GetMaxRwSizeInBytes()
{
    return (SV_ULONG)(8 * 1024 * 1024);
}

bool CRawFile::Online(bool readOnly)
{
    return true;
}

bool CRawFile::Offline(bool readOnly)
{
    return true;
}

SV_ULONG CRawFile::GetCopyBlockSize()
{
    return (1024*1024);
}

SV_ULONGLONG CRawFile::GetDeviceSize()
{
    if (0 != m_ullFileSize)
    {
        return m_ullFileSize;
    }

    boost::filesystem::path filepath = m_fileName;
    m_ullFileSize =  boost::filesystem::file_size(m_fileName);

    if (m_ullFileSize < 0)
    {
        throw CBlockDeviceException("%s: GetDeviceSize %s failed with err=0x%x",
            __FUNCTION__, m_fileName.c_str(), m_platformAPIs->GetLastError());
    }

    return m_ullFileSize;
}

bool CRawFile::Read(void* buffer, SV_ULONG dwBytesToRead, SV_ULONG& dwBytesRead)
{
    bool bRet = m_platformAPIs->Read(m_hFile, buffer, dwBytesToRead, dwBytesRead);
    if (!bRet || (dwBytesToRead != dwBytesRead))
    {
        throw CBlockDeviceException(
            "%s Err: File read failed with error=0x%x dwBytesToRead=0x%x dwBytesRead=0x%x",
            __FUNCTION__, m_platformAPIs->GetLastError(), dwBytesToRead, dwBytesRead);
    }

    return bRet;
}

bool CRawFile::Write(const void* buffer, SV_ULONG dwBytesToWrite, SV_ULONG& dwBytesWritten)
{
    bool bRet = m_platformAPIs->Write(m_hFile, buffer, dwBytesToWrite, dwBytesWritten);
    if (!bRet || (dwBytesToWrite != dwBytesWritten))
    {
        throw CBlockDeviceException(
            "%s Err: File Write failed with error=0x%x dwBytesToWrite=0x%x dwBytesWritten=0x%x",
            __FUNCTION__, m_platformAPIs->GetLastError(), dwBytesToWrite, dwBytesWritten);
    }
    return bRet;
}

bool CRawFile::DeviceIoControlSync(
    _In_        SV_ULONG    dwIoControlCode,
    _In_opt_    void*       lpInBuffer,
    _In_        SV_ULONG    nInBufferSize,
    _Out_opt_   void*       lpOutBuffer,
    _In_        SV_ULONG    nOutBufferSize,
    _Out_opt_   SV_ULONG*   lpBytesReturned
    )
{
    return m_platformAPIs->DeviceIoControlSync(m_hFile, dwIoControlCode, lpInBuffer, nInBufferSize,
        lpOutBuffer, nOutBufferSize, lpBytesReturned);
}

bool CRawFile::SeekFile(FileMoveMethod dwMoveMethod, SV_ULONGLONG ullOffset)
{
    return m_platformAPIs->SeekFile(m_hFile, dwMoveMethod, ullOffset);
}

std::string CRawFile::GetDeviceName()
{
    return m_fileName;
}

std::string CRawFile::GetDeviceId()
{
    return m_fileName;
}
