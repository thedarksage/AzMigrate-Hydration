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
    GetLvmFilesToMonitor(fileinfo);
    return true;
}



bool VolumeMonitorTask::MaskOffSIGCLDForThisThread(void)
{
	/** Do nothign for now in linux */
	return true;
}
