///
///  \file  CDiskDevice.h
///
///  \brief contains CBlockDevice class declarations 
///

#ifndef DISKDEVICE_H
#define DISKDEVICE_H

#include "IBlockDevice.h"
#include "IPlatformAPIs.h"
#include "DiskDeviceMajor.h"


class BLOCKDEVICE_API CDiskDevice : public IBlockDevice
{
private:
#ifdef SV_WINDOWS
    void*                   m_hDisk;
#else
    HANDLE                  m_hDisk;
#endif
    int                     m_diskNumber;
    std::string             m_diskName;
    std::string             m_diskId;
    SV_ULONG                m_dwBlockSize;
    IPlatformAPIs*          m_platformAPIs;
    SV_ULONGLONG            m_ullDiskSize;
    bool                    DiskOnlineOffline(bool online, bool readonly, bool persist);
    void                    Open();
    void                    PopulateDiskInfo();

public:
    CDiskDevice(int diskNumber, IPlatformAPIs* platformAPIs);
#ifdef SV_WINDOWS
    CDiskDevice(wchar_t* wszDiskName, IPlatformAPIs* platformAPIs);
#else
    CDiskDevice(const std::string &strDiskName, IPlatformAPIs* platformAPIs);
#endif

    ~CDiskDevice();
    bool            Online(bool readOnly);
    bool            Offline(bool readonly);
    SV_ULONG        GetCopyBlockSize();
    SV_ULONGLONG    GetDeviceSize();
#ifdef SV_UNIX
    SV_ULONG        GetDevBlockSize();
    bool            Fsync();
#endif
    SV_ULONG        GetMaxRwSizeInBytes();
    std::string     GetDeviceName();
    SV_ULONG        GetDeviceNumber();
    bool            Read(void* buffer, SV_ULONG dwBytesToRead, SV_ULONG& dwBytesRead);
    bool            Write(const void* buffer, SV_ULONG dwBytesToBytes, SV_ULONG& dwBytesWritten);

    bool DeviceIoControlSync(
        SV_ULONG    dwIoControlCode,
        void*       lpInBuffer,
        SV_ULONG    nInBufferSize,
        void*       lpOutBuffer,
        SV_ULONG    nOutBufferSize,
        SV_ULONG*   lpBytesReturned
    );

    bool            SeekFile(FileMoveMethod dwMoveMethod, SV_ULONGLONG offset);
    std::string     GetDeviceId();
};
#endif /* DISKDEVICE_H */


