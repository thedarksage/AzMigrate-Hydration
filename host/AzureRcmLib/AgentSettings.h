/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp. 2016
+------------------------------------------------------------------------------------+
File        :   AgentSettings.h

Description :   contains the agent settings for the RCM jobs and actions as defined
in the RCM - source agent contract

+------------------------------------------------------------------------------------+
*/

#ifndef _AGENT_SETTINGS_H
#define _AGENT_SETTINGS_H

#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>

#include "svstatus.h"
#include "json_reader.h"
#include "json_writer.h"

#include "RcmContracts.h"
#include "SyncSettings.h"

#include <fstream>
#include <streambuf>
#include <list>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

namespace RcmClientLib {

    /// \brief drain settings per each disk that is being replicated
    class DrainSettings : public AzureToAzureDrainLogSettings,
        public OnPremToAzureDrainLogSettings {
    public:

        /// \brief disk ID is signature in Windows and SCSI-ID or device name in Linux
        std::string DiskId;

        /// \brief serializer method for JSON
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "DrainSettings", false);

            JSON_E(adapter, DiskId);
            JSON_E(adapter, DataPathTransportType);
            JSON_E(adapter, DataPathTransportSettings);
            JSON_T(adapter, LogFolder);
        }

        void serialize(ptree& node)
        {
            JSON_P(node, DiskId);
            JSON_P(node, DataPathTransportType);
            JSON_P(node, DataPathTransportSettings);
            JSON_P(node, LogFolder);
        }

        bool operator==(const DrainSettings& rhs) const
        {
            return AzureToAzureDrainLogSettings::operator==(rhs) &&
                OnPremToAzureDrainLogSettings::operator==(rhs) &&
                boost::iequals(DiskId, rhs.DiskId);
        }

        bool operator!=(const DrainSettings& rhs) const
        {
            return !operator==(rhs);
        }
    };

    typedef std::vector<DrainSettings> DrainSettings_t;

    /// \brief replication settings for agent to consume
    /// serialized data of this class will be cached/persisted by agent
    class AgentSettings {
    public:

        AgentSettings()
            : m_bPrivateEndpointEnabled(false) {}

        /// \brief consistency settings
        ConsistencySettings      m_consistencySettings;

        /// \brief SourceAgent ProtectionPairContext
        std::string        m_protectionPairContext;

        /// \brief m_clusterVirtualNodeProtectionContext used for virtual node health issues
        std::string        m_clusterVirtualNodeProtectionContext;

        /// \brief ClusterProtectionMessageContext used for cluster health issues
        std::string        m_clusterProtectionMessageContext;

        /// \brief map of DiskId and SourceAgentReplicationPairMsgContext
        std::map<std::string, std::string>  m_replicationPairMsgContexts;

        /// \brief list of protected disks
        std::vector<std::string>    m_protectedDiskIds;

        /// \brief list of protected disks
        std::vector<std::string>    m_ProtectedSharedDiskIds;

        /// \brief settings for diff file draining
        DrainSettings_t       m_drainSettings;

        /// \brief settings used for posting logs, telemetry, health 
        AgentMonitoringMsgSettings    m_monitorMsgSettings;

        /// \brief settings used for posting health for virtual node and cluster
        AgentMonitoringMsgSettings    m_clusterMonitorMsgSettings;

        /// \biref shoebox settings used for posting stats
        ShoeboxEventSettings     m_shoeboxEventSettings;

        /// \brief IR or resync settings for V2A
        SyncSettings_t       m_syncSettings;

        /// \brief the control path settings type for V2A
        std::string         m_controlPathType;

        /// \brief serialized string of the control path settings for V2A
        std::string         m_controlPathSettings;

        /// \brief the data path settings type for V2A
        std::string         m_dataPathType;

        /// \brief serialized string of the data path settings for V2A
        std::string         m_dataPathSettings;

        /// \brief serialized telemetry settings
        std::string         m_telemetrySettings;

        /// \brief global tunables supplied by RCM
        std::map<std::string, std::string> m_tunables;

        /// \brief miscellaneous properties supplied by RCM on appliance
        std::map<std::string, std::string> m_properties;

        /// \brief AutoResyncTimeWindow at VM level
        TimeWindow          m_autoResyncTimeWindow;

        ///  \brief flag indicating if private endpoint is enabled
        bool                m_bPrivateEndpointEnabled;

        /// \brief serializer method for JSON
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "AgentSettings", false);

            JSON_E(adapter, m_consistencySettings);
            JSON_E(adapter, m_protectionPairContext);
            JSON_E(adapter, m_clusterVirtualNodeProtectionContext);
            JSON_E(adapter, m_clusterProtectionMessageContext);
            JSON_KV_E(adapter, "m_replicationPairMsgContexts", m_replicationPairMsgContexts);
            JSON_E(adapter, m_protectedDiskIds);
            JSON_E(adapter, m_ProtectedSharedDiskIds);
            JSON_E(adapter, m_drainSettings);
            JSON_E(adapter, m_monitorMsgSettings);
            JSON_E(adapter, m_clusterMonitorMsgSettings);
            JSON_E(adapter, m_shoeboxEventSettings);
            JSON_E(adapter, m_syncSettings);
            JSON_E(adapter, m_controlPathType);
            JSON_E(adapter, m_controlPathSettings);
            JSON_E(adapter, m_dataPathType);
            JSON_E(adapter, m_dataPathSettings);
            JSON_E(adapter, m_telemetrySettings);
            JSON_KV_E(adapter, "m_tunables", m_tunables);
            JSON_KV_E(adapter, "m_properties", m_properties);
            JSON_E(adapter, m_autoResyncTimeWindow);
            JSON_T(adapter, m_bPrivateEndpointEnabled);

        }

        /// \brief serializer method with ptree as input
        void serialize(ptree &node)
        {
            JSON_CL(node, m_consistencySettings);
            JSON_P(node, m_protectionPairContext);
            JSON_P(node, m_clusterVirtualNodeProtectionContext);
            JSON_P(node, m_clusterProtectionMessageContext);
            JSON_KV_P(node, m_replicationPairMsgContexts);
            JSON_VP(node, m_protectedDiskIds);
            JSON_VP(node, m_ProtectedSharedDiskIds);
            JSON_VCL(node, m_drainSettings);
            JSON_CL(node, m_monitorMsgSettings);
            JSON_CL(node, m_clusterMonitorMsgSettings);
            JSON_CL(node, m_shoeboxEventSettings);
            JSON_VCL(node, m_syncSettings);
            JSON_P(node, m_controlPathType);
            JSON_P(node, m_controlPathSettings);
            JSON_P(node, m_dataPathType);
            JSON_P(node, m_dataPathSettings);
            JSON_P(node, m_telemetrySettings);
            JSON_KV_P(node, m_tunables);
            JSON_KV_P(node, m_properties);
            JSON_CL(node, m_autoResyncTimeWindow);
            JSON_P(node, m_bPrivateEndpointEnabled);
        }

    public:
        virtual void ClearCachedSettings()
        {
            m_protectedDiskIds.clear();
            m_consistencySettings.clear();
            m_drainSettings.clear();
        }
    };

    /// \brief persists the settings received from RCM
    /// the service (svagents) retrievs the settings from RCM
    /// s2 reads the drain settings
    /// vacp reads the consistemcy settings
    /// sychronization is achived by file lock
    class AgentSettingsCacher {

        /// \brief cache file location
        boost::filesystem::path m_cacheFilePath;

        /// \brief error message 
        mutable std::string m_errMsg;

        /// \brief a checksum to verify if the settings have changed
        std::string m_prevChecksum;

        /// \brief last modified timestamp of cache file
        time_t m_MTime;

        /// \brief in-memory copy of the settings
        AgentSettings   m_settings;

        /// \brief read write lock to serialize access to settings
        mutable boost::shared_mutex    m_settingsMutex;

    public:

        AgentSettingsCacher(const std::string& cacheFilePath = std::string());

        /// \brief writes the current settings to file if there is a change
        SVSTATUS Persist(AgentSettings& settings);

        /// \brief reads the settings from cache file
        SVSTATUS Read(AgentSettings& settings) const;

        /// \brief returns the last modified time of the cache file
        time_t GetLastModifiedTime() const;

        /// \brief returns error message
        const std::string Error()
        {
            return m_errMsg;
        }
    };

    typedef boost::shared_ptr<AgentSettingsCacher> AgentSettingsCacherPtr;

}

#endif // _AGENT_SETTINGS_H
