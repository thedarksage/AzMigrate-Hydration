//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : unixvolumemonitor.cpp
// 
// Description: 
// 

#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <error.h>
#include <sys/types.h>
#include <dirent.h>
#include <ace/OS.h>
#include <boost/shared_array.hpp>

#include "volumemonitormajor.h"
#include "portable.h"
#include "file.h"
#include "host.h"
#include "portablehelpersminor.h"



bool VolumeMonitorTask::GetFileListToMonitor(std::vector<struct FileInformation> &fileinfo)
{
    fileinfo.push_back(FileInformation(SV_PROC_MNTS,true));
    return true;
}


bool VolumeMonitorTask::MaskOffSIGCLDForThisThread(void)
{
    sigset_t sigset;
    bool bretval = false;



    if (0 == sigemptyset(&sigset))
    {
        if (0 == sigaddset(&sigset, SIGCLD))
        {
            if (0 == pthread_sigmask(SIG_BLOCK, &sigset, NULL))
            {
                bretval = true;
            }
        }
    }
    
    return bretval;
}
