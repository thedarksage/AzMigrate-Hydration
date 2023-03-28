/*
+-------------------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp. 2016
+-------------------------------------------------------------------------------------------------+
File  : Converter.cpp

Description :   Converter class implementation

+-------------------------------------------------------------------------------------------------+
*/
#include "Converter.h"
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>

#include "RcmClientUtils.h"

using namespace RcmClientLib;

namespace RcmClientLib
{

    const char *WriteCacheState[] = { "Unknown", "Disabled", "Enabled" };
    const char *PartitionScheme[] = { "Unknown", "Smi", "Efi", "Mbr", "Gpt", "Raw" };
    const char OS_TYPE_WINDOWS[] = "Windows";
    const char INTERFACE_IDE[] = "ide";
    const char INTERFACE_ATA[] = "ata";
    const char INTERFACE_SCSI[] = "scsi";
    const char INTERFACE_FILE_BACKED_VIRTUAL[] = "FILEBACKEDVIRTUAL";

#ifdef WIN32
    __declspec(thread) CONVERTER_STATUS Converter::m_errorCode;
#else
    __thread CONVERTER_STATUS Converter::m_errorCode;
#endif


    /// \brief checks if a system flag is set for a disk entity
    /// \returns
    /// \li true if flag is set
    /// \li flase if flag not set
    bool IsSystemDisk(const VolumeSummary* ptrVolSum)
    {
        bool bSystemDisk = false;

        if ((ptrVolSum->isSystemVolume) ||
            (ptrVolSum->isCacheVolume))
            bSystemDisk = true;

        if (!ptrVolSum->mountPoint.empty())
        {
            if ((0 == ptrVolSum->mountPoint.compare("/")) ||
                (0 == ptrVolSum->mountPoint.compare("/etc")) ||
                (0 == ptrVolSum->mountPoint.compare("/usr")) ||
                (0 == ptrVolSum->mountPoint.compare("/usr/local")) ||
                (0 == ptrVolSum->mountPoint.compare("/var")))
                bSystemDisk = true;
        }

        return bSystemDisk;
    }

    /// \brief checks if a boot flag is set for a disk entity
    /// \returns
    /// \li true if flag is set
    /// \li flase if flag not set
    bool IsBootDisk(const VolumeSummary* ptrVolSum)
    {
        bool bBootDisk = false;
        std::string strBoot = RcmClientUtils::GetAttributeValue(ptrVolSum->attributes,
            NsVolumeAttributes::HAS_BOOTABLE_PARTITION);

        if (0 == strBoot.compare("true"))
            bBootDisk = true;

        return bBootDisk;
    }

    /// \brief /// \brief some of disks dont support scsi address
    /// \returns
    /// \li true if disk doesn't support scsi address.
    /// \li false otherwise
    bool IsScsiUnsupported(const VolumeSummary* ptrVolSum)
    {
        Attributes_t attrs = ptrVolSum->attributes;

        return ((attrs.end() == attrs.find(NsVolumeAttributes::SCSI_BUS)) ||
            (attrs.end() == attrs.find(NsVolumeAttributes::SCSI_LOGICAL_UNIT)) ||
            (attrs.end() == attrs.find(NsVolumeAttributes::SCSI_PORT)) ||
            (attrs.end() == attrs.find(NsVolumeAttributes::SCSI_TARGET_ID)));
    }


    typedef bool(*CheckUnsupportedDisk_t)(const VolumeSummary* ptrVolSum);

    /// \brief checks if disk is not supported for replication on Azure Linux VM
    /// \returns
    /// \li true if Temp, BEK, ISCSI or NVME disk
    /// \li flase othewise
    bool CheckUnsupportedDiskOnLinuxOnAzure(const VolumeSummary* ptrVolSum)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

        // On Linux resource disk is detected by the volume info collector, reuse the same
        // the IDE disks are reported as SCSI disks in Linux
        std::string strAzureTempDisk = RcmClientUtils::GetAttributeValue(ptrVolSum->attributes,
            NsVolumeAttributes::IS_RESOURCE_DISK);
        std::string strAzureBekVolume = RcmClientUtils::GetAttributeValue(ptrVolSum->attributes,
            NsVolumeAttributes::IS_BEK_VOLUME);
        std::string strAzureIscsiDisk = RcmClientUtils::GetAttributeValue(ptrVolSum->attributes,
            NsVolumeAttributes::IS_ISCSI_DISK);
        std::string strAzureNvmeDisk = RcmClientUtils::GetAttributeValue(ptrVolSum->attributes,
            NsVolumeAttributes::IS_NVME_DISK);

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return (0 == strAzureTempDisk.compare("true") || 0 == strAzureBekVolume.compare("true") ||
                0 == strAzureIscsiDisk.compare("true") || 0 == strAzureNvmeDisk.compare("true"));
    }

    /// \brief checks if disk is not supported for replication on on-prem Linux VM
   /// \returns
   /// \li flase always
    bool CheckUnsupportedDiskOnLinuxOnPrem(const VolumeSummary* ptrVolSum)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        if (IsAzureStackVirtualMachine())
        {
            std::string strAzureTempDisk = RcmClientUtils::GetAttributeValue(ptrVolSum->attributes,
                NsVolumeAttributes::IS_RESOURCE_DISK);

            if (boost::iequals(strAzureTempDisk, "true"))
            {
                DebugPrintf(SV_LOG_ALWAYS, "Skipping disk %s as it is Azure Stack VM temp disk\n", ptrVolSum->name.c_str());
                return true;
            }
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return false;
    }

    /// \brief checks if disk is on unsupported interface
    /// \returns
    /// \li true if it is data disk and it is not on scsi for Gen 1 Azure VMs.
    ///     for Gen 2 VMs, if it is on scsi and has is_resource_disk=true
    /// \li flase othewise
    bool CheckUnsupportedDiskOnWindowsOnAzure(const VolumeSummary* ptrVolSum)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

        if (IsSystemDisk(ptrVolSum)) {
            DebugPrintf(SV_LOG_DEBUG, "Disk %s is system disk. It cannot be temp disk.\n", ptrVolSum->name.c_str());
            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
            return false;
        }

        bool bRet = false;

        std::string strInterfaceType = RcmClientUtils::GetAttributeValue(ptrVolSum->attributes,
            NsVolumeAttributes::INTERFACE_TYPE);

        DebugPrintf(SV_LOG_DEBUG, "Disk %s interface %s\n", ptrVolSum->name.c_str(), strInterfaceType.c_str());

        std::string strAzureTempDisk = RcmClientUtils::GetAttributeValue(ptrVolSum->attributes,
            NsVolumeAttributes::IS_RESOURCE_DISK);

        if (boost::iequals(strAzureTempDisk, "true"))
        {
            DebugPrintf(SV_LOG_ALWAYS, "Skipping disk %s as it is Azure VM temp disk\n", ptrVolSum->name.c_str());
            return true;
        }

        if (IsScsiUnsupported(ptrVolSum)) {
            DebugPrintf(SV_LOG_ALWAYS, "Skipping disk %s as it doesn't support scsi address\n", ptrVolSum->name.c_str());
            return true;
        }

        // On Windows, if data disk is not on scsi bus it cannot be a blob.
        // Each blob is connected through scsi interface
        if ((boost::iequals(strInterfaceType, INTERFACE_FILE_BACKED_VIRTUAL) ||
            boost::iequals(strInterfaceType, INTERFACE_ATA) ||
            boost::iequals(strInterfaceType, INTERFACE_IDE))) {
            DebugPrintf(SV_LOG_ALWAYS, "Skipping disk %s as it is on %s interface\n", ptrVolSum->name.c_str(), strInterfaceType.c_str());
            bRet = true;
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return bRet;
    }

    /// \brief checks if disk is on unsupported interface
    /// \returns
    /// \li true if it is data disk and it is file backed storage
    /// \li flase othewise
    bool CheckUnsupportedDiskOnWindowsOnPrem(const VolumeSummary* ptrVolSum)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

        if (IsSystemDisk(ptrVolSum)) {
            DebugPrintf(SV_LOG_DEBUG, "Disk %s is system disk. It is always supported disk.\n", ptrVolSum->name.c_str());
            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
            return false;
        }

        bool bRet = false;

        std::string strInterfaceType = RcmClientUtils::GetAttributeValue(ptrVolSum->attributes,
            NsVolumeAttributes::INTERFACE_TYPE);

        DebugPrintf(SV_LOG_DEBUG, "Disk %s interface %s\n", ptrVolSum->name.c_str(), strInterfaceType.c_str());

        if (boost::iequals(strInterfaceType, INTERFACE_FILE_BACKED_VIRTUAL))
        {
            DebugPrintf(SV_LOG_ALWAYS, "Skipping disk %s as it is on %s interface\n", ptrVolSum->name.c_str(), strInterfaceType.c_str());
            bRet = true;
        }

        if (IsAzureStackVirtualMachine())
        {
            std::string strAzureTempDisk = RcmClientUtils::GetAttributeValue(ptrVolSum->attributes,
                NsVolumeAttributes::IS_RESOURCE_DISK);

            if (boost::iequals(strAzureTempDisk, "true"))
            {
                DebugPrintf(SV_LOG_ALWAYS, "Skipping disk %s as it is Azure Stack VM temp disk\n", ptrVolSum->name.c_str());
                return true;
            }
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return bRet;
    }

    /// \brief updates the parent disk entity based on volume attributes in VolumeSummary
    SVSTATUS Converter::UpdateParentDiskAttributes(const VolumeSummary* ptrVolSum,
        const std::string& parentId)
    {
        SVSTATUS ret = SVS_OK;
        uint32_t uPurpose = 0;
        if ((ptrVolSum->isSystemVolume) ||
            (ptrVolSum->isCacheVolume) ||
            (IsInstallPathVolume(ptrVolSum->name)))
            uPurpose |= RcmClientLib::SystemVolume;

        if (!ptrVolSum->mountPoint.empty())
        {
            if ((0 == ptrVolSum->mountPoint.compare("/")) ||
                (0 == ptrVolSum->mountPoint.compare("/etc")) ||
                (0 == ptrVolSum->mountPoint.compare("/usr")) ||
                (0 == ptrVolSum->mountPoint.compare("/usr/local")) ||
                (0 == ptrVolSum->mountPoint.compare("/var")))
                uPurpose |= RcmClientLib::SystemVolume;
        }

        std::string strBoot = RcmClientUtils::GetAttributeValue(ptrVolSum->attributes,
            NsVolumeAttributes::HAS_BOOTABLE_PARTITION);

        if (0 == strBoot.compare("true"))
            uPurpose |= RcmClientLib::BootVolume;

        if (parentId.empty())
        {
            std::map<std::string, std::list<std::string> >::iterator dgIter;
            dgIter = m_mapDiskGroupDisk.find(ptrVolSum->volumeGroup);
            if (dgIter != m_mapDiskGroupDisk.end())
            {
                std::list<std::string>::iterator diskIter = dgIter->second.begin();
                while (diskIter != dgIter->second.end())
                {
                    std::string strParentId = *diskIter;
                    ret = UpdateDiskAttributes(uPurpose, strParentId);

                    if (ret != SVS_OK)
                        return ret;
                    diskIter++;
                }
            }
        }
        else
        {
            ret = UpdateDiskAttributes(uPurpose, parentId);
        }

        return ret;
    }

    /// \brief /// \brief updates the disk entity based on volume attributes
    /// \returns
    /// \li SVS_OK on success
    /// \li SCS_INVALIDARG on input validation failure
    SVSTATUS Converter::UpdateDiskAttributes(uint32_t uPurpose,
        const std::string& diskId)
    {
        DiskEntityPtr_t ptrde;
        std::map<std::string, DiskEntityPtr_t>::iterator dePtrIter;

        dePtrIter = m_mapDiskEntities.find(diskId);
        if (dePtrIter != m_mapDiskEntities.end())
        {
            ptrde = dePtrIter->second;

            if (uPurpose & RcmClientLib::SystemVolume)
                ptrde->IsSystemDisk = true;

            if (uPurpose & RcmClientLib::BootVolume)
                ptrde->IsBootDisk = true;
        }
        else
        {
            std::stringstream err;
            err << "Error: no disk found for the diskid "
                << diskId
                << " to update the disk attributes";

            DebugPrintf(SV_LOG_ERROR, "UpdateDiskAttributes: %s\n", err.str().c_str());
            m_errorCode = CONVERTER_NO_PARENT_DISK_FOR_VOLUME;
            return SVE_INVALIDARG;
        }

        return SVS_OK;
    }

    /// \brief create disk entities from volume summaries
    /// \returns
    /// \li SVS_OK on success
    /// \li SCS_INVALIDARG on input validation failure
    SVSTATUS Converter::ProcessVolumeSummaries(const std::string& osType,
        VolumeSummaries_t const& volumeSummaries,
        VolumeDynamicInfos_t const & volumeDynamicInfos,
        const bool bClusterContext)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS ret = SVS_OK;
        VolumeSummary::Devicetype devtype[] = { VolumeSummary::DISK,
            VolumeSummary::LOGICAL_VOLUME };

        for (uint8_t index = 0; index < sizeof(devtype) / sizeof(devtype[0]); index++)
        {
            VolumeSummaries_t::const_iterator volSummaryIter = volumeSummaries.begin();
            while (volSummaryIter != volumeSummaries.end())
            {
                if (volSummaryIter->type != devtype[index])
                {
                    volSummaryIter++;
                    continue;
                }

                if (volSummaryIter->type == VolumeSummary::DISK &&
                    volSummaryIter->vendor == VolumeSummary::NATIVE)
                {

                    if (bClusterContext && (volSummaryIter->attributes.find(NsVolumeAttributes::IS_PART_OF_CLUSTER) != volSummaryIter->attributes.end() &&
                        boost::iequals(volSummaryIter->attributes.at(NsVolumeAttributes::IS_PART_OF_CLUSTER), "true")))
                    {
                        
                        ret = UpdateClusterDiskEntities(osType, &(*volSummaryIter));
                        
                    }
                    else
                    {
                        if ( !bClusterContext && volSummaryIter->attributes.find(NsVolumeAttributes::IS_PART_OF_CLUSTER) != volSummaryIter->attributes.end() &&
                            boost::iequals(volSummaryIter->attributes.at(NsVolumeAttributes::IS_PART_OF_CLUSTER), "true"))
                        {
                            DebugPrintf(SV_LOG_ALWAYS, "%s: disk %s is clustered disk but context is non cluster\n", FUNCTION_NAME, volSummaryIter->id.c_str());
                        }

                        ret = UpdateDiskEntities(osType, &(*volSummaryIter));
                    }
                }
                else if (volSummaryIter->type == VolumeSummary::LOGICAL_VOLUME)
                {
                    VolumeDynamicInfo volumeDynamicInfo;

                    ret = GetVolumeDynamicInfo(volSummaryIter->name, volumeDynamicInfos, volumeDynamicInfo);
                    if (ret != SVS_OK)
                    {
                        DebugPrintf(SV_LOG_ERROR, "%s: failed to get the volume dynamic info for the volume %s\n",
                            FUNCTION_NAME,
                            volSummaryIter->id.c_str());
                    }
                    if (bClusterContext && (volSummaryIter->attributes.find(NsVolumeAttributes::IS_PART_OF_CLUSTER) != volSummaryIter->attributes.end() &&
                        boost::iequals(volSummaryIter->attributes.at(NsVolumeAttributes::IS_PART_OF_CLUSTER), "true")))
                    {
                        DebugPrintf(SV_LOG_ALWAYS, "%s: Not reporting the volume %s as it is detected as clustered volume in a cluster context\n", FUNCTION_NAME, volSummaryIter->name.c_str());
                    }
                    else
                    {
                        ret = UpdateLvEntities(&(*volSummaryIter), volumeDynamicInfo);
                    }
                }
                else if (volSummaryIter->type == VolumeSummary::PARTITION)
                {
                    /// a partition should never be in top level summaries
                    /// a partition should always be a child
                    std::stringstream err;
                    err << "Error: found a partition in top level volume summaries "
                        << volSummaryIter->name;

                    DebugPrintf(SV_LOG_ERROR, "ProcessVolumeSummaries: %s\n", err.str().c_str());
                    m_errorCode = CONVERTER_PARTITION_FOUND_IN_TOP_LELVEL_VOLUMESUMMARIES;
                    return SVE_INVALIDARG;
                }

                if (ret != SVS_OK)
                    return ret;

                volSummaryIter++;
            }
        }

        if (m_mapDiskEntities.empty())
        {
            m_errorCode = CONVERTER_FOUND_NO_SYSTEM_DISKS;
            DebugPrintf(SV_LOG_ERROR,
                "%s: no disks found in volume summaries. Error : %d.\n",
                FUNCTION_NAME,
                m_errorCode);

            return SVE_INVALIDARG;
        }

        if (bClusterContext && m_mapClusterDiskEntities.empty())
        {
           
            m_errorCode = CONVERTER_FOUND_NO_CLUSTER_DISK;
            DebugPrintf(SV_LOG_ALWAYS,
                "%s:No cluster disks found in volume summaries. Error : %d.\n",
                FUNCTION_NAME,
                m_errorCode);
        }
        
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

        return SVS_OK;
    }

    /// \brief utility routine to get SCSI HCTL numbers
    void Converter::GetScsiAddress(const VolumeSummary *ptrVolSum,
        uint32_t& scsiBus,
        uint32_t& scsiLu,
        uint32_t& scsiPort,
        uint32_t& scsiTgt)
    {
        scsiBus = strtol((RcmClientUtils::GetAttributeValue(
            ptrVolSum->attributes,
            NsVolumeAttributes::SCSI_BUS, true)).c_str(),
            NULL,
            10);

        scsiLu = strtol((RcmClientUtils::GetAttributeValue(
            ptrVolSum->attributes,
            NsVolumeAttributes::SCSI_LOGICAL_UNIT, true)).c_str(),
            NULL,
            10);

        scsiPort = strtol((RcmClientUtils::GetAttributeValue(
            ptrVolSum->attributes,
            NsVolumeAttributes::SCSI_PORT, true)).c_str(),
            NULL,
            10);

        scsiTgt = strtol((RcmClientUtils::GetAttributeValue(
            ptrVolSum->attributes,
            NsVolumeAttributes::SCSI_TARGET_ID, true)).c_str(),
            NULL,
            10);
    }

    /// \brief checks if a disk is reported more than once
    /// \note on Windows it checks the disk signature
    /// \returns
    /// \li true if flag is set
    /// \li flase if flag not set
    bool Converter::IsDuplicateDisk(DiskEntityPtr_t ptrDe, const VolumeSummary *ptrVolSum)
    {
        std::string strNameBasedon = RcmClientUtils::GetAttributeValue(ptrVolSum->attributes,
            NsVolumeAttributes::NAME_BASED_ON);

        if (0 != strNameBasedon.compare(NsVolumeAttributes::SIGNATURE))
            return false;

        uint32_t scsiBus, scsiLu, scsiTgt, scsiPort;
        GetScsiAddress(ptrVolSum, scsiBus, scsiLu, scsiPort, scsiTgt);

        if ((scsiBus != ptrDe->ScsiBus) ||
            (scsiLu != ptrDe->ScsiLogicalUnit) ||
            (scsiPort != ptrDe->ScsiPort) ||
            (scsiTgt != ptrDe->ScsiTargetId))
            return true;

        return false;
    }

    /// \brief add disk entities from volume summaries
    /// \returns
    /// \li SVS_OK on success
    /// \li SCS_INVALIDARG on input validation failure
    SVSTATUS Converter::UpdateDiskEntities(const std::string& osType,
        const VolumeSummary* ptrVolSum)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s\n", FUNCTION_NAME, ptrVolSum->name.c_str());
        const std::string strDataDisk("DataDisk");
        const std::string strRootDisk("RootDisk");
        SVSTATUS ret = SVS_OK;
        DiskEntityPtr_t ptrde;
        std::map<std::string, DiskEntityPtr_t>::iterator dePtrIter;
        std::string id;
        std::string displayName;

        CheckUnsupportedDisk_t IsUnsupportedDiskOnPrem[] = { &CheckUnsupportedDiskOnLinuxOnPrem, &CheckUnsupportedDiskOnWindowsOnPrem };
        CheckUnsupportedDisk_t IsUnsupportedDiskOnAzure[] = { &CheckUnsupportedDiskOnLinuxOnAzure, &CheckUnsupportedDiskOnWindowsOnAzure };

        int os = (0 == osType.compare(OS_TYPE_WINDOWS));
        CheckUnsupportedDisk_t isUnsupportedDisk;
        if (m_diskReportType == RCM_DISK_REPORT)
        {
            isUnsupportedDisk = IsUnsupportedDiskOnAzure[os];
        }
        else if (m_diskReportType = RCM_PROXY_DISK_REPORT)
        {
            isUnsupportedDisk = IsUnsupportedDiskOnPrem[os];
        }

        if ((*isUnsupportedDisk)(ptrVolSum))
        {
            DebugPrintf(SV_LOG_ERROR,
                "UpdateDiskEntities: Not reporting disk %s to RCM as it is Azure VM Unsupported disk\n",
                ptrVolSum->name.c_str());
            m_listNotReportedDisks.push_back(ptrVolSum->name);
            m_listNotReportedDiskGroups.push_back(ptrVolSum->volumeGroup);
            return SVS_OK;
        }

        uint32_t scsiBus, scsiLu, scsiTgt, scsiPort;
        GetScsiAddress(ptrVolSum, scsiBus, scsiLu, scsiPort, scsiTgt);

        if ((m_diskReportType == RCM_DISK_REPORT) && !IsBootDisk(ptrVolSum))
        {
            if ((m_dataDiskScsiBus == UNINITIALIZED_SCSI_CONTROLLER_ID) &&
                (m_dataDiskScsiPort == UNINITIALIZED_SCSI_CONTROLLER_ID) &&
                (m_dataDiskScsiTarget == UNINITIALIZED_SCSI_CONTROLLER_ID))
            {
                m_dataDiskScsiBus = scsiBus;
                m_dataDiskScsiPort = scsiPort;
                m_dataDiskScsiTarget = scsiTgt;
            }
            else if ((m_dataDiskScsiBus != scsiBus) ||
                (m_dataDiskScsiPort != scsiPort) ||
                (m_dataDiskScsiTarget != scsiTgt))
            {
                std::stringstream errmsg;
                errmsg << "Not all data disks are on same SCSI controller."
                    << " SCSI Bus: " << m_dataDiskScsiBus << " " << scsiBus
                    << " SCSI Port " << m_dataDiskScsiPort << " " << scsiPort
                    << " SCSI Target " << m_dataDiskScsiTarget << " " << scsiTgt;

                m_errorCode = CONVERTER_FOUND_MULTIPLE_CONTROLLERS_FOR_DATA_DISKS;

                DebugPrintf(SV_LOG_ERROR,
                    "%s: Error %d : %s\n",
                    FUNCTION_NAME,
                    m_errorCode,
                    errmsg.str().c_str());

                return SVE_INVALIDARG;
            }
        }

        id = ptrVolSum->name;
        displayName = RcmClientUtils::GetAttributeValue(ptrVolSum->attributes,
                                                        NsVolumeAttributes::DEVICE_NAME);

        if (0 == osType.compare(OS_TYPE_WINDOWS))
        {
            // for A2A and V2A scenarios agent should always report
            // name based on disk signature 

            std::string strNameBasedon = RcmClientUtils::GetAttributeValue(ptrVolSum->attributes,
                NsVolumeAttributes::NAME_BASED_ON);

            if (0 != strNameBasedon.compare(NsVolumeAttributes::SIGNATURE))
            {
                std::stringstream errmsg;
                errmsg << "Error : For  "
                    << displayName
                    << ", Id is not based on disk signature.";
                DebugPrintf(SV_LOG_ERROR, "UpdateDiskEntities: %s\n", errmsg.str().c_str());

                m_errorCode = CONVERTER_DEVICE_ID_NOT_BASED_ON_SIGNATURE;
                return SVE_INVALIDARG;
            }
        }

        dePtrIter = m_mapDiskEntities.find(id);
        if (dePtrIter != m_mapDiskEntities.end())
        {
            ptrde = dePtrIter->second;

            if (IsDuplicateDisk(ptrde, ptrVolSum))
            {
                std::stringstream errmsg;
                errmsg << "Error : Found duplicate disk "
                    << ptrVolSum->name
                    << " for "
                    << ptrde->Id;

                DebugPrintf(SV_LOG_ERROR, "UpdateDiskEntities: %s\n", errmsg.str().c_str());
                m_errorCode = CONVERTER_FOUND_DUPLICATE_DISK_ID;
                return SVE_INVALIDARG;
            }

            std::set<std::string> deviceNames(ptrde->DeviceNames.begin(),
                                                ptrde->DeviceNames.end());
            deviceNames.insert(displayName);
            ptrde->DeviceNames.assign(deviceNames.begin(),
                deviceNames.end());

            if (IsSystemDisk(ptrVolSum))
                ptrde->IsSystemDisk = true;
            if (IsBootDisk(ptrVolSum))
                ptrde->IsBootDisk = true;
        }
        else
        {
            std::vector<std::string> deviceNames;
            deviceNames.push_back(displayName);

            std::string type = RcmClientUtils::GetAttributeValue(ptrVolSum->attributes,
                NsVolumeAttributes::STORAGE_TYPE);
            if (type.empty())
                type = NsVolumeAttributes::BASIC;

            if (0 == type.compare(NsVolumeAttributes::BASIC))
                type = "Basic";
            else if (0 == type.compare(NsVolumeAttributes::DYNAMIC))
                type = "Dynamic";
            bool isSystemDisk = IsSystemDisk(ptrVolSum);
            bool isBootDisk = IsBootDisk(ptrVolSum);

            std::string diskUuid = RcmClientUtils::GetAttributeValue(ptrVolSum->attributes,
                NsVolumeAttributes::SCSI_UUID);

            ptrde.reset(new DiskEntity(id,
                deviceNames,
                ptrVolSum->rawcapacity,
                ptrVolSum->sectorSize,
                WriteCacheState[ptrVolSum->writeCacheState],
                scsiBus,
                scsiLu,
                scsiPort,
                scsiTgt,
                ptrVolSum->fileSystem,
                0,
                isSystemDisk,
                isBootDisk,
                ptrVolSum->mountPoint,
                type,
                PartitionScheme[ptrVolSum->formatLabel],
                ptrVolSum->volumeGroup,
                diskUuid));

            m_mapDiskEntities.insert(std::make_pair(id, ptrde));
        }

        if (!ptrVolSum->volumeGroup.empty())
        {
            std::map<std::string, std::list<std::string> >::iterator dgIter;
            dgIter = m_mapDiskGroupDisk.find(ptrVolSum->volumeGroup);
            if (dgIter != m_mapDiskGroupDisk.end())
            {
                dgIter->second.push_back(id);
            }
            else
            {
                std::list<std::string> diskList(1, id);
                m_mapDiskGroupDisk.insert(std::make_pair(ptrVolSum->volumeGroup, diskList));
            }
        }

        VolumeSummaries_t::const_iterator volSummaryIter = ptrVolSum->children.begin();
        while (volSummaryIter != ptrVolSum->children.end())
        {
            ret = UpdateDiskPartitionEntities(osType, id, &(*volSummaryIter));

            if (ret != SVS_OK)
                return ret;

            volSummaryIter++;
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

        return SVS_OK;
    }


    /// \brief add artition entities from volume summaries
    /// \returns
    /// \li SVS_OK on success
    /// \li SCS_INVALIDARG on input validation failure
    SVSTATUS Converter::UpdateDiskPartitionEntities(const std::string& osType,
        const std::string& parent,
        const VolumeSummary* ptrVolSum)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s\n", FUNCTION_NAME, ptrVolSum->name.c_str());
        SVSTATUS ret = SVS_OK;
        if (ptrVolSum->type == VolumeSummary::EXTENDED_PARTITION)
        {
            // do not report extented partitions
            return SVS_OK;
        }

        if (ptrVolSum->type != VolumeSummary::PARTITION)
        {
            std::stringstream errmsg;
            errmsg << "For parent disk " << parent << " child "
                << ptrVolSum->name << " is not a partition ";

            DebugPrintf(SV_LOG_ERROR, "UpdateDiskPartitionEntities: %s\n", errmsg.str().c_str());
            m_errorCode = CONVERTER_DISK_CHILD_IS_NOT_PARTITION;
            return SVE_INVALIDARG;
        }


        DiskPartitionEntityPtr_t ptrDiskPart;
        std::map<std::string, DiskPartitionEntityPtr_t>::iterator dpeIter;

        std::string id = ptrVolSum->name;


        dpeIter = m_mapDiskPartitionEntities.find(id);
        if (dpeIter != m_mapDiskPartitionEntities.end())
        {
            ptrDiskPart = dpeIter->second;
            std::set<std::string> deviceNames(ptrDiskPart->DeviceNames.begin(),
                ptrDiskPart->DeviceNames.end());

            deviceNames.insert(ptrVolSum->name);

            ptrDiskPart->DeviceNames.assign(deviceNames.begin(),
                deviceNames.end());

            ret = UpdateParentDiskAttributes(ptrVolSum, ptrDiskPart->ParentDisk);
        }
        else
        {
            std::vector<std::string> deviceNames(1, ptrVolSum->name);

            ret = UpdateParentDiskAttributes(ptrVolSum, parent);

            ptrDiskPart.reset(new DiskPartitionEntity(id,
                ptrVolSum->volumeLabel,
                deviceNames,
                ptrVolSum->rawcapacity,
                parent,
                ptrVolSum->fileSystem,
                ptrVolSum->mountPoint,
                ptrVolSum->volumeGroup));

            m_mapDiskPartitionEntities.insert(std::make_pair(id, ptrDiskPart));
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return ret;
    }

    /// \brief add logical volume entities from volume summaries
    /// \returns
    /// \li SVS_OK on success
    /// \li SCS_INVALIDARG on input validation failure
    SVSTATUS Converter::UpdateLvEntities(const VolumeSummary* ptrVolSum,
        VolumeDynamicInfo &volDynamicInfo)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s\n", FUNCTION_NAME, ptrVolSum->name.c_str());
        SVSTATUS ret = SVS_OK;
        LogicalVolumeEntityPtr_t ptrLv;
        std::map<std::string, LogicalVolumeEntityPtr_t>::iterator lveIter;

        std::string id = ptrVolSum->name;

        lveIter = m_mapLogicalVolumeEntities.find(id);
        if (lveIter != m_mapLogicalVolumeEntities.end())
        {
            std::stringstream errMsg;
            errMsg << "Error: Found duplicate volume in summaries for "
                << id;

            DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.str().c_str());
            m_errorCode = CONVERTER_FOUND_DUPLICATE_VOLUME_ID;

            return SVE_INVALIDARG;
        }

        std::vector<std::string> deviceNames(1, ptrVolSum->name);
        std::vector<std::string> parents;

        std::map<std::string, std::list<std::string> >::iterator dgIter;
        dgIter = m_mapDiskGroupDisk.find(ptrVolSum->volumeGroup);
        if (dgIter != m_mapDiskGroupDisk.end())
        {
            parents.assign(dgIter->second.begin(), dgIter->second.end());
        }
        else
        {
            // this is added to not report any logical volumes on the Azure VM temp disk or cluster volume for now
            std::list<std::string>::iterator findIter =
                std::find(m_listNotReportedDiskGroups.begin(),
                m_listNotReportedDiskGroups.end(),
                ptrVolSum->volumeGroup);

            if (findIter != m_listNotReportedDiskGroups.end())
            {
                DebugPrintf(SV_LOG_ERROR,
                    "%s: Volume %s is not reported to RCM as it is on unreported diskgroup %s.\n",
                    FUNCTION_NAME,
                    ptrVolSum->name.c_str(),
                    ptrVolSum->volumeGroup.c_str());

                return SVS_OK;
            }

            std::stringstream errmsg;
            errmsg << "Error : No parent disk found for logical volume "
                << ptrVolSum->name;

            DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errmsg.str().c_str());

            return SVS_OK;
        }

        ret = UpdateParentDiskAttributes(ptrVolSum, std::string());

        ptrLv.reset(new LogicalVolumeEntity(ptrVolSum->name,
            ptrVolSum->volumeLabel,
            deviceNames,
            ptrVolSum->rawcapacity,
            volDynamicInfo.freeSpace,
            parents,
            ptrVolSum->volumeGroup,
            ptrVolSum->fileSystem,
            ptrVolSum->mountPoint));

        m_mapLogicalVolumeEntities.insert(std::make_pair(id, ptrLv));

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return ret;
    }

    /// \brief add disk entities to the machine entity
    /// \returns
    /// \li SVS_OK on success
    /// \li SCS_INVALIDARG on input validation failure
    SVSTATUS Converter::AddDiskEntities(MachineEntity &me, const bool bClusterContext)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

        std::map<std::string, DiskEntityPtr_t>::iterator
            dePtrIter = m_mapDiskEntities.begin();
        while (dePtrIter != m_mapDiskEntities.end())
        {
            DiskEntityPtr_t ptrDe = dePtrIter->second;

            me.DiskInputs.push_back(*(dePtrIter->second));
            dePtrIter++;
        }

        std::map<std::string, DiskPartitionEntityPtr_t>::iterator
            dpePtrIter = m_mapDiskPartitionEntities.begin();
        while (dpePtrIter != m_mapDiskPartitionEntities.end())
        {
            DiskPartitionEntityPtr_t ptrDpe = dpePtrIter->second;

            me.PartitionInputs.push_back(*(dpePtrIter->second));
            dpePtrIter++;
        }

        std::map<std::string, LogicalVolumeEntityPtr_t>::iterator
            lvePtrIter = m_mapLogicalVolumeEntities.begin();
        while (lvePtrIter != m_mapLogicalVolumeEntities.end())
        {
            LogicalVolumeEntityPtr_t ptrLve = lvePtrIter->second;

            me.LogicalVolumes.push_back(*(lvePtrIter->second));
            lvePtrIter++;
        }

        std::map<std::string, std::list<std::string> >::iterator
            dgIter = m_mapDiskGroupDisk.begin();
        while (dgIter != m_mapDiskGroupDisk.end())
        {
            DiskGroupEntity dge(dgIter->first);
            me.DiskGroups.push_back(dge);
            dgIter++;
        }
        
        if(bClusterContext)
        {
            std::map<std::string, ClusterDiskEntityPtr_t>::iterator
                cdePtrIter = m_mapClusterDiskEntities.begin();
            while (cdePtrIter != m_mapClusterDiskEntities.end())
            {
                me.RegisterSourceClusterInput.RegisterSourceClusterVirtualNodeInput.DiskInputs.push_back(*(cdePtrIter->second));
                cdePtrIter++;
            }

            std::map<std::string, DiskPartitionEntityPtr_t>::iterator
                cdpePtrIter = m_mapClusterDiskPartitionEntities.begin();
            while (cdpePtrIter != m_mapClusterDiskPartitionEntities.end())
            {
                //DiskPartitionEntityPtr_t ptrDpe = dpePtrIter->second;

                me.RegisterSourceClusterInput.RegisterSourceClusterVirtualNodeInput.PartitionInputs.push_back(*(cdpePtrIter->second));
                cdpePtrIter++;
            }

            std::map<std::string, LogicalVolumeEntityPtr_t>::iterator
                clvePtrIter = m_mapClusterLogicalVolumeEntities.begin();
            while (clvePtrIter != m_mapClusterLogicalVolumeEntities.end())
            {
                me.RegisterSourceClusterInput.RegisterSourceClusterVirtualNodeInput.LogicalVolumes.push_back(*(clvePtrIter->second));
                clvePtrIter++;
            }

            std::map<std::string, std::list<std::string> >::iterator
                cdgIter = m_mapClusterDiskGroupDisk.begin();
            while (cdgIter != m_mapClusterDiskGroupDisk.end())
            {
                DiskGroupEntity cdge(cdgIter->first);
                me.RegisterSourceClusterInput.RegisterSourceClusterVirtualNodeInput.DiskGroups.push_back(cdge);
                cdgIter++;
            }
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

        return SVS_OK;
    }

    /// \brief returns the error code
    int32_t Converter::GetErrorCode()
    {
        return m_errorCode;
    }


    SVSTATUS Converter::GetVolumeDynamicInfo(const std::string &volName,
        const VolumeDynamicInfos_t  &volumeDynamicInfos,
        VolumeDynamicInfo &volDynamicInfo)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

        SVSTATUS status = SVE_FAIL;
        ConstVolumeDynamicInfosIter iter = volumeDynamicInfos.begin();
        for (/*empty*/; iter != volumeDynamicInfos.end(); iter++)
        {
            if (volName == iter->name)
            {
                volDynamicInfo = *iter;
                status = SVS_OK;
                DebugPrintf(SV_LOG_DEBUG, "%s: for volume %s freespace is %lld bytes.\n",
                    FUNCTION_NAME,
                    volName.c_str(),
                    iter->freeSpace);

                break;
            }
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }
    
    /// \brief add cluster disk partition entities from cluster volume summaries
    /// \returns
    /// \li SVS_OK on success
    /// \li SCS_INVALIDARG on input validation failure
    SVSTATUS Converter::UpdateClusterDiskPartitionEntities(const std::string& osType,
        const std::string& parent,
        const VolumeSummary* ptrVolSum)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s\n", FUNCTION_NAME, ptrVolSum->name.c_str());
        SVSTATUS ret = SVS_OK;
        if (ptrVolSum->type == VolumeSummary::EXTENDED_PARTITION)
        {
            // do not report extented partitions
            return SVS_OK;
        }

        if (ptrVolSum->type != VolumeSummary::PARTITION)
        {
            std::stringstream errmsg;
            errmsg << "For parent disk " << parent << " child "
                << ptrVolSum->name << " is not a partition ";

            DebugPrintf(SV_LOG_ERROR, "UpdateClusterDiskPartitionEntities: %s\n", errmsg.str().c_str());
            m_errorCode = CONVERTER_CLUSTER_DISK_CHILD_IS_NOT_PARTITION;

            return SVE_INVALIDARG;
        }

        DiskPartitionEntityPtr_t ptrClusterDiskPart;
        std::map<std::string, DiskPartitionEntityPtr_t>::iterator dpeIter;

        std::string id = ptrVolSum->name;

        dpeIter = m_mapClusterDiskPartitionEntities.find(id);
        if (dpeIter == m_mapClusterDiskPartitionEntities.end())
        {
            std::vector<std::string> deviceNames(1, ptrVolSum->name);
            
            ptrClusterDiskPart.reset(new DiskPartitionEntity(id,
                ptrVolSum->volumeLabel,
                deviceNames,
                ptrVolSum->rawcapacity,
                parent,
                ptrVolSum->fileSystem,
                ptrVolSum->mountPoint,
                ptrVolSum->volumeGroup));

            m_mapClusterDiskPartitionEntities.insert(std::make_pair(id, ptrClusterDiskPart));
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return ret;
    }

    /// \brief add cluster disk entities from cluster volume summaries
    /// \returns
    /// \li SVS_OK on success
    /// \li SCS_INVALIDARG on input validation failure
    SVSTATUS Converter::UpdateClusterDiskEntities(const std::string& osType,
        const VolumeSummary* ptrVolSum)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s\n", FUNCTION_NAME, ptrVolSum->name.c_str());
        const std::string strDataDisk("DataDisk");
        const std::string strRootDisk("RootDisk");
        SVSTATUS ret = SVS_OK;
        ClusterDiskEntityPtr_t ptrcde;
        std::map<std::string, ClusterDiskEntityPtr_t>::iterator cdePtrIter;
        std::string id;
        std::string displayName;

        CheckUnsupportedDisk_t IsUnsupportedDiskOnPrem[] = { &CheckUnsupportedDiskOnLinuxOnPrem, &CheckUnsupportedDiskOnWindowsOnPrem };
        CheckUnsupportedDisk_t IsUnsupportedDiskOnAzure[] = { &CheckUnsupportedDiskOnLinuxOnAzure, &CheckUnsupportedDiskOnWindowsOnAzure };

        int os = (0 == osType.compare(OS_TYPE_WINDOWS));
        CheckUnsupportedDisk_t isUnsupportedDisk;
        if (m_diskReportType == RCM_DISK_REPORT)
        {
            isUnsupportedDisk = IsUnsupportedDiskOnAzure[os];
        }
        else if (m_diskReportType = RCM_PROXY_DISK_REPORT)
        {
            isUnsupportedDisk = IsUnsupportedDiskOnPrem[os];
        }

        if ((*isUnsupportedDisk)(ptrVolSum))
        {
            DebugPrintf(SV_LOG_ERROR,
                "UpdateClusterDiskEntities: Not reporting disk %s to RCM as it is Azure VM Unsupported disk\n",
                ptrVolSum->name.c_str());
            m_listNotReportedDisks.push_back(ptrVolSum->name);
            m_listNotReportedDiskGroups.push_back(ptrVolSum->volumeGroup);
            return SVS_OK;
        }

        uint32_t scsiBus, scsiLu, scsiTgt, scsiPort;
        GetScsiAddress(ptrVolSum, scsiBus, scsiLu, scsiPort, scsiTgt);

        if ((m_diskReportType == RCM_DISK_REPORT) && !IsBootDisk(ptrVolSum))
        {
            if ((m_dataDiskScsiBus == UNINITIALIZED_SCSI_CONTROLLER_ID) &&
                (m_dataDiskScsiPort == UNINITIALIZED_SCSI_CONTROLLER_ID) &&
                (m_dataDiskScsiTarget == UNINITIALIZED_SCSI_CONTROLLER_ID))
            {
                m_dataDiskScsiBus = scsiBus;
                m_dataDiskScsiPort = scsiPort;
                m_dataDiskScsiTarget = scsiTgt;
            }
            else if ((m_dataDiskScsiBus != scsiBus) ||
                (m_dataDiskScsiPort != scsiPort) ||
                (m_dataDiskScsiTarget != scsiTgt))
            {
                std::stringstream errmsg;
                errmsg << "Not all data disks are on same SCSI controller."
                    << " SCSI Bus: " << m_dataDiskScsiBus << " " << scsiBus
                    << " SCSI Port " << m_dataDiskScsiPort << " " << scsiPort
                    << " SCSI Target " << m_dataDiskScsiTarget << " " << scsiTgt;

                m_errorCode = CONVERTER_FOUND_MULTIPLE_CONTROLLERS_FOR_DATA_DISKS;

                DebugPrintf(SV_LOG_ERROR,
                    "%s: Error %d : %s\n",
                    FUNCTION_NAME,
                    m_errorCode,
                    errmsg.str().c_str());

                return SVE_INVALIDARG;
            }
        }

        id = ptrVolSum->name;
        displayName = RcmClientUtils::GetAttributeValue(ptrVolSum->attributes,
            NsVolumeAttributes::DEVICE_NAME);

        if (0 == osType.compare(OS_TYPE_WINDOWS))
        {
            // for A2A and V2A scenarios agent should always report
            // name based on disk signature 

            std::string strNameBasedon = RcmClientUtils::GetAttributeValue(ptrVolSum->attributes,
                NsVolumeAttributes::NAME_BASED_ON);

            if (0 != strNameBasedon.compare(NsVolumeAttributes::SIGNATURE))
            {
                std::stringstream errmsg;
                errmsg << "Error : For  "
                    << displayName
                    << ", Id is not based on disk signature.";
                DebugPrintf(SV_LOG_ERROR, "UpdateClusterDiskEntities: %s\n", errmsg.str().c_str());

                m_errorCode = CONVERTER_DEVICE_ID_NOT_BASED_ON_SIGNATURE;
                return SVE_INVALIDARG;
            }
        }

        cdePtrIter = m_mapClusterDiskEntities.find(id);
        if (cdePtrIter != m_mapClusterDiskEntities.end())
        {
            ptrcde = cdePtrIter->second;

            if (IsDuplicateDisk(ptrcde, ptrVolSum))
            {
                std::stringstream errmsg;
                errmsg << "Error : Found duplicate disk "
                    << ptrVolSum->name
                    << " for "
                    << ptrcde->Id;

                DebugPrintf(SV_LOG_ERROR, "UpdateClusterDiskEntities: %s\n", errmsg.str().c_str());
                m_errorCode = CONVERTER_FOUND_DUPLICATE_CLUSTER_DISK_ID;
                return SVE_INVALIDARG;
            }

            std::set<std::string> deviceNames(ptrcde->DeviceNames.begin(),
                ptrcde->DeviceNames.end());
            deviceNames.insert(displayName);
            ptrcde->DeviceNames.assign(deviceNames.begin(),
                deviceNames.end());

            if (IsSystemDisk(ptrVolSum))
                ptrcde->IsSystemDisk = true;
            if (IsBootDisk(ptrVolSum))
                ptrcde->IsBootDisk = true;
        }
        else
        {
            std::vector<std::string> deviceNames;
            deviceNames.push_back(displayName);

            std::string type = RcmClientUtils::GetAttributeValue(ptrVolSum->attributes,
                NsVolumeAttributes::STORAGE_TYPE);
            if (type.empty())
                type = NsVolumeAttributes::BASIC;

            if (0 == type.compare(NsVolumeAttributes::BASIC))
                type = "Basic";
            else if (0 == type.compare(NsVolumeAttributes::DYNAMIC))
                type = "Dynamic";
            bool isSystemDisk = IsSystemDisk(ptrVolSum);
            bool isBootDisk = IsBootDisk(ptrVolSum);

            std::string diskUuid = RcmClientUtils::GetAttributeValue(ptrVolSum->attributes,
                NsVolumeAttributes::SCSI_UUID);

            ptrcde.reset(new SourceClusterDiskEntity(id,
                deviceNames,
                ptrVolSum->rawcapacity,
                ptrVolSum->sectorSize,
                WriteCacheState[ptrVolSum->writeCacheState],
                scsiBus,
                scsiLu,
                scsiPort,
                scsiTgt,
                ptrVolSum->fileSystem,
                0,
                isSystemDisk,
                isBootDisk,
                ptrVolSum->mountPoint,
                type,
                PartitionScheme[ptrVolSum->formatLabel],
                ptrVolSum->volumeGroup,
                diskUuid));

            m_mapClusterDiskEntities.insert(std::make_pair(id, ptrcde));
        }

        if (!ptrVolSum->volumeGroup.empty())
        {
            std::map<std::string, std::list<std::string> >::iterator cdgIter;
            cdgIter = m_mapClusterDiskGroupDisk.find(ptrVolSum->volumeGroup);
            if (cdgIter != m_mapClusterDiskGroupDisk.end())
            {
                cdgIter->second.push_back(id);
            }
            else
            {
                std::list<std::string> diskList(1, id);
                m_mapClusterDiskGroupDisk.insert(std::make_pair(ptrVolSum->volumeGroup, diskList));
            }
        }

        VolumeSummaries_t::const_iterator volSummaryIter = ptrVolSum->children.begin();
        while (volSummaryIter != ptrVolSum->children.end())
        {
            ret = UpdateClusterDiskPartitionEntities(osType, id, &(*volSummaryIter));

            if (ret != SVS_OK)
                return ret;

            volSummaryIter++;
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

        return SVS_OK;
    }
}