//                                       
// File       : volumeinfocollectorinfo.cpp
// 
// Description: 
// 

#include <set>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/replace.hpp>

#include "volumeinfocollectorinfo.h"
#include "protecteddevicedetails.h"

const std::string strBootDisk("/dev/");
const std::string strRootDisk("RootDisk");
const std::string strTempDisk("TempDisk");
const std::string strDataDisk("DataDisk");

/// \brief returns the device name to be reported based on the device name type
///     For boot disk returns 'RootDisk' unless the device name is already present in the persistent map
///     For data disk and type DEVICE_NAME_TYPE_LU_NUMBER, the device name is SCSI Logical Unit number appended to 'DataDisk'
///     For data disk and type DEVICE_NAME_TYPE_FROM_PERSISTENT_MAP, the device name is 'DataDisk' + a number succeeding
///         the highest index alrady present in the persistent device map if the device name already doesn't exit
///         For the disk whose device name is already present in the persistent device map, return the existing from map
///     Update the persistent device map if not already present in the persistent device map file
std::string VolumeInfoCollectorInfo::DeviceNameToReport(DeviceNameTypes_t deviceNameType)
{
    if ( ((deviceNameType == DEVICE_NAME_TYPE_LU_NUMBER) ||
          (deviceNameType == DEVICE_NAME_TYPE_FROM_PERSISTENT_MAP)) &&
         (VolumeSummary::NATIVE == m_DeviceVendor) &&
         (VolumeSummary::DISK == m_DeviceType))
    {
        std::string deviceName;
        VxProtectedDeviceDetailPeristentStore devDetailsPersistentStore;

        if (!devDetailsPersistentStore.Init())
        {
            const char *errorMsg = "Failed to initialize persistent store";
            throw INMAGE_EX(errorMsg);
        }

        VxProtectedDeviceDetails_t devDetails;
        if (!devDetailsPersistentStore.GetCurrentDetails(devDetails))
        {
            const char *errorMsg = "Failed to get current details persistent store";
            throw INMAGE_EX(errorMsg);
        }

        if (m_BootDisk)
        {
            VxProtectedDeviceDetailIter_t iter = devDetails.begin();
            for (/*empty*/; iter != devDetails.end(); iter++)
            {
                if (boost::starts_with(iter->m_id, strBootDisk) ||
                    boost::starts_with(iter->m_id, strRootDisk))
                {
                    deviceName = iter->m_id;
                }
             }
                
            if (deviceName.empty())
            {
                deviceName = strRootDisk;
                if (deviceNameType == DEVICE_NAME_TYPE_FROM_PERSISTENT_MAP)
                {
                    VxProtectedDeviceDetail details;
                    details.m_id = deviceName;
                    details.m_deviceName = m_DeviceName;
                    if (!devDetailsPersistentStore.AddMap(details))
                    {
                        const char *errorMsg = "Failed to write boot disk map to persistent store";
                        throw INMAGE_EX(errorMsg);
                    }
                }
            }
        }
        else
        {
            std::string strIsTempDisk =  GetAttribute(m_Attributes, NsVolumeAttributes::IS_RESOURCE_DISK, false);
            std::string strIsNvmeDisk = GetAttribute(m_Attributes, NsVolumeAttributes::IS_NVME_DISK, false);
            std::string strIsBekVolume = GetAttribute(m_Attributes, NsVolumeAttributes::IS_BEK_VOLUME, false);
            std::string strIsIscsiDisk = GetAttribute(m_Attributes, NsVolumeAttributes::IS_ISCSI_DISK, false);

            if (0 == strIsTempDisk.compare("true"))
            {
                deviceName = strTempDisk;
            }
            else if ((0 == strIsNvmeDisk.compare("true")) ||
                     (0 == strIsBekVolume.compare("true")))
            {
                deviceName = m_DeviceName;
            }
            else if (deviceNameType == DEVICE_NAME_TYPE_LU_NUMBER)
            {
                if (0 == strIsIscsiDisk.compare("true"))
                {
                    deviceName = m_DeviceName;
                }
                else
                {
                    std::stringstream sspath;
                    sspath << strDataDisk
                        << GetAttribute(m_Attributes, NsVolumeAttributes::SCSI_LOGICAL_UNIT, true);

                    deviceName = sspath.str();
                    m_Attributes.insert(std::make_pair(NsVolumeAttributes::NAME_BASED_ON, NsVolumeAttributes::SCSIHCTL));
                }
            }
            else if (deviceNameType == DEVICE_NAME_TYPE_FROM_PERSISTENT_MAP)
            {
                std::set<unsigned int> prevIndices;
                VxProtectedDeviceDetailIter_t iter = devDetails.begin();
                for (/*empty*/; iter != devDetails.end(); iter++)
                {
                    if (boost::iequals(iter->m_deviceName, m_DeviceName)) 
                    {
                        deviceName = iter->m_id;
                        break;
                    }

                    if (!boost::starts_with(iter->m_id, strBootDisk) &&
                        !boost::starts_with(iter->m_id, strRootDisk))
                    {
                        std::string id = iter->m_id;
                        boost::replace_all(id, strDataDisk, "");
                        prevIndices.insert(boost::lexical_cast<unsigned int>(id));
                    }
                }

                if (deviceName.empty())
                {
                    if (prevIndices.empty())
                        deviceName = strDataDisk + boost::lexical_cast<std::string>(0);
                    else
                        deviceName = strDataDisk + boost::lexical_cast<std::string>(*prevIndices.rbegin()+1);

                    VxProtectedDeviceDetail details;
                    details.m_id = deviceName;
                    details.m_deviceName = m_DeviceName;
                    if (!devDetailsPersistentStore.AddMap(details))
                    {
                        const char *errorMsg = "Failed to write map to persistent store";
                        throw INMAGE_EX(errorMsg);
                    }
                }

                m_Attributes.insert(std::make_pair(NsVolumeAttributes::NAME_BASED_ON, NsVolumeAttributes::PERSISTENT_NAME_MAP));
            }
        }
        return deviceName;
    }
    else
    {
        return (VolumeSummary::DEVMAPPER==m_DeviceVendor) ? m_DeviceName : m_RealName;
    }
}
