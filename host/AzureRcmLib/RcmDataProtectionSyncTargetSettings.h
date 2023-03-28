#ifndef _RCM_DATAPROTECTION_SYNC_TARGET_SETTINGS_H
#define _RCM_DATAPROTECTION_SYNC_TARGET_SETTINGS_H

#include "json_writer.h"
#include "json_reader.h"

#include <string>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>

#include "RcmJobs.h"
#include "AgentSettings.h"

using boost::property_tree::ptree;

namespace RcmClientLib {

    /// \biref replication settings for source agent
    class DataProtectionSyncRcmTargetSettings {
    public:

        /// \brief replication pair (disk) action
        RcmReplicationPairAction  ReplicationPairAction;

        /// \brief to be used when sending events at VM level
        std::string ProtectionPairContext;

        /// \brief to be used when sending events at disk level
        std::string ReplicationPairMessageContext;

        std::string ControlPathTransportType;

        std::string ControlPathTransportSettings;

        std::string DataPathTransportType;

        std::string DataPathTransportSettings;

        /// \brief single path for telemetry data supplied by RCM on appliance
        ProtectedMachineTelemetrySettings TelemetrySettings;

        /// \brief global tunables supplied by RCM
        std::map<std::string, std::string> Tunables;

        /// \brief miscellaneous properties supplied by RCM on appliance
        std::map<std::string, std::string> Properties;

        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "DataProtectionSyncRcmTargetSettings", false);

            JSON_E(adapter, ReplicationPairAction);
            JSON_E(adapter, ProtectionPairContext);
            JSON_E(adapter, ReplicationPairMessageContext);
            JSON_E(adapter, ControlPathTransportType);
            JSON_E(adapter, ControlPathTransportSettings);
            JSON_E(adapter, DataPathTransportType);
            JSON_E(adapter, DataPathTransportSettings);
            JSON_E(adapter, TelemetrySettings);
            JSON_KV_E(adapter, "Tunables", Tunables);
            JSON_KV_T(adapter, "Properties", Properties);
        }

        void serialize(ptree& node)
        {
            JSON_CL(node, ReplicationPairAction);
            JSON_P(node, ProtectionPairContext);
            JSON_P(node, ReplicationPairMessageContext);
            JSON_P(node, ControlPathTransportType);
            JSON_P(node, ControlPathTransportSettings);
            JSON_P(node, DataPathTransportType);
            JSON_P(node, DataPathTransportSettings);
            JSON_CL(node, TelemetrySettings);
            JSON_KV_P(node, Tunables);
            JSON_KV_P(node, Properties);
        }
    };

} // namespace RcmClientLib 
#endif