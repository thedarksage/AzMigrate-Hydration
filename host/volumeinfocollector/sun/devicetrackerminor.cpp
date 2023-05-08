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
#include <sys/dkio.h>
#include <sys/vtoc.h>

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
    return IsFixedDisk(deviceFile);
}

/* TODO: for solaris x86, since disk is p0, need to 
 * check if for cdrom, are we able to detect ? For 
 * fixed disks, ioctl is working */
int DeviceTracker::IsFixedDisk(char const * deviceFile) const
{
    std::string dskname = deviceFile;
    std::string rdskname = GetRawDeviceName(dskname);
    int isfixeddisk = 1; //true; TODO: should a disk be reported in case ioctl 
                         // DKIOCINFO fails ? 
    int ioctlrval = -1;
    struct dk_cinfo cinfo;
 
    int fd = open(rdskname.c_str(), O_RDONLY);
    
    if (-1 != fd)
    {
        ioctlrval = ioctl(fd, DKIOCINFO, &cinfo); 
        if (!ioctlrval)
        {
            //SUCCEEDS
            switch (cinfo.dki_ctype)
            {
                case DKC_CDROM:
                    isfixeddisk = 0; //This is a CDROM
                    break;
                default:
                    break;
            }
        }
        close(fd);
    } 
    else
    {
        isfixeddisk = 0;
    }

    return isfixeddisk;
}
