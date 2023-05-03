//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : dumpdevices.cpp
// 
// Description: 
//

#include <iostream>
#include <sstream>

#include "executecommand.h"
#include "dumpdevice.h"


DumpDevice::DumpDevice()
{
    // assumes the following will return lines that look like
    //    Dump device: /dev/dsk/c0t0d0s1
    // with possibly more data after the device name
    std::string cmd("/usr/sbin/dumpadm | /usr/bin/grep \"Dump device\"");
    
    std::stringstream results;
    
    if (!executePipe(cmd, results)) {
        return;
    }

    std::string tag1;
    std::string tag2;
    std::string device;

    std::string line;

    while (!results.eof()) {
        device.clear();
        results >> tag1 >> tag2 >> device;
        if (!device.empty()) {
            m_DumpDevices.insert(device);
        }
        std::getline(results, line);
    }
}

bool DumpDevice::IsDumpDevice(std::string const & deviceFile) const
{    
    return (m_DumpDevices.end() != m_DumpDevices.find(deviceFile));
}


