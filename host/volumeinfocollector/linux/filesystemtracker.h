//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : filesystemtracker.h
// 
// Description: 
// 

#ifndef FILESYSTEMTRACKER_H
#define FILESYSTEMTRACKER_H

#include <string>

class FileSystemTracker {
public:
    std::string GetFileSystem(std::string const & deviceName) const;
};


#endif // ifndef FILESYSTEMTRACKER_H
