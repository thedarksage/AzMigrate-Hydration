//                                       
// Copyright (c) 2016 Microsoft Corp.
// This file contains proprietary and confidential information and
// remains the unpublished property of Microsoft. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with Microsoft.
// 
// File       : tempdevice.h
// 
// Description: 
// 

#ifndef TEMPDEVICE_H
#define TEMPDEVICE_H

#include <string>
#include <set>

class TempDevice {
public:
    
    TempDevice();
    
    bool IsTempDevice(std::string const & deviceFile) const
    {
        return (m_TempDevices.end() != m_TempDevices.find(deviceFile));
    }
    
protected:
    typedef std::set<std::string> TempDevices_t;
    
    
private:        
    TempDevices_t m_TempDevices;
};

#endif // ifndef TEMPDEVICE_H
