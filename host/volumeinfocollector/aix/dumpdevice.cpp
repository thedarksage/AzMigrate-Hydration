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
#include <sstream>

#include "executecommand.h"
#include "dumpdevice.h"
#include <iostream>

DumpDevice::DumpDevice()
{
    // assumes the following will return lines that look like
    //    Dump device: /dev/dsk/c0t0d0s1
    // with possibly more data after the device name
	//std::cout<< " In DumpDevice" << std::endl ;
    std::string cmd("sysdumpdev -l |egrep 'primary|secondary' |awk '{print$2}'");

    
    std::stringstream results;
    
    if (!executePipe(cmd, results)) {
        return;
    }

    std::string tag1;
    std::string tag2;
    std::string device;

    char line[256];

    while (!results.eof()) {
        device.clear();
        results >>  device;
        if (!device.empty()) {
            m_DumpDevices.insert(device);
			//std::cout<<"Dump Device is :" << device << std::endl ;
        }
        results.getline(line, sizeof(line));
    }
}

bool DumpDevice::IsDumpDevice(std::string const & deviceFile) const
{    
    return (m_DumpDevices.end() != m_DumpDevices.find(deviceFile));
}


