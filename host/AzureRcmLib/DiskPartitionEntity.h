/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp. 2016
+------------------------------------------------------------------------------------+
File		:	DiskPartitionEntity.h

Description	:   DiskPartitionEntity class is defind as per the RCM DiskPartitionInput
class. The member names should match that defined in the RCM DiskPartitionInput class.
Uses the ESJ JSON formatter to serialize and unserialize JSON string.

+------------------------------------------------------------------------------------+
*/

#ifndef _DISK_PARTITION_ENTITY_H
#define _DISK_PARTITION_ENTITY_H

#include "json_writer.h"
#include "json_reader.h"

#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>

namespace RcmClientLib
{
    /// \brief represents a disk partition
    class DiskPartitionEntity 
    {
    public:

        /// \brief an identifier to uniquely identify a partition in a system
        std::string                 Id;

        /// \brief a label for partition
        std::string                 Label;

        /// \brief device names for the partition
        std::vector<std::string>    DeviceNames;

        /// \brief size of partition in bytes
        int64_t                     Capacity;

        /// \brief parent disk Id
        std::string                 ParentDisk;

        /// \brief type of filesystem if any
        std::string                 FileSystem;

        /// \brief mount point if any
        std::string                 MountPoint;
        
        /// \brief disk group Id if any
        std::string                 DiskGroupId;

        DiskPartitionEntity()
        {
            Capacity = 0;
        }

        /// \brief serialize function for the JSON serilaizer
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "DiskPartitionEntity", false);

            JSON_E(adapter,Id);
            JSON_E(adapter, Label);
            JSON_E(adapter,DeviceNames);
            JSON_E(adapter,Capacity);
            JSON_E(adapter,ParentDisk);
            JSON_E(adapter,FileSystem);
            JSON_E(adapter,MountPoint);
            JSON_T(adapter,DiskGroupId);
        }

        DiskPartitionEntity(std::string id,
                        std::string label,
                        std::vector<std::string>& deviceNames,
                        int64_t capacity,
                        std::string parentDisk,
                        std::string fileSystem,
                        std::string mountPoint,
                        std::string diskGroup)
            :Id(id),
            Label(label),
            DeviceNames(deviceNames.begin(), deviceNames.end()),
            Capacity(capacity),
            ParentDisk(parentDisk),
            FileSystem(fileSystem),
            MountPoint(mountPoint),
            DiskGroupId(diskGroup)
        {

        }

    };

    typedef boost::shared_ptr<DiskPartitionEntity>   DiskPartitionEntityPtr_t;
    typedef std::vector<DiskPartitionEntity> DiskPartitionEntities_t;
}

#endif
