#include "swapvolumes.h"
#include "executecommand.h"
#include "volumeinfocollectorinfo.h"
#include <iostream>
SwapVolumes::SwapVolumes()
{
    std::string cmd("/usr/sbin/swap -l ");
    
    std::stringstream swaps;
    
    if (!executePipe(cmd, swaps)) {
        return ;
    }
    ParseSwapVolumes(swaps);
}
