#include "AgentMonitoringHandler.h"

#include "inm_md5.h"

#include <boost/bind.hpp>

using namespace RcmClientLib;

bool AgentMonitoringMessageHandler::HandleHealthIssues(
    SourceAgentProtectionPairHealthIssues& healthIssues)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    static std::vector<unsigned char> prevAgentHealthIssuesMsgHash(HASH_LENGTH, 0);
    static long long s_lastReportTime = 0;
    std::string uri;
    bool status = false;

    do
    {
        if (!ValidateSettingsAndGetPostUri(RcmMsgHealth, uri, SOURCE_AGENT))
        {
            break;
        }

        SourceAgentProtectionPairHealthIssues agentHealthIssues =
            FilterHealthIssues(healthIssues, SOURCE_AGENT);

        std::string serializedAgentHealthIssues = GetSerailizedHealthIssueInput(agentHealthIssues);

        std::vector<unsigned char> currhash(HASH_LENGTH, 0);
        INM_MD5_CTX ctx;
        INM_MD5Init(&ctx);
        INM_MD5Update(&ctx, (unsigned char*)serializedAgentHealthIssues.c_str(), serializedAgentHealthIssues.size());
        INM_MD5Final(&currhash[0], &ctx);

        bool IsChecksumMismatch = comparememory(&prevAgentHealthIssuesMsgHash[0], &currhash[0], HASH_LENGTH);

        if (IsChecksumMismatch)
        {
            DebugPrintf(SV_LOG_INFO, "Agent health issues checksum mis-matched\n");
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "Agent health issues checksum matched\n");
            if (!IsMinResendTimePassed(s_lastReportTime))
            {
                DebugPrintf(SV_LOG_INFO, "Agent health issues checksum matched and last message sent was within rate limit. skipping post\n");
                status = true;
                break;
            }
        }

        if (agentHealthIssues.HealthIssues.empty() && agentHealthIssues.DiskLevelHealthIssues.empty())
        {
            DebugPrintf(SV_LOG_INFO, "No Agent health issues identified.\n");
        }
        else
        {
            DebugPrintf(SV_LOG_ALWAYS, "reporting agent health issues: %s\n", serializedAgentHealthIssues.c_str());
        }

        std::map<std::string, std::string> propertyMap;
        std::string msgType;

        if (RcmConfigurator::getInstance()->IsAzureToAzureReplication())
        {
            msgType = MonitoringMessageType::AGENT_PROTECTION_PAIR_HEALTH_ISSUES;
        }
        else if (RcmConfigurator::getInstance()->IsOnPremToAzureReplication())
        {
            msgType = MonitoringMessageType::ONPREM_TO_AZURE_VM_HEALTH;
        }
        else if (RcmConfigurator::getInstance()->IsAzureToOnPremReplication())
        {
            msgType = MonitoringMessageType::AZURE_TO_ONPREM_VM_HEALTH;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "%s: unknown replication scenario\n", FUNCTION_NAME);
            return false;
        }

        DebugPrintf(SV_LOG_DEBUG, "%s:  %s\n", FUNCTION_NAME, msgType.c_str());

        RcmMonitoringEventProperties::GenerateMonitoringCustomProperties(
            RcmConfigurator::getInstance()->getHostId(),
            *RcmConfigurator::getInstance()->GetRcmClientSettings(),
            msgType,
            MonitoringMessageSource::SOURCE_AGENT,
            propertyMap);

        status = MonitoringAgent::GetInstance().SendMessageToRcm(uri,
            serializedAgentHealthIssues,
            (RcmConfigurator::getInstance()->IsAzureToAzureReplication()) ? MonitoringMessageLevel::CRITICAL : MonitoringMessageLevel::INFORMATIONAL,
            propertyMap);

        if (status)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: successfuly reported agent health issues.\n", FUNCTION_NAME);
            inm_memcpy_s(&prevAgentHealthIssuesMsgHash[0], prevAgentHealthIssuesMsgHash.size(), &currhash[0], currhash.size());
            s_lastReportTime = ACE_OS::gettimeofday().sec();
        }

    } while (false);


    if (!status)
    {
        DebugPrintf(SV_LOG_ERROR, " %s: Failed to send agent health issues to RCM with status=%d\n", FUNCTION_NAME, status);
    }

    // chain processing part. 
    if (nextInChain != NULL)
    {
        status = nextInChain->HandleHealthIssues(healthIssues) && status;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

bool AgentMonitoringMessageHandler::HandleStats(const VolumesStats_t& stats, const DiskIds_t& diskIds)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::string uri;
    bool status = false;

    do
    {
        if (!ValidateSettingsAndGetPostUri(RcmMsgStats, uri, SOURCE_AGENT))
        {
            break;
        }

        std::string statsMsg;
        boost::function<bool(std::string)> predicate =
            boost::bind(&AgentMonitoringMessageHandler::agentPredicate, this, _1);

        if (!FilterVolumeStats(stats, diskIds, statsMsg, predicate))
        {
            DebugPrintf(SV_LOG_ERROR, "%s agent handler filtering failed. skipping posting agent issue\n", FUNCTION_NAME);
            break;
        }

        DebugPrintf(SV_LOG_DEBUG, " %s: Serialized stats %s \n", FUNCTION_NAME, statsMsg.c_str());

        std::map<std::string, std::string> propertyMap;
        RcmMonitoringEventProperties::GenerateMonitoringCustomProperties(
            RcmConfigurator::getInstance()->getClusterId(),
            *RcmConfigurator::getInstance()->GetRcmClientSettings(),
            MonitoringMessageType::SOURCE_AGENT_STATS,
            MonitoringMessageSource::SOURCE_AGENT,
            propertyMap);

        DebugPrintf(SV_LOG_DEBUG, "%s: Info Queue URI %s\n", FUNCTION_NAME, uri.c_str());

        status = MonitoringAgent::GetInstance().SendMessageToRcm(uri,
            statsMsg,
            MonitoringMessageLevel::INFORMATIONAL,
            propertyMap);

    } while (false);

    if (!status)
    {
        DebugPrintf(SV_LOG_ERROR, " %s: Failed to send stats to RCM, status=%d \n", FUNCTION_NAME, status);
    }

    // chain processing part. 
    if (nextInChain != NULL)
    {
        status = nextInChain->HandleStats(stats, diskIds) && status;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

bool AgentMonitoringMessageHandler::HandleAlerts(SV_ALERT_TYPE alertType,
    SV_ALERT_MODULE alertingModule,
    const InmAlert& alert, const std::string& diskId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool status = false;
    std::string uri;

    do
    {
        if (!ValidateSettingsAndGetPostUri(RcmMsgAlert, uri, SOURCE_AGENT))
        {
            break;
        }

        if ((!diskId.empty() && !agentPredicate(diskId)) || (diskId.empty() && alertingModule == SV_ALERT_MODULE_CLUSTER))
        {
            DebugPrintf(SV_LOG_DEBUG, "%s skipping processing alert at agent handler.\n",
                FUNCTION_NAME);
            status = true;
            break;
        }

        std::string eventMessage;
        SVSTATUS eventMsgStatus = GetEventMessage(alertType, alert, eventMessage);
        if (eventMsgStatus == SVE_INVALIDARG)
        {
            DebugPrintf(SV_LOG_ERROR, "%s failed to form event message to post because of rate limit.\n",
                FUNCTION_NAME);
            status = true;
            break;
        }

        if (eventMsgStatus != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR, "%s failed to post error message with msg status=%d", FUNCTION_NAME, eventMsgStatus);
            break;
        }

        std::map<std::string, std::string> propertyMap;
        std::string msgType;

        if (diskId.empty())
        {
            if (RcmConfigurator::getInstance()->IsAzureToAzureReplication())
                msgType = MonitoringMessageType::PROTECTION_PAIR_HEALTH_EVENT;
            else if (RcmConfigurator::getInstance()->IsOnPremToAzureReplication())
                msgType = MonitoringMessageType::ONPREM_TO_AZURE_VM_HEALTH_EVENT;
            else if (RcmConfigurator::getInstance()->IsAzureToOnPremReplication())
                msgType = MonitoringMessageType::AZURE_TO_ONPREM_VM_HEALTH_EVENT;
            else
            {
                DebugPrintf(SV_LOG_ERROR, "%s: unknown replication scenario\n", FUNCTION_NAME);
                return false;
            }

            DebugPrintf(SV_LOG_DEBUG, "%s:  %s\n", FUNCTION_NAME, msgType.c_str());
            RcmMonitoringEventProperties::GenerateMonitoringCustomProperties(
                RcmConfigurator::getInstance()->getHostId(),
                *RcmConfigurator::getInstance()->GetRcmClientSettings(),
                msgType,
                MonitoringMessageSource::SOURCE_AGENT,
                propertyMap);
        }
        else
        {
            if (RcmConfigurator::getInstance()->IsAzureToAzureReplication())
                msgType = MonitoringMessageType::REPLICATION_PAIR_HEALTH_EVENT;
            else if (RcmConfigurator::getInstance()->IsOnPremToAzureReplication())
                msgType = MonitoringMessageType::ONPREM_TO_AZURE_DISK_HEALTH_EVENT;
            else if (RcmConfigurator::getInstance()->IsAzureToOnPremReplication())
                msgType = MonitoringMessageType::AZURE_TO_ONPREM_DISK_HEALTH_EVENT;
            else
            {
                DebugPrintf(SV_LOG_ERROR, "%s: unknown replication scenario\n", FUNCTION_NAME);
                return false;
            }

            DebugPrintf(SV_LOG_DEBUG, "%s:  %s\n", FUNCTION_NAME, msgType.c_str());
            RcmMonitoringEventProperties::GenerateMonitoringCustomProperties(
                RcmConfigurator::getInstance()->getHostId(),
                diskId,
                *RcmConfigurator::getInstance()->GetRcmClientSettings(),
                msgType,
                MonitoringMessageSource::SOURCE_AGENT,
                propertyMap);
        }
        status = MonitoringAgent::GetInstance().SendMessageToRcm(uri,
            eventMessage,
            MonitoringMessageLevel::CRITICAL,
            propertyMap);

    } while (false);

    if (!status)
    {
        DebugPrintf(SV_LOG_ERROR, " %s: Failed to send alert to RCM with status=%d\n", FUNCTION_NAME, status);
    }

    // chain processing part. 
    if (nextInChain != NULL)
    {
        status = nextInChain->HandleAlerts(alertType, alertingModule, alert, diskId) && status;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}