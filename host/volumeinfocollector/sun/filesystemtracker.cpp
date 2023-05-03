//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : filesystemtracker.cpp
// 
// Description: 
// 

#include <fstream>
#include <sstream>

#include "filesystemtracker.h"
#include "executecommand.h"
    

std::string FileSystemTracker::GetFileSystem(std::string const & deviceName) const
{
    std::string cmd("/usr/sbin/fstyp  ");

    cmd += deviceName + " 2>/dev/null";

    std::stringstream results;

    /*
    * -bash-3.00# fstyp /dev/dsk/c1d0s0
    * ufs
    */
    if (!executePipe(cmd, results)) {
        return "" ;
    }

    std::string fs;

    results >> fs;

    return fs;
}
