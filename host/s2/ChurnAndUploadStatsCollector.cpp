#include <cstring>
#include <iostream>
#include <string>
#include <list>

#include "filterdrvifmajor.h"
#include "ChurnAndUploadStatsCollector.h"
#include "localconfigurator.h"
#include "ShoeboxEventSettings.h"
#include "Monitoring.h"
#include "host.h"
#include "RcmConfigurator.h"
#include "cxpsheaders.h"

using namespace RcmClientLib;
using namespace boost::chrono;

namespace StatsCollectorConsts {
    std::string const StrMonStat("ChurStat/pre_complete_v1.0_");
    std::string const ThrpStat("ThrpStat/pre_complete_v1.0_");
    std::string const ChurnCategory("AzureSiteRecoveryProtectedDiskDataChurn");
    std::string const ThroughputCategory("AzureSiteRecoveryReplicationDataUploadRate");
}

void ChurnAndUploadStatsCollectorBase::Init(DevMetricsRecordType type, uint32_t statsSendIntervalInSecs)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    m_strHostId = RcmConfigurator::getInstance()->getHostId();
    m_strHostName = Host::GetInstance().GetHostName();

    bool bRet = GetBiosId(m_strBiosId);
    if (bRet != true)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Could not get BiosId for this machine.\n", FUNCTION_NAME);
        m_strBiosId = "NULL";
    }
    boost::erase_all(m_strBiosId, " ");

    m_requiredRecordType = type;

    m_previousThroughputSendTime = m_previousChurnSendTime = ACE_OS::gettimeofday();

    //Get Churn Statistics sending interval
    m_nChurnNThroughputStatsSendingInterval = statsSendIntervalInSecs;
    if (m_nChurnNThroughputStatsSendingInterval < 300)
    {
        m_nChurnNThroughputStatsSendingInterval = 300; //Setting minimum value to 5 minutes
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

bool ChurnAndUploadStatsCollectorBase::IsDiskIdRegistered(const std::string& diskId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s: diskId %s\n", FUNCTION_NAME, diskId.c_str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return m_mapDeviceIDDevMetrics.count(diskId) != 0;
}

void ChurnAndUploadStatsCollectorBase::RegisterDiskId(const std::string& diskId,
    const std::string& deviceName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s: diskId %s\n", FUNCTION_NAME, diskId.c_str());

    m_mapDeviceIDDevMetrics.insert(std::pair<std::string, boost::shared_ptr<DevMetrics> >(diskId,
        boost::shared_ptr<DevMetrics>(new DevMetrics(diskId, deviceName, m_strBiosId, m_strHostId, m_strHostName, m_requiredRecordType))));

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return;
}

void ChurnAndUploadStatsCollectorBase::SetCollectionTime()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    ACE_Time_Value currTime = ACE_OS::gettimeofday();

    m_elapsedCollectionTime = currTime - m_previousCollectionTime;
    m_previousCollectionTime = currTime;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void ChurnAndUploadStatsCollectorBase::SetPreviousChurnStatSendTime(const ACE_Time_Value& time)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    m_previousChurnSendTime = time;
}

void ChurnAndUploadStatsCollectorBase::SetPreviousThroughputStatSendTime(const ACE_Time_Value& time)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    m_previousThroughputSendTime = time;
}

ACE_Time_Value ChurnAndUploadStatsCollectorBase::GetPreviousChurnStatSendTime() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return m_previousChurnSendTime;
}

ACE_Time_Value ChurnAndUploadStatsCollectorBase::GetPreviousThroughputStatSendTime() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return m_previousThroughputSendTime;
}

uint32_t ChurnAndUploadStatsCollectorBase::GetStatSendInterval() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return m_nChurnNThroughputStatsSendingInterval;
}

void ChurnAndUploadStatsCollectorBase::SetCumulativeTrackedBytes(const std::string& diskId, SV_ULONGLONG cumTrackedBytes)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (!IsDiskIdRegistered(diskId))
    {
        DebugPrintf(SV_LOG_ERROR, "%s: diskId %s is not registered.\n", FUNCTION_NAME, diskId.c_str());
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return;
    }

    SV_ULONGLONG trackedBytes = 0;

    SV_ULONGLONG prevCumTrackedBytes = m_mapDeviceIDDevMetrics[diskId]->GetPrevCumulativeTrackedBytesCount();

    m_mapDeviceIDDevMetrics[diskId]->SetCumulativeTrackedBytes(cumTrackedBytes);

    if (cumTrackedBytes < prevCumTrackedBytes)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Ignoring tracked bytes for disk %s as it is less than previously reported. "
            "Mostly, \"clear differentials\" is called on driver.\n", FUNCTION_NAME, diskId.c_str());
    }
    else
    {
        trackedBytes = cumTrackedBytes - prevCumTrackedBytes;

        if ((0 == prevCumTrackedBytes) && (trackedBytes != 0))
        {
            DebugPrintf(SV_LOG_DEBUG, "Dropping churn stats of %lld bytes at first collection interval after s2 start.\n", trackedBytes);
        }
        else
        {
            m_mapDeviceIDDevMetrics[diskId]->AddChurnRecord(trackedBytes, m_elapsedCollectionTime);//Add this as a Churn record
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void ChurnAndUploadStatsCollectorBase::SetCumulativeTransferredBytes(const std::string& diskId, SV_ULONGLONG cumTransferredBytes)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (!IsDiskIdRegistered(diskId))
    {
        DebugPrintf(SV_LOG_ERROR, "%s: diskId %s is not registered.\n", FUNCTION_NAME, diskId.c_str());
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return;
    }

    SV_ULONGLONG transferredBytes = 0;

    SV_ULONGLONG prevCumTransferredBytes = m_mapDeviceIDDevMetrics[diskId]->GetPrevCumulativeTransferredBytesCount();

    m_mapDeviceIDDevMetrics[diskId]->SetCumulativeTransferredBytes(cumTransferredBytes);

    if (cumTransferredBytes < prevCumTransferredBytes)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Ignoring current cumulative sent bytes as it is less than"
            "previous value for device %s .\n", FUNCTION_NAME, diskId.c_str());
    }
    else
    {
        transferredBytes = cumTransferredBytes - prevCumTransferredBytes;
        if (prevCumTransferredBytes != 0)
        {
            m_mapDeviceIDDevMetrics[diskId]->AddThroughputRecord(transferredBytes, m_elapsedCollectionTime);//Add this as a Throughput record
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

ChurnAndUploadStatsCollectorAzure::ChurnAndUploadStatsCollectorAzure(const std::string& vaultResourceId,
    const std::map<std::string, std::string> &diskIdToDiskNameMap,
    uint32_t statsSendIntervalInSecs): m_vaultResourceId(vaultResourceId),
    m_diskIdToDiskNameMap(diskIdToDiskNameMap)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    Init(RECORD_TYPE_AZURE, statsSendIntervalInSecs);
}

void ChurnAndUploadStatsCollectorAzure::SendChurnAndThroughputStats(QuitFunction_t qf)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    int iStatus = SV_SUCCESS;
    ACE_Time_Value curTime = ACE_OS::gettimeofday();
    if ((curTime - GetPreviousChurnStatSendTime()).sec() >= GetStatSendInterval())
    {
         iStatus = SendMonitoringStatstoRCM(StatsCollectorConsts::ChurnCategory, qf);
        if (iStatus == SV_SUCCESS)
        {
            SetPreviousChurnStatSendTime(curTime);
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Failed to transfer Churn Stats to RCM\n", FUNCTION_NAME);
        }
    }

    if ((curTime - GetPreviousThroughputStatSendTime()).sec() >= GetStatSendInterval())
    {
        iStatus = SendMonitoringStatstoRCM(StatsCollectorConsts::ThroughputCategory, qf);
        if (iStatus == SV_SUCCESS)
        {
            SetPreviousThroughputStatSendTime(curTime);
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Failed to transfer Upload Rate Stats to RCM\n", FUNCTION_NAME);
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

int ChurnAndUploadStatsCollectorAzure::SendMonitoringStatstoRCM(std::string Category, QuitFunction_t qf)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    std::string statsMsg;
    int nStatus = SV_SUCCESS;
    CollectiveMonitoringStats monStatsData;

    MAP_DEVICEID_DEVMETRICS::const_iterator iter = m_mapDeviceIDDevMetrics.begin();
    for (/* empty */; iter != m_mapDeviceIDDevMetrics.end() && !qf(0); iter++)
    {
        std::string key;

        key += m_strHostName;
        key += std::string(":");
        key += m_strHostId;
        key += std::string(":");

        std::string device_id = (iter->second)->GetDeviceId();
        // To avoid crashes in case the device_id is missing from the m_diskIdToDiskNameMap
        // to handle the scenarios where the protected disk is removed manually from the vm.
        if (m_diskIdToDiskNameMap.find(device_id) == m_diskIdToDiskNameMap.end()) {
            continue;
        }

        std::string diskName = m_diskIdToDiskNameMap.at(device_id);
        key += diskName;

        namespace now = boost::posix_time;
        MonitoringStats monStatsDetails;
        monStatsDetails.time = now::to_iso_extended_string(now::microsec_clock::universal_time());
        monStatsDetails.resourceId = m_vaultResourceId;
        monStatsDetails.category = Category;
        monStatsDetails.level = "Information";
        monStatsDetails.operationName = "MICROSOFT.ASRANALYTICS/MONITORINGSTATS/WRITE";
        
        StatsPropertyMap propMap;
        if (Category == StatsCollectorConsts::ChurnCategory)
        {
            propMap.Map["InstanceName"] = key;
            propMap.Map["Value"] = (iter->second)->GetChurnStats();
        }
        else if (Category == StatsCollectorConsts::ThroughputCategory)
        {
            propMap.Map["InstanceName"] = key;
            propMap.Map["Value"] = (iter->second)->GetThroughputStats();
        }

        try {
            std::string  propString = JSON::producer<StatsPropertyMap>::convert(propMap);


            //Serialized object string also contains member variable name which we dont want in this case. So removing it.
            const std::string mapString = "{\"Map\":";
            propString.replace(0, mapString.length(), mapString.length(), ' ');

            if (!propString.empty())
                *propString.rbegin() = ' ';

            DebugPrintf(SV_LOG_DEBUG, "propstring is %s\n", propString.c_str());
            monStatsDetails.properties = propString;
            monStatsData.CollectiveStats.push_back(monStatsDetails);
        }
        catch (JSON::json_exception &je)
        {
            DebugPrintf(SV_LOG_ERROR, " %s: caught exception %s\n", FUNCTION_NAME, je.what());
        }
        catch (...)
        {
            DebugPrintf(SV_LOG_ERROR, " %s: caught unknown exception.\n", FUNCTION_NAME);
        }
    }

    try {
        statsMsg = JSON::producer<CollectiveMonitoringStats>::convert(monStatsData);
    }
    catch (JSON::json_exception &je)
    {
        std::string errMsg("Failed to serialize monitoring stats with error");
        errMsg += je.what();
        DebugPrintf(SV_LOG_ERROR, " %s: %s\n", FUNCTION_NAME, errMsg.c_str());
        nStatus = SV_FAILURE;
        return nStatus;
    }

    //Serialized object string also contains member variable name which we dont want in this case. So removing it.
    const std::string collectivestatsString = "{\"CollectiveStats\":";
    statsMsg.replace(0, collectivestatsString.length(), collectivestatsString.length(), ' ');

    if (!statsMsg.empty())
        *statsMsg.rbegin() = ' ';

    DebugPrintf(SV_LOG_DEBUG, "Serialized stats %s\n", statsMsg.c_str());

    if (MonitoringLib::PostMonitoringStatsToRcm(statsMsg, m_strHostId))
    {
        MAP_DEVICEID_DEVMETRICS::const_iterator iter = m_mapDeviceIDDevMetrics.begin();
        while (iter != m_mapDeviceIDDevMetrics.end())
        {
            if (Category == StatsCollectorConsts::ChurnCategory)
            {
                (iter->second)->ClearChurnRecords();
            }
            else
            {
                (iter->second)->ClearThroughputRecords();
            }
            iter++;
        }

        nStatus = SV_SUCCESS;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, " %s: Failed to post throughput stats to RCM\n", FUNCTION_NAME);
        nStatus = SV_FAILURE;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return nStatus;
}


ChurnAndUploadStatsCollectorOnPrem::ChurnAndUploadStatsCollectorOnPrem(uint32_t statsSendIntervalInSecs)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    Init(RECORD_TYPE_ONPREM, statsSendIntervalInSecs);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void ChurnAndUploadStatsCollectorOnPrem::SendChurnAndThroughputStats(QuitFunction_t qf)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    int status = SV_SUCCESS;
    int iStatus = SV_SUCCESS;
    ACE_Time_Value curTime = ACE_OS::gettimeofday();
    static bool s_bTelemetryPathErrorLogged = false;

    std::string  strStatsDest = RcmConfigurator::getInstance()->GetTelemetryFolderPathForOnPremToAzure();
    if (strStatsDest.empty())
    {
        DebugPrintf(s_bTelemetryPathErrorLogged? SV_LOG_DEBUG : SV_LOG_ERROR, "%s: telemetry folder path is empty.\n", FUNCTION_NAME);
        s_bTelemetryPathErrorLogged = true;
        return;
    }

    s_bTelemetryPathErrorLogged = false;

    if ((curTime - GetPreviousChurnStatSendTime()).sec() >= GetStatSendInterval())
    {
        iStatus = SendChurnStatsToPS(strStatsDest, qf);
        if (iStatus == SV_SUCCESS)
        {
            SetPreviousChurnStatSendTime(curTime);
        }
        else
        {
            DebugPrintf(SV_LOG_WARNING, "%s: Failed to transfer churn stats to PS.\n", FUNCTION_NAME);
        }
    }

    if ((curTime - GetPreviousThroughputStatSendTime()).sec() >= GetStatSendInterval())
    {
        iStatus = SendThroughputStatsToPS(strStatsDest, qf);
        if (iStatus == SV_SUCCESS)
        {
            SetPreviousThroughputStatSendTime(curTime);
        }
        else
        {
            DebugPrintf(SV_LOG_WARNING, "%s: Failed to transfer throughput stats to PS.\n", FUNCTION_NAME);
        }
    }

    // close connection to PS
    m_cxTransport.reset();

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

int ChurnAndUploadStatsCollectorOnPrem::SendChurnStatsToPS(const std::string& strStatsDest,
    QuitFunction_t qf)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    bool bStatsSent = false;

    MAP_DEVICEID_DEVMETRICS::const_iterator iter = m_mapDeviceIDDevMetrics.begin();
    for (/*empty*/; iter != m_mapDeviceIDDevMetrics.end() && !qf(0) ; iter++)
    {
        //Churn data will be sent to PS in the following file content format...
        //MachineName=<hostName>,HostId=<hostId>,DeviceId=<diskName>,BiosId=<biosId>,TimeStamp=<timeStamp>,TrackedBytes=<TrackedBytes>,TrackedBytesRate=<TrackedBytesRate>
        //Churn Data will be sent to PS in the following file name format :
        // <TelemetryFolderPath/ChurStat/pre_complete_v1.0_<hostId>_<deviceId>_<timeStamp>.churstat

        std::string strData = (iter->second)->GetChurnStats();
        if (strData.empty())
            continue;

        std::string strChurnDest = strStatsDest;
        strChurnDest += "/";
        strChurnDest += StatsCollectorConsts::StrMonStat;

        strChurnDest += m_strHostId;
        strChurnDest += std::string("_");

        std::string device_id = (iter->second)->GetDeviceId();
        std::replace(device_id.begin(), device_id.end(), '/', '_');
        strChurnDest += device_id;
        strChurnDest += std::string("_");

        time_t currTime = time(NULL);
        std::string timeStr = boost::lexical_cast<std::string>(currTime);
        strChurnDest += timeStr;
        strChurnDest += ".churstat";

        int nStatus = SV_SUCCESS;

        try
        {
            nStatus = SendStatsToPS((iter->second)->GetDeviceId(), strData.c_str(), strData.length(), strChurnDest, CxpsTelemetry::FileType_ChurStat, qf);
            if (SV_SUCCESS != nStatus)
            {
                DebugPrintf(SV_LOG_WARNING,
                    "%s: Failed to send churn stats to PS for device %s.\n",
                    FUNCTION_NAME,
                    (iter->second)->GetDeviceId().c_str());
            }
            else
            {
                DebugPrintf(SV_LOG_INFO, "Successfully sent churn stats to PS.\n");

                (iter->second)->ClearChurnRecords();//after successful sending, clear the in-memory Churn records
            }
        }
        catch (const std::exception &e)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: failed with exception %s\n", FUNCTION_NAME, e.what());
            nStatus = SV_FAILURE;
        }
        catch (...)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: failed with unknown exception.\n", FUNCTION_NAME);
            nStatus = SV_FAILURE;
        }

        bStatsSent |= nStatus == SV_SUCCESS;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return bStatsSent ? SV_SUCCESS : SV_FAILURE;
}

int ChurnAndUploadStatsCollectorOnPrem::SendThroughputStatsToPS(const std::string& strStatsDest,
    QuitFunction_t qf)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    bool bStatsSent = false;

    //Throughput Stats content format...
    //MachineName = <hostName>, HostId = <hostId>, DeviceId = <diskName>, BiosId = <biosId>, TimeStamp = <timeStamp>, SentBytes = <TrackedBytes>, SentBytesRate = <TrackedBytesRate>
    //Throughput Stats are sent to PS in the following file name format...
   // <TelemetryFolderPath>/svsystems/ChurStat/ThrpStat/pre_complete_v1.0_<hostId>_<deviceId>_<timeStamp>.thrpstat
    
    MAP_DEVICEID_DEVMETRICS::const_iterator iter = m_mapDeviceIDDevMetrics.begin();
    for (/*empty*/; iter != m_mapDeviceIDDevMetrics.end() && !qf(0); iter++)
    {
        std::string strData = (iter->second)->GetThroughputStats();
        if (strData.empty())
            continue;

        std::string  strThrpDest = strStatsDest;
        strThrpDest  += "/";
        strThrpDest += StatsCollectorConsts::ThrpStat;

        strThrpDest += m_strHostId;
        strThrpDest += std::string("_");

        std::string device_id = (iter->second)->GetDeviceId();
        std::replace(device_id.begin(), device_id.end(), '/', '_');
        strThrpDest += device_id;
        strThrpDest += std::string("_");

        time_t currTime = time(NULL);
        std::string timeStr = boost::lexical_cast<std::string>(currTime);
        strThrpDest += timeStr;
        strThrpDest += ".thrpstat";

        int nStatus = SV_SUCCESS;

        try
        {
            nStatus = SendStatsToPS((iter->second)->GetDeviceId(), strData.c_str(), strData.length(), strThrpDest, CxpsTelemetry::FileType_ThrpStat, qf);
            if (SV_SUCCESS != nStatus)
            {
                DebugPrintf(SV_LOG_WARNING,
                    "%s: Failed to send throughput stats for device %s.\n",
                    FUNCTION_NAME,
                    (iter->second)->GetDeviceId().c_str());
            }
            else
            {
                DebugPrintf(SV_LOG_INFO, "Successfully sent throughput stats to PS.\n");
                (iter->second)->ClearThroughputRecords();//after successful sending, clear the in-memory Churn records
            }
        }
        catch (const std::exception &e)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: failed with exception %s\n", FUNCTION_NAME, e.what());
            nStatus = SV_FAILURE;
        }
        catch (...)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: failed with unknown exception.\n", FUNCTION_NAME);
            nStatus = SV_FAILURE;
        }

        bStatsSent |= nStatus == SV_SUCCESS;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return bStatsSent ? SV_SUCCESS : SV_FAILURE;
}

int ChurnAndUploadStatsCollectorOnPrem::SendStatsToPS(const std::string& diskId,
    const char* sDataToSend,
    const long int liLength,
    const std::string& sDestination,
    const CxpsTelemetry::FileType& fileType,
    QuitFunction_t qf)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    int iStatus = SV_SUCCESS;
    if (sDestination.empty())
    {
        DebugPrintf(SV_LOG_ERROR, "%s:stats file name is empty.\n", FUNCTION_NAME);
        return SV_FAILURE;
    }

    if (SV_SUCCESS != ConnectToPS())
    {
        return SV_FAILURE;
    }

    std::map < std::string, std::string>headers;
    headers[std::string(HTTP_PARAM_TAG_FILETYPE)] = boost::lexical_cast<std::string>(fileType);
    headers[std::string(HTTP_PARAM_TAG_DISKID)] = diskId;
    if (!m_cxTransport->putFile(sDestination, liLength, sDataToSend, false, COMPRESS_NONE, headers, true))
    {
        iStatus = SV_FAILURE;
        DebugPrintf(SV_LOG_ERROR, "%s: Failed to send stats file %s for disk % to PS with status %s.\n",
            FUNCTION_NAME,
            sDestination.c_str(),
            diskId.c_str(),
            m_cxTransport->status());
    }
    else
    {
        std::string repName;
        repName = sDestination;
        repName.replace(repName.find("pre_complete_"), sizeof("pre_complete_") - 1, "completed_");
        if (m_cxTransport->renameFile(sDestination, repName, COMPRESS_NONE, headers))
        {
            iStatus = SV_SUCCESS;
        }
        else
        {
            iStatus = SV_FAILURE;
            DebugPrintf(SV_LOG_ERROR, "%s: Failed to rename stats file %s for disk %s to PS with status %s.\n",
                FUNCTION_NAME,
                sDestination.c_str(),
                diskId.c_str(),
                m_cxTransport->status());
        }
    }

    if (iStatus != SV_SUCCESS)
    {
        m_cxTransport.reset();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return iStatus;
}

int ChurnAndUploadStatsCollectorOnPrem::ConnectToPS()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    int iStatus =  SV_FAILURE;
    TRANSPORT_CONNECTION_SETTINGS transConnSettings;

    if (!m_cxTransport.get())
    {
        try {
            std::string dataPathTransportType, dataPathTransportSettings;
            RcmConfigurator::getInstance()->GetDataPathTransportSettings(dataPathTransportType,
                dataPathTransportSettings);

            ProcessServerTransportSettings psTransportSettings =
                JSON::consumer<ProcessServerTransportSettings>::convert(dataPathTransportSettings, true);

            transConnSettings.ipAddress = psTransportSettings.GetIPAddress();
            transConnSettings.sslPort = boost::lexical_cast<std::string>(psTransportSettings.Port);

            m_cxTransport.reset(new CxTransport(TRANSPORT_PROTOCOL_HTTP, transConnSettings, true));
            iStatus = SV_SUCCESS;
        }
        catch (JSON::json_exception &je)
        {
            DebugPrintf(SV_LOG_ERROR, "%s:failed with serialize exception %s.\n", FUNCTION_NAME, je.what());
        }
        catch (const std::exception& e)
        {
            DebugPrintf(SV_LOG_ERROR, "%s:failed with exception %s.\n", FUNCTION_NAME, e.what());
        }
        catch (...)
        {
            DebugPrintf(SV_LOG_ERROR, "%s:failed with unknow exception.\n", FUNCTION_NAME);
        }
    }
    else
    {
        iStatus = SV_SUCCESS;
        DebugPrintf(SV_LOG_DEBUG, "%s: transport already initalized.\n", FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return iStatus;
}