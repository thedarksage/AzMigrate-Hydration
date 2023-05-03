// DiskDeviceMajor.cpp : Defines the Unix implementation of BlockDevice APIs.
//


#include <string.h>
#include <linux/fs.h> //Required for BLKGETSIZE64 BLKSSZGET
#include <sys/ioctl.h>
#include <string>
#include <map>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "PlatformAPIs.h"
#include "CDiskDevice.h"
#include "DiskDeviceMajor.h"
#include "InmageTestException.h"
#include "inmsafecapis.h"

#include <iostream>

#define BLKGETSIZE   _IO(0x12,96)    /* return device size */
#define BLKSSZGET    _IO(0x12,104)   /* get block device sector size */
#define BLKGETSIZE64 _IOR(0x12,114,size_t)  /* return device size.  */

std::map<std::string,int>g_mapDiskNameToDiskNum;

bool CreateDiskNameToNumMapping()
{
    bool bRet = true;
    std::string strDiskPrefix = "/dev/sd";
    std::string strDisk;
    char buf[2] = {0};
    buf[0] = 'a';

    for (int i = 0, num = 0; i < 26;i++,num++)
    {
        //Repeat the loop for /dev/sdx
        memset((void*)buf, 0x00, sizeof(char));
        buf[0] = 'a' + i;
        strDisk = strDiskPrefix + std::string((const char*)buf);
        g_mapDiskNameToDiskNum[strDisk] = num;

        //Repeat the loop for /dev/hdx
        if (num == 25)
        {
            num = 100;
            i = -1;
            strDiskPrefix = "/dev/hd";
        }

        //TODO : Need to extend this for volume groups or some other disk types
    }
    return bRet;
}

int GetDiskNumFromDiskName(const std::string & strDiskName)
{
    return g_mapDiskNameToDiskNum[strDiskName];
}

std::string GetDiskNameFromDiskNum(const int nDiskNum)
{
    std::string strDiskName;
    std::map<std::string, int>::iterator iter = g_mapDiskNameToDiskNum.begin();
    while (iter != g_mapDiskNameToDiskNum.end())
    {
        if ((*iter).second == nDiskNum)
        {
            strDiskName = iter->second;
            break;
        }
        else
            iter++;
    }
    return iter->first;
}

CDiskDevice::CDiskDevice(int diskNumber, IPlatformAPIs *platformAPIs)
{
    CreateDiskNameToNumMapping();
    m_diskNumber = diskNumber;
    m_diskName = GetDiskNameFromDiskNum(diskNumber);
    m_platformAPIs = platformAPIs;
    Open();

    PopulateDiskInfo();
}

CDiskDevice::CDiskDevice(const std::string &strDiskName, IPlatformAPIs *platformAPIs)
{
    CreateDiskNameToNumMapping();
    m_platformAPIs = platformAPIs;
    m_diskName = strDiskName;
    m_diskNumber = GetDiskNumFromDiskName(m_diskName);
    Open();
    PopulateDiskInfo();
}

void CDiskDevice::PopulateDiskInfo()
{
    m_diskId = m_diskName;
    unsigned long lbytesReturned = 0;
    unsigned long lbytesRetForBlkSize = 0;

    if (m_platformAPIs == NULL)
        std::cout << " m_platformAPIs is NULL\n";

    m_ullDiskSize = 0;
    m_dwBlockSize = 0;

    if (m_platformAPIs->DeviceIoControlSync(m_hDisk, BLKGETSIZE64,
        &m_ullDiskSize, 4, &m_ullDiskSize, 4, &lbytesReturned) != true)
    {
        throw  CBlockDeviceException("%s: ioctl BLKGETSIZE64 failed for %s with error=%d",
            __FUNCTION__, m_diskName.c_str(), errno);
    }

    if (m_platformAPIs->DeviceIoControlSync(m_hDisk, BLKSSZGET,
        &m_dwBlockSize, 4, &m_dwBlockSize, 4, &lbytesRetForBlkSize) != true)
    {
        throw  CBlockDeviceException("%s: ioctl BLKSSZGET failed for %s with error=%d",
            __FUNCTION__, m_diskName.c_str(), errno);
    }
}

bool CDiskDevice::DiskOnlineOffline(bool online, bool readOnly, bool persist)
{
    //we are currently returning true assuming the disks are being used raw
    return true;
}

std::string CDiskDevice::GetDeviceId()
{
    return m_diskName;
}
