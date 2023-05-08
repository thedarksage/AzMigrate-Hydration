#include "MonitoringMsgHandler.h"
#include "AgentMonitoringHandler.h"
#include "VirtualNodeMonitoringHandler.h"
#include "SharedDiskClusterMonitoringHandler.h"
#include "RcmClientUtils.h"

#include "inmerroralertnumbers.h"
#include "filterdrvifmajor.h"
#ifdef SV_UNIX
#include "datacacher.h"
#include "persistentdevicenames.h"
#endif

#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>

#define MIN_RESEND_TIMEPERIOD 3600

using namespace RcmClientLib;

static std::string GetEventSeverityForAlertType(SV_ALERT_TYPE& alertType)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::string severity;
    switch (alertType)
    {
    case SV_ALERT_CRITICAL:
    case SV_ALERT_CRITICAL_WITH_SUBJECT:
        severity = EVENT_SEVERITY_ERROR;
        break;
    case SV_ALERT_ERROR:
        severity = EVENT_SEVERITY_WARNING;
        break;
    case SV_ALERT_SIMPLE:
        severity = EVENT_SEVERITY_INFORMATION;
        break;
    default:
        severity = EVENT_SEVERITY_UNKNOWN;
        break;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return severity;
}

boost::shared_ptr<AbstractMonMsgPostToRcmHandler> AbstractMonMsgPostToRcmHandler::GetHandlerChain()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    boost::shared_ptr<AbstractMonMsgPostToRcmHandler> msgHandler;

    msgHandler = boost::make_shared<AgentMonitoringMessageHandler>();

    if (RcmConfigurator::getInstance()->IsClusterSharedDiskEnabled())
    {
        msgHandler->SetNextHandler(boost::make_shared<VirtualNodeMonitoringMessageHandler>())
            ->SetNextHandler(boost::make_shared<SharedDiskClusterMonitoringMessageHandler>());
        DebugPrintf(SV_LOG_DEBUG, "Clustered monitoring chain returned\n");
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return msgHandler;
}

boost::shared_ptr<AbstractMonMsgPostToRcmHandler>& AbstractMonMsgPostToRcmHandler::SetNextHandler(
    const boost::shared_ptr<AbstractMonMsgPostToRcmHandler>& nextHandler)
{
    nextInChain = nextHandler;
    return nextInChain;
}

bool AbstractMonMsgPostToRcmHandler::IsPostMonitoringToRcmEnabled()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool isPostMonitoringEventsToRcmEnabled = true;

    std::string protPairContext;
    std::map<std::string, std::string> repPairMessageContexts;

    RcmConfigurator::getInstance()->GetProtectionPairContext(protPairContext);
    RcmConfigurator::getInstance()->GetReplicationPairMessageContext(repPairMessageContexts);

    if (protPairContext.empty() && repPairMessageContexts.size() == 0)
    {
        DebugPrintf(SV_LOG_ALWAYS, "%s: protection and replication contexts are empty. skipping health checks.\n", FUNCTION_NAME);
        isPostMonitoringEventsToRcmEnabled = false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return isPostMonitoringEventsToRcmEnabled;
}

bool AbstractMonMsgPostToRcmHandler::ValidateSettingsAndGetPostUri(
    MonitoringMsgType msgType, 
    std::string& uri, 
    MonitoringMessageHandlers handler)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool isSettingsValid = false;

    do
    {
        if (!IsPostMonitoringToRcmEnabled())
        {
            DebugPrintf(SV_LOG_DEBUG, "%s posting to rcm is not enabled, skipping.\n", FUNCTION_NAME);
            break;
        }

        bool isClusterVirtualNodeContext = (handler == VIRTUAL_NODE || handler == CLUSTER);
        bool isUriFetchSuccess = isClusterVirtualNodeContext ?
            MonitoringAgent::GetInstance().GetTransportUri(msgType, uri, isClusterVirtualNodeContext) 
            : MonitoringAgent::GetInstance().GetTransportUri(msgType, uri);
        
        if (!isUriFetchSuccess)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s failed to fetch uri, IsClusterVirtualNodeContext=%d ,skipping.\n", FUNCTION_NAME, isClusterVirtualNodeContext);
            break;
        }
        
        std::string context;
        switch (handler)
        {
        case SOURCE_AGENT:
            RcmConfigurator::getInstance()->GetProtectionPairContext(context);
            if (context.empty())
            {
                DebugPrintf(SV_LOG_ALWAYS, "%s failed to get protection context to post source agent message, skipping.\n", FUNCTION_NAME);
            }
            break;
        case VIRTUAL_NODE:
            RcmConfigurator::getInstance()->GetClusterVirtualNodeProtectionContext(context);
            if (context.empty())
            {
                DebugPrintf(SV_LOG_ALWAYS, "%s failed to get protection context to post virtual node message, skipping.\n", FUNCTION_NAME);
            }
            break;
        case CLUSTER:
            RcmConfigurator::getInstance()->GetClusterProtectionContext(context);
            if (context.empty())
            {
                DebugPrintf(SV_LOG_ALWAYS, "%s failed to get protection context to post  cluster message, skipping.\n", FUNCTION_NAME);
            }
            break;
        default : 
            DebugPrintf(SV_LOG_ERROR, "%s invalid handler=%d.\n", FUNCTION_NAME, handler);
        }

        if (!context.empty())
            isSettingsValid = true;

    } while (false);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return isSettingsValid;
}

int AbstractMonMsgPostToRcmHandler::GetDriverDevStream(boost::shared_ptr<DeviceStream>& devStream)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    FilterDrvIfMajor drvif;
    std::string filterDriverName = drvif.GetDriverName(VolumeSummary::DISK);
    Device dev(filterDriverName);
    devStream.reset(new DeviceStream(dev));
    if (devStream.get() == NULL)
    {
        DebugPrintf(SV_LOG_ERROR, "%s:Failed to allocate memory for creating DeviceStream.\n", FUNCTION_NAME);
        return SV_FAILURE;
    }
    int iStatus = 0;
    if (SV_SUCCESS != (iStatus = devStream->Open(DeviceStream::Mode_Open | DeviceStream::Mode_Read | DeviceStream::Mode_ShareRead)))
    {
        DebugPrintf(SV_LOG_ERROR, "%s:Failed to open the device stream for driver %s with %d.\n",
            FUNCTION_NAME,
            filterDriverName.c_str(), iStatus);
        devStream.reset();
        return SV_FAILURE;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return SV_SUCCESS;
}

double AbstractMonMsgPostToRcmHandler::GetPendingChangesAtSourceInMB(std::string& strDiskId, boost::shared_ptr<DeviceStream> pDeviceStream)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    VolumeStats vs;
    FilterDrvIfMajor drvif;
    double dblPendingChangesAtSourceInMB = 0;

    if (!strDiskId.empty() && pDeviceStream.get() != NULL)
    {
        FilterDrvVol_t volume = drvif.GetFormattedDeviceNameForDriverInput(strDiskId, VolumeSummary::DISK);
        int iStatus = drvif.FillVolumeStats(volume, *pDeviceStream, vs);
        if (SV_SUCCESS != iStatus)
        {
            DebugPrintf(SV_LOG_ERROR,
                "%s:For Disk : %s, DeviceNameForDriver:%s, fill volume stats failed with %d. Hence returning 0 bytes as Pending Changes at Source.\n",
                FUNCTION_NAME,
                strDiskId.c_str(),
                volume.c_str(),
                iStatus);
        }
        else
        {
            dblPendingChangesAtSourceInMB = (double)(vs.totalChangesPending) / (double)(BYTES_IN_MEGA_BYTE);
            DebugPrintf(SV_LOG_INFO,
                "%s:Pending Changes at Source in MB for the disk %s and DeviceNameForDirver: %s is %lf.\n",
                FUNCTION_NAME,
                strDiskId.c_str(),
                volume.c_str(),
                dblPendingChangesAtSourceInMB);
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return dblPendingChangesAtSourceInMB;
}

std::string AbstractMonMsgPostToRcmHandler::GetSerailizedHealthIssueInputForAzureToAzure(SourceAgentProtectionPairHealthIssues& issues)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::string errMsg;
    SVSTATUS status = SVS_OK;
    SourceAgentProtectionPairHealthIssues   healthIssues;

    // include the disk level health issue for which the disk context is available
    // DiskContext is overloaded with diskId and is replaced before sending health to RCM
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

        healthIssues.DiskLevelHealthIssues.push_back(*it);//adding disk level health issues which are having disk context.
    }

    healthIssues.HealthIssues.assign(issues.HealthIssues.begin(), issues.HealthIssues.end());//adding vm level health issues

    std::string serializedAgentHealthIssues;
    try {
        serializedAgentHealthIssues = JSON::producer<SourceAgentProtectionPairHealthIssues>::convert(healthIssues);
        DebugPrintf(SV_LOG_DEBUG, "%s: Serialized Health Issues: %s\n", FUNCTION_NAME, serializedAgentHealthIssues.c_str());
    }
    RCM_CLIENT_CATCH_EXCEPTION(errMsg, status);

    if (status != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR, " %s: %s\n", FUNCTION_NAME, errMsg.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return serializedAgentHealthIssues;

}

std::string AbstractMonMsgPostToRcmHandler::GetSerailizedHealthIssueInputForOnPremToAzure(SourceAgentProtectionPairHealthIssues& issues)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::string errMsg;
    SVSTATUS status = SVS_OK;
    OnPremToAzureSourceAgentVmHealthMsg   healthIssues;

    if (SVS_OK != RcmConfigurator::getInstance()->GetProtectionPairContext(healthIssues.ProtectionPairContext))
    {
        std::stringstream   ssError;
        ssError << "Failed to get protection pair context. "
            << ". Dropping Health Issues: "
            << JSON::producer<SourceAgentProtectionPairHealthIssues>::convert(issues);
        DebugPrintf(SV_LOG_ERROR, " %s: %s\n", FUNCTION_NAME, ssError.str().c_str());

        return std::string();
    }

    boost::shared_ptr<DeviceStream> pDeviceStream;
    if (SV_SUCCESS != GetDriverDevStream(pDeviceStream))
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Failed to open handle to driver. Hence returning 0 bytes as Pending Changes at Source.\n", FUNCTION_NAME);
    }

#ifdef SV_UNIX
    DataCacher dataCacher;
    DataCacher::VolumesCache volumesCache;
    DataCacher::CACHE_STATE cs = DataCacher::E_CACHE_DOES_NOT_EXIST;

    if (dataCacher.Init())
    {
        cs = dataCacher.GetVolumesFromCache(volumesCache);
        if (DataCacher::E_CACHE_VALID == cs)
        {
            DebugPrintf(SV_LOG_DEBUG, "volume summaries are present in local cache.\n");
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "%s: failed getting disk discovery cache with error: %s. Hence returning 0 bytes as Pending Changes at Source.\n",
                FUNCTION_NAME, dataCacher.GetErrMsg().c_str());
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "%s: failed to initialize data cacher to get disk discovery cache. Hence returning 0 bytes as Pending Changes at Source.\n", FUNCTION_NAME);
    }
#endif

    //Disk stats shoud be sent for the disks which don't have health issues every 5 minutes.
    //If there are health issues, then health issues should also be included.
    std::vector<std::string>vProtectedDiskIds;
    RcmConfigurator::getInstance()->GetProtectedDiskIds(vProtectedDiskIds);
    for (std::vector<std::string>::const_iterator protDiskIter = vProtectedDiskIds.begin();
        protDiskIter != vProtectedDiskIds.end(); protDiskIter++)
    {
        std::string diskId = *protDiskIter;
        std::string diskContext;
        if (SVS_OK != RcmConfigurator::getInstance()->GetReplicationPairMessageContext(diskId, diskContext))
        {
            std::stringstream ssError;
            ssError << "Dropping updating of Disk Health Issues/Stats to RCM for DiskId:"
                << diskId
                << " as ReplicationPairMessageContext could not be obtained.";
            DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, ssError.str().c_str());
            continue;
        }

        OnPremToAzureSourceAgentDiskHealthMsg diskHealth;
        diskHealth.ReplicationPairContext = diskContext;

        std::string diskIdForPendingChanges;
#ifdef SV_UNIX
        if (DataCacher::E_CACHE_VALID == cs)
        {
            diskIdForPendingChanges = GetDiskNameForPersistentDeviceName(diskId, volumesCache.m_Vs);
        }
#else
        diskIdForPendingChanges = diskId;
#endif

        diskHealth.DataPendingAtSourceAgentInMB = (pDeviceStream.get() != NULL) ? GetPendingChangesAtSourceInMB(diskIdForPendingChanges, pDeviceStream) : 0;

        for (std::vector<AgentDiskLevelHealthIssue>::const_iterator it = issues.DiskLevelHealthIssues.begin();
            it != issues.DiskLevelHealthIssues.end(); it++)
        {
            if (boost::iequals(diskId, it->DiskContext))
            {
                HealthIssue hi(it->IssueCode, it->MessageParams);
                diskHealth.HealthIssues.push_back(hi);
            }
        }

        healthIssues.DiskHealthDetails.push_back(diskHealth);//adding disk level health issues which are having disk context.
    }

    healthIssues.HealthIssues.assign(issues.HealthIssues.begin(), issues.HealthIssues.end());//adding vm level health issues

    std::string serializedAgentHealthIssues;
    try {
        serializedAgentHealthIssues = JSON::producer<OnPremToAzureSourceAgentVmHealthMsg>::convert(healthIssues);
        DebugPrintf(SV_LOG_DEBUG, "%s: Serialized Health Issues: %s\n", FUNCTION_NAME, serializedAgentHealthIssues.c_str());
    }
    RCM_CLIENT_CATCH_EXCEPTION(errMsg, status);

    if (status != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR, " %s: %s\n", FUNCTION_NAME, errMsg.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return serializedAgentHealthIssues;

}

std::string AbstractMonMsgPostToRcmHandler::GetSerailizedHealthIssueInput(SourceAgentProtectionPairHealthIssues& issues)
{
    if (RcmConfigurator::getInstance()->IsAzureToAzureReplication())
    {
        return GetSerailizedHealthIssueInputForAzureToAzure(issues);
    }
    else
    {
        return GetSerailizedHealthIssueInputForOnPremToAzure(issues);
    }

}

bool AbstractMonMsgPostToRcmHandler::IsMinResendTimePassed(const long long& lastMessageSendTime)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    bool canResend = false;
    ACE_Time_Value currentTime = ACE_OS::gettimeofday();
    canResend = (lastMessageSendTime != 0) && ((currentTime.sec() - lastMessageSendTime) > MIN_RESEND_TIMEPERIOD);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return canResend;
}

SourceAgentProtectionPairHealthIssues AbstractMonMsgPostToRcmHandler::FilterHealthIssues(
    SourceAgentProtectionPairHealthIssues& healthIssues, MonitoringMessageHandlers handler)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SourceAgentProtectionPairHealthIssues agentFiltererdHealthIssues;

    for (std::vector<HealthIssue>::iterator itr = healthIssues.HealthIssues.begin();
        itr != healthIssues.HealthIssues.end();
        itr++)
    {
        HealthIssue issue = *itr;

        if (handler == SOURCE_AGENT && issue.Source == EVENT_SOURCE_AGENT)
        {
            agentFiltererdHealthIssues.HealthIssues.push_back(issue);
        }
        else if (handler == CLUSTER && issue.Source == EVENT_SHARED_DISK_CLUSTER)
        {
            issue.Source = EVENT_SOURCE_AGENT;
            agentFiltererdHealthIssues.HealthIssues.push_back(issue);
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "%s invalid handler=%d and source=%s\n", FUNCTION_NAME, handler, issue.Source.c_str());
        }
    }
    
    std::vector<AgentDiskLevelHealthIssue>::const_iterator itr = healthIssues.DiskLevelHealthIssues.begin();
    bool isVirtualDisk = false;
    std::string diskId;
    for (; itr != healthIssues.DiskLevelHealthIssues.end(); itr++)
    {
        diskId = itr->DiskContext;
        isVirtualDisk = RcmConfigurator::getInstance()->IsClusterSharedDiskProtected(diskId);

        switch (handler)
        {
        case SOURCE_AGENT:
            if (!isVirtualDisk)
                agentFiltererdHealthIssues.DiskLevelHealthIssues.push_back(*itr);
            else 
                DebugPrintf(SV_LOG_DEBUG, "filtered disk=%s from source agent disk health issue\n",diskId.c_str());
            break;
        case VIRTUAL_NODE:
            if (isVirtualDisk)
                agentFiltererdHealthIssues.DiskLevelHealthIssues.push_back(*itr);
            else 
                DebugPrintf(SV_LOG_DEBUG, "filtered disk=%s from virtual node disk health issue\n", diskId.c_str());
            break;
        default:
            break;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return agentFiltererdHealthIssues;
}

bool AbstractMonMsgPostToRcmHandler::FilterVolumeStats(const VolumesStats_t& stats,
    const DiskIds_t& diskIds, std::string& statsMsg,
    boost::function<bool(std::string)> predicate)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool status = true;
    SVSTATUS ret = SVS_OK;
    std::string errMsg;

    AgentProtectionPairHealthMsg agentHealthMsg;

    VolumesStats_t::const_iterator vssIter = stats.begin();
    for (; vssIter != stats.end(); vssIter++)
    {
        DiskIds_t::const_iterator diskIdIter = diskIds.find(vssIter->first);

        if (diskIdIter != diskIds.end())
        {
            DebugPrintf(SV_LOG_DEBUG, " %s: volume name %s  disk id %s\n",
                FUNCTION_NAME,
                diskIdIter->first.c_str(),
                diskIdIter->second.c_str());

            if (!diskIdIter->second.length())
            {
                errMsg = "Disk id for volume ";
                errMsg += vssIter->first;
                errMsg += " is empty. Hence not reporting the stats.";
                errMsg += " data pending at source in bytes ";
                errMsg += boost::lexical_cast<std::string>(vssIter->second.totalChangesPending);
                DebugPrintf(SV_LOG_ERROR, " %s: %s\n", FUNCTION_NAME, errMsg.c_str());
                return false;
            }

            if (!predicate(diskIdIter->second))
            {
                DebugPrintf(SV_LOG_DEBUG, "%s Skipping processing for disk id=%s for current handler \n", FUNCTION_NAME,
                    diskIdIter->second.c_str());
                continue;
            }

            AgentReplicationPairHealthDetails replicationPairHealthMsg;
            replicationPairHealthMsg.SourceDiskId = diskIdIter->second;

            replicationPairHealthMsg.DataPendingAtSourceAgentInMB = static_cast<double>(vssIter->second.totalChangesPending) / BYTES_IN_MEGA_BYTE;

            DebugPrintf(SV_LOG_DEBUG, " %s: %s %f\n", FUNCTION_NAME,
                replicationPairHealthMsg.SourceDiskId.c_str(),
                replicationPairHealthMsg.DataPendingAtSourceAgentInMB);

            agentHealthMsg.DiskDetails.push_back(replicationPairHealthMsg);
        }
        else
        {
            errMsg = "Failed to find disk id for ";
            errMsg += vssIter->first;
            DebugPrintf(SV_LOG_ERROR, " %s: %s\n", FUNCTION_NAME, errMsg.c_str());
            return false;
        }
    }

    try {
        statsMsg = JSON::producer<AgentProtectionPairHealthMsg>::convert(agentHealthMsg);
    }
    RCM_CLIENT_CATCH_EXCEPTION(errMsg, ret);

    if (ret != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR, " %s: %s\n", FUNCTION_NAME, errMsg.c_str());
        status = false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS AbstractMonMsgPostToRcmHandler::GetEventMessage(SV_ALERT_TYPE alertType,
    const InmAlert& alert, std::string& eventMesg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    std::string errMsg;

    do
    {
        std::stringstream ssAlertMessage;
        std::string currentAlertType;
        AgentEventMsg eventMsg;

        static std::string lastAlertType;
        static uint64_t lastAlertTime = 0;

        eventMsg.EventCode = alert.GetID();
        eventMsg.EventSeverity = GetEventSeverityForAlertType(alertType);
        eventMsg.EventSource = EVENT_SOURCE_AGENT;

        ssAlertMessage << eventMsg.EventCode
            << eventMsg.EventSeverity
            << eventMsg.EventSource;

        currentAlertType = ssAlertMessage.str();

        InmAlert::Parameters_t params = alert.GetParameters();

        if (params.size())
        {
            InmAlert::Parameters_t::iterator paramIter = params.begin();
            for (; paramIter != params.end(); paramIter++)
            {
                eventMsg.MessageParams.insert(std::make_pair(paramIter->first, paramIter->second));
                ssAlertMessage << paramIter->first
                    << paramIter->second;
            }
        }

        // The .Net DateTime needs time since 01/01/0001 in 100 nsec granularity
        uint64_t eventTime = GetTimeInSecSinceAd0001();
        eventMsg.EventTimeUtc = eventTime * HUNDRED_NANO_SECS_IN_SEC;

        bool needsRateControl =
            (MonitoringAgent::s_noRateControlAlertTypes.find(eventMsg.EventCode) ==
                MonitoringAgent::s_noRateControlAlertTypes.end());

        // if an alert of the same type (disk read error, network failure etc.), then
        // drop the alert until rate control interval is passed
        std::map<std::string, uint64_t>& alertMap = MonitoringAgent::GetInstance().GetAlertMap();

        if (needsRateControl)
        {
            std::map<std::string, uint64_t>::iterator iterAlert =
                alertMap.find(currentAlertType);

            if (iterAlert != alertMap.end())
            {
                if (eventTime < iterAlert->second + MonitoringAgent::GetInstance().GetAlertInterval())
                {
                    DebugPrintf(SV_LOG_DEBUG,
                        "%s : Dropping alert within the rate control timeout %s\n",
                        FUNCTION_NAME,
                        ssAlertMessage.str().c_str());
                    errMsg += "Dropping alert within the rate control timeout ";
                    status = SVE_INVALIDARG;
                    break;
                }
            }
            else
            {
                alertMap[currentAlertType] = eventTime;
                DebugPrintf(SV_LOG_DEBUG,
                    "%s : Dropping first alert within the rate control timeout %s\n",
                    FUNCTION_NAME,
                    ssAlertMessage.str().c_str());
                errMsg += "Dropping first alert within the rate control timeout";
                status = SVE_INVALIDARG;
                break;
            }
        }

        try {
            eventMesg = JSON::producer<AgentEventMsg>::convert(eventMsg);
        }
        RCM_CLIENT_CATCH_EXCEPTION(errMsg, status);

        if (status != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR, "%s failed: %s\n", FUNCTION_NAME, errMsg.c_str());
            break;
        }

        alertMap[currentAlertType] = eventTime;

        DebugPrintf(SV_LOG_ALWAYS, "%s : %s\n", FUNCTION_NAME, ssAlertMessage.str().c_str());

    } while (false);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}