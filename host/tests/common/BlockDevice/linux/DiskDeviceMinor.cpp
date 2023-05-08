// BlockDeviceMinor.cpp : Defines the linux APIs.
//

#include <string.h>
#include <wchar.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/fs.h> //Required for BLKGETSIZE64 BLKSSZGET 
#include <sys/ioctl.h>
#include "PlatformAPIs.h"
#include "CDiskDevice.h"
#include "DiskDeviceMajor.h"
#include "InmageTestException.h"
#include <iostream>

void CDiskDevice::PopulateDiskInfo()
{
    std::cout<< "Entered PopulateDiskInfo with m_hDisk = " << m_hDisk << "\n";

    if (m_platformAPIs == NULL)
    {
        std::cout << "m_platformAPIs is NULL\n";
        throw CBlockDeviceException("%s: m_platformAPIs is NULL", __FUNCTION__);
    }

    if (m_platformAPIs->DeviceIoControlSync(m_hDisk, BLKGETSIZE64,
        &m_ullDiskSize, 0, NULL, 0, NULL) != true)
    {
        throw CBlockDeviceException("%s: ioctl failed for BLKGETSIZE64 %s with error=%d",
            __FUNCTION__, m_diskName.c_str(), errno);
    } 

    if (m_platformAPIs->DeviceIoControlSync(m_hDisk, BLKSSZGET,
        &m_dwBlockSize, 0, NULL, 0, NULL) != true)
    {
        throw CBlockDeviceException("%s: ioctl failed for BLKSSZGET %s with error=%d",
            __FUNCTION__, m_diskName.c_str(), errno);
    } 

    std::cout<< "Exited PopulateDiskInfo m_ullDiskSize = " <<
        m_ullDiskSize << " and m_dwBlockSize = " << m_dwBlockSize << std::endl;
}

