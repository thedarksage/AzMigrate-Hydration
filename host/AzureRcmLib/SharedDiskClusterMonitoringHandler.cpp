#include "SharedDiskClusterMonitoringHandler.h"

#include "RcmClientUtils.h"
#include "inm_md5.h"

using namespace RcmClientLib;

bool SharedDiskClusterMonitoringMessageHandler::HandleHealthIssues(
    SourceAgentProtectionPairHealthIssues& healthIssues)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    
    static std::vector<unsigned char> prevClusterHealthIssuesMsgHash(HASH_LENGTH, 0);
    static long long s_lastReportTime = 0;

    std::string errMsg;
    std::string uri;
    bool isHealthIssuePostSuccess = false;

    do
    {
        if (!ValidateSettingsAndGetPostUri(RcmMsgHealth, uri, CLUSTER))
        {
            break;
        }

        if (!RcmConfigurator::getInstance()->IsAzureToAzureReplication())
        {
            DebugPrintf(SV_LOG_ERROR, "%s  cluster handler is called for unsupported scenario, skipping.\n", FUNCTION_NAME);
            break;
        }

        SourceAgentProtectionPairHealthIssues filteredHealthIssue =
            FilterHealthIssues(healthIssues, CLUSTER);

        SourceAgentClusterProtectionHealthMessage clusterHealthIssues;
        clusterHealthIssues.HealthIssues = filteredHealthIssue.HealthIssues;

        std::string serializedShareddiskclusterHealthIssues;
        SVSTATUS status = SVS_OK;
        try {
            serializedShareddiskclusterHealthIssues = JSON::producer<SourceAgentClusterProtectionHealthMessage>::convert(clusterHealthIssues);
            DebugPrintf(SV_LOG_DEBUG, "%s: Serialized Cluster Health Issues: %s\n", FUNCTION_NAME, serializedShareddiskclusterHealthIssues.c_str());
        }
        RCM_CLIENT_CATCH_EXCEPTION(errMsg, status);

        if (status != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR, "%s parsing failed with error message=%s\n", FUNCTION_NAME, errMsg.c_str());
            break;
        }
        

        std::vector<unsigned char> currhash(HASH_LENGTH, 0);
        INM_MD5_CTX ctx;
        INM_MD5Init(&ctx);
        INM_MD5Update(&ctx, (unsigned char*)serializedShareddiskclusterHealthIssues.c_str(), serializedShareddiskclusterHealthIssues.size());
        INM_MD5Final(&currhash[0], &ctx);

        bool IsChecksumMismatch = comparememory(&prevClusterHealthIssuesMsgHash[0], &currhash[0], HASH_LENGTH);

        if (IsChecksumMismatch)
        {
            DebugPrintf(SV_LOG_INFO, "cluster health issues checksum mis-matched\n");
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "cluster health issues checksum matched\n");
            if (!IsMinResendTimePassed(s_lastReportTime))
            {
                DebugPrintf(SV_LOG_INFO, "cluster health issues checksum matched and last message sent was within rate limit. skipping post\n");
                isHealthIssuePostSuccess = true;
                break;
            }
        }

        std::map<std::string, std::string> propertyMap;
        std::string msgType = MonitoringMessageType::CLUSTER_PROTECTION_HEALTH_ISSUES;;

        DebugPrintf(SV_LOG_DEBUG, "%s:  %s\n", FUNCTION_NAME, msgType.c_str());

        std::string clusterContext;
        RcmConfigurator::getInstance()->GetClusterProtectionContext(clusterContext);

        RcmMonitoringEventProperties::GenerateMonitoringCustomProperties(
            "",
            *RcmConfigurator::getInstance()->GetRcmClientSettings(),
            msgType,
            MonitoringMessageSource::SOURCE_AGENT,
            propertyMap,
            clusterContext);

        isHealthIssuePostSuccess = MonitoringAgent::GetInstance().SendMessageToRcm(uri,
            serializedShareddiskclusterHealthIssues,
            MonitoringMessageLevel::CRITICAL,
            propertyMap);

        if (isHealthIssuePostSuccess)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: successfuly reported virtual node health issues.\n", FUNCTION_NAME);
            inm_memcpy_s(&prevClusterHealthIssuesMsgHash[0], prevClusterHealthIssuesMsgHash.size(), &currhash[0], currhash.size());
            s_lastReportTime = ACE_OS::gettimeofday().sec();
        }

    } while (false);

    if (!isHealthIssuePostSuccess)
    {
        DebugPrintf(SV_LOG_ERROR, " %s: Failed to send cluster health issues to RCM.\n", FUNCTION_NAME);
    }

    // chain processing part. 
    if (nextInChain != NULL)
    {
        isHealthIssuePostSuccess = nextInChain->HandleHealthIssues(healthIssues) && isHealthIssuePostSuccess;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return isHealthIssuePostSuccess;
}

bool SharedDiskClusterMonitoringMessageHandler::HandleStats(const VolumesStats_t& stats, const DiskIds_t& diskIds)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool status = true;

    if (nextInChain != NULL)
    {
        status = nextInChain->HandleStats(stats, diskIds) && status;
    }
   
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

bool SharedDiskClusterMonitoringMessageHandler::HandleAlerts(SV_ALERT_TYPE alertType,
    SV_ALERT_MODULE alertingModule,
    const InmAlert& alert, const std::string& diskId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool status = true;
    std::string uri;

    do
    {
        // alerts needs to be sent in virtual node protection pair context
        if (!ValidateSettingsAndGetPostUri(RcmMsgAlert, uri, VIRTUAL_NODE))
        {
            break;
        }

        if (alertingModule != SV_ALERT_MODULE_CLUSTER)
        {
            DebugPrintf(SV_LOG_ERROR, "%s skipping as not a cluster alert, alert module=%d \n",
                FUNCTION_NAME, alertingModule);
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
            DebugPrintf(SV_LOG_ERROR, "%s failed to post error message with status=%d", FUNCTION_NAME, eventMsgStatus);
            break;
        }


        std::map<std::string, std::string> propertyMap;
        std::string msgType;

        msgType = MonitoringMessageType::PROTECTION_PAIR_HEALTH_EVENT;
      

        DebugPrintf(SV_LOG_DEBUG, "%s:  %s\n", FUNCTION_NAME, msgType.c_str());
        std::string clusterProtectionContext;
        // cluster events are passed with virtual node context.
        RcmConfigurator::getInstance()->GetClusterVirtualNodeProtectionContext(clusterProtectionContext);

        RcmMonitoringEventProperties::GenerateMonitoringCustomProperties(
            RcmConfigurator::getInstance()->getHostId(),
            *RcmConfigurator::getInstance()->GetRcmClientSettings(),
            msgType,
            MonitoringMessageSource::SOURCE_AGENT,
            propertyMap, 
            clusterProtectionContext);

        status = MonitoringAgent::GetInstance().SendMessageToRcm(uri,
            eventMessage,
            MonitoringMessageLevel::CRITICAL,
            propertyMap);

        if (!status)
        {
            DebugPrintf(SV_LOG_ERROR, " %s: Failed to send alert to RCM\n", FUNCTION_NAME);
        }

    } while (false);

    // chain processing part. 
    if (nextInChain != NULL)
    {
        status = nextInChain->HandleAlerts(alertType, alertingModule, alert, diskId) && status;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}