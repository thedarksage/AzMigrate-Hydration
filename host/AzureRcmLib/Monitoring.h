/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp. 2016
+------------------------------------------------------------------------------------+
File        :   Monitoring.h

Description :   Contains helper routines to post stats and events to an Azure service
bus for RCM to consume.

+------------------------------------------------------------------------------------+
*/
#ifndef _AGENT_MONITORING_H
#define _AGENT_MONITORING_H

#include "portable.h"
#include "inmalert.h"
#include "svstatus.h"
#include "volumegroupsettings.h"

#include "AgentHealthContract.h"

#define HASH_LENGTH 16

namespace RcmClientLib
{
    typedef std::map<std::string, std::string> DiskIds_t;

    class MonitoringLib
    {
    public:

        static void PostHealthIssuesToRcm(
            SourceAgentProtectionPairHealthIssues& healthIssues,
            const std::string &hostId);

        static bool PostStatsToRcm(const VolumesStats_t &stats,
            const DiskIds_t &diskIds,
            const std::string &hostId);

        static bool PostMonitoringStatsToRcm(std::string monStats,
            std::string hostId);
        
        static bool PostSyncProgressMessageToRcm(const std::string& synvProgMsg,
            const std::string &diskId);

        static bool SendAlertToRcm(SV_ALERT_TYPE alertType,
            SV_ALERT_MODULE alertingModule,
            const InmAlert &alert,
            const std::string &hostId, 
            const std::string &diskId = std::string());

        static bool PostAgentLogsToRcm(const std::string &messageType,
            const std::string &messageSource,
            const std::string &serializedLogMessages);
    };

}

#endif //_AGENT_MONITORING_H