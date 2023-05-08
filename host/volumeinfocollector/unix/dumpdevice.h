//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : dumpdevice.h
// 
// Description: 
// 

#ifndef DUMPDEVICE_H
#define DUMPDEVICE_H

#include <string>
#include <set>

class DumpDevice {
public:
    
    DumpDevice();
    
    bool IsDumpDevice(std::string const & deviceFile) const;
    
protected:
    typedef std::set<std::string> DumpDevices_t;
    
    
private:        
    DumpDevices_t m_DumpDevices;
};

#endif // ifndef DUMPDEVICE_H
