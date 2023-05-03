#ifndef _AGENT_MONITORING_HANDLER_H
#define _AGENT_MONITORING_HANDLER_H

#include "MonitoringMsgHandler.h"

namespace RcmClientLib
{
    class AgentMonitoringMessageHandler : public AbstractMonMsgPostToRcmHandler
    {
    private:
        bool agentPredicate(const std::string& diskId)
        {
            return !RcmConfigurator::getInstance()->IsClusterSharedDiskProtected(diskId);
        }

    public:
        AgentMonitoringMessageHandler() {}

        ~AgentMonitoringMessageHandler()
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
