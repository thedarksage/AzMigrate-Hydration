//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : devicetrackermajor.cpp
// 
// Description: DeviceTracker class implementation see devicetracker.h for more details
// 

#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <cstring>

#include "devicetracker.h"
#include "configurator2.h"
#include "utilfunctionsmajor.h"    
#include "scsicommandissuer.h"
#include "portablehelpers.h"
#include "inmsafecapis.h"

bool DeviceTracker::IsVolPack(char const * deviceFile) const
{
    LocalConfigurator TheLocalConfigurator;

    int counter = TheLocalConfigurator.getVirtualVolumesId();;
    while(counter != 0) {
        char regData[26];
		inm_sprintf_s(regData, ARRAYSIZE(regData), "VirVolume%d", counter);

        std::string data = TheLocalConfigurator.getVirtualVolumesPath(regData);

        std::string sparsefilename;
        std::string volume;

        if (!parseSparseFileNameAndVolPackName(data, sparsefilename, volume))
        {
            counter--;
            continue;
        }

        if(0 == strcmp(volume.c_str(),deviceFile)) {
            return true;
        } else {
            counter--;
            continue;
        }

    }
    return false;
}


void DeviceTracker::UpdateVendorType(const std::string &devname, VolumeSummary::Vendor &vendor, bool &bismultipath)
{
    /* NOTE: please do not change the order; 
     * since not to give vxdmp or dm to check
     * for hdlm or emcpp */
    if (m_DevMapperDockerTracker.IsDevMapperDockerName(devname))
    {
        vendor = VolumeSummary::DEVMAPPERDOCKER;
    }
    else if (m_DevMapperTracker.IsDevMapperName(devname))
    {
        vendor = VolumeSummary::DEVMAPPER;
        /* Do not update bismultipath here */
    }
    else if (m_MpxioTracker.IsMpxioName(devname)) 
    {
        vendor = VolumeSummary::MPXIO;
        bismultipath = true;
    }
    else if (m_VxDmpTracker.IsVxDmpName(devname))
    {
        vendor = VolumeSummary::VXDMP;
        bismultipath = true;
    }
    else if (m_DevDidTracker.IsDevDidName(devname))
    {
        vendor = VolumeSummary::DEVDID;
    }
    /*
    else if (m_DevGlblTracker.IsDevGlblName(devname))
    {
        vendor = VolumeSummary::DEVGLOBAL;
    }
    */
    else if (m_HdlmTracker.IsHdlmName(devname)) 
    {
        vendor = VolumeSummary::HDLM;
        bismultipath = true;
    }
    else if (m_EMCPPTracker.IsEMCPP(devname))
    {
        vendor = VolumeSummary::EMC;
        bismultipath = true;
    }
    else
    {
        vendor = VolumeSummary::NATIVE;
    }
}

bool DeviceTracker::IsInmageATLun(char const * deviceFile) const
{
    INQUIRY_DETAILS InqDetails;
    ScsiCommandIssuer sci;
    bool isinmageatlun;

    isinmageatlun = false;
    InqDetails.device = deviceFile;
    if (sci.StartSession(deviceFile))
    {
        if (sci.GetStdInqValues(&InqDetails))
        {
            std::string vendor = InqDetails.GetVendor();
            Trim(vendor, " \t");
            if (INMATLUNVENDOR == vendor)
            {
                isinmageatlun = true;
                /* TODO: should we match lun name prefix like inmage000.. */
                DebugPrintf(SV_LOG_DEBUG, "%s is inmage at lun as its vendor is inmage\n", deviceFile);
            }
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "failed to do standard inquiry on device %s to know if it is inmage AT lun\n", deviceFile);
        }
        sci.EndSession();
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "failed to open session with scsi command issuer with error %s for device %s to know if it is inmage atlun\n", 
                                  sci.GetErrorMessage().c_str(), deviceFile);
    }

    return isinmageatlun;
}
