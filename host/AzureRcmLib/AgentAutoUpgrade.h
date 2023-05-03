/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp. All rights reserved.
+------------------------------------------------------------------------------------+
File        :    AgentAutoUpgrade.h

Description    :   Contains definitions of corresponding Agent Upgrade

+------------------------------------------------------------------------------------+
*/

#ifndef _AGENT_AUTO_UPGRADE_H
#define _AGENT_AUTO_UPGRADE_H 

#include "json_writer.h"
#include "json_reader.h"

#include <string>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>

#include "CommonRcmContracts.h"


using boost::property_tree::ptree;

namespace RcmClientLib {

    namespace AutoUpgradeScheduleParams {
        const std::string Daily = "Daily";
        const std::string Weekly = "Weekly";
        const std::string Monthly = "Monthly";
        const std::string Yearly = "Yearly";
        const int ThrottleLimitInSeconds = 1800;
        const int InitiateJobMaxRetryCount = 3;
#ifdef SV_WINDOWS
        const int AgentDownloadDirReqSpace = 524288000;    // 500MB
        const std::string CommandSuccessIndicator = "UNIFIEDAGENTUPGRADER";
#else
        const int AgentDownloadDirReqSpace = 1073741824;    // 1GB
        const std::string CommandSuccessIndicator = "AgentUpgrade";
#endif
    }

    namespace AutoUpgradeLogParams {

        namespace Local {
#ifdef SV_WINDOWS
            const std::string InstallFiles[] = {"ASRUnifiedAgentInstaller.log",
                "UnifiedAgentUpgrader.log", "UnifiedAgentMSIInstall.log",
                "MobilityServiceValidations.log", "WrapperUnifiedAgent.log"};
            const std::string AgentLogDir = "C:\\Program Files (x86)\\Microsoft Azure Site Recovery\\agent";
#else
            const std::string InstallFiles[] = {"ua_install.log", "ua_upgrader.log"};
            const std::string AgentLogDir = "/var/log/";
#endif
            const std::string AgentFiles[] = {"AzureRcmCli.log", "svagents_curr*.log"};
        }

        namespace Remote {
            const std::string Prefix = "pre_";
            const std::string InstallFiles[] = {"ASRUnifiedAgentInstaller.log",
                "ASRUnifiedAgentUpgrader.log", "ASRUnifiedAgentMSI.log",
                "ASRUnifiedAgentPrecheck.log", "ASRUnifiedAgentWrapper.log"};
            const std::string AgentFiles[] = {"ASR_AzureRcmCli.log", "ASR_svagents.log"};
        }
    }

    /// \brief daily schedule for agent upgrade
    class MobilityAgentUpgradeDailySchedule {
    public:

        /// \brief Upgrade time window
        TimeWindow UpgradeWindow;

        /// \brief Day of week when upgrade is to be scheduled
        /// \brief empty list indicate that upgrade can done on any day of week.
        std::vector<std::string> ScheduledDays;

        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "MobilityAgentUpgradeDailySchedule", false);

            JSON_E(adapter, UpgradeWindow);
            JSON_T(adapter, ScheduledDays);
        }

        void serialize(ptree& node)
        {
            JSON_CL(node, UpgradeWindow);
            JSON_VP(node, ScheduledDays);
        }
    };

    /// \brief schedule for agent upgrade
    class MobilityAgentAutoUpgradeSchedule {
    public:


        /// \brief currently only supported value is "Daily" schedule
        std::string RecurrencePattern;

        /// \brief serialized representation of the schedule
        std::string RecurrenceDetails;

        void serialize(JSON::Adapter& adapter)
        {
            JSON::Class root(adapter, "MobilityAgentAutoUpgradeSchedule", false);
            JSON_E(adapter, RecurrencePattern);
            JSON_T(adapter, RecurrenceDetails);
        }

        void serialize(ptree& node)
        {
            JSON_P(node, RecurrencePattern);
            JSON_P(node, RecurrenceDetails);
        }
    };
}
#endif // _AGENT_AUTO_UPGRADE_H
