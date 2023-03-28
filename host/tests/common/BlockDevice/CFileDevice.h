
#ifndef CRAWFILE_H
#define CRAWFILE_H

#include "IBlockDevice.h"
#include "PlatformAPIs.h"
#include "DiskDeviceMajor.h"

#include <string>

class BLOCKDEVICE_API CRawFile : public IBlockDevice
{
private:
    HANDLE          m_hFile;
    std::string     m_fileName;
    std::string     m_fileId;
    SV_ULONG        m_dwBlockSize;
    IPlatformAPIs*  m_platformAPIs;
    SV_ULONGLONG    m_ullDiskSize;
    SV_ULONGLONG    m_ullFileSize;
    void            Open();
    SV_ULONG        m_fileNumber;
#ifdef SV_UNIX
    boost::mutex    m_mutex;
#endif

protected:
    static volatile SV_ULONG    s_gFileNumber;

public:
#ifdef SV_UNIX
    CRawFile(std::string fileName, SV_ULONGLONG ullMaxSize, IPlatformAPIs* platformAPIs);
#else
    CRawFile(std::string fileName, ULONGLONG ullMaxSize, IPlatformAPIs* platformAPIs);
#endif
    ~CRawFile();
    bool            Online(bool readOnly);
    bool            Offline(bool readonly);
    SV_ULONG        GetCopyBlockSize();
    SV_ULONGLONG    GetDeviceSize();
    SV_ULONG        GetMaxRwSizeInBytes();
    std::string     GetDeviceName();
    SV_ULONG        GetDeviceNumber();

    bool            Read(void* buffer, SV_ULONG ulBytesToRead, SV_ULONG& ulBytesRead);
    bool            Write(const void* buffer, SV_ULONG ulBytesToWrite, SV_ULONG& ulBytesWritten);

    bool DeviceIoControlSync(
        _In_        SV_ULONG    dwIoControlCode,
        _In_opt_    void*       lpInBuffer,
        _In_        SV_ULONG    nInBufferSize,
        _Out_opt_   void*       lpOutBuffer,
        _In_        SV_ULONG    nOutBufferSize,
        _Out_opt_   SV_ULONG*   lpBytesReturned
    );

    bool            SeekFile(FileMoveMethod dwMoveMethod, SV_ULONGLONG offset);
    std::string     GetDeviceId();
};

#endif
