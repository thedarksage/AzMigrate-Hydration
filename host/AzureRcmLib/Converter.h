/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp. 2016
+------------------------------------------------------------------------------------+
File        :   Converter.h

Description :   Converter class creates the RCM Entities from the VolumeSummary class
objects.

+------------------------------------------------------------------------------------+
*/

#ifndef _CONVERTER_H
#define _CONVERTER_H

#include "volumegroupsettings.h"
#include "protecteddevicedetails.h"
#include "MachineEntity.h"

#include "ConverterErrorCodes.h"

namespace RcmClientLib
{

    enum DiskReportTypes { RCM_DISK_REPORT, RCM_PROXY_DISK_REPORT };

    const int32_t UNINITIALIZED_SCSI_CONTROLLER_ID = -1;

    ///// \brief fetches an attribute value for the key specified
    ///// \exception conditionally throws std::string exception if not found
    //std::string GetAttributeValue(Attributes_t const& attrs,
    //    const char *key,
    //    bool throwIfNotFound = false);

    /// \brief converts VolumeSummaries to RCM Disk/Partition/LogicalVolume Entities
    /// \note this class is not thread safe
    /// \exception throws std::exception on failure
    class Converter
    {
        /// \brief maps disk Id and Disk Entity
        std::map<std::string, DiskEntityPtr_t> m_mapDiskEntities;

        /// \brief maps disk partition Id and Disk Partition Entities
        std::map<std::string, DiskPartitionEntityPtr_t> m_mapDiskPartitionEntities;

        /// \brief maps Logical Volume Id and Logical Volume Entities
        std::map<std::string, LogicalVolumeEntityPtr_t> m_mapLogicalVolumeEntities;

        /// \brief maps disk group Id and list of disk Ids
        std::map<std::string, std::list<std::string> > m_mapDiskGroupDisk;

        /// \brief maps cluster disk Id and Disk Entity
        std::map<std::string, ClusterDiskEntityPtr_t> m_mapClusterDiskEntities;

        /// \brief maps cluster disk partition Id and cluster Disk Partition Entities
        std::map<std::string, DiskPartitionEntityPtr_t> m_mapClusterDiskPartitionEntities;

        /// \brief maps cluster Logical Volume Id and cluster Logical Volume Entities
        std::map<std::string, LogicalVolumeEntityPtr_t> m_mapClusterLogicalVolumeEntities;

        /// \brief maps cluster disk group Id and list of cluster disk Ids
        std::map<std::string, std::list<std::string> > m_mapClusterDiskGroupDisk;

        /// \brief list of disks that are not reported
        std::list<std::string> m_listNotReportedDisks;

        /// \brief list of diskgroups that are not reported
        std::list<std::string> m_listNotReportedDiskGroups;

        int32_t m_dataDiskScsiBus;
        int32_t m_dataDiskScsiPort;
        int32_t m_dataDiskScsiTarget;
    
        DiskReportTypes  m_diskReportType;

#ifdef WIN32
        /// \brief error code 
        static __declspec(thread) CONVERTER_STATUS m_errorCode;
#else
        static __thread CONVERTER_STATUS m_errorCode;
#endif

        /// \brief converts VolumeSummary to DiskEntity and adds to m_mapDiskEntities
        SVSTATUS UpdateDiskEntities(const std::string& osType,
            const VolumeSummary* ptrVolSum);

        /// \brief converts Cluster VolumeSummary to ClusterDiskEntity and adds to m_mapClusterDiskEntities
        SVSTATUS UpdateClusterDiskEntities(const std::string& osType,
            const VolumeSummary* ptrVolSum);

        /// \brief converts VolumeSummary to DiskPartitionEntity and
        /// adds to m_mapDiskPartitionEntities
        SVSTATUS UpdateDiskPartitionEntities(const std::string& osType,
            const std::string& parent,
            const VolumeSummary* ptrVolSum);

        /// \brief converts Cluster VolumeSummary to Cluster DiskPartitionEntity and
        /// adds to m_mapClusterDiskPartitionEntities
        SVSTATUS UpdateClusterDiskPartitionEntities(const std::string& osType,
            const std::string& parent,
            const VolumeSummary* ptrVolSum);

        /// \brief converts VolumeSummary in LogicalVolumeEntity and
        /// adds to m_mapLogicalVolumeEntities
        SVSTATUS UpdateLvEntities(const VolumeSummary* ptrVolSum,
            VolumeDynamicInfo &volDynamicInfo);

        /// \brief converts Cluster VolumeSummary in Cluster LogicalVolumeEntity and
        /// adds to m_mapClusterLogicalVolumeEntities
        SVSTATUS UpdateClusterLvEntities(const VolumeSummary* ptrVolSum,
            VolumeDynamicInfo& volDynamicInfo);

        /// \brief updates the parent disk entity based on volume attributes in VolumeSummary
        SVSTATUS UpdateParentDiskAttributes(const VolumeSummary* ptrVolSum,
            const std::string& parentId);


        /// \brief updates the disk entity based on volume attributes
        SVSTATUS UpdateDiskAttributes(uint32_t purpose,
            const std::string& diskId);

        /// \brief validates if there are duplicate disk entities
        bool IsDuplicateDisk(DiskEntityPtr_t ptrde, const VolumeSummary* ptrVolSum);

        /// \brief returns the SCSI H:C:T:L attributes
        void GetScsiAddress(const VolumeSummary *ptrVolSum,
            uint32_t& scsiBus,
            uint32_t& scsiLu,
            uint32_t& scsiPort,
            uint32_t& scsiTgt);

        SVSTATUS GetVolumeDynamicInfo(const std::string &volName,
            const VolumeDynamicInfos_t  &volumeDynamicInfos,
            VolumeDynamicInfo &volDynamicInfo);

    public:

        Converter(DiskReportTypes type)
            :m_diskReportType(type),
            m_dataDiskScsiBus(UNINITIALIZED_SCSI_CONTROLLER_ID),
            m_dataDiskScsiPort(UNINITIALIZED_SCSI_CONTROLLER_ID),
            m_dataDiskScsiTarget(UNINITIALIZED_SCSI_CONTROLLER_ID)
        {
            m_errorCode = (CONVERTER_STATUS)0;
        }

        /// \brief iterates through the input volume summaries and convers
        /// them into RCM Disk/Partition/LogicalVolume entities
        SVSTATUS ProcessVolumeSummaries(const std::string& osType,
            VolumeSummaries_t const & volumeSummaries,
            VolumeDynamicInfos_t const & volumeDynamicInfos,
            const bool bClusterContext);
        
        /// \brief adds the processed volume summaries to Machine Entity
        SVSTATUS AddDiskEntities(MachineEntity &me,const bool bClusterContext);

        /// \brief return error code set by this class
        int32_t GetErrorCode();
    };
}

#endif
