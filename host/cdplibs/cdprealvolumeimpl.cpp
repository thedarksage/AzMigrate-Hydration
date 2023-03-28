/*
* Copyright (c) 2005 InMage
* This file contains proprietary and confidential information and
* remains the unpublished property of InMage. Use, disclosure,
* or reproduction is prohibited except as permitted by express
* written license aggreement with InMage.
*
* File       : cdprealvolumeimpl.cpp
*
* Description: 
*/
#include <iostream>
#include <string>
#include "portablehelpers.h"
#include "cdprealvolumeimpl.h"
#include "boost/shared_array.hpp"

/*
* FUNCTION NAME : cdp_realVolumeimpl_t
*
* DESCRIPTION : constructor
*
* INPUT PARAMETERS : physicalvolume name and mount point as std::strings
*
* OUTPUT PARAMETERS : 
*
* NOTES :
*
* return value : 
*
*/
cdp_realVolumeimpl_t::cdp_realVolumeimpl_t(const std::string& sVolumeName)               
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    m_volume.reset(new Volume(sVolumeName,""));
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

/*
* FUNCTION NAME : ~cdp_realVolumeimpl_t
*
* DESCRIPTION : destructor
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
cdp_realVolumeimpl_t::~cdp_realVolumeimpl_t()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

