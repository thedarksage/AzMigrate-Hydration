/*
+------------------------------------------------------------------------------------+
File		:	SourceClusterEntity.h

Description	:   SourceClusterEntity class is defind as per the RCM RegisterMachineInput
class. The member names should match that defined in the RCM RegisterSourceClusterInput class.
Uses the ESJ JSON formatter to serialize and unserialize JSON string.

+------------------------------------------------------------------------------------+
*/

#ifndef _CLUSTER_ENTITY_H
#define _CLUSTER_ENTITY_H

#include "DiskEntity.h"
#include "DiskGroupEntity.h"
#include "DiskPartitionEntity.h"
#include "LogicalVolumeEntity.h"

#include "json_writer.h"
#include "json_reader.h"

#include <string>
#include <vector>

namespace RcmClientLib
{
    class SourceClusterDiskEntity : public DiskEntity
    {
    public:
        SourceClusterDiskEntity()
        {
            Capacity = 0;
            SectorSize = 0;
            ScsiBus = 0;
            ScsiLogicalUnit = 0;
            ScsiPort = 0;
            ScsiTargetId = 0;
            FreeSpace = 0;
            IsSystemDisk = false;
            IsBootDisk = false;
        }

        SourceClusterDiskEntity(std::string id,
            std::vector<std::string>& deviceNames,
            int64_t  capacity,
            int64_t  sectorSize,
            std::string wcState,
            int32_t  scsiBus,
            int32_t  scsiLu,
            int32_t  scsiPort,
            int32_t  scsiTgt,
            std::string fs,
            int64_t  freeSpace,
            bool systemDisk,
            bool bootDisk,
            std::string mountPoint,
            std::string type,
            std::string partitionScheme,
            std::string dgId,
            std::string diskUuid)
        {
            Id = id;
            DeviceNames = deviceNames;
            Capacity = capacity,
            SectorSize = sectorSize;
            DiskWriteCacheState = wcState;
            ScsiBus = scsiBus;
            ScsiLogicalUnit = scsiLu;
            ScsiPort = scsiPort;
            ScsiTargetId = scsiTgt,
            FileSystem = fs;
            FreeSpace = freeSpace;
            IsSystemDisk = systemDisk;
            IsBootDisk = bootDisk;
            MountPoint = mountPoint;
            DiskType = type;
            DiskPartitioningScheme = partitionScheme; 
            DiskGroupId = dgId;
            DiskUUId = diskUuid;

        }


    };

    typedef boost::shared_ptr<SourceClusterDiskEntity>   ClusterDiskEntityPtr_t;

    class SourceClusterVirtualNodeEntity
    {

    public:

        /// \brief the virtual node Id which represents the shared cluster disks.
        /// This is a generated id by source agent based on cluster id and will be reported same from all the cluster members.
        std::string ClusterVirtualNodeId;

        /// \brief shared disk details of the cluster running over machine.
        std::vector<SourceClusterDiskEntity> DiskInputs;

        /// \brief partition details of a disk of the cluster running over machine.
        std::vector<DiskPartitionEntity> PartitionInputs;

        /// \brief logical volume details of a disk group of the cluster over machine.
        std::vector<LogicalVolumeEntity> LogicalVolumes;

        /// \brief about the disk group details of the cluster running over machine.
        std::vector<DiskGroupEntity> DiskGroups;


        SourceClusterVirtualNodeEntity()
        { }

        /// \brief serialize function for the JSON serilaizer
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "RegisterSourceClusterVirtualNodeInput", false);

            JSON_E(adapter, ClusterVirtualNodeId);
            JSON_E(adapter, DiskInputs);
            JSON_E(adapter, PartitionInputs);
            JSON_E(adapter, LogicalVolumes);
            JSON_T(adapter, DiskGroups);
        }
    };

    class SourceClusterEntity
    {
    public:

        /// \brief a unique id of the cluster
        std::string  ClusterId;

        /// \brief a name of the cluster
        std::string  ClusterName;

        /// \brief a type of the cluster
        std::string  ClusterType;

        /// \brief unique id as defined by the management plane for the cluster
        std::string  ClusterManagementId;

        ///  \brief node name of the current machine performing registration
        std::string  CurrentClusterNodeName;

        /// \brief about the cluster nodes
        std::vector<std::string> ClusterNodeNames;

        /// \brief represt of a virtual node hosting all the shared cluster disks
        SourceClusterVirtualNodeEntity RegisterSourceClusterVirtualNodeInput;

        SourceClusterEntity(std::string clusterId,
            std::string clusterName,
            std::string clusterType,
            std::string clusterManagementId,
            std::string currentClusterNodeName)
            :ClusterId(clusterId),
            ClusterName(clusterName),
            ClusterType(clusterType),
            ClusterManagementId(clusterManagementId),
            CurrentClusterNodeName(currentClusterNodeName)
        {
        }

        SourceClusterEntity() {}

        /// \brief serialize function for the JSON serilaizer
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "RegisterSourceClusterInput", false);

            JSON_E(adapter, ClusterId);
            JSON_E(adapter, ClusterName);
            JSON_E(adapter, ClusterType);
            JSON_E(adapter, ClusterManagementId);
            JSON_E(adapter, CurrentClusterNodeName);
            JSON_E(adapter, ClusterNodeNames);
            JSON_T(adapter, RegisterSourceClusterVirtualNodeInput);
        }
    };

}
#endif
