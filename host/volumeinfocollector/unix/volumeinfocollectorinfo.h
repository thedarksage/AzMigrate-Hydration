//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : volumeinfocollectorinfo.h
// 
// Description: 
// 

#ifndef VOLUMEINFOCOLLECTORINFO_H
#define VOLUMEINFOCOLLECTORINFO_H

#include <sstream>
#include <string>
#include <set>
#include <map>

#include "volumegroupsettings.h"
#include "dumpmsg.h"
#include "volumemanagerfunctionsminor.h"
#include "diskpartition.h"
#include "configwrapper.h"


struct VolumeInfoCollectorInfo;
typedef std::map<std::string, VolumeInfoCollectorInfo> VolumeInfoCollectorInfos_t;

typedef enum deviceNameTypes {
    DEVICE_NAME_TYPE_OS_NAME = 1,
    DEVICE_NAME_TYPE_LU_NUMBER = 2,
    DEVICE_NAME_TYPE_FROM_PERSISTENT_MAP = 3
} DeviceNameTypes_t;


struct VolumeInfoCollectorInfo
{
    typedef std::set<std::string> SymbolicLinks_t;
    
    VolumeInfoCollectorInfo()
        {
            m_ShouldCollect = true;
            m_ShouldCollectParent = true;
            m_DeviceType = VolumeSummary::UNKNOWN_DEVICETYPE;
            m_DeviceVendor = VolumeSummary::UNKNOWN_VENDOR;
            m_Mounted = false;
            m_SystemVolume = false;
            m_SwapVolume = false;
            m_DumpDevice = false;
            m_BootDisk = false;
            m_EncryptedDisk = false;
            m_BekVolume = false;
            m_CacheVolume = false;
            m_RawSize = 0;
            m_LunCapacity = 0;
            m_FreeSpace= 0;
            m_PhysicalOffset = 0;
            m_Locked = false;
            m_PageVolume = false;
            m_Available = true;
            m_IsStartingAtBlockZero = false;
            m_IsMultipath = false;
            m_VolumeGroupVendor = VolumeSummary::UNKNOWN_VENDOR;
            m_Major = 0;
            m_Minor = 0;
            m_WCState = VolumeSummary::WRITE_CACHE_DONTKNOW;
            m_FormatLabel = VolumeSummary::LABEL_UNKNOWN;
            m_SectorSize = 0;
            m_HasLvmMountedOnRoot = false;
        }

    std::string DeviceNameToReport(DeviceNameTypes_t deviceNameType = DEVICE_NAME_TYPE_OS_NAME);
    
    bool operator<(VolumeInfoCollectorInfo const & rhs) const
        {
            return (m_DeviceName < rhs.m_DeviceName);
        }

    void UpdateReportStatus(const bool &registerSystemDevices, const bool &reportFullDevicesOnly)
        {
            if (m_ShouldCollect)
            {
                m_ShouldCollect = m_Available;
            }

            /* TODO: get this value once only */
            if (reportFullDevicesOnly)
            {
                m_ShouldCollect = (m_Available && (VolumeSummary::DISK == m_DeviceType));
            }
            else if (m_ShouldCollect)
            {
                if (SystemVolume())
                {
                    m_ShouldCollect = registerSystemDevices;
                }
            }
        }
        

    std::string GetAttribute(Attributes_t const& attrs,
            const char *key,
            bool throwIfNotFound)
        {
            ConstAttributesIter_t iter = attrs.find(key);
            if (iter != attrs.end())
            {
                return iter->second;
            }

            if (throwIfNotFound)
            {
                std::stringstream err;
                err << "Error: no attribute found for key "
                << key
                << ".";

                throw std::runtime_error(err.str().c_str());
            }

            return std::string("");
        }

    void Dump(void) const
        {
            std::stringstream msg;
            msg << "----------------------------------------------------------\n"
                << "Device Should be Reported        : " << m_ShouldCollect << '\n'
                << "ID                               : " << m_DeviceID << '\n'
                << "Name                             : " << m_DeviceName << '\n'
                << "Real Name                        : " << m_RealName << '\n'
                << "Type                             : " << StrDeviceType[m_DeviceType] << '\n'
                << "Vendor                           : " << StrVendor[m_DeviceVendor] << '\n'
                << "File System                      : " << m_FileSystem << '\n'
                << "Mount Point                      : " << m_MountPoint << '\n'
                << "Mounted                          : " << m_Mounted << '\n'
                << "System Volume                    : " << m_SystemVolume << '\n'
                << "Swap Volume                      : " << m_SwapVolume << '\n'
                << "Dump Device                      : " << m_DumpDevice << '\n'        
                << "Boot Disk                        : " << m_BootDisk << '\n'
                << "Encrypted Disk                   : " << m_EncryptedDisk << '\n'
                << "BEK Volume                       : " << m_BekVolume << '\n'
                << "Cache Volume                     : " << m_CacheVolume << '\n'
                << "Parent Should be Reported        : " << m_ShouldCollectParent << '\n'
                << "Raw Size                         : " << m_RawSize << '\n'
                << "Lun Capacity                     : " << m_LunCapacity << '\n'
                << "Free Space                       : " << m_FreeSpace << '\n'
                << "Physical Offset                  : " << m_PhysicalOffset << '\n'
                << "Locked                           : " << m_Locked << '\n'
                << "Volume Label                     : " << m_VolumeLabel << '\n'
                << "Contains Page File               : " << m_PageVolume << '\n'
                << "Available                        : " << m_Available << '\n'
                << "Starting At Block Zero           : " << m_IsStartingAtBlockZero << '\n'
                << "Multipath                        : " << m_IsMultipath << '\n'
                << "Volume Group                     : " << m_VolumeGroup << '\n'
                << "Volume Group Vendor              : " << StrVendor[m_VolumeGroupVendor] << '\n'
                << "Under Extended Partition         : " << m_ExtendedPartitionName << '\n'
                << "Major Device Number              : " << m_Major << '\n'
                << "Minor Device Number              : " << m_Minor << '\n'
                << "Write Cache State                : " << StrWCState[m_WCState] << '\n'
                << "Format Label                     : " << StrFormatLabel[m_FormatLabel] << '\n'
                << "Sector Size                      : " << m_SectorSize << '\n'
                << "attributes                       : " << m_Attributes << '\n';

            msg << "Symbolic Link(s)                 : " ;
            if (!m_SymbolicLinks.empty()) {
                SymbolicLinks_t::const_iterator iter(m_SymbolicLinks.begin());
                SymbolicLinks_t::const_iterator endIter(m_SymbolicLinks.end());
                do {
                    msg << (*iter) << '\n';
                    ++iter;
                    if (iter != endIter) {
                        msg << "                                   ";
                    }
                } while (iter != endIter);
            }
            msg << "\n\n";

            DumpMsg(msg.str());
  
            VolumeInfoCollectorInfos_t::const_iterator iter = m_Children.begin();
            for ( /* empty */ ; iter != m_Children.end(); iter++)
            {
                iter->second.Dump();
            }
        }

    bool SystemVolume() const
        {
            /* TODO: every disk in linux seems to be having boot partition;
             * also previously boot disk was not being reported as system volume. */
            return (m_SystemVolume | m_SwapVolume | m_DumpDevice | m_BootDisk);
        }
    
    bool m_ShouldCollect;                      /* Always true except in case if cannot decide on the vg; if disk cant be collected, its slices too */
    bool m_ShouldCollectParent;                /* Always true except in case if because of child, parent should not be collected */
    std::string m_DeviceID;                    /* scsi id for disk if found. if not found empty; FIXME: what about IDE/SATA/PATA and others ? */
    std::string m_DeviceName;                  /* the inital name discovered (could be a symbolic link) */
    std::string m_RealName;                    /* the real name in case of symbolic links (could be same as m_DeviceName) */
    VolumeSummary::Devicetype m_DeviceType;    /* should device type be declared here and given to VolumeSummary */
    VolumeSummary::Vendor m_DeviceVendor;      /* same as above */
    std::string m_FileSystem;   
    std::string m_MountPoint;                  /* Although set if mountpoints are there, this reaches to CX since this will be actual mounted */
    bool m_Mounted;             
    bool m_SystemVolume;                       /* "/", "/usr" etc,. should go to CX as system volume */
    bool m_SwapVolume;                         /* "swap" file system or swap -l. should go to CX as system volume */
    bool m_DumpDevice;                         /* dumpadm .should go to CX as system volume */
    bool m_BootDisk;                           /* prtconf -pv .should go to CX as system volume */
    bool m_EncryptedDisk;                      /* Is the disk encrypted one? */
    bool m_BekVolume;                          /* Is the disk stores the encrypted keys? */
    bool m_CacheVolume;                        /* should go to CX as cache volume */
    bool m_HasLvmMountedOnRoot;
    /* TODO: config/volumegroupsettings.h has size and free space as signed values. Need to make common */
    unsigned long long m_RawSize;
    unsigned long long m_LunCapacity;
    unsigned long long m_FreeSpace;
    unsigned long long m_PhysicalOffset;
    bool m_Locked;                             /* should go to CX as locked */
                                               /* physicalOffset and  sectorSize are assigned 0 directly. what is purpose? */
    std::string m_VolumeLabel;                 /* what for? */
	bool m_PageVolume;                         /* should go to CX as page volume */
    bool m_Available;                          /* If available (from lvstatus only), send to CX */
    bool m_IsStartingAtBlockZero;              /* Although previously added for partitions, disks should not make this true */
    bool m_IsMultipath;                        /* true if this is multipath (source: either from specific commands (or) dirs */
    std::string m_VolumeGroup;                  
    VolumeSummary::Vendor m_VolumeGroupVendor;
    std::string m_ExtendedPartitionName;       /* used in linux */
    SymbolicLinks_t m_SymbolicLinks;
    int m_Major;
    int m_Minor;
    VolumeSummary::WriteCacheState m_WCState;
    VolumeSummary::FormatLabel m_FormatLabel;
    int m_SectorSize;
	Attributes_t m_Attributes;                   /* attributes specific to device */
    VolumeInfoCollectorInfos_t m_Children;
};

typedef std::pair<const std::string, VolumeInfoCollectorInfo> VolumeInfoCollectorInfos_pair_t;
typedef VolumeInfoCollectorInfos_t::const_iterator ConstVolInfoIter;
typedef VolumeInfoCollectorInfos_t::iterator VolInfoIter;

class EqDeviceName: public std::unary_function<VolumeInfoCollectorInfos_pair_t, bool>
{
    std::string m_DevName;
public:
    explicit EqDeviceName(const std::string &devname) : m_DevName(devname) { }
    bool operator()(const VolumeInfoCollectorInfos_pair_t &in) const 
    {
        bool beq = (in.second.m_DeviceName == m_DevName);
        if (!beq)
        {
            const VolumeInfoCollectorInfos_t &children = in.second.m_Children;
            ConstVolInfoIter iter = find_if(children.begin(), children.end(), EqDeviceName(m_DevName));
            beq = (children.end() != iter);
        }
        return beq;
    }
};


class EqDeviceType: public std::unary_function<VolumeInfoCollectorInfos_pair_t, bool>
{
    VolumeSummary::Devicetype m_DeviceType;
public:
    explicit EqDeviceType(const VolumeSummary::Devicetype devicetype) : m_DeviceType(devicetype) { }
    bool operator()(const VolumeInfoCollectorInfos_pair_t &in) const 
    {
        return (in.second.m_DeviceType == m_DeviceType);
    }
};


class CopyAttrsFromParentToChild: public std::unary_function<VolumeInfoCollectorInfos_pair_t, void>
{
    VolumeInfoCollectorInfo *m_pParentVolInfo;
public:
    explicit CopyAttrsFromParentToChild(VolumeInfoCollectorInfo *pvolinfo) : m_pParentVolInfo(pvolinfo) { }
    void operator()(VolumeInfoCollectorInfos_pair_t &in) const 
    {
        VolumeInfoCollectorInfo &childVolInfo = in.second;
        VolumeInfoCollectorInfo &parentVolInfo = (*m_pParentVolInfo);

        if (parentVolInfo.m_SystemVolume)
        {
            if (!childVolInfo.m_SystemVolume)
            {
                childVolInfo.m_SystemVolume = parentVolInfo.m_SystemVolume;
            }
        }

        if (parentVolInfo.m_BootDisk)
        {
            if (!childVolInfo.m_BootDisk)
            {
                childVolInfo.m_BootDisk = parentVolInfo.m_BootDisk;
            }
        }

        if (parentVolInfo.m_SwapVolume)
        {
            if (!childVolInfo.m_SwapVolume)
            {
                childVolInfo.m_SwapVolume = parentVolInfo.m_SwapVolume;
            }
        }
       
        if (parentVolInfo.m_DumpDevice)
        {
            if (!childVolInfo.m_DumpDevice)
            {
                childVolInfo.m_DumpDevice = parentVolInfo.m_DumpDevice;
            }
        }

        if (parentVolInfo.m_CacheVolume)
        {
            if (!childVolInfo.m_CacheVolume)
            {
                childVolInfo.m_CacheVolume = parentVolInfo.m_CacheVolume;
            }
        }

        /* TODO: Need to check should we copy always ? */
        if (!parentVolInfo.m_VolumeGroup.empty())
        {
            childVolInfo.m_VolumeGroup = parentVolInfo.m_VolumeGroup;
            childVolInfo.m_VolumeGroupVendor = parentVolInfo.m_VolumeGroupVendor;
        }

        if (!parentVolInfo.m_ShouldCollect)
        {
            if (childVolInfo.m_ShouldCollect)
            {
                childVolInfo.m_ShouldCollect = parentVolInfo.m_ShouldCollect;
            }
        }

        if (!parentVolInfo.m_Available)
        {
            if (childVolInfo.m_Available)
            {
                childVolInfo.m_Available = parentVolInfo.m_Available;
            }
        }

        if (parentVolInfo.m_BekVolume)
        {
            if (!childVolInfo.m_BekVolume)
            {
                childVolInfo.m_BekVolume = parentVolInfo.m_BekVolume;
            }
        }
    }
};


class CopyAttrsFromChildToParent: public std::unary_function<VolumeInfoCollectorInfos_pair_t, void>
{
    VolumeInfoCollectorInfo *m_pParentVolInfo;
public:
    explicit CopyAttrsFromChildToParent(VolumeInfoCollectorInfo *pvolinfo) : m_pParentVolInfo(pvolinfo) { }
    void operator()(VolumeInfoCollectorInfos_pair_t &in) const 
    {
        VolumeInfoCollectorInfo &childVolInfo = in.second;
        VolumeInfoCollectorInfo &parentVolInfo = (*m_pParentVolInfo);

        if (!childVolInfo.m_ShouldCollectParent)
        {
            if (parentVolInfo.m_ShouldCollect)
            {
                parentVolInfo.m_ShouldCollect = childVolInfo.m_ShouldCollectParent;
                parentVolInfo.m_ShouldCollectParent = childVolInfo.m_ShouldCollectParent;
            }
        }

        if (childVolInfo.m_SystemVolume)
        {
            if (!parentVolInfo.m_SystemVolume)
            {
                parentVolInfo.m_SystemVolume = childVolInfo.m_SystemVolume;
            }
        }

        if (childVolInfo.m_BootDisk)
        {
            if (!parentVolInfo.m_BootDisk)
            {
                parentVolInfo.m_BootDisk = childVolInfo.m_BootDisk;
                parentVolInfo.m_Attributes.insert(std::make_pair(NsVolumeAttributes::HAS_BOOTABLE_PARTITION, "true"));
            }
        }

        if (childVolInfo.m_SwapVolume)
        {
            if (!parentVolInfo.m_SwapVolume)
            {
                parentVolInfo.m_SwapVolume = childVolInfo.m_SwapVolume;
            }
        }
       
        if (childVolInfo.m_DumpDevice)
        {
            if (!parentVolInfo.m_DumpDevice)
            {
                parentVolInfo.m_DumpDevice = childVolInfo.m_DumpDevice;
            }
        }

        if (childVolInfo.m_CacheVolume)
        {
            if (!parentVolInfo.m_CacheVolume)
            {
                parentVolInfo.m_CacheVolume = childVolInfo.m_CacheVolume;
            }
        }

        /* For a disk, only copy the first partition's vg. 
         * partition2 of same disk having different vg than 
         * partition1 should be handled at CX, cdpcli */
        if (!childVolInfo.m_VolumeGroup.empty())
        {
            if (parentVolInfo.m_VolumeGroup.empty() || childVolInfo.m_HasLvmMountedOnRoot)
            {
                parentVolInfo.m_VolumeGroup = childVolInfo.m_VolumeGroup;
                parentVolInfo.m_VolumeGroupVendor = childVolInfo.m_VolumeGroupVendor;
            }
        }

        if (childVolInfo.m_EncryptedDisk)
        {
            if (!parentVolInfo.m_EncryptedDisk)
            {
                parentVolInfo.m_EncryptedDisk = childVolInfo.m_EncryptedDisk;
            }
        }

        if (childVolInfo.m_BekVolume)
        {
            if (!parentVolInfo.m_BekVolume)
            {
                parentVolInfo.m_BekVolume = childVolInfo.m_BekVolume;
            }

            parentVolInfo.m_Attributes.insert(std::make_pair(NsVolumeAttributes::IS_BEK_VOLUME, STRBOOL(true)));
        }
    }
};

#endif // ifndef VOLUMEINFOCOLLECTORINFO_H
