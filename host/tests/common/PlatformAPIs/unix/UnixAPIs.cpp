// UnixAPIs.cpp : Defines the Platform APIs definations for Unix
//

#include "involflt.h"
#include "ioctl_codes.h"
#include "boost/filesystem.hpp"
#include "PlatformAPIs.h"
#include "InmageTestException.h"
#include "PlatformAPIsMajor.h"
#include <unistd.h> 
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <string.h>

#define IOCTL_INMAGE_GET_PROTECTED_VOLUME_LIST  _IO(FLT_IOCTL, GET_PROTECTED_VOLUME_LIST_CMD)
#define IOCTL_INMAGE_IOBARRIER_TAG_VOLUME       _IOWR(FLT_IOCTL, TAG_CMD_V3, tag_info_t_v2)
#define IOCTL_INMAGE_CREATE_BARRIER_ALL         _IOWR(FLT_IOCTL, CREATE_BARRIER, flt_barrier_create_t)
#define IOCTL_INMAGE_REMOVE_BARRIER_ALL         _IOWR(FLT_IOCTL, REMOVE_BARRIER, flt_barrier_remove_t)
#define IOCTL_INMAGE_TAG_COMMIT_V2              _IOWR(FLT_IOCTL, TAG_COMMIT_V2, flt_tag_commit_t)
#define IOCTL_INMAGE_TAG_VOLUME_V2              _IOWR(FLT_IOCTL, TAG_CMD_V2, tag_info_t_v2)
#define BLKGETSIZE   _IO(0x12,96)    /* return device size */
#define BLKSSZGET    _IO(0x12,104)   /* get block device sector size */
#define BLKGETSIZE64 _IOR(0x12,114,size_t)  /* return device size.  */

const char *
inm_ioctl_str(unsigned long ioctl_code)
{
    switch (ioctl_code) {
    case IOCTL_INMAGE_GET_PROTECTED_VOLUME_LIST:
        return "IOCTL_INMAGE_GET_PROTECTED_VOLUME_LIST";
    break;
    case IOCTL_INMAGE_VOLUME_STACKING:
        return "IOCTL_INMAGE_VOLUME_STACKING";
    break;
    case IOCTL_INMAGE_MIRROR_VOLUME_STACKING:
        return "IOCTL_INMAGE_MIRROR_VOLUME_STACKING";
    break;
    case IOCTL_INMAGE_PROCESS_START_NOTIFY:
        return "IOCTL_INMAGE_PROCESS_START_NOTIFY";
    break;
    case IOCTL_INMAGE_SERVICE_SHUTDOWN_NOTIFY:
        return "IOCTL_INMAGE_SERVICE_SHUTDOWN_NOTIFY";
    break;
    case IOCTL_INMAGE_STOP_FILTERING_DEVICE:
        return "IOCTL_INMAGE_STOP_FILTERING_DEVICE";
    break;
    case IOCTL_INMAGE_START_FILTERING_DEVICE:
        return "IOCTL_INMAGE_START_FILTERING_DEVICE";
    break;
    case IOCTL_INMAGE_START_MIRRORING_DEVICE:
        return "IOCTL_INMAGE_START_MIRRORING_DEVICE";
    break;
    case IOCTL_INMAGE_STOP_MIRRORING_DEVICE:
        return "IOCTL_INMAGE_STOP_MIRRORING_DEVICE";
    break;
    case IOCTL_INMAGE_START_FILTERING_DEVICE_V2:
        return "IOCTL_INMAGE_START_FILTERING_DEVICE_V2";
    break;
    case IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS:
        return "IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS";
    break;
    case IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS:
        return "IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS";
    break;
    case IOCTL_INMAGE_SET_VOLUME_FLAGS:
        return "IOCTL_INMAGE_SET_VOLUME_FLAGS";
    break;
    case IOCTL_INMAGE_GET_VOLUME_FLAGS:
        return "IOCTL_INMAGE_GET_VOLUME_FLAGS";
    break;
    case IOCTL_INMAGE_WAIT_FOR_DB:
        return "IOCTL_INMAGE_WAIT_FOR_DB";
    break;
    case IOCTL_INMAGE_CLEAR_DIFFERENTIALS:
        return "IOCTL_INMAGE_CLEAR_DIFFERENTIALS";
    break;
    case IOCTL_INMAGE_GET_NANOSECOND_TIME:
        return "IOCTL_INMAGE_GET_NANOSECOND_TIME";
    break;
    case IOCTL_INMAGE_UNSTACK_ALL:
        return "IOCTL_INMAGE_UNSTACK_ALL";
    break;
    case IOCTL_INMAGE_SYS_SHUTDOWN:
        return "IOCTL_INMAGE_SYS_SHUTDOWN";
    break;
    case IOCTL_INMAGE_TAG_VOLUME:
        return "IOCTL_INMAGE_TAG_VOLUME";
    break;
    case IOCTL_INMAGE_WAKEUP_ALL_THREADS:
        return "IOCTL_INMAGE_WAKEUP_ALL_THREADS";
    break;
    case IOCTL_INMAGE_GET_DB_NOTIFY_THRESHOLD:
        return "IOCTL_INMAGE_GET_DB_NOTIFY_THRESHOLD";
    break;
    case IOCTL_INMAGE_RESYNC_START_NOTIFICATION:
        return "IOCTL_INMAGE_RESYNC_START_NOTIFICATION";
    break;
    case IOCTL_INMAGE_RESYNC_END_NOTIFICATION:
        return "IOCTL_INMAGE_RESYNC_END_NOTIFICATION";
    break;
    case IOCTL_INMAGE_GET_DRIVER_VERSION:
        return "IOCTL_INMAGE_GET_DRIVER_VERSION";
    break;
    case IOCTL_INMAGE_SHELL_LOG:
        return "IOCTL_INMAGE_SHELL_LOG";
    break;
    case IOCTL_INMAGE_AT_LUN_CREATE:
        return "IOCTL_INMAGE_AT_LUN_CREATE";
    break;
    case IOCTL_INMAGE_AT_LUN_DELETE:
        return "IOCTL_INMAGE_AT_LUN_DELETE";
    break;
    case IOCTL_INMAGE_AT_LUN_LAST_WRITE_VI:
        return "IOCTL_INMAGE_AT_LUN_LAST_WRITE_VI";
    break;
    case IOCTL_INMAGE_IOBARRIER_TAG_VOLUME:
        return "IOCTL_INMAGE_IOBARRIER_TAG_VOLUME";
    break;
    case IOCTL_INMAGE_CREATE_BARRIER_ALL:
        return "IOCTL_INMAGE_CREATE_BARRIER_ALL";
    break;
    case IOCTL_INMAGE_REMOVE_BARRIER_ALL:
        return "IOCTL_INMAGE_REMOVE_BARRIER_ALL";
    break;
    case IOCTL_INMAGE_TAG_COMMIT_V2:
        return "IOCTL_INMAGE_TAG_COMMIT_V2";
    break;
    case IOCTL_INMAGE_TAG_VOLUME_V2:
        return "IOCTL_INMAGE_TAG_VOLUME_V2";
    break;
    case BLKGETSIZE64:
        return "BLKGETSIZE64";
    break;
    case BLKSSZGET:
        return "BLKSSZGET";
    break;
    default:
        return "Unknown";
    break;
    }
}

PlatformAPIs::PlatformAPIs(bool directIO)
{
    m_directIO = directIO;
}

bool PlatformAPIs::DeviceIoControlSync(
    HANDLE          hDevice,
    unsigned long   ulIoControlCode,
    void*           lpInBuffer,
    unsigned long   nInBufferSize,
    void*           lpOutBuffer,
    unsigned long   nOutBufferSize,
    unsigned long*  lpBytesReturned)
{
    bool status = true;
    int ret = ioctl(hDevice, ulIoControlCode, lpInBuffer);
    if(ret)
    {
        std::cout << "ioctl(" << ulIoControlCode
            << ") call failed with error : " << ret << std::endl;
        status = false;
    }

    return status;
}

bool PlatformAPIs::DeviceIoControl(
    HANDLE          hDevice,
    unsigned long   ulIoControlCode,
    void*           lpInBuffer,
    unsigned long   nInBufferSize,
    void*           lpOutBuffer,
    unsigned long   nOutBufferSize,
    unsigned long*  lpBytesReturned,
    LPOVERLAPPED    overlapped)
{
    throw CPlatformException(" No Implementation ");

}

bool PlatformAPIs::Fsync(HANDLE hDevice)
{
        int ret = fsync(hDevice);
        if (ret < 0)
        {
            return false;
        } else
        {
            return true;
        }
}

bool PlatformAPIs::Read(HANDLE hDevice, void* buffer, unsigned long bytesToRead,
    unsigned long& bytesRead)
{
    ssize_t retval  = read(hDevice, buffer, bytesToRead);
    if (-1 != retval)
    {
        bytesRead = retval;
        return true;
    }
    else
    {
        std::cout << "Read failed with error num =" << errno << std::endl;
        return false;
    }
}

bool PlatformAPIs::Write(HANDLE hDevice, const void* buffer, unsigned long bytesToWrite,
    unsigned long& bytesWritten)
{
    ssize_t retval = write(hDevice, buffer, bytesToWrite);
    if (-1 != retval)
    {
        bytesWritten = retval;
        return true;
    }
    else
    {
        std::cout<<"Write failed with error = " << errno << std::endl;
        return false;
    }
}

bool PlatformAPIs::SeekFile(HANDLE hDevice, FileMoveMethod dwMoveMethod, unsigned long long ullOffset)
{
    
    int whence = (dwMoveMethod == BEGIN) ? SEEK_SET :
        (dwMoveMethod == CURRENT) ? SEEK_CUR : SEEK_END;
    off_t location = lseek(hDevice, ullOffset, whence);
    if (-1 == location) 
    {
        return false;
    }
    else
    {
        return true;
    }
}

HANDLE PlatformAPIs::OpenDefault(std::string wszDeviceName)
{
    int fd;

    if (m_directIO)
        fd = open(wszDeviceName.c_str(), O_RDWR|O_CREAT|O_DIRECT|O_SYNC, 0644);
    else
        fd = open(wszDeviceName.c_str(), O_RDWR|O_CREAT, 0644);

    return (HANDLE)fd;
}

HANDLE PlatformAPIs::Open(std::string szDevice, SV_ULONG access, SV_ULONG mode,
    SV_ULONG creationDeposition, SV_ULONG flags)
{
    int fd;

    if (m_directIO)
        fd = open(szDevice.c_str(), O_RDWR|O_CREAT|O_DIRECT|O_SYNC, 0644);
    else
        fd = open(szDevice.c_str(), O_RDWR|O_CREAT, 0644);

    return (HANDLE)fd;
}

bool PlatformAPIs::Close(HANDLE hDevice)
{
    if (close(hDevice) == 0)
        return true;
    else
        return false;
}

bool PlatformAPIs::SafeClose(HANDLE hDevice)
{
    if(hDevice)
    {
        if (close(hDevice) == 0)
            return true;
        else
            return false;
    }
}

unsigned long PlatformAPIs::GetLastError()
{
    return errno;
}

bool PlatformAPIs::PathExists(const char* path)
{
    return (boost::filesystem::exists(path));
}

bool PlatformAPIs::IsValidHandle(HANDLE handle)
{
    return ((-1==handle) ? false : true);
}
