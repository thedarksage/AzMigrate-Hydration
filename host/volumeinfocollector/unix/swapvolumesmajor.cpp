//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : swapvolumesmajor.cpp
// 
// Description: 
// 

#include <sys/stat.h>
#include <string>

#include "swapvolumes.h"
#include "volumeinfocollectorinfo.h"

static std::string const SWAP_FILE_SYSTEM("swap");

bool SwapVolumes::IsSwapVolume(VolumeInfoCollectorInfo const & volInfo) const 
{
    struct stat64 volStat;

    if (-1 == stat64(volInfo.m_DeviceName.c_str(), &volStat)) {
        return false;;
    }

    if (0 == volStat.st_rdev) {
        return false;
    }
    
    return (m_SwapVolumes.end() != m_SwapVolumes.find(volStat.st_rdev));
}


bool SwapVolumes::IsSwapFS(const std::string &fileSystem) const 
{
    bool bisswapfs = (SWAP_FILE_SYSTEM == fileSystem);
    return bisswapfs;
}
