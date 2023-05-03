#include <cstring>
#include <iostream>
#include <string>
#include <list>

#include <boost/lexical_cast.hpp>

#include "inmageex.h""
#include "devmetrics.h"
#include "logger.h"
#include "portablehelpers.h"

#define MAX_RECORDS_TO_CACHE    60 // one per minute, for 60 mins

DevMetrics::DevMetrics(const std::string& devId,
    const std::string& devName,
    const std::string& biosId,
    const std::string& hostId,
    const std::string& hostName,
    DevMetricsRecordType type) :m_deviceId(devId),
    m_deviceName(devName),
    m_biosId(biosId),
    m_hostId(hostId),
    m_hostName(hostName),
    m_recordType(type),
    m_prevCumulativeTrackedBytes(0),
    m_prevCumulativeTransferredBytes(0)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (m_deviceId.empty() || m_deviceName.empty() || m_biosId.empty() || m_hostId.empty() || m_hostName.empty())
        throw INMAGE_EX("One or more required input is empty.")(m_deviceId)(m_deviceName)(m_biosId)(m_hostId)(m_hostName);

    if ((m_recordType != RECORD_TYPE_AZURE) && (m_recordType != RECORD_TYPE_ONPREM))
        throw INMAGE_EX("Unsupported record type")(m_recordType);

    Init();

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void DevMetrics::Init()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    m_addChurnRecord.resize(RECORD_TYPE_MAX);

    m_addChurnRecord[RECORD_TYPE_AZURE] = &DevMetrics::AddChurnRecordForAzure;
    m_addChurnRecord[RECORD_TYPE_ONPREM] = &DevMetrics::AddChurnRecordForOnPrem;

    m_addThrpRecord.resize(RECORD_TYPE_MAX);

    m_addThrpRecord[RECORD_TYPE_AZURE] = &DevMetrics::AddThroughputRecordForAzure;
    m_addThrpRecord[RECORD_TYPE_ONPREM] = &DevMetrics::AddThroughputRecordForOnPrem;

    m_getChurnRecord.resize(RECORD_TYPE_MAX);

    m_getChurnRecord[RECORD_TYPE_AZURE] = &DevMetrics::GetChurnStatsForAzure;
    m_getChurnRecord[RECORD_TYPE_ONPREM] = &DevMetrics::GetChurnStatsForOnPrem;

    m_getThrpRecord.resize(RECORD_TYPE_MAX);

    m_getThrpRecord[RECORD_TYPE_AZURE] = &DevMetrics::GetThroughputStatsForAzure;
    m_getThrpRecord[RECORD_TYPE_ONPREM] = &DevMetrics::GetThroughputStatsForOnPrem;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

std::string DevMetrics::GetDeviceId() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s, deviceId %s\n", FUNCTION_NAME, m_deviceId.c_str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return m_deviceId;
}

std::string DevMetrics::GetDeviceName() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s, deviceName %s\n", FUNCTION_NAME, m_deviceName.c_str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return m_deviceName;
}

void DevMetrics::SetCumulativeTrackedBytes(uint64_t cumTrackedBytes)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s, tarackedBytes %lld\n", FUNCTION_NAME, m_prevCumulativeTrackedBytes);

    m_prevCumulativeTrackedBytes = cumTrackedBytes;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s, tarackedBytes %lld\n", FUNCTION_NAME, m_prevCumulativeTrackedBytes);

}

void DevMetrics::SetCumulativeTransferredBytes(uint64_t cumTransferredBytes)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s, transferredBytes %lld\n", FUNCTION_NAME, m_prevCumulativeTransferredBytes);

    m_prevCumulativeTransferredBytes = cumTransferredBytes;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s, transferredBytes %lld\n", FUNCTION_NAME, m_prevCumulativeTransferredBytes);
}

uint64_t DevMetrics::GetPrevCumulativeTrackedBytesCount()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s, tarackedBytes %lld\n", FUNCTION_NAME, m_prevCumulativeTrackedBytes);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return m_prevCumulativeTrackedBytes;
}
uint64_t DevMetrics::GetPrevCumulativeTransferredBytesCount()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s, transferredBytes %lld\n", FUNCTION_NAME, m_prevCumulativeTransferredBytes);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return m_prevCumulativeTransferredBytes;
}

void DevMetrics::AddChurnRecord(uint64_t trackedBytes, ACE_Time_Value elapsedTime)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (elapsedTime.sec() == 0)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: dropping churn record (%lld) as elapsed time (%lld ms) is less than a sec.\n", FUNCTION_NAME, trackedBytes, elapsedTime.msec());
        return;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return (this->*m_addChurnRecord[m_recordType])(trackedBytes, elapsedTime);
}

void DevMetrics::AddThroughputRecord(uint64_t transferredBytes, ACE_Time_Value elapsedTime)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (elapsedTime.sec() == 0)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: dropping throughput record (%lld) as elapsed time (%lld ms) is less than a sec.\n", FUNCTION_NAME, transferredBytes, elapsedTime.msec());
        return;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return (this->*m_addThrpRecord[m_recordType])(transferredBytes, elapsedTime);
}

void DevMetrics::AddChurnRecordForAzure(uint64_t trackedBytes, ACE_Time_Value elapsedTime)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s, trackedBytes %lld, elapsedTime %lld\n", FUNCTION_NAME, trackedBytes, elapsedTime.msec());

    if (m_vChurnRecords.size() > MAX_RECORDS_TO_CACHE)
        m_vChurnRecords.erase(m_vChurnRecords.begin());

    //We have to send Tracked Bytes/sec data rate
    uint64_t currTrackedBytesRate = (trackedBytes / elapsedTime.sec());//Tracked bytes per second

    //Insert this record to the list
    std::string strTrackedBytesRate = boost::lexical_cast<std::string>(currTrackedBytesRate);
    m_vChurnRecords.push_back(strTrackedBytesRate);

    DebugPrintf(SV_LOG_DEBUG, "Churn record %s added\n", strTrackedBytesRate.c_str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return;
}

void DevMetrics::AddThroughputRecordForAzure(uint64_t transferredBytes, ACE_Time_Value elapsedTime)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s, transferredBytes %lld, elapsedTime %lld\n", FUNCTION_NAME, transferredBytes, elapsedTime.msec());

    if (m_vThroughputRecords.size() > MAX_RECORDS_TO_CACHE)
        m_vThroughputRecords.erase(m_vThroughputRecords.begin());

    //We have to send Sent Bytes/sec data rate
    uint64_t currTransferredBytesRate = (transferredBytes / elapsedTime.sec());//Transferred bytes per second

    //Insert this record to the list
    std::string strTransferredBytesRate = boost::lexical_cast<std::string>(currTransferredBytesRate);
    m_vThroughputRecords.push_back(strTransferredBytesRate);

    DebugPrintf(SV_LOG_DEBUG, "Throughput record %s added\n", strTransferredBytesRate.c_str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return;
}

void DevMetrics::ClearChurnRecords()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    m_vChurnRecords.clear();
}

void DevMetrics::ClearThroughputRecords()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    m_vThroughputRecords.clear();
}

std::string DevMetrics::GetChurnStats()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return (this->*m_getChurnRecord[m_recordType])();
}

std::string DevMetrics::GetChurnStatsForAzure()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    uint64_t stat = 0;

    if (m_vChurnRecords.size() > 0){
        for (int i = 0; i < m_vChurnRecords.size(); ++i)
        {
            stat += boost::lexical_cast<uint64_t>(m_vChurnRecords[i]);
        }
        stat /= m_vChurnRecords.size();
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return boost::lexical_cast<std::string> (stat);
}

std::string DevMetrics::GetThroughputStats()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return (this->*m_getThrpRecord[m_recordType])();
}

std::string DevMetrics::GetThroughputStatsForAzure()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    uint64_t stat = 0;

    if (m_vThroughputRecords.size() > 0){
        for (int i = 0; i < m_vThroughputRecords.size(); ++i)
        {
            stat += boost::lexical_cast<uint64_t>(m_vThroughputRecords[i]);
        }
        stat /= m_vThroughputRecords.size();
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return boost::lexical_cast<std::string> (stat);
}

std::string DevMetrics::GetTimeStampAsString()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    time_t t;
    std::vector<char> szTime(128);
    t = time(NULL);
    if (t == 0)
    {
        throw INMAGE_EX("Failed to fetch current time.");
    }
    strftime(&szTime[0], 128, "%Y - %m - %d %H:%M:%S", localtime(&t));
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return std::string(&szTime[0]);
}

// HostId=<hostid>,DeviceId=<devid>,TimeStamp=<ts>,TrackedBytesCount=<no of bytes tracked>,TrackedBytesRate-<Rate of tracked bytes per second>
// Note: This contract of Names and sequence is enforced between Agent and PS.
// Any new field should be appended to the last and any change in the existing fields
// should also be propagated to PS.
void DevMetrics::AddChurnRecordForOnPrem(uint64_t trackedBytes, 
    ACE_Time_Value elapsedTime)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (m_vChurnRecords.size() > MAX_RECORDS_TO_CACHE)
        m_vChurnRecords.erase(m_vChurnRecords.begin());

    uint64_t ulBytesTracked = 0;
    std::string strRecord;
    //Machine Name
    strRecord += "MachineName=";
    strRecord += m_hostName;
    strRecord += ",";

    //Host Id
    strRecord += "HostId=";
    strRecord += std::string(m_hostId);
    strRecord += ",";

    //Device Id
    strRecord += "DeviceId=";
    strRecord += m_deviceName;
    strRecord += ",";

    //Bios Id
    strRecord += "BiosId=";
    strRecord += m_biosId;
    strRecord += ",";

    //Time Stamp
    strRecord += "TimeStamp=";
    std::string strTimeStamp = GetTimeStampAsString();
    strRecord += strTimeStamp;
    strRecord += ",";

    //Number of Tracked bytes = Total Churned bytes - skipped bytes (the writes that correspond to page file and bitmap file should be skipped for tracking)
    strRecord += "TrackedBytes=";
    std::string strTrackedBytes = boost::lexical_cast<std::string>(trackedBytes);
    strRecord += strTrackedBytes;
    strRecord += ",";

    strRecord += "TrackedBytesRate=";
    std::string strTrackedBytesRate = boost::lexical_cast<std::string>(trackedBytes/elapsedTime.sec());
    strRecord += strTrackedBytesRate;
    //printing as Churn record added
    DebugPrintf(SV_LOG_DEBUG, "Churn record %s added\n", strRecord.c_str());

    //Insert this record to the list;
    m_vChurnRecords.push_back(strRecord);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

// Note: This contract of Names and sequence is enforced between Agent and PS.
// Any new field should be appended to the last and any change in the existing fields
// should also be propagated to PS.
void DevMetrics::AddThroughputRecordForOnPrem(uint64_t cumTransferredBytes, 
    ACE_Time_Value elapsedTime)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (m_vThroughputRecords.size() > MAX_RECORDS_TO_CACHE)
        m_vThroughputRecords.erase(m_vThroughputRecords.begin());

    uint64_t ulBytesSent = 0;
    std::string strRecord;
    //Machine Name
    strRecord += "MachineName=";
    strRecord += m_hostName;
    strRecord += ",";

    //Host Id
    strRecord += "HostId=";
    strRecord += m_hostId;
    strRecord += ",";

    //Device Id
    strRecord += "DeviceId=";
    strRecord += m_deviceName;
    strRecord += ",";

    //Bios Id
    strRecord += "BiosId=";
    strRecord += m_biosId;
    strRecord += ",";

    //Time Stamp
    strRecord += "TimeStamp=";
    std::string strTimeStamp = GetTimeStampAsString();
    strRecord += strTimeStamp;
    strRecord += ",";

    //Number of Tracked bytes = Total Churned bytes - skipped bytes (the writes that correspond to page file and bitmap file should be skipped for tracking)
    strRecord += "SentBytes=";
    std::string strTrackedBytes = boost::lexical_cast<std::string>(cumTransferredBytes);//std::to_string(m_TrackedBytes);
    strRecord += strTrackedBytes;
    strRecord += ",";

    strRecord += "SentBytesRate=";
    std::string strTrackedBytesRate = boost::lexical_cast<std::string>(cumTransferredBytes/(elapsedTime.sec()));//std::to_string(m_TrackedBytesRate);
    strRecord += strTrackedBytesRate;
    //printing as Churn record added
    DebugPrintf(SV_LOG_DEBUG, "Throughput record %s added\n", strRecord.c_str());
    //Insert this record to the list;
    m_vThroughputRecords.push_back(strRecord);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

std::string DevMetrics::GetChurnStatsForOnPrem()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::string strChurnStats;

    std::vector<std::string>::iterator iter = m_vChurnRecords.begin();
    for (/*empty*/; iter != m_vChurnRecords.end(); iter++)
    {
        strChurnStats += (*iter);
        if ((iter+1) != m_vChurnRecords.end())
            strChurnStats += "\n";
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s, stats %s\n", FUNCTION_NAME, strChurnStats.c_str());

    return strChurnStats;
}

std::string DevMetrics::GetThroughputStatsForOnPrem()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::string strThroughputStats;

    std::vector<std::string>::iterator iter = m_vThroughputRecords.begin();
    for (/*empty*/; iter != m_vThroughputRecords.end(); iter++)
    {
        strThroughputStats += (*iter);
        if ((iter+1) != m_vThroughputRecords.end())
            strThroughputStats += "\n";
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s, stats %s\n", FUNCTION_NAME, strThroughputStats.c_str());

    return strThroughputStats;
}
