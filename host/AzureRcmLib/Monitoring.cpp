/*
+-------------------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp. 2016
+-------------------------------------------------------------------------------------------------+
File        :   Monitoring.cpp

Description :   Monitoring Library implementation

+-------------------------------------------------------------------------------------------------+
*/


#include "ServiceBusQueue.h"
#include "EventHub.h"
#include "Monitoring.h"
#include "MonitoringMsgSettings.h"
#include "MonitoringMsgHandler.h"
#include "AgentSettings.h"
#include "RcmClientSettings.h"
#include "RcmLibConstants.h"
#include "RcmClientUtils.h"

#include "securityutils.h"
#include "localconfigurator.h"
#include "proxysettings.h"

#include "inmerroralertnumbers.h"

#include <boost/tokenizer.hpp>
#include <boost/foreach.hpp>

using namespace AzureStorageRest;

namespace RcmClientLib
{
    const std::string OPEN_BRACES = "{";
    const std::string CLOSE_BRACES = "}";
    const std::string DOUBLE_QUOTES = "\"";
    const std::string KEY_VALUE_SEPERATOR = ":";
    const std::string PARAM_SEPERATOR = ",";
    const std::string WHITE_SPACE = " ";

    const uint64_t HUNDRED_NANO_SECS_IN_SEC = 10 * 1000 * 1000;

    boost::mutex MonitoringAgent::s_lock;
    boost::shared_ptr<MonitoringAgent> MonitoringAgent::s_monitoringSettingsPtr;

    std::set<std::string> MonitoringAgent::s_noRateControlAlertTypes;
    unsigned char m_prevAgentProtectionPairHealthIssuesMsgHash;

    MonitoringAgent& MonitoringAgent::GetInstance()
    {
        if (NULL == MonitoringAgent::s_monitoringSettingsPtr.get())
        {
            boost::unique_lock<boost::mutex> lock(s_lock);
            if (NULL == MonitoringAgent::s_monitoringSettingsPtr.get())
            {
                std::vector<uint32_t> noRateControlAlertTypes;
                noRateControlAlertTypes.push_back(E_CRASH_CONSISTENCY_FAILURE);
                noRateControlAlertTypes.push_back(E_APP_CONSISTENCY_FAILURE);
                noRateControlAlertTypes.push_back(E_CRASH_CONSISTENCY_FAILURE_NON_DATA_MODE);
                noRateControlAlertTypes.push_back(E_REBOOT_ALERT);
                noRateControlAlertTypes.push_back(E_AGENT_UPGRADE_ALERT);
                noRateControlAlertTypes.push_back(E_LOG_UPLOAD_NETWORK_FAILURE_ALERT);
                noRateControlAlertTypes.push_back(E_DISK_CHURN_PEAK_ALERT);
                noRateControlAlertTypes.push_back(E_LOG_UPLOAD_NETWORK_HIGH_LATENCY_ALERT);
                noRateControlAlertTypes.push_back(E_DISK_RESIZE_ALERT);
                noRateControlAlertTypes.push_back(E_TIME_JUMPED_FORWARD_ALERT);
                noRateControlAlertTypes.push_back(E_TIME_JUMPED_BACKWARD_ALERT);
                noRateControlAlertTypes.push_back(E_ASR_VSS_PROVIDER_MISSING);
                noRateControlAlertTypes.push_back(E_ASR_VSS_SERVICE_DISABLED);

                std::vector<uint32_t>::iterator alertIter = noRateControlAlertTypes.begin();
                for (/*empty*/; alertIter != noRateControlAlertTypes.end(); alertIter++)
                {
                    std::stringstream ssAlertType;
                    ssAlertType << "EA" << std::setw(4) << std::setfill('0') << *alertIter;
                    s_noRateControlAlertTypes.insert(ssAlertType.str());
                }
                boost::shared_ptr<MonitoringAgent> ptrMonitoringAgent(new MonitoringAgent());
                ptrMonitoringAgent->Init();
                s_monitoringSettingsPtr = ptrMonitoringAgent;
            }
        }
        return *s_monitoringSettingsPtr.get();
    }

    void MonitoringAgent::Init()
    {
        LocalConfigurator lc;
        m_hostId = lc.getHostId();
        m_alertInterval = lc.getRepeatingAlertIntervalInSeconds();
        m_requestTimeout = lc.getRcmRequestTimeout();

        GetTransportType();

        std::string proxySettingsPath = lc.getProxySettingsPath();
        if (!boost::filesystem::exists(proxySettingsPath))
        {
            DebugPrintf(SV_LOG_INFO, "%s: Proxy settings not found.\n", FUNCTION_NAME);
        }
        else
        {
            ProxySettings proxy(proxySettingsPath);
            if (!proxy.m_Address.empty() &&
                !proxy.m_Port.empty())
            {
                DebugPrintf(SV_LOG_INFO, "%s : Using proxy settings %s %s for monitoring.\n",
                    FUNCTION_NAME,
                    proxy.m_Address.c_str(),
                   proxy.m_Port.c_str());
                m_proxy.m_address = proxy.m_Address;
                m_proxy.m_port = proxy.m_Port;
                m_proxy.m_bypasslist = proxy.m_Bypasslist;
            }
        }

        return;
    }

    void MonitoringAgent::GetTransportType()
    {
        m_transportType = UnknownTransport;
        if (RcmConfigurator::getInstance()->IsAzureToAzureReplication() || RcmConfigurator::getInstance()->IsAzureToOnPremReplication())
        {
            if (RcmConfigurator::getInstance()->IsPrivateEndpointEnabled())
            {
                m_transportType = RcmRestApi;
            }
            else
            {
                m_transportType = RcmEventHub;
            }
        }
        else
        {
            m_transportType = RcmProxy;
        }
    }

    bool MonitoringAgent::SendMessageToRcm(const std::string& uri,
        const std::string& message,
        const std::string& msgLevel,
        const headers_t& propertyMap)
    {
        std::string encodedHealthIssuesMsg = securitylib::base64Encode(message.c_str(), message.length());

        /// Event Hub supports a max size of 1 MB including headers
        /// based on the size of the message this needs to be further looked at if the size is more than max size
        /// one approach is to split the message into multiple messages
        /// but keep in mind when sending health issues as one message should contain all the health issues.
        assert(encodedHealthIssuesMsg.length() < HttpRestUtil::ServiceBusConstants::StandardEventHubMessageInBytesMaxSize);

        bool status = false;

        if (m_transportType == RcmEventHub)
        {
            status = SendMessageToEventHub(uri,
                RcmConfigurator::getInstance()->IsAzureToAzureReplication() ?
                    encodedHealthIssuesMsg :
                    message,
                propertyMap);
        }
        else if (m_transportType == RcmProxy)
        {
            if (!boost::iequals(msgLevel, MonitoringMessageLevel::CRITICAL) &&
                !boost::iequals(msgLevel, MonitoringMessageLevel::INFORMATIONAL))
            {
                DebugPrintf(SV_LOG_ERROR, " %s: unsupported msgType %s for RcmProxy transport\n", FUNCTION_NAME, msgLevel.c_str());
            }
            else
            {
                // for proxy send plain message, no encoding is required
                status = SendMessageToRcmProxy(uri, message, propertyMap);
            }
        }
        else if (m_transportType == RcmRestApi)
        {
            status = SendMessageToRcmRestApi(uri,
                RcmConfigurator::getInstance()->IsAzureToOnPremReplication() ? message : encodedHealthIssuesMsg,
                propertyMap);
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "%s: unsupported transport type %d\n", FUNCTION_NAME, m_transportType);
        }

        return status;
    }

    bool MonitoringAgent::SendMessageToEventHub(const std::string& uri,
        const std::string& message,
        const headers_t& propertyMap)
    {
        bool status =    false;
        std::string errMsg;

        const std::string keyHttps("https");
        const std::string keyEndpoint("Endpoint=sb");
        const std::string keySas("SharedAccessSignature=");
        const std::string keyEntityPath("EntityPath=");
        const std::string keyPublishers("Publisher=");

        boost::char_separator<char> delm(";");
        boost::tokenizer < boost::char_separator<char> > strtokens(uri, delm);
        errMsg = "Event Hub URI is not in expected format. URI ";
        errMsg += uri;

        std::vector<std::string> tokens;
        BOOST_FOREACH(std::string t, strtokens)
        {
            tokens.push_back(t);
            DebugPrintf(SV_LOG_DEBUG, "%s: %s\n", FUNCTION_NAME, t.c_str());
        }

        if (tokens.size() != 4)
        {
            errMsg += " tokens size mismatch expected 4 but found ";
            errMsg += boost::lexical_cast<std::string>(tokens.size());
            DebugPrintf(SV_LOG_ERROR, "%s: %s.\n", FUNCTION_NAME, errMsg.c_str());
            return false;
        }

        std::string endPointUri(tokens[0]);
        size_t keyEndpointPos = endPointUri.find(keyEndpoint);
        if (keyEndpointPos == std::string::npos)
        {
            errMsg += "  endpoint not found.";
            DebugPrintf(SV_LOG_DEBUG, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
            return false;
        }

        assert(keyEndpointPos == 0);
        endPointUri.replace(keyEndpointPos, keyEndpoint.length(), keyHttps);
        DebugPrintf(SV_LOG_DEBUG, "%s: Endpoint %s\n", FUNCTION_NAME, endPointUri.c_str());

        std::string sasToken(tokens[1]);
        size_t keySasPos = sasToken.find(keySas);
        if (keySasPos == std::string::npos)
        {
            errMsg += " SAS key not found.";
            DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
            return false;
        }

        assert(keySasPos == 0);
        sasToken.erase(keySasPos, keySas.length());

        // DebugPrintf(SV_LOG_DEBUG, "%s: SAS %s\n", FUNCTION_NAME, sasToken.c_str());

        std::string entityName(tokens[2]);
        size_t keyEntityPos = entityName.find(keyEntityPath);
        if (keyEntityPos == std::string::npos)
        {
            errMsg += " Entity name not found.";
            DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
            return false;
        }

        assert(keyEndpointPos == 0);
        entityName.erase(keyEntityPos, keyEntityPath.length());

        DebugPrintf(SV_LOG_DEBUG, "%s: Entitiy %s\n", FUNCTION_NAME, entityName.c_str());

        std::string publisher(tokens[3]);
        size_t keyPublisherPos = publisher.find(keyPublishers);
        if (keyPublisherPos == std::string::npos)
        {
            errMsg += " Publisher not found.";
            DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
            return false;
        }

        assert(keyPublisherPos == 0);
        publisher.erase(keyPublisherPos, keyPublishers.length());

        DebugPrintf(SV_LOG_DEBUG, "%s: Publisher %s\n", FUNCTION_NAME, publisher.c_str());

        EventHub eventHub(endPointUri, entityName, publisher, sasToken);

        if (m_requestTimeout)
            eventHub.SetTimeout(m_requestTimeout);

        if (m_proxy.m_address.length() &&
            m_proxy.m_port.length())
        {
            eventHub.SetHttpProxy(m_proxy);
        }

        status = eventHub.Put(message, publisher, propertyMap);

        if (!status)
        {
            errMsg = "Failed to send the message to RCM";
            DebugPrintf(SV_LOG_ERROR, " %s: %s\n", FUNCTION_NAME, errMsg.c_str());
        }

        return status;
    }

    bool MonitoringAgent::SendMessageToRcmProxy(const std::string& uri,
        const std::string& message,
        const headers_t& propertyMap)
    {
        header_const_iter_t headerIter = propertyMap.find(MonitoringProperties::MESSAGE_CONTEXT);
        if (headerIter == propertyMap.end())
        {
            DebugPrintf(SV_LOG_ERROR, " %s: MessageContext is not found.\n", FUNCTION_NAME);
            return false;
        }

        PostMessageInput messageInput;
        messageInput.Message = message;
        messageInput.MessageContext = headerIter->second;

        // remove MessageContext from headers as it is included in the MessageInput
        headers_t propertyMapForProxy = propertyMap;
        propertyMapForProxy.erase(MonitoringProperties::MESSAGE_CONTEXT);

        std::string serializedMessageInput;
        std::string errMsg;
        SVSTATUS status = SVS_OK;

        try {
            serializedMessageInput = JSON::producer<PostMessageInput>::convert(messageInput);
            DebugPrintf(SV_LOG_DEBUG, " %s: %s\n", FUNCTION_NAME, serializedMessageInput.c_str());

            status = RcmConfigurator::getInstance()->GetRcmClient()->PostAgentMessage(uri, serializedMessageInput, propertyMapForProxy);
        }
        RCM_CLIENT_CATCH_EXCEPTION(errMsg, status);

        if (status != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR, "%s failed: %s\n", FUNCTION_NAME, errMsg.c_str());
        }

        return (status == SVS_OK);
    }

    bool MonitoringAgent::SendMessageToRcmRestApi(const std::string& uri,
        const std::string& message,
        const std::map<std::string, std::string>& propertyMap)
    {
        std::string errMsg;
        SVSTATUS status = SVS_OK;

        try {
            SourceAgentMessageInput srcAgentMsgInput(
                propertyMap.at(MonitoringProperties::MESSAGE_TYPE),
                propertyMap.at(MonitoringProperties::MESSAGE_SOURCE),
                propertyMap.at(MonitoringProperties::MESSAGE_CONTEXT),
                message);

            std::string serializedMessageInput =
                JSON::producer<SourceAgentMessageInput>::convert(srcAgentMsgInput);

            assert(serializedMessageInput.length() < HttpRestUtil::ServiceBusConstants::StandardSBMessageInBytesMaxSize);

            DebugPrintf(SV_LOG_DEBUG, " %s: %s\n", FUNCTION_NAME, serializedMessageInput.c_str());

            status = RcmConfigurator::getInstance()->GetRcmClient()->PostAgentMessage(uri, serializedMessageInput, propertyMap);
        }
        RCM_CLIENT_CATCH_EXCEPTION(errMsg, status);

        if (status != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR, "%s failed: %s\n", FUNCTION_NAME, errMsg.c_str());
        }

        return (status == SVS_OK);
    }

    bool MonitoringAgent::GetTransportUri(MonitoringMsgType msgType,
        std::string& uri, 
        const bool& isClusterVirtualNodeContext)
    {
        uri.clear();
        AgentMonitoringMsgSettings monitoringSettings;
        SV_LOG_LEVEL logLevel = SV_LOG_ERROR;
        bool IsMonitoringMsgSettingsFetchSuccess = false;

        GetTransportType();

        switch (m_transportType)
        {
        case RcmEventHub:
            IsMonitoringMsgSettingsFetchSuccess = isClusterVirtualNodeContext ?
                (SVS_OK == RcmConfigurator::getInstance()->GetMonitoringMessageSettings(monitoringSettings, isClusterVirtualNodeContext))
                    : (SVS_OK == RcmConfigurator::getInstance()->GetMonitoringMessageSettings(monitoringSettings));
            if (!IsMonitoringMsgSettingsFetchSuccess)
            {
                DebugPrintf(SV_LOG_ERROR, " %s: failed to get monitoring settings, isClusterVirtualNodeContext=%d\n", FUNCTION_NAME, isClusterVirtualNodeContext);
                return false;
            }

            switch (msgType)
            {
            case RcmMsgAlert:
            case RcmMsgHealth:
                uri = monitoringSettings.CriticalEventHubUri;
                break;
            case RcmMsgStats:
            case RcmMsgSyncStats:
            case RcmMsgMonitoringStats:
                uri = monitoringSettings.InformationEventHubUri;
                break;
            case RcmMsgLogs:
                uri = monitoringSettings.LoggingEventHubUri;
                break;
            default:
                break;
            }
            break;
        case RcmRestApi:
            switch (msgType)
            {
            case RcmMsgAlert:
            case RcmMsgHealth:
            case RcmMsgStats:
                uri = POST_MONITORING_MESSAGE_URI;
                break;
            case RcmMsgLogs:
            case RcmMsgMonitoringStats:
                // skip stats for V2A RCM failback in private link environment
                if (RcmConfigurator::getInstance()->IsAzureToOnPremReplication())
                    logLevel = SV_LOG_INFO;
                else
                    uri = POST_DIAGNOSTIC_MESSAGE_URI;
                break;
            default:
                break;
            }
            break;
        case RcmProxy:
            switch (msgType)
            {
            case RcmMsgAlert:
            case RcmMsgHealth:
            case RcmMsgSyncStats:
                uri = RCM_ACTION_POST_MESSAGE;
                break;
            case RcmMsgStats:
                // in case of RCM Proxy, the stats are sent along with the Health Issues
                // supress error logs
                logLevel = SV_LOG_INFO;
                break;
            case RcmMsgLogs:
                // in case of RCM Proxy, the logs are sent to process server
                break;
            case RcmMsgMonitoringStats:
                // in case of RCM Proxy, the monitoring stats are sent to process server
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }

        if (uri.empty())
        {
            DebugPrintf(logLevel,
                "%s : URI is empty: transport type %d, message type %d\n",
                FUNCTION_NAME,
                m_transportType,
                msgType);
            return false;
        }

        DebugPrintf(SV_LOG_DEBUG,
            "%s : For transport type %s and message type %s, URI is %s\n",
            FUNCTION_NAME,
            StrMonitoringTransportType.at(m_transportType).c_str(),
            StrMonitoringMsgType.at(msgType).c_str(),
            uri.c_str());

        return true;
    }

    void MonitoringLib::PostHealthIssuesToRcm(
        SourceAgentProtectionPairHealthIssues& issues,
        const std::string &hostId)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

        boost::shared_ptr<AbstractMonMsgPostToRcmHandler> handlerChain =
            AbstractMonMsgPostToRcmHandler::GetHandlerChain();

        if (handlerChain->HandleHealthIssues(issues))
        {
            DebugPrintf(SV_LOG_ALWAYS, " %s: agent health issues successfully posted to RCM.\n", FUNCTION_NAME);
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, " %s: Failed to send agent health issues to RCM.\n", FUNCTION_NAME);
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    }

    bool MonitoringLib::PostStatsToRcm(const VolumesStats_t &stats,
        const DiskIds_t &diskIds,
        const std::string &hostId)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        std::string errMsg;

        boost::shared_ptr<AbstractMonMsgPostToRcmHandler> handlerChain = 
            AbstractMonMsgPostToRcmHandler::GetHandlerChain();

        bool status = handlerChain->HandleStats(stats, diskIds);

        if (!status)
        {
            DebugPrintf(SV_LOG_ERROR, " %s: Failed to send stats to RCM.\n", FUNCTION_NAME);
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }

    bool MonitoringLib::PostMonitoringStatsToRcm(std::string statsMsg, std::string hostId)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS ret = SVS_OK;
        std::string errMsg;

        if (!AbstractMonMsgPostToRcmHandler::IsPostMonitoringToRcmEnabled())
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s Sending monitoring stats to rcm skipped.\n", FUNCTION_NAME);
            return false;
        }

        ShoeboxEventSettings settings;
        ret = RcmConfigurator::getInstance()->GetShoeboxEventSettings(settings);

        if (ret != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR, " %s: failed to get shoebox settings with error %d\n", FUNCTION_NAME, ret);
            return false;
        }

        std::string uri;
        if (!MonitoringAgent::GetInstance().GetTransportUri(RcmMsgMonitoringStats, uri))
        {
            return false;
        }

        DebugPrintf(SV_LOG_DEBUG, "%s: Getting ResourceId as %s\n", FUNCTION_NAME, settings.VaultResourceArmId.c_str());

        std::map<std::string, std::string> propertyMap;
        RcmMonitoringEventProperties::GenerateMonitoringCustomProperties(
            hostId,
            *RcmConfigurator::getInstance()->GetRcmClientSettings(),
            MonitoringMessageType::SOURCE_MONITORING_STATS,
            MonitoringMessageSource::SOURCE_AGENT,
            propertyMap);

        bool status = MonitoringAgent::GetInstance().SendMessageToRcm(uri,
            statsMsg,
            MonitoringMessageLevel::INFORMATIONAL,
            propertyMap);

        if (!status)
        {
            errMsg = "Failed to send monitoring data to RCM.";
            DebugPrintf(SV_LOG_ERROR, " %s: %s status: %d\n", FUNCTION_NAME, errMsg.c_str(), status);
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }

    bool MonitoringLib::PostSyncProgressMessageToRcm(const std::string& statsMsg, const std::string &diskId)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS ret = SVS_OK;
        std::string errMsg;

        std::string uri;
        if (!MonitoringAgent::GetInstance().GetTransportUri(RcmMsgSyncStats, uri))
        {
            return false;
        }

        std::map<std::string, std::string> propertyMap;
        RcmMonitoringEventProperties::GenerateMonitoringCustomProperties(
            MonitoringAgent::GetInstance().GetHostId(),
            diskId,
            *RcmConfigurator::getInstance()->GetRcmClientSettings(),
            (RcmConfigurator::getInstance()->IsOnPremToAzureReplication() ?
                MonitoringMessageType::ONPREM_TO_AZURE_SYNC_PROGRESS_MESSAGE :
                MonitoringMessageType::AZURE_TO_ONPREM_SYNC_PROGRESS_MESSAGE),
            MonitoringMessageSource::SOURCE_AGENT,
            propertyMap);

        bool status = MonitoringAgent::GetInstance().SendMessageToRcm(uri,
            statsMsg,
            MonitoringMessageLevel::INFORMATIONAL,
            propertyMap);

        if (!status)
        {
            errMsg = "Failed to send monitoring data to RCM.";
            DebugPrintf(SV_LOG_ERROR, " %s: %s\n", FUNCTION_NAME, errMsg.c_str());
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return status;
    }

    bool MonitoringLib::SendAlertToRcm(SV_ALERT_TYPE alertType,
        SV_ALERT_MODULE alertingModule,
        const InmAlert &alert,
        const std::string &hostId,
        const std::string &diskId)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

        boost::mutex::scoped_lock guard(MonitoringAgent::GetInstance().GetAlertGuard());

        boost::shared_ptr<AbstractMonMsgPostToRcmHandler> handlerChain =
            AbstractMonMsgPostToRcmHandler::GetHandlerChain();

        bool status = handlerChain->HandleAlerts(alertType, alertingModule, alert, diskId);

        if (!status)
        {
            DebugPrintf(SV_LOG_ERROR, " %s: Failed to send alert to RCM\n", FUNCTION_NAME);
            return false;
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return true;
    }


    bool MonitoringLib::PostAgentLogsToRcm(const std::string &messageType,
        const std::string &messageSource,
        const std::string &serializedLogMessages)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        SVSTATUS ret = SVS_OK;
        std::string errMsg;

        try
        {
            std::map<std::string, std::string> propertyMap;

            RcmMonitoringEventProperties::GenerateMonitoringCustomProperties(
                MonitoringAgent::GetInstance().GetHostId(),
                *RcmConfigurator::getInstance()->GetRcmClientSettings(),
                messageType,
                messageSource,
                propertyMap);

            std::string uri;
            if (!MonitoringAgent::GetInstance().GetTransportUri(RcmMsgLogs, uri))
            {
                errMsg = "Agent Logging Queue URI in Cached settings is empty.";
                DebugPrintf(SV_LOG_ERROR, " %s: %s\n", FUNCTION_NAME, errMsg.c_str());
                return false;
            }

            DebugPrintf(SV_LOG_DEBUG, "%s: Agent Logging Queue URI %s\n", FUNCTION_NAME, uri.c_str());

            bool status = MonitoringAgent::GetInstance().SendMessageToRcm(uri,
                serializedLogMessages,
                MonitoringMessageLevel::DIAGNOSTIC,
                propertyMap);

            if (!status)
            {
                errMsg = "Failed to send agent logs to RCM.";
                DebugPrintf(SV_LOG_ERROR, " %s: %s\n", FUNCTION_NAME, errMsg.c_str());
                return false;
            }
        }
        RCM_CLIENT_CATCH_EXCEPTION(errMsg, ret);

        if (ret != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR, "PostAgentLogsToRcm failed: %s\n", errMsg.c_str());
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return (ret != SVS_OK) ? false : true;
    }

    void RcmMonitoringEventProperties::GenerateMonitoringCustomProperties(
        const std::string &hostId,
        const RcmClientSettings &clientSettings,
        const std::string &messageType,
        const std::string &messageSource,
        std::map<std::string, std::string> &propertyMap, 
        const std::string &messageContext)
    {
        RcmMonitoringEventProperties monEventProperties;

        monEventProperties.ActivityId = GenerateUuid();
        monEventProperties.ClientRequestId = GenerateUuid();
        DebugPrintf(SV_LOG_DEBUG, "ClientRequestId : %s\n", monEventProperties.ClientRequestId.c_str());
        monEventProperties.MessageSource = messageSource;
        monEventProperties.MessageType = messageType;

        if (messageContext.empty())
        {
            RcmConfigurator::getInstance()->GetProtectionPairContext(monEventProperties.MessageContext);
        }
        else
        {
            monEventProperties.MessageContext = messageContext;
        }
        monEventProperties.Serialize(propertyMap);
    }

    void RcmMonitoringEventProperties::GenerateMonitoringCustomProperties(
        const std::string &hostId,
        const std::string &diskId,
        const RcmClientSettings &clientSettings,
        const std::string &messageType,
        const std::string &messageSource,
        std::map<std::string, std::string> &propertyMap)
    {
        RcmMonitoringEventProperties monEventProperties;

        monEventProperties.ActivityId = GenerateUuid();
        monEventProperties.ClientRequestId = GenerateUuid();
        DebugPrintf(SV_LOG_DEBUG, "ClientRequestId : %s\n", monEventProperties.ClientRequestId.c_str());
        monEventProperties.MessageSource = messageSource;
        monEventProperties.MessageType = messageType;
        
        RcmConfigurator::getInstance()->GetReplicationPairMessageContext(diskId, monEventProperties.MessageContext);
        monEventProperties.Serialize(propertyMap);
    }

}
