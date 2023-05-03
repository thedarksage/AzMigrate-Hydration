// Win32APIs.cpp : CWin32APIs implementation 
//

#include "PlatformAPIs.h"
#include "InmageTestException.h"
#include <Shlwapi.h>


bool CWin32APIs::DeviceIoControlSync(
	void*		hDevice,
	SV_ULONG        dwIoControlCode,
	void*       lpInBuffer,
	SV_ULONG        nInBufferSize,
	void*       lpOutBuffer,
	SV_ULONG        nOutBufferSize,
	SV_ULONG*      lpBytesReturned)
{
	return (TRUE == ::DeviceIoControl(hDevice, dwIoControlCode, lpInBuffer, nInBufferSize, lpOutBuffer, nOutBufferSize, lpBytesReturned, NULL));
}

bool CWin32APIs::DeviceIoControl(
	void* hDevice,
	SV_ULONG        dwIoControlCode,
	void*       lpInBuffer,
	SV_ULONG        nInBufferSize,
	void*       lpOutBuffer,
	SV_ULONG        nOutBufferSize,
	SV_ULONG*      lpBytesReturned,
	void*	voverlapped)
{
	LPOVERLAPPED	overlapped = (LPOVERLAPPED)voverlapped;
	return (TRUE == ::DeviceIoControl(hDevice, dwIoControlCode, lpInBuffer, nInBufferSize, lpOutBuffer, nOutBufferSize, lpBytesReturned, overlapped));
}

bool CWin32APIs::Read(void* hDevice, void* buffer, SV_ULONG dwBytesToRead, SV_ULONG& dwBytesRead)
{
    return (TRUE == ::ReadFile(hDevice, buffer, dwBytesToRead, &dwBytesRead, NULL));
}

bool CWin32APIs::Write(void* hDevice, const void* buffer, SV_ULONG dwBytesToWrite, SV_ULONG& dwBytesWritten)
{
    return (TRUE == ::WriteFile(hDevice, buffer, dwBytesToWrite, &dwBytesWritten, NULL));
}

bool    CWin32APIs::SeekFile(void* hDevice, FileMoveMethod dwMoveMethod, SV_ULONGLONG ullOffset)
{
	LARGE_INTEGER	liOffset = { 0 };
	liOffset.QuadPart = ullOffset;

	SV_ULONG dwFilePtr = (dwMoveMethod == BEGIN) ? FILE_BEGIN : (dwMoveMethod == CURRENT) ? FILE_CURRENT : FILE_END;
	return (TRUE == ::SetFilePointerEx(hDevice, liOffset, NULL, dwFilePtr));
}

void* CWin32APIs::OpenDefault(std::string szDeviceName)
{
    return ::CreateFileA(szDeviceName.c_str(), 
						GENERIC_READ|GENERIC_WRITE, 
						FILE_SHARE_READ | FILE_SHARE_WRITE, 
						NULL, 
						OPEN_EXISTING,
						FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,
						NULL);
}

void* CWin32APIs::Open(std::string szDeviceName, SV_ULONG access, SV_ULONG mode, SV_ULONG creationDeposition, SV_ULONG flags)
{
	return ::CreateFileA(szDeviceName.c_str(),
		access,
		mode,
		NULL,
		creationDeposition,
		flags,
		NULL);
}

bool CWin32APIs::Close(void* hDevice)
{
	return (TRUE == ::CloseHandle(hDevice));
}

bool CWin32APIs::SafeClose(void* hDevice)
{
	if (INVALID_HANDLE_VALUE != hDevice)
	{
		return (TRUE == ::CloseHandle(hDevice));
	}

	return true;
}

SV_ULONG	CWin32APIs::GetLastError()
{
	return ::GetLastError();
}

bool CWin32APIs::PathExists(const char* path)
{
    return (TRUE == ::PathFileExistsA(path));
}

bool CWin32APIs::IsValidHandle(void* handle)
{
	return (INVALID_HANDLE_VALUE != handle);
}

void CWin32APIs::GetVolumeMappings(void* hDevice, SV_LONGLONG   llStartingVcn, void* retBuffer, SV_ULONG ulBufferSize)
{
    STARTING_VCN_INPUT_BUFFER   startingVcn = { 0 };
    bool                        bDone = true;
    SV_ULONG                    dwBytes;

    startingVcn.StartingVcn.QuadPart = llStartingVcn;

    BOOL    bStatus = DeviceIoControlSync(
        hDevice,
        FSCTL_GET_RETRIEVAL_POINTERS,
        &startingVcn,
        sizeof(startingVcn),
        retBuffer,
        ulBufferSize,
        &dwBytes);

    if (!bStatus) {
        SV_ULONG err = GetLastError();
        if (ERROR_MORE_DATA != err) {
            throw CInmageExcludeBlocksException("%s: FSCTL_GET_RETRIEVAL_POINTERS failed with error=0x%x", __FUNCTION__, GetLastError());
        }
    }
}