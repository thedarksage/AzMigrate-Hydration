/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp. 2016
+------------------------------------------------------------------------------------+
File		:	DiskEntity.h

Description	:   DiskEntity class is defind as per the RCM DiskInput class. The member
names should match that defined in the RCM DiskInput class. Uses the ESJ JSON 
formatter to serialize and unserialize JSON string.

+------------------------------------------------------------------------------------+
*/

#ifndef _DISK_ENTITY_H
#define _DISK_ENTITY_H

#include "json_writer.h"
#include "json_reader.h"

#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>

namespace RcmClientLib
{

    /// \brief a disk entitiy as defined in RCM data contract
    class DiskEntity 
    {
    public:

        /// \brief a unique identifier for the disk in a system
        /// For Source machines :
        /// Windows case:
        ///     MBR : Disk signature.
        ///     GPT : Disk GUID.
        /// Linux case:
        ///     device SCSI Id.
        std::string             Id;

        /// \brief device names
        std::vector<std::string>  DeviceNames;

        /// \brief disk capacity in bytes
        int64_t    Capacity;

        /// \brief disk sector size in bytes
        int64_t    SectorSize;

        /// \brief disk write cache state
        /// valid values - Enabled, Disabled, Unknown
        std::string DiskWriteCacheState;

        /// \brief SCSI bus/host/hba number the disk is connected to
        int32_t    ScsiBus;

        /// \brief SCSI LU number
        int32_t    ScsiLogicalUnit;

        /// \brief SCSI port/channel the disk is connected to 
        int32_t    ScsiPort;

        /// \brief SCSI target Id to which the LU is connected
        int32_t    ScsiTargetId;

        /// \brief filesystem type
        /// eg. - NTFS, ext4
        std::string FileSystem;

        /// \brief free space on the disk
        int64_t    FreeSpace;

        /// \brief indicates if this is a system disk
        /// disk containing OS files, page file, cache dir, installation dir
        /// i.e. any disk that cannot be excluded from protection
        bool IsSystemDisk;

        /// \brief indicates if this is a system disk
        /// active partition on Windows and /boot on Linux
        bool IsBootDisk;

        /// \brief mount point if any
        std::string  MountPoint;

        /// \brief disk type
        /// valid values - Basic, Dynamic
        std::string  DiskType;

        /// \brief disk partitioning scheme
        /// valid values are "Smi", "Efi", "Mbr", "Gpt", "Raw"
        std::string  DiskPartitioningScheme;

        /// \brief disk group id if any
        std::string  DiskGroupId;

        /// \brief SCSI disk UUID from Page 83
        std::string     DiskUUId;

        DiskEntity()
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

        DiskEntity(std::string id,
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
           : Id(id),
           DeviceNames(deviceNames.begin(), deviceNames.end()),
           Capacity(capacity),
           SectorSize(sectorSize),
           DiskWriteCacheState(wcState),
           ScsiBus(scsiBus),
           ScsiLogicalUnit(scsiLu),
           ScsiPort(scsiPort),
           ScsiTargetId(scsiTgt),
           FileSystem(fs),
           FreeSpace(freeSpace),
           IsSystemDisk(systemDisk),
           IsBootDisk(bootDisk),
           MountPoint(mountPoint),
           DiskType(type),
           DiskPartitioningScheme(partitionScheme),
           DiskGroupId(dgId),
           DiskUUId(diskUuid)
        {
        }


        /// \brief a serialize function for JSON serializer 
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "DiskEntity", false);

            JSON_E(adapter,Id);
            JSON_E(adapter,DeviceNames);
            JSON_E(adapter,Capacity);
            JSON_E(adapter,SectorSize);
            JSON_E(adapter,DiskWriteCacheState);
            JSON_E(adapter,ScsiBus);
            JSON_E(adapter,ScsiLogicalUnit);
            JSON_E(adapter,ScsiPort);
            JSON_E(adapter,ScsiTargetId);
            JSON_E(adapter,FileSystem);
            JSON_E(adapter,FreeSpace);
            JSON_E(adapter, IsSystemDisk);
            JSON_E(adapter, IsBootDisk);
            JSON_E(adapter,MountPoint);
            JSON_E(adapter,DiskType);
            JSON_E(adapter,DiskPartitioningScheme);
            JSON_E(adapter,DiskGroupId);
            JSON_T(adapter, DiskUUId);
        }

    };

    typedef boost::shared_ptr<DiskEntity>   DiskEntityPtr_t;
    typedef std::vector<DiskEntity> DiskEntities_t;

}
#endif
