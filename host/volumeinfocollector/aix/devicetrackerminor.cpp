//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : devicetrackerminor.cpp
// 
// Description: DeviceTracker class implementation see devicetracker.h for more details
// 
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <math.h>
#include <string.h>
#include <lvm.h>
#include <sys/devinfo.h>
#include <sys/types.h>
#include <string>
#include <sstream>
#include <fstream>
#include <errno.h>
#include <iostream>
#include <list>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/devinfo.h>

#include "devicetracker.h"
#include "volumemanagerfunctions.h"
#include "volumemanagerfunctionsminor.h"
    
DeviceTracker::DeviceTracker()
{
}

bool DeviceTracker::LoadDevices()
{
    return true;
}

bool DeviceTracker::IsTracked(int major, char const * deviceFile) const
{
    if (IsFixedDisk(deviceFile)) {
        return true;
    }

    if (m_VxDmpTracker.IsVxDmpName(deviceFile)) {
        return true;
    } 

    return false;
}

/* TODO: for solaris x86, since disk is p0, need to 
 * check if for cdrom, are we able to detect ? For 
 * fixed disks, ioctl is working */
int DeviceTracker::IsFixedDisk(char const * deviceFile) const
{
    std::string dskname = deviceFile;
    std::string rdskname = GetRawDeviceName(dskname);
    int isfixeddisk = 1; //true
    int ioctlrval = -1;
	struct devinfo devInfo;

    int fd = open(rdskname.c_str(), O_RDONLY);

    if (-1 != fd)
    {
        ioctlrval = ioctl(fd, IOCINFO, &devInfo) ;
        if (!ioctlrval)
        {
            switch (devInfo.devtype)
            {
                case DD_DISK:
					isfixeddisk = 1 ;	
                    break;
				case DD_SCDISK:
					isfixeddisk = 1 ;	
                    break;
				case DS_PV:
					isfixeddisk = 1 ;	
                    break;
                default:
                    break;
            }
        }
        close(fd);
    }
    else
    {
		DebugPrintf(SV_LOG_ERROR, "Unable to open %s error %d\n", rdskname.c_str(), errno);
        isfixeddisk = 0;
    }

    if (isfixeddisk)
    {
        isfixeddisk = !IsInmageATLun(deviceFile);
    }

    return isfixeddisk;
}
