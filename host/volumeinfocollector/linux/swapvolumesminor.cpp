//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : swapvolumes.cpp
// 
// Description: 
//

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <fstream>
#include <sstream>
#include <string>

#include "swapvolumes.h"
#include "volumeinfocollectorinfo.h"

SwapVolumes::SwapVolumes()
{
    std::ifstream swaps("/proc/swaps");

    if (!swaps.good()) {
        return;
    }

    ParseSwapVolumes(swaps);
}


