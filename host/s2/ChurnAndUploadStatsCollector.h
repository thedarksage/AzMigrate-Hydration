#ifndef CHURN_AND_UPLOAD_STATS_COLLECTOR__H
#define CHURN_AND_UPLOAD_STATS_COLLECTOR__H

//#pragma once

#include <string>
#include <map>

#include <ace/Time_Value.h>

#include "devmetrics.h"
#include "cxtransport.h"

typedef std::map<std::string, boost::shared_ptr<DevMetrics> > MAP_DEVICEID_DEVMETRICS;

class ChurnAndUploadStatsCollectorBase
{
public:

    ChurnAndUploadStatsCollectorBase() {}

    virtual ~ChurnAndUploadStatsCollectorBase() {}

    virtual void SendChurnAndThroughputStats(QuitFunction_t qf) = 0;

    void Init(DevMetricsRecordType type, uint32_t statsSendIntervalInSecs);

    bool IsDiskIdRegistered(const std::string& diskId);

    void RegisterDiskId(const std::string& diskId, const std::string& deviceName);

    void SetCumulativeTrackedBytes(const std::string& diskId, SV_ULONGLONG cumTrackedBytes);

    void SetCumulativeTransferredBytes(const std::string& diskId, SV_ULONGLONG cumTransferredBytes);

    void SetCollectionTime();

    ACE_Time_Value GetPreviousChurnStatSendTime() const;
    ACE_Time_Value GetPreviousThroughputStatSendTime() const;

    void SetPreviousChurnStatSendTime(const ACE_Time_Value& time);
    void SetPreviousThroughputStatSendTime(const ACE_Time_Value& time);

    uint32_t GetStatSendInterval() const;

protected:
    std::string                     m_strHostId;
    std::string                     m_strHostName;
    std::string                     m_strBiosId;

    MAP_DEVICEID_DEVMETRICS         m_mapDeviceIDDevMetrics;

private:

    DevMetricsRecordType            m_requiredRecordType;

    ACE_Time_Value                  m_previousCollectionTime;
    ACE_Time_Value                  m_elapsedCollectionTime;

    //churn and data upload metrics
    ACE_Time_Value m_previousChurnSendTime;
    ACE_Time_Value m_previousThroughputSendTime;
    uint32_t m_nChurnNThroughputStatsSendingInterval;

};

typedef boost::shared_ptr<ChurnAndUploadStatsCollectorBase> ChurnAndUploadStatsCollectorBasePtr;


class ChurnAndUploadStatsCollectorAzure: public ChurnAndUploadStatsCollectorBase
{
public:
    ChurnAndUploadStatsCollectorAzure(const std::string& vaultResourceId,
        const std::map<std::string, std::string> &diskIdToDiskNameMap,
        uint32_t statsSendIntervalInSecs);

    virtual void SendChurnAndThroughputStats(QuitFunction_t qf);

private:

    int SendMonitoringStatstoRCM(std::string Category, QuitFunction_t qf);

private:
    std::string                             m_vaultResourceId;
    std::map<std::string, std::string>      m_diskIdToDiskNameMap;

};


class ChurnAndUploadStatsCollectorOnPrem : public ChurnAndUploadStatsCollectorBase
{
public:
    ChurnAndUploadStatsCollectorOnPrem(uint32_t statsSendIntervalInSecs);

    virtual void SendChurnAndThroughputStats(QuitFunction_t qf);

private:

    int SendChurnStatsToPS(const std::string& strChurnDest, QuitFunction_t qf);
    int SendThroughputStatsToPS(const std::string& strThrpDest, QuitFunction_t qf);

    int SendStatsToPS(const std::string& diskId,
        const char* sDataToSend,
        const long int liLength,
        const std::string& sDestination,
        const CxpsTelemetry::FileType& fileType,
        QuitFunction_t qf);

    int ConnectToPS();

private:
    CxTransport::ptr        m_cxTransport;
};
#endif