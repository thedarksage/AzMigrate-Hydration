///
///  \file  CUnixAPIs.h
///
///  \brief contains Unix PlatformAPIs class decalrations
///

#ifndef CUNIXAPIS_H
#define CUNIXAPIS_H

#include "IPlatformAPIs.h"
#include "boost/shared_ptr.hpp"
#include <string.h>

class PlatformAPIs : public IPlatformAPIs
{
    bool m_directIO;
public:
    PlatformAPIs(bool directIO=false);
    bool DeviceIoControlSync(
        HANDLE          hDevice,
        unsigned long   dwIoControlCode,
        void*           lpInBuffer,
        unsigned long   nInBufferSize,
        void*           lpOutBuffer,
        unsigned long   nOutBufferSize,
        unsigned long*  lpBytesReturned);

    bool DeviceIoControl(
        HANDLE          hDevice,
        unsigned long   dwIoControlCode,
        void*           lpInBuffer,
        unsigned long   nInBufferSize,
        void*           lpOutBuffer,
        unsigned long   nOutBufferSize,
        unsigned long*  lpBytesReturned,
        LPOVERLAPPED    overlapped);

    bool Fsync(HANDLE hDevice);
    bool Read(HANDLE hDevice, void* buffer,
        unsigned long dwBytesToRead, unsigned long& dwBytesRead);
    bool Write(HANDLE hDevice, const void* buffer, unsigned long dwBytesToBytes,
        unsigned long& dwBytesWritten);
    bool SeekFile(HANDLE hDevice, FileMoveMethod dwMoveMethod, unsigned long long offset);
    bool Close(HANDLE hDevice);
    bool SafeClose(HANDLE hDevice);
    unsigned long GetLastError();
    void* CreateEvent(void* lpEventAttributes, bool bManualReset,
        bool bInitialState, wchar_t* lpName);
    bool PathExists(const char* path);
    HANDLE Open(std::string szDevice, SV_ULONG access, SV_ULONG mode,
        SV_ULONG creationDeposition, SV_ULONG flags);
    HANDLE OpenDefault(std::string szDevice);
    bool IsValidHandle(HANDLE handle);
};
#endif /* CUNIXAPIS_H */
