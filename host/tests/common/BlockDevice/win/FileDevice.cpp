// BlockDevice.cpp : Defines the exported functions for the DLL application.
//


#include "PlatformAPIs.h"
#include "CFileDevice.h"
#include "InmageTestException.h"
#include <iostream>
#include <string.h> 
#include <cstdlib>

volatile SV_ULONG CRawFile::s_gFileNumber = 1000;

CRawFile::CRawFile(std::string szFilePath, SV_ULONGLONG ullMaxSize, IPlatformAPIs* platformAPIs):
m_fileName(szFilePath),
m_ullDiskSize(ullMaxSize),
m_platformAPIs(platformAPIs),
m_ullFileSize(0)
{
	m_fileNumber = InterlockedIncrement(&s_gFileNumber);
	Open();
}

CRawFile::~CRawFile()
{
	m_platformAPIs->SafeClose(m_hFile);
}

void CRawFile::Open()
{
	if (m_platformAPIs->PathExists(m_fileName.c_str()))
	{
		m_hFile = m_platformAPIs->Open(m_fileName,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL);

		if (!m_platformAPIs->IsValidHandle(m_hFile))
		{
			throw CBlockDeviceException("Failed to open file %s err=0x%x", m_fileName, m_platformAPIs->GetLastError());
		}

		if (m_ullDiskSize == GetDeviceSize())
		{
			return;
		}

		m_platformAPIs->SafeClose(m_hFile);
		if (remove(m_fileName.c_str()))
		{
			throw CBlockDeviceException("%s: File Size mismatch.. file: %s size=0x%I64x DiskSize=0x%I64x err=%d", __FUNCTION__, m_fileName.c_str(), m_ullFileSize, m_ullDiskSize, m_platformAPIs->GetLastError());
		}
	}
	
	m_hFile = m_platformAPIs->Open(m_fileName,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		CREATE_NEW,
		FILE_ATTRIBUTE_NORMAL);

	if (!m_platformAPIs->IsValidHandle(m_hFile))
	{
		throw CBlockDeviceException("Failed to open file %s err=0x%x", m_fileName, m_platformAPIs->GetLastError());
	}
	

	LARGE_INTEGER lSize = { 0 };
	lSize.QuadPart = m_ullDiskSize;

	if (!SetFilePointerEx(m_hFile, lSize, NULL, FILE_BEGIN))
	{
		throw CBlockDeviceException("%s SetFilePointerEx Failed for file %s size=0x%x err=0x%x", __FUNCTION__, m_fileName.c_str(), m_ullDiskSize, GetLastError());
	}

	if (!SetEndOfFile(m_hFile))
	{
		throw CBlockDeviceException("%s SetEndOfFile Failed for file %s size=0x%x err=0x%x", __FUNCTION__, m_fileName.c_str(), m_ullDiskSize, GetLastError());
	}

	m_ullFileSize = m_ullDiskSize;
}


DWORD CRawFile::GetDeviceNumber()
{
	return m_fileNumber;
}

DWORD   CRawFile::GetMaxRwSizeInBytes()
{
	return (DWORD)(8*1024 * 1024);
}

bool	CRawFile::Online(bool readOnly)
{
	return true;
}

bool	CRawFile::Offline(bool readOnly)
{
	return true;
}

DWORD	CRawFile::GetCopyBlockSize()
{
	return (1024*1024);
}

ULONGLONG	CRawFile::GetDeviceSize()
{
	if (0 != m_ullFileSize)
	{
		return m_ullFileSize;
	}

	LARGE_INTEGER	liSize = { 0 };
	if (!GetFileSizeEx(m_hFile, &liSize))
	{
		throw CBlockDeviceException("%s: GetFileSizeEx %s failed with err=0x%x", __FUNCTION__, m_fileName.c_str(), m_platformAPIs->GetLastError());
	}

	m_ullFileSize = liSize.QuadPart;
	return m_ullFileSize;
}

bool CRawFile::Read(void* buffer, SV_ULONG dwBytesToRead, SV_ULONG& dwBytesRead)
{
	bool bRet = m_platformAPIs->Read(m_hFile, buffer, dwBytesToRead, dwBytesRead);
	if (!bRet || (dwBytesToRead != dwBytesRead))
	{
		throw CBlockDeviceException("%s Err: File read failed with error=0x%x dwBytesToRead=0x%x dwBytesRead=0x%x", __FUNCTION__, GetLastError(), dwBytesToRead, dwBytesRead);
	}

	return bRet;
}

bool    CRawFile::Write(const void* buffer, SV_ULONG dwBytesToWrite, SV_ULONG& dwBytesWritten)
{
	bool bRet = m_platformAPIs->Write(m_hFile, buffer, dwBytesToWrite, dwBytesWritten);
	if (!bRet || (dwBytesToWrite != dwBytesWritten))
	{
		throw CBlockDeviceException("%s Err: File Write failed with error=0x%x dwBytesToWrite=0x%x dwBytesWritten=0x%x", __FUNCTION__, GetLastError(), dwBytesToWrite, dwBytesWritten);
	}
	return bRet;
}

bool CRawFile::DeviceIoControlSync(
	_In_        SV_ULONG        dwIoControlCode,
	_In_opt_    void*       lpInBuffer,
	_In_        SV_ULONG        nInBufferSize,
	_Out_opt_   void*       lpOutBuffer,
	_In_        SV_ULONG        nOutBufferSize,
	_Out_opt_   SV_ULONG*      lpBytesReturned
	)
{
	return m_platformAPIs->DeviceIoControlSync(m_hFile, dwIoControlCode, lpInBuffer, nInBufferSize, lpOutBuffer, nOutBufferSize, lpBytesReturned);
}

bool CRawFile::SeekFile(FileMoveMethod dwMoveMethod, SV_ULONGLONG ullOffset)
{
	return m_platformAPIs->SeekFile(m_hFile, dwMoveMethod, ullOffset);
}

std::string	CRawFile::GetDeviceName()
{
	return m_fileName;
}

std::string	CRawFile::GetDeviceId()
{
	return m_fileName;
}