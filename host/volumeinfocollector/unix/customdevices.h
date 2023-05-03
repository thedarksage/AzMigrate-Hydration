//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : customdevices.h
// 
// Description: used to process custom devices
//              a custom device is a divice name optional size and mount point
//              that is kept in a file. it is used to specify devices that
//              are not recognized by the volumeinfocollector as a possible
//              source or target device.
// 

#ifndef CUSTOMDEVICES_H
#define CUSTOMDEVICES_H

#include <set>
#include <string>

class CustomDevices {
public:
       
    struct CustomDevice {    

        CustomDevice() : m_Size(0), m_Valid(true) {}
        
        bool operator<(CustomDevice const & rhs) const {
            return (m_Name < rhs.m_Name);
        }

        std::string m_Name;
        std::string m_Mount;

        long long m_Size;

        bool m_Valid;
    };

    typedef std::set<CustomDevice, std::less<CustomDevice> > CustomDevices_t;

    explicit CustomDevices(std::string const & path)
            : m_FileName(path),
              m_MaxNameLen(0),
              m_MaxSizeLen(0)
        {
            if ('/' != m_FileName[m_FileName.length() - 1]) {
                m_FileName += '/';
            }
            m_FileName += "customdevices.dat";
        }

    void Print();

    bool Load(std::string const & fileName = std::string())
        {
            if (!fileName.empty()) {
                m_FileName = fileName;
            }
            
            return Read();
        }
    
    bool DeleteCustomDevice(CustomDevice const & customDevice);
    bool AddCustomDevice(CustomDevice & customDevice);

    CustomDevices_t::const_iterator begin()
        {
            return m_CustomDevices.begin();
        }
            
    CustomDevices_t::const_iterator end()
        {
            return m_CustomDevices.end();
        }
    
protected:

    bool Read();
    bool Write() const;

    bool ValidateSize(CustomDevice const & device);
    
    
private:

    std::string m_FileName;

    CustomDevices_t m_CustomDevices;

    unsigned int m_MaxNameLen;  // for formatting when print custom devices
    unsigned int m_MaxSizeLen;  // for formatting when print custom devices
};
   

#endif // ifndef CUSTOMDEVICES_H
