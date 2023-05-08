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

/*
 * FUNCTION NAME : Init
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
int Volume::Init()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    int rc = SV_SUCCESS;
    memset(m_sVolumeGUID, 0, sizeof(m_sVolumeGUID));
	/* check if the volume can be filtered.*/
	m_bIsFilteredVolume = true;
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rc;
}

/*
 * FUNCTION NAME : GetBytesPerSector
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
const unsigned long int Volume::GetBytesPerSector()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	assert( !"need to implemnt the linux version of this function." );
   	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return 0;
}

/*
 * FUNCTION NAME : GetFreeBytes
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
SV_ULONGLONG Volume::GetFreeBytes(const std::string& sVolume)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	assert( !"need to implemnt the linux version of this function." );
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return 0;
}

/*
 * FUNCTION NAME : GetVolumeGUID
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
const char* Volume::GetVolumeGUID() const
{
    return m_sVolumeGUID;
}
