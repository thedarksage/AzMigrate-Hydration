#include "consistencythread-rcm.h"
#include "logger.h"
#include "inmalertdefs.h"
#include "Monitoring.h"
#include "host.h"

#include <boost/chrono.hpp>

using namespace TagTelemetry;
using namespace RcmClientLib;
using namespace TagTelemetry;

void ConsistencyThreadRCM::GetConsistencySettings()
{
    SVSTATUS ret = RcmConfigurator::getInstance()->GetConsistencySettings(m_consistencySettings);
    if (ret != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Failed to fetch consistency settings with %d.\n", FUNCTION_NAME, ret);
        return;
    }

    m_prevSettingsFetchTime = boost::chrono::steady_clock::now();

    return;
}

void ConsistencyThreadRCM::GetConsistencyIntervals(uint64_t &crashInterval, uint64_t &appInterval)
{
    boost::chrono::steady_clock::time_point currentTime = boost::chrono::steady_clock::now();
    if (currentTime > m_prevSettingsFetchTime + boost::chrono::seconds(CONSISTENCY_SETTINGS_FETCH_INTERVAL))
    {
        GetConsistencySettings();
    }

    crashInterval = m_consistencySettings.CrashConsistencyInterval;
    crashInterval /= 10 * 1000 * 1000;

    appInterval = m_consistencySettings.AppConsistencyInterval;
    appInterval /= 10 * 1000 * 1000;
}


bool ConsistencyThreadRCM::IsMasterNode() const
{
    // default true as for single node each node is master node
    bool bIsMasterNode = true;
    if (m_consistencySettings.MultiVmConsistencyParticipatingNodes.size() > 1)
    {
        if (!boost::iequals(m_consistencySettings.CoordinatorMachine, Host::GetInstance().GetHostName()))
        {
            bIsMasterNode = false;
        }
    }
    return bIsMasterNode;
}

void ConsistencyThreadRCM::SendAlert(InmAlert &inmAlert) const
{
    MonitoringLib::SendAlertToRcm(SV_ALERT_SIMPLE, SV_ALERT_MODULE_CONSISTENCY, inmAlert, m_hostId);
}

void ConsistencyThreadRCM::AddDiskInfoInTagStatus(const TagTelemetry::TagType &tagType,
    const std::string &cmdOutput,
    TagTelemetry::TagStatus &tagStatus) const
{
    if (tagType == BASELINE_CONSISTENCY)
    {
        ADD_INT_TO_MAP(tagStatus, NUMOFDISKINPOLICY, 1);

        std::string strVal;
        GetValueForPropertyKey(cmdOutput, "-v", strVal);
        ADD_STRING_TO_MAP(tagStatus, DISKIDLISTINPOLICY, strVal);
    }
    else
    {
        std::vector<std::string> protectedDiskIds;

        SVSTATUS ret = RcmConfigurator::getInstance()->GetProtectedDiskIds(protectedDiskIds);
        if (ret != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: failed to fetch protected device list with %d\n",
                FUNCTION_NAME,
                ret);
            return;
        }

        ADD_INT_TO_MAP(tagStatus, NUMOFDISKINPOLICY, protectedDiskIds.size());

        std::string diskList;
        for (std::vector<std::string>::const_iterator dsit = protectedDiskIds.begin();
            dsit != protectedDiskIds.end();
            dsit++)
        {
            diskList += *dsit;
            diskList += ",";
        }

        ADD_STRING_TO_MAP(tagStatus, DISKIDLISTINPOLICY, diskList);
    }

    return;
}

uint32_t ConsistencyThreadRCM::GetNoOfNodes()
{
    return m_consistencySettings.MultiVmConsistencyParticipatingNodes.size();
}

bool ConsistencyThreadRCM::IsTagFailureHealthIssueRequired() const
{
    return (boost::iequals(RcmConfigurator::getInstance()->getCSType(), CSTYPE_CSPRIME) &&
        RcmClientLib::RcmConfigurator::getInstance()->IsAzureToOnPremReplication());
}