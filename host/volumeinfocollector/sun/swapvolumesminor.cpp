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

#include "swapvolumes.h"
#include "executecommand.h"
#include "volumeinfocollectorinfo.h"

SwapVolumes::SwapVolumes()
{
    std::string cmd("/usr/sbin/swap -l ");
    
    std::stringstream swaps;
    
    if (!executePipe(cmd, swaps)) {
        return ;
    }
    
    ParseSwapVolumes(swaps);
}


