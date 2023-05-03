/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp. 2016
+------------------------------------------------------------------------------------+
File		:	SourceComponent.h

Description	:   SourceComponent class is input to RCM dataplane API 
RegisterSourceComponent. This class should match RCM SourceAgentEntity class.

+------------------------------------------------------------------------------------+
*/
#ifndef _SOURCE_COMPONENT_H
#define _SOURCE_COMPONENT_H

#include "json_writer.h"
#include "json_reader.h"

#include <string>

namespace RcmClientLib
{
    namespace ProetctionScenario
    {
        const std::string AzureToAzure = "AzureToAzure";
        const std::string AzureToOnPrem = "AzureToOnPrem";
        const std::string OnPremToAzure = "OnPremToAzure";
    }

    const std::string SourceAgentSupportedFeatures[] =
    {
        "PrivateLink"
    };

    /// \brief represent the InMage agent installed on the system
    class SourceAgent
    {
    public:

        /// \\brief unique id of the agent
        std::string  Id;

        /// \brief FQDN
        std::string  FriendlyName;

        /// \brief agent version as defined by installer
        std::string  AgentVersion;

        /// \brief disk filter driver version
        std::string  DriverVersion;

        /// \brief agent service name, i.e. svagents
        std::string ServiceName;

        /// \brief agent service display name i.e. Volume Replication Service
        std::string ServiceDisplayName;

        /// \brief agent installation location
        std::string  InstallationLocation;

        /// \brief distro name
        std::string DistroName;

        /// \brief protection scenario
        std::string ProtectionScenario;

        /// \brief source agent supported job list
        std::vector<std::string> SupportedJobs;

        /// \brief source agent supported feature list
        std::vector<std::string> SupportedFeatures;

    public:
        SourceAgent(const std::string& id,
                    const std::string& hostname,
                    const std::string& agentVer,
                    const std::string& driverVer,
                    const std::string& installLoc,
                    const std::string& distroName,
                    const std::string& protectionScenario,
                    const std::vector<std::string>& supportedJobs
                    )
            : Id(id),
            FriendlyName(hostname),
            AgentVersion(agentVer),
            DriverVersion(driverVer),
            InstallationLocation(installLoc),
            DistroName(distroName),
            ProtectionScenario(protectionScenario),
            SupportedJobs(supportedJobs),
            ServiceName("svagents"),
            ServiceDisplayName("Volume Replication Service")
        {
            SupportedFeatures.assign(SourceAgentSupportedFeatures,
                SourceAgentSupportedFeatures + sizeof(SourceAgentSupportedFeatures) / sizeof(std::string));
        }

        /// \brief serialize function for the JSON serilaizer
        void serialize(JSON::Adapter& adapter)
        {
             JSON::Class root(adapter, "SourceAgent", false);
             JSON_E(adapter, Id);
             JSON_E(adapter, FriendlyName);
             JSON_E(adapter, AgentVersion);
             JSON_E(adapter, DriverVersion);
             JSON_E(adapter, ServiceName);
             JSON_E(adapter, ServiceDisplayName);
             JSON_E(adapter, InstallationLocation);
             JSON_E(adapter, DistroName);
             JSON_E(adapter, ProtectionScenario);
             JSON_E(adapter, SupportedJobs);
             JSON_T(adapter, SupportedFeatures);
        }

        void serialize(ptree& node)
        {
            JSON_P(node, Id);
            JSON_P(node, FriendlyName);
            JSON_P(node, AgentVersion);
            JSON_P(node, DriverVersion);
            JSON_P(node, ServiceName);
            JSON_P(node, ServiceDisplayName);
            JSON_P(node, InstallationLocation);
            JSON_P(node, DistroName);
            JSON_P(node, ProtectionScenario);
            JSON_VP(node, SupportedJobs);
            JSON_VP(node, SupportedFeatures);
            return;
        }
    };
}
#endif
