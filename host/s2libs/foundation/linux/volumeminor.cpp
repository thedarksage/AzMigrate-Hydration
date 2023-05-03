/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : volume_port.cpp
 *
 * Description: 
 */
#include "portable.h"
#include "entity.h"

#include "error.h"
#include "synchronize.h"

#include "genericfile.h"
#include "volume.h"

#include <sys/ioctl.h>   /* for _IO */
#include "portablehelpers.h"
#include "volumeinfocollector.h"
#include "portablehelpersminor.h"

/*
 * FUNCTION NAME : GetRawVolumeSize
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
SV_ULONGLONG Volume::GetRawVolumeSize(const std::string &sVolume)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    std::string formattedVolume = sVolume;
    unsigned long nrsectors;
    unsigned long long raw_size = 0ULL;
	int fd;

    if ((fd = open(formattedVolume.c_str(), O_RDONLY))< 0)
		return SVE_FAIL;

   /* 
    * This returns number of sectors of 512 byte
    * But the actual sector size may be different from 512 bytes
	*/
   if (ioctl(fd, BLKGETSIZE, &nrsectors))
      nrsectors = 0;

   //Get rawsize. This ioctl is may not be supported on all linux versions
   if (ioctl(fd, BLKGETSIZE64, &raw_size))
       raw_size = 0;

   close(fd);
    /*
     * If BLKGETSIZE64 was unknown or broken, use longsectors.
     * (Kernels 2.4.15-2.4.17 had a broken BLKGETSIZE64
     * that returns sectors instead of bytes.)
     */

    if (raw_size == 0 || raw_size == nrsectors)
      raw_size = ((unsigned long long) nrsectors) * 512;

	 return raw_size;
}

