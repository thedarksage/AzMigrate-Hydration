#ifndef _VIRTUAL_NODE_MONITORING_HANDLER_H
#define _VIRTUAL_NODE_MONITORING_HANDLER_H

#include "MonitoringMsgHandler.h"

namespace RcmClientLib
{
    class VirtualNodeMonitoringMessageHandler : public AbstractMonMsgPostToRcmHandler
    {
    private:
        bool virtualNodePredicate(const std::string& diskId)
        {
            return RcmConfigurator::getInstance()->IsClusterSharedDiskProtected(diskId);
        }

        void GetListOfOnlineSharedDisks(std::set<std::string>& onlineDisks);

        std::string GetSerailizedHealthIssueInputForVirtualNode(SourceAgentProtectionPairHealthIssues& issues, 
            const std::set<std::string>& onlineSharedDisks);

    public:
        VirtualNodeMonitoringMessageHandler() {}

        ~VirtualNodeMonitoringMessageHandler()
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
