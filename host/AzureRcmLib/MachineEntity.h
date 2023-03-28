/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp. 2016
+------------------------------------------------------------------------------------+
File		:	MachineEntity.h

Description	:   MachineEntity class is defind as per the RCM RegisterMachineInput
class. The member names should match that defined in the RCM RegisterMachineInput class.
Uses the ESJ JSON formatter to serialize and unserialize JSON string.

+------------------------------------------------------------------------------------+
*/
#ifndef _MACHINE_ENTITY_H
#define _MACHINE_ENTITY_H

#include "DiskEntity.h"
#include "DiskGroupEntity.h"
#include "DiskPartitionEntity.h"
#include "LogicalVolumeEntity.h"
#include "NetworkEntity.h"
#include "SourceClusterEntity.h"

#include "json_writer.h"
#include "json_reader.h"

#include <string>
#include <vector>

namespace RcmClientLib
{
    /// \brief represents a physical or virtual machine
    class MachineEntity 
    {
    public:

        /// \brief a unique host id of the machine
        std::string  Id;

        /// \brief FQDN
        std::string  FriendlyName;

        /// \brief BIOS Id
        std::string BiosId;

        /// \brief unique id as defined by the management plane
        std::string  ManagementId;

        /// \brief agent certificate thumbprint
        std::string  CertificateThumbprint;

        /// \brief OS type
        /// valid values - Windows, Linux
        std::string  OperatingSystemType;

        /// \brief OS Name
        /// eg. - RHEL6u3, Windows 2012 R2
        std::string  OsName;

        /// \brief OS description
        /// eg. - Windows 2012 R2 Datacenter
        std::string  OsDescription;

        /// \brief OS architecture
        /// valid values  - x86 , x64
        std::string  OsArchitecture;

        /// \brief OS endianness
        /// valid values - LittleEndian, BigEndian
        std::string  OsEndianness;

        /// \brief OS Major Version
        /// eg. - 6 for RHEL6u3, 2012 for Windows 2012 R2
        std::string  OsMajorVersion;

        /// \brief OS Minor Version
        /// eg. - 3 for RHEL6u3, 2 for Windows 2012 R2
        std::string  OsMinorVersion;

        /// \biref OS Build version
        /// eg. Windows 9200,  Linux - 2.6.32-71.el6.x86_64.
        std::string  OsBuildVersion;

        /// \brief hypervisor type
        /// valid values - Physical, Vmware, HyperV
        std::string  VirtualizationPlatform;

        /// \brief hypervisor version
        std::string  VirtualizationPlatformVersion;

        /// \brief the disk extents for operating system volume
        std::string OsVolumeDiskExtents;

        /// \brief the operating system volume
        std::string OperatingSystemVolumeName;

        /// \brief the operating system folder
        std::string OperatingSystemFolder;

        /// \brief the number of processors
        uint32_t    ProcessorCount;

        /// \brief amount of physical memory in bytes
        uint64_t    MemoryInBytes;

        /// \brief firmware type of the machine for booting - see RcmLibFirmwareTypes
        std::string     FirmwareType;

        /// \brief protection scenario
        std::string ProtectionScenario;

        /// \brief GUID for detecting the failover VM
        std::string FailoverVmDetectionId;

        /// \brief disks that are connected to the system
        std::vector<DiskEntity> DiskInputs;

        /// \brief disk partitioins present on the systems
        std::vector<DiskPartitionEntity> PartitionInputs;

        /// \brief logical volumes present onthe system
        std::vector<LogicalVolumeEntity> LogicalVolumes;

        /// \brief disk groups present on the system
        std::vector<DiskGroupEntity> DiskGroups;

        /// \brief list of network adapter information 
        std::vector<NetworkAdaptor> NetworkAdaptors;

        /// \brief GUID representing the agent resource id
        std::string AgentResourceId;

        /// \brief indicated whether it is credential less discovery or not
        bool IsCredentialLessDiscovery;

        /// \brief about the source failover cluster
        SourceClusterEntity RegisterSourceClusterInput;

        MachineEntity(std::string id,
                    std::string hostname,
                    std::string biosId,
                    std::string managementId,
                    std::string certificateThumbprint,
                    std::string osType,
                    std::string osName,
                    std::string osDesc,
                    std::string osArch,
                    std::string endianness,
                    std::string osMaj,
                    std::string osMin,
                    std::string osBuild,
                    std::string firmwareType,
                    std::string protectionScenario,
                    std::string failoverVmDetectionId,
                    std::string osVolumeDiskExtents,
                    std::string virtPlatform,
                    std::string virtPlatformVer,
                    std::string agentResourceId)
            :Id(id),
            FriendlyName(hostname),
            BiosId(biosId),
            ManagementId(managementId),
            CertificateThumbprint(certificateThumbprint),
            OperatingSystemType(osType),
            OsName(osName),
            OsDescription(osDesc),
            OsArchitecture(osArch),
            OsEndianness(endianness),
            OsMajorVersion(osMaj),
            OsMinorVersion(osMin),
            OsBuildVersion(osBuild),
            FirmwareType(firmwareType),
            ProtectionScenario(protectionScenario),
            FailoverVmDetectionId(failoverVmDetectionId),
            OsVolumeDiskExtents(osVolumeDiskExtents),
            VirtualizationPlatform(virtPlatform),
            VirtualizationPlatformVersion(virtPlatformVer),
            AgentResourceId(agentResourceId)
        {
        }


        /// \brief serialize function for the JSON serilaizer
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "RegisterMachineInput", false);

            JSON_E(adapter, Id);
            JSON_E(adapter, FriendlyName);
            JSON_E(adapter, BiosId);
            JSON_E(adapter, ManagementId);
            JSON_E(adapter, CertificateThumbprint);
            JSON_E(adapter, OperatingSystemType);
            JSON_E(adapter, OsName);
            JSON_E(adapter, OsDescription);
            JSON_E(adapter, OsArchitecture);
            JSON_E(adapter, OsEndianness);
            JSON_E(adapter, OsMajorVersion);
            JSON_E(adapter, OsMinorVersion);
            JSON_E(adapter, OsBuildVersion);
            JSON_E(adapter, OsVolumeDiskExtents);
            JSON_E(adapter, VirtualizationPlatform);
            JSON_E(adapter, VirtualizationPlatformVersion);
            JSON_E(adapter, OperatingSystemVolumeName);
            JSON_E(adapter, OperatingSystemFolder);
            JSON_E(adapter, ProcessorCount);
            JSON_E(adapter, MemoryInBytes);
            JSON_E(adapter, FirmwareType);
            JSON_E(adapter, ProtectionScenario);
            JSON_E(adapter, FailoverVmDetectionId);
            JSON_E(adapter, DiskInputs);
            JSON_E(adapter, PartitionInputs);
            JSON_E(adapter, LogicalVolumes);
            JSON_E(adapter, DiskGroups);
            JSON_E(adapter, NetworkAdaptors);
            JSON_E(adapter, AgentResourceId);
            if (!RegisterSourceClusterInput.ClusterId.empty())
            {
                JSON_E(adapter, IsCredentialLessDiscovery);
                JSON_T(adapter, RegisterSourceClusterInput);
            }
            else
            {
                JSON_T(adapter, IsCredentialLessDiscovery);
            }
        }
    };
}
#endif
