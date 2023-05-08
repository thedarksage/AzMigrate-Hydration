#ifndef _SHARED_DISK_CLUSTER_MONITORING_HANDLER_H
#define _SHARED_DISK_CLUSTER_MONITORING_HANDLER_H

#include "MonitoringMsgHandler.h"

namespace RcmClientLib
{
    class SharedDiskClusterMonitoringMessageHandler : public AbstractMonMsgPostToRcmHandler
    {
    public:

        SharedDiskClusterMonitoringMessageHandler() {}

        ~SharedDiskClusterMonitoringMessageHandler()
        {
            DebugPrintf(SV_LOG_DEBUG, "%s\n", FUNCTION_NAME);
        }

        virtual bool HandleHealthIssues(
            SourceAgentProtectionPairHealthIssues& healthIssues);

        virtual bool HandleStats(
            const VolumesStats_t& stats,
            const DiskIds_t& diskIds);

        virtual bool HandleAlerts(
            SV_ALERT_TYPE alertType,
            SV_ALERT_MODULE alertingModule,
            const InmAlert& alert,
            const std::string& diskId = std::string());
    };

}

#endif
