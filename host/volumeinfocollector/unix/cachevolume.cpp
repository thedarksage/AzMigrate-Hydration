//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : cachevolume.cpp
// 
// Description: 
//

#include <sys/stat.h>

#include "configurator2.h"
#include "cachevolume.h"

CacheVolume::CacheVolume()
    : m_DeviceId(0)
{
    struct stat64 cacheStat;
	LocalConfigurator TheLocalConfigurator;
    // TODO: what about cx dirs, fx dirs if they are also installed on the same host
    // as well as cache dir id being a different location then the install dir
    if (-1 == stat64(TheLocalConfigurator.getCacheDirectory().c_str(), &cacheStat)) {
        return;
    }
    m_DeviceId = cacheStat.st_dev;
}


