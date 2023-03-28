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
    return GetRawVolSize(sVolume);
}

