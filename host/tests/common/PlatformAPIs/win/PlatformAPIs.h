///
///  \file  PlatformAPIs.h
///
///  \brief contains PlatformAPIs declarations for windows
///



#ifndef PLATFORMAPIS_H
#define PLATFORMAPIS_H

#include "IPlatformAPIs.h"
#include "PlatformAPIsMajor.h"

class PLATFORMAPIS_API CWin32APIs : public IPlatformAPIs
{
public:
	bool DeviceIoControlSync(
		void* hDevice,
		unsigned long        dwIoControlCode,
		void*       lpInBuffer,
		unsigned long        nInBufferSize,
		void*       lpOutBuffer,
		unsigned long        nOutBufferSize,
		unsigned long*      lpBytesReturned);

	bool DeviceIoControl(
		void* hDevice,
		unsigned long        dwIoControlCode,
		void*       lpInBuffer,
		unsigned long        nInBufferSize,
		void*       lpOutBuffer,
		unsigned long        nOutBufferSize,
		unsigned long*      lpBytesReturned,
		void*	overlapped);

	bool    Read(void* hDevice, void* buffer, unsigned long dwBytesToRead, unsigned long& dwBytesRead);
	bool    Write(void* hDevice, const void* buffer, unsigned long dwBytesToBytes, unsigned long& dwBytesWritten);
	bool    SeekFile(void* hDevice, FileMoveMethod dwMoveMethod, unsigned long long offset);
	void*	OpenDefault(std::string szDevice);
	virtual void*		Open(std::string szDevice, SV_ULONG access, SV_ULONG mode, SV_ULONG creationDeposition, SV_ULONG flags);
	bool	Close(void* hDevice);
	bool	SafeClose(void* hDevice);
	SV_ULONG	GetLastError();
    bool PathExists(const char* path);
	bool IsValidHandle(void* handle);
    void        GetVolumeMappings(void* hDevice, SV_LONGLONG   llStartingVcn, void* retBuffer, SV_ULONG ulBufferSize);
};

#endif /* PLATFORMAPIS_H */