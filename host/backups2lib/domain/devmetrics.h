#ifndef DEVMETRICS__H
#define DEVMETRICS__H

//#pragma once
#include <string>
#include <vector>
#include <ace/Time_Value.h>

typedef enum devMetricsRecortType {
    RECORD_TYPE_UNKNOWN = 0,
    RECORD_TYPE_AZURE,
    RECORD_TYPE_ONPREM,
    RECORD_TYPE_MAX
} DevMetricsRecordType;

class DevMetrics
{
private:
    /// \brief id used in context with control plane
    /// ex: disk-signature for Windows OS or DataDisk1 for Linux
    const std::string m_deviceId;

    /// \brief device name as represented by OS
    /// ex: PHYSICALDRIVE0 or /dev/sdc
    const std::string m_deviceName;

    const std::string m_biosId;
    const std::string m_hostId;
    const std::string m_hostName;

    DevMetricsRecordType    m_recordType;

    // format for Azure : Hostid=<hostid>,DeviceId=<dev id>,Timestamp=<ts>,TrackedBytes=<bytesTracked>,
    // TrackedBytesRate=<bytes tracked per second>

    // format for OnPrem: MachineName=<hostname>,Hostid=<hostid>,deviceId=<dev id>,BiosId=<biosid>,
    // Timestamp=<ts>, TrackedBytes=<bytesTracked>, TrackedBytesRate=<bytes tracked per second>

    std::vector<std::string>m_vChurnRecords;
    std::vector<std::string>m_vThroughputRecords;

    // Cumulative data transferred by this volume in bytes
    uint64_t m_prevCumulativeTrackedBytes;
    uint64_t m_prevCumulativeTransferredBytes;

    typedef std::string (DevMetrics::*GetStats_t)();
    std::vector<GetStats_t> m_getThrpRecord;
    std::vector<GetStats_t> m_getChurnRecord;

    typedef void (DevMetrics::*AddRecord_t)(uint64_t, ACE_Time_Value);
    std::vector<AddRecord_t> m_addThrpRecord;
    std::vector<AddRecord_t> m_addChurnRecord;

private:
    std::string GetTimeStampAsString();

    void AddChurnRecordForAzure(uint64_t trackedBytes, ACE_Time_Value elapsedTime);
    void AddChurnRecordForOnPrem(uint64_t trackedBytes, ACE_Time_Value elapsedTime);

    void AddThroughputRecordForAzure(uint64_t transferredBytes, ACE_Time_Value elapsedTime);
    void AddThroughputRecordForOnPrem(uint64_t transferredBytes, ACE_Time_Value elapsedTime);

    std::string GetChurnStatsForAzure();
    std::string GetChurnStatsForOnPrem();

    std::string GetThroughputStatsForAzure();
    std::string GetThroughputStatsForOnPrem();

    void Init();
public:

    explicit DevMetrics(const std::string& devId,
        const std::string& devName,
        const std::string& biosId,
        const std::string& hostId,
        const std::string& hostname,
        DevMetricsRecordType type);

    std::string GetDeviceId() const;

    std::string GetDeviceName() const;

    void AddChurnRecord(uint64_t trackedBytes, ACE_Time_Value elapsedTime);
    void AddThroughputRecord(uint64_t transferredBytes, ACE_Time_Value elapsedTime);

    void SetCumulativeTrackedBytes(uint64_t cumTrackedBytes);
    void SetCumulativeTransferredBytes(uint64_t cumTransferredBytes);

    uint64_t GetPrevCumulativeTrackedBytesCount();
    uint64_t GetPrevCumulativeTransferredBytesCount();

    void ClearChurnRecords();
    void ClearThroughputRecords();

    std::string GetChurnStats();
    std::string GetThroughputStats();
};

#endif