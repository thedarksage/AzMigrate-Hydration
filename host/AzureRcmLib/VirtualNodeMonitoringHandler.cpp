#include "VirtualNodeMonitoringHandler.h"

#include "RcmClientUtils.h"
#include "inm_md5.h"
#include "volumeinfocollector.h"
#include "portablehelpersmajor.h"
#include "datacacher.h"

#include <boost/bind.hpp>

using namespace RcmClientLib;

void RcmClientLib::VirtualNodeMonitoringMessageHandler::GetListOfOnlineSharedDisks(std::set<std::string>& onlineDisks)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

#ifdef SV_WINDOWS
    DataCacher dataCacher;
    DataCacher::VolumesCache volumesCache;
    DataCacher::CACHE_STATE cs = DataCacher::E_CACHE_DOES_NOT_EXIST;
    std::string errMsg;

    if (dataCacher.Init())
    {
        cs = dataCacher.GetVolumesFromCache(volumesCache);
        if (DataCacher::E_CACHE_VALID == cs)
        {
            VolumeSummaries_t volsummaries = volumesCache.m_Vs;

            for (VolumeSummaries_t::iterator itr = volsummaries.begin(); itr != volsummaries.end(); itr++)
            {   
                Attributes_t attri = (*itr).attributes;
                if (attri.find(NsVolumeAttributes::DEVICE_NAME) != attri.end() 
                    && attri.find(NsVolumeAttributes::IS_PART_OF_CLUSTER) != attri.end())
                {
                    std::string deviceName = attri.at(NsVolumeAttributes::DEVICE_NAME);
                    bool isOnline = false;
                    if (IsDiskOnline(deviceName, isOnline, errMsg) && isOnline)
                    {
                        onlineDisks.insert((*itr).name);
                    }
                }
            }
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "volume summaries are not present in local cache. skipping cluster alerts.\n");
        }
    }
#endif

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

std::string RcmClientLib::VirtualNodeMonitoringMessageHandler::GetSerailizedHealthIssueInputForVirtualNode(
    SourceAgentProtectionPairHealthIssues& issues,
    const std::set<std::string>& onlineSharedDisks)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::string errMsg;
    SVSTATUS status = SVS_OK;
    SourceAgentSharedDiskReplicationHealthMessage sharedDiskReplicationHealthMessage;
    
    // send empty in case disk is online
    for (std::set<std::string>::iterator itr = onlineSharedDisks.begin(); itr != onlineSharedDisks.end(); itr++)
    {
        std::string repContext; 
        if (SVS_OK == RcmConfigurator::getInstance()->GetReplicationPairMessageContext(*itr, repContext))
        {
            sharedDiskReplicationHealthMessage.SharedDiskHealthDetailsMap
                .insert(std::pair<std::string, std::vector<HealthIssue> >(repContext, std::vector<HealthIssue>()));
        }
    }
    
    for (std::vector<AgentDiskLevelHealthIssue>::iterator it = issues.DiskLevelHealthIssues.begin();
        it != issues.DiskLevelHealthIssues.end(); it++)
    {
        std::string diskId = it->DiskContext;
        if (SVS_OK != RcmConfigurator::getInstance()->GetReplicationPairMessageContext(diskId, it->DiskContext))
        {
            std::stringstream   ssError;
            AgentDiskLevelHealthIssue healthIssue = *it;
            ssError << "Failed to get context for DiskId: "
                << diskId
                << ". Dropping HealthIssue: "
                << JSON::producer<AgentDiskLevelHealthIssue>::convert(healthIssue);
            DebugPrintf(SV_LOG_ERROR, " %s: %s\n", FUNCTION_NAME, ssError.str().c_str());
            continue;
        }

        if (onlineSharedDisks.find(diskId) == onlineSharedDisks.end())
        {
            std::stringstream   ssError;
            AgentDiskLevelHealthIssue healthIssue = *it;
            ssError << "The DiskId: "
                << diskId
                << " is offline on machine. Dropping HealthIssue: "
                << JSON::producer<AgentDiskLevelHealthIssue>::convert(healthIssue);
            DebugPrintf(SV_LOG_DEBUG, " %s: %s\n", FUNCTION_NAME, ssError.str().c_str());
            continue;
        }

        HealthIssue healthIssue(it->IssueCode, it->Severity, it->Source, it->MessageParams);

        std::vector<HealthIssue>& healthIssues = sharedDiskReplicationHealthMessage.SharedDiskHealthDetailsMap.at(it->DiskContext);
        healthIssues.push_back(healthIssue);
    }

    std::string serializedVirtualNodeHealthIssues;
    
    try {
        serializedVirtualNodeHealthIssues = JSON::producer<SourceAgentSharedDiskReplicationHealthMessage>::convert(sharedDiskReplicationHealthMessage);
        DebugPrintf(SV_LOG_DEBUG, "%s: Serialized Health Issues: %s\n", FUNCTION_NAME, serializedVirtualNodeHealthIssues.c_str());
    }
    RCM_CLIENT_CATCH_EXCEPTION(errMsg, status);

    if (status != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR, " %s: %s\n", FUNCTION_NAME, errMsg.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return serializedVirtualNodeHealthIssues;
}

bool VirtualNodeMonitoringMessageHandler::HandleHealthIssues(
    SourceAgentProtectionPairHealthIssues& healthIssues)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    static std::vector<unsigned char> prevVirtualNodeHealthIssuesMsgHash(HASH_LENGTH, 0);
    static long long s_lastReportTime = 0;
    std::string uri;
    bool status = false;

    do
    {
        if (!ValidateSettingsAndGetPostUri(RcmMsgHealth, uri, VIRTUAL_NODE))
        {
            break;
        }

        if (!RcmConfigurator::getInstance()->IsAzureToAzureReplication())
        {
            DebugPrintf(SV_LOG_ERROR, "%s  virtual node handler is called for unsupported scenario, skipping.\n", FUNCTION_NAME);
            break;
        }

        std::set<std::string> onlineSharedDisksOnMachine; 
        GetListOfOnlineSharedDisks(onlineSharedDisksOnMachine);

        if (onlineSharedDisksOnMachine.empty())
        {
            DebugPrintf(SV_LOG_DEBUG, "%s  no shared disks are online on the machine, skipping.\n", FUNCTION_NAME);
            status = true;
            break;
        }

        SourceAgentProtectionPairHealthIssues virtualNodeHealthIssues =
            FilterHealthIssues(healthIssues, VIRTUAL_NODE);

        std::string serializedVirtualNodeHealthIssues = GetSerailizedHealthIssueInputForVirtualNode(virtualNodeHealthIssues, onlineSharedDisksOnMachine);

        std::vector<unsigned char> currhash(HASH_LENGTH, 0);
        INM_MD5_CTX ctx;
        INM_MD5Init(&ctx);
        INM_MD5Update(&ctx, (unsigned char*)serializedVirtualNodeHealthIssues.c_str(), serializedVirtualNodeHealthIssues.size());
        INM_MD5Final(&currhash[0], &ctx);

        bool IsChecksumMismatch = comparememory(&prevVirtualNodeHealthIssuesMsgHash[0], &currhash[0], HASH_LENGTH);

        if (IsChecksumMismatch)
        {
            DebugPrintf(SV_LOG_INFO, "virtual node health issues checksum mis-matched\n");
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "virtual node health issues checksum matched\n");
            if (!IsMinResendTimePassed(s_lastReportTime))
            {
                DebugPrintf(SV_LOG_INFO, "virtual node health issues checksum matched and last message sent was within rate limit. skipping post\n");
                status = true;
                break;
            }
        }

        if (virtualNodeHealthIssues.HealthIssues.empty() && virtualNodeHealthIssues.DiskLevelHealthIssues.empty())
        {
            DebugPrintf(SV_LOG_INFO, "No health issues identified.\n");
        }
        else
        {
            DebugPrintf(SV_LOG_ALWAYS, "reporting virtual node health issues: %s\n", serializedVirtualNodeHealthIssues.c_str());
        }

        std::map<std::string, std::string> propertyMap;
        std::string msgType = MonitoringMessageType::VIRUTAL_NODE_REPLICATION_PAIR_HEALTH_ISSUES;;

        DebugPrintf(SV_LOG_DEBUG, "%s:  %s\n", FUNCTION_NAME, msgType.c_str());

        std::string virtualNodeContext;
        RcmConfigurator::getInstance()->GetClusterVirtualNodeProtectionContext(virtualNodeContext);

        RcmMonitoringEventProperties::GenerateMonitoringCustomProperties(
            RcmConfigurator::getInstance()->getClusterId(),
            *RcmConfigurator::getInstance()->GetRcmClientSettings(),
            msgType,
            MonitoringMessageSource::SOURCE_AGENT,
            propertyMap,
            virtualNodeContext);

        status = MonitoringAgent::GetInstance().SendMessageToRcm(uri,
            serializedVirtualNodeHealthIssues,
            (RcmConfigurator::getInstance()->IsAzureToAzureReplication()) ? MonitoringMessageLevel::CRITICAL : MonitoringMessageLevel::INFORMATIONAL,
            propertyMap);

        if (status)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: successfuly reported virtual node health issues.\n", FUNCTION_NAME);
            inm_memcpy_s(&prevVirtualNodeHealthIssuesMsgHash[0], prevVirtualNodeHealthIssuesMsgHash.size(), &currhash[0], currhash.size());
            s_lastReportTime = ACE_OS::gettimeofday().sec();
        }

    } while (false);

    if (!status)
    {
        DebugPrintf(SV_LOG_ERROR, " %s: Failed to send virtual node health issues to RCM with status=%d\n", FUNCTION_NAME, status);
    }

    // chain processing part. 
    if (nextInChain != NULL)
    {
        status = nextInChain->HandleHealthIssues(healthIssues) && status;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;

}

bool VirtualNodeMonitoringMessageHandler::HandleStats(const VolumesStats_t& stats, const DiskIds_t& diskIds)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::string uri;
    bool status = false;

    do
    {
        if (!ValidateSettingsAndGetPostUri(RcmMsgStats, uri, VIRTUAL_NODE))
        {
            break;
        }

        std::string statsMsg;
        boost::function<bool(std::string)> predicate =
            boost::bind(&VirtualNodeMonitoringMessageHandler::virtualNodePredicate, this, _1);

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
        DebugPrintf(SV_LOG_ERROR, " %s: Failed to send stats to RCM with status=%d\n", FUNCTION_NAME, status);
    }

    // chain processing part. 
    if (nextInChain != NULL)
    {
        status = nextInChain->HandleStats(stats, diskIds) && status;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

bool VirtualNodeMonitoringMessageHandler::HandleAlerts(SV_ALERT_TYPE alertType,
    SV_ALERT_MODULE alertingModule,
    const InmAlert& alert, const std::string& diskId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool status = false;
    std::string uri;

    do
    {
        if (!ValidateSettingsAndGetPostUri(RcmMsgAlert, uri, VIRTUAL_NODE))
        {
            break;
        }

        if (diskId.empty() || !virtualNodePredicate(diskId))
        {
            DebugPrintf(SV_LOG_DEBUG, "%s skipping processing alert at virtual node handler.\n",
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
            DebugPrintf(SV_LOG_ERROR, "%s failed to post error message with status=%d", FUNCTION_NAME, eventMsgStatus);
            break;
        }

        std::map<std::string, std::string> propertyMap;
        std::string msgType;

        msgType = MonitoringMessageType::REPLICATION_PAIR_HEALTH_EVENT;

        DebugPrintf(SV_LOG_DEBUG, "%s:  %s\n", FUNCTION_NAME, msgType.c_str());
        RcmMonitoringEventProperties::GenerateMonitoringCustomProperties(
            RcmConfigurator::getInstance()->getClusterId(),
            diskId,
            *RcmConfigurator::getInstance()->GetRcmClientSettings(),
            msgType,
            MonitoringMessageSource::SOURCE_AGENT,
            propertyMap);

        status = MonitoringAgent::GetInstance().SendMessageToRcm(uri,
            eventMessage,
            MonitoringMessageLevel::CRITICAL,
            propertyMap);

        if (!status)
        {
            DebugPrintf(SV_LOG_ERROR, " %s: Failed to send alert to RCM with status=%d\n", FUNCTION_NAME, status);
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