/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp. 2016
+------------------------------------------------------------------------------------+
File		:	LogicalVolumeEntity.h

Description	:   LogicalVolumeEntity class is defind as per the RCM LogicalVolumeEntity
class. The member names should match that defined in the RCM LogicalVolumeInput class.
Uses the ESJ JSON formatter to serialize and unserialize JSON string.

+------------------------------------------------------------------------------------+
*/
#ifndef _LOGICAL_VOLUME_ENTITY_H
#define _LOGICAL_VOLUME_ENTITY_H


#include "json_writer.h"
#include "json_reader.h"

#include <string>
#include <vector>

#include <boost/shared_ptr.hpp>

namespace RcmClientLib
{
    typedef enum volumePurpose
    {
        /// <summary>
        /// Defines the volume as system volume.
        /// e.g. Windows: system volume(c:\).
        /// </summary>
        SystemVolume = 1,

        /// <summary>
        /// Volume contains boot information.
        /// e.g. /boot.
        /// </summary>
        BootVolume = 2,
    } VolumePurpose;


    /// \brief represents a logical volume
    class LogicalVolumeEntity 
    {
    public:

        /// \brief an identifier to uniquely identify a logical volume in a system
        std::string               Id;

        /// \brief volume label, only on Windows
        std::string               Label;

        /// \brief device names
        std::vector<std::string>  DeviceNames;

        /// \brief volume capacity in bytes
        int64_t                   Capacity;

        /// \brief volume free space in bytes
        int64_t                   FreeSpace;

        /// \brief list of partent disks this logical volume is part of
        /// includes all the disks of the disk group that the LV is part of
        std::vector<std::string>  ParentDisks;

        /// \brief disk group id
        std::string               DiskGroupId;

        /// \brief filesystem type
        std::string               FileSystem;

        /// \brief mount point for the volume if any
        std::string               MountPoint;

        LogicalVolumeEntity()
        {
            Capacity = 0;
            FreeSpace = 0;
        }

        /// \brief serialize function for the JSON serilaizer
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "LogicalVolumeEntity", false);

            JSON_E(adapter, Id);
            JSON_E(adapter, Label);
            JSON_E(adapter, DeviceNames);
            JSON_E(adapter, Capacity);
            JSON_E(adapter, FreeSpace);
            JSON_E(adapter, ParentDisks);
            JSON_E(adapter, DiskGroupId);
            JSON_E(adapter, FileSystem);
            JSON_T(adapter, MountPoint);
        }


        LogicalVolumeEntity(std::string id,
            std::string label,
                        std::vector<std::string>& deviceNames,
                        int64_t capacity,
                        int64_t freespace,
                        std::vector<std::string>& parentDisks,
                        std::string diskGroup,
                        std::string fileSystem,
                        std::string mountPoint)
            :Id(id),
            Label(label),
            DeviceNames(deviceNames.begin(), deviceNames.end()),
            Capacity(capacity),
            FreeSpace(freespace),
            ParentDisks(parentDisks.begin(), parentDisks.end()),
            FileSystem(fileSystem),
            MountPoint(mountPoint),
            DiskGroupId(diskGroup)
        {

        }
    };

    typedef boost::shared_ptr<LogicalVolumeEntity>   LogicalVolumeEntityPtr_t;
    typedef std::vector<LogicalVolumeEntity> LogicalVolumeEntities_t;

}
#endif
