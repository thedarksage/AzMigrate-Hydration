// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
//
// File       : volumeinfocollectormajor.cpp
//
// Description:
//


#include <string>
#include <set>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <cstdio>
#include <fcntl.h>
#include <sstream>
#include <fstream>
#include <cerrno>
#include <functional>
#include <list>
#include <iostream>
#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include <cstring>

#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/spirit/include/classic.hpp>
#include <boost/spirit/include/classic_core.hpp>
#include <boost/spirit/include/phoenix1_primitives.hpp>
#include <boost/spirit/include/phoenix1_functions.hpp>

#include "configurator2.h"
#include "executecommand.h"
#include "utildirbasename.h"
#include "volumeinfocollector.h"
#include "VsnapShared.h"
#include "logger.h"
#include "portablehelpersmajor.h"
#include "dumpmsg.h"
#include "devicemajorminordec.h"
#include "utilfunctionsmajor.h"
#include "volumemanagerfunctions.h"
#include "volumemanagerfunctionsminor.h"
#include "appdevicereporter.h"
#include "configwrapper.h"
#include "VsnapUser.h"

using namespace BOOST_SPIRIT_CLASSIC_NS;

std::string const VolumeInfoCollector::KB_UNITS = "KB";
std::string const VolumeInfoCollector::MB_UNITS = "MB";
std::string const VolumeInfoCollector::GB_UNITS = "GB";
std::string const VolumeInfoCollector::TB_UNITS = "TB";

VolumeInfoCollector::VolumeInfoCollector(bool dumpToStdout)
: m_CustomDevices(m_localConfigurator.getCacheDirectory()),
  m_DeviceIDInformer(&m_ScsiCommandIssuer),
  m_WriteCacheInformer(&m_ScsiCommandIssuer),
  m_LunCapacityInformer(&m_ScsiCommandIssuer)
{
    Init(dumpToStdout);
}

VolumeInfoCollector::VolumeInfoCollector(DeviceNameTypes_t deviceNameTypeToReport, bool dumpToStdout)
: m_CustomDevices(m_localConfigurator.getCacheDirectory()),
  m_DeviceIDInformer(&m_ScsiCommandIssuer),
  m_WriteCacheInformer(&m_ScsiCommandIssuer),
  m_LunCapacityInformer(&m_ScsiCommandIssuer)
{
    Init(dumpToStdout);
    m_deviceNameTypeToReport = deviceNameTypeToReport;
}

void VolumeInfoCollector::Init(bool dumpToStdout)
{
    /* m_RegisterSystemDevices = m_localConfigurator.getRegisterSystemDrive();  */
    m_RegisterSystemDevices = true; /* TODO: true until we resolve the register system device issue */
    m_deviceNameTypeToReport = DEVICE_NAME_TYPE_OS_NAME;
    m_ReportFullDevicesOnly = m_localConfigurator.reportFullDeviceNamesOnly();
    if (!m_localConfigurator.getScsiId())
        m_DeviceIDInformer.DoNotTryScsiID();
    DumpMsgInit(dumpToStdout);
}

void VolumeInfoCollector::GetVolumeInfos(VolumeSummaries_t & volumeSummaries, VolumeDynamicInfos_t & volumeDynamicInfos, bool dump)
{
    // NOTE:
    // order is important here so do not change it
    GetVolumeManagerInfos();
    GetVsnapInfos();
    UpdateVolumeManagerInfos();
    GetMountInfos();
    CollectScsiHCTL();
    // NOTE:
    // order is important here so do not change it
    GetCustomDevices();
    //   GetVsnapDevices();
    GetVolPacks();
    GetVirtualVolumes();
    m_DeviceTracker.LoadDevices();
    FindLogicalVolumeMountedOnRoot();
    /* need all, and allow exclusions */
    GetDiskVolumeInfos(false, false);
    GetLogicalVolumeInfos();
    MarkDisksOnBootDiskController();
    CopyVolumeInfosToVolumeSummaries(m_VolumeInfos, volumeSummaries, volumeDynamicInfos);

    if (dump) {
        // NOTE: DO NOT comment out the call to DumpVolumeInfos()
        // instead when calling GetVolumeInfos set the dump argument to true or false depending
        // on whether you want to dump the volume infos or not
        DumpMsg("C O L L E C T E D   V O L U M E   I N F O M A T I O N\n");
        DumpVolumeInfos();
        DumpMsg("R E G I S T E R   V O L U M E   I N F O M A T I O N\n");
        DumpVolumeSummaries(volumeSummaries);
        DumpVolumeDynamicInfos(volumeDynamicInfos);
    }
}

void VolumeInfoCollector::GetOsDiskInfos(VolumeSummaries_t & volumeSummaries, const bool &dump)
{
    m_DeviceTracker.LoadDevices();
    /* need only native disks, and do not allow exclusions */
    GetDiskVolumeInfos(true, true);
    CopyAllVolumeInfosToVolumeSummaries(m_VolumeInfos, volumeSummaries);
    if (dump) {
        // NOTE: DO NOT comment out the call to DumpVolumeInfos()
        // instead when calling GetVolumeInfos set the dump argument to true or false depending
        // on whether you want to dump the volume infos or not
        DumpMsg("C O L L E C T E D   O S D I S K   I N F O M A T I O N\n");
        DumpVolumeInfos();
        DumpMsg("S U M M A R Y   O S D I S K   I N F O M A T I O N\n");
        DumpVolumeSummaries(volumeSummaries);
    }
}


bool VolumeInfoCollector::GetVolumeManagerInfos(void)
{
    bool bgotosvms = GetOSVolumeManagerInfos();
    bool bgotcustomvms = GetCustomVolumeManagerInfos();

    return (bgotosvms && bgotcustomvms);
}


bool VolumeInfoCollector::GetCustomVolumeManagerInfos(void)
{
    AppDevicesList_t appdevslist;
    bool bgotcustomvms = true ;

    // 1905 - disable application device list discovery
    // Bug 4653493: Disable Oracle ASM device discovery in Linux
    // bgotcustomvms = getAppDevicesList(appdevslist);

    if (bgotcustomvms)
    {
        printAppDevicesList(appdevslist);
        ConstAppDevicesListIter_t iter = appdevslist.begin(); 
        for ( /* empty */ ; iter != appdevslist.end(); iter++)
        {
            const AppDevices_t &appdevs = *iter;
            Vg_t vg;
            vg.m_Vendor = appdevs.m_Vendor;
            vg.m_Name = appdevs.m_Name;
            if (vg.m_Name.empty())
            {
                vg.m_Name = StrVendor[appdevs.m_Vendor];
            }
            std::set<std::string>::const_iterator diter = appdevs.m_Devices.begin();
            for ( /* empty */ ; diter != appdevs.m_Devices.end(); diter++)
            {
                const std::string &dev = *diter;
                struct stat64 devstat;
                if ((0 == stat64(dev.c_str(), &devstat)) && devstat.st_rdev)
                {
                    vg.m_InsideDevts.insert(devstat.st_rdev);
                }
            }
            InsertVg(vg);
        }
    }
    else
    {
        /* printing as error; This should be a genuine error 
        * but api should not fail if no application is present or 
        * application is present but not using any devices */
        DebugPrintf(SV_LOG_ERROR, "could not get application devices list from getAppDevicesList\n");
    }

    return bgotcustomvms;
}


void VolumeInfoCollector::CopyVolumeInfoToVolumeSummary(VolumeInfoCollectorInfo & volumeInfo, VolumeSummary & volumeSummary)
{
    volumeSummary.id = volumeInfo.m_DeviceID;
    volumeSummary.name = volumeInfo.DeviceNameToReport(m_deviceNameTypeToReport);
    volumeSummary.type = volumeInfo.m_DeviceType;
    volumeSummary.vendor = volumeInfo.m_DeviceVendor;
    volumeSummary.fileSystem = volumeInfo.m_FileSystem;
    volumeSummary.mountPoint = volumeInfo.m_MountPoint;
    volumeSummary.isMounted = volumeInfo.m_Mounted;
    volumeSummary.isSystemVolume = volumeInfo.SystemVolume();
    volumeSummary.isCacheVolume = volumeInfo.m_CacheVolume;
    volumeSummary.capacity = volumeInfo.m_RawSize;
    volumeSummary.locked = volumeInfo.m_Locked;
    volumeSummary.physicalOffset = volumeInfo.m_PhysicalOffset;
    volumeSummary.sectorSize = volumeInfo.m_SectorSize;
    volumeSummary.volumeLabel = volumeInfo.m_VolumeLabel;
    volumeSummary.containPagefile =  volumeInfo.m_PageVolume;
    volumeSummary.isStartingAtBlockZero = volumeInfo.m_IsStartingAtBlockZero;
    volumeSummary.isMultipath = volumeInfo.m_IsMultipath;
    volumeSummary.writeCacheState = volumeInfo.m_WCState;
    volumeSummary.formatLabel = volumeInfo.m_FormatLabel;
    volumeSummary.volumeGroup = volumeInfo.m_VolumeGroup;
    volumeSummary.volumeGroupVendor = volumeInfo.m_VolumeGroupVendor;
    volumeSummary.rawcapacity = volumeInfo.m_LunCapacity;
    volumeSummary.attributes = volumeInfo.m_Attributes;
}


void VolumeInfoCollector::CopyVolumeInfoToVolumeDynamicInfo(VolumeInfoCollectorInfo & volumeInfo, VolumeDynamicInfo & volumeDynamicInfo)
{
    volumeDynamicInfo.id = volumeInfo.m_DeviceID;
    volumeDynamicInfo.name = volumeInfo.DeviceNameToReport(m_deviceNameTypeToReport);
    volumeDynamicInfo.freeSpace = volumeInfo.m_FreeSpace;
}


void VolumeInfoCollector::CopyVolumeInfosToVolumeSummaries(VolumeInfoCollectorInfos_t & volInfos,
                                                           VolumeSummaries_t & volumeSummaries,
                                                           VolumeDynamicInfos_t & volumeDynamicInfos)
{
    // for now we only add leaf devices as follows
    // 1) logical volume group volumes are always added
    // 2) partion logical volumes are added if not part of a logical volume group
    // 3) extended partions are added if no partion logical volumes nor part of a logical volume group
    // 4) partions are added if not part of a logical volume group
    // 5) full devices are added it no partions (primary or extended) nor part of a logical volume group
    VolumeInfoCollectorInfos_t::iterator diter(volInfos.begin());

    for ( /* empty */ ; diter != volInfos.end(); ++diter) 
    {
        VolumeSummary dvol;
        VolumeDynamicInfo dvdi;
        VolumeSummaries_t *pvs = &volumeSummaries;
        VolumeDynamicInfos_t *pvdi = &volumeDynamicInfos;
        VolumeInfoCollectorInfo &dvolinfo = diter->second;
        dvolinfo.UpdateReportStatus(m_RegisterSystemDevices, m_ReportFullDevicesOnly);
        if (dvolinfo.m_ShouldCollect)
        {
            CopyVolumeInfoToVolumeSummary(dvolinfo, dvol);
            CopyVolumeInfoToVolumeDynamicInfo(dvolinfo, dvdi);
            VolumeSummariesIter vsiter = volumeSummaries.insert(volumeSummaries.end(), dvol);
            VolumeSummary &dvs = *vsiter;
            pvs = &dvs.children;
            VolumeDynamicInfosIter vdiiter = volumeDynamicInfos.insert(volumeDynamicInfos.end(), dvdi);
            VolumeDynamicInfo &newdvdi = *vdiiter;
            pvdi = &newdvdi.children;
        }

        VolumeInfoCollectorInfos_t::iterator dpiter(dvolinfo.m_Children.begin());
        for ( /* empty */ ; dpiter != dvolinfo.m_Children.end(); dpiter++)
        {
            VolumeSummary dpvol;
            VolumeDynamicInfo dpvdi;
            VolumeSummaries_t *pdvs = pvs;
            VolumeDynamicInfos_t *pdvdi = pvdi;
            VolumeInfoCollectorInfo &dpvolinfo = dpiter->second;
            dpvolinfo.UpdateReportStatus(m_RegisterSystemDevices, m_ReportFullDevicesOnly);
            if (dpvolinfo.m_ShouldCollect)
            {
                CopyVolumeInfoToVolumeSummary(dpvolinfo, dpvol);
                CopyVolumeInfoToVolumeDynamicInfo(dpvolinfo, dpvdi);
                VolumeSummariesIter dpvsiter = pvs->insert(pvs->end(), dpvol);
                VolumeSummary &dpvs = *dpvsiter;
                pdvs = &dpvs.children;
                VolumeDynamicInfosIter dpvdiiter = pvdi->insert(pvdi->end(), dpvdi);
                VolumeDynamicInfo &newdpvdi = *dpvdiiter;
                pdvdi = &newdpvdi.children;
            }

            VolumeInfoCollectorInfos_t::iterator piter(dpvolinfo.m_Children.begin());
            for ( /* empty */ ; piter != dpvolinfo.m_Children.end(); piter++)
            {
                VolumeSummary pvol;
                VolumeDynamicInfo partitionvdi;
                VolumeInfoCollectorInfo &pvolinfo = piter->second;
                pvolinfo.UpdateReportStatus(m_RegisterSystemDevices, m_ReportFullDevicesOnly);
                if (pvolinfo.m_ShouldCollect)
                {
                    CopyVolumeInfoToVolumeSummary(pvolinfo, pvol);
                    CopyVolumeInfoToVolumeDynamicInfo(pvolinfo, partitionvdi);
                    pdvs->push_back(pvol);
                    pdvdi->push_back(partitionvdi);
                }
            }
        }
    }
}


void VolumeInfoCollector::CopyAllVolumeInfosToVolumeSummaries(VolumeInfoCollectorInfos_t & volInfos,
                                                              VolumeSummaries_t & volumeSummaries)
{
    VolumeInfoCollectorInfos_t::iterator diter(volInfos.begin());

    for ( /* empty */ ; diter != volInfos.end(); ++diter) 
    {
        VolumeSummary dvol;
        VolumeSummaries_t *pvs = &volumeSummaries;
        VolumeInfoCollectorInfo &dvolinfo = diter->second;
        if (dvolinfo.m_ShouldCollect)
        {
            CopyVolumeInfoToVolumeSummary(dvolinfo, dvol);
            VolumeSummariesIter vsiter = volumeSummaries.insert(volumeSummaries.end(), dvol);
            VolumeSummary &dvs = *vsiter;
            pvs = &dvs.children;
        }

        VolumeInfoCollectorInfos_t::iterator dpiter(dvolinfo.m_Children.begin());
        for ( /* empty */ ; dpiter != dvolinfo.m_Children.end(); dpiter++)
        {
            VolumeSummary dpvol;
            VolumeSummaries_t *pdvs = pvs;
            VolumeInfoCollectorInfo &dpvolinfo = dpiter->second;
            if (dpvolinfo.m_ShouldCollect)
            {
                CopyVolumeInfoToVolumeSummary(dpvolinfo, dpvol);
                VolumeSummariesIter dpvsiter = pvs->insert(pvs->end(), dpvol);
                VolumeSummary &dpvs = *dpvsiter;
                pdvs = &dpvs.children;
            }

            VolumeInfoCollectorInfos_t::iterator piter(dpvolinfo.m_Children.begin());
            for ( /* empty */ ; piter != dpvolinfo.m_Children.end(); piter++)
            {
                VolumeSummary pvol;
                VolumeInfoCollectorInfo &pvolinfo = piter->second;
                if (pvolinfo.m_ShouldCollect)
                {
                    CopyVolumeInfoToVolumeSummary(pvolinfo, pvol);
                    pdvs->push_back(pvol);
                }
            }
        }
    }
}

void VolumeInfoCollector::InsertVolumeInfo(VolumeInfoCollectorInfo const & volInfo)
{
    m_VolumeInfos.insert(std::make_pair(volInfo.m_DeviceName, volInfo));
}

void VolumeInfoCollector::InsertVolumeInfoChild(VolumeInfoCollectorInfo &parentVolInfo, VolumeInfoCollectorInfo const & childVolInfo)
{
    parentVolInfo.m_Children.insert(std::make_pair(childVolInfo.m_DeviceName, childVolInfo));
}

void VolumeInfoCollector::DumpVolumeInfos() const
{
    VolumeInfoCollectorInfos_t::const_iterator iter(m_VolumeInfos.begin());
    VolumeInfoCollectorInfos_t::const_iterator endIter(m_VolumeInfos.end());

    for (/* empty */; iter != endIter; ++iter) {
        (*iter).second.Dump();
    }
}

void VolumeInfoCollector::DumpVolumeSummaries(VolumeSummaries_t & volumeSummaries) const
{
    VolumeSummaries_t::iterator iter(volumeSummaries.begin());
    VolumeSummaries_t::iterator endIter(volumeSummaries.end());

    for (/* empty */; iter != endIter; ++iter) {
        std::stringstream msg;
        msg << "----------------------------------------------------------\n"
            << "ID                               : " << (*iter).id << '\n'
            << "Name                             : " << (*iter).name << '\n'
            << "Type                             : " << StrDeviceType[(*iter).type] << '\n'
            << "Vendor                           : " << StrVendor[(*iter).vendor] << '\n'
            << "File System                      : " << (*iter).fileSystem << '\n'
            << "Mount Point                      : " << (*iter).mountPoint << '\n'
            << "Mounted                          : " << (*iter).isMounted << '\n'
            << "System Volume                    : " << (*iter).isSystemVolume << '\n'
            << "Cache Volume                     : " << (*iter).isCacheVolume << '\n'
            << "Capacity                         : " << (*iter).capacity << '\n'
            << "Locked                           : " << (*iter).locked << '\n'
            << "Physical Offset                  : " << (*iter).physicalOffset << '\n'
            << "Sector Size                      : " << (*iter).sectorSize << '\n'
            << "Volume Label                     : " << (*iter).volumeLabel << '\n'
            << "Contains Page File               : " << (*iter).containPagefile << '\n'
            << "Starting At Block Zero           : " << (*iter).isStartingAtBlockZero << '\n'
            << "Volume Group                     : " << (*iter).volumeGroup << '\n'
            << "Volume Group Vendor              : " << StrVendor[(*iter).volumeGroupVendor] << '\n'
            << "Multipath                        : " << (*iter).isMultipath << '\n'
            << "Write Cache State                : " << StrWCState[(*iter).writeCacheState] << '\n'
            << "Format Label                     : " << StrFormatLabel[(*iter).formatLabel] << '\n'
            << "Raw Size                         : " << (*iter).rawcapacity << '\n'
            << "attributes                       : " << (*iter).attributes << '\n'
            << "\n\n";
        DumpMsg(msg.str().c_str());

        if (!(*iter).children.empty())
        {
            DumpVolumeSummaries((*iter).children);
        }
    }
}


void VolumeInfoCollector::DumpVolumeDynamicInfos(VolumeDynamicInfos_t & volumeDynamicInfos) const
{
    VolumeDynamicInfos_t::iterator iter(volumeDynamicInfos.begin());
    VolumeDynamicInfos_t::iterator endIter(volumeDynamicInfos.end());

    for (/* empty */; iter != endIter; ++iter) {
        std::stringstream msg;
        msg << "----------------------------------------------------------\n"
            << "ID                               : " << (*iter).id << '\n'
            << "Name                             : " << (*iter).name << '\n'
            << "Free Space                       : " << (*iter).freeSpace << '\n'
            << "\n\n";
        DumpMsg(msg.str().c_str());

        if (!(*iter).children.empty())
        {
            DumpVolumeDynamicInfos((*iter).children);
        }
    }
}

bool VolumeInfoCollector::GetCustomDevices()
{
    m_CustomDevices.Load();

    CustomDevices::CustomDevices_t::const_iterator iter(m_CustomDevices.begin());
    CustomDevices::CustomDevices_t::const_iterator endIter(m_CustomDevices.end());

    struct stat64 volStat;

    VolumeInfoCollectorInfo volInfo;

    for (/*empty*/; iter != endIter; ++iter) {
        if (!(*iter).m_Valid) {
            continue;
        }

        // make sure custom device exists
        if (-1 == stat64((*iter).m_Name.c_str(), &volStat)) {
            continue;
        }

        volInfo.m_DeviceName = (*iter).m_Name;
        volInfo.m_Locked = IsDeviceLocked((*iter).m_Name);
        volInfo.m_MountPoint = (*iter).m_Mount;
        volInfo.m_RawSize = GetVolumeSizeInBytes((*iter).m_Name, (*iter).m_Size);
        volInfo.m_LunCapacity = volInfo.m_RawSize + m_LunCapacityInformer.GetLengthForLunMakeup();
        volInfo.m_DumpDevice = m_DumpDevice.IsDumpDevice(volInfo.m_DeviceName);
#ifdef SV_AIX
        volInfo.m_Major = major64(volStat.st_rdev);
        volInfo.m_Minor = minor64(volStat.st_rdev);
#else
        volInfo.m_Major = major(volStat.st_rdev);
        volInfo.m_Minor = minor(volStat.st_rdev);
#endif
        volInfo.m_DeviceType = VolumeSummary::CUSTOM_DEVICE;
        volInfo.m_DeviceVendor = VolumeSummary::CUSTOMVENDOR;
        volInfo.m_DeviceID = m_DeviceIDInformer.GetID(volInfo.m_DeviceName);
        if (volInfo.m_DeviceID.empty())
        {
            DebugPrintf(SV_LOG_WARNING, "could not get ID for %s with error %s\n", volInfo.m_DeviceName.c_str(), m_DeviceIDInformer.GetErrorMessage().c_str());
        }
        volInfo.m_WCState = m_WriteCacheInformer.GetWriteCacheState(volInfo.m_DeviceName);
        UpdateMountDetails(volInfo);
        UpdateSwapDetails(volInfo);
        volInfo.m_Attributes = GetAttributes(volInfo.m_DeviceName);

        InsertVolumeInfo(volInfo);
    }

    return true;
}


bool VolumeInfoCollector::GetLogicalVolumeInfos()
{
    bool bgotlvs = GetLvs();
    bool bgotvglvs = GetVgLvs();
    bool bgotinfos = (bgotlvs && bgotvglvs);

    return bgotinfos;
}


off_t VolumeInfoCollector::SizeUnitsToSizeBytes(float size, std::string const & units)
{
    if (std::string::npos != units.find(GB_UNITS)) {
        return static_cast<off_t>(size * ONE_GB);
    } else if (std::string::npos != units.find(MB_UNITS)) {
        return static_cast<off_t>(size * ONE_MB);
    } else if (std::string::npos != units.find(TB_UNITS)) {
        return static_cast<off_t>(size * ONE_TB);
    } else if (std::string::npos != units.find(KB_UNITS)) {
        return static_cast<off_t>(size * ONE_KB);
    }
    return 0;
}


bool VolumeInfoCollector::IsDeviceLocked(std::string const & deviceName)
{
    // FIXME: need to implement this
    deviceName.empty(); // keep compiler happy for now
    return false;
}

bool VolumeInfoCollector::IsDeviceTracked(int major, std::string const & deviceName)
{
    return (m_DeviceTracker.IsTracked(major, deviceName.c_str()));
}


void VolumeInfoCollector::GetVirtualVolumes()
{
    // gets the onse created by cdpcli and placed in drscout.conf
    // makes the following assumptions:
    // 1. virtual volume paths look like
    //       filepath--volume
    // 2. volume does not contain any dashes ('-') in its name even though
    //    that would be legal for volume names to have them
    // 3. volume is at least at idx 4. that  allows for the shortest
    //    possible path of something like "/a" where '/' is the dir and 'a'
    //    is the file
    // 4. virtual volume ids start at 1 (i.e there is no id = 0)
    //

    long long size = 0;

    for (int counter = m_localConfigurator.getVirtualVolumesId(); counter > 0; --counter) {

        std::stringstream regData;

        regData << "VirVolume" << counter;

        std::string data = m_localConfigurator.getVirtualVolumesPath(regData.str());

        std::string sparsefilename;
        std::string volume;

        if (!parseSparseFileNameAndVolPackName(data, sparsefilename, volume))
        {
            continue;
        }

        if(!m_localConfigurator.IsVolpackDriverAvailable())
        {
            //ACE_LOFF_T size = 0;
            ACE_stat s;
            std::string sparsefile;
            bool newvirtualvolume = false; 
           IsVolPackDevice(volume.c_str(),sparsefile,newvirtualvolume);

            if(newvirtualvolume)
            {
                int i = 0;
                std::stringstream sparsepartfile;
                while(true)
                {
                    sparsepartfile.str("");
                    sparsepartfile << sparsefilename<< SPARSE_PARTFILE_EXT << i;
                    if( sv_stat( getLongPathName(sparsepartfile.str().c_str()).c_str(), &s ) < 0 ) 
                    {
                        break;
                    }
                    size+=s.st_size;
                    i++;
                }
            }
            else
            {
                size  = ( sv_stat( getLongPathName(sparsefilename.c_str()).c_str(), &s ) < 0 ) ? 0 : s.st_size;
            }
        }
        else
        {
            std::string rawvolume = GetRawDeviceName(volume);
            int fd = open(rawvolume.c_str(), O_RDONLY);
            if (-1 == fd) {
                continue;
            }

            /*
            * TODO: we should use GetVolumeSizeInBytes(name, offset, sector) instead of this
            * since below only hits BLKGETSIZE64 which is not supported on all linuses
            */
            size = GetVolumeSizeInBytes(fd, volume);

            close(fd);
        }


        VolumeInfoCollectorInfo volInfo;
        volInfo.m_DeviceName = volume;
        volInfo.m_RealName = volume;
        volInfo.m_Mounted = true;
        volInfo.m_Locked = IsDeviceLocked(volume);
        volInfo.m_DeviceType = VolumeSummary::VOLPACK;
        volInfo.m_DeviceVendor = VolumeSummary::INMVOLPACK;
        volInfo.m_RawSize = size;
        volInfo.m_LunCapacity = volInfo.m_RawSize;

        struct stat64 volStat;
        if (0 == stat64(volInfo.m_DeviceName.c_str(), &volStat))
        {
#ifdef SV_AIX
            volInfo.m_Major = major64(volStat.st_rdev);
            volInfo.m_Minor = minor64(volStat.st_rdev); 
#else
            volInfo.m_Major = major(volStat.st_rdev);
            volInfo.m_Minor = minor(volStat.st_rdev); 
#endif

        }
        UpdateMountDetails(volInfo);

        InsertVolumeInfo(volInfo);
    }
}

void VolumeInfoCollector::GetVolPacks()
{
    // gets the ones created by vol_pack.sh and placed in volpack.conf
    long long size = 0;

    std::string volpackConfName(m_localConfigurator.getInstallPath() + "/etc/");
    volpackConfName += "volpack.conf";

    std::ifstream volpackConfFile(volpackConfName.c_str());
    if (!volpackConfFile.is_open()) {
        return;
    }

    std::string fileName;
    std::string deviceName;
    unsigned long long tmp;

    while (!volpackConfFile.eof()) {

        volpackConfFile >> fileName >> deviceName >> tmp;

        std::string rawDeviceName = GetRawDeviceName(deviceName);
        int fd = open(rawDeviceName.c_str(), O_RDONLY);
        if (-1 == fd) {
            continue;
        }

        size = GetVolumeSizeInBytes(fd, deviceName);

        close(fd);

        VolumeInfoCollectorInfo volInfo;
        volInfo.m_DeviceName = deviceName;
        volInfo.m_RealName = deviceName;
        volInfo.m_Mounted = false;
        volInfo.m_Locked = IsDeviceLocked(deviceName);
        volInfo.m_DeviceType = VolumeSummary::VOLPACK;
        volInfo.m_DeviceVendor = VolumeSummary::INMVOLPACK;
        volInfo.m_RawSize = size;
        volInfo.m_LunCapacity = volInfo.m_RawSize;
        struct stat64 volStat;
        if (0 == stat64(volInfo.m_DeviceName.c_str(), &volStat))
        {
#ifdef SV_AIX
            volInfo.m_Major = major64(volStat.st_rdev);
            volInfo.m_Minor = minor64(volStat.st_rdev); 
#else
            volInfo.m_Major = major(volStat.st_rdev);
            volInfo.m_Minor = minor(volStat.st_rdev); 
#endif

        }
        UpdateMountDetails(volInfo);

        InsertVolumeInfo(volInfo);
    }
}


void VolumeInfoCollector::UpdateAttrsWithChildren(VolumeInfoCollectorInfo &parentVolInfo)
{
    CopyAttrsToChildren(parentVolInfo);
    CopyAttrsToParents(parentVolInfo);
}


void VolumeInfoCollector::CopyAttrsToChildren(VolumeInfoCollectorInfo &parentVolInfo)
{
    VolInfoIter childBeginIter = parentVolInfo.m_Children.begin();
    VolInfoIter childEndIter = parentVolInfo.m_Children.end();
    for (VolInfoIter childIter = childBeginIter; childIter != childEndIter; childIter++)
    {
        VolumeInfoCollectorInfo &chldVolInfo = childIter->second;
        CopyAttrsToChildren(chldVolInfo);
        /* From parent, copy attributes and vg to child; 
        * then from child, copy attributes and vg to parent */
        CopyAttrsFromParentToChild ptoc(&parentVolInfo);
        ptoc(*childIter);
    }
}


void VolumeInfoCollector::CopyAttrsToParents(VolumeInfoCollectorInfo &parentVolInfo)
{
    VolInfoIter childBeginIter = parentVolInfo.m_Children.begin();
    VolInfoIter childEndIter = parentVolInfo.m_Children.end();
    for (VolInfoIter childIter = childBeginIter; childIter != childEndIter; childIter++)
    {
        VolumeInfoCollectorInfo &chldVolInfo = childIter->second;
        CopyAttrsToParents(chldVolInfo);
        /* From parent, copy attributes and vg to child; 
        * then from child, copy attributes and vg to parent */
        CopyAttrsFromChildToParent ctop(&parentVolInfo);
        ctop(*childIter);
    }
}


void VolumeInfoCollector::InsertLv(const Lv_t &lv)
{
    m_Lvs.insert(std::make_pair(lv.m_Name, lv));
}


void VolumeInfoCollector::InsertLv(Lvs_t &lvs, const Lv_t &lv)
{
    lvs.insert(std::make_pair(lv.m_Name, lv));
}


void VolumeInfoCollector::InsertVg(const Vg_t &vg)
{
    m_Vgs.insert(std::make_pair(vg.m_Name, vg));
}


void VolumeInfoCollector::InsertMountInfo(const MountInfo_t &mntinfo)
{
    if(std::find(m_DirMountPoints.begin(),m_DirMountPoints.end(), mntinfo.m_MountPoint) == m_DirMountPoints.end()){
        m_MountInfos.insert(std::make_pair(mntinfo.m_MountedResource, mntinfo));
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "Not reporting %s as it is a directory mount point\n", mntinfo.m_MountPoint.c_str()) ;
    }
}


void VolumeInfoCollector::UpdateLvToDisk(VolumeInfoCollectorInfos_t &volInfos, VolumeInfoCollectorInfo &lvVolInfo)
{
    VolumeInfoCollectorInfos_t::iterator diter(volInfos.begin());

    for ( /* empty */ ; diter != volInfos.end(); ++diter)
    {
        VolumeInfoCollectorInfo &dvolinfo = diter->second;

        if (!boost::equals(dvolinfo.m_VolumeGroup, lvVolInfo.m_VolumeGroup) ||
                VolumeSummary::DISK != dvolinfo.m_DeviceType)
            continue;

        if (lvVolInfo.m_SystemVolume)
            dvolinfo.m_SystemVolume = lvVolInfo.m_SystemVolume;

        if (lvVolInfo.m_BootDisk && !dvolinfo.m_BootDisk)
        {
            dvolinfo.m_BootDisk = lvVolInfo.m_BootDisk;
            dvolinfo.m_Attributes.insert(std::make_pair(NsVolumeAttributes::HAS_BOOTABLE_PARTITION, "true"));
        }

        if (lvVolInfo.m_EncryptedDisk)
            dvolinfo.m_EncryptedDisk = lvVolInfo.m_EncryptedDisk;
    }
}


void VolumeInfoCollector::UpdateMountDetails(VolumeInfoCollectorInfo &volInfo)
{
    MountInfos_t::const_iterator begin = m_MountInfos.begin();
    MountInfos_t::const_iterator end = m_MountInfos.end();
#ifdef SV_AIX
    dev_t devt = makedev64(volInfo.m_Major, volInfo.m_Minor);
#else
    dev_t devt = makedev(volInfo.m_Major, volInfo.m_Minor);
#endif 
    do
    {
        MountInfos_t::const_iterator iter = find_if(begin, end, EqMountedResourceDevt(devt));
        if (end != iter)
        {
            CopyMountInfoToVolInfo(iter->second, volInfo);
            /* ++ to give next interator as new begin */
            iter++;
        }
        begin = iter;
    } while (begin != end);
}

//
// the attrributes of the mount point are collected from multiple sources
// for ex on Linux - /etc/mtab, /etc/fstab and /proc/mounts
// this function is called multiple times for a mount point for each source of info
// an union of the attributes are populated in volinfo
//
// only in case the current mount point is / and if a previous mount point is
// not /, overwrite the mount attributes 
//
void VolumeInfoCollector::CopyMountInfoToVolInfo(const MountInfo_t &mntInfo, VolumeInfoCollectorInfo &volInfo)
{
    DebugPrintf(SV_LOG_DEBUG,"%s: device %s, mountinfo %s\n",
                FUNCTION_NAME,
                volInfo.m_DeviceName.c_str(),
                mntInfo.Dump().c_str());

    if (!volInfo.m_MountPoint.empty())
    {
        if (!boost::equals(mntInfo.m_MountPoint, volInfo.m_MountPoint))
        {
            if (boost::equals(volInfo.m_MountPoint, "/"))
            {
                /* when a resource is found mounted at multiple mount points, and if the prev
                 * mount point is '/' skip the current reported mount point
                 */
                DebugPrintf(SV_LOG_ALWAYS,
                    "Not reporting mount point %s for resource %s as another mount point is already found at %s.\n",
                    mntInfo.m_MountPoint.c_str(),
                    mntInfo.m_MountedResource.c_str(),
                    volInfo.m_MountPoint.c_str());

                if (!volInfo.m_BootDisk)
                    volInfo.m_BootDisk = m_SystemMountPoints.IsBootMountPoint(mntInfo.m_MountPoint);

                return;
            }
            else if (boost::equals(mntInfo.m_MountPoint, "/"))
            {
                /* when a resource is found mounted at multiple mount points and if the current
                 * mount point is '/',  overwrite mount info
                 */
                DebugPrintf(SV_LOG_ALWAYS,
                    "Reporting mount point %s for resource %s instead of mount point %s.\n",
                    mntInfo.m_MountPoint.c_str(),
                    mntInfo.m_MountedResource.c_str(),
                    volInfo.m_MountPoint.c_str());

                volInfo.m_Mounted = mntInfo.m_IsMounted;
                volInfo.m_SystemVolume = mntInfo.m_IsSystemMountPoint;
                volInfo.m_SwapVolume = mntInfo.m_IsSwapFileSystem;
                volInfo.m_FileSystem = mntInfo.m_FileSystem;
                volInfo.m_MountPoint = mntInfo.m_MountPoint;
            }
        }
    }

    if (!volInfo.m_Mounted)
    {
        volInfo.m_Mounted = mntInfo.m_IsMounted;
    }
    if (!volInfo.m_SystemVolume)
    {
        volInfo.m_SystemVolume = mntInfo.m_IsSystemMountPoint;
    }
    if (!volInfo.m_SwapVolume)
    {
        volInfo.m_SwapVolume = mntInfo.m_IsSwapFileSystem;
    }

    if (volInfo.m_FileSystem.empty())
    {
        volInfo.m_FileSystem = mntInfo.m_FileSystem;
    }

    if (volInfo.m_MountPoint.empty())
    {
        volInfo.m_MountPoint = mntInfo.m_MountPoint;
    }

    /* assumed to be only once and always present; 
    * TODO: IsCacheVolume is not getting updated for
    *       mount points from "/etc/mtab" and "/etc/fstab" 
    *       files; should this be done ? as for ubuntu, the
    *       mount point from "/etc/mtab" is not correct ? */
    if (mntInfo.m_IsMounted)
    {
        GetFreeSpace(mntInfo, volInfo);
        volInfo.m_CacheVolume = mntInfo.m_IsCacheMountPoint;
        volInfo.m_MountPoint = mntInfo.m_MountPoint;
        if (!volInfo.m_BootDisk)
            volInfo.m_BootDisk = m_SystemMountPoints.IsBootMountPoint(volInfo.m_MountPoint);
        volInfo.m_FileSystem = mntInfo.m_FileSystem;
    }
}


void VolumeInfoCollector::GetFreeSpace(const MountInfo_t &mntInfo, VolumeInfoCollectorInfo &volInfo)
{
    struct statvfs64 mntStat;
    if (0 == statvfs64(mntInfo.m_MountPoint.c_str(), &mntStat)) 
    {
        off_t rawSize = static_cast<off_t>(mntStat.f_blocks) * static_cast<off_t>(mntStat.f_frsize);
        off_t freeSpace = CalculateFSFreeSpace(mntStat);

        if (0 == volInfo.m_RawSize) 
        {
            volInfo.m_RawSize = GetVolumeSizeInBytes(volInfo.m_DeviceName, rawSize);
        }
        if (rawSize == volInfo.m_RawSize) 
        {
            // only if the raw size was the same, the reasons is if it is not
            // then we can't trust the free space value
            volInfo.m_FreeSpace = freeSpace;
        } 
        else 
        {
            // looks like we can actually read more then what statvfs reports as the raw size.
            // as such the free space that is reported is not correct with respect to the actual size.
            // So what we will do is figure out the precent that is report and to be conservative we
            // will report the lesser of of 2. this is done as in some cases statvfs reports a very large
            // raw size and free space that is not correct. once that issue is fixed we could just
            // always report the statvfs free space value.
            off_t roughFreeSpace = static_cast<off_t>((static_cast<long double>(freeSpace) / static_cast<long double>(rawSize))
                * volInfo.m_RawSize);
            volInfo.m_FreeSpace = (freeSpace < roughFreeSpace ? freeSpace : roughFreeSpace);
        }
    }
}


void VolumeInfoCollector::UpdateSwapDetails(VolumeInfoCollectorInfo &volInfo)
{
    if (!volInfo.m_SwapVolume)
    {
        volInfo.m_SwapVolume = m_SwapVolumes.IsSwapVolume(volInfo);
    }
}


void VolumeInfoCollector::UpdateVgLvFromLvInfo(const std::string &vgname, Lv_t &lvinfo)
{
    LvsIter begin = m_Lvs.begin();
    LvsIter end = m_Lvs.end();

    /* we expect one to one match from lv to dm. but do while added to handle all cases */
    do
    {
        LvsIter iter = find_if(begin, end, LvEqDevt(lvinfo.m_Devt));
        if (end != iter)
        {
            iter->second.m_VgName = vgname;
            lvinfo.m_ShouldCollect = lvinfo.m_IsValid = false;
            /* ++ to give next interator as new begin */
            iter++;
        }
        begin = iter;
    } while (begin != end);
}


void VolumeInfoCollector::UpdateLvFromVgLvInfo(const std::string &vgname, const Lv_t &lvinfo)
{
    LvsIter begin = m_Lvs.begin();
    LvsIter end = m_Lvs.end();

    /* we expect one to one match from lv to dm. but do while added to handle all cases */
    do
    {
        LvsIter iter = find_if(begin, end, LvEqDevt(lvinfo.m_Devt));
        if (end != iter)
        {
            iter->second.m_VgName = vgname;
            iter->second.m_ShouldCollect = iter->second.m_IsValid = false;
            /* ++ to give next interator as new begin */
            iter++;
        }
        begin = iter;
    } while (begin != end);
}


/* should be called for all except lvs standalone and vglvs */
/* NOTE: wc state, mount info, swap info of volInfo has to be calculated before  
calling this. do not call this function for lvs and lvs in vgs */
void VolumeInfoCollector::UpdateLvInsideDevts(VolumeInfoCollectorInfo &volInfo)
{
    if (VolumeSummary::LOGICAL_VOLUME != volInfo.m_DeviceType) 
    {
        /* TODO: Gets called from dev mapper disks and partitions too ;
        * Need to check its impact since updatelvfromlvinfo is also
        * done; For now not seeing any effects */
#ifdef SV_AIX
        dev_t devt = makedev64(volInfo.m_Major, volInfo.m_Minor); 
#else 
        dev_t devt = makedev(volInfo.m_Major, volInfo.m_Minor); 
#endif
        /* exercise already done by UpdateLvFromLvInfo */
        LvsIter lvbegin = m_Lvs.begin();
        LvsIter lvend = m_Lvs.end();
        dev_t prevlvdevt = 0;
        std::string prevvgname;
  
        do 
        {
            LvsIter lviter = find_if(lvbegin, lvend, LvEqOnlyInsideDevt(devt));
            if (lvend != lviter)
            {
                dev_t currlvdevt = lviter->second.m_Devt;
                if (volInfo.m_VolumeGroup.empty() && !boost::equals(lviter->second.m_VgName, lviter->second.m_Name))
                {
                    volInfo.m_VolumeGroup = lviter->second.m_VgName;
                    volInfo.m_VolumeGroupVendor = lviter->second.m_Vendor;
                }
                else
                {
                    if (prevlvdevt && (currlvdevt != prevlvdevt) && (lviter->second.m_VgName != prevvgname))
                    {
                        DebugPrintf(SV_LOG_DEBUG,
                        "%s: not collecting %s because of mismatch in devt (%d, %d) and vgname (%s, %s).\n",
                        FUNCTION_NAME,
                        volInfo.m_DeviceName.c_str(),
                        currlvdevt,
                        prevlvdevt,
                        lviter->second.m_VgName.c_str(),
                        prevvgname.c_str());

                        if (volInfo.m_ShouldCollect)
                        {
                            volInfo.m_ShouldCollect = false;
                        }
                        if (volInfo.m_ShouldCollectParent)
                        {
                            volInfo.m_ShouldCollectParent = false;
                        }
                    }
                }
                prevlvdevt = currlvdevt;
                prevvgname = lviter->second.m_VgName;

                Lv_t &lv = lviter->second;
                if (lv.m_IsMountedOnRoot)
                    volInfo.m_HasLvmMountedOnRoot = true;

                /* do not overwrite cache enabled state */
                if ((VolumeSummary::WRITE_CACHE_DONTKNOW != volInfo.m_WCState) &&
                    ((VolumeSummary::WRITE_CACHE_DONTKNOW == lviter->second.m_WCState) || 
                    (VolumeSummary::WRITE_CACHE_DISABLED == lviter->second.m_WCState)))
                {
                    lviter->second.m_WCState = volInfo.m_WCState;
                    UpdateWCStateForLv(&m_Lvs, volInfo.m_WCState, &lv);
                } 
                if (lviter->second.m_NTopLvs > 1)
                {
                    /* Do not report if lviter->second.m_NTopLvs > 1 and there are more 
                    * than one top most lvs */
                    if (!lviter->second.m_HasOneTopMostLv)
                    {
                        DebugPrintf(SV_LOG_DEBUG,
                        "%s: not collecting %s because there is no top most lv detected but has %d top lvs.\n",
                        FUNCTION_NAME,
                        volInfo.m_DeviceName.c_str(),
                        lviter->second.m_NTopLvs,
                        lviter->second.m_HasOneTopMostLv);
                    }

                    if (volInfo.m_ShouldCollect)
                    {
                        volInfo.m_ShouldCollect = lviter->second.m_HasOneTopMostLv;
                    }
                    if (volInfo.m_ShouldCollectParent)
                    {
                        volInfo.m_ShouldCollectParent = lviter->second.m_HasOneTopMostLv;
                    }
                }
                if (volInfo.m_SwapVolume && !(lviter->second.m_SwapVolume))
                {
                    lviter->second.m_SwapVolume = volInfo.m_SwapVolume;
                    UpdateSwapStateForLv(&m_Lvs, volInfo.m_SwapVolume, 
                        &lv);
                }

                if (!volInfo.m_EncryptedDisk && lviter->second.m_Encrypted)
                    volInfo.m_EncryptedDisk = lviter->second.m_Encrypted;

                if (!volInfo.m_BekVolume && lviter->second.m_BekVolume)
                    volInfo.m_BekVolume = lviter->second.m_BekVolume;

                lviter++;
            }
            lvbegin = lviter;
        } while (lvend != lvbegin);

        VgsIter vgbegin = m_Vgs.begin();
        VgsIter vgend = m_Vgs.end();

        do
        {
            VgsIter vgiter = find_if(vgbegin, vgend, VgEqInsideDevt(devt));
            if (vgend != vgiter)
            {
                if (volInfo.m_VolumeGroup.empty())
                {
                    volInfo.m_VolumeGroup = vgiter->second.m_Name;
                    volInfo.m_VolumeGroupVendor = vgiter->second.m_Vendor;
                }
                else if(volInfo.m_VolumeGroup != vgiter->second.m_Name)
                {
                    DebugPrintf(SV_LOG_DEBUG,
                    "%s: not collecting %s because of mismatch in vgname (%s, %s).\n",
                    FUNCTION_NAME,
                    volInfo.m_DeviceName.c_str(),
                    volInfo.m_VolumeGroup.c_str(),
                    vgiter->second.m_Name.c_str());

                    if (volInfo.m_ShouldCollect)
                    {
                        volInfo.m_ShouldCollect = false;
                    }
                    if (volInfo.m_ShouldCollectParent)
                    {
                        volInfo.m_ShouldCollectParent = false;
                    }
                }

                if (vgiter->second.m_HasLvmMountedOnRoot)
                    volInfo.m_HasLvmMountedOnRoot = true;

                const LvsIter vglvbegin = vgiter->second.m_Lvs.begin();
                const LvsIter vglvend = vgiter->second.m_Lvs.end();
                if (VolumeSummary::WRITE_CACHE_DONTKNOW != volInfo.m_WCState)
                {
                    for_each(vglvbegin, vglvend, UpdateWcStateForAll(volInfo.m_WCState));
                }
                if (volInfo.m_SwapVolume)
                {
                    for_each(vglvbegin, vglvend, UpdateSwapStateForAll(volInfo.m_SwapVolume));
                }
                vgiter++;
            }
            vgbegin = vgiter;
        } while (vgend != vgbegin);
    } 
}


void VolumeInfoCollector::UpdateVolumeManagerInfos(void)
{
    bool bcopyvgdown = true;

    MarkInValidAndVsnapLvs(m_Lvs, m_VsnapDevts);
    MarkVsnapVgs(m_Vgs, m_VsnapDevts);
    /* NOTE: order is important here. please do not change */
    for_each(m_Lvs.begin(), m_Lvs.end(), UpdateLvFromLvInfo(&m_Lvs, bcopyvgdown));
    for_each(m_Vgs.begin(), m_Vgs.end(), UpdateVgFromVgInfo);
    UpdateTopMostLvInfo(&m_Lvs);
}


bool VolumeInfoCollector::IsLvInsideDevtEncrypted(dev_t devt)
{
    LvsIter lvbegin = m_Lvs.begin();
    LvsIter lvend = m_Lvs.end();

    LvsIter lviter = find_if(lvbegin, lvend, LvEqOnlyInsideDevt(devt));
    if (lvend == lviter)
        return false;

    if (lviter->second.m_Encrypted)
        return true;

    return false;
}


bool VolumeInfoCollector::IsLogicalVolumeMountedOnRoot(Lv_t &lvinfo)
{
    MountInfos_t::const_iterator begin = m_MountInfos.begin();
    MountInfos_t::const_iterator end = m_MountInfos.end();

    dev_t devt = lvinfo.m_Devt;

    do
    {
        MountInfos_t::const_iterator iter = find_if(begin, end, EqMountedResourceDevt(devt));
        if (end != iter)
        {
            if (boost::equals(iter->second.m_MountPoint, "/"))
                return true;

            /* ++ to give next interator as new begin */
            iter++;
        }
        begin = iter;
    } while (begin != end);

    return false;
}


void VolumeInfoCollector::FindLogicalVolumeMountedOnRoot(void)
{
    LvsIter begin = m_Lvs.begin();
    LvsIter end = m_Lvs.end();

    do
    {
        LvsIter iter = find_if(begin, end, ShouldReportLv);
        if (end != iter)
        {
            Lv_t &lv = iter->second;
            if (IsLogicalVolumeMountedOnRoot(lv))
            {
                lv.m_IsMountedOnRoot = true;

                for (VgsIter vgiter = m_Vgs.begin(); vgiter != m_Vgs.end(); vgiter++)
                {
                    Vg_t &vg = vgiter->second;

                    if (!boost::equals(vg.m_Name, lv.m_VgName))
                        continue;

                    vg.m_HasLvmMountedOnRoot = true;
                    break;
                }

                break;
            }
            iter++;
        }
        begin = iter;
    } while (begin != end);
}


bool VolumeInfoCollector::GetLvs(void)
{
    bool bgotlvs = true;
    std::string bootDisk = m_BootDiskInfo.GetBootDisk();
    LvsIter begin = m_Lvs.begin();
    LvsIter end = m_Lvs.end();

    do
    {
        LvsIter iter = find_if(begin, end, ShouldReportLv);
        if (end != iter)
        {
            Lv_t &lv = iter->second;

            std::string realName;
            VolumeInfoCollectorInfo::SymbolicLinks_t symbolicLinks;
            GetSymbolicLinksAndRealName(lv.m_Name, symbolicLinks, realName);

            VolumeInfoCollectorInfo lvVolInfo;
            lvVolInfo.m_DeviceName = lv.m_Name;
            lvVolInfo.m_RealName = realName;
            lvVolInfo.m_DeviceType = VolumeSummary::LOGICAL_VOLUME;
            lvVolInfo.m_DeviceVendor = lv.m_Vendor;
            lvVolInfo.m_DumpDevice = m_DumpDevice.IsDumpDevice(lvVolInfo.m_DeviceName);
            lvVolInfo.m_RawSize = lv.m_Size;
            lvVolInfo.m_LunCapacity = lvVolInfo.m_RawSize + m_LunCapacityInformer.GetLengthForLunMakeup();
            lvVolInfo.m_Locked = IsDeviceLocked(lvVolInfo.m_DeviceName);
            lvVolInfo.m_SymbolicLinks = symbolicLinks;
#ifdef SV_AIX
            lvVolInfo.m_Major = major64(lv.m_Devt);
            lvVolInfo.m_Minor = minor64(lv.m_Devt);
#else
            lvVolInfo.m_Major = major(lv.m_Devt);
            lvVolInfo.m_Minor = minor(lv.m_Devt);
#endif

            lvVolInfo.m_VolumeGroup = lv.m_VgName;
            lvVolInfo.m_VolumeGroupVendor = lv.m_Vendor;
            lvVolInfo.m_WCState = lv.m_WCState;
            if (!bootDisk.empty())
            {
                lvVolInfo.m_BootDisk = (std::string::npos != realName.find(bootDisk));
            }

            if (!lv.m_Encrypted)
            {
                lv.m_Encrypted = IsLvInsideDevtEncrypted(lv.m_Devt);
            }

            lvVolInfo.m_EncryptedDisk = lv.m_Encrypted;
            UpdateMountDetails(lvVolInfo);
            UpdateLvToDisk(m_VolumeInfos, lvVolInfo);
            lvVolInfo.m_SwapVolume = lv.m_SwapVolume;
            UpdateSwapDetails(lvVolInfo);
            lv.m_Collected = true;
            InsertVolumeInfo(lvVolInfo);
            /* ++ to give next interator as new begin */
            iter++;
        }
        begin = iter;
    } while (begin != end);

    return bgotlvs;
}


bool VolumeInfoCollector::GetVgLvs(void)
{
    bool bgotlvminfos = true;

    for (VgsIter vgiter = m_Vgs.begin(); vgiter != m_Vgs.end(); vgiter++)
    {
        GetLvsInVg(vgiter->second);
    }

    return bgotlvminfos;
}


bool VolumeInfoCollector::GetLvsInVg(Vg_t &vg)
{
    bool bcollect = true;
    std::string bootDisk = m_BootDiskInfo.GetBootDisk();

    LvsIter begin = vg.m_Lvs.begin();
    LvsIter end = vg.m_Lvs.end();

    do
    {
        LvsIter iter = find_if(begin, end, ShouldReportLv);
        if (end != iter)
        {
            Lv_t &lv = iter->second;

            std::string realName;
            VolumeInfoCollectorInfo::SymbolicLinks_t symbolicLinks;
            GetSymbolicLinksAndRealName(lv.m_Name, symbolicLinks, realName);

            VolumeInfoCollectorInfo lvVolInfo;
            lvVolInfo.m_DeviceName = lv.m_Name;
            lvVolInfo.m_RealName = realName;
            lvVolInfo.m_DeviceType = VolumeSummary::LOGICAL_VOLUME;
            lvVolInfo.m_DeviceVendor = lv.m_Vendor;
            lvVolInfo.m_DumpDevice = m_DumpDevice.IsDumpDevice(lvVolInfo.m_DeviceName);
            lvVolInfo.m_RawSize = lv.m_Size;
            lvVolInfo.m_LunCapacity = lvVolInfo.m_RawSize + m_LunCapacityInformer.GetLengthForLunMakeup();
            lvVolInfo.m_Locked = IsDeviceLocked(lvVolInfo.m_DeviceName);
            lvVolInfo.m_SymbolicLinks = symbolicLinks;
#ifdef SV_AIX
            lvVolInfo.m_Major = major64(lv.m_Devt);
            lvVolInfo.m_Minor = minor64(lv.m_Devt);
#else
            lvVolInfo.m_Major = major(lv.m_Devt);
            lvVolInfo.m_Minor = minor(lv.m_Devt);
#endif

            lvVolInfo.m_VolumeGroup = vg.m_Name;
            lvVolInfo.m_VolumeGroupVendor = vg.m_Vendor;
            lvVolInfo.m_WCState = lv.m_WCState;
            if (!bootDisk.empty())
            {
                lvVolInfo.m_BootDisk = (std::string::npos != realName.find(bootDisk));
            }
            UpdateMountDetails(lvVolInfo);
            UpdateLvToDisk(m_VolumeInfos, lvVolInfo);
            lvVolInfo.m_SwapVolume = lv.m_SwapVolume;
            UpdateSwapDetails(lvVolInfo);
            lv.m_Collected = true;
            InsertVolumeInfo(lvVolInfo);

            /* ++ to give next interator as new begin */
            iter++;
        }
        begin = iter;
    } while (begin != end);

    return bcollect;
}


bool VolumeInfoCollector::GetVxVMVgs(void)
{
    bool bgotvxvmvgs = true;

    /* use "vxdisk list" to do this. fills vgs */
    GetVgsFromVxdisk();

    return bgotvxvmvgs;
}


void VolumeInfoCollector::GetVgsFromVxdisk(void)
{
    /*
    DEVICE       TYPE            DISK         GROUP        STATUS
    disk_0       auto:cdsdisk    disk_0       oradatadg    online shared
    disk_1       auto:cdsdisk    -            -            online
    disk_2       auto:cdsdisk    disk_8       ocrvotedg    online shared
    disk_3       auto:cdsdisk    disk_2       oradatadg    online shared
    disk_4       auto:cdsdisk    oradatadg01  oradatadg    online shared
    */

    std::string cmd(m_localConfigurator.getVxDiskCmd());
    const std::string VXDISKARGS = "list";
    cmd += " ";
    cmd += VXDISKARGS;
    
    std::stringstream results;

    std::set<dev_t> vxvmvsets;
    GetVxVMVSets(vxvmvsets);

    if (!executePipe(cmd, results)) {
        DebugPrintf(SV_LOG_ERROR,"Unable to run vxdisk list. executing it failed with errno = %d\n", errno);
        return;
    }

    if (results.str().empty()) {
        DebugPrintf(SV_LOG_DEBUG,"vxdisk list has returned empty output\n");
        return;
    }

    std::string line;
    std::getline(results, line);
    if (!line.empty())
    {
        std::stringstream ssline(line);
        const std::string STATUS = "STATUS";
        const std::string GROUP = "GROUP";
        int statuspos = -1, grouppos = -1;

        for (int i = 0; !ssline.eof(); i++)
        {
            std::string token;
            ssline >> token;
            if (STATUS == token)
            {
                statuspos = i;
            }
            else if (GROUP == token)
            {
                grouppos = i;
            }
        }

        while (!results.eof())
        {
            std::string opline;
            std::getline(results, opline);
            if (opline.empty())
            {
                break;
            }
            GetVgInfoForDevice(vxvmvsets, opline, m_Vgs, grouppos, statuspos);
        }
    }
}


void VolumeInfoCollector::GetVgInfoForDevice(
    const std::set<dev_t> &vxvmvsets,
    const std::string &line, 
    Vgs_t &Vgs,
    const int &grouppos,
    const int &statuspos
    )
{
    std::stringstream ssline(line);
    const std::string NOVXVGTOK = "-";

    std::string vxdev;
    ssline >> vxdev;
    if (vxdev.empty())
    {
        return;
    }
    /* start from 1 since first field already vxdev */
    for(int i = 1; i < grouppos; i++)
    {
        std::string skip;
        ssline >> skip;
    }
    std::string vgname;
    ssline >> vgname;
    if (NOVXVGTOK == vgname)
    {
        vgname = VXVMDEFAULTVG;
    }
    std::string status;
    std::getline(ssline, status);

    VolumeSummary::Vendor vendor = VolumeSummary::VXVM;
    if (IsDriveUnderVxVM(status))
    {
        VgsIter vgiter = find_if(Vgs.begin(), Vgs.end(), EqVgNameAndVendor(vgname, vendor));
        Vg_t vg;
        Lvs_t &lvs = vg.m_Lvs;
        std::set<dev_t> *pinsidedevts = &vg.m_InsideDevts;
        bool bneedtoinsert = false;

        if (Vgs.end() == vgiter)
        {
            vg.m_Name = vgname;
            vg.m_Vendor = vendor;
            bneedtoinsert = true;
        }
        else 
        {
            pinsidedevts = &(*vgiter).second.m_InsideDevts;
        }

        std::string vxdevbasename = BaseName(vxdev);
        std::string disk = VXDMP_PATH;
        disk += UNIX_PATH_SEPARATOR;
        disk += vxdevbasename;
        bool bgotdevts = GetDiskDevts(disk, *pinsidedevts);
        if (!bgotdevts)
        {
            /* For older vxvms, not having enclosure based naming ? TODO: test */
            disk = GetNativeDiskDir();
            disk += UNIX_PATH_SEPARATOR;
            disk += vxdevbasename;
            bgotdevts = GetDiskDevts(disk, *pinsidedevts);
        }

        /* keep it commented for print later
        if (Vgs.end() != vgiter)
        {
        DebugPrintf(SV_LOG_DEBUG, "Devts for vg %s: start ===== \n", vgname.c_str());
        for (std::set<dev_t>::iterator iter = (*vgiter).second.m_InsideDevts.begin(); iter != (*vgiter).second.m_InsideDevts.end(); iter++)
        {
        std::stringstream strdevt;
        strdevt << (*iter);
        DebugPrintf(SV_LOG_DEBUG, "%s\n", strdevt.str().c_str());
        }
        DebugPrintf(SV_LOG_DEBUG, "Devts for vg %s: end ===== \n", vgname.c_str());
        }
        */

        if ((VXVMDEFAULTVG != vgname) && bneedtoinsert)
        {
            std::string diskgroupdir = VXVMDSKDIR;
            diskgroupdir += UNIX_PATH_SEPARATOR;
            diskgroupdir += vgname;
            GetVxVMVolumesInThisDiskGroup(vgname, diskgroupdir, vxvmvsets, vendor, lvs);
        }
        if (bneedtoinsert) 
        {
            InsertVg(vg);
        }
    }
}


bool VolumeInfoCollector::GetVxVMVolumesInThisDiskGroup(
    const std::string &vgname,
    const std::string &diskgroupdir, 
    const std::set<dev_t> &vxvmvsets, 
    const VolumeSummary::Vendor vendor,
    Lvs_t &lvs
    )
{
    DIR *dirp;
    struct dirent *dentp, *dentresult;
    bool bretval = true;
    size_t direntsize = sizeof(struct dirent) + PATH_MAX + 1;



    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    dentresult = NULL;

    dirp = opendir(diskgroupdir.c_str());

    if (dirp)
    {
        dentp = (struct dirent *)calloc(direntsize, 1);

        if (dentp)
        {
            while ((0 == readdir_r(dirp, dentp, &dentresult)) && dentresult)
            {
                if (strcmp(dentp->d_name, ".") &&
                    strcmp(dentp->d_name, ".."))                /* skip . and .. */
                {
                    std::string vxvmdskvolume = diskgroupdir + "/" + dentp->d_name;
                    struct stat64 vxStat;
                    if ((0 == stat64(vxvmdskvolume.c_str(), &vxStat)) && vxStat.st_rdev)
                    {
                        if (vxvmvsets.end() == vxvmvsets.find(vxStat.st_rdev))
                        {
                            std::string vxvmrdskvolume = GetRawDeviceName(vxvmdskvolume);
                            int fd = -1;

                            fd = open(vxvmrdskvolume.c_str(), O_NONBLOCK | O_RDONLY);
                            if (-1 != fd) 
                            {
                                long long size = GetVolumeSizeInBytes(fd, vxvmdskvolume);
                                if (size)
                                {
                                    Lv_t lv;
                                    lv.m_Name = vxvmdskvolume;
                                    lv.m_Size = size;
                                    lv.m_VgName = vgname;
                                    lv.m_Devt = vxStat.st_rdev;
                                    lv.m_Vendor = vendor;
                                    InsertLv(lvs, lv);
                                }
                                close(fd);
                            } /* end of if (-1 != fd) */
                            else
                            {
                                DebugPrintf(SV_LOG_WARNING, "@LINE %d in FILE %s open on %s failed\n", __LINE__, __FILE__, vxvmrdskvolume.c_str());
                            }
                        } /* end of if (vxvmvsets.end() == vxvmvsets.find(vxStat.st_rdev)) */
                    } /* end of stat */
                } /* end of skipping . and .. */
                memset(dentp, 0, direntsize);
            } /* end of while readdir_r */
            free(dentp);
        }
        else
        {
            bretval = false;
        }
        closedir(dirp); 
    } /* end of if (dirp) */

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return bretval;
}


bool VolumeInfoCollector::IsVolInfoPresent(const std::string &devname)
{
    ConstVolInfoIter iter = find_if(m_VolumeInfos.begin(), m_VolumeInfos.end(), EqDeviceName(devname));
    bool beq = (iter != m_VolumeInfos.end());
    return beq;
}


void VolumeInfoCollector::InsertVolumeInfos(VolumeInfoCollectorInfos_t &volinfos, const VolumeInfoCollectorInfos_t &newvolinfos)
{
    volinfos.insert(newvolinfos.begin(), newvolinfos.end());
}


bool VolumeInfoCollector::IsDeviceValidLv(const std::string &device)
{
    /* add code only to filter dev mapper volumes as
     * rhel 6 lists these as disks;
     * TODO: add full code later */
    bool bislv = false;
    ConstLvsIter lit = m_Lvs.find(device);

    if (lit != m_Lvs.end())
    {
        const Lv_t &lv = lit->second;
        bislv = (VolumeSummary::DEVMAPPER == lv.m_Vendor);
    }

    return bislv;
}


/* inquiry using scsi else ata */
/* if scsi, then update hctl */
Attributes_t VolumeInfoCollector::GetAttributes(const std::string &devicename)
{
    Attributes_t attrs;

    std::string vendor, model;
    std::string h, c, t, l;
    /* This is because ide may also have these ? 
     * atleast on solaris, this is the case */
    GetScsiHCTL(devicename, h, c, t, l);

    if (!h.empty())
    {
        attrs.insert(std::make_pair(NsVolumeAttributes::SCSI_BUS, h));
    }

    if (!c.empty())
    {
        attrs.insert(std::make_pair(NsVolumeAttributes::SCSI_PORT, c));
    }

    if (!t.empty())
    {
        attrs.insert(std::make_pair(NsVolumeAttributes::SCSI_TARGET_ID, t));
    }

    if (!l.empty())
    {
        attrs.insert(std::make_pair(NsVolumeAttributes::SCSI_LOGICAL_UNIT, l));
    }

    INQUIRY_DETAILS InqDetails;
    InqDetails.device = devicename;
    ScsiCommandIssuer sci;
    bool bgotstdinq = sci.StartSession(devicename);
    if (bgotstdinq)
    {
        bgotstdinq = sci.GetStdInqValues(&InqDetails);
        if (bgotstdinq)
        {
            vendor = InqDetails.m_Vendor;
            model = InqDetails.m_Product;
    
            /* TODO: no intervention for now */
            /* Trim(vendor, " \t"); */
            /* Trim(model, " \t"); */
    
            attrs.insert(std::make_pair(NsVolumeAttributes::INTERFACE_TYPE, "scsi"));
        }
        sci.EndSession();
    }

    if (!bgotstdinq)
    {
        if (GetModelVendorFromAtaCmd(devicename, vendor, model))
        {
            attrs.insert(std::make_pair(NsVolumeAttributes::INTERFACE_TYPE, "ata"));
        }
    }

    if (!vendor.empty())
        attrs.insert(std::make_pair(NsVolumeAttributes::MANUFACTURER, vendor));
    if (!model.empty())
        attrs.insert(std::make_pair(NsVolumeAttributes::MODEL, model));

    attrs.insert(std::make_pair(NsVolumeAttributes::DEVICE_NAME, devicename));

    // Get the vendor assigned serial number
    std::string sno = m_DeviceIDInformer.GetVendorAssignedSerialNumber(devicename);
    if (sno.empty())
        DebugPrintf(SV_LOG_WARNING, "could not get vendor assigned serial number for %s with error %s\n", devicename.c_str(), m_DeviceIDInformer.GetErrorMessage().c_str());
    else
        attrs.insert(std::make_pair(NsVolumeAttributes::SERIAL_NUMBER, sno));

     if (m_TempDevice.IsTempDevice(devicename) && (IsAzureVirtualMachine() || IsAzureStackVirtualMachine()))
        attrs.insert(std::make_pair(NsVolumeAttributes::IS_RESOURCE_DISK, STRBOOL(true)));

    return attrs;
}


void VolumeInfoCollector::GetScsiHCTL(const std::string &devicename, std::string &h, std::string &c, std::string &t, std::string &l)
{
    UpdateHCTLFromDevice(devicename);

    ConstDeviceToHCTLIter_t cit = m_DeviceToHCTL.find(devicename);

    if (cit != m_DeviceToHCTL.end())
    {
        const std::string &hctl =  cit->second;

        /* format is "h:0;c:1;t:ABC;l:10" */
        bool parsed = parse(
            hctl.begin(),
            hctl.end(),
            lexeme_d
            [
            HOSTCHAR >> ch_p(HCTLLABELVALUESEP) >> (+(~ch_p(HCTLSEP))) [phoenix::var(h) = phoenix::construct_<std::string>(phoenix::arg1,phoenix::arg2)]
            >> HCTLSEP >>
            !(CHANNELCHAR >> ch_p(HCTLLABELVALUESEP) >> (+(~ch_p(HCTLSEP))) [phoenix::var(c) = phoenix::construct_<std::string>(phoenix::arg1,phoenix::arg2)]
              >> HCTLSEP) >>
            !(TARGETCHAR >> ch_p(HCTLLABELVALUESEP) >> (+(~ch_p(HCTLSEP))) [phoenix::var(t) = phoenix::construct_<std::string>(phoenix::arg1,phoenix::arg2)]
              >> HCTLSEP) >>
            LUNCHAR >> ch_p(HCTLLABELVALUESEP) >> (+(~ch_p(HCTLSEP))) [phoenix::var(l) = phoenix::construct_<std::string>(phoenix::arg1,phoenix::arg2)]
            ] >> end_p, 
            space_p).full;
    }
}


void VolumeInfoCollector::GetVsnapInfos(void)
{
	UnixVsnapMgr mgr;
	std::vector<VsnapPersistInfo> vsnaps;
	std::vector<VsnapPersistInfo> ::const_iterator vsnapiter ;

    if (!mgr.RetrieveVsnapInfo(vsnaps, "all"))
    {
        DebugPrintf(SV_LOG_ERROR, "Unable to retrieve vsnap information in volumeinfocollector\n");
        return;
    }

    for (vsnapiter = vsnaps.begin(); vsnapiter != vsnaps.end(); vsnapiter++)
    {
        struct stat64 volStat;
        if (0 == stat64(vsnapiter->devicename.c_str(), &volStat))
        {
            DebugPrintf(SV_LOG_DEBUG, "%s is a vsnap\n", vsnapiter->devicename.c_str());
            m_VsnapDevts.insert(volStat.st_rdev);
        }
        else
        {
            DebugPrintf(SV_LOG_WARNING, "Failed to stat on %s with error %d\n", vsnapiter->devicename.c_str(), errno);
        }
    }
}


void VolumeInfoCollector::InsertDeviceHCTLPair(DeviceHCTLPair_t devicehctlpair)
{
    m_DeviceToHCTL.insert(devicehctlpair);
}

// on Azure VM the following logic tries fo find the temp disk
// If it is a Gen 1 VM, the temp disk is detected by using the IDE controller logic. See IsTempDisk()
// if it is a Gen 2 VM, any disk on the same controller as the boot disk is considered temp disk
void VolumeInfoCollector::MarkDisksOnBootDiskController()
{
    if (!IsAzureVirtualMachine() && !IsAzureStackVirtualMachine())
        return;

    int32_t bootdiskscsiport = -1;
    int32_t bootdiskscsibus = -1;
    int32_t bootdiskscsitarget = -1;
    int32_t bootdiskscsilun = -1;
    std::string bootdiskname;
    uint32_t numbootdisks = 0;

    VolumeInfoCollectorInfos_t::iterator diter(m_VolumeInfos.begin());

    for ( /* empty */ ; diter != m_VolumeInfos.end(); ++diter)
    {
        VolumeInfoCollectorInfo &dvolinfo = diter->second;
        if ( (VolumeSummary::NATIVE == dvolinfo.m_DeviceVendor) &&
             (VolumeSummary::DISK == dvolinfo.m_DeviceType) &&
             (dvolinfo.m_BootDisk) )
        {
            std::string strNvmeDisk = dvolinfo.GetAttribute(dvolinfo.m_Attributes, NsVolumeAttributes::IS_NVME_DISK, false);
	    if(0 == strNvmeDisk.compare("true"))
		continue;

            int32_t scsibus = strtol(dvolinfo.GetAttribute(dvolinfo.m_Attributes, NsVolumeAttributes::SCSI_BUS, true).c_str(), NULL, 10);
            int32_t scsiport = strtol(dvolinfo.GetAttribute(dvolinfo.m_Attributes, NsVolumeAttributes::SCSI_PORT, true).c_str(), NULL, 10);
            int32_t scsitarget = strtol(dvolinfo.GetAttribute(dvolinfo.m_Attributes, NsVolumeAttributes::SCSI_TARGET_ID, true).c_str(), NULL, 10);
            int32_t scsilun = strtol(dvolinfo.GetAttribute(dvolinfo.m_Attributes, NsVolumeAttributes::SCSI_LOGICAL_UNIT, true).c_str(), NULL, 10);
            numbootdisks++;

            if (bootdiskname.empty())
            {
                bootdiskname = dvolinfo.m_DeviceName;
                bootdiskscsiport = scsiport;
                bootdiskscsibus = scsibus;
                bootdiskscsitarget = scsitarget;
                bootdiskscsilun = scsilun;

                DebugPrintf(SV_LOG_DEBUG, "Found h:c:t:l %d:%d:%d:%d for boot disk %s\n",
                            bootdiskscsibus, bootdiskscsiport, bootdiskscsitarget, bootdiskscsilun, bootdiskname.c_str());
            }
            else
            {
                if (bootdiskscsiport == scsiport &&
                    bootdiskscsibus == scsibus &&
                    bootdiskscsitarget == scsitarget &&
                    bootdiskscsilun == scsilun)
                {
                    DebugPrintf(SV_LOG_WARNING, "Found same h:c:t:l %d:%d:%d:%d for boot disk %s and %s\n",
                                bootdiskscsibus, bootdiskscsiport, bootdiskscsitarget, bootdiskscsilun,
                                bootdiskname.c_str(), dvolinfo.m_DeviceName.c_str());
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "Found different boot disks at h:c:t:l %d:%d:%d:%d for boot disk %s and at h:c:t:l %d:%d:%d:%d for %s\n",
                                bootdiskscsibus, bootdiskscsiport, bootdiskscsitarget, bootdiskscsilun,
                                bootdiskname.c_str(),
                                scsibus, scsiport, scsitarget, scsilun,
                                dvolinfo.m_DeviceName.c_str());

                    return;
                }

            }
        }
    }

    if (!numbootdisks)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Found no boot disks\n", FUNCTION_NAME);
        return;
    }

    if (numbootdisks > 1)
        DebugPrintf(SV_LOG_WARNING, "Found %d boot disks\n", numbootdisks);

    uint32_t numtempdisks = 0;
    diter = m_VolumeInfos.begin();
    for ( /* empty */ ; diter != m_VolumeInfos.end(); ++diter)
    {
        VolumeInfoCollectorInfo &dvolinfo = diter->second;
        if ( (VolumeSummary::NATIVE == dvolinfo.m_DeviceVendor) &&
             (VolumeSummary::DISK == dvolinfo.m_DeviceType) &&
             (!dvolinfo.m_BootDisk) )
        {
            std::string strNvmeDisk = dvolinfo.GetAttribute(dvolinfo.m_Attributes, NsVolumeAttributes::IS_NVME_DISK, false);
	    if(0 == strNvmeDisk.compare("true"))
		continue;

            int32_t scsibus = strtol(dvolinfo.GetAttribute(dvolinfo.m_Attributes, NsVolumeAttributes::SCSI_BUS, true).c_str(), NULL, 10);
            int32_t scsiport = strtol(dvolinfo.GetAttribute(dvolinfo.m_Attributes, NsVolumeAttributes::SCSI_PORT, true).c_str(), NULL, 10);
            int32_t scsitarget = strtol(dvolinfo.GetAttribute(dvolinfo.m_Attributes, NsVolumeAttributes::SCSI_TARGET_ID, true).c_str(), NULL, 10);
            int32_t scsilun = strtol(dvolinfo.GetAttribute(dvolinfo.m_Attributes, NsVolumeAttributes::SCSI_LOGICAL_UNIT, true).c_str(), NULL, 10);

            if (bootdiskscsiport == scsiport &&
                bootdiskscsibus == scsibus &&
                bootdiskscsitarget == scsitarget &&
                bootdiskscsilun != scsilun)
            {
                DebugPrintf(SV_LOG_DEBUG, "Marking resource disk. found same h:c:t %d:%d:%d for boot disk %s and %s at lun %d\n",
                            scsibus, scsiport, scsitarget,
                            bootdiskname.c_str(), dvolinfo.m_DeviceName.c_str(), scsilun);
                dvolinfo.m_Attributes.insert(std::make_pair(NsVolumeAttributes::IS_RESOURCE_DISK, STRBOOL(true) ));
                numtempdisks++;
            }
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "Found %d disks on Boot Disk controller\n", numtempdisks);

    return;
}
