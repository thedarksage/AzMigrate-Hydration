/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp. All rights reserved.
+------------------------------------------------------------------------------------+
File        :    RcmContracts.h

Description    :   Contains definitions of correspondinf RCM contracts

+------------------------------------------------------------------------------------+
*/

#ifndef _RCM_CONTRACTS_H
#define _RCM_CONTRACTS_H

#include "json_writer.h"
#include "json_reader.h"

#include <string>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>

#include "RcmJobs.h"
#include "RcmProxyDetails.h"
#include "AgentAutoUpgrade.h"
#include "RcmLibConstants.h"
#include "ServerAuthDetails.h"
#include "CommonRcmContracts.h"

using boost::property_tree::ptree;

namespace RcmClientLib {

    /// \brief data path transport type for Azure blobs
    const char TRANSPORT_TYPE_AZURE_BLOB[] = "AzureBlobStorageBasedTransport";

    /// \brief data path transport type for Process Server 
    const char TRANSPORT_TYPE_PROCESS_SERVER[] = "ProcessServerBasedTransport";

    /// \brief data path transport type for Local File Transfer
    const char TRANSPORT_TYPE_LOCAL_FILE[] = "LocalFileTransport";

    /// \brief control path type for Rcm Proxy
    const char CONTROL_PATH_TYPE_RCM_PROXY[] = "RcmProxyControlPlane";

    const std::string CONTROL_PATH_TYPE_RCM = "RcmControlPlane";

    /// \brief sync type for zero copy resync
    const std::string SYNC_TYPE_NO_DATA_TRANSFER = "NoDataTranferSync";

    /// \brief Azure page blob
    const std::string AZURE_BLOB_TYPE_PAGE = "Page";

    /// \brief Azure block blob
    const std::string AZURE_BLOB_TYPE_BLOK = "Block";

    /// \brief RCM action as defined in agent contract
    class RcmProtectionPairAction {
    public:
        std::string ConsumerId;

        std::string ComponentId;

        std::string ActionType;

        /// \brief serialized string of input data
        std::string InputPayload;

        /// \brief a serialize function for JSON serializer 
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "RcmProtectionPairAction", false);

            JSON_E(adapter, ConsumerId);
            JSON_E(adapter, ComponentId);
            JSON_E(adapter, ActionType);
            JSON_T(adapter, InputPayload);
        }

        /// \brief a serialize function that can handle members in any order
        void serialize(ptree& node)
        {
            JSON_P(node, ConsumerId);
            JSON_P(node, ComponentId);
            JSON_P(node, ActionType);
            JSON_P(node, InputPayload);
            return;
        }
    };

    /// \brief RCM action as defined in agent contract
    class RcmReplicationPairAction {
    public:
        std::string ConsumerId;

        std::string ComponentId;

        std::string DiskId;

        std::string ActionType;

        /// \brief serialized string of input data
        std::string InputPayload;

        /// \brief a serialize function for JSON serializer 
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "RcmAction", false);

            JSON_E(adapter, ConsumerId);
            JSON_E(adapter, ComponentId);
            JSON_E(adapter, DiskId);
            JSON_E(adapter, ActionType);
            JSON_T(adapter, InputPayload);
        }

        /// \brief a serialize function that can handle members in any order
        void serialize(ptree& node)
        {
            JSON_P(node, ConsumerId);
            JSON_P(node, ComponentId);
            JSON_P(node, DiskId);
            JSON_P(node, ActionType);
            JSON_P(node, InputPayload);
            return;
        }
    };

    /// \brief replication settings for agent to consume
    /// serialized data of this class will be cached/persisted by agent
    class AgentMonitoringMsgSettings
    {
    public:

        /// \brief SAS URI of the Critical message event hub
        std::string CriticalEventHubUri;

        /// \brief SAS URI of the Information message event hub
        std::string InformationEventHubUri;

        /// \brief SAS URI of the Logging event hub
        std::string LoggingEventHubUri;

        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "AgentMonitoringMsgSettings", false);

            JSON_E(adapter, CriticalEventHubUri);
            JSON_E(adapter, InformationEventHubUri);
            JSON_T(adapter, LoggingEventHubUri);
        }

        void serialize(ptree& node)
        {
            JSON_P(node, CriticalEventHubUri);
            JSON_P(node, InformationEventHubUri);
            JSON_P(node, LoggingEventHubUri);
        }
    };

    /// \biref replication settings for source agent for A2A
    class AgentReplicationSettings
    {
    public:

        /// \brief indicating whether it is a cluster member
        bool IsClusterMember;

        /// \brief indicating whether this is cluster virtual node hosting the
        bool IsClusterVirtualNode;

        /// \brief a list of RCM jobs
        std::vector<RcmJob>     RcmJobs;

        /// \brief a list of protection pair (machine) actions
        std::vector<RcmProtectionPairAction>  RcmProtectionPairActions;

        /// \brief a list of replication pair (disk) actions
        std::vector<RcmReplicationPairAction>  RcmReplicationPairActions;

        /// \brief message settings to be included in each critical/info message 
        /// and agent logs to be sent to RCM from source agent
        AgentMonitoringMsgSettings VmMonitoringMsgSettings;

        /// \brief message settings to be included in each critical/info message 
        /// and agent logs to be sent to RCM from source agent
        std::string MonitoringMsgSettings;

        std::string ShoeboxEventSettings;

        /// \brief to be used when sending events at VM level
        std::string ProtectionPairContext;

        /// \brief to be used when sending events at Cluster level
        std::string ClusterProtectionMessageContext;

        /// \brief to be used when sending events at disk level
        std::map<std::string, std::string> ReplicationPairMessageContexts;

        /// \brief flag indicated initial replication is done for migration
        bool IsInitialReplicationComplete;

        std::vector<std::string> ProtectedDiskIds;

        AgentReplicationSettings() {
            IsClusterMember = false;
            IsClusterVirtualNode = false;
            IsInitialReplicationComplete = false;
        }

        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "AgentReplicationSettings", false);

            JSON_E(adapter, IsClusterMember);
            JSON_E(adapter, IsClusterVirtualNode);
            JSON_E(adapter, RcmJobs);
            JSON_E(adapter, RcmProtectionPairActions);
            JSON_E(adapter, RcmReplicationPairActions);
            JSON_E(adapter, VmMonitoringMsgSettings);
            JSON_E(adapter, MonitoringMsgSettings);
            JSON_E(adapter, ShoeboxEventSettings);
            JSON_E(adapter, ProtectionPairContext);
            JSON_E(adapter, ClusterProtectionMessageContext);
            JSON_KV_E(adapter, "ReplicationPairMessageContexts", ReplicationPairMessageContexts);
            JSON_E(adapter, IsInitialReplicationComplete);
            JSON_T(adapter, ProtectedDiskIds);
        }

        void serialize(ptree& node)
        {
            JSON_P(node, IsClusterMember);
            JSON_P(node, IsClusterVirtualNode);
            JSON_VCL(node, RcmJobs);
            JSON_VCL(node, RcmProtectionPairActions);
            JSON_VCL(node, RcmReplicationPairActions);
            JSON_CL(node, VmMonitoringMsgSettings);
            JSON_P(node, MonitoringMsgSettings);
            JSON_P(node, ShoeboxEventSettings);
            JSON_P(node, ProtectionPairContext);
            JSON_P(node, ClusterProtectionMessageContext);
            JSON_KV_P(node, ReplicationPairMessageContexts);
            JSON_P(node, IsInitialReplicationComplete);
            JSON_VP(node, ProtectedDiskIds);
        }
    };

    /// \brief telemetry path settings for protected source machine
    class ProtectedMachineTelemetrySettings {
    public:

        /// \brief common telemetry path on appliance for all components of source agent
        std::string TelemetryFolder;

        bool operator==(const ProtectedMachineTelemetrySettings& rhs) const
        {
            return boost::iequals(TelemetryFolder, rhs.TelemetryFolder);
        }

        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "ProtectedMachineTelemetrySettings", false);

            JSON_T(adapter, TelemetryFolder);
        }

        void serialize(ptree& node)
        {
            JSON_P(node, TelemetryFolder);
        }
    };

    /// \brief OnPremToAzureSourceAgentReplicationSettings
    class OnPremToAzureSourceAgentReplicationSettings : public AgentReplicationSettings {
    
    public:
    
        std::string Version;

        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "OnPremToAzureSourceAgentReplicationSettings", false);

            JSON_E(adapter, Version);
            JSON_E(adapter, RcmJobs);
            JSON_E(adapter, RcmProtectionPairActions);
            JSON_E(adapter, RcmReplicationPairActions);
            JSON_E(adapter, VmMonitoringMsgSettings);
            JSON_E(adapter, MonitoringMsgSettings);
            JSON_E(adapter, ShoeboxEventSettings);
            JSON_E(adapter, ProtectionPairContext);
            JSON_KV_E(adapter, "ReplicationPairMessageContexts", ReplicationPairMessageContexts);
            JSON_E(adapter, IsInitialReplicationComplete);
            JSON_T(adapter, ProtectedDiskIds);
        }

        void serialize(ptree& node)
        {
            JSON_P(node, Version);
            JSON_VCL(node, RcmJobs);
            JSON_VCL(node, RcmProtectionPairActions);
            JSON_VCL(node, RcmReplicationPairActions);
            JSON_CL(node, VmMonitoringMsgSettings);
            JSON_P(node, MonitoringMsgSettings);
            JSON_P(node, ShoeboxEventSettings);
            JSON_P(node, ProtectionPairContext);            
            JSON_KV_P(node, ReplicationPairMessageContexts);
            JSON_P(node, IsInitialReplicationComplete);
            JSON_VP(node, ProtectedDiskIds);
        }

    };

    /// \biref settings for OnPremToAzure source agent
    class OnPremToAzureSourceAgentSettings {
    public:

        OnPremToAzureSourceAgentSettings()
            :IsAgentUpgradeable(false) {}

        std::map<std::string, std::string> Tunables;

        std::string DataPathTransportType;

        std::string DataPathTransportSettings;

        std::string ControlPathTransportType;

        std::string ControlPathTransportSettings;

        ProtectedMachineTelemetrySettings TelemetrySettings;

        OnPremToAzureSourceAgentReplicationSettings ReplicationSettings;

        bool IsAgentUpgradeable;

        MobilityAgentAutoUpgradeSchedule AutoUpgradeSchedule;

        TimeWindow AutoResyncTimeWindow;

        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "OnPremToAzureSourceAgentSettings", false);

            JSON_KV_E(adapter, "Tunables", Tunables);
            JSON_E(adapter, DataPathTransportType);
            JSON_E(adapter, DataPathTransportSettings);
            JSON_E(adapter, ControlPathTransportType);
            JSON_E(adapter, ControlPathTransportSettings);
            JSON_E(adapter, TelemetrySettings);
            JSON_E(adapter, ReplicationSettings);
            JSON_E(adapter, IsAgentUpgradeable);
            JSON_E(adapter, AutoUpgradeSchedule);
            JSON_T(adapter, AutoResyncTimeWindow);
        }

        void serialize(ptree& node)
        {
            JSON_KV_P(node, Tunables);
            JSON_P(node, DataPathTransportType);
            JSON_P(node, DataPathTransportSettings);
            JSON_P(node, ControlPathTransportType);
            JSON_P(node, ControlPathTransportSettings);
            JSON_CL(node, TelemetrySettings);
            JSON_CL(node, ReplicationSettings);
            JSON_P(node, IsAgentUpgradeable);
            JSON_CL(node, AutoUpgradeSchedule);
            JSON_CL(node, AutoResyncTimeWindow);
        }
    };

    /// \brief AzureToOnPremTelemetrySettings
    class AzureToOnPremTelemetrySettings {

    public:
        std::string     KustoEventHubUri;

        std::string     SasUriRenewalTimeUtc;

        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "AzureToOnPremTelemetrySettings", false);

            JSON_E(adapter, KustoEventHubUri);
            JSON_T(adapter, SasUriRenewalTimeUtc);
        }

        void serialize(ptree& node)
        {
            JSON_P(node, KustoEventHubUri);
            JSON_P(node, SasUriRenewalTimeUtc);
        }
    };

    /// \brief SourceAgentAzureToOnPremReplicationSettings
    class SourceAgentAzureToOnPremReplicationSettings : public AgentReplicationSettings {
    
    public:

        std::string Version;

        AzureToOnPremTelemetrySettings TelemetrySettings;

        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "SourceAgentAzureToOnPremReplicationSettings", false);

            JSON_E(adapter, Version);
            JSON_E(adapter, TelemetrySettings);
            JSON_E(adapter, RcmJobs);
            JSON_E(adapter, RcmProtectionPairActions);
            JSON_E(adapter, RcmReplicationPairActions);
            JSON_E(adapter, VmMonitoringMsgSettings);
            JSON_E(adapter, MonitoringMsgSettings);
            JSON_E(adapter, ShoeboxEventSettings);
            JSON_E(adapter, ProtectionPairContext);
            JSON_KV_E(adapter, "ReplicationPairMessageContexts", ReplicationPairMessageContexts);
            JSON_T(adapter, ProtectedDiskIds);
        }

        void serialize(ptree& node)
        {
            JSON_P(node, Version);
            JSON_CL(node, TelemetrySettings);
            JSON_VCL(node, RcmJobs);
            JSON_VCL(node, RcmProtectionPairActions);
            JSON_VCL(node, RcmReplicationPairActions);
            JSON_CL(node, VmMonitoringMsgSettings);
            JSON_P(node, MonitoringMsgSettings);
            JSON_P(node, ShoeboxEventSettings);
            JSON_P(node, ProtectionPairContext);
            JSON_KV_P(node, ReplicationPairMessageContexts);
            JSON_VP(node, ProtectedDiskIds);
        }
    };

    /// \biref settings for AzureToOnPrem source agent
    class AzureToOnPremSourceAgentSettings {
    public:

        AzureToOnPremSourceAgentSettings()
            :IsAgentUpgradeable(false),
            IsPrivateEndpointEnabled(false)
        {}

        std::map<std::string, std::string> Tunables;

        SourceAgentAzureToOnPremReplicationSettings ReplicationSettings;

        bool IsAgentUpgradeable;

        MobilityAgentAutoUpgradeSchedule AutoUpgradeSchedule;

        TimeWindow AutoResyncTimeWindow;

        RcmProxyTransportSetting RcmProxyTransportSettings;

        bool IsPrivateEndpointEnabled;

        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "AzureToOnPremSourceAgentSettings", false);

            JSON_KV_E(adapter, "Tunables", Tunables);
            JSON_E(adapter, ReplicationSettings);
            JSON_E(adapter, IsAgentUpgradeable);
            JSON_E(adapter, RcmProxyTransportSettings);
            JSON_E(adapter, AutoUpgradeSchedule);
            JSON_E(adapter, AutoResyncTimeWindow);
            JSON_T(adapter, IsPrivateEndpointEnabled);
        }

        void serialize(ptree& node)
        {
            JSON_KV_P(node, Tunables);
            JSON_CL(node, ReplicationSettings);
            JSON_P(node, IsAgentUpgradeable);
            JSON_CL(node, RcmProxyTransportSettings);
            JSON_CL(node, AutoUpgradeSchedule);
            JSON_CL(node, AutoResyncTimeWindow);
            JSON_P(node, IsPrivateEndpointEnabled);
        }
    };

    /// \brief transport settings for blob transport type
    class AzureBlobBasedTransportSettings {
    public:
        AzureBlobBasedTransportSettings() : SasUriExpirationTime(0){}

        /// \brief SAS URI of the Azure blob container to which
        /// the diff files will be uploaded
        std::string BlobContainerSasUri;

        /// \brief SAS URI expiration time
        uint64_t    SasUriExpirationTime;

        /// \brief Azure blob type
        std::string LogStorageBlobType;

        /// \brief a serializer for JSON
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "AzureBlobBasedTransportSettings", false);

            JSON_E(adapter, BlobContainerSasUri);
            JSON_E(adapter, SasUriExpirationTime);
            JSON_T(adapter, LogStorageBlobType);
        }

        /// \brief serializer method with ptree as input
        void serialize(ptree& node)
        {
            JSON_P(node, BlobContainerSasUri);
            JSON_P(node, SasUriExpirationTime);
            JSON_P(node, LogStorageBlobType);
        }

        bool operator==(const AzureBlobBasedTransportSettings& rhs) const
        {
            return boost::iequals(BlobContainerSasUri, rhs.BlobContainerSasUri) &&
                (SasUriExpirationTime == rhs.SasUriExpirationTime) &&
                boost::iequals(LogStorageBlobType, rhs.LogStorageBlobType);
        }
    };

    class ProcessServerTransportSettings {
    public:
        /// \brief Initializes a new instance of the class.
        ProcessServerTransportSettings() : Port(0) { }

        /// \brief machine ID of the process server.
        std::string Id;

        /// \brief the Friendly name of the process server.
        std::string FriendlyName;

        /// \brief the version of the process server.
        std::string AgentVersion;

        /// \brief IP address for communication with process server
        std::vector<std::string> IpAddresses;

        /// \brief Nat IP address for communication with process server
        std::string NatIpAddress;

        /// \brief the port to be used for communicating with the process server.
        uint64_t Port;

        /// \brief auth details to be used for connecting with process server
        ProcessServerAuthDetail    ProcessServerAuthDetails;

        /// \brief a serializer for JSON
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "ProcessServerTransportSettings", false);

            JSON_E(adapter, Id);
            JSON_E(adapter, FriendlyName);
            JSON_E(adapter, AgentVersion);
            JSON_E(adapter, IpAddresses);
            JSON_E(adapter, NatIpAddress);
            JSON_E(adapter, Port);
            JSON_T(adapter, ProcessServerAuthDetails);
        }

        /// \brief serializer method with ptree as input
        void serialize(ptree &node)
        {
            JSON_P(node, Id);
            JSON_P(node, FriendlyName);
            JSON_P(node, AgentVersion);
            JSON_VP(node, IpAddresses);
            JSON_P(node, NatIpAddress);
            JSON_P(node, Port);
            JSON_CL(node, ProcessServerAuthDetails);
        }

        bool operator==(const ProcessServerTransportSettings& rhs) const
        {
            return boost::iequals(Id, rhs.Id) &&
                boost::iequals(FriendlyName, rhs.FriendlyName) &&
                boost::iequals(AgentVersion, rhs.AgentVersion) &&
                (IpAddresses == rhs.IpAddresses) &&
                boost::iequals(NatIpAddress, rhs.NatIpAddress) &&
                (Port == rhs.Port) &&
                (ProcessServerAuthDetails == rhs.ProcessServerAuthDetails);
        }

        std::string GetIPAddress() const;
    };

    /// \brief properties for a machine participating in multi VM consistency protocol
    class MultiVmConsistencyParticipatingNode {
    public:

        /// \brief machine identity
        std::string Id;

        /// \brief fully qualified domain name
        std::string Name;

        /// \brief certificate thumbprint used for server side validation
        std::string CertificateThumbprint;

        /// \brief serializer method for JSON
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "MultiVmConsistencyParticipatingNode", false);

            JSON_E(adapter, Id);
            JSON_E(adapter, Name);
            JSON_T(adapter, CertificateThumbprint);
        }

        void serialize(ptree& node)
        {
            JSON_P(node, Id);
            JSON_P(node, Name);
            JSON_P(node, CertificateThumbprint);
        }

        bool operator==(const MultiVmConsistencyParticipatingNode& rhs) const
        {
            return boost::iequals(Id, rhs.Id);
        }
    };

    /// \brief consistency settings as received in source agent settings from RCM
    class ConsistencySettings {
    public:
        ConsistencySettings()
            :AppConsistencyInterval(0),
            CrashConsistencyInterval(0)
        {}

        /// \brief all nodes that are participating in multi-vm consistency
        /// for single vm consistency, only one node wll be present.
        std::vector<std::string> ParticipatingMachines;

        /// \brief all nodes that are participating in multi-vm consistency
        /// for single vm consistency, only one node wll be present.
        std::vector<MultiVmConsistencyParticipatingNode> MultiVmConsistencyParticipatingNodes;

        /// \brief the FQDN of the coordinator/master node in multi-vm consistency
        /// null in case of single vm consistency
        std::string CoordinatorMachine;

        /// \brief Application consistency bookmark interval in 100 nano sec granularity
        uint64_t AppConsistencyInterval;

        /// \brief crash consistency bookmark interval in 100 nano sec granularity
        uint64_t CrashConsistencyInterval;

        /// \brief a serializer method for JSON
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "ConsistencySettings", false);

            JSON_E(adapter, ParticipatingMachines);
            JSON_E(adapter, MultiVmConsistencyParticipatingNodes);
            JSON_E(adapter, CoordinatorMachine);
            JSON_E(adapter, AppConsistencyInterval);
            JSON_T(adapter, CrashConsistencyInterval);
        }

        void serialize(ptree& node)
        {
            JSON_VP(node, ParticipatingMachines);
            JSON_VCL(node, MultiVmConsistencyParticipatingNodes);
            JSON_P(node, CoordinatorMachine);
            JSON_P(node, AppConsistencyInterval);
            JSON_P(node, CrashConsistencyInterval);
        }

        bool operator==(const ConsistencySettings& rhs) const
        {
            return ((ParticipatingMachines == rhs.ParticipatingMachines) &&
                (MultiVmConsistencyParticipatingNodes == rhs.MultiVmConsistencyParticipatingNodes) &&
                boost::iequals(CoordinatorMachine, rhs.CoordinatorMachine) &&
                (AppConsistencyInterval == rhs.AppConsistencyInterval) &&
                (CrashConsistencyInterval == rhs.CrashConsistencyInterval));
        }

        void clear()
        {
            CoordinatorMachine.clear();
            AppConsistencyInterval = 0;
            CrashConsistencyInterval = 0;
            ParticipatingMachines.clear();
        }
    };


    /// \brief drain log settings per each disk that is being replicated
    /// part of the source agent settings received from RCM
    class AzureToAzureDrainLogSettings {
    public:

        /// \brief the transport type to use for draining diff data logs
        std::string DataPathTransportType;

        /// \brief serialized string of transport settings based on transport type
        std::string DataPathTransportSettings;

        /// \brief serializer method for JSON
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "AzureToAzureDrainLogSettings", false);

            JSON_E(adapter, DataPathTransportType);
            JSON_T(adapter, DataPathTransportSettings);
        }

        /// \brief serialize 
        void serialize(ptree &node)
        {
            JSON_P(node, DataPathTransportType);
            JSON_P(node, DataPathTransportSettings);
        }

        bool operator==(const AzureToAzureDrainLogSettings& rhs) const
        {
            return boost::iequals(DataPathTransportType, rhs.DataPathTransportType) &&
                boost::iequals(DataPathTransportSettings, rhs.DataPathTransportSettings);
        }

        bool operator!=(const AzureToAzureDrainLogSettings& rhs) const
        {
            return !operator==(rhs);
        }
    };


    /// \brief drain log settings per each disk that is being replicated
    /// part of the source agent settings received from RCM
    class OnPremToAzureDrainLogSettings {
    public:

        std::string LogFolder;

        /// \brief serializer method for JSON
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "OnPremToAzureDrainLogSettings", false);

            JSON_T(adapter, LogFolder);
        }

        /// \brief serialize 
        void serialize(ptree &node)
        {
            JSON_P(node, LogFolder);
        }

        bool operator==(const OnPremToAzureDrainLogSettings& rhs) const
        {
            return boost::iequals(LogFolder, rhs.LogFolder);
        }

        bool operator!=(const OnPremToAzureDrainLogSettings& rhs) const
        {
            return !operator==(rhs);
        }
    };

    /// \brief drain log settings for AzureToOnPrem
    class AzureToOnPremDrainLogSettings {

    public:
        std::string     ReplicationPairId;

        std::string     ReplicationSessionId;

        AzureBlobBasedTransportSettings     TransportSettings;

        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "AzureToOnPremDrainLogSettings", false);

            JSON_E(adapter, ReplicationPairId);
            JSON_E(adapter, ReplicationSessionId);
            JSON_T(adapter, TransportSettings);
        }

        void serialize(ptree& node)
        {
            JSON_P(node, ReplicationPairId);
            JSON_P(node, ReplicationSessionId);
            JSON_CL(node, TransportSettings);
        }
    };

    class ShoeboxEventSettings {
    public:

        /// \brief vault resource ARM id
        std::string VaultResourceArmId;

        /// \brief  mapping for disk identifier to the corresponding disk name visible in the portal.
        std::map<std::string, std::string> DiskIdToDiskNameMap;

        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "ShoeboxEventSettings", false);

            JSON_E(adapter, VaultResourceArmId);
            JSON_KV_T(adapter, "DiskIdToDiskNameMap", DiskIdToDiskNameMap);
        }

        void serialize(ptree& node)
        {
            JSON_P(node, VaultResourceArmId);
            JSON_KV_P(node, DiskIdToDiskNameMap);
        }
    };

    /// \brief Service Principal details
    class SpnDetails {
    public:

        std::string TenantId;

        std::string ApplicationId;

        std::string ObjectId;

        std::string Audience;

        std::string AadAuthority;

        std::string CertificateThumbprint;

        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "SpnDetails", false);

            JSON_E(adapter, TenantId);
            JSON_E(adapter, ApplicationId);
            JSON_E(adapter, ObjectId);
            JSON_E(adapter, Audience);
            JSON_E(adapter, AadAuthority);
            JSON_T(adapter, CertificateThumbprint);
        }

        void serialize(ptree& node)
        {
            JSON_P(node, TenantId);
            JSON_P(node, ApplicationId);
            JSON_P(node, ObjectId);
            JSON_P(node, Audience);
            JSON_P(node, AadAuthority);
            JSON_P(node, CertificateThumbprint);
        }

        bool operator==(const SpnDetails& rhs) const
        {
            return boost::iequals(TenantId, rhs.TenantId) &&
                boost::iequals(ApplicationId, rhs.ApplicationId) &&
                boost::iequals(ObjectId, rhs.ObjectId) &&
                boost::iequals(Audience, rhs.Audience) &&
                boost::iequals(AadAuthority, rhs.AadAuthority) &&
                boost::iequals(CertificateThumbprint, rhs.CertificateThumbprint);
        }

        bool empty() const
        {
            return TenantId.empty() ||
                ApplicationId.empty() ||
                ObjectId.empty() ||
                Audience.empty() ||
                AadAuthority.empty() ||
                CertificateThumbprint.empty();
        }

        const char* c_str() const
        {
            std::stringstream ss;
            ss << MAKE_NAME_VALUE_STR(TenantId)
                << MAKE_NAME_VALUE_STR(ApplicationId)
                << MAKE_NAME_VALUE_STR(ObjectId)
                << MAKE_NAME_VALUE_STR(Audience)
                << MAKE_NAME_VALUE_STR(AadAuthority)
                << MAKE_NAME_VALUE_STR(CertificateThumbprint);

            m_string = ss.str();
            return m_string.c_str();
        }
    private:
        mutable std::string     m_string;
    };

    class RcmRelaySettings
    {
    public:
        RcmRelaySettings()
            :ExpiryTimeoutInSeconds(60) {}

        std::string RelayServiceUri;

        std::string RelayKeyPolicyName;

        std::string RelaySharedKey;

        std::string RelayServicePathSuffix;

        int ExpiryTimeoutInSeconds;

        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "RcmRelaySettings", false);

            JSON_E(adapter, RelayServiceUri);
            JSON_E(adapter, RelayKeyPolicyName);
            JSON_E(adapter, RelaySharedKey);
            JSON_E(adapter, RelayServicePathSuffix);
            JSON_T(adapter, ExpiryTimeoutInSeconds);
        }

        void serialize(ptree& node)
        {
            JSON_P(node, RelayServiceUri);
            JSON_P(node, RelayKeyPolicyName);
            JSON_P(node, RelaySharedKey);
            JSON_P(node, RelayServicePathSuffix);
            JSON_P(node, ExpiryTimeoutInSeconds);
        }

        bool operator==(const RcmRelaySettings& rhs) const
        {
            return boost::iequals(RelayServiceUri, rhs.RelayServiceUri) &&
                boost::iequals(RelayKeyPolicyName, rhs.RelayKeyPolicyName) &&
                boost::iequals(RelaySharedKey, rhs.RelaySharedKey) &&
                boost::iequals(RelayServicePathSuffix, rhs.RelayServicePathSuffix) &&
                (ExpiryTimeoutInSeconds == rhs.ExpiryTimeoutInSeconds);
        }

        bool empty() const
        {
            return RelayServiceUri.empty() ||
                RelayKeyPolicyName.empty() ||
                RelaySharedKey.empty() ||
                RelayServicePathSuffix.empty();
        }

        const char* c_str() const
        {
            std::stringstream ss;
            ss << MAKE_NAME_VALUE_STR(RelayServiceUri)
                << MAKE_NAME_VALUE_STR(RelayKeyPolicyName)
                << MAKE_NAME_VALUE_STR(RelaySharedKey)
                << MAKE_NAME_VALUE_STR(RelayServicePathSuffix)
                << MAKE_NAME_VALUE_STR(ExpiryTimeoutInSeconds);

            m_string = ss.str();
            return m_string.c_str();
        }
    private:
        mutable std::string     m_string;
    };

    /// \brief RCM details for Reprotect
    class RcmDetails {
    public:

        std::string RcmServiceUrl;

        SpnDetails SpnIdentity;

        std::string Certificate;

        /// \brief EndPointType Public or Private
        std::string EndPointType;

        std::string ServiceConnectionMode;

        RcmRelaySettings RelaySettings;

        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "RcmDetails", false);

            JSON_E(adapter, RcmServiceUrl);
            JSON_E(adapter, SpnIdentity);
            JSON_E(adapter, Certificate);
            JSON_E(adapter, EndPointType);
            JSON_E(adapter, ServiceConnectionMode);
            JSON_T(adapter, RelaySettings);

            // for backward compatibility where RcmProxy doesn't support ServiceConnectionMode
            if (ServiceConnectionMode.empty())
                ServiceConnectionMode = ServiceConnectionModes::DIRECT;

            if (EndPointType.empty())
                EndPointType = NetworkEndPointType::PUBLIC_ENDPOINT;
        }

        void serialize(ptree& node)
        {
            JSON_P(node, RcmServiceUrl);
            JSON_CL(node, SpnIdentity);
            JSON_P(node, Certificate);
            JSON_P(node, EndPointType);
            JSON_P(node, ServiceConnectionMode);
            JSON_CL(node, RelaySettings);

            // for backward compatibility where RcmProxy doesn't support ServiceConnectionMode
            if (ServiceConnectionMode.empty())
                ServiceConnectionMode = ServiceConnectionModes::DIRECT;

            if (EndPointType.empty())
                EndPointType = NetworkEndPointType::PUBLIC_ENDPOINT;
        }

        bool operator==(const RcmDetails& rhs) const
        {
            // check only the cert thumbprint but not the certificate
            return boost::iequals(RcmServiceUrl, rhs.RcmServiceUrl) &&
                boost::iequals(EndPointType, rhs.EndPointType) &&
                boost::iequals(ServiceConnectionMode, rhs.ServiceConnectionMode) &&
                (RelaySettings == rhs.RelaySettings) &&
                (SpnIdentity == rhs.SpnIdentity);
        }

        bool operator!=(const RcmDetails& rhs) const
        {
            return !operator==(rhs);
        }

        const char* c_str() const
        {
            std::stringstream ss;
            ss << MAKE_NAME_VALUE_STR(RcmServiceUrl)
                << "Certificate : " << (Certificate.empty() ? "empty," : "not-empty redacted")
                << MAKE_NAME_VALUE_STR(ServiceConnectionMode)
                << MAKE_NAME_VALUE_STR(EndPointType)
                << RelaySettings.c_str()
                << SpnIdentity.c_str();

            m_string = ss.str();
            return m_string.c_str();
        }
    private:
        mutable std::string     m_string;
    };

    class AzureToAzureSourceAgentReplicationSettings {
    public:

        std::map<std::string, AgentReplicationSettings> ReplicationSettings;

        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "AzureToAzureSourceAgentReplicationSettings", false);
          
            JSON_KV_T(adapter, "ReplicationSettings", ReplicationSettings);
        }

        void serialize(ptree& node)
        {
            JSON_KV_CL(node, ReplicationSettings);
        
        }
    };
}
#endif // _RCM_CONTRACTS_H
