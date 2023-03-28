/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp. 2016
+------------------------------------------------------------------------------------+
File        :   MonitoringMsgSettings.h

Description :   Contains serializable classes used to post stats and alerts
to RCM.

+------------------------------------------------------------------------------------+
*/

#include <string>
#include <vector>

#include "json_reader.h"
#include "json_writer.h"

#include <boost/thread/mutex.hpp>
#include <boost/assign.hpp>

#include "RcmClientSettings.h"
#include "AgentSettings.h"
#include "HttpUtil.h"
#include "RcmClient.h"

#ifndef _RCM_MONITORING_MSG_SETTINGS_H
#define _RCM_MONITORING_MSG_SETTINGS_H

using namespace AzureStorageRest;

namespace RcmClientLib
{
    const std::string RCM_PUBLIC_ENDPOINT_TYPE = "Public";
    const std::string RCM_PRIVATE_ENDPOINT_TYPE = "Private";
    const std::string RCM_MIXED_ENDPOINT_TYPE = "Mixed";
    const std::string POST_MONITORING_MESSAGE_USING_REST_API_TO_RCM = "SendMonitoringMessage";
    const std::string POST_DIAGNOSTIC_MESSAGE_USING_REST_API_TO_RCM = "SendDiagnosticLog";

    namespace MonitoringProperties
    {
        const std::string MESSAGE_SOURCE = "MessageSource";
        const std::string MESSAGE_TYPE = "MessageType";
        const std::string MESSAGE_CONTEXT = "MessageContext";
        const std::string CLIENT_REQUEST_ID = "ClientRequestId";
        const std::string ACTIVITY_ID = "ActivityId";
    }

    namespace MonitoringMessageSource
    {
        const std::string PROTECTION_SERVICE = "ProtectionService";
        const std::string RCM = "Rcm";
        const std::string GATEWAY_SERVICE = "GatewayService";
        const std::string SOURCE_AGENT = "SourceAgent";
    }

    namespace MonitoringMessageType
    {
        const std::string AGENT_LOG_ADMIN_EVENT = "AgentLogAdminEvent";
        const std::string AGENT_LOG_OPERATIONAL_EVENT = "AgentLogOperationalEvent";
        const std::string AGENT_LOG_DEBUG_EVENT = "AgentLogDebugEvent";
        const std::string AGENT_LOG_ANALYTIC_EVENT = "AgentLogAnalyticEvent";

        const std::string AGENT_TELEMETRY_SOURCE = "AgentTelemetrySourceV2";
        const std::string AGENT_TELEMETRY_INDSKFLT = "AgentTelemetryInDskFltV2";
        const std::string AGENT_TELEMETRY_SCHEDULER = "AgentTelemetrySrcTaskSchedV2";
        const std::string AGENT_TELEMETRY_VACP = "AgentTelemetryVacpV2";

        const std::string SOURCE_MONITORING_STATS = "AgentShoeboxEvent";

        const std::string SOURCE_AGENT_STATS = "AgentProtectionPairHealth";
        const std::string PROTECTION_PAIR_HEALTH_EVENT = "ProtectionPairHealthEvent";
        const std::string REPLICATION_PAIR_HEALTH_EVENT = "ReplicationPairHealthEvent";
        const std::string AGENT_PROTECTION_PAIR_HEALTH_ISSUES = "AgentProtectionPairHealthIssues";

        const std::string ONPREM_TO_AZURE_SYNC_PROGRESS_MESSAGE = "OnPremToAzureSyncProgress";
        const std::string ONPREM_TO_AZURE_VM_HEALTH = "OnPremToAzureVmHealth";
        const std::string ONPREM_TO_AZURE_VM_HEALTH_EVENT = "OnPremToAzureVmHealthEvent";
        const std::string ONPREM_TO_AZURE_DISK_HEALTH_EVENT = "OnPremToAzureDiskHealthEvent";

        const std::string AZURE_TO_ONPREM_SYNC_PROGRESS_MESSAGE = "AzureToOnPremSyncProgress";
        const std::string AZURE_TO_ONPREM_VM_HEALTH = "AzureToOnPremVmHealth";
        const std::string AZURE_TO_ONPREM_VM_HEALTH_EVENT = "AzureToOnPremVmHealthEvent";
        const std::string AZURE_TO_ONPREM_DISK_HEALTH_EVENT = "AzureToOnPremDiskHealthEvent";

        const std::string VIRUTAL_NODE_REPLICATION_PAIR_HEALTH_ISSUES =
            "SharedDiskReplicationHealth";
        const std::string CLUSTER_PROTECTION_HEALTH_ISSUES = "SourceClusterProtectionHealth";
    }

    namespace MonitoringMessageLevel
    {
        const std::string CRITICAL = "Critical";
        const std::string INFORMATIONAL = "Informational";
        const std::string DIAGNOSTIC = "Diagnostic";
    }

    namespace RcmComponentId
    {
        const std::string SOURCE_AGENT = "SourceAgent";
        const std::string GATEWAY_SERVICE = "GatewayService";
        const std::string PROCESS_SERVER = "ProcessServer";
        const std::string MASTER_TARGET = "MasterTarget";
    }

    class RcmMonitoringEventProperties
    {
    public:
        static void GenerateMonitoringCustomProperties(
            const std::string &hostId,
            const RcmClientSettings &clientSettings,
            const std::string &messageType,
            const std::string &messageSource,
            std::map<std::string, std::string> &propertyMap, 
            const std::string &messageContext = std::string());

        static void GenerateMonitoringCustomProperties(
            const std::string &hostId,
            const std::string &diskId,
            const RcmClientSettings &clientSettings,
            const std::string &messageType,
            const std::string &messageSource,
            std::map<std::string, std::string> &propertyMap);

    private:
        std::string MessageSource;
        std::string MessageType;
        std::string MessageContext;
        std::string ClientRequestId;
        std::string ActivityId;

        void Serialize(std::map<std::string, std::string> &propertyMap) const
        {
            propertyMap.insert(std::make_pair(MonitoringProperties::MESSAGE_SOURCE, MessageSource));
            propertyMap.insert(std::make_pair(MonitoringProperties::MESSAGE_TYPE, MessageType));
            propertyMap.insert(std::make_pair(MonitoringProperties::MESSAGE_CONTEXT, MessageContext));
            propertyMap.insert(std::make_pair(MonitoringProperties::CLIENT_REQUEST_ID, ClientRequestId));
            propertyMap.insert(std::make_pair(MonitoringProperties::ACTIVITY_ID, ActivityId));
        }

    };


    class AgentReplicationPairHealthDetails
    {
    public:
        /// \brief the source disk identifier.
        std::string SourceDiskId;

        /// \brief the opaque list of replication pair identifiers associated
        /// with this source machine.
        std::string ReplicationPairsContext;

        /// \brief the data pending for replication at source in MB. 
        double DataPendingAtSourceAgentInMB;

        /// \brief a serializer method for JSON
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "AgentReplicationPairHealthDetails", false);

            JSON_E(adapter, SourceDiskId);
            JSON_E(adapter, ReplicationPairsContext);
            JSON_T(adapter, DataPendingAtSourceAgentInMB);
        }
    };

    class AgentProtectionPairHealthMsg
    {
    public:
        /// \brief list of health details of associated disks.
        std::vector<AgentReplicationPairHealthDetails> DiskDetails;

        /// \brief a serializer method for JSON
        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "AgentProtectionPairHealthMsg", false);

            JSON_T(adapter, DiskDetails);
        }
    };

    class AgentEventMsg
    {
    public:
        /// \brief Event code as defined in the contract
        /// refer to inmerroralertnumbers.h
        std::string EventCode;

        /// \brief Event severity
        /// valid values defined in RCM are Error, Warning, Information
        std::string EventSeverity;

        /// \brief Event source
        /// fixed as SourceAgent
        std::string EventSource;

        /// \brief Event time in 100 ns granularity
        uint64_t    EventTimeUtc;

        /// \brief a dictionary of message params in JSON format
        std::map<std::string, std::string> MessageParams;

        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "AgentEventMsg", false);

            JSON_E(adapter, EventCode);
            JSON_E(adapter, EventSeverity);
            JSON_E(adapter, EventSource);
            JSON_E(adapter, EventTimeUtc);
            JSON_T(adapter, MessageParams);
        }
    };

    class AgentProtectionPairMsgContext
    {
    public:
        std::string TenantId;
        std::string ObjectId;
        std::string SourceMachineId;

        void serialize(JSON::Adapter &adapter)
        {
            JSON::Class root(adapter, "AgentProtectionPairMsgContext", false);

            JSON_E(adapter, TenantId);
            JSON_E(adapter, ObjectId);
            JSON_T(adapter, SourceMachineId);
        }
    };

    class AgentReplicationPairMsgContext
    {
    public:
        std::string TenantId;
        std::string ObjectId;
        std::string SourceMachineId;
        std::string SourceDiskId;

        void serialize(JSON::Adapter &adapter)
        {
            JSON::Class root(adapter, "AgentReplicationPairMsgContext", false);

            JSON_E(adapter, TenantId);
            JSON_E(adapter, ObjectId);
            JSON_E(adapter, SourceMachineId);
            JSON_T(adapter, SourceDiskId);
        }
    };


    /// \brief message input to be sent to RCM Proxy.
    /// it encapsulates the message context and message
    class PostMessageInput
    {
    public:
        /// the message context
        std::string MessageContext;

        /// the message content.
        std::string Message;

        void serialize(JSON::Adapter &adapter)
        {
            JSON::Class root(adapter, "PostMessageInput", false);

            JSON_E(adapter, MessageContext);
            JSON_T(adapter, Message);
        }
    };

    /// \brief message input to be sent to RCM REST API.
    /// it encapsulates the message context and message
    class SourceAgentMessageInput
    {
    public:
        SourceAgentMessageInput() {}
        SourceAgentMessageInput(const std::string& msgType,
            const std::string& msgSource,
            const std::string& msgContext,
            const std::string& msgContent)
            :MessageType(msgType),
            MessageSource(msgSource),
            MessageContext(msgContext),
            MessageContent(msgContent) {}

        void serialize(JSON::Adapter &adapter)
        {
            JSON::Class root(adapter, "SourceAgentMessageInput", false);

            JSON_E(adapter, MessageType);
            JSON_E(adapter, MessageSource);
            JSON_E(adapter, MessageContext);
            JSON_T(adapter, MessageContent);
        }
    private:
        std::string MessageType;
        std::string MessageSource;
        std::string MessageContext;
        std::string MessageContent;
    };

    enum MonitoringTransportType
    {
        UnknownTransport = 0,
        RcmEventHub,
        RcmProxy,
        RcmRestApi
    };

    const std::map<MonitoringTransportType, std::string> StrMonitoringTransportType = boost::assign::map_list_of
        (UnknownTransport, "Unknown")
        (RcmEventHub, "RcmEventHub")
        (RcmProxy, "RcmProxy")
        (RcmRestApi, "RcmRestApi");

    enum MonitoringMsgType {
        RcmMsgAlert = 1,
        RcmMsgStats,
        RcmMsgLogs,
        RcmMsgHealth,
        RcmMsgMonitoringStats,
        RcmMsgSyncStats
    };

    const std::map<MonitoringMsgType, std::string> StrMonitoringMsgType = boost::assign::map_list_of
        (RcmMsgAlert, "RcmMsgAlert")
        (RcmMsgStats, "RcmMsgStats")
        (RcmMsgLogs, "RcmMsgLogs")
        (RcmMsgHealth, "RcmMsgHealth")
        (RcmMsgMonitoringStats, "RcmMsgMonitoringStats")
        (RcmMsgSyncStats, "RcmMsgSyncStats");

    class MonitoringAgent
    {
    public:
        static MonitoringAgent& GetInstance();

        bool SendMessageToRcm(const std::string& uri,
            const std::string& msg,
            const std::string& msgLevel,
            const headers_t& propertyMap);

        bool GetTransportUri(MonitoringMsgType msgType,
            std::string& uri, const bool& isClusterVirtualNodeContext = false);

        uint64_t GetAlertInterval() { return m_alertInterval; }

        boost::mutex& GetAlertGuard() { return m_alertMutex; }

        std::map<std::string, uint64_t>& GetAlertMap() { return m_alertMap; }

        std::string GetHostId() const { return m_hostId; }

        static std::set<std::string>    s_noRateControlAlertTypes;

    private:
        void Init();

        MonitoringAgent(){};

        bool SendMessageToEventHub(const std::string& uri,
            const std::string& message,
            const headers_t& propertyMap);

        bool SendMessageToRcmProxy(const std::string& uri,
            const std::string& message,
            const headers_t& propertyMap);

        bool SendMessageToRcmRestApi(const std::string& uri,
            const std::string& message,
            const std::map<std::string, std::string>& propertyMap);

        bool SendMessageToRcm(const std::string& uri,
            const std::string& message,
            const std::map<std::string, std::string>& propertyMap);

        void GetTransportType();

        static boost::mutex                          s_lock;
        static boost::shared_ptr<MonitoringAgent>    s_monitoringSettingsPtr;

        boost::mutex                            m_alertMutex;
        uint64_t                                m_alertInterval;
        std::map<std::string, uint64_t>         m_alertMap;

        HttpProxy                               m_proxy;

        MonitoringTransportType                 m_transportType;
        uint32_t                                m_requestTimeout;
        std::string                             m_hostId;

    };

}

#endif