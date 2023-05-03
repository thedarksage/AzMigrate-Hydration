//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : DeviceTracker.h
// 
// Description: used to track linux devices that can be used as a source or target
// 

#ifndef DEVICETRACKER_H
#define DEVICETRACKER_H

#include <set>
#include <string>

#include "hdlmtracker.h"
#include "emcpptracker.h"
#include "vxdmptracker.h"
#include "devdidtracker.h"
#include "devglbltracker.h"
#include "devmappertracker.h"
#include "mpxiotracker.h"
#include "devmapperdockertracker.h"
#include "volumegroupsettings.h"

class DeviceTracker {
public:

    static bool const REQUIRES_CHECK = true; // use for major numbers that are ambiguous
                                             // e.g.
                                             // major number 3 can be either an IDE hard disk or cdrom
                                             // we need to check which one it is as we want to track
                                             // the hard disk but not the cdrom
    
    static bool const EXPERIMENTAL_CHECK = true; // some numbers are for expermental use
                                                 // and some devices are using those numbers
                                                 // this is used to identify them so that
                                                 // we track experimental numbers if trackExperimental
                                                 // is set in the .conf file

    class MajorNumber {
    public:
        MajorNumber(int major = -1, bool check = false, bool experimental = false)
                : m_Major(major),
                  m_RequiresCheck(check),
                  m_Experimental(experimental)
            {}
        
        void Major(int major) {
            m_Major = major;
        }
        
        int Major() const {
            return m_Major;
        }
        
        bool operator<(MajorNumber const & rhs) const {
            return (m_Major < rhs.m_Major);
        }

        bool RequiresCheck() const {
            return m_RequiresCheck;
        }

        bool IsExperimental() const {
            return m_Experimental;
        }

    private:
        int   m_Major;
        bool  m_RequiresCheck; // true for device types that can be either disk or something else (e.g. cd/dvd rom)
        bool  m_Experimental;  // true for device types is for experimental use
        
    };
    
    typedef std::set<MajorNumber, std::less<MajorNumber> > MajorNumbers_t;

    DeviceTracker();
    
    bool LoadDevices();
    
    bool IsTracked(int major, char const * deviceFile) const;

    std::string GetSysfsBlockDirectory() const;

    void UpdateVendorType(const std::string &devname, VolumeSummary::Vendor &vendor, bool &bismultipath);

protected:
    
    bool IsVolPack(char const * deviceFile) const;
    bool IsCDRom(char const * deviceFile) const;
    bool IsHardDisk(char const * deviceFile, std::string & media) const;
    bool IsHdlm(char const * deviceFile) const;
    bool IsEMCPP(char const * deviceFile) const;
    int IsFixedDisk(char const * deviceFile) const;
    bool IsIdeDevice(char const * deviceFile) const;
    bool IsInmageATLun(char const * deviceFile) const;
    bool IsDevMapperDockerDevice(char const * deviceFile) const;
    
private:

    mutable std::string m_SysFs;
   
    MajorNumbers_t m_MajorNumbers;    
    MajorNumbers_t m_TrackedNumbers;

    HdlmTracker m_HdlmTracker;
    EMCPPTracker m_EMCPPTracker;
    VxDmpTracker m_VxDmpTracker;
    DevDidTracker m_DevDidTracker;
    /* DevGlblTracker m_DevGlblTracker; */
    DevMapperTracker m_DevMapperTracker;
    MpxioTracker m_MpxioTracker;
    DevMapperDockerTracker m_DevMapperDockerTracker;
};


#endif // ifndef DEVICETRACKER_H
