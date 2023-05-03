//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : dumpdevices.cpp<2>
// 
// Description: 
//

#include <fstream>

#include "dumpdevice.h"

DumpDevice::DumpDevice()
{
    std::ifstream dumpDevicesFile("/proc/diskdump");

    if (!dumpDevicesFile.good()) {
        // assume that there are no dump devices
        // NOTE: to be sure could do 
        // if (ENOENT != errno) {
        //    throw some type of error
        // }
        return;
    }

    std::string line;
    while (!dumpDevicesFile.eof()) {
        line.clear();

        std::getline(dumpDevicesFile, line);
        if (line.empty()) {
            continue;
        }

        std::string::size_type idx = 0;
        std::string::size_type size = line.size();

        while (idx < size && isspace(line[idx])) {
            ++idx;
        }

        if (idx == size) {
            continue;
        }
        
        if ('#' == line[idx]) {
            continue;
        }

        std::string::size_type eIdx = line.find_first_of(" \t", idx);
        
        if (eIdx == std::string::npos) {
            eIdx = size;
        }

        m_DumpDevices.insert(line.substr(idx, eIdx - idx));
    }
}

bool DumpDevice::IsDumpDevice(std::string const & deviceFile) const
{
    // NOTE: we don't use m_DumpDevices.find(deviceFile)
    // because /procs/diskdump may not always include the full path
    // so we check agains exact match or against match with out prefix /dev/
    // assuming that /dev/ might be removed in /proc/diskdump
    DumpDevices_t::const_iterator iter(m_DumpDevices.begin());
    DumpDevices_t::const_iterator endIter(m_DumpDevices.end());
    for(/* empty */; iter != endIter; ++iter) {
        if (deviceFile == (*iter) || deviceFile.substr(sizeof("/dev/") - 1) == *iter) {
            return true;
        }
    }
    
    return false;
}


