#ifndef _DISK__PARTITION__H_
#define _DISK__PARTITION__H_

#include <string>
#include <vector>
#include <sstream>
#include <iterator>
#include "volumegroupsettings.h"

typedef struct VolumeAttrs
{
    bool m_BootVolume;
    bool m_SwapVolume;
    bool m_SystemVolume;

    VolumeAttrs()
    {
        m_BootVolume = m_SwapVolume = m_SystemVolume = false;
    }

} VolumeAttrs_t;

/* contains very basic information like name, devicetype etc; 
   Used to separate getting disk/partition logic with the 
   filling volinfos */
typedef struct DiskAndPartition
{
    std::string m_name;
    unsigned long long m_size;
    unsigned long long m_physicaloffset;
    VolumeSummary::Devicetype m_devicetype;
    bool m_ispartitionatblockzero;
    VolumeSummary::FormatLabel m_formatlabel;

    DiskAndPartition(
                     const std::string &name, 
                     unsigned long long size, 
                     unsigned long long physicaloffset, 
                     VolumeSummary::Devicetype devicetype, 
                     bool isPartitionAtBlockZero,
                     VolumeSummary::FormatLabel formatlabel
                    )
    {
        m_name = name;    
        m_size = size;
        m_physicaloffset = physicaloffset;
        m_devicetype = devicetype;
        m_ispartitionatblockzero = isPartitionAtBlockZero;
        m_formatlabel = formatlabel;
    }

} DiskPartition;

typedef DiskPartition DiskPartition_t;
typedef std::vector<DiskPartition_t> DiskPartitions_t;
typedef DiskPartitions_t::const_iterator ConstDiskPartitionIter;
typedef DiskPartitions_t::iterator DiskPartitionIter;


class DPEqDeviceType: public std::unary_function<DiskPartition_t, bool>
{
    VolumeSummary::Devicetype m_DeviceType;
public:
    explicit DPEqDeviceType(const VolumeSummary::Devicetype devicetype) : m_DeviceType(devicetype) { }
    bool operator()(const DiskPartition_t &dp) const
    {
        return (dp.m_devicetype == m_DeviceType);
    }
};

#endif /* _DISK__PARTITION__H_ */
